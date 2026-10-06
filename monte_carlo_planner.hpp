#pragma once
#include <bits/stdc++.h>

// 有限手数の期待得点を比較する単一スレッドのモンテカルロ計画
// C++20。Model は planner より長く生存し、探索中は採点・遷移・推定分布を変更しない
// State / Info / Action はコピー可能。ID・試行数・手数は int の範囲、得点は有限値
// 得点の二乗と試行数の積も double の範囲に収める。極大値用の拡張演算は行わない
// CRN の指定はコールバックと既定 rollout の乱数共有だけを変更し、候補参加・試行配分・停止条件を変えない
// コールバックの途中は停止できない。時間指定はマイクロ秒単位で、厳密な超過ゼロではない
// 部分観測では方策・キー・終了判定は Info のみ。leaf_value / finish_value は const State& と const Info& でも採点可能
enum class MonteCarloFinishReason { terminal, horizon };

class MonteCarloNoise {
    uint64_t seed_, state_;

    static uint64_t mix(uint64_t x) {
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    }

    uint64_t key_seed(uint64_t key, uint64_t channel) const {
        return mix(seed_ ^ mix(key + 0x243f6a8885a308d3ULL)
                         ^ mix(channel + 0x13198a2e03707344ULL));
    }

public:
    using result_type = uint64_t;

    // seed で乱数系列を初期化する O(1)
    explicit MonteCarloNoise(uint64_t seed = 1) : seed_(seed), state_(seed) {}
    // URBG の最小値を返す O(1)
    static constexpr result_type min() { return 0; }
    // URBG の最大値を返す O(1)
    static constexpr result_type max() { return UINT64_MAX; }
    // 次の 64 bit 乱数を返す O(1)
    result_type operator()() { return mix(state_ += 0x9e3779b97f4a7c15ULL); }
    // この系列の seed を返す O(1)
    uint64_t scenario_seed() const { return seed_; }
    // [0,1) の一様乱数を返す O(1)
    double uniform01() { return ((*this)() >> 11) * 0x1.0p-53; }
    // [0,bound) の整数を返す。1値の場合も乱数を1個消費する 期待 O(1)
    uint64_t uniform_int(uint64_t bound) {
        assert(bound > 0);
        if (bound == 1) { (*this)(); return 0; }
        uint64_t x = (*this)();
        // 棄却閾値はbound未満なので、必要な場合だけ求める。
        if (x < bound) {
            const uint64_t threshold = -bound % bound;
            while (x < threshold) x = (*this)();
        }
        return x % bound;
    }
    // 指定した出来事の独立した stream を作る O(1)
    template<std::uniform_random_bit_generator Rng = MonteCarloNoise>
    Rng stream(uint64_t key, uint64_t channel = 0) const {
        return Rng(key_seed(key, channel));
    }
    // seed から利用者の URBG を作る 生成関数の計算量
    template<class Factory>
    auto stream(uint64_t key, uint64_t channel, Factory&& factory) const {
        return std::invoke(std::forward<Factory>(factory), key_seed(key, channel));
    }
    // 同じ出来事・成分を再現する O(1)
    double uniform01(uint64_t key, uint64_t channel, uint64_t draw_index) const {
        return (mix(key_seed(key, channel) + 0x9e3779b97f4a7c15ULL * (draw_index + 1)) >> 11)
               * 0x1.0p-53;
    }
};

// Rng は uint64_t seed から構築可能な URBG。CRN ON はコールバックごとに構築するため軽量なものを推奨
template<class Model, std::uniform_random_bit_generator Rng = MonteCarloNoise,
         class Clock = std::chrono::steady_clock>
class MonteCarloPlanner {
    static_assert(std::constructible_from<Rng, uint64_t>);
    static constexpr bool partial = requires { typename Model::Info; };
    static auto info_type() {
        if constexpr (partial) return std::type_identity<typename Model::Info>{};
        else return std::type_identity<typename Model::State>{};
    }

public:
    using State = typename Model::State;
    using Info = typename decltype(info_type())::type;
    using Action = typename Model::Action;
    struct Options {
        // 全乱数系列の seed。CRN は共有の有無だけを切り替える
        uint64_t seed = 1;
        bool use_crn = false, tree = false;
        // 0 は残り手数まで完走。短くする場合は Model::leaf_value が必要
        int simulation_depth = 0;
        // value_scale=0 は通常の評価から尺度を推定。値を切り詰めない
        double discount = 1, exploration = 1, value_scale = 0;
        int max_nodes = 32768, max_edges = 131072;
        // 参加上限は widening*sqrt(完了数+1)。0 なら毎試行一候補を追加
        double widening = 2;
        int clock_interval = 8;
    };
    struct Limits {
        // -1 はその制約なし。少なくとも一つ指定し、最初に達した制約で止める
        int64_t time_us = -1;
        typename Clock::time_point deadline = Clock::time_point::max();
        int max_simulations = -1, max_transitions = -1;
    };
    struct ActionId {
        uint64_t generation = 0;
        int index = -1;
        friend bool operator==(const ActionId&, const ActionId&) = default;
    };
    enum class Status { ready, not_ready, terminal };
    enum class Stop { none, time, simulations, transitions, not_ready, terminal };
    struct RunStats {
        int started = 0, simulations = 0, transitions = 0;
        int64_t elapsed_us = 0;
        Stop stop = Stop::none;
    };
    struct Candidate {
        ActionId id;
        int trials = 0;
        std::optional<double> mean;
    };
    struct Result {
        std::optional<Action> action;
        ActionId id;
        Status status = Status::not_ready;
        bool provisional = false;
        std::vector<Candidate> candidates;
        RunStats last_run;
    };

private:
    struct Empty {};
    using WorkingInfo = std::conditional_t<partial, Info, Empty>;
    static decltype(auto) key(Model& model, const Info& info) {
        if constexpr (requires { model.tree_key(info); }) return model.tree_key(info);
        else return (info);
    }
    using Key = std::decay_t<decltype(key(std::declval<Model&>(), std::declval<const Info&>()))>;
    static constexpr bool comparable = requires(const Key& a, const Key& b) {
        { a == b } -> std::convertible_to<bool>;
    };
    struct Moments {
        int n = 0;
        double mean = 0, m2 = 0;
        void add(double x) {
            ++n;
            double d = x - mean;
            mean += d / n;
            m2 += d * (x - mean);
        }
    };
    struct Node {
        std::vector<int> edges;
        int active = 0, proposed = 0;
        bool loaded = false, exhausted = false, truncated = false;
        Moments returns;
    };
    struct Edge {
        Action action;
        Moments value;
        std::vector<int> children;
        double inverse_sqrt = 0;
    };
    struct Trace { int node, edge; double reward; };
    struct Pending {
        State state;
        WorkingInfo info;
        uint64_t seed;
        int depth = 0, node = 0, root_position = 0, edge = -1;
        std::optional<Action> action{};
        double tail = 0, tail_discount = 1;
    };
    Model& model_;
    Options options_;
    MonteCarloNoise root_noise_;
    std::optional<Rng> rng_;
    std::optional<Info> root_;
    std::optional<Pending> pending_;
    std::vector<Node> nodes_;
    std::vector<Edge> edges_;
    // 根以外のノードiのキーはkeys_[i-1]に保存する
    std::vector<Key> keys_;
    std::vector<int> crn_counts_;
    std::vector<Action> action_buffer_;
    std::vector<Trace> trace_;
    uint64_t generation_ = 0;
    int horizon_ = 0, fallback_ = -1;
    RunStats last_;

