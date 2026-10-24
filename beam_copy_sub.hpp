#pragma once
#include <bits/stdc++.h>

// beam_copy_sub_v23.hpp : ターン境界で比較をそろえる sub 探索。cost は小さいほど良い。
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

namespace bs {
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
struct Beam {
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
    enum class EventType { RunStart, TurnEnd, SearchEnd, RunEnd, SubStepEnd };
    struct Param {
        int max_turn = 0;              // 完了させるターン数。負値は0。
        int beam_width = 1;            // 次ターン開始候補の幅。1以上。
        double time_limit_ms = 0;      // 探索全体のsoft limit。0以下・NaNは無制限。
        int time_check_interval = 64;  // 通常の時計確認間隔。層境界でも確認。
        bool continue_on_timeout = true; // 時間切れで解未発見なら救済探索を続ける。
        bool expand_answers = true; // falseなら有効解を保存のみ行い、探索候補へ登録しない。
        bool max_turn_is_answer = true;
        bool use_hash_dedup = true;    // Hashが明示された候補だけを重複排除。

        int sub_beam_width = 0;        // 0以下ならbeam_widthを使用。
        bool use_turn_time_limit = false; // 残り時間を残りturnへ配分。全体制限が有効な場合だけ使用。
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
        std::int64_t turn_start_us = -1, turn_deadline_us = -1, turn_elapsed_us = 0;
        bool turn_time_limit_reached = false;
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
        std::uint64_t turns_cut_by_time = 0;
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
                event == EventType::RunEnd ;
            const bool changed = !rows.empty() &&
                (rt.diagnostic_flags != rows.back().runtime.diagnostic_flags ||
                 (rt.time_limit_reached && !rows.back().runtime.time_limit_reached) || rt.turns_cut_by_time > rows.back().runtime.turns_cut_by_time);
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
                "scope", "turn", "sub_step",
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
                // 終了・時間切れ・異常
                "stop_reason", "completed_max_turn", "time_limit_reached", "overrun_ms",
                "finalize_ms", "turn_budget_used_pct", "turn_budget_ms", "turn_time_limit_reached",
                "turns_cut_by_time", "expanded_after_timeout", "diagnostic_flags",
                // 設定
                "beam_width", "sub_beam_width", "max_turn", "time_limit_ms",
                "use_turn_time_limit", "time_check_interval", "continue_on_timeout", "use_hash_dedup",
                "max_turn_is_answer", "expand_answers",
                // 方式・スキーマの識別
                "algorithm", "state_mode", "turn_mode", "schema_version"
            }) cell(name);
            end();
            DebugRuntime zero{};
            const DebugRuntime* previous = &zero;
            const DebugRuntime* search_end = nullptr;

