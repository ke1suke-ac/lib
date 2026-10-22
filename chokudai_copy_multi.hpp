#pragma once
#include <bits/stdc++.h>

namespace beam_history_detail {
// 出力用履歴だけを共有する。祖先のStateは保持しない。
// indexで参照するためvector再確保でもLink/Viewは有効。Action参照は再取得が必要。
template<class Value>
class Pool {
public:
    static constexpr std::size_t none = std::numeric_limits<std::size_t>::max();
private:
    struct Node {
        std::optional<Value> value;
        std::size_t parent = none, refs = 0;
    };
    std::vector<Node> nodes_;
    std::size_t free_ = none;
    void retain(std::size_t id) noexcept { if (id != none) ++nodes_[id].refs; }
    void release(std::size_t id) noexcept {
        // 長い経路も再帰せず解放。各ノードの解放費用は生成回数に償却される。
        while (id != none && --nodes_[id].refs == 0) {
            auto& node = nodes_[id];
            const auto parent = node.parent;
            node.value.reset(); node.parent = free_; free_ = id;
            id = parent;
        }
    }
public:
    class Link {
        friend class Pool;
        Pool* pool_ = nullptr;
        std::size_t id_ = none;
        Link(Pool* pool, std::size_t id) noexcept : pool_(pool), id_(id) {}
    public:
        Link() = default;
        Link(const Link& other) noexcept : pool_(other.pool_), id_(other.id_) {
            if (pool_) pool_->retain(id_);
        }
        Link(Link&& other) noexcept
            : pool_(std::exchange(other.pool_, nullptr)), id_(other.id_) {}
        Link& operator=(Link other) noexcept {
            swap(other);
            return *this;
        }
        ~Link() { if (pool_) pool_->release(id_); }
        void swap(Link& other) noexcept {
            std::swap(pool_, other.pool_); std::swap(id_, other.id_);
        }
        std::size_t id() const noexcept { return pool_ ? id_ : none; }
    };
    Pool() = default;
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    template<class... Args>
    Link make(std::size_t parent, Args&&... args) {
        std::size_t id = free_;
        if (id == none) {
            id = nodes_.size();
            nodes_.push_back(Node{std::optional<Value>(std::in_place, std::forward<Args>(args)...), parent, 1});
        } else {
            auto& node = nodes_[id];
            node.value.emplace(std::forward<Args>(args)...);
            free_ = node.parent; node.parent = parent; node.refs = 1;
        }
        retain(parent);
        return Link(this, id);
    }
    const Value& value(std::size_t id) const noexcept { return *nodes_[id].value; }
    std::size_t parent(std::size_t id) const noexcept { return nodes_[id].parent; }
};
template<class State, class Value>
struct TracedState {
    State state;
    typename Pool<Value>::Link trace;
};
} // namespace beam_history_detail

// コピー型 chokudai サーチ（コスト最小化、C++20、単一スレッド）
// 各ターンに最大 W=beam_width 件を保持し、1スイープで各ターンから
// 最大K件を展開する。初回のKはfirst_sweep_width、以降はsweep_width。
// 初回幅の0以下は通常幅を使う。未展開候補は次スイープへ持ち越し、時間切れの救済幅は1。
// State のコピー・移動を O(S)、最大ターンを T とするとメモリは O(T W S)
// ターンキューは初回の有効候補で確保する。その初回確保は O(W S)
// ハッシュは同じ到着ターンの未展開候補だけを重複排除し、展開済み集合は持たない。
// is_answer は解候補の印。途中の is_answer 状態も expand されるので、必要ならそこで打ち切る。
// 最良有効解は幅・Hashの選別と独立して保存する。操作列はAction型指定時だけ共有木で保持する。
// pushのboolは探索候補の受理を表す。falseでも最良解を保存する場合がある。
// push_lazyのmakerは探索候補または最良解の保存に必要な場合だけ、1回呼ぶ。
// 時間制限は expand の途中では検査しない。Runtime の時刻は直近の検査値。

namespace cs {
template<class T>
concept CostType = ((std::integral<T> && std::is_signed_v<T>) || std::floating_point<T>) &&
                   !std::same_as<T, bool>;
template<class T>
concept HashType = std::integral<T> && std::is_unsigned_v<T> && !std::same_as<T, bool> &&
                   sizeof(T) <= sizeof(std::uint64_t);
template<class H, class Hash>
concept CompatibleHashArg = HashType<Hash> && HashType<std::remove_cvref_t<H>> &&
    std::convertible_to<std::remove_cvref_t<H>, Hash> && sizeof(std::remove_cvref_t<H>) <= sizeof(Hash);
template<class B> concept BoolArg = std::same_as<std::remove_cvref_t<B>, bool>;
template<class S> concept StepArg = std::same_as<std::remove_cvref_t<S>, int>;
template<class U, class State> concept StateArg = std::same_as<std::remove_cvref_t<U>, State>;
} // namespace cs

