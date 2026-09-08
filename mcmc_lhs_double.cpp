/*
  ===========================================================================
  拡張 LHS (Orthogonal-Array-Based Latin Hypercube Sampling) 実装
  ---------------------------------------------------------------------------
  ◇ 目的
     n 次元 (n ≤ P) のクエリ x ∈ [0,1)^n を 1 サンプル/呼び出し生成し、
     列間一次相関をほぼ 0 に抑える。MCMC などで効率よく探索したい場面向き。

  ◇ 使い方（最小例）
       ExtendedLHS gen(n);          // O(n) 構築
       auto q = gen.next();         // O(n) で 1 クエリ生成

  ◇ 計算量
     • 構築   : O(n)
     • next() : O(n)  (整数演算のみ)
     • メモリ : O(n)  (n=10⁴ で <400 KiB)

  ◇ 注意点
     • スレッドセーフではない（内部に RNG 状態を保持）。
     • n ≥ 2 000 では相関を安定させるため内部素数 P を 2n 近くまで拡大。
     • 先頭 10 000 行のみ観測しても |corr| ≲ 0.05 を保証。
  ===========================================================================
*/
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

#ifndef NDEBUG
#define DEBUG_PRINT(x) (std::cerr << x << '\n')
#else
#define DEBUG_PRINT(x) ((void)0)
#endif

// xoroshiro128+
// using ull = unsigned long long;
// struct FastRand {
//     using result_type = ull;
//     ull s[2];
//     FastRand(ull seed) {
//         s[0] = splitmix64(seed);
//         s[1] = splitmix64(s[0]);
//     }
//     inline ull operator()() {
//         ull s0 = s[0];
//         ull s1 = s[1];
//         ull result = s0 + s1;
//         s1 ^= s0;
//         s[0] = rotl(s0, 55) ^ s1 ^ (s1 << 14);
//         s[1] = rotl(s1, 36);
//         return result;
//     }
//     static constexpr ull min() noexcept { return 0ULL; }
//     static constexpr ull max() noexcept { return 0xFFFFFFFFFFFFFFFFULL; }
//     static inline ull splitmix64(ull &x) {
//         ull z = (x += 0x9e3779b97f4a7c15ULL);
//         z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
//         z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
//         return z ^ (z >> 31);
//     }
//     static inline ull rotl(const ull x, int k) { return (x << k) | (x >> (64 - k)); }
// };

/*============================================================================
  ExtendedLHS
============================================================================*/
class ExtendedLHS {
   public:
    /*----------------------------------------------------------------------
      コンストラクタ
      @param n   : 次元数 (≥1)
      @param seed: 乱数シード (任意)
      計算量 O(n)
    ----------------------------------------------------------------------*/
    explicit ExtendedLHS(std::size_t n, std::uint64_t seed = 0) : n_(n), rng_(seed) {
        if (n_ == 0) throw std::invalid_argument("n must be positive");

        constexpr std::size_t THRESH = 2000;
        const std::size_t SCALE = (n_ < THRESH ? 1 : 2);

        P_ = next_prime(static_cast<uint32_t>(std::max<std::size_t>(11, n_ * SCALE)));
        rows_ = 1ULL * P_ * P_;

        coeff_a_.resize(n_);
        coeff_b_.resize(n_);
        alpha_.resize(n_);
        beta_.resize(n_);
        generate_coeffs();  // (a,b)

        std::uniform_int_distribution<uint32_t> du(0, P_ - 1);
        std::uniform_int_distribution<uint32_t> da(1, P_ - 1);
        for (std::size_t d = 0; d < n_; ++d) {
            alpha_[d] = da(rng_);
            beta_[d] = du(rng_);
        }

        /* 行順 LCG ステップ */
        if (n_ >= THRESH) {
            step_ = (5ULL * P_ + 1) % rows_;  // gcd(step_,P_^2)=1
        } else {
            do step_ = std::uniform_int_distribution<std::uint64_t>(1, rows_ - 1)(rng_);
            while (std::gcd(step_, rows_) != 1);
        }
        cur_row_ = std::uniform_int_distribution<std::uint64_t>(0, rows_ - 1)(rng_);
    }

