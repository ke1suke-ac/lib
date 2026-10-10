#pragma once
#include <bits/stdc++.h>

// 連結な領域へのグラフ分割を、容量・接触・固定条件付きで改善する C++20 単一ヘッダ。
// 最小化。既定は境界コスト。独自モデルの関数は対応する既定項を置き換える。
// α:逆Ackermann関数。N:頂点数、M:辺数、K:領域数、D:資源次元、Q:特徴次元、P:登録ペア数、A:許可集合総要素数、B:均衡項数、G:最大次数。
// Cost は double / int64_t。ID・要素数は int、整数の集計・差分・積は int64_t に収める。
// double の入力・評価値は有限値。自己ループは禁止、平行辺・負コスト・符号付き資源は可。
// 実数の差分集計には丸め誤差がある。目的値の厳密比較には int64_t を使う。
// 独自関数は決定的で例外を投げない。load は非合法な割当でも呼ばれる。
// 例外を明示的には投げない。形式不正は assert、合法解未発見は best_cost == nullopt。
// 時間・配列・中間演算は通常のAHC規模を想定し、世代番号の増分合計は各2^32未満とする。
// 同一インスタンスは単一スレッドで使用。ビューは次の変更操作まで（モデルのビューは呼出し中のみ）。
template<class Cost = double>
class ConnectedPartitionSolver {
    static_assert(std::is_same_v<Cost, double> || std::is_same_v<Cost, int64_t>);
    struct State;
    struct DefaultModel {};
public:
    using Clock = std::chrono::steady_clock;
    struct IntRange { int64_t lower = INT64_MIN, upper = INT64_MAX; };
    enum EdgeRole : unsigned { Connect = 1u, Contact = 2u, PairCost = 4u };
    struct Edge {
        int u, v;
        Cost cut_cost = Cost{1};
        double length = 1;
        bool enabled = true;
        unsigned roles = Connect | Contact | PairCost;
    };
    struct RegionRule {
        bool required = true, connected = true;
        int min_vertices = 1, max_vertices = INT_MAX;
        std::vector<IntRange> load_bounds;
    };
    struct LabelDomain { int vertex; std::vector<int> labels; };
    struct ContactRule { int a, b; int min_edges = 0, max_edges = INT_MAX; };
    struct BalanceTerm { int region, dim; Cost target, coefficient; };
    struct Problem {
        int n, k;
        std::vector<Edge> edges;
        std::vector<RegionRule> regions;
        int load_dim = 0, feature_dim = 0;
        std::vector<int64_t> load;
        std::vector<double> feature;
        std::vector<int> fixed;
        std::vector<LabelDomain> domains;
        std::vector<ContactRule> contacts;
        bool allow_unlisted_contacts = true;
        int min_active_regions = 0, max_active_regions = INT_MAX;
        std::vector<Cost> unary_cost, activation_cost;
        std::vector<BalanceTerm> balance_terms;
        // 領域の既定条件を用意する。O(k)。n >= 0, k > 0。
        explicit Problem(int vertices, int labels) : n(vertices), k(labels) {
            assert(n >= 0 && k > 0);
            regions.resize(k);
        }
    };
    struct RegionStatsView {
        int vertices;
        std::span<const int64_t> load;
        std::span<const double> feature;
        double boundary;
    };
    struct ContactStats { int edges = 0; double length = 0; };
    class SummaryView {
        const ConnectedPartitionSolver* owner_;
        const State* state_;
        friend class ConnectedPartitionSolver;
        SummaryView(const ConnectedPartitionSolver* o, const State* s) : owner_(o), state_(s) {}
    public:
        // 頂点数を返す。O(1)。
        int size() const { return owner_->p_.n; }
        // 領域数を返す。O(1)。
        int region_count() const { return owner_->p_.k; }
        // 完全な候補割当のラベルを返す。O(1)。
        int label(int v) const { assert(0 <= v && v < size()); return state_->label[v]; }
        // 使用領域数を返す。O(1)。
        int active_regions() const { return state_->active; }
        // 領域集計の一時ビューを返す。O(1)。
        RegionStatsView region(int r) const { return owner_->region_view(*state_, r); }
        // 登録した接触ペア数を返す。O(1)。
        int contact_count() const { return int(state_->contact.size()); }
        // 入力 contacts と同じ添字の接触集計を返す。O(1)。
        ContactStats contact(int i) const { assert(0 <= i && i < int(state_->contact.size())); return state_->contact[i]; }
    };
    enum class Acceptance { greedy, annealing };
    enum Neighborhood { Move, Swap, Block, Relabel, Recombine, NeighborhoodCount };
    struct Options {
        int64_t budget_us = 1'000'000, max_steps = -1;
        std::optional<Clock::time_point> deadline;
        uint64_t seed = 0;
        std::optional<std::span<const int>> movable;
        Acceptance acceptance = Acceptance::annealing;
        std::array<int, NeighborhoodCount> weights{60, 18, 12, 2, 7};
        double temperature = 0; // 0: 正の差分から自動推定。正値: 初期温度を指定。
        double final_temperature_ratio = 0.02;
        bool profile = false; // 近傍別時間を測る。通常は計時の費用を避ける。
    };
    enum class StopReason { time_limit, step_limit, no_moves, invalid_initial };
    struct NeighborhoodStats {
        // feasible は合法と確認した候補数。目的値で早期棄却した未検査候補は数えない。
        int64_t proposed = 0, feasible = 0, accepted = 0, improved = 0, elapsed_ns = 0;
    };
    struct Statistics {
        int64_t steps = 0, construction_attempts = 0, elapsed_us = 0;
        std::array<NeighborhoodStats, NeighborhoodCount> neighborhood{};
    };
    struct Evaluation { bool feasible = false; std::optional<Cost> cost; };
    struct Result { std::optional<Cost> best_cost; StopReason reason; Statistics statistics; };

    // 問題を所有して索引を用意する。O(入力サイズ + M + A log A + P log P)。
    explicit ConnectedPartitionSolver(Problem problem) : p_(std::move(problem)) { prepare(false); }

    // 全体から合法性・評価値を計算する。O(N(1+D+Q+log(A+1))+M(α(N)+log(P+1))+K(1+D+Q)+B+モデル費用)。
    template<class Model = DefaultModel>
    Evaluation evaluate(std::span<const int> labels, const Model& model = {}) const {
        auto s = assemble<false>(labels, model);
        return {s.feasible, s.feasible ? std::optional<Cost>(s.cost) : std::nullopt};
    }

    // 初期解を改善する。O(初期評価 + 試行数×候補評価)。非合法なら探索しない。
    template<class Model = DefaultModel>
    Result improve(std::span<const int> labels, const Options& options = {}, const Model& model = {}) {
        Run run(options);
        start(labels, options, model);
        if (!current_.feasible) return finish(run, StopReason::invalid_initial);
        search(run, model);
        return finish(run, run.reason());
    }

    // 保持状態から続行する。標準モデル・movable未指定・変更なしなら前処理 O(1)、他は全体評価や集合整列。
    template<class Model = DefaultModel>
    Result resume(const Options& options = {}, const Model& model = {}) {
        Run run(options);
        assert(has_candidate_);
        // 外側は前回の返却候補を基準に固定する。モデル変更時は候補を再評価する。
        bool changed = set_scope(options);
        bool reset = changed && !respects_scope(current_.label, best_);
        constexpr bool standard = std::is_same_v<Model, DefaultModel>;
        if (!ready_ || !standard || !standard_model_ || reset) {
            auto best_state = assemble(best_, model);
            // 同じラベルなら複製せず、下の選択で best_state を直接引き継ぐ。
            State now = (reset || current_is_best_) ? State{} : assemble(current_.label, model);
            best_cost_.reset();
            if (best_state.feasible) best_cost_ = best_state.cost;
            if (now.feasible && (!best_cost_ || now.cost <= *best_cost_)) {
                best_ = now.label; best_cost_ = now.cost;
            }
            current_ = now.feasible ? std::move(now) : std::move(best_state);
            ready_ = current_.feasible;
            current_is_best_ = ready_ && current_.cost == *best_cost_;
            standard_model_ = standard;
            scale_ = 0;
        }
        if (!current_.feasible) return finish(run, StopReason::invalid_initial);
        search(run, model);
        return finish(run, run.reason());
    }

    // 完全な割当を出発点に修復後、残予算で改善する。修復1試行は全体評価と同じ計算量。
    template<class Model = DefaultModel>
    Result repair(std::span<const int> labels, const Options& options = {}, const Model& model = {}) {
        Run run(options);
        start(labels, options, model, true);
        restore_feasibility(run, model, false);
        if (current_.feasible) search(run, model);
        return finish(run, run.reason());
    }

    // 更新前などから保持した候補を修復する。計算量は repair(labels, ...) と同じ。
    template<class Model = DefaultModel>
    Result repair(const Options& options = {}, const Model& model = {}) {
        assert(has_candidate_);
        return repair(std::span<const int>(best_), options, model);
    }

    // 合法解の構築・修復後に別構築と比較して改善。1構築 O((NK+M)(1+log(A+1))+(N+M)log(K+1)+MG+全体評価)。
    template<class Model = DefaultModel>
    Result solve(const Options& options = {}, const Model& model = {}) {
        assert(!options.movable);
        Run run(options);
        // 新しい構築から開始し、既存の評価値は混ぜない。
        rng_ = options.seed; scale_ = 0;
        set_scope(options, true);
        has_candidate_ = true; best_cost_.reset();
        best_ = grow(model);
        current_ = assemble(best_, model);
        standard_model_ = std::is_same_v<Model, DefaultModel>;
        ready_ = current_.feasible;
        if (current_.feasible) best_cost_ = current_.cost;
        current_is_best_ = current_.feasible;
        ++run.stats.construction_attempts;
        restore_feasibility(run, model, true);
        // 追加構築は合法解の確保後だけ行う。未発見の段階では修復を優先する。
        if (current_.feasible && !run.stop()) {
            auto alternative = assemble(grow(model, true), model);
            ++run.stats.construction_attempts;
            if (!run.expired() && alternative.feasible && alternative.cost < current_.cost) {
                current_ = std::move(alternative); best_ = current_.label;
                best_cost_ = current_.cost; current_is_best_ = true;
            }
        }
        if (current_.feasible) search(run, model);
        return finish(run, run.reason());
    }

    // 問題を更新し、互換なラベル候補と同じ端点列のCSRを再利用する。O(入力サイズ + 索引構築)。
    void update(Problem problem) {
        // CSRの形は端点列だけで決まり、役割・有効状態の変更では作り直さない。
        bool same = p_.n == problem.n && p_.edges.size() == problem.edges.size();
        if (same) for (size_t i = 0; i < p_.edges.size(); ++i)
            if (p_.edges[i].u != problem.edges[i].u || p_.edges[i].v != problem.edges[i].v) { same = false; break; }
        if (p_.n != problem.n || p_.k != problem.k) { current_is_best_ = false; has_candidate_ = false; best_.clear(); current_ = {}; }
        p_ = std::move(problem);
        best_cost_.reset(); ready_ = false; scope_ready_ = false; scale_ = 0;
        prepare(same);
    }

