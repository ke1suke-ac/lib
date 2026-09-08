#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace bs {

// ============================================================
// detail: 低レベル補助関数
// ============================================================

namespace detail {

// Beamに依存しない時刻取得。
inline std::int64_t now_microseconds() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

template<class S>
inline constexpr bool is_step_arg_v = std::is_same_v<std::remove_cvref_t<S>, int>;

template<class B>
inline constexpr bool is_bool_arg_v = std::is_same_v<std::remove_cvref_t<B>, bool>;

template<class U, class State>
inline constexpr bool is_state_arg_v = std::is_same_v<std::remove_cvref_t<U>, State>;

template<class H, class Hash>
inline constexpr bool is_hash_arg_v =
    std::is_integral_v<std::remove_cvref_t<H>> &&
    std::is_unsigned_v<std::remove_cvref_t<H>> &&
    !std::is_same_v<std::remove_cvref_t<H>, bool> &&
    std::is_convertible_v<std::remove_cvref_t<H>, Hash> &&
    (sizeof(std::remove_cvref_t<H>) <= sizeof(Hash));

} // namespace detail

// ============================================================
// 公開APIと探索本体
// ============================================================

template<class Cost = long long, class Hash = std::uint64_t>
struct Beam {
    using cost_type = Cost;
    using hash_type = Hash;

private:
    template<class State>
    class Engine;

public:

// 探索パラメータ。
// run 開始時に max_turn は 0 以上、beam_width は 1 以上、
// max_step は [1, max(1,max_turn)] に丸められる。
// 解候補は finished=true の候補、または max_turn_is_answer=true のときの max_turn 到達候補。
// finished=true は解候補として採用可能という印であり、自動的な展開停止ではない。
// 終端状態をそれ以上展開したくない場合は、ユーザー側の expand で候補を出さないこと。
// time_limit_ms は soft limit。expand実行中は中断しない。
// 制限到達を検出したturnでは、通常展開を止め、未展開候補から最良1本だけを追加展開する。
// 以後は各turnで最良1本だけを進め、到着候補も最良1件だけをmaterializeする。
struct Param {
    int max_turn = 0;               // 探索終了ターン。候補は 0..max_turn に到着できる。
    int max_step = 1;               // 1回の展開で進める最大ターン。
    int beam_width = 1;             // 各到着ターンに残す候補数。

    int hash_capacity = 0;          // use_hash_dedup=true かつ 0なら自動設定。過大値もbeam_width基準で丸める。

    double time_limit_ms = 0.0;     // 0なら無制限。正なら制限到達検出後に最良1本の縮退探索へ移るsoft limit。
    int time_check_interval = 64;   // 通常ビーム探索中、何ノード展開ごとに時間を見るか。expand中は中断しない。
    bool max_turn_is_answer = true; // trueならmax_turn到達候補も解候補にする。
    bool use_hash_dedup = true;     // trueならHash付きpushで同一到着ターン内の重複排除を行う。Hashなしpushは対象外。
    bool print_warnings = true;     // 現状はhash表満杯など、結果解釈に注意が必要な状態をstderrへ警告する。
};

// Hookの発火タイミング。small版ではdebug_hookは受け取るだけで呼び出さない。
enum class EventType {
    RunStart,
    RunEnd,
    TurnStart,
    TurnEnd,
    BestUpdate,
};

// expand に渡す現在状態の読み取りビュー。
// state はライブラリが保持する状態への const 参照であり、ユーザー側で直接変更しない。
// has_hash=false のとき hash は重複排除に使われていない。ユーザー側でも has_hash を確認して扱うこと。
template<class State>
struct StateView {
    int turn = 0;
    Cost cost{};
    const State& state;
    Hash hash{};
    bool has_hash = false;

    StateView(int turn_, Cost cost_, const State& state_, Hash hash_, bool has_hash_) noexcept
        : turn(turn_), cost(cost_), state(state_), hash(hash_), has_hash(has_hash_) {}
};

// HookやResultから観測できる実行時情報。
// small版ではライブラリ挙動に必要な最小限の情報だけを保持する。
struct Runtime {
    int turn = 0;                    // 現在展開中のターン。RunEndでは最後に展開したターンを表す。
    long long expanded_nodes = 0;    // 展開済みノード数。soft limitの間引き判定にも使う。
    int pending_candidates = 0;      // selector 内に保持中の候補数。探索継続判定にも使う。
    std::int64_t elapsed_us = 0;     // run開始からの経過時間。
    bool time_limit_reached = false; // time_limit_ms到達を検出したか。
    bool hash_table_full = false;    // Hash表満杯による棄却が発生したか。
    bool found = false;
    Cost best_cost{};
    int best_turn = 0;
};

// デフォルトHook。small版では任意のHookも呼び出さない。
struct NoOp {
    void operator()(EventType, const Runtime&) const noexcept {}
};

// CSV出力Hook。small版ではコンストラクタ互換のみを残す空実装とする。
struct CsvStatHook {
    explicit CsvStatHook(std::string csv_filename = "beam_stat.csv", double interval_ms = 50.0) {
        (void)csv_filename;
        (void)interval_ms;
    }
};

// 探索結果。
// アクション列はライブラリ側では保持しない。必要ならユーザー側で State に含める。
// 結果の最良値は best_cost / best_turn に一本化する。
template<class State>
struct Result {
    bool found = false;
    // time_limit_msに達したことを検出したか。
    // 検出後に探索を継続する場合は最良1本の縮退探索へ移る。
    // expand中の即時中断を意味しない。
    bool time_limit_reached = false;
    // max_turn到着候補を少なくとも1件materializeした。
    // 全候補を通常幅で展開した保証ではない。
    bool completed_max_turn = false;
    bool hash_table_full = false;

    Cost best_cost{};
    int best_turn = 0;