namespace cs {
namespace multi_detail {
#if __INCLUDE_LEVEL__ == 0
// 単体実行時だけの仮想時計。通常includeでは領域・分岐とも存在しない。
struct TestClock {
    static inline bool enabled = false;
    static inline std::int64_t value = 0;
    static inline std::uint64_t calls = 0;
};
#endif
// 単調時計の現在時刻を取得する、O(1)
inline std::int64_t now_us() {
#if __INCLUDE_LEVEL__ == 0
    ++TestClock::calls;
    if (TestClock::enabled) return TestClock::value;
#endif
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
// ミリ秒を飽和付きでマイクロ秒へ変換する、O(1)
inline std::int64_t to_us(double ms) {
    if (!(ms > 0.0)) return 0;
    const long double us = static_cast<long double>(ms) * 1000.0L;
    const auto upper = std::numeric_limits<std::int64_t>::max();
    if (us >= static_cast<long double>(upper) - 0.5L) return upper;
    return std::max<std::int64_t>(1, static_cast<std::int64_t>(us + 0.5L));
}
// 整数ハッシュを拡散する、O(1)
inline std::uint64_t mix(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
} // namespace multi_detail

template<CostType Cost = long long, HashType Hash = std::uint64_t, bool EnableDiagnostics = true>
struct ChokudaiCopyMulti {
    using cost_type = Cost;
    using hash_type = Hash;

    // 診断の保持・更新・hook通知を切り替える。探索に必要なRuntimeは常に有効。
#ifdef LOCAL
    static constexpr bool diagnostics_enabled = EnableDiagnostics;
#else
    static constexpr bool diagnostics_enabled = false;
#endif
private:
    template<class State> class Engine;
public:
    // 既存の Param の並びを維持し、chokudai 固有項目を末尾に追加
    struct Param {
        int max_turn = 0;            // 負なら 0
        int max_step = 1;            // 最大遷移ターン数
        int beam_width = 1;          // 各ターンの保持上限。1 以上へ丸める
        int hash_capacity = 0;       // 0 は自動。実際には W 基準で丸めて確保
        double time_limit_ms = 0.0;  // 0 以下・NaN は無制限、expand は中断しない
        int time_check_interval = 64;
        bool continue_on_timeout = true; // 時間切れで解未発見なら救済探索を続ける。
        bool expand_answers = true; // falseなら有効解を保存のみ行い、探索候補へ登録しない。
        bool max_turn_is_answer = true;
        bool use_hash_dedup = true;
        int sweep_width = 1;      // 各ターン・各スイープの取り出し上限。1..beam_widthへ丸める
        std::int64_t max_sweeps = 0; // 0 以下は無制限
        int first_sweep_width = 0; // 初回だけの取出し上限。0以下はsweep_width、正値は1..beam_width
    };
    enum class EventType { RunStart, TurnEnd, SearchEnd, RunEnd, SweepStart, SweepEnd };
    enum class StopReason { None, Completed, Exhausted, TimeLimit, SweepLimit, InvalidInitial };
    template<class State>
    struct StateView {
        int turn = 0;
        Cost cost{};
        const State& state;
        Hash hash{};
        bool has_hash = false;
        // 現在状態の読み取り専用ビューを構築する、O(1)
        StateView(int t, Cost c, const State& s, Hash h, bool hh) noexcept
            : turn(t), cost(c), state(s), hash(h), has_hash(hh) {}
    };
        // expand と Result が見る本番用情報。診断カウンタは保持しない。
    struct Runtime {
        Param param;
        int turn = 0;
        std::int64_t elapsed_us = 0;
        std::uint64_t expanded_nodes = 0;
        bool time_limit_reached = false;
        bool found = false;
        Cost best_cost{};
        int best_turn = 0;
        bool completed_max_turn = false;
        StopReason stop_reason = StopReason::None;
        unsigned diagnostic_flags = 0; // 1: Hash表容量不足、2: 遅延削除キュー容量不足。
        std::int64_t sweep = 0;
    };
    struct SelectionStats {
        std::uint64_t accepted = 0, rejected_by_width = 0, rejected_by_hash = 0;
        std::uint64_t replaced_by_width = 0, replaced_by_hash = 0, hash_checked = 0;
        std::uint64_t rejected_by_capacity = 0;
        std::uint64_t attempts() const {
            return accepted + rejected_by_width + rejected_by_hash + rejected_by_capacity;
        }
    };
    // debug hook にだけ渡す情報。全カウンタは run 開始からの累計。
    struct DebugRuntime : Runtime {
        std::uint64_t generated_candidates = 0, invalid_candidates = 0, answer_candidates = 0;
        std::uint64_t best_update_count = 0, expanded_after_timeout = 0;
        // 最終改善時刻はexpand終了時に確定。expand中の改善は-1で未確定を表す。
        std::int64_t first_solution_us = -1, last_best_update_us = -1;
        std::size_t frontier_size = 0, frontier_limit = 0, frontier_expanded = 0;
        std::size_t pending_candidates = 0, peak_pending_candidates = 0;
        SelectionStats selection;
        std::size_t hash_table_capacity = 0; // 1選別領域の実効バケット数。未確保でも確保時の容量。
        bool sweep_completed = false;
        std::int64_t completed_sweeps = 0;
        std::uint64_t generated_step_sum = 0, expanded_step_sum = 0;
        int generated_step_max = 0;
        int pending_turns = 0, full_pending_turns = 0;
    };

    struct NoOp {
        // イベントを無視する、O(1)
        void operator()(EventType, const Runtime&) const noexcept {}
    };
    // LOCAL定義時のみCSVを記録する。未定義時は同じ呼び出し方で使える空実装。
        // 診断の有効・無効は外側の diagnostics_enabled で決まる。
    struct CsvStatHook {
#ifdef LOCAL
        struct Row { EventType event; DebugRuntime runtime; };
        std::string filename;
        std::int64_t interval_us;
        std::vector<Row> rows;
        explicit CsvStatHook(std::string path = "beam_stat.csv", double interval_ms = 50)
            : filename(std::move(path)), interval_us(multi_detail::to_us(interval_ms)) {}
        void operator()(EventType event, const DebugRuntime& rt) {
            // CSVはsweep単位。独自hookへのTurnEnd/SweepStart通知は維持する。
            if (event == EventType::TurnEnd || event == EventType::SweepStart) return;
            if (event == EventType::RunStart) rows.clear();
            const bool mandatory = event == EventType::RunStart || event == EventType::SearchEnd ||
                event == EventType::RunEnd || (event == EventType::SweepEnd && (rt.sweep == 1 || !rt.sweep_completed));
            const bool changed = !rows.empty() &&
                (rt.diagnostic_flags != rows.back().runtime.diagnostic_flags ||
                 (rt.time_limit_reached && !rows.back().runtime.time_limit_reached) );
            if (mandatory || changed || rows.empty() || interval_us == 0 ||
                rt.elapsed_us - rows.back().runtime.elapsed_us >= interval_us) rows.push_back({event, rt});
            if (event == EventType::RunEnd) write();
        }
    private:
        static const char* event_name(EventType e) {
            switch(e) {
            case EventType::RunStart: return "RunStart";
            case EventType::SearchEnd: return "SearchEnd";
            case EventType::RunEnd: return "RunEnd";
            case EventType::SweepEnd: return "SweepEnd";
            default: return "";
            }
        }
        static const char* reason_name(StopReason r) {
            switch(r) {
            case StopReason::None: return "None";
            case StopReason::Completed: return "Completed";
            case StopReason::Exhausted: return "Exhausted";
            case StopReason::TimeLimit: return "TimeLimit";
            case StopReason::SweepLimit: return "SweepLimit";
            case StopReason::InvalidInitial: return "InvalidInitial";
            }
            return "";
        }
        static const char* flags(unsigned f) {
            return f == 0 ? "" : f == 1 ? "HashCapacityExceeded" : f == 2 ? "RemovalQueueOverflow" : "HashCapacityExceeded|RemovalQueueOverflow";
        }
        void write() const {
            std::ofstream out(filename, std::ios::binary | std::ios::trunc);
            if (!out) throw std::runtime_error("CSV file could not be opened: " + filename);
            out.imbue(std::locale::classic());
            out << std::setprecision(std::numeric_limits<long double>::max_digits10);
            bool first = true;
            auto cell = [&](const auto& value) {
                if (!first) out << ',';
                first = false;
                if constexpr (std::integral<std::remove_cvref_t<decltype(value)>>) out << +value;
                else out << value;
            };
            auto end = [&] { out << '\n'; first = true; };
            auto opt = [&](bool valid, const auto& value) { if (valid) cell(value); else cell(""); };
            auto ratio = [&](long double n, long double d, int scale) {
                if (d > 0) cell(n / d * scale); else cell("");
            };
            auto optional_ratio = [&](bool valid, long double n, long double d, int scale) {
                if (valid) ratio(n, d, scale); else cell("");
            };
            auto ms = [](std::int64_t us) { return static_cast<double>(us) * 0.001; };
            auto write_selection = [&](const SelectionStats& a, const SelectionStats& b) {
                const auto attempts = a.attempts() - b.attempts();
                ratio((a.rejected_by_width - b.rejected_by_width) + (a.replaced_by_width - b.replaced_by_width), attempts, 100);
                ratio((a.rejected_by_hash - b.rejected_by_hash) + (a.replaced_by_hash - b.replaced_by_hash), attempts, 100);
                cell(attempts);
            };
            for (const char* name : {
                // 経過・観測位置
                "elapsed_ms", "time_used_pct", "period_ms", "event",
                "scope", "sweep",
                // 解の品質・改善
                "best_cost", "best_gain", "best_updates", "best_unchanged_ms",
                "found", "best_turn", "answer_candidates", "first_solution_ms",
                "first_solution_budget_pct",
                // 探索量・速度
                "expand_per_sec", "expanded_nodes", "candidates_per_expand", "generated_candidates",
                "invalid_candidates",
                // 幅・Hashによる選別
                "width_discard_pct", "hash_discard_pct", "selection_attempts",
                // 残候補・方式別の負荷
                "pending_candidates", "peak_pending_candidates", "pending_turns", "full_pending_turn_pct",
                "generated_step_avg", "expanded_step_avg",
                // スイープの完了・初回と追加探索
                "sweep_completed", "completed_sweeps", "first_sweep_completed", "first_sweep_found",
                "first_sweep_ms", "first_sweep_budget_pct", "refinement_ms", "best_gain_after_first_sweep",
                // 終了・時間切れ・異常
                "stop_reason", "completed_max_turn", "time_limit_reached", "overrun_ms",
                "finalize_ms", "expanded_after_timeout", "diagnostic_flags",
                // 設定
                "beam_width", "first_sweep_width", "sweep_width", "max_sweeps",
                "max_turn", "max_step", "time_limit_ms", "time_check_interval",
                "continue_on_timeout", "use_hash_dedup", "hash_table_capacity", "max_turn_is_answer",
                "expand_answers",
                // 方式・スキーマの識別
                "algorithm", "state_mode", "turn_mode", "schema_version"
            }) cell(name);
            end();
            DebugRuntime zero{};
            const DebugRuntime* previous = &zero;
            const DebugRuntime* search_end = nullptr;
            const DebugRuntime* first_sweep = nullptr;
            for (const auto& row : rows) {
                const auto& r = row.runtime;
                if (row.event == EventType::SearchEnd) search_end = &r;
                if (row.event == EventType::SweepEnd && r.sweep == 1) first_sweep = &r;
                const bool total = row.event == EventType::RunEnd;
                const auto& s = total && search_end ? *search_end : r;
                const auto& p = total ? zero : *previous;
                const auto dt = s.elapsed_us - p.elapsed_us;
                const auto expanded = s.expanded_nodes - p.expanded_nodes;
                const auto generated = s.generated_candidates - p.generated_candidates;
                cell(ms(r.elapsed_us));
                ratio(ms(r.elapsed_us), r.param.time_limit_ms, 100);
                cell(ms(dt));
                cell(event_name(row.event));
                cell(total ? "search_total" : "interval");
                cell(r.sweep);
                opt(r.found, r.best_cost);
                opt(s.found && p.found, (p.best_cost == s.best_cost ? 0.0L : static_cast<long double>(p.best_cost) - static_cast<long double>(s.best_cost)));
                cell(s.best_update_count - p.best_update_count);
                opt(s.found, ms(s.elapsed_us - s.last_best_update_us));
                cell(r.found);
                opt(r.found, r.best_turn);
                cell(s.answer_candidates - p.answer_candidates);
                opt(s.first_solution_us >= 0, ms(s.first_solution_us));
                optional_ratio(s.first_solution_us >= 0, ms(s.first_solution_us), r.param.time_limit_ms, 100);
                ratio(expanded * 1000000.0L, dt, 1);
                cell(expanded);
                ratio(generated, expanded, 1);
                cell(generated);
                cell(s.invalid_candidates - p.invalid_candidates);
                write_selection(s.selection, p.selection);
                cell(s.pending_candidates);
                cell(s.peak_pending_candidates);
                cell(s.pending_turns);
                ratio(s.full_pending_turns, s.pending_turns, 100);
                ratio(s.generated_step_sum - p.generated_step_sum, generated - (s.invalid_candidates - p.invalid_candidates), 1);
                ratio(s.expanded_step_sum - p.expanded_step_sum,
                    expanded - (p.expanded_nodes == 0 && s.expanded_nodes > 0), 1);
                opt(row.event == EventType::SweepEnd, r.sweep_completed);
                cell(s.completed_sweeps - p.completed_sweeps);
                opt(total && first_sweep, first_sweep && first_sweep->sweep_completed);
                opt(total && first_sweep, first_sweep && first_sweep->found);
                opt(total && first_sweep, ms(first_sweep ? first_sweep->elapsed_us : 0));
                optional_ratio(total && first_sweep, ms(first_sweep ? first_sweep->elapsed_us : 0), r.param.time_limit_ms, 100);
                opt(total && first_sweep && first_sweep->sweep_completed, ms(s.elapsed_us - (first_sweep ? first_sweep->elapsed_us : 0)));
                opt(total && first_sweep && first_sweep->sweep_completed && first_sweep->found && s.found, (first_sweep && first_sweep->best_cost == s.best_cost ? 0.0L : static_cast<long double>(first_sweep ? first_sweep->best_cost : Cost{}) - static_cast<long double>(s.best_cost)));
                cell(reason_name(r.stop_reason));
                cell(r.completed_max_turn);
                cell(r.time_limit_reached);
                opt(total && r.param.time_limit_ms > 0, std::max(0.0, ms(r.elapsed_us) - r.param.time_limit_ms));
                opt(total && search_end, ms(r.elapsed_us - s.elapsed_us));
                cell(s.expanded_after_timeout - p.expanded_after_timeout);
                cell(flags(r.diagnostic_flags));
                cell(r.param.beam_width);
                cell(r.param.first_sweep_width);
                cell(r.param.sweep_width);
                cell(r.param.max_sweeps);
                cell(r.param.max_turn);
                cell(r.param.max_step);
                cell(r.param.time_limit_ms);
                cell(r.param.time_check_interval);
                cell(r.param.continue_on_timeout);
                cell(r.param.use_hash_dedup);
                cell(r.hash_table_capacity);
                cell(r.param.max_turn_is_answer);
                cell(r.param.expand_answers);
                cell("chokudai");
                cell("copy");
                cell("multi");
                cell(6);
                end();
                previous = &r;
            }
            out.close();
            if (!out) throw std::runtime_error("CSV write failed: " + filename);
        }
#else
        explicit CsvStatHook(std::string = "beam_stat.csv", double = 50) {}
        void operator()(EventType, const Runtime&) const noexcept {}
#endif
    };


    template<class State, class Action = void> struct Result;
    template<class State> struct Result<State, void> {
        bool found = false;
        bool time_limit_reached = false;

        bool completed_max_turn = false; // 不正でない最大ターン候補が1件でも生成された
        Cost best_cost{};
        int best_turn = 0;
        std::optional<State> best_state;
        Runtime runtime;
        bool exhausted = false;
        bool sweep_limit_reached = false;
    };
    template<class State, class Action = void> class Emitter;
    template<class State> class Emitter<State, void> {
    public:
    // 状態を候補として追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class U, class S>
    requires StateArg<U, State> && StepArg<S>
    bool push(U&& state, Cost cost, S step) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return engine_.push_candidate(turn_, step, cost, now_hash_, false, false, maker);
    }

    // 状態を候補として追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class U, class S, class B>
    requires StateArg<U, State> && StepArg<S> && BoolArg<B>
    bool push(U&& state, Cost cost, S step, B is_answer) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return engine_.push_candidate(turn_, step, cost, now_hash_, false, is_answer, maker);
    }

    // 状態を候補として追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class U, class H, class S>
    requires StateArg<U, State> && CompatibleHashArg<H, Hash> && StepArg<S>
    bool push(U&& state, Cost cost, H hash, S step) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return engine_.push_candidate(turn_, step, cost, static_cast<Hash>(hash), true, false, maker);
    }

