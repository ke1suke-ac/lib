/*
  ============================================================
    weighted_roulette.cpp  ― テスト & ベンチマーク
  ============================================================
*/
#include "weighted_roulette.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cassert>
#include <random>

/* ---- 期待確率通りに動くかを χ² 統計量で粗く検証 ---- */
void test_distribution() {
    std::cout << "[TEST] distribution check ... ";
    const std::size_t N = 10;
    std::vector<double> base{1, 3, 5, 2, 4, 6, 7, 9, 8, 10};
    WeightedRoulette ru(base);

    constexpr std::size_t TRIAL = 1'000'000;
    std::vector<std::size_t> cnt(N);
    for (std::size_t i = 0; i < TRIAL; ++i) ++cnt[ru.sample()];

    double chi2 = 0.0, tot = ru.total_weight();
    for (std::size_t i = 0; i < N; ++i) {
        double exp = TRIAL * base[i] / tot;
        double diff = static_cast<double>(cnt[i]) - exp;   // ← 型変換警告を回避
        chi2 += diff * diff / exp;
    }
    assert(chi2 < 25.0 && "分布が期待値から外れすぎています");
    std::cout << "passed (chi2=" << chi2 << ")\n";
}

/* ---- エッジケース一式 ---- */
void test_edge_cases() {
    std::cout << "[TEST] edge cases ... ";

    /* 1 要素 */
    {
        WeightedRoulette one({5.0});
        for (int i = 0; i < 100; ++i) assert(one.sample() == 0);
    }
    /* 0 重み → 更新で非 0 に */
    {
        WeightedRoulette ru({0.0, 1.0});
        for (int i = 0; i < 50; ++i) assert(ru.sample() == 1);
        ru.set_weight(0, 2.0);
        bool hit0 = false, hit1 = false;
        for (int i = 0; i < 200; ++i) {
            std::size_t id = ru.sample();
            hit0 |= (id == 0);
            hit1 |= (id == 1);
        }
        assert(hit0 && hit1);
    }
    /* 全 0 → 例外 */
    {
        WeightedRoulette ru({0.0, 0.0});
        bool thrown = false;
        try { ru.sample(); } catch (const std::runtime_error&) { thrown = true; }
        assert(thrown);
    }
    std::cout << "passed\n";
}

/* ---- ベンチマークユーティリティ ---- */
template < class F >
double bench(std::size_t iter, F&& fn) {
    auto beg = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iter; ++i) fn();
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(end - beg).count();
}

void benchmark() {
    constexpr std::size_t N    = 1'000;
    constexpr std::size_t ITER = 100'000'000;

    std::vector<double> init_w(N, 1.0);
    WeightedRoulette ru(init_w);

    /* --- select --- */
    double t_sel = bench(ITER, [&] { ru.sample(); });

    /* --- update --- */
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<std::size_t> pos(0, N - 1);
    std::uniform_real_distribution<double> upd(0.0, 10.0);
    double t_upd = bench(ITER, [&] {
        ru.set_weight(pos(rng), upd(rng));
    });

    std::cout << "\n=== Benchmarks (N=" << N << ", ITER=" << ITER << ") ===\n";
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "select : " << t_sel << " s  | "
              << t_sel / ITER * 1e6 << " µs / op\n";
    std::cout << "update : " << t_upd << " s  | "
              << t_upd / ITER * 1e6 << " µs / op\n";
}

int main() {
    test_edge_cases();
    test_distribution();
    benchmark();
    return 0;
}

// 実行結果
// [TEST] edge cases ... passed
// [TEST] distribution check ... passed (chi2=10.4526)

// === Benchmarks (N=1000, ITER=100000000) ===
// select : 0.744270 s  | 0.007443 µs / op
// update : 2.824665 s  | 0.028247 µs / op
// = 1億回