    std::optional<State> best_state;
    Runtime runtime;
};

// expand に渡す候補追加器。
// 公開型なので、expand関数の引数型として明示できる。
// push/push_lazy の戻り値 true は、この呼び出し時点で候補がSelectorに入ったことを表す。
// 後続候補に置換される可能性があるため、最終結果に残る保証ではない。
// finished=true は解候補フラグであり、自動的な展開停止ではない。
// push_lazy は候補が受理可能な場合だけ maker() を呼ぶ。maker は保存されない。
template<class State>
class Emitter {
public:
    template<class U, class S>
    auto push(U&& state, Cost cost, S step)
        -> std::enable_if_t<detail::is_state_arg_v<U, State> && detail::is_step_arg_v<S>, bool> {
        return push_state_impl(std::forward<U>(state), cost, now_hash_, static_cast<int>(step), false, false);
    }

    template<class U, class S, class B>
    auto push(U&& state, Cost cost, S step, B finished)
        -> std::enable_if_t<detail::is_state_arg_v<U, State> && detail::is_step_arg_v<S> && detail::is_bool_arg_v<B>, bool> {
        return push_state_impl(std::forward<U>(state), cost, now_hash_, static_cast<int>(step), false, finished);
    }

    template<class U, class H, class S>
    auto push(U&& state, Cost cost, H hash, S step)
        -> std::enable_if_t<detail::is_state_arg_v<U, State> && detail::is_hash_arg_v<H, Hash> && detail::is_step_arg_v<S>, bool> {
        return push_state_impl(std::forward<U>(state), cost, static_cast<Hash>(hash), static_cast<int>(step), true, false);
    }

    template<class U, class H, class S, class B>
    auto push(U&& state, Cost cost, H hash, S step, B finished)
        -> std::enable_if_t<detail::is_state_arg_v<U, State> && detail::is_hash_arg_v<H, Hash> && detail::is_step_arg_v<S> && detail::is_bool_arg_v<B>, bool> {
        return push_state_impl(std::forward<U>(state), cost, static_cast<Hash>(hash), static_cast<int>(step), true, finished);
    }

    template<class S, class Maker>
    auto push_lazy(Cost cost, S step, Maker&& maker)
        -> std::enable_if_t<detail::is_step_arg_v<S>, bool> {
        return push_lazy_impl(cost, now_hash_, static_cast<int>(step), false, false, std::forward<Maker>(maker));
    }

    template<class S, class B, class Maker>
    auto push_lazy(Cost cost, S step, B finished, Maker&& maker)
        -> std::enable_if_t<detail::is_step_arg_v<S> && detail::is_bool_arg_v<B>, bool> {
        return push_lazy_impl(cost, now_hash_, static_cast<int>(step), false, finished, std::forward<Maker>(maker));
    }

    template<class H, class S, class Maker>
    auto push_lazy(Cost cost, H hash, S step, Maker&& maker)
        -> std::enable_if_t<detail::is_hash_arg_v<H, Hash> && detail::is_step_arg_v<S>, bool> {
        return push_lazy_impl(cost, static_cast<Hash>(hash), static_cast<int>(step), true, false, std::forward<Maker>(maker));
    }

    template<class H, class S, class B, class Maker>
    auto push_lazy(Cost cost, H hash, S step, B finished, Maker&& maker)
        -> std::enable_if_t<detail::is_hash_arg_v<H, Hash> && detail::is_step_arg_v<S> && detail::is_bool_arg_v<B>, bool> {
        return push_lazy_impl(cost, static_cast<Hash>(hash), static_cast<int>(step), true, finished, std::forward<Maker>(maker));
    }

private:
    friend class Engine<State>;

    Emitter(Engine<State>& engine, int turn, Hash now_hash) noexcept
        : engine_(engine), turn_(turn), now_hash_(now_hash) {}

    template<class U>
    bool push_state_impl(U&& state, Cost cost, Hash hash, int step, bool has_hash, bool finished) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return engine_.push_candidate(turn_, step, cost, hash, has_hash, finished, maker);
    }

    template<class Maker>
    bool push_lazy_impl(Cost cost, Hash hash, int step, bool has_hash, bool finished, Maker&& maker) {
        return engine_.push_candidate(turn_, step, cost, hash, has_hash, finished, std::forward<Maker>(maker));
    }

    Engine<State>& engine_;
    int turn_ = 0;
    Hash now_hash_{};
};

private:

// ============================================================
// 固定長Hash表。Selector内の重複排除に使う。
// clear は配列初期化ではなく epoch 更新で行う。
// 削除は行わず、置換で生じる古いentryは参照時に候補側と照合して無視する。
// 古いentryで表が詰まった場合だけ rebuild する。
// ============================================================
class FixedHashMap {
public:
    struct Lookup {
        bool found = false;
        int slot = -1;
        int value = -1;
    };

    void clear() {
        ++epoch_;
        if (epoch_ == 0) {
            std::fill(used_.begin(), used_.end(), 0);
            epoch_ = 1;
        }
    }

    Lookup lookup(Hash key) const {
        std::uint64_t x = static_cast<std::uint64_t>(key) + 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        x ^= x >> 31;
        int i = static_cast<int>(x) & mask_;
        for (int probe = 0; probe < capacity_; ++probe) {
            if (used_[i] != epoch_) return Lookup{false, i, -1};
            if (keys_[i] == key) return Lookup{true, i, values_[i]};
            i = (i + 1) & mask_;
        }
        return Lookup{false, -1, -1};
    }

    void set(int slot, Hash key, int value) {
        if (slot < 0) return;
        used_[slot] = epoch_;
        keys_[slot] = key;
        values_[slot] = value;
    }

    int capacity_ = 0;
    int mask_ = 0;
    std::uint32_t epoch_ = 1;
    std::vector<Hash> keys_;
    std::vector<int> values_;
    std::vector<std::uint32_t> used_;
};

// ============================================================
// 状態コピー型 run 本体
// ============================================================

template<class State>
class Engine {
    friend class Emitter<State>;

    struct Item {
        // 選抜時に頻繁に読むメタ情報を先頭に置き、Stateが大きい場合のキャッシュ効率を少し良くする。
        Cost cost{};
        Hash hash{};
        int turn = 0;
        int step = 0;
        bool has_hash = false;
        bool finished = false;
        State state;

        template<class U>
        Item(U&& state_, Cost cost_, Hash hash_, int turn_, int step_, bool has_hash_, bool finished_)
            : cost(cost_), hash(hash_), turn(turn_), step(step_), has_hash(has_hash_), finished(finished_), state(std::forward<U>(state_)) {}
    };

