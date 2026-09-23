// C++20 / 複数初期解を選別しながら探索する SA。全個体・全期間の最良解を返す。
#pragma once
#include <bits/stdc++.h>
#include "sa_func_v17.hpp"
using namespace std;

namespace sa {

// 個体の選別基準。評価値が小さい個体を残す。迷ったら既定の Combination。
enum class SaPopSelectionPolicy {
    Current,      // 現在コスト
    LifetimeBest, // その個体が過去に到達した最良コスト（過去の状態へは戻さない）
    Combination, // 過去最良 + selection_current_weight × max(0, 現在 - 過去最良)
};

// 個体群用の設定。実際の初期個体数は sa_pop に渡す vector の要素数で決まる。
struct SaPopParam {
    double selection_time_ratio = 0.75; // [0,1]。選別までの時間割合。残りを最後の 1 個体に使う
    int max_state_count = 256;     // 受け付ける個体数の上限。実装上の上限は設定によらず 256

    SaPopSelectionPolicy selection_policy = SaPopSelectionPolicy::Combination;
    double selection_current_weight = 0.25; // [0,1]、Combination 用。0 は過去最良、1 は現在コストに相当
    double auto_end_temp_scale = 0.1; // 有限・正。自動推定した終端温度に掛ける倍率。手動時は掛けない
    // 各内部 SA の最良コストから許す悪化幅 = この倍率×温度。SaParam の同名項目より優先。
    // 非負を指定。0 は悪化拒否、+inf は幅制限なし。幅内の悪化には通常の確率判定も行う。
    double acceptance_width_scale = 1.0;
};

// 内部 SA 用。ユーザーの Snapshot は全個体共通の最良解 1 つだけ保存する。
struct SaPopEmptyBest {};

// DebugHook(event, runtime) の通知。sa_pop からの呼び出しは LOCAL 時のみ。
enum class SaPopEventType {
    RunStart,        // 初期個体の評価後、温度推定前
    AutoSampleStart, // 自動推定の開始（auto_mode && samples>0 のとき）
    AutoSampleEnd,   // 自動推定の終了。温度・標本数・fallback が確定
    PhaseStart,      // 選別段階の開始
    StateRunStart,   // 個体 1 つの内部 SA 開始
    StateRunEnd,     // 個体 1 つの内部 SA 終了
    StateSelection, // 個体ごとの順位と生存判定が確定
    PhaseEnd,        // 選別段階の終了
    FinalRunStart,   // 残った 1 個体で仕上げ開始
    FinalRunEnd,     // 仕上げ終了
    RunEnd,          // 全体終了。final_best_cost が返却コスト
};

// 外側の DebugHook 用。各イベントに関係する項目だけが有効。*_us の単位は μs、添字は 0 始まり。
// propose に渡るのはこの型ではなく、各内部 SA の SaRuntime<Cost>。
template<Numeric Cost>
struct SaPopRuntime {
    int initial_state_count = 0; // 最初の個体数
    int phase = -1;              // 選別段階の番号。段階外は -1
    int phase_count = 0;         // 選別段階の総数（最後の仕上げを除く）

    int active_state_count = 0;      // この段階の個体数
    int next_active_state_count = 0; // 次の段階に残す個体数

    int state_order = -1; // phase 内の処理順
    int state_id = -1;    // 初期 work_states の添字（個体の識別子）

    int rank = -1;              // StateSelection で確定する順位（0 が最良）
    bool survived = false;     // StateSelection での生存判定
    bool final_polish = false; // 最後の 1 個体の仕上げ中

    uint64_t seed = 0;
    int64_t time_limit_us = 0; // 外側の全体時間予算（μs）
    double selection_time_ratio = 0.75;
    int64_t phase_time_limit_us = 0; // この選別段階の予算
    int64_t inner_time_limit_us = 0; // 今回の内部 SA の予算
    int64_t final_time_limit_us = 0; // 最終 SA の予算

    int64_t elapsed_us = 0; // 外側の開始からの経過時間（μs）
    int64_t phase_elapsed_us = 0; // この選別段階で使った時間
    int64_t inner_elapsed_us = 0; // 今回の内部 SA で使った時間

    bool auto_mode = false;
    bool auto_fallback = false;       // 推定失敗により param の手動温度を使用
    int auto_per_state_samples = 0;   // 各個体への予定提案数
    int auto_total_samples = 0;       // 実際に採取した合計提案数
    int auto_worse_count = 0;         // 推定に使えた有限の悪化標本数
    double auto_avg_worse_delta = 0.0; // 悪化標本の平均（有効標本なしなら 0）

    double global_start_temp = 0.0; // 全体の温度スケジュールの開始・終了値
    double global_end_temp = 0.0;

    double phase_weight = 0.0;       // 温度区間を分ける重み
    double total_phase_weight = 0.0; // 全選別段階の重み合計

    double temp_progress_begin = 0.0; // 温度スケジュール上の区間始点（実測進捗とは異なる）
    double temp_progress_end = 0.0;   // 温度スケジュール上の区間終点
    double inner_start_temp = 0.0; // 今回の内部 SA の開始・終了温度
    double inner_end_temp = 0.0;

    Cost current_cost_before{}; // 今回の内部 SA の前後の現在コスト
    Cost current_cost_after{};
    Cost run_best_cost{}; // 対象個体を今回走らせた内部 SA の最良コスト

    Cost lifetime_best_cost_before{}; // その個体が過去のフェーズも含め到達した最良コスト
    Cost lifetime_best_cost_after{};

    double selection_key = 0.0;        // この個体の選別評価値（小さいほど優先）
    double selection_cutoff_key = 0.0; // 最下位の生存個体の評価値。同値でも順位で選別する

    int selection_policy = static_cast<int>(SaPopSelectionPolicy::Combination);
    double selection_current_weight = 0.25;

    int final_state_id = -1; // 最終 SA を走らせた個体。返却 Snapshot の取得元とは限らない
    double final_start_temp = 0.0;
    double final_end_temp = 0.0;
    Cost final_best_cost{}; // 返却する最良コスト（全個体・全フェーズを通じた最良）

    SaRuntime<Cost> inner_runtime; // 内部 SA 終了時の統計。Iteration ごとの外側通知はない
};

namespace detail {

template<class>
inline constexpr bool sa_pop_always_false_v = false;

inline uint64_t sa_pop_mix_seed(uint64_t seed, uint64_t a, uint64_t b = 0, uint64_t c = 0) {
    uint64_t x = seed + 0x9E3779B97F4A7C15ULL;
    x ^= a + 0xBF58476D1CE4E5B9ULL + (x << 6) + (x >> 2);
    x ^= b + 0x94D049BB133111EBULL + (x << 6) + (x >> 2);
    x ^= c + 0xD1B54A32D192ED03ULL + (x << 6) + (x >> 2);
    uint64_t z = x;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

template<Numeric Cost, class GetCost, class WorkState>
inline Cost call_sa_pop_get_cost(GetCost& get_cost, WorkState& state) {
    if constexpr (is_invocable_r_v<Cost, GetCost, WorkState&>) {
        return get_cost(state);
    } else if constexpr (is_invocable_r_v<Cost, GetCost, const WorkState&>) {
        return get_cost(static_cast<const WorkState&>(state));
    } else {
        static_assert(sa_pop_always_false_v<GetCost>, "GetCost must be callable as Cost(WorkState&) or Cost(const WorkState&)");
    }
}

template<class Snapshot, class GetSnapshot, class WorkState>
inline Snapshot call_sa_pop_get_snapshot(GetSnapshot& get_snapshot, WorkState& state) {
    if constexpr (is_invocable_r_v<Snapshot, GetSnapshot, WorkState&>) {
        return get_snapshot(state);
    } else if constexpr (is_invocable_r_v<Snapshot, GetSnapshot, const WorkState&>) {
        return get_snapshot(static_cast<const WorkState&>(state));
    } else {
        static_assert(sa_pop_always_false_v<GetSnapshot>, "GetSnapshot must be callable as Snapshot(WorkState&) or Snapshot(const WorkState&)");
    }
}

template<class Finalize, class WorkState>
inline void call_sa_pop_finalize(Finalize& finalize, WorkState& state, bool accepted) {
    if constexpr (is_invocable_v<Finalize, WorkState&, bool>) {
        finalize(state, accepted);
    } else if constexpr (is_invocable_v<Finalize, bool>) {
        finalize(accepted);
    } else {
        static_assert(sa_pop_always_false_v<Finalize>, "Finalize must be callable as void(WorkState&, bool) or void(bool)");
    }
}

template<class DebugHook, Numeric Cost>
inline void call_sa_pop_debug_hook(DebugHook& debug_hook, SaPopEventType event_type, const SaPopRuntime<Cost>& runtime) {
#ifdef LOCAL
    debug_hook(event_type, runtime);
#else
    (void)debug_hook;
    (void)event_type;
    (void)runtime;
#endif
}

inline const char* sa_pop_stage_name(bool final_polish) {
    return final_polish ? "final" : "selection";
}

inline const char* sa_pop_selection_policy_name(SaPopSelectionPolicy policy) {
    switch (policy) {
        case SaPopSelectionPolicy::Current: return "current";
        case SaPopSelectionPolicy::LifetimeBest: return "lifetime_best";
        case SaPopSelectionPolicy::Combination: return "combination";
    }
    return "unknown";
}

template<Numeric Cost>
inline double sa_pop_selection_key(
    SaPopSelectionPolicy policy,
    double current_weight,
    Cost current_cost,
    Cost lifetime_best_cost) {
    const double cur = to_double(current_cost);
    const double life = to_double(lifetime_best_cost);
    if (policy == SaPopSelectionPolicy::Current) return cur;
    if (policy == SaPopSelectionPolicy::LifetimeBest) return life;
    return life + current_weight * max(0.0, cur - life);
}

} // namespace detail

// LOCAL 用の CSV 記録。RunEnd で 3 ファイルを上書き。非 LOCAL は同じ構築・呼び出しができる空実装。
template<Numeric Cost>
class SaPopCsvStatHook {
public:
#ifdef LOCAL
    // 保存先の接頭辞。既定では sa_pop_trace.csv / sa_pop_phase.csv / sa_pop_summary.csv。
    explicit SaPopCsvStatHook(string csv_filename_prefix = "sa_pop_") : prefix_(move(csv_filename_prefix)) {}

    // RunStart でリセット。個体/段階/全体を記録する。通常 O(1)、PhaseEnd は O(個体数)、RunEnd は O(記録数)。
    void operator()(SaPopEventType event_type, const SaPopRuntime<Cost>& runtime) {
        // 個体・フェーズ単位で集計し、探索終了時だけファイルへ書き出す
        switch (event_type) {
            case SaPopEventType::RunStart:
                on_run_start(runtime);
                break;
            case SaPopEventType::AutoSampleEnd:
                on_auto_sample_end(runtime);
                break;
            case SaPopEventType::PhaseStart:
                on_phase_start(runtime);
                break;
            case SaPopEventType::StateRunEnd:
                on_state_run_end(runtime);
                break;
            case SaPopEventType::StateSelection:
                on_state_selection(runtime);
                break;
            case SaPopEventType::PhaseEnd:
                on_phase_end(runtime);
                break;
            case SaPopEventType::FinalRunEnd:
                on_final_run_end(runtime);
                break;
            case SaPopEventType::RunEnd:
                on_run_end(runtime);
                break;
            default:
                break;
        }
    }

private:
    struct TraceRow {
        string stage;
        double elapsed_ms = 0.0;
        int phase = -1;
        int phase_count = 0;
        int active_state_count = 0;
        int next_active_state_count = 0;
        int state_order = -1;
        int state_id = -1;
        int rank = -1;
        int survived = 0;
        double inner_time_limit_ms = 0.0;
        double inner_elapsed_ms = 0.0;
        int inner_iterations = 0;
        double temp_progress_begin = 0.0;
        double temp_progress_end = 0.0;
        double inner_start_temp = 0.0;
        double inner_end_temp = 0.0;
        Cost current_cost_before{};
        Cost current_cost_after{};
        Cost run_best_cost{};
        Cost lifetime_best_cost_before{};
        Cost lifetime_best_cost_after{};
        double selection_key = 0.0;
        double selection_cutoff_key = 0.0;
        int accepted_count = 0;
        int worse_accepted_count = 0;
        int best_update_count = 0;
        int propose_rejected_count = 0;
    };

