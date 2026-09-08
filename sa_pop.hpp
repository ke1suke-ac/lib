#pragma once
#include <bits/stdc++.h>
#include "sa_func.hpp"
using namespace std;

namespace sa {

// sa_pop の選別方針。
// TSPなどの最小化問題を前提に、小さい selection key の個体を残す。
enum class SaPopSelectionPolicy {
    Current,     // 現在の cost で選別する
    Lifetime,    // その lineage が過去に到達した best cost で選別する
    Combination, // lifetime best に current との乖離ペナルティを加えて選別する
};

// sa_pop のパラメータ。
// AHC 用の軽量な population annealing 風ラッパーとして、個体数は最大 256 を想定する。
struct SaPopParam {
    double selection_ratio = 0.75; // 全体時間のうち、個体選別に使う割合
    int max_population = 256;     // 受け付ける最大個体数

    // デフォルトでは、過去bestのポテンシャルを重視しつつ、現在位置が悪すぎる個体を少し減点する。
    SaPopSelectionPolicy selection_policy = SaPopSelectionPolicy::Combination;
    double selection_current_weight = 0.25; // Combination時: lifetime + weight * max(0, current - lifetime)
};

// 選別フェーズでは best state 本体を保持しない。
// 既存 sa の State 型として空型を渡し、best cost だけを使う。
struct SaPopEmptyBest {};

// sa_pop 用 Hook の発火タイミング。
enum class SaPopEventType {
    RunStart,
    AutoSampleStart,
    AutoSampleEnd,
    PhaseStart,
    StateRunStart,
    StateRunEnd,
    StateSelection,
    PhaseEnd,
    FinalRunStart,
    FinalRunEnd,
    RunEnd,
};

// sa_pop の実行時情報。
// 重い集計や文字列化は Hook 側で行うため、ここには軽量な数値だけを持たせる。
template<Numeric Cost>
struct SaPopRuntime {
    int initial_population = 0;
    int phase = -1;
    int phase_count = 0;

    int active_count = 0;
    int next_active_count = 0;

    int state_order = -1; // phase 内の処理順
    int state_id = -1;    // 初期 states の index。物理 compact しないので lineage id と同義

    int rank = -1;
    bool survived = false;
    bool final_polish = false;

    uint64_t param_seed = 0;
    double time_limit_ms = 0.0;
    double selection_ratio = 0.75;
    double phase_time_limit_ms = 0.0;
    double inner_time_limit_ms = 0.0;
    double final_time_limit_ms = 0.0;

    int64_t elapsed_us = 0;
    int64_t phase_elapsed_us = 0;
    int64_t inner_elapsed_us = 0;
    int64_t total_elapsed_us = 0;

    bool auto_mode = false;
    bool auto_fallback = false;
    int auto_per_state_samples = 0;
    int auto_total_samples = 0;
    int auto_worse_count = 0;
    double auto_avg_worse_delta = 0.0;

    double global_start_temp = 0.0;
    double global_end_temp = 0.0;

    double phase_weight = 0.0;
    double total_phase_weight = 0.0;

    double temp_r0 = 0.0;
    double temp_r1 = 0.0;
    double start_temp = 0.0;
    double end_temp = 0.0;

    Cost current_cost_before{};
    Cost current_cost_after{};
    Cost run_best_cost{};

    Cost lifetime_best_cost_before{};
    Cost lifetime_best_cost_after{};

    double selection_key_cost = 0.0;
    double selection_cutoff_cost = 0.0;

    int selection_policy = static_cast<int>(SaPopSelectionPolicy::Combination);
    double selection_current_weight = 0.25;

    int final_state_id = -1;
    double final_start_temp = 0.0;
    double final_end_temp = 0.0;
    Cost final_best_cost{};

    SaRuntime<Cost> inner;
};

// デフォルト Hook。
struct SaPopNoOp {
    template<Numeric Cost>
    void operator()(SaPopEventType, const SaPopRuntime<Cost>&) const noexcept {}
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

template<Numeric Cost, class Propose, class WorkState>
inline Cost call_sa_pop_propose(Propose& propose, WorkState& state, const SaRuntime<Cost>& runtime) {
    if constexpr (is_invocable_r_v<Cost, Propose, WorkState&, const SaRuntime<Cost>&>) {
        return propose(state, runtime);
    } else if constexpr (is_invocable_r_v<Cost, Propose, WorkState&>) {
        return propose(state);
    } else {
        static_assert(sa_pop_always_false_v<Propose>, "Propose must be callable as Cost(WorkState&) or Cost(WorkState&, const SaRuntime<Cost>&)");
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
        case SaPopSelectionPolicy::Lifetime: return "lifetime";
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
    if (policy == SaPopSelectionPolicy::Lifetime) return life;
    return life + current_weight * max(0.0, cur - life);
}

} // namespace detail

// sa_pop 用 CSV Hook。
// sa_func の CsvStatHook と同様に、途中ではファイルへ書かず、RunEnd でまとめて出力する。
template<Numeric Cost>
class SaPopCsvHook {
public:
    explicit SaPopCsvHook(string prefix = "sa_pop_") : prefix_(move(prefix)) {}

