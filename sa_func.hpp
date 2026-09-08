#pragma once

#include <bits/stdc++.h>
using namespace std;

namespace sa {

// 数値型制約
// Cost は符号付き整数または浮動小数を想定する。
template<class T>
concept Numeric = integral<T> || floating_point<T>;

// 内部実装詳細
// 競技用に軽量な乱数や補助関数をまとめる。
namespace detail {

struct FastRng {
    uint64_t state;

    explicit FastRng(uint64_t seed = 1) : state(seed) {}

    inline uint64_t operator()() {
        uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    inline double rand_unit() {
        return static_cast<double>((*this)() >> 11) * (1.0 / 9007199254740992.0);
    }
};

inline int64_t steady_elapsed_us(const chrono::steady_clock::time_point& start_clock) {
    return chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - start_clock).count();
}

template<Numeric Cost>
inline double to_double(Cost x) {
    return static_cast<double>(x);
}

inline double exp_temperature(double start_temp, double end_temp, double ratio) {
    ratio = clamp(ratio, 0.0, 1.0);
    return start_temp * exp(log(end_temp / start_temp) * ratio);
}

inline double temperature_from_avg_delta(double avg_worse_delta, double accept_prob) {
    return -avg_worse_delta / log(accept_prob);
}

inline double us_to_ms(int64_t us) {
    return static_cast<double>(us) * 0.001;
}

#ifdef LOCAL
inline constexpr bool debug_hook_enabled = true;
#else
inline constexpr bool debug_hook_enabled = false;
#endif

}  // namespace detail

// SA のパラメータ
// 必須最小限に絞り、温度決定に必要な項目だけを持つ。
struct SaParam {
    uint64_t seed = 1;               // 乱数 seed
    bool auto_mode = true;           // true: 事前サンプリングから温度自動推定
    int samples = 300;               // auto_mode 時の提案回数
    double start_accept_prob = 0.8;  // auto_mode 時の初期目標受理率
    double end_accept_prob = 0.01;   // auto_mode 時の最終目標受理率
    double start_temp = 100.0;       // manual 時、および自動推定失敗時の初期温度
    double end_temp = 1.0;           // manual 時、および自動推定失敗時の最終温度
    bool enable_end_cost_check = true; // LOCAL 時の終了時 cost 整合性チェック有効化フラグ
};

// 実行時情報
// Hook 側から現在の進行状況を観測するための軽量構造体。
template<Numeric Cost>
struct SaRuntime {
    int iteration = 0;                    // 現在のイテレーション（1始まり、RunStart 時は 0）
    double temperature = 0.0;            // 現在温度

    Cost current_cost{};                 // 現在コスト
    Cost best_cost{};                    // 最良コスト

    int best_update_count = 0;           // best_cost 更新回数
    int last_best_update_iter = 0;       // 最後に best_cost を更新した iteration

    int accepted_count = 0;              // 受理回数
    int worse_accepted_count = 0;        // 悪化受理回数

    int64_t elapsed_us = 0;              // 開始からの経過時間（μs）
    int64_t elapsed_us_at_last_best = 0; // 最後に best 更新された時刻（μs）

    bool best_updated_this_iter = false;   // 今回 best 更新されたか
    bool accepted_this_iter = false;       // 今回 accept されたか
    bool worse_accepted_this_iter = false; // 今回悪化受理されたか