    // 状態を候補として追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class U, class H, class S, class B>
    requires StateArg<U, State> && CompatibleHashArg<H, Hash> && StepArg<S> && BoolArg<B>
    bool push(U&& state, Cost cost, H hash, S step, B is_answer) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return engine_.push_candidate(turn_, step, cost, static_cast<Hash>(hash), true, is_answer, maker);
    }

    // 探索候補または最良解の保存時だけ状態を生成する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class Maker, class S>
    requires StepArg<S>
    bool push_lazy(Cost cost, S step, Maker&& maker) {
        return engine_.push_candidate(turn_, step, cost, now_hash_, false, false, std::forward<Maker>(maker));
    }

    // 探索候補または最良解の保存時だけ状態を生成する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class Maker, class S, class B>
    requires StepArg<S> && BoolArg<B>
    bool push_lazy(Cost cost, S step, B is_answer, Maker&& maker) {
        return engine_.push_candidate(turn_, step, cost, now_hash_, false, is_answer, std::forward<Maker>(maker));
    }

    // 探索候補または最良解の保存時だけ状態を生成する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class Maker, class H, class S>
    requires CompatibleHashArg<H, Hash> && StepArg<S>
    bool push_lazy(Cost cost, H hash, S step, Maker&& maker) {
        return engine_.push_candidate(turn_, step, cost, static_cast<Hash>(hash), true, false, std::forward<Maker>(maker));
    }

    // 探索候補または最良解の保存時だけ状態を生成する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class Maker, class H, class S, class B>
    requires CompatibleHashArg<H, Hash> && StepArg<S> && BoolArg<B>
    bool push_lazy(Cost cost, H hash, S step, B is_answer, Maker&& maker) {
        return engine_.push_candidate(turn_, step, cost, static_cast<Hash>(hash), true, is_answer, std::forward<Maker>(maker));
    }

    private:
        friend class Engine<State>;
        Emitter(Engine<State>& e, int t, Hash h) noexcept : engine_(e), turn_(t), now_hash_(h) {}
        Engine<State>& engine_;
        int turn_;
        Hash now_hash_;
    };

public:
    template<class Action> struct Edge {
        Action action;
        int turn, step;
        int next_turn() const noexcept { return turn + step; }
    };
    template<class Action> class HistoryView {
        template<class, class> friend class Emitter;
        using Pool = beam_history_detail::Pool<Edge<Action>>;
        const Pool* pool_;
        std::size_t id_;
        HistoryView(const Pool& pool, std::size_t id) noexcept : pool_(&pool), id_(id) {}
    public:
        // expand内だけ有効。Action参照はpush/push_lazyをまたいで保持しない。
        bool empty() const noexcept { return id_ == Pool::none; }
        const Action& action() const noexcept { assert(!empty()); return pool_->value(id_).action; }
        HistoryView parent() const noexcept { assert(!empty()); return {*pool_, pool_->parent(id_)}; }
    };
    template<class State, class Action>
    struct Result : Result<State, void> {
        std::vector<Edge<Action>> path;
    };
    // Action型指定時だけ有効。Stateと操作列は分離し、祖先Stateは保持しない。
    template<class State, class Action> class Emitter {
        friend struct ChokudaiCopyMulti;
        using Value = Edge<Action>;
        using Pool = beam_history_detail::Pool<Value>;
        using Stored = beam_history_detail::TracedState<State, Value>;
        Emitter<Stored, void>& raw_;
        Pool& pool_;
        std::size_t parent_;
        int turn_;
        Emitter(Emitter<Stored, void>& raw, Pool& pool, std::size_t parent, int turn) noexcept
            : raw_(raw), pool_(pool), parent_(parent), turn_(turn) {}
        template<class A, class Maker>
        bool emit(A&& action, Cost cost, Hash hash, bool has_hash, bool is_answer, int step, Maker&& maker) {
            auto traced = [&]() {
                return Stored{State(std::invoke(std::forward<Maker>(maker))), pool_.make(parent_, Value{std::forward<A>(action), turn_, step})};
            };

            if (has_hash) return raw_.push_lazy(cost, hash, step, is_answer, traced);
            return raw_.push_lazy(cost, step, is_answer, traced);
        }
    public:
        HistoryView<Action> history() const noexcept { return {pool_, parent_}; }
        template<class U, class A, class S>
        requires StateArg<U, State> && std::same_as<std::remove_cvref_t<A>, Action> && StepArg<S>
        bool push(U&& state, A&& action, Cost cost, S step) {
            return emit(std::forward<A>(action), cost, Hash{}, false, false, step, [&]() -> decltype(auto) { return std::forward<U>(state); });
        }
        template<class A, class S, class Maker>
        requires std::same_as<std::remove_cvref_t<A>, Action> && std::invocable<Maker&&> && StepArg<S>
        bool push_lazy(A&& action, Cost cost, S step, Maker&& maker) {
            return emit(std::forward<A>(action), cost, Hash{}, false, false, step, std::forward<Maker>(maker));
        }
        template<class U, class A, class S, class B>
        requires StateArg<U, State> && std::same_as<std::remove_cvref_t<A>, Action> && StepArg<S> && BoolArg<B>
        bool push(U&& state, A&& action, Cost cost, S step, B is_answer) {
            return emit(std::forward<A>(action), cost, Hash{}, false, is_answer, step, [&]() -> decltype(auto) { return std::forward<U>(state); });
        }
        template<class A, class S, class B, class Maker>
        requires std::same_as<std::remove_cvref_t<A>, Action> && std::invocable<Maker&&> && StepArg<S> && BoolArg<B>
        bool push_lazy(A&& action, Cost cost, S step, B is_answer, Maker&& maker) {
            return emit(std::forward<A>(action), cost, Hash{}, false, is_answer, step, std::forward<Maker>(maker));
        }
        template<class U, class A, class H, class S>
        requires StateArg<U, State> && std::same_as<std::remove_cvref_t<A>, Action> && CompatibleHashArg<H, Hash> && StepArg<S>
        bool push(U&& state, A&& action, Cost cost, H hash, S step) {
            return emit(std::forward<A>(action), cost, static_cast<Hash>(hash), true, false, step, [&]() -> decltype(auto) { return std::forward<U>(state); });
        }
        template<class A, class H, class S, class Maker>
        requires std::same_as<std::remove_cvref_t<A>, Action> && std::invocable<Maker&&> && CompatibleHashArg<H, Hash> && StepArg<S>
        bool push_lazy(A&& action, Cost cost, H hash, S step, Maker&& maker) {
            return emit(std::forward<A>(action), cost, static_cast<Hash>(hash), true, false, step, std::forward<Maker>(maker));
        }
        template<class U, class A, class H, class S, class B>
        requires StateArg<U, State> && std::same_as<std::remove_cvref_t<A>, Action> && CompatibleHashArg<H, Hash> && StepArg<S> && BoolArg<B>
        bool push(U&& state, A&& action, Cost cost, H hash, S step, B is_answer) {
            return emit(std::forward<A>(action), cost, static_cast<Hash>(hash), true, is_answer, step, [&]() -> decltype(auto) { return std::forward<U>(state); });
        }
        template<class A, class H, class S, class B, class Maker>
        requires std::same_as<std::remove_cvref_t<A>, Action> && std::invocable<Maker&&> && CompatibleHashArg<H, Hash> && StepArg<S> && BoolArg<B>
        bool push_lazy(A&& action, Cost cost, H hash, S step, B is_answer, Maker&& maker) {
            return emit(std::forward<A>(action), cost, static_cast<Hash>(hash), true, is_answer, step, std::forward<Maker>(maker));
        }
    };