    void operator()(SaPopEventType event_type, const SaPopRuntime<Cost>& runtime) {
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
        int active_count = 0;
        int next_active_count = 0;
        int state_order = -1;
        int state_id = -1;
        int rank = -1;
        int survived = 0;
        double inner_time_limit_ms = 0.0;
        double inner_elapsed_ms = 0.0;
        int inner_iterations = 0;
        double temp_r0 = 0.0;
        double temp_r1 = 0.0;
        double start_temp = 0.0;
        double end_temp = 0.0;
        Cost current_cost_before{};
        Cost current_cost_after{};
        Cost run_best_cost{};
        Cost lifetime_best_cost_before{};
        Cost lifetime_best_cost_after{};
        double selection_key_cost = 0.0;
        double selection_cutoff_cost = 0.0;
        int accepted_count = 0;
        int worse_accepted_count = 0;
        int best_update_count = 0;
    };

    struct PhaseRow {
        double elapsed_ms = 0.0;
        int phase = -1;
        int phase_count = 0;
        int active_count = 0;
        int next_active_count = 0;
        double phase_weight = 0.0;
        double total_phase_weight = 0.0;
        double temp_r0 = 0.0;
        double temp_r1 = 0.0;
        double start_temp = 0.0;
        double end_temp = 0.0;
        double phase_time_limit_ms = 0.0;
        double phase_elapsed_ms = 0.0;
        Cost best_lifetime_cost{};
        Cost best_current_cost{};
        double selection_cutoff_cost = 0.0;
        Cost survivor_best_lifetime_cost{};
        Cost survivor_worst_lifetime_cost{};
        int64_t total_iterations = 0;
        int64_t total_accepted_count = 0;
        int64_t total_worse_accepted_count = 0;
        int64_t total_best_update_count = 0;
        double avg_accept_rate = 0.0;
        double avg_worse_accept_rate = 0.0;
    };

    struct SummaryRow {
        double time_limit_ms = 0.0;
        double total_elapsed_ms = 0.0;
        double selection_ratio = 0.75;
        int selection_policy = static_cast<int>(SaPopSelectionPolicy::Combination);
        double selection_current_weight = 0.25;
        int initial_population = 0;
        int phase_count = 0;
        uint64_t param_seed = 0;
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
        row.active_count = rt.active_count;
        row.next_active_count = rt.next_active_count;
        row.state_order = rt.state_order;
        row.state_id = rt.state_id;
        row.rank = rt.rank;
        row.survived = rt.survived ? 1 : 0;
        row.inner_time_limit_ms = rt.inner_time_limit_ms;
        row.inner_elapsed_ms = ms_from_us(rt.inner_elapsed_us);
        row.inner_iterations = rt.inner.iteration;
        row.temp_r0 = rt.temp_r0;
        row.temp_r1 = rt.temp_r1;
        row.start_temp = rt.start_temp;
        row.end_temp = rt.end_temp;
        row.current_cost_before = rt.current_cost_before;
        row.current_cost_after = rt.current_cost_after;
        row.run_best_cost = rt.run_best_cost;
        row.lifetime_best_cost_before = rt.lifetime_best_cost_before;
        row.lifetime_best_cost_after = rt.lifetime_best_cost_after;
        row.selection_key_cost = rt.selection_key_cost;
        row.selection_cutoff_cost = rt.selection_cutoff_cost;
        row.accepted_count = rt.inner.accepted_count;
        row.worse_accepted_count = rt.inner.worse_accepted_count;
        row.best_update_count = rt.inner.best_update_count;
        return row;
    }

    void on_run_start(const SaPopRuntime<Cost>& rt) {
        trace_rows_.clear();
        phase_rows_.clear();
        pending_trace_rows_.clear();
        pending_index_by_state_.fill(-1);
        phase_agg_ = PhaseAgg{};

        summary_ = SummaryRow{};
        summary_.time_limit_ms = rt.time_limit_ms;
        summary_.selection_ratio = rt.selection_ratio;
        summary_.selection_policy = rt.selection_policy;
        summary_.selection_current_weight = rt.selection_current_weight;
        summary_.initial_population = rt.initial_population;
        summary_.phase_count = rt.phase_count;
        summary_.param_seed = rt.param_seed;
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
        row.active_count = rt.active_count;
        row.next_active_count = rt.next_active_count;
        row.phase_weight = rt.phase_weight;
        row.total_phase_weight = rt.total_phase_weight;
        row.temp_r0 = rt.temp_r0;
        row.temp_r1 = rt.temp_r1;
        row.start_temp = rt.start_temp;
        row.end_temp = rt.end_temp;
        row.phase_time_limit_ms = rt.phase_time_limit_ms;
    }

    void on_state_run_end(const SaPopRuntime<Cost>& rt) {
        TraceRow row = make_trace_row(rt);
        if (0 <= rt.state_id && rt.state_id < 256) {
            pending_index_by_state_[rt.state_id] = static_cast<int>(pending_trace_rows_.size());
        }
        pending_trace_rows_.push_back(row);

        if (!phase_agg_.active) return;
        PhaseRow& pr = phase_agg_.row;
        pr.total_iterations += rt.inner.iteration;
        pr.total_accepted_count += rt.inner.accepted_count;
        pr.total_worse_accepted_count += rt.inner.worse_accepted_count;
        pr.total_best_update_count += rt.inner.best_update_count;

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
                row.selection_key_cost = rt.selection_key_cost;
                row.selection_cutoff_cost = rt.selection_cutoff_cost;
            }
        }

