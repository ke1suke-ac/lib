#pragma once
#include <bits/stdc++.h>

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

// Emitter::push の引数誤用を減らすため、hash/step/boolを概念で分ける。
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

template<class U, class Action>
concept ActionArg = std::same_as<std::remove_cvref_t<U>, Action>;

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
// Cost と Hash を固定したターン制ビームサーチ本体。
// using BS = bs::Beam<std::int64_t, std::uint64_t>; のように別名化して使う。
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
    template<class Action> class Engine;

public:
// 探索パラメータ。
// max_step=0 は 1 として扱う。複数ターン行動を使う問題では必ず明示すること。
// time_limit_ms は探索全体のsoft limit。expand実行中は中断しない。
// use_turn_time_limit=trueでは各turn開始時に残り時間/残りturn数を配分する。
// turn期限後は未来候補があれば進み、なければ候補ができるか尽きるまで展開する。
// turn期限だけでは保持幅を縮めず、次turnで通常展開へ戻る。time_check_interval=1なら毎展開確認。
// 全体制限後は、解保存済みまたはcontinue_on_timeout=falseなら終了する。
// 未発見の救済時は、未展開候補から最良1本だけを追加展開する。
// 以後は各turnで最良1本だけを進め、到着候補も最良1件だけをmaterializeする。
// Hash重複排除は既定で有効。hash_capacity=0 の場合は内部で自動設定される。
// 解候補はis_answer=true、またはmax_turn_is_answer=trueでmax_turnへ到達した候補。
// max_turn==INT_MAX または beam_width>268435451 はlength_error（内部添字の上限）。
struct Param {
    int max_turn = 0;               // 探索終了ターン。候補は 0..max_turn に到着できる。
    int max_step = 1;               // 1手で進める最大ターン。0以下なら1扱い。
    int beam_width = 1;             // 各到着ターンに残す候補数。

    int hash_capacity = 0;          // use_hash_dedup=true かつ 0なら自動設定。

    double time_limit_ms = 0.0;    // 0なら無制限。全体のsoft limit。到達後の救済はcontinue_on_timeoutで指定。
    int time_check_interval = 64;   // 何ノード展開ごとに時間を見るか。
    bool continue_on_timeout = true; // 時間切れで解未発見なら救済探索を続ける。
    bool expand_answers = true; // falseなら有効解を保存のみ行い、探索候補へ登録しない。
    bool max_turn_is_answer = true; // trueならmax_turn到達ノードも解候補にする。
    bool use_hash_dedup = true;     // trueならHash付きpushで同一ターン重複排除を行う。
    bool print_warnings = true;     // 危険状態を検出したら終了時にstderrへ1回だけ警告する。
    bool use_turn_time_limit = false; // 残り時間を残りturnへ配分。全体制限が有効な場合だけ使用。
    bool apply_best_on_return = false; // 正常返却時、解があれば最良経路を再生して外部Stateを進める
};

enum class StopReason { None, Completed, Exhausted, TimeLimit, SweepLimit, InvalidInitial };

enum class EventType { RunStart, TurnEnd, SearchEnd, RunEnd };

// expand に渡す現在ノード情報。
struct NodeView {
    int turn = 0;
    Cost cost{};
    Hash hash{};
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
        std::uint64_t forward_calls = 0, backward_calls = 0;
        std::size_t peak_node_capacity = 0;
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
                ratio((a.rejected_by_width - b.rejected_by_width) + (a.replaced_by_width - b.replaced_by_width), attempts, 100);
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
                // 幅・Hashによる選別
                "width_discard_pct", "hash_discard_pct", "selection_attempts",
                // 残候補・方式別の負荷
                "pending_candidates", "peak_pending_candidates", "pending_turns", "full_pending_turn_pct",
                "generated_step_avg", "expanded_step_avg", "moves_per_expand", "peak_node_capacity",
                // 終了・時間切れ・異常
                "stop_reason", "completed_max_turn", "time_limit_reached", "overrun_ms",
                "finalize_ms", "turn_budget_used_pct", "turn_budget_ms", "turn_time_limit_reached",
                "turns_cut_by_time", "expanded_after_timeout", "diagnostic_flags",
                // 設定
                "beam_width", "max_turn", "max_step", "time_limit_ms",
                "use_turn_time_limit", "time_check_interval", "continue_on_timeout", "use_hash_dedup",
                "hash_table_capacity", "max_turn_is_answer", "expand_answers", "apply_best_on_return",
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
                ratio((s.forward_calls - p.forward_calls) + (s.backward_calls - p.backward_calls), expanded, 1);
                cell(s.peak_node_capacity);
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
                cell(r.param.apply_best_on_return);
                cell("beam");
                cell("delta");
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


// 木の1辺。結果復元のため、開始ターンと消費ターンをActionと別に保持する。
template<class Action>
struct Edge {
    Action action{};
    int turn = 0;
    int step = 1;
    [[nodiscard]] int next_turn() const noexcept { return turn + step; }
};

// 探索結果。
template<class Action>
struct Result {
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