private:
    template<class State> class Engine {
        static_assert(std::copy_constructible<State>, "State must be copy constructible");
        static_assert(std::assignable_from<State&, State>, "State must be assignable");
        struct Key {
            Cost cost{};
            Hash hash{};
            // 使用中は受理順、空きスロットでは次の空き位置を格納する。
            union { std::uint64_t serial = 0; int next_free; };
            bool has_hash = false;
            [[no_unique_address]] std::conditional_t<diagnostics_enabled, int, std::monostate> incoming_step{};
        };
        struct Ends { int best = -1; int worst = -1; };
        // 状態本体と比較用情報を分け、最良・最悪の両端を同じ木で管理する
        class Queue {
        public:
            std::vector<std::optional<State>> items;
            std::vector<Key> keys;
            int free_head = -1;
            std::vector<Ends> tree;
            std::vector<int> table; // スロット番号。-1=空
            std::size_t base;
            std::size_t hash_size = 0;
            int size = 0;
            // 空のターンキューを確保する、O(W)
            Queue(int width, std::size_t capacity)
                : base(std::bit_ceil(static_cast<std::size_t>(width))), hash_size(capacity) {
                keys.reserve(static_cast<std::size_t>(width));
                items.reserve(static_cast<std::size_t>(width));
                // 葉の情報は items から読み、木には内部ノードだけを置く。
                // State の領域は予約し、実際に使うスロットから構築する。
                tree.resize(base);
            }
            int free_slot() const { return free_head < 0 ? static_cast<int>(items.size()) : free_head; }
            // 非空キューの最良スロットを取得する、O(1)
            int best() const { return base == 1 ? 0 : tree[1].best; }
            // 非空キューの最悪スロットを取得する、O(1)
            int worst() const { return base == 1 ? 0 : tree[1].worst; }
            // キーに対応する現在保持中のスロットを探す、期待 O(1)
            int find(Hash hash) const {
                if (table.empty()) return -1;
                std::size_t pos = bucket(hash);
                while (table[pos] != -1) {
                    if (keys[static_cast<std::size_t>(table[pos])].hash == hash) return table[pos];
                    pos = (pos + 1) & (table.size() - 1);
                }
                return -1;
            }
            // 指定スロットを取り除く、期待 O(log W + S)
            void erase(int slot, bool updating = true) {
                // 線形探索の穴を詰め、削除マークと定期再構築を不要にする
                const auto id = static_cast<std::size_t>(slot);
                if (keys[id].has_hash && !table.empty()) {
                    std::size_t pos = bucket(keys[id].hash);
                    while (table[pos] != slot) pos = (pos + 1) & (table.size() - 1);
                    const auto mask = table.size() - 1;
                    std::size_t next = (pos + 1) & mask;
                    while (table[next] >= 0) {
                        const auto home = bucket(keys[static_cast<std::size_t>(table[next])].hash);
                        if (((pos - home) & mask) < ((next - home) & mask)) {
                            table[pos] = table[next];
                            pos = next;
                        }
                        next = (next + 1) & mask;
                    }
                    table[pos] = -1;
                }
                items[id].reset();
                --size;
                keys[id].next_free = free_head;
                free_head = slot;
                if (updating) update(slot);
            }
            // 検査済みの候補を指定スロットへ格納する、期待償却 O(log W + S)
            template<class Maker>
            void insert(int slot, Cost cost, Hash hash, bool hashed, std::uint64_t serial, Maker&& maker) {
                // maker の例外が出た場合には古い候補をまだ取り除いていない
                State made(std::invoke(std::forward<Maker>(maker)));
                const auto id = static_cast<std::size_t>(slot);
                if (id == items.size()) { items.emplace_back(); keys.emplace_back(); }
                if (items[id]) erase(slot, false);
                if (free_head >= 0) free_head = keys[id].next_free;
                items[id].emplace(std::move(made));
                keys[id] = Key{cost, hash, serial, hashed};
                ++size;
                update(slot);
                if (hashed && hash_size > 0) {
                    if (table.empty()) table.assign(hash_size, -1);
                    std::size_t pos = bucket(hash);
                    while (table[pos] >= 0) pos = (pos + 1) & (table.size() - 1);
                    table[pos] = slot;
                }
            }
        private:
            bool before(int a, int b) const {
                const Key& x = keys[static_cast<std::size_t>(a)];
                const Key& y = keys[static_cast<std::size_t>(b)];
                if (x.cost != y.cost) return x.cost < y.cost;
                return x.serial < y.serial;
            }
            void update(int slot) {
                // 空の子を除外して、同点なら受理順が早いものを best にする
                std::size_t pos = base + static_cast<std::size_t>(slot);
                for (pos /= 2; pos > 0; pos /= 2) {
                    auto child = [&](std::size_t i) {
                        if (i < base) return tree[i];
                        const auto j = i - base;
                        return j < items.size() && items[j] ? Ends{static_cast<int>(j), static_cast<int>(j)} : Ends{};
                    };
                    const Ends a = child(pos * 2), b = child(pos * 2 + 1);
                    if (a.best < 0) tree[pos] = b;
                    else if (b.best < 0) tree[pos] = a;
                    else tree[pos] = Ends{before(a.best, b.best) ? a.best : b.best,
                                          before(a.worst, b.worst) ? b.worst : a.worst};
                }
            }
            std::size_t bucket(Hash hash) const {
                return static_cast<std::size_t>(multi_detail::mix(static_cast<std::uint64_t>(hash))) & (table.size() - 1);
            }
        };
        using ActiveRuntime = std::conditional_t<diagnostics_enabled, DebugRuntime, Runtime>;
        ActiveRuntime rt_;
        std::optional<State> best_state_;
        std::vector<std::unique_ptr<Queue>> layers_;
        std::unique_ptr<Queue> spare_; // 空になったキューの領域を次のターンへ再利用する。
        std::uint64_t pending_ = 0;
        std::uint64_t serial_ = 0;
        std::size_t hash_size_ = 0;
        std::int64_t start_us_ = 0;
        std::int64_t limit_us_ = 0;
        int unchecked_ = 0;

    public:
        // 初期状態とキューを準備する、O(T + W S)
        Engine(const Param& param, State&& initial, Cost cost, Hash hash, bool hashed) {
            rt_.param = param;
            start_us_ = multi_detail::now_us();
            // サイズ計算は符号付き int の加算・乗算を避ける

            rt_.param.max_turn = std::max(0, rt_.param.max_turn);
            rt_.param.beam_width = std::max(1, rt_.param.beam_width);
            rt_.param.sweep_width = std::clamp(rt_.param.sweep_width, 1, rt_.param.beam_width);
            rt_.param.first_sweep_width = rt_.param.first_sweep_width <= 0 ? rt_.param.sweep_width :
                std::min(rt_.param.first_sweep_width, rt_.param.beam_width);
            rt_.param.time_check_interval = std::max(1, rt_.param.time_check_interval);
            rt_.param.max_step = std::clamp(rt_.param.max_step, 1, std::max(1, rt_.param.max_turn));
            limit_us_ = multi_detail::to_us(rt_.param.time_limit_ms);
            if (rt_.param.use_hash_dedup) {
                const auto w = static_cast<std::uint64_t>(rt_.param.beam_width);
                // 削除時に穴を詰めるため、使用率を1/2以下にすれば空きバケットを保てる。
                const auto requested = rt_.param.hash_capacity <= 0 ? 2 * w : static_cast<std::uint64_t>(rt_.param.hash_capacity);
                const auto capacity = std::clamp(requested, 2 * w, 4 * w + 17);
                hash_size_ = std::bit_ceil(capacity);
                if constexpr (diagnostics_enabled) rt_.hash_table_capacity = hash_size_;
            }
            if (!valid_cost(cost)) { rt_.stop_reason = StopReason::InvalidInitial; return; }
            rt_.completed_max_turn = rt_.param.max_turn == 0;
            if (rt_.completed_max_turn) {
                if (rt_.param.max_turn_is_answer) save_best(std::move(initial), cost, 0);
                return;
            }
            layers_.resize(static_cast<std::size_t>(rt_.param.max_turn));
            layer(0).insert(0, cost, hash, hashed, serial_++, [&]() -> State&& { return std::move(initial); });
            pending_ = 1; peak();
        }
        // 候補を検査し、探索候補または最良解の保存に必要なら状態を生成する、期待償却 O(log W + S)
        template<class Maker>
        bool push_candidate(int from, int step, Cost cost, Hash hash, bool has_hash, bool is_answer, Maker&& maker) {
            static_assert(std::invocable<Maker&&>, "maker must be callable without arguments");
            static_assert(std::constructible_from<State, std::invoke_result_t<Maker&&>>, "maker must produce State");
            if constexpr (diagnostics_enabled) ++rt_.generated_candidates;
            // 到着ターンの加算前に検査し、巨大な step でもオーバーフローさせない
            if (!valid_cost(cost) || step <= 0 || step > rt_.param.max_step || step > rt_.param.max_turn - from) {
                if constexpr (diagnostics_enabled) ++rt_.invalid_candidates;
                return false;
            }
            if constexpr (diagnostics_enabled) {
                rt_.generated_step_sum += static_cast<unsigned>(step);
                rt_.generated_step_max = std::max(rt_.generated_step_max, step);
            }
            const int turn = from + step;
            const bool answer = is_answer || (rt_.param.max_turn_is_answer && turn == rt_.param.max_turn);
            if constexpr (diagnostics_enabled) rt_.answer_candidates += answer;
            // 棄却する候補でも、改善する有効解なら保存する。makerは受理側と合わせて高々1回。
            auto save_answer = [&]() {
                if (!rt_.found || cost < rt_.best_cost)
                    save_best(std::invoke(std::forward<Maker>(maker)), cost, turn);
            };
            if (turn == rt_.param.max_turn || (!rt_.param.expand_answers && is_answer)) {
                if (turn == rt_.param.max_turn) { rt_.completed_max_turn = true;  }

                if (answer) save_answer();
                return false; // 最良解だけを保存し、キューを作らない。
            }
            Queue& q = layer(turn);
            const bool full = q.size == rt_.param.beam_width;
            // 満杯時は幅による棄却を先に判定する。同点は既存候補を優先する
            if (full && !(cost < q.keys[static_cast<std::size_t>(q.worst())].cost)) {
                if constexpr (diagnostics_enabled) ++rt_.selection.rejected_by_width;
                if (answer) save_answer();
                return false;
            }
            if constexpr (diagnostics_enabled) rt_.selection.hash_checked += has_hash && rt_.param.use_hash_dedup;
            int slot = has_hash ? q.find(hash) : -1;
            const bool duplicate = slot >= 0;
            if (duplicate && !(cost < q.keys[static_cast<std::size_t>(slot)].cost)) {
                if constexpr (diagnostics_enabled) ++rt_.selection.rejected_by_hash;
                if (answer) save_answer();
                return false;
            }
            if (!duplicate) slot = full ? q.worst() : q.free_slot();
            q.insert(slot, cost, hash, has_hash, serial_++, std::forward<Maker>(maker));
            if constexpr (diagnostics_enabled) q.keys[static_cast<std::size_t>(slot)].incoming_step = step;
            if constexpr (diagnostics_enabled) ++rt_.selection.accepted;
            if (duplicate) { if constexpr (diagnostics_enabled) ++rt_.selection.replaced_by_hash; }
            else if (full) { if constexpr (diagnostics_enabled) ++rt_.selection.replaced_by_width; }
            else {
                ++pending_;
                peak();
            }
            if (answer) save_best(*q.items[static_cast<std::size_t>(slot)], cost, turn);
            return true;
        }
        // 探索し、最良状態を返す、候補数 G に対し期待 O(T W S + G(log W + S) + スイープ数 T)
        template<class Expand, class Hook>
        Result<State> run(Expand&& expand, Hook&& hook, bool defer_finish = false) {
            sample_time();
            if constexpr (diagnostics_enabled) if (rt_.found) rt_.first_solution_us = rt_.last_best_update_us = 0;
            notify(EventType::RunStart, hook);
            sample_time();
            rt_.stop_reason = reason();
            // スイープの境界でも時間を検査するので、空のターンが多くても制限を検出する
            while (rt_.stop_reason == StopReason::None) do_sweep(expand, hook, false);
            while (rt_.time_limit_reached && rt_.param.continue_on_timeout && !rt_.found && pending_ > 0) {
                do_sweep(expand, hook, true);
            }
            notify(EventType::SearchEnd, hook);
            // キューとは独立した best_state を移動して、engine の破棄後も有効にする
            Result<State> result;
            result.found = rt_.found;

            result.exhausted = rt_.stop_reason == StopReason::Exhausted;
            result.sweep_limit_reached = rt_.stop_reason == StopReason::SweepLimit;
            result.completed_max_turn = rt_.completed_max_turn;
            result.best_cost = rt_.best_cost;
            result.best_turn = rt_.best_turn;
            result.best_state = std::move(best_state_);

            if (!defer_finish) finish(result, hook);
            return result;
        }
    private:
        static bool valid_cost(Cost cost) {
            if constexpr (std::floating_point<Cost>) return !std::isnan(cost);
            else { (void)cost; return true; }
        }
        Queue& layer(int turn) {
            auto& q = layers_[static_cast<std::size_t>(turn)];
            if (!q) q = spare_ ? std::move(spare_) : std::make_unique<Queue>(rt_.param.beam_width, hash_size_);
            return *q;
        }
        template<class U>
        void save_best(U&& state, Cost cost, int turn) {
            // キューとは独立した保存。受理したStateはコピーし、棄却時の生成結果は直接構築する。
            if (!rt_.found || cost < rt_.best_cost) {
                best_state_.emplace(std::forward<U>(state));
                rt_.found = true;
                rt_.best_cost = cost;
                rt_.best_turn = turn;
                if constexpr (diagnostics_enabled) { ++rt_.best_update_count; rt_.last_best_update_us = -1; }
            }
        }
        void sample_time() {
            rt_.elapsed_us = multi_detail::now_us() - start_us_;
            unchecked_ = 0;
            if (limit_us_ > 0 && rt_.elapsed_us >= limit_us_) rt_.time_limit_reached = true;
        }
        StopReason reason() const {
            if (rt_.stop_reason == StopReason::InvalidInitial) return StopReason::InvalidInitial;
            if (rt_.param.max_turn == 0 && rt_.completed_max_turn) return StopReason::Completed;
            if (rt_.time_limit_reached) return StopReason::TimeLimit;
            if (pending_ == 0) return rt_.completed_max_turn ? StopReason::Completed : StopReason::Exhausted;
            if (rt_.param.max_sweeps > 0 && rt_.sweep >= rt_.param.max_sweeps) return StopReason::SweepLimit;
            return StopReason::None;
        }
        bool remaining_in_sweep(int turn, int used, int width) const {
            if (used < width && layers_[static_cast<std::size_t>(turn)]->size > 0) return true;
            for (int t = turn + 1; t < rt_.param.max_turn; ++t) {
                const auto& q = layers_[static_cast<std::size_t>(t)];
                if (q && q->size > 0) return true;
            }
            return false;
        }
        template<class Expand, class Hook>
        void do_sweep(Expand& expand, Hook& hook, bool fallback) {
            ++rt_.sweep;
            if constexpr (diagnostics_enabled) rt_.sweep_completed = false;
            notify(EventType::SweepStart, hook);
            // SweepStartのHook時間も計上し、時間切れなら通常の展開を始めない。
            if (!fallback && limit_us_ > 0) sample_time();
            const int width = fallback ? 1 :
                (rt_.sweep == 1 ? rt_.param.first_sweep_width : rt_.param.sweep_width);
            bool stopped = !fallback && rt_.time_limit_reached;
            // 空の中間ターンを飛ばし、同じスイープ中に生成した未来の候補も展開する
            for (int turn = 0; turn < rt_.param.max_turn && !stopped; ++turn) {
                auto& ptr = layers_[static_cast<std::size_t>(turn)];
                if (!ptr) continue;
                Queue& q = *ptr;
                rt_.turn = turn;
                begin_frontier(static_cast<std::size_t>(q.size), static_cast<std::size_t>(width));
                for (int k = 0; k < width && q.size > 0; ++k) {
                    const int slot = q.best();
                    const Key key = q.keys[static_cast<std::size_t>(slot)];
                    State item = std::move(*q.items[static_cast<std::size_t>(slot)]);
                    q.erase(slot);

                    --pending_;
                    begin_expansion();
                    if constexpr (diagnostics_enabled) rt_.expanded_step_sum += key.incoming_step;
                    const StateView<State> now(turn, key.cost, item, key.hash, key.has_hash);
                    Emitter<State, void> emit(*this, turn, key.hash);
                    // Runtime ありの形式を優先し、利用するだけでは時計を追加で読まない
                    if constexpr (std::is_invocable_v<Expand&, const StateView<State>&, const Runtime&, Emitter<State, void>&>) {
                        expand(now, static_cast<const Runtime&>(rt_), emit);
                    } else {
                        static_assert(std::is_invocable_v<Expand&, const StateView<State>&, Emitter<State, void>&>, "invalid expand signature; Runtime must be const");
                        expand(now, emit);
                    }
                    ++rt_.expanded_nodes;
                    observe_best_update();
                    if (!fallback && limit_us_ > 0 && ++unchecked_ >= rt_.param.time_check_interval) sample_time();
                    if ((!fallback && rt_.time_limit_reached) || (fallback && rt_.found)) {
                        if constexpr (diagnostics_enabled) rt_.sweep_completed = !remaining_in_sweep(turn, k + 1, width);
                        stopped = true;
                        break;
                    }
                }
                notify(EventType::TurnEnd, hook);
                // hookで使った時間も、次turnの展開前に確認する。
                if constexpr (diagnostics_enabled) if (!fallback && limit_us_ > 0) {
                    sample_time();
                    if (rt_.time_limit_reached) {
                        rt_.sweep_completed = !remaining_in_sweep(turn, static_cast<int>(rt_.frontier_expanded), width);
                        stopped = true;
                    }
                }
                if (q.size == 0) spare_ = std::move(ptr);
            }
            if constexpr (diagnostics_enabled) { if (!stopped) rt_.sweep_completed = true; rt_.completed_sweeps += rt_.sweep_completed; }
            sample_time();
            rt_.stop_reason = reason();
            notify(EventType::SweepEnd, hook);
            // Hook 自身にかかった時間も、次のスイープ開始前に検査する
            sample_time();
            rt_.stop_reason = reason();
        }

    private:
        void peak() {
            if constexpr (diagnostics_enabled) {
                rt_.pending_candidates = static_cast<std::size_t>(pending_);
                rt_.peak_pending_candidates = std::max(rt_.peak_pending_candidates, rt_.pending_candidates);
            }
        }
        void snapshot() {
        if constexpr (diagnostics_enabled) {
            peak();
            rt_.pending_turns = rt_.full_pending_turns = 0;
            for (const auto& q : layers_) {
                if (q && q->size > 0) { ++rt_.pending_turns; rt_.full_pending_turns += q->size >= rt_.param.beam_width; }
            }
        }
        }
        template<class Hook> void notify(EventType event, Hook& hook) {
            if constexpr (diagnostics_enabled) { sample_time(); snapshot(); hook(event, static_cast<const DebugRuntime&>(rt_)); }
        }

    private:
        void begin_frontier(std::size_t size, std::size_t limit) {
            if constexpr (diagnostics_enabled) {
                rt_.frontier_size = std::min(size, limit);
                rt_.frontier_limit = limit;
                rt_.frontier_expanded = 0;
            }
        }
        void begin_expansion() {
            if constexpr (diagnostics_enabled) {
                ++rt_.frontier_expanded;

                rt_.expanded_after_timeout += rt_.time_limit_reached;
            }
        }
        void observe_best_update() {
            if constexpr (diagnostics_enabled) {
                if (rt_.found && rt_.last_best_update_us < 0) {
                    if (rt_.first_solution_us < 0) {
                        sample_time();
                        rt_.first_solution_us = rt_.last_best_update_us = rt_.elapsed_us;
                    } else rt_.last_best_update_us = multi_detail::now_us() - start_us_;
                }
            }
        }
    public:
        template<class ResultType, class Hook>
        void finish(ResultType& result, Hook& hook) {
            sample_time();
            if constexpr (diagnostics_enabled) { snapshot(); hook(EventType::RunEnd, static_cast<const DebugRuntime&>(rt_)); }
            result.runtime = rt_; result.time_limit_reached = rt_.time_limit_reached;
        }
};
private:
    template<class State, class Action, class Expand> struct HistoryExpand {
        using Value = Edge<Action>;
        using Pool = beam_history_detail::Pool<Value>;
        using Stored = beam_history_detail::TracedState<State, Value>;
        Pool& pool;
        Expand& expand;
        template<class... Context>
        void invoke(const StateView<Stored>& now, Emitter<Stored, void>& raw, Context&&... context) {
            const auto& stored = now.state;
            StateView<State> view{now.turn, now.cost, stored.state, now.hash, now.has_hash};
            Emitter<State, Action> emit(raw, pool, stored.trace.id(), now.turn);
            std::invoke(expand, view, std::forward<Context>(context)..., emit);
        }
        void operator()(const StateView<Stored>& now, Emitter<Stored, void>& raw)
            requires std::invocable<Expand&, const StateView<State>&, Emitter<State, Action>&>
            { invoke(now, raw); }
        void operator()(const StateView<Stored>& now, const Runtime& rt, Emitter<Stored, void>& raw)
            requires std::invocable<Expand&, const StateView<State>&, const Runtime&, Emitter<State, Action>&>
            { invoke(now, raw, rt); }
    };
    template<class State, class Action, class Expand, class DebugHook>
    static Result<State, Action> run_history(const Param& param, State initial_state, Cost initial_cost,
                                            Hash hash, bool has_hash, Expand& expand, DebugHook& hook) {
        static_assert(std::copy_constructible<Action>, "Action must be copy constructible for result.path");
        using Value = Edge<Action>;
        using Pool = beam_history_detail::Pool<Value>;
        using Stored = beam_history_detail::TracedState<State, Value>;
        Pool pool;
        HistoryExpand<State, Action, Expand> bridge{pool, expand};
        Engine<Stored> engine(param, Stored{std::move(initial_state), {}}, initial_cost, hash, has_hash);
        auto raw = engine.run(bridge, hook, true);
        Result<State, Action> result;
        result.found = raw.found;

        result.exhausted = raw.exhausted;
        result.completed_max_turn = raw.completed_max_turn;
        result.best_cost = raw.best_cost;
        result.best_turn = raw.best_turn;
        result.sweep_limit_reached = raw.sweep_limit_reached;

        if (raw.best_state) {
            result.best_state.emplace(std::move(raw.best_state->state));
            std::vector<std::size_t> reverse;
            for (auto id = raw.best_state->trace.id(); id != Pool::none; id = pool.parent(id)) reverse.push_back(id);
            result.path.reserve(reverse.size());
            for (auto i = reverse.rbegin(); i != reverse.rend(); ++i) result.path.push_back(pool.value(*i));
        }
        engine.finish(result, hook);
        return result;
    }
