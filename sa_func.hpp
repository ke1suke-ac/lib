// C++20 / コスト最小化の単独 SA。状態はユーザーが管理し、探索中の最良解を返す。
#pragma once

#include <bits/stdc++.h>
using namespace std;

namespace sa {

// sa / sa_pop の Cost には符号付き整数または浮動小数を使う（差分は負になり得る）。
template<class T>
concept Numeric = integral<T> || floating_point<T>;

// 内部実装。
namespace detail {

struct FastRng {
    uint64_t state;

    // 指定 seed で初期化する O(1)
    explicit FastRng(uint64_t seed = 1) : state(seed) {}

    // 64 bit の乱数を返す O(1)
    inline uint64_t operator()() {
        uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    // [0, 1) の乱数を返す O(1)
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
    if (ratio <= 0.0) return start_temp;
    if (ratio >= 1.0) return end_temp;
    return exp(lerp(log(start_temp), log(end_temp), ratio));
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

// まず既定値で実行。温度を手動指定するときは auto_mode=false と start/end_temp を設定する。
struct SaParam {
    uint64_t seed = 1;               // 受理判定用。近傍生成の乱数はユーザー側で用意する
    bool auto_mode = true;           // 初期状態で悪化量を採取して温度を推定。各提案は必ず取り消す
    int samples = 300;               // 採取する提案数（>=0）。0 は推定なし。sa_pop は個体数で割って切り上げ
    double start_accept_prob = 0.8;  // 推定に使う基準確率（0<p<1）。実際の受理率を保証する値ではない
    double end_accept_prob = 0.01;   // 終端の基準確率（0<p<1）。sa は推定温度をさらに 0.1 倍する
    double start_temp = 1000.0;      // 有限・正。手動/推定失敗時の開始温度、および事前採取中の温度
    double end_temp = 0.1;           // 有限・正。手動/推定失敗時の終了温度。通常は start_temp 以下にする
    bool enable_end_cost_check = true; // LOCAL: 終了時に get_cost() で再評価し差を stderr に表示。sa_pop は最終 SA のみ
    // 悪化候補の「提案後コスト - この SA の最良コスト」が倍率×温度未満なら確率判定へ進む。
    // 非負を指定。0 は悪化拒否、+inf は幅制限なし。sa_pop では SaPopParam の同名項目を使う。
    double acceptance_width_scale = 1.0;
    // LOCAL: 次回用の auto_mode/start_temp/end_temp を stderr に出力。今回の温度は変えない。
    // 悪化データなし・独自受理関数・幅 0 では推定不可。sa_pop での出力は内部 SA ごとの推奨値。
    bool enable_temperature_report = false;
};

// propose / DebugHook が参照する統計。コスト・回数はこの 1 回の SA 内の値。
template<Numeric Cost>
struct SaRuntime {
    int iteration = 0;                    // 本探索は 1 始まり。事前採取中と RunStart は 0
    double temperature = 0.0;            // 現在温度

    Cost current_cost{};                 // 現在コスト
    Cost best_cost{};                    // この 1 回の SA で到達した最良コスト

    int best_update_count = 0;           // この SA 内での best_cost 更新回数
    int last_best_update_iter = 0;       // 最後に best_cost を更新した iteration

    int accepted_count = 0;              // 改善・同値・悪化を合わせた受理回数
    int worse_accepted_count = 0;        // 悪化受理回数

    int64_t elapsed_us = 0;              // 開始からの経過 μs。本探索は 32 反復ごとに更新、事前採取中は 0
    int64_t elapsed_us_at_last_best = 0; // 最後に best 更新された時刻（μs）

    bool best_updated_this_iter = false;   // IterationEnd で今回の最良更新を確認できる
    bool accepted_this_iter = false;       // IterationEnd で今回の受理を確認できる
    bool worse_accepted_this_iter = false; // IterationEnd で今回の悪化受理を確認できる

    int64_t time_limit_us = 0;           // SA 開始時に一度だけ設定する時間予算（μs）

    // 最良更新からの経過 μs（未更新なら開始から）。時計を取得しない O(1)。
    int64_t elapsed_us_since_last_best() const {
        return elapsed_us - elapsed_us_at_last_best;
    }

    // 時間予算の消費率 [0,1]。予算<=0 は 1。既存の経過時刻を使う O(1)。
    // sa_pop では各内部 SA の進捗であり、全体の進捗ではない。
    double progress() const {
        return time_limit_us > 0
            ? clamp(static_cast<double>(elapsed_us) / static_cast<double>(time_limit_us), 0.0, 1.0)
            : 1.0;
    }
};

// DebugHook(event, runtime) の通知。sa からの呼び出しは LOCAL 時のみ。
enum class SaEventType {
    RunStart,       // 初期評価・保存・温度推定の後
    RunEnd,         // 探索終了後（終了時コスト検証・温度診断の前）
    IterationStart, // 本探索の提案前。今回の各フラグは false
    IterationEnd,   // finalize とコスト・統計更新の後
};

// 何もしない既定の DebugHook。sa / sa_pop の Hook を省略したときに使う。
struct NoOp {
    // イベントを何もせず受け取る O(1)
    template<class Event, class Runtime>
    void operator()(Event, const Runtime&) const noexcept {}
};

namespace detail {

// Propose 呼び出し補助
// () -> Cost と (const SaRuntime<Cost>&) -> Cost の両方を受ける。
template<Numeric Cost, class Propose>
inline Cost call_propose(Propose& propose, const SaRuntime<Cost>& runtime) {
    if constexpr (is_invocable_r_v<Cost, Propose&, const SaRuntime<Cost>&>) {
        return propose(runtime);
    } else {
        static_assert(is_invocable_r_v<Cost, Propose&>, "Propose must return Cost with no argument or const SaRuntime<Cost>&");
        return propose();
    }
}

// Hook 呼び出し補助
// LOCAL でない場合は完全に空にしてデバッグ機能を消す。
template<class DebugHook, Numeric Cost>
inline void call_debug_hook(DebugHook& debug_hook, SaEventType event_type, const SaRuntime<Cost>& runtime) {
#ifdef LOCAL
    debug_hook(event_type, runtime);
#else
    (void)debug_hook;
    (void)event_type;
    (void)runtime;
#endif
}

#ifdef LOCAL
// 事前サンプリングは含めず、本探索で提案された有限の悪化量を採否によらず集計する。
// 各 1/4 時間区間を定数メモリで保持する。温度の最適性や目標受理率は保証しない。
struct TemperatureReport {
    array<uint64_t, 4> count{};
    array<double, 4> mean{};
    uint64_t width_rejected = 0;

    void add(double delta, double progress) {
        if (!(delta > 0.0 && isfinite(delta))) return;
        const int phase = min(3, static_cast<int>(progress * 4));
        mean[phase] += (delta - mean[phase]) / static_cast<double>(++count[phase]);
    }

    void print(const SaParam& param, bool custom_acceptance) const {
        uint64_t total = 0;
        double average = 0.0;
        int first = -1, last = -1;
        for (int i = 0; i < 4; ++i) if (count[i]) {
            total += count[i];
            average += (mean[i] - average) * (static_cast<double>(count[i]) / total);
            if (first < 0) first = i;
            last = i;
        }
        // cerr の書式設定に依存せず、またその設定を変更せず、貼り付け可能な値を出す。
        ostringstream out;
        out << setprecision(17) << "[sa] temperature report: worsening=" << total << ", quarters=[";
        for (int i = 0; i < 4; ++i) out << (i ? "," : "") << count[i];
        out << "]";
        if (!custom_acceptance) out << ", width_rejected=" << width_rejected;
        out << '\n';
        if (!total || custom_acceptance || param.acceptance_width_scale == 0.0) {
            out << "[sa] temperature recommendation unavailable: "
                << (!total ? "no finite worsening moves in the search."
                    : custom_acceptance ? "custom sa_accept_worse is not supported."
                    : "acceptance_width_scale=0 disables worsening moves.") << '\n';
        } else {
            // 最初・最後に悪化を観測した区間を使う。100 標本未満なら全体平均で補う。
            const int begin = count[first] >= 100 ? first : -1;
            const int end = count[last] >= 100 ? last : -1;
            const double start_temp = temperature_from_avg_delta(begin < 0 ? average : mean[begin], param.start_accept_prob);
            const double end_temp = temperature_from_avg_delta(end < 0 ? average : mean[end], param.end_accept_prob) * 0.1;
            if (isfinite(start_temp) && start_temp > 0.0 && isfinite(end_temp) && end_temp > 0.0) {
                out << "[sa] suggested temperatures (heuristic; source quarter 1-4, 0=all): start="
                    << begin + 1 << ", end=" << end + 1 << '\n'
                    << "param.auto_mode = false;\nparam.start_temp = " << start_temp
                    << ";\nparam.end_temp = " << end_temp << ";\n";
                if (total < 100) out << "[sa] warning: fewer than 100 worsening samples; recommendation is uncertain.\n";
                if (end_temp > start_temp) out << "[sa] warning: suggested end_temp exceeds start_temp (heating schedule).\n";
            } else {
                out << "[sa] temperature recommendation unavailable: estimated temperatures are out of range.\n";
            }
        }
        cerr << out.str();
    }
};
#endif

}  // namespace detail

// DebugHook に渡す周期 CSV 記録。sa からは LOCAL 時だけ呼ばれ、RunEnd で一括保存する。
// Hook は値渡し。呼び出し元でも rows を読みたい場合は std::ref(hook) を渡す。
template<Numeric Cost>
struct SaCsvStatHook {
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
        double accept_rate = 0.0;       // 区間の受理回数 / 区間の全反復数
        double worse_accept_rate = 0.0; // 区間の悪化受理回数 / 区間の全反復数
        double iter_per_sec = 0.0;
    };

    string filename;
    int64_t interval_us;
    vector<Row> rows;
    int64_t next_record_us = 0;
    bool started = false;

    // 保存先（上書き）と記録間隔 ms。有限値を指定し、0 以下は 1μs とする。
    explicit SaCsvStatHook(string csv_filename = "sa_stat.csv", double interval_ms = 50.0)
        : filename(move(csv_filename)) {
        const double raw_us = max(0.0, interval_ms) * 1000.0;
        assert(isfinite(interval_ms) && raw_us < 0x1p63 && "CSV interval must fit int64_t microseconds");
        interval_us = max<int64_t>(1, llround(raw_us));
    }

    // RunStart で記録をリセットし、RunEnd で保存。通常は償却 O(1)、開始・終了は O(記録行数)。
    void operator()(SaEventType event_type, const SaRuntime<Cost>& runtime) {
        // 開始イベントで記録を初期化し、開始前のイベントは無視する
        if (event_type == SaEventType::RunStart) {
            rows.clear();
            started = true;
        } else if (!started) {
            return;
        }

        // 初回と記録間隔の到達時だけ保存し、次回時刻の整数あふれを防ぐ
        if (event_type == SaEventType::RunStart ||
            (event_type == SaEventType::IterationEnd && runtime.elapsed_us >= next_record_us)) {
            push_row(runtime);
            next_record_us = runtime.elapsed_us + min(interval_us, INT64_MAX - runtime.elapsed_us);
        } else if (event_type == SaEventType::RunEnd) {
            // 最終行の重複を避け、終了時だけファイルへ書き出す
            if (rows.empty() || rows.back().iteration != runtime.iteration ||
                rows.back().elapsed_ms != detail::us_to_ms(runtime.elapsed_us)) {
                push_row(runtime);
            }
            write_csv();
        }
    }

private:
    void push_row(const SaRuntime<Cost>& runtime) {
        // 現在の Runtime をスナップショットへ変換する
        Row row;
        row.elapsed_ms = detail::us_to_ms(runtime.elapsed_us);
        row.iteration = runtime.iteration;
        row.temperature = runtime.temperature;
        row.current_cost = runtime.current_cost;
        row.best_cost = runtime.best_cost;
        row.accepted_count = runtime.accepted_count;
        row.worse_accepted_count = runtime.worse_accepted_count;
        row.best_update_count = runtime.best_update_count;

        // 直前の記録との差から区間統計を計算する（受理率の分母は区間反復数）
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
        // ファイルを開き、出力先に問題があれば警告する
        ofstream ofs(filename);
        if (!ofs) {
            cerr << "[sa] warning: failed to open csv file: " << filename << '\n';
            return;
        }
        // スナップショットと区間統計を同じ行に出力する
        ofs << "elapsed_ms,iteration,temperature,current_cost,best_cost,accepted_count,worse_accepted_count,best_update_count,";
        ofs << "period_ms,period_iterations,period_accepted,period_worse_accepted,period_best_updates,accept_rate,worse_accept_rate,iter_per_sec\n";
        ofs << fixed << setprecision(6);
        for (const Row& row : rows) {
            ofs << row.elapsed_ms << ','
                << row.iteration << ','
                << row.temperature << ','
                << +row.current_cost << ','
                << +row.best_cost << ','
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
        // バッファを確実に書き出してから、容量不足などの書き込み失敗を確認する
        ofs.close();
        if (!ofs) cerr << "[sa] warning: failed to write csv file: " << filename << '\n';
    }
};

// best_cost はこの SA の最良値。外部の existing_best_cost とは合成しない。
template<class Snapshot, Numeric Cost>
struct SaResult {
    Cost best_cost;
    optional<Snapshot> best_snapshot; // 基準未指定なら必ず存在。指定時はそれを厳密に改善した場合だけ存在
};

// 単独 SA。time_limit_ms は有限の ms（小数可）。<=0 は初期評価・保存だけで返す。
// 初期評価・初期 Snapshot と終了処理は計時外。時間確認は 32 反復ごとなので余裕を取る。
// get_snapshot() -> Snapshot: 初期時と最良更新時に保存。独立した、移動構築可能な値を返す。
//   get_snapshot(const SaRuntime<Cost>&) も可。両方あれば引数なしを優先。
//   Runtime 付きではコストのみ更新済み、反復統計の後処理前。初期保存時はコスト以外が既定値。
// get_cost() -> Cost: 現在の絶対コスト。初期時と LOCAL の終了検証時に呼ぶ。
// propose([const SaRuntime<Cost>&]) -> Cost: 提案後 - 提案前の差分。両方あれば Runtime 付きを優先。
// finalize(bool accepted): 受理なら提案を確定、棄却なら提案前の状態にする。事前採取は必ず false。
// existing_best_cost: マルチスタートの全体ベスト等。これを厳密に改善する Snapshot だけ保存する。
//   省略時は必ず保存。保存基準は受理判定に影響しない。戻り値は SaResult を参照。
// DebugHook(event, runtime): LOCAL のみ通知。コールバックは値渡し（必要なら std::ref を使う）。
// 独自の propose.sa_accept_worse(runtime, delta, u) があれば悪化受理を全て委ねる。u は [0,1)。
// Cost の加算と int カウンタが範囲内に収まること。状態は終了時点のままなので、出力には Snapshot を使う。
// 計算量: O(S(P+F)+I(P+F+H)+(B+1)G+C)。S/I/B は採取/反復/最良更新数、P/F/H/G/C は各処理量。
template<
    class Snapshot,
    Numeric Cost,
    class GetSnapshot,
    class GetCost,
    class Propose,
    class Finalize,
    class DebugHook = NoOp>
SaResult<Snapshot, Cost> sa(
    const SaParam& param,
    double time_limit_ms,
    GetSnapshot get_snapshot,
    GetCost get_cost,
    Propose propose,
    Finalize finalize,
    optional<Cost> existing_best_cost = nullopt,
    DebugHook debug_hook = DebugHook{}) {
    if constexpr (integral<Cost>) static_assert(is_signed_v<Cost>, "Signed integral Cost is required because delta can be negative");

    // 変換やユーザー状態へのアクセスより先に、パラメータの前提を確認する
    assert((param.samples >= 0) && "sa::SaParam.samples must be >= 0");
    assert((0.0 < param.start_accept_prob && param.start_accept_prob < 1.0) && "sa::SaParam.start_accept_prob must satisfy 0 < p < 1");
    assert((0.0 < param.end_accept_prob && param.end_accept_prob < 1.0) && "sa::SaParam.end_accept_prob must satisfy 0 < p < 1");
    assert((isfinite(param.start_temp) && param.start_temp > 0.0) && "start_temp must be finite and > 0");
    assert((isfinite(param.end_temp) && param.end_temp > 0.0) && "end_temp must be finite and > 0");
    assert(param.acceptance_width_scale >= 0.0 && "acceptance_width_scale must be nonnegative; +infinity is allowed");
    if constexpr (floating_point<Cost>) assert(!existing_best_cost || !isnan(*existing_best_cost));

    const double limit_us_raw = max(0.0, time_limit_ms) * 1000.0;
    assert(isfinite(time_limit_ms) && limit_us_raw < 0x1p63 && "time budget must fit int64_t microseconds");
    const int64_t limit_us = static_cast<int64_t>(limit_us_raw);

    // 初期解を評価し、保存基準を満たすときだけ Snapshot を作る
    detail::FastRng rng(param.seed);
    SaRuntime<Cost> runtime;
    runtime.current_cost = get_cost();
    runtime.best_cost = runtime.current_cost;
    auto take_snapshot = [&]() -> Snapshot {
        if constexpr (is_invocable_r_v<Snapshot, GetSnapshot&>) return get_snapshot();
        else return get_snapshot(static_cast<const SaRuntime<Cost>&>(runtime));
    };
    optional<Snapshot> best_snapshot;
    auto save_snapshot = [&]() {
        if (!existing_best_cost || runtime.best_cost < *existing_best_cost)
            best_snapshot.emplace(take_snapshot());
    };
    save_snapshot();

    const auto start_clock = chrono::steady_clock::now();
    const auto elapsed_now_us = [&]() -> int64_t {
        return detail::steady_elapsed_us(start_clock);
    };
    runtime.time_limit_us = limit_us;

    // サンプリングでは毎回ロールバックし、有限な悪化量だけを温度推定に使う
    double start_temp = param.start_temp;
    double end_temp = param.end_temp;
    if (param.auto_mode && param.samples > 0 && limit_us > 0) {
        double avg_worse_delta = 0.0;
        int worse_count = 0;
        runtime.temperature = param.start_temp;

        for (int sample_id = 0; sample_id < param.samples; ++sample_id) {
            if ((sample_id & 31) == 0 && elapsed_now_us() >= limit_us) break;
            const double delta = detail::to_double(detail::call_propose<Cost>(propose, runtime));
            if (delta > 0.0 && isfinite(delta)) {
                // 正の値の逐次平均なら、総和が double の上限を超える場合も扱える
                avg_worse_delta += (delta - avg_worse_delta) / static_cast<double>(++worse_count);
            }
            finalize(false);
        }

#ifdef LOCAL
        if (worse_count < 100) {
            cerr << "[sa] warning: auto temperature sampling collected only " << worse_count << " worsening moves.\n";
        }
#endif

        if (worse_count > 0) {
            start_temp = detail::temperature_from_avg_delta(avg_worse_delta, param.start_accept_prob);
            end_temp = detail::temperature_from_avg_delta(avg_worse_delta, param.end_accept_prob) * 0.1;
        }
        // 有効標本がない場合や、推定温度が表現範囲を外れる場合は手動値へ戻す
        if (worse_count == 0 || !isfinite(start_temp) || !isfinite(end_temp) || start_temp <= 0.0 || end_temp <= 0.0) {
            start_temp = param.start_temp;
            end_temp = param.end_temp;
#ifdef LOCAL
            cerr << "[sa] warning: auto temperature sampling fallback to manual temperatures.\n";
#endif
        }
    }

    // 決定した初期温度を通知し、有効予算がなければ初期解を返す
    runtime.temperature = start_temp;
    runtime.elapsed_us = min(elapsed_now_us(), limit_us);
    detail::call_debug_hook(debug_hook, SaEventType::RunStart, runtime);

#ifdef LOCAL
    detail::TemperatureReport temperature_report;
    auto report_temperature = [&]() {
        if (param.enable_temperature_report) temperature_report.print(param,
            requires { { propose.sa_accept_worse(runtime, declval<const Cost&>(), 0.0) } -> convertible_to<bool>; });
    };
#endif

    if (limit_us <= 0) {
        detail::call_debug_hook(debug_hook, SaEventType::RunEnd, runtime);
#ifdef LOCAL
        report_temperature();
#endif
        return {runtime.best_cost, move(best_snapshot)};
    }

    // 温度の対数を一度だけ計算する。温度同士の除算を避けて範囲外へのあふれを防ぐ
    const double limit_us_d = static_cast<double>(limit_us);
    const double log_start_temp = log(start_temp);
    const double log_end_temp = log(end_temp);
    double acceptance_limit = param.acceptance_width_scale * runtime.temperature;
    const bool unrestricted_width = isinf(param.acceptance_width_scale);
    while (true) {
        if ((runtime.iteration & 31) == 0) {
            runtime.elapsed_us = elapsed_now_us();
            if (runtime.elapsed_us >= limit_us) break;
        }
        // 第 1 反復は初期温度、第 2 反復と以後の時刻更新時だけ指数補間する
        if (runtime.iteration != 0 && ((runtime.iteration & 31) == 0 || runtime.iteration == 1) && runtime.elapsed_us > 0) {
            runtime.temperature = exp(lerp(log_start_temp, log_end_temp,
                static_cast<double>(runtime.elapsed_us) / limit_us_d));
            acceptance_limit = param.acceptance_width_scale * runtime.temperature;
        }
        ++runtime.iteration;
        runtime.best_updated_this_iter = false;
        runtime.accepted_this_iter = false;
        runtime.worse_accepted_this_iter = false;

        detail::call_debug_hook(debug_hook, SaEventType::IterationStart, runtime);

        const Cost delta = detail::call_propose<Cost>(propose, runtime);
#ifdef LOCAL
        if (param.enable_temperature_report) temperature_report.add(detail::to_double(delta), runtime.progress());
#endif

        // 改善・同値は必ず受理し、悪化だけ乱数を消費する
        bool accepted = false;
        if (delta <= Cost{0}) {
            accepted = true;
        } else {
            if constexpr (requires { { propose.sa_accept_worse(runtime, delta, 0.0) } -> convertible_to<bool>; }) {
                accepted = propose.sa_accept_worse(runtime, delta, rng.rand_unit());
            } else {
                double gap = detail::to_double(runtime.current_cost) - detail::to_double(runtime.best_cost);
                if constexpr (integral<Cost>)
                    gap = detail::to_double(static_cast<uint64_t>(runtime.current_cost) - static_cast<uint64_t>(runtime.best_cost));
                const double u = rng.rand_unit();
#ifdef LOCAL
                if (param.enable_temperature_report && isfinite(detail::to_double(delta)) && !unrestricted_width
                    && gap + detail::to_double(delta) >= acceptance_limit) ++temperature_report.width_rejected;
#endif
                accepted = (unrestricted_width || gap + detail::to_double(delta) < acceptance_limit)
                    && u < exp(-detail::to_double(delta) / runtime.temperature);
            }
        }

        // 先に状態を確定してからコストと統計を更新し、最良更新時だけ状態を保存する
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
                save_snapshot();
                ++runtime.best_update_count;
                runtime.last_best_update_iter = runtime.iteration;
                runtime.elapsed_us_at_last_best = runtime.elapsed_us;
            }
        }

        detail::call_debug_hook(debug_hook, SaEventType::IterationEnd, runtime);
    }