    std::vector<Edge<Action>> path;
    Runtime runtime;
};

// expand に渡す候補追加器。
// 診断設定は外側のBeam型で決まり、Emitterの型引数には含めない。
// push の戻り値 true は、この呼び出し時点で候補がSelectorに入ったことを表す。
// 後続候補に置換される可能性があるため、最終結果に残る保証ではない。
// 有効解は幅・Hashの選別前に保存する。falseを返しても最良解を更新する場合がある。
// is_answer=true は解候補フラグ。expand_answers=falseなら保存だけを行う。
template<class Action> class Emitter {
public:
    // 引数順は action, cost, [hash,] step, [is_answer] とする。step は int 固定。
    template<class U, StepArg S>
    requires ActionArg<U, Action>
    bool push(U&& action, Cost cost, S step) {
        return push_impl(std::forward<U>(action), cost, now_hash_, step, false, false);
    }

    template<class U, StepArg S, BoolArg B>
    requires ActionArg<U, Action>
    bool push(U&& action, Cost cost, S step, B is_answer) {
        return push_impl(std::forward<U>(action), cost, now_hash_, step, false, is_answer);
    }

    template<class U, class H, StepArg S>
    requires ActionArg<U, Action> && CompatibleHashArg<H, Hash>
    bool push(U&& action, Cost cost, H hash, S step) {
        return push_impl(std::forward<U>(action), cost, static_cast<Hash>(hash), step, true, false);
    }

    template<class U, class H, StepArg S, BoolArg B>
    requires ActionArg<U, Action> && CompatibleHashArg<H, Hash>
    bool push(U&& action, Cost cost, H hash, S step, B is_answer) {
        return push_impl(std::forward<U>(action), cost, static_cast<Hash>(hash), step, true, is_answer);
    }

private:
    friend class Engine<Action>;

    Emitter(Engine<Action>& engine, int turn, int parent, Hash now_hash) noexcept
        : engine_(engine), turn_(turn), parent_(parent), now_hash_(now_hash) {}

    template<class U>
    requires ActionArg<U, Action>
    bool push_impl(U&& action, Cost cost, Hash hash, int step, bool has_hash, bool is_answer);

    Engine<Action>& engine_;
    int turn_ = 0;
    int parent_ = -1;
    Hash now_hash_{};
};

private:

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

    template<class Fn, class Action>
    static void call_move(Fn& fn, const Edge<Action>& edge) {
        if constexpr (std::is_invocable_v<Fn&, const Edge<Action>&>) {
            fn(edge);
        } else if constexpr (std::is_invocable_v<Fn&, Edge<Action>>) {
            fn(Edge<Action>(edge));
        } else if constexpr (std::is_invocable_v<Fn&, const Action&>) {
            fn(edge.action);
        } else if constexpr (std::is_invocable_v<Fn&, Action>) {
            fn(Action(edge.action));
        } else {
            static_assert(
                std::is_invocable_v<Fn&, const Edge<Action>&> ||
                std::is_invocable_v<Fn&, Edge<Action>> ||
                std::is_invocable_v<Fn&, const Action&> ||
                std::is_invocable_v<Fn&, Action>,
                "move callback must accept const Edge<Action>&, Edge<Action>, const Action&, or Action"
            );
        }
    }