    int64_t elapsed_us_since_last_best() const {
        return elapsed_us - elapsed_us_at_last_best;
    }
};

// Hook の発火タイミング
enum class EventType {
    RunStart,
    RunEnd,
    IterationStart,
    IterationEnd,
};

// デフォルト Hook
// 非 LOCAL では Hook 呼び出し自体をコンパイル時に消すため、
// 何もしない型を既定値として用意する。
struct NoOp {
    template<Numeric Cost>
    void operator()(EventType, const SaRuntime<Cost>&) const noexcept {}
};

namespace detail {

// Propose 呼び出し補助
// () -> Cost と (const SaRuntime<Cost>&) -> Cost の両方を受ける。
template<Numeric Cost, class Propose>
inline Cost call_propose(Propose& propose, const SaRuntime<Cost>& runtime) {
    if constexpr (is_invocable_r_v<Cost, Propose, const SaRuntime<Cost>&>) {
        return propose(runtime);
    } else {
        return propose();
    }
}

// Hook 呼び出し補助
// LOCAL でない場合は完全に空にしてデバッグ機能を消す。
template<class DebugHook, Numeric Cost>
inline void call_debug_hook(DebugHook& debug_hook, EventType event_type, const SaRuntime<Cost>& runtime) {
#ifdef LOCAL
    debug_hook(event_type, runtime);
#else
    (void)debug_hook;
    (void)event_type;
    (void)runtime;
#endif
}

}  // namespace detail

// CSV 出力 Hook
// SA 本体とは独立した外部機能として、Run/Iteration の Hook だけで
// 定期スナップショットと差分統計を最後に CSV 出力する。
template<Numeric Cost>
struct CsvStatHook {
    struct Row {
        double elapsed_ms = 0.0;
        int iteration = 0;
        double temperature = 0.0;
        Cost current_cost{};
        Cost best_cost{};
        int accepted_count = 0;
        int worse_accepted_count = 0;
        int best_update_count = 0;

        double period_ms = 0.0;
        int period_iterations = 0;
        int period_accepted = 0;
        int period_worse_accepted = 0;
        int period_best_updates = 0;
        double accept_rate = 0.0;
        double worse_accept_rate = 0.0;
        double iter_per_sec = 0.0;
    };

    string filename;
    int64_t interval_us;
    vector<Row> rows;
    int64_t next_record_us = 0;
    bool started = false;

    explicit CsvStatHook(string csv_filename = "sa_stat.csv", double interval_ms = 50.0)
        : filename(move(csv_filename)),
          interval_us(max<int64_t>(1, static_cast<int64_t>(llround(interval_ms * 1000.0)))) {}

    void operator()(EventType event_type, const SaRuntime<Cost>& runtime) {
        if (event_type == EventType::RunStart) {
            rows.clear();
            started = true;
            next_record_us = runtime.elapsed_us + interval_us;
            push_row(runtime);
            return;
        }

        if (!started) return;

        if (event_type == EventType::IterationEnd) {
            if (runtime.elapsed_us >= next_record_us) {
                push_row(runtime);
                next_record_us = runtime.elapsed_us + interval_us;
            }
            return;
        }

        if (event_type == EventType::RunEnd) {
            if (rows.empty() || rows.back().iteration != runtime.iteration ||
                fabs(rows.back().elapsed_ms - detail::us_to_ms(runtime.elapsed_us)) > 1e-9) {
                push_row(runtime);
            }
            write_csv();
        }
    }

private:
    void push_row(const SaRuntime<Cost>& runtime) {
        Row row;
        row.elapsed_ms = detail::us_to_ms(runtime.elapsed_us);
        row.iteration = runtime.iteration;
        row.temperature = runtime.temperature;
        row.current_cost = runtime.current_cost;
        row.best_cost = runtime.best_cost;
        row.accepted_count = runtime.accepted_count;
        row.worse_accepted_count = runtime.worse_accepted_count;
        row.best_update_count = runtime.best_update_count;

        if (!rows.empty()) {
            const Row& prev = rows.back();
            row.period_ms = row.elapsed_ms - prev.elapsed_ms;
            row.period_iterations = row.iteration - prev.iteration;
            row.period_accepted = row.accepted_count - prev.accepted_count;
            row.period_worse_accepted = row.worse_accepted_count - prev.worse_accepted_count;
            row.period_best_updates = row.best_update_count - prev.best_update_count;
            if (row.period_iterations > 0) {
                row.accept_rate = static_cast<double>(row.period_accepted) / row.period_iterations;
                row.worse_accept_rate = static_cast<double>(row.period_worse_accepted) / row.period_iterations;
            }
            if (row.period_ms > 0.0) {
                row.iter_per_sec = row.period_iterations * 1000.0 / row.period_ms;
            }
        }

        rows.push_back(row);
    }