public:
    // ハッシュありで探索する、候補数 G に対し期待 O(T W S + G(log W + S) + スイープ数 T)
    template<class State, class Action = void, class H, class Expand, class DebugHook = NoOp>
    requires CompatibleHashArg<H, Hash>
    static Result<State, Action> run(const Param& param, State initial_state, Cost initial_cost,
                             H initial_hash, Expand&& expand, DebugHook&& debug_hook = DebugHook{}) {
        if constexpr (std::is_void_v<Action>) {
        Engine<State> engine(param, std::move(initial_state), initial_cost, static_cast<Hash>(initial_hash), true);
        return engine.run(std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));
        } else {
            return run_history<State, Action>(param, std::move(initial_state), initial_cost, static_cast<Hash>(initial_hash), true, expand, debug_hook);
        }
    }
    // ハッシュなしで探索する、候補数 G に対し期待 O(T W S + G(log W + S) + スイープ数 T)
    template<class State, class Action = void, class Expand, class DebugHook = NoOp>
    requires (!CompatibleHashArg<Expand, Hash>)
    static Result<State, Action> run(const Param& param, State initial_state, Cost initial_cost,
                             Expand&& expand, DebugHook&& debug_hook = DebugHook{}) {
        if constexpr (std::is_void_v<Action>) {
        Engine<State> engine(param, std::move(initial_state), initial_cost, Hash{}, false);
        return engine.run(std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));
        } else {
            return run_history<State, Action>(param, std::move(initial_state), initial_cost, Hash{}, false, expand, debug_hook);
        }
    }
};
} // namespace cs