    struct PhaseRow {
        double elapsed_ms = 0.0;
        int phase = -1;
        int phase_count = 0;
        int active_state_count = 0;
        int next_active_state_count = 0;
        double phase_weight = 0.0;
        double total_phase_weight = 0.0;
        double temp_progress_begin = 0.0;
        double temp_progress_end = 0.0;
        double inner_start_temp = 0.0;
        double inner_end_temp = 0.0;
        double phase_time_limit_ms = 0.0;
        double phase_elapsed_ms = 0.0;
        Cost best_lifetime_cost{};
        Cost best_current_cost{};
        double selection_cutoff_key = 0.0;
        Cost survivor_best_lifetime_cost{};
        Cost survivor_worst_lifetime_cost{};
        int64_t total_iterations = 0;
        int64_t total_accepted_count = 0;
        int64_t total_worse_accepted_count = 0;
        int64_t total_best_update_count = 0;
        int64_t total_propose_rejected_count = 0;
        double accept_rate = 0.0;
        double worse_accept_rate = 0.0;
    };

    struct SummaryRow {
        double time_limit_ms = 0.0;
        double elapsed_ms = 0.0;
        double selection_time_ratio = 0.75;
        int selection_policy = static_cast<int>(SaPopSelectionPolicy::Combination);
        double selection_current_weight = 0.25;
        int initial_state_count = 0;
        int phase_count = 0;
        uint64_t seed = 0;
        int auto_mode = 0;
        int auto_per_state_samples = 0;
        int auto_total_samples = 0;
        int auto_worse_count = 0;
        double auto_avg_worse_delta = 0.0;
        int auto_fallback = 0;
        double global_start_temp = 0.0;
        double global_end_temp = 0.0;
        int final_state_id = -1;
        double final_start_temp = 0.0;
        double final_end_temp = 0.0;
        double final_time_limit_ms = 0.0;
        double final_elapsed_ms = 0.0;
        int final_iterations = 0;
        Cost final_best_cost{};
        int64_t total_iterations = 0;
        int64_t total_accepted_count = 0;
        int64_t total_propose_rejected_count = 0;
    };

    struct PhaseAgg {
        bool active = false;
        bool best_initialized = false;
        bool survivor_initialized = false;
        PhaseRow row;
    };

    string prefix_;
    vector<TraceRow> trace_rows_;
    vector<PhaseRow> phase_rows_;
    SummaryRow summary_;
    bool has_summary_ = false;

    vector<TraceRow> pending_trace_rows_;
    array<int, 256> pending_index_by_state_{};
    PhaseAgg phase_agg_;

    static double ms_from_us(int64_t us) {
        return detail::us_to_ms(us);
    }

    static TraceRow make_trace_row(const SaPopRuntime<Cost>& rt) {
        TraceRow row;
        row.stage = detail::sa_pop_stage_name(rt.final_polish);
        row.elapsed_ms = ms_from_us(rt.elapsed_us);
        row.phase = rt.phase;
        row.phase_count = rt.phase_count;
        row.active_state_count = rt.active_state_count;
        row.next_active_state_count = rt.next_active_state_count;
        row.state_order = rt.state_order;
        row.state_id = rt.state_id;
        row.rank = rt.rank;
        row.survived = rt.survived ? 1 : 0;
        row.inner_time_limit_ms = ms_from_us(rt.inner_time_limit_us);
        row.inner_elapsed_ms = ms_from_us(rt.inner_elapsed_us);
        row.inner_iterations = rt.inner_runtime.iteration;
        row.temp_progress_begin = rt.temp_progress_begin;
        row.temp_progress_end = rt.temp_progress_end;
        row.inner_start_temp = rt.inner_start_temp;
        row.inner_end_temp = rt.inner_end_temp;
        row.current_cost_before = rt.current_cost_before;
        row.current_cost_after = rt.current_cost_after;
        row.run_best_cost = rt.run_best_cost;
        row.lifetime_best_cost_before = rt.lifetime_best_cost_before;
        row.lifetime_best_cost_after = rt.lifetime_best_cost_after;
        row.selection_key = rt.selection_key;
        row.selection_cutoff_key = rt.selection_cutoff_key;
        row.accepted_count = rt.inner_runtime.accepted_count;
        row.worse_accepted_count = rt.inner_runtime.worse_accepted_count;
        row.best_update_count = rt.inner_runtime.best_update_count;
        row.propose_rejected_count = rt.inner_runtime.propose_rejected_count;
        return row;
    }

    void on_run_start(const SaPopRuntime<Cost>& rt) {
        trace_rows_.clear();
        phase_rows_.clear();
        pending_trace_rows_.clear();
        pending_index_by_state_.fill(-1);
        phase_agg_ = PhaseAgg{};

        summary_ = SummaryRow{};
        summary_.time_limit_ms = ms_from_us(rt.time_limit_us);
        summary_.selection_time_ratio = rt.selection_time_ratio;
        summary_.selection_policy = rt.selection_policy;
        summary_.selection_current_weight = rt.selection_current_weight;
        summary_.initial_state_count = rt.initial_state_count;
        summary_.phase_count = rt.phase_count;
        summary_.seed = rt.seed;
        summary_.auto_mode = rt.auto_mode ? 1 : 0;
        summary_.global_start_temp = rt.global_start_temp;
        summary_.global_end_temp = rt.global_end_temp;
        has_summary_ = true;
    }

    void on_auto_sample_end(const SaPopRuntime<Cost>& rt) {
        summary_.auto_per_state_samples = rt.auto_per_state_samples;
        summary_.auto_total_samples = rt.auto_total_samples;
        summary_.auto_worse_count = rt.auto_worse_count;
        summary_.auto_avg_worse_delta = rt.auto_avg_worse_delta;
        summary_.auto_fallback = rt.auto_fallback ? 1 : 0;
        summary_.global_start_temp = rt.global_start_temp;
        summary_.global_end_temp = rt.global_end_temp;
    }

    void on_phase_start(const SaPopRuntime<Cost>& rt) {
        pending_trace_rows_.clear();
        pending_index_by_state_.fill(-1);
        phase_agg_ = PhaseAgg{};
        phase_agg_.active = true;
        PhaseRow& row = phase_agg_.row;
        row.elapsed_ms = ms_from_us(rt.elapsed_us);
        row.phase = rt.phase;
        row.phase_count = rt.phase_count;
        row.active_state_count = rt.active_state_count;
        row.next_active_state_count = rt.next_active_state_count;
        row.phase_weight = rt.phase_weight;
        row.total_phase_weight = rt.total_phase_weight;
        row.temp_progress_begin = rt.temp_progress_begin;
        row.temp_progress_end = rt.temp_progress_end;
        row.inner_start_temp = rt.inner_start_temp;
        row.inner_end_temp = rt.inner_end_temp;
        row.phase_time_limit_ms = ms_from_us(rt.phase_time_limit_us);
    }

    void on_state_run_end(const SaPopRuntime<Cost>& rt) {
        TraceRow row = make_trace_row(rt);
        add_summary_counts(rt.inner_runtime);
        if (0 <= rt.state_id && rt.state_id < 256) {
            pending_index_by_state_[rt.state_id] = static_cast<int>(pending_trace_rows_.size());
        }
        pending_trace_rows_.push_back(row);

        if (!phase_agg_.active) return;
        PhaseRow& pr = phase_agg_.row;
        pr.total_iterations += rt.inner_runtime.iteration;
        pr.total_accepted_count += rt.inner_runtime.accepted_count;
        pr.total_worse_accepted_count += rt.inner_runtime.worse_accepted_count;
        pr.total_best_update_count += rt.inner_runtime.best_update_count;
        pr.total_propose_rejected_count += rt.inner_runtime.propose_rejected_count;

        if (!phase_agg_.best_initialized) {
            pr.best_lifetime_cost = rt.lifetime_best_cost_after;
            pr.best_current_cost = rt.current_cost_after;
            phase_agg_.best_initialized = true;
        } else {
            pr.best_lifetime_cost = min(pr.best_lifetime_cost, rt.lifetime_best_cost_after);
            pr.best_current_cost = min(pr.best_current_cost, rt.current_cost_after);
        }
    }

    void on_state_selection(const SaPopRuntime<Cost>& rt) {
        if (0 <= rt.state_id && rt.state_id < 256) {
            const int idx = pending_index_by_state_[rt.state_id];
            if (0 <= idx && idx < static_cast<int>(pending_trace_rows_.size())) {
                TraceRow& row = pending_trace_rows_[idx];
                row.rank = rt.rank;
                row.survived = rt.survived ? 1 : 0;
                row.selection_key = rt.selection_key;
                row.selection_cutoff_key = rt.selection_cutoff_key;
            }
        }

        if (!phase_agg_.active) return;
        PhaseRow& pr = phase_agg_.row;
        pr.selection_cutoff_key = rt.selection_cutoff_key;
        if (rt.survived) {
            if (!phase_agg_.survivor_initialized) {
                pr.survivor_best_lifetime_cost = rt.lifetime_best_cost_after;
                pr.survivor_worst_lifetime_cost = rt.lifetime_best_cost_after;
                phase_agg_.survivor_initialized = true;
            } else {
                pr.survivor_best_lifetime_cost = min(pr.survivor_best_lifetime_cost, rt.lifetime_best_cost_after);
                pr.survivor_worst_lifetime_cost = max(pr.survivor_worst_lifetime_cost, rt.lifetime_best_cost_after);
            }
        }
    }

    void on_phase_end(const SaPopRuntime<Cost>& rt) {
        for (const TraceRow& row : pending_trace_rows_) trace_rows_.push_back(row);
        pending_trace_rows_.clear();
        pending_index_by_state_.fill(-1);

        if (!phase_agg_.active) return;
        PhaseRow row = phase_agg_.row;
        row.elapsed_ms = ms_from_us(rt.elapsed_us);
        row.phase_elapsed_ms = ms_from_us(rt.phase_elapsed_us);
        // どちらの受理率も分母は全反復数（悪化提案数ではない）
        if (row.total_iterations > 0) {
            row.accept_rate = static_cast<double>(row.total_accepted_count) / static_cast<double>(row.total_iterations);
            row.worse_accept_rate = static_cast<double>(row.total_worse_accepted_count) / static_cast<double>(row.total_iterations);
        }
        phase_rows_.push_back(row);
        phase_agg_ = PhaseAgg{};
    }

    void on_final_run_end(const SaPopRuntime<Cost>& rt) {
        trace_rows_.push_back(make_trace_row(rt));
        add_summary_counts(rt.inner_runtime);
        summary_.final_state_id = rt.final_state_id;
        summary_.final_start_temp = rt.final_start_temp;
        summary_.final_end_temp = rt.final_end_temp;
        summary_.final_time_limit_ms = ms_from_us(rt.final_time_limit_us);
        summary_.final_elapsed_ms = ms_from_us(rt.inner_elapsed_us);
        summary_.final_iterations = rt.inner_runtime.iteration;
        summary_.final_best_cost = rt.final_best_cost;
    }