    void write_csv() const {
        ofstream ofs(filename);
        if (!ofs) {
            cerr << "[sa] warning: failed to open csv file: " << filename << '\n';
            return;
        }
        ofs << "elapsed_ms,iteration,temperature,current_cost,best_cost,accepted_count,worse_accepted_count,best_update_count,";
        ofs << "period_ms,period_iterations,period_accepted,period_worse_accepted,period_best_updates,accept_rate,worse_accept_rate,iter_per_sec\n";
        ofs << fixed << setprecision(6);
        for (const Row& row : rows) {
            ofs << row.elapsed_ms << ','
                << row.iteration << ','
                << row.temperature << ','
                << row.current_cost << ','
                << row.best_cost << ','
                << row.accepted_count << ','
                << row.worse_accepted_count << ','
                << row.best_update_count << ','
                << row.period_ms << ','
                << row.period_iterations << ','
                << row.period_accepted << ','
                << row.period_worse_accepted << ','
                << row.period_best_updates << ','
                << row.accept_rate << ','
                << row.worse_accept_rate << ','
                << row.iter_per_sec << '\n';
        }
    }
};

// 焼きなまし本体
// 状態本体は持たず、現在状態の管理はユーザーに委ねる。
// ライブラリ側は最良コストと最良状態コピーのみ保持する。
template<
    class State,
    Numeric Cost,
    class GetState,
    class GetCost,
    class Propose,
    class Finalize,
    class DebugHook = NoOp>
pair<Cost, State> sa(
    const SaParam& param,
    double time_limit_ms,
    GetState get_state,
    GetCost get_cost,
    Propose propose,
    Finalize finalize,
    DebugHook debug_hook = DebugHook{}) {
    static_assert(copy_constructible<State>, "State must be copy constructible");
    static_assert(is_invocable_r_v<State, GetState>, "GetState must be callable as State()");
    static_assert(is_invocable_r_v<Cost, GetCost>, "GetCost must be callable as Cost()");
    static_assert(is_invocable_v<Finalize, bool>, "Finalize must be callable as void(bool)");
    static_assert(is_invocable_r_v<Cost, Propose> || is_invocable_r_v<Cost, Propose, const SaRuntime<Cost>&>, "Propose must be callable as Cost() or Cost(const SaRuntime<Cost>&)");
    if constexpr (integral<Cost>) static_assert(is_signed_v<Cost>, "Signed integral Cost is required because delta can be negative");

    assert((param.samples >= 0) && "sa::SaParam.samples must be >= 0");
    assert((0.0 < param.start_accept_prob && param.start_accept_prob < 1.0) && "sa::SaParam.start_accept_prob must satisfy 0 < p < 1");
    assert((0.0 < param.end_accept_prob && param.end_accept_prob < 1.0) && "sa::SaParam.end_accept_prob must satisfy 0 < p < 1");
    assert((param.start_temp > 0.0) && "sa::SaParam.start_temp must be > 0");
    assert((param.end_temp > 0.0) && "sa::SaParam.end_temp must be > 0");

    detail::FastRng rng(param.seed);
    SaRuntime<Cost> runtime;
    runtime.current_cost = get_cost();
    runtime.best_cost = runtime.current_cost;
    State best_state = get_state();

    const auto start_clock = chrono::steady_clock::now();
    const auto elapsed_now_us = [&]() -> int64_t {
        return detail::steady_elapsed_us(start_clock);
    };
    assert(isfinite(time_limit_ms) && "time_limit_ms must be finite");
    const double limit_us_raw = max(0.0, time_limit_ms * 1000.0);
    const int64_t limit_us = static_cast<int64_t>(limit_us_raw);

    double start_temp = param.start_temp;
    double end_temp = param.end_temp;
    if (param.auto_mode && param.samples > 0 && limit_us > 0) {
        double worse_delta_sum = 0.0;
        int worse_count = 0;
        runtime.iteration = 0;
        runtime.temperature = param.start_temp;
        runtime.elapsed_us = 0;

        for (int sample_id = 0; sample_id < param.samples; ++sample_id) {
            if ((sample_id & 31) == 0 && elapsed_now_us() >= limit_us) break;
            const Cost delta = detail::call_propose<Cost>(propose, runtime);
            if (delta > Cost{0}) {
                worse_delta_sum += detail::to_double(delta);
                ++worse_count;
            }
            finalize(false);
        }

#ifdef LOCAL
        if (worse_count < 100) {
            cerr << "[sa] warning: auto temperature sampling collected only " << worse_count << " worsening moves.\n";
        }
#endif

        if (worse_count > 0) {
            const double avg_worse_delta = worse_delta_sum / static_cast<double>(worse_count);
            start_temp = detail::temperature_from_avg_delta(avg_worse_delta, param.start_accept_prob);
            end_temp = detail::temperature_from_avg_delta(avg_worse_delta, param.end_accept_prob);
        }
#ifdef LOCAL
        else {
            cerr << "[sa] warning: auto temperature sampling fallback to manual temperatures.\n";
        }
#endif
    }

    runtime.iteration = 0;
    runtime.temperature = start_temp;
    runtime.elapsed_us = min(elapsed_now_us(), limit_us);
    detail::call_debug_hook(debug_hook, EventType::RunStart, runtime);

    if (limit_us <= 0) {
        detail::call_debug_hook(debug_hook, EventType::RunEnd, runtime);
        return {runtime.best_cost, move(best_state)};
    }

    const double limit_us_d = static_cast<double>(limit_us);
    int64_t measured_elapsed_us = runtime.elapsed_us;
    while (true) {
        if ((runtime.iteration & 31) == 0) {
            measured_elapsed_us = elapsed_now_us();
            if (measured_elapsed_us >= limit_us) break;
        }

        runtime.elapsed_us = measured_elapsed_us;
        ++runtime.iteration;
        if (runtime.iteration == 1) {
            runtime.temperature = start_temp;
        } else {
            runtime.temperature = detail::exp_temperature(start_temp, end_temp, static_cast<double>(runtime.elapsed_us) / limit_us_d);
        }
        runtime.best_updated_this_iter = false;
        runtime.accepted_this_iter = false;
        runtime.worse_accepted_this_iter = false;

        detail::call_debug_hook(debug_hook, EventType::IterationStart, runtime);

        const Cost delta = detail::call_propose<Cost>(propose, runtime);

        bool accepted = false;
        if (delta <= Cost{0}) {
            accepted = true;
        } else {
            const double prob = exp(-detail::to_double(delta) / runtime.temperature);
            accepted = (rng.rand_unit() < prob);
        }

        finalize(accepted);

        if (accepted) {
            runtime.accepted_this_iter = true;
            ++runtime.accepted_count;
            runtime.current_cost += delta;

            if (delta > Cost{0}) {
                runtime.worse_accepted_this_iter = true;
                ++runtime.worse_accepted_count;
            }

            if (runtime.current_cost < runtime.best_cost) {
                runtime.best_updated_this_iter = true;
                runtime.best_cost = runtime.current_cost;
                best_state = get_state();
                ++runtime.best_update_count;
                runtime.last_best_update_iter = runtime.iteration;
                runtime.elapsed_us_at_last_best = runtime.elapsed_us;
            }
        }

        detail::call_debug_hook(debug_hook, EventType::IterationEnd, runtime);
    }

    runtime.elapsed_us = elapsed_now_us();
    runtime.temperature = detail::exp_temperature(start_temp, end_temp, static_cast<double>(min(runtime.elapsed_us, limit_us)) / limit_us_d);
    detail::call_debug_hook(debug_hook, EventType::RunEnd, runtime);

#ifdef LOCAL
    if (param.enable_end_cost_check) {
        const Cost checked_cost = get_cost();
        cerr << setprecision(15);
        cerr << "[sa] end cost check: current_cost=" << runtime.current_cost
             << ", get_cost()=" << checked_cost
             << ", diff=" << (detail::to_double(checked_cost) - detail::to_double(runtime.current_cost))
             << '\n';
    }
#endif

    return {runtime.best_cost, move(best_state)};
}


}  // namespace sa