    // 各到着ターンのSelector。push時点で beam_width 個だけを保持する。
    // 幅制限の比較はcost-only。同costでは既存候補を優先し、hashではtie-breakしない。
    // hashは同一到着ターン内の重複排除とStateView用にだけ使う。
    // State生成前に cost/hash で受理可能性を判定するため、push_lazy と通常pushの両方で内部保存の無駄を抑えられる。
    class Selector {
        friend class Engine;

    public:
        [[nodiscard]] bool full() const { return static_cast<int>(candidates_.size()) >= beam_width_; }

    private:
        int beam_width_ = 1;
        int seg_n_ = 1;
        bool use_hash_ = false;
        std::vector<Item> candidates_;
        std::vector<Cost> costs_;
        FixedHashMap hash_;
        std::vector<int> seg_;
        Cost worst_cost_cache{};
        int worst_index_cache = -1;

        // segment treeでは「最悪候補」を上に上げる。同costでは後ろのindexを最悪とし、既存候補を残しやすくする。
        int worse_index(int lhs, int rhs) const {
            if (lhs < 0) return rhs;
            if (rhs < 0) return lhs;
            if (costs_[lhs] != costs_[rhs]) return costs_[lhs] < costs_[rhs] ? rhs : lhs;
            return lhs < rhs ? rhs : lhs;
        }

        template<class Maker>
        void append_candidate(int arrival_turn, int step, Cost cost, Hash hash, bool has_hash, bool finished, Maker&& maker) {
            candidates_.emplace_back(std::invoke(std::forward<Maker>(maker)), cost, hash, arrival_turn, step, has_hash, finished);
            const int j = static_cast<int>(candidates_.size()) - 1;
            costs_[j] = cost;
            if (static_cast<int>(candidates_.size()) == beam_width_) {
                std::fill(seg_.begin(), seg_.end(), -1);
                for (int i = 0; i < beam_width_; ++i) seg_[seg_n_ + i] = i;
                for (int i = seg_n_ - 1; i >= 1; --i) seg_[i] = worse_index(seg_[i << 1], seg_[i << 1 | 1]);
                worst_index_cache = seg_[1];
                if (worst_index_cache >= 0) worst_cost_cache = costs_[worst_index_cache];
            }
        }

        template<class Maker>
        void replace_candidate(int j, int arrival_turn, int step, Cost cost, Hash hash, bool has_hash, bool finished, Maker&& maker) {
            Item& item = candidates_[j];
            item.cost = cost;
            item.hash = hash;
            item.turn = arrival_turn;
            item.step = step;
            item.has_hash = has_hash;
            item.finished = finished;
            item.state = std::invoke(std::forward<Maker>(maker));
            costs_[j] = cost;
            if (full()) {
                int i = seg_n_ + j;
                seg_[i] = j;
                while (i >>= 1) seg_[i] = worse_index(seg_[i << 1], seg_[i << 1 | 1]);
                worst_index_cache = seg_[1];
                if (worst_index_cache >= 0) worst_cost_cache = costs_[worst_index_cache];
            }
        }
    };

public:
    Engine(Param param_, State&& initial_state_, Cost initial_cost_, Hash initial_hash_, bool initial_has_hash_)
        : param(param_),
          initial_state(std::move(initial_state_)),
          initial_cost(initial_cost_),
          initial_hash(initial_hash_),
          initial_has_hash(initial_has_hash_) {
        // ユーザー指定値を内部で扱いやすい範囲へ正規化する。
        // hash_capacity はSelector保持数が高々beam_width個であることを前提に丸める。
        param.max_turn = std::max(0, param.max_turn);
        param.beam_width = std::max(1, param.beam_width);
        param.max_step = std::min(std::max(1, param.max_step), std::max(1, param.max_turn));
        param.time_check_interval = std::max(1, param.time_check_interval);
        if (param.use_hash_dedup) {
            if (param.hash_capacity <= 0) param.hash_capacity = param.beam_width * 4 + 17;
            const int lower = param.beam_width * 2 + 1;
            const int upper = param.beam_width * 4 + 17;
            param.hash_capacity = std::min(std::max(param.hash_capacity, lower), upper);
        } else {
            param.hash_capacity = 0;
        }
    }

