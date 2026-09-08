#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

using namespace std;

// ============================================================
// 高速乱数生成器 FastRng
//
// - 本体は splitmix64
// - C++20 / gcc 12.2 前提
// - UniformRandomBitGenerator 準拠
// - 競技プログラミング向けに速度を優先
// - 範囲乱数は厳密な無偏り性より軽さを重視
// ============================================================
struct FastRng {
    using result_type = uint64_t;

    // 内部状態は 64bit 1語だけ持つ
    static constexpr result_type default_seed = 0;
    result_type state = default_seed;

    // 構築と seed 設定
    constexpr FastRng() noexcept = default;
    constexpr explicit FastRng(result_type seed_value) noexcept : state(seed_value) {}
    constexpr void seed(result_type seed_value) noexcept { state = seed_value; }

    // UniformRandomBitGenerator 用の境界値
    static constexpr result_type min() noexcept { return 0; }
    static constexpr result_type max() noexcept { return numeric_limits<result_type>::max(); }

    // splitmix64 本体。64bit 乱数を 1 個返す
    inline result_type operator()() noexcept {
        auto z = state += 0x9e3779b97f4a7c15ULL;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    // 生の乱数取得
    inline result_type next_u64() noexcept { return (*this)(); }
    inline uint32_t next_u32() noexcept { return (uint32_t)((*this)() >> 32); }
    inline bool next_bool() noexcept { return (bool)((*this)() >> 63); }

    // [0, n) の符号なし整数乱数
    // n == 0 は full range とみなし、そのまま 64bit 乱数を返す
    inline result_type uniform_u64(result_type n) noexcept {
        if (!n) return next_u64();
        auto x = (*this)();
        if ((n & (n - 1)) == 0) return x & (n - 1);
        if ((uint32_t)(n >> 32) == 0) return (result_type)(((uint64_t)(uint32_t)x * (uint32_t)n) >> 32);
        if (n <= (1ULL << 53)) return (result_type)((double)n * (double)(x >> 11) * 0x1.0p-53);
        return (result_type)(((__uint128_t)x * n) >> 64);
    }

    // [0, n) の 32bit 版
    inline uint32_t uniform_u32(uint32_t n) noexcept {
        if (!n) return next_u32();
        return (uint32_t)uniform_u64(n);
    }

    // 閉区間 [l, r] の 32bit 整数乱数
    inline int32_t uniform_int(int32_t l, int32_t r) noexcept {
        assert(l <= r);
        return (int32_t)uniform_ll(l, r);
    }

    // 閉区間 [l, r] の 64bit 符号付き整数乱数
    inline int64_t uniform_ll(int64_t l, int64_t r) noexcept {
        assert(l <= r);
        auto ul = (uint64_t)l;
        return (int64_t)(ul + uniform_u64((uint64_t)r - ul + 1));
    }

    // 閉区間 [l, r] の 64bit 符号なし整数乱数
    inline result_type uniform_u64(result_type l, result_type r) noexcept {
        assert(l <= r);
        return l + uniform_u64(r - l + 1);
    }

    // [0, 1) の double を返す。53bit を使って作る
    inline double uniform_double() noexcept {
        return (double)((*this)() >> 11) * 0x1.0p-53;
    }

    // [l, r) の double を返す
    // 丸めで r 以上になったときは、r 未満の隣接値に寄せる
    inline double uniform_double(double l, double r) noexcept {
        assert(l <= r);
        if (l == r) return l;
        auto x = l + (r - l) * uniform_double();
        return x < r ? x : nextafter(r, l);
    }

    // 確率 p で true を返す
    inline bool bernoulli(double p) noexcept {
        assert(0.0 <= p && p <= 1.0);
        return p > 0.0 && (p >= 1.0 || uniform_double() < p);
    }
};

#if __INCLUDE_LEVEL__ == 0
// ============================================================
// 単体実行時のみ有効なテスト / ベンチマーク
// ============================================================
#include <array>
#include <chrono>
#include <concepts>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

namespace {

// 比較用の軽量 RNG。生成速度の比較に使う
struct WyRand {
    using result_type = uint64_t;
    result_type state = 0;

    WyRand() = default;
    explicit WyRand(result_type seed_value) : state(seed_value) {}
    void seed(result_type seed_value) noexcept { state = seed_value; }
    static constexpr result_type min() noexcept { return 0; }
    static constexpr result_type max() noexcept { return numeric_limits<result_type>::max(); }