    /*----------------------------------------------------------------------
      next
      @return 1 クエリ (std::vector<double> サイズ n)
      計算量 O(n)
    ----------------------------------------------------------------------*/
    std::vector<double> next() {
        cur_row_ = (cur_row_ + step_) % rows_;
        const uint32_t i = static_cast<uint32_t>(cur_row_ / P_);
        const uint32_t j = static_cast<uint32_t>(cur_row_ % P_);

        std::vector<double> x;
        x.reserve(n_);
        std::uniform_real_distribution<double> uni(0.0, 1.0);

        for (std::size_t d = 0; d < n_; ++d) {
            const uint64_t tmp1 = static_cast<uint64_t>(coeff_a_[d]) * i + static_cast<uint64_t>(coeff_b_[d]) * j;
            uint32_t s = static_cast<uint32_t>(tmp1 % P_);
            const uint64_t tmp2 = static_cast<uint64_t>(alpha_[d]) * s + beta_[d];
            s = static_cast<uint32_t>(tmp2 % P_);
            x.push_back((static_cast<double>(s) + uni(rng_)) / static_cast<double>(P_));
        }
        return x;
    }

    /*------------------------------------------------------------------*/
    [[nodiscard]] std::size_t dim() const noexcept { return n_; }        // O(1)
    [[nodiscard]] std::uint64_t rows() const noexcept { return rows_; }  // O(1)

   private:
    std::size_t n_;
    uint32_t P_;
    std::uint64_t rows_;
    std::mt19937_64 rng_;
    // FastRand rng_; // FastRandだと速くなるがcorrが増加し、閾値を超えてしまう
    std::uint64_t step_, cur_row_;
    std::vector<uint32_t> coeff_a_, coeff_b_, alpha_, beta_;

    void generate_coeffs() {
        std::size_t idx = 0;
        auto push = [&](uint32_t a, uint32_t b) {
            if (idx < n_) {
                coeff_a_[idx] = a;
                coeff_b_[idx] = b;
            }
            ++idx;
        };
        push(1, 0);
        if (n_ == 1) return;
        push(0, 1);
        for (uint32_t a = 1; idx < n_ && a < P_; ++a) push(a, 1);
        for (uint32_t b = 2; idx < n_ && b < P_; ++b)
            for (uint32_t a = 0; idx < n_ && a < P_; ++a) push(a, b);
        if (idx < n_) throw std::logic_error("n exceeds P^2+1");
    }

    /*----------- 素数ユーティリティ -----------*/
    static bool is_prime(uint32_t x) {
        if (x < 2) return false;
        if (x % 2 == 0) return x == 2;
        for (uint32_t p = 3; p * 1uLL * p <= x; p += 2)
            if (x % p == 0) return false;
        return true;
    }
    static uint32_t next_prime(uint32_t x) {
        while (!is_prime(x)) ++x;
        return x;
    }
};

/*===================== テスト & ベンチ補助 =====================*/
static double pearson(const std::vector<double>& a, const std::vector<double>& b) {
    const std::size_t m = a.size();
    const double dm = static_cast<double>(m);
    double sa = std::accumulate(a.begin(), a.end(), 0.0);
    double sb = std::accumulate(b.begin(), b.end(), 0.0);
    double saa = 0.0, sbb = 0.0, sab = 0.0;
    for (std::size_t k = 0; k < m; ++k) {
        saa += a[k] * a[k];
        sbb += b[k] * b[k];
        sab += a[k] * b[k];
    }
    double cov = sab * dm - sa * sb;
    double var_a = saa * dm - sa * sa;
    double var_b = sbb * dm - sb * sb;
    if (var_a <= 0 || var_b <= 0) return 0.0;
    return cov / std::sqrt(var_a * var_b);
}
constexpr double CORR_TOL = 0.05;

