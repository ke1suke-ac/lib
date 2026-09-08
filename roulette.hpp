#pragma once
#include <bits/stdc++.h>
using namespace std;

// 重み付きルーレット選択（Vose のエイリアス法, Lemire法 + RNG 1回化 + t前計算 + xorshift32拡散）
// T は整数型または浮動小数点型を許可
template <typename T>
    requires (integral<T> || floating_point<T>)
struct Roulette {
    explicit Roulette() : rng(0x9E3779B97F4A7C15ULL) {}
    explicit Roulette(const vector<T>& w) : Roulette() {
        init(w);
    }

    // ルーレット選択（重みに比例したインデックスを返す）期待 O(1)
    size_t sample() {
        if (n == 0) [[unlikely]] return 0;

        // --- Lemire 法で無偏りに i ∈ [0, n) を作る（除算/剰余なし） ---
        uint64_t x, l;
        size_t i;
        for (;;) {
            x = rng();
            __uint128_t m = static_cast<__uint128_t>(x) * n_u64;
            i = static_cast<size_t>(m >> 64);
            l = static_cast<uint64_t>(m);
            if (l >= t_) break;  // 補正（レアに再試行）
        }

        // --- RNG 1回化：x から xorshift32 で 32bit 一様乱数を得る ---
        const uint32_t y = xorshift32_from64(x);

        // 量子化しきい値と整数比較
        return (y < prob_scaled[i]) ? i : static_cast<size_t>(alias[i]);
    }

    size_t size()        const noexcept { return n; }
    double total_weight() const noexcept { return total_w; }

private:
    size_t   n = 0;
    uint64_t n_u64 = 1;
    uint64_t t_ = 0;            // Lemire の補正しきい値 t = (-n) % n（前計算）
    double   total_w = 0.0;

    vector<uint32_t> prob_scaled;   // しきい値（0..UINT32_MAX）
    vector<uint32_t> alias;         // 代替先
    mt19937_64 rng;

    static inline double sanitize_weight(T v) noexcept {
        if constexpr (floating_point<T>) {
            if (isnan(v)) return 0.0;
            return (v < static_cast<T>(0)) ? 0.0 : static_cast<double>(v);
        } else {
            if constexpr (is_signed_v<T>) {
                return (v < static_cast<T>(0)) ? 0.0 : static_cast<double>(v);
            } else {
                return static_cast<double>(v);
            }
        }
    }

    static inline uint32_t to_u32_prob(double p) noexcept {
        if (p <= 0.0) return 0u;
        if (p >= 1.0) return numeric_limits<uint32_t>::max();
        const double scaled = p * static_cast<double>(numeric_limits<uint32_t>::max());
        return static_cast<uint32_t>(scaled + 0.5); // 四捨五入
    }

    // xorshift32：x の下位32bitから拡散して 32bit 一様値を得る（追加の RNG 呼び出しなし）
    static inline uint32_t xorshift32_from64(uint64_t x) noexcept {
        uint32_t s = static_cast<uint32_t>(x); // 下位32を使用（高速）
        s ^= (s << 13);
        s ^= (s >> 17);
        s ^= (s << 5);
        return s;
    }

public:
    void init(const vector<T>& w) {
        n = w.size();
        n_u64 = (n ? static_cast<uint64_t>(n) : 1ULL);
        prob_scaled.assign(n ? n : 1, 0u);
        alias.assign(n ? n : 1, 0u);

        if (n == 0) {
            // 空はダミー一様
            n = 1;
            n_u64 = 1ULL;
            t_ = 0ULL; // l>=0 は常に真→1発受理
            prob_scaled[0] = numeric_limits<uint32_t>::max();
            alias[0] = 0u;
            total_w = 1.0;
            return;
        } else {
            // t = (-n) % n を前計算（Lemire の補正に使用）
            t_ = static_cast<uint64_t>(-static_cast<int64_t>(n_u64)) % n_u64;
        }

        vector<double> ws(n);
        total_w = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const double v = sanitize_weight(w[i]);
            ws[i] = v;
            total_w += v;
        }