    void add_summary_counts(const SaRuntime<Cost>& rt) {
        summary_.total_iterations += rt.iteration;
        summary_.total_accepted_count += rt.accepted_count;
        summary_.total_propose_rejected_count += rt.propose_rejected_count;
    }

    static double valid_accept_rate(int64_t iterations, int64_t accepted, int64_t rejected) {
        return iterations > rejected ? static_cast<double>(accepted) / (iterations - rejected) : 0.0;
    }

    void on_run_end(const SaPopRuntime<Cost>& rt) {
        if (!has_summary_) on_run_start(rt);
        summary_.elapsed_ms = ms_from_us(rt.elapsed_us);
        if (rt.final_state_id >= 0) summary_.final_state_id = rt.final_state_id;
        summary_.final_best_cost = rt.final_best_cost;
        write_all();
    }

    void write_all() const {
        write_trace_csv(prefix_ + "trace.csv");
        write_phase_csv(prefix_ + "phase.csv");
        write_summary_csv(prefix_ + "summary.csv");
    }

    void write_trace_csv(const string& filename) const {
        ofstream ofs(filename);
        if (!ofs) {
            cerr << "[sa_pop] warning: failed to open csv file: " << filename << '\n';
            return;
        }
        ofs << "stage,elapsed_ms,phase,phase_count,active_state_count,next_active_state_count,state_order,state_id,rank,survived,";
        ofs << "inner_time_limit_ms,inner_elapsed_ms,inner_iterations,temp_progress_begin,temp_progress_end,inner_start_temp,inner_end_temp,";
        ofs << "current_cost_before,current_cost_after,run_best_cost,lifetime_best_cost_before,lifetime_best_cost_after,";
        ofs << "selection_key,selection_cutoff_key,accepted_count,worse_accepted_count,best_update_count,";
        ofs << "propose_rejected_count,sa_rejected_count,valid_accept_rate\n";
        ofs << fixed << setprecision(9);
        for (const TraceRow& r : trace_rows_) {
            ofs << r.stage << ',' << r.elapsed_ms << ',' << r.phase << ',' << r.phase_count << ','
                << r.active_state_count << ',' << r.next_active_state_count << ',' << r.state_order << ',' << r.state_id << ','
                << r.rank << ',' << r.survived << ',' << r.inner_time_limit_ms << ',' << r.inner_elapsed_ms << ','
                << r.inner_iterations << ',' << r.temp_progress_begin << ',' << r.temp_progress_end << ',' << r.inner_start_temp << ',' << r.inner_end_temp << ','
                << r.current_cost_before << ',' << r.current_cost_after << ',' << r.run_best_cost << ','
                << r.lifetime_best_cost_before << ',' << r.lifetime_best_cost_after << ','
                << r.selection_key << ',' << r.selection_cutoff_key << ','
                << r.accepted_count << ',' << r.worse_accepted_count << ',' << r.best_update_count << ','
                << r.propose_rejected_count << ',' << r.inner_iterations - r.accepted_count - r.propose_rejected_count << ','
                << valid_accept_rate(r.inner_iterations, r.accepted_count, r.propose_rejected_count) << '\n';
        }
    }

    void write_phase_csv(const string& filename) const {
        ofstream ofs(filename);
        if (!ofs) {
            cerr << "[sa_pop] warning: failed to open csv file: " << filename << '\n';
            return;
        }
        ofs << "elapsed_ms,phase,phase_count,active_state_count,next_active_state_count,phase_weight,total_phase_weight,";
        ofs << "temp_progress_begin,temp_progress_end,inner_start_temp,inner_end_temp,phase_time_limit_ms,phase_elapsed_ms,";
        ofs << "best_lifetime_cost,best_current_cost,selection_cutoff_key,survivor_best_lifetime_cost,survivor_worst_lifetime_cost,";
        ofs << "total_iterations,total_accepted_count,total_worse_accepted_count,total_best_update_count,accept_rate,worse_accept_rate,";
        ofs << "total_propose_rejected_count,total_sa_rejected_count,valid_accept_rate\n";
        ofs << fixed << setprecision(9);
        for (const PhaseRow& r : phase_rows_) {
            ofs << r.elapsed_ms << ',' << r.phase << ',' << r.phase_count << ',' << r.active_state_count << ',' << r.next_active_state_count << ','
                << r.phase_weight << ',' << r.total_phase_weight << ',' << r.temp_progress_begin << ',' << r.temp_progress_end << ','
                << r.inner_start_temp << ',' << r.inner_end_temp << ',' << r.phase_time_limit_ms << ',' << r.phase_elapsed_ms << ','
                << r.best_lifetime_cost << ',' << r.best_current_cost << ',' << r.selection_cutoff_key << ','
                << r.survivor_best_lifetime_cost << ',' << r.survivor_worst_lifetime_cost << ','
                << r.total_iterations << ',' << r.total_accepted_count << ',' << r.total_worse_accepted_count << ','
                << r.total_best_update_count << ',' << r.accept_rate << ',' << r.worse_accept_rate << ','
                << r.total_propose_rejected_count << ',' << r.total_iterations - r.total_accepted_count - r.total_propose_rejected_count << ','
                << valid_accept_rate(r.total_iterations, r.total_accepted_count, r.total_propose_rejected_count) << '\n';
        }
    }

    void write_summary_csv(const string& filename) const {
        ofstream ofs(filename);
        if (!ofs) {
            cerr << "[sa_pop] warning: failed to open csv file: " << filename << '\n';
            return;
        }
        ofs << "time_limit_ms,elapsed_ms,selection_time_ratio,selection_policy,selection_current_weight,initial_state_count,phase_count,seed,";
        ofs << "auto_mode,auto_per_state_samples,auto_total_samples,auto_worse_count,auto_avg_worse_delta,auto_fallback,";
        ofs << "global_start_temp,global_end_temp,final_state_id,final_start_temp,final_end_temp,";
        ofs << "final_time_limit_ms,final_elapsed_ms,final_iterations,final_best_cost,";
        ofs << "total_iterations,total_accepted_count,total_propose_rejected_count,total_sa_rejected_count,valid_accept_rate\n";
        ofs << fixed << setprecision(9);
        const SummaryRow& r = summary_;
        ofs << r.time_limit_ms << ',' << r.elapsed_ms << ',' << r.selection_time_ratio << ','
            << r.selection_policy << ',' << r.selection_current_weight << ',' << r.initial_state_count << ','
            << r.phase_count << ',' << r.seed << ',' << r.auto_mode << ',' << r.auto_per_state_samples << ','
            << r.auto_total_samples << ',' << r.auto_worse_count << ',' << r.auto_avg_worse_delta << ',' << r.auto_fallback << ','
            << r.global_start_temp << ',' << r.global_end_temp << ',' << r.final_state_id << ',' << r.final_start_temp << ','
            << r.final_end_temp << ',' << r.final_time_limit_ms << ',' << r.final_elapsed_ms << ','
            << r.final_iterations << ',' << r.final_best_cost << ','
            << r.total_iterations << ',' << r.total_accepted_count << ',' << r.total_propose_rejected_count << ','
            << r.total_iterations - r.total_accepted_count - r.total_propose_rejected_count << ','
            << valid_accept_rate(r.total_iterations, r.total_accepted_count, r.total_propose_rejected_count) << '\n';
    }
#else
    explicit SaPopCsvStatHook(string = "sa_pop_") {}
    void operator()(SaPopEventType, const SaPopRuntime<Cost>&) {}
#endif
};

// 複数個体 SA。work_states は 1〜min(256,max_state_count) 個。move で渡すと初期群のコピーを避けられる。
// time_limit_ms は有限の非負 ms。0 は初期最良だけを返す。初期個体の評価・終了出力等は計時外。
// get_snapshot(state) -> Snapshot: 全体最良の初期解と、全体最良の更新時だけ呼ぶ。
//   Snapshot は探索状態から独立した、コピー構築・代入が可能な値型（WorkState と同じ型でも可）。
// get_cost(state) -> Cost: 現在の絶対コスト。各個体の評価や内部 SA の開始・終了時などに呼ぶ。
// propose(state[, const SaRuntime<Cost>&]) -> Cost または optional<Cost>: 提案後 - 提案前。Runtime 付き優先。
//   nullopt は強制棄却。finalize(state, false) で巻き戻す。0 は同値の有効提案として受理する。
//   各内部 SA の propose_rejected_count に集計し、温度推定には使わない（事前採取は回数に含めない）。
// finalize(state, bool accepted): 受理なら確定、棄却なら提案前に戻す。finalize(bool) も可。
// 事前採取は各個体に ceil(samples/K) 回を予定し、必ず finalize(false)。合計数は切り上がる。
// Runtime 付き propose の progress() / best_cost は各内部 SA の値（全体進捗・個体の過去最良ではない）。
// 戻り値は pair<Cost,Snapshot>。消えた個体を含む全期間の最良解で、Snapshot は必ず存在する。
//   個体ごとの過去状態は保存・復元しない。Cost の加算と内部 SA の int カウンタは範囲内に収める。
// DebugHook(event, SaPopRuntime): LOCAL のみ。全コールバックは値渡し（必要なら std::ref を使う）。
// param.enable_temperature_report は内部 SA ごとの診断。全体の推奨温度をまとめる機能ではない。
// 計算量: O(S(P+F)+I(P+F)+(B+1)G+KC+K log K+EH)。K/S/I/B/E は個体/採取/反復/全体最良更新/イベント数。
// P/F/G/C/H は各コールバックの処理量。WorkState 群のコピー・構築時間は別途考慮する。
template<
    class Snapshot,
    Numeric Cost,
    class WorkState,
    class GetSnapshot,
    class GetCost,
    class Propose,
    class Finalize,
    class DebugHook = NoOp>
pair<Cost, Snapshot> sa_pop(
    const SaParam& param,
    double time_limit_ms,
    vector<WorkState> work_states,
    GetSnapshot get_snapshot,
    GetCost get_cost,
    Propose propose,
    Finalize finalize,
    const SaPopParam& pop_param = SaPopParam{},
    DebugHook debug_hook = DebugHook{}) {
    static_assert(copy_constructible<Snapshot>, "Snapshot must be copy constructible");
    if constexpr (integral<Cost>) static_assert(is_signed_v<Cost>, "Signed integral Cost is required because delta can be negative");

    // 個体数・時間・パラメータの前提を確認する
    const int K = static_cast<int>(work_states.size());
    assert(K > 0 && "sa_pop requires at least one state");
    assert(K <= 256 && "sa_pop supports at most 256 work_states");
    assert(K <= pop_param.max_state_count && "work_states.size() exceeds SaPopParam.max_state_count");
    assert(isfinite(time_limit_ms) && time_limit_ms >= 0.0 && time_limit_ms * 1000.0 < 0x1p63 && "time budget must fit int64_t microseconds");
    assert(isfinite(pop_param.selection_time_ratio) && 0.0 <= pop_param.selection_time_ratio && pop_param.selection_time_ratio <= 1.0 && "selection_time_ratio must be in [0, 1]");
    assert(isfinite(pop_param.selection_current_weight) && 0.0 <= pop_param.selection_current_weight && pop_param.selection_current_weight <= 1.0 && "selection_current_weight must be in [0, 1]");
    assert(isfinite(pop_param.auto_end_temp_scale) && pop_param.auto_end_temp_scale > 0.0 && "auto_end_temp_scale must be finite and positive");
    assert(pop_param.acceptance_width_scale >= 0.0 && "acceptance_width_scale must be nonnegative; +infinity is allowed");
    assert(param.samples >= 0 && "sa::SaParam.samples must be >= 0");
    assert(0.0 < param.start_accept_prob && param.start_accept_prob < 1.0);
    assert(0.0 < param.end_accept_prob && param.end_accept_prob < 1.0);
    assert(param.start_temp > 0.0 && param.end_temp > 0.0);

    // 状態を移動せず、識別子とコストだけで生存個体を管理する
    array<uint16_t, 256> active_ids{};
    array<uint16_t, 256> order{};
    array<Cost, 256> current_costs{};
    array<Cost, 256> lifetime_best_costs{};
    array<double, 256> selection_keys{};

    for (int i = 0; i < K; ++i) {
        active_ids[i] = static_cast<uint16_t>(i);
        current_costs[i] = detail::call_sa_pop_get_cost<Cost>(get_cost, work_states[i]);
        lifetime_best_costs[i] = current_costs[i];
        selection_keys[i] = detail::sa_pop_selection_key(pop_param.selection_policy, pop_param.selection_current_weight, current_costs[i], lifetime_best_costs[i]);
    }

    // 各段階で4分の3を残し、冷却区間を段階ごとに等分する
    int phase_count = 0;
    for (int a = K; a > 1; a = max(1, (3 * a) / 4)) ++phase_count;

    const double total_phase_weight = static_cast<double>(phase_count);

    const auto global_start_clock = chrono::steady_clock::now();
    const auto elapsed_us_now = [&]() -> int64_t {
        return detail::steady_elapsed_us(global_start_clock);
    };

    const int64_t total_us = static_cast<int64_t>(max(0.0, time_limit_ms * 1000.0));
    const int64_t selection_us = static_cast<int64_t>(static_cast<double>(total_us) * pop_param.selection_time_ratio);
    const auto selection_deadline = global_start_clock + chrono::microseconds(selection_us);

    double global_start_temp = param.start_temp;
    double global_end_temp = param.end_temp;

    // 外側の Runtime には全体予算を、内部実行の各 Runtime にはそれぞれの予算を持たせる
    SaPopRuntime<Cost> rt_base;
    rt_base.initial_state_count = K;
    rt_base.phase_count = phase_count;
    rt_base.seed = param.seed;
    rt_base.time_limit_us = total_us;
    rt_base.selection_time_ratio = pop_param.selection_time_ratio;
    rt_base.selection_policy = static_cast<int>(pop_param.selection_policy);
    rt_base.selection_current_weight = pop_param.selection_current_weight;
    rt_base.auto_mode = param.auto_mode;
    rt_base.global_start_temp = global_start_temp;
    rt_base.global_end_temp = global_end_temp;

    auto fill_common_runtime = [&](SaPopRuntime<Cost>& rt) {
        rt.initial_state_count = K;
        rt.phase_count = phase_count;
        rt.seed = param.seed;
        rt.time_limit_us = total_us;
        rt.selection_time_ratio = pop_param.selection_time_ratio;
        rt.selection_policy = static_cast<int>(pop_param.selection_policy);
        rt.selection_current_weight = pop_param.selection_current_weight;
        rt.auto_mode = param.auto_mode;
        rt.global_start_temp = global_start_temp;
        rt.global_end_temp = global_end_temp;
        rt.total_phase_weight = total_phase_weight;
        rt.elapsed_us = elapsed_us_now();
    };

    {
        SaPopRuntime<Cost> rt = rt_base;
        fill_common_runtime(rt);
        detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::RunStart, rt);
    }