    // 確認済み最良解をコピーせず参照する。O(1)。次の変更操作まで有効。
    std::span<const int> best_labels() const { assert(best_cost_); return best_; }

private:
    struct State {
        std::vector<int> label, count, position;
        std::vector<std::vector<int>> members;
        std::vector<int64_t> load;
        std::vector<double> feature, boundary;
        std::vector<ContactStats> contact;
        int active = 0, unlisted = 0;
        double violation = 0;
        Cost cost = 0, global = 0;
        bool feasible = false;
    };
    struct Run {
        const Options& options;
        Clock::time_point start = Clock::now(), end = Clock::time_point::max();
        Statistics stats;
        bool timed_out = false, exhausted = false;
        explicit Run(const Options& o) : options(o) {
            // 指定した停止条件を検査し、相対期限と絶対期限の早い方を使う。
            assert(o.budget_us >= -1 && o.max_steps >= -1);
            assert(o.budget_us >= 0 || o.max_steps >= 0 || o.deadline);
            assert(o.temperature >= 0 && o.final_temperature_ratio > 0 && o.final_temperature_ratio <= 1);
            for (int w : o.weights) { assert(w >= 0); (void)w; }
            if (o.budget_us >= 0) end = start + std::chrono::microseconds(o.budget_us);
            if (o.deadline) end = std::min(end, *o.deadline);
        }
        bool expired() { return timed_out = timed_out || (end != Clock::time_point::max() && Clock::now() >= end); }
        bool stop() { return expired() || (options.max_steps >= 0 && stats.steps >= options.max_steps); }
        double progress() const {
            // 時間と試行数のうち、先に上限へ近づいている方を冷却に使う。
            double t = 0;
            if (end != Clock::time_point::max()) {
                auto total = std::chrono::duration<double>(end - start).count();
                t = total <= 0 ? 1 : std::chrono::duration<double>(Clock::now() - start).count() / total;
            }
            if (options.max_steps >= 0) t = std::max(t, double(stats.steps) / double(std::max<int64_t>(1, options.max_steps)));
            return std::clamp(t, 0.0, 1.0);
        }
        StopReason reason() const {
            return timed_out ? StopReason::time_limit : exhausted ? StopReason::no_moves : StopReason::step_limit;
        }
    };
    struct Change { int vertex, to, from = -1; };
    struct Arc { int edge, to; };
    Problem p_;
    State current_;
    std::vector<int> offsets_, domain_index_;
    std::vector<Arc> adjacency_;
    std::vector<std::pair<uint64_t, int>> contact_index_;
    std::vector<std::vector<int>> balance_index_;
    std::vector<int> best_, free_, scope_;
    std::vector<unsigned char> movable_, free_mask_;
    std::optional<Cost> best_cost_;
    bool has_candidate_ = false, ready_ = false, standard_model_ = true, current_is_best_ = false;
    bool scope_ready_ = false, limited_scope_ = false;
    uint64_t rng_ = 0;
    double scale_ = 0;
    // 候補の作業領域。集計の旧値を保持して、取消時には丸め誤差を残さず戻す。
    std::vector<Change> changes_;
    std::vector<int> touched_, touched_pairs_, incident_, queue_, block_, tree_, subtree_, entry_;
    std::vector<uint32_t> vertex_mark_, edge_mark_, region_mark_, pair_mark_, seen_;
    uint32_t epoch_ = 0, visit_ = 0;
    std::vector<int> old_label_, old_count_;
    std::vector<int64_t> old_load_;
    std::vector<double> old_feature_, old_boundary_;
    std::vector<ContactStats> old_contact_;

