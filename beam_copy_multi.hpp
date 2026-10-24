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

namespace bs {
// ============================================================
// 公開concept
// ============================================================

// コスト最小化専用の軽量数値型である。
// 最大化問題では、ユーザー側で cost = -score のように変換する。
// floating point Cost も使えるが、NaN は比較順序を壊すため候補として棄却される。
template<class T>
concept CostType =
    ((std::integral<T> && std::is_signed_v<T>) || std::floating_point<T>) &&
    (!std::same_as<T, bool>);

// Hash重複排除用の型。64bit以下の符号なし整数のみを受け付ける。
template<class T>
concept HashType =
    std::integral<T> &&
    std::is_unsigned_v<T> &&
    (!std::same_as<T, bool>) &&
    (sizeof(T) <= sizeof(std::uint64_t));

// Emitter::push の引数誤用を減らすため、hash/step/bool/stateを概念で分ける。
template<class H, class Hash>
concept CompatibleHashArg =
    HashType<Hash> &&
    HashType<std::remove_cvref_t<H>> &&
    std::convertible_to<std::remove_cvref_t<H>, Hash> &&
    (sizeof(std::remove_cvref_t<H>) <= sizeof(Hash));

template<class S>
concept StepArg = std::same_as<std::remove_cvref_t<S>, int>;

template<class B>
concept BoolArg = std::same_as<std::remove_cvref_t<B>, bool>;

template<class U, class State>
concept StateArg = std::same_as<std::remove_cvref_t<U>, State>;

// ============================================================
// detail: 低レベル補助関数
// ============================================================

namespace detail {
// Beamに依存しない低レベル補助関数。
inline int ceil_pow2(int x) {
    int p = 1;
    while (p < x) p <<= 1;
    return p;
}

inline std::uint64_t mix64(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

template<CostType Cost>
inline bool is_valid_cost(Cost cost) {
    if constexpr (std::floating_point<Cost>) {
        return !std::isnan(cost);
    } else {
        (void)cost;
        return true;
    }
}

// doubleのms指定を内部比較用のマイクロ秒へ丸める。正の極小値は1usとして扱う。
inline std::int64_t time_limit_to_microseconds(double time_limit_ms) {
    if (!(time_limit_ms > 0.0)) return 0;
    const long double us = static_cast<long double>(time_limit_ms) * 1000.0L;
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
inline std::int64_t now_microseconds() {
#if __INCLUDE_LEVEL__ == 0
    ++TestClock::calls;
    if (TestClock::enabled) return TestClock::value;
#endif
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}
} // namespace detail

// ============================================================
// 公開APIと探索本体
// ============================================================

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
    template<class State> class Engine;

public:

// 探索パラメータ。
// run 開始時に max_turn は 0 以上、beam_width は 1 以上、
// max_step は [1, max(1,max_turn)] に丸められる。
// 解候補は is_answer=true の候補、または max_turn_is_answer=true のときの max_turn 到達候補。
// is_answer=true は解の印。expand_answers=falseなら保存だけを行い、展開対象へ登録しない。
// 有効解の一部だけを終端にする場合はexpand_answers=trueとし、終端だけexpandで候補を出さない。
// time_limit_ms は探索全体のsoft limit。expand実行中は中断しない。
// use_turn_time_limit=trueでは各turn開始時に残り時間/残りturn数を配分する。
// turn期限後は未来候補があれば進み、なければ候補ができるか尽きるまで展開する。
// turn期限だけでは保持幅を縮めず、次turnで通常展開へ戻る。time_check_interval=1なら毎展開確認。
// 全体制限後は、解保存済みまたはcontinue_on_timeout=falseなら終了する。
// 未発見の救済時は、未展開候補から最良1本だけを追加展開する。
// 以後は各turnで最良1本だけを進め、到着候補も最良1件だけをmaterializeする。
struct Param {
    int max_turn = 0;               // 探索終了ターン。候補は 0..max_turn に到着できる。
    int max_step = 1;               // 1回の展開で進める最大ターン。
    int beam_width = 1;             // 各到着ターンに残す候補数。

    int hash_capacity = 0;          // use_hash_dedup=true かつ 0なら自動設定。過大値もbeam_width基準で丸める。

    double time_limit_ms = 0.0;    // 0なら無制限。全体のsoft limit。到達後の救済はcontinue_on_timeoutで指定。
    int time_check_interval = 64;   // 通常ビーム探索中、何ノード展開ごとに時間を見るか。expand中は中断しない。
    bool continue_on_timeout = true; // 時間切れで解未発見なら救済探索を続ける。
    bool expand_answers = true; // falseなら有効解を保存のみ行い、探索候補へ登録しない。
    bool max_turn_is_answer = true; // trueならmax_turn到達候補も解候補にする。
    bool use_hash_dedup = true;     // trueならHash付きpushで同一到着ターン内の重複排除を行う。Hashなしpushは対象外。
    bool print_warnings = true;     // 現状はhash表満杯など、結果解釈に注意が必要な状態をstderrへ警告する。
    bool use_turn_time_limit = false; // 残り時間を残りturnへ配分。全体制限が有効な場合だけ使用。
};

// Hookの発火タイミング。
enum class StopReason { None, Completed, Exhausted, TimeLimit, SweepLimit, InvalidInitial };

enum class EventType { RunStart, TurnEnd, SearchEnd, RunEnd };

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

// 実行情報と、diagnostics_enabled時だけ保持する診断情報。

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
        SelectionStats selection;
        std::size_t hash_table_capacity = 0; // 1選別領域の実効バケット数。未確保でも確保時の容量。
        std::uint64_t turns_cut_by_time = 0;
        std::uint64_t generated_step_sum = 0, expanded_step_sum = 0;
        int generated_step_max = 0;
        int pending_turns = 0, full_pending_turns = 0;
    private:
        template<class> friend class Engine;
        std::size_t pending_frontier_ = 0;
    };

// デフォルトHook。何もしない。
struct NoOp {
    void operator()(EventType, const Runtime&) const noexcept {}
};

// CSV出力Hook。
// 共通の実行情報と方式別の診断をCSVに出力する。
// 行はメモリ上に保持し、RunEndで一括してファイルへ書き出す。
// LOCAL定義時のみCSVを記録する。未定義時は同じ呼び出し方で使える空実装。
    // 診断の有効・無効は外側の diagnostics_enabled で決まる。
    struct CsvStatHook {
#ifdef LOCAL
        struct Row { EventType event; DebugRuntime runtime; };
        std::string filename;
        std::int64_t interval_us;
        std::vector<Row> rows;
        explicit CsvStatHook(std::string path = "beam_stat.csv", double interval_ms = 50)
            : filename(std::move(path)), interval_us(detail::time_limit_to_microseconds(interval_ms)) {}
        void operator()(EventType event, const DebugRuntime& rt) {
            if (event == EventType::RunStart) rows.clear();
            const bool mandatory = event == EventType::RunStart || event == EventType::SearchEnd ||
                event == EventType::RunEnd ;
            const bool changed = !rows.empty() &&
                (rt.diagnostic_flags != rows.back().runtime.diagnostic_flags ||
                 (rt.time_limit_reached && !rows.back().runtime.time_limit_reached) || rt.turns_cut_by_time > rows.back().runtime.turns_cut_by_time);
            if (mandatory || changed || rows.empty() || interval_us == 0 ||
                rt.elapsed_us - rows.back().runtime.elapsed_us >= interval_us) rows.push_back({event, rt});
            if (event == EventType::RunEnd) write();
        }
    private:
        static const char* event_name(EventType e) {
            switch(e) {
            case EventType::RunStart: return "RunStart";
            case EventType::TurnEnd: return "TurnEnd";
            case EventType::SearchEnd: return "SearchEnd";
            case EventType::RunEnd: return "RunEnd";
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
                "scope", "turn",
                // 解の品質・改善
                "best_cost", "best_gain", "best_updates", "best_unchanged_ms",
                "found", "best_turn", "answer_candidates", "first_solution_ms",
                "first_solution_budget_pct",
                // 層のcost・充足・進行
                "layer_size", "layer_cost_min", "layer_cost_mean", "layer_cost_max",
                "layer_cost_stddev", "frontier_fill_pct", "frontier_done_pct", "frontier_limit",
                // 探索量・速度
                "expand_per_sec", "expanded_nodes", "candidates_per_expand", "generated_candidates",
                "invalid_candidates",
                // Hash照会・除外と選別量
                "hash_checked_pct", "hash_discard_pct", "selection_attempts",
                // 残候補・方式別の負荷
                "pending_candidates", "peak_pending_candidates", "pending_turns", "full_pending_turn_pct",
                "generated_step_avg", "expanded_step_avg",
                // 終了・時間切れ・異常
                "stop_reason", "completed_max_turn", "time_limit_reached", "overrun_ms",
                "finalize_ms", "turn_budget_used_pct", "turn_budget_ms", "turn_time_limit_reached",
                "turns_cut_by_time", "expanded_after_timeout", "diagnostic_flags",
                // 設定
                "beam_width", "max_turn", "max_step", "time_limit_ms",
                "use_turn_time_limit", "time_check_interval", "continue_on_timeout", "use_hash_dedup",
                "hash_table_capacity", "max_turn_is_answer", "expand_answers",
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
                const bool local = row.event == EventType::TurnEnd ;
                const bool frontier = row.event == EventType::TurnEnd;
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
                opt(r.found, r.best_cost);
                opt(s.found && p.found, (p.best_cost == s.best_cost ? 0.0L : static_cast<long double>(p.best_cost) - static_cast<long double>(s.best_cost)));
                cell(s.best_update_count - p.best_update_count);
                opt(s.found, ms(s.elapsed_us - s.last_best_update_us));
                cell(r.found);
                opt(r.found, r.best_turn);
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
                write_selection(s.selection, p.selection);
                cell(s.pending_candidates);
                cell(s.peak_pending_candidates);
                cell(s.pending_turns);
                ratio(s.full_pending_turns, s.pending_turns, 100);
                ratio(s.generated_step_sum - p.generated_step_sum, generated - (s.invalid_candidates - p.invalid_candidates), 1);
                ratio(s.expanded_step_sum - p.expanded_step_sum,
                    expanded - (p.expanded_nodes == 0 && s.expanded_nodes > 0), 1);
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
                cell(r.param.max_turn);
                cell(r.param.max_step);
                cell(r.param.time_limit_ms);
                cell(r.param.use_turn_time_limit);
                cell(r.param.time_check_interval);
                cell(r.param.continue_on_timeout);
                cell(r.param.use_hash_dedup);
                cell(r.hash_table_capacity);
                cell(r.param.max_turn_is_answer);
                cell(r.param.expand_answers);
                cell("beam");
                cell("copy");
                cell("multi");
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


// 探索結果。
// Action型を指定した場合は操作列を共有木で保持し、pathに返す。
// 結果の最良値は best_cost / best_turn に一本化する。
template<class State, class Action = void> struct Result;
    template<class State> struct Result<State, void> {
    bool found = false;
    // time_limit_msに達したことを検出したか。
    // 検出後に探索を継続する場合は最良1本の縮退探索へ移る。
    // expand中の即時中断を意味しない。
    bool time_limit_reached = false;
    // 不正でないmax_turn到着候補を少なくとも1件生成した。
    // 全候補を通常幅で展開した保証ではない。
    bool completed_max_turn = false;
    bool hash_table_full = false;

    Cost best_cost{};
    int best_turn = 0;

    std::optional<State> best_state;
    Runtime runtime;
};

// expand に渡す候補追加器。
// 診断設定は外側のBeam型で決まり、Emitterの型引数には含めない。
// push/push_lazy の戻り値 true は、この呼び出し時点で候補がSelectorに入ったことを表す。
// 後続候補に置換される可能性があるため、最終結果に残る保証ではない。
// 有効解は幅・Hashの選別前に保存する。falseを返しても最良解を更新する場合がある。
// is_answer=true は解候補フラグ。expand_answers=falseなら保存だけを行う。
// push_lazy は探索候補または最良有効解の保存に必要な場合だけ maker() を1回呼ぶ。makerは保存されない。
template<class State, class Action = void> class Emitter;
    template<class State> class Emitter<State, void> {
public:
    template<class U, StepArg S>
    requires StateArg<U, State>
    bool push(U&& state, Cost cost, S step) {
        return push_state_impl(std::forward<U>(state), cost, now_hash_, static_cast<int>(step), false, false);
    }

    template<class U, StepArg S, BoolArg B>
    requires StateArg<U, State>
    bool push(U&& state, Cost cost, S step, B is_answer) {
        return push_state_impl(std::forward<U>(state), cost, now_hash_, static_cast<int>(step), false, is_answer);
    }

    template<class U, class H, StepArg S>
    requires StateArg<U, State> && CompatibleHashArg<H, Hash>
    bool push(U&& state, Cost cost, H hash, S step) {
        return push_state_impl(std::forward<U>(state), cost, static_cast<Hash>(hash), static_cast<int>(step), true, false);
    }

    template<class U, class H, StepArg S, BoolArg B>
    requires StateArg<U, State> && CompatibleHashArg<H, Hash>
    bool push(U&& state, Cost cost, H hash, S step, B is_answer) {
        return push_state_impl(std::forward<U>(state), cost, static_cast<Hash>(hash), static_cast<int>(step), true, is_answer);
    }

    template<StepArg S, class Maker>
    bool push_lazy(Cost cost, S step, Maker&& maker) {
        return push_lazy_impl(cost, now_hash_, static_cast<int>(step), false, false, std::forward<Maker>(maker));
    }

    template<StepArg S, BoolArg B, class Maker>
    bool push_lazy(Cost cost, S step, B is_answer, Maker&& maker) {
        return push_lazy_impl(cost, now_hash_, static_cast<int>(step), false, is_answer, std::forward<Maker>(maker));
    }

    template<class H, StepArg S, class Maker>
    requires CompatibleHashArg<H, Hash>
    bool push_lazy(Cost cost, H hash, S step, Maker&& maker) {
        return push_lazy_impl(cost, static_cast<Hash>(hash), static_cast<int>(step), true, false, std::forward<Maker>(maker));
    }

    template<class H, StepArg S, BoolArg B, class Maker>
    requires CompatibleHashArg<H, Hash>
    bool push_lazy(Cost cost, H hash, S step, B is_answer, Maker&& maker) {
        return push_lazy_impl(cost, static_cast<Hash>(hash), static_cast<int>(step), true, is_answer, std::forward<Maker>(maker));
    }

private:
    friend class Engine<State>;

    Emitter(Engine<State>& engine, int turn, Hash now_hash) noexcept
        : engine_(engine), turn_(turn), now_hash_(now_hash) {}

    template<class U>
    bool push_state_impl(U&& state, Cost cost, Hash hash, int step, bool has_hash, bool is_answer) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return engine_.push_candidate(turn_, step, cost, hash, has_hash, is_answer, maker);
    }

    template<class Maker>
    bool push_lazy_impl(Cost cost, Hash hash, int step, bool has_hash, bool is_answer, Maker&& maker) {
        return engine_.push_candidate(turn_, step, cost, hash, has_hash, is_answer, std::forward<Maker>(maker));
    }

    Engine<State>& engine_;
    int turn_ = 0;
    Hash now_hash_{};
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
        friend struct Beam;
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

// ============================================================
// callback呼び出し補助
// ============================================================

template<class Expand, class State>
static void call_expand(Expand& expand, const StateView<State>& now, Runtime& rt, Emitter<State, void>& emit) {
    // expand は Runtime なし、または const Runtime& ありの2系統を許可する。
    // Runtime はライブラリ管理情報なので、ユーザー側から変更できないよう const 参照だけを渡す。
    if constexpr (std::is_invocable_v<Expand&, const StateView<State>&, const Runtime&, Emitter<State, void>&>) {
        expand(now, static_cast<const Runtime&>(rt), emit);
    } else {
        static_assert(std::is_invocable_v<Expand&, const StateView<State>&, Emitter<State, void>&>,
                      "expand must accept (const StateView&, const Runtime&, Emitter&) or (const StateView&, Emitter&). Runtime must be const.");
        expand(now, emit);
    }
}

// ============================================================
// 固定長Hash表。Selector内の重複排除に使う。
// clear は配列初期化ではなく epoch 更新で行う。
// 削除は行わず、置換で生じる古いentryは参照時に候補側と照合して無視する。
// 古いentryが増えたら空きが十分あるうちに rebuild する。
// ============================================================
class FixedHashMap {
public:
    struct Lookup {
        bool found = false;
        int slot = -1;
    };

    void init(int cap) {
        cap = std::max(4, cap);
        capacity_ = detail::ceil_pow2(cap);
        mask_ = capacity_ - 1; occupied_ = 0;
        keys_.assign(static_cast<std::size_t>(capacity_), Hash{});
        values_.assign(static_cast<std::size_t>(capacity_), -1);
        used_.assign(static_cast<std::size_t>(capacity_), 0);
        epoch_ = 1;
    }

    void clear() {
        occupied_ = 0;
        ++epoch_;
        if (epoch_ == 0) {
            std::fill(used_.begin(), used_.end(), 0);
            epoch_ = 1;
        }
    }

    Lookup lookup(Hash key) const {
        int i = bucket(key);
        for (int probe = 0; probe < capacity_; ++probe) {
            if (used_[i] != epoch_) return Lookup{false, occupied_ >= capacity_ - capacity_ / 4 ? -1 : i};
            if (keys_[i] == key) return Lookup{true, i};
            i = (i + 1) & mask_;
        }
        return Lookup{false, -1};
    }

    int get(int slot) const { return values_[slot]; }

    void set(int slot, Hash key, int value) {
        if (slot < 0) return;
        occupied_ += used_[slot] != epoch_;
        used_[slot] = epoch_;
        keys_[slot] = key;
        values_[slot] = value;
    }

private:
    int capacity_ = 0, occupied_ = 0;
    int mask_ = 0;
    std::uint32_t epoch_ = 1;
    std::vector<Hash> keys_;
    std::vector<int> values_;
    std::vector<std::uint32_t> used_;

    int bucket(Hash key) const { return static_cast<int>(detail::mix64(key)) & mask_; }
};

// ============================================================
// 状態コピー型 run 本体
// ============================================================

template<class State> class Engine {
    // 診断無効時は、通知用の集計・時計取得も行わない。
    static_assert(std::copy_constructible<State>, "State must be copy constructible because best_state is copied when updated");
    static_assert(std::assignable_from<State&, State>, "State must be assignable because selectors replace candidates in-place");

    friend class Emitter<State, void>;

    struct Item {
        // 選抜時に頻繁に読むメタ情報を先頭に置き、Stateが大きい場合のキャッシュ効率を少し良くする。
        Cost cost{};
        Hash hash{};
        bool has_hash = false;
        [[no_unique_address]] std::conditional_t<diagnostics_enabled, int, std::monostate> incoming_step{};
        State state;

        template<class U>
        Item(U&& state_, Cost cost_, Hash hash_, bool has_hash_)
            : cost(cost_),
              hash(hash_),
              has_hash(has_hash_),
              state(std::forward<U>(state_)) {}
    };

    // 各到着ターンのSelector。push時点で beam_width 個だけを保持する。
    // 幅制限の比較はcost-only。同costでは既存候補を優先し、hashではtie-breakしない。
    // hashは同一到着ターン内の重複排除とStateView用にだけ使う。
    // State生成前に cost/hash で受理可能性を判定するため、push_lazy と通常pushの両方で内部保存の無駄を抑えられる。
    class Selector {
        friend class Engine;

    public:
        void init(int beam_width, int hash_capacity, bool use_hash) {
            beam_width_ = beam_width;
            use_hash_ = use_hash;
            candidates_.reserve(beam_width_);
            costs_.assign(beam_width_, Cost{});
            if (use_hash_) hash_.init(hash_capacity);
            seg_n_ = detail::ceil_pow2(beam_width_);
            seg_.assign(seg_n_ * 2, -1);
        }

        void clear() {
            candidates_.clear();
            worst_index_cache = -1;
            if (use_hash_) hash_.clear();
        }

        [[nodiscard]] int size() const { return static_cast<int>(candidates_.size()); }
        [[nodiscard]] bool full() const { return size() >= beam_width_; }
        std::vector<Item>& selected() { return candidates_; }
        const std::vector<Item>& selected() const { return candidates_; }

        // Stateを生成する前に、幅枝刈りで確実に落ちるか判定する。
        // fullでない場合や、現在の最悪候補より良い可能性がある場合はfalseを返す。
        [[nodiscard]] bool rejects_by_width(Cost cost) const {
            return full() && !(cost < worst_cost_cache);
        }

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
        void append_candidate(Cost cost, Hash hash, bool has_hash, Maker&& maker) {
            candidates_.emplace_back(std::invoke(std::forward<Maker>(maker)), cost, hash, has_hash);
            const int j = static_cast<int>(candidates_.size()) - 1;
            costs_[j] = cost;
            if (static_cast<int>(candidates_.size()) == beam_width_) build_segment_tree();
        }

        template<class Maker>
        void replace_candidate(int j, Cost cost, Hash hash, bool has_hash, Maker&& maker) {
            Item& item = candidates_[j];
            item.cost = cost;
            item.hash = hash;
            item.has_hash = has_hash;
            // 直接代入できる戻り値はそのまま使い、明示変換が必要な場合だけStateを構築する。
            if constexpr (std::is_assignable_v<State&, std::invoke_result_t<Maker&&>>)
                item.state = std::invoke(std::forward<Maker>(maker));
            else item.state = State(std::invoke(std::forward<Maker>(maker)));
            costs_[j] = cost;
            if (full()) seg_set(j);
        }

        bool is_live_hash_entry(int j, Hash hash) const {
            return 0 <= j && j < static_cast<int>(candidates_.size()) &&
                   candidates_[j].has_hash && candidates_[j].hash == hash;
        }

        void rebuild_hash() {
            hash_.clear();
            for (int i = 0; i < static_cast<int>(candidates_.size()); ++i) {
                if (!candidates_[i].has_hash) continue;
                auto slot = hash_.lookup(candidates_[i].hash);
                if (slot.slot >= 0) hash_.set(slot.slot, candidates_[i].hash, i);
            }
        }

        void build_segment_tree() {
            // 未使用の葉は初期値-1のまま。内部節点は全て再計算する。
            for (int i = 0; i < beam_width_; ++i) seg_[seg_n_ + i] = i;
            for (int i = seg_n_ - 1; i >= 1; --i) seg_[i] = worse_index(seg_[i << 1], seg_[i << 1 | 1]);
            worst_index_cache = seg_[1];
            worst_cost_cache = costs_[worst_index_cache];
        }

        void seg_set(int pos) {
            int i = seg_n_ + pos;
            while (i >>= 1) seg_[i] = worse_index(seg_[i << 1], seg_[i << 1 | 1]);
            worst_index_cache = seg_[1];
            worst_cost_cache = costs_[worst_index_cache];
        }
    };

public:
    explicit Engine(Param param) {
        rt.param = normalize(param);
        if constexpr (diagnostics_enabled) rt.hash_table_capacity = rt.param.use_hash_dedup ?
            detail::ceil_pow2(std::max(4, rt.param.hash_capacity)) : 0;
    }

    template<class Expand, class DebugHook>
    Result<State> run(State&& initial_state, Cost initial_cost, Hash initial_hash, bool initial_has_hash,
                      Expand&& expand, DebugHook&& debug_hook, bool defer_finish = false) {
        start_clock_us = detail::now_microseconds();
        limit_us = detail::time_limit_to_microseconds(rt.param.time_limit_ms);
        use_time_limit = (limit_us > 0);

        const bool valid_initial = detail::is_valid_cost(initial_cost);
        if (valid_initial && rt.param.max_turn > 0) {
            allocate_buffers();
            frontier.emplace_back(std::move(initial_state), initial_cost, initial_hash, initial_has_hash);
        }
        rt.best_cost = initial_cost;

        update_time();
        if (use_time_limit && rt.elapsed_us >= limit_us) rt.time_limit_reached = true;
        if (valid_initial && rt.param.max_turn == 0 && rt.param.max_turn_is_answer) {
            best_state.emplace(std::move(initial_state));
            set_best(initial_cost, 0);
        }
        rt.completed_max_turn = valid_initial && rt.param.max_turn == 0;

        if constexpr (diagnostics_enabled) if (rt.found) rt.first_solution_us = rt.last_best_update_us = 0;
        begin_frontier(valid_initial && rt.param.max_turn > 0 ? 1 : 0, rt.param.beam_width);
        rt.stop_reason = !valid_initial ? StopReason::InvalidInitial :
            rt.param.max_turn == 0 ? StopReason::Completed : StopReason::None;
        emit_hook(EventType::RunStart, debug_hook);
        if (valid_initial && !rt.completed_max_turn) {
            if (use_time_limit) run_loop<true>(expand, debug_hook);
            else run_loop<false>(expand, debug_hook);
        }

        turn_timing_active = false;
        rt.stop_reason = !valid_initial ? StopReason::InvalidInitial :
            stop_on_timeout() ? StopReason::TimeLimit :
            rt.completed_max_turn ? StopReason::Completed : StopReason::Exhausted;
        emit_hook(EventType::SearchEnd, debug_hook);

        Result<State> res = build_result();
        if (!defer_finish) finish(res, debug_hook);
        print_result_warnings(res);
        return res;
    }

private:
    using ActiveRuntime = std::conditional_t<diagnostics_enabled, DebugRuntime, Runtime>;
    ActiveRuntime rt;
    int pending_candidates = 0;

    std::int64_t start_clock_us = 0;
    std::int64_t limit_us = 0;
    bool use_time_limit = false;
    bool turn_timing_active = false;

    std::vector<Item> frontier;
    std::vector<Selector> selectors;
    std::optional<State> best_state;

    int ring_size = 2;

    // ユーザー指定値を内部で扱いやすい範囲へ正規化する。
    // hash_capacity はSelector保持数が高々beam_width個であることを前提に丸める。
    static Param normalize(Param param) {
        param.max_turn = std::max(0, param.max_turn);
        param.beam_width = std::max(1, param.beam_width);
        param.max_step = std::min(std::max(1, param.max_step), std::max(1, param.max_turn));
        param.time_check_interval = std::max(1, param.time_check_interval);
        if (param.max_turn == std::numeric_limits<int>::max() ||
            param.beam_width > ((1 << 30) - 17) / 4)
            throw std::length_error("beam parameters exceed internal index limits");

        if (param.use_hash_dedup) {
            // Selectorが保持する候補は高々beam_width個なので、過大なhash表はキャッシュ効率のため丸める。
            if (param.hash_capacity <= 0) param.hash_capacity = param.beam_width * 4 + 17;
            const int lower = param.beam_width * 2 + 1;
            const int upper = param.beam_width * 4 + 17;
            param.hash_capacity = std::min(std::max(param.hash_capacity, lower), upper);
        } else {
            param.hash_capacity = 0;
        }
        return param;
    }

    // 現在frontierは1本。未来の到着候補だけをmax_step+1のSelectorリングで管理する。
    void allocate_buffers() {
        ring_size = rt.param.max_step + 1;
        frontier.reserve(rt.param.beam_width);
        selectors.resize(ring_size);
        for (auto& s : selectors) s.init(rt.param.beam_width, rt.param.hash_capacity, rt.param.use_hash_dedup);
    }

    void update_time() {
        rt.elapsed_us = detail::now_microseconds() - start_clock_us;
        if (turn_timing_active) {
            rt.turn_elapsed_us = rt.elapsed_us - rt.turn_start_us;
            rt.turn_time_limit_reached |= rt.elapsed_us >= rt.turn_deadline_us;
        }
    }

    template<class DebugHook>
    void emit_hook(EventType event_type, DebugHook& debug_hook) {
        if constexpr (diagnostics_enabled) {
            update_time(); snapshot();
            debug_hook(event_type, static_cast<const DebugRuntime&>(rt));
        }
    }

    // 呼出し直前に全体の時計を更新する。turn中は締切を延長しない。
    void begin_turn_time() {
        if (!rt.param.use_turn_time_limit || !use_time_limit) return;
        turn_timing_active = true;
        rt.turn_start_us = rt.elapsed_us;
        rt.turn_elapsed_us = 0;
        const auto remaining = std::max<std::int64_t>(0, limit_us - rt.elapsed_us);
        rt.turn_deadline_us = rt.elapsed_us + remaining / (rt.param.max_turn - rt.turn);
        rt.turn_time_limit_reached = rt.elapsed_us >= rt.turn_deadline_us;
    }

    bool stop_on_timeout() const {
        return rt.time_limit_reached && (!rt.param.continue_on_timeout || rt.found);
    }

    bool mark_time_limit_if_elapsed() {
        if (!use_time_limit) return false;
        update_time();
        if (rt.elapsed_us < limit_us) return false;
        rt.time_limit_reached = true;
        return true;
    }

    // soft limit判定。expand完了後に間引いて呼び、到達後は縮退探索へ移る。
    // expandの途中はライブラリ側では中断しない。
    bool need_switch_to_minimal_by_time() {
        if (!use_time_limit) return false;
        if ((rt.expanded_nodes % rt.param.time_check_interval) != 0) return false;
        return mark_time_limit_if_elapsed();
    }

    template<class Maker>
    bool push_candidate(int turn, int step, Cost cost, Hash hash, bool has_hash, bool is_answer, Maker&& maker) {
        static_assert(std::invocable<Maker&&>, "maker must be callable with no arguments");
        static_assert(std::constructible_from<State, std::invoke_result_t<Maker&&>>, "maker result must be constructible as State");
        if constexpr (diagnostics_enabled) ++rt.generated_candidates;
        if (!detail::is_valid_cost(cost) || step <= 0 || step > rt.param.max_step || step > rt.param.max_turn - turn) {
            if constexpr (diagnostics_enabled) ++rt.invalid_candidates;
            return false;
        }

        if constexpr (diagnostics_enabled) {
            rt.generated_step_sum += static_cast<unsigned>(step);
            rt.generated_step_max = std::max(rt.generated_step_max, step);
        }
        if (turn + step == rt.param.max_turn) { rt.completed_max_turn = true; }
        if (is_answer || (rt.param.max_turn_is_answer && turn + step == rt.param.max_turn)) {
            if constexpr (diagnostics_enabled) ++rt.answer_candidates;
            const bool answer_only = !rt.param.expand_answers && turn + step < rt.param.max_turn;

            if (!rt.found || cost < rt.best_cost) {
                best_state.emplace(std::invoke(std::forward<Maker>(maker)));
                set_best(cost, turn + step);
                if (turn + step == rt.param.max_turn || answer_only) return false;
                return select_candidate(turn, step, cost, hash, has_hash, [&]() -> const State& { return *best_state; });
            }
            if (answer_only) return false;
        }
        if (turn + step == rt.param.max_turn) return false;
        return select_candidate(turn, step, cost, hash, has_hash, std::forward<Maker>(maker));
    }

    template<class Maker>
    bool select_candidate(int turn, int step, Cost cost, Hash hash, bool has_hash, Maker&& maker) {
        const int arrival = turn + step;
        Selector& selector = selectors[arrival % ring_size];
        if (selector.rejects_by_width(cost)) {
            if constexpr (diagnostics_enabled) ++rt.selection.rejected_by_width;

            return false;
        }

        auto accept_appended = [&]() {
            if constexpr (diagnostics_enabled) ++rt.selection.accepted;
            ++pending_candidates;
            peak();
        };
        auto accept_replaced_width = [&]() {
            if constexpr (diagnostics_enabled) { ++rt.selection.accepted; ++rt.selection.replaced_by_width; }
        };

        if (!(selector.use_hash_ && has_hash)) {
            if (!selector.full()) {
                selector.append_candidate(cost, hash, has_hash, std::forward<Maker>(maker));
                if constexpr (diagnostics_enabled) selector.candidates_.back().incoming_step = step;
                accept_appended();
                return true;
            }
            const int j = selector.worst_index_cache;
            selector.replace_candidate(j, cost, hash, has_hash, std::forward<Maker>(maker));
            if constexpr (diagnostics_enabled) selector.candidates_[j].incoming_step = step;
            accept_replaced_width();
            return true;
        }

        if constexpr (diagnostics_enabled) ++rt.selection.hash_checked;
        auto found = selector.hash_.lookup(hash);
        if (found.found) {
            const int j = selector.hash_.get(found.slot);
            if (selector.is_live_hash_entry(j, hash)) {
                if (!(cost < selector.costs_[j])) {
                    if constexpr (diagnostics_enabled) ++rt.selection.rejected_by_hash;
                    return false;
                }
                selector.replace_candidate(j, cost, hash, true, std::forward<Maker>(maker));
                if constexpr (diagnostics_enabled) selector.candidates_[j].incoming_step = step;
                if constexpr (diagnostics_enabled) { ++rt.selection.accepted; ++rt.selection.replaced_by_hash; }
                return true;
            }
            // stale entry なので、この slot を上書きしてよい。候補本体は既に別候補で置換されている。
        }

        if (found.slot < 0) {
            selector.rebuild_hash();

            found = selector.hash_.lookup(hash);
            if (found.slot < 0) {
                rt.diagnostic_flags |= 1u;
                if constexpr (diagnostics_enabled) ++rt.selection.rejected_by_capacity;
                return false;
            }
        }

        if (!selector.full()) {
            const int j = static_cast<int>(selector.candidates_.size());
            selector.append_candidate(cost, hash, true, std::forward<Maker>(maker));
            if constexpr (diagnostics_enabled) selector.candidates_.back().incoming_step = step;
            selector.hash_.set(found.slot, hash, j);
            accept_appended();
            return true;
        }

        const int j = selector.worst_index_cache;
        selector.replace_candidate(j, cost, hash, true, std::forward<Maker>(maker));
        if constexpr (diagnostics_enabled) selector.candidates_[j].incoming_step = step;
        selector.hash_.set(found.slot, hash, j);
        accept_replaced_width();
        return true;
    }

    template<class Expand>
    void expand_item_body(Item& item, int current_turn, Expand& expand) {
        StateView<State> now(current_turn, item.cost, item.state, item.hash, item.has_hash);
        Emitter<State, void> emit(*this, current_turn, item.hash);

        begin_expansion();
        if constexpr (diagnostics_enabled) rt.expanded_step_sum += item.incoming_step;
        call_expand(expand, now, rt, emit);
        ++rt.expanded_nodes;
        observe_best_update();
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

    // 時間制限の有無だけをコンパイル時に分け、探索手順は一つに保つ。
    template<bool CheckTime, class Expand, class DebugHook>
    void run_loop(Expand& expand, DebugHook& debug_hook) {
        bool minimal_mode = false;
        for (int turn = 0; turn < rt.param.max_turn; ++turn) {
            begin_turn(turn);
            if constexpr (CheckTime) {
                minimal_mode |= rt.time_limit_reached;
                if (stop_on_timeout()) break;
            }
            if constexpr (diagnostics_enabled) begin_frontier(frontier.size(), minimal_mode ? 1 : static_cast<std::size_t>(rt.param.beam_width));
            if constexpr (diagnostics_enabled) sample_layer_costs([&](const auto& add) {
                for (const auto& item : frontier) add(item.cost);
            });
            expand_frontier<CheckTime>(turn, minimal_mode, expand);
            if constexpr (CheckTime) if (stop_on_timeout()) { end_turn(debug_hook); break; }
            frontier.clear();
            if (minimal_mode) materialize_arrival<true>(turn + 1);
            else materialize_arrival<false>(turn + 1);
            end_turn(debug_hook);
            if (frontier.empty() && pending_candidates == 0) break;
        }
    }

    void begin_turn(int current_turn) {
        rt.turn = current_turn;

        if (use_time_limit) mark_time_limit_if_elapsed();
        begin_turn_time();
    }

    template<class DebugHook>
    void end_turn(DebugHook& debug_hook) {
        if (use_time_limit) mark_time_limit_if_elapsed();
        end_visit(debug_hook);
    }

    template<bool CheckTime, class Expand>
    void expand_frontier(int turn, bool& minimal_mode, Expand& expand) {
        int i = 0;
        if (!minimal_mode) {
            for (; i < static_cast<int>(frontier.size()); ++i) {
                if constexpr (CheckTime) {
                    if (rt.turn_time_limit_reached && (pending_candidates != 0 || rt.completed_max_turn)) break;
                }
                expand_item_body(frontier[i], turn, expand);
                if constexpr (CheckTime) {
                    if (need_switch_to_minimal_by_time()) { minimal_mode = true; ++i; break; }
                }
            }
        }
        if constexpr (CheckTime) {
            // 全体時間切れ後は、未展開部分から最良1本だけを追加展開する。
            if (minimal_mode && !stop_on_timeout()) {
                const int best = best_item_index(frontier, i);
                if (best >= 0) expand_item_body(frontier[best], turn, expand);
            }
        }
    }

    void set_best(Cost cost, int turn) {
        rt.found = true;
        rt.best_cost = cost;
        rt.best_turn = turn;

        if constexpr (diagnostics_enabled) { ++rt.best_update_count; rt.last_best_update_us = -1; }
    }

    template<bool Minimal>
    void materialize_arrival(int arrival_turn) {
        // 有効解の保存とは独立して、到着ターンの探索候補を実体化する。
        // Minimal=false: Selector内の全候補を次frontierへ移す。
        // Minimal=true : Selector内の最良cost 1件だけを移し、他は破棄する。
        Selector& selector = selectors[arrival_turn % ring_size];
        auto& selected = selector.selected();
        const int original_count = static_cast<int>(selected.size());
        const int best_idx = Minimal ? best_item_index(selected) : -1;

        if constexpr (Minimal) {
            if (best_idx >= 0) frontier.push_back(std::move(selected[best_idx]));
        } else {
            frontier.swap(selected);
        }

        pending_candidates -= original_count;
        selector.clear();

        if constexpr (diagnostics_enabled) rt.pending_frontier_ = frontier.size();
    }

    Result<State> build_result() {
        Result<State> res;
        res.found = rt.found;

        res.completed_max_turn = rt.completed_max_turn;
        res.hash_table_full = ((rt.diagnostic_flags & 1u) != 0);
        res.best_cost = rt.best_cost;
        res.best_turn = rt.best_turn;
        res.best_state = std::move(best_state);

        return res;
    }

    void print_result_warnings(const Result<State>& res) const {
        if (!rt.param.print_warnings) return;
        if (res.hash_table_full) {
            std::cerr << "[bs] warning: hash_table_full; hash_capacity="
                      << rt.param.hash_capacity
                      << ", beam_width=" << rt.param.beam_width << '\n';
        }
    }

    private:
    void peak() {
        if constexpr (diagnostics_enabled) {
            rt.pending_candidates = static_cast<std::size_t>(pending_candidates) + rt.pending_frontier_;
            rt.peak_pending_candidates = std::max(rt.peak_pending_candidates, rt.pending_candidates);
        }
    }
    void snapshot() {
        if constexpr (diagnostics_enabled) {
            peak();
            rt.pending_turns = rt.pending_frontier_ > 0;
            rt.full_pending_turns = rt.pending_frontier_ >= static_cast<std::size_t>(rt.param.beam_width);
            for (const auto& q : selectors) if (q.size() > 0) {
                ++rt.pending_turns;
                rt.full_pending_turns += q.size() >= rt.param.beam_width;
            }
        }
    }

    private:
        // 選別後・展開前のcostを2回走査。基準値との差で平均・母分散を安定に求める。
        template<class Visit> void sample_layer_costs(Visit visit) {
            if constexpr (diagnostics_enabled) {
                rt.layer_size = 0;
                rt.layer_cost_mean = rt.layer_cost_stddev = 0;
                long double origin = 0, sum = 0;
                bool finite = true;
                visit([&](Cost cost) {
                    if (rt.layer_size++ == 0) rt.layer_cost_min = rt.layer_cost_max = cost;
                    else {
                        rt.layer_cost_min = std::min(rt.layer_cost_min, cost);
                        rt.layer_cost_max = std::max(rt.layer_cost_max, cost);
                    }
                    const long double value = static_cast<long double>(cost);
                    if (!std::isfinite(value)) { finite = false; return; }
                    if (rt.layer_size == 1) origin = value;
                    sum += value - origin;
                });
                if (rt.layer_size == 0) return;
                const long double mean = sum / static_cast<long double>(rt.layer_size);
                if (!finite || !std::isfinite(mean)) {
                    rt.layer_cost_mean = rt.layer_cost_stddev = std::numeric_limits<long double>::quiet_NaN();
                    return;
                }
                long double variance = 0;
                visit([&](Cost cost) {
                    const long double diff = (static_cast<long double>(cost) - origin) - mean;
                    variance += diff * diff;
                });
                rt.layer_cost_mean = origin + mean;
                rt.layer_cost_stddev = std::sqrt(variance / static_cast<long double>(rt.layer_size));
            }
        }
        void begin_frontier(std::size_t size, std::size_t limit) {
            if constexpr (diagnostics_enabled) {
                rt.frontier_size = std::min(size, limit);
                rt.frontier_limit = limit;
                rt.frontier_expanded = 0;
                rt.pending_frontier_ = size;
            }
        }
        void begin_expansion() {
            if constexpr (diagnostics_enabled) {
                ++rt.frontier_expanded;
                --rt.pending_frontier_;
                rt.expanded_after_timeout += rt.time_limit_reached;
            }
        }
        void observe_best_update() {
            if constexpr (diagnostics_enabled) {
                if (rt.found && rt.last_best_update_us < 0) {
                    if (rt.first_solution_us < 0) {
                        update_time();
                        rt.first_solution_us = rt.last_best_update_us = rt.elapsed_us;
                    } else rt.last_best_update_us = detail::now_microseconds() - start_clock_us;
                }
            }
        }
    public:
        template<class ResultType, class Hook>
        void finish(ResultType& result, Hook& hook) {
            update_time();
            if (limit_us > 0 && rt.elapsed_us >= limit_us) rt.time_limit_reached = true;
            if constexpr (diagnostics_enabled) { snapshot(); hook(EventType::RunEnd, static_cast<const DebugRuntime&>(rt)); }
            result.runtime = rt; result.time_limit_reached = rt.time_limit_reached;
        }

    private:
        template<class Hook> void end_visit(Hook& hook) {
            turn_timing_active = false;
            if constexpr (diagnostics_enabled) {
                if (rt.frontier_expanded < rt.frontier_size && (rt.time_limit_reached || rt.turn_time_limit_reached)) ++rt.turns_cut_by_time;
                emit_hook(EventType::TurnEnd, hook);
            }
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
        Engine<Stored> engine(param);
        auto raw = engine.run(Stored{std::move(initial_state), {}}, initial_cost, hash, has_hash, bridge, hook, true);
        Result<State, Action> result;
        result.found = raw.found;

        result.completed_max_turn = raw.completed_max_turn;
        result.hash_table_full = raw.hash_table_full;
        result.best_cost = raw.best_cost;
        result.best_turn = raw.best_turn;

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
    // Hashあり初期状態版。
    // expand は (const StateView<State>&, Emitter<State, void>&)、または
    // (const StateView<State>&, const Runtime&, Emitter<State, void>&) を受け取れる。
    // Runtime は const 参照固定であり、expand から Runtime を変更することはできない。
    // 操作列が必要な場合はrun<State, Action>でAction型を指定する。
    template<class State, class Action = void, class H, class Expand, class DebugHook = NoOp>
    requires CompatibleHashArg<H, Hash>
    static Result<State, Action> run(
        const Param& param,
        State initial_state,
        Cost initial_cost,
        H initial_hash,
        Expand&& expand,
        DebugHook&& debug_hook = DebugHook{}
    ) {
        if constexpr (std::is_void_v<Action>) {
            Engine<State> engine(param);
            return engine.run(std::move(initial_state), initial_cost, static_cast<Hash>(initial_hash), true,
                              std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));
        } else {
            return run_history<State, Action>(param, std::move(initial_state), initial_cost, static_cast<Hash>(initial_hash), true, expand, debug_hook);
        }
    }

    // Hashなし初期状態版。
    // Hash付きpushを行うまでは has_hash=false として扱われ、重複排除されない。
    template<class State, class Action = void, class Expand, class DebugHook = NoOp>
    requires (!CompatibleHashArg<Expand, Hash>)
    static Result<State, Action> run(
        const Param& param,
        State initial_state,
        Cost initial_cost,
        Expand&& expand,
        DebugHook&& debug_hook = DebugHook{}
    ) {
        if constexpr (std::is_void_v<Action>) {
            Engine<State> engine(param);
            return engine.run(std::move(initial_state), initial_cost, Hash{}, false,
                              std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));
        } else {
            return run_history<State, Action>(param, std::move(initial_state), initial_cost, Hash{}, false, expand, debug_hook);
        }
    }
};
} // namespace bs


#if __INCLUDE_LEVEL__ == 0
#define RUNTIME_TEST_BEAM bs::Beam
#define RUNTIME_TEST_CLOCK bs::detail::TestClock
#define RUNTIME_TEST_DELTA 0
#define RUNTIME_TEST_MULTI 1
#define RUNTIME_TEST_SUB 0
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
        p.max_turn = 4; p.max_step = 2; p.beam_width = 3;
        p.time_limit_ms = 4; p.time_check_interval = 1; p.use_turn_time_limit = true;
        p.continue_on_timeout = false;
        bool observed = false;
        B::CsvStatHook csv("runtime_turn_cut.csv", 0);
        auto expand = [&](const auto& now, auto& emit) {
            if (now.turn == 0) {
                for (int i = 1; i <= 3; ++i) h.push(emit, i, i, static_cast<std::uint64_t>(i));
                Clock::value += 100;
            } else if (now.turn == 1) {
                emit.push(10, 0, std::uint64_t{10}, 2);
                Clock::value += 1500;
            } else {
                check(now.turn == 3, "empty arrival turn is skipped");
                h.push(emit, 20, -1, 20); Clock::value += 100;
            }
        };
        auto hook = [&](E event, const B::DebugRuntime& rt) {
            if (event == E::TurnEnd && rt.turn == 1) {
                observed = true;
                check(rt.frontier_size == 3 && rt.frontier_expanded == 1 && rt.turns_cut_by_time == 1, "turn cut progress");
                check(rt.pending_candidates == 1 && rt.pending_turns == 1 && rt.full_pending_turns == 0,
                      "discarded leaves are absent from pending counts");
            }
            if (event == E::TurnEnd && rt.turn == 2)
                check(rt.pending_candidates == 1 && rt.pending_turns == 1, "materialized future candidate counted once");
            csv(event, rt);
        };
        auto result = h.run(p, 0, expand, hook);
        check(observed && result.found && result.runtime.expanded_nodes == 3, "turn cut preserves future path");
        check(h.world == 0, "delta tree cleanup leaves initial state");
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