    auto best_current_id = [&]() -> int {
        int best = 0;
        for (int i = 1; i < K; ++i) {
            if (current_costs[i] < current_costs[best]) best = i;
        }
        return best;
    };

    // 有効予算がなければ、探索せず最良の初期解だけを保存する
    if (total_us <= 0) {
        const int best_id = best_current_id();
        Snapshot snap = detail::call_sa_pop_get_snapshot<Snapshot>(get_snapshot, work_states[best_id]);
        SaPopRuntime<Cost> rt = rt_base;
        fill_common_runtime(rt);
        rt.final_state_id = best_id;
        rt.final_best_cost = current_costs[best_id];
        detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::RunEnd, rt);
        return {current_costs[best_id], move(snap)};
    }

    // 加熱・選別で失わないよう、初期最良の Snapshot を一つ保持する
    const int initial_best_id = best_current_id();
    Cost global_best_cost = current_costs[initial_best_id];
    Snapshot global_best_snapshot = detail::call_sa_pop_get_snapshot<Snapshot>(get_snapshot, work_states[initial_best_id]);
    auto save_best = [&](int id) {
        return [&, id](const SaRuntime<Cost>& inner_rt) -> SaPopEmptyBest {
            global_best_cost = inner_rt.best_cost;
            global_best_snapshot = detail::call_sa_pop_get_snapshot<Snapshot>(get_snapshot, work_states[id]);
            return {};
        };
    };

    // population 全体から一度だけ auto 温度を推定する。
    int auto_per_state_samples = 0;
    int auto_total_samples = 0;
    int auto_worse_count = 0;
    double auto_avg_worse_delta = 0.0;
    bool auto_fallback = false;

    if (param.auto_mode && param.samples > 0) {
        auto_per_state_samples = param.samples / K + (param.samples % K != 0);
        {
            SaPopRuntime<Cost> rt = rt_base;
            fill_common_runtime(rt);
            rt.auto_per_state_samples = auto_per_state_samples;
            detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::AutoSampleStart, rt);
        }

        double mean_worse_delta = 0.0;
        SaRuntime<Cost> sample_runtime;
        sample_runtime.iteration = 0;
        sample_runtime.temperature = param.start_temp;
        // 単独 SA と同様、サンプリング中は経過時間 0 と正の時間予算を渡す
        sample_runtime.time_limit_us = total_us;

        for (int i = 0; i < K; ++i) {
            sample_runtime.current_cost = current_costs[i];
            sample_runtime.best_cost = current_costs[i];
            for (int t = 0; t < auto_per_state_samples; ++t) {
                if ((auto_total_samples & 31) == 0 && elapsed_us_now() >= total_us) break;
                Cost delta{};
                const bool valid = detail::unpack_delta(detail::call_propose<Cost>(propose, sample_runtime, work_states[i]), delta);
                const double value = detail::to_double(delta);
                if (valid && value > 0.0 && isfinite(value)) {
                    mean_worse_delta += (value - mean_worse_delta) / static_cast<double>(++auto_worse_count);
                }
                detail::call_sa_pop_finalize(finalize, work_states[i], false);
                ++auto_total_samples;
            }
            if (elapsed_us_now() >= total_us) break;
        }

        if (auto_worse_count > 0) {
            auto_avg_worse_delta = mean_worse_delta;
            global_start_temp = detail::temperature_from_avg_delta(auto_avg_worse_delta, param.start_accept_prob);
            global_end_temp = detail::temperature_from_avg_delta(auto_avg_worse_delta, param.end_accept_prob) * pop_param.auto_end_temp_scale;
        }
        if (auto_worse_count == 0 || !isfinite(global_start_temp) || !isfinite(global_end_temp)
            || global_start_temp <= 0.0 || global_end_temp <= 0.0) {
            auto_fallback = true;
            global_start_temp = param.start_temp;
            global_end_temp = param.end_temp;
        }

