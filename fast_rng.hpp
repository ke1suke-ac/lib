#pragma once
#include <bits/stdc++.h>
using namespace std;

// FastRng: splitmix64 を使う、状態 64bit の競技プログラミング用 RNG
// uniform は半開区間、uniform_closed は整数の閉区間を返す
// uniform_closed_delta は整数の閉区間から 0 を除いた差分を返す
// 対応型は bool を除く 64bit 以下の整数と double、2 引数は同じ型を指定
// 範囲縮小は速度優先で厳密な無偏り性を保証しない、暗号用途には使わない
struct FastRng {
    using result_type = uint64_t;
    static constexpr result_type default_seed = 0;
    result_type state = default_seed;

    // 構築と seed 設定、コピーすると内部状態もそのまま複製する
    constexpr FastRng() noexcept = default;
    constexpr explicit FastRng(result_type seed_value) noexcept : state(seed_value) {}
    constexpr void seed(result_type seed_value) noexcept { state = seed_value; }

    // UniformRandomBitGenerator 用の出力範囲
    static constexpr result_type min() noexcept { return 0; }
    static constexpr result_type max() noexcept { return numeric_limits<result_type>::max(); }

    // splitmix64 本体、64bit の生乱数を 1 個返す
    inline result_type operator()() noexcept {
        auto z = state += 0x9e3779b97f4a7c15ULL;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    // 生乱数の取得、32bit と bool は上位 bit を使う
    inline result_type next_u64() noexcept { return (*this)(); }
    inline uint32_t next_u32() noexcept { return (uint32_t)((*this)() >> 32); }
    inline bool next_bool() noexcept { return (bool)((*this)() >> 63); }

private:
    // GCC の整数型から bool と 128bit 型を除外する
    template<class T>
    static constexpr bool integer_type = integral<T> && !same_as<remove_cv_t<T>, bool>
                                      && numeric_limits<T>::digits <= 64;

    // 整数を [0, n) に縮小する共通処理、内部でのみ n == 0 を要素数 2^64 と解釈
    inline result_type bounded(result_type n) noexcept {
        if (!n) return next_u64();
        auto x = (*this)();
        if ((n & (n - 1)) == 0) return x & (n - 1);
        if ((uint32_t)(n >> 32) == 0) return (result_type)(((uint64_t)(uint32_t)x * (uint32_t)n) >> 32);
        if (n <= (1ULL << 53)) return (result_type)((double)n * (double)(x >> 11) * 0x1.0p-53);
        return (result_type)(((__uint128_t)x * n) >> 64);
    }

public:
    // [0, 1) の double を返す、上位 53bit を使う
    inline double uniform() noexcept {
        return (double)((*this)() >> 11) * 0x1.0p-53;
    }

    // 半開区間 [0, r)、r > 0 が前提、戻り値は引数と同じ型
    template<class T> requires (integer_type<T> || same_as<remove_cv_t<T>, double>)
    inline T uniform(T r) noexcept { return uniform(T{0}, r); }

    // 半開区間 [l, r)、l < r が前提、double は端点と r - l が有限の範囲のみ
    template<class T> requires (integer_type<T> || same_as<remove_cv_t<T>, double>)
    inline T uniform(T l, T r) noexcept {
        assert(l < r);
        if constexpr (same_as<remove_cv_t<T>, double>) {
            // 上端への丸めだけを補正する、通常の浮動小数点演算を前提とする
            auto width = r - l;
            assert(isfinite(width));
            auto x = l + width * uniform();
            return x < r ? x : nextafter(r, l);
        } else {
            // 幅とオフセットは unsigned で計算し、符号付きの桁あふれを避ける
            return (T)((uint64_t)l + bounded((uint64_t)r - (uint64_t)l));
        }
    }

    // 整数の閉区間 [0, r]、r >= 0 が前提
    template<class T> requires integer_type<T>
    inline T uniform_closed(T r) noexcept { return uniform_closed(T{0}, r); }

    // 整数の閉区間 [l, r]、l <= r が前提、同値端点と型の全域にも対応
    template<class T> requires integer_type<T>
    inline T uniform_closed(T l, T r) noexcept {
        assert(l <= r);
        auto ul = (uint64_t)l;
        return (T)(ul + bounded((uint64_t)r - ul + 1));
    }

    // 整数の閉区間 [l, r] から 0 を除いた差分、l <= r かつ [0, 0] 以外が前提
    template<class T> requires integer_type<T>
    inline T uniform_closed_delta(T l, T r) noexcept {
        assert(l <= r && (l != 0 || r != 0));
        // 0 を含むときは候補を 1 個減らし、非負の結果を 1 ずらす（再抽選なし）
        auto skip_zero = l <= 0 && r >= 0;
        auto x = uniform_closed(l, (T)(r - skip_zero));
        return (T)(x + (skip_zero && x >= 0));
    }

    // 確率 p で true、0 <= p <= 1 が前提、端点確率では乱数を消費しない
    inline bool bernoulli(double p) noexcept {
        assert(0.0 <= p && p <= 1.0);
        return p > 0.0 && (p >= 1.0 || uniform() < p);
    }
};

#if __INCLUDE_LEVEL__ == 0
// 単体コンパイル時だけ、テストとベンチマークを有効にする
namespace {
struct Tester {
    uint64_t checks = 0;
    void require(bool ok, const char* message) {
        ++checks;
        if (!ok) {
            cerr << "[TEST FAILED] " << message << '\n';
            exit(1);
        }
    }
};

// 型制約を、関数本体ではなく呼び出し可能性として検査する
// cv 付きの明示指定も bool を許可しない
struct Convertible { operator int() const { return 1; } };
enum PlainEnum { enum_value };
enum class ScopedEnum { value };
template<class T> concept HasUniform = requires(FastRng& rng, T x) {
    rng.uniform(x); rng.uniform(x, x);
};
template<class T> concept HasClosed = requires(FastRng& rng, T x) {
    rng.uniform_closed(x); rng.uniform_closed(x, x);
};
template<class T> concept HasClosedDelta = requires(FastRng& rng, T x) {
    rng.uniform_closed_delta(x, x);
};
template<class T> concept ExplicitClosedDelta = requires(FastRng& rng) { rng.uniform_closed_delta<T>(0, 1); };
template<class T, class U> concept MixedClosedDelta = requires(FastRng& rng, T l, U r) {
    rng.uniform_closed_delta(l, r);
};
template<class T> concept ExplicitUniform = requires(FastRng& rng) { rng.uniform<T>(1); };
template<class T, class U> concept MixedUniform = requires(FastRng& rng, T l, U r) { rng.uniform(l, r); };
template<class T, class U> concept MixedClosed = requires(FastRng& rng, T l, U r) { rng.uniform_closed(l, r); };
template<class R> concept OldApi = requires(R& rng) { rng.uniform_u64(10); };
template<class R> concept ExposedBounded = requires(R& rng) { rng.bounded(10); };

static_assert(uniform_random_bit_generator<FastRng>);
static_assert(sizeof(FastRng) == sizeof(uint64_t));
static_assert(same_as<decltype(FastRng{}.uniform()), double>);
static_assert(!HasUniform<bool> && !ExplicitUniform<const bool> && !HasClosed<bool>);
static_assert(!HasUniform<float> && !HasUniform<long double> && !HasClosed<double>);
static_assert(!HasUniform<__int128_t> && !HasUniform<__uint128_t>);
static_assert(!HasUniform<PlainEnum> && !HasUniform<ScopedEnum> && !HasUniform<Convertible>);
static_assert(!HasUniform<void*> && !HasUniform<string>);
static_assert(!MixedUniform<int, long long> && !MixedUniform<int, unsigned int>);
static_assert(!MixedUniform<int, double> && !MixedClosed<int, long long>);
static_assert(!OldApi<FastRng> && !ExposedBounded<FastRng>);
static_assert(same_as<decltype(FastRng{}.uniform<long long>(0, 10)), long long>);
static_assert(same_as<decltype(FastRng{}.uniform<double>(0, 1)), double>);

static_assert(!HasClosedDelta<bool> && !ExplicitClosedDelta<const bool>);
static_assert(!HasClosedDelta<double> && !HasClosedDelta<float> && !HasClosedDelta<long double>);
static_assert(!HasClosedDelta<__int128_t> && !HasClosedDelta<__uint128_t>);
static_assert(!HasClosedDelta<PlainEnum> && !HasClosedDelta<ScopedEnum> && !HasClosedDelta<Convertible>);
static_assert(!HasClosedDelta<void*> && !HasClosedDelta<string>);
static_assert(!MixedClosedDelta<int, long long> && !MixedClosedDelta<int, unsigned int>);
static_assert(same_as<decltype(FastRng{}.uniform_closed_delta<long long>(-1, 1)), long long>);

// 本体とは独立に幅を 128bit で計算し、境界値と縮小結果の一致を確かめる
uint64_t reference_raw(uint64_t& state) {
    state += 0x9e3779b97f4a7c15ULL;
    auto x = state;
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
uint64_t reference_reduce(uint64_t x, __uint128_t width) {
    if (width == ((__uint128_t)1 << 64)) return x;
    auto n = (uint64_t)width;
    if ((n & (n - 1)) == 0) return x % n;
    if (n <= UINT32_MAX) return (uint64_t)((__uint128_t)(uint32_t)x * n / ((__uint128_t)1 << 32));
    if (n <= (1ULL << 53)) return (uint64_t)((double)n * (double)(x >> 11) * 0x1.0p-53);
    return (uint64_t)((__uint128_t)x * n / ((__uint128_t)1 << 64));
}

// 欲しい生乱数が次の 1 回で出る seed を逆算し、稀な丸めも確実に発生させる
uint64_t seed_for_raw(uint64_t x) {
    auto undo = [](uint64_t y, int shift) {
        auto z = y;
        for (auto k = shift; k < 64; k += shift) z ^= y >> k;
        return z;
    };
    x = undo(x, 31) * 0x319642b2d24d8ec3ULL;
    x = undo(x, 27) * 0x96de1b173f119089ULL;
    return undo(x, 30) - 0x9e3779b97f4a7c15ULL;
}
constexpr array<uint64_t, 10> raw_edges = {
    0, 1, 2, UINT32_MAX, 1ULL << 32, (1ULL << 53) - 1,
    1ULL << 63, UINT64_MAX - 2047, UINT64_MAX - 1, UINT64_MAX
};

// 負区間と正区間の候補数を別々に数え、0 を除く差分と状態消費を検査する
template<class T>
void check_delta_interval(Tester& t, T l, T r, uint64_t seed_value) {
    if (l == 0 && r == 0) return;
    auto left = (__int128_t)l, right = (__int128_t)r;
    auto negatives = left < 0 ? min(right, (__int128_t)-1) - left + 1 : 0;
    auto positives = right > 0 ? right - max(left, (__int128_t)1) + 1 : 0;
    auto ref_state = seed_value;
    auto rank = (__int128_t)reference_reduce(reference_raw(ref_state), (__uint128_t)(negatives + positives));
    auto expected = rank < negatives ? left + rank : max(left, (__int128_t)1) + rank - negatives;

    // 端点が 0 の場合と型の全域でも、常に生乱数を 1 個だけ消費する
    FastRng rng(seed_value);
    auto x = rng.uniform_closed_delta(l, r);
    t.require(l <= x && x <= r && x != 0, "delta closed range excludes zero");
    t.require((__int128_t)x == expected, "delta independent reference");
    t.require(rng.state == ref_state, "delta consumes one word");
}

// 整数 1 区間の出力と消費状態を確認する、期待値の加算も 128bit で行う
// 半開区間と閉区間は同じ生乱数から比較する
// l == r の場合は、閉区間だけが有効
// 生成結果からテスト用の区間を作らず、検証対象との循環を避ける
// T への変換前に期待値が範囲内であることも確かめる
template<class T>
void check_interval(Tester& t, T l, T r, uint64_t seed_value) {
    check_delta_interval(t, l, r, seed_value);
    auto width = (__int128_t)r - (__int128_t)l;
    auto ref_state = seed_value;
    auto raw = reference_raw(ref_state);
    FastRng closed(seed_value);
    auto expected = (__int128_t)l + (__int128_t)reference_reduce(raw, (__uint128_t)(width + 1));
    auto x = closed.uniform_closed(l, r);
    t.require(l <= x && x <= r, "integer closed range");
    t.require((__int128_t)x == expected, "integer closed reference");
    t.require(closed.state == ref_state, "closed consumes one word");

    if (l < r) {
        FastRng half(seed_value);
        expected = (__int128_t)l + (__int128_t)reference_reduce(raw, (__uint128_t)width);
        x = half.uniform(l, r);
        t.require(l <= x && x < r, "integer half-open range");
        t.require((__int128_t)x == expected, "integer half-open reference");
        t.require(half.state == ref_state, "half-open consumes one word");
    }
}

// 各整数型で、極値・幅の分岐点・ランダム区間を調べる
template<class T>
void test_integer(Tester& t, mt19937_64& data) {
    static_assert(HasUniform<T> && HasClosed<T>);
    static_assert(HasClosedDelta<T>);
    static_assert(same_as<decltype(FastRng{}.uniform_closed_delta(T{0}, T{1})), T>);
    static_assert(noexcept(FastRng{}.uniform_closed_delta(T{0}, T{1})));
    static_assert(same_as<decltype(FastRng{}.uniform(T{1})), T>);
    static_assert(same_as<decltype(FastRng{}.uniform(T{0}, T{1})), T>);
    static_assert(same_as<decltype(FastRng{}.uniform_closed(T{1})), T>);
    static_assert(same_as<decltype(FastRng{}.uniform_closed(T{0}, T{1})), T>);
    static_assert(noexcept(FastRng{}.uniform(T{1})));
    constexpr auto low = numeric_limits<T>::lowest();
    constexpr auto high = numeric_limits<T>::max();

    // 型の端点と 2 の冪付近を列挙し、全組合せを検証する
    vector<T> edges{low, (T)(low + 1), T{0}, T{1}, (T)(high - 1), high};
    if constexpr (is_signed_v<T>) edges.push_back((T)-1);
    for (auto k : {8, 16, 31, 32, 33, 52, 53, 54, 62, 63}) {
        for (auto d : {-1, 0, 1}) {
            auto v = ((__int128_t)1 << k) + d;
            if (v <= (__int128_t)high) edges.push_back((T)v);
            if (-v >= (__int128_t)low) edges.push_back((T)-v);
        }
    }
    sort(edges.begin(), edges.end());
    edges.erase(unique(edges.begin(), edges.end()), edges.end());
    for (auto l : edges) for (auto r : edges) if (l <= r) {
        for (auto raw : raw_edges) check_interval(t, l, r, seed_for_raw(raw));
    }

    // 1 引数版と 0 始まりの 2 引数版の系列を比較する
    for (auto r : edges) if (r >= 0) {
        FastRng a(data()), b = a;
        t.require(a.uniform_closed(r) == b.uniform_closed(T{0}, r), "closed one/two argument match");
        if (r > 0) t.require(a.uniform(r) == b.uniform(T{0}, r), "half-open one/two argument match");
    }

    // 独立 RNG で作った区間を、複数の生乱数で検証する
    for (auto i = 0; i < 12000; ++i) {
        auto l = (T)data(), r = (T)data();
        if (l > r) swap(l, r);
        for (auto j = 0; j < 4; ++j) check_interval(t, l, r, data());
    }
}

// 小さい整数型は全ての有効な端点組を列挙する
template<class T>
void test_all_8bit_intervals(Tester& t) {
    for (auto l = (int)numeric_limits<T>::lowest(); l <= (int)numeric_limits<T>::max(); ++l) {
        for (auto r = l; r <= (int)numeric_limits<T>::max(); ++r) {
            for (auto raw : {0ULL, 0x123456789abcdef0ULL, 1ULL << 63, UINT64_MAX - 1ULL, UINT64_MAX + 0ULL}) {
                check_interval(t, (T)l, (T)r, seed_for_raw(raw));
            }
        }
    }
}

// 8bit 整数の全有効端点組で、全ての出力候補が 1 対 1 に対応することを確認する
template<class T>
void test_all_8bit_deltas(Tester& t) {
    for (auto l = (int)numeric_limits<T>::lowest(); l <= (int)numeric_limits<T>::max(); ++l) {
        vector<T> allowed;
        for (auto r = l; r <= (int)numeric_limits<T>::max(); ++r) {
            if (r != 0) allowed.push_back((T)r);
            auto n = (uint64_t)allowed.size();
            for (auto k = uint64_t{0}; k < n; ++k) {
                // 縮小後の順位が k になる生乱数を選び、列挙した候補と比較する
                auto raw = has_single_bit(n) ? k : ((k << 32) + n - 1) / n;
                auto seed_value = seed_for_raw(raw);
                FastRng rng(seed_value);
                t.require(rng.uniform_closed_delta((T)l, (T)r) == allowed[(size_t)k], "delta exhaustive 8bit rank");
                t.require(rng.state == seed_value + 0x9e3779b97f4a7c15ULL, "delta exhaustive one word");
            }
        }
    }
}

// 対称・非対称・0 が端点の小区間を検査し、0 を含まない区間では既存系列も維持する
void test_delta_sequences(Tester& t) {
    for (auto [l, r] : {pair{-1, 1}, {-4, 2}, {-2, 7}, {-3, 0}, {0, 5}, {-9, -3}, {3, 9}, {-1, -1}, {1, 1}}) {
        FastRng rng(123456789);
        vector<int> allowed;
        for (auto x = l; x <= r; ++x) if (x != 0) allowed.push_back(x);
        vector<int> counts(allowed.size());
        for (auto i = 0; i < 200000; ++i) {
            auto before = rng;
            auto x = rng.uniform_closed_delta(l, r);
            auto it = find(allowed.begin(), allowed.end(), x);
            t.require(it != allowed.end(), "delta sequence range");
            ++counts[(size_t)(it - allowed.begin())];
            t.require(rng.state == before.state + 0x9e3779b97f4a7c15ULL, "delta sequence one word");
            if (l > 0 || r < 0) t.require(x == before.uniform_closed(l, r), "nonzero interval keeps closed sequence");
        }
        // 頻度検査は実装ミス検出用であり、厳密な無偏り性の検証ではない
        auto expected = 200000.0 / (double)counts.size();
        for (auto count : counts) t.require(abs(count - expected) < 8.0 * sqrt(expected), "delta frequency smoke test");
    }
}

// double の通常区間・隣接値・非正規化数・極大値と、上端への丸めを調べる
void test_double(Tester& t, mt19937_64& data) {
    auto check = [&](double l, double r, uint64_t seed_value) {
        FastRng rng(seed_value);
        auto x = rng.uniform(l, r);
        t.require(isfinite(x) && l <= x && x < r, "double half-open range");
        t.require(rng.state == seed_value + 0x9e3779b97f4a7c15ULL, "double consumes one word");
        if (l == 0.0) {
            FastRng other(seed_value);
            t.require(x == other.uniform(r), "double one/two argument match");
        }
    };

    // 生乱数の最大値を使い、確率に頼らず境界を確認する
    auto tiny = numeric_limits<double>::denorm_min();
    auto large = numeric_limits<double>::max();
    vector<pair<double, double>> intervals = {
        {0.0, 1.0}, {1.0, 2.0}, {-3.5, 7.25}, {-2.0, -1.0},
        {0.0, tiny}, {-tiny, 0.0}, {-tiny, tiny}, {tiny, 2 * tiny},
        {1.0, nextafter(1.0, 2.0)}, {nextafter(-1.0, -2.0), -1.0},
        {0.0, large}, {-large, 0.0}, {-large / 2, large / 2},
        {nextafter(large, 0.0), large}, {-large, nextafter(-large, 0.0)},
        {0.0, numeric_limits<double>::min()}, {-1.0e-300, 1.0e-300}
    };
    for (auto [l, r] : intervals) for (auto raw : raw_edges) check(l, r, seed_for_raw(raw));
    FastRng regression(0x31628af67b2131abULL);
    auto x = regression.uniform(1.0, 2.0);
    t.require(x == nextafter(2.0, 1.0), "upper-end rounding regression");

    // 乱数を double のビット列として解釈し、多様な指数の有効区間を作る
    auto accepted = 0;
    while (accepted < 50000) {
        auto l = bit_cast<double>(data()), r = bit_cast<double>(data());
        if (l > r) swap(l, r);
        if (!(isfinite(l) && isfinite(r) && l < r && isfinite(r - l))) continue;
        ++accepted;
        check(l, r, seed_for_raw(0));
        check(l, r, seed_for_raw(UINT64_MAX));
        for (auto j = 0; j < 4; ++j) check(l, r, data());
    }

    // 引数なしの範囲と式を確認し、簡易統計と確率端点も確認する
    FastRng rng(3141592653589793ULL), raw = rng;
    auto sum = 0.0;
    auto successes = 0;
    for (auto i = 0; i < 300000; ++i) {
        auto u = rng.uniform();
        t.require(0.0 <= u && u < 1.0, "unit double range");
        t.require(u == (double)(raw() >> 11) * 0x1.0p-53, "unit double reference");
        sum += u;
    }
    t.require(abs(sum / 300000.0 - 0.5) < 0.005, "unit double mean");
    for (auto i = 0; i < 300000; ++i) successes += rng.bernoulli(0.3);
    t.require(abs((double)successes / 300000.0 - 0.3) < 0.01, "bernoulli mean");
    auto state = rng.state;
    t.require(!rng.bernoulli(0.0) && rng.bernoulli(1.0), "bernoulli endpoints");
    t.require(rng.state == state, "bernoulli endpoints consume no words");
}

// 全テストを実行する、検査は NDEBUG に依存せず有効
void run_tests() {
    Tester t;
    mt19937_64 data(0x5b74d03ace197fa2ULL);

    // 既知の出力と独立実装、seed 再設定とコピーの再現性を確認する
    constexpr array<uint64_t, 3> known{0xe220a8397b1dcdafULL, 0x6e789e6aa1b965f4ULL, 0x06c45d188009454fULL};
    FastRng first;
    for (auto expected : known) t.require(first() == expected, "known splitmix64 sequence");
    for (auto raw : raw_edges) t.require(FastRng(seed_for_raw(raw))() == raw, "inverse seed");
    for (auto i = 0; i < 4096; ++i) {
        auto seed_value = data(), state = seed_value;
        FastRng rng(seed_value), copied = rng, reseeded;
        reseeded.seed(seed_value);
        for (auto j = 0; j < 32; ++j) {
            auto expected = reference_raw(state);
            t.require(rng() == expected, "raw reference");
            t.require(copied.next_u64() == expected && reseeded() == expected, "copy/reseed reference");
        }
    }
    FastRng raw(42), u32 = raw, bit = raw;
    for (auto i = 0; i < 20000; ++i) {
        auto x = raw();
        t.require(u32.next_u32() == (uint32_t)(x >> 32), "raw u32 extraction");
        t.require(bit.next_bool() == (bool)(x >> 63), "raw bool extraction");
    }
    t.require(FastRng::min() == 0 && FastRng::max() == UINT64_MAX, "URBG limits");

    // 組み込み整数型を網羅する、typedef の違いにも依存しない
    [&]<class... Ts>(tuple<Ts...>) { (test_integer<Ts>(t, data), ...); }
        (tuple<signed char, unsigned char, short, unsigned short, int, unsigned int,
               long, unsigned long, long long, unsigned long long,
               char, wchar_t, char8_t, char16_t, char32_t>{});
    test_all_8bit_intervals<int8_t>(t);
    test_all_8bit_intervals<uint8_t>(t);
    test_all_8bit_deltas<int8_t>(t);
    test_all_8bit_deltas<uint8_t>(t);
    test_delta_sequences(t);
    test_double(t, data);

    // 小区間の頻度が極端に崩れていないかを確認する、品質の完全な検証ではない
    for (auto n : {2, 3, 5, 7, 16, 31}) {
        FastRng rng(1234 + (uint64_t)n);
        vector<int> counts((size_t)n);
        for (auto i = 0; i < 200000; ++i) ++counts[(size_t)rng.uniform(n)];
        auto expected = 200000.0 / n;
        for (auto count : counts) t.require(abs(count - expected) < 8.0 * sqrt(expected), "frequency smoke test");
    }

    // 標準ライブラリの利用も維持する、メンバーの shuffle は追加しない
    FastRng rng(987654321);
    uniform_int_distribution<int> dist(-17, 31);
    uniform_real_distribution<double> real(-5.0, 2.0);
    bernoulli_distribution bern(0.7);
    auto sum = 0;
    for (auto i = 0; i < 100000; ++i) {
        auto x = dist(rng);
        auto y = real(rng);
        t.require(-17 <= x && x <= 31, "standard integer distribution");
        t.require(-5.0 <= y && y < 2.0, "standard real distribution");
        sum += bern(rng);
    }
    t.require(abs(sum - 70000) < 2000, "standard Bernoulli distribution");
    array<int, 128> permutation{};
    iota(permutation.begin(), permutation.end(), 0);
    shuffle(permutation.begin(), permutation.end(), rng);
    sort(permutation.begin(), permutation.end());
    for (auto i = 0; i < 128; ++i) t.require(permutation[(size_t)i] == i, "standard shuffle");
    cout << "All tests passed. checks = " << t.checks << '\n';
}

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


// 結果は計測後に消費する、ホットループでは volatile を使わない
volatile uint64_t bench_sink = 0;
struct Sample { double ms; uint64_t checksum; };

// バッチ単位で呼び出し境界を保つ、RNG と API 自体は通常通り最適化する
// double は 4 本の和に畳み、整数への変換をループに混ぜない
template<class R, class F>
[[gnu::noinline]] uint64_t benchmark_kernel(uint64_t seed_value, uint64_t count, F fn) {
    asm volatile("" : "+r"(seed_value), "+r"(count) : : "memory");
    R rng(seed_value);
    if constexpr (same_as<invoke_result_t<F, R&, uint64_t>, double>) {
        auto a = 0.0, b = 0.0, c = 0.0, d = 0.0;
        auto i = uint64_t{0};
        for (; i + 4 <= count; i += 4) {
            a += fn(rng, i); b += fn(rng, i + 1);
            c += fn(rng, i + 2); d += fn(rng, i + 3);
        }
        for (; i < count; ++i) a += fn(rng, i);
        return bit_cast<uint64_t>((a + b) + (c + d));
    } else {
        auto checksum = uint64_t{0};
        for (auto i = uint64_t{0}; i < count; ++i) checksum ^= (uint64_t)fn(rng, i);
        return checksum;
    }
}

template<class R, class F>
Sample measure(uint64_t seed_value, uint64_t count, F fn) {
    auto start = chrono::steady_clock::now();
    auto checksum = benchmark_kernel<R>(seed_value, count, fn);
    asm volatile("" : "+r"(checksum) : : "memory");
    auto end = chrono::steady_clock::now();
    bench_sink = bench_sink ^ checksum;
    return {chrono::duration<double, milli>(end - start).count(), checksum};
}

// worst_batch_ns/op は最も遅いバッチの平均、個別の 1 操作の最大時間ではない
void benchmark_heading(const char* name) {
    cout << "\n=== " << name << " ===\n";
    cout << left << setw(30) << "operation" << right << setw(12) << "ops/run"
         << setw(14) << "avg_run_ms" << setw(16) << "worst_run_ms"
         << setw(14) << "avg_ns/op" << setw(20) << "worst_batch_ns/op"
         << setw(23) << "checksum_xor" << '\n';
}

template<class R, class F>
void benchmark_row(const char* name, uint64_t count, int repeat, F fn) {
    bench_sink = bench_sink ^ benchmark_kernel<R>(123, min(count, uint64_t{50000}), fn);
    auto sum = 0.0, worst = 0.0;
    auto checksum = uint64_t{0};
    for (auto rep = 0; rep < repeat; ++rep) {
        auto sample = measure<R>((uint64_t)(1000 + rep), count, fn);
        sum += sample.ms;
        worst = max(worst, sample.ms);
        checksum ^= sample.checksum;
    }
    auto average = sum / repeat;
    cout << left << setw(30) << name << right << setw(12) << count
         << fixed << setprecision(3) << setw(14) << average << setw(16) << worst
         << setw(14) << average * 1e6 / (double)count << setw(20) << worst * 1e6 / (double)count
         << setw(23) << checksum << '\n';
}

// 初期化の少ない用途と多い用途を、同じ回数で比較する
void run_rng_benchmark(uint64_t count, int repeat) {
    auto raw = [](auto& rng, uint64_t) { return rng(); };
    auto reseed = [](auto& rng, uint64_t i) { rng.seed(123456789 + i); return rng(); };
    benchmark_heading("Case A: single seed + bulk generation");
    benchmark_row<FastRng>("FastRng", count, repeat, raw);
    benchmark_row<WyRand>("WyRand", count, repeat, raw);
    benchmark_row<Xoshiro256PlusPlus>("xoshiro256++", count, repeat, raw);
    benchmark_row<mt19937_64>("mt19937_64", count, repeat, raw);
    benchmark_heading("Case B: reseed + one generation");
    auto trials = min(count, uint64_t{100000});
    benchmark_row<FastRng>("FastRng", trials, repeat, reseed);
    benchmark_row<WyRand>("WyRand", trials, repeat, reseed);
    benchmark_row<Xoshiro256PlusPlus>("xoshiro256++", trials, repeat, reseed);
    benchmark_row<mt19937_64>("mt19937_64", trials, repeat, reseed);
}

// 型・端点・幅の分岐ごとに計測する、定数引数だけでなく可変幅も含める
void run_api_benchmark(uint64_t count, int repeat) {
    benchmark_heading("FastRng API benchmark");
    auto row = [&](const char* name, auto fn) { benchmark_row<FastRng>(name, count, repeat, fn); };
    row("next_u64", [](FastRng& rng, uint64_t) { return rng.next_u64(); });
    row("next_u32", [](FastRng& rng, uint64_t) { return rng.next_u32(); });
    row("next_bool", [](FastRng& rng, uint64_t) { return rng.next_bool(); });
    row("uniform<u32> pow2", [](FastRng& rng, uint64_t) { return rng.uniform(1U << 20); });
    row("uniform<u32> general", [](FastRng& rng, uint64_t) { return rng.uniform(1000000007U); });
    row("uniform<u64> pow2", [](FastRng& rng, uint64_t) { return rng.uniform(1ULL << 40); });
    row("uniform<u64> small", [](FastRng& rng, uint64_t) { return rng.uniform(1000000007ULL); });
    row("uniform<u64> medium", [](FastRng& rng, uint64_t) { return rng.uniform((1ULL << 40) + 12345); });
    row("uniform<u64> large", [](FastRng& rng, uint64_t) { return rng.uniform((1ULL << 63) + 12345); });
    row("uniform<int> r", [](FastRng& rng, uint64_t) { return rng.uniform(2001); });
    row("uniform<int> l,r", [](FastRng& rng, uint64_t) { return rng.uniform(-1000, 1001); });
    row("uniform<i64> l,r", [](FastRng& rng, uint64_t) { return rng.uniform(-1000000000000LL, 1000000000001LL); });
    row("uniform<u64> l,r", [](FastRng& rng, uint64_t) { return rng.uniform(123456789ULL, 1123456797ULL); });
    row("closed<int> small", [](FastRng& rng, uint64_t) { return rng.uniform_closed(-1000, 1000); });
    row("closed<int> wide", [](FastRng& rng, uint64_t) { return rng.uniform_closed(-1000000000, 1000000000); });
    row("closed<int> full", [](FastRng& rng, uint64_t) { return rng.uniform_closed(INT32_MIN, INT32_MAX); });
    row("closed<i64> medium", [](FastRng& rng, uint64_t) { return rng.uniform_closed(-1000000000000LL, 1000000000000LL); });
    row("closed<i64> full", [](FastRng& rng, uint64_t) { return rng.uniform_closed(INT64_MIN, INT64_MAX); });
    row("closed<u64> small", [](FastRng& rng, uint64_t) { return rng.uniform_closed(123456789ULL, 1123456796ULL); });
    row("closed<u64> large", [](FastRng& rng, uint64_t) { return rng.uniform_closed(1ULL << 63, UINT64_MAX - 12345ULL); });
    row("closed<u64> full", [](FastRng& rng, uint64_t) { return rng.uniform_closed<uint64_t>(0, UINT64_MAX); });
    row("closed<int> r", [](FastRng& rng, uint64_t) { return rng.uniform_closed(1000); });
    row("uniform()", [](FastRng& rng, uint64_t) { return rng.uniform(); });
    row("uniform<double> r", [](FastRng& rng, uint64_t) { return rng.uniform(7.25); });
    row("uniform<double> l,r", [](FastRng& rng, uint64_t) { return rng.uniform(-3.5, 7.25); });
    row("bernoulli(0.3)", [](FastRng& rng, uint64_t) { return rng.bernoulli(0.3); });

    // 差分生成は非ゼロ候補への写像と単純な再抽選を比較する（生成列は異なる）
    row("delta<int> +/-1", [](FastRng& rng, uint64_t) { return rng.uniform_closed_delta(-1, 1); });
    row("retry<int> +/-1", [](FastRng& rng, uint64_t) {
        auto x = 0;
        do { x = rng.uniform_closed(-1, 1); } while (x == 0);
        return x;
    });
    row("delta<int> +/-1000", [](FastRng& rng, uint64_t) { return rng.uniform_closed_delta(-1000, 1000); });
    row("retry<int> +/-1000", [](FastRng& rng, uint64_t) {
        auto x = 0;
        do { x = rng.uniform_closed(-1000, 1000); } while (x == 0);
        return x;
    });
    row("delta<int> positive", [](FastRng& rng, uint64_t) { return rng.uniform_closed_delta(1, 1000); });
    row("delta<int> negative", [](FastRng& rng, uint64_t) { return rng.uniform_closed_delta(-1000, -1); });
    row("delta<int> zero-left", [](FastRng& rng, uint64_t) { return rng.uniform_closed_delta(0, 1000); });
    row("delta<int> zero-right", [](FastRng& rng, uint64_t) { return rng.uniform_closed_delta(-1000, 0); });
    row("delta<i64> medium", [](FastRng& rng, uint64_t) { return rng.uniform_closed_delta(-1000000000000LL, 1000000000000LL); });
    row("delta<i64> full", [](FastRng& rng, uint64_t) { return rng.uniform_closed_delta(INT64_MIN, INT64_MAX); });
    row("delta<u64> full", [](FastRng& rng, uint64_t) { return rng.uniform_closed_delta<uint64_t>(0, UINT64_MAX); });

    // 混在する幅で、定数畳み込みされない実行時分岐も測る
    array<uint64_t, 1024> widths{};
    mt19937_64 data(9173);
    for (auto i = size_t{0}; i < widths.size(); ++i) {
        auto x = data();
        if (i % 3 == 0) widths[i] = 1 + x % UINT32_MAX;
        else if (i % 3 == 1) widths[i] = (1ULL << 32) + x % (1ULL << 51);
        else widths[i] = (1ULL << 54) + x % (1ULL << 62);
    }
    row("uniform<u64> runtime-mixed", [&](FastRng& rng, uint64_t i) { return rng.uniform(widths[i & 1023]); });

    // 0 をまたぐ・またがない・端点が 0 の区間を混ぜて、実行時引数で計測する
    array<pair<int, int>, 1024> deltas{};
    for (auto i = size_t{0}; i < deltas.size(); ++i) {
        auto a = 1 + (int)(data() % 100000), b = 1 + (int)(data() % 100000);
        switch (i % 5) {
            case 0: deltas[i] = {-a, b}; break;
            case 1: deltas[i] = {a, a + b}; break;
            case 2: deltas[i] = {-a - b, -a}; break;
            case 3: deltas[i] = {0, b}; break;
            default: deltas[i] = {-a, 0}; break;
        }
    }
    shuffle(deltas.begin(), deltas.end(), data);
    row("delta<int> runtime-mixed", [&](FastRng& rng, uint64_t i) {
        auto [l, r] = deltas[i & 1023];
        return rng.uniform_closed_delta(l, r);
    });
    row("retry<int> runtime-mixed", [&](FastRng& rng, uint64_t i) {
        auto [l, r] = deltas[i & 1023];
        auto x = 0;
        do { x = rng.uniform_closed(l, r); } while (x == 0);
        return x;
    });
}
}

// 引数なしはテストと全ベンチ、--tests-only はテストのみ
// ベンチは --bench-only / --api-bench-only / --rng-bench-only [回数 [反復数]]
int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    auto mode = argc > 1 ? string_view(argv[1]) : string_view{};
    auto count = uint64_t{5000000};
    auto repeat = 4;
    auto parse = [](const char* text, auto& value) {
        auto end = text + strlen(text);
        auto [ptr, error] = from_chars(text, end, value);
        return error == errc{} && ptr == end && value > 0;
    };
    if (argc > 4 || (argc > 2 && !parse(argv[2], count)) || (argc > 3 && !parse(argv[3], repeat)) ||
        !(mode.empty() || mode == "--tests-only" || mode == "--bench-only" ||
          mode == "--api-bench-only" || mode == "--rng-bench-only")) {
        cerr << "Usage: rng [--tests-only|--bench-only|--api-bench-only|--rng-bench-only] [count [repeat]]\n";
        return 2;
    }
    cout << "Compiler: GCC " << __VERSION__ << '\n';
    if (mode.empty() || mode == "--tests-only") run_tests();
    if (mode.empty() || mode == "--bench-only" || mode == "--rng-bench-only") run_rng_benchmark(count, repeat);
    if (mode.empty() || mode == "--bench-only" || mode == "--api-bench-only") run_api_benchmark(count, repeat);
    if (mode != "--tests-only") cout << "\nfinal_sink = " << bench_sink << '\n';
}
#endif