    const Info& information() const {
        if constexpr (partial) return pending_->info;
        else return pending_->state;
    }
    bool terminal(const Info& info) const {
        if constexpr (requires { model_.terminal(info); }) return model_.terminal(info);
        else return false;
    }
    double finish(MonteCarloFinishReason reason) {
        const auto& state = pending_->state;
        if constexpr (partial && requires { model_.finish_value(state, information(), reason); })
            return model_.finish_value(state, information(), reason);
        else if constexpr (partial && requires { model_.finish_value(state, information()); })
            return model_.finish_value(state, information());
        else if constexpr (requires { model_.finish_value(state, reason); })
            return model_.finish_value(state, reason);
        else if constexpr (requires { model_.finish_value(state); })
            return model_.finish_value(state);
        else return 0;
    }
    double leaf() {
        const auto& state = pending_->state;
        const int remaining = horizon_ - pending_->depth;
        if constexpr (partial && requires { model_.leaf_value(state, information(), remaining); })
            return model_.leaf_value(state, information(), remaining);
        else if constexpr (requires { model_.leaf_value(information(), remaining); })
            return model_.leaf_value(information(), remaining);
        else { assert(false && "simulation_depth requires leaf_value"); return 0; }
    }
    static int random_index(Rng& rng, int n) {
        assert(n > 0);
        if constexpr (std::same_as<Rng, MonteCarloNoise>) return int(rng.uniform_int(n));
        else return std::uniform_int_distribution<int>(0, n - 1)(rng);
    }
    template<class Visitor>
    void actions(const Info& info, int remaining, Visitor&& visit) {
        if constexpr (requires { model_.actions(info, remaining, action_buffer_); }) {
            action_buffer_.clear();
            model_.actions(info, remaining, action_buffer_);
            for (auto& a : action_buffer_) visit(std::move(a));
        } else if constexpr (requires { model_.actions(info); }) {
            const auto& list = model_.actions(info);
            for (const auto& a : list) visit(a);
        }
    }
    template<class A>
    int register_action(int node, A&& action) {
        assert(int(edges_.size()) < options_.max_edges);
        int id = int(edges_.size());
        edges_.push_back(Edge{std::forward<A>(action), {}, {}});
        nodes_[node].edges.push_back(id);
        if (node == 0) {
            if (fallback_ < 0) fallback_ = id;
            if (options_.use_crn) crn_counts_.push_back(0);
        }
        return id;
    }
    static constexpr bool has_proposer = requires(Model& m, const Info& info, Rng& rng) {
        m.propose_action(info, 1, 0, rng);
    } || requires(Model& m, const Info& info, Rng& rng, const MonteCarloNoise& context) {
        m.propose_action(info, 1, 0, rng, context);
    };
    auto propose(const Info& info, int remaining, int index, Rng& rng,
                 const MonteCarloNoise& context) {
        if constexpr (requires { model_.propose_action(info, remaining, index, rng, context); })
            return model_.propose_action(info, remaining, index, rng, context);
        else return model_.propose_action(info, remaining, index, rng);
    }
    void prepare(int node, const Info& info, int remaining) {
        if (nodes_[node].loaded) return;
        nodes_[node].loaded = true;
        if constexpr (!has_proposer) {
            actions(info, remaining, [&](auto&& action) {
                if (int(edges_.size()) < options_.max_edges)
                    register_action(node, std::forward<decltype(action)>(action));
                else nodes_[node].truncated = true;
            });
            nodes_[node].exhausted = true;
        }
    }
    template<bool Crn>
    bool admit(int node, const Info& info, int remaining) {
        auto& n = nodes_[node];
        const int target = options_.widening == 0 ? INT_MAX :
            std::max(1, int(options_.widening * std::sqrt(n.returns.n + 1.0)));
        if (n.active >= target) return false;
        if (n.active < int(n.edges.size())) { ++n.active; return true; }
        if constexpr (has_proposer) {
            if (!n.exhausted && int(edges_.size()) < options_.max_edges) {
                // CRN ON の候補系列は初回訪問の試行番号・それ以前の乱数消費数から独立
                const auto context = root_noise_.stream(remaining, Crn ? 0 : node)
                                                .stream(n.proposed, 3);
                auto a = with_rng<Crn>(context, remaining, 3, [&](Rng& rng) {
                    return propose(info, remaining, n.proposed++, rng, context);
                });
                if (a) { register_action(node, std::move(*a)); ++n.active; return true; }
                n.exhausted = true;
            }
        }
        return false;
    }
    double scale(const Node& n) const {
        if (options_.value_scale > 0) return options_.value_scale;
        return n.returns.n > 1 ? 2 * std::sqrt(std::max(0.0, n.returns.m2 / (n.returns.n - 1))) : 0;
    }
    int recommend() const {
        int best = -1;
        for (int e : std::span(nodes_[0].edges).first(nodes_[0].active)) {
            const auto& v = edges_[e].value;
            if (!v.n) continue;
            if (best < 0) { best = e; continue; }
            const auto& b = edges_[best].value;
            const bool better = std::tuple(v.mean, v.n, -e) > std::tuple(b.mean, b.n, -best);
            if (better) best = e;
        }
        return best < 0 ? fallback_ : best;
    }
    template<bool Crn>
    int select(int node, const Info& info, int remaining) {
        prepare(node, info, remaining);
        if (admit<Crn>(node, info, remaining)) return nodes_[node].active - 1;
        const auto& n = nodes_[node];
        if (!n.active) return -1;
        const double width = scale(n);
        if (width == 0 && options_.value_scale == 0) {
            int best = 0;
            for (int i = 1; i < n.active; ++i)
                if (edges_[n.edges[i]].value.n < edges_[n.edges[best]].value.n) best = i;
            return best;
        }
        const double log_n = std::log(n.returns.n + 1.0);
        const double coefficient = width * std::sqrt(log_n);
        int best = 0;
        double best_score = -std::numeric_limits<double>::infinity();
        // 候補の試行数と尺度を使うが、CRN の指定は参照しない
        for (int i = 0; i < n.active; ++i) {
            const auto& v = edges_[n.edges[i]].value;
            if (!v.n) return i;
            double bonus = coefficient * edges_[n.edges[i]].inverse_sqrt;
            double score = v.mean + options_.exploration * bonus;
            if (score > best_score) { best_score = score; best = i; }
        }
        return best;
    }
    Action rollout(const Info& info, int remaining, Rng& rng, const MonteCarloNoise& context) {
        if constexpr (requires { model_.rollout_action(info, remaining, rng, context); }) {
            return model_.rollout_action(info, remaining, rng, context);
        } else if constexpr (requires { model_.rollout_action(info, remaining, rng); }) {
            return model_.rollout_action(info, remaining, rng);
        } else if constexpr (requires { model_.actions(info, remaining, action_buffer_); }) {
            action_buffer_.clear();
            model_.actions(info, remaining, action_buffer_);
            assert(!action_buffer_.empty());
            return std::move(action_buffer_[random_index(rng, int(action_buffer_.size()))]);
        } else if constexpr (requires { model_.actions(info); }) {
            const auto& list = model_.actions(info);
            assert(!std::ranges::empty(list));
            return *std::next(std::ranges::begin(list), random_index(rng, int(std::ranges::size(list))));
        } else {
            std::optional<Action> picked;
            if constexpr (has_proposer) picked = propose(info, remaining, 0, rng, context);
            assert(picked && "nonterminal state requires actions or rollout_action");
            return std::move(*picked);
        }
    }
    template<bool Crn, class Callback>
    decltype(auto) with_rng(const MonteCarloNoise& context, int remaining, uint64_t role, Callback&& callback) {
        if constexpr (!Crn) return callback(*rng_);
        uint64_t seed = context.scenario_seed();
        if (role != 3) seed = MonteCarloNoise(seed + 0x9e3779b97f4a7c15ULL * (uint64_t(remaining) * 4 + role))();
        Rng rng(seed);
        return callback(rng);
    }
    template<bool Crn>
    bool begin_trial() {
        int position = select<Crn>(0, *root_, horizon_);
        if (position < 0) return false;
        const int ordinal = Crn ? crn_counts_[position] : nodes_[0].returns.n;
        const auto context = root_noise_.stream(ordinal);
        const uint64_t seed = context.scenario_seed();
        if constexpr (partial) {
            const Info& info = *root_;
            auto state = with_rng<Crn>(context, horizon_, 0, [&](Rng& rng) {
                if constexpr (requires { model_.sample_root(info, rng, context); })
                    return model_.sample_root(info, rng, context);
                else return model_.sample_root(info, rng);
            });
            pending_.emplace(std::move(state), info, seed);
        } else pending_.emplace(*root_, WorkingInfo{}, seed);
        pending_->root_position = position;
        pending_->edge = nodes_[0].edges[position];
        pending_->action.emplace(edges_[pending_->edge].action);
        trace_.clear();
        return true;
    }
    int successor(int edge, const Info& info) {
        if constexpr (!comparable) { (void)edge; (void)info; return -1; }
        else {
            const auto& value = key(model_, info);
            for (int child : edges_[edge].children)
                if (keys_[child - 1] == value) return child;
            if (int(nodes_.size()) >= options_.max_nodes) return -1;
            int id = int(nodes_.size());
            nodes_.emplace_back();
            keys_.push_back(value);
            edges_[edge].children.push_back(id);
            return -1; // 新しい枝は一つ保存し、残りはロールアウトする
        }
    }
    // ロールアウト後半の逐次集計と、保存した経路の逆順集計を結合する
    template<bool Crn>
    void backup(double value) {
        assert(std::isfinite(value));
        value = pending_->tail + pending_->tail_discount * value;
        for (int i = int(trace_.size()) - 1; i >= 0; --i) {
            const auto t = trace_[i];
            value = t.reward + options_.discount * value;
            assert(std::isfinite(value));
            auto& e = edges_[t.edge];
            e.value.add(value);
            e.inverse_sqrt = 1 / std::sqrt(double(e.value.n));
            nodes_[t.node].returns.add(value);
        }
        if constexpr (Crn) ++crn_counts_[pending_->root_position];
        pending_.reset();
    }
    template<class... Args>
    decltype(auto) call_step(Rng& rng, const MonteCarloNoise& context, Args&... args) {
        if constexpr (requires { model_.step(args..., rng, context); })
            return model_.step(args..., rng, context);
        else return model_.step(args..., rng);
    }
    // 小さい遷移で呼び出し境界の費用を抑える（GCC/Clang）。
    template<bool Crn>
    [[gnu::always_inline]] bool transition() {
        auto& p = *pending_;
        const int remaining = horizon_ - p.depth;
        const MonteCarloNoise context(p.seed);
        // 選択済みの行動は中断をまたいで保持する
        if (!p.action) {
            if (p.node >= 0) {
                int pos = select<Crn>(p.node, information(), remaining);
                if (pos >= 0) {
                    p.edge = nodes_[p.node].edges[pos];
                    p.action.emplace(edges_[p.edge].action);
                }
            }
            if (!p.action) {
                p.node = p.edge = -1;
                p.action.emplace(with_rng<Crn>(context, remaining, 2, [&](Rng& rng) {
                    return rollout(information(), remaining, rng, context);
                }));
            }
        }
        // 用途 0: 初期状態、1: 遷移、2: rollout、3: 保存する候補提案
        // ON は手数・用途ごとに再構築、OFF は保持した RNG を続けて使う
        double reward = with_rng<Crn>(context, remaining, 1, [&](Rng& rng) {
            auto step = [&]() -> decltype(auto) {
                if constexpr (partial) return call_step(rng, context, p.state, p.info, *p.action);
                else return call_step(rng, context, p.state, *p.action);
            };
            if constexpr (std::is_void_v<decltype(step())>) { step(); return 0.0; }
            else return step();
        });
        assert(std::isfinite(reward));
        if (p.node >= 0) trace_.push_back({p.node, p.edge, reward});
        else {
            p.tail += p.tail_discount * reward;
            p.tail_discount *= options_.discount;
        }
        ++p.depth;
        p.action.reset();

        // 終了と浅い近似を区別し、完了した軌跡だけを平均へ反映する
        if (terminal(information())) { backup<Crn>(finish(MonteCarloFinishReason::terminal)); return true; }
        if (p.depth == horizon_) { backup<Crn>(finish(MonteCarloFinishReason::horizon)); return true; }
        if (options_.simulation_depth && p.depth == options_.simulation_depth) {
            backup<Crn>(leaf()); return true;
        }
        p.node = options_.tree && p.node >= 0 ? successor(p.edge, information()) : -1;
        p.edge = -1;
        return false;
    }
    void next_generation() {
        ++generation_;
        root_noise_ = MonteCarloNoise(options_.seed).stream(generation_);
        if (!options_.use_crn) rng_.emplace(root_noise_.scenario_seed());
    }
    void reset(const Info& root, int horizon) {
        assert(horizon >= 0);
        root_ = root;
        horizon_ = horizon;
        next_generation();
        pending_.reset();
        nodes_.clear(); edges_.clear(); keys_.clear(); crn_counts_.clear();
        trace_.clear();
        nodes_.emplace_back();
        fallback_ = -1;
        last_ = {};
    }
    void compact(int root_node) {
        std::vector<int> node_map(nodes_.size(), -1), edge_map(edges_.size(), -1);
        node_map[root_node] = 0;
        // 子のIDは親より大きいので、到達印を前から伝播できる
        for (int i = root_node; i < int(nodes_.size()); ++i) if (node_map[i] == 0)
            for (int e : nodes_[i].edges) {
                edge_map[e] = 0;
                for (int c : edges_[e].children) node_map[c] = 0;
            }
        int nn = 0, ne = 0;
        for (int& x : node_map) if (x == 0) x = nn++;
        for (int& x : edge_map) if (x == 0) x = ne++;
        // 木なので子の ID は親より大きく、前から詰めれば未読要素を壊さない
        for (int i = 0; i < int(nodes_.size()); ++i) if (node_map[i] >= 0) {
            int dst = node_map[i];
            if (dst != i) nodes_[dst] = std::move(nodes_[i]);
            auto& n = nodes_[dst];
            for (int& e : n.edges) e = edge_map[e];
            if (dst > 0 && dst != i) keys_[dst - 1] = std::move(keys_[i - 1]);
        }
        for (int i = 0; i < int(edges_.size()); ++i) if (edge_map[i] >= 0) {
            int dst = edge_map[i];
            if (dst != i) edges_[dst] = std::move(edges_[i]);
            for (int& c : edges_[dst].children) c = node_map[c];
        }
        nodes_.erase(nodes_.begin() + nn, nodes_.end());
        edges_.erase(edges_.begin() + ne, edges_.end());
        keys_.erase(keys_.begin() + nn - 1, keys_.end());
    }