        {
            SaPopRuntime<Cost> rt = rt_base;
            fill_common_runtime(rt);
            rt.auto_per_state_samples = auto_per_state_samples;
            rt.auto_total_samples = auto_total_samples;
            rt.auto_worse_count = auto_worse_count;
            rt.auto_avg_worse_delta = auto_avg_worse_delta;
            rt.auto_fallback = auto_fallback;
            rt.global_start_temp = global_start_temp;
            rt.global_end_temp = global_end_temp;
            detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::AutoSampleEnd, rt);
        }
    }

    const auto temperature_at = [&](double ratio) -> double {
        return detail::exp_temperature(global_start_temp, global_end_temp, ratio);
    };

    int active = K;
    double prefix_weight = 0.0;

    for (int phase = 0; phase < phase_count; ++phase) {
        const auto phase_start_clock = chrono::steady_clock::now();
        const int next_active = max(1, (3 * active) / 4);
        const double phase_weight = 1.0;
        const double temp_progress_begin = (total_phase_weight > 0.0 ? pop_param.selection_time_ratio * prefix_weight / total_phase_weight : 0.0);
        const double temp_progress_end = (total_phase_weight > 0.0 ? pop_param.selection_time_ratio * (prefix_weight + phase_weight) / total_phase_weight : pop_param.selection_time_ratio);
        prefix_weight += phase_weight;
        const double phase_start_temp = temperature_at(temp_progress_begin);
        const double phase_end_temp = temperature_at(temp_progress_end);

        const int remaining_phases = phase_count - phase;
        int64_t remaining_selection_us = chrono::duration_cast<chrono::microseconds>(selection_deadline - chrono::steady_clock::now()).count();
        remaining_selection_us = max<int64_t>(0, remaining_selection_us);
        const int64_t phase_budget_us = (remaining_phases > 0 ? remaining_selection_us / remaining_phases : 0);
        const auto phase_deadline = chrono::steady_clock::now() + chrono::microseconds(phase_budget_us);

        {
            SaPopRuntime<Cost> rt = rt_base;
            fill_common_runtime(rt);
            rt.phase = phase;
            rt.active_state_count = active;
            rt.next_active_state_count = next_active;
            rt.phase_weight = phase_weight;
            rt.temp_progress_begin = temp_progress_begin;
            rt.temp_progress_end = temp_progress_end;
            rt.inner_start_temp = phase_start_temp;
            rt.inner_end_temp = phase_end_temp;
            rt.phase_time_limit_us = phase_budget_us;
            detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::PhaseStart, rt);
        }

        // 残り時間を未実行の個体へ均等に配り、各個体を独立な短時間 SA で更新する
        for (int state_order = 0; state_order < active; ++state_order) {
            const int id = active_ids[state_order];
            const Cost current_before = current_costs[id];
            const Cost lifetime_before = lifetime_best_costs[id];

            int64_t rem_phase_us = chrono::duration_cast<chrono::microseconds>(phase_deadline - chrono::steady_clock::now()).count();
            rem_phase_us = max<int64_t>(0, rem_phase_us);
            const int remaining_states = active - state_order;
            const int64_t slice_us = (remaining_states > 0 ? rem_phase_us / remaining_states : 0);
            const double slice_ms = detail::us_to_ms(slice_us);

            {
                SaPopRuntime<Cost> rt = rt_base;
                fill_common_runtime(rt);
                rt.phase = phase;
                rt.active_state_count = active;
                rt.next_active_state_count = next_active;
                rt.state_order = state_order;
                rt.state_id = id;
                rt.phase_weight = phase_weight;
                rt.temp_progress_begin = temp_progress_begin;
                rt.temp_progress_end = temp_progress_end;
                rt.inner_start_temp = phase_start_temp;
                rt.inner_end_temp = phase_end_temp;
                rt.phase_time_limit_us = phase_budget_us;
                rt.inner_time_limit_us = slice_us;
                rt.current_cost_before = current_before;
                rt.lifetime_best_cost_before = lifetime_before;
                detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::StateRunStart, rt);
            }

            Cost run_best_cost = current_before;
            SaRuntime<Cost> inner_end_runtime;
            inner_end_runtime.current_cost = current_before;
            inner_end_runtime.best_cost = current_before;
            bool has_inner_runtime = false;

            if (slice_us > 0) {
                SaParam inner_param = param;
                inner_param.acceptance_width_scale = pop_param.acceptance_width_scale;
                inner_param.seed = detail::sa_pop_mix_seed(param.seed, static_cast<uint64_t>(phase), static_cast<uint64_t>(id), 0x51E1EC710FULL);
                inner_param.auto_mode = false;
                inner_param.samples = 0;
                inner_param.start_temp = phase_start_temp;
                inner_param.end_temp = phase_end_temp;
                inner_param.enable_end_cost_check = false; // sa_pop 内部の短時間SAでは LOCAL 時の終了整合性チェックを抑制する

                auto inner_hook = [&](SaEventType event_type, const SaRuntime<Cost>& inner_rt) {
#ifdef LOCAL
                    if (event_type == SaEventType::RunEnd) {
                        inner_end_runtime = inner_rt;
                        has_inner_runtime = true;
                    }
#else
                    (void)event_type;
                    (void)inner_rt;
#endif
                };

                auto [best_cost, ignored_best] = sa<SaPopEmptyBest, Cost>(
                    inner_param,
                    slice_ms,
                    save_best(id),
                    [&]() -> Cost { return detail::call_sa_pop_get_cost<Cost>(get_cost, work_states[id]); },
                    [&](const SaRuntime<Cost>& inner_rt) { return detail::call_propose<Cost>(propose, inner_rt, work_states[id]); },
                    [&](bool accepted) { detail::call_sa_pop_finalize(finalize, work_states[id], accepted); },
                    global_best_cost,
                    inner_hook);
                (void)ignored_best;
                run_best_cost = best_cost;
                if (!has_inner_runtime) {
                    inner_end_runtime.best_cost = best_cost;
                    inner_end_runtime.current_cost = detail::call_sa_pop_get_cost<Cost>(get_cost, work_states[id]);
                }
            }

            current_costs[id] = detail::call_sa_pop_get_cost<Cost>(get_cost, work_states[id]);
            lifetime_best_costs[id] = min(lifetime_best_costs[id], run_best_cost);
            selection_keys[id] = detail::sa_pop_selection_key(pop_param.selection_policy, pop_param.selection_current_weight, current_costs[id], lifetime_best_costs[id]);

            {
                SaPopRuntime<Cost> rt = rt_base;
                fill_common_runtime(rt);
                rt.phase = phase;
                rt.active_state_count = active;
                rt.next_active_state_count = next_active;
                rt.state_order = state_order;
                rt.state_id = id;
                rt.phase_weight = phase_weight;
                rt.temp_progress_begin = temp_progress_begin;
                rt.temp_progress_end = temp_progress_end;
                rt.inner_start_temp = phase_start_temp;
                rt.inner_end_temp = phase_end_temp;
                rt.phase_time_limit_us = phase_budget_us;
                rt.inner_time_limit_us = slice_us;
                rt.inner_elapsed_us = inner_end_runtime.elapsed_us;
                rt.current_cost_before = current_before;
                rt.current_cost_after = current_costs[id];
                rt.run_best_cost = run_best_cost;
                rt.lifetime_best_cost_before = lifetime_before;
                rt.lifetime_best_cost_after = lifetime_best_costs[id];
                rt.selection_key = selection_keys[id];
                rt.inner_runtime = inner_end_runtime;
                detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::StateRunEnd, rt);
            }
        }

        // 選抜キー・最良コスト・現在コスト・識別子の順で決定的に順位を付ける
        for (int j = 0; j < active; ++j) order[j] = active_ids[j];
        sort(order.begin(), order.begin() + active, [&](uint16_t x, uint16_t y) {
            const int a = static_cast<int>(x);
            const int b = static_cast<int>(y);
            if (selection_keys[a] != selection_keys[b]) return selection_keys[a] < selection_keys[b];
            if (lifetime_best_costs[a] != lifetime_best_costs[b]) return lifetime_best_costs[a] < lifetime_best_costs[b];
            if (current_costs[a] != current_costs[b]) return current_costs[a] < current_costs[b];
            return a < b;
        });

        const double cutoff_key = selection_keys[order[next_active - 1]];
        for (int rank = 0; rank < active; ++rank) {
            const int id = order[rank];
            SaPopRuntime<Cost> rt = rt_base;
            fill_common_runtime(rt);
            rt.phase = phase;
            rt.active_state_count = active;
            rt.next_active_state_count = next_active;
            rt.state_id = id;
            rt.rank = rank;
            rt.survived = (rank < next_active);
            rt.phase_weight = phase_weight;
            rt.temp_progress_begin = temp_progress_begin;
            rt.temp_progress_end = temp_progress_end;
            rt.inner_start_temp = phase_start_temp;
            rt.inner_end_temp = phase_end_temp;
            rt.phase_time_limit_us = phase_budget_us;
            rt.current_cost_after = current_costs[id];
            rt.lifetime_best_cost_after = lifetime_best_costs[id];
            rt.selection_key = selection_keys[id];
            rt.selection_cutoff_key = cutoff_key;
            detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::StateSelection, rt);
        }

        for (int j = 0; j < next_active; ++j) active_ids[j] = order[j];

        {
            SaPopRuntime<Cost> rt = rt_base;
            fill_common_runtime(rt);
            rt.phase = phase;
            rt.active_state_count = active;
            rt.next_active_state_count = next_active;
            rt.phase_weight = phase_weight;
            rt.temp_progress_begin = temp_progress_begin;
            rt.temp_progress_end = temp_progress_end;
            rt.inner_start_temp = phase_start_temp;
            rt.inner_end_temp = phase_end_temp;
            rt.phase_time_limit_us = phase_budget_us;
            rt.phase_elapsed_us = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - phase_start_clock).count();
            rt.selection_cutoff_key = cutoff_key;
            detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::PhaseEnd, rt);
        }
        active = next_active;
    }

    // 残った個体を残り時間で改善する。返却用の最良は全探索を通じて保持する
    const int final_state_id = active_ids[0];
    const int64_t elapsed_before_final_us = elapsed_us_now();
    const int64_t final_budget_us = max<int64_t>(0, total_us - elapsed_before_final_us);
    const double final_budget_ms = detail::us_to_ms(final_budget_us);
    const double final_r0 = (K == 1 ? 0.0 : pop_param.selection_time_ratio);
    const double final_r1 = 1.0;
    const double final_start_temp = temperature_at(final_r0);
    const double final_end_temp = temperature_at(final_r1);

    {
        SaPopRuntime<Cost> rt = rt_base;
        fill_common_runtime(rt);
        rt.final_polish = true;
        rt.final_state_id = final_state_id;
        rt.state_id = final_state_id;
        rt.active_state_count = 1;
        rt.next_active_state_count = 1;
        rt.final_time_limit_us = final_budget_us;
        rt.inner_time_limit_us = final_budget_us;
        rt.temp_progress_begin = final_r0;
        rt.temp_progress_end = final_r1;
        rt.inner_start_temp = final_start_temp;
        rt.inner_end_temp = final_end_temp;
        rt.final_start_temp = final_start_temp;
        rt.final_end_temp = final_end_temp;
        rt.current_cost_before = current_costs[final_state_id];
        rt.lifetime_best_cost_before = lifetime_best_costs[final_state_id];
        detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::FinalRunStart, rt);
    }

    SaParam final_param = param;
    final_param.acceptance_width_scale = pop_param.acceptance_width_scale;
    final_param.seed = detail::sa_pop_mix_seed(param.seed, 0xF17A1ULL, static_cast<uint64_t>(final_state_id), 0xA11CEULL);
    final_param.auto_mode = false;
    final_param.samples = 0;
    final_param.start_temp = final_start_temp;
    final_param.end_temp = final_end_temp;
    // 最終 SA では param.enable_end_cost_check をそのまま使う（非 LOCAL では無効）

    SaRuntime<Cost> final_inner_runtime;
    final_inner_runtime.current_cost = current_costs[final_state_id];
    final_inner_runtime.best_cost = current_costs[final_state_id];
    bool has_final_runtime = false;

    auto final_inner_hook = [&](SaEventType event_type, const SaRuntime<Cost>& inner_rt) {
#ifdef LOCAL
        if (event_type == SaEventType::RunEnd) {
            final_inner_runtime = inner_rt;
            has_final_runtime = true;
        }
#else
        (void)event_type;
        (void)inner_rt;
#endif
    };

    auto [final_best_cost, ignored_final_best] = sa<SaPopEmptyBest, Cost>(
        final_param,
        final_budget_ms,
        save_best(final_state_id),
        [&]() -> Cost { return detail::call_sa_pop_get_cost<Cost>(get_cost, work_states[final_state_id]); },
        [&](const SaRuntime<Cost>& inner_rt) { return detail::call_propose<Cost>(propose, inner_rt, work_states[final_state_id]); },
        [&](bool accepted) { detail::call_sa_pop_finalize(finalize, work_states[final_state_id], accepted); },
        global_best_cost,
        final_inner_hook);
    (void)ignored_final_best;

    if (!has_final_runtime) {
        final_inner_runtime.best_cost = final_best_cost;
        final_inner_runtime.current_cost = detail::call_sa_pop_get_cost<Cost>(get_cost, work_states[final_state_id]);
    }

    {
        SaPopRuntime<Cost> rt = rt_base;
        fill_common_runtime(rt);
        rt.final_polish = true;
        rt.final_state_id = final_state_id;
        rt.state_id = final_state_id;
        rt.active_state_count = 1;
        rt.next_active_state_count = 1;
        rt.final_time_limit_us = final_budget_us;
        rt.inner_time_limit_us = final_budget_us;
        rt.inner_elapsed_us = final_inner_runtime.elapsed_us;
        rt.temp_progress_begin = final_r0;
        rt.temp_progress_end = final_r1;
        rt.inner_start_temp = final_start_temp;
        rt.inner_end_temp = final_end_temp;
        rt.final_start_temp = final_start_temp;
        rt.final_end_temp = final_end_temp;
        rt.current_cost_before = current_costs[final_state_id];
        rt.current_cost_after = final_inner_runtime.current_cost;
        rt.run_best_cost = final_best_cost;
        rt.lifetime_best_cost_before = lifetime_best_costs[final_state_id];
        rt.lifetime_best_cost_after = min(lifetime_best_costs[final_state_id], final_best_cost);
        rt.final_best_cost = global_best_cost;
        rt.inner_runtime = final_inner_runtime;
        detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::FinalRunEnd, rt);
    }

    {
        SaPopRuntime<Cost> rt = rt_base;
        fill_common_runtime(rt);
        rt.elapsed_us = elapsed_us_now();
        rt.final_state_id = final_state_id;
        rt.final_best_cost = global_best_cost;
        rt.final_start_temp = final_start_temp;
        rt.final_end_temp = final_end_temp;
        rt.final_time_limit_us = final_budget_us;
        rt.inner_runtime = final_inner_runtime;
        detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::RunEnd, rt);
    }

    return {global_best_cost, move(global_best_snapshot)};
}

} // namespace sa

#if __INCLUDE_LEVEL__ == 0

namespace {

struct TestState {
    int value = 0;
    int pending = 0;
};

// テストとTSP比較で使用する軽量な統計Hook。
// LOCAL時だけsa/sa_popから呼ばれるため、テストコンパイルは-DLOCALで行う。
struct SaPopTestStatsHook {
    int state_run_end_count = 0;
    int state_selection_count = 0;
    int phase_end_count = 0;
    int auto_total_samples = 0;
    int final_state_id = -1;
    long long total_iterations = 0;
    int accepted = 0;
    double final_best_cost = 0.0;