    template<class Expand, class DebugHook>
    Result<State> run(Expand&& expand, DebugHook&& debug_hook) {
        (void)debug_hook;
        rt = Runtime{};
        completed_max_turn = false;
        best_state.reset();
        active_states_count = 0;
        rt.pending_candidates = 0;

        start_clock_us = detail::now_microseconds();
        // doubleのms指定を内部比較用のマイクロ秒へ丸める。正の極小値は1usとして扱う。
        if (!(param.time_limit_ms > 0.0)) {
            limit_us = 0;
        } else {
            const double limit_double = param.time_limit_ms * 1000.0;
            const double max_us = static_cast<double>(std::numeric_limits<std::int64_t>::max());
            limit_us = limit_double >= max_us
                ? std::numeric_limits<std::int64_t>::max()
                : std::max<std::int64_t>(1, static_cast<std::int64_t>(limit_double + 0.5));
        }
        use_time_limit = (limit_us > 0);

        // max_step+1 のリングで、現在frontierと未来到着selectorを管理する。
        ring_size = param.max_step + 1;
        frontiers.clear();
        frontiers.resize(ring_size);
        selectors.clear();
        selectors.resize(ring_size);
        for (auto& f : frontiers) f.reserve(std::max(1, param.beam_width));
        for (auto& selector : selectors) {
            selector.beam_width_ = param.beam_width;
            selector.use_hash_ = param.use_hash_dedup;
            selector.candidates_.reserve(selector.beam_width_);
            selector.costs_.assign(selector.beam_width_, Cost{});
            if (selector.use_hash_) {
                auto& h = selector.hash_;
                int cap = std::max(param.hash_capacity, selector.beam_width_ * 2 + 1);
                cap = std::max(4, cap);
                h.capacity_ = 1;
                while (h.capacity_ < cap) h.capacity_ <<= 1;
                h.mask_ = h.capacity_ - 1;
                h.keys_.assign(static_cast<std::size_t>(h.capacity_), Hash{});
                h.values_.assign(static_cast<std::size_t>(h.capacity_), -1);
                h.used_.assign(static_cast<std::size_t>(h.capacity_), 0);
                h.epoch_ = 1;
            }
            selector.seg_n_ = 1;
            while (selector.seg_n_ < selector.beam_width_) selector.seg_n_ <<= 1;
            selector.seg_.assign(selector.seg_n_ * 2, -1);
            selector.worst_index_cache = -1;
        }

        frontiers[0].push_back(Item(std::move(initial_state), initial_cost, initial_hash, 0, 0, initial_has_hash, false));
        active_states_count = 1;
        rt.best_cost = initial_cost;
        rt.best_turn = 0;
        update_time();
        if (use_time_limit && rt.elapsed_us >= limit_us) rt.time_limit_reached = true;
        if (param.max_turn_is_answer && param.max_turn == 0) set_best(frontiers[0][0]);
        completed_max_turn = (param.max_turn == 0);

        if (!completed_max_turn && !use_time_limit) {
            // time_limit_ms==0用の通常探索ホットパス。縮退探索用の分岐を混ぜない。
            for (int current_turn = 0; current_turn < param.max_turn; ++current_turn) {
                rt.turn = current_turn;
                auto& cur = frontiers[current_turn % ring_size];
                const int n = static_cast<int>(cur.size());
                for (int i = 0; i < n; ++i) expand_item_body(cur[i], current_turn, expand);
                clear_frontier(current_turn);
                materialize_arrival<false>(current_turn + 1);
                if (completed_max_turn) break;
                if (active_states_count == 0 && rt.pending_candidates == 0) break;
            }
        } else if (!completed_max_turn) {
            // soft limit有効時の探索。制限到達後は各ターンで最良1本だけを進め、max_turn到達を試す。
            bool minimal_mode = rt.time_limit_reached;
            for (int current_turn = 0; current_turn < param.max_turn; ++current_turn) {
                rt.turn = current_turn;
                mark_time_limit_if_elapsed();
                if (rt.time_limit_reached) minimal_mode = true;

                auto& cur = frontiers[current_turn % ring_size];
                int next_index = 0;
                if (!minimal_mode) {
                    for (; next_index < static_cast<int>(cur.size()); ++next_index) {
                        expand_item_body(cur[next_index], current_turn, expand);
                        if ((rt.expanded_nodes % param.time_check_interval) == 0 && mark_time_limit_if_elapsed()) {
                            minimal_mode = true;
                            ++next_index;
                            break;
                        }
                    }
                }
                if (minimal_mode) {
                    const int best = best_item_index(cur, next_index);
                    if (best >= 0) expand_item_body(cur[best], current_turn, expand);
                }

                clear_frontier(current_turn);
                if (minimal_mode) materialize_arrival<true>(current_turn + 1);
                else materialize_arrival<false>(current_turn + 1);
                mark_time_limit_if_elapsed();
                if (rt.time_limit_reached) minimal_mode = true;
                if (completed_max_turn) break;
                if (active_states_count == 0 && rt.pending_candidates == 0) break;
            }
        }

        update_time();
        Result<State> res;
        res.found = rt.found;
        res.time_limit_reached = rt.time_limit_reached;
        res.completed_max_turn = completed_max_turn;
        res.hash_table_full = rt.hash_table_full;
        res.best_cost = rt.best_cost;
        res.best_turn = rt.best_turn;
        res.best_state = std::move(best_state);
        res.runtime = rt;
        if (param.print_warnings && res.hash_table_full) {
            std::cerr << "[bs] warning: hash_table_full; hash_capacity=" << param.hash_capacity
                      << ", beam_width=" << param.beam_width << '\n';
        }
        return res;
    }

private:
    Param param;
    State initial_state;
    Cost initial_cost{};
    Hash initial_hash{};
    bool initial_has_hash = false;

    Runtime rt;
    std::int64_t start_clock_us = 0;
    std::int64_t limit_us = 0;
    bool use_time_limit = false;

    std::vector<std::vector<Item>> frontiers;
    std::vector<Selector> selectors;
    std::optional<State> best_state;

    int ring_size = 2;
    int active_states_count = 0;
    bool completed_max_turn = false;

    void update_time() {
        rt.elapsed_us = detail::now_microseconds() - start_clock_us;
    }

    bool mark_time_limit_if_elapsed() {
        if (!use_time_limit) return false;
        update_time();
        if (rt.elapsed_us < limit_us) return false;
        rt.time_limit_reached = true;
        return true;
    }

    template<class Maker>
    bool push_candidate(int turn, int step, Cost cost, Hash hash, bool has_hash, bool finished, Maker&& maker) {
        if constexpr (std::is_floating_point_v<Cost>) {
            if (std::isnan(cost)) return false;
        }
        if (step <= 0 || step > param.max_step || turn + step > param.max_turn) return false;

        const int arrival = turn + step;
        Selector& selector = selectors[arrival % ring_size];
        // Stateを生成する前に、幅枝刈りで確実に落ちる候補を除外する。
        if (selector.full() && selector.worst_index_cache >= 0 && !(cost < selector.worst_cost_cache)) return false;

        if (!(selector.use_hash_ && has_hash)) {
            if (!selector.full()) {
                selector.append_candidate(arrival, step, cost, hash, has_hash, finished, std::forward<Maker>(maker));
                ++rt.pending_candidates;
                return true;
            }
            const int j = selector.worst_index_cache;
            if (j < 0) return false;
            selector.replace_candidate(j, arrival, step, cost, hash, has_hash, finished, std::forward<Maker>(maker));
            return true;
        }

        auto found = selector.hash_.lookup(hash);
        if (found.found) {
            const int j = found.value;
            if (0 <= j && j < static_cast<int>(selector.candidates_.size()) && selector.candidates_[j].has_hash && selector.candidates_[j].hash == hash) {
                if (!(cost < selector.costs_[j])) return false;
                selector.replace_candidate(j, arrival, step, cost, hash, true, finished, std::forward<Maker>(maker));
                selector.hash_.set(found.slot, hash, j);
                return true;
            }
            // stale entry なので、この slot を上書きしてよい。候補本体は既に別候補で置換されている。
        }

        if (found.slot < 0) {
            selector.hash_.clear();
            for (int i = 0; i < static_cast<int>(selector.candidates_.size()); ++i) {
                if (!selector.candidates_[i].has_hash) continue;
                auto slot = selector.hash_.lookup(selector.candidates_[i].hash);
                if (slot.slot >= 0) selector.hash_.set(slot.slot, selector.candidates_[i].hash, i);
            }
            found = selector.hash_.lookup(hash);
            if (found.slot < 0) {
                rt.hash_table_full = true;
                return false;
            }
        }

        if (!selector.full()) {
            const int j = static_cast<int>(selector.candidates_.size());
            selector.append_candidate(arrival, step, cost, hash, true, finished, std::forward<Maker>(maker));
            selector.hash_.set(found.slot, hash, j);
            ++rt.pending_candidates;
            return true;
        }

        const int j = selector.worst_index_cache;
        if (j < 0) return false;
        selector.replace_candidate(j, arrival, step, cost, hash, true, finished, std::forward<Maker>(maker));
        selector.hash_.set(found.slot, hash, j);
        return true;
    }