        if (total_w <= 0.0) {
            // 全 0 → 一様
            for (size_t i = 0; i < n; ++i) {
                prob_scaled[i] = numeric_limits<uint32_t>::max();
                alias[i] = static_cast<uint32_t>(i);
            }
            total_w = static_cast<double>(n);
            return;
        }

        // エイリアス法の前処理（平均が 1 のスケーリング）
        vector<double> scaled(n);
        const double inv_avg = static_cast<double>(n) / total_w;
        for (size_t i = 0; i < n; ++i) scaled[i] = ws[i] * inv_avg;

        vector<size_t> small, large;
        small.reserve(n);
        large.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            (scaled[i] < 1.0 ? small : large).push_back(i);
        }

        while (!small.empty() && !large.empty()) {
            const size_t s = small.back(); small.pop_back();
            const size_t l = large.back(); large.pop_back();

            prob_scaled[s] = to_u32_prob(scaled[s]); // < 1
            alias[s] = static_cast<uint32_t>(l);

            // l の残りを更新
            scaled[l] = (scaled[l] + scaled[s]) - 1.0;

            if (scaled[l] < 1.0) small.push_back(l);
            else                 large.push_back(l);
        }
        while (!large.empty()) {
            const size_t i = large.back(); large.pop_back();
            prob_scaled[i] = numeric_limits<uint32_t>::max();
            alias[i] = static_cast<uint32_t>(i);
        }
        while (!small.empty()) {
            const size_t i = small.back(); small.pop_back();
            prob_scaled[i] = numeric_limits<uint32_t>::max();
            alias[i] = static_cast<uint32_t>(i);
        }
    }
};

#if 0  // ===== テスト & ベンチマーク & 可視化 =====

#include <cassert>
#include <chrono>
#include <iomanip>

// 期待度数に対する簡易統計チェック
// |obs-exp| <= k * sqrt(exp + 1e-9) + 1.0
static bool check_close_poisson(double obs, double exp, double k = 7.5) {
    if (exp <= 0.0) return obs == 0.0;
    const double dev = abs(obs - exp);
    const double sig = sqrt(max(exp, 1e-9));
    return dev <= k * sig + 1.0;
}

static string bar(double ratio, int width = 40) {
    ratio = clamp(ratio, 0.0, 1.0);
    const int filled = static_cast<int>(round(ratio * width));
    string s; s.reserve(width);
    for (int i = 0; i < width; ++i) s.push_back(i < filled ? '#' : '.');
    return s;
}