    inline result_type operator()() noexcept {
        state += 0xa0761d6478bd642fULL;
        auto t = (__uint128_t)state * (__uint128_t)(state ^ 0xe7037ed1a0b428dbULL);
        return (result_type)t ^ (result_type)(t >> 64);
    }
};

// 比較用の xoshiro256++。seed には FastRng を使う
struct Xoshiro256PlusPlus {
    using result_type = uint64_t;
    uint64_t s[4]{};

    Xoshiro256PlusPlus() { seed(0); }
    explicit Xoshiro256PlusPlus(result_type seed_value) { seed(seed_value); }

    void seed(result_type seed_value) noexcept {
        FastRng seeder(seed_value);
        s[0] = seeder();
        s[1] = seeder();
        s[2] = seeder();
        s[3] = seeder();
        if ((s[0] | s[1] | s[2] | s[3]) == 0) s[0] = 1;
    }

    static constexpr result_type min() noexcept { return 0; }
    static constexpr result_type max() noexcept { return numeric_limits<result_type>::max(); }

    static inline uint64_t rotl(uint64_t x, int k) noexcept {
        return (x << k) | (x >> (64 - k));
    }

    inline result_type operator()() noexcept {
        auto result = rotl(s[0] + s[3], 23) + s[0];
        auto t = s[1] << 17;
        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];
        s[2] ^= t;
        s[3] = rotl(s[3], 45);
        return result;
    }
};

struct Tester {
    size_t checks = 0;