    template<class Expand>
    void expand_item_body(Item& item, int current_turn, Expand& expand) {
        StateView<State> now(current_turn, item.cost, item.state, item.hash, item.has_hash);
        Emitter<State> emit(*this, current_turn, item.hash);
        if constexpr (std::is_invocable_v<Expand&, const StateView<State>&, const Runtime&, Emitter<State>&>) {
            expand(now, static_cast<const Runtime&>(rt), emit);
        } else {
            expand(now, emit);
        }
        ++rt.expanded_nodes;
    }

    // 縮退探索で使うcost-onlyの最良候補選択。同costでは先に残っている候補を優先する。
    static int best_item_index(const std::vector<Item>& items, int begin = 0) {
        if (begin >= static_cast<int>(items.size())) return -1;
        int best = begin;
        for (int i = begin + 1; i < static_cast<int>(items.size()); ++i) {
            if (items[i].cost < items[best].cost) best = i;
        }
        return best;
    }

    void clear_frontier(int current_turn) {
        auto& cur = frontiers[current_turn % ring_size];
        active_states_count -= static_cast<int>(cur.size());
        if (active_states_count < 0) active_states_count = 0;
        cur.clear();
    }

    void set_best(const Item& item) {
        // best更新もcost-only。同costの解候補では先に見つかったものを残す。
        if (rt.found && !(item.cost < rt.best_cost)) return;
        best_state.emplace(item.state);
        rt.found = true;
        rt.best_cost = item.cost;
        rt.best_turn = item.turn;
        update_time();
    }

    template<bool Minimal>
    void materialize_arrival(int arrival_turn) {
        // 候補は到着ターンでmaterializeされたときに初めてanswer判定する。
        // Minimal=false: Selector内の全候補を次frontierへ移す。
        // Minimal=true : Selector内の最良cost 1件だけを移し、他は破棄する。
        Selector& selector = selectors[arrival_turn % ring_size];
        auto& selected = selector.candidates_;
        const int original_count = static_cast<int>(selected.size());
        int selected_width = original_count;
        int best_idx = -1;
        if constexpr (Minimal) {
            best_idx = best_item_index(selected);
            selected_width = best_idx >= 0 ? 1 : 0;
        }

        auto& dst = frontiers[arrival_turn % ring_size];
        dst.clear();
        if (dst.capacity() < static_cast<std::size_t>(param.beam_width)) dst.reserve(param.beam_width);

        if constexpr (Minimal) {
            if (best_idx >= 0) materialize_one(selected[best_idx], dst);
        } else {
            for (Item& item : selected) materialize_one(item, dst);
        }
        // completed_max_turn は「max_turn到着候補を実体化した」ことを表す。通常幅で全展開した保証ではない。
        if (selected_width > 0 && arrival_turn == param.max_turn) completed_max_turn = true;

        rt.pending_candidates = std::max(0, rt.pending_candidates - original_count);
        active_states_count += selected_width;
        selected.clear();
        selector.worst_index_cache = -1;
        if (selector.use_hash_) selector.hash_.clear();
    }

    // best_stateはmove前にコピーする。これにより返却されるbest_stateはfrontierの寿命に依存しない。
    void materialize_one(Item& item, std::vector<Item>& dst) {
        if (item.finished || (param.max_turn_is_answer && item.turn == param.max_turn)) set_best(item);
        dst.push_back(std::move(item));
    }
};

public:
    // Hashあり初期状態版。
    // expand は (const StateView<State>&, Emitter<State>&)、または
    // (const StateView<State>&, const Runtime&, Emitter<State>&) を受け取れる。
    // Runtime は const 参照固定であり、expand から Runtime を変更することはできない。
    // 履歴が必要な場合はユーザー側で State に含める。
    template<class State, class H, class Expand, class DebugHook = NoOp>
    static auto run(
        const Param& param,
        State initial_state,
        Cost initial_cost,
        H initial_hash,
        Expand&& expand,
        DebugHook&& debug_hook = DebugHook{}
    ) -> std::enable_if_t<detail::is_hash_arg_v<H, Hash>, Result<State>> {
        Engine<State> engine(param, std::move(initial_state), initial_cost, static_cast<Hash>(initial_hash), true);
        return engine.run(std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));
    }

    // Hashなし初期状態版。
    // Hash付きpushを行うまでは has_hash=false として扱われ、重複排除されない。
    template<class State, class Expand, class DebugHook = NoOp>
    static auto run(
        const Param& param,
        State initial_state,
        Cost initial_cost,
        Expand&& expand,
        DebugHook&& debug_hook = DebugHook{}
    ) -> std::enable_if_t<!detail::is_hash_arg_v<Expand, Hash>, Result<State>> {
        Engine<State> engine(param, std::move(initial_state), initial_cost, Hash{}, false);
        return engine.run(std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));
    }
};

} // namespace bs


#if __INCLUDE_LEVEL__ == 0

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <thread>

