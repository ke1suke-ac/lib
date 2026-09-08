// Thompson Sampling の方が UCB1 より収束が速く、また動作も高速
// また、時間経過による状況の変化にも対応しやすい
// ただし、Thompson Samplingは報酬 true/false にのみ対応。UCB1は[0.0-1.0]の報酬に対応


// -------------------------------------------------
// ヒューリスティックコンテスト用・高速 Thompson Sampling ライブラリ＋テストスイート
//   * Vanilla Thompson Sampling（ベータ–ベルヌーイ）
//   * 時間割引付き Thompson Sampling
//   * スライディングウィンドウ Thompson Sampling
// すべてヘッダオンリー／単一ファイルで完結。
// -------------------------------------------------

#include <bits/stdc++.h>
using namespace std;

/*==================================================
  1. 高速 RNG （xoshiro128++）
==================================================*/
struct Xoshiro128PP {
    using result_type = uint32_t;
    uint32_t s[4];
    static constexpr result_type min() { return 0u; }
    static constexpr result_type max() { return UINT32_MAX; }
    Xoshiro128PP(uint64_t seed = 88172645463325252ull) { reseed(seed); }
    static uint64_t splitmix64(uint64_t &x) {
        uint64_t z = (x += 0x9e3779b97f4a7c15ull);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
    void reseed(uint64_t seed) {
        for (int i = 0; i < 4; ++i) {
            s[i] = static_cast<uint32_t>(splitmix64(seed));
        }
    }
    inline result_type operator()() {
        uint32_t const result = std::rotl(s[0] + s[3], 7) + s[0];
        uint32_t const t = s[1] << 9;
        s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
        s[2] ^= t; s[3] = std::rotl(s[3], 11);
        return result;
    }
    inline double uniform01() { return (*this)() * (1.0 / 4294967296.0); }
};

/*==================================================
  2. 共通ユーティリティ
==================================================*/
#define ASSERT(expr, msg)                                                                            \
    do {                                                                                            \
        if (!(expr)) {                                                                              \
            cerr << "[ASSERT] " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")\n";   \
            std::exit(EXIT_FAILURE);                                                                 \
        }                                                                                           \
    } while (0)

/*==================================================
  3. Thompson Sampling 基底テンプレート（静的多態）
==================================================*/
namespace ts {

// Beta(a,b) を Gamma サンプリングで生成（a,b > 0）
template <class RNG>
inline double sample_beta(RNG &rng, double a, double b) {
    std::gamma_distribution<double> gd_a(a, 1.0), gd_b(b, 1.0);
    double x = gd_a(rng);
    double y = gd_b(rng);
    return x / (x + y);
}

/*------------------------------
   3‑A. 標準 Thompson Sampling
   10 k ステップで 97〜99 % をベスト腕に集中。
   収束スピードは極めて速く、他腕はほぼ試行されません。
------------------------------*/
template <class RNG = Xoshiro128PP>
class ThompsonSampling {
    size_t n;                    // 腕の数
    vector<int> succ, fail;      // 成功 / 失敗回数
    RNG &rng;
public:
    ThompsonSampling(size_t n_, RNG &rng_) : n(n_), succ(n, 0), fail(n, 0), rng(rng_) {}
    inline size_t select() {
        size_t best = 0;
        double best_val = -1.0;
        for (size_t i = 0; i < n; ++i) {
            double theta = sample_beta(rng, succ[i] + 1.0, fail[i] + 1.0);
            if (theta > best_val) { best_val = theta; best = i; }
        }
        return best;
    }
    inline void update(size_t arm, bool reward) {
        if (reward) ++succ[arm]; else ++fail[arm];
    }
};

/*-------------------------------------------
   3‑B. 時間割引付き Thompson Sampling
     ‑ 係数 gamma (0 < gamma ≤ 1) で逐次割引

    常環境では収束が甘くなるが、報酬分布がドリフトするときは素早く追従できます。
    ベスト腕の選択比率は 35〜45 % 程度 に落ち着き、次点腕もそこそこ選ばれています。
    例：Iter 1 ではベスト腕(3) 35 %、次点(4) 24 %、その他 6〜20 %。
-------------------------------------------*/
template <class RNG = Xoshiro128PP>
class DiscountedThompsonSampling {
    size_t n;                     // 腕の数
    double gamma;                 // 割引係数
    vector<double> succ, fail;    // 浮動小数で保持
    RNG &rng;
public:
    DiscountedThompsonSampling(size_t n_, double gamma_, RNG &rng_)
        : n(n_), gamma(gamma_), succ(n, 0.0), fail(n, 0.0), rng(rng_) {}
    inline size_t select() {
        size_t best = 0;
        double best_val = -1.0;
        for (size_t i = 0; i < n; ++i) {
            double theta = sample_beta(rng, succ[i] + 1.0, fail[i] + 1.0);
            if (theta > best_val) { best_val = theta; best = i; }
        }
        return best;
    }
    inline void update(size_t arm, bool reward) {
        // 全腕を割引 (少数腕なら O(n) でも十分高速)
        for (size_t i = 0; i < n; ++i) {
            succ[i] *= gamma;
            fail[i] *= gamma;
        }
        if (reward) succ[arm] += 1.0; else fail[arm] += 1.0;
    }
};

/*-------------------------------------------
   3‑C. スライディングウィンドウ Thompson Sampling
   ベスト腕集中率は Vanilla に近いが、探索回数がわずかに増加。
   例：Iter 1 でベスト腕 92 %、次点腕(4) 2.5 %。
   まれに窓幅より長い周期で確率が接近すると、一時的に他腕を多く引くケースもあり得ます。
-------------------------------------------*/
template <class RNG = Xoshiro128PP>
class WindowThompsonSampling {
    size_t n;                       // 腕の数
    size_t W;                       // 窓幅
    vector<int> succ, fail;         // 現在窓内の成功 / 失敗
    vector<vector<int8_t>> buf;     // ring buffer: {0,1}
    vector<size_t> idx;             // 次に書き込む位置
    RNG &rng;
public:
    WindowThompsonSampling(size_t n_, size_t W_, RNG &rng_)
        : n(n_), W(W_), succ(n, 0), fail(n, 0), buf(n, vector<int8_t>(W, -1)), idx(n, 0), rng(rng_) {}
    inline size_t select() {
        size_t best = 0;
        double best_val = -1.0;
        for (size_t i = 0; i < n; ++i) {
            double theta = sample_beta(rng, succ[i] + 1.0, fail[i] + 1.0);
            if (theta > best_val) { best_val = theta; best = i; }
        }
        return best;
    }
    inline void update(size_t arm, bool reward) {
        // 古い値を取り除く
        int8_t &slot = buf[arm][idx[arm]];
        if (slot != -1) {
            if (slot) --succ[arm]; else --fail[arm];
        }
        // 新しい値を挿入
        slot = reward ? 1 : 0;
        if (reward) ++succ[arm]; else ++fail[arm];
        // ポインタ進行
        idx[arm] = (idx[arm] + 1) % W;
    }
};

} // namespace ts

/*==================================================
  4. テストスイート
==================================================*/
namespace test {

using RNG = Xoshiro128PP;

// ------------------------------------------------- //
// 汎用ユーティリティ
// ------------------------------------------------- //
struct Timer {
    using clock = chrono::high_resolution_clock;
    chrono::time_point<clock> st;
    Timer() : st(clock::now()) {}
    double elapsed_ms() const {
        return chrono::duration<double, std::milli>(clock::now() - st).count();
    }
};

// 成功：."」と表示
inline void ok(const string &name) {
    cerr << "[PASS] " << name << "\n";
}

// ------------------------------------------------- //
// 4‑A. ベーシックユニットテスト
// ------------------------------------------------- //
void basic_selection_order() {
    RNG rng(123456);
    ts::ThompsonSampling<RNG> algo(3, rng);
    // 腕0を常に成功、他を常に失敗させる
    for (int t = 0; t < 5000; ++t) {
        size_t a = algo.select();
        algo.update(a, a == 0);
    }
    ASSERT(algo.select() == 0, "basic_selection_order");
    ok("basic_selection_order");
}

void single_arm_degeneracy() {
    RNG rng(42);
    ts::ThompsonSampling<RNG> algo(1, rng);
    for (int t = 0; t < 100; ++t) {
        size_t a = algo.select();
        ASSERT(a == 0, "single_arm_degeneracy select");
        algo.update(0, true);
    }
    ok("single_arm_degeneracy");
}

void zero_reward_stability() {
    RNG rng(99);
    ts::ThompsonSampling<RNG> algo(4, rng);
    for (int t = 0; t < 400; ++t) {
        size_t a = algo.select();
        algo.update(a, false); // 全失敗
    }
    // すべて均等に近いはず
    ok("zero_reward_stability");
}

// ------------------------------------------------- //
// 4‑B. ランダム隠れ確率シナリオテスト
//      各実装 × 10回イテレーション
// ------------------------------------------------- //

template <class AlgoMaker>
void hidden_probability_scenario(const string &name, AlgoMaker make) {
    RNG rng_master(20250417u);
    uniform_real_distribution<double> uni(0.1, 0.9);
    constexpr int NUM_TRIAL = 10;
    constexpr int N_ARMS = 5;
    constexpr int T_STEPS = 10000;
    cerr << "\n==== " << name << " hidden‑prob test ====" << endl;
    for (int iter = 1; iter <= NUM_TRIAL; ++iter) {
        // ランダム隠れ確率
        vector<double> p(N_ARMS);
        for (double &v : p) v = uni(rng_master);
        size_t best_arm = max_element(p.begin(), p.end()) - p.begin();
        // アルゴリズム初期化
        RNG rng_iter(rng_master());
        auto algo = make(rng_iter);
        vector<int> cnt(N_ARMS, 0);
        // シミュレーション
        for (int t = 0; t < T_STEPS; ++t) {
            size_t a = algo.select();
            bool reward = (rng_iter.uniform01() < p[a]);
            algo.update(a, reward);
            ++cnt[a];
        }
        size_t picked_best = max_element(cnt.begin(), cnt.end()) - cnt.begin();
        ASSERT(picked_best == best_arm, name + " best‑arm dominance");
        // 結果表示
        cerr << "[Iter " << iter << "] probs:";
        for (double v : p) cerr << " " << fixed << setprecision(3) << v;
        cerr << " | counts:";
        for (int c : cnt) cerr << " " << c;
        cerr << " | best=" << best_arm << "\n";
    }
    ok(name);
}

void run_all() {
    basic_selection_order();
    single_arm_degeneracy();
    zero_reward_stability();

    // シナリオテスト
    hidden_probability_scenario("VanillaTS", [](RNG &rng) {
        return ts::ThompsonSampling<RNG>(5, rng);
    });
    hidden_probability_scenario("DiscountedTS", [](RNG &rng) {
        return ts::DiscountedThompsonSampling<RNG>(5, 0.97, rng);
    });
    hidden_probability_scenario("WindowTS", [](RNG &rng) {
        return ts::WindowThompsonSampling<RNG>(5, 256, rng);
    });
}

} // namespace test

/*==================================================
  5. main()
==================================================*/
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    test::Timer tim;
    test::run_all();
    cerr << "\nAll tests completed successfully in " << tim.elapsed_ms() << " ms\n";
    return 0;
}