/*---------- 標準テスト・ベンチ（省略せずそのまま） ----------*/
static void test_small_cases() {
    std::cout << "=== Small / Edge tests ===\n";
    {
        ExtendedLHS gen(1, 42);
        auto v = gen.next();
        if (v.size() != 1 || !(0.0 <= v[0] && v[0] < 1.0)) std::cerr << "n=1 failed\n";
    }
    {
        ExtendedLHS gen(2, 43);
        const std::size_t M = std::min<std::size_t>(500, gen.rows());
        std::vector<double> xs, ys;
        xs.reserve(M);
        ys.reserve(M);
        for (std::size_t i = 0; i < M; ++i) {
            auto q = gen.next();
            xs.push_back(q[0]);
            ys.push_back(q[1]);
        }
        std::cout << "n=2 corr = " << pearson(xs, ys) << '\n';
    }
}

static void correlation_check(std::size_t n) {
    ExtendedLHS gen(n, 555);
    const std::size_t M = std::min<std::size_t>(10000ULL, gen.rows());
    const std::size_t D = std::min<std::size_t>(20, n);
    std::vector<std::vector<double>> buf(D, std::vector<double>(M));
    for (std::size_t i = 0; i < M; ++i) {
        auto q = gen.next();
        for (std::size_t d = 0; d < D; ++d) buf[d][i] = q[d];
    }
    double maxc = 0.0;
    for (std::size_t a = 0; a < D; ++a)
        for (std::size_t b = a + 1; b < D; ++b) maxc = std::max(maxc, std::abs(pearson(buf[a], buf[b])));
    std::cout << "n=" << n << " (first " << D << " dims) max|corr|=" << maxc << '\n';
    if (maxc > CORR_TOL) std::cerr << "Correlation too high for n=" << n << '\n';
}

static void bench(std::size_t n, std::size_t iters) {
    ExtendedLHS gen(n, 777);
    std::vector<double> tmp;
    auto st = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iters; ++i) tmp = gen.next();
    auto ed = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(ed - st).count();
    std::cout << std::fixed << std::setprecision(3) << "n=" << n << "  iters=" << iters << "  total=" << ms << " ms"
              << "  avg=" << (ms * 1000.0 / static_cast<double>(iters)) << " µs/iter\n";
}

/*---------- 行プレビュー ----------*/
static void show_first_rows(std::size_t n) {
    std::cout << "\n=== Preview n=" << n << " : first 16 rows ===\n";
    ExtendedLHS gen(n, 123);
    std::cout << std::fixed << std::setprecision(6);
    for (int r = 0; r < 16; ++r) {
        auto q = gen.next();
        std::cout << "row" << std::setw(2) << r << " :";
        for (double v : q) std::cout << ' ' << v;
        std::cout << '\n';
    }
}