namespace {

using BS = bs::Beam<int, std::uint64_t>;
using BS32 = bs::Beam<int, std::uint32_t>;

static std::int64_t test_now_microseconds() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

#define BS_TEST_ASSERT(expr) do { \
    if (!(expr)) { \
        std::cerr << "[test] assertion failed: " #expr << " at line " << __LINE__ << std::endl; \
        std::abort(); \
    } \
} while (false)

struct SmallState {
    int x = 0;
    std::vector<int> actions;
};

struct TinyState {
    int v = 0;
    int checksum = 0;
};

struct RefItem {
    TinyState state;
    int cost = 0;
    std::uint64_t hash = 0;
    int turn = 0;
};

static std::uint64_t test_mix(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

static void ref_push(std::vector<RefItem>& bucket, int beam_width, const RefItem& cand, bool use_hash) {
    if (use_hash) {
        for (RefItem& old : bucket) {
            if (old.hash == cand.hash) {
                if (cand.cost < old.cost) old = cand;
                return;
            }
        }
    }
    if (static_cast<int>(bucket.size()) < beam_width) {
        bucket.push_back(cand);
        return;
    }
    int worst = 0;
    for (int i = 1; i < static_cast<int>(bucket.size()); ++i) {
        if (bucket[worst].cost < bucket[i].cost || (bucket[worst].cost == bucket[i].cost && worst < i)) worst = i;
    }
    if (cand.cost < bucket[worst].cost) bucket[worst] = cand;
}

static std::pair<int, TinyState> run_reference(int seed, int max_turn, int beam_width, int max_step, int branch, bool use_hash) {
    std::vector<std::vector<RefItem>> frontier(max_turn + 1);
    std::vector<std::vector<RefItem>> buckets(max_turn + 1);
    TinyState init{seed % 17, seed * 3 + 1};
    frontier[0].push_back(RefItem{init, 0, static_cast<std::uint64_t>(init.v), 0});
    bool found = false;
    int best_cost = 0;
    TinyState best{};

    for (int t = 0; t < max_turn; ++t) {
        for (const RefItem& now : frontier[t]) {
            for (int k = 0; k < branch; ++k) {
                int step = 1 + static_cast<int>(test_mix(seed * 1000003ULL + now.state.v * 131ULL + t * 17ULL + k) % max_step);
                if (t + step > max_turn) step = max_turn - t;
                TinyState nx;
                nx.v = static_cast<int>((now.state.v * 37LL + now.state.checksum * 11LL + k * 19LL + step * 7LL + seed) % 1009);
                nx.checksum = static_cast<int>((now.state.checksum * 43LL + nx.v * 5LL + k + step) % 1000003);
                int delta = static_cast<int>((test_mix(nx.v + nx.checksum * 3ULL + k * 1009ULL + seed) % 41));
                int cost = now.cost + delta;
                std::uint64_t h = use_hash ? static_cast<std::uint64_t>(nx.v) : now.hash;
                ref_push(buckets[t + step], beam_width, RefItem{nx, cost, h, t + step}, use_hash);
            }
        }
        frontier[t + 1] = buckets[t + 1];
        if (t + 1 == max_turn) {
            for (const RefItem& item : frontier[t + 1]) {
                if (!found || item.cost < best_cost) {
                    found = true;
                    best_cost = item.cost;
                    best = item.state;
                }
            }
        }
    }
    BS_TEST_ASSERT(found || max_turn == 0);
    return {best_cost, best};
}

static void test_edge_cases() {
    {
        BS::Param param;
        param.max_turn = 0;
        param.print_warnings = false;
        SmallState init{42, {}};
        auto expand = [](const BS::StateView<SmallState>&, auto&) { BS_TEST_ASSERT(false); };
        auto res = BS::run<SmallState>(param, init, 7, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 7);
        BS_TEST_ASSERT(res.best_turn == 0);
        BS_TEST_ASSERT(res.best_state->x == 42);
    }
    {
        BS::Param param;
        param.max_turn = 3;
        param.max_turn_is_answer = false;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>&, auto&) {};
        auto res = BS::run<SmallState>(param, SmallState{}, 0, expand);
        BS_TEST_ASSERT(!res.found);
        BS_TEST_ASSERT(res.runtime.expanded_nodes == 1);
    }
    {
        BS::Param param;
        param.max_turn = 2;
        param.max_step = 2;
        param.beam_width = 1;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.turn == 0) {
                SmallState a = now.state;
                a.x = 100;
                emit.push(std::move(a), 100, std::uint64_t{100}, 2);
                SmallState b = now.state;
                b.x = 1;
                emit.push(std::move(b), 1, std::uint64_t{1}, 1);
            } else if (now.turn == 1 && now.state.x == 1) {
                SmallState c = now.state;
                c.x = 2;
                emit.push(std::move(c), 2, std::uint64_t{2}, 1);
            }
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 2);
        BS_TEST_ASSERT(res.best_state->x == 2);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 10;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            SmallState a = now.state; a.x = 10;
            SmallState b = now.state; b.x = 5;
            emit.push(std::move(a), 10, std::uint64_t{123}, 1);
            emit.push(std::move(b), 5, std::uint64_t{123}, 1);
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 5);
        BS_TEST_ASSERT(res.best_state->x == 5);
    }
    {
        BS32::Param param;
        param.max_turn = 1;
        param.beam_width = 4;
        param.print_warnings = false;
        auto expand = [](const BS32::StateView<SmallState>& now, BS32::Emitter<SmallState>& emit) {
            BS_TEST_ASSERT(now.has_hash);
            BS_TEST_ASSERT(now.hash == std::uint32_t{9});
            SmallState a = now.state; a.x = 20;
            SmallState b = now.state; b.x = 3;
            emit.push(std::move(a), 20, std::uint32_t{555}, 1);
            emit.push(std::move(b), 3, std::uint32_t{555}, 1);
        };
        auto res = BS32::run<SmallState>(param, SmallState{}, 0, std::uint32_t{9}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 3);
        BS_TEST_ASSERT(res.best_state->x == 3);

        auto expand_no_initial_hash = [](const BS32::StateView<SmallState>& now, BS32::Emitter<SmallState>& emit) {
            BS_TEST_ASSERT(!now.has_hash);
            SmallState s = now.state;
            s.x = 8;
            emit.push(std::move(s), 8, std::uint32_t{8}, 1);
        };
        auto res2 = BS32::run<SmallState>(param, SmallState{}, 0, expand_no_initial_hash);
        BS_TEST_ASSERT(res2.found);
        BS_TEST_ASSERT(res2.best_state->x == 8);
    }
    {
        using BSD = bs::Beam<double, std::uint64_t>;
        BSD::Param param;
        param.max_turn = 1;
        param.beam_width = 2;
        param.print_warnings = false;
        auto expand = [](const BSD::StateView<SmallState>& now, BSD::Emitter<SmallState>& emit) {
            SmallState bad = now.state;
            bad.x = -1;
            emit.push(std::move(bad), std::numeric_limits<double>::quiet_NaN(), std::uint64_t{1}, 1);
            SmallState good = now.state;
            good.x = 5;
            emit.push(std::move(good), 3.0, std::uint64_t{2}, 1);
        };
        auto res = BSD::run<SmallState>(param, SmallState{}, 0.0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 3.0);
        BS_TEST_ASSERT(res.best_state->x == 5);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 1;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            SmallState a = now.state; a.x = 10;
            SmallState b = now.state; b.x = 5;
            emit.push(std::move(a), 10, std::uint64_t{10}, 1);
            emit.push(std::move(b), 5, std::uint64_t{5}, 1);
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 5);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 1;
        param.print_warnings = false;
        int maker_calls = 0;
        auto expand = [&](const BS::StateView<SmallState>& now, auto& emit) {
            emit.push_lazy(1, std::uint64_t{1}, 1, [&]() {
                ++maker_calls;
                SmallState s = now.state;
                s.x = 1;
                return s;
            });
            emit.push_lazy(100, std::uint64_t{2}, 1, [&]() {
                ++maker_calls;
                SmallState s = now.state;
                s.x = 100;
                return s;
            });
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 1);
        BS_TEST_ASSERT(maker_calls == 1);
    }
    {
        BS::Param param;
        param.max_turn = 5;
        param.max_turn_is_answer = false;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, const BS::Runtime& rt, auto& emit) {
            BS_TEST_ASSERT(now.turn == rt.turn);
            if (now.turn == 0) {
                SmallState s = now.state;
                s.x = 77;
                emit.push(std::move(s), 3, std::uint64_t{77}, 1, true);
            }
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_turn == 1);
        BS_TEST_ASSERT(res.best_cost == 3);
        BS_TEST_ASSERT(res.best_state->x == 77);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.max_step = 1;
        param.beam_width = 3;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            SmallState s = now.state;
            emit.push(s, 1, 0);  // invalid step
            emit.push(s, 2, 2);  // invalid step
            emit.push(s, 3, 1);  // valid
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 3);
    }
    {
        BS::Param param;
        param.max_turn = 2;
        param.beam_width = 3;
        param.time_limit_ms = 2000;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, const BS::Runtime& rt, auto& emit) {
            BS_TEST_ASSERT(now.turn == rt.turn);
            if (now.turn + 1 <= 2) {
                SmallState s = now.state;
                s.x += 1;
                emit.push(std::move(s), now.cost + 1, static_cast<std::uint64_t>(now.turn + 1), 1);
            }
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_turn == 2);
    }
    {
        struct CountingHook {
            int calls = 0;
            void operator()(BS::EventType, const BS::Runtime&) { ++calls; }
        } hook;
        BS::Param param;
        param.max_turn = 1;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            SmallState s = now.state;
            s.x = 1;
            emit.push(std::move(s), 1, std::uint64_t{1}, 1);
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand, hook);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(hook.calls == 0);
    }
    {
        const char* filename = "beam_copy_multi_small_v34_stat.csv";
        std::remove(filename);
        BS::Param param;
        param.max_turn = 1;
        param.print_warnings = false;
        BS::CsvStatHook hook(filename, 0.0);
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            SmallState s = now.state;
            s.x = 1;
            emit.push(std::move(s), 1, std::uint64_t{1}, 1);
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand, hook);
        BS_TEST_ASSERT(res.found);
        std::ifstream ifs(filename);
        BS_TEST_ASSERT(!ifs.good());
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 1;
        param.time_limit_ms = 1;
        param.time_check_interval = 1;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            SmallState s = now.state;
            s.x = 1;
            emit.push(std::move(s), 1, 1);
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(res.time_limit_reached);
    }
    {
        BS::Param param;
        param.max_turn = 3;
        param.beam_width = 4;
        param.time_limit_ms = 1;
        param.time_check_interval = 1;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.turn == 0) std::this_thread::sleep_for(std::chrono::milliseconds(3));
            for (int k = 0; k < 4; ++k) {
                SmallState s = now.state;
                s.x = now.state.x * 10 + k;
                const int cost = now.cost + k;
                emit.push(std::move(s), cost, static_cast<std::uint64_t>(100 + s.x), 1);
            }
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.time_limit_reached);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_turn == 3);
        BS_TEST_ASSERT(res.best_cost == 0);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 1;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            SmallState a = now.state; a.x = 10;
            SmallState b = now.state; b.x = 1;
            emit.push(std::move(a), 5, std::uint64_t{100}, 1);
            emit.push(std::move(b), 5, std::uint64_t{1}, 1);
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 5);
        BS_TEST_ASSERT(res.best_state->x == 10);
    }
    {
        BS::Param param;
        param.max_turn = 2;
        param.max_step = 100;
        param.beam_width = 5;
        param.hash_capacity = 1000000;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            SmallState s = now.state;
            s.x += 1;
            emit.push(std::move(s), now.cost + 1, std::uint64_t{static_cast<unsigned>(now.turn + 1)}, 1);
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_turn == 2);
    }
    {
        BS::Param param;
        param.max_turn = 0;
        param.max_step = 100;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>&, auto&) { BS_TEST_ASSERT(false); };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, expand);
        BS_TEST_ASSERT(res.completed_max_turn);
    }
}