    // CRN の分岐は探索開始時に済ませ、各手の RNG 選択から除く
    template<bool Crn>
    RunStats search_impl(const Limits& limits) {
        assert(root_);
        assert(limits.time_us >= -1 && limits.max_simulations >= -1 && limits.max_transitions >= -1);
        assert(limits.time_us >= 0 || limits.deadline != Clock::time_point::max()
               || limits.max_simulations >= 0 || limits.max_transitions >= 0);
        const auto begin = Clock::now();
        auto deadline = limits.deadline;
        if (limits.time_us >= 0) deadline = std::min(deadline, begin + std::chrono::microseconds(limits.time_us));
        const bool timed = deadline != Clock::time_point::max();
        last_ = {};
        int check_at = 0, stride = options_.clock_interval;
        auto last_clock = begin;
        bool root_checked = false;
        while (true) {
            if (limits.max_simulations >= 0 && last_.simulations >= limits.max_simulations) { last_.stop = Stop::simulations; break; }
            if (limits.max_transitions >= 0 && last_.transitions >= limits.max_transitions) { last_.stop = Stop::transitions; break; }
            if (timed && last_.transitions >= check_at) {
                auto now = Clock::now();
                if (now >= deadline) { last_.stop = Stop::time; break; }
                if (deadline - now <= 2 * (now - last_clock)) stride = 1;
                last_clock = now;
                check_at = last_.transitions + stride;
            }
            if (!root_checked) {
                if (!horizon_ || terminal(*root_)) { last_.stop = Stop::terminal; break; }
                root_checked = true;
            }
            if (!pending_) {
                if (!begin_trial<Crn>()) { last_.stop = Stop::not_ready; break; }
                ++last_.started;
                if (timed && Clock::now() >= deadline) { last_.stop = Stop::time; break; }
            }
            if (transition<Crn>()) ++last_.simulations;
            ++last_.transitions;
        }
        last_.elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - begin).count();
        return last_;
    }