        if (!phase_agg_.active) return;
        PhaseRow& pr = phase_agg_.row;
        pr.selection_cutoff_cost = rt.selection_cutoff_cost;
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
        if (row.total_iterations > 0) {
            row.avg_accept_rate = static_cast<double>(row.total_accepted_count) / static_cast<double>(row.total_iterations);
            row.avg_worse_accept_rate = static_cast<double>(row.total_worse_accepted_count) / static_cast<double>(row.total_iterations);
        }
        phase_rows_.push_back(row);
        phase_agg_ = PhaseAgg{};
    }

    void on_final_run_end(const SaPopRuntime<Cost>& rt) {
        trace_rows_.push_back(make_trace_row(rt));
        summary_.final_state_id = rt.final_state_id;
        summary_.final_start_temp = rt.final_start_temp;
        summary_.final_end_temp = rt.final_end_temp;
        summary_.final_time_limit_ms = rt.final_time_limit_ms;
        summary_.final_elapsed_ms = ms_from_us(rt.inner_elapsed_us);
        summary_.final_iterations = rt.inner.iteration;
        summary_.final_best_cost = rt.final_best_cost;
    }

    void on_run_end(const SaPopRuntime<Cost>& rt) {
        if (!has_summary_) on_run_start(rt);
        summary_.total_elapsed_ms = ms_from_us(rt.total_elapsed_us);
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
        ofs << "stage,elapsed_ms,phase,phase_count,active_count,next_active_count,state_order,state_id,rank,survived,";
        ofs << "inner_time_limit_ms,inner_elapsed_ms,inner_iterations,temp_r0,temp_r1,start_temp,end_temp,";
        ofs << "current_cost_before,current_cost_after,run_best_cost,lifetime_best_cost_before,lifetime_best_cost_after,";
        ofs << "selection_key_cost,selection_cutoff_cost,accepted_count,worse_accepted_count,best_update_count\n";
        ofs << fixed << setprecision(9);
        for (const TraceRow& r : trace_rows_) {
            ofs << r.stage << ',' << r.elapsed_ms << ',' << r.phase << ',' << r.phase_count << ','
                << r.active_count << ',' << r.next_active_count << ',' << r.state_order << ',' << r.state_id << ','
                << r.rank << ',' << r.survived << ',' << r.inner_time_limit_ms << ',' << r.inner_elapsed_ms << ','
                << r.inner_iterations << ',' << r.temp_r0 << ',' << r.temp_r1 << ',' << r.start_temp << ',' << r.end_temp << ','
                << r.current_cost_before << ',' << r.current_cost_after << ',' << r.run_best_cost << ','
                << r.lifetime_best_cost_before << ',' << r.lifetime_best_cost_after << ','
                << r.selection_key_cost << ',' << r.selection_cutoff_cost << ','
                << r.accepted_count << ',' << r.worse_accepted_count << ',' << r.best_update_count << '\n';
        }
    }

    void write_phase_csv(const string& filename) const {
        ofstream ofs(filename);
        if (!ofs) {
            cerr << "[sa_pop] warning: failed to open csv file: " << filename << '\n';
            return;
        }
        ofs << "elapsed_ms,phase,phase_count,active_count,next_active_count,phase_weight,total_phase_weight,";
        ofs << "temp_r0,temp_r1,start_temp,end_temp,phase_time_limit_ms,phase_elapsed_ms,";
        ofs << "best_lifetime_cost,best_current_cost,selection_cutoff_cost,survivor_best_lifetime_cost,survivor_worst_lifetime_cost,";
        ofs << "total_iterations,total_accepted_count,total_worse_accepted_count,total_best_update_count,avg_accept_rate,avg_worse_accept_rate\n";
        ofs << fixed << setprecision(9);
        for (const PhaseRow& r : phase_rows_) {
            ofs << r.elapsed_ms << ',' << r.phase << ',' << r.phase_count << ',' << r.active_count << ',' << r.next_active_count << ','
                << r.phase_weight << ',' << r.total_phase_weight << ',' << r.temp_r0 << ',' << r.temp_r1 << ','
                << r.start_temp << ',' << r.end_temp << ',' << r.phase_time_limit_ms << ',' << r.phase_elapsed_ms << ','
                << r.best_lifetime_cost << ',' << r.best_current_cost << ',' << r.selection_cutoff_cost << ','
                << r.survivor_best_lifetime_cost << ',' << r.survivor_worst_lifetime_cost << ','
                << r.total_iterations << ',' << r.total_accepted_count << ',' << r.total_worse_accepted_count << ','
                << r.total_best_update_count << ',' << r.avg_accept_rate << ',' << r.avg_worse_accept_rate << '\n';
        }
    }