static void test_random_cases() {
    std::mt19937 rng(1234567);
    for (int case_id = 0; case_id < 400; ++case_id) {
        int max_turn = 1 + static_cast<int>(rng() % 10);
        int max_step = 1 + static_cast<int>(rng() % 4);
        max_step = std::min(max_step, max_turn);
        int beam_width = 1 + static_cast<int>(rng() % 12);
        int branch = 1 + static_cast<int>(rng() % 14);
        bool use_hash = (rng() & 1U) != 0;
        int seed = static_cast<int>(rng() % 100000);

        BS::Param param;
        param.max_turn = max_turn;
        param.max_step = max_step;
        param.beam_width = beam_width;
        param.use_hash_dedup = use_hash;
        param.print_warnings = false;

        TinyState init{seed % 17, seed * 3 + 1};
        auto expand = [=](const BS::StateView<TinyState>& now, auto& emit) {
            for (int k = 0; k < branch; ++k) {
                int step = 1 + static_cast<int>(test_mix(seed * 1000003ULL + now.state.v * 131ULL + now.turn * 17ULL + k) % max_step);
                if (now.turn + step > max_turn) step = max_turn - now.turn;
                TinyState nx;
                nx.v = static_cast<int>((now.state.v * 37LL + now.state.checksum * 11LL + k * 19LL + step * 7LL + seed) % 1009);
                nx.checksum = static_cast<int>((now.state.checksum * 43LL + nx.v * 5LL + k + step) % 1000003);
                int delta = static_cast<int>((test_mix(nx.v + nx.checksum * 3ULL + k * 1009ULL + seed) % 41));
                int cost = now.cost + delta;
                if (use_hash) emit.push(nx, cost, static_cast<std::uint64_t>(nx.v), step);
                else emit.push(nx, cost, step);
            }
        };
        auto res = use_hash
            ? BS::run<TinyState>(param, init, 0, static_cast<std::uint64_t>(init.v), expand)
            : BS::run<TinyState>(param, init, 0, expand);
        auto ref = run_reference(seed, max_turn, beam_width, max_step, branch, use_hash);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == ref.first);
        BS_TEST_ASSERT(res.best_state->v == ref.second.v);
        BS_TEST_ASSERT(res.best_state->checksum == ref.second.checksum);
    }
}