// 実行結果
// [PASS] basic_selection_order
// [PASS] single_arm_degeneracy
// [PASS] zero_reward_stability

// ==== VanillaTS hidden‑prob test ====
// [Iter 1] probs: 0.646 0.726 0.397 0.816 0.752 | counts: 38 186 40 9596 140 | best=3
// [Iter 2] probs: 0.493 0.849 0.650 0.337 0.328 | counts: 18 9922 41 11 8 | best=1
// [Iter 3] probs: 0.275 0.587 0.708 0.751 0.137 | counts: 8 24 153 9808 7 | best=3
// [Iter 4] probs: 0.429 0.835 0.409 0.150 0.708 | counts: 13 9852 20 5 110 | best=1
// [Iter 5] probs: 0.612 0.496 0.828 0.687 0.602 | counts: 12 19 9776 144 49 | best=2
// [Iter 6] probs: 0.526 0.373 0.393 0.800 0.143 | counts: 51 15 9 9920 5 | best=3
// [Iter 7] probs: 0.634 0.623 0.349 0.815 0.652 | counts: 27 30 10 9802 131 | best=3
// [Iter 8] probs: 0.211 0.265 0.840 0.484 0.555 | counts: 11 7 9940 18 24 | best=2
// [Iter 9] probs: 0.640 0.140 0.861 0.435 0.320 | counts: 18 7 9946 20 9 | best=2
// [Iter 10] probs: 0.666 0.605 0.155 0.108 0.301 | counts: 9760 196 17 11 16 | best=0
// [PASS] VanillaTS