    uint64_t random() {
        uint64_t z = (rng_ += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }
    int pick(int n) { assert(n > 0); return int(random() % uint64_t(n)); }
    double uniform() { return double(random() >> 11) * 0x1.0p-53; }
    bool role(int e, unsigned r) const { return p_.edges[e].roles & r; }
    static uint64_t pair_key(int a, int b) {
        if (a > b) std::swap(a, b);
        return (uint64_t(uint32_t(a)) << 32) | uint32_t(b);
    }
    int pair_index(int a, int b) const {
        auto key = pair_key(a, b);
        auto it = std::lower_bound(contact_index_.begin(), contact_index_.end(), std::pair<uint64_t, int>{key, -1});
        return it != contact_index_.end() && it->first == key ? it->second : -1;
    }
    bool allowed(int v, int r) const {
        if (!p_.fixed.empty() && p_.fixed[v] >= 0 && p_.fixed[v] != r) return false;
        int i = domain_index_[v];
        return i < 0 || std::binary_search(p_.domains[i].labels.begin(), p_.domains[i].labels.end(), r);
    }
    void assert_labels(std::span<const int> x) const {
        assert(int(x.size()) == p_.n);
        for (int r : x) { assert(0 <= r && r < p_.k); (void)r; }
    }
    void prepare(bool same_topology) {
        // サイズやIDのみを確認する。可行性は evaluate / 探索側で判定する。
        assert(p_.n >= 0 && p_.k > 0 && int(p_.regions.size()) == p_.k);
        assert(p_.load_dim >= 0 && p_.feature_dim >= 0);
        assert(p_.load.empty() || p_.load.size() == size_t(p_.n) * p_.load_dim);
        assert(p_.feature.empty() || p_.feature.size() == size_t(p_.n) * p_.feature_dim);
        assert(p_.unary_cost.empty() || p_.unary_cost.size() == size_t(p_.n) * p_.k);
        assert(p_.activation_cost.empty() || int(p_.activation_cost.size()) == p_.k);
        assert(p_.fixed.empty() || int(p_.fixed.size()) == p_.n);
        assert(0 <= p_.min_active_regions && p_.min_active_regions <= p_.max_active_regions);
        for (int r : p_.fixed) { assert(-1 <= r && r < p_.k); (void)r; }
        for (auto& r : p_.regions) {
            assert(r.min_vertices >= 0 && r.min_vertices <= r.max_vertices);
            assert(r.load_bounds.empty() || int(r.load_bounds.size()) == p_.load_dim);
            for (auto b : r.load_bounds) { assert(b.lower <= b.upper); (void)b; }
        }
        domain_index_.assign(p_.n, -1);
        for (int i = 0; i < int(p_.domains.size()); ++i) {
            auto& d = p_.domains[i];
            assert(0 <= d.vertex && d.vertex < p_.n && domain_index_[d.vertex] < 0);
            domain_index_[d.vertex] = i;
            std::sort(d.labels.begin(), d.labels.end());
            d.labels.erase(std::unique(d.labels.begin(), d.labels.end()), d.labels.end());
            for (int r : d.labels) { assert(0 <= r && r < p_.k); (void)r; }
        }
        contact_index_.clear();
        for (int i = 0; i < int(p_.contacts.size()); ++i) {
            const auto& c = p_.contacts[i];
            assert(0 <= c.a && c.a < p_.k && 0 <= c.b && c.b < p_.k && c.a != c.b);
            assert(0 <= c.min_edges && c.min_edges <= c.max_edges);
            contact_index_.push_back({pair_key(c.a, c.b), i});
        }
        std::sort(contact_index_.begin(), contact_index_.end());
        for (size_t i = 1; i < contact_index_.size(); ++i) assert(contact_index_[i - 1].first != contact_index_[i].first);
        balance_index_.assign(p_.k, {});
        for (int i = 0; i < int(p_.balance_terms.size()); ++i) {
            auto b = p_.balance_terms[i];
            assert(0 <= b.region && b.region < p_.k && 0 <= b.dim && b.dim < p_.load_dim);
            balance_index_[b.region].push_back(i);
        }
        // CSR は辺の有効状態・役割に依存しないため、端点が同じなら再利用できる。
        if (!same_topology) {
            offsets_.assign(p_.n + 1, 0);
            for (auto& e : p_.edges) {
                assert(0 <= e.u && e.u < p_.n && 0 <= e.v && e.v < p_.n && e.u != e.v);
                ++offsets_[e.u + 1]; ++offsets_[e.v + 1];
            }
            std::partial_sum(offsets_.begin(), offsets_.end(), offsets_.begin());
            adjacency_.resize(2 * p_.edges.size());
            auto at = offsets_;
            for (int i = 0; i < int(p_.edges.size()); ++i) {
                adjacency_[at[p_.edges[i].u]++] = {i, p_.edges[i].v}; adjacency_[at[p_.edges[i].v]++] = {i, p_.edges[i].u};
            }
        }
        for (auto& e : p_.edges) {
            assert(e.length >= 0 && std::isfinite(e.length) && e.roles < 8);
            if (!e.enabled) e.roles = 0; // 所有するコピーを正規化。update時も新入力から再実行する。
        }
        vertex_mark_.assign(p_.n, 0); edge_mark_.assign(p_.edges.size(), 0);
        region_mark_.assign(p_.k, 0); pair_mark_.assign(p_.contacts.size(), 0); seen_.assign(p_.n, 0);
        epoch_ = visit_ = 0;
        old_label_.resize(p_.n); old_count_.resize(p_.k); old_boundary_.resize(p_.k);
        old_load_.resize(size_t(p_.k) * p_.load_dim); old_feature_.resize(size_t(p_.k) * p_.feature_dim);
        old_contact_.resize(p_.contacts.size()); subtree_.resize(p_.n); entry_.resize(p_.n);
    }
    RegionStatsView region_view(const State& s, int r) const {
        assert(0 <= r && r < p_.k);
        return {s.count[r], std::span(s.load).subspan(size_t(r) * p_.load_dim, p_.load_dim),
            std::span(s.feature).subspan(size_t(r) * p_.feature_dim, p_.feature_dim), s.boundary[r]};
    }
    template<class Model> int64_t load(const Model& m, int v, int r, int d) const {
        if constexpr (requires { m.load(v, r, d); }) return m.load(v, r, d);
        else return p_.load.empty() ? 0 : p_.load[size_t(v) * p_.load_dim + d];
    }
    template<class Model> Cost vertex_cost(const Model& m, int v, int r) const {
        if constexpr (requires { m.vertex_cost(v, r); }) return m.vertex_cost(v, r);
        else return p_.unary_cost.empty() ? Cost{0} : p_.unary_cost[size_t(v) * p_.k + r];
    }
    template<class Model> Cost edge_cost(const Model& m, int e, int a, int b) const {
        if constexpr (requires { m.edge_cost(e, a, b); }) return m.edge_cost(e, a, b);
        else return a == b ? Cost{0} : p_.edges[e].cut_cost;
    }
    template<class Model> Cost region_cost(const Model& m, int r, const RegionStatsView& v) const {
        if constexpr (requires { m.region_cost(r, v); }) return m.region_cost(r, v);
        else {
            if (!v.vertices) return 0;
            Cost c = p_.activation_cost.empty() ? Cost{0} : p_.activation_cost[r];
            for (int i : balance_index_[r]) {
                auto b = p_.balance_terms[i]; Cost x = Cost(v.load[b.dim]) - b.target;
                c += b.coefficient * x * x;
            }
            return c;
        }
    }
    template<class Model> Cost global_cost(const Model& m, const State& s) const {
        if constexpr (requires { m.global_cost(SummaryView(this, &s)); }) return m.global_cost(SummaryView(this, &s));
        else return 0;
    }
    template<class Model> bool extra_feasible(const Model& m, const State& s, Run* run = nullptr) const {
        if constexpr (requires { m.extra_feasible(SummaryView(this, &s)); })
            return m.extra_feasible(SummaryView(this, &s)) && (!run || !run->expired());
        else return true;
    }
    double region_violation(const State& s, int r) const {
        const auto& rule = p_.regions[r];
        int n = s.count[r];
        if (!n) return rule.required ? std::max(1, rule.min_vertices) : 0;
        double v = std::max(0, rule.min_vertices - n) + std::max(0, n - rule.max_vertices);
        // 合法性の比較は整数で行い、修復用の大きさだけを実数へ変換する。
        for (int d = 0; d < int(rule.load_bounds.size()); ++d) {
            auto b = rule.load_bounds[d]; int64_t w = s.load[size_t(r) * p_.load_dim + d];
            if (w < b.lower) v += std::max(1.0, double(b.lower) - double(w)) / std::max(1.0, std::abs(double(b.lower)) / n);
            if (w > b.upper) v += std::max(1.0, double(w) - double(b.upper)) / std::max(1.0, std::abs(double(b.upper)) / n);
        }
        return v;
    }
    template<bool TrackMembers = true, class Model> State assemble(std::span<const int> x, const Model& m, Run* run = nullptr) const {
        assert_labels(x);
        State s;
        s.label.assign(x.begin(), x.end()); s.count.assign(p_.k, 0);
        s.load.assign(size_t(p_.k) * p_.load_dim, 0); s.feature.assign(size_t(p_.k) * p_.feature_dim, 0);
        s.boundary.assign(p_.k, 0); s.contact.resize(p_.contacts.size());
        // evaluateだけでは探索用の所属索引を作らない。
        if constexpr (TrackMembers) { s.members.resize(p_.k); s.position.resize(p_.n); }
        // 完全な割当から集計する。独自コストは合法性の確認後にだけ呼ぶ。
        for (int v = 0; v < p_.n; ++v) {
            if (run && (v & 255) == 0 && run->expired()) return s;
            int r = x[v]; ++s.count[r]; s.violation += !allowed(v, r);
            if constexpr (TrackMembers) { s.position[v] = int(s.members[r].size()); s.members[r].push_back(v); }
            for (int d = 0; d < p_.load_dim; ++d) s.load[size_t(r) * p_.load_dim + d] += load(m, v, r, d);
            if (!p_.feature.empty()) for (int d = 0; d < p_.feature_dim; ++d)
                s.feature[size_t(r) * p_.feature_dim + d] += p_.feature[size_t(v) * p_.feature_dim + d];
        }
        // 独自モデルへ切り替えるresumeでは集計し直すため、未参照の接触量は不要。
        if (!std::is_same_v<Model, DefaultModel> || !p_.contacts.empty() || !p_.allow_unlisted_contacts)
        for (int i = 0; i < int(p_.edges.size()); ++i) if (role(i, Contact)) {
            if (run && (i & 255) == 0 && run->expired()) return s;
            auto e = p_.edges[i]; int a = x[e.u], b = x[e.v];
            if (a == b) continue;
            s.boundary[a] += e.length; s.boundary[b] += e.length;
            int j = pair_index(a, b);
            if (j < 0) ++s.unlisted;
            else { ++s.contact[j].edges; s.contact[j].length += e.length; }
        }
        for (int r = 0; r < p_.k; ++r) { s.active += s.count[r] != 0; s.violation += region_violation(s, r); }
        s.violation += std::max(0, p_.min_active_regions - s.active) + std::max(0, s.active - p_.max_active_regions);
        if (!p_.allow_unlisted_contacts) s.violation += s.unlisted;
        for (int i = 0; i < int(p_.contacts.size()); ++i) {
            auto c = p_.contacts[i]; int n = s.contact[i].edges;
            s.violation += std::max(0, c.min_edges - n) + std::max(0, n - c.max_edges);
        }
        // 同ラベルの接続辺を併合し、領域ごとの連結成分数を数える。
        if (std::ranges::any_of(p_.regions, &RegionRule::connected)) {
            std::vector<int> parent(p_.n, -1), components = s.count;
            auto root = [&](int v) {
                while (parent[v] >= 0) { if (parent[parent[v]] >= 0) parent[v] = parent[parent[v]]; v = parent[v]; }
                return v;
            };
            for (int i = 0; i < int(p_.edges.size()); ++i) {
                if (run && (i & 255) == 0 && run->expired()) return s;
                const auto& e = p_.edges[i];
                if (!role(i, Connect) || x[e.u] != x[e.v] || !p_.regions[x[e.u]].connected) continue;
                int a = root(e.u), b = root(e.v); if (a == b) continue;
                if (parent[a] > parent[b]) std::swap(a, b);
                parent[a] += parent[b]; parent[b] = a; --components[x[e.u]];
            }
            // 分断は小さな容量違反より戻しにくいため、修復の案内値では重く扱う。
            for (int r = 0; r < p_.k; ++r) if (p_.regions[r].connected) s.violation += 8.0 * std::max(0, components[r] - 1);
        }
        s.feasible = s.violation == 0 && extra_feasible(m, s, run);
        if (!s.feasible) { if (!s.violation) s.violation = 1; return s; }
        for (int v = 0; v < p_.n; ++v) s.cost += vertex_cost(m, v, x[v]);
        for (int i = 0; i < int(p_.edges.size()); ++i) if (role(i, PairCost))
            s.cost += edge_cost(m, i, x[p_.edges[i].u], x[p_.edges[i].v]);
        for (int r = 0; r < p_.k; ++r) s.cost += region_cost(m, r, region_view(s, r));
        s.global = global_cost(m, s); s.cost += s.global;
        return s;
    }
    bool set_scope(const Options& o, bool force = false) {
        // 未指定の連続 resume は O(1)。明示集合はソート・比較して変更を検出する。
        std::vector<int> next;
        if (o.movable) {
            next.assign(o.movable->begin(), o.movable->end());
            std::sort(next.begin(), next.end()); next.erase(std::unique(next.begin(), next.end()), next.end());
            for (int v : next) { assert(0 <= v && v < p_.n); (void)v; }
        }
        bool changed = force || !scope_ready_ || limited_scope_ != bool(o.movable) || scope_ != next;
        if (!changed) return false;
        scope_ = std::move(next); limited_scope_ = bool(o.movable); scope_ready_ = true;
        movable_.assign(p_.n, !limited_scope_);
        for (int v : scope_) movable_[v] = 1;
        free_.clear(); free_mask_.assign(p_.n, 0);
        for (int v = 0; v < p_.n; ++v) if (movable_[v] && (p_.fixed.empty() || p_.fixed[v] < 0)) { free_.push_back(v); free_mask_[v] = 1; }
        return true;
    }
    bool can_move(int v) const { return free_mask_[v]; }
    bool respects_scope(const std::vector<int>& x, const std::vector<int>& anchor) const {
        for (int v = 0; v < p_.n; ++v) if (!movable_[v] && x[v] != anchor[v]) return false;
        return true;
    }
    template<class Model> void start(std::span<const int> x, const Options& o, const Model& m, bool fix_assignment = false) {
        // best_labels() 自身を渡された場合に備え、保持状態を壊す前に取り込む。
        best_ = std::vector<int>(x.begin(), x.end()); best_cost_.reset(); has_candidate_ = true;
        rng_ = o.seed; scale_ = 0; set_scope(o, true);
        // 修復時の固定・許可違反は、全体評価を行う前に直す。
        if (fix_assignment) {
            assert_labels(best_);
            for (int v = 0; v < p_.n; ++v) if (movable_[v] && !allowed(v, best_[v]))
                for (int r = 0; r < p_.k; ++r) if (allowed(v, r)) { best_[v] = r; break; }
        }
        current_ = assemble(best_, m);
        ready_ = current_.feasible; standard_model_ = std::is_same_v<Model, DefaultModel>;
        if (current_.feasible) best_cost_ = current_.cost;
        current_is_best_ = current_.feasible;
    }
    Result finish(Run& r, StopReason reason) {
        r.stats.elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - r.start).count();
        return {best_cost_, reason, r.stats};
    }
    void touch(int r) {
        // 同じ候補内では最初の旧値だけを保存する。
        if (region_mark_[r] == epoch_) return;
        region_mark_[r] = epoch_; touched_.push_back(r);
        old_count_[r] = current_.count[r]; old_boundary_[r] = current_.boundary[r];
        for (int d = 0; d < p_.load_dim; ++d) { size_t i = size_t(r) * p_.load_dim + d; old_load_[i] = current_.load[i]; }
        for (int d = 0; d < p_.feature_dim; ++d) { size_t i = size_t(r) * p_.feature_dim + d; old_feature_[i] = current_.feature[i]; }
    }
    void contact_delta(int a, int b, double length, int sign) {
        if (a == b) return;
        // 所属頂点が変わらない第三領域の境界量は、候補全体でも変わらない。
        if (region_mark_[a] == epoch_) current_.boundary[a] += sign * length;
        if (region_mark_[b] == epoch_) current_.boundary[b] += sign * length;
        int i = pair_index(a, b);
        if (i < 0) { current_.unlisted += sign; return; }
        if (pair_mark_[i] != epoch_) {
            pair_mark_[i] = epoch_; touched_pairs_.push_back(i); old_contact_[i] = current_.contact[i];
        }
        current_.contact[i].edges += sign; current_.contact[i].length += sign * length;
    }
    int before(int v) const { return vertex_mark_[v] == epoch_ ? old_label_[v] : current_.label[v]; }
    bool connected(int r, Run& run) {
        if (!p_.regions[r].connected || current_.count[r] <= 1) return true;
        // 単一追加は接続辺1本で十分。単一除去は、残った隣接頂点同士の接続だけを調べる。
        visit_ += 2; // visit_-1 は確認対象の隣接点、visit_ は探索済み。
        bool removal = changes_.size() == 1 && changes_[0].from == r;
        int needed = current_.count[r];
        if (changes_.size() == 1) {
            int v = changes_[0].vertex; block_.clear();
            for (int j = offsets_[v]; j < offsets_[v + 1]; ++j) {
                auto [e, u] = adjacency_[j];
                if (role(e, Connect) && current_.label[u] == r && seen_[u] != visit_ - 1) { seen_[u] = visit_ - 1; block_.push_back(u); }
            }
            if (!removal) return !block_.empty();
            if (block_.size() <= 1) return !block_.empty();
            needed = int(block_.size());
        }
        int root = -1;
        if (removal) root = block_[0];
        else for (int v : current_.members[r]) if (current_.label[v] == r) { root = v; break; }
        if (root < 0) for (auto c : changes_) if (c.to == r) { root = c.vertex; break; }
        assert(root >= 0);
        queue_.clear(); queue_.push_back(root); seen_[root] = visit_;
        --needed;
        // 最後の頂点を発見したら、その頂点の辺を走査せず終了できる。
        for (size_t at = 0; at < queue_.size() && needed; ++at) {
            if ((at & 127) == 0 && run.expired()) return false;
            int v = queue_[at];
            for (int j = offsets_[v]; j < offsets_[v + 1]; ++j) {
                auto [e, u] = adjacency_[j];
                if (role(e, Connect) && current_.label[u] == r && seen_[u] != visit_) {
                    if (!removal || seen_[u] == visit_ - 1) --needed;
                    seen_[u] = visit_; queue_.push_back(u);
                }
            }
        }
        return needed == 0;
    }
    bool accept(Cost next, const Run& run) {
        if (next <= current_.cost) return true;
        if (run.options.acceptance == Acceptance::greedy) return false;
        double delta = double(next - current_.cost);
        double unit = delta / double(changes_.size());
        scale_ = scale_ == 0 ? unit : scale_ * 0.98 + unit * 0.02;
        double t = run.options.temperature > 0 ? run.options.temperature : scale_;
        t *= std::pow(run.options.final_temperature_ratio, run.progress());
        return uniform() < std::exp(-delta / std::max(t, 1e-100));
    }
    template<class Model> void attempt(Run& run, const Model& model, int kind) {
        // 各生成器は頂点を重複させない。無変化だけを除き、許可条件を先に確認する。
        changes_.erase(std::remove_if(changes_.begin(), changes_.end(), [&](auto c) { return current_.label[c.vertex] == c.to; }), changes_.end());
        if (changes_.empty()) return;
        for (auto c : changes_) if (!can_move(c.vertex) || !allowed(c.vertex, c.to)) return;
        ++epoch_; touched_.clear(); touched_pairs_.clear(); incident_.clear();
        int old_active = current_.active, old_unlisted = current_.unlisted;
        for (auto& c : changes_) {
            int v = c.vertex; c.from = current_.label[v];
            assert(vertex_mark_[v] != epoch_);
            vertex_mark_[v] = epoch_; old_label_[v] = c.from;
            touch(c.from); touch(c.to);
            --current_.count[c.from]; ++current_.count[c.to];
            for (int d = 0; d < p_.load_dim; ++d) {
                current_.load[size_t(c.from) * p_.load_dim + d] -= load(model, v, c.from, d);
                current_.load[size_t(c.to) * p_.load_dim + d] += load(model, v, c.to, d);
            }
            if (!p_.feature.empty()) for (int d = 0; d < p_.feature_dim; ++d) {
                double f = p_.feature[size_t(v) * p_.feature_dim + d];
                current_.feature[size_t(c.from) * p_.feature_dim + d] -= f;
                current_.feature[size_t(c.to) * p_.feature_dim + d] += f;
            }
            current_.label[v] = c.to;
            for (int j = offsets_[v]; j < offsets_[v + 1]; ++j) {
                int e = adjacency_[j].edge;
                if (edge_mark_[e] != epoch_) { edge_mark_[e] = epoch_; incident_.push_back(e); }
            }
        }
        for (int r : touched_) current_.active += (current_.count[r] > 0) - (old_count_[r] > 0);
        // 標準モデルで接触制約もない場合、境界・接触集計は参照されない。モデル変更時は resume が再集計する。
        if (!std::is_same_v<Model, DefaultModel> || !p_.contacts.empty() || !p_.allow_unlisted_contacts)
        for (int i : incident_) if (role(i, Contact)) {
            auto e = p_.edges[i];
            contact_delta(before(e.u), before(e.v), e.length, -1);
            contact_delta(current_.label[e.u], current_.label[e.v], e.length, 1);
        }
        // 独自の目的関数は連結性・追加条件の確認後だけ呼ぶ。標準の貪欲探索は悪化候補を先に棄却できる。
        bool feasible = p_.min_active_regions <= current_.active && current_.active <= p_.max_active_regions;
        feasible &= p_.allow_unlisted_contacts || !current_.unlisted;
        for (int r : touched_) feasible &= region_violation(current_, r) == 0;
        for (int i : touched_pairs_) {
            auto c = p_.contacts[i]; int n = current_.contact[i].edges;
            feasible &= c.min_edges <= n && n <= c.max_edges;
        }
        Cost global = 0;
        auto score = [&]() {
            Cost delta = 0;
            for (auto c : changes_) delta += vertex_cost(model, c.vertex, c.to) - vertex_cost(model, c.vertex, c.from);
            for (int i : incident_) if (role(i, PairCost)) {
                auto e = p_.edges[i];
                delta += edge_cost(model, i, current_.label[e.u], current_.label[e.v]) - edge_cost(model, i, before(e.u), before(e.v));
            }
            for (int r : touched_) {
                RegionStatsView old{old_count_[r], std::span(old_load_).subspan(size_t(r) * p_.load_dim, p_.load_dim),
                    std::span(old_feature_).subspan(size_t(r) * p_.feature_dim, p_.feature_dim), old_boundary_[r]};
                delta += region_cost(model, r, region_view(current_, r)) - region_cost(model, r, old);
            }
            global = global_cost(model, current_);
            return current_.cost + delta + (global - current_.global);
        };
        std::optional<Cost> cached;
        if constexpr (std::is_same_v<Model, DefaultModel>)
            if (feasible && run.options.acceptance == Acceptance::greedy) { cached = score(); feasible = *cached <= current_.cost; }
        for (int r : touched_) if (feasible) feasible = connected(r, run);
        if (feasible && !run.expired()) feasible = extra_feasible(model, current_, &run);
        else feasible = false;
        bool accepted = false;
        if (feasible) {
            ++run.stats.neighborhood[kind].feasible;
            Cost next = cached ? *cached : score();
            accepted = !run.expired() && accept(next, run);
            if (accepted) {
                // 所属集合は採用時だけ変更する。末尾交換により1頂点 O(1)。
                for (auto c : changes_) {
                    auto& from = current_.members[c.from]; int pos = current_.position[c.vertex];
                    from[pos] = from.back(); current_.position[from[pos]] = pos; from.pop_back();
                    auto& to = current_.members[c.to]; current_.position[c.vertex] = int(to.size()); to.push_back(c.vertex);
                }
                current_.cost = next; current_.global = global;
                ++run.stats.neighborhood[kind].accepted;
                if (next <= *best_cost_) {
                    bool improved = next < *best_cost_;
                    if (current_is_best_) for (auto c : changes_) best_[c.vertex] = c.to;
                    else best_ = current_.label;
                    best_cost_ = next; current_is_best_ = true;
                    run.stats.neighborhood[kind].improved += improved;
                } else current_is_best_ = false;
            }
        }
        if (!accepted) {
            for (auto c : changes_) current_.label[c.vertex] = c.from;
            for (int r : touched_) {
                current_.count[r] = old_count_[r]; current_.boundary[r] = old_boundary_[r];
                for (int d = 0; d < p_.load_dim; ++d) { size_t i = size_t(r) * p_.load_dim + d; current_.load[i] = old_load_[i]; }
                for (int d = 0; d < p_.feature_dim; ++d) { size_t i = size_t(r) * p_.feature_dim + d; current_.feature[i] = old_feature_[i]; }
            }
            for (int i : touched_pairs_) current_.contact[i] = old_contact_[i];
            current_.active = old_active; current_.unlisted = old_unlisted;
        }
    }
    int destination(int v) {
        // 接続先を主に使い、空領域・非連結領域への移動も無作為抽選で残す。
        int degree = offsets_[v + 1] - offsets_[v];
        if (degree && pick(8)) {
            int shift = pick(degree);
            for (int i = 0; i < degree; ++i) {
                int index = i + shift; if (index >= degree) index -= degree;
                auto [e, u] = adjacency_[offsets_[v] + index];
                if (role(e, Connect) && current_.label[u] != current_.label[v]) return current_.label[u];
            }
        }
        return pick(p_.k);
    }
    void make_block(int root, int limit, std::vector<int>& out) {
        // 同じ領域の可動頂点だけを接続辺で広げ、連結な塊を作る。
        ++visit_; out.clear(); out.push_back(root); seen_[root] = visit_;
        for (size_t at = 0; at < out.size() && int(out.size()) < limit; ++at) {
            int v = out[at], degree = offsets_[v + 1] - offsets_[v];
            int shift = degree ? pick(degree) : 0;
            for (int j = 0; j < degree && int(out.size()) < limit; ++j) {
                int index = j + shift; if (index >= degree) index -= degree;
                auto [e, u] = adjacency_[offsets_[v] + index];
                if (role(e, Connect) && can_move(u) && current_.label[u] == current_.label[root] && seen_[u] != visit_) {
                    seen_[u] = visit_; out.push_back(u);
                }
            }
        }
    }
    template<class Model> void recombine(int a, int b, Run& run, const Model& model) {
        if (a == b || current_.members[a].empty()) return;
        struct Frame { int v, at, shift; };
        std::vector<Frame> stack;
        tree_.clear(); ++visit_;
        auto enter = [&](int v) {
            seen_[v] = visit_; entry_[v] = int(tree_.size()); tree_.push_back(v);
            int deg = offsets_[v + 1] - offsets_[v]; stack.push_back({v, 0, deg ? pick(deg) : 0});
        };
        enter(current_.members[a][pick(int(current_.members[a].size()))]);
        // DFS木の部分木は訪問順の連続区間。1本切れば双方の連結性が保たれる。
        int scanned = 0;
        while (!stack.empty()) {
            if ((++scanned & 255) == 0 && run.expired()) return;
            auto& f = stack.back(); int degree = offsets_[f.v + 1] - offsets_[f.v];
            if (f.at == degree) { subtree_[f.v] = int(tree_.size()) - entry_[f.v]; stack.pop_back(); continue; }
            int index = f.at++ + f.shift; if (index >= degree) index -= degree;
            auto [e, u] = adjacency_[offsets_[f.v] + index];
            if (role(e, Connect) && seen_[u] != visit_ && (current_.label[u] == a || current_.label[u] == b)) enter(u);
        }
        if (tree_.size() != current_.members[a].size() + current_.members[b].size()) return;
        int chosen = -1, orientation = 0, candidates = 0, total = int(tree_.size());
        int dimensions = p_.regions[a].load_bounds.empty() && p_.regions[b].load_bounds.empty() ? 0 : p_.load_dim;
        std::vector<int64_t> pa(size_t(total + 1) * dimensions), pb(pa.size());
        for (int i = 0; i < total; ++i) for (int d = 0; d < dimensions; ++d) {
            pa[size_t(i + 1) * dimensions + d] = pa[size_t(i) * dimensions + d] + load(model, tree_[i], a, d);
            pb[size_t(i + 1) * dimensions + d] = pb[size_t(i) * dimensions + d] + load(model, tree_[i], b, d);
        }
        auto capacity_ok = [&](int v, int flip) {
            for (int side = 0; side < 2; ++side) {
                int r = side ? b : a; const auto& prefix = side ? pb : pa;
                for (int d = 0; d < int(p_.regions[r].load_bounds.size()); ++d) {
                    int64_t value = prefix[size_t(entry_[v] + subtree_[v]) * dimensions + d] - prefix[size_t(entry_[v]) * dimensions + d];
                    if (bool(side) != bool(flip)) value = prefix[size_t(total) * dimensions + d] - value;
                    auto bounds = p_.regions[r].load_bounds[d];
                    if (value < bounds.lower || bounds.upper < value) return false;
                }
            }
            return true;
        };
        auto size_ok = [&](int r, int n) { return p_.regions[r].min_vertices <= n && n <= p_.regions[r].max_vertices; };
        for (int v : tree_) if (entry_[v]) for (int flip = 0; flip < 2; ++flip) {
            int na = flip ? total - subtree_[v] : subtree_[v];
            if (size_ok(a, na) && size_ok(b, total - na) && capacity_ok(v, flip) && pick(++candidates) == 0) { chosen = v; orientation = flip; }
        }
        if (chosen < 0) return;
        for (int v : tree_) {
            bool inside = entry_[chosen] <= entry_[v] && entry_[v] < entry_[chosen] + subtree_[chosen];
            int r = (inside != bool(orientation)) ? a : b;
            if (current_.label[v] != r) changes_.push_back({v, r});
        }
    }
    template<class Model> void propose(int kind, Run& run, const Model& model) {
        changes_.clear();
        int v = free_[pick(int(free_.size()))];
        int a = current_.label[v], b = destination(v);
        // 軽い頂点操作、領域全体の操作、ブロック操作の順に分岐する。
        if (kind == Move) { if (a != b) changes_.push_back({v, b}); return; }
        if (kind == Swap) {
            if (current_.members[b].empty() || a == b) return;
            int u = current_.members[b][pick(int(current_.members[b].size()))];
            changes_ = {{v, b}, {u, a}}; return;
        }
        if (kind == Relabel) {
            b = pick(p_.k); if (a == b) return;
            for (int u : current_.members[a]) changes_.push_back({u, b});
            if (pick(4)) for (int u : current_.members[b]) changes_.push_back({u, a});
            return;
        }
        if (kind == Recombine) { recombine(a, b, run, model); return; }
        if (a == b) return;
        int size = 2 + pick(7);
        if (!current_.count[b]) size = std::max(size, p_.regions[b].min_vertices);
        make_block(v, size, block_);
        for (int u : block_) changes_.push_back({u, b});
        if (current_.count[b] && pick(2)) {
            int u = current_.members[b][pick(int(current_.members[b].size()))];
            if (!can_move(u)) return;
            make_block(u, int(changes_.size()), block_);
            for (int w : block_) changes_.push_back({w, a});
        }
    }
    template<class Model> void search(Run& run, const Model& model) {
        assert(current_.feasible && best_cost_);
        int total = std::accumulate(run.options.weights.begin(), run.options.weights.end(), 0);
        if (!total || free_.empty() || p_.k == 1) { run.exhausted = true; return; }
        // 近傍生成→原子的な候補評価を繰り返す。棄却も試行数に含む。
        while (!run.stop()) {
            int roll = pick(total), kind = 0;
            while (roll >= run.options.weights[kind]) roll -= run.options.weights[kind++];
            auto start_time = run.options.profile ? Clock::now() : Clock::time_point{};
            ++run.stats.steps; ++run.stats.neighborhood[kind].proposed;
            propose(kind, run, model);
            if (!run.expired()) attempt(run, model, kind);
            if (run.options.profile) run.stats.neighborhood[kind].elapsed_ns +=
                std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_time).count();
        }
    }
    template<class Model> std::vector<int> grow(const Model& model, bool guided = false) {
        std::vector<int> x(p_.n, -1), count(p_.k, 0), q;
        if (!p_.fixed.empty()) for (int v = 0; v < p_.n; ++v) if (p_.fixed[v] >= 0) {
            x[v] = p_.fixed[v]; ++count[x[v]]; q.push_back(v);
        }
        std::vector<int> order(p_.n); std::iota(order.begin(), order.end(), 0);
        for (int i = p_.n - 1; i > 0; --i) std::swap(order[i], order[pick(i + 1)]);
        int active = int(std::count_if(count.begin(), count.end(), [](int n) { return n != 0; }));
        for (int r = 0; r < p_.k; ++r) if (!count[r] && (p_.regions[r].required || active < p_.min_active_regions)) {
            for (int v : order) if (x[v] < 0 && allowed(v, r)) { x[v] = r; ++count[r]; q.push_back(v); ++active; break; }
        }
        // 割当済みの隣接領域との禁止接触を、成長中の選択から除く。
        auto contact_ok = [&](int v, int r) {
            if (p_.allow_unlisted_contacts) return true;
            for (int j = offsets_[v]; j < offsets_[v + 1]; ++j) {
                auto [e, u] = adjacency_[j];
                if (role(e, Contact) && x[u] >= 0 && x[u] != r && pair_index(r, x[u]) < 0) return false;
            }
            return true;
        };
        // 個数の不足率が大きい領域を先に成長させる。符号付き資源には単調性を仮定しない。
        std::vector<std::vector<int>> frontier(p_.k);
        auto append = [&](int v, int r) {
            for (int i = offsets_[v]; i < offsets_[v + 1]; ++i) {
                auto [e, u] = adjacency_[i];
                if (role(e, Connect) && x[u] < 0 && allowed(u, r)) frontier[r].push_back(u);
            }
        };
        for (int v : q) append(v, x[v]);
        auto priority = [&](int r) {
            int target = std::clamp(p_.n / std::max(1, active), std::max(1, p_.regions[r].min_vertices), std::max(1, p_.regions[r].max_vertices));
            return double(count[r]) / target;
        };
        std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<>> pending;
        for (int r = 0; r < p_.k; ++r) if (!frontier[r].empty()) pending.push({priority(r), r});
        while (!pending.empty()) {
            int r = pending.top().second; pending.pop(); auto& f = frontier[r];
            if (count[r] >= p_.regions[r].max_vertices) continue;
            while (!f.empty()) {
                int j = pick(int(f.size())); Cost best = std::numeric_limits<Cost>::max();
                // 独自コストは非合法な状態で呼べないため、上書きされていない既定項だけを手掛かりにする。
                for (int trial = 0; trial < (guided ? 12 : 0); ++trial) {
                    int index = pick(int(f.size())), v = f[index]; if (x[v] >= 0) continue;
                    Cost value = 0;
                    if constexpr (!(requires { model.vertex_cost(v, r); }))
                        if (!p_.unary_cost.empty()) value += p_.unary_cost[size_t(v) * p_.k + r];
                    if constexpr (!(requires { model.edge_cost(0, r, r); }))
                        for (int at = offsets_[v]; at < offsets_[v + 1]; ++at) {
                            auto [e, u] = adjacency_[at];
                            if (role(e, PairCost) && x[u] >= 0) value += p_.edges[e].cut_cost * Cost(x[u] == r ? -1 : 1);
                        }
                    if (value < best) { best = value; j = index; }
                }
                int v = f[j]; f[j] = f.back(); f.pop_back();
                if (x[v] >= 0 || !contact_ok(v, r)) continue;
                x[v] = r; ++count[r]; append(v, r); break;
            }
            if (!f.empty()) pending.push({priority(r), r});
        }
        for (int v : order) if (x[v] < 0) {
            x[v] = pick(p_.k);
            for (int j = 0; j < p_.k; ++j) { int r = (x[v] + j) % p_.k; if (allowed(v, r) && contact_ok(v, r)) { x[v] = r; break; } }
        }
        return x;
    }
    template<class Model> void restore_feasibility(Run& run, const Model& model, bool restart) {
        if (current_.feasible) return;
        if (free_.empty()) { run.exhausted = true; return; }
        auto least = current_.label; double lowest = current_.violation;
        int stalled = 0, restarts = 0;
        // 修復では全体の違反量を用いる。違反中に利用者の目的関数を呼ばない。
        while (!run.stop() && !current_.feasible) {
            ++run.stats.steps;
            int kind = pick(10); kind = kind < 6 ? Move : kind < 8 ? Swap : kind == 8 ? Block : Recombine;
            propose(kind, run, model);
            auto labels = current_.label;
            bool permitted = true;
            for (auto c : changes_) {
                if (!can_move(c.vertex) || !allowed(c.vertex, c.to)) { permitted = false; break; }
                labels[c.vertex] = c.to;
            }
            if (permitted && !changes_.empty() && !run.expired()) {
                auto next = assemble(labels, model, &run);
                if (run.expired()) break;
                double delta = next.violation - current_.violation;
                if (next.feasible || delta <= 0 || uniform() < std::exp(-delta / (0.05 + 0.5 * (1 - run.progress())))) current_ = std::move(next);
                if (current_.violation < lowest) { lowest = current_.violation; least = current_.label; stalled = 0; }
            }
            if (++stalled == 256 && !run.stop()) {
                // 良かった途中状態を再利用し、4回に1回は新しい構築へ切り替える。
                bool rebuild = restart && ++restarts % 4 == 0;
                current_ = assemble(rebuild ? grow(model, true) : least, model); stalled = 0;
                if (rebuild) ++run.stats.construction_attempts;
            }
        }
        ready_ = current_.feasible;
        if (current_.feasible) { best_ = current_.label; best_cost_ = current_.cost; current_is_best_ = true; }
        else best_ = std::move(least);
    }
};