#if __INCLUDE_LEVEL__ == 0
#define RUNTIME_TEST_BEAM cs::ChokudaiCopyMulti
#define RUNTIME_TEST_CLOCK cs::multi_detail::TestClock
#define RUNTIME_TEST_DELTA 0
#define RUNTIME_TEST_MULTI 1
#define RUNTIME_TEST_SUB 0
#define RUNTIME_TEST_CHOKU 1
#include <cstdlib>
#include <sstream>
namespace runtime_test {
using B = RUNTIME_TEST_BEAM<double, std::uint64_t>;
using Clock = RUNTIME_TEST_CLOCK;
using E = B::EventType;
using R = B::StopReason;
struct Action { int from = 0, to = 0; };
void check(bool value, const char* text) {
    if (!value) { std::cerr << "runtime regression: " << text << '\n'; std::abort(); }
}
template<class T> concept HasDiagnosticCounters = requires(T t) { t.generated_candidates; };
static_assert(!HasDiagnosticCounters<B::Runtime>);
static_assert(HasDiagnosticCounters<B::DebugRuntime>);
static_assert(sizeof(B::Runtime) < sizeof(B::DebugRuntime));
static_assert(!RUNTIME_TEST_BEAM<double, std::uint64_t, false>::diagnostics_enabled);
#ifdef LOCAL
static_assert(B::diagnostics_enabled);
#else
static_assert(!B::diagnostics_enabled);
#endif
void reset_clock() { Clock::enabled = true; Clock::value = 0; }
B::Param parameters() {
    B::Param p; p.max_turn = 2; p.beam_width = 2; p.expand_answers = false;
#if RUNTIME_TEST_CHOKU
    p.sweep_width = 2; p.first_sweep_width = 2; p.max_sweeps = 1;
#endif
#if RUNTIME_TEST_MULTI
    p.max_step = 2;
#endif
    return p;
}
struct Harness {
    int world = 0;
    std::uint64_t forward_calls = 0, backward_calls = 0;
    std::vector<bool> accepted;
    template<class Emit> bool push(Emit& emit, int child, double cost, std::uint64_t hash, bool answer = false) {
#if RUNTIME_TEST_DELTA
        Action item{world, child};
#else
        int item = child;
#endif
#if RUNTIME_TEST_MULTI
        bool ok = emit.push(item, cost, hash, 1, answer);
#else
        bool ok = emit.push(item, cost, hash, answer);
#endif
        accepted.push_back(ok); return ok;
    }
    template<class Expand, class Hook>
    auto run(B::Param p, double cost, Expand&& expand, Hook&& hook) {
#if RUNTIME_TEST_DELTA
        return B::run<Action>(p, cost, expand,
            [&](const Action& a) { check(world == a.from, "forward origin"); world = a.to; ++forward_calls; },
            [&](const Action& a) { check(world == a.to, "backward origin"); world = a.from; ++backward_calls; }, hook);
#else
        return B::run<int>(p, 0, cost, expand, hook);
#endif
    }
};
void counts() {
    reset_clock(); Harness h; auto p = parameters();
#if RUNTIME_TEST_DELTA
    p.apply_best_on_return = true;
#endif
    B::DebugRuntime search{}, end{};
    std::vector<E> events;
    B::CsvStatHook csv("runtime_counts.csv", 0);
    auto expand = [&](const auto& now, auto& emit) {
        if (now.turn == 0) {
            h.push(emit, 1, 10, 1); h.push(emit, 2, 5, 2); h.push(emit, 3, 12, 3);
            h.push(emit, 4, 9, 1); h.push(emit, 5, 8, 3); h.push(emit, 6, 4, 2);
            h.push(emit, 7, 4.5, 2); h.push(emit, 8, std::numeric_limits<double>::quiet_NaN(), 8);
            h.push(emit, 9, -100, 9, true); h.push(emit, 10, -99, 9, true);
        } else h.push(emit, 100, -101, 100);
        Clock::value += 1000;
    };
    auto hook = [&](E event, const B::DebugRuntime& rt) {
        events.push_back(event);
        if (event == E::SearchEnd) {
            search = rt;
#if RUNTIME_TEST_DELTA
            check(rt.forward_calls == h.forward_calls && rt.backward_calls == h.backward_calls, "all search moves counted");
#endif
        }
        if (event == E::RunEnd) {
            end = rt;
#if RUNTIME_TEST_DELTA
            check(h.world == 100, "RunEnd after replay");
            check(rt.forward_calls == h.forward_calls && rt.backward_calls == h.backward_calls, "all final moves counted");
#endif
        }
        csv(event, rt);
    };
    auto result = h.run(p, 0, expand, hook);
    check(result.found && result.best_cost == -101 && result.best_turn == 2, "best solution");
    check(search.generated_candidates == 12 && search.invalid_candidates == 1, "generation and invalid count");
    check(search.answer_candidates == 4 && search.best_update_count == 2, "answers before selection; strict best updates");
    check(search.expanded_nodes == 3 && search.first_solution_us == 1000, "first solution after first callback");
    check(search.last_best_update_us == 2000, "last improvement time is expand end");
    check(search.frontier_size == 2 && search.frontier_expanded == 2 && search.frontier_limit == 2, "frontier progress");
    check(search.pending_candidates == 0 && search.peak_pending_candidates == 2, "pending excludes current and archive");
#if RUNTIME_TEST_SUB
    const auto& s = search.turn_selection;
    check(search.sub_selection.attempts() == 0 && search.sub_expanded_nodes == 0, "sub counters separated");
#else
    const auto& s = search.selection;
#endif
    check(s.attempts() == 7 && s.accepted == 5, "selection attempts and admissions");
    check(s.rejected_by_width == 1 && s.replaced_by_width == 1, "width rejection and displacement");
    check(s.rejected_by_hash == 1 && s.replaced_by_hash == 2 && s.hash_checked == 6, "actual hash checks and matches");
#if RUNTIME_TEST_MULTI
    check(search.generated_step_sum == 11 && search.generated_step_max == 1 && search.expanded_step_sum == 2, "generated and expanded step stats");
#endif
    check(result.runtime.elapsed_us == end.elapsed_us && result.runtime.expanded_nodes == end.expanded_nodes, "RunEnd equals returned Runtime");
    check(events.front() == E::RunStart && events.back() == E::RunEnd && events[events.size()-2] == E::SearchEnd, "event ordering");
    auto accepted = h.accepted;
    h = Harness{}; reset_clock();
    auto no_hook = h.run(p, 0, expand, B::NoOp{});
    check(no_hook.best_cost == result.best_cost && no_hook.runtime.expanded_nodes == search.expanded_nodes && h.accepted == accepted, "NoOp behavior equals hook behavior");
}
void boundaries() {
    reset_clock(); Harness h; auto p = parameters();
    auto never = [&](const auto&, auto&) { check(false, "unexpected expansion"); };
    B::DebugRuntime end{};
    auto capture = [&](E event, const B::DebugRuntime& rt) { if (event == E::RunEnd) end = rt; };
    p.max_turn = 0;
    auto root = h.run(p, -3, never, capture);
    check(root.found && root.best_cost == -3 && end.best_update_count == 1 && end.first_solution_us == 0 && end.last_best_update_us == 0 && end.answer_candidates == 0, "root is not a pushed candidate");
    check(end.stop_reason == R::Completed && end.completed_max_turn && end.pending_candidates == 0, "zero-turn completion");
    p.max_turn = 3; reset_clock();
    auto invalid = h.run(p, std::numeric_limits<double>::quiet_NaN(), never, capture);
    check(!invalid.found && end.last_best_update_us == -1 && end.stop_reason == R::InvalidInitial && end.generated_candidates == 0, "invalid initial state");
    p.time_limit_ms = 1.5; p.time_check_interval = 1; p.continue_on_timeout = false;
    p.expand_answers = true; p.beam_width = 4;
#if RUNTIME_TEST_CHOKU
    p.first_sweep_width = 2; p.sweep_width = 1; p.max_sweeps = 0;
#endif
    reset_clock(); h = Harness{};
    B::CsvStatHook csv("runtime_timeout.csv", 100000);
    bool partial = false;
    auto timed = [&](const auto& now, auto& emit) {
        for (int i = 0; i < 4; ++i) h.push(emit, now.turn * 10 + i + 1, i, static_cast<std::uint64_t>(i));
        Clock::value += 1000;
    };
    auto timed_hook = [&](E event, const B::DebugRuntime& rt) {
        if (event == E::RunEnd) end = rt;
#if RUNTIME_TEST_CHOKU
        if (event == E::SweepEnd && rt.sweep == 1) partial = !rt.sweep_completed;
#else
        if (event == E::TurnEnd) partial |= rt.frontier_expanded < rt.frontier_size;
#endif
        csv(event, rt);
    };
    auto timeout = h.run(p, 0, timed, timed_hook);
    check(timeout.time_limit_reached && end.stop_reason == R::TimeLimit && partial, "partial unit reported on timeout");
    check(end.expanded_nodes == 2, "time check interval");
    reset_clock(); h = Harness{};
    auto slow_start = [&](E e, const B::DebugRuntime&) { if (e == E::RunStart) Clock::value += 10000; };
    auto stopped = h.run(p, 0, never, slow_start);
    check(stopped.time_limit_reached && stopped.runtime.expanded_nodes == 0, "detect RunStart hook time before expanding");
}
#if RUNTIME_TEST_CHOKU
void refinement() {
    reset_clock(); Harness h; auto p = parameters();
    p.beam_width = 4; p.first_sweep_width = 1; p.sweep_width = 1; p.max_sweeps = 0;
    B::CsvStatHook csv("runtime_refinement.csv", 100000);
    B::DebugRuntime first{}, search{};
    auto expand = [&](const auto& now, auto& emit) {
        if (now.turn == 0) for (int i = 1; i <= 4; ++i) h.push(emit, i, i, static_cast<std::uint64_t>(i));
        else h.push(emit, 100, -now.state, 100);
        Clock::value += 1000;
    };
    auto result = h.run(p, 0, expand, [&](E e, const B::DebugRuntime& rt) {
        if (e == E::SweepEnd && rt.sweep == 1) first = rt;
        if (e == E::SearchEnd) search = rt;
        csv(e, rt);
    });
    check(first.sweep_completed && first.found && first.best_cost == -1, "first sweep baseline");
    check(result.best_cost == -4 && search.completed_sweeps == 4, "refinement gains");
}
#ifdef LOCAL
void sweep_csv() {
    auto run_refinement = [&](B::CsvStatHook& csv) {
        reset_clock(); Harness h; auto p = parameters();
        p.beam_width = 4; p.first_sweep_width = 1; p.sweep_width = 1; p.max_sweeps = 0;
        int turns = 0, starts = 0;
        auto expand = [&](const auto& now, auto& emit) {
            if (now.turn == 0) for (int i = 1; i <= 4; ++i) h.push(emit, i, i, static_cast<std::uint64_t>(i));
            else h.push(emit, 100, -now.state, 100);
            Clock::value += 1000;
        };
        const auto result = h.run(p, 0, expand, [&](E e, const B::DebugRuntime& rt) {
            turns += e == E::TurnEnd; starts += e == E::SweepStart;
            csv(e, rt);
        });
        check(result.best_cost == -4 && turns == 5 && starts == 4, "custom hook retains all layer and sweep start events");
        for (const auto& row : csv.rows)
            check(row.event != E::TurnEnd && row.event != E::SweepStart, "CSV excludes layer and sweep start rows");
    };
    B::CsvStatHook all("runtime_sweep_all.csv", 0);
    run_refinement(all);
    check(all.rows.size() == 7, "zero interval records four sweeps and three lifecycle rows");
    for (int i = 1; i <= 4; ++i)
        check(all.rows[static_cast<std::size_t>(i)].event == E::SweepEnd &&
              all.rows[static_cast<std::size_t>(i)].runtime.sweep == i, "all sweep ends recorded in order");
    B::CsvStatHook sampled("runtime_sweep_sampled.csv", 2);
    run_refinement(sampled);
    check(sampled.rows.size() == 5 && sampled.rows[1].runtime.sweep == 1 &&
          sampled.rows[2].runtime.sweep == 3, "interval is measured between retained sweep rows");
    check(sampled.rows[2].runtime.expanded_nodes - sampled.rows[1].runtime.expanded_nodes == 2 &&
          sampled.rows[2].runtime.completed_sweeps - sampled.rows[1].runtime.completed_sweeps == 2,
          "sampled row covers multiple sweeps");

    reset_clock(); Harness h; auto p = parameters();
    p.max_turn = 3; p.beam_width = 4; p.first_sweep_width = 1; p.sweep_width = 1; p.max_sweeps = 0;
    p.time_limit_ms = 4; p.time_check_interval = 1; p.continue_on_timeout = false;
    B::CsvStatHook partial("runtime_sweep_partial.csv", 100000);
    auto expand = [&](const auto& now, auto& emit) {
        if (now.turn == 0) for (int i = 1; i <= 4; ++i) h.push(emit, i, i, static_cast<std::uint64_t>(i));
        else if (now.turn == 1) for (int j = 0; j < 2; ++j) {
            const int child = now.state * 10 + j;
            h.push(emit, child, child, static_cast<std::uint64_t>(child));
        } else h.push(emit, 100, -now.state, 100);
        Clock::value += 1000;
    };
    const auto result = h.run(p, 0, expand, partial);
    check(result.time_limit_reached && partial.rows.size() == 5, "partial sweep is retained despite long interval");
    check(partial.rows[1].runtime.sweep_completed && partial.rows[2].event == E::SweepEnd &&
          partial.rows[2].runtime.sweep == 2 && !partial.rows[2].runtime.sweep_completed,
          "partial row is the second sweep rather than the mandatory first sweep");

    // A turn notification must not consume a flag change or reset the sampling interval.
    B::CsvStatHook flags("runtime_sweep_flags.csv", 100000);
    B::DebugRuntime rt{};
    flags(E::RunStart, rt);
    rt.sweep = 1; rt.sweep_completed = true; rt.elapsed_us = 1000; rt.completed_sweeps = 1;
    flags(E::SweepEnd, rt);
    rt.sweep = 2; rt.elapsed_us = 2000; rt.diagnostic_flags = 1;
    flags(E::TurnEnd, rt);
    check(flags.rows.size() == 2, "TurnEnd flags are deferred to a sweep boundary");
    rt.completed_sweeps = 2; flags(E::SweepEnd, rt);
    check(flags.rows.size() == 3 && flags.rows.back().event == E::SweepEnd, "flag change forces the next eligible row");
    rt.sweep = 3; rt.elapsed_us = 3000; rt.time_limit_reached = true;
    flags(E::SweepStart, rt);
    check(flags.rows.size() == 3, "SweepStart timeout is not a CSV row");
    rt.completed_sweeps = 3; flags(E::SweepEnd, rt);
    check(flags.rows.size() == 4, "completed sweep still reports a newly detected timeout");
    rt.sweep = 4; rt.elapsed_us = 3100; rt.sweep_completed = false;
    flags(E::SweepEnd, rt);
    check(flags.rows.size() == 5 && !flags.rows.back().runtime.sweep_completed,
          "partial sweep is mandatory even if timeout was already recorded");
    flags(E::SearchEnd, rt); flags(E::RunEnd, rt);

    B::CsvStatHook reused("runtime_sweep_reused.csv", 0);
    run_refinement(reused);
    reset_clock(); h = Harness{}; p = parameters(); p.max_turn = 0;
    auto never = [&](const auto&, auto&) { check(false, "empty search must not expand"); };
    const auto root = h.run(p, 7, never, reused);
    check(root.found && reused.rows.size() == 3 && reused.rows[1].event == E::SearchEnd,
          "reusing a hook clears old sweep rows for max_turn zero");
    reused.filename = "runtime_sweep_invalid.csv";
    reset_clock(); p = parameters();
    const auto invalid = h.run(p, std::numeric_limits<double>::quiet_NaN(), never, reused);
    check(!invalid.found && reused.rows.size() == 3 && reused.rows[1].runtime.sweep == 0,
          "invalid initial cost has lifecycle rows only");
}
#endif
#endif
}
namespace runtime_test {
void review_regressions() {
    {
        reset_clock(); Harness h; auto p = parameters();
        p.max_turn = 3; p.time_limit_ms = 1; p.continue_on_timeout = false;
        auto expand = [&](const auto& now, auto& emit) {
            check(now.turn == 0, "TurnEnd hook time is checked before expanding next turn");
            h.push(emit, 1, 1, 1);
        };
        auto hook = [&](E event, const B::DebugRuntime& rt) {
            if (event == E::TurnEnd && rt.turn == 0) Clock::value += 2000;
        };
        auto result = h.run(p, 0, expand, hook);
        check(result.time_limit_reached && result.runtime.expanded_nodes == 1, "TurnEnd hook timeout");
    }
}
}


