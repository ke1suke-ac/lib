/**********************************************************************
 *  fast_roulette.hpp  ―  16-bit 総和前提の極限高速ルーレット選択
 *
 *  ▷ roulette8 (長さ 8 専用)
 *  ▷ roulette16(長さ 16 専用)
 *  ▷ roulette_any(任意長 fallback)
 *  ▷ roulette   (サイズ分岐フロント)
 *
 *  ■ 引数   : std::span<const int> または int 配列ポインタ
 *  ■ 戻り値 : 選択されたインデックス (size_t)
 *  ■ 制約   : すべての重みは非負、総和 total は 1〜65 535 に収まる
 *
 *  乱数には PCG-XSH-RR 32-bit を 1 call 使用。
 *********************************************************************/
#include <bits/stdc++.h>
using namespace std;

/*====================================================================
  PCG-32 乱数生成器（32-bit／1 call）
====================================================================*/
struct pcg32_fast {
    using result_type = uint32_t;
    uint64_t state = 0x853c49e6748fea9bULL,
             inc   = 0xda3e39cb94b95bdbULL;
    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return UINT32_MAX; }
    uint32_t operator()() noexcept {
        uint64_t old = state;
        state = old * 6364136223846793005ULL + inc;
        uint32_t x   = uint32_t(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = old >> 59u;
        return (x >> rot) | (x << ((-rot) & 31));
    }
};
inline pcg32_fast& rng() {
    thread_local pcg32_fast g{
        .state = (reinterpret_cast<uintptr_t>(&g) ^ 0x9e3779b97f4a7c15ULL) + 0x632be5ab};
    return g;
}

/**********************************************************************
 *  roulette8
 *    引数   : const int* w (要素数 8)
 *    戻り値 : 選択インデックス (0–7)
 *    制約   : 総和 1–65 535
 *********************************************************************/
[[gnu::always_inline]]
inline std::size_t roulette8(const int* __restrict w) {
    uint32_t total =
        uint32_t(w[0]) + uint32_t(w[1]) + uint32_t(w[2]) + uint32_t(w[3]) +
        uint32_t(w[4]) + uint32_t(w[5]) + uint32_t(w[6]) + uint32_t(w[7]);

    uint32_t r = (uint64_t(rng()()) * total) >> 32;

    uint32_t acc = 0;
#define STEP(i) acc += uint32_t(w[i]); if (r < acc) return i
    STEP(0); STEP(1); STEP(2); STEP(3);
    STEP(4); STEP(5); STEP(6);
    return 7;
#undef STEP
}

/**********************************************************************
 *  roulette16
 *    引数   : const int* w (要素数 16)
 *    戻り値 : 選択インデックス (0–15)
 *    制約   : 総和 1–65 535
 *********************************************************************/
[[gnu::always_inline]]
inline std::size_t roulette16(const int* __restrict w) {
    uint32_t total =
        uint32_t(w[0])  + uint32_t(w[1])  + uint32_t(w[2])  + uint32_t(w[3]) +
        uint32_t(w[4])  + uint32_t(w[5])  + uint32_t(w[6])  + uint32_t(w[7]) +
        uint32_t(w[8])  + uint32_t(w[9])  + uint32_t(w[10]) + uint32_t(w[11])+
        uint32_t(w[12]) + uint32_t(w[13]) + uint32_t(w[14]) + uint32_t(w[15]);

    uint32_t r = (uint64_t(rng()()) * total) >> 32;

    uint32_t acc = 0;
#define STEP(i) acc += uint32_t(w[i]); if (r < acc) return i
    STEP(0);  STEP(1);  STEP(2);  STEP(3);
    STEP(4);  STEP(5);  STEP(6);  STEP(7);
    STEP(8);  STEP(9);  STEP(10); STEP(11);
    STEP(12); STEP(13); STEP(14);
    return 15;
#undef STEP
}