    void write_summary_csv(const string& filename) const {
        ofstream ofs(filename);
        if (!ofs) {
            cerr << "[sa_pop] warning: failed to open csv file: " << filename << '\n';
            return;
        }
        ofs << "time_limit_ms,total_elapsed_ms,selection_ratio,selection_policy,selection_current_weight,initial_population,phase_count,param_seed,";
        ofs << "auto_mode,auto_per_state_samples,auto_total_samples,auto_worse_count,auto_avg_worse_delta,auto_fallback,";
        ofs << "global_start_temp,global_end_temp,final_state_id,final_start_temp,final_end_temp,";
        ofs << "final_time_limit_ms,final_elapsed_ms,final_iterations,final_best_cost\n";
        ofs << fixed << setprecision(9);
        const SummaryRow& r = summary_;
        ofs << r.time_limit_ms << ',' << r.total_elapsed_ms << ',' << r.selection_ratio << ','
            << r.selection_policy << ',' << r.selection_current_weight << ',' << r.initial_population << ','
            << r.phase_count << ',' << r.param_seed << ',' << r.auto_mode << ',' << r.auto_per_state_samples << ','
            << r.auto_total_samples << ',' << r.auto_worse_count << ',' << r.auto_avg_worse_delta << ',' << r.auto_fallback << ','
            << r.global_start_temp << ',' << r.global_end_temp << ',' << r.final_state_id << ',' << r.final_start_temp << ','
            << r.final_end_temp << ',' << r.final_time_limit_ms << ',' << r.final_elapsed_ms << ','
            << r.final_iterations << ',' << r.final_best_cost << '\n';
    }
};

// 複数個体 SA。
// 物理 compact は行わず、active_ids だけで生存個体を管理する。
template<
    class WorkState,
    Numeric Cost,
    class Snapshot,
    class GetCost,
    class GetSnapshot,
    class Propose,
    class Finalize,
    class DebugHook = SaPopNoOp>