#if __INCLUDE_LEVEL__ == 0
#include "tsp_problem.hpp"

// テストコード
// ヘッダ単体実行時のみ有効にし、通常 include では無効化する。
int main() {
    using namespace sa;

    auto fail = [&](const string& message) {
        cerr << "[test] failed: " << message << '\n';
        exit(1);
    };
    auto check = [&](bool cond, const string& message) {
        if (!cond) fail(message);
        cout << "[test] ok: " << message << '\n';
    };
    auto check_near = [&](double a, double b, double eps, const string& message) {
        if (fabs(a - b) > eps) {
            cerr << setprecision(15);
            cerr << "[test] near check failed: " << message << " a=" << a << " b=" << b << " eps=" << eps << '\n';
            exit(1);
        }
        cout << "[test] ok: " << message << '\n';
    };

    // 0ms では反復せず、初期解がそのまま返ることを確認する。
    {
        int state = 42;
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 10.0;
        param.end_temp = 1.0;
        auto [best_cost, best_state] = sa<int, int>(
            param,
            0,
            [&]() { return state; },
            [&]() { return state; },
            [&]() { return -1; },
            [&](bool accepted) {
                if (accepted) --state;
            });
        check(best_cost == 42, "time_limit_ms = 0 returns initial cost");
        check(best_state == 42, "time_limit_ms = 0 returns initial state");
        check(state == 42, "time_limit_ms = 0 does not mutate state");
    }

    // Propose 時に仮適用し、reject なら Finalize(false) で戻す流儀を確認する。
    // 温度を極小にして悪化手を必ず reject させる。
    {
        int state = 0;
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 1e-12;
        param.end_temp = 1e-12;
        auto [best_cost, best_state] = sa<int, int>(
            param,
            5,
            [&]() { return state; },
            [&]() { return state; },
            [&]() {
                ++state;
                return 1;
            },
            [&](bool accepted) {
                if (!accepted) --state;
            });
        check(best_cost == 0, "rejected worsening move keeps best cost");
        check(best_state == 0, "rejected worsening move keeps best state");
        check(state == 0, "Finalize(false) restores original state");
    }

    // Propose 時にはまだ適用せず、accept なら Finalize(true) で反映する流儀を確認する。
    // 常に改善手を返して、best_state が最終状態に追従することを確認する。
    {
        int state = 50;
        int pending = 0;
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 1.0;
        param.end_temp = 1.0;
        auto [best_cost, best_state] = sa<int, int>(
            param,
            5,
            [&]() { return state; },
            [&]() { return state; },
            [&]() {
                pending = -1;
                return pending;
            },
            [&](bool accepted) {
                if (accepted) state += pending;
                pending = 0;
            });
        check(best_cost == state, "accept-apply style keeps best cost equal to current at end");
        check(best_state == state, "accept-apply style stores latest best state");
        check(best_cost < 50, "always-improving moves actually improve cost");
    }

    // 同値手 delta=0 も accept 扱いになり、状態が壊れないことを確認する。
    {
        int state = 7;
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 10.0;
        param.end_temp = 10.0;
        auto [best_cost, best_state] = sa<int, int>(
            param,
            3,
            [&]() { return state; },
            [&]() { return state; },
            [&]() { return 0; },
            [&](bool accepted) {
                if (!accepted) fail("delta=0 must be accepted");
            });
        check(best_cost == 7, "zero-delta move does not change best cost");
        check(best_state == 7, "zero-delta move does not change best state");
        check(state == 7, "zero-delta move does not corrupt state");
    }

    // 浮動小数コストでも動作することを確認する。
    {
        double state = 10.0;
        double pending = 0.0;
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 1.0;
        param.end_temp = 1.0;
        auto [best_cost, best_state] = sa<double, double>(
            param,
            4,
            [&]() { return state; },
            [&]() { return state; },
            [&](const SaRuntime<double>&) {
                pending = -0.25;
                return pending;
            },
            [&](bool accepted) {
                if (accepted) state += pending;
                pending = 0.0;
            });
        check_near(best_cost, state, 1e-9, "floating-point best cost matches current state");
        check_near(best_state, state, 1e-9, "floating-point best state matches current state");
    }

    // auto_mode 成功時、sampling で集めた悪化量から温度が推定されることを確認する。
    // sampling 中は iteration=0 を使うので、それを見て挙動を分ける。
    {
        int state = 0;
        double first_temperature = -1.0;
        SaParam param;
        param.auto_mode = true;
        param.samples = 120;
        param.start_accept_prob = 0.8;
        param.end_accept_prob = 0.01;
        param.start_temp = 1000.0;
        param.end_temp = 100.0;

        auto [best_cost, best_state] = sa<int, int>(
            param,
            3,
            [&]() { return state; },
            [&]() { return state; },
            [&](const SaRuntime<int>& runtime) {
                if (runtime.iteration == 0) {
                    ++state;
                    return 5;
                }
                if (first_temperature < 0.0) first_temperature = runtime.temperature;
                return 0;
            },
            [&](bool accepted) {
                if (!accepted) state = 0;
            });

        const double expected_start_temp = -5.0 / log(0.8);
        check_near(first_temperature, expected_start_temp, 1.0, "auto temperature estimation uses sampled worsening delta");
        check(best_cost == 0 && best_state == 0, "auto temperature success case keeps state valid");
    }

    // auto_mode で悪化手が 0 個なら manual 温度へフォールバックすることを確認する。
    {
        int state = 0;
        double first_temperature = -1.0;
        SaParam param;
        param.auto_mode = true;
        param.samples = 64;
        param.start_temp = 123.0;
        param.end_temp = 7.0;

        auto [best_cost, best_state] = sa<int, int>(
            param,
            3,
            [&]() { return state; },
            [&]() { return state; },
            [&](const SaRuntime<int>& runtime) {
                if (runtime.iteration == 0) return -1;
                if (first_temperature < 0.0) first_temperature = runtime.temperature;
                return 0;
            },
            [&](bool) {});

        check_near(first_temperature, 123.0, 1e-6, "auto temperature fallback uses manual start_temp");
        check(best_cost == 0 && best_state == 0, "auto temperature fallback keeps initial best");
    }

    // CSV Hook がファイルを書き出すことと、主要ヘッダを含むことを確認する。
    {
        const string csv_filename = "sa_test_stat_v4.csv";
        filesystem::remove(csv_filename);

        int state = 10;
        int pending = 0;
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 1.0;
        param.end_temp = 1.0;

        CsvStatHook<int> csv_hook(csv_filename, 1.0);
        auto combo_hook = [&](EventType event_type, const SaRuntime<int>& runtime) {
            csv_hook(event_type, runtime);
        };

        auto [best_cost, best_state] = sa<int, int>(
            param,
            20,
            [&]() { return state; },
            [&]() { return state; },
            [&]() {
                pending = (state > 0 ? -1 : 0);
                return pending;
            },
            [&](bool accepted) {
                if (accepted) state += pending;
                pending = 0;
            },
            combo_hook);
        (void)best_cost;
        (void)best_state;

        if (detail::debug_hook_enabled) {
            ifstream ifs(csv_filename);
            check(static_cast<bool>(ifs), "csv hook writes output file");
            string header;
            getline(ifs, header);
            check(header.find("elapsed_ms") != string::npos, "csv header contains elapsed_ms");
            check(header.find("period_iterations") != string::npos, "csv header contains period stats");
        } else {
            check(true, "csv hook test is skipped without LOCAL");
        }
    }

    // TSP デモ
    // 問題定義を別ヘッダに切り出し、同じ 2-opt で改善する様子を確認する。
    {
        cout << "\n[tsp demo] start\n";

        const tsp_problem::Input input = tsp_problem::gen(20260422ULL);
        tsp_problem::Output current;
        current.route.resize(input.n);
        iota(current.route.begin(), current.route.end(), 0);

        tsp_problem::detail::FastRng problem_rng(20260422ULL);
        for (int i = 0; i < input.n; ++i) {
            (void)problem_rng();
            (void)problem_rng();
        }
        for (int i = input.n - 1; i > 0; --i) {
            const int j = static_cast<int>(problem_rng() % static_cast<uint64_t>(i + 1));
            swap(current.route[i], current.route[j]);
        }

        const long long initial_cost = tsp_problem::compute_score(input, current);
        int last_l = -1;
        int last_r = -1;

        SaParam param;
        param.seed = 123456789ULL;
        param.auto_mode = true;
        param.samples = 300;
        param.start_accept_prob = 0.8;
        param.end_accept_prob = 0.01;
        param.start_temp = 1000.0;
        param.end_temp = 1.0;

        CsvStatHook<long long> csv_hook("sa_tsp_stat_v5.csv", 50.0);
        int64_t next_print_us = 0;
        auto debug_hook = [&](EventType event_type, const SaRuntime<long long>& runtime) {
            csv_hook(event_type, runtime);
            if (event_type == EventType::IterationEnd && runtime.elapsed_us >= next_print_us) {
                cerr << "[tsp] elapsed_ms=" << detail::us_to_ms(runtime.elapsed_us)
                     << " iter=" << runtime.iteration
                     << " temp=" << runtime.temperature
                     << " current=" << runtime.current_cost
                     << " best=" << runtime.best_cost
                     << " accept=" << runtime.accepted_count
                     << " worse_accept=" << runtime.worse_accepted_count
                     << '\n';
                next_print_us = runtime.elapsed_us + 100000;
            }
            if (event_type == EventType::RunEnd) {
                cerr << "[tsp] finished: elapsed_ms=" << detail::us_to_ms(runtime.elapsed_us)
                     << " iter=" << runtime.iteration
                     << " best_updates=" << runtime.best_update_count
                     << " best=" << runtime.best_cost
                     << '\n';
            }
        };

        auto [best_cost, best_state] = sa<tsp_problem::Output, long long>(
            param,
            1000,
            [&]() { return current; },
            [&]() -> long long { return tsp_problem::compute_score(input, current); },
            [&](const SaRuntime<long long>&) -> long long {
                int l = static_cast<int>(problem_rng() % static_cast<uint64_t>(input.n));
                int r = static_cast<int>(problem_rng() % static_cast<uint64_t>(input.n));
                if (l > r) swap(l, r);
                if (l == r) {
                    r = (l + 1) % input.n;
                    if (l > r) swap(l, r);
                }
                if (l == 0 && r == input.n - 1) r = input.n - 2;

                last_l = l;
                last_r = r;

                const int a = current.route[(l - 1 + input.n) % input.n];
                const int b = current.route[l];
                const int c = current.route[r];
                const int d = current.route[(r + 1) % input.n];
                const long long delta = static_cast<long long>(input.dist[a][c]) + input.dist[b][d] - input.dist[a][b] - input.dist[c][d];
                reverse(current.route.begin() + l, current.route.begin() + r + 1);
                return delta;
            },
            [&](bool accepted) {
                if (!accepted) reverse(current.route.begin() + last_l, current.route.begin() + last_r + 1);
            },
            debug_hook);

        check(best_cost <= initial_cost, "TSP demo improves or keeps initial route");
        check(tsp_problem::compute_score(input, best_state) == best_cost, "best_state score matches best_cost");
        cout << "[tsp demo] initial_cost=" << initial_cost << '\n';
        cout << "[tsp demo] best_cost=" << best_cost << '\n';
        cout << "[tsp demo] best_state_size=" << best_state.route.size() << '\n';
        cout << "[tsp demo] done\n";
    }

    cout << "\nAll tests finished successfully.\n";

    // 追加ベンチマーク
    // 既存のテストや出力はそのまま残し、seed 0..10 の問題に対して
    // 同様の TSP サンプル solver を実行し、集計値を出力する。
    {
        auto solve_tsp_sample = [&](uint64_t problem_seed) -> pair<long long, tsp_problem::Output> {
            const tsp_problem::Input input = tsp_problem::gen(problem_seed);
            tsp_problem::Output current;
            current.route.resize(input.n);
            iota(current.route.begin(), current.route.end(), 0);

            tsp_problem::detail::FastRng problem_rng(problem_seed);
            for (int i = 0; i < input.n; ++i) {
                (void)problem_rng();
                (void)problem_rng();
            }
            for (int i = input.n - 1; i > 0; --i) {
                const int j = static_cast<int>(problem_rng() % static_cast<uint64_t>(i + 1));
                swap(current.route[i], current.route[j]);
            }

            int last_l = -1;
            int last_r = -1;

            SaParam param;
            param.seed = 123456789ULL;
            param.auto_mode = true;
            param.samples = 300;
            param.start_accept_prob = 0.8;
            param.end_accept_prob = 0.01;
            param.start_temp = 1000.0;
            param.end_temp = 1.0;

            auto [best_cost, best_state] = sa<tsp_problem::Output, long long>(
                param,
                1000,
                [&]() { return current; },
                [&]() -> long long { return tsp_problem::compute_score(input, current); },
                [&](const SaRuntime<long long>&) -> long long {
                    int l = static_cast<int>(problem_rng() % static_cast<uint64_t>(input.n));
                    int r = static_cast<int>(problem_rng() % static_cast<uint64_t>(input.n));
                    if (l > r) swap(l, r);
                    if (l == r) {
                        r = (l + 1) % input.n;
                        if (l > r) swap(l, r);
                    }
                    if (l == 0 && r == input.n - 1) r = input.n - 2;

                    last_l = l;
                    last_r = r;

                    const int a = current.route[(l - 1 + input.n) % input.n];
                    const int b = current.route[l];
                    const int c = current.route[r];
                    const int d = current.route[(r + 1) % input.n];
                    const long long delta = static_cast<long long>(input.dist[a][c]) + input.dist[b][d] - input.dist[a][b] - input.dist[c][d];
                    reverse(current.route.begin() + l, current.route.begin() + r + 1);
                    return delta;
                },
                [&](bool accepted) {
                    if (!accepted) reverse(current.route.begin() + last_l, current.route.begin() + last_r + 1);
                });
            return {best_cost, move(best_state)};
        };

        cout << "\n[tsp multi-seed] start\n";
        int error_count = 0;
        long long score_sum = 0;
        long long min_score = numeric_limits<long long>::max();
        long long max_score = numeric_limits<long long>::min();

        for (uint64_t seed = 0; seed <= 10; ++seed) {
            const tsp_problem::Input input = tsp_problem::gen(seed);
            auto [best_cost, best_state] = solve_tsp_sample(seed);
            const long long verified_score = tsp_problem::compute_score(input, best_state);
            const bool ok = (verified_score == best_cost);
            if (!ok) ++error_count;
            score_sum += best_cost;
            min_score = min(min_score, best_cost);
            max_score = max(max_score, best_cost);
            cout << "[tsp multi-seed] seed=" << seed
                 << " score=" << best_cost
                 << " verified=" << verified_score
                 << " ok=" << (ok ? 1 : 0)
                 << '\n';
        }

        const double average_score = static_cast<double>(score_sum) / 11.0;
        cout << fixed << setprecision(6);
        cout << "[tsp multi-seed] error_count=" << error_count << '\n';
        cout << "[tsp multi-seed] average=" << average_score << '\n';
        cout << "[tsp multi-seed] min=" << min_score << '\n';
        cout << "[tsp multi-seed] max=" << max_score << '\n';
    }

    return 0;
}

#endif