public:
    // モデルを参照して探索器を作る O(1)
    explicit MonteCarloPlanner(Model& model, Options options = {})
        : model_(model), options_(options) {
        assert(options_.max_nodes >= 1 && options_.max_edges >= 1);
        assert(options_.discount >= 0 && options_.discount <= 1);
        assert(options_.exploration >= 0 && options_.value_scale >= 0 && options_.widening >= 0);
        assert(options_.simulation_depth >= 0 && options_.clock_interval >= 1);
        assert(!options_.tree || comparable);
    }
    MonteCarloPlanner(Model&&, Options = {}) = delete;
    // 新しい根を保持して自動候補生成を準備する 旧探索の破棄と Info コピーの計算量
    void start(const Info& root, int horizon) { reset(root, horizon); }
    // 指定した候補だけを根へ登録する O(候補数 + Info コピー)
    template<std::ranges::input_range Range>
    void start(const Info& root, int horizon, const Range& candidates) {
        reset(root, horizon);
        nodes_[0].loaded = nodes_[0].exhausted = true;
        for (const auto& a : candidates) register_action(0, a);
    }
    // 候補を追加し未参加集合の先頭へ置く O(根の候補数 + Action コピー)
    ActionId add_root_action(const Action& action) {
        assert(root_);
        int id = register_action(0, action);
        auto& n = nodes_[0];
        std::rotate(n.edges.begin() + n.active, n.edges.end() - 1, n.edges.end());
        if (options_.use_crn)
            std::rotate(crn_counts_.begin() + n.active, crn_counts_.end() - 1, crn_counts_.end());
        return {generation_, id};
    }
    // 時間だけを指定して追加探索する 実行したシミュレーションの計算量
    RunStats search(int64_t time_us) { Limits l; l.time_us = time_us; return search(l); }
    // 指定した RNG 型で追加探索する 実行したシミュレーションの計算量
    RunStats search(const Limits& limits) {
        return options_.use_crn ? search_impl<true>(limits) : search_impl<false>(limits);
    }
    // 推薦行動をコピーして返す O(根の候補数 + Action コピー)
    std::optional<Action> best_action() const {
        if (!root_ || !horizon_ || terminal(*root_)) return std::nullopt;
        int e = recommend();
        if (e < 0) return std::nullopt;
        return edges_[e].action;
    }
    // 探索を進めず推薦を返す。false なら候補統計 vector を作らない O(根の候補数 + Action コピー)
    Result result(bool include_candidates = true) const {
        Result r;
        r.last_run = last_;
        if (!root_) return r;
        if (!horizon_ || terminal(*root_)) { r.status = Status::terminal; return r; }
        int e = recommend();
        if (e >= 0) {
            r.action = edges_[e].action;
            r.id = {generation_, e};
            r.provisional = !edges_[e].value.n;
            r.status = Status::ready;
        }
        if (include_candidates) {
            r.candidates.reserve(nodes_[0].edges.size());
            for (int id : nodes_[0].edges) {
                const auto& v = edges_[id].value;
                r.candidates.push_back({{generation_, id}, v.n, v.n ? std::optional<double>(v.mean) : std::nullopt});
            }
        }
        return r;
    }
    // 実ターンを進め、整合する完全観測の部分木だけを再利用する O(保存木のサイズ)
    bool advance(ActionId action, const Info& next, int horizon) {
        assert(root_ && action.generation == generation_);
        assert(std::find(nodes_[0].edges.begin(), nodes_[0].edges.end(), action.index) != nodes_[0].edges.end());
        int next_node = -1;
        if constexpr (!partial && comparable) {
            if (options_.tree && horizon == horizon_ - 1
                && (!options_.simulation_depth || options_.simulation_depth >= horizon_)) {
                const auto& value = key(model_, next);
                for (int child : edges_[action.index].children)
                    if (keys_[child - 1] == value) { next_node = child; break; }
            }
        }
        // 上限で候補が欠けた枝は、制限を新しい根へ引き継がず生成し直す
        if (next_node < 0 || nodes_[next_node].truncated || nodes_[next_node].edges.empty()) {
            reset(next, horizon); return false;
        }
        root_ = next;
        horizon_ = horizon;
        next_generation();
        pending_.reset(); trace_.clear();
        compact(next_node);
        fallback_ = nodes_[0].edges.front();
        if (options_.use_crn) crn_counts_.assign(nodes_[0].edges.size(), 0);
        last_ = {};
        return true;
    }
    // 論理状態を空にし大きい連続領域の容量を保持する O(保存状態のサイズ)
    void clear() {
        root_.reset(); pending_.reset();
        nodes_.clear(); edges_.clear(); keys_.clear(); crn_counts_.clear();
        action_buffer_.clear(); trace_.clear();
        fallback_ = -1;
        next_generation();
        last_ = {};
    }
};

// 候補を借用して一試行の評価ラムダだけで比較する 時間予算内の評価と O(候補数)
template<std::uniform_random_bit_generator Rng = MonteCarloNoise,
         std::ranges::random_access_range Range, class Evaluate>