// ==== DiscountedTS hidden‑prob test ====
// [Iter 1] probs: 0.646 0.726 0.397 0.816 0.752 | counts: 1499 1973 574 3523 2431 | best=3
// [Iter 2] probs: 0.493 0.849 0.650 0.337 0.328 | counts: 765 6801 1361 544 529 | best=1
// [Iter 3] probs: 0.275 0.587 0.708 0.751 0.137 | counts: 610 1607 3151 4133 499 | best=3
// [Iter 4] probs: 0.429 0.835 0.409 0.150 0.708 | counts: 724 6332 655 433 1856 | best=1
// [Iter 5] probs: 0.612 0.496 0.828 0.687 0.602 | counts: 1067 811 5307 1587 1228 | best=2
// [Iter 6] probs: 0.526 0.373 0.393 0.800 0.143 | counts: 1070 692 736 7026 476 | best=3
// [Iter 7] probs: 0.634 0.623 0.349 0.815 0.652 | counts: 1445 1176 605 5155 1619 | best=3
// [Iter 8] probs: 0.211 0.265 0.840 0.484 0.555 | counts: 475 496 7246 737 1046 | best=2
// [Iter 9] probs: 0.640 0.140 0.861 0.435 0.320 | counts: 1340 410 7102 638 510 | best=2
// [Iter 10] probs: 0.666 0.605 0.155 0.108 0.301 | counts: 4894 2901 648 607 950 | best=0
// [PASS] DiscountedTS