namespace runtime_test {
template<bool Enabled>
void diagnostic_mode() {
    using T = RUNTIME_TEST_BEAM<double, std::uint64_t, Enabled>;
#ifdef LOCAL
    static_assert(T::diagnostics_enabled == Enabled);
#else
    static_assert(!T::diagnostics_enabled);
#endif
    typename T::Param p;
    p.max_turn = 2;
    p.beam_width = 2;
#if RUNTIME_TEST_CHOKU
    p.max_sweeps = 1;
#endif
    reset_clock();
    int calls = 0;
#if RUNTIME_TEST_DELTA
    int world = 0;
#endif
    typename T::DebugRuntime last{};
#if RUNTIME_TEST_DELTA
    using View = typename T::NodeView;
    using Emit = typename T::template Emitter<Action>;
    p.apply_best_on_return = true;
#else
    using View = typename T::template StateView<int>;
    using Emit = typename T::template Emitter<int>;
#endif
    auto expand = [&](const View& now, const typename T::Runtime& rt, Emit& emit) {
        check(rt.param.max_turn == 2 && now.turn < 2, "typed Runtime and Emitter");
#if RUNTIME_TEST_DELTA
        Action child{world, world + 1};
#else
        int child = now.state + 1;
#endif
#if RUNTIME_TEST_MULTI
        emit.push(child, -double(now.turn + 1), std::uint64_t(now.turn + 1), 1);
#else
        emit.push(child, -double(now.turn + 1), std::uint64_t(now.turn + 1));
#endif
    };
    auto hook = [&](typename T::EventType event, const typename T::DebugRuntime& rt) {
        ++calls;
        if (event == T::EventType::RunEnd) last = rt;
    };
    auto run = [&](auto&& callback) {
#if RUNTIME_TEST_DELTA
        world = 0;
        return T::template run<Action>(p, 0, expand,
            [&](const Action& a) { check(world == a.from, "typed forward"); world = a.to; },
            [&](const Action& a) { check(world == a.to, "typed backward"); world = a.from; }, callback);
#else
        return T::template run<int>(p, 0, 0, expand, callback);
#endif
    };
    const auto result = run(hook);
    check(result.found && result.best_cost == -2 && result.best_turn == 2, "mode best solution");
    check(result.runtime.expanded_nodes == 2 && result.runtime.completed_max_turn, "mode core Runtime");
#if RUNTIME_TEST_DELTA
    check(world == 2 && result.path.size() == 2, "mode best State replay");
#else
    check(result.best_state && *result.best_state == 2, "mode final State");
#endif
    if constexpr (T::diagnostics_enabled) {
        check(calls > 0 && last.generated_candidates == 2 && last.best_update_count == 1,
              "enabled diagnostic counters and custom hook");
    } else check(calls == 0, "disabled custom hook is not invoked");
    typename T::NoOp empty;
    const auto no_hook = run(empty);
    check(no_hook.best_cost == result.best_cost && no_hook.runtime.expanded_nodes == 2,
          "NoOp does not affect the search");
    if constexpr (!T::diagnostics_enabled) {
        // The disabled path must not even instantiate a hook call.
        struct Uncallable {} uncallable;
        check(run(uncallable).best_cost == -2, "disabled hook call is discarded");
    }
}
}

int main() {
#ifdef LOCAL
    runtime_test::review_regressions();
    runtime_test::counts(); runtime_test::boundaries();
#if RUNTIME_TEST_CHOKU
    runtime_test::refinement();
    runtime_test::sweep_csv();
#endif
#endif
    runtime_test::diagnostic_mode<true>();
    runtime_test::diagnostic_mode<false>();
    std::cout << "runtime diagnostics tests passed\n";
}

#endif