struct BenchStateTrivial {
    int v = 0;
    std::uint64_t h = 0;
    std::array<int, 16> data{};
};

struct BenchStateVector {
    std::vector<int> data;
    int v = 0;
};

static long long median_us(std::vector<long long> xs) {
    std::sort(xs.begin(), xs.end());
    return xs[xs.size() / 2];
}

static long long benchmark_trivial_state_once(double time_limit_ms) {
    BS::Param param;
    param.max_turn = 80;
    param.max_step = 3;
    param.beam_width = 256;
    param.time_limit_ms = time_limit_ms;
    param.print_warnings = false;

    auto run_once = [&]() {
        BenchStateTrivial init;
        auto expand = [](const BS::StateView<BenchStateTrivial>& now, auto& emit) {
            for (int k = 0; k < 20; ++k) {
                int step = 1 + (k % 3);
                if (now.turn + step > 80) step = 80 - now.turn;
                BenchStateTrivial nx = now.state;
                nx.v = static_cast<int>((nx.v * 33LL + k * 17LL + step * 5LL + now.turn) & 0x7fffffff);
                nx.h = test_mix(now.hash + static_cast<std::uint64_t>(k + 1) * 1000003ULL + step);
                nx.data[k & 15] += nx.v + step;
                int cost = now.cost + static_cast<int>(nx.h % 97);
                emit.push(std::move(nx), cost, nx.h, step);
            }
        };
        auto beg = test_now_microseconds();
        auto res = BS::run<BenchStateTrivial>(param, init, 0, std::uint64_t{0}, expand);
        auto end = test_now_microseconds();
        BS_TEST_ASSERT(res.found);
        return end - beg;
    };

    std::vector<long long> times;
    for (int i = 0; i < 7; ++i) times.push_back(run_once());
    return median_us(times);
}

static long long benchmark_vector_state_lazy_once(double time_limit_ms) {
    BS::Param param;
    param.max_turn = 50;
    param.max_step = 2;
    param.beam_width = 128;
    param.time_limit_ms = time_limit_ms;
    param.print_warnings = false;

    auto run_once = [&]() {
        BenchStateVector init;
        init.data.assign(64, 0);
        auto expand = [](const BS::StateView<BenchStateVector>& now, auto& emit) {
            for (int k = 0; k < 18; ++k) {
                int step = 1 + (k & 1);
                if (now.turn + step > 50) step = 50 - now.turn;
                int nv = static_cast<int>((now.state.v * 31LL + k * 13LL + step + now.turn) % 1000003);
                std::uint64_t h = test_mix(static_cast<std::uint64_t>(nv));
                int cost = now.cost + static_cast<int>(h % 89);
                emit.push_lazy(cost, h, step, [&]() {
                    BenchStateVector nx = now.state;
                    nx.v = nv;
                    nx.data[static_cast<std::size_t>(k) % nx.data.size()] += nv;
                    return nx;
                });
            }
        };
        auto beg = test_now_microseconds();
        auto res = BS::run<BenchStateVector>(param, init, 0, std::uint64_t{0}, expand);
        auto end = test_now_microseconds();
        BS_TEST_ASSERT(res.found);
        return end - beg;
    };

    std::vector<long long> times;
    for (int i = 0; i < 7; ++i) times.push_back(run_once());
    return median_us(times);
}

static void benchmark_trivial_state() {
    std::cout << "benchmark trivial_state time_limit_ms=0 median_us=" << benchmark_trivial_state_once(0) << '\n';
    std::cout << "benchmark trivial_state time_limit_ms=2000 median_us=" << benchmark_trivial_state_once(2000) << '\n';
}

static void benchmark_vector_state_lazy() {
    std::cout << "benchmark vector_state_push_lazy time_limit_ms=0 median_us=" << benchmark_vector_state_lazy_once(0) << '\n';
    std::cout << "benchmark vector_state_push_lazy time_limit_ms=2000 median_us=" << benchmark_vector_state_lazy_once(2000) << '\n';
}

} // namespace

int main() {
    test_edge_cases();
    test_random_cases();
    benchmark_trivial_state();
    benchmark_vector_state_lazy();
    std::cout << "all tests passed" << std::endl;
    return 0;
}

#endif // __INCLUDE_LEVEL__ == 0