    template<sa::Numeric Cost>
    void operator()(sa::SaPopEventType event_type, const sa::SaPopRuntime<Cost>& runtime) {
        if (event_type == sa::SaPopEventType::AutoSampleEnd) {
            auto_total_samples = runtime.auto_total_samples;
        } else if (event_type == sa::SaPopEventType::StateRunEnd) {
            ++state_run_end_count;
            total_iterations += runtime.inner_runtime.iteration;
            accepted += runtime.inner_runtime.accepted_count;
        } else if (event_type == sa::SaPopEventType::StateSelection) {
            ++state_selection_count;
        } else if (event_type == sa::SaPopEventType::PhaseEnd) {
            ++phase_end_count;
        } else if (event_type == sa::SaPopEventType::FinalRunEnd) {
            final_state_id = runtime.final_state_id;
            total_iterations += runtime.inner_runtime.iteration;
            accepted += runtime.inner_runtime.accepted_count;
            final_best_cost = static_cast<double>(runtime.final_best_cost);
        }
    }
};

struct SaSingleStatsHook {
    int iterations = 0;
    int accepted = 0;
    int best_updates = 0;
    double best_cost = 0.0;

    template<sa::Numeric Cost>
    void operator()(sa::SaEventType event_type, const sa::SaRuntime<Cost>& runtime) {
        if (event_type == sa::SaEventType::RunEnd) {
            iterations = runtime.iteration;
            accepted = runtime.accepted_count;
            best_updates = runtime.best_update_count;
            best_cost = static_cast<double>(runtime.best_cost);
        }
    }
};

void run_basic_tests() {
    using namespace sa;

    auto check = [](bool cond, const string& message) {
        if (!cond) {
            cerr << "[test] failed: " << message << '\n';
            exit(1);
        }
        cout << "[test] ok: " << message << '\n';
    };

    {
        SaPopParam defaults;
        check(defaults.selection_policy == SaPopSelectionPolicy::Combination && fabs(defaults.selection_current_weight - 0.25) < 1e-12,
              "default selection policy is combination with current weight 0.25");
    }

    {
        vector<TestState> work_states = {{10}, {3}, {7}};
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 1.0;
        param.end_temp = 1.0;
        auto [cost, snapshot] = sa_pop<int, int>(
            param,
            0.0,
            move(work_states),
            [](const TestState& s) { return s.value; },
            [](const TestState& s) { return s.value; },
            [](TestState& s) {
                s.pending = -1;
                return -1;
            },
            [](TestState& s, bool accepted) {
                if (accepted) s.value += s.pending;
                s.pending = 0;
            });
        check(cost == 3 && snapshot == 3, "time_limit_ms=0 returns best initial snapshot");
    }

    {
        vector<TestState> work_states = {{30}};
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 1.0;
        param.end_temp = 1.0;
        auto [cost, snapshot] = sa_pop<int, int>(
            param,
            4.0,
            move(work_states),
            [](const TestState& s) { return s.value; },
            [](const TestState& s) { return s.value; },
            [](TestState& s, const SaRuntime<int>&) {
                s.pending = -1;
                return -1;
            },
            [](TestState& s, bool accepted) {
                if (accepted) s.value += s.pending;
                s.pending = 0;
            });
        check(cost < 30 && snapshot == cost, "K=1 uses final long SA and improves");
    }

    {
        struct LifetimeState {
            int id = 0;
            int value = 0;
            int step = 0;
            int pending = 0;
        };
        vector<LifetimeState> work_states = {{0, 0, 0, 0}, {1, -50, 0, 0}};
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 1e12;
        param.end_temp = 1e12;
        SaPopParam pop_param;
        pop_param.selection_time_ratio = 1.0;
        auto [cost, snapshot_id] = sa_pop<int, int>(
            param,
            3.0,
            move(work_states),
            [](const LifetimeState& s) { return s.id; },
            [](const LifetimeState& s) { return s.value; },
            [](LifetimeState& s) {
                if (s.id == 0) {
                    if (s.step == 0) { s.pending = -100; ++s.step; }
                    else if (s.step == 1) { s.pending = 90; ++s.step; }
                    else { s.pending = 0; }
                } else {
                    s.pending = 0;
                }
                s.value += s.pending;
                return s.pending;
            },
            [](LifetimeState& s, bool accepted) {
                if (!accepted) s.value -= s.pending;
                s.pending = 0;
            },
            pop_param);
        (void)cost;
        check(cost == -100 && snapshot_id == 0, "intermediate best survives elimination of its state");
    }

    {
        vector<TestState> work_states;
        for (int i = 0; i < 8; ++i) work_states.push_back({100 + i});
        SaParam param;
        param.auto_mode = true;
        param.samples = 32;
        param.start_temp = 10.0;
        param.end_temp = 0.1;
        SaPopParam pop_param;
        pop_param.selection_time_ratio = 1.0;
        auto [cost, snapshot] = sa_pop<int, int>(
            param,
            5.0,
            move(work_states),
            [](const TestState& s) { return s.value; },
            [](const TestState& s) { return s.value; },
            [](TestState& s, const SaRuntime<int>& rt) {
                if (rt.iteration == 0) {
                    ++s.value;
                    return 1;
                }
                s.pending = -1;
                return -1;
            },
            [](TestState& s, bool accepted) {
                if (!accepted && s.pending == 0) --s.value;
                if (accepted) s.value += s.pending;
                s.pending = 0;
            },
            pop_param);
        check(cost <= snapshot, "selection_time_ratio=1 with auto mode completes");
    }

#ifdef LOCAL
    {
        vector<TestState> work_states = {{20}, {25}, {30}, {35}};
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 2.0;
        param.end_temp = 0.2;
        SaPopCsvStatHook<int> hook("sa_pop_test_");
        auto [cost, snapshot] = sa_pop<int, int>(
            param,
            3.0,
            move(work_states),
            [](const TestState& s) { return s.value; },
            [](const TestState& s) { return s.value; },
            [](TestState& s) {
                s.pending = -1;
                return -1;
            },
            [](TestState& s, bool accepted) {
                if (accepted) s.value += s.pending;
                s.pending = 0;
            },
            SaPopParam{},
            hook);
        (void)cost;
        (void)snapshot;
        ifstream trace("sa_pop_test_trace.csv"), phase("sa_pop_test_phase.csv"), summary("sa_pop_test_summary.csv");
        check(trace.good() && phase.good() && summary.good(), "SaPopCsvStatHook writes 3 CSV files at RunEnd");
    }
#endif
}

// 外側には progress() を追加せず、時間予算と内部 Runtime の型を確認する
// 検査はテスト専用とし、ライブラリ本体に追加の制約や処理を持ち込まない
template<class T>
concept TestHasProgress = requires(const T& runtime) { { runtime.progress() } -> same_as<double>; };
static_assert(TestHasProgress<sa::SaRuntime<int>>);
static_assert(!TestHasProgress<sa::SaPopRuntime<int>>);
static_assert(same_as<decltype(sa::SaPopRuntime<int>{}.time_limit_us), int64_t>);
static_assert(same_as<decltype(sa::SaPopRuntime<int>{}.phase_time_limit_us), int64_t>);
static_assert(same_as<decltype(sa::SaPopRuntime<int>{}.inner_time_limit_us), int64_t>);
static_assert(same_as<decltype(sa::SaPopRuntime<int>{}.final_time_limit_us), int64_t>);
static_assert(same_as<decltype(sa::SaPopRuntime<int>{}.inner_runtime), sa::SaRuntime<int>>);
static_assert(is_nothrow_invocable_v<sa::NoOp, sa::SaEventType, const sa::SaRuntime<int>&>);
static_assert(is_nothrow_invocable_v<sa::NoOp, sa::SaPopEventType, const sa::SaPopRuntime<int>&>);

// API の統一、サンプリング初期化、終了時診断と CSV の単位を確認する
void run_api_tests() {
    using namespace sa;
    const auto check = [](bool ok, const string& message) -> void {
        if (!ok) {
            cerr << "[test] failed: " << message << '\n';
            exit(1);
        }
    };

    // Snapshot / Cost / WorkState が異なる型でも、先頭 2 型だけを明示して呼べる
    struct Snapshot { int value; vector<int> data; };
    struct WorkState { int value; unique_ptr<int> cache; };
    for (double budget_ms : {0.0, 0.0009}) {
        vector<WorkState> work_states;
        for (int value : {8, 3, 12}) work_states.push_back({value, make_unique<int>(value)});
        int snapshots = 0;
        int hooks = 0;
        const auto result = sa_pop<Snapshot, long long>(SaParam{}, budget_ms, move(work_states),
            [&](const WorkState& state) -> Snapshot { ++snapshots; return {state.value, {*state.cache}}; },
            [](const WorkState& state) -> long long { return state.value; },
            [&](WorkState&, const SaRuntime<long long>&) -> long long { check(false, "zero budget must not propose"); return 0; },
            [&](WorkState&, bool) -> void { check(false, "zero budget must not finalize"); },
            SaPopParam{},
            [&](SaPopEventType, const SaPopRuntime<long long>& runtime) -> void {
                ++hooks;
                check(runtime.time_limit_us == 0 && runtime.initial_state_count == 3, "zero effective population budget");
            });
        check(result.first == 3 && result.second.value == 3 && result.second.data == vector<int>{3} && snapshots == 1,
              "Snapshot, Cost and deduced move-only WorkState are independent");
        check(hooks == (detail::debug_hook_enabled ? 2 : 0), "population hooks remain LOCAL-only");
    }
    cout << "[test] ok: common template/argument order and move-only WorkState\n";

    // 64 個体では 300 件を 5 件ずつに切り上げる。正の予算でサンプル進捗は必ず 0
    {
        SaParam param;
        param.enable_end_cost_check = false;
        vector<int> work_states(64, 0);
        int samples = 0;
        int rollbacks = 0;
        bool sampling = false;
        int reported_samples = -1;
        const auto result = sa_pop<int, int>(param, 10.25, move(work_states),
            [](const int& state) -> int { return state; },
            [](const int& state) -> int { return state; },
            [&](int& state, const SaRuntime<int>& runtime) -> int {
                sampling = runtime.iteration == 0;
                if (!sampling) return 0;
                check(runtime.time_limit_us == 10250 && runtime.elapsed_us == 0 && runtime.progress() == 0.0,
                      "sampling runtime has global budget and zero elapsed time");
                ++samples;
                ++state;
                return 1;
            },
            [&](int& state, bool accepted) -> void {
                if (sampling) {
                    check(!accepted, "sample must always be rejected");
                    --state;
                    ++rollbacks;
                }
            }, SaPopParam{},
            [&](SaPopEventType event, const SaPopRuntime<int>& runtime) -> void {
                check(runtime.time_limit_us == 10250, "population budget uses microseconds");
                if (event == SaPopEventType::AutoSampleEnd) {
                    check(runtime.auto_per_state_samples == 5, "equal per-state sample allocation");
                    reported_samples = runtime.auto_total_samples;
                }
            });
        check(samples == 320 && rollbacks == 320 && result == pair<int, int>{0, 0}, "sampling count and rollback contract");
        check(reported_samples == (detail::debug_hook_enabled ? 320 : -1), "sampling stats remain LOCAL-only");
    }
    cout << "[test] ok: sampling progress is zero; 300/64 allocation still produces 320 samples\n";

    // 選抜中は診断を抑制し、最終 SA だけフラグを反映する。非 LOCAL では呼び出し数も同じ
    for (int state_count : {1, 3}) {
        array<int, 2> cost_calls{};
        for (int enabled = 0; enabled < 2; ++enabled) {
            SaParam param;
            param.auto_mode = false;
            param.enable_end_cost_check = enabled != 0;
            SaPopParam pop_param;
            pop_param.selection_time_ratio = 0.5;
            ostringstream diagnostics;
            streambuf* old_buffer = cerr.rdbuf(diagnostics.rdbuf());
            bool final_has_budget = false;
            const auto result = sa_pop<int, int>(param, 10.0, vector<int>(static_cast<size_t>(state_count), 3),
                [](const int& state) -> int { return state; },
                [&](const int& state) -> int { ++cost_calls[enabled]; return state; },
                [](int&, const SaRuntime<int>&) -> int { return 0; },
                [](int&, bool) -> void {}, pop_param,
                [&](SaPopEventType event, const SaPopRuntime<int>& runtime) -> void {
                    if (event == SaPopEventType::FinalRunStart) final_has_budget = runtime.final_time_limit_us > 0;
                });
            cerr.rdbuf(old_buffer);
            const string text = diagnostics.str();
            const string marker = "[sa] end cost check:";
            const size_t first = text.find(marker);
            const bool expect_check = detail::debug_hook_enabled && enabled != 0 && final_has_budget;
            check(result == pair<int, int>{3, 3}, "end diagnostic must not change the result");
            check(expect_check ? first != string::npos && text.find(marker, first + marker.size()) == string::npos
                               && text.find("diff=0") != string::npos : text.empty(),
                  "end cost flag controls only the final diagnostic");
        }
        // 複数個体では期限到達で内部 SA の実行数が変わるため、取得回数の比較は選抜のない 1 個体で行う
        if (state_count == 1) check(cost_calls[1] - cost_calls[0] == (detail::debug_hook_enabled ? 1 : 0), "end cost flag has no non-LOCAL effect");
    }
    cout << "[test] ok: final end-cost flag; no diagnostic or extra get_cost in non-LOCAL\n";

    // 両方式・全選抜方針をランダムな差分操作で検証し、毎回の状態を単純再計算と比較する
    int64_t verified_proposals = 0;
    for (int case_id = 0; case_id < 120; ++case_id) {
        struct VectorState {
            vector<int> values;
            long long cost = 0;
            int index = 0;
            int before = 0;
            int after = 0;
            long long delta = 0;
        };
        const auto score = [](const vector<int>& values) -> long long {
            long long sum = 0;
            for (int value : values) sum += static_cast<long long>(value) * value;
            return sum;
        };
        detail::FastRng rng(static_cast<uint64_t>(case_id));
        const bool preapply = (case_id & 1) != 0;
        const int count = array<int, 7>{1, 2, 3, 8, 32, 64, 256}[static_cast<size_t>(case_id / 6) % 7];
        vector<VectorState> work_states(static_cast<size_t>(count));
        for (auto& state : work_states) {
            state.values.resize(8);
            for (int& value : state.values) value = static_cast<int>(rng() % 41) - 20;
            state.cost = score(state.values);
        }
        SaParam param;
        param.seed = static_cast<uint64_t>(case_id);
        param.auto_mode = case_id % 4 < 2;
        param.samples = 25;
        param.enable_end_cost_check = false;
        SaPopParam pop_param;
        pop_param.selection_policy = static_cast<SaPopSelectionPolicy>((case_id / 2) % 3);
        const auto result = sa_pop<vector<int>, long long>(param, 2.75, move(work_states),
            [](const VectorState& state) -> vector<int> { return state.values; },
            [&](const VectorState& state) -> long long {
                check(state.cost == score(state.values), "cached cost matches naive score");
                return state.cost;
            },
            [&](VectorState& state, const SaRuntime<long long>& runtime) -> long long {
                ++verified_proposals;
                check(state.cost == score(state.values) && runtime.current_cost == state.cost, "pre-proposal state consistency");
                state.index = static_cast<int>(rng() % 8);
                state.before = state.values[state.index];
                state.after = static_cast<int>(rng() % 41) - 20;
                state.delta = static_cast<long long>(state.after) * state.after - static_cast<long long>(state.before) * state.before;
                if (preapply) {
                    state.values[state.index] = state.after;
                    state.cost += state.delta;
                }
                return state.delta;
            },
            [&](VectorState& state, bool accepted) -> void {
                if (preapply && !accepted) {
                    state.values[state.index] = state.before;
                    state.cost -= state.delta;
                } else if (!preapply && accepted) {
                    state.values[state.index] = state.after;
                    state.cost += state.delta;
                }
                check(state.cost == score(state.values), "post-finalize state consistency");
            }, pop_param, NoOp{});
        check(result.first == score(result.second), "returned snapshot matches its cost");
    }
    check(verified_proposals > 0, "randomized tests exercised proposals");
    cout << "[test] ok: 120 randomized cases, " << verified_proposals << " proposals match naive scores\n";

#ifdef LOCAL
    // CSV は Runtime の整数 μs を ms に変換し、共通名と区間温度名で出力する
    {
        SaPopCsvStatHook<int> hook("sa_pop_api_v04_");
        SaPopRuntime<int> runtime;
        runtime.initial_state_count = 4;
        runtime.active_state_count = 4;
        runtime.next_active_state_count = 2;
        runtime.phase_count = 2;
        runtime.seed = 77;
        runtime.time_limit_us = 12345;
        runtime.phase_time_limit_us = 3000;
        runtime.inner_time_limit_us = 1500;
        runtime.inner_start_temp = 100.0;
        runtime.inner_end_temp = 10.0;
        runtime.temp_progress_begin = 0.0;
        runtime.temp_progress_end = 0.25;
        hook(SaPopEventType::RunStart, runtime);
        runtime.elapsed_us = 100;
        hook(SaPopEventType::PhaseStart, runtime);
        runtime.state_id = 0;
        runtime.elapsed_us = 1600;
        runtime.inner_elapsed_us = 1500;
        runtime.inner_runtime.iteration = 100;
        runtime.inner_runtime.accepted_count = 60;
        runtime.inner_runtime.worse_accepted_count = 10;
        hook(SaPopEventType::StateRunEnd, runtime);
        runtime.survived = true;
        runtime.selection_key = 7.5;
        runtime.selection_cutoff_key = 9.5;
        hook(SaPopEventType::StateSelection, runtime);
        runtime.elapsed_us = 3100;
        runtime.phase_elapsed_us = 3000;
        hook(SaPopEventType::PhaseEnd, runtime);
        runtime.final_polish = true;
        runtime.final_state_id = 0;
        runtime.final_time_limit_us = 7000;
        runtime.elapsed_us = 9000;
        hook(SaPopEventType::FinalRunEnd, runtime);
        runtime.elapsed_us = 12346;
        hook(SaPopEventType::RunEnd, runtime);

        // 先頭データ行を列名で読み、列順に依存しない独立な検証を行う
        const auto split = [](const string& line) -> vector<string> {
            vector<string> cells;
            istringstream stream(line);
            for (string cell; getline(stream, cell, ',');) cells.push_back(cell);
            return cells;
        };
        const auto read_value = [&](const string& file, const string& column) -> double {
            ifstream input(file);
            string header, row;
            check(static_cast<bool>(getline(input, header)) && static_cast<bool>(getline(input, row)), "CSV has header and data");
            const auto names = split(header);
            const auto cells = split(row);
            const auto pos = find(names.begin(), names.end(), column);
            check(pos != names.end() && names.size() == cells.size() && header.find("_us") == string::npos, "CSV names and units");
            return stod(cells[static_cast<size_t>(pos - names.begin())]);
        };
        const string prefix = "sa_pop_api_v04_";
        check(read_value(prefix + "summary.csv", "time_limit_ms") == 12.345, "CSV total time budget is ms");
        check(read_value(prefix + "summary.csv", "elapsed_ms") == 12.346, "CSV elapsed time is ms");
        check(read_value(prefix + "summary.csv", "seed") == 77, "CSV seed name");
        check(read_value(prefix + "trace.csv", "inner_time_limit_ms") == 1.5, "CSV inner budget is ms");
        check(read_value(prefix + "trace.csv", "inner_start_temp") == 100.0, "CSV inner temperature name");
        check(read_value(prefix + "trace.csv", "selection_key") == 7.5, "CSV selection key name");
        check(read_value(prefix + "phase.csv", "phase_time_limit_ms") == 3.0, "CSV phase budget is ms");
        check(read_value(prefix + "phase.csv", "accept_rate") == 0.6 && read_value(prefix + "phase.csv", "worse_accept_rate") == 0.1,
              "CSV rate denominator is total iterations");
    }
    cout << "[test] ok: CSV names, millisecond budgets, elapsed time and acceptance-rate denominators\n";
#else
    // 非 LOCAL は記録用メンバーを持たず、直接呼んでも 3 ファイルを作らない。
    {
        static_assert(is_empty_v<SaPopCsvStatHook<int>>);
        const string prefix = "sa_pop_nonlocal_";
        for (const char* suffix : {"trace.csv", "phase.csv", "summary.csv"}) filesystem::remove(prefix + suffix);
        SaPopCsvStatHook<int> hook(prefix);
        SaPopRuntime<int> runtime;
        for (SaPopEventType event : {SaPopEventType::RunStart, SaPopEventType::AutoSampleStart,
                SaPopEventType::AutoSampleEnd, SaPopEventType::PhaseStart, SaPopEventType::StateRunStart,
                SaPopEventType::StateRunEnd, SaPopEventType::StateSelection, SaPopEventType::PhaseEnd,
                SaPopEventType::FinalRunStart, SaPopEventType::FinalRunEnd, SaPopEventType::RunEnd}) hook(event, runtime);
        for (const char* suffix : {"trace.csv", "phase.csv", "summary.csv"})
            check(!filesystem::exists(prefix + suffix), "non-LOCAL population CSV hook creates no file");
        cout << "[test] ok: non-LOCAL population CSV hook is empty and direct calls create no files\n";
    }
#endif
}

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct TspState {
    const vector<Point2D>* points = nullptr;
    vector<int> tour;
    double cost = 0.0;
    uint64_t rng = 1;
    int l = 0;
    int r = 0;
    double pending_delta = 0.0;

    static double dist(const Point2D& a, const Point2D& b) {
        const double dx = a.x - b.x;
        const double dy = a.y - b.y;
        return sqrt(dx * dx + dy * dy);
    }

    double edge_cost(int i, int j) const {
        return dist((*points)[tour[i]], (*points)[tour[j]]);
    }

    double calc_cost() const {
        double s = 0.0;
        const int n = static_cast<int>(tour.size());
        for (int i = 0; i < n; ++i) s += edge_cost(i, (i + 1) % n);
        return s;
    }

    uint64_t next_u64() {
        uint64_t z = (rng += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    double propose_2opt(const sa::SaRuntime<double>&) {
        const int n = static_cast<int>(tour.size());
        int a = static_cast<int>(next_u64() % n);
        int b = static_cast<int>(next_u64() % n);
        if (a > b) swap(a, b);
        if (a == b) b = (b + 1) % n;
        if (a > b) swap(a, b);
        if (a == 0 && b == n - 1) {
            b = n - 2;
            if (a > b) swap(a, b);
        }
        l = a;
        r = b;
        const int a_prev = (l - 1 + n) % n;
        const int b_next = (r + 1) % n;
        const double before = edge_cost(a_prev, l) + edge_cost(r, b_next);
        const double after = dist((*points)[tour[a_prev]], (*points)[tour[r]]) + dist((*points)[tour[l]], (*points)[tour[b_next]]);
        pending_delta = after - before;
        reverse(tour.begin() + l, tour.begin() + r + 1);
        return pending_delta;
    }

    void finalize(bool accepted) {
        if (accepted) {
            cost += pending_delta;
        } else {
            reverse(tour.begin() + l, tour.begin() + r + 1);
        }
        pending_delta = 0.0;
    }
};

vector<Point2D> make_points(int n, uint64_t seed) {
    sa::detail::FastRng rng(seed);
    vector<Point2D> p(n);
    for (int i = 0; i < n; ++i) {
        p[i].x = rng.rand_unit() * 1000.0;
        p[i].y = rng.rand_unit() * 1000.0;
    }
    return p;
}

TspState make_random_tsp_state(const vector<Point2D>& points, uint64_t seed) {
    TspState s;
    s.points = &points;
    s.rng = seed;
    const int n = static_cast<int>(points.size());
    s.tour.resize(n);
    iota(s.tour.begin(), s.tour.end(), 0);
    sa::detail::FastRng rng(seed ^ 0x123456789ABCDEFULL);
    for (int i = n - 1; i > 0; --i) {
        int j = static_cast<int>(rng() % (i + 1));
        swap(s.tour[i], s.tour[j]);
    }
    s.cost = s.calc_cost();
    return s;
}

struct TspRunStats {
    string method;
    int K = 1;
    int n = 0;
    long long total_candidates = 0; // propose 呼び出し総数。auto sampling も含む
    long long total_iters = 0;      // SA 本体の反復数
    long long auto_samples = 0;     // total_candidates - total_iters
    int state_runs = 0;
    int phase_count = 0;
    int final_state_id = -1;
    double time_limit_ms = 0.0;
    double elapsed_ms = 0.0;
    double initial_best = 0.0;
    double initial_avg = 0.0;
    double best_cost = 0.0;
    int best_tour_size = 0;
    int accepted = 0;
};

TspRunStats run_tsp_sa_pop(const vector<Point2D>& points, int K, double time_limit_ms, uint64_t seed) {
    vector<TspState> work_states;
    work_states.reserve(K);
    double initial_sum = 0.0;
    double initial_best = numeric_limits<double>::infinity();
    for (int i = 0; i < K; ++i) {
        work_states.push_back(make_random_tsp_state(points, seed + i * 1009ULL));
        initial_sum += work_states.back().cost;
        initial_best = min(initial_best, work_states.back().cost);
    }

    sa::SaParam param;
    param.seed = seed ^ 0xA5A5A5A5ULL;
    param.auto_mode = true;
    param.samples = 100;
    param.start_accept_prob = 0.8;
    param.end_accept_prob = 0.02;
    param.start_temp = 100.0;
    param.end_temp = 0.01;

    sa::SaPopParam pop_param;
    pop_param.selection_time_ratio = 0.75;

    long long total_candidates = 0;
    SaPopTestStatsHook hook;
    const auto st = chrono::steady_clock::now();
    auto [best_cost, best_tour] = sa::sa_pop<vector<int>, double>(
        param,
        time_limit_ms,
        move(work_states),
        [](const TspState& s) { return s.tour; },
        [](const TspState& s) { return s.cost; },
        [&](TspState& s, const sa::SaRuntime<double>& rt) {
            ++total_candidates;
            return s.propose_2opt(rt);
        },
        [](TspState& s, bool accepted) { s.finalize(accepted); },
        pop_param,
        std::ref(hook));
    const double elapsed_ms = sa::detail::us_to_ms(sa::detail::steady_elapsed_us(st));

    TspRunStats res;
    res.method = "sa_pop";
    res.K = K;
    res.n = static_cast<int>(points.size());
    res.total_candidates = total_candidates;
    res.total_iters = hook.total_iterations;
    res.auto_samples = total_candidates - hook.total_iterations;
    res.state_runs = hook.state_run_end_count;
    res.phase_count = 0;
    for (int a = K; a > 1; a = max(1, (3 * a) / 4)) ++res.phase_count;
    res.final_state_id = hook.final_state_id;
    res.time_limit_ms = time_limit_ms;
    res.elapsed_ms = elapsed_ms;
    res.initial_best = initial_best;
    res.initial_avg = initial_sum / K;
    res.best_cost = best_cost;
    res.best_tour_size = static_cast<int>(best_tour.size());
    res.accepted = hook.accepted;
    return res;
}

TspRunStats run_tsp_sa_multi(const vector<Point2D>& points, int K, double time_limit_ms, uint64_t seed) {
    vector<TspState> work_states;
    work_states.reserve(K);
    double initial_sum = 0.0;
    double initial_best = numeric_limits<double>::infinity();
    for (int i = 0; i < K; ++i) {
        work_states.push_back(make_random_tsp_state(points, seed + i * 1009ULL));
        initial_sum += work_states.back().cost;
        initial_best = min(initial_best, work_states.back().cost);
    }

    sa::SaParam param;
    param.auto_mode = true;
    param.samples = 100;
    param.start_accept_prob = 0.8;
    param.end_accept_prob = 0.02;
    param.start_temp = 100.0;
    param.end_temp = 0.01;

    const auto st = chrono::steady_clock::now();
    double best_cost = numeric_limits<double>::infinity();
    vector<int> best_tour;
    long long total_candidates = 0;
    long long total_iters = 0;
    int total_accepted = 0;
    int best_idx = -1;
    const double per_ms = time_limit_ms / max(1, K);
    for (int i = 0; i < K; ++i) {
        param.seed = seed ^ (0xBADC0FFEEULL + static_cast<uint64_t>(i) * 10007ULL);
        SaSingleStatsHook hook;
        auto [cost, tour] = sa::sa<vector<int>, double>(
            param,
            per_ms,
            [&]() { return work_states[i].tour; },
            [&]() { return work_states[i].cost; },
            [&](const sa::SaRuntime<double>& rt) {
                ++total_candidates;
                return work_states[i].propose_2opt(rt);
            },
            [&](bool accepted) { work_states[i].finalize(accepted); },
            best_cost,
            std::ref(hook));
        total_iters += hook.iterations;
        total_accepted += hook.accepted;
        if (cost < best_cost) {
            best_cost = cost;
            best_tour = move(*tour);
            best_idx = i;
        }
    }
    const double elapsed_ms = sa::detail::us_to_ms(sa::detail::steady_elapsed_us(st));

    TspRunStats res;
    res.method = "sa_multi";
    res.K = K;
    res.n = static_cast<int>(points.size());
    res.total_candidates = total_candidates;
    res.total_iters = total_iters;
    res.auto_samples = total_candidates - total_iters;
    res.state_runs = K;
    res.phase_count = 0;
    res.final_state_id = best_idx;
    res.time_limit_ms = time_limit_ms;
    res.elapsed_ms = elapsed_ms;
    res.initial_best = initial_best;
    res.initial_avg = initial_sum / K;
    res.best_cost = best_cost;
    res.best_tour_size = static_cast<int>(best_tour.size());
    res.accepted = total_accepted;
    return res;
}

void run_tsp_comparison() {
    // LOCAL時のsa_funcの終了時チェック出力が計測を歪めないよう、TSP比較中だけcerrを無効化する。
    const auto old_cerr_state = cerr.rdstate();
    cerr.setstate(ios::badbit);

    const int n = 64;
    const double time_limit_ms = 20.0;
    const uint64_t seed = 20260503ULL;
    const vector<Point2D> points = make_points(n, seed);

    cout << "\n[tsp] n=" << n << ", time_limit_ms=" << time_limit_ms << ", auto_mode=true\n";
    cout << "method,K,total_candidates,total_iters,auto_samples,state_runs,phase_count,final_state_id,initial_best,initial_avg,best_cost,elapsed_ms,best_tour_size,accepted\n";

    for (int K = 1; K <= 256; K <<= 1) {
        TspRunStats pop = run_tsp_sa_pop(points, K, time_limit_ms, seed + K * 17ULL);
        TspRunStats multi = run_tsp_sa_multi(points, K, time_limit_ms, seed + K * 17ULL);
        for (const TspRunStats& r : {pop, multi}) {
            cout << fixed << setprecision(6)
                 << r.method << ',' << r.K << ',' << r.total_candidates << ',' << r.total_iters << ','
                 << r.auto_samples << ',' << r.state_runs << ',' << r.phase_count << ',' << r.final_state_id << ','
                 << r.initial_best << ',' << r.initial_avg << ',' << r.best_cost << ','
                 << r.elapsed_ms << ',' << r.best_tour_size << ',' << r.accepted << '\n';
        }
    }

    cerr.clear(old_cerr_state);
}

} // namespace


// 初期最良を加熱後も保持し、最終 SA が改善した場合はその保存解を返す
void run_initial_archive_tests() {
    const auto check = [](bool ok, const char* message) {
        if (!ok) { cerr << "[test] failed: " << message << '\n'; exit(1); }
    };
    sa::SaParam param;
    param.auto_mode = false;
    param.start_temp = param.end_temp = 1e200;
    param.enable_end_cost_check = false;
    int snapshots = 0;
    auto initial = sa::sa_pop<double, double>(param, 5.0, vector<double>{12., -25., 8.},
        [&](const double& value) { ++snapshots; return value; }, [](const double& value) { return value; },
        [](double& value) { value += .25; return .25; },
        [](double& value, bool accepted) { if (!accepted) value -= .25; });
    check(initial == pair<double, double>{-25., -25.} && snapshots == 1,
          "one global initial snapshot survives heating without a redundant final copy");

    struct State { unique_ptr<int> value; int pending = 0; };
    vector<State> states;
    for (int value : {30, 40, 50}) states.push_back({make_unique<int>(value), 0});
    auto improved = sa::sa_pop<int, int>(param, 5.0, move(states),
        [](const State& state) { return *state.value; }, [](const State& state) { return *state.value; },
        [](State& state) { state.pending = *state.value > -10 ? -1 : 0; return state.pending; },
        [](State& state, bool accepted) { if (accepted) *state.value += state.pending; });
    check(improved == pair<int, int>{-10, -10}, "final improvement and move-only work state");

    // 旧4要素の aggregate 初期化を維持し、手動温度と推定失敗時には倍率を掛けない
    const sa::SaPopParam legacy{.75, 256, sa::SaPopSelectionPolicy::Combination, .25};
    check(legacy.auto_end_temp_scale == .1, "legacy aggregate parameter construction");
    for (int mode = 0; mode < 3; ++mode) {
        sa::SaParam p;
        p.auto_mode = mode != 0;
        p.samples = 32;
        p.start_temp = 17.; p.end_temp = .7;
        p.enable_end_cost_check = false;
        sa::SaPopParam pp;
        pp.auto_end_temp_scale = .2;
        double start = -1., end = -1.;
        const auto temperatures = sa::sa_pop<int, int>(p, 5., vector<int>{0},
            [](const int& state) { return state; }, [](const int& state) { return state; },
            [mode](int&, const sa::SaRuntime<int>& runtime) { return mode == 2 && runtime.iteration == 0 ? 10 : 0; },
            [](int&, bool) {}, pp,
            [&](sa::SaPopEventType event, const sa::SaPopRuntime<int>& runtime) {
                if (event == sa::SaPopEventType::FinalRunStart) { start = runtime.global_start_temp; end = runtime.global_end_temp; }
            });
        check(temperatures == pair<int, int>{0, 0}, "temperature sampling preserves state");
        if constexpr (sa::detail::debug_hook_enabled) {
            const double expected_start = mode == 2 ? -10. / log(p.start_accept_prob) : 17.;
            const double expected_end = mode == 2 ? -10. / log(p.end_accept_prob) * .2 : .7;
            check(abs(start - expected_start) < 1e-10 && abs(end - expected_end) < 1e-10,
                  "auto scale, manual temperature and fallback contracts");
        }
    }
    cout << "[test] ok: initial archive, final improvement, move-only state, auto temperature extension\n";
}

int main() {
    run_initial_archive_tests();
    run_basic_tests();
    run_api_tests();
    run_tsp_comparison();
    return 0;
}

#endif