    // ループ終了時には時間切れを計測済みなので、再計測・再補間は不要
    runtime.temperature = end_temp;
    detail::call_debug_hook(debug_hook, SaEventType::RunEnd, runtime);

#ifdef LOCAL
    report_temperature();
    if (param.enable_end_cost_check) {
        const Cost checked_cost = get_cost();
        const streamsize old_precision = cerr.precision(15);
        cerr << "[sa] end cost check: current_cost=" << +runtime.current_cost
             << ", get_cost()=" << +checked_cost << ", diff=";
        if constexpr (integral<Cost>) {
            // 符号付きの差が型の範囲を超えても、符号と符号なしの絶対差で正確に出力する
            const uint64_t a = static_cast<uint64_t>(checked_cost);
            const uint64_t b = static_cast<uint64_t>(runtime.current_cost);
            if (checked_cost < runtime.current_cost) cerr << '-' << (b - a);
            else cerr << (a - b);
        } else {
            cerr << detail::to_double(checked_cost) - detail::to_double(runtime.current_cost);
        }
        cerr << '\n';
        cerr.precision(old_precision);
    }
#endif

    return {runtime.best_cost, move(best_snapshot)};
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
        if (!(fabs(a - b) <= eps)) {
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
        auto [best_cost, best_snapshot] = sa<int, int>(
            param,
            0,
            [&]() { return state; },
            [&]() { return state; },
            [&]() { return -1; },
            [&](bool accepted) {
                if (accepted) --state;
            });
        check(best_cost == 42, "time_limit_ms = 0 returns initial cost");
        check(best_snapshot == 42, "time_limit_ms = 0 returns initial state");
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
        auto [best_cost, best_snapshot] = sa<int, int>(
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
        check(best_snapshot == 0, "rejected worsening move keeps best state");
        check(state == 0, "Finalize(false) restores original state");
    }

    // Propose 時にはまだ適用せず、accept なら Finalize(true) で反映する流儀を確認する。
    // 常に改善手を返して、best_snapshot が最終状態に追従することを確認する。
    {
        int state = 50;
        int pending = 0;
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 1.0;
        param.end_temp = 1.0;
        auto [best_cost, best_snapshot] = sa<int, int>(
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
        check(best_snapshot == state, "accept-apply style stores latest best state");
        check(best_cost < 50, "always-improving moves actually improve cost");
    }

    // 同値手 delta=0 も accept 扱いになり、状態が壊れないことを確認する。
    {
        int state = 7;
        SaParam param;
        param.auto_mode = false;
        param.start_temp = 10.0;
        param.end_temp = 10.0;
        auto [best_cost, best_snapshot] = sa<int, int>(
            param,
            3,
            [&]() { return state; },
            [&]() { return state; },
            [&]() { return 0; },
            [&](bool accepted) {
                if (!accepted) fail("delta=0 must be accepted");
            });
        check(best_cost == 7, "zero-delta move does not change best cost");
        check(best_snapshot == 7, "zero-delta move does not change best state");
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
        auto [best_cost, best_snapshot] = sa<double, double>(
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
        check_near(*best_snapshot, state, 1e-9, "floating-point best state matches current state");
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

        auto [best_cost, best_snapshot] = sa<int, int>(
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
        check(best_cost == 0 && best_snapshot == 0, "auto temperature success case keeps state valid");
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

        auto [best_cost, best_snapshot] = sa<int, int>(
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
        check(best_cost == 0 && best_snapshot == 0, "auto temperature fallback keeps initial best");
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

        SaCsvStatHook<int> csv_hook(csv_filename, 1.0);
        auto combo_hook = [&](SaEventType event_type, const SaRuntime<int>& runtime) {
            csv_hook(event_type, runtime);
        };

        auto [best_cost, best_snapshot] = sa<int, int>(
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
            nullopt,
            combo_hook);
        (void)best_cost;
        (void)best_snapshot;

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

        SaCsvStatHook<long long> csv_hook("sa_tsp_stat_v5.csv", 50.0);
        int64_t next_print_us = 0;
        auto debug_hook = [&](SaEventType event_type, const SaRuntime<long long>& runtime) {
            csv_hook(event_type, runtime);
            if (event_type == SaEventType::IterationEnd && runtime.elapsed_us >= next_print_us) {
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
            if (event_type == SaEventType::RunEnd) {
                cerr << "[tsp] finished: elapsed_ms=" << detail::us_to_ms(runtime.elapsed_us)
                     << " iter=" << runtime.iteration
                     << " best_updates=" << runtime.best_update_count
                     << " best=" << runtime.best_cost
                     << '\n';
            }
        };

        auto [best_cost, best_snapshot] = sa<tsp_problem::Output, long long>(
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
            nullopt,
            debug_hook);

        check(best_cost <= initial_cost, "TSP demo improves or keeps initial route");
        check(tsp_problem::compute_score(input, *best_snapshot) == best_cost, "best_snapshot score matches best_cost");
        cout << "[tsp demo] initial_cost=" << initial_cost << '\n';
        cout << "[tsp demo] best_cost=" << best_cost << '\n';
        cout << "[tsp demo] best_state_size=" << best_snapshot->route.size() << '\n';
        cout << "[tsp demo] done\n";
    }

    cout << "\nAll tests finished successfully.\n";

    // 追加ベンチマーク
    // 既存のテストや出力はそのまま残し、seed 0..10 の問題に対して
    // 同様の TSP サンプル solver を実行し、集計値を出力する。
    {
        auto solve_tsp_sample = [&](uint64_t problem_seed) -> SaResult<tsp_problem::Output, long long> {
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

            auto [best_cost, best_snapshot] = sa<tsp_problem::Output, long long>(
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
            return {best_cost, move(best_snapshot)};
        };

        cout << "\n[tsp multi-seed] start\n";
        int error_count = 0;
        long long score_sum = 0;
        long long min_score = numeric_limits<long long>::max();
        long long max_score = numeric_limits<long long>::min();

        for (uint64_t seed = 0; seed <= 10; ++seed) {
            const tsp_problem::Input input = tsp_problem::gen(seed);
            auto [best_cost, best_snapshot] = solve_tsp_sample(seed);
            const long long verified_score = tsp_problem::compute_score(input, *best_snapshot);
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
        if (error_count != 0) fail("TSP multi-seed consistency");
    }

    // 進捗率の境界値とランダムな経過時間を、直接計算した比率と比較する。
    {
        static_assert(is_aggregate_v<SaRuntime<long long>>);
        static_assert(same_as<decltype(SaRuntime<float>{}.progress()), double>);
        static_assert(same_as<decltype(SaRuntime<double>{}.progress()), double>);
        SaRuntime<long long> runtime;
        check(runtime.progress() == 1.0, "progress with zero budget is one");
        runtime.time_limit_us = -1;
        check(runtime.progress() == 1.0, "progress with negative budget is one");
        runtime.time_limit_us = 1000;
        for (const auto& [elapsed, expected] : array<pair<int64_t, double>, 7>{{
                 {-1, 0.0}, {0, 0.0}, {125, 0.125}, {500, 0.5},
                 {999, 0.999}, {1000, 1.0}, {1001, 1.0}}}) {
            runtime.elapsed_us = elapsed;
            check_near(runtime.progress(), expected, 1e-15, "progress boundary value");
        }
        runtime.time_limit_us = numeric_limits<int64_t>::max();
        runtime.elapsed_us = runtime.time_limit_us;
        check(runtime.progress() == 1.0, "progress handles large timestamps");

        // 乱数で予算と経過時間を変え、下限・上限と計算結果を確認する。
        detail::FastRng rng(987654321);
        for (int i = 0; i < 10000; ++i) {
            runtime.time_limit_us = static_cast<int64_t>(rng() % 1000000ULL) + 1;
            runtime.elapsed_us = static_cast<int64_t>(rng() % 3000001ULL) - 1000000;
            const double expected = runtime.elapsed_us <= 0 ? 0.0
                : runtime.elapsed_us >= runtime.time_limit_us ? 1.0
                : static_cast<double>(runtime.elapsed_us) / static_cast<double>(runtime.time_limit_us);
            if (runtime.progress() != expected) fail("random progress differs from reference");
        }
        check(true, "10000 randomized progress checks");
    }

    // 実行中の進捗率、サンプリング時の初期化、終了時の上限処理を確認する。
    for (const bool auto_mode : {false, true}) {
        SaParam param;
        param.auto_mode = auto_mode;
        param.enable_end_cost_check = false;
        constexpr double budget_ms = 10.25;
        constexpr int64_t budget_us = 10250;
        int sample_count = 0;
        int main_count = 0;
        int hook_count = 0;
        int64_t previous_elapsed = 0;
        double previous_progress = 0.0;
        double end_progress = -1.0;
        auto verify_progress = [&](const SaRuntime<int>& runtime) -> void {
            const double value = runtime.progress();
            if (runtime.time_limit_us != budget_us || value < previous_progress || value > 1.0) {
                fail("runtime progress budget/range/monotonicity");
            }
            const double expected = min(static_cast<double>(runtime.elapsed_us) / 10250.0, 1.0);
            if (value != expected) fail("runtime progress differs from elapsed/budget");
            previous_progress = value;
        };

        // Propose からの参照は LOCAL の有無に依存しない。
        const auto result = sa<int, int>(
            param, budget_ms,
            []() -> int { return 0; },
            []() -> int { return 0; },
            [&](const SaRuntime<int>& runtime) -> int {
                verify_progress(runtime);
                if (runtime.iteration == 0) {
                    ++sample_count;
                    if (runtime.progress() != 0.0) fail("sampling progress must remain zero");
                    return 1;
                }
                ++main_count;
                if (((runtime.iteration - 1) & 31) != 0 && runtime.elapsed_us != previous_elapsed) {
                    fail("elapsed time changed inside a 32-iteration block");
                }
                previous_elapsed = runtime.elapsed_us;
                return 0;
            },
            [](bool) -> void {},
            nullopt,
            [&](SaEventType event_type, const SaRuntime<int>& runtime) -> void {
                ++hook_count;
                verify_progress(runtime);
                if (event_type == SaEventType::RunEnd) end_progress = runtime.progress();
            });
        check(result.best_cost == 0 && result.best_snapshot == 0 && main_count > 0,
              "progress access keeps SA state/result valid");
        check(sample_count == (auto_mode ? param.samples : 0), "progress initialized before sampling");
        check(detail::debug_hook_enabled ? end_progress == 1.0 : hook_count == 0,
              "progress at RunEnd is one; hooks remain LOCAL-only");
    }

    // 実行予算が 0μs になる場合は、提案せず、Hook からは消費率 1 を返す。
    for (const double budget_ms : {0.0, -1.0, 0.0009}) {
        SaParam param;
        param.enable_end_cost_check = false;
        int hook_count = 0;
        const auto result = sa<int, int>(
            param, budget_ms,
            []() -> int { return 42; },
            []() -> int { return 42; },
            [&]() -> int { fail("zero budget must not propose"); return 0; },
            [&](bool) -> void { fail("zero budget must not finalize"); },
            nullopt,
            [&](SaEventType, const SaRuntime<int>& runtime) -> void {
                ++hook_count;
                if (runtime.progress() != 1.0) fail("zero effective budget progress must be one");
            });
        check((result.best_cost == 42 && result.best_snapshot == 42), "zero effective budget keeps initial result");
        check(hook_count == (detail::debug_hook_enabled ? 2 : 0), "zero-budget hook behavior unchanged");
    }


    // 温度の更新頻度を減らしても、各反復で従来の計算式と完全一致することを確認する
    for (const auto& [start_temp, end_temp] : array<pair<double, double>, 4>{{
             {100.0, 0.1}, {5.0, 5.0}, {0.1, 100.0}, {1e-200, 1e-199}}}) {
        SaParam param;
        param.auto_mode = false;
        param.start_temp = start_temp;
        param.end_temp = end_temp;
        param.enable_end_cost_check = false;
        int proposals = 0;
        int finalizes = 0;
        int state_copies = 0;
        int hooks = 0;
        double previous_temperature = start_temp;
        const auto result = sa<int, int>(
            param, 3.25,
            [&]() -> int { ++state_copies; return 0; },
            []() -> int { return 0; },
            [&](const SaRuntime<int>& runtime) -> int {
                ++proposals;
                const double expected = runtime.iteration == 1 ? start_temp
                    : detail::exp_temperature(start_temp, end_temp,
                        static_cast<double>(runtime.elapsed_us) / 3250.0);
                if (runtime.temperature != expected) fail("temperature differs from per-iteration reference");
                if (runtime.iteration > 2 && ((runtime.iteration - 1) & 31) != 0 &&
                    runtime.temperature != previous_temperature) {
                    fail("temperature changed without a time update");
                }
                previous_temperature = runtime.temperature;
                return 0;
            },
            [&](bool accepted) -> void {
                if (!accepted) fail("zero delta must remain accepted");
                ++finalizes;
            },
            nullopt,
            [&](SaEventType event, const SaRuntime<int>& runtime) -> void {
                ++hooks;
                if (event == SaEventType::RunEnd) {
                    const double expected = detail::exp_temperature(start_temp, end_temp, 1.0);
                    if (runtime.temperature != expected) fail("RunEnd temperature changed");
                }
            });
        check((result.best_cost == 0 && result.best_snapshot == 0) && proposals >= 32 && proposals == finalizes && state_copies == 1,
              "cached temperature matches reference and preserves callback counts");
        check(hooks == (detail::debug_hook_enabled ? 2 * proposals + 2 : 0),
              "temperature optimization preserves all hook events");
    }

    // 温度比が表現範囲を外れる場合と、ランダムな温度の補間を検証する
    {
        check_near(detail::exp_temperature(1e-300, 1e300, 0.5), 1.0, 1e-12, "extreme heating midpoint is finite");
        check_near(detail::exp_temperature(1e300, 1e-300, 0.5), 1.0, 1e-12, "extreme cooling midpoint is positive");
        detail::FastRng rng(20260914);
        for (int i = 0; i < 10000; ++i) {
            const double start = ldexp(1.0, static_cast<int>(rng() % 1801) - 900);
            const double end = ldexp(1.0, static_cast<int>(rng() % 1801) - 900);
            const double ratio = static_cast<double>(rng() % 1001) / 1000.0;
            const double actual = detail::exp_temperature(start, end, ratio);
            const double expected = pow(start, 1.0 - ratio) * pow(end, ratio);
            if (!(actual > 0.0) || !isfinite(actual) ||
                fabs(actual - expected) > max(expected * 2e-12, 2 * numeric_limits<double>::denorm_min())) {
                fail("random exponential interpolation");
            }
        }
        check(true, "10000 randomized temperature checks against independent power formula");
    }

    // 大きな標本の平均、無効標本の除外、推定温度が表現できない場合を確認する
    for (int mode = 0; mode < 4; ++mode) {
        SaParam param;
        param.start_temp = 7.0;
        param.end_temp = 0.5;
        param.enable_end_cost_check = false;
        if (mode == 0) {
            param.start_accept_prob = exp(-4.0);
            param.end_accept_prob = exp(-8.0);
        }
        int samples = 0;
        int finalizes = 0;
        double first_temp = 0.0;
        ostringstream warnings;
        auto* previous = cerr.rdbuf(warnings.rdbuf());
        const auto result = sa<int, double>(param, 3.0,
            []() -> int { return 0; }, []() -> double { return 0.0; },
            [&](const SaRuntime<double>& runtime) -> double {
                if (runtime.iteration == 0) {
                    ++samples;
                    if (mode <= 1) return 1e308;
                    if (mode == 2) return numeric_limits<double>::infinity();
                    return samples <= 40 ? numeric_limits<double>::quiet_NaN() : 5.0;
                }
                if (first_temp == 0.0) first_temp = runtime.temperature;
                return 0.0;
            }, [&](bool accepted) -> void { if (!accepted) ++finalizes; });
        cerr.rdbuf(previous);
        const double expected = mode == 0 ? 2.5e307 : (mode == 3 ? -5.0 / log(0.8) : 7.0);
        check(isfinite(first_temp) && fabs(first_temp - expected) <= expected * 1e-12 &&
                  samples == 300 && finalizes == 300 && (result.best_cost == 0.0 && result.best_snapshot == 0),
              "auto temperature handles large or nonfinite samples without state changes");
    }

    // 左辺値専用コールバックと、Runtime 付き Propose の優先を確認する
    {
        struct Getter {
            // 現在状態を返す O(1)
            int operator()() & { return 0; }
        };
        struct Proposal {
            int* calls;
            // Runtime 付きの呼び出しを記録する O(1)
            int operator()(const SaRuntime<int>&) & { ++*calls; return 0; }
            // Runtime 付きが呼ばれるため、こちらは使われない O(1)
            int operator()() & { assert(false); return 1; }
        };
        struct Finalizer {
            int* calls;
            // 確定処理の呼び出しを記録する O(1)
            void operator()(bool) & { ++*calls; }
        };
        SaParam param;
        param.auto_mode = false;
        param.enable_end_cost_check = false;
        int proposals = 0;
        int finalizes = 0;
        const auto result = sa<int, int>(param, 2.0, Getter{}, Getter{}, Proposal{&proposals}, Finalizer{&finalizes});
        check((result.best_cost == 0 && result.best_snapshot == 0) && proposals > 0 && proposals == finalizes,
              "lvalue-only callbacks work and Runtime overload takes precedence");
    }

    // 64 bit 整数の微小差・最大差と、cerr の精度を変更しないことを確認する
    for (const auto& [initial, checked] : array<pair<long long, long long>, 5>{{
             {1LL << 60, (1LL << 60) + 1}, {(1LL << 60) + 1, 1LL << 60},
             {LLONG_MIN, LLONG_MAX}, {LLONG_MAX, LLONG_MIN}, {7, 7}}}) {
        SaParam param;
        param.auto_mode = false;
        int cost_calls = 0;
        ostringstream output;
        auto* previous = cerr.rdbuf(output.rdbuf());
        const streamsize precision = cerr.precision();
        (void)sa<int, long long>(param, 1.0, []() -> int { return 0; },
            [&]() -> long long { return cost_calls++ == 0 ? initial : checked; },
            []() -> long long { return 0; }, [](bool) -> void {});
        cerr.rdbuf(previous);
        const string difference = initial == checked ? "0" : initial == LLONG_MIN
            ? "18446744073709551615" : initial == LLONG_MAX ? "-18446744073709551615"
            : initial < checked ? "1" : "-1";
        check(cerr.precision() == precision && (detail::debug_hook_enabled
                  ? cost_calls == 2 && output.str().find(", diff=" + difference + "\n") != string::npos
                  : cost_calls == 1 && output.str().empty()),
              "end check reports exact integer difference only in LOCAL and preserves stream precision");
    }

    // CSV の時刻あふれ、小さい整数型、書き込み失敗を確認する
    {
        SaCsvStatHook<signed char> hook("sa_small_cost_v12.csv", 1.0);
        SaRuntime<signed char> runtime;
        runtime.current_cost = -7;
        runtime.best_cost = -8;
        runtime.elapsed_us = 1;
        hook.interval_us = INT64_MAX;
        hook(SaEventType::RunStart, runtime);
        check(hook.next_record_us == INT64_MAX, "CSV next timestamp saturates without signed overflow");
        hook(SaEventType::RunEnd, runtime);
        ifstream csv(hook.filename);
        const string text((istreambuf_iterator<char>(csv)), istreambuf_iterator<char>());
        check(text.find(",-7,-8,") != string::npos, "CSV prints small integer costs as numbers");
        SaCsvStatHook<int> full("/dev/full");
        SaRuntime<int> state;
        ostringstream warning;
        auto* previous = cerr.rdbuf(warning.rdbuf());
        full(SaEventType::RunStart, state);
        full(SaEventType::RunEnd, state);
        cerr.rdbuf(previous);
        check(warning.str().find("failed to write csv file") != string::npos, "CSV reports buffered write failures");

        // 記録された区間統計を、直前の Runtime との単純な差分計算と突き合わせる
        SaCsvStatHook<int> stats("sa_periods_v12.csv", 0.1);
        stats(SaEventType::RunStart, state);
        SaRuntime<int> last = state;
        detail::FastRng rng(92741);
        for (int i = 0; i < 10000; ++i) {
            ++state.iteration;
            state.elapsed_us += static_cast<int64_t>(rng() % 31);
            const bool accepted = (rng() & 1) != 0;
            state.accepted_count += accepted;
            state.worse_accepted_count += accepted && (rng() & 1) != 0;
            state.best_update_count += accepted && (rng() & 7) == 0;
            const size_t previous_size = stats.rows.size();
            stats(SaEventType::IterationEnd, state);
            if (stats.rows.size() == previous_size) continue;
            const auto& row = stats.rows.back();
            if (row.period_iterations != state.iteration - last.iteration ||
                row.period_accepted != state.accepted_count - last.accepted_count ||
                row.period_worse_accepted != state.worse_accepted_count - last.worse_accepted_count ||
                row.period_best_updates != state.best_update_count - last.best_update_count ||
                fabs(row.period_ms - detail::us_to_ms(state.elapsed_us - last.elapsed_us)) > 1e-10 ||
                row.accept_rate != static_cast<double>(row.period_accepted) / row.period_iterations ||
                row.worse_accept_rate != static_cast<double>(row.period_worse_accepted) / row.period_iterations) {
                fail("CSV randomized period statistics");
            }
            last = state;
        }
        stats(SaEventType::RunEnd, state);
        check(stats.rows.back().iteration == 10000, "10000 randomized CSV events match naive period statistics");
        stats(SaEventType::RunStart, SaRuntime<int>{});
        check(stats.rows.size() == 1 && stats.rows.front().period_iterations == 0, "CSV reuse resets the period baseline");
    }

    // 保存用状態にはコピー構築を要求せず、取得したスナップショットを移動できればよい
    {
        SaParam param;
        param.auto_mode = false;
        param.enable_end_cost_check = false;
        int current = 10;
        int pending = 0;
        int snapshots = 0;
        auto result = sa<unique_ptr<int>, int>(param, 2.0,
            [&]() -> unique_ptr<int> { ++snapshots; return make_unique<int>(current); },
            [&]() -> int { return current; },
            [&]() -> int { pending = current > 0 ? -1 : 0; return pending; },
            [&](bool accepted) -> void { if (accepted) current += pending; });
        check(result.best_cost == 0 && result.best_snapshot && **result.best_snapshot == 0 && snapshots == 11,
              "move-only snapshots are captured exactly once per best update");
    }

    return 0;
}

#endif