    void require(bool ok, const char* message) {
        ++checks;
        if (!ok) {
            cerr << "[TEST FAILED] " << message << '\n';
            exit(1);
        }
    }
};

struct BenchResult {
    const char* name;
    double run_ms;
    double ns_per_op;
    uint64_t checksum;
};

struct BenchSummary {
    const char* name;
    double avg_run_ms = 0.0;
    double worst_run_ms = 0.0;
    double avg_ns_per_op = 0.0;
    double worst_ns_per_op = 0.0;
    uint64_t checksum_xor = 0;
};

// ベンチ中の計算が消されないように吸い込む
volatile uint64_t g_sink = 0;

// 1 回 seed してから大量生成するケース
template <class RNG>
BenchResult bench_bulk(const char* name, uint64_t seed_value, uint64_t count) {
    RNG rng(seed_value);
    uint64_t checksum = 0;
    auto t0 = chrono::steady_clock::now();
    for (uint64_t i = 0; i < count; ++i) checksum ^= rng();
    auto t1 = chrono::steady_clock::now();
    auto run_ms = chrono::duration<double, milli>(t1 - t0).count();
    g_sink ^= checksum;
    return {name, run_ms, run_ms * 1.0e6 / (double)count, checksum};
}

// seed + 1 回生成を何度も繰り返すケース
template <class RNG>
BenchResult bench_reseed(const char* name, uint64_t base_seed, uint64_t count) {
    RNG rng;
    uint64_t checksum = 0;
    auto t0 = chrono::steady_clock::now();
    for (uint64_t i = 0; i < count; ++i) {
        rng.seed(base_seed + i);
        checksum ^= rng();
    }
    auto t1 = chrono::steady_clock::now();
    auto run_ms = chrono::duration<double, milli>(t1 - t0).count();
    g_sink ^= checksum;
    return {name, run_ms, run_ms * 1.0e6 / (double)count, checksum};
}

// 同じ条件の複数 run をまとめる
BenchSummary summarize(const vector<BenchResult>& runs) {
    BenchSummary out{runs.front().name};
    for (auto& run : runs) {
        out.avg_run_ms += run.run_ms;
        out.worst_run_ms = max(out.worst_run_ms, run.run_ms);
        out.avg_ns_per_op += run.ns_per_op;
        out.worst_ns_per_op = max(out.worst_ns_per_op, run.ns_per_op);
        out.checksum_xor ^= run.checksum;
    }
    out.avg_run_ms /= (double)runs.size();
    out.avg_ns_per_op /= (double)runs.size();
    return out;
}

// RNG 間比較の結果を表形式で出す
void print_table(const char* title, uint64_t ops_per_run, const vector<BenchSummary>& rows) {
    cout << "\n=== " << title << " ===\n";
    cout << "ops per run = " << ops_per_run << "\n\n";
    cout << left << setw(28) << "RNG"
         << right << setw(14) << "avg_run_ms"
         << setw(16) << "worst_run_ms"
         << setw(16) << "avg_ns/op"
         << setw(16) << "worst_ns/op"
         << setw(24) << "checksum_xor" << '\n';
    cout << string(28 + 14 + 16 + 16 + 16 + 24, '-') << '\n';
    for (auto& row : rows) {
        cout << left << setw(28) << row.name
             << right << setw(14) << fixed << setprecision(3) << row.avg_run_ms
             << setw(16) << row.worst_run_ms
             << setw(16) << row.avg_ns_per_op
             << setw(16) << row.worst_ns_per_op
             << setw(24) << row.checksum_xor << '\n';
    }
}

// 境界値とランダムケースをまとめて検証する
void run_tests() {
    Tester t;

    static_assert(uniform_random_bit_generator<FastRng>);
    static_assert(same_as<FastRng::result_type, uint64_t>);
    t.require(FastRng::min() == 0, "min() must be 0");
    t.require(FastRng::max() == numeric_limits<uint64_t>::max(), "max() must be UINT64_MAX");
    t.require(sizeof(FastRng) == sizeof(uint64_t), "FastRng must keep 64bit state only");

    // seed の再現性と系列の分岐を確認する
    {
        FastRng a(123456789), b(123456789), c(987654321);
        for (int i = 0; i < 10000; ++i) t.require(a() == b(), "same seed must match");
        bool differ = false;
        for (int i = 0; i < 64; ++i) if (a() != c()) differ = true;
        t.require(differ, "different seeds should diverge quickly");

        FastRng d(1);
        array<uint64_t, 1024> s1{}, s2{};
        for (auto& x : s1) x = d();
        d.seed(1);
        for (auto& x : s2) x = d();
        t.require(s1 == s2, "seed() must reset sequence");
    }

    // 生の乱数 API が operator() と整合するかを見る
    {
        FastRng a(42), b(42), c(42);
        for (int i = 0; i < 20000; ++i) {
            auto x = a();
            t.require((uint32_t)(x >> 32) == b.next_u32(), "next_u32() mismatch");
            t.require((bool)(x >> 63) == c.next_bool(), "next_bool() mismatch");
        }
    }

    // [0, n) 系の範囲外が出ないことを確認する
    {
        FastRng rng(777);
        constexpr array<uint64_t, 12> u64_cases = {
            1ULL, 2ULL, 3ULL, 7ULL, 8ULL, 9ULL,
            (1ULL << 32) - 1, (1ULL << 32),
            (1ULL << 63) - 1, (1ULL << 63),
            numeric_limits<uint64_t>::max() - 1,
            numeric_limits<uint64_t>::max()
        };
        for (auto n : u64_cases) {
            for (int i = 0; i < 5000; ++i) t.require(rng.uniform_u64(n) < n, "uniform_u64(n) out of range");
        }

        constexpr array<uint32_t, 8> u32_cases = {
            1U, 2U, 3U, 7U, 8U, 65535U, 65536U, numeric_limits<uint32_t>::max()
        };
        for (auto n : u32_cases) {
            for (int i = 0; i < 5000; ++i) t.require(rng.uniform_u32(n) < n, "uniform_u32(n) out of range");
        }

        for (int i = 0; i < 10000; ++i) {
            t.require(rng.uniform_u64(1) == 0, "uniform_u64(1) must be 0");
            t.require(rng.uniform_u32(1) == 0, "uniform_u32(1) must be 0");
        }

        FastRng a(123), b(123);
        for (int i = 0; i < 10000; ++i) t.require(a.uniform_u64(0) == b.next_u64(), "uniform_u64(0) must match next_u64");
    }

    // 閉区間版の整数乱数を境界値つきで確認する
    {
        FastRng rng(888);
        for (int i = 0; i < 10000; ++i) {
            t.require(rng.uniform_int(5, 5) == 5, "uniform_int(l,l) must return l");
            t.require(rng.uniform_ll(-7, -7) == -7, "uniform_ll(l,l) must return l");
            t.require(rng.uniform_u64(9, 9) == 9, "uniform_u64(l,l) must return l");
        }
        for (int i = 0; i < 30000; ++i) {
            auto x1 = rng.uniform_int(-100, 100);
            auto x2 = rng.uniform_int(numeric_limits<int32_t>::min(), numeric_limits<int32_t>::max());
            auto x3 = rng.uniform_ll(numeric_limits<int64_t>::min(), numeric_limits<int64_t>::max());
            auto x4 = rng.uniform_u64(123456789ULL, 123456789ULL + (1ULL << 40));
            t.require(-100 <= x1 && x1 <= 100, "uniform_int small range out of bounds");
            t.require(numeric_limits<int32_t>::min() <= x2 && x2 <= numeric_limits<int32_t>::max(), "uniform_int full range out of bounds");
            t.require(numeric_limits<int64_t>::min() <= x3 && x3 <= numeric_limits<int64_t>::max(), "uniform_ll full range out of bounds");
            t.require(123456789ULL <= x4 && x4 <= 123456789ULL + (1ULL << 40), "uniform_u64(l,r) out of bounds");
        }

        FastRng a(999), b(999);
        for (int i = 0; i < 10000; ++i) {
            t.require(a.uniform_u64(0, numeric_limits<uint64_t>::max()) == b.next_u64(), "uniform_u64 full range should match next_u64");
        }
    }

    // 実数乱数と Bernoulli の簡易統計を確認する
    {
        FastRng rng(3141592653589793ULL);
        constexpr int samples = 300000;
        double sum01 = 0.0;
        double sumlr = 0.0;
        int bern = 0;
        for (int i = 0; i < samples; ++i) {
            auto x = rng.uniform_double();
            auto y = rng.uniform_double(-3.5, 7.25);
            t.require(0.0 <= x && x < 1.0, "uniform_double() out of range");
            t.require(-3.5 <= y && y < 7.25, "uniform_double(l,r) out of range");
            sum01 += x;
            sumlr += y;
            bern += (int)rng.bernoulli(0.3);
        }
        t.require(abs(sum01 / samples - 0.5) <= 0.005, "uniform_double() mean looks wrong");
        t.require(abs((double)bern / samples - 0.3) <= 0.015, "bernoulli(0.3) looks wrong");
        t.require(abs(sumlr / samples - 1.875) <= 0.02, "uniform_double(l,r) mean looks wrong");
        {
            FastRng rng2(0x31628af67b2131abULL);
            auto x = rng2.uniform_double(1.0, 2.0);
            t.require(x < 2.0, "uniform_double(l,r) returned r");
        }
        for (int i = 0; i < 10000; ++i) {
            t.require(!rng.bernoulli(0.0), "bernoulli(0) must be false");
            t.require(rng.bernoulli(1.0), "bernoulli(1) must be true");
        }
    }

    // 小さい n では頻度が極端に崩れていないかを軽く見る
    {
        constexpr int samples = 200000;
        for (int n : {2, 3, 5, 7, 16, 31}) {
            FastRng rng(1234 + (uint64_t)n);
            vector<int> freq((size_t)n);
            for (int i = 0; i < samples; ++i) ++freq[rng.uniform_u64((uint64_t)n)];
            auto expected = (double)samples / n;
            for (int count : freq) {
                auto diff = abs(count - expected);
                auto tol = max(8.0 * sqrt(expected), expected * 0.08);
                t.require(diff <= tol, "simple frequency check failed");
            }
        }
    }

    // 標準 distribution とそのまま組み合わせられるかを確認する
    {
        FastRng rng(987654321);
        uniform_int_distribution<int32_t> int_dist(-17, 31);
        uniform_real_distribution<double> real_dist(-5.0, 2.0);
        bernoulli_distribution bern_dist(0.7);
        for (int i = 0; i < 200000; ++i) {
            auto xi = int_dist(rng);
            auto xd = real_dist(rng);
            auto xb = bern_dist(rng);
            t.require(-17 <= xi && xi <= 31, "uniform_int_distribution out of range");
            t.require(-5.0 <= xd && xd < 2.0, "uniform_real_distribution out of range");
            t.require(xb == false || xb == true, "bernoulli_distribution must return bool");
        }
    }

    // ランダムに作った区間でも範囲外を出さないことを確認する
    {
        FastRng rng(135791357913579ULL);
        for (int tc = 0; tc < 30000; ++tc) {
            auto li = rng.uniform_int(numeric_limits<int32_t>::min(), numeric_limits<int32_t>::max());
            auto ri = rng.uniform_int(li, numeric_limits<int32_t>::max());
            auto xi = rng.uniform_int(li, ri);
            t.require(li <= xi && xi <= ri, "randomized uniform_int failed");

            auto ll = rng.uniform_ll(numeric_limits<int64_t>::min(), numeric_limits<int64_t>::max());
            auto rr = rng.uniform_ll(ll, numeric_limits<int64_t>::max());
            auto xl = rng.uniform_ll(ll, rr);
            t.require(ll <= xl && xl <= rr, "randomized uniform_ll failed");

            auto lu = rng.next_u64();
            auto ru = rng.uniform_u64(lu, numeric_limits<uint64_t>::max());
            auto xu = rng.uniform_u64(lu, ru);
            t.require(lu <= xu && xu <= ru, "randomized uniform_u64(l,r) failed");

            auto n = rng.uniform_u64(1, numeric_limits<uint64_t>::max());
            t.require(rng.uniform_u64(n) < n, "randomized uniform_u64(n) failed");

            auto l = -1000.0 + 2000.0 * rng.uniform_double();
            auto r = l + 1000.0 * rng.uniform_double();
            auto xd = rng.uniform_double(l, r);
            t.require((l == r) ? (xd == l) : (l <= xd && xd < r), "randomized uniform_double failed");
        }
    }

    cout << "All tests passed. checks = " << t.checks << '\n';
}

// 他の RNG と生の生成速度を比較する
void run_rng_benchmark() {
    constexpr int repeat = 4;
    constexpr uint64_t bulk = 10'000'000ULL;
    constexpr uint64_t reseed = 5'000'000ULL;
    constexpr uint64_t base_seed = 123456789ULL;

    // ウォームアップして初回のぶれを減らす
    {
        FastRng a(base_seed);
        WyRand b(base_seed);
        Xoshiro256PlusPlus c(base_seed);
        mt19937_64 d(base_seed);
        uint64_t tmp = 0;
        for (int i = 0; i < 200000; ++i) {
            tmp ^= a();
            tmp ^= b();
            tmp ^= c();
            tmp ^= d();
        }
        g_sink ^= tmp;
    }

    vector<BenchResult> a_splitmix, a_wyrand, a_xoshiro, a_mt;
    vector<BenchResult> b_splitmix, b_wyrand, b_xoshiro, b_mt;
    a_splitmix.reserve(repeat); a_wyrand.reserve(repeat); a_xoshiro.reserve(repeat); a_mt.reserve(repeat);
    b_splitmix.reserve(repeat); b_wyrand.reserve(repeat); b_xoshiro.reserve(repeat); b_mt.reserve(repeat);

    for (int rep = 0; rep < repeat; ++rep) {
        auto seed = base_seed + (uint64_t)rep * (uint64_t)1'000'000'007ULL;
        a_splitmix.push_back(bench_bulk<FastRng>("FastRng", seed, bulk));
        a_wyrand.push_back(bench_bulk<WyRand>("WyRand", seed, bulk));
        a_xoshiro.push_back(bench_bulk<Xoshiro256PlusPlus>("xoshiro256++", seed, bulk));
        a_mt.push_back(bench_bulk<mt19937_64>("mt19937_64", seed, bulk));

        b_splitmix.push_back(bench_reseed<FastRng>("FastRng", seed, reseed));
        b_wyrand.push_back(bench_reseed<WyRand>("WyRand", seed, reseed));
        b_xoshiro.push_back(bench_reseed<Xoshiro256PlusPlus>("xoshiro256++", seed, reseed));
        b_mt.push_back(bench_reseed<mt19937_64>("mt19937_64", seed, reseed));
    }

    print_table("Case A: single seed + bulk generation", bulk,
                {summarize(a_splitmix), summarize(a_wyrand), summarize(a_xoshiro), summarize(a_mt)});
    print_table("Case B: reseed + one generation", reseed,
                {summarize(b_splitmix), summarize(b_wyrand), summarize(b_xoshiro), summarize(b_mt)});
}

// FastRng の各 API を個別に計測する
void run_api_benchmark() {
    constexpr int repeat = 4;
    constexpr uint64_t samples = 5'000'000ULL;

    auto print_row = [](const char* name, uint64_t ops_per_run, double avg_run_ms, double worst_run_ms,
                        double avg_ns, double worst_ns, uint64_t checksum) {
        cout << left << setw(28) << name
             << right << setw(14) << ops_per_run
             << setw(14) << fixed << setprecision(3) << avg_run_ms
             << setw(16) << worst_run_ms
             << setw(16) << avg_ns
             << setw(16) << worst_ns
             << setw(24) << checksum << '\n';
    };

    cout << "\n=== FastRng API benchmark ===\n";
    cout << left << setw(28) << "operation"
         << right << setw(14) << "ops/run"
         << setw(14) << "avg_run_ms"
         << setw(16) << "worst_run_ms"
         << setw(16) << "avg_ns/op"
         << setw(16) << "worst_ns/op"
         << setw(24) << "checksum_xor" << '\n';
    cout << string(28 + 14 + 14 + 16 + 16 + 16 + 24, '-') << '\n';
    auto bench_op = [&](const char* name, auto fn, uint64_t ops_per_run) {
        double sum_run_ms = 0.0, worst_run_ms = 0.0, sum_ns = 0.0, worst_ns = 0.0;
        uint64_t checksum_xor = 0;
        for (int rep = 0; rep < repeat; ++rep) {
            FastRng rng((uint64_t)(1000 + rep));
            uint64_t checksum = 0;
            auto t0 = chrono::steady_clock::now();
            fn(rng, checksum);
            auto t1 = chrono::steady_clock::now();
            auto run_ms = chrono::duration<double, milli>(t1 - t0).count();
            auto ns_per_op = run_ms * 1.0e6 / (double)ops_per_run;
            g_sink ^= checksum;
            sum_run_ms += run_ms;
            worst_run_ms = max(worst_run_ms, run_ms);
            sum_ns += ns_per_op;
            worst_ns = max(worst_ns, ns_per_op);
            checksum_xor ^= checksum;
        }
        print_row(name, ops_per_run, sum_run_ms / repeat, worst_run_ms, sum_ns / repeat, worst_ns, checksum_xor);
    };

    bench_op("next_u64", [&](FastRng& rng, uint64_t& checksum) {
        for (uint64_t i = 0; i < samples; ++i) checksum ^= rng.next_u64();
    }, samples);

    bench_op("next_u32", [&](FastRng& rng, uint64_t& checksum) {
        for (uint64_t i = 0; i < samples; ++i) checksum ^= rng.next_u32();
    }, samples);

    bench_op("next_bool", [&](FastRng& rng, uint64_t& checksum) {
        for (uint64_t i = 0; i < samples; ++i) checksum ^= (uint64_t)rng.next_bool();
    }, samples);

    bench_op("uniform_u32_pow2", [&](FastRng& rng, uint64_t& checksum) {
        for (uint64_t i = 0; i < samples; ++i) checksum ^= rng.uniform_u32(1U << 20);
    }, samples);

    bench_op("uniform_u32_general", [&](FastRng& rng, uint64_t& checksum) {
        for (uint64_t i = 0; i < samples; ++i) checksum ^= rng.uniform_u32(1'000'000'007U);
    }, samples);

    bench_op("uniform_u64_pow2", [&](FastRng& rng, uint64_t& checksum) {
        for (uint64_t i = 0; i < samples; ++i) checksum ^= rng.uniform_u64(1ULL << 40);
    }, samples);

    bench_op("uniform_u64_small", [&](FastRng& rng, uint64_t& checksum) {
        for (uint64_t i = 0; i < samples; ++i) checksum ^= rng.uniform_u64(1'000'000'007ULL);
    }, samples);

    bench_op("uniform_u64_large", [&](FastRng& rng, uint64_t& checksum) {
        constexpr uint64_t range = (1ULL << 63) + 12345ULL;
        for (uint64_t i = 0; i < samples; ++i) checksum ^= rng.uniform_u64(range);
    }, samples);

    bench_op("uniform_int_small", [&](FastRng& rng, uint64_t& checksum) {
        for (uint64_t i = 0; i < samples; ++i) checksum ^= (uint64_t)rng.uniform_int(-1000, 1000);
    }, samples);

    bench_op("uniform_int_wide", [&](FastRng& rng, uint64_t& checksum) {
        for (uint64_t i = 0; i < samples; ++i) checksum ^= (uint64_t)rng.uniform_int(-1'000'000'000, 1'000'000'000);
    }, samples);

    bench_op("uniform_int_full", [&](FastRng& rng, uint64_t& checksum) {
        for (uint64_t i = 0; i < samples; ++i) checksum ^= (uint64_t)rng.uniform_int(numeric_limits<int32_t>::min(), numeric_limits<int32_t>::max());
    }, samples);

    bench_op("uniform_ll_small", [&](FastRng& rng, uint64_t& checksum) {
        for (uint64_t i = 0; i < samples; ++i) checksum ^= (uint64_t)rng.uniform_ll(-1'000'000'000'000LL, 1'000'000'000'000LL);
    }, samples);

    bench_op("uniform_ll_full", [&](FastRng& rng, uint64_t& checksum) {
        for (uint64_t i = 0; i < samples; ++i) checksum ^= (uint64_t)rng.uniform_ll(numeric_limits<int64_t>::min(), numeric_limits<int64_t>::max());
    }, samples);

    bench_op("uniform_u64_lr_small", [&](FastRng& rng, uint64_t& checksum) {
        for (uint64_t i = 0; i < samples; ++i) checksum ^= rng.uniform_u64(123456789ULL, 123456789ULL + 1'000'000'007ULL);
    }, samples);

    bench_op("uniform_u64_lr_large", [&](FastRng& rng, uint64_t& checksum) {
        constexpr uint64_t l = 1ULL << 63;
        constexpr uint64_t r = numeric_limits<uint64_t>::max() - 12345ULL;
        for (uint64_t i = 0; i < samples; ++i) checksum ^= rng.uniform_u64(l, r);
    }, samples);

    bench_op("uniform_u64_lr_full", [&](FastRng& rng, uint64_t& checksum) {
        for (uint64_t i = 0; i < samples; ++i) checksum ^= rng.uniform_u64(0, numeric_limits<uint64_t>::max());
    }, samples);

    // double 系は毎回整数化せず、4 本の和に畳んで最後だけビット化する
    auto bench_double_op = [&](const char* name, auto fn, uint64_t ops_per_run) {
        double sum_run_ms = 0.0, worst_run_ms = 0.0, sum_ns = 0.0, worst_ns = 0.0;
        uint64_t checksum_xor = 0;
        for (int rep = 0; rep < repeat; ++rep) {
            FastRng rng((uint64_t)(1000 + rep));
            double s0 = 0.0, s1 = 0.0, s2 = 0.0, s3 = 0.0;
            auto t0 = chrono::steady_clock::now();
            uint64_t i = 0;
            for (; i + 4 <= ops_per_run; i += 4) {
                s0 += fn(rng);
                s1 += fn(rng);
                s2 += fn(rng);
                s3 += fn(rng);
            }
            for (; i < ops_per_run; ++i) s0 += fn(rng);
            auto t1 = chrono::steady_clock::now();
            auto sum = (s0 + s1) + (s2 + s3);
            union { double d; uint64_t u; } bits{sum};
            g_sink ^= bits.u;
            auto run_ms = chrono::duration<double, milli>(t1 - t0).count();
            auto ns_per_op = run_ms * 1.0e6 / (double)ops_per_run;
            sum_run_ms += run_ms;
            worst_run_ms = max(worst_run_ms, run_ms);
            sum_ns += ns_per_op;
            worst_ns = max(worst_ns, ns_per_op);
            checksum_xor ^= bits.u;
        }
        print_row(name, ops_per_run, sum_run_ms / repeat, worst_run_ms, sum_ns / repeat, worst_ns, checksum_xor);
    };

    bench_double_op("uniform_double", [&](FastRng& rng) {
        return rng.uniform_double();
    }, samples);

    bench_double_op("uniform_double_lr", [&](FastRng& rng) {
        return rng.uniform_double(-3.5, 7.25);
    }, samples);

    bench_op("bernoulli_0.3", [&](FastRng& rng, uint64_t& checksum) {
        for (uint64_t i = 0; i < samples; ++i) checksum ^= (uint64_t)rng.bernoulli(0.3);
    }, samples);
}

}

// 単体実行時はテストとベンチを順に回す
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    run_tests();
    run_rng_benchmark();
    run_api_benchmark();

    cout << "\nfinal_sink = " << g_sink << '\n';
    return 0;
}
#endif


// 実行結果(atcoder)
// All tests passed. checks = 1710073

// === FastRng API benchmark ===
// operation                          ops/run    avg_run_ms    worst_run_ms       avg_ns/op     worst_ns/op            checksum_xor
// --------------------------------------------------------------------------------------------------------------------------------
// next_u64                           5000000         1.864           1.895           0.373           0.379    13356002974676463571
// next_u32                           5000000         2.158           2.204           0.432           0.441              3109686769
// next_bool                          5000000         2.180           2.205           0.436           0.441                       1
// uniform_u32_pow2                   5000000         2.033           2.082           0.407           0.416                  438227
// uniform_u32_general                5000000         2.848           2.888           0.570           0.578               932883746
// uniform_u64_pow2                   5000000         1.967           1.987           0.393           0.397           1036104675283
// uniform_u64_small                  5000000         2.859           2.901           0.572           0.580               932883746
// uniform_u64_large                  5000000         4.605           4.635           0.921           0.927     6678001198353302192
// uniform_int_small                  5000000         3.140           3.162           0.628           0.632    18446744073709550610
// uniform_int_wide                   5000000         3.116           3.145           0.623           0.629               913712272
// uniform_int_full                   5000000         2.322           2.357           0.464           0.471              1017556947
// uniform_ll_small                   5000000         3.181           3.238           0.636           0.648    18446743964522489968
// uniform_ll_full                    5000000         2.203           2.253           0.441           0.451    13356002974676463571
// uniform_u64_lr_small               5000000         3.051           3.083           0.610           0.617              1348137669
// uniform_u64_lr_large               5000000         5.583           5.706           1.117           1.141     6678001419325284715
// uniform_u64_lr_full                5000000         1.821           1.849           0.364           0.370    13356002974676463571
// uniform_double                     5000000         2.614           2.656           0.523           0.531           5927408164868
// uniform_double_lr                  5000000         4.459           4.524           0.892           0.905          57937936905882
// bernoulli_0.3                      5000000         3.061           3.069           0.612           0.614                       0

// final_sink = 13356054241575762562


// 実行結果(chatgpt)
// All tests passed. checks = 1710073

// === Case A: single seed + bulk generation ===
// ops per run = 10000000

// RNG                             avg_run_ms    worst_run_ms       avg_ns/op     worst_ns/op            checksum_xor
// ------------------------------------------------------------------------------------------------------------------
// FastRng                              4.418           4.549           0.442           0.455     5996346739661175277
// WyRand                               7.406           7.773           0.741           0.777      301706195511615249
// xoshiro256++                        11.700          11.741           1.170           1.174     7276203832120700336
// mt19937_64                          17.325          17.440           1.732           1.744     9429132475706860347

// === Case B: reseed + one generation ===
// ops per run = 5000000

// RNG                             avg_run_ms    worst_run_ms       avg_ns/op     worst_ns/op            checksum_xor
// ------------------------------------------------------------------------------------------------------------------
// FastRng                              2.074           2.177           0.415           0.435     4698882017515505596
// WyRand                               5.546           5.623           1.109           1.125     6093296816857506784
// xoshiro256++                        10.269          10.415           2.054           2.083     4367655942766497819
// mt19937_64                        3153.527        3211.380         630.705         642.276    10381061362517846008

// === FastRng API benchmark ===
// operation                          ops/run    avg_run_ms    worst_run_ms       avg_ns/op     worst_ns/op            checksum_xor
// --------------------------------------------------------------------------------------------------------------------------------
// next_u64                           5000000         2.196           2.213           0.439           0.443    13356002974676463571
// next_u32                           5000000         2.733           2.804           0.547           0.561              3109686769
// next_bool                          5000000         2.708           2.718           0.542           0.544                       1
// uniform_u32_pow2                   5000000         2.738           2.885           0.548           0.577                  438227
// uniform_u32_general                5000000         3.645           3.679           0.729           0.736               932883746
// uniform_u64_pow2                   5000000         2.521           2.538           0.504           0.508           1036104675283
// uniform_u64_small                  5000000         3.669           3.691           0.734           0.738               932883746
// uniform_u64_large                  5000000         7.388           7.407           1.478           1.481     6678001198353302192
// uniform_int_small                  5000000         4.713           5.671           0.943           1.134    18446744073709550610
// uniform_int_wide                   5000000         5.673           5.685           1.135           1.137               913712272
// uniform_int_full                   5000000         3.770           4.059           0.754           0.812              1017556947
// uniform_ll_small                   5000000         4.955           5.586           0.991           1.117    18446743964522489968
// uniform_ll_full                    5000000         2.953           3.007           0.591           0.601    13356002974676463571
// uniform_u64_lr_small               5000000         3.987           4.116           0.797           0.823              1348137669
// uniform_u64_lr_large               5000000         7.155           8.105           1.431           1.621     6678001419325284715
// uniform_u64_lr_full                5000000         2.219           2.259           0.444           0.452    13356002974676463571
// uniform_double                     5000000         2.668           2.691           0.534           0.538           5927408164868
// uniform_double_lr                  5000000         3.777           3.927           0.755           0.785          57937936905882
// bernoulli_0.3                      5000000         3.602           3.696           0.720           0.739                       0

// final_sink = 17633389049377866975