pair<Cost, Snapshot> sa_pop(
    const SaParam& param,
    double time_limit_ms,
    vector<WorkState> states,
    GetCost get_cost,
    GetSnapshot get_snapshot,
    Propose propose,
    Finalize finalize,
    const SaPopParam& pop_param = SaPopParam{},
    DebugHook debug_hook = DebugHook{}) {
    static_assert(copy_constructible<Snapshot>, "Snapshot must be copy constructible");
    if constexpr (integral<Cost>) static_assert(is_signed_v<Cost>, "Signed integral Cost is required because delta can be negative");

    const int K = static_cast<int>(states.size());
    assert(K > 0 && "sa_pop requires at least one state");
    assert(K <= 256 && "sa_pop supports at most 256 states");
    assert(K <= pop_param.max_population && "states.size() exceeds SaPopParam.max_population");
    assert(isfinite(time_limit_ms) && time_limit_ms >= 0.0 && "time_limit_ms must be finite and non-negative");
    assert(isfinite(pop_param.selection_ratio) && 0.0 <= pop_param.selection_ratio && pop_param.selection_ratio <= 1.0 && "selection_ratio must be in [0, 1]");
    assert(isfinite(pop_param.selection_current_weight) && 0.0 <= pop_param.selection_current_weight && pop_param.selection_current_weight <= 1.0 && "selection_current_weight must be in [0, 1]");
    assert(param.samples >= 0 && "sa::SaParam.samples must be >= 0");
    assert(0.0 < param.start_accept_prob && param.start_accept_prob < 1.0);
    assert(0.0 < param.end_accept_prob && param.end_accept_prob < 1.0);
    assert(param.start_temp > 0.0 && param.end_temp > 0.0);

    array<uint16_t, 256> active_ids{};
    array<uint16_t, 256> order{};
    array<Cost, 256> current_costs{};
    array<Cost, 256> lifetime_best_costs{};
    array<double, 256> selection_keys{};

    for (int i = 0; i < K; ++i) {
        active_ids[i] = static_cast<uint16_t>(i);
        current_costs[i] = detail::call_sa_pop_get_cost<Cost>(get_cost, states[i]);
        lifetime_best_costs[i] = current_costs[i];
        selection_keys[i] = detail::sa_pop_selection_key(pop_param.selection_policy, pop_param.selection_current_weight, current_costs[i], lifetime_best_costs[i]);
    }

    const int phase_count = (K <= 1 ? 0 : static_cast<int>(bit_width(static_cast<unsigned>(K - 1))));

    vector<int> active_counts;
    active_counts.reserve(max(1, phase_count));
    for (int a = K, p = 0; p < phase_count; ++p) {
        active_counts.push_back(a);
        a = max(1, (a + 1) / 2);
    }

    double total_phase_weight = 0.0;
    for (int a : active_counts) total_phase_weight += 1.0 / static_cast<double>(a);

    const auto global_start_clock = chrono::steady_clock::now();
    const auto elapsed_us_now = [&]() -> int64_t {
        return detail::steady_elapsed_us(global_start_clock);
    };

    const int64_t total_us = static_cast<int64_t>(max(0.0, time_limit_ms * 1000.0));
    const int64_t selection_us = static_cast<int64_t>(static_cast<double>(total_us) * pop_param.selection_ratio);
    const auto selection_deadline = global_start_clock + chrono::microseconds(selection_us);

    double global_start_temp = param.start_temp;
    double global_end_temp = param.end_temp;

    SaPopRuntime<Cost> rt_base;
    rt_base.initial_population = K;
    rt_base.phase_count = phase_count;
    rt_base.param_seed = param.seed;
    rt_base.time_limit_ms = time_limit_ms;
    rt_base.selection_ratio = pop_param.selection_ratio;
    rt_base.selection_policy = static_cast<int>(pop_param.selection_policy);
    rt_base.selection_current_weight = pop_param.selection_current_weight;
    rt_base.auto_mode = param.auto_mode;
    rt_base.global_start_temp = global_start_temp;
    rt_base.global_end_temp = global_end_temp;

    auto fill_common_runtime = [&](SaPopRuntime<Cost>& rt) {
        rt.initial_population = K;
        rt.phase_count = phase_count;
        rt.param_seed = param.seed;
        rt.time_limit_ms = time_limit_ms;
        rt.selection_ratio = pop_param.selection_ratio;
        rt.selection_policy = static_cast<int>(pop_param.selection_policy);
        rt.selection_current_weight = pop_param.selection_current_weight;
        rt.auto_mode = param.auto_mode;
        rt.global_start_temp = global_start_temp;
        rt.global_end_temp = global_end_temp;
        rt.total_phase_weight = total_phase_weight;
        rt.elapsed_us = elapsed_us_now();
        rt.total_elapsed_us = rt.elapsed_us;
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

    if (total_us <= 0) {
        const int best_id = best_current_id();
        Snapshot snap = detail::call_sa_pop_get_snapshot<Snapshot>(get_snapshot, states[best_id]);
        SaPopRuntime<Cost> rt = rt_base;
        fill_common_runtime(rt);
        rt.final_state_id = best_id;
        rt.final_best_cost = current_costs[best_id];
        detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::RunEnd, rt);
        return {current_costs[best_id], move(snap)};
    }

    // population 全体から一度だけ auto 温度を推定する。
    int auto_per_state_samples = 0;
    int auto_total_samples = 0;
    int auto_worse_count = 0;
    double auto_avg_worse_delta = 0.0;
    bool auto_fallback = false;

    if (param.auto_mode && param.samples > 0) {
        auto_per_state_samples = (param.samples + K - 1) / K;
        {
            SaPopRuntime<Cost> rt = rt_base;
            fill_common_runtime(rt);
            rt.auto_per_state_samples = auto_per_state_samples;
            detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::AutoSampleStart, rt);
        }

        double worse_delta_sum = 0.0;
        SaRuntime<Cost> sample_runtime;
        sample_runtime.iteration = 0;
        sample_runtime.temperature = param.start_temp;

        for (int i = 0; i < K; ++i) {
            sample_runtime.current_cost = current_costs[i];
            sample_runtime.best_cost = current_costs[i];
            for (int t = 0; t < auto_per_state_samples; ++t) {
                if ((auto_total_samples & 31) == 0 && elapsed_us_now() >= total_us) break;
                const Cost delta = detail::call_sa_pop_propose<Cost>(propose, states[i], sample_runtime);
                if (delta > Cost{0}) {
                    worse_delta_sum += detail::to_double(delta);
                    ++auto_worse_count;
                }
                detail::call_sa_pop_finalize(finalize, states[i], false);
                ++auto_total_samples;
            }
            if (elapsed_us_now() >= total_us) break;
        }

        if (auto_worse_count > 0) {
            auto_avg_worse_delta = worse_delta_sum / static_cast<double>(auto_worse_count);
            global_start_temp = detail::temperature_from_avg_delta(auto_avg_worse_delta, param.start_accept_prob);
            global_end_temp = detail::temperature_from_avg_delta(auto_avg_worse_delta, param.end_accept_prob);
        } else {
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
        const int next_active = max(1, (active + 1) / 2);
        const double phase_weight = 1.0 / static_cast<double>(active);
        const double temp_r0 = (total_phase_weight > 0.0 ? pop_param.selection_ratio * prefix_weight / total_phase_weight : 0.0);
        const double temp_r1 = (total_phase_weight > 0.0 ? pop_param.selection_ratio * (prefix_weight + phase_weight) / total_phase_weight : pop_param.selection_ratio);
        prefix_weight += phase_weight;
        const double phase_start_temp = temperature_at(temp_r0);
        const double phase_end_temp = temperature_at(temp_r1);

        const int remaining_phases = phase_count - phase;
        int64_t remaining_selection_us = chrono::duration_cast<chrono::microseconds>(selection_deadline - chrono::steady_clock::now()).count();
        remaining_selection_us = max<int64_t>(0, remaining_selection_us);
        const int64_t phase_budget_us = (remaining_phases > 0 ? remaining_selection_us / remaining_phases : 0);
        const auto phase_deadline = chrono::steady_clock::now() + chrono::microseconds(phase_budget_us);

        {
            SaPopRuntime<Cost> rt = rt_base;
            fill_common_runtime(rt);
            rt.phase = phase;
            rt.active_count = active;
            rt.next_active_count = next_active;
            rt.phase_weight = phase_weight;
            rt.temp_r0 = temp_r0;
            rt.temp_r1 = temp_r1;
            rt.start_temp = phase_start_temp;
            rt.end_temp = phase_end_temp;
            rt.phase_time_limit_ms = detail::us_to_ms(phase_budget_us);
            detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::PhaseStart, rt);
        }

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
                rt.active_count = active;
                rt.next_active_count = next_active;
                rt.state_order = state_order;
                rt.state_id = id;
                rt.phase_weight = phase_weight;
                rt.temp_r0 = temp_r0;
                rt.temp_r1 = temp_r1;
                rt.start_temp = phase_start_temp;
                rt.end_temp = phase_end_temp;
                rt.phase_time_limit_ms = detail::us_to_ms(phase_budget_us);
                rt.inner_time_limit_ms = slice_ms;
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
                inner_param.seed = detail::sa_pop_mix_seed(param.seed, static_cast<uint64_t>(phase), static_cast<uint64_t>(id), 0x51E1EC710FULL);
                inner_param.auto_mode = false;
                inner_param.samples = 0;
                inner_param.start_temp = phase_start_temp;
                inner_param.end_temp = phase_end_temp;
                inner_param.enable_end_cost_check = false; // sa_pop 内部の短時間SAでは LOCAL 時の終了整合性チェックを抑制する

                auto inner_hook = [&](EventType event_type, const SaRuntime<Cost>& inner_rt) {
#ifdef LOCAL
                    if (event_type == EventType::RunEnd) {
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
                    []() -> SaPopEmptyBest { return SaPopEmptyBest{}; },
                    [&]() -> Cost { return detail::call_sa_pop_get_cost<Cost>(get_cost, states[id]); },
                    [&](const SaRuntime<Cost>& inner_rt) -> Cost { return detail::call_sa_pop_propose<Cost>(propose, states[id], inner_rt); },
                    [&](bool accepted) { detail::call_sa_pop_finalize(finalize, states[id], accepted); },
                    inner_hook);
                (void)ignored_best;
                run_best_cost = best_cost;
                if (!has_inner_runtime) {
                    inner_end_runtime.best_cost = best_cost;
                    inner_end_runtime.current_cost = detail::call_sa_pop_get_cost<Cost>(get_cost, states[id]);
                }
            }

            current_costs[id] = detail::call_sa_pop_get_cost<Cost>(get_cost, states[id]);
            lifetime_best_costs[id] = min(lifetime_best_costs[id], run_best_cost);
            selection_keys[id] = detail::sa_pop_selection_key(pop_param.selection_policy, pop_param.selection_current_weight, current_costs[id], lifetime_best_costs[id]);

            {
                SaPopRuntime<Cost> rt = rt_base;
                fill_common_runtime(rt);
                rt.phase = phase;
                rt.active_count = active;
                rt.next_active_count = next_active;
                rt.state_order = state_order;
                rt.state_id = id;
                rt.phase_weight = phase_weight;
                rt.temp_r0 = temp_r0;
                rt.temp_r1 = temp_r1;
                rt.start_temp = phase_start_temp;
                rt.end_temp = phase_end_temp;
                rt.phase_time_limit_ms = detail::us_to_ms(phase_budget_us);
                rt.inner_time_limit_ms = slice_ms;
                rt.inner_elapsed_us = inner_end_runtime.elapsed_us;
                rt.current_cost_before = current_before;
                rt.current_cost_after = current_costs[id];
                rt.run_best_cost = run_best_cost;
                rt.lifetime_best_cost_before = lifetime_before;
                rt.lifetime_best_cost_after = lifetime_best_costs[id];
                rt.selection_key_cost = selection_keys[id];
                rt.inner = inner_end_runtime;
                detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::StateRunEnd, rt);
            }
        }

        for (int j = 0; j < active; ++j) order[j] = active_ids[j];
        sort(order.begin(), order.begin() + active, [&](uint16_t x, uint16_t y) {
            const int a = static_cast<int>(x);
            const int b = static_cast<int>(y);
            if (selection_keys[a] != selection_keys[b]) return selection_keys[a] < selection_keys[b];
            if (lifetime_best_costs[a] != lifetime_best_costs[b]) return lifetime_best_costs[a] < lifetime_best_costs[b];
            if (current_costs[a] != current_costs[b]) return current_costs[a] < current_costs[b];
            return a < b;
        });

        const double cutoff_cost = selection_keys[order[next_active - 1]];
        for (int rank = 0; rank < active; ++rank) {
            const int id = order[rank];
            SaPopRuntime<Cost> rt = rt_base;
            fill_common_runtime(rt);
            rt.phase = phase;
            rt.active_count = active;
            rt.next_active_count = next_active;
            rt.state_id = id;
            rt.rank = rank;
            rt.survived = (rank < next_active);
            rt.phase_weight = phase_weight;
            rt.temp_r0 = temp_r0;
            rt.temp_r1 = temp_r1;
            rt.start_temp = phase_start_temp;
            rt.end_temp = phase_end_temp;
            rt.phase_time_limit_ms = detail::us_to_ms(phase_budget_us);
            rt.current_cost_after = current_costs[id];
            rt.lifetime_best_cost_after = lifetime_best_costs[id];
            rt.selection_key_cost = selection_keys[id];
            rt.selection_cutoff_cost = cutoff_cost;
            detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::StateSelection, rt);
        }

        for (int j = 0; j < next_active; ++j) active_ids[j] = order[j];
        active = next_active;

        {
            SaPopRuntime<Cost> rt = rt_base;
            fill_common_runtime(rt);
            rt.phase = phase;
            rt.active_count = active_counts[phase];
            rt.next_active_count = next_active;
            rt.phase_weight = phase_weight;
            rt.temp_r0 = temp_r0;
            rt.temp_r1 = temp_r1;
            rt.start_temp = phase_start_temp;
            rt.end_temp = phase_end_temp;
            rt.phase_time_limit_ms = detail::us_to_ms(phase_budget_us);
            rt.phase_elapsed_us = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - phase_start_clock).count();
            rt.selection_cutoff_cost = cutoff_cost;
            detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::PhaseEnd, rt);
        }
    }

    const int final_state_id = active_ids[0];
    const int64_t elapsed_before_final_us = elapsed_us_now();
    const int64_t final_budget_us = max<int64_t>(0, total_us - elapsed_before_final_us);
    const double final_budget_ms = detail::us_to_ms(final_budget_us);
    const double final_r0 = (K == 1 ? 0.0 : pop_param.selection_ratio);
    const double final_r1 = 1.0;
    const double final_start_temp = temperature_at(final_r0);
    const double final_end_temp = temperature_at(final_r1);

    {
        SaPopRuntime<Cost> rt = rt_base;
        fill_common_runtime(rt);
        rt.final_polish = true;
        rt.final_state_id = final_state_id;
        rt.state_id = final_state_id;
        rt.active_count = 1;
        rt.next_active_count = 1;
        rt.final_time_limit_ms = final_budget_ms;
        rt.inner_time_limit_ms = final_budget_ms;
        rt.temp_r0 = final_r0;
        rt.temp_r1 = final_r1;
        rt.start_temp = final_start_temp;
        rt.end_temp = final_end_temp;
        rt.final_start_temp = final_start_temp;
        rt.final_end_temp = final_end_temp;
        rt.current_cost_before = current_costs[final_state_id];
        rt.lifetime_best_cost_before = lifetime_best_costs[final_state_id];
        detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::FinalRunStart, rt);
    }

    SaParam final_param = param;
    final_param.seed = detail::sa_pop_mix_seed(param.seed, 0xF17A1ULL, static_cast<uint64_t>(final_state_id), 0xA11CEULL);
    final_param.auto_mode = false;
    final_param.samples = 0;
    final_param.start_temp = final_start_temp;
    final_param.end_temp = final_end_temp;
    final_param.enable_end_cost_check = false; // sa_pop 内部のfinal SAでも LOCAL 時の終了整合性チェックを抑制する

    SaRuntime<Cost> final_inner_runtime;
    final_inner_runtime.current_cost = current_costs[final_state_id];
    final_inner_runtime.best_cost = current_costs[final_state_id];
    bool has_final_runtime = false;

    auto final_inner_hook = [&](EventType event_type, const SaRuntime<Cost>& inner_rt) {
#ifdef LOCAL
        if (event_type == EventType::RunEnd) {
            final_inner_runtime = inner_rt;
            has_final_runtime = true;
        }
#else
        (void)event_type;
        (void)inner_rt;
#endif
    };

    auto [final_best_cost, final_best_snapshot] = sa<Snapshot, Cost>(
        final_param,
        final_budget_ms,
        [&]() -> Snapshot { return detail::call_sa_pop_get_snapshot<Snapshot>(get_snapshot, states[final_state_id]); },
        [&]() -> Cost { return detail::call_sa_pop_get_cost<Cost>(get_cost, states[final_state_id]); },
        [&](const SaRuntime<Cost>& inner_rt) -> Cost { return detail::call_sa_pop_propose<Cost>(propose, states[final_state_id], inner_rt); },
        [&](bool accepted) { detail::call_sa_pop_finalize(finalize, states[final_state_id], accepted); },
        final_inner_hook);

    if (!has_final_runtime) {
        final_inner_runtime.best_cost = final_best_cost;
        final_inner_runtime.current_cost = detail::call_sa_pop_get_cost<Cost>(get_cost, states[final_state_id]);
    }

    {
        SaPopRuntime<Cost> rt = rt_base;
        fill_common_runtime(rt);
        rt.final_polish = true;
        rt.final_state_id = final_state_id;
        rt.state_id = final_state_id;
        rt.active_count = 1;
        rt.next_active_count = 1;
        rt.final_time_limit_ms = final_budget_ms;
        rt.inner_time_limit_ms = final_budget_ms;
        rt.inner_elapsed_us = final_inner_runtime.elapsed_us;
        rt.temp_r0 = final_r0;
        rt.temp_r1 = final_r1;
        rt.start_temp = final_start_temp;
        rt.end_temp = final_end_temp;
        rt.final_start_temp = final_start_temp;
        rt.final_end_temp = final_end_temp;
        rt.current_cost_before = current_costs[final_state_id];
        rt.current_cost_after = final_inner_runtime.current_cost;
        rt.run_best_cost = final_best_cost;
        rt.lifetime_best_cost_before = lifetime_best_costs[final_state_id];
        rt.lifetime_best_cost_after = min(lifetime_best_costs[final_state_id], final_best_cost);
        rt.final_best_cost = final_best_cost;
        rt.inner = final_inner_runtime;
        detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::FinalRunEnd, rt);
    }

    {
        SaPopRuntime<Cost> rt = rt_base;
        fill_common_runtime(rt);
        rt.total_elapsed_us = elapsed_us_now();
        rt.final_state_id = final_state_id;
        rt.final_best_cost = final_best_cost;
        rt.final_start_temp = final_start_temp;
        rt.final_end_temp = final_end_temp;
        rt.final_time_limit_ms = final_budget_ms;
        rt.inner = final_inner_runtime;
        detail::call_sa_pop_debug_hook(debug_hook, SaPopEventType::RunEnd, rt);
    }

    return {final_best_cost, move(final_best_snapshot)};
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
            total_iterations += runtime.inner.iteration;
            accepted += runtime.inner.accepted_count;
        } else if (event_type == sa::SaPopEventType::StateSelection) {
            ++state_selection_count;
        } else if (event_type == sa::SaPopEventType::PhaseEnd) {
            ++phase_end_count;
        } else if (event_type == sa::SaPopEventType::FinalRunEnd) {
            final_state_id = runtime.final_state_id;
            total_iterations += runtime.inner.iteration;
            accepted += runtime.inner.accepted_count;
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
    void operator()(sa::EventType event_type, const sa::SaRuntime<Cost>& runtime) {
        if (event_type == sa::EventType::RunEnd) {
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
        vector<TestState> states = {{10}, {3}, {7}};
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 1.0;
        param.end_temp = 1.0;
        auto [cost, snapshot] = sa_pop<TestState, int, int>(
            param,
            0.0,
            move(states),
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
        vector<TestState> states = {{30}};
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 1.0;
        param.end_temp = 1.0;
        auto [cost, snapshot] = sa_pop<TestState, int, int>(
            param,
            4.0,
            move(states),
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
        vector<LifetimeState> states = {{0, 0, 0, 0}, {1, -50, 0, 0}};
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 1e12;
        param.end_temp = 1e12;
        SaPopParam pop_param;
        pop_param.selection_ratio = 1.0;
        auto [cost, snapshot_id] = sa_pop<LifetimeState, int, int>(
            param,
            3.0,
            move(states),
            [](const LifetimeState& s) { return s.value; },
            [](const LifetimeState& s) { return s.id; },
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
        check(snapshot_id == 0, "default combination selection keeps lifetime-best potential with current penalty");
    }

    {
        vector<TestState> states;
        for (int i = 0; i < 8; ++i) states.push_back({100 + i});
        SaParam param;
        param.auto_mode = true;
        param.samples = 32;
        param.start_temp = 10.0;
        param.end_temp = 0.1;
        SaPopParam pop_param;
        pop_param.selection_ratio = 1.0;
        auto [cost, snapshot] = sa_pop<TestState, int, int>(
            param,
            5.0,
            move(states),
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
        check(cost <= snapshot, "selection_ratio=1 with auto mode completes");
    }

#ifdef LOCAL
    {
        vector<TestState> states = {{20}, {25}, {30}, {35}};
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 2.0;
        param.end_temp = 0.2;
        SaPopCsvHook<int> hook("sa_pop_test_");
        auto [cost, snapshot] = sa_pop<TestState, int, int>(
            param,
            3.0,
            move(states),
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
        check(trace.good() && phase.good() && summary.good(), "SaPopCsvHook writes 3 CSV files at RunEnd");
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
    vector<TspState> states;
    states.reserve(K);
    double initial_sum = 0.0;
    double initial_best = numeric_limits<double>::infinity();
    for (int i = 0; i < K; ++i) {
        states.push_back(make_random_tsp_state(points, seed + i * 1009ULL));
        initial_sum += states.back().cost;
        initial_best = min(initial_best, states.back().cost);
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
    pop_param.selection_ratio = 0.75;

    long long total_candidates = 0;
    SaPopTestStatsHook hook;
    const auto st = chrono::steady_clock::now();
    auto [best_cost, best_tour] = sa::sa_pop<TspState, double, vector<int>>(
        param,
        time_limit_ms,
        move(states),
        [](const TspState& s) { return s.cost; },
        [](const TspState& s) { return s.tour; },
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
    res.phase_count = (K <= 1 ? 0 : static_cast<int>(bit_width(static_cast<unsigned>(K - 1))));
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
    vector<TspState> states;
    states.reserve(K);
    double initial_sum = 0.0;
    double initial_best = numeric_limits<double>::infinity();
    for (int i = 0; i < K; ++i) {
        states.push_back(make_random_tsp_state(points, seed + i * 1009ULL));
        initial_sum += states.back().cost;
        initial_best = min(initial_best, states.back().cost);
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
            [&]() { return states[i].tour; },
            [&]() { return states[i].cost; },
            [&](const sa::SaRuntime<double>& rt) {
                ++total_candidates;
                return states[i].propose_2opt(rt);
            },
            [&](bool accepted) { states[i].finalize(accepted); },
            std::ref(hook));
        total_iters += hook.iterations;
        total_accepted += hook.accepted;
        if (cost < best_cost) {
            best_cost = cost;
            best_tour = move(tour);
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

int main() {
    run_basic_tests();
    run_tsp_comparison();
    return 0;
}

#endif