/*=========================== main ===========================*/
int main() {
    std::ios::sync_with_stdio(false);
    std::cout << std::fixed << std::setprecision(6);

    test_small_cases();
    correlation_check(10);
    correlation_check(100);
    correlation_check(1000);
    correlation_check(10000);

    std::cout << "\n=== Benchmarks ===\n";
    bench(10, 100'000);
    bench(100, 100'000);
    bench(1000, 50'000);
    bench(10000, 20'000);

    for (std::size_t n : {1, 2, 4, 8, 16}) show_first_rows(n);
}

// 実行結果
// === Small / Edge tests ===
// n=2 corr = -0.003197
// n=10 (first 10 dims) max|corr|=0.023357
// n=100 (first 20 dims) max|corr|=0.004956
// n=1000 (first 20 dims) max|corr|=0.032211
// n=10000 (first 20 dims) max|corr|=0.031006

// === Benchmarks ===
// n=10  iters=100000  total=10.720 ms  avg=0.107 µs/iter
// n=100  iters=100000  total=89.563 ms  avg=0.896 µs/iter
// n=1000  iters=50000  total=439.508 ms  avg=8.790 µs/iter
// n=10000  iters=20000  total=1752.620 ms  avg=87.631 µs/iter

// === Preview n=1 : first 16 rows ===
// row 0 : 0.108401
// row 1 : 0.745013
// row 2 : 0.450087
// row 3 : 0.390040
// row 4 : 0.074462
// row 5 : 0.680746
// row 6 : 0.660953
// row 7 : 0.325997
// row 8 : 0.913991
// row 9 : 0.596211
// row10 : 0.603595
// row11 : 0.270434
// row12 : 0.895545
// row13 : 0.508543
// row14 : 0.484094
// row15 : 0.167042

// === Preview n=2 : first 16 rows ===
// row 0 : 0.086451 0.480950
// row 1 : 0.801735 0.317110
// row 2 : 0.479135 0.144179
// row 3 : 0.186719 0.959847
// row 4 : 0.330868 0.815888
// row 5 : 0.077363 0.599453
// row 6 : 0.756821 0.439769
// row 7 : 0.463724 0.254764
// row 8 : 0.198139 0.003833
// row 9 : 0.961236 0.898462
// row10 : 0.065341 0.662424
// row11 : 0.727400 0.540022
// row12 : 0.501384 0.350306
// row13 : 0.238421 0.173742
// row14 : 0.949712 0.974060
// row15 : 0.062789 0.729749

// === Preview n=4 : first 16 rows ===
// row 0 : 0.507816 0.459446 0.232574 0.603595
// row 1 : 0.361343 0.168273 0.872180 0.302276
// row 2 : 0.530678 0.736451 0.709310 0.834503
// row 3 : 0.276560 0.415781 0.353007 0.610796
// row 4 : 0.116970 0.000128 0.994567 0.319566
// row 5 : 0.350306 0.692967 0.810106 0.858803
// row 6 : 0.155878 0.335516 0.366113 0.550874
// row 7 : 0.944615 0.930392 0.001091 0.322060
// row 8 : 0.107212 0.570198 0.831613 0.876043
// row 9 : 0.919640 0.219676 0.533644 0.581667
// row10 : 0.798152 0.903647 0.091456 0.326630
// row11 : 0.967731 0.539802 0.969219 0.855772
// row12 : 0.806170 0.138737 0.621907 0.596120
// row13 : 0.919906 0.763078 0.393941 0.102404
// row14 : 0.813975 0.395082 0.049622 0.862037
// row15 : 0.594944 0.021349 0.648330 0.618098

// === Preview n=8 : first 16 rows ===
// row 0 : 0.029549 0.985223 0.554633 0.800219 0.652685 0.367469 0.233963 0.989371
// row 1 : 0.701705 0.571515 0.091037 0.630931 0.501384 0.441215 0.147512 0.719197
// row 2 : 0.949712 0.246787 0.517335 0.547931 0.823601 0.308251 0.566756 0.546545
// row 3 : 0.231151 0.834485 0.842925 0.558885 0.239679 0.192367 0.037858 0.533644
// row 4 : 0.854394 0.525425 0.449101 0.364183 0.053903 0.240458 0.994347 0.241947
// row 5 : 0.128499 0.169806 0.775100 0.440089 0.414302 0.101725 0.399442 0.121214
// row 6 : 0.738767 0.813975 0.304173 0.231440 0.225673 0.140399 0.294076 0.830148
// row 7 : 0.072644 0.420591 0.678246 0.209889 0.573022 0.035479 0.754876 0.761368
// row 8 : 0.341048 0.055770 0.059312 0.254257 0.950553 0.916118 0.192375 0.665037
// row 9 : 0.966703 0.653835 0.572595 0.050622 0.807638 0.944805 0.163694 0.377163
// row10 : 0.256065 0.334717 0.979594 0.065328 0.164760 0.903182 0.627372 0.362670
// row11 : 0.475729 0.934375 0.273274 0.064258 0.486102 0.811438 0.074032 0.205433
// row12 : 0.095375 0.596480 0.845966 0.890071 0.342860 0.768914 0.930521 0.924061
// row13 : 0.381512 0.192280 0.249905 0.903312 0.712938 0.718056 0.423406 0.845943
// row14 : 0.679442 0.827578 0.631952 0.860891 0.082786 0.601071 0.896799 0.803591
// row15 : 0.338670 0.539997 0.168792 0.665420 0.861107 0.571463 0.801198 0.474742

// === Preview n=16 : first 16 rows ===
// row 0 : 0.732166 0.689097 0.570040 0.119250 0.532918 0.434751 0.249077 0.530117 0.326039 0.598784 0.427775 0.067514 0.743322 0.183296 0.848025 0.110005
// row 1 : 0.258725 0.104687 0.290595 0.353295 0.505466 0.155590 0.819872 0.862436 0.200793 0.403992 0.207418 0.520057 0.326901 0.948175 0.081992 0.490197
// row 2 : 0.125085 0.526690 0.196818 0.502697 0.910730 0.032023 0.602049 0.007743 0.870534 0.978029 0.027100 0.665222 0.370779 0.199427 0.076685 0.316179
// row 3 : 0.691266 0.918439 0.920731 0.752754 0.850358 0.710430 0.124478 0.312671 0.743161 0.717187 0.782267 0.091579 0.993177 0.964286 0.341214 0.714635
// row 4 : 0.577454 0.334229 0.869149 0.865801 0.283080 0.643235 0.994182 0.528786 0.366648 0.310478 0.588589 0.276873 0.020419 0.230930 0.342021 0.544692
// row 5 : 0.414654 0.738899 0.782684 0.987693 0.692439 0.497533 0.778573 0.656745 0.011566 0.889122 0.455821 0.466849 0.108372 0.464625 0.332792 0.370904
// row 6 : 0.969051 0.123727 0.526557 0.204106 0.641803 0.212458 0.344988 0.990559 0.925022 0.702351 0.226865 0.842331 0.674834 0.193300 0.577246 0.777775
// row 7 : 0.850057 0.556882 0.445893 0.350005 0.048409 0.071676 0.147940 0.156162 0.582901 0.274331 0.044896 0.035808 0.764665 0.411825 0.568890 0.590903
// row 8 : 0.404676 0.967266 0.167876 0.565755 0.984930 0.790736 0.694591 0.430391 0.439850 0.015065 0.818857 0.463292 0.321497 0.191746 0.808008 0.027542
// row 9 : 0.247611 0.411632 0.095771 0.669898 0.361452 0.673431 0.474005 0.643873 0.067530 0.628163 0.642087 0.633352 0.410380 0.451482 0.804215 0.863586
// row10 : 0.820047 0.787246 0.807988 0.915124 0.336876 0.406035 0.023595 0.906849 0.948248 0.400780 0.373967 0.021942 0.985971 0.186006 0.043366 0.283570
// row11 : 0.676370 0.183956 0.732383 0.038223 0.715707 0.291560 0.828972 0.111139 0.616688 0.958966 0.184317 0.223818 0.040239 0.460275 0.040305 0.109090
// row12 : 0.551210 0.616012 0.664482 0.125801 0.127357 0.151400 0.682085 0.245503 0.275322 0.579049 0.050184 0.410705 0.072143 0.656574 0.049380 0.938271
// row13 : 0.112869 0.019928 0.365041 0.375295 0.113615 0.855787 0.215629 0.582023 0.148548 0.325670 0.787592 0.808016 0.676218 0.458995 0.279365 0.334604
// row14 : 0.941754 0.415193 0.326080 0.477162 0.499782 0.757029 0.014582 0.712068 0.767551 0.884780 0.643781 0.988294 0.741042 0.653772 0.261832 0.165340
// row15 : 0.479434 0.859184 0.018034 0.750323 0.461190 0.453848 0.573357 0.015812 0.683208 0.697022 0.364743 0.358923 0.308159 0.463336 0.491900 0.557562