/**********************************************************************
 *  roulette_any
 *    引数   : std::span<const int> w (長さ任意)
 *    戻り値 : 選択インデックス
 *    制約   : 総和 1–65 535
 *********************************************************************/
inline std::size_t roulette_any(std::span<const int> w) {
    uint32_t total = 0;
    for (int v : w) total += uint32_t(v);

    uint32_t r = (uint64_t(rng()()) * total) >> 32;

    uint32_t acc = 0;
    for (std::size_t i = 0, n = w.size(); i < n; ++i) {
        acc += uint32_t(w[i]);
        if (r < acc) return i;
    }
    return w.size() - 1;
}

/**********************************************************************
 *  roulette
 *    引数   : std::span<const int> w
 *    戻り値 : 選択インデックス
 *    制約   : 総和 1–65 535
 *********************************************************************/
inline std::size_t roulette(std::span<const int> w) {
    if (w.size() == 8)  return roulette8 (w.data());
    if (w.size() == 16) return roulette16(w.data());
    return roulette_any(w);
}

/*====================================================================
  －－ 以下はベンチ & 一様性テスト －－
  無効化したい場合は #if 1 を 0 に変更
====================================================================*/
#if 0
template<class Vec, class F>
void bench(const char* tag, F f, const std::vector<Vec>& data) {
    volatile std::size_t sink = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (auto& v : data) sink += f(std::span<const int>(v));
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - t0).count();
    std::cout << std::left << std::setw(20) << tag << ": "
              << std::setw(8) << us << " µs  ("
              << std::fixed << std::setprecision(2)
              << (1e6 * data.size() / us) / 1e6 << " M call/s)\n";
}

template<class F>
void chi2(const char* name, F f, std::size_t trials, std::size_t len) {
    std::vector<int> ones(len, 1);
    std::vector<uint32_t> freq(len, 0);
    for (std::size_t i = 0; i < trials; ++i)
        ++freq[f(std::span<const int>(ones))];
    double exp = double(trials) / len, chi = 0;
    for (auto v : freq) { double d = v - exp; chi += d * d / exp; }
    std::cout << name << " χ² = " << chi << '\n';
}

int main() {
    constexpr std::size_t CASES  = 1'000'000;
    constexpr std::size_t TRIALS = 10'000'000;

    std::uniform_int_distribution<int> dist(1, 100);
    std::vector<std::array<int,8>>  d8 (CASES);
    std::vector<std::array<int,16>> d16(CASES);
    std::vector<std::array<int,12>> d12(CASES);
    for (auto& v : d8 )  for (int& x : v) x = dist(rng());
    for (auto& v : d16) for (int& x : v) x = dist(rng());
    for (auto& v : d12) for (int& x : v) x = dist(rng());

    std::cout << "== benchmark (total ≤ 65535) ==\n";
    bench("roulette8"      , [](auto w){ return roulette8 (w.data()); }, d8 );
    bench("roulette16"     , [](auto w){ return roulette16(w.data()); }, d16);
    bench("roulette_any len12", roulette_any, std::vector<std::array<int,12>>(d12.begin(), d12.end()));

    std::cout << "\n== uniformity χ² (1e7 trials) ==\n";
    chi2("roulette8" , [](auto w){ return roulette8 (w.data()); }, TRIALS, 8 );
    chi2("roulette16", [](auto w){ return roulette16(w.data()); }, TRIALS, 16);
    chi2("roulette_any len12", roulette_any, TRIALS, 12);
}
#endif

// 実行結果
// == benchmark (total ≤ 65535) ==
// roulette8           : 11206    µs  (89.24 M call/s)
// roulette16          : 18689    µs  (53.51 M call/s)
// roulette_any len12  : 20206    µs  (49.49 M call/s)

// == uniformity χ² (1e7 trials) ==
// roulette8 χ² = 6.20
// roulette16 χ² = 14.67
// roulette_any len12 χ² = 7.74
