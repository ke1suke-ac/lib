#pragma once
#include <bits/stdc++.h>

// chokudai_copy_sub_v24.hpp : ターン境界で比較をそろえる sub 探索。cost は小さいほど良い。
// W=境界幅、U=sub幅、G=生成数、E=展開数、D=経路長。利用側の処理費用は別途加算。
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

namespace cs {
template<class T>
concept CostType = ((std::integral<T> && std::is_signed_v<T>) || std::floating_point<T>) && !std::same_as<T, bool>;
template<class T>
concept HashType = std::unsigned_integral<T> && !std::same_as<T, bool> && sizeof(T) <= 8;
template<class H, class Hash>
concept CompatibleHashArg = HashType<std::remove_cvref_t<H>> && sizeof(std::remove_cvref_t<H>) <= sizeof(Hash);
template<class B> concept BoolArg = std::same_as<std::remove_cvref_t<B>, bool>;
template<class U, class T> concept StateArg = std::same_as<std::remove_cvref_t<U>, T>;

namespace sub_detail {
inline std::uint64_t mix(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
inline std::int64_t to_us(double ms) noexcept {
    if (!(ms > 0.0)) return 0;
    const long double us = static_cast<long double>(ms) * 1000.0L;
    const auto upper = std::numeric_limits<std::int64_t>::max();
    if (us >= static_cast<long double>(upper) - 0.5L) return upper;
    return std::max<std::int64_t>(1, static_cast<std::int64_t>(us + 0.5L));
}
#if __INCLUDE_LEVEL__ == 0
// 単体実行時だけの仮想時計。通常includeでは領域・分岐とも存在しない。
struct TestClock {
    static inline bool enabled = false;
    static inline std::int64_t value = 0;
    static inline std::uint64_t calls = 0;
};
#endif
inline std::int64_t now_us() {
#if __INCLUDE_LEVEL__ == 0
    ++TestClock::calls;
    if (TestClock::enabled) return TestClock::value;
#endif
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
template<class Cost> bool valid(Cost c) {
    if constexpr (std::floating_point<Cost>) return !std::isnan(c);
    else return true;
}
} // namespace sub_detail

template<CostType Cost = long long, HashType Hash = std::uint64_t, bool EnableDiagnostics = true>
struct ChokudaiCopySub {
    using cost_type = Cost;
    using hash_type = Hash;

    // 診断の保持・更新・hook通知を切り替える。探索に必要なRuntimeは常に有効。
#ifdef LOCAL
    static constexpr bool diagnostics_enabled = EnableDiagnostics;
#else
    static constexpr bool diagnostics_enabled = false;
#endif
private:
    template<class Value> class Engine;
public:
    enum class Next { Sub, Turn };
    enum class StopReason { None, Completed, Exhausted, TimeLimit, SweepLimit, InvalidInitial };
    enum class EventType { RunStart, TurnEnd, SearchEnd, RunEnd, SubStepEnd, SweepStart, SweepEnd };
    struct Param {
        int max_turn = 0;              // 完了させるターン数。負値は0。
        int beam_width = 1;            // 次ターン開始候補の幅。1以上。
        double time_limit_ms = 0;      // 探索全体のsoft limit。0以下・NaNは無制限。
        int time_check_interval = 64;  // 通常の時計確認間隔。層境界でも確認。
        bool continue_on_timeout = true; // 時間切れで解未発見なら救済探索を続ける。
        bool expand_answers = true; // falseなら有効解を保存のみ行い、探索候補へ登録しない。
        bool max_turn_is_answer = true;
        bool use_hash_dedup = true;    // Hashが明示された候補だけを重複排除。
        int sweep_width = 1;        // 1ターン・1スイープで取り出す開始候補数。
        std::int64_t max_sweeps = 0;   // 通常スイープ上限。時間超過後の解なし救済には適用しない。
        int sub_beam_width = 0;        // 0以下ならbeam_widthを使用。
        int first_sweep_width = 0; // 初回だけの取出し上限。0以下はsweep_width、正値は1..beam_width
    };
    template<class Value> struct StateView {
        int turn = 0; Cost cost{}; const Value& state; Hash hash{}; bool has_hash = false;
        std::int64_t sub_step = 0, depth = 0;
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
        std::int64_t sub_step = 0, best_sub_step = 0, best_depth = 0;
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

        // 展開開始時の1層全体。layer_size==0ではcost統計は無効。
        std::size_t layer_size = 0;
        Cost layer_cost_min{}, layer_cost_max{};
        long double layer_cost_mean = 0, layer_cost_stddev = 0;
        std::uint64_t generated_candidates = 0, invalid_candidates = 0, answer_candidates = 0;
        std::uint64_t best_update_count = 0, expanded_after_timeout = 0;
        // 最終改善時刻はexpand終了時に確定。expand中の改善は-1で未確定を表す。
        std::int64_t first_solution_us = -1, last_best_update_us = -1;
        std::size_t frontier_size = 0, frontier_limit = 0, frontier_expanded = 0;
        std::size_t pending_candidates = 0, peak_pending_candidates = 0;
        SelectionStats sub_selection, turn_selection;
        std::uint64_t sub_expanded_nodes = 0;
        bool sweep_completed = false;
        std::int64_t completed_sweeps = 0;
    };

    struct NoOp {
        // 通知を無視する。O(1)。
        void operator()(EventType, const Runtime&) const noexcept {}
    };
    // LOCAL定義時のみCSVを記録する。未定義時は同じ呼び出し方で使える空実装。
        // 診断の有効・無効は外側の diagnostics_enabled で決まる。
    struct CsvStatHook {
#ifdef LOCAL
        struct Row { EventType event; DebugRuntime runtime; std::int64_t expanded_sub_step_max = -1; };
        std::string filename;
        std::int64_t interval_us, interval_sub_step_max = -1, total_sub_step_max = -1;
        std::vector<Row> rows;
        explicit CsvStatHook(std::string path = "beam_stat.csv", double interval_ms = 50)
            : filename(std::move(path)), interval_us(sub_detail::to_us(interval_ms)) {}
        void operator()(EventType event, const DebugRuntime& rt) {
            if (event == EventType::RunStart) {
                rows.clear();
                interval_sub_step_max = total_sub_step_max = -1;
            }
            // 間引く層も含め、実際に展開した深さを記録前に集計する。
            if (event == EventType::SubStepEnd && rt.frontier_expanded > 0) {
                interval_sub_step_max = std::max(interval_sub_step_max, rt.sub_step);
                total_sub_step_max = std::max(total_sub_step_max, rt.sub_step);
            }
            const bool mandatory = event == EventType::RunStart || event == EventType::SearchEnd ||
                event == EventType::RunEnd || (event == EventType::SweepEnd && rt.sweep == 1);
            const bool changed = !rows.empty() &&
                (rt.diagnostic_flags != rows.back().runtime.diagnostic_flags ||
                 (rt.time_limit_reached && !rows.back().runtime.time_limit_reached) );
            if (mandatory || changed || rows.empty() || interval_us == 0 ||
                rt.elapsed_us - rows.back().runtime.elapsed_us >= interval_us) {
                rows.push_back({event, rt, event == EventType::RunEnd ? total_sub_step_max : interval_sub_step_max});
                if (event != EventType::RunEnd) interval_sub_step_max = -1;
            }
            if (event == EventType::RunEnd) write();
        }
    private:
        static const char* event_name(EventType e) {
            switch(e) {
            case EventType::RunStart: return "RunStart";
            case EventType::TurnEnd: return "TurnEnd";
            case EventType::SearchEnd: return "SearchEnd";
            case EventType::RunEnd: return "RunEnd";
            case EventType::SubStepEnd: return "SubStepEnd";
            case EventType::SweepStart: return "SweepStart";
            case EventType::SweepEnd: return "SweepEnd";
            }
            return "";
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
                ratio(a.hash_checked - b.hash_checked, attempts, 100);
                ratio((a.rejected_by_hash - b.rejected_by_hash) + (a.replaced_by_hash - b.replaced_by_hash), attempts, 100);
                cell(attempts);
            };
            for (const char* name : {
                // 経過・観測位置
                "elapsed_ms", "time_used_pct", "period_ms", "event",
                "scope", "sweep", "turn", "sub_step",
                // 解の品質・改善
                "best_cost", "best_gain", "best_updates", "best_unchanged_ms",
                "found", "best_turn", "best_sub_step", "best_depth",
                "answer_candidates", "first_solution_ms", "first_solution_budget_pct",
                // 層のcost・充足・進行
                "layer_size", "layer_cost_min", "layer_cost_mean", "layer_cost_max",
                "layer_cost_stddev", "frontier_fill_pct", "frontier_done_pct", "frontier_limit",
                // 探索量・速度
                "expand_per_sec", "expanded_nodes", "candidates_per_expand", "generated_candidates",
                "invalid_candidates",
                // Hash照会・除外と選別量
                "sub_hash_checked_pct", "sub_hash_discard_pct", "sub_selection_attempts", "turn_hash_checked_pct",
                "turn_hash_discard_pct", "turn_selection_attempts",
                // 残候補・方式別の負荷
                "pending_candidates", "peak_pending_candidates", "sub_expand_pct", "expanded_sub_step_max",
                // スイープの完了・初回と追加探索
                "sweep_completed", "completed_sweeps", "first_sweep_completed", "first_sweep_found",
                "first_sweep_ms", "first_sweep_budget_pct", "refinement_ms", "best_gain_after_first_sweep",
                // 終了・時間切れ・異常
                "stop_reason", "completed_max_turn", "time_limit_reached", "overrun_ms",
                "finalize_ms", "expanded_after_timeout", "diagnostic_flags",
                // 設定
                "beam_width", "sub_beam_width", "first_sweep_width", "sweep_width",
                "max_sweeps", "max_turn", "time_limit_ms", "time_check_interval",
                "continue_on_timeout", "use_hash_dedup", "max_turn_is_answer", "expand_answers",
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
                const bool local = row.event == EventType::TurnEnd || row.event == EventType::SubStepEnd;
                const bool frontier = row.event == EventType::SubStepEnd;
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
                opt(local, r.turn);
                opt(local, r.sub_step);
                opt(r.found, r.best_cost);
                opt(s.found && p.found, (p.best_cost == s.best_cost ? 0.0L : static_cast<long double>(p.best_cost) - static_cast<long double>(s.best_cost)));
                cell(s.best_update_count - p.best_update_count);
                opt(s.found, ms(s.elapsed_us - s.last_best_update_us));
                cell(r.found);
                opt(r.found, r.best_turn);
                opt(r.found, r.best_sub_step);
                opt(r.found, r.best_depth);
                cell(s.answer_candidates - p.answer_candidates);
                opt(s.first_solution_us >= 0, ms(s.first_solution_us));
                optional_ratio(s.first_solution_us >= 0, ms(s.first_solution_us), r.param.time_limit_ms, 100);
                opt(frontier, r.layer_size);
                opt(frontier && r.layer_size != 0, r.layer_cost_min);
                opt(frontier && r.layer_size != 0 && std::isfinite(r.layer_cost_mean), r.layer_cost_mean);
                opt(frontier && r.layer_size != 0, r.layer_cost_max);
                opt(frontier && r.layer_size != 0 && std::isfinite(r.layer_cost_stddev), r.layer_cost_stddev);
                optional_ratio(frontier, r.frontier_size, r.frontier_limit, 100);
                optional_ratio(frontier, r.frontier_expanded, r.frontier_size, 100);
                opt(frontier, r.frontier_limit);
                ratio(expanded * 1000000.0L, dt, 1);
                cell(expanded);
                ratio(generated, expanded, 1);
                cell(generated);
                cell(s.invalid_candidates - p.invalid_candidates);
                write_selection(s.sub_selection, p.sub_selection);
                write_selection(s.turn_selection, p.turn_selection);
                cell(s.pending_candidates);
                cell(s.peak_pending_candidates);
                ratio(s.sub_expanded_nodes - p.sub_expanded_nodes, expanded, 100);
                opt(row.expanded_sub_step_max >= 0, row.expanded_sub_step_max);
                opt((row.event == EventType::SweepEnd || total) && s.sweep > 0, s.sweep_completed);
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
                cell(r.param.sub_beam_width);
                cell(r.param.first_sweep_width);
                cell(r.param.sweep_width);
                cell(r.param.max_sweeps);
                cell(r.param.max_turn);
                cell(r.param.time_limit_ms);
                cell(r.param.time_check_interval);
                cell(r.param.continue_on_timeout);
                cell(r.param.use_hash_dedup);
                cell(r.param.max_turn_is_answer);
                cell(r.param.expand_answers);
                cell("chokudai");
                cell("copy");
                cell("sub");
                cell(7);
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


    template<class Value, class Action = void> struct Result;
    template<class Value> struct Result<Value, void> {
        bool found = false, time_limit_reached = false, completed_max_turn = false;
        Cost best_cost{};
        int best_turn = 0;
        std::int64_t best_sub_step = 0, best_depth = 0;
        std::optional<Value> best_state;
        Runtime runtime;
    };
    template<class Value, class Action = void> class Emitter;
    template<class Value> class Emitter<Value, void> {
    public:
        // 候補を追加する。期待償却O(log(2+幅)+保存費用)。trueは探索候補への一時保存。最良解だけ保存した場合はfalse。
        template<class U> requires StateArg<U, Value>
        bool push(U&& value, Cost cost, Next next = Next::Turn) {
            return push_value(std::forward<U>(value), cost, hash_, false, false, next);
        }
        // 有効解の印を指定する。同じ計算量。expand_answers=falseなら保存だけを行い、展開対象へ登録しない。
        template<class U, class B> requires StateArg<U, Value> && BoolArg<B>
        bool push(U&& value, Cost cost, B is_answer, Next next = Next::Turn) {
            return push_value(std::forward<U>(value), cost, hash_, false, is_answer, next);
        }
        // Hashを明示する。同じ計算量。同じ保存先内だけで重複排除する。
        template<class U, class H> requires StateArg<U, Value> && CompatibleHashArg<H, Hash>
        bool push(U&& value, Cost cost, H hash, Next next = Next::Turn) {
            return push_value(std::forward<U>(value), cost, static_cast<Hash>(hash), true, false, next);
        }
        // Hashと有効解の印を指定する。同じ計算量。
        template<class U, class H, class B> requires StateArg<U, Value> && CompatibleHashArg<H, Hash> && BoolArg<B>
        bool push(U&& value, Cost cost, H hash, B is_answer, Next next = Next::Turn) {
            return push_value(std::forward<U>(value), cost, static_cast<Hash>(hash), true, is_answer, next);
        }
        // 必要な候補だけmakerで生成する。期待償却O(log(2+幅)+採用時の生成・保存費用)。
        template<class Maker> requires std::invocable<Maker&&>
        bool push_lazy(Cost cost, Maker&& maker, Next next = Next::Turn) {
            return engine_.push_candidate(cost, hash_, false, false, next, std::forward<Maker>(maker));
        }
        // 有効解の印付きの遅延生成。同じ計算量。makerは候補または最良解の保存に必要な場合だけ1回。
        template<class B, class Maker> requires BoolArg<B> && std::invocable<Maker&&>
        bool push_lazy(Cost cost, B is_answer, Maker&& maker, Next next = Next::Turn) {
            return engine_.push_candidate(cost, hash_, false, is_answer, next, std::forward<Maker>(maker));
        }
        // Hash付きの遅延生成。同じ計算量。
        template<class H, class Maker> requires CompatibleHashArg<H, Hash> && std::invocable<Maker&&>
        bool push_lazy(Cost cost, H hash, Maker&& maker, Next next = Next::Turn) {
            return engine_.push_candidate(cost, static_cast<Hash>(hash), true, false, next, std::forward<Maker>(maker));
        }
        // Hashと有効解の印付きの遅延生成。同じ計算量。
        template<class H, class B, class Maker>
        requires CompatibleHashArg<H, Hash> && BoolArg<B> && std::invocable<Maker&&>
        bool push_lazy(Cost cost, H hash, B is_answer, Maker&& maker, Next next = Next::Turn) {
            return engine_.push_candidate(cost, static_cast<Hash>(hash), true, is_answer, next, std::forward<Maker>(maker));
        }

    private:
        friend class Engine<Value>;
        Engine<Value>& engine_;
        Hash hash_;
        Emitter(Engine<Value>& engine, Hash hash) : engine_(engine), hash_(hash) {}
        template<class U>
        bool push_value(U&& value, Cost cost, Hash hash, bool has_hash, bool is_answer, Next next) {
            auto maker = [&]() -> decltype(auto) { return std::forward<U>(value); };
            return engine_.push_candidate(cost, hash, has_hash, is_answer, next, maker);
        }
    };

private:
    template<class Action> struct HistoryStep { Action action; bool next_turn; };
public:
    template<class Action> class HistoryView {
        template<class, class> friend class Emitter;
        using Pool = beam_history_detail::Pool<HistoryStep<Action>>;
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
        std::vector<Action> path;
        std::vector<std::size_t> turn_offsets;
    };
    // Action型指定時だけ有効。Stateと操作列は分離し、祖先Stateは保持しない。
    template<class State, class Action> class Emitter {
        friend struct ChokudaiCopySub;
        using Value = HistoryStep<Action>;
        using Pool = beam_history_detail::Pool<Value>;
        using Stored = beam_history_detail::TracedState<State, Value>;
        Emitter<Stored, void>& raw_;
        Pool& pool_;
        std::size_t parent_;
        Emitter(Emitter<Stored, void>& raw, Pool& pool, std::size_t parent) noexcept
            : raw_(raw), pool_(pool), parent_(parent) {}
        template<class A, class Maker>
        bool emit(A&& action, Cost cost, Hash hash, bool has_hash, bool is_answer, Next next, Maker&& maker) {
            auto traced = [&]() {
                return Stored{State(std::invoke(std::forward<Maker>(maker))), pool_.make(parent_, Value{std::forward<A>(action), next == Next::Turn})};
            };

            if (has_hash) return raw_.push_lazy(cost, hash, is_answer, traced, next);
            return raw_.push_lazy(cost, is_answer, traced, next);
        }
    public:
        HistoryView<Action> history() const noexcept { return {pool_, parent_}; }
        template<class U, class A>
        requires StateArg<U, State> && std::same_as<std::remove_cvref_t<A>, Action>
        bool push(U&& state, A&& action, Cost cost, Next next = Next::Turn) {
            return emit(std::forward<A>(action), cost, Hash{}, false, false, next, [&]() -> decltype(auto) { return std::forward<U>(state); });
        }
        template<class A, class Maker>
        requires std::same_as<std::remove_cvref_t<A>, Action> && std::invocable<Maker&&>
        bool push_lazy(A&& action, Cost cost, Maker&& maker, Next next = Next::Turn) {
            return emit(std::forward<A>(action), cost, Hash{}, false, false, next, std::forward<Maker>(maker));
        }
        template<class U, class A, class B>
        requires StateArg<U, State> && std::same_as<std::remove_cvref_t<A>, Action> && BoolArg<B>
        bool push(U&& state, A&& action, Cost cost, B is_answer, Next next = Next::Turn) {
            return emit(std::forward<A>(action), cost, Hash{}, false, is_answer, next, [&]() -> decltype(auto) { return std::forward<U>(state); });
        }
        template<class A, class B, class Maker>
        requires std::same_as<std::remove_cvref_t<A>, Action> && std::invocable<Maker&&> && BoolArg<B>
        bool push_lazy(A&& action, Cost cost, B is_answer, Maker&& maker, Next next = Next::Turn) {
            return emit(std::forward<A>(action), cost, Hash{}, false, is_answer, next, std::forward<Maker>(maker));
        }
        template<class U, class A, class H>
        requires StateArg<U, State> && std::same_as<std::remove_cvref_t<A>, Action> && CompatibleHashArg<H, Hash>
        bool push(U&& state, A&& action, Cost cost, H hash, Next next = Next::Turn) {
            return emit(std::forward<A>(action), cost, static_cast<Hash>(hash), true, false, next, [&]() -> decltype(auto) { return std::forward<U>(state); });
        }
        template<class A, class H, class Maker>
        requires std::same_as<std::remove_cvref_t<A>, Action> && std::invocable<Maker&&> && CompatibleHashArg<H, Hash>
        bool push_lazy(A&& action, Cost cost, H hash, Maker&& maker, Next next = Next::Turn) {
            return emit(std::forward<A>(action), cost, static_cast<Hash>(hash), true, false, next, std::forward<Maker>(maker));
        }
        template<class U, class A, class H, class B>
        requires StateArg<U, State> && std::same_as<std::remove_cvref_t<A>, Action> && CompatibleHashArg<H, Hash> && BoolArg<B>
        bool push(U&& state, A&& action, Cost cost, H hash, B is_answer, Next next = Next::Turn) {
            return emit(std::forward<A>(action), cost, static_cast<Hash>(hash), true, is_answer, next, [&]() -> decltype(auto) { return std::forward<U>(state); });
        }
        template<class A, class H, class B, class Maker>
        requires std::same_as<std::remove_cvref_t<A>, Action> && std::invocable<Maker&&> && CompatibleHashArg<H, Hash> && BoolArg<B>
        bool push_lazy(A&& action, Cost cost, H hash, B is_answer, Maker&& maker, Next next = Next::Turn) {
            return emit(std::forward<A>(action), cost, static_cast<Hash>(hash), true, is_answer, next, std::forward<Maker>(maker));
        }
    };

private:
    template<class Value> class Engine {
        // 診断無効時は、通知用の集計・時計取得も行わない。
        friend class Emitter<Value, void>;
        static_assert(std::copy_constructible<Value> && std::assignable_from<Value&, Value>);
        using ActiveRuntime = std::conditional_t<diagnostics_enabled, DebugRuntime, Runtime>;
        ActiveRuntime rt_;
        std::int64_t start_us_ = sub_detail::now_us(), limit_us_ = 0;
        int unchecked_ = 0;
        bool observe_time_ = false, terminal_seen_ = false;
        struct Position { int turn = 0; std::int64_t sub_step = 0, depth = 0; };
        Position from_;
        static Param normalize(Param p) {
            p.max_turn = std::max(0, p.max_turn);
            p.beam_width = std::max(1, p.beam_width);
            p.sub_beam_width = p.sub_beam_width <= 0 ? p.beam_width : p.sub_beam_width;
            p.time_check_interval = std::max(1, p.time_check_interval);
            if (!(p.time_limit_ms > 0)) p.time_limit_ms = 0;
            p.sweep_width = std::clamp(p.sweep_width, 1, p.beam_width);
            p.first_sweep_width = p.first_sweep_width <= 0 ? p.sweep_width : std::min(p.first_sweep_width, p.beam_width);
            p.max_sweeps = std::max<std::int64_t>(0, p.max_sweeps);
            return p;
        }
        void tick(bool force = false) {
            // 観測しない実行でも、確認カウンタを繰り越さない。
            unchecked_ = 0;
            if (!force && limit_us_ == 0 && !observe_time_) return;
            rt_.elapsed_us = sub_detail::now_us() - start_us_;
            if (limit_us_ > 0 && rt_.elapsed_us >= limit_us_) rt_.time_limit_reached = true;
        }
        void best_metadata(Cost cost, const Position& p) {
            rt_.found = true; rt_.best_cost = cost; rt_.best_turn = p.turn;
            rt_.best_sub_step = p.sub_step; rt_.best_depth = p.depth;
            if constexpr (diagnostics_enabled) { ++rt_.best_update_count; rt_.last_best_update_us = -1; }
        }

        void after_expand() {
            ++rt_.expanded_nodes;
            observe_best_update();
            if (++unchecked_ >= rt_.param.time_check_interval) tick();
        }
        template<class View, class Expand>
        void invoke_expand(Expand& expand, const View& now) {
            Emitter<Value, void> emit(*this, now.hash);
            if constexpr (std::is_invocable_v<Expand&, const View&, const Runtime&, Emitter<Value, void>&>) {
                expand(now, static_cast<const Runtime&>(rt_), emit);
            } else {
                static_assert(std::is_invocable_v<Expand&, const View&, Emitter<Value, void>&>, "unsupported expand signature");
                expand(now, emit);
            }
        }
        template<class Hook> void notify(EventType event, Hook& hook) {
            if constexpr (diagnostics_enabled) {
                tick(true); peak();
                hook(event, static_cast<const DebugRuntime&>(rt_));
                tick();
            }
        }
        bool destination(Next next, Cost cost, Position& to, SelectionStats*& stats) {
            if constexpr (diagnostics_enabled) ++rt_.generated_candidates;
            if (next != Next::Sub && next != Next::Turn) { if constexpr (diagnostics_enabled) ++rt_.invalid_candidates; return false; }
            if constexpr (diagnostics_enabled) stats = next == Next::Sub ? &rt_.sub_selection : &rt_.turn_selection;
            if (!sub_detail::valid(cost) || from_.turn >= rt_.param.max_turn) { if constexpr (diagnostics_enabled) ++rt_.invalid_candidates; return false; }
            to.turn = from_.turn + (next == Next::Turn);
            if (to.turn == rt_.param.max_turn) {
                rt_.completed_max_turn = true;
                terminal_seen_ = true;
            }
            return true;
        }
        bool improves(Cost cost, bool is_answer, const Position& to) const {
            return (is_answer || (to.turn == rt_.param.max_turn && rt_.param.max_turn_is_answer)) &&
                (!rt_.found || cost < rt_.best_cost);
        }
        void result_metadata(Result<Value>& result) {
            result.found = rt_.found;

            result.completed_max_turn = rt_.completed_max_turn;
            result.best_cost = rt_.best_cost; result.best_turn = rt_.best_turn;
            result.best_sub_step = rt_.best_sub_step; result.best_depth = rt_.best_depth;
        }

        // turn/sub_stepはQueueで共有し、経路ごとのdepthはKeyに残す。
        struct Key {
            Cost cost{}; Hash hash{}; std::int64_t depth = 0;
            std::uint64_t serial = 0;
            int next_free = -1;
            bool has_hash = false;
        };
        struct Item { Cost cost; Hash hash; Position pos; bool has_hash; Value value; };
        struct Ends { int best = -1, worst = -1; };
        // 最良・最悪を一つの木で管理。Stateを比較用の配列から分離する。
        class Queue {
            int width_;
            bool hashed_;
            std::size_t base_;
            std::vector<std::optional<Value>> values_;
            std::vector<Key> keys_;
            std::vector<Ends> tree_;
            std::vector<int> table_;
            int free_ = -1, size_ = 0;
            Position position_;
            std::size_t bucket(Hash h) const { return static_cast<std::size_t>(sub_detail::mix(h)) & (table_.size() - 1); }
            bool before(int a, int b) const {
                if (keys_[a].cost != keys_[b].cost) return keys_[a].cost < keys_[b].cost;
                return keys_[a].serial < keys_[b].serial;
            }
            Ends ends(std::size_t p) const {
                if (p < base_) return tree_[p];
                const auto i = p - base_;
                return i < values_.size() && values_[i] ? Ends{static_cast<int>(i), static_cast<int>(i)} : Ends{};
            }
            void update(int slot) {
                std::size_t p = base_ + static_cast<std::size_t>(slot);
                while ((p /= 2) != 0) {
                    const auto a = ends(p * 2), b = ends(p * 2 + 1);
                    if (a.best < 0) tree_[p] = b;
                    else if (b.best < 0) tree_[p] = a;
                    else tree_[p] = {before(a.best, b.best) ? a.best : b.best,
                                     before(a.worst, b.worst) ? b.worst : a.worst};
                }
            }
            int find(Hash h) const {
                if (table_.empty()) return -1;
                auto p = bucket(h);
                while (table_[p] >= 0) {
                    if (keys_[table_[p]].hash == h) return table_[p];
                    p = (p + 1) & (table_.size() - 1);
                }
                return -1;
            }
            void erase(int slot, bool update_tree = true) {
                if (hashed_ && keys_[slot].has_hash) {
                    auto p = bucket(keys_[slot].hash);
                    while (table_[p] != slot) p = (p + 1) & (table_.size() - 1);
                    const auto mask = table_.size() - 1;
                    for (auto j = (p + 1) & mask; table_[j] >= 0; j = (j + 1) & mask) {
                        const auto home = bucket(keys_[table_[j]].hash);
                        if (((p - home) & mask) < ((j - home) & mask)) { table_[p] = table_[j]; p = j; }
                    }
                    table_[p] = -1;
                }
                values_[slot].reset();
                keys_[slot].next_free = free_; free_ = slot; --size_;
                if (update_tree) update(slot);
            }
            Item take_value(int slot) {
                const auto& k = keys_[slot];
                auto pos = position_; pos.depth = k.depth;
                return {k.cost, k.hash, pos, k.has_hash, std::move(*values_[slot])};
            }
        public:
            struct Choice { int slot = -1; bool duplicate = false; };
            Queue(int width, bool hashed)
                : width_(width), hashed_(hashed), base_(std::bit_ceil(static_cast<std::size_t>(width))) {
                values_.reserve(static_cast<std::size_t>(width)); keys_.reserve(static_cast<std::size_t>(width));
                tree_.resize(base_);
            }
            template<class Add> void visit_costs(const Add& add) const {
                for (std::size_t i = 0; i < values_.size(); ++i) if (values_[i]) add(keys_[i].cost);
            }
            int size() const { return size_; }
            bool empty() const { return size_ == 0; }
            int best() const { return base_ == 1 ? 0 : tree_[1].best; }
            int worst() const { return base_ == 1 ? 0 : tree_[1].worst; }

            Cost worst_cost() const { return keys_[worst()].cost; }
            Choice choose(Cost cost, Hash hash, bool has_hash, SelectionStats* stats) const {
                const bool full = size_ == width_;
                if (full && !(cost < worst_cost())) { if constexpr (diagnostics_enabled) ++stats->rejected_by_width; return {}; }
                if constexpr (diagnostics_enabled) stats->hash_checked += has_hash && hashed_;
                const int found = has_hash && hashed_ ? find(hash) : -1;
                if (found >= 0) {
                    if (!(cost < keys_[found].cost)) { if constexpr (diagnostics_enabled) ++stats->rejected_by_hash; return {}; }
                    return {found, true};
                }
                return {full ? worst() : (free_ >= 0 ? free_ : static_cast<int>(values_.size())), false};
            }
            // 受理位置へ保存。返り値は純増件数。初期Stateの挿入にも使う。
            int put(Choice choice, Item&& item, std::uint64_t serial) {
                if (empty()) position_ = item.pos;
                const int i = choice.slot;
                if (static_cast<std::size_t>(i) == values_.size()) { values_.emplace_back(); keys_.emplace_back(); }
                const bool replaced = values_[i].has_value();
                if (replaced) erase(i, false);
                if (free_ >= 0) { assert(free_ == i); free_ = keys_[i].next_free; }
                values_[i].emplace(std::move(item.value));
                keys_[i] = {item.cost, item.hash, item.pos.depth, serial, -1, item.has_hash};
                ++size_; update(i);
                if (hashed_ && item.has_hash) {
                    if (table_.empty()) table_.assign(std::bit_ceil(static_cast<std::size_t>(width_) * 2), -1);
                    auto p = bucket(item.hash);
                    while (table_[p] >= 0) p = (p + 1) & (table_.size() - 1);
                    table_[p] = i;
                }
                return replaced ? 0 : 1;
            }
            Item pop() {
                const int i = best(); Item item = take_value(i); erase(i); return item;
            }
            void clear() {
                values_.clear(); keys_.clear(); free_ = -1; size_ = 0;
                std::fill(tree_.begin(), tree_.end(), Ends{});
                std::fill(table_.begin(), table_.end(), -1);
            }
            void drain(std::vector<Item>& frontier) {
                frontier.clear(); frontier.reserve(static_cast<std::size_t>(size_));
                for (int i = 0; i < static_cast<int>(values_.size()); ++i) if (values_[i]) frontier.push_back(take_value(i));
                clear();
            }
        };

        std::vector<std::unique_ptr<Queue>> layers_;
        std::unique_ptr<Queue> spare_; // 空になったキューの領域を次のターンへ再利用する。
        std::unique_ptr<Queue> sub_;
        std::vector<Item> frontier_;
        std::vector<std::size_t> order_;
        std::optional<Value> best_;
        std::size_t pending_ = 0;
        std::uint64_t serial_ = 0;
        bool sub_empty() const { return !sub_ || sub_->empty(); }
        Queue& layer(int t) {
            auto& q = layers_[static_cast<std::size_t>(t)];
            if (!q) q = spare_ ? std::move(spare_) : std::make_unique<Queue>(rt_.param.beam_width, rt_.param.use_hash_dedup);
            return *q;
        }
        Queue* next_queue() const {
            const auto t = static_cast<std::size_t>(rt_.turn) + 1;
            return t < layers_.size() ? layers_[t].get() : nullptr;
        }
        bool next_exists() const {
            const auto* q = next_queue();
            return terminal_seen_ || (q && !q->empty());
        }
        bool stop_on_timeout() {
            if (!rt_.time_limit_reached || (rt_.param.continue_on_timeout && !rt_.found)) return false;
            rt_.stop_reason = StopReason::TimeLimit;
            return true;
        }
        void peak() {
            if constexpr (diagnostics_enabled) {
                rt_.pending_candidates = pending_ + (rt_.sub_step > 0 ? (rt_.frontier_size - rt_.frontier_expanded) : 0) + (sub_ ? static_cast<std::size_t>(sub_->size()) : 0);
                rt_.peak_pending_candidates = std::max(rt_.peak_pending_candidates, rt_.pending_candidates);
            }
        }
        template<class Maker>
        bool push_candidate(Cost cost, Hash hash, bool has_hash, bool is_answer, Next next, Maker&& maker) {
            Position to;
            SelectionStats* stats = nullptr;
            if (!destination(next, cost, to, stats)) return false;
            if constexpr (diagnostics_enabled) rt_.answer_candidates += is_answer || (to.turn == rt_.param.max_turn && rt_.param.max_turn_is_answer);
            const bool best = improves(cost, is_answer, to);
            const bool answer_only = is_answer && !rt_.param.expand_answers && to.turn < rt_.param.max_turn;

            Queue* q = nullptr;
            typename Queue::Choice choice;
            if (to.turn < rt_.param.max_turn && !answer_only) {
                if (next == Next::Sub && !sub_) sub_ = std::make_unique<Queue>(rt_.param.sub_beam_width, rt_.param.use_hash_dedup);
                q = next == Next::Sub ? sub_.get() : &layer(to.turn);
                choice = q->choose(cost, hash, has_hash, stats);
            }
            if (!best && choice.slot < 0) return false;
            to.sub_step = next == Next::Sub ? from_.sub_step + 1 : 0;
            to.depth = from_.depth + 1;
            // 最良解だけを残す場合は、候補用Valueを作ってコピーしない
            if (choice.slot < 0) {
                best_.emplace(std::invoke(std::forward<Maker>(maker)));
                best_metadata(cost, to);
                return false;
            }
            // 中間Valueからの追加moveを避け、明示的な型変換は維持する。
            Item item{cost, hash, to, has_hash, Value(std::invoke(std::forward<Maker>(maker)))};
            if (best) { best_.emplace(item.value); best_metadata(cost, to); }
            const int added = q->put(choice, std::move(item), serial_++);
            if constexpr (diagnostics_enabled) { ++stats->accepted; if (choice.duplicate) ++stats->replaced_by_hash; else if (!added) ++stats->replaced_by_width; }
            if (next == Next::Turn) pending_ += static_cast<std::size_t>(added);
            if (added) peak();
            return true;
        }
        template<class Expand> void expand_item(Item& item, Expand& expand) {
            from_ = item.pos;
            const StateView<Value> now{from_.turn, item.cost, item.value, item.hash, item.has_hash, from_.sub_step, from_.depth};
            begin_expansion();
            invoke_expand(expand, now);

            after_expand();
        }
        template<class Expand, class Hook> void visit(int t, Expand& expand, Hook& hook) {
            auto& q = *layers_[static_cast<std::size_t>(t)];
            rt_.turn = t; rt_.sub_step = 0; terminal_seen_ = false;
            const int width = rt_.sweep == 1 ? rt_.param.first_sweep_width : rt_.param.sweep_width;
            const int limit = rt_.time_limit_reached ? 1 : width;
            const int count = std::min(limit, q.size());
            begin_frontier(static_cast<std::size_t>(count), static_cast<std::size_t>(limit));
            if constexpr (diagnostics_enabled) sample_layer_costs([&](const auto& add) { q.visit_costs(add); });
            bool interrupted = false, step_reported = false;
            auto body = [&] {
                int done = 0;
                for (; done < count; ++done) {
                    if (stop_on_timeout()) { interrupted = true; return; }
                    if (rt_.time_limit_reached && next_exists()) break;
                    auto item = q.pop(); --pending_;
                    expand_item(item, expand);
                    if (stop_on_timeout()) { interrupted = true; return; }
                }
                tick();
                if (stop_on_timeout()) { interrupted = true; return; }
                notify(EventType::SubStepEnd, hook); step_reported = true;
                if constexpr (diagnostics_enabled) if (done < count) rt_.sweep_completed = false;
                while (!sub_empty() && !(rt_.time_limit_reached && next_exists())) {
                    if (stop_on_timeout()) { interrupted = true; return; }
                    sub_->drain(frontier_); step_reported = false;
                    rt_.sub_step = frontier_.front().pos.sub_step;
                    begin_frontier(frontier_.size(), rt_.param.sub_beam_width);
                    if constexpr (diagnostics_enabled) sample_layer_costs([&](const auto& add) {
                        for (const auto& item : frontier_) add(item.cost);
                    });
                    bool ordered = false;
                    std::size_t i = 0;
                    for (; i < frontier_.size(); ++i) {
                        if (stop_on_timeout()) { interrupted = true; return; }
                        if (rt_.time_limit_reached && next_exists()) break;
                        if (rt_.time_limit_reached && !ordered) {
                            order_.resize(frontier_.size()); std::iota(order_.begin(), order_.end(), std::size_t{0});
                            std::stable_sort(order_.begin() + static_cast<std::ptrdiff_t>(i), order_.end(),
                                [&](std::size_t a, std::size_t b) { return frontier_[a].cost < frontier_[b].cost; });
                            ordered = true;
                        }
                        expand_item(frontier_[ordered ? order_[i] : i], expand);
                    }
                    tick();
                    if (stop_on_timeout()) { interrupted = true; return; }
                    notify(EventType::SubStepEnd, hook); step_reported = true;
                    if constexpr (diagnostics_enabled) if (i < frontier_.size()) rt_.sweep_completed = false;
                }
                if (stop_on_timeout()) { interrupted = true; return; }
            };
            body();
            if (interrupted && !step_reported) notify(EventType::SubStepEnd, hook);
            if constexpr (diagnostics_enabled)
                if (rt_.frontier_expanded < rt_.frontier_size || !sub_empty()) rt_.sweep_completed = false;
            notify(EventType::TurnEnd, hook);
            if (interrupted) return;
            frontier_.clear(); if (!sub_empty()) sub_->clear();
            if constexpr (diagnostics_enabled) rt_.frontier_expanded = rt_.frontier_size;
            if (q.empty()) spare_ = std::move(layers_[static_cast<std::size_t>(t)]);
        }
    public:
        Engine(const Param& param, Value&& initial, Cost cost, Hash hash, bool has_hash) {
            rt_.param = normalize(param); limit_us_ = sub_detail::to_us(rt_.param.time_limit_ms);
            if (!sub_detail::valid(cost)) { rt_.stop_reason = StopReason::InvalidInitial; return; }
            if (rt_.param.max_turn == 0) {
                rt_.completed_max_turn = true; rt_.stop_reason = StopReason::Completed;
                if (rt_.param.max_turn_is_answer) { best_.emplace(std::move(initial)); best_metadata(cost, {}); }
                return;
            }
            layers_.resize(static_cast<std::size_t>(rt_.param.max_turn));
            layer(0).put({0, false}, {cost, hash, {}, has_hash, std::move(initial)}, serial_++);
            pending_ = 1; peak();
        }
        template<class Expand, class Hook>
        Result<Value> run(Expand&& expand, Hook&& hook, bool defer_finish = false) {
            observe_time_ = diagnostics_enabled ||
                std::is_invocable_v<Expand&, const StateView<Value>&, const Runtime&, Emitter<Value, void>&>;
            tick(true); if constexpr (diagnostics_enabled) if (rt_.found) rt_.first_solution_us = rt_.last_best_update_us = 0;
            notify(EventType::RunStart, hook);
            while (rt_.stop_reason == StopReason::None) {
                tick();
                if (stop_on_timeout()) break;
                if (pending_ == 0) { rt_.stop_reason = rt_.completed_max_turn ? StopReason::Completed : StopReason::Exhausted; break; }
                const bool rescue = rt_.time_limit_reached;
                if (!rescue && rt_.param.max_sweeps > 0 && rt_.sweep >= rt_.param.max_sweeps) {
                    rt_.stop_reason = StopReason::SweepLimit; break;
                }
                ++rt_.sweep;
                if constexpr (diagnostics_enabled) rt_.sweep_completed = true;
                notify(EventType::SweepStart, hook);
                int t = 0;
                for (; t < rt_.param.max_turn; ++t) {
                    tick();
                    if (stop_on_timeout()) break;
                    if (layers_[static_cast<std::size_t>(t)] && !layers_[static_cast<std::size_t>(t)]->empty()) visit(t, expand, hook);
                    if (rt_.stop_reason != StopReason::None) { ++t; break; }
                }
                if constexpr (diagnostics_enabled) {
                    // 訪問済みturnの残りはvisitで判定済み。未訪問turnの予定候補だけを確認する。
                    rt_.sweep_completed &= std::none_of(layers_.begin() + t, layers_.end(),
                        [](const auto& q) { return q && !q->empty(); });
                    rt_.completed_sweeps += rt_.sweep_completed;
                }
                tick();
                stop_on_timeout();
                notify(EventType::SweepEnd, hook);
                if (rt_.stop_reason != StopReason::None) break;
            }
            notify(EventType::SearchEnd, hook);
            Result<Value> result;
            result.best_state = std::move(best_);

            result_metadata(result);
            if (!defer_finish) finish(result, hook);
            return result;
        }

    private:
        // 選別後・展開前のcostを2回走査。基準値との差で平均・母分散を安定に求める。
        template<class Visit> void sample_layer_costs(Visit visit) {
            if constexpr (diagnostics_enabled) {
                rt_.layer_size = 0;
                rt_.layer_cost_mean = rt_.layer_cost_stddev = 0;
                long double origin = 0, sum = 0;
                bool finite = true;
                visit([&](Cost cost) {
                    if (rt_.layer_size++ == 0) rt_.layer_cost_min = rt_.layer_cost_max = cost;
                    else {
                        rt_.layer_cost_min = std::min(rt_.layer_cost_min, cost);
                        rt_.layer_cost_max = std::max(rt_.layer_cost_max, cost);
                    }
                    const long double value = static_cast<long double>(cost);
                    if (!std::isfinite(value)) { finite = false; return; }
                    if (rt_.layer_size == 1) origin = value;
                    sum += value - origin;
                });
                if (rt_.layer_size == 0) return;
                const long double mean = sum / static_cast<long double>(rt_.layer_size);
                if (!finite || !std::isfinite(mean)) {
                    rt_.layer_cost_mean = rt_.layer_cost_stddev = std::numeric_limits<long double>::quiet_NaN();
                    return;
                }
                long double variance = 0;
                visit([&](Cost cost) {
                    const long double diff = (static_cast<long double>(cost) - origin) - mean;
                    variance += diff * diff;
                });
                rt_.layer_cost_mean = origin + mean;
                rt_.layer_cost_stddev = std::sqrt(variance / static_cast<long double>(rt_.layer_size));
            }
        }
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
                rt_.sub_expanded_nodes += rt_.sub_step > 0;
            }
        }
        void observe_best_update() {
            if constexpr (diagnostics_enabled) {
                if (rt_.found && rt_.last_best_update_us < 0) {
                    if (rt_.first_solution_us < 0) {
                        tick(true);
                        rt_.first_solution_us = rt_.last_best_update_us = rt_.elapsed_us;
                    } else rt_.last_best_update_us = sub_detail::now_us() - start_us_;
                }
            }
        }
    public:
        template<class ResultType, class Hook>
        void finish(ResultType& result, Hook& hook) {
            tick(true);
            if constexpr (diagnostics_enabled) { peak(); hook(EventType::RunEnd, static_cast<const DebugRuntime&>(rt_)); }
            result.runtime = rt_; result.time_limit_reached = rt_.time_limit_reached;
        }
};
private:
    template<class State, class Action, class Expand> struct HistoryExpand {
        using Value = HistoryStep<Action>;
        using Pool = beam_history_detail::Pool<Value>;
        using Stored = beam_history_detail::TracedState<State, Value>;
        Pool& pool;
        Expand& expand;
        template<class... Context>
        void invoke(const StateView<Stored>& now, Emitter<Stored, void>& raw, Context&&... context) {
            const auto& stored = now.state;
            StateView<State> view{now.turn, now.cost, stored.state, now.hash, now.has_hash};
            view.sub_step = now.sub_step; view.depth = now.depth;
            Emitter<State, Action> emit(raw, pool, stored.trace.id());
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
        using Value = HistoryStep<Action>;
        using Pool = beam_history_detail::Pool<Value>;
        using Stored = beam_history_detail::TracedState<State, Value>;
        Pool pool;
        HistoryExpand<State, Action, Expand> bridge{pool, expand};
        Engine<Stored> engine(param, Stored{std::move(initial_state), {}}, initial_cost, hash, has_hash);
        auto raw = engine.run(bridge, hook, true);
        Result<State, Action> result;
        result.found = raw.found;

        result.completed_max_turn = raw.completed_max_turn;
        result.best_cost = raw.best_cost;
        result.best_turn = raw.best_turn;
        result.best_sub_step = raw.best_sub_step;
        result.best_depth = raw.best_depth;

        if (raw.best_state) {
            result.best_state.emplace(std::move(raw.best_state->state));
            std::vector<std::size_t> reverse;
            for (auto id = raw.best_state->trace.id(); id != Pool::none; id = pool.parent(id)) reverse.push_back(id);
            result.path.reserve(reverse.size()); result.turn_offsets.push_back(0);
            for (auto i = reverse.rbegin(); i != reverse.rend(); ++i) {
                const auto& step = pool.value(*i);
                result.path.push_back(step.action);
                if (step.next_turn) result.turn_offsets.push_back(result.path.size());
            }
        }
        engine.finish(result, hook);
        return result;
    }
public:
    // Hash付き初期状態から探索する。期待O(G log(2+max(W,U))+E+状態保存費用)。
    // chokudai版はスイープのターン走査費用を加算する。操作列はAction型指定時だけライブラリが保持する。
    template<class State, class Action = void, class H, class Expand, class DebugHook = NoOp>
    requires CompatibleHashArg<H, Hash>
    static Result<State, Action> run(const Param& param, State initial_state, Cost initial_cost, H initial_hash,
                             Expand&& expand, DebugHook&& debug_hook = DebugHook{}) {
        if constexpr (std::is_void_v<Action>) {
        Engine<State> engine(param, std::move(initial_state), initial_cost, static_cast<Hash>(initial_hash), true);
        return engine.run(std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));
        } else {
            return run_history<State, Action>(param, std::move(initial_state), initial_cost, static_cast<Hash>(initial_hash), true, expand, debug_hook);
        }
    }
    // Hashなし初期状態から探索する。同じ計算量。Hash付きpushまで重複排除に参加しない。
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
#define RUNTIME_TEST_BEAM cs::ChokudaiCopySub
#define RUNTIME_TEST_CLOCK cs::sub_detail::TestClock
#define RUNTIME_TEST_DELTA 0
#define RUNTIME_TEST_MULTI 0
#define RUNTIME_TEST_SUB 1
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
#if RUNTIME_TEST_SUB
        const bool layer_event = event == E::SubStepEnd;
#else
        const bool layer_event = event == E::TurnEnd;
#endif
        if (layer_event && rt.turn == 0)
            check(rt.layer_size == 1 && rt.layer_cost_min == 0 && rt.layer_cost_max == 0 &&
                  rt.layer_cost_mean == 0 && rt.layer_cost_stddev == 0, "initial layer snapshot");
        if (layer_event && rt.turn == 1)
            check(rt.layer_size == 2 && rt.layer_cost_min == 4 && rt.layer_cost_max == 8 &&
                  rt.layer_cost_mean == 6 && rt.layer_cost_stddev == 2, "selected input layer moments");
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

// Included in the standalone source so its virtual clock is available.
namespace full_review_test {
void sweep_completion() {
    using namespace runtime_test;
    for (int scenario = 0; scenario != 4; ++scenario) {
        reset_clock();
        auto p = parameters();
        p.max_turn = scenario == 1 ? 3 : 2;
        p.beam_width = 2; p.first_sweep_width = scenario == 2 ? 2 : 1;
        p.max_sweeps = 1; p.time_limit_ms = 1; p.time_check_interval = 1;
        p.continue_on_timeout = false;
        bool completed = false, saw_end = false;
        auto expand = [&](const B::StateView<int>& now, B::Emitter<int>& emit) {
            if (now.turn == 0) {
                emit.push(1, 1, std::uint64_t{1});
                emit.push(2, 2, std::uint64_t{2});
            } else {
                if (scenario == 3) emit.push(3, 0, std::uint64_t{3}, B::Next::Sub);
                else emit.push(3, 0, std::uint64_t{3});
                Clock::value += 1000;
            }
        };
        const auto result = B::run<int>(p, 0, 0, expand, [&](E e, const B::DebugRuntime& rt) {
            if (e == E::SweepEnd) { completed = rt.sweep_completed; saw_end = true; }
        });
        // 0: last scheduled expansion finished; an extra root candidate belongs to the next sweep.
        // 1: a later turn is still pending. 2: the current quota is unfinished. 3: a Sub layer remains.
        check(saw_end && result.time_limit_reached, "completion boundary was reached");
        check(completed == (scenario == 0), "sweep completion follows remaining scheduled work");
    }
}
}

int main() {
#ifdef LOCAL
    full_review_test::sweep_completion();
    runtime_test::review_regressions();
    runtime_test::counts(); runtime_test::boundaries();
#if RUNTIME_TEST_CHOKU
    runtime_test::refinement();
#endif
#endif
    runtime_test::diagnostic_mode<true>();
    runtime_test::diagnostic_mode<false>();
    std::cout << "runtime diagnostics tests passed\n";
}

#endif