            for (const auto& row : rows) {
                const auto& r = row.runtime;
                if (row.event == EventType::SearchEnd) search_end = &r;

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
                cell(reason_name(r.stop_reason));
                cell(r.completed_max_turn);
                cell(r.time_limit_reached);
                opt(total && r.param.time_limit_ms > 0, std::max(0.0, ms(r.elapsed_us) - r.param.time_limit_ms));
                opt(total && search_end, ms(r.elapsed_us - s.elapsed_us));
                optional_ratio(local && r.turn_deadline_us >= 0, r.turn_elapsed_us, r.turn_deadline_us - r.turn_start_us, 100);
                opt(local && r.turn_deadline_us >= 0, ms(r.turn_deadline_us - r.turn_start_us));
                opt(local, r.turn_time_limit_reached);
                cell(s.turns_cut_by_time - p.turns_cut_by_time);
                cell(s.expanded_after_timeout - p.expanded_after_timeout);
                cell(flags(r.diagnostic_flags));
                cell(r.param.beam_width);
                cell(r.param.sub_beam_width);
                cell(r.param.max_turn);
                cell(r.param.time_limit_ms);
                cell(r.param.use_turn_time_limit);
                cell(r.param.time_check_interval);
                cell(r.param.continue_on_timeout);
                cell(r.param.use_hash_dedup);
                cell(r.param.max_turn_is_answer);
                cell(r.param.expand_answers);
                cell("beam");
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
        friend struct Beam;
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
    // 削除のないepoch表。stale登録が増えたら空きが十分あるうちに生存候補から再構築する。
    class FixedHashMap {
        std::vector<Hash> keys_;
        std::vector<int> values_;
        std::vector<std::uint32_t> epochs_;
        std::uint32_t epoch_ = 1;
        std::size_t mask_ = 0, occupied_ = 0;
    public:
        struct Hit { bool found; std::size_t slot; };
        static constexpr std::size_t absent = std::numeric_limits<std::size_t>::max();
        void init(int width) {
            const auto n = std::bit_ceil(static_cast<std::size_t>(width) * 4 + 17);
            mask_ = n - 1; occupied_ = 0;
            keys_.resize(n); values_.resize(n); epochs_.assign(n, 0);
        }
        void clear() {
            occupied_ = 0;
            if (++epoch_ == 0) { std::fill(epochs_.begin(), epochs_.end(), 0); epoch_ = 1; }
        }
        Hit lookup(Hash h) const {
            auto p = static_cast<std::size_t>(sub_detail::mix(h)) & mask_;
            for (std::size_t i = 0; i <= mask_; ++i, p = (p + 1) & mask_) {
                if (epochs_[p] != epoch_) return {false, occupied_ >= (mask_ + 1) - (mask_ + 1) / 4 ? absent : p};
                if (keys_[p] == h) return {true, p};
            }
            return {false, absent};
        }
        int get(std::size_t p) const { return values_[p]; }
        void set(std::size_t p, Hash h, int slot) {
            assert(p != absent);
            occupied_ += epochs_[p] != epoch_;
            keys_[p] = h; values_[p] = slot; epochs_[p] = epoch_;
        }
    };
    template<class Item> class Selector {
        int width_;
        std::size_t base_;
        bool hashed_;
        std::vector<int> tree_;
        std::vector<Cost> costs_;
        FixedHashMap map_;
        int worst_ = -1;
        int worse(int a, int b) const {
            if (a < 0) return b;
            if (b < 0) return a;
            if (costs_[a] != costs_[b]) return costs_[a] < costs_[b] ? b : a;
            return std::max(a, b);
        }
        void rebuild_hash() {
            map_.clear();
            for (int i = 0; i < size(); ++i) if (items[i].has_hash) {
                const auto h = items[i].hash;
                map_.set(map_.lookup(h).slot, h, i);
            }
        }
    public:
        struct Choice { int slot = -1; std::size_t hash_slot = FixedHashMap::absent; bool duplicate = false; };
        std::vector<Item> items;
        Selector(int width, bool hashed)
            : width_(width), base_(std::bit_ceil(static_cast<std::size_t>(width))), hashed_(hashed) {
            items.reserve(static_cast<std::size_t>(width));
            costs_.resize(static_cast<std::size_t>(width)); tree_.assign(base_ * 2, -1);
            if (hashed_) map_.init(width);
        }
        int size() const { return static_cast<int>(items.size()); }
        bool empty() const { return items.empty(); }

        void clear() {
            items.clear(); worst_ = -1;
            if (hashed_) map_.clear();
        }
        // State/Actionを作らずに保存位置を決める。幅判定をhash判定より先に行う。
        Choice choose(Cost cost, Hash h, bool has_hash, SelectionStats* stats) {
            const bool full = size() == width_;
            if (full && !(cost < costs_[worst_])) { if constexpr (diagnostics_enabled) ++stats->rejected_by_width; return {}; }
            Choice c;
            if (hashed_ && has_hash) {
                if constexpr (diagnostics_enabled) ++stats->hash_checked;
                auto hit = map_.lookup(h);
                if (hit.found) {
                    const int i = map_.get(hit.slot);
                    if (i < size() && items[i].has_hash && items[i].hash == h) {
                        if (!(cost < costs_[i])) { if constexpr (diagnostics_enabled) ++stats->rejected_by_hash; return {}; }
                        return {i, FixedHashMap::absent, true};
                    }
                }
                if (hit.slot == FixedHashMap::absent) { rebuild_hash(); hit = map_.lookup(h); }
                assert(hit.slot != FixedHashMap::absent);
                c.hash_slot = hit.slot;
            }
            c.slot = full ? worst_ : size();
            return c;
        }
        void put(Choice c, Item&& item, SelectionStats* stats) {
            const Cost cost = item.cost;
            const bool appended = c.slot == size();
            if (appended) items.push_back(std::move(item));
            else items[c.slot] = std::move(item);
            costs_[c.slot] = cost;
            if (c.hash_slot != FixedHashMap::absent) map_.set(c.hash_slot, items[c.slot].hash, c.slot);
            if constexpr (diagnostics_enabled) { ++stats->accepted; if (c.duplicate) ++stats->replaced_by_hash; else if (!appended) ++stats->replaced_by_width; }
            if (size() == width_) {
                if (appended) {
                    // 未使用の葉は初期値-1のまま。内部節点は下から全て再計算する。
                    for (int i = 0; i < size(); ++i) tree_[base_ + static_cast<std::size_t>(i)] = i;
                    for (std::size_t p = base_; --p > 0;) tree_[p] = worse(tree_[p * 2], tree_[p * 2 + 1]);
                } else {
                    std::size_t p = base_ + static_cast<std::size_t>(c.slot);
                    while ((p /= 2) != 0) tree_[p] = worse(tree_[p * 2], tree_[p * 2 + 1]);
                }
                worst_ = tree_[1];
            }
        }
    };
    template<class Value> class Engine {
        // 診断無効時は、通知用の集計・時計取得も行わない。
        friend class Emitter<Value, void>;
        static_assert(std::copy_constructible<Value> && std::assignable_from<Value&, Value>);
        using ActiveRuntime = std::conditional_t<diagnostics_enabled, DebugRuntime, Runtime>;
        ActiveRuntime rt_;
        std::int64_t start_us_ = sub_detail::now_us(), limit_us_ = 0;
        int unchecked_ = 0;
        bool observe_time_ = false;
        bool turn_timing_active_ = false;
        struct Position { int turn = 0; std::int64_t sub_step = 0, depth = 0; };
        Position from_;
        static Param normalize(Param p) {
            p.max_turn = std::max(0, p.max_turn);
            p.beam_width = std::max(1, p.beam_width);
            p.sub_beam_width = p.sub_beam_width <= 0 ? p.beam_width : p.sub_beam_width;
            p.time_check_interval = std::max(1, p.time_check_interval);
            if (!(p.time_limit_ms > 0)) p.time_limit_ms = 0;
            return p;
        }
        void tick(bool force = false) {
            // 観測しない実行でも、確認カウンタを繰り越さない。
            unchecked_ = 0;
            if (!force && limit_us_ == 0 && !observe_time_) return;
            rt_.elapsed_us = sub_detail::now_us() - start_us_;
            if (turn_timing_active_) {
                rt_.turn_elapsed_us = rt_.elapsed_us - rt_.turn_start_us;
                rt_.turn_time_limit_reached |= rt_.elapsed_us >= rt_.turn_deadline_us;
            }
            if (limit_us_ > 0 && rt_.elapsed_us >= limit_us_) rt_.time_limit_reached = true;
        }
        // 外側のturnにつき一度だけ配分。Sub層を進めても同じ締切を使う。
        void begin_turn_time() {
            if (!rt_.param.use_turn_time_limit || limit_us_ == 0) return;
            tick();
            turn_timing_active_ = true;
            rt_.turn_start_us = rt_.elapsed_us;
            rt_.turn_elapsed_us = 0;
            const auto remaining = std::max<std::int64_t>(0, limit_us_ - rt_.elapsed_us);
            rt_.turn_deadline_us = rt_.elapsed_us + remaining / (rt_.param.max_turn - rt_.turn);
            rt_.turn_time_limit_reached = rt_.elapsed_us >= rt_.turn_deadline_us;
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

        // turn/sub_stepはfrontierで共通。経路ごとに異なるdepthだけを保存する。
        struct Item {
            Cost cost;
            Hash hash;
            std::int64_t depth;
            bool has_hash;
            Value value;
        };
        using Pool = Selector<Item>;
        std::optional<Pool> sub_, next_;
        std::vector<Item> frontier_;
        std::optional<Value> best_;
        std::vector<std::size_t> order_;
        bool sub_empty() const { return !sub_ || sub_->empty(); }
        bool next_exists() const { return rt_.completed_max_turn || (next_ && !next_->empty()); }
        bool stop_on_timeout() {
            if (!rt_.time_limit_reached || (rt_.param.continue_on_timeout && !rt_.found)) return false;
            rt_.stop_reason = StopReason::TimeLimit;
            return true;
        }
        void peak() {
            if constexpr (diagnostics_enabled) {
                rt_.pending_candidates = (rt_.frontier_size - rt_.frontier_expanded) + (sub_ ? sub_->items.size() : 0) + (next_ ? next_->items.size() : 0);
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

            if (!answer_only && next == Next::Sub && !sub_) sub_.emplace(rt_.param.sub_beam_width, rt_.param.use_hash_dedup);
            Pool* pool = answer_only ? nullptr : (next == Next::Sub ? &*sub_ : &*next_);
            typename Pool::Choice choice;
            if (to.turn < rt_.param.max_turn && !answer_only) choice = pool->choose(cost, hash, has_hash, stats);
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
            Item item{cost, hash, to.depth, has_hash, Value(std::invoke(std::forward<Maker>(maker)))};
            if (best) { best_.emplace(item.value); best_metadata(cost, to); }
            const bool added = choice.slot == pool->size();
            pool->put(choice, std::move(item), stats);
            if (added) peak();
            return true;
        }
        const Item& item_at(std::size_t i) const { return frontier_[i]; }
        void materialize(Pool& pool, bool by_parent) {
            frontier_.swap(pool.items); pool.clear();
            begin_frontier(frontier_.size(), by_parent ? rt_.param.sub_beam_width : rt_.param.beam_width);
            if (by_parent) ++rt_.sub_step;
            else { ++rt_.turn; rt_.sub_step = 0; }
            peak();
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
            next_.emplace(rt_.param.beam_width, rt_.param.use_hash_dedup);
            frontier_.push_back({cost, hash, {}, has_hash, std::move(initial)});
            begin_frontier(1, rt_.param.beam_width); peak();
        }
        template<class Expand, class Hook>
        Result<Value> run(Expand&& expand, Hook&& hook, bool defer_finish = false) {
            observe_time_ = diagnostics_enabled ||
                std::is_invocable_v<Expand&, const StateView<Value>&, const Runtime&, Emitter<Value, void>&>;
            tick(true);
            if constexpr (diagnostics_enabled) if (rt_.found) rt_.first_solution_us = rt_.last_best_update_us = 0;
            notify(EventType::RunStart, hook);
            while (rt_.stop_reason == StopReason::None) {
                begin_turn_time();
                for (;;) {
                    tick();
                    if (stop_on_timeout()) break;
                    const std::size_t n = frontier_.size();
                    begin_frontier(n, rt_.sub_step == 0 ? rt_.param.beam_width : rt_.param.sub_beam_width);
                    if constexpr (diagnostics_enabled) sample_layer_costs([&](const auto& add) {
                        for (std::size_t j = 0; j < n; ++j) add(item_at(j).cost);
                    });
                    bool ordered = false;
                    std::size_t i = 0;
                    for (; i < n; ++i) {
                        if (stop_on_timeout()) break;
                        if ((rt_.time_limit_reached || rt_.turn_time_limit_reached) && next_exists()) break;
                        if (rt_.time_limit_reached && !ordered) {
                            order_.resize(n);
                            std::iota(order_.begin(), order_.end(), std::size_t{0});
                            std::stable_sort(order_.begin() + static_cast<std::ptrdiff_t>(i), order_.end(),
                                [&](std::size_t a, std::size_t b) { return item_at(a).cost < item_at(b).cost; });
                            ordered = true;
                        }
                        const std::size_t j = ordered ? order_[i] : i;

                        const auto& item = item_at(j);
                        from_ = {rt_.turn, rt_.sub_step, item.depth};
                        const StateView<Value> now{from_.turn, item.cost, item.value, item.hash, item.has_hash, from_.sub_step, from_.depth};
                        begin_expansion();
                        invoke_expand(expand, now);

                        after_expand();
                    }
                    tick();
                    notify(EventType::SubStepEnd, hook);
                    if (stop_on_timeout()) break;
                    if ((rt_.time_limit_reached || rt_.turn_time_limit_reached) && next_exists()) {
                        break;
                    }
                    if (sub_empty()) break;
                    materialize(*sub_, true);
                }
                turn_timing_active_ = false;
                if constexpr (diagnostics_enabled) {
                    if ((rt_.time_limit_reached || rt_.turn_time_limit_reached) &&
                        (rt_.frontier_expanded < rt_.frontier_size || !sub_empty())) ++rt_.turns_cut_by_time;
                }
                notify(EventType::TurnEnd, hook);
                if (rt_.stop_reason != StopReason::None) break;
                if (rt_.completed_max_turn) rt_.stop_reason = StopReason::Completed;
                else if (next_->empty()) rt_.stop_reason = StopReason::Exhausted;
                else {
                    if (sub_) sub_->clear();
                    materialize(*next_, false);
                }
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
    // 操作列はAction型指定時だけライブラリが保持する。
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
} // namespace bs


#if __INCLUDE_LEVEL__ == 0
#define RUNTIME_TEST_BEAM bs::Beam
#define RUNTIME_TEST_CLOCK bs::sub_detail::TestClock
#define RUNTIME_TEST_DELTA 0
#define RUNTIME_TEST_MULTI 0
#define RUNTIME_TEST_SUB 1
#define RUNTIME_TEST_CHOKU 0
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
        p.max_turn = 2; p.beam_width = 3; p.sub_beam_width = 1;
        p.time_limit_ms = 1; p.continue_on_timeout = false;
        B::DebugRuntime search{};
        auto expand = [&](const auto& now, auto& emit) {
            check(now.turn == 0, "no expansion after boundary timeout");
            for (int i = 1; i <= 3; ++i) h.push(emit, i, i, static_cast<std::uint64_t>(i));
        };
        auto hook = [&](E event, const B::DebugRuntime& rt) {
            if (event == E::TurnEnd && rt.turn == 0) Clock::value += 2000;
            if (event == E::SearchEnd) search = rt;
        };
        auto result = h.run(p, 0, expand, hook);
        check(result.time_limit_reached && result.runtime.expanded_nodes == 1, "boundary timeout stops before expanding");
        check(search.pending_candidates == 3 && search.frontier_size == 3 && search.frontier_limit == 3,
              "turn frontier uses beam width even when stopped before next expansion");
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
#endif
#endif
    runtime_test::diagnostic_mode<true>();
    runtime_test::diagnostic_mode<false>();
    std::cout << "runtime diagnostics tests passed\n";
}

#endif