auto mc_choose(const Range& candidates, Evaluate&& evaluate, int64_t time_us,
               bool use_crn = false, uint64_t seed = 1)
    -> std::optional<std::ranges::range_value_t<Range>> {
    using Value = std::ranges::range_value_t<Range>;
    using Clock = std::chrono::steady_clock;
    auto deadline = Clock::now() + std::chrono::microseconds(time_us);
    assert(time_us >= 0);
    if (std::ranges::empty(candidates)) return std::nullopt;
    if (time_us == 0 || std::ranges::size(candidates) == 1) return Value(*std::ranges::begin(candidates));
    struct Adapter {
        struct State {};
        using Action [[maybe_unused]] = int;
        const Range& candidates;
        Evaluate& evaluate;
        double step(State&, int index, Rng& rng, const MonteCarloNoise& context) {
            const auto& action = *(std::ranges::begin(candidates) + index);
            if constexpr (std::invocable<Evaluate&, decltype(action), Rng&, const MonteCarloNoise&>)
                return std::invoke(evaluate, action, rng, context);
            else return std::invoke(evaluate, action, rng);
        }
    } model{candidates, evaluate};
    using Planner = MonteCarloPlanner<Adapter, Rng>;
    typename Planner::Options options;
    options.seed = seed;
    options.use_crn = use_crn;
    Planner planner(model, options);
    planner.start(typename Adapter::State{}, 1, std::views::iota(0, int(std::ranges::size(candidates))));
    typename Planner::Limits limits;
    limits.deadline = deadline;
    planner.search(limits);
    auto selected = planner.best_action();
    return selected ? std::optional<Value>(*(std::ranges::begin(candidates) + *selected)) : std::nullopt;
}

#if __INCLUDE_LEVEL__ == 0
#include <sys/wait.h>
#include <unistd.h>