#if __INCLUDE_LEVEL__ == 0
// 以下は単独コンパイル時の検証・実測専用。ライブラリ本体からは独立した検証器を使う。
class ConnectedPartitionSolverTests {
    using S = ConnectedPartitionSolver<>;
    using P = S::Problem;
    using O = S::Options;
    static void check(bool ok, const char* message) {
        if (!ok) { std::cerr << "FAILED: " << message << '\n'; std::abort(); }
    }
    static bool close(double a, double b) { return std::abs(a - b) <= 1e-7 * (1 + std::max(std::abs(a), std::abs(b))); }
    template<class C> static typename ConnectedPartitionSolver<C>::Evaluation naive(
        const typename ConnectedPartitionSolver<C>::Problem& p, std::span<const int> x) {
        using T = ConnectedPartitionSolver<C>;
        std::vector<int> count(p.k), parent(p.n), contacts(p.contacts.size());
        std::vector<int64_t> loads(size_t(p.k) * p.load_dim);
        std::iota(parent.begin(), parent.end(), 0);
        auto root = [&](int v) { while (parent[v] != v) v = parent[v]; return v; };
        bool valid = true;
        for (int v = 0; v < p.n; ++v) {
            ++count[x[v]];
            if (!p.fixed.empty() && p.fixed[v] >= 0 && p.fixed[v] != x[v]) valid = false;
            for (const auto& d : p.domains) if (d.vertex == v && std::find(d.labels.begin(), d.labels.end(), x[v]) == d.labels.end()) valid = false;
            for (int d = 0; d < p.load_dim; ++d) if (!p.load.empty()) loads[size_t(x[v]) * p.load_dim + d] += p.load[size_t(v) * p.load_dim + d];
        }
        // 連結性はDSU、接触は入力ペアの線形探索。実装のCSR・索引を使わない。
        for (auto e : p.edges) if (e.enabled && (e.roles & T::Connect) && x[e.u] == x[e.v]) parent[root(e.u)] = root(e.v);
        for (auto e : p.edges) if (e.enabled && (e.roles & T::Contact) && x[e.u] != x[e.v]) {
            bool listed = false;
            for (int i = 0; i < int(p.contacts.size()); ++i) {
                auto c = p.contacts[i];
                if ((c.a == x[e.u] && c.b == x[e.v]) || (c.b == x[e.u] && c.a == x[e.v])) { listed = true; ++contacts[i]; }
            }
            if (!listed && !p.allow_unlisted_contacts) valid = false;
        }
        int active = 0;
        for (int r = 0; r < p.k; ++r) {
            auto rule = p.regions[r];
            if (!count[r]) { valid &= !rule.required; continue; }
            ++active;
            valid &= rule.min_vertices <= count[r] && count[r] <= rule.max_vertices;
            for (int d = 0; d < int(rule.load_bounds.size()); ++d) {
                auto b = rule.load_bounds[d]; auto w = loads[size_t(r) * p.load_dim + d];
                valid &= b.lower <= w && w <= b.upper;
            }
            if (rule.connected) {
                int first = -1;
                for (int v = 0; v < p.n; ++v) if (x[v] == r) { if (first < 0) first = root(v); valid &= root(v) == first; }
            }
        }
        valid &= p.min_active_regions <= active && active <= p.max_active_regions;
        for (int i = 0; i < int(p.contacts.size()); ++i) valid &= p.contacts[i].min_edges <= contacts[i] && contacts[i] <= p.contacts[i].max_edges;
        if (!valid) return {};
        C cost = 0;
        for (int v = 0; v < p.n; ++v) if (!p.unary_cost.empty()) cost += p.unary_cost[size_t(v) * p.k + x[v]];
        for (auto e : p.edges) if (e.enabled && (e.roles & T::PairCost) && x[e.u] != x[e.v]) cost += e.cut_cost;
        for (int r = 0; r < p.k; ++r) if (count[r]) {
            if (!p.activation_cost.empty()) cost += p.activation_cost[r];
            for (auto b : p.balance_terms) if (b.region == r) {
                C z = C(loads[size_t(r) * p.load_dim + b.dim]) - b.target; cost += b.coefficient * z * z;
            }
        }
        return {true, cost};
    }
    static std::pair<P, std::vector<int>> random_problem(int seed) {
        std::mt19937 gen(seed);
        auto rnd = [&](int n) { return int(gen() % unsigned(n)); };
        int n = 4 + rnd(21), k = 2 + rnd(4);
        k = std::min(k, n);
        P p(n, k); std::vector<int> x(n);
        for (int v = 0; v < n; ++v) x[v] = v * k / n;
        p.load_dim = rnd(4); p.feature_dim = rnd(4);
        p.load.resize(size_t(n) * p.load_dim); p.feature.resize(size_t(n) * p.feature_dim);
        for (auto& w : p.load) w = rnd(9) - 3;
        for (auto& f : p.feature) f = double(rnd(17) - 8) / 4;
        for (int v = 1; v < n; ++v) p.edges.push_back({v - 1, v, double(rnd(11) - 3), double(rnd(4)) / 2});
        for (int i = 0; i < 3 * n; ++i) {
            int u = rnd(n), v = rnd(n); if (u == v) continue;
            p.edges.push_back({u, v, double(rnd(11) - 5), double(rnd(5)) / 2, rnd(8) != 0, unsigned(rnd(8))});
        }
        p.fixed.assign(n, -1);
        for (int v = 0; v < n; ++v) {
            if (!rnd(9)) p.fixed[v] = x[v];
            if (!rnd(4)) { std::vector<int> d{x[v]}; for (int r = 0; r < k; ++r) if (!rnd(2)) d.push_back(r); p.domains.push_back({v, d}); }
        }
        for (int r = 0; r < k; ++r) {
            int count = int(std::count(x.begin(), x.end(), r));
            p.regions[r].required = rnd(4) != 0; p.regions[r].connected = rnd(4) != 0;
            int slack = rnd(4); p.regions[r].min_vertices = std::max(1, count - slack); p.regions[r].max_vertices = count + slack;
            if (rnd(2)) {
                p.regions[r].load_bounds.resize(p.load_dim);
                for (int d = 0; d < p.load_dim; ++d) {
                    int64_t w = 0; for (int v = 0; v < n; ++v) if (x[v] == r) w += p.load[size_t(v) * p.load_dim + d];
                    p.regions[r].load_bounds[d] = {w - 4, w + 4};
                }
            }
        }
        p.allow_unlisted_contacts = rnd(2) != 0;
        for (int a = 0; a < k; ++a) for (int b = a + 1; b < k; ++b) {
            int ct = 0;
            for (auto e : p.edges) if (e.enabled && (e.roles & S::Contact) && std::min(x[e.u], x[e.v]) == a && std::max(x[e.u], x[e.v]) == b) ++ct;
            if (!p.allow_unlisted_contacts || rnd(2)) p.contacts.push_back({a, b, std::max(0, ct - rnd(3)), ct + rnd(3)});
        }
        p.unary_cost.resize(size_t(n) * k); for (auto& c : p.unary_cost) c = rnd(21) - 10;
        p.activation_cost.resize(k); for (auto& c : p.activation_cost) c = rnd(9) - 4;
        for (int r = 0; r < k; ++r) for (int d = 0; d < p.load_dim; ++d) p.balance_terms.push_back({r, d, double(rnd(10) - 5), double(rnd(3)) / 4});
        return {p, x};
    }
    struct CheckedModel {
        const P& p;
        bool label_load = false, restrict_shape = false;
        mutable int inspections = 0, costs = 0;
        int64_t load(int v, int r, int d) const { return (p.load.empty() ? 0 : p.load[size_t(v) * p.load_dim + d]) + (label_load ? (v + r + d) % 3 - 1 : 0); }
        double vertex_cost(int v, int r) const { ++costs; return double((v + 1) * (r + 2) % 13 - 7); }
        double edge_cost(int e, int a, int b) const { ++costs; return double((e + a * 3 + b * 5) % 11 - 4); }
        double region_cost(int r, const S::RegionStatsView& s) const {
            ++costs; double z = s.boundary * (r + 1) + s.vertices * s.vertices;
            for (auto w : s.load) z += double(w * w) / 4;
            for (double f : s.feature) z += f * f / 8;
            return z;
        }
        bool extra_feasible(const S::SummaryView& s) const {
            ++inspections;
            check(s.size() == p.n && s.region_count() == p.k && s.contact_count() == int(p.contacts.size()), "summary dimensions");
            int active = 0;
            for (int r = 0; r < p.k; ++r) {
                int count = 0; std::vector<int64_t> w(p.load_dim); std::vector<double> f(p.feature_dim); double boundary = 0;
                for (int v = 0; v < p.n; ++v) if (s.label(v) == r) {
                    ++count;
                    for (int d = 0; d < p.load_dim; ++d) w[d] += load(v, r, d);
                    for (int d = 0; d < p.feature_dim; ++d) if (!p.feature.empty()) f[d] += p.feature[size_t(v) * p.feature_dim + d];
                }
                for (auto e : p.edges) if (e.enabled && (e.roles & S::Contact) && ((s.label(e.u) == r) != (s.label(e.v) == r))) boundary += e.length;
                auto view = s.region(r);
                check(count == view.vertices && close(boundary, view.boundary), "candidate count/boundary");
                check(std::equal(w.begin(), w.end(), view.load.begin()), "candidate load");
                for (int d = 0; d < p.feature_dim; ++d) check(close(f[d], view.feature[d]), "candidate feature");
                active += count != 0;
            }
            check(active == s.active_regions(), "candidate active");
            for (int i = 0; i < int(p.contacts.size()); ++i) {
                auto rule = p.contacts[i]; int count = 0; double length = 0;
                for (auto e : p.edges) if (e.enabled && (e.roles & S::Contact)) {
                    int a = s.label(e.u), b = s.label(e.v);
                    if ((a == rule.a && b == rule.b) || (a == rule.b && b == rule.a)) { ++count; length += e.length; }
                }
                auto c = s.contact(i); check(c.edges == count && close(c.length, length), "candidate contact");
            }
            return !restrict_shape || p.n == 0 || s.label(0) != s.label(p.n - 1);
        }
        double global_cost(const S::SummaryView& s) const {
            ++costs; double z = 0;
            for (int v = 0; v < p.n; ++v) z += (v + 1) * s.label(v) * 0.125;
            for (int i = 0; i < int(p.contacts.size()); ++i) { auto c = s.contact(i); z += c.edges * c.length * 0.25; }
            return z;
        }
    };
    static void random_tests() {
        std::mt19937 gen(7919);
        for (int seed = 0; seed < 180; ++seed) {
            auto [p, x] = random_problem(seed); S solver(p);
            auto expected = naive<double>(p, x), actual = solver.evaluate(x);
            check(expected.feasible && actual.feasible && close(*expected.cost, *actual.cost), "witness");
            for (int i = 0; i < 150; ++i) {
                auto y = x; for (int& r : y) r = int(gen() % unsigned(p.k));
                auto a = naive<double>(p, y), b = solver.evaluate(y);
                check(a.feasible == b.feasible && bool(a.cost) == bool(b.cost), "random feasibility");
                if (a.feasible) check(close(*a.cost, *b.cost), "random full cost");
            }
            O o; o.budget_us = -1; o.max_steps = 800; o.seed = unsigned(seed); o.profile = true;
            auto r = solver.improve(x, o);
            auto n = naive<double>(p, solver.best_labels());
            check(n.feasible && close(*n.cost, *r.best_cost) && *r.best_cost <= *actual.cost + 1e-7, "incremental standard");
            auto previous = r;
            r = solver.resume(o);
            check(*r.best_cost <= *previous.best_cost + 1e-7, "resume nonworsening");
            n = naive<double>(p, solver.best_labels()); check(n.feasible && close(*n.cost, *r.best_cost), "resume cost");
            r = solver.improve(solver.best_labels(), o); check(r.best_cost.has_value(), "self alias");
            CheckedModel model{p}; o.max_steps = 300;
            r = solver.improve(x, o, model);
            check(r.best_cost.has_value(), "custom initial");
            auto full = solver.evaluate(solver.best_labels(), model); check(full.feasible && close(*full.cost, *r.best_cost), "custom incremental");
            // 毎回1候補で止め、採否の両方を独立な全体評価と照合する。
            o.max_steps = 1;
            for (int i = 0; i < 60; ++i) {
                r = solver.resume(o, model);
                full = solver.evaluate(solver.best_labels(), model); check(full.feasible && close(*full.cost, *r.best_cost), "custom resume rollback");
            }
            // 領域依存資源、配列省略、任意の追加判定。
            for (auto& rule : p.regions) rule.load_bounds.clear();
            model.label_load = true; model.restrict_shape = true; solver.update(p); o.max_steps = 500;
            r = solver.improve(x, o, model); check(r.best_cost.has_value(), "dependent load");
            full = solver.evaluate(solver.best_labels(), model); check(full.feasible && close(*full.cost, *r.best_cost), "dependent incremental");
        }
    }
    static void api_tests() {
        O zero; zero.budget_us = -1; zero.max_steps = 0;
        P empty(0, 3); for (auto& r : empty.regions) r.required = false;
        S s(empty); std::vector<int> x;
        auto r = s.improve(x, zero); check(r.best_cost && *r.best_cost == 0 && s.best_labels().empty(), "empty problem");
        check(s.solve(zero).best_cost.has_value(), "empty solve");
        empty.regions[1].required = true; s.update(empty);
        check(!s.resume(zero).best_cost, "empty infeasible update");
        P p(4, 2); p.edges = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
        for (auto& a : p.regions) a.min_vertices = a.max_vertices = 2;
        p.unary_cost = {0, 0, 10, 0, 0, 0, 0, 10}; x = {0, 0, 1, 1};
        S cycle(p); O o; o.budget_us = -1; o.max_steps = 3000; o.weights = {0, 1, 0, 0, 0}; o.acceptance = S::Acceptance::greedy;
        r = cycle.improve(x, o); check(r.best_cost && *r.best_cost == 2, "atomic exact-capacity swap");
        std::vector<int> snapshot(cycle.best_labels().begin(), cycle.best_labels().end());
        o.movable = std::span<const int>{}; auto old = r;
        r = cycle.resume(o); check(r.statistics.steps == 0 && r.best_cost == old.best_cost, "empty scope");
        std::vector<int> scope{1, 2}; o.movable = scope; r = cycle.resume(o);
        check(cycle.best_labels()[0] == snapshot[0] && cycle.best_labels()[3] == snapshot[3], "scope freezes previous best");
        o.movable.reset(); p.unary_cost[1] += 100; cycle.update(p); r = cycle.resume(zero);
        check(r.best_cost && close(*r.best_cost, *naive<double>(p, cycle.best_labels()).cost), "cost update");
        p.edges[0].roles &= ~S::Connect; p.edges[1].enabled = false; cycle.update(p); r = cycle.resume(zero);
        if (r.best_cost) check(naive<double>(p, cycle.best_labels()).feasible, "edge invalidation");
        std::vector<int> illegal{0, 1, 0, 1};
        r = cycle.improve(illegal, zero); check(!r.best_cost && r.reason == S::StopReason::invalid_initial, "no stale best");
        r = cycle.repair(zero); check(!r.best_cost, "retained invalid repair zero");
        P independent(3, 2); independent.edges = {{0, 1}, {1, 2}, {0, 2, -100, 0, true, S::PairCost}};
        x = {0, 1, 0}; S remote(independent);
        check(!remote.evaluate(x).feasible, "remote cost is not connection");
        independent.regions[0].connected = false; remote.update(independent); check(remote.evaluate(x).feasible, "disconnected background");
        independent.allow_unlisted_contacts = false; independent.contacts = {{0, 1, 2, 2}}; remote.update(independent);
        check(remote.evaluate(x).feasible, "whitelist zero length");
        independent.edges.push_back({0, 1, 0, 0, true, S::Contact}); remote.update(independent);
        check(!remote.evaluate(x).feasible, "parallel contact counted");
        // 必須の追加判定が失敗した場合、目的関数は一度も呼ばない。
        struct Reject {
            mutable int calls = 0;
            bool extra_feasible(const S::SummaryView&) const { return false; }
            double vertex_cost(int, int) const { ++calls; return 0; }
        } reject;
        P two(2, 2); two.edges = {{0, 1}}; S t(two); x = {0, 1};
        check(!t.evaluate(x, reject).feasible && reject.calls == 0, "objective not called on extra failure");
        x = {0, 0}; check(!t.evaluate(x, reject).feasible && reject.calls == 0, "objective not called on builtin failure");
        struct VertexOnly { double vertex_cost(int v, int r) const { return v + r * 3; } } vertex;
        struct EdgeOnly { double edge_cost(int, int a, int b) const { return a + 10 * b; } } edge;
        struct RegionOnly { double region_cost(int r, const S::RegionStatsView& a) const { return r + a.vertices; } } region;
        struct GlobalOnly { double global_cost(const S::SummaryView& a) const { return a.active_regions() * 7; } } global;
        struct LoadOnly { int64_t load(int v, int r, int d) const { return v - r + d; } } load;
        x = {0, 1}; check(*t.evaluate(x, vertex).cost == 5, "vertex replacement");
        check(*t.evaluate(x, edge).cost == 10, "edge orientation");
        check(*t.evaluate(x, region).cost == 4, "region replacement");
        check(*t.evaluate(x, global).cost == 15, "global callback");
        two.load_dim = 2; two.regions[1].load_bounds = {{0, 0}, {1, 1}}; t.update(two);
        check(t.evaluate(x, load).feasible, "load callback omitted array");
        two.domains = {{0, {}}}; t.update(two); check(!t.evaluate(x).feasible, "empty domain");
        // 任意領域の下限は非空の時だけ適用。使用数・接触下限は常に適用する。
        P opt(6, 2); for (int v = 1; v < 6; ++v) opt.edges.push_back({v - 1, v});
        opt.regions[1].required = false; opt.regions[1].min_vertices = 2; opt.activation_cost = {0, -100};
        S birth(opt); x.assign(6, 0); o = O{}; o.budget_us = -1; o.max_steps = 5000; o.weights = {1, 0, 1, 0, 0};
        r = birth.improve(x, o); check(r.best_cost && *r.best_cost < 0, "multi-vertex birth");
        opt.regions[1].load_bounds = {{5, 8}}; opt.load_dim = 1; birth.update(opt);
        check(birth.evaluate(x).feasible, "empty skips resource minimum");
        opt.contacts = {{0, 1, 1, INT_MAX}}; birth.update(opt); check(!birth.evaluate(x).feasible, "empty contact minimum remains");
        // 全領域の形状を維持した担当交換。
        P relabel(4, 2); relabel.edges = {{0, 1}, {1, 2}, {2, 3}};
        relabel.unary_cost = {20, 0, 20, 0, 0, 20, 0, 20};
        S labels(relabel); x = {0, 0, 1, 1}; o.weights = {0, 0, 0, 1, 0}; o.max_steps = 20;
        r = labels.improve(x, o); check(r.best_cost && *r.best_cost == 1, "whole-label swap");
        // 標準→独自→標準で集計・評価の再利用条件を切り替える。
        o.max_steps = 0; r = labels.resume(o, vertex); auto e = labels.evaluate(labels.best_labels(), vertex); check(close(*r.best_cost, *e.cost), "switch to custom");
        r = labels.resume(o); e = labels.evaluate(labels.best_labels()); check(close(*r.best_cost, *e.cost), "switch to standard");
        relabel.fixed = {0, -1, -1, 1}; labels.update(relabel);
        r = labels.repair(o); if (r.best_cost) check(labels.evaluate(labels.best_labels()).feasible, "forced seed repair");
        // マイクロ秒・絶対期限・中断後も合法な返却解を維持する。
        o = O{}; o.budget_us = 1; o.deadline = S::Clock::now(); x = {0, 0, 1, 1};
        r = labels.improve(x, o); check(r.best_cost && labels.evaluate(labels.best_labels()).feasible, "expired deadline retains initial");
        for (int us : {0, 1, 5, 20, 100}) { o.budget_us = us; o.deadline.reset(); r = labels.resume(o); check(r.best_cost && labels.evaluate(labels.best_labels()).feasible, "cancellation rollback"); }
        // N/K変更後の新しい初期解。
        labels.update(P(1, 1)); x = {0}; r = labels.improve(x, zero); check(r.best_cost && *r.best_cost == 0, "shape update");
        using I = ConnectedPartitionSolver<int64_t>;
        I::Problem ip(4, 2); ip.edges = {{0, 1, -5}, {1, 2, 10}, {2, 3, 7}, {3, 0, -2}};
        ip.load_dim = 1; ip.load = {-3, 2, 7, -1}; ip.balance_terms = {{0, 0, -2, 3}, {1, 0, 4, 2}};
        ip.unary_cost = {1, -2, 3, -4, 5, 6, -7, 8}; I is(ip); I::Options io; io.budget_us = -1; io.max_steps = 5000;
        x = {0, 0, 1, 1}; auto ir = is.improve(x, io); auto ie = naive<int64_t>(ip, is.best_labels());
        check(ie.feasible && ie.cost == ir.best_cost, "exact int64 cost");
        // 整数目的値への定数加算は、固定試行数・同じseedの焼きなましを変えない。
        I::Problem shifted_base(8, 2);
        for (int v = 0; v < 8; ++v) {
            shifted_base.edges.push_back({v, (v + 1) % 8, 0});
            shifted_base.unary_cost.push_back(0); shifted_base.unary_cost.push_back(v * 13 % 7 - 3);
        }
        for (auto& rule : shifted_base.regions) { rule.min_vertices = 3; rule.max_vertices = 5; }
        auto shifted = shifted_base;
        for (int rgn = 0; rgn < 2; ++rgn) shifted.unary_cost[rgn] += 10000000000000000LL;
        I::Options anneal; anneal.budget_us = -1; anneal.max_steps = 2000; anneal.acceptance = I::Acceptance::annealing;
        std::vector<int> seed_labels{0, 0, 0, 0, 1, 1, 1, 1};
        for (int seed = 0; seed < 12; ++seed) {
            I a(shifted_base), b(shifted); anneal.seed = unsigned(seed);
            auto ra = a.improve(seed_labels, anneal), rb = b.improve(seed_labels, anneal);
            check(*rb.best_cost - *ra.best_cost == 10000000000000000LL, "integer objective shift");
            check(std::equal(a.best_labels().begin(), a.best_labels().end(), b.best_labels().begin()), "integer annealing uses exact delta before conversion");
        }
        ip.regions[0].load_bounds = {{9007199254740993LL, 9007199254740993LL}}; ip.load = {9007199254740992LL, 0, 0, 0};
        ip.balance_terms.clear(); is.update(ip); check(!is.evaluate(x).feasible, "integer bound beyond double exactness");
    }
    static void scenario_tests() {
        O o; o.budget_us = -1; o.max_steps = 500; o.acceptance = S::Acceptance::greedy;
        P p(4, 2); p.edges = {{0, 1}, {1, 2}, {2, 3}, {1, 3}};
        p.fixed = {0, -1, 0, 1}; p.unary_cost = {0, 0, 10, 0, 0, 0, 0, 0};
        std::vector<int> x{0, 0, 0, 1}, scope{1}; o.movable = scope; o.weights = {1, 0, 0, 0, 0};
        S s(p); auto r = s.improve(x, o); check(s.best_labels()[1] == 0, "separate fixed components cannot be contracted");
        p.edges.push_back({0, 2, 0, 0, true, S::Connect}); s.update(p);
        r = s.resume(o); check(s.best_labels()[1] == 1, "connect through frozen exterior");
        p.domains = {{0, {1}}}; s.update(p); check(!s.resume(o).best_cost, "fixed domain contradiction");
        check(!s.repair(o).best_cost, "scope can prevent repair");
        p = P(4, 2); p.edges = {{0, 1}, {2, 3}}; x = {0, 0, 1, 1};
        s.update(p); o.movable.reset(); r = s.improve(x, o); check(r.best_cost && s.evaluate(s.best_labels()).feasible, "disconnected input graph");
        p.fixed = x; s.update(p); r = s.resume(o); check(r.best_cost && r.reason == S::StopReason::no_moves, "all fixed");
        // 同じモデル型でも外部入力は呼出し間に変わり得る。
        struct Changing {
            int* factor;
            double vertex_cost(int v, int label) const { return (v + label * 3) * *factor; }
        };
        int factor = 2; Changing model{&factor};
        r = s.improve(x, o, model); factor = -7; r = s.resume(o, model);
        check(close(*r.best_cost, *s.evaluate(s.best_labels(), model).cost), "external custom objective changed");
        // 実際のターン列: 負荷・コスト・通路・端点を更新し、再評価か修復を選ぶ。
        for (int seed = 0; seed < 16; ++seed) {
            auto [problem, initial] = random_problem(500 + seed);
            for (auto& rule : problem.regions) { rule.load_bounds.clear(); rule.min_vertices = 1; rule.max_vertices = problem.n; }
            problem.edges.push_back({0, problem.n - 1, 0, 0, true, S::Connect});
            S solver(problem); o = O{}; o.budget_us = -1; o.max_steps = 150; o.seed = unsigned(seed);
            r = solver.improve(initial, o);
            for (int turn = 0; turn < 24; ++turn) {
                problem.unary_cost[size_t(turn) % problem.unary_cost.size()] += 0.5;
                if (!problem.load.empty()) problem.load[size_t(turn) % problem.load.size()] -= 1;
                if (turn % 3 == 0 && !problem.edges.empty()) problem.edges.back().roles ^= S::Connect;
                if (turn % 5 == 0 && !problem.edges.empty()) problem.edges.back().enabled = !problem.edges.back().enabled;
                if (turn % 7 == 0 && !problem.edges.empty()) {
                    auto& e = problem.edges.back(); e.u = (e.u + 1) % problem.n; if (e.u == e.v) e.u = (e.u + 1) % problem.n;
                }
                solver.update(problem); r = solver.resume(o);
                if (!r.best_cost) r = solver.repair(o);
                if (!r.best_cost) { r = solver.improve(initial, o); check(r.best_cost.has_value(), "persistent witness"); }
                auto n = naive<double>(problem, solver.best_labels()); check(n.feasible && close(*n.cost, *r.best_cost), "turn sequence independent validation");
            }
        }
        // すべての近傍を単独で通し、時間中断と採否の合法性を確認する。
        for (int kind = 0; kind < S::NeighborhoodCount; ++kind) {
            auto [problem, initial] = random_problem(901);
            for (auto& rule : problem.regions) { rule.load_bounds.clear(); rule.min_vertices = 1; rule.max_vertices = problem.n; }
            S solver(problem); CheckedModel custom{problem}; o = O{}; o.weights.fill(0); o.weights[kind] = 1;
            o.budget_us = -1; o.max_steps = 600; o.acceptance = S::Acceptance::annealing;
            r = solver.improve(initial, o, custom);
            check(r.statistics.neighborhood[kind].proposed == 600, "neighborhood step accounting");
            auto exact = solver.evaluate(solver.best_labels(), custom); check(exact.feasible && close(*exact.cost, *r.best_cost), "isolated neighborhood");
            for (int us : {1, 3, 10, 100}) {
                o.budget_us = us; o.max_steps = -1; r = solver.resume(o, custom);
                exact = solver.evaluate(solver.best_labels(), custom); check(exact.feasible && close(*exact.cost, *r.best_cost), "timed candidate rollback");
            }
        }
    }
    static void enumeration_tests() {
        for (int n = 2; n <= 8; ++n) {
            P p(n, 3); p.regions[2].required = false;
            for (int v = 0; v < n; ++v) p.edges.push_back({v, (v + 1) % n, double(v % 3 - 1)});
            p.load_dim = 1; for (int v = 0; v < n; ++v) p.load.push_back(v % 3 - 1);
            p.balance_terms = {{0, 0, 1, 1}, {1, 0, 0, 2}};
            S s(p); int total = 1; for (int i = 0; i < n; ++i) total *= 3;
            std::optional<double> optimum; std::vector<int> witness;
            for (int code = 0; code < total; ++code) {
                std::vector<int> x(n); int z = code; for (int& r : x) { r = z % 3; z /= 3; }
                auto a = naive<double>(p, x), b = s.evaluate(x);
                check(a.feasible == b.feasible && a.cost == b.cost, "exhaustive evaluation");
                if (a.feasible && (!optimum || *a.cost < *optimum)) { optimum = a.cost; witness = x; }
            }
            O o; o.budget_us = -1; o.max_steps = 1000;
            auto r = s.improve(witness, o); check(r.best_cost && close(*r.best_cost, *optimum), "optimum incumbent preserved");
            r = s.solve(o); if (r.best_cost) check(*r.best_cost >= *optimum - 1e-8, "not better than oracle");
        }
    }
    static void review_tests() {
        O zero; zero.budget_us = -1; zero.max_steps = 0;
        // 修復開始時は固定・許可を直してから一度だけ集計する。
        P p(4, 2); p.edges = {{0, 1}, {1, 2}, {2, 3}};
        p.load_dim = 1; p.fixed = {0, -1, -1, 1};
        struct Count {
            mutable int vertices = 0, loads = 0;
            double vertex_cost(int, int) const { ++vertices; return 0; }
            int64_t load(int, int, int) const { ++loads; return 1; }
        };
        for (bool illegal : {false, true}) {
            Count m; S s(p); std::vector<int> x{int(illegal), 0, 1, 1};
            auto r = s.repair(x, zero, m);
            check(r.best_cost && m.vertices == 4 && m.loads == 4, "repair evaluates corrected seed once");
        }
        // 修復でも可動集合の外を変更せず、明示固定と許可集合の両方を守る。
        S s(p); std::vector<int> bad{1, 0, 1, 1}, scope{1, 2};
        O local = zero; local.movable = scope;
        check(!s.repair(bad, local).best_cost, "repair cannot fix frozen exterior");
        p.domains = {{0, {1}}}; s.update(p);
        check(!s.repair(bad, zero).best_cost, "repair cannot bypass fixed-domain conflict");
        // 所属集合が変わらない第三領域の境界・領域評価を更新しない。
        P third(5, 3); third.fixed = {-1, 0, 1, 1, 2}; third.domains = {{0, {0, 1}}};
        for (auto& rule : third.regions) rule.connected = false;
        third.edges = {{0, 4, 0, 3, true, S::Contact}};
        third.unary_cost.assign(15, 0); third.unary_cost[1] = -10;
        struct RegionCounter {
            mutable std::array<int, 3> calls{};
            double region_cost(int r, const S::RegionStatsView& v) const {
                ++calls[r]; return (r + 1) * v.vertices + v.boundary * 0.25;
            }
        } counter;
        S a(third); O o = zero; o.max_steps = 1000; o.weights = {1, 0, 0, 0, 0};
        auto result = a.improve(std::vector<int>{0, 0, 1, 1, 2}, o, counter);
        check(result.best_cost && a.best_labels()[0] == 1 && counter.calls[2] == 1, "unchanged third region is skipped");
        check(close(*result.best_cost, *a.evaluate(a.best_labels(), counter).cost), "third-region cost remains exact");
        // 独自モデルによる失効後も、同じラベルの候補を壊さず再開できる。
        struct Toggle {
            bool valid = true;
            bool extra_feasible(const S::SummaryView&) const { return valid; }
        } toggle;
        P two(4, 2); two.edges = {{0, 1}, {1, 2}, {2, 3}};
        S t(two); std::vector<int> initial{0, 0, 1, 1};
        check(t.improve(initial, zero, toggle).best_cost.has_value(), "same-state model initial");
        toggle.valid = false;
        check(!t.resume(zero, toggle).best_cost && !t.resume(zero, toggle).best_cost, "same-state invalid resumes retain labels");
        toggle.valid = true;
        check(t.resume(zero, toggle).best_cost.has_value(), "same-state validity restored");
        // 悪化受理後の現在解・最良解を、モデル変更時にそれぞれ正しく評価する。
        struct Direction {
            int sign = 1;
            double vertex_cost(int v, int r) const { return sign * ((v * 7 + r * 11) % 17 - 8); }
        } direction;
        for (auto& rule : two.regions) rule.connected = false;
        two.edges.clear(); t.update(two); o = zero; o.max_steps = 200;
        o.acceptance = S::Acceptance::annealing; o.temperature = 1000;
        for (int seed = 0; seed < 40; ++seed) {
            o.seed = unsigned(seed); t.improve(initial, o, direction);
            std::vector<int> best(t.best_labels().begin(), t.best_labels().end());
            direction.sign = -direction.sign;
            double incumbent = *t.evaluate(best, direction).cost;
            result = t.resume(zero, direction);
            check(result.best_cost && *result.best_cost <= incumbent, "reevaluated incumbent preserved");
            check(*result.best_cost == *t.evaluate(t.best_labels(), direction).cost, "resumed best labels and cost agree");
        }
        // 使用領域数の上下限を、空領域の個数・資源条件と別に扱う。
        P active(3, 3); for (auto& rule : active.regions) { rule.required = false; rule.connected = false; }
        active.min_active_regions = active.max_active_regions = 2; S used(active);
        check(used.evaluate(std::vector<int>{0, 1, 1}).feasible, "active count exact");
        check(!used.evaluate(std::vector<int>{0, 0, 0}).feasible, "active count lower");
        check(!used.evaluate(std::vector<int>{0, 1, 2}).feasible, "active count upper");
        // 追加判定が締切を越えたら、目的関数を新たに呼ばず候補を取り消す。
        struct SlowPredicate {
            S::Clock::time_point deadline;
            mutable int predicates = 0, late_costs = 0;
            mutable bool crossed = false;
            bool extra_feasible(const S::SummaryView&) const {
                if (++predicates > 1) { std::this_thread::sleep_until(deadline); crossed = true; }
                return true;
            }
            double vertex_cost(int, int) const { late_costs += crossed; return 0; }
        };
        bool exercised = false;
        for (int trial = 0; trial < 3 && !exercised; ++trial) {
            S solver(two); SlowPredicate model;
            o = O{}; o.budget_us = 20'000; o.weights = {1, 0, 0, 0, 0};
            model.deadline = S::Clock::now() + std::chrono::microseconds(o.budget_us); o.deadline = model.deadline;
            result = solver.improve(initial, o, model); exercised = model.crossed;
            check(result.best_cost && !model.late_costs, "no scoring after predicate timeout");
            if (exercised) check(result.reason == S::StopReason::time_limit, "predicate timeout reason");
        }
        check(exercised, "predicate timeout branch exercised");
    }
    static void metamorphic_tests() {
        std::mt19937 rng(918273);
        // 高次元資源・特徴と、頂点／ラベルの付け替え・評価の尺度変更を独立に照合する。
        for (int seed = 0; seed < 40; ++seed) {
            auto [p, initial] = random_problem(2100 + seed);
            p.load_dim = seed % 2 ? 8 : 4; p.feature_dim = 8;
            p.load.resize(size_t(p.n) * p.load_dim); p.feature.resize(size_t(p.n) * p.feature_dim);
            for (auto& w : p.load) w = int(rng() % 9) - 3;
            for (auto& f : p.feature) f = double(int(rng() % 17) - 8) / 4;
            for (int r = 0; r < p.k; ++r) {
                p.regions[r].load_bounds.resize(p.load_dim);
                for (int d = 0; d < p.load_dim; ++d) {
                    int64_t sum = 0; for (int v = 0; v < p.n; ++v) if (initial[v] == r) sum += p.load[size_t(v) * p.load_dim + d];
                    p.regions[r].load_bounds[d] = {sum - 10, sum + 10};
                }
            }
            std::vector<int> vertex(p.n), label(p.k);
            std::iota(vertex.begin(), vertex.end(), 0); std::iota(label.begin(), label.end(), 0);
            std::shuffle(vertex.begin(), vertex.end(), rng); std::shuffle(label.begin(), label.end(), rng);
            P q = p;
            for (auto& e : q.edges) { e.u = vertex[e.u]; e.v = vertex[e.v]; e.cut_cost *= 2.5; }
            for (int r = 0; r < p.k; ++r) { q.regions[label[r]] = p.regions[r]; q.activation_cost[label[r]] = 2.5 * p.activation_cost[r]; }
            for (auto& b : q.balance_terms) { b.region = label[b.region]; b.coefficient *= 2.5; }
            for (auto& c : q.contacts) { c.a = label[c.a]; c.b = label[c.b]; }
            for (auto& d : q.domains) { d.vertex = vertex[d.vertex]; for (int& r : d.labels) r = label[r]; }
            for (int v = 0; v < p.n; ++v) {
                q.fixed[vertex[v]] = p.fixed[v] < 0 ? -1 : label[p.fixed[v]];
                for (int r = 0; r < p.k; ++r) q.unary_cost[size_t(vertex[v]) * p.k + label[r]] = 2.5 * p.unary_cost[size_t(v) * p.k + r] + 3;
                for (int d = 0; d < p.load_dim; ++d) q.load[size_t(vertex[v]) * p.load_dim + d] = p.load[size_t(v) * p.load_dim + d];
                for (int d = 0; d < p.feature_dim; ++d) q.feature[size_t(vertex[v]) * p.feature_dim + d] = p.feature[size_t(v) * p.feature_dim + d];
            }
            S a(p), b(q); auto x = initial;
            for (int i = 0; i < 51; ++i) {
                if (i) for (int& r : x) r = int(rng() % unsigned(p.k));
                std::vector<int> y(p.n); for (int v = 0; v < p.n; ++v) y[vertex[v]] = label[x[v]];
                auto ea = a.evaluate(x), eb = b.evaluate(y), oracle = naive<double>(p, x);
                check(ea.feasible == oracle.feasible && ea.feasible == eb.feasible, "permuted high-dimensional feasibility");
                if (ea.feasible) {
                    check(close(*ea.cost, *oracle.cost), "high-dimensional independent cost");
                    check(close(*eb.cost, 2.5 * *ea.cost + 3 * p.n), "objective permutation scale and shift");
                }
            }
            CheckedModel model{p}; O o; o.budget_us = -1; o.max_steps = 300;
            auto result = a.improve(initial, o, model);
            check(result.best_cost && close(*result.best_cost, *a.evaluate(a.best_labels(), model).cost), "high-dimensional candidate views");
        }
    }
    static void optimized_cache_tests() {
        for (int seed = 0; seed < 60; ++seed) {
            auto [p, initial] = random_problem(4700 + seed);
            p.contacts.clear(); p.allow_unlisted_contacts = true;
            S solver(p); O o; o.budget_us = -1; o.max_steps = 400; o.seed = unsigned(seed);
            auto r = solver.improve(initial, o);
            auto exact = naive<double>(p, solver.best_labels());
            check(exact.feasible && close(*exact.cost, *r.best_cost), "early rejection objective/rollback");
            for (auto stat : r.statistics.neighborhood)
                check(stat.improved <= stat.accepted && stat.accepted <= stat.feasible && stat.feasible <= stat.proposed, "verified candidate counters");
            CheckedModel model{p}; o.max_steps = 0;
            r = solver.resume(o, model);
            check(r.best_cost && close(*r.best_cost, *solver.evaluate(solver.best_labels(), model).cost), "unused aggregate rebuilt for custom model");
            o.max_steps = 400; r = solver.resume(o, model);
            check(r.best_cost && close(*r.best_cost, *solver.evaluate(solver.best_labels(), model).cost), "custom aggregate incremental/rollback");
            o.max_steps = 0; r = solver.resume(o);
            exact = naive<double>(p, solver.best_labels());
            check(exact.feasible && close(*exact.cost, *r.best_cost), "return to standard model");
            if (!p.edges.empty()) p.edges[0].roles ^= S::Contact;
            solver.update(p); r = solver.resume(o);
            check(r.best_cost.has_value(), "unrestricted contact update");
            r = solver.resume(o, model);
            check(r.best_cost && close(*r.best_cost, *solver.evaluate(solver.best_labels(), model).cost), "updated aggregate rebuilt for custom model");
        }
    }
    template<class Cost> static void construction_candidate_tests() {
        using Solver = ConnectedPartitionSolver<Cost>;
        typename Solver::Problem p(16, 4);
        for (auto& rule : p.regions) rule.connected = false;
        for (int v = 1; v < p.n; ++v) p.edges.push_back({v - 1, v, Cost(v % 5 - 2)});
        for (int v = 0; v < p.n; ++v) for (int r = 0; r < p.k; ++r) p.unary_cost.push_back(Cost((v * 7 + r * 11) % 19 - 9));
        struct OnlyInitial {
            std::vector<int> labels;
            bool extra_feasible(const typename Solver::SummaryView& s) const {
                for (int v = 0; v < s.size(); ++v) if (s.label(v) != labels[v]) return false;
                return true;
            }
        };
        for (int seed = 0; seed < 32; ++seed) {
            Solver first(p); typename Solver::Options o;
            o.budget_us = -1; o.max_steps = 0; o.seed = unsigned(seed);
            auto first_result = first.solve(o);
            check(first_result.best_cost && first_result.statistics.construction_attempts == 1, "zero-step first construction");
            OnlyInitial model{std::vector<int>(first.best_labels().begin(), first.best_labels().end())};
            Solver guarded(p); o.max_steps = 500;
            auto result = guarded.solve(o, model);
            check(result.best_cost && result.best_cost == first_result.best_cost, "retain feasible construction with restrictive model");
            check(std::equal(model.labels.begin(), model.labels.end(), guarded.best_labels().begin()), "invalid alternate construction cannot replace incumbent");
            check(guarded.evaluate(guarded.best_labels(), model).cost == result.best_cost, "construction model objective");
        }
    }
    static void construction_deadline_test() {
        P p(16, 4);
        for (auto& rule : p.regions) rule.connected = false;
        for (int v = 1; v < p.n; ++v) p.edges.push_back({v - 1, v});
        for (int v = 0; v < p.n; ++v) for (int r = 0; r < p.k; ++r) p.unary_cost.push_back(v / 4 == r ? 0 : 10);
        struct SlowAlternative {
            std::vector<int> initial;
            S::Clock::time_point deadline;
            mutable int calls = 0;
            mutable bool exercised = false;
            double global_cost(const S::SummaryView& view) const {
                bool different = false;
                for (int v = 0; v < view.size(); ++v) different |= view.label(v) != initial[v];
                if (++calls == 2) {
                    exercised = different;
                    std::this_thread::sleep_until(deadline + std::chrono::milliseconds(1));
                }
                return different ? -10000 : 10000;
            }
        };
        bool exercised = false;
        for (int seed = 0; seed < 4 && !exercised; ++seed) {
            S first(p); O zero; zero.budget_us = -1; zero.max_steps = 0; zero.seed = unsigned(seed);
            auto initial_result = first.solve(zero);
            SlowAlternative model{std::vector<int>(first.best_labels().begin(), first.best_labels().end()), {}};
            S solver(p); O o; o.seed = unsigned(seed); o.budget_us = 20000;
            model.deadline = S::Clock::now() + std::chrono::microseconds(o.budget_us); o.deadline = model.deadline;
            auto result = solver.solve(o, model); exercised = model.exercised;
            check(result.best_cost && *result.best_cost == *initial_result.best_cost + 10000, "late alternate construction is not adopted");
            check(std::equal(model.initial.begin(), model.initial.end(), solver.best_labels().begin()), "deadline keeps existing feasible candidate");
            check(result.reason == S::StopReason::time_limit, "late construction reports deadline");
        }
        check(exercised, "construction deadline branch exercised");
    }
    class QualityRegressionTests {
        static void check(bool ok, const char* what) {
            if (!ok) { std::cerr << "FAILED: " << what << '\n'; std::abort(); }
        }
        template<class Cost> static void optional_birth() {
            using S = ConnectedPartitionSolver<Cost>;
            typename S::Problem p(8, 2);
            for (int v = 1; v < p.n; ++v) p.edges.push_back({v - 1, v});
            p.regions[0].min_vertices = 2;
            p.regions[1].required = false; p.regions[1].min_vertices = 3;
            p.regions[1].max_vertices = 5; p.activation_cost = {0, -10};
            std::vector<int> initial(p.n, 0);
            typename S::Options o; o.budget_us = -1; o.max_steps = 1000;
            o.weights = {0, 0, 0, 0, 1};
            for (int seed = 0; seed < 16; ++seed) {
                o.seed = seed;
                S solver(p); auto r = solver.improve(initial, o);
                check(r.best_cost && *r.best_cost == Cost{-9}, "birth by Recombine only");
                auto labels = solver.best_labels();
                int ones = int(std::count(labels.begin(), labels.end(), 1));
                int cuts = 0; for (int v = 1; v < p.n; ++v) cuts += labels[v] != labels[v - 1];
                check(3 <= ones && ones <= 5 && cuts == 1, "independent path connectivity and count");
                check(solver.evaluate(labels).cost == r.best_cost, "birth reported objective");
            }
            p.max_active_regions = 1;
            S restricted(p); auto r = restricted.improve(initial, o);
            check(r.best_cost && *r.best_cost == Cost{0}, "active-region limit prevents birth");
            check(std::equal(initial.begin(), initial.end(), restricted.best_labels().begin()), "birth rollback");
            p.max_active_regions = 2;
            std::vector<int> only_one{7}; o.movable = only_one;
            S partial(p); r = partial.improve(initial, o);
            check(r.best_cost && *r.best_cost == Cost{0}, "scope cannot satisfy minimum birth size");
        }
        static void signed_label_resources() {
            using S = ConnectedPartitionSolver<>;
            S::Problem p(8, 2);
            for (int v = 1; v < p.n; ++v) p.edges.push_back({v - 1, v});
            p.load_dim = 2;
            p.regions[0].load_bounds = {{-5, 4}, {5, 8}};
            p.regions[1].required = false; p.regions[1].min_vertices = 3;
            p.regions[1].load_bounds = {{12, 12}, {3, 3}};
            p.activation_cost = {0, -10};
            p.fixed.assign(8, -1); p.fixed[0] = 0; p.domains.push_back({4, {0}});
            struct Model {
                int64_t load(int v, int r, int d) const { return d ? 1 : v - 3 + r; }
                bool extra_feasible(const S::SummaryView& s) const {
                    for (int r = 0; r < 2; ++r) {
                        int64_t sum = 0; int count = 0;
                        for (int v = 0; v < s.size(); ++v) if (s.label(v) == r) { sum += v - 3 + r; ++count; }
                        check(s.region(r).load[0] == sum && s.region(r).load[1] == count, "candidate signed label loads");
                    }
                    return true;
                }
            } model;
            std::vector<int> initial(8, 0), movable{5, 6, 7};
            S::Options o; o.budget_us = -1; o.max_steps = 2000; o.weights = {0, 0, 0, 0, 1}; o.movable = movable;
            for (int seed = 0; seed < 16; ++seed) {
                o.seed = seed; S solver(p); auto r = solver.improve(initial, o, model);
                check(r.best_cost && *r.best_cost == -9, "signed capacity birth");
                for (int v = 0; v < 8; ++v) check(solver.best_labels()[v] == (v >= 5), "unique legal signed-resource cut");
                check(solver.evaluate(solver.best_labels(), model).cost == r.best_cost, "custom model recomputation");
            }
        }
    public:
        static int run() {
            optional_birth<double>(); optional_birth<int64_t>(); signed_label_resources();
            std::cout << "PASS: optional birth, active limit, scope, fixed/domain, signed label resources, rollback\n";
            return 0;
        }
    };
public:
    static int run() {
        api_tests(); enumeration_tests(); random_tests(); scenario_tests(); review_tests(); metamorphic_tests();
        optimized_cache_tests(); construction_candidate_tests<double>(); construction_candidate_tests<int64_t>(); construction_deadline_test();
        QualityRegressionTests::run();
        std::cout << "PASS: API, oracle enumeration, 27000 random evaluations, randomized candidate/rollback/model/update checks\n";
        return 0;
    }
};
int main() { return ConnectedPartitionSolverTests::run(); }
#endif