// ==== WindowTS hidden‑prob test ====
// [Iter 1] probs: 0.646 0.726 0.397 0.816 0.752 | counts: 47 462 41 9198 252 | best=3
// [Iter 2] probs: 0.493 0.849 0.650 0.337 0.328 | counts: 15 9924 44 9 8 | best=1
// [Iter 3] probs: 0.275 0.587 0.708 0.751 0.137 | counts: 6 22 250 9715 7 | best=3
// [Iter 4] probs: 0.429 0.835 0.409 0.150 0.708 | counts: 22 9545 24 6 403 | best=1
// [Iter 5] probs: 0.612 0.496 0.828 0.687 0.602 | counts: 17 16 9796 125 46 | best=2
// [Iter 6] probs: 0.526 0.373 0.393 0.800 0.143 | counts: 61 13 16 9904 6 | best=3
// [Iter 7] probs: 0.634 0.623 0.349 0.815 0.652 | counts: 25 30 10 9650 285 | best=3
// [Iter 8] probs: 0.211 0.265 0.840 0.484 0.555 | counts: 8 7 9949 18 18 | best=2
// [Iter 9] probs: 0.640 0.140 0.861 0.435 0.320 | counts: 26 7 9942 21 4 | best=2
// [Iter 10] probs: 0.666 0.605 0.155 0.108 0.301 | counts: 9291 663 18 11 17 | best=0
// [PASS] WindowTS

// All tests completed successfully in 150.066 ms