template <typename W>
void sanity_count_test(const vector<W>& w, int trials, double tol_k = 7.5) {
    Roulette<W> r(w);
    const int N = static_cast<int>(w.size());
    vector<int> cnt(N, 0);
    for (int t = 0; t < trials; ++t) ++cnt[r.sample()];

    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
        if constexpr (floating_point<W>) {
            if (!isnan(w[i]) && w[i] > static_cast<W>(0)) sum += static_cast<double>(w[i]);
        } else {
            if constexpr (is_signed_v<W>) {
                if (w[i] > 0) sum += static_cast<double>(w[i]);
            } else {
                sum += static_cast<double>(w[i]);
            }
        }
    }
    const bool uniform = (sum <= 0.0);
    for (int i = 0; i < N; ++i) {
        double wi = 0.0;
        if (uniform) wi = 1.0;
        else {
            if constexpr (floating_point<W>) {
                if (!isnan(w[i]) && w[i] > static_cast<W>(0)) wi = static_cast<double>(w[i]);
            } else {
                if constexpr (is_signed_v<W>) {
                    if (w[i] > 0) wi = static_cast<double>(w[i]);
                } else {
                    wi = static_cast<double>(w[i]);
                }
            }
        }
        const double p = uniform ? (1.0 / N) : (wi / sum);
        const double exp = p * static_cast<double>(trials);
        const double obs = static_cast<double>(cnt[i]);
        assert(check_close_poisson(obs, exp, tol_k));
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cout << "[Edge cases] start\n";
    {
        // N=1（常に 0）
        {
            vector<int> w = {123};
            Roulette<int> r(w);
            for (int t = 0; t < 1000; ++t) assert(r.sample() == 0u);
        }
        // 0 と 正（integral）
        {
            vector<int> w = {0, 0, 10, 0};
            Roulette<int> r(w);
            for (int t = 0; t < 2000; ++t) assert(r.sample() == 2u);
        }
        // 0 と 正（floating）
        {
            vector<double> w = {0.0, 0.0, 5.5, 0.0};
            Roulette<double> r(w);
            for (int t = 0; t < 2000; ++t) assert(r.sample() == 2u);
        }
        // すべて 0（→一様分布）
        {
            vector<int> w(7, 0);
            Roulette<int> r(w);
            array<int,7> cnt{}; cnt.fill(0);
            const int trials = 14000;
            for (int t = 0; t < trials; ++t) cnt[r.sample()]++;
            for (int i = 0; i < 7; ++i) {
                const double exp = static_cast<double>(trials) / 7.0;
                assert(check_close_poisson(static_cast<double>(cnt[i]), exp, 8.0));
            }
        }
        // NaN / 負重みを含む（NaN, 負は 0 扱い）
        {
            vector<double> w = {nan(""), 10.0, -3.0, 0.0};
            Roulette<double> r(w);
            array<int,4> cnt{}; cnt.fill(0);
            const int trials = 30000;
            for (int t = 0; t < trials; ++t) cnt[r.sample()]++;
            assert(cnt[1] >= static_cast<int>(trials * 0.99));
        }
        // 非常に大きな重み（double 正規化）
        {
            using U = unsigned long long;
            vector<U> w = {numeric_limits<U>::max()/2, numeric_limits<U>::max()/4, 1ULL};
            Roulette<U> r(w);
            array<int,3> cnt{}; cnt.fill(0);
            const int trials = 100000;
            for (int t = 0; t < trials; ++t) cnt[r.sample()]++;
            const double p0 = static_cast<double>(cnt[0]) / static_cast<double>(trials);
            const double p1 = static_cast<double>(cnt[1]) / static_cast<double>(trials);
            assert(p0 > 0.60 && p1 > 0.25);
        }
        cout << "[Edge cases] ok\n";
    }

    // ランダム一貫性テスト（整数）
    cout << "[Random consistency - integral] trials=200\n";
    {
        mt19937_64 gen(123456789);
        uniform_int_distribution<int> ndis(1, 2000);
        uniform_int_distribution<int> wdis(0, 1000);

        for (int tc = 0; tc < 200; ++tc) {
            const int N = ndis(gen);
            vector<int> w(N);
            for (int i = 0; i < N; ++i) w[i] = wdis(gen);
            const int S = clamp(50 * N, 50000, 300000);
            sanity_count_test(w, S, 7.5);
        }
        cout << "[Random consistency - integral] ok\n";
    }

    // ランダム一貫性テスト（浮動小数点）
    cout << "[Random consistency - floating] trials=200\n";
    {
        mt19937_64 gen(987654321);
        uniform_int_distribution<int> ndis(1, 2000);
        uniform_real_distribution<double> wdis(0.0, 1000.0);

        for (int tc = 0; tc < 200; ++tc) {
            const int N = ndis(gen);
            vector<double> w(N);
            for (int i = 0; i < N; ++i) w[i] = wdis(gen);
            const int S = clamp(50 * N, 50000, 300000);
            sanity_count_test(w, S, 7.5);
        }
        cout << "[Random consistency - floating] ok\n";
    }

    // ベンチマーク（3データセット、各 200 万サンプル）
    cout << "[Benchmark] start (iters=3, datasets up to N=1e6)\n";
    {
        mt19937_64 gen(135791357913ULL);
        uniform_int_distribution<int> ndis(1000, 1'000'000);
        uniform_real_distribution<double> wdis(0.0, 1000.0);

        const int iters = 3;
        const int samples_per_dataset = 2'000'000;
        double sum_ms = 0.0;
        double sum_ns_per_call = 0.0;
        double avg_runlen_acc = 0.0;

        for (int it = 0; it < iters; ++it) {
            const int N = ndis(gen);
            vector<double> w(N);
            for (int i = 0; i < N; ++i) w[i] = wdis(gen);

            auto t0 = chrono::high_resolution_clock::now();
            Roulette<double> r(w); // 前処理 O(N)
            auto t1 = chrono::high_resolution_clock::now();

            size_t last = r.sample();
            long long run_len_sum = 0, run_len_cnt = 0, cur_len = 1;

            auto s0 = chrono::high_resolution_clock::now();
            for (int s = 1; s < samples_per_dataset; ++s) {
                const size_t idx = r.sample();
                if (idx == last) ++cur_len;
                else {
                    run_len_sum += cur_len;
                    ++run_len_cnt;
                    cur_len = 1;
                    last = idx;
                }
            }
            run_len_sum += cur_len; ++run_len_cnt;
            auto s1 = chrono::high_resolution_clock::now();

            const double init_ms = chrono::duration<double, milli>(t1 - t0).count();
            const double ms = chrono::duration<double, milli>(s1 - s0).count();
            sum_ms += ms;
            sum_ns_per_call += (ms * 1e6) / static_cast<double>(samples_per_dataset);
            avg_runlen_acc += static_cast<double>(run_len_sum) / static_cast<double>(run_len_cnt);

            cout << "  N=" << N
                 << " | build=" << fixed << setprecision(3) << init_ms << " ms"
                 << " | sample=" << fixed << setprecision(3) << ms
                 << " ms (" << setprecision(2)
                 << (ms*1e6/static_cast<double>(samples_per_dataset)) << " ns/call)\n";
        }
        cout << "  avg time per dataset : " << fixed << setprecision(3) << (sum_ms / iters) << " ms\n";
        cout << "  avg time per call    : " << fixed << setprecision(2) << (sum_ns_per_call / iters) << " ns\n";
        cout << "  avg run-length       : " << fixed << setprecision(3) << (avg_runlen_acc / iters) << "\n";
        cout << "[Benchmark] done\n";
    }

    // ASCII 可視化（小規模 N）
    cout << "[ASCII Visualization] One random sample\n";
    {
        mt19937_64 gen(42);
        const int N = 16;
        vector<double> w(N);
        for (int i = 0; i < N; ++i) w[i] = static_cast<double>((i+1)*(i+1));
        double sum = 0.0;
        for (int i = 0; i < N; ++i) sum += w[i];

        Roulette<double> r(w);
        const int S = 200000;
        vector<int> cnt(N, 0);
        for (int s = 0; s < S; ++s) cnt[r.sample()]++;

        cout << "  index | expected% | observed% | bar(observed/expected)\n";
        for (int i = 0; i < N; ++i) {
            const double p = w[i] / sum;
            const double expc = p * static_cast<double>(S);
            const double obsp = 100.0 * static_cast<double>(cnt[i]) / static_cast<double>(S);
            const double expp = 100.0 * p;
            const double ratio = (expc > 0 ? static_cast<double>(cnt[i]) / expc : 1.0);
            cout << "  " << setw(5) << i
                 << " | " << setw(9) << fixed << setprecision(3) << expp
                 << " | " << setw(9) << fixed << setprecision(3) << obsp
                 << " | " << bar(min(1.0, ratio)) << "\n";
        }
    }

    cout << "[All tests completed successfully]\n";
    return 0;
}

#endif  // ===== テストブロック =====