struct MonteCarloPlannerTests {
    inline static int checks = 0;
    static void check(bool ok, const char* text, int line) {
        ++checks;
        if (!ok) { std::cerr << "FAIL " << line << ": " << text << '\n'; std::abort(); }
    }
    static void close(double a, double b) {
        check(std::abs(a-b) <= 1e-9 * (1+std::abs(b)), "numeric comparison", __LINE__);
    }
    struct Bandit {
        struct State {};
        using Action = int;
        std::vector<double> means{0,1,2};
        double amplitude = 0;
        std::vector<std::pair<int,uint64_t>> log;
        std::vector<int> actions(const State&) const {
            std::vector<int> a(means.size()); std::iota(a.begin(),a.end(),0); return a;
        }
        double step(State&,int a,MonteCarloNoise& noise,const MonteCarloNoise& context) {
            log.emplace_back(a,context.scenario_seed());
            return means[a]+amplitude*(noise.uniform01()-0.5);
        }
    };
    struct NoHooks {
        struct State { explicit State(int x): value(x) {} int value; };
        using Action=std::vector<int>;
        void step(State& s,const Action& a,MonteCarloNoise&) { s.value += a[0]; }
        double finish_value(const State& s) const { return s.value; }
    };
    struct Tree {
        struct State { int turn=0, invested=0; bool operator==(const State&) const = default; };
        using Action=int;
        std::array<int,2> actions(const State&) const { return {0,1}; }
        double step(State& s,int a,MonteCarloNoise&) const {
            if(s.turn++==0) { s.invested=a; return -double(a); }
            return a*(s.invested?3.0:1.0);
        }
        template<class Rng> int rollout_action(const State&,int,Rng&) const { return 1; }
    };
    struct Partial {
        struct State { int hidden; int result=0; };
        struct Info { int observed=-1; bool done=false; bool operator==(const Info&) const = default; };
        using Action=int;
        mutable int samples=0;
        State sample_root(const Info&,MonteCarloNoise& noise) const {
            ++samples; return {int(noise.uniform_int(2)),0};
        }
        std::array<int,3> actions(const Info&) const { return {0,1,2}; }
        double step(State& s,Info& info,int a,MonteCarloNoise&) const {
            if(a==2 && info.observed<0) { info.observed=s.hidden; return -1; }
            info.done=true; s.result=(a==s.hidden?10:0); return 0;
        }
        template<class Rng> int rollout_action(const Info& info,int,Rng& rng) const {
            return info.observed<0?std::uniform_int_distribution<int>(0,1)(rng):info.observed;
        }
        bool terminal(const Info& info) const { return info.done; }
        double finish_value(const State& s) const { return s.result; }
        double leaf_value(const Info& i,int) const { return i.observed<0?5:10; }
    };
    struct Forms {
        struct State { int t=0; };
        using Action=int;
        mutable int short_calls=0,long_calls=0;
        int end_at=99;
        std::array<int,1> actions(const State&) const { ++short_calls; return {0}; }
        void actions(const State&,int remaining,std::vector<int>& out) const {
            ++long_calls; check(out.empty(),"out is empty",__LINE__); check(remaining>0,"remaining",__LINE__); out.push_back(0);
        }
        template<class Rng> double step(State& s,int,Rng&) const { ++s.t; return 1; }
        bool terminal(const State& s) const { return s.t>=end_at; }
        double finish_value(const State&) const { return 999; }
        double finish_value(const State&,MonteCarloFinishReason why) const {
            return why==MonteCarloFinishReason::horizon?8:4;
        }
        double leaf_value(const State&,int remaining) const { return remaining*2; }
    };
    struct Proposal {
        struct State {};
        using Action=int;
        int proposals=0;
        template<class Rng> std::optional<int> propose_action(const State&,int,int index,Rng&) {
            ++proposals; if(index==4) return std::nullopt; return index;
        }
        double step(State&,int a,MonteCarloNoise&) { return a; }
    };
    struct FakeClock {
        using duration=std::chrono::microseconds;
        using rep=duration::rep;
        using period=duration::period;
        using time_point=std::chrono::time_point<FakeClock>;
        static constexpr bool is_steady=true;
        inline static int64_t ticks=0;
        static time_point now() { return time_point(duration(ticks)); }
    };
    struct Timed : Forms {
        double step(State& s,int a,MonteCarloNoise& noise) const {
            FakeClock::ticks+=7; return Forms::step(s,a,noise);
        }
    };
    struct SlowRoot : Partial {
        State sample_root(const Info& i,MonteCarloNoise& n) const {
            FakeClock::ticks+=100; return Partial::sample_root(i,n);
        }
    };
    struct NonzeroRng {
        using result_type=uint32_t;
        uint32_t x;
        explicit NonzeroRng(uint64_t seed) : x(uint32_t(seed)) {}
        static constexpr uint32_t min() { return 17; }
        static constexpr uint32_t max() { return 32; }
        uint32_t operator()() { return 17+((x++)&15); }
    };
    struct Large {
        std::array<int,1024> data{};
        int id=0;
        inline static int copies=0;
        explicit Large(int x):id(x) {}
        Large(const Large& b):data(b.data),id(b.id) { ++copies; }
        Large(Large&&)=default;
    };
    struct Keyed : Tree {
        int tree_key(const State& s) const { return s.turn*2+s.invested; }
    };
    struct EqualTree : Tree {
        double step(State& s,int,MonteCarloNoise&) const { ++s.turn; return 0; }
    };
    struct Recorded {
        struct State {int turn=0,pos=0,first=-1; double sum=0;};
        using Action=int;
        std::vector<std::pair<int,double>> completed;
        std::vector<std::pair<int,uint64_t>> calls;
        std::array<int,3> actions(const State&) const {return {0,1,2};}
        std::pair<int,int> tree_key(const State& s) const {return {s.turn,s.pos};}
        double step(State& s,int a,MonteCarloNoise& n) {
            calls.emplace_back(a,n.scenario_seed());
            if(s.turn==0)s.first=a;
            double r=a*.5+int(n.uniform_int(3))-1;
            s.sum+=std::pow(.9,s.turn)*r;
            ++s.turn;s.pos=(s.pos+a)%5;
            return r;
        }
        double finish_value(const State& s) {
            double tail=.25*s.pos;
            completed.emplace_back(s.first,s.sum+std::pow(.9,s.turn)*tail);
            return tail;
        }
    };
    struct Potential {
        struct State { int turn=0, pos=0; bool operator==(const State&) const = default; };
        using Action=int;
        static double value(const State& s) { return 3*s.pos-s.turn+7; }
        std::array<int,3> actions(const State&) const { return {0,1,2}; }
        double step(State& s,int a,MonteCarloNoise& n) const {
            double before=value(s);
            ++s.turn; s.pos=(s.pos*2+a+int(n.uniform_int(3)))%11;
            return before-.5*value(s);
        }
        bool terminal(const State& s) const { return s.turn==6; }
        double finish_value(const State& s) const { return value(s); }
        double leaf_value(const State& s,int) const { return value(s); }
    };
    struct Copies {
        struct State {
            int turn=0;
            inline static int copies=0;
            State()=default;
            State(const State& s):turn(s.turn) { ++copies; }
            State(State&&)=default;
            State& operator=(const State&)=default;
        };
        using Action=int;
        std::array<int,1> actions(const State&) const { return {0}; }
        void step(State& s,int,MonteCarloNoise&) const { ++s.turn; }
    };
    template<class Planner> static auto work(int n) {
        typename Planner::Limits l; l.max_simulations=n; return l;
    }
    template<class F> static void dies(F f) {
#ifndef NDEBUG
        pid_t pid=fork();
        if(pid==0) { if (!freopen("/dev/null","w",stderr)) _exit(2); f(); _exit(0); }
        int status=0; waitpid(pid,&status,0);
        check(WIFSIGNALED(status)&&WTERMSIG(status)==SIGABRT,"contract assert",__LINE__);
#else
        (void)f;
#endif
    }
    static void run() {
#define MC_CHECK(x) check(bool(x),#x,__LINE__)
        static_assert(std::uniform_random_bit_generator<MonteCarloNoise>);
        static_assert(std::uniform_random_bit_generator<NonzeroRng>);
        static_assert(!std::is_constructible_v<MonteCarloPlanner<Bandit>,Bandit&&>);
        MonteCarloNoise n(42),same(42);
        for(int i=0;i<1000;++i) MC_CHECK(n()==same());
        auto stream=n.stream(3,4);
        for(int i=0;i<20;++i) close(stream.uniform01(),n.uniform01(3,4,i));
        for(int i=0;i<100;++i) { MC_CHECK(n.uniform01()>=0); MC_CHECK(n.uniform01()<1); MC_CHECK(n.uniform_int(7)<7); }
        auto custom=n.stream(1,2,[](uint64_t s){return std::mt19937_64(s);});
        auto custom2=n.stream(1,2,[](uint64_t s){return std::mt19937_64(s);});
        MC_CHECK(custom()==custom2());
        std::normal_distribution<double> normal;
        MC_CHECK(std::isfinite(normal(n)));

        // 最小 API と候補の所有・借用
        std::vector<int> empty;
        int calls=0;
        auto evaluate=[&](int a,auto&){++calls;return double(a);};
        MC_CHECK(!mc_choose(empty,evaluate,0));
        std::array<int,1> one{9};
        MC_CHECK(*mc_choose(one,evaluate,100)==9 && calls==0);
        std::array<int,3> choices{0,1,2};
        MC_CHECK(*mc_choose(choices,evaluate,0)==0 && calls==0);
        auto simple=mc_choose(std::span<const int>(choices),evaluate,1000);
        MC_CHECK(simple && *simple>=0 && *simple<=2);
        if(calls>=3)MC_CHECK(*simple==2);
        std::vector<Large> large; large.emplace_back(0); large.emplace_back(1); Large::copies=0;
        int large_calls=0;
        auto chosen=mc_choose(large,[&](const Large& a,auto&){++large_calls;return double(a.id);},1000);
        MC_CHECK(chosen && Large::copies==1);
        if(large_calls>=2)MC_CHECK(chosen->id==1);
        NoHooks nh; MonteCarloPlanner np(nh);
        np.start(NoHooks::State(5),1,std::array<std::vector<int>,2>{{{1},{3}}});
        np.search(work<decltype(np)>(32));
        MC_CHECK((*np.best_action())[0]==3);
        for(const auto& c:np.result().candidates) if(c.mean) MC_CHECK(*c.mean==6 || *c.mean==8);

        // 境界・追加・読み取りの副作用
        Bandit bandit; MonteCarloPlanner planner(bandit);
        MC_CHECK(!planner.best_action());
        planner.start(Bandit::State{},1,choices);
        MC_CHECK(planner.result().provisional && *planner.best_action()==0);
        planner.search(0); MC_CHECK(bandit.log.empty());
        auto done=planner.search(work<decltype(planner)>(30));
        MC_CHECK(done.simulations==30 && done.transitions==30);
        MC_CHECK(*planner.best_action()==2 && !planner.result().provisional);
        auto old_log=bandit.log;
        for(int i=0;i<10;++i) { planner.best_action(); planner.result(); }
        MC_CHECK(bandit.log==old_log);
        auto brief=planner.result(false);
        MC_CHECK(brief.candidates.empty() && brief.id==planner.result().id && brief.action==planner.best_action());
        auto oldid=planner.result().id;
        planner.clear(); MC_CHECK(!planner.best_action());
        planner.start(Bandit::State{},1,empty);
        MC_CHECK(planner.search(work<decltype(planner)>(1)).stop==decltype(planner)::Stop::not_ready);
        planner.add_root_action(1); planner.search(work<decltype(planner)>(3)); MC_CHECK(*planner.best_action()==1);
        planner.start(Bandit::State{},0,choices); MC_CHECK(!planner.best_action());
        MC_CHECK(planner.result().status==decltype(planner)::Status::terminal);
        dies([&]{planner.advance(oldid,{},0);});

        // 追加候補の参加順を変えても、完全同点の推薦は登録順にする
        for(bool crn:{false,true}) {
            Bandit tied; tied.means={0,0,0};
            using P=MonteCarloPlanner<Bandit>; P::Options o; o.use_crn=crn;
            P p(tied,o); p.start({},1,std::array<int,1>{0});
            auto added=p.add_root_action(1); p.search(work<P>(2));
            MC_CHECK(*p.best_action()==0 && p.result().id.index<added.index);
            p.add_root_action(2); p.search(work<P>(1));
            MC_CHECK(*p.best_action()==0);
        }

        // 各試行に根の State を一回だけコピーする
        Copies copied; MonteCarloPlanner copied_p(copied); copied_p.start({},4);
        Copies::State::copies=0;
        copied_p.search(work<decltype(copied_p)>(20));
        MC_CHECK(Copies::State::copies==20);

        // CRN の有無が配分を直接変えないことと第 k seed の一致
        Bandit off,on;
        using BP=MonteCarloPlanner<Bandit>;
        BP::Options onopt; onopt.use_crn=true;
        BP poff(off),pon(on,onopt);
        poff.start({},1); pon.start({},1);
        poff.search(work<BP>(200)); pon.search(work<BP>(200));
        MC_CHECK(off.log.size()==on.log.size());
        std::array<std::vector<uint64_t>,3> seeds;
        std::set<uint64_t> independent;
        for(size_t i=0;i<off.log.size();++i) {
            MC_CHECK(off.log[i].first==on.log[i].first);
            seeds[on.log[i].first].push_back(on.log[i].second);
            independent.insert(off.log[i].second);
        }
        MC_CHECK(independent.size()==off.log.size());
        for(int a=1;a<3;++a) for(size_t k=0;k<std::min(seeds[0].size(),seeds[a].size());++k) MC_CHECK(seeds[0][k]==seeds[a][k]);
        Bandit late; BP pl(late,onopt); pl.start({},1,std::array<int,1>{2}); pl.search(work<BP>(10));
        uint64_t firstseed=late.log.front().second;
        pl.add_root_action(1); pl.search(work<BP>(1));
        MC_CHECK(late.log.back().first==1 && late.log.back().second==firstseed);

        // 固定作業量の分割と各 URBG
        Forms fa,fb;
        MonteCarloPlanner pa(fa),pb(fb);
        pa.start({},5); pb.start({},5);
        decltype(pa)::Limits la; la.max_transitions=101;
        pa.search(la);
        decltype(pb)::Limits lb; lb.max_transitions=1;
        for(int i=0;i<101;++i) pb.search(lb);
        MC_CHECK(pa.result().candidates[0].trials==20);
        MC_CHECK(pa.result().candidates[0].trials==pb.result().candidates[0].trials);
        close(*pa.result().candidates[0].mean,*pb.result().candidates[0].mean);
        MC_CHECK(fa.short_calls==0 && fa.long_calls>0);
        [&]<class... Rngs>() {
            auto test = [&]<class R>() {
                Forms f; MonteCarloPlanner<Forms, R> p(f); p.start({}, 3);
                MC_CHECK(p.search(work<decltype(p)>(3)).simulations == 3);
            };
            (test.template operator()<Rngs>(), ...);
        }.template operator()<NonzeroRng, std::mt19937, std::mt19937_64>();
        MC_CHECK(pa.search(0).transitions==0);

        // 終端理由・割引・浅い打切り
        Forms ff; using FP=MonteCarloPlanner<Forms>;
        FP::Options fopt; fopt.discount=.5;
        FP fp(ff,fopt); fp.start({},3); fp.search(work<FP>(1)); close(*fp.result().candidates[0].mean,2.75);
        ff.end_at=2; fp.start({},3); fp.search(work<FP>(1)); close(*fp.result().candidates[0].mean,2.5);
        ff.end_at=99; fopt.simulation_depth=1;
        FP shallow(ff,fopt); shallow.start({},3); shallow.search(work<FP>(1)); close(*shallow.result().candidates[0].mean,3);
        Proposal proposal; MonteCarloPlanner pp(proposal); pp.start({},1); pp.search(work<decltype(pp)>(100));
        MC_CHECK(proposal.proposals==5 && *pp.best_action()==3);

        // 観測を使うロールアウトと木、実観測後のリセット
        for(bool tree:{false,true}) {
            Partial m; using P=MonteCarloPlanner<Partial>; P::Options opt; opt.tree=tree;
            P p(m,opt); p.start({},2); p.search(work<P>(1000));
            MC_CHECK(*p.best_action()==2);
            auto r=p.result(); MC_CHECK(m.samples==1000);
            MC_CHECK(!p.advance(r.id,Partial::Info{1,false},1));
            MC_CHECK(p.result().candidates.empty());
        }
        Partial m; using P=MonteCarloPlanner<Partial>; P::Options shallowp; shallowp.simulation_depth=1;
        P sp(m,shallowp); sp.start({},2); sp.search(work<P>(1000)); MC_CHECK(*sp.best_action()==2);

        // 完全観測の再利用と固定幅の再計画、メモリ上限
        Tree tree; using TP=MonteCarloPlanner<Tree>; TP::Options topt; topt.tree=true;
        TP tp(tree,topt); tp.start({},3); tp.search(work<TP>(500)); MC_CHECK(*tp.best_action()==1);
        auto tr=tp.result(); Tree::State next; MonteCarloNoise dummy; tree.step(next,*tr.action,dummy);
        MC_CHECK(tp.advance(tr.id,next,2));
        MC_CHECK(!tp.result().candidates.empty() && tp.result().candidates[0].trials>0);
        tp.search(work<TP>(100));
        auto tr2=tp.result(); tree.step(next,*tr2.action,dummy); MC_CHECK(!tp.advance(tr2.id,next,2));
        Keyed keyed; MonteCarloPlanner<Keyed>::Options ko; ko.tree=true;
        MonteCarloPlanner kp(keyed,ko); kp.start({},3); kp.search(work<decltype(kp)>(100)); MC_CHECK(*kp.best_action()==1);
        topt.max_nodes=2; topt.max_edges=8;
        TP tiny(tree,topt); tiny.start({},8); tiny.search(work<TP>(100)); MC_CHECK(tiny.best_action().has_value());

        // 空になった子と、一部の候補しか保存できなかった子の両方を生成し直す
        for(bool crn:{false,true}) for(int cap:{2,3}) {
            EqualTree model; using P=MonteCarloPlanner<EqualTree>; P::Options o;
            o.tree=true; o.use_crn=crn; o.max_edges=cap;
            P p(model,o); p.start({},3); p.search(work<P>(20));
            auto id=p.result().candidates.front().id;
            MC_CHECK(!p.advance(id,EqualTree::State{1,0},2));
            MC_CHECK(p.search(work<P>(20)).simulations==20);
            MC_CHECK(p.result().candidates.size()==2);
            MC_CHECK(p.best_action().has_value());
        }

        // 望遠和により正解が各状態の value になるモデルで、部分木の局所値を検査
        // 確率遷移・割引・自然終了・浅い近似・上限・CRN・繰返し advance を組み合わせる
        for(int seed=0;seed<12;++seed) for(bool use_tree:{false,true})
        for(bool crn:{false,true}) for(int depth:{0,3}) for(int cap:{2,9,128}) {
            Potential model; using P=MonteCarloPlanner<Potential>; P::Options o;
            o.seed=seed; o.tree=use_tree; o.use_crn=crn; o.discount=.5;
            o.simulation_depth=depth; o.max_edges=cap; o.max_nodes=16;
            P p(model,o); Potential::State state; MonteCarloNoise world(seed+700);
            p.start(state,8);
            for(int h=8;!model.terminal(state);--h) {
                MC_CHECK(p.search(work<P>(40)).simulations==40);
                auto r=p.result(); MC_CHECK(r.action.has_value());
                for(const auto& c:r.candidates) if(c.mean) close(*c.mean,model.value(state));
                model.step(state,*r.action,world);
                p.advance(r.id,state,h-1);
            }
            MC_CHECK(!p.best_action());
        }

        // 独立に記録した割引和との突合と、中断境界に依存しない乱数・行動列
        for(bool use_tree:{false,true}) for(bool crn:{false,true}) {
            Recorded a,b;using RP=MonteCarloPlanner<Recorded>;RP::Options o;
            o.tree=use_tree;o.use_crn=crn;o.discount=.9;
            RP x(a,o),y(b,o);x.start({},7);y.start({},7);
            RP::Limits l;l.max_transitions=4003;x.search(l);
            for(int i=0;i<4003;++i){l.max_transitions=1;y.search(l);if(i%11==0){y.best_action();y.result();}}
            MC_CHECK(a.calls==b.calls && a.completed==b.completed);
            std::array<int,3> counts{};std::array<double,3> sums{};
            for(auto [action,total]:a.completed){++counts[action];sums[action]+=total;}
            auto r=x.result();
            for(int i=0;i<3;++i){MC_CHECK(r.candidates[i].trials==counts[i]);close(*r.candidates[i].mean,sums[i]/counts[i]);}
        }
        Recorded restricted;MonteCarloPlanner limited(restricted);
        limited.start({},4,std::array<int,1>{2});
        limited.search(work<decltype(limited)>(50));
        bool other=false;
        for(size_t i=0;i<restricted.calls.size();++i){
            if(i%4==0)MC_CHECK(restricted.calls[i].first==2);
            else other|=restricted.calls[i].first!=2;
        }
        MC_CHECK(other); // 外部候補による制限は根にだけ適用される
        // 多数回の部分木切替で行動 ID と保持統計の整合を検査する
        for(int seed=0;seed<20;++seed){
            Tree model;TP::Options o;o.tree=true;o.seed=seed;TP p(model,o);
            Tree::State state;p.start(state,12);MonteCarloNoise n(seed);
            for(int h=12;h>0;--h){
                p.search(work<TP>(200));auto r=p.result();MC_CHECK(r.action.has_value());
                for(auto& c:r.candidates)if(c.mean)MC_CHECK(std::isfinite(*c.mean));
                model.step(state,*r.action,n);
                if(h>1)p.advance(r.id,state,h-1);
            }
        }
        TP::Options full_depth;full_depth.tree=true;full_depth.simulation_depth=5;
        TP reuse_full(tree,full_depth);reuse_full.start({},3);reuse_full.search(work<TP>(500));
        auto full_result=reuse_full.result();Tree::State full_next;
        tree.step(full_next,*full_result.action,dummy);
        MC_CHECK(reuse_full.advance(full_result.id,full_next,2));
        Bandit scaled;scaled.means={-1e12,-1e12+1e6,-1e12+2e6};
        BP::Options known_scale;known_scale.value_scale=1e6;
        BP sc(scaled,known_scale);sc.start({},1);sc.search(work<BP>(100));
        MC_CHECK(*sc.best_action()==2);
        auto counts=sc.result().candidates;
        MC_CHECK(counts[2].trials>counts[0].trials); // 有望な候補へ追加評価を集中
        Bandit many;many.means.assign(1000,0);BP sparse(many);sparse.start({},1);
        sparse.search(work<BP>(5));int evaluated=0;
        for(const auto& c:sparse.result().candidates)evaluated+=c.trials;
        MC_CHECK(evaluated==5); // 全候補の初回評価を強制しない
        // 深さ指定より残り手数が短いときも本来の終端評価を使う
        fopt.simulation_depth=5;FP short_h(ff,fopt);short_h.start({},2);
        short_h.search(work<FP>(1));close(*short_h.result().candidates[0].mean,3.5);

        // 制御時計で中断・再開と長い根生成を検査
        FakeClock::ticks=0; Timed tm; using TimedP=MonteCarloPlanner<Timed,MonteCarloNoise,FakeClock>;
        TimedP::Options tc; tc.clock_interval=1; TimedP timed(tm,tc); timed.start({},3);
        auto t=timed.search(10); MC_CHECK(t.transitions==2 && t.simulations==0 && t.elapsed_us==14);
        t=timed.search(7); MC_CHECK(t.transitions==1 && t.simulations==1);
        FakeClock::ticks=0; SlowRoot slow; using SlowP=MonteCarloPlanner<SlowRoot,MonteCarloNoise,FakeClock>;
        SlowP slowp(slow); slowp.start({},2,std::array<int,1>{2});
        auto sr=slowp.search(10); MC_CHECK(sr.started==1 && sr.transitions==0 && slow.samples==1);
        slowp.search(work<SlowP>(1)); MC_CHECK(slow.samples==1);
        TimedP::Limits expired; expired.deadline=FakeClock::time_point{};
        MC_CHECK(timed.search(expired).transitions==0);
        dies([&]{Bandit b; BP p(b); p.search(1);});
        dies([&]{BP::Options o; o.tree=true; Bandit b; BP p(b,o);});
        dies([&]{Forms f; FP p(f); p.start({},1); p.search(typename FP::Limits{});});
        dies([&]{MonteCarloNoise n;n.uniform_int(0);});
        dies([&]{BP::Options o;o.simulation_depth=1;Bandit b;BP p(b,o);p.start({},2);p.search(work<BP>(1));});
        std::cout << "PASS " << checks << " checks\n";
#undef MC_CHECK
    }
};
struct ReviewChecks {
    inline static int checks=0;
    static void check(bool x){++checks;if(!x)std::abort();}
    struct Key {
        int id=0;
        inline static int copies=0;
        Key()=default;
        Key(const Key& x):id(x.id){++copies;}
        Key(Key&&) noexcept=default;
        Key& operator=(const Key&)=default;
        Key& operator=(Key&&) noexcept=default;
        bool operator==(const Key&)const=default;
    };
    struct ReferenceKey {
        struct State {int t=0;};
        using Action=int;
        std::array<Key,3> keys;
        int queries=0;
        mutable int terminal_calls=0;
        ReferenceKey(){for(int i=0;i<3;++i)keys[i].id=i;}
        const Key& tree_key(const State& s){++queries;return keys[s.t];}
        std::array<int,1> actions(const State&)const{return {0};}
        double step(State& s,int,MonteCarloNoise&)const{++s.t;return 1;}
        bool terminal(const State& s)const{++terminal_calls;return s.t==2;}
    };
    struct ValueKey : ReferenceKey {
        Key tree_key(const State& s){++queries;return keys[s.t];}
    };
    template<class M> static void key_test(bool reference){
        M m;using P=MonteCarloPlanner<M>;typename P::Options o;o.tree=true;P p(m,o);
        p.start({},2);typename P::Limits l;l.max_simulations=50;Key::copies=0;
        check(p.search(l).simulations==50);
        std::cout<<(reference?"reference":"value")<<"_key queries="<<m.queries<<" copies="<<Key::copies<<'\n';
        check(Key::copies==(reference?1:m.queries+1));
        check(*p.result().candidates[0].mean==2);
        auto id=p.result(false).id;
        check(p.advance(id,{1},1));
        check(*p.result().candidates[0].mean==1);
        check(p.search(l).simulations==50);
        check(*p.result().candidates[0].mean==1);
    }
    static void run(){
        key_test<ReferenceKey>(true);key_test<ValueKey>(false);
        ReferenceKey model;using P=MonteCarloPlanner<ReferenceKey>;P p(model);
        p.start({},1,std::array<int,1>{0});model.terminal_calls=0;
        p.result(false);std::cout<<"result_terminal_calls="<<model.terminal_calls<<'\n';
        check(model.terminal_calls==1);
        model.terminal_calls=0;p.best_action();check(model.terminal_calls==1);
        std::cout<<"PASS "<<checks<<" review checks\n";
    }
};

int main() { MonteCarloPlannerTests::run(); ReviewChecks::run(); }
#endif