    template<class Expand, class Action>
static void call_expand(Expand& expand, const NodeView& now, Runtime& rt, Emitter<Action>& emit) {
        // expand は Runtime なし、または const Runtime& ありの2系統を許可する。
        // Runtime はライブラリ管理情報なので、ユーザー側から変更できないよう const 参照だけを渡す。
        if constexpr (std::is_invocable_v<Expand&, const NodeView&, const Runtime&, Emitter<Action>&>) {
            expand(now, static_cast<const Runtime&>(rt), emit);
        } else {
            static_assert(std::is_invocable_v<Expand&, const NodeView&, Emitter<Action>&>,
                          "expand must accept (const NodeView&, const Runtime&, Emitter&) or (const NodeView&, Emitter&). Runtime must be const.");
            expand(now, emit);
        }
    }

template<class Action> class Engine {
    // 診断無効時は、通知用の集計・時計取得も行わない。

    friend class Emitter<Action>;

    // 内部候補表現。ユーザーは Emitter::push で候補を追加する。
    struct Candidate {
        Edge<Action> edge{};
        Cost cost{};
        Hash hash{};
        int parent = -1;
        bool has_hash = false;
    };

    static_assert(std::default_initializable<Action>,
                  "Action must be default initializable because the root node stores a dummy action");
    static_assert(std::copy_constructible<Action>,
                  "Action must be copy constructible because candidates and paths store actions");
    static_assert(std::assignable_from<Action&, Action>,
                  "Action must be assignable because selectors replace candidates in-place");

    // 1回の run で使うユーザーcallback群。
    // Engine自体はAction型だけに依存させ、関数ポインタなしのEmitterを維持する。
    template<class Expand, class MoveForward, class MoveBackward>
    struct RunContext {
        Expand& expand;
        MoveForward& move_forward;
        MoveBackward& move_backward;
    };

    struct Node {
        Edge<Action> edge{};
        Cost cost{};
        Hash hash{};
        int turn = 0;
        int parent = -1;
        int child = -1;
        int left = -1;
        int right = -1;
        int future_children = 0;
        int generation = 0;
        bool alive = false;
        bool active = true;
        bool in_remove_queue = false;
    };

    // 遅延削除entryはノードIDとは別プールで持つ。
    // ノードIDを即再利用しても、entry側のgeneration照合で古い予約を安全に無視できる。
    struct RemoveEntry {
        int node = -1;
        int generation = 0;
        int next = -1;
    };

    // 各到着ターンのSelector。push時点でbeam_width個だけを保持する。
    class Selector {
    public:
        void init(int beam_width, int hash_capacity, bool use_hash, std::vector<Node>* nodes) {
            beam_width_ = beam_width;
            use_hash_ = use_hash;
            nodes_ = nodes;
            candidates_.reserve(beam_width_);
            costs_.resize(beam_width_);
            if (use_hash_) hash_.init(hash_capacity);
            seg_n_ = 1;
            while (seg_n_ < beam_width_) seg_n_ <<= 1;
            seg_.assign(seg_n_ * 2, -1);
            full_ = false;
        }

        void clear() {
            candidates_.clear();
            if (use_hash_) hash_.clear();
            full_ = false;
        }

        [[nodiscard]] bool empty() const { return candidates_.empty(); }
        [[nodiscard]] int size() const { return static_cast<int>(candidates_.size()); }
        [[nodiscard]] bool rejects_by_width(Cost cost) const { return full_ && !(cost < worst_cost_); }
        std::vector<Candidate>& selected() { return candidates_; }
        const std::vector<Candidate>& selected() const { return candidates_; }

        struct Choice {
            int slot = -1, hash_slot = -1;
            bool duplicate = false;
        };

        // Actionを作らずに選別する。chooseからputまでは候補を変更しない。
        Choice choose(Cost cost, Hash hash, bool has_hash) {
            int hash_slot = -1;
            if (use_hash_ && has_hash) {
                auto hit = hash_.lookup(hash);
                if (hit.found) {
                    const int j = hash_.get(hit.slot);
                    if (0 <= j && j < size() && candidates_[j].has_hash && candidates_[j].hash == hash) {
                        if (!(cost < costs_[j]))
                            return {-1, -1, true};
                        return {j, -1, true};
                    }
                } else if (hit.slot < 0) {
                    rebuild_hash();
                    hit = hash_.lookup(hash);
                    if (hit.slot < 0) return {};
                }
                hash_slot = hit.slot;
            }
            const int j = full_ ? seg_[1] : size();
            return {j, hash_slot, false};
        }

        void put(const Choice& choice, Candidate&& cand) {
            const Hash hash = cand.hash;
            costs_[choice.slot] = cand.cost;
            const bool appended = choice.slot == size();
            if (!appended) change_future_child(candidates_[choice.slot].parent, -1);
            change_future_child(cand.parent, +1);
            if (appended) candidates_.push_back(std::move(cand));
            else {
                candidates_[choice.slot] = std::move(cand);
            }
            full_ = size() == beam_width_;
            if (full_) {
                if (appended) {
                    build_tree();
                } else {
                    for (int i = (seg_n_ + choice.slot) / 2; i > 0; i /= 2)
                        seg_[i] = worse_of(seg_[i * 2], seg_[i * 2 + 1]);
                }
                worst_cost_ = costs_[seg_[1]];
            }
            if (choice.hash_slot >= 0) hash_.set(choice.hash_slot, hash, choice.slot);
        }

    private:
        int beam_width_ = 1;
        int seg_n_ = 1;
        bool full_ = false;
        bool use_hash_ = false;
        std::vector<Node>* nodes_ = nullptr;
        std::vector<Candidate> candidates_;
        std::vector<Cost> costs_; // 比較用のキーを連続配置し、Actionを含む候補配列の読み出しを避ける。
        FixedHashMap hash_;
        std::vector<int> seg_;
        Cost worst_cost_{}; // full_のとき有効。棄却判定では候補配列を参照しない。

        int worse_of(int a, int b) const {
            if (a < 0) return b;
            if (b < 0) return a;
            if (costs_[a] != costs_[b]) return costs_[a] > costs_[b] ? a : b;
            return a > b ? a : b;
        }

        void change_future_child(int node_id, int delta) {
            if (node_id < 0) return;
            (*nodes_)[node_id].future_children += delta;
            assert((*nodes_)[node_id].future_children >= 0);
        }

        // 満杯になる瞬間だけの構築ループを分離し、頻繁な置換処理を小さく保つ。
        [[gnu::noinline]] void build_tree() {
            for (int i = 0; i < beam_width_; ++i) seg_[seg_n_ + i] = i;
            for (int i = seg_n_ - 1; i > 0; --i)
                seg_[i] = worse_of(seg_[i * 2], seg_[i * 2 + 1]);
        }
        void rebuild_hash() {
            if (!use_hash_) return;
            hash_.clear();
            for (int i = 0; i < static_cast<int>(candidates_.size()); ++i) {
                if (!candidates_[i].has_hash) continue;
                auto slot = hash_.lookup(candidates_[i].hash);
                if (slot.slot >= 0) hash_.set(slot.slot, candidates_[i].hash, i);
            }
        }
    };

public:
    Engine(Param param_, Cost initial_cost_, Hash initial_hash_)
        : rt{}, initial_cost(initial_cost_), initial_hash(initial_hash_) { rt.param = normalize(param_);
        if constexpr (diagnostics_enabled) rt.hash_table_capacity = rt.param.use_hash_dedup ?
            detail::ceil_pow2(std::max(4, rt.param.hash_capacity)) : 0;
    }

    template<class Expand, class MoveForward, class MoveBackward, class DebugHook>
    Result<Action> run(Expand&& expand, MoveForward&& move_forward,
                       MoveBackward&& move_backward, DebugHook&& debug_hook) {
        using Ctx = RunContext<std::remove_reference_t<Expand>, std::remove_reference_t<MoveForward>,
                               std::remove_reference_t<MoveBackward>>;
        Ctx ctx{expand, move_forward, move_backward};

        start_clock_us = detail::now_microseconds();
        limit_us = detail::time_limit_to_microseconds(rt.param.time_limit_ms);
        use_time_limit = (limit_us > 0);

        const bool valid_initial = detail::is_valid_cost(initial_cost);
        if (valid_initial && rt.param.max_turn > 0) allocate_buffers();
        rt.best_cost = initial_cost;
        rt.best_turn = 0;

        update_time();
        if (use_time_limit && rt.elapsed_us >= limit_us) rt.time_limit_reached = true;
        if (valid_initial && rt.param.max_turn == 0 && rt.param.max_turn_is_answer) {
            set_best(initial_cost, 0);
        }

        bool minimal_mode = rt.time_limit_reached;
        rt.completed_max_turn = valid_initial && rt.param.max_turn == 0;

        if constexpr (diagnostics_enabled) if (rt.found) rt.first_solution_us = rt.last_best_update_us = 0;
        begin_frontier(valid_initial && rt.param.max_turn > 0 ? 1 : 0, rt.param.beam_width);
        rt.stop_reason = !valid_initial ? StopReason::InvalidInitial :
            rt.param.max_turn == 0 ? StopReason::Completed : StopReason::None;
        emit_hook(EventType::RunStart, debug_hook);
        for (int current_turn = 0; valid_initial && current_turn < rt.param.max_turn; ++current_turn) {
            rt.turn = current_turn;

            if (mark_time_limit_if_elapsed()) minimal_mode = true;
            begin_turn_time();
            if constexpr (diagnostics_enabled) begin_frontier(rt.pending_frontier_, minimal_mode ? 1 : static_cast<std::size_t>(rt.param.beam_width));
            if constexpr (diagnostics_enabled) {
                collect_active_leaves(current_turn);
                sample_layer_costs([&](const auto& add) {
                    for (int id : tmp_ids) add(nodes[id].cost);
                });
            }
            if (stop_on_timeout()) break;

            // 通常時はDFSで全幅を展開する。時間切れ後は各ターンの最良葉だけを展開する。
            if (!minimal_mode) {
                if (rt.param.use_turn_time_limit && use_time_limit) run_dfs<true>(current_turn, minimal_mode, ctx);
                else run_dfs<false>(current_turn, minimal_mode, ctx);
            }
            if (stop_on_timeout()) { end_visit(debug_hook); break; }
            if (minimal_mode) run_minimal_dfs(current_turn, ctx);
            if (stop_on_timeout()) { end_visit(debug_hook); break; }

            // 次ターンに到着した候補を木に接続する。時間切れ後は最良1候補だけを残す。
            materialize_arrival(current_turn + 1, minimal_mode);

            if (mark_time_limit_if_elapsed()) minimal_mode = true;
            end_visit(debug_hook);

            if (rt.time_limit_reached) minimal_mode = true;

            // 未来候補もactive葉もない場合は、max_turnまで空回ししない。
            if (pending_candidates == 0 && !nodes[root].active) break;
        }

        turn_timing_active = false;
        rt.stop_reason = !valid_initial ? StopReason::InvalidInitial :
            stop_on_timeout() ? StopReason::TimeLimit :
            rt.completed_max_turn ? StopReason::Completed : StopReason::Exhausted;
        emit_hook(EventType::SearchEnd, debug_hook);

        // 探索木の不要ノードを明示解放し、探索中に進めたroot prefix分を巻き戻す。
        cleanup_after_run();
        for (int i = static_cast<int>(committed_edges.size()) - 1; i >= 0; --i) {
            call_move(ctx.move_backward, committed_edges[i]); if constexpr (diagnostics_enabled) ++rt.backward_calls;
        }
        // 圧縮済みprefixも巻き戻した初期状態から、保存済み経路を一度だけ再生する。
        if (rt.param.apply_best_on_return && rt.found) {
            for (const auto& edge : best_path) { call_move(ctx.move_forward, edge); if constexpr (diagnostics_enabled) ++rt.forward_calls; }
        }
        Result<Action> res = build_result();
        finish(res, debug_hook);
        print_result_warnings(res);
        return res;
    }

private:
    using ActiveRuntime = std::conditional_t<diagnostics_enabled, DebugRuntime, Runtime>;
    ActiveRuntime rt;
    int pending_candidates = 0;
    Cost initial_cost{};
    Hash initial_hash{};

    std::int64_t start_clock_us = 0;
    std::int64_t limit_us = 0;
    bool use_time_limit = false;
    bool turn_timing_active = false;

    std::vector<Node> nodes;
    std::vector<int> free_nodes;
    std::vector<Selector> selectors;
    std::vector<int> remove_heads;
    std::vector<RemoveEntry> remove_entries;
    std::vector<int> remove_free;
    std::vector<Edge<Action>> committed_edges;
    std::vector<Edge<Action>> best_path;
    std::size_t best_prefix_size = 0; // 保存済み経路のうち、確定prefixと共通する長さ。
    std::vector<int> tmp_nodes;
    std::vector<int> tmp_ids;

    int ring_size = 2;
    int root = 0;
    int used_nodes = 0;
    int next_generation = 1;

    int current_expand_max_step = 1;

    static Param normalize(Param param) {
        param.max_turn = std::max(0, param.max_turn);
        param.beam_width = std::max(1, param.beam_width);
        param.max_step = std::min(std::max(1, param.max_step), std::max(1, param.max_turn));
        param.time_check_interval = std::max(1, param.time_check_interval);

        // int添字の比較木・Hash表・経路予約が表現できない指定は確保前に拒否する。
        if (param.max_turn == std::numeric_limits<int>::max() ||
            param.beam_width > ((1 << 30) - 17) / 4)
            throw std::length_error("beam parameters exceed internal index limits");

        if (param.use_hash_dedup) {
            // Hash重複排除を使う場合、hash_capacity=0 は自動設定とする。
            // Selectorが保持する候補は高々beam_width個なので、過大指定はキャッシュ効率のため丸める。
            if (param.hash_capacity <= 0) param.hash_capacity = param.beam_width * 4 + 17;
            const int lower = param.beam_width * 2 + 1;
            const int upper = param.beam_width * 4 + 17;
            param.hash_capacity = std::min(std::max(param.hash_capacity, lower), upper);
        } else {
            param.hash_capacity = 0;
        }
        return param;
    }

    void allocate_buffers() {
        // Engineはrunごとに生成するため、既定の空領域をそのまま予約する
        nodes.reserve(4096);
        free_nodes.reserve(4096);
        selectors.resize(rt.param.max_step + 1);
        ring_size = rt.param.max_step + 1;
        const bool use_hash = rt.param.use_hash_dedup;
        const int hash_cap = use_hash ? rt.param.hash_capacity : 0;
        for (Selector& s : selectors) s.init(rt.param.beam_width, hash_cap, use_hash, &nodes);

        remove_heads.assign(ring_size, -1);
        const long long remove_cap_ll = 1LL * rt.param.beam_width * (2LL + rt.param.max_step) + 8;
        const int remove_cap = static_cast<int>(std::min<long long>(std::max<long long>(remove_cap_ll, 8), std::numeric_limits<int>::max() / 4));
        remove_entries.assign(remove_cap, RemoveEntry{});
        remove_free.reserve(remove_cap);
        for (int i = remove_cap - 1; i >= 0; --i) remove_free.push_back(i);
        committed_edges.reserve(rt.param.max_turn + 1);
        best_path.reserve(rt.param.max_turn + 1);
        tmp_nodes.reserve(rt.param.max_turn + 1);
        tmp_ids.reserve(rt.param.beam_width);

        Node root_node;
        root_node.cost = initial_cost;
        root_node.hash = initial_hash;
        root = new_node(std::move(root_node));
        assert(root == 0);
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

    int new_node(Node&& node) {
        int id;
        if (!free_nodes.empty()) {
            id = free_nodes.back();
            free_nodes.pop_back();

            nodes[id] = std::move(node);
        } else {
            if (nodes.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max()))
                throw std::length_error("beam node index limit exceeded");
            id = static_cast<int>(nodes.size());
            nodes.push_back(std::move(node));

            used_nodes = static_cast<int>(nodes.size());
        }
        nodes[id].alive = true;
        nodes[id].generation = next_generation++;
        if (next_generation == std::numeric_limits<int>::max()) next_generation = 1;
        return id;
    }

    void delete_node(int id) {
        if (id < 0 || id == root || !nodes[id].alive) return;
        nodes[id] = Node{};
        free_nodes.push_back(id);
    }

    template<class U>
    requires ActionArg<U, Action>
    bool push_candidate(int turn, int parent, U&& action, int step, Cost cost, Hash hash, bool has_hash, bool is_answer) {
        if constexpr (diagnostics_enabled) ++rt.generated_candidates;
        if (!detail::is_valid_cost(cost) || step <= 0 || step > rt.param.max_step || step > rt.param.max_turn - turn) {
            if constexpr (diagnostics_enabled) ++rt.invalid_candidates;
            return false;
        }
        if constexpr (diagnostics_enabled) {
            rt.generated_step_sum += static_cast<unsigned>(step);
            rt.generated_step_max = std::max(rt.generated_step_max, step);
        }
        const int arrival = turn + step;
        if (arrival == rt.param.max_turn) { rt.completed_max_turn = true; }
        if (is_answer || (rt.param.max_turn_is_answer && arrival == rt.param.max_turn)) {
            if constexpr (diagnostics_enabled) ++rt.answer_candidates;
            if (!rt.found || cost < rt.best_cost) {
                save_best_path(parent, Edge<Action>{action, turn, step});
                set_best(cost, arrival);
            }
            if (!rt.param.expand_answers && arrival < rt.param.max_turn) {
                return false;
            }
        }
        if (arrival == rt.param.max_turn) return false;
        Selector& selector = selectors[arrival % ring_size];
        if (selector.rejects_by_width(cost)) {
            if constexpr (diagnostics_enabled) ++rt.selection.rejected_by_width;

            return false;
        }
        const int before = selector.size();
        if constexpr (diagnostics_enabled) rt.selection.hash_checked += rt.param.use_hash_dedup && has_hash;
        const auto choice = selector.choose(cost, hash, has_hash);

        // 幅による棄却はchooseの前に済んでいる。ここでの不採用はhashによるもの。
        if (choice.slot < 0) {
            if constexpr (diagnostics_enabled) {
                if (choice.duplicate) ++rt.selection.rejected_by_hash;
                else ++rt.selection.rejected_by_capacity;
            }
            if (!choice.duplicate) rt.diagnostic_flags |= 1u;
            return false;
        }
        if constexpr (diagnostics_enabled) {
            ++rt.selection.accepted;
            if (choice.duplicate) ++rt.selection.replaced_by_hash;
            else if (choice.slot < before) ++rt.selection.replaced_by_width;
        }
        selector.put(choice, Candidate{Edge<Action>{std::forward<U>(action), turn, step}, cost, hash, parent, has_hash});
        if (step > current_expand_max_step) current_expand_max_step = step;
        if (selector.size() > before) {
            ++pending_candidates;
            peak();
        }
        return true;
    }

    void remove_leaf(int start_v) {
        int v = start_v;
        while (true) {
            if (v < 0 || v >= used_nodes || !nodes[v].alive) return;
            if (v == root) return;
            if (nodes[v].future_children > 0 || nodes[v].in_remove_queue || nodes[v].child != -1) return;
            const int left = nodes[v].left;
            const int right = nodes[v].right;
            const int parent = nodes[v].parent;
            if (parent < 0) return;

            delete_node(v);
            if (left == -1) {
                nodes[parent].child = right;
                if (right != -1) nodes[right].left = -1;
                v = parent;
            } else {
                nodes[left].right = right;
                if (right != -1) nodes[right].left = left;
                return;
            }
        }
    }

    void enqueue_remove(int node_id, int turn) {
        if (remove_free.empty()) {
            rt.diagnostic_flags |= 2u;
            return;
        }
        const int entry_id = remove_free.back();
        remove_free.pop_back();
        const int slot = turn % ring_size;
        remove_entries[entry_id] = RemoveEntry{node_id, nodes[node_id].generation, remove_heads[slot]};
        remove_heads[slot] = entry_id;
        nodes[node_id].in_remove_queue = true;
    }

    void remove_useless_nodes(int turn) {
        int& head = remove_heads[turn % ring_size];
        int entry_id = head;
        head = -1;
        while (entry_id != -1) {
            const RemoveEntry entry = remove_entries[entry_id];
            const int next_entry = entry.next;
            remove_free.push_back(entry_id);

            const int v = entry.node;
            if (0 <= v && v < used_nodes && nodes[v].alive && nodes[v].generation == entry.generation && nodes[v].in_remove_queue) {
                nodes[v].in_remove_queue = false;
                if (v != root && nodes[v].child == -1 && nodes[v].future_children == 0) remove_leaf(v);
            }
            entry_id = next_entry;
        }
    }

    // active葉と未来候補のLCAがrootより下に進んだら、共通prefixを確定する。
    // 条件は「root直下の実体化済み子が1本、かつroot直下に未到着候補なし」。
    template<class Ctx>
    void update_root(Ctx& ctx) {
        int child = nodes[root].child;
        while (child != -1 && nodes[child].right == -1 && nodes[root].future_children == 0) {
            const int old_root = root;
            root = child;
            committed_edges.push_back(nodes[root].edge);
            call_move(ctx.move_forward, nodes[root].edge); if constexpr (diagnostics_enabled) ++rt.forward_calls;

            // old_root から root への辺は committed_edges に保存済みなので、木から切り離して解放できる。
            nodes[root].parent = -1;
            nodes[root].left = -1;
            nodes[root].right = -1;
            delete_node(old_root);

            child = nodes[root].child;
        }
    }

    template<class MoveForward>
    int move_to_leaf(int v, MoveForward& move_forward) {
        int child = nodes[v].child;
        while (child != -1) {
            while (child != -1 && !nodes[child].active) child = nodes[child].right;
            if (child == -1) break;
            nodes[v].active = false;
            v = child;
            call_move(move_forward, nodes[v].edge); if constexpr (diagnostics_enabled) ++rt.forward_calls;
            child = nodes[v].child;
        }
        nodes[v].active = false;
        return v;
    }

    template<class MoveForward, class MoveBackward>
    int move_to_ancestor(int v, MoveForward& move_forward, MoveBackward& move_backward) {
        while (v != root) {
            call_move(move_backward, nodes[v].edge); if constexpr (diagnostics_enabled) ++rt.backward_calls;
            int u = nodes[v].right;
            while (u != -1) {
                if (nodes[u].active) {
                    call_move(move_forward, nodes[u].edge); if constexpr (diagnostics_enabled) ++rt.forward_calls;
                    return u;
                }
                u = nodes[u].right;
            }
            v = nodes[v].parent;
        }
        return root;
    }

    template<class MoveBackward>
    void move_direct_to_root(int v, MoveBackward& move_backward) {
        while (v != root) {
            call_move(move_backward, nodes[v].edge); if constexpr (diagnostics_enabled) ++rt.backward_calls;
            v = nodes[v].parent;
        }
    }

    void add_leaf(Candidate& cand) {
        const int parent = cand.parent;
        Node node;
        node.edge = std::move(cand.edge);
        node.cost = cand.cost;
        node.hash = cand.hash;
        node.turn = node.edge.next_turn();
        node.parent = parent;
        node.right = nodes[parent].child;

        const int v = new_node(std::move(node));
        --nodes[parent].future_children;
        assert(nodes[parent].future_children >= 0);
        if (nodes[parent].child != -1) nodes[nodes[parent].child].left = v;
        nodes[parent].child = v;

        int u = parent;
        while (!nodes[u].active) {
            nodes[u].active = true;
            if (u == root) break;
            u = nodes[u].parent;
        }
    }

    template<class Ctx>
    void expand_leaf(int v, Ctx& ctx) {
        NodeView now{nodes[v].turn, nodes[v].cost, nodes[v].hash};
        current_expand_max_step = 1;
        Emitter<Action> emit(*this, now.turn, v, now.hash);

        begin_expansion();
        if constexpr (diagnostics_enabled) rt.expanded_step_sum += now.turn > 0 ? nodes[v].edge.step : 0;
        call_expand(ctx.expand, now, rt, emit);
        ++rt.expanded_nodes;
        observe_best_update();

        // 削除は、この葉から最も遅く届く候補が到着するまでは遅延する。
        // future_childrenが残っていればremove_leaf側で削除されない。
        const int remove_turn = std::min(rt.param.max_turn, nodes[v].turn + current_expand_max_step);
        enqueue_remove(v, remove_turn);
    }

    template<bool TurnLimit, class Ctx>
    void run_dfs(int current_turn, bool& switch_to_minimal, Ctx& ctx) {
        remove_useless_nodes(current_turn);
        update_root(ctx);
        int v = root;
        if (!nodes[v].active) return;

        if constexpr (TurnLimit) if (rt.turn_time_limit_reached && (pending_candidates != 0 || rt.completed_max_turn)) {
            prune_active_leaves_at_turn(current_turn, false);
            return;
        }
        while (!switch_to_minimal) {
            v = move_to_leaf(v, ctx.move_forward);
            if (nodes[v].turn != current_turn) {
                // 本来は現在ターンの葉だけがactiveになる。安全のためroot状態へ戻して終了する。
                move_direct_to_root(v, ctx.move_backward);
                break;
            }
            expand_leaf(v, ctx);
            if (need_switch_to_minimal_by_time()) {
                // move_to_leaf() で展開経路の active flag を落としている。
                // ここで通常DFSを打ち切る場合も、残り兄弟を縮退探索で見つけられるよう親方向のactiveを復元する。
                deactivate_leaf_path(v);
                move_direct_to_root(v, ctx.move_backward);
                switch_to_minimal = true;
                break;
            }

            if constexpr (TurnLimit) if (rt.turn_time_limit_reached && (pending_candidates != 0 || rt.completed_max_turn)) {
                // DFS途中で落とした祖先のactiveを戻し、未展開葉だけを外す。
                // 未来候補が参照する親・経路は保持する。
                deactivate_leaf_path(v);
                move_direct_to_root(v, ctx.move_backward);
                prune_active_leaves_at_turn(current_turn, false);
                break;
            }
            v = move_to_ancestor(v, ctx.move_forward, ctx.move_backward);
            if (v == root) break;
        }
    }

    void release_future_child(const Candidate& cand) {
        if (cand.parent >= 0 && cand.parent < used_nodes && nodes[cand.parent].alive) {
            --nodes[cand.parent].future_children;
            assert(nodes[cand.parent].future_children >= 0);
        }
    }

    static int best_candidate_index(const std::vector<Candidate>& cands) {
        int best = 0;
        for (int i = 1; i < static_cast<int>(cands.size()); ++i) {
            const Candidate& cand = cands[i];
            const Candidate& current = cands[best];
            if (cand.cost < current.cost) best = i;
        }
        return best;
    }

    void materialize_arrival(int arrival_turn, bool minimal) {
        Selector& selector = selectors[arrival_turn % ring_size];

        if constexpr (diagnostics_enabled) rt.pending_frontier_ = minimal ? !selector.empty() : selector.size();
        if (selector.empty()) return;

        auto& cands = selector.selected();
        const int best_idx = minimal ? best_candidate_index(cands) : -1;

        for (int i = 0; i < static_cast<int>(cands.size()); ++i) {
            if (minimal && i != best_idx) {
                release_future_child(cands[i]);
                continue;
            }
            add_leaf(cands[i]);
        }
        pending_candidates -= selector.size();
        selector.clear();
    }

    // 同じターンの未展開葉をDFS順に収集。最良候補の選択と打ち切りで共有する。
    void collect_active_leaves(int turn) {
        tmp_ids.clear();
        tmp_nodes.clear();
        tmp_nodes.push_back(root);
        while (!tmp_nodes.empty()) {
            const int v = tmp_nodes.back();
            tmp_nodes.pop_back();
            if (v < 0 || v >= used_nodes) continue;
            const Node& node = nodes[v];
            if (!node.alive || !node.active || node.turn > turn) continue;
            if (node.child == -1) {
                if (node.turn == turn) tmp_ids.push_back(v);
            } else {
                for (int c = node.child; c != -1; c = nodes[c].right) tmp_nodes.push_back(c);
            }
        }
    }

    void deactivate_leaf_path(int leaf) {
        nodes[leaf].active = false;
        int v = leaf;
        while (v != root) {
            const int parent = nodes[v].parent;
            bool any_active = false;
            for (int c = nodes[parent].child; c != -1; c = nodes[c].right) {
                if (nodes[c].active) { any_active = true; break; }
            }
            nodes[parent].active = any_active;
            v = parent;
        }
    }

    int prune_active_leaves_at_turn(int turn, bool keep_best) {
        // 一度収集した葉から残す候補を選び、兄弟走査を終えてから木を変更する。
        collect_active_leaves(turn);
        int keep = -1;
        if (keep_best) for (int leaf : tmp_ids)
            if (keep < 0 || nodes[leaf].cost < nodes[keep].cost) keep = leaf;
        for (int leaf : tmp_ids) {
            if (leaf == keep) continue;
            if (leaf < 0 || leaf >= used_nodes || !nodes[leaf].alive || !nodes[leaf].active) continue;
            deactivate_leaf_path(leaf);
            if (nodes[leaf].child == -1 && nodes[leaf].future_children == 0 && !nodes[leaf].in_remove_queue) remove_leaf(leaf);
        }
        return keep;
    }

    template<class Ctx>
    void run_minimal_dfs(int current_turn, Ctx& ctx) {
        remove_useless_nodes(current_turn);
        update_root(ctx);
        // 時間切れ後は最良の1本を残す。残した葉は破棄もroot圧縮も削除しない。
        const int v = prune_active_leaves_at_turn(current_turn, true);
        if (v < 0) return;
        update_root(ctx);

        tmp_nodes.clear();
        for (int u = v; u != root; u = nodes[u].parent) tmp_nodes.push_back(u);
        for (int i = static_cast<int>(tmp_nodes.size()) - 1; i >= 0; --i) {
            call_move(ctx.move_forward, nodes[tmp_nodes[i]].edge); if constexpr (diagnostics_enabled) ++rt.forward_calls;
        }
        expand_leaf(v, ctx);
        deactivate_leaf_path(v);
        move_direct_to_root(v, ctx.move_backward);
    }

    void set_best(Cost cost, int turn) {
        rt.found = true;
        rt.best_cost = cost;
        rt.best_turn = turn;

        if constexpr (diagnostics_enabled) { ++rt.best_update_count; rt.last_best_update_us = -1; }
    }

    void save_best_path(int parent, Edge<Action> tail) {
        best_path.erase(best_path.begin() + best_prefix_size, best_path.end());
        best_path.insert(best_path.end(), committed_edges.begin() + best_prefix_size, committed_edges.end());
        best_prefix_size = committed_edges.size();

        // 経路の一時領域はIDだけを保持し、Actionの二重コピーを避ける
        tmp_nodes.clear();
        for (int v = parent; v != -1 && nodes[v].parent != -1; v = nodes[v].parent) {
            tmp_nodes.push_back(v);
        }
        for (auto i = tmp_nodes.rbegin(); i != tmp_nodes.rend(); ++i) {
            best_path.push_back(nodes[*i].edge);
        }
        best_path.push_back(std::move(tail));
    }

    void cleanup_after_run() {
        // 木全体を破棄するため、ノード別の参照数・空きリスト更新は不要。
        for (auto& selector : selectors) selector.clear();
        nodes.clear();
        pending_candidates = 0;
        if constexpr (diagnostics_enabled) rt.pending_frontier_ = 0;
    }

    void print_result_warnings(const Result<Action>& res) const {
        if (!rt.param.print_warnings) return;
        if (res.hash_table_full) {
            std::cerr << "[bs] warning: hash_table_full; hash_capacity="
                 << rt.param.hash_capacity
                 << ", beam_width=" << rt.param.beam_width << '\n';
        }
        if ((res.runtime.diagnostic_flags & 2u) != 0) {
            std::cerr << "[bs] warning: remove_entry_overflow" << '\n';
        }
    }

    Result<Action> build_result() {
        Result<Action> res;
        res.found = rt.found;

        res.completed_max_turn = rt.completed_max_turn;
        res.hash_table_full = ((rt.diagnostic_flags & 1u) != 0);
        res.best_cost = rt.best_cost;
        res.best_turn = rt.best_turn;
        res.path = std::move(best_path);

        return res;
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
            rt.peak_node_capacity = nodes.capacity();
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

public:
    // 探索実行API。
    // Action型では外部状態を move_forward / move_backward で管理する。
    // 正常返却時は初期状態。apply_best_on_return=trueかつ解ありなら最良経路の到達状態。
    // turn型では Result::path は Edge<Action> の列として返る。
    // initial_hash はNodeView.hashと差分Hash計算の起点。省略時はHash{0}。
    template<
        class Action,
        class InitialHash,
        class Expand,
        class MoveForward,
        class MoveBackward,
        class DebugHook = NoOp>
    requires CompatibleHashArg<InitialHash, Hash>
    static Result<Action> run(
        const Param& param,
        Cost initial_cost,
        InitialHash initial_hash,
        Expand&& expand,
        MoveForward&& move_forward,
        MoveBackward&& move_backward,
        DebugHook&& debug_hook = DebugHook{}) {
        Engine<Action> engine(param, initial_cost, static_cast<Hash>(initial_hash));
        return engine.run(std::forward<Expand>(expand),
                          std::forward<MoveForward>(move_forward),
                          std::forward<MoveBackward>(move_backward),
                          std::forward<DebugHook>(debug_hook));
    }

    // 初期Hashの省略形。生成候補の重複排除はHash付きpushで指定する。
    template<class Action, class Expand, class MoveForward, class MoveBackward, class DebugHook = NoOp>
    requires (!CompatibleHashArg<Expand, Hash>)
    static Result<Action> run(const Param& param, Cost initial_cost, Expand&& expand,
                              MoveForward&& move_forward, MoveBackward&& move_backward,
                              DebugHook&& debug_hook = DebugHook{}) {
        return run<Action>(param, initial_cost, Hash{}, std::forward<Expand>(expand),
                           std::forward<MoveForward>(move_forward), std::forward<MoveBackward>(move_backward),
                           std::forward<DebugHook>(debug_hook));
    }
};

template<CostType Cost, HashType Hash, bool EnableDiagnostics>
template<class Action>
template<class U>
requires ActionArg<U, Action>
bool Beam<Cost, Hash, EnableDiagnostics>::Emitter<Action>::push_impl(U&& action, Cost cost, Hash hash, int step, bool has_hash, bool is_answer) {
    return engine_.push_candidate(turn_, parent_, std::forward<U>(action), step, cost, hash, has_hash, is_answer);
}
}  // namespace bs


#if __INCLUDE_LEVEL__ == 0
#define RUNTIME_TEST_BEAM bs::Beam
#define RUNTIME_TEST_CLOCK bs::detail::TestClock
#define RUNTIME_TEST_DELTA 1
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
                emit.push(Action{h.world, 10}, 0, std::uint64_t{10}, 2);
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
