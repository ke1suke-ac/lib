#pragma once
#include <bits/stdc++.h>
using namespace std;

// FastRng: C++17 / GCC 用、splitmix64 を使う状態 64bit の競技プログラミング用 RNG
// uniform は半開区間、uniform_closed は整数の閉区間を返す
// uniform_closed_delta は整数の閉区間から 0 を除いた差分を返す
// 配列は vector / array のみ対応、uniform / uniform_closed は充填、perm は開始値付き順列
// uniform_sum は double 配列を非負・合計指定の一様分布で埋める
// normal は平均・標準偏差を指定する正規乱数、double のスカラーと配列に対応
// weighted_index は重み付きの添字を選ぶ、反復利用は WeightedTable で事前準備
// 想定型は bool を除く 64bit 以下の整数と double、2 引数は同じ型、厳密な型検査はしない
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
    // vector / array の要素型、オーバーロードの区別にだけ使う
    template<class R>
    using array_element = typename decay_t<R>::value_type;

    // 非空・非負・有限の重みを合計する、合計は正かつ有限が前提
    template<class A>
    static double weight_sum(const A& a) noexcept {
        assert(!a.empty() && a.size() <= INT32_MAX);
        auto sum = accumulate(a.begin(), a.end(), 0.0);
        assert(sum > 0 && isfinite(sum));
        return sum;
    }

    // 整数を [0, n) に縮小する共通処理、内部でのみ n == 0 を要素数 2^64 と解釈
    inline result_type bounded(result_type n) noexcept {
        if (!n) return next_u64();
        auto x = (*this)();
        if ((n & (n - 1)) == 0) return x & (n - 1);
        if ((uint32_t)(n >> 32) == 0) return (result_type)(((uint64_t)(uint32_t)x * (uint32_t)n) >> 32);
        if (n <= (1ULL << 53)) return (result_type)((double)n * (double)(x >> 11) * 0x1.0p-53);
        return (result_type)(((__uint128_t)x * n) >> 64);
    }

    // 極座標法で標準正規乱数を 2 個生成、単位円の外と原点は再抽選する
    inline pair<double, double> normal_pair() noexcept {
        for (;;) {
            auto x = 2 * uniform() - 1, y = 2 * uniform() - 1;
            auto r = x * x + y * y;
            if (r >= 1 || r == 0) continue;
            auto scale = sqrt(-2 * log(r) / r);
            return {x * scale, y * scale};
        }
    }

public:
    // [0, 1) の double を返す、上位 53bit を使う
    inline double uniform() noexcept {
        return (double)((*this)() >> 11) * 0x1.0p-53;
    }

    // 半開区間 [0, r)、r > 0 が前提、戻り値は引数と同じ型
    template<class T, enable_if_t<is_arithmetic_v<T>, int> = 0>
    inline T uniform(T r) noexcept { return uniform(T{0}, r); }

    // 半開区間 [l, r)、l < r が前提、double は端点と r - l が有限の範囲のみ
    template<class T>
    inline T uniform(T l, T r) noexcept {
        assert(l < r);
        if constexpr (is_same_v<remove_cv_t<T>, double>) {
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
    template<class T>
    inline T uniform_closed(T r) noexcept { return uniform_closed(T{0}, r); }

    // 整数の閉区間 [l, r]、l <= r が前提、同値端点と型の全域にも対応
    template<class T>
    inline T uniform_closed(T l, T r) noexcept {
        assert(l <= r);
        auto ul = (uint64_t)l;
        return (T)(ul + bounded((uint64_t)r - ul + 1));
    }

    // 整数の閉区間 [l, r] から 0 を除いた差分、l <= r かつ [0, 0] 以外が前提
    template<class T>
    inline T uniform_closed_delta(T l, T r) noexcept {
        assert(l <= r && (l != 0 || r != 0));
        // 0 を含むときは候補を 1 個減らし、非負の結果を 1 ずらす（再抽選なし）
        auto skip_zero = l <= 0 && r >= 0;
        auto x = uniform_closed(l, (T)(r - skip_zero));
        return (T)(x + (skip_zero && x >= 0));
    }

    // 平均 mean・標準偏差 stddev の正規乱数、引数は有限かつ stddev >= 0、0 なら乱数不要
    inline double normal(double mean = 0.0, double stddev = 1.0) noexcept {
        assert(isfinite(mean) && isfinite(stddev) && stddev >= 0);
        return stddev == 0 ? mean : mean + stddev * normal_pair().first;
    }

    // double 配列を [0, 1) で埋める、空配列は乱数を消費しない
    template<class R, class = array_element<R>>
    inline void uniform(R&& out) noexcept {
        for (auto& x : out) x = uniform();
    }

    // 配列を半開区間 [0, r) で埋める、端点は配列の要素型に変換される
    template<class R>
    inline void uniform(R&& out, array_element<R> r) noexcept {
        uniform(out, array_element<R>{0}, r);
    }

    // 配列を半開区間 [l, r) で埋める、各要素は先頭からスカラー版で生成する
    template<class R>
    inline void uniform(R&& out, array_element<R> l, array_element<R> r) noexcept {
        for (auto& x : out) x = uniform(l, r);
    }

    // 整数配列を閉区間 [0, r] で埋める
    template<class R>
    inline void uniform_closed(R&& out, array_element<R> r) noexcept {
        uniform_closed(out, array_element<R>{0}, r);
    }

    // 整数配列を閉区間 [l, r] で埋める、各要素は先頭からスカラー版で生成する
    template<class R>
    inline void uniform_closed(R&& out, array_element<R> l, array_element<R> r) noexcept {
        for (auto& x : out) x = uniform_closed(l, r);
    }

    // double 配列を正規乱数で埋める、空配列・標準偏差 0 は乱数不要、生成値の桁あふれは対象外
    template<class R, class = array_element<R>>
    inline void normal(R&& a, double mean = 0.0, double stddev = 1.0) noexcept {
        if (a.empty()) return;
        assert(isfinite(mean) && isfinite(stddev) && stddev >= 0);
        if (stddev == 0) { fill(a.begin(), a.end(), mean); return; }
        // 2 個ずつ使い切る、スカラー版の反復とは列が異なる、呼び出しをまたぐ保存はしない
        auto i = size_t{0};
        for (; i + 1 < a.size(); i += 2) {
            auto [x, y] = normal_pair();
            a[i] = mean + stddev * x;
            a[i + 1] = mean + stddev * y;
        }
        if (i < a.size()) a[i] = normal(mean, stddev);
    }

    // double 配列を非負・合計 s の単体上一様分布で埋める、合計の丸め誤差は許容する
    template<class R>
    inline void uniform_sum(R&& a, double s = 1.0) noexcept {
        assert(s >= 0 && isfinite(s) && (!a.empty() || s == 0));
        // 自明な場合は乱数不要、2 要素は一様乱数 1 個と残りに分割する
        if (a.size() <= 1 || s == 0) { fill(a.begin(), a.end(), s); return; }
        if (a.size() == 2) { a[0] = uniform(s); a[1] = s - a[0]; return; }
        // 3 要素は正方形を三角形へ折り畳む、乱数 2 個で再抽選・対数・除算は不要
        if (a.size() == 3) {
            auto x = uniform(), y = uniform();
            a[0] = ::min(x, 1 - y) * s; a[1] = ::min(y, 1 - x) * s;
            a[2] = abs((1 - y) - x) * s;
            return;
        }
        // (0, 1) の 52bit 中点格子から指数乱数を作り、log(0) と総和 0 を避ける
        auto sum = 0.0;
        for (auto& x : a) {
            x = -log(((double)((*this)() >> 12) + 0.5) * 0x1p-52);
            sum += x;
        }
        // 先に比率を求め、s / sum のオーバーフロー・アンダーフローを避ける
        for (auto& x : a) x = (x / sum) * s;
    }

    // start から配列サイズ個の整数をランダムに並べる、全ての値が要素型に収まることが前提
    template<class R>
    inline void perm(R&& a, array_element<R> start = 0) noexcept {
        using T = array_element<R>;
        if (a.empty()) return;
        assert(a.size() - 1 <= (uint64_t)numeric_limits<T>::max() - (uint64_t)start);
        // 先頭を初期化し、以降は連番の追加と入れ替えを 1 パスで行う
        a[0] = start;
        for (auto i = size_t{1}; i < a.size(); ++i) {
            // unsigned で計算して、型の最大値付近でも符号付きの桁あふれを避ける
            a[i] = (T)((uint64_t)start + i);
            swap(a[i], a[uniform(i + 1)]);
        }
    }

    // 同じ重みからの反復抽選用エイリアス表、元配列は保持せず、更新は assign で再構築
    struct WeightedTable {
    private:
        friend struct FastRng;
        struct Entry { double probability; int32_t alias; };
        vector<Entry> entries;

    public:
        // 未構築の空テーブル、抽選前に assign が必要
        WeightedTable() = default;

        // 整数・double の重み配列から表を構築する、乱数は消費しない
        template<class R, class = array_element<R>>
        explicit WeightedTable(R&& weights) { assign(weights); }

        // 表を再構築する、重みは非空・非負・有限、合計は正かつ有限が前提
        template<class R>
        void assign(R&& a) {
            auto sum = weight_sum(a);
            auto n = (int32_t)a.size();
            entries.resize(a.size());
            vector<int32_t> small, large;
            small.reserve(a.size());
            large.reserve(a.size());
            auto positive = int32_t{0};

            // 平均を 1 に正規化し、小さい列と大きい列に分ける
            for (auto i = int32_t{0}; i < n; ++i) {
                auto p = ((double)a[i] / sum) * n;
                entries[i] = {p, i};
                (p < 1 ? small : large).push_back(i);
                if (a[i] > 0) positive = i;
            }
            // 小さい列の不足分を大きい列で埋め、代替添字を記録する
            while (!small.empty() && !large.empty()) {
                auto i = small.back(), j = large.back();
                small.pop_back();
                entries[i].alias = j;
                auto& p = entries[j].probability;
                p = (p - 1) + entries[i].probability;
                if (p < 1) { large.pop_back(); small.push_back(j); }
            }
            // 丸めで残った列を閉じる、重み 0 の列は正の重みへ必ず転送する
            for (auto i : large) entries[i].probability = 1;
            for (auto i : small) entries[i] = {a[i] > 0 ? 1.0 : 0.0, positive};
        }
    };

    // 重みに比例して添字を 1 個選ぶ、確保なし・乱数 1 個、重み 0 は選ばない
    template<class R, class = array_element<R>>
    inline int32_t weighted_index(R&& a) noexcept {
        auto x = uniform(weight_sum(a));
        auto i = int32_t{0};
        auto sum = (double)a[0];
        while (x >= sum) sum += (double)a[++i];
        return i;
    }

    // 準備済みの表から添字を 1 個選ぶ、確保・再抽選なし、乱数 1 個
    inline int32_t weighted_index(const WeightedTable& table) noexcept {
        auto x = uniform((double)table.entries.size());
        auto i = (int32_t)x;
        const auto& e = table.entries[i];
        return x - i < e.probability ? i : e.alias;
    }

    // 確率 p で true、0 <= p <= 1 が前提、端点確率では乱数を消費しない
    inline bool bernoulli(double p) noexcept {
        assert(0.0 <= p && p <= 1.0);
        return p > 0.0 && (p >= 1.0 || uniform() < p);
    }
};

#if __INCLUDE_LEVEL__ == 0
// テスト用の数値補助、C++17で使える標準処理だけを使用する
namespace {
template<class To, class From> To test_bit_cast(const From& value) {
    static_assert(sizeof(To) == sizeof(From));
    To result;
    memcpy(&result, &value, sizeof(result));
    return result;
}
template<class T> bool test_has_single_bit(T x) { return x != 0 && (x & (x - 1)) == 0; }
uint64_t test_rotl(uint64_t x, int s) { return (x << s) | (x >> (64 - s)); }
template<class T> struct TestType { using type = T; };
template<class... T, class F> void test_each_type(tuple<T...>, F fn) { (fn(TestType<T>{}), ...); }
}

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
                auto raw = test_has_single_bit(n) ? k : ((k << 32) + n - 1) / n;
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
        auto l = test_bit_cast<double>(data()), r = test_bit_cast<double>(data());
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

// スカラーを順番に呼んだ場合と、全要素・生乱数消費・配列サイズが一致することを検査する
template<class T, class Fill, class Scalar>
void check_fill(Tester& t, size_t n, uint64_t seed_value, Fill fill, Scalar scalar) {
    vector<T> values;
    values.resize(n);
    auto original_data = values.data();
    FastRng rng(seed_value), reference(seed_value);
    fill(rng, values);
    t.require(values.size() == n && values.data() == original_data, "fill keeps storage and size");
    for (auto x : values) {
        auto expected = scalar(reference);
        if constexpr (is_same_v<T, double>) {
            t.require(test_bit_cast<uint64_t>(x) == test_bit_cast<uint64_t>(expected), "double fill scalar bit equality");
        } else {
            t.require(x == expected, "integer fill scalar equality");
        }
    }
    t.require(rng.state == reference.state, "fill scalar state equality");
}

template<class T>
void test_integer_arrays(Tester& t, mt19937_64& data) {
    // 型の全域と 1 候補を含めて、4 種の配列オーバーロードを比較する
    auto check = [&](size_t n, T l, T r, uint64_t seed_value) {
        check_fill<T>(t, n, seed_value,
            [&](FastRng& rng, auto& a) { rng.uniform_closed(a, l, r); },
            [&](FastRng& rng) { return rng.uniform_closed(l, r); });
        if (l < r) {
            check_fill<T>(t, n, seed_value,
                [&](FastRng& rng, auto& a) { rng.uniform(a, l, r); },
                [&](FastRng& rng) { return rng.uniform(l, r); });
        }
        if (r >= 0) {
            check_fill<T>(t, n, seed_value,
                [&](FastRng& rng, auto& a) { rng.uniform_closed(a, r); },
                [&](FastRng& rng) { return rng.uniform_closed(r); });
            if (r > 0) {
                check_fill<T>(t, n, seed_value,
                    [&](FastRng& rng, auto& a) { rng.uniform(a, r); },
                    [&](FastRng& rng) { return rng.uniform(r); });
            }
        }
    };
    for (auto n : {0U, 1U, 2U, 7U, 32U, 257U}) {
        for (auto raw : raw_edges) {
            check(n, numeric_limits<T>::min(), numeric_limits<T>::max(), seed_for_raw(raw));
            check(n, T{0}, T{0}, seed_for_raw(raw));
            check(n, T{1}, T{7}, seed_for_raw(raw));
        }
    }
    for (auto i = 0; i < 500; ++i) {
        auto l = (T)data(), r = (T)data();
        if (l > r) swap(l, r);
        check((size_t)(data() % 129), l, r, data());
    }

    // vector と array は同じ系列、サイズや格納領域は変更しない
    vector<T> values(17);
    array<T, 17> fixed{};
    FastRng a(123), b(123);
    a.uniform(values, T{1}, T{7}); b.uniform(fixed, T{1}, T{7});
    t.require(equal(values.begin(), values.end(), fixed.begin()), "fill vector/array equivalence");
    t.require(a.state == b.state, "fill vector/array state");
    array<T, 0> empty_array{};
    vector<T> empty_vector;
    auto state = a.state;
    a.uniform(empty_vector, T{1}); a.uniform_closed(empty_array, T{0});
    t.require(a.state == state, "empty fill consumes no words");

}

void test_double_arrays(Tester& t, mt19937_64& data) {
    auto check = [&](size_t n, double l, double r, uint64_t seed_value) {
        check_fill<double>(t, n, seed_value,
            [&](FastRng& rng, auto& a) { rng.uniform(a, l, r); },
            [&](FastRng& rng) { return rng.uniform(l, r); });
        if (r > 0.0) {
            check_fill<double>(t, n, seed_value,
                [&](FastRng& rng, auto& a) { rng.uniform(a, r); },
                [&](FastRng& rng) { return rng.uniform(r); });
        }
        check_fill<double>(t, n, seed_value,
            [](FastRng& rng, auto& a) { rng.uniform(a); },
            [](FastRng& rng) { return rng.uniform(); });
    };
    auto tiny = numeric_limits<double>::denorm_min(), large = numeric_limits<double>::max();
    for (auto [l, r] : vector<pair<double, double>>{
        {0, 1}, {1, 2}, {-3.5, 7.25}, {-2, -1}, {-tiny, tiny}, {0, tiny},
        {1, nextafter(1.0, 2.0)}, {0, large}, {-large, 0}, {nextafter(large, 0.0), large}}) {
        for (auto n : {0U, 1U, 7U, 33U, 257U}) {
            for (auto raw : raw_edges) check(n, l, r, seed_for_raw(raw));
        }
    }
    for (auto accepted = 0; accepted < 2000;) {
        auto l = test_bit_cast<double>(data()), r = test_bit_cast<double>(data());
        if (l > r) swap(l, r);
        if (!(isfinite(l) && isfinite(r) && l < r && isfinite(r - l))) continue;
        check((size_t)(data() % 129), l, r, data());
        ++accepted;
    }

    // 過去の上端丸めの再現 seed を配列版でも確認する
    array<double, 1> regression{};
    FastRng rng(0x31628af67b2131abULL);
    rng.uniform(regression, 1.0, 2.0);
    t.require(regression[0] == nextafter(2.0, 1.0), "array upper-end rounding regression");
    array<double, 9> a{};
    vector<double> b(9);
    FastRng first(11), second(11);
    first.uniform(a); second.uniform(b);
    t.require(equal(a.begin(), a.end(), b.begin()), "unit double vector/array equivalence");
    t.require(first.state == second.state, "unit double vector/array state");

}

// 型の端でも 128bit で正解を数え、順列の一意性・範囲・消費回数を独立に確認する
template<class T>
void check_perm(Tester& t, size_t n, T start, uint64_t seed_value) {
    vector<T> actual(n, T{7}), reference(n);
    auto storage = actual.data();
    FastRng rng(seed_value);
    rng.perm(actual, start);
    t.require(actual.data() == storage && actual.size() == n, "perm keeps storage and size");
    for (auto i = size_t{0}; i < n; ++i) reference[i] = (T)((__int128_t)start + i);
    auto state = seed_value;
    // 参照側は全連番を先に作り、独立した乱数生成・縮小処理で前から交換する
    for (auto i = size_t{1}; i < n; ++i) {
        auto j = (size_t)reference_reduce(reference_raw(state), i + 1);
        swap(reference[i], reference[j]);
    }
    t.require(actual == reference, "perm independent draw reference");
    t.require(rng.state == state, "perm consumes max(n-1,0) words");
    sort(actual.begin(), actual.end());
    for (auto i = size_t{0}; i < n; ++i) {
        t.require((__int128_t)actual[i] == (__int128_t)start + i, "perm each consecutive value exactly once");
    }
}

template<class T>
void test_permutations(Tester& t, mt19937_64& data) {
    auto low = (__int128_t)numeric_limits<T>::min(), high = (__int128_t)numeric_limits<T>::max();
    auto limit = (size_t)min((__int128_t)513, high - low + 1);
    for (auto n : {size_t{0}, size_t{1}, size_t{2}, size_t{7}, size_t{31}, limit}) {
        for (auto raw : raw_edges) {
            check_perm<T>(t, n, (T)low, seed_for_raw(raw));
            check_perm<T>(t, n, (T)(high - (__int128_t)(n ? n - 1 : 0)), seed_for_raw(raw));
        }
    }
    for (auto i = 0; i < 500; ++i) {
        auto n = (size_t)(data() % (limit + 1));
        auto top = high - (__int128_t)(n ? n - 1 : 0);
        auto start = (T)(low + (__int128_t)((__uint128_t)data() % (__uint128_t)(top - low + 1)));
        check_perm<T>(t, n, start, data());
    }

    // 初期内容は無関係、開始値省略と明示 0、vector と array も一致する
    array<T, 17> a{}, b{};
    vector<T> c(17);
    a.fill(T{9}); b.fill(T{7});
    FastRng first(89), second(89), third(89);
    first.perm(a); second.perm(b, T{0}); third.perm(c);
    t.require(a == b && equal(a.begin(), a.end(), c.begin()), "perm independent of initial contents");
    t.require(first.state == second.state && second.state == third.state, "perm container state equivalence");
    array<T, 0> empty{};
    vector<T> empty_vector;
    auto state = first.state;
    first.perm(empty, numeric_limits<T>::max()); first.perm(empty_vector);
    t.require(first.state == state, "empty perm consumes no words");

}

void test_permutation_frequencies(Tester& t) {
    // 小さい n で全順列が出現し、頻度が極端に偏っていないかを確認する
    // 速度優先の範囲縮小を使用するので、数学的な無偏り性の証明ではない
    for (auto n = 2; n <= 5; ++n) {
        auto factorial = 1;
        for (auto i = 2; i <= n; ++i) factorial *= i;
        vector<int> counts((size_t)factorial), a((size_t)n);
        FastRng rng(0x742937dfULL + (uint64_t)n);
        auto trials = 240000;
        for (auto rep = 0; rep < trials; ++rep) {
            rng.perm(a);
            auto rank = 0;
            for (auto i = 0; i < n; ++i) {
                auto smaller = 0;
                for (auto j = i + 1; j < n; ++j) smaller += a[(size_t)j] < a[(size_t)i];
                rank = rank * (n - i) + smaller;
            }
            ++counts[(size_t)rank];
        }
        auto expected = (double)trials / factorial;
        for (auto frequency : counts) t.require(abs(frequency - expected) < 8 * sqrt(expected), "permutation frequency smoke test");
    }
}

void test_array_apis(Tester& t, mt19937_64& data) {
    test_each_type(tuple<signed char, unsigned char, short, unsigned short, int, unsigned int,
            long, unsigned long, long long, unsigned long long,
            char, wchar_t, char16_t, char32_t>{}, [&](auto type) {
        using T = typename decltype(type)::type;
        test_integer_arrays<T>(t, data);
        test_permutations<T>(t, data);
    });
    test_double_arrays(t, data);
    test_permutation_frequencies(t);
}

// 独立した累積配列と upper_bound でワンショットの結果を照合する
int32_t reference_weighted(const vector<double>& weights, uint64_t raw) {
    vector<double> prefix(weights.size());
    partial_sum(weights.begin(), weights.end(), prefix.begin());
    auto sum = prefix.back();
    auto u = (double)(raw >> 11) * 0x1.0p-53;
    auto x = sum * u;
    if (!(x < sum)) x = nextafter(sum, 0.0);
    return (int32_t)(upper_bound(prefix.begin(), prefix.end(), x) - prefix.begin());
}

// 範囲・重み 0 の除外・再現性・乱数消費を、任意の重みと seed で確かめる
void check_weighted(Tester& t, const vector<double>& weights, const FastRng::WeightedTable& table,
                    uint64_t seed_value) {
    FastRng one(seed_value), prepared(seed_value), copied(seed_value);
    auto state = seed_value;
    auto raw = reference_raw(state);
    auto i = one.weighted_index(weights), j = prepared.weighted_index(table);
    t.require(i == reference_weighted(weights, raw), "weighted one-shot reference");
    t.require(i >= 0 && (size_t)i < weights.size() && weights[(size_t)i] > 0, "weighted one-shot support");
    t.require(j >= 0 && (size_t)j < weights.size() && weights[(size_t)j] > 0, "weighted table support");
    t.require(j == copied.weighted_index(table), "weighted table repeatability");
    t.require(one.state == state && prepared.state == state, "weighted one raw per sample");
}

template<class T>
void test_weight_type(Tester& t) {
    const array<T, 5> a{T{0}, T{1}, T{3}, T{6}, T{0}};
    const vector<T> v(a.begin(), a.end());
    FastRng::WeightedTable first(a), second(v);
    for (auto raw : raw_edges) {
        auto seed_value = seed_for_raw(raw);
        FastRng r1(seed_value), r2(seed_value);
        t.require(r1.weighted_index(a) == r2.weighted_index(v), "weighted const vector/array");
        t.require(r1.state == r2.state, "weighted containers state");
        r1.seed(seed_value); r2.seed(seed_value);
        t.require(r1.weighted_index(first) == r2.weighted_index(second), "weighted table containers");
        t.require(r1.state == r2.state, "weighted table containers state");
    }
    if constexpr (!is_same_v<T, double>) {
        vector<T> edge{numeric_limits<T>::max(), T{0}, numeric_limits<T>::max()};
        FastRng::WeightedTable table(edge);
        FastRng rng(31);
        for (auto i = 0; i < 1000; ++i) {
            t.require(rng.weighted_index(edge) != 1, "weighted integer maximum one-shot");
            t.require(rng.weighted_index(table) != 1, "weighted integer maximum table");
        }
    }
}

// 正規化後の目標確率を長倍精度で求め、多数抽選の頻度を比較する
void test_weighted_frequencies(Tester& t) {
    const vector<vector<double>> cases{
        {1, 3, 6, 0}, {0, 1, 0}, {0, 2, 0, 3, 0}, {1, 1, 1, 1, 1},
        {1, 100, 1, 1}, {100, 1, 1, 1}, {1, 1, 1, 100},
        {0.1, 0.2, 0.3, 0.4}, {1e-200, 2e-200, 7e-200}, {1e200, 2e200, 7e200},
        {1e-4, 1, 2, 3}, {1, 0, 1, 0, 1, 0, 1}
    };
    auto case_id = uint64_t{0};
    constexpr auto draws = 300000;
    for (const auto& weights : cases) {
        FastRng::WeightedTable table(weights);
        FastRng one(9173 + case_id), prepared(3100 + case_id++);
        vector<int> a(weights.size()), b(weights.size());
        for (auto i = 0; i < draws; ++i) {
            ++a[(size_t)one.weighted_index(weights)];
            ++b[(size_t)prepared.weighted_index(table)];
        }
        auto total = accumulate(weights.begin(), weights.end(), 0.0L);
        for (auto i = size_t{0}; i < weights.size(); ++i) {
            auto p = (long double)weights[i] / total;
            auto expected = p * draws;
            auto tolerance = 9 * sqrt(expected * (1 - p)) + 4;
            t.require(abs(a[i] - expected) <= tolerance, "weighted one-shot frequencies");
            t.require(abs(b[i] - expected) <= tolerance, "weighted table frequencies");
            if (weights[i] == 0) t.require(a[i] == 0 && b[i] == 0, "weighted zero frequency");
        }
    }
}

void test_weighted(Tester& t, mt19937_64& data) {
    auto before = t.checks;
    test_each_type(tuple<signed char, unsigned char, short, unsigned short, int, unsigned int,
               long, unsigned long, long long, unsigned long long,
               char, wchar_t, char16_t, char32_t, double>{}, [&](auto type) {
        using T = typename decltype(type)::type; test_weight_type<T>(t); });

    // 小さい整数重みを全列挙し、端点の乱数と任意 seed から抽選する
    for (auto n = 1; n <= 6; ++n) {
        for (auto code = 1; code < (1 << (2 * n)); ++code) {
            vector<double> weights((size_t)n);
            for (auto i = 0; i < n; ++i) weights[(size_t)i] = (code >> (2 * i)) & 3;
            FastRng::WeightedTable table(weights);
            for (auto raw : raw_edges) check_weighted(t, weights, table, seed_for_raw(raw));
            check_weighted(t, weights, table, data());
        }
    }

    // 隣接値・非正規化数・極大値、0 が先頭/途中/末尾に並ぶ配列
    auto tiny = numeric_limits<double>::denorm_min();
    auto big = numeric_limits<double>::max();
    vector<vector<double>> edges{{tiny}, {0, tiny, 0}, {tiny, tiny, tiny, 0},
        {big}, {0, big, 0}, {big / 2, big / 4, 0}, {tiny, big, 0}, {0, 1, tiny},
        {1, nextafter(1.0, 0.0), nextafter(1.0, 2.0)}, {-0.0, 1, -0.0}};
    for (auto n : {1, 2, 3, 7, 16, 63, 257, 1024}) {
        for (auto index : {0, n / 2, n - 1}) {
            vector<double> w((size_t)n);
            w[(size_t)index] = 1;
            edges.push_back(move(w));
        }
    }
    for (const auto& weights : edges) {
        FastRng::WeightedTable table(weights);
        for (auto raw : raw_edges) check_weighted(t, weights, table, seed_for_raw(raw));
        for (auto i = 0; i < 256; ++i) check_weighted(t, weights, table, data());
    }

    // 再構築・コピー・移動・元配列の破棄が抽選結果に影響しないことを検査する
    FastRng::WeightedTable reused;
    for (auto trial = 0; trial < 10000; ++trial) {
        auto n = 1 + (size_t)(data() % 257);
        vector<double> weights(n);
        for (auto& w : weights) {
            auto value = data();
            w = value % 4 == 0 ? 0.0 : ldexp((double)(1 + value % 100), (int)(data() % 1201) - 600);
        }
        weights[(size_t)(data() % n)] = 1.0;
        reused.assign(weights);
        const FastRng::WeightedTable fresh(weights);
        auto copied = fresh;
        auto moved = move(copied);
        auto saved = weights;
        const FastRng::WeightedTable detached(weights);
        fill(weights.begin(), weights.end(), 0.0);
        weights.clear();
        weights.shrink_to_fit();
        for (auto j = 0; j < 8; ++j) {
            auto seed_value = data();
            check_weighted(t, saved, reused, seed_value);
            FastRng r1(seed_value), r2(seed_value), r3(seed_value), r4(seed_value);
            auto expected = r1.weighted_index(fresh);
            t.require(expected == r2.weighted_index(reused) && expected == r3.weighted_index(moved) &&
                      expected == r4.weighted_index(detached), "weighted assign/copy/move/ownership");
        }
    }

    // 累積値ちょうどに当たる場合は右側を選び、端の重み 0 を選ばない
    const vector<double> boundaries{0, 1, 0, 1, 2, 0};
    t.require(FastRng(seed_for_raw(0)).weighted_index(boundaries) == 1, "weighted lower endpoint");
    t.require(FastRng(seed_for_raw(1ULL << 62)).weighted_index(boundaries) == 3, "weighted exact cdf boundary");
    t.require(FastRng(seed_for_raw(UINT64_MAX)).weighted_index(boundaries) == 4, "weighted upper endpoint");
    test_weighted_frequencies(t);
    cout << "Weighted tests passed. checks = " << t.checks - before << '\n';
}

// 合計指定は double の vector / array を使う

// 比較用の指数乱数正規化、3 要素にも一般の場合と同じ処理を行う
void sum_exponential(FastRng& rng, vector<double>& a, double s) {
    assert(s >= 0 && isfinite(s) && (!a.empty() || s == 0));
    if (a.size() <= 1 || s == 0) { fill(a.begin(), a.end(), s); return; }
    if (a.size() == 2) { a[0] = rng.uniform(s); a[1] = s - a[0]; return; }
    auto sum = 0.0;
    for (auto& x : a) {
        x = -log(((double)(rng() >> 12) + 0.5) * 0x1p-52);
        sum += x;
    }
    for (auto& x : a) x = (x / sum) * s;
}

// 別方式の比較用、区切り位置をソートして隣接差分を取り、追加配列は使わない
void sum_sorted_cuts(FastRng& rng, vector<double>& a, double s) {
    if (a.size() <= 1 || s == 0) { fill(a.begin(), a.end(), s); return; }
    for (auto it = a.begin(); it != a.end() - 1; ++it) *it = rng.uniform();
    a.back() = 1;
    sort(a.begin(), a.end() - 1);
    for (auto i = a.size() - 1; i > 0; --i) a[i] = (a[i] - a[i - 1]) * s;
    a[0] *= s;
}

// 長倍精度で合計を求め、有限・非負・上端・通常丸めと非正規化数の誤差を検査する
void check_sum_values(Tester& t, const vector<double>& a, double s) {
    auto total = 0.0L;
    for (auto x : a) {
        t.require(isfinite(x) && x >= 0 && x <= s, "uniform_sum element range");
        total += (long double)x;
    }
    auto tolerance = 16 * (long double)a.size() * numeric_limits<double>::epsilon() * (long double)s
                   + (long double)a.size() * numeric_limits<double>::denorm_min();
    t.require(abs(total - (long double)s) <= tolerance, "uniform_sum rounded total");
}

// 生乱数から長倍精度で参照値を独立生成し、格納領域・系列・消費回数を照合する
void check_sum_case(Tester& t, size_t n, double s, uint64_t seed_value) {
    vector<double> a(n, -123.0), copied(n);
    auto ptr = a.data(); auto capacity = a.capacity();
    FastRng rng(seed_value), twin(seed_value);
    rng.uniform_sum(a, s);
    twin.uniform_sum(copied, s);
    check_sum_values(t, a, s);
    t.require(equal(a.begin(), a.end(), copied.begin()), "uniform_sum reproducibility");
    t.require(a.data() == ptr && a.size() == n && a.capacity() == capacity, "uniform_sum keeps storage");

    // 空・ゼロ・1 要素は消費なし、2 要素は 1 個、3 要素は 2 個、それ以外は n 個
    auto expected_state = seed_value;
    auto draws = s == 0 || n <= 1 ? size_t{0} : n <= 3 ? n - 1 : n;
    for (auto i = size_t{0}; i < draws; ++i) (void)reference_raw(expected_state);
    t.require(rng.state == expected_state && twin.state == expected_state, "uniform_sum raw consumption");
    t.require(rng() == reference_raw(expected_state), "uniform_sum next raw");
    if (n <= 1 || s == 0) {
        for (auto x : a) t.require(x == s, "uniform_sum degenerate");
        return;
    }

    // 2 要素の直接分割・3 要素の折り畳み・指数乱数の正規化を独立に確認する
    auto ref_state = seed_value;
    if (n == 2) {
        auto u = (double)(reference_raw(ref_state) >> 11) * 0x1p-53;
        auto left = s * u;
        if (!(left < s)) left = nextafter(s, 0.0);
        t.require(a[0] == left && a[1] == s - left, "uniform_sum pair reference");
    } else if (n == 3) {
        // 53bit 整数格子で三角形への写像を計算し、実装の min/abs に依存しない参照を作る
        auto u = reference_raw(ref_state) >> 11, v = reference_raw(ref_state) >> 11;
        constexpr auto unit = uint64_t{1} << 53;
        array<uint64_t, 3> numerators = u + v <= unit ? array{u, v, unit - u - v}
                                                     : array{unit - v, unit - u, u + v - unit};
        for (auto i = size_t{0}; i < 3; ++i) {
            auto expected = (long double)numerators[i] * 0x1p-53L * (long double)s;
            auto tolerance = 2 * numeric_limits<double>::epsilon() * abs(expected)
                           + numeric_limits<double>::denorm_min();
            t.require(abs((long double)a[i] - expected) <= tolerance, "uniform_sum triangle reference");
        }
    } else {
        vector<long double> e(n);
        auto total = 0.0L;
        for (auto& x : e) {
            auto u = ((long double)(reference_raw(ref_state) >> 12) + 0.5L) * 0x1p-52L;
            t.require(u > 0 && u < 1, "uniform_sum open logarithm input");
            x = -log(u);
            total += x;
        }
        for (auto i = size_t{0}; i < n; ++i) {
            auto expected = e[i] / total * (long double)s;
            auto tolerance = 16 * (long double)n * numeric_limits<double>::epsilon() * abs(expected)
                           + numeric_limits<double>::denorm_min();
            t.require(abs((long double)a[i] - expected) <= tolerance, "uniform_sum long double reference");
        }
    }
}

// 一様単体の各要素は Beta(1,n-1)、平均・二乗平均・相関・周辺 CDF を確認する
// 合計だけが一致する誤実装（独立一様乱数を正規化）も検出する
template<class F>
void check_sum_distribution(Tester& t, F generate) {
    constexpr auto trials = 100000;
    for (auto seed_value : {uint64_t{12591}, uint64_t{95813}}) {
        for (auto n : {2, 3, 4, 8, 16}) {
            FastRng rng(seed_value);
            vector<double> a((size_t)n);
            vector<long double> first((size_t)n), second((size_t)n);
            array<int, 3> below{};
            auto cross = 0.0L;
            auto nd = (long double)n;
            array<long double, 3> cuts{0.25L / nd, 1.0L / nd, 2.0L / nd};
            for (auto k = 0; k < trials; ++k) {
                generate(rng, a, 1.0);
                for (auto i = size_t{0}; i < a.size(); ++i) {
                    auto x = (long double)a[i];
                    first[i] += x;
                    second[i] += x * x;
                }
                cross += (long double)a[0] * a[1];
                for (auto j = size_t{0}; j < cuts.size(); ++j) below[j] += (long double)a[0] <= cuts[j];
            }
            auto mean = 1 / nd, moment2 = 2 / (nd * (nd + 1));
            auto moment4 = 24 / (nd * (nd + 1) * (nd + 2) * (nd + 3));
            auto mean_tol = 8 * sqrt((moment2 - mean * mean) / trials);
            auto second_tol = 8 * sqrt((moment4 - moment2 * moment2) / trials);
            for (auto i = size_t{0}; i < a.size(); ++i) {
                t.require(abs(first[i] / trials - mean) < mean_tol, "uniform_sum marginal mean");
                t.require(abs(second[i] / trials - moment2) < second_tol, "uniform_sum second moment");
            }
            auto expected_cross = 1 / (nd * (nd + 1));
            auto cross2 = 4 / (nd * (nd + 1) * (nd + 2) * (nd + 3));
            auto cross_tol = 8 * sqrt((cross2 - expected_cross * expected_cross) / trials);
            t.require(abs(cross / trials - expected_cross) < cross_tol, "uniform_sum cross moment");
            for (auto j = size_t{0}; j < cuts.size(); ++j) {
                auto probability = 1 - pow(1 - cuts[j], n - 1);
                auto tolerance = 8 * sqrt(probability * (1 - probability) / trials) + 2.0L / trials;
                t.require(abs((long double)below[j] / trials - probability) <= tolerance, "uniform_sum marginal CDF");
            }
        }
    }
}

void test_uniform_sum(Tester& t, mt19937_64& data) {
    auto before = t.checks;
    // 極端な s と乱数の端点、空・単一・2 要素と SIMD 境界付近の長さ
    array<double, 13> sums{0.0, -0.0, 1.0, 100.0, 0.1, 1e-300, 1e300,
        numeric_limits<double>::denorm_min(), 16 * numeric_limits<double>::denorm_min(),
        numeric_limits<double>::min(), numeric_limits<double>::max(), nextafter(1.0, 0.0), nextafter(1.0, 2.0)};
    for (auto n : {size_t{0}, size_t{1}, size_t{2}, size_t{3}, size_t{4}, size_t{7}, size_t{8},
                   size_t{16}, size_t{31}, size_t{127}, size_t{1024}}) {
        for (auto s : sums) {
            if (n == 0 && s != 0) continue;
            for (auto raw : raw_edges) check_sum_case(t, n, s, seed_for_raw(raw));
        }
    }
    // 大量のランダムなサイズ・正規化数・非正規化数・seed を組み合わせる
    for (auto k = 0; k < 12000; ++k) {
        auto n = (size_t)(data() % 257);
        auto raw = data() & 0x7fffffffffffffffULL;
        if ((raw >> 52) == 0x7ff) raw ^= 1ULL << 52;
        auto s = n == 0 ? 0.0 : test_bit_cast<double>(raw);
        check_sum_case(t, n, s, data());
    }
    for (auto n : {size_t{65536}, size_t{1000000}}) {
        vector<double> a(n);
        FastRng rng(data());
        rng.uniform_sum(a, 10.0);
        check_sum_values(t, a, 10.0);
    }

    // vector と array は同じ値・同じ状態、初期内容には依存しない
    for (auto k = 0; k < 64; ++k) {
        auto seed_value = data();
        vector<double> expected(7);
        array<double, 7> a{};
        FastRng ref(seed_value), rng(seed_value);
        ref.uniform_sum(expected); rng.uniform_sum(a);
        t.require(equal(a.begin(), a.end(), expected.begin()), "uniform_sum vector/array default sum");
        t.require(rng.state == ref.state, "uniform_sum container state");
    }
    array<double, 0> empty{};
    vector<double> empty_vector;
    FastRng rng(10);
    rng.uniform_sum(empty, 0); rng.uniform_sum(empty_vector, 0);
    t.require(rng.state == 10, "uniform_sum empty");
    // 3 要素に集中したランダム入力と、vector / array の系列一致
    for (auto k = 0; k < 20000; ++k) {
        auto raw = data() & 0x7fffffffffffffffULL;
        if ((raw >> 52) == 0x7ff) raw ^= 1ULL << 52;
        auto s = test_bit_cast<double>(raw);
        auto seed_value = data();
        check_sum_case(t, 3, s, seed_value);
        array<double, 3> a{};
        array<double, 3> c{};
        vector<double> v(3);
        FastRng r1(seed_value), r2(seed_value), r3(seed_value);
        r1.uniform_sum(a, s); r2.uniform_sum(c, s); r3.uniform_sum(v, s);
        t.require(equal(a.begin(), a.end(), c.begin()) && equal(a.begin(), a.end(), v.begin()),
                  "uniform_sum triangle container agreement");
        t.require(r1.state == r2.state && r1.state == r3.state, "uniform_sum triangle container state");
    }
    check_sum_distribution(t, [](auto& g, auto& a, double s) { g.uniform_sum(a, s); });
    check_sum_distribution(t, sum_sorted_cuts);
    cout << "Uniform-sum tests passed. checks = " << t.checks - before << '\n';
}

// 正規分布の配列版は double の vector / array を使う

// 乱数状態は独立した生乱数実装で進め、対数・平方根は長倍精度で検算する
pair<long double, long double> reference_normal_pair(uint64_t& state, uint64_t& attempts) {
    for (;;) {
        ++attempts;
        auto x = (double)(reference_raw(state) >> 11) * 0x1p-52 - 1;
        auto y = (double)(reference_raw(state) >> 11) * 0x1p-52 - 1;
        auto radius = x * x + y * y;
        if (radius == 0 || radius >= 1) continue;
        auto scale = sqrt(-2 * log((long double)radius) / radius);
        return {(long double)x * scale, (long double)y * scale};
    }
}

void check_normal_value(Tester& t, double actual, long double z, double mean, double stddev) {
    auto expected = (long double)mean + (long double)stddev * z;
    // 平均付近の相殺・非正規化数を含めて、演算値に比例する丸め誤差だけを許す
    auto tolerance = 256 * (long double)numeric_limits<double>::epsilon() *
                     (abs((long double)mean) + (long double)stddev * (1 + abs(z))) +
                     8 * (long double)numeric_limits<double>::denorm_min();
    t.require(isfinite(actual) && abs((long double)actual - expected) <= tolerance, "normal long double reference");
}

// 格納領域の維持、コピー・seed 再設定、奇偶サイズ、乱数消費を同時に調べる
void check_normal_case(Tester& t, size_t n, double mean, double stddev, uint64_t seed_value) {
    vector<double> a(n, -12345.0), copied(n), reseeded(n);
    auto ptr = a.data(); auto capacity = a.capacity();
    FastRng rng(seed_value), twin = rng, reset(111);
    reset.seed(seed_value);
    rng.normal(a, mean, stddev);
    twin.normal(copied, mean, stddev);
    reset.normal(reseeded, mean, stddev);
    t.require(equal(a.begin(), a.end(), copied.begin()) && copied == reseeded, "normal fill copy/reseed reproducibility");
    t.require(a.data() == ptr && a.size() == n && a.capacity() == capacity, "normal keeps storage");
    t.require(rng.state == twin.state && rng.state == reset.state, "normal copy/reseed state");
    auto state = seed_value, attempts = uint64_t{0};
    if (stddev == 0) {
        for (auto x : a) t.require(test_bit_cast<uint64_t>(x) == test_bit_cast<uint64_t>(mean), "normal zero stddev preserves mean bits");
    } else {
        for (auto i = size_t{0}; i < n; i += 2) {
            auto [x, y] = reference_normal_pair(state, attempts);
            check_normal_value(t, a[i], x, mean, stddev);
            if (i + 1 < n) check_normal_value(t, a[i + 1], y, mean, stddev);
        }
    }
    t.require(rng.state == state, "normal fill reference raw consumption");
    t.require(rng() == reference_raw(state), "normal next raw after fill");
}

// 平均・分散・4 次モーメント・CDF・前後相関を調べ、偶奇位置も別に検査する
void test_normal_distribution(Tester& t, size_t block, double mean, double stddev) {
    constexpr auto count = size_t{1000000};
    FastRng rng(0x5699c73308e74d31ULL + block);
    vector<double> buffer(max(size_t{1}, block));
    array<long double, 2> sum{}, square{};
    auto fourth = 0.0L, lag1 = 0.0L, lag2 = 0.0L, prev = 0.0L, prev2 = 0.0L;
    array<uint64_t, 7> below{};
    auto samples = size_t{0};
    while (samples < count) {
        auto n = min(buffer.size(), count - samples);
        if (block == 0) buffer[0] = rng.normal(mean, stddev);
        else { buffer.resize(n); rng.normal(buffer, mean, stddev); }
        for (auto i = size_t{0}; i < n; ++i, ++samples) {
            auto z = ((long double)buffer[i] - mean) / stddev;
            t.require(isfinite(z), "normal finite distribution sample");
            sum[samples & 1] += z;
            square[samples & 1] += z * z;
            fourth += z * z * z * z;
            if (samples >= 1) lag1 += prev * z;
            if (samples >= 2) lag2 += prev2 * z;
            prev2 = prev; prev = z;
            for (auto k = 0; k < 7; ++k) below[(size_t)k] += z < k - 3;
        }
    }
    // 8 標準誤差を許し、決め打ちの seed 群で再現可能な統計的スモークテストとする
    auto total_mean = (sum[0] + sum[1]) / count;
    auto second = (square[0] + square[1]) / count;
    for (auto parity = size_t{0}; parity < 2; ++parity) {
        t.require(abs(sum[parity] / (count / 2)) < 8 / sqrt((long double)count / 2), "normal parity mean");
        t.require(abs(square[parity] / (count / 2) - 1) < 8 * sqrt(2.0L / (count / 2)), "normal parity second moment");
    }
    t.require(abs(fourth / count - 3) < 8 * sqrt(96.0L / count), "normal fourth moment");
    t.require(abs(lag1 / (count - 1)) < 8 / sqrt((long double)count), "normal lag-1 correlation");
    t.require(abs(lag2 / (count - 2)) < 8 / sqrt((long double)count), "normal lag-2 correlation");
    for (auto k = 0; k < 7; ++k) {
        auto probability = 0.5L * erfc(-(long double)(k - 3) / sqrt(2.0L));
        auto tolerance = 8 * sqrt(probability * (1 - probability) / count) + 2.0L / count;
        t.require(abs((long double)below[(size_t)k] / count - probability) < tolerance, "normal CDF including tails");
    }
    cout << "Normal stats: block=" << block << " count=" << count << " mean=" << (double)total_mean
         << " variance=" << (double)(second - total_mean * total_mean) << " fourth=" << (double)(fourth / count)
         << " lag1=" << (double)(lag1 / (count - 1)) << '\n';
}

// 同じ式でも array/vector や固定長の違いで FMA の適用が変わりうるため、型をまたぐ比較だけ丸めを許容する
template<class A, class B>
bool equal_normal_arrays(const A& a, const B& b) {
    if (a.size() != b.size()) return false;
    for (auto i = size_t{0}; i < a.size(); ++i) {
        auto tolerance = 4 * numeric_limits<double>::epsilon() * max({1.0, abs(a[i]), abs(b[i])});
        if (abs(a[i] - b[i]) > tolerance) return false;
    }
    return true;
}

void test_normal(Tester& t, mt19937_64& data) {
    auto before = t.checks;
    // 値域の端、ゼロ標準偏差、非正規化数、小さい区間から極大の有限値まで確認する
    const array parameters{
        pair{0.0, 1.0}, pair{10.0, 2.0}, pair{-3.0, 0.125}, pair{0.0, 0.0}, pair{-0.0, -0.0},
        pair{numeric_limits<double>::max(), 0.0}, pair{-numeric_limits<double>::max(), 0.0},
        pair{0.0, numeric_limits<double>::denorm_min()}, pair{0.0, numeric_limits<double>::min()},
        pair{1e-200, 1e-200}, pair{1e200, 1e197},
        pair{numeric_limits<double>::max() / 2, numeric_limits<double>::max() / 64},
        pair{-numeric_limits<double>::max() / 2, numeric_limits<double>::max() / 64}
    };
    for (auto n : {size_t{0}, size_t{1}, size_t{2}, size_t{3}, size_t{4}, size_t{7}, size_t{31}, size_t{32},
                   size_t{127}, size_t{128}, size_t{1023}, size_t{1024}}) {
        for (auto [mean, stddev] : parameters) {
            for (auto seed_value : {uint64_t{0}, uint64_t{1}, UINT64_MAX, seed_for_raw(0), seed_for_raw(1ULL << 63)}) {
                check_normal_case(t, n, mean, stddev, seed_value);
            }
        }
    }
    // ランダムな多数の条件で配列とスカラーを検算する、値の生成には独立した RNG を使う
    for (auto trial = 0; trial < 10000; ++trial) {
        auto exponent = (int)(data() % 1801) - 900;
        auto mean = ldexp(((double)(data() % 2001) - 1000) / 1000, exponent);
        auto stddev = trial % 31 == 0 ? 0.0 : ldexp(0.5 + (double)(data() % 1001) / 1000, exponent);
        auto seed_value = data();
        check_normal_case(t, (size_t)(data() % 64), mean, stddev, seed_value);
        FastRng first(seed_value), twin = first;
        auto state = seed_value, attempts = uint64_t{0};
        for (auto i = 0; i < 8; ++i) {
            auto x = first.normal(mean, stddev);
            t.require(test_bit_cast<uint64_t>(x) == test_bit_cast<uint64_t>(twin.normal(mean, stddev)), "normal scalar copy reproducibility");
            if (stddev == 0) t.require(test_bit_cast<uint64_t>(x) == test_bit_cast<uint64_t>(mean), "normal scalar zero stddev");
            else check_normal_value(t, x, reference_normal_pair(state, attempts).first, mean, stddev);
            t.require(first.state == state, "normal scalar reference consumption");
        }
    }
    // 生乱数の極値を最初に発生させ、棄却を経ても正しい値と状態に到達することを確認する
    auto rejected = false;
    for (auto raw : raw_edges) {
        auto seed_value = seed_for_raw(raw);
        FastRng rng(seed_value);
        auto state = seed_value, attempts = uint64_t{0};
        auto expected = reference_normal_pair(state, attempts).first;
        check_normal_value(t, rng.normal(), expected, 0, 1);
        t.require(rng.state == state, "normal raw endpoint reference state");
        rejected = rejected || attempts > 1;
    }
    t.require(rejected, "normal rejection exercised by raw endpoint");

    // vector / array の既定引数、空・単一要素と隠れた状態がないことを確認する
    for (auto seed_value = uint64_t{0}; seed_value < 100; ++seed_value) {
        array<double, 7> a{};
        vector<double> b(7);
        FastRng r1(seed_value), r2(seed_value), r3(seed_value), r4(seed_value);
        r1.normal(a); r2.normal(b);
        t.require(equal_normal_arrays(a, b), "normal container defaults");
        t.require(r1.state == r2.state, "normal container state");
        r1.normal(a, 10); r2.normal(b, 10.0, 1.0);
        t.require(equal_normal_arrays(a, b), "normal mean-only defaults");
        r1.normal(a, -2, 0); r2.normal(b, -2, 0);
        for (auto x : a) t.require(x == -2, "normal array zero deviation");
        t.require(r1.state == r2.state, "normal zero fill state");
        t.require(r1.normal(7) == r2.normal(7.0, 1.0), "normal scalar mean-only default");
        array<double, 1> one{};
        auto x = r3.normal(); r4.normal(one);
        t.require(x == one[0] && r3.state == r4.state, "normal single element equals scalar");
        array<double, 0> empty{}; vector<double> empty_vector;
        r3.normal(empty); r3.normal(empty_vector);
        t.require(r3.state == r4.state, "normal empty consumes no words");
    }
    // 偶数個ずつなら列は一致し、奇数個の呼び出しでは余りを次回に保存しない
    for (auto seed_value = uint64_t{0}; seed_value < 64; ++seed_value) {
        array<double, 32> a{}, c{};
        array<double, 16> first{}, second{};
        array<double, 13> odd_first{}; array<double, 19> odd_second{};
        FastRng r1(seed_value), r2(seed_value), r3(seed_value);
        r1.normal(a); r2.normal(first); r2.normal(second);
        t.require(equal(first.begin(), first.end(), a.begin()) && equal(second.begin(), second.end(), a.begin() + 16)
                  && r1.state == r2.state, "normal even split equivalence");
        r3.normal(odd_first); r3.normal(odd_second);
        t.require(r3.state != r1.state, "normal odd split discards spare");
        r3.seed(seed_value); r3.normal(c);
        t.require(a == c && r3.state == r1.state, "normal seed has no hidden cache");
    }
    test_normal_distribution(t, 0, 0.0, 1.0);
    test_normal_distribution(t, 128, 10.0, 2.0);
    test_normal_distribution(t, 257, -3.0, 0.25);
    cout << "Normal tests passed. checks = " << t.checks - before << '\n';
}

// 固定長 array の前後に番兵を置き、vector との全要素・状態の一致を比較する
template<class T, size_t N>
void check_native_array(Tester& t, uint64_t seed) {
    struct Guarded {
        array<uint64_t, 2> before{UINT64_MAX, UINT64_MAX};
        array<T, N> values{};
        array<uint64_t, 2> after{UINT64_MAX, UINT64_MAX};
    } a;
    vector<T> v(N);
    FastRng first(seed), second(seed);
    auto verify = [&] {
        t.require(first.state == second.state, "native array state");
        for (auto i = size_t{0}; i < N; ++i) {
            if constexpr (is_same_v<T, double>) {
                auto tolerance = 4 * numeric_limits<double>::epsilon() * max({1.0, abs(a.values[i]), abs(v[i])});
                t.require(abs(a.values[i] - v[i]) <= tolerance, "native array double values");
            } else t.require(a.values[i] == v[i], "native array integer values");
        }
        t.require(a.before[0] == UINT64_MAX && a.before[1] == UINT64_MAX &&
                  a.after[0] == UINT64_MAX && a.after[1] == UINT64_MAX, "native array guard values");
    };
    first.uniform(a.values, 7); second.uniform(v, 7); verify();
    first.uniform(a.values, 1, 7); second.uniform(v, 1, 7); verify();
    if constexpr (is_same_v<T, double>) {
        first.uniform(a.values); second.uniform(v); verify();
        first.normal(a.values); second.normal(v); verify();
        first.normal(a.values, 10, 2); second.normal(v, 10, 2); verify();
        first.normal(a.values, -0.0, 0.0); second.normal(v, -0.0, 0.0); verify();
        first.uniform_sum(a.values, 0); second.uniform_sum(v, 0); verify();
        if constexpr (N > 0) { first.uniform_sum(a.values, 1); second.uniform_sum(v, 1); verify(); }
    } else {
        first.uniform_closed(a.values, 7); second.uniform_closed(v, 7); verify();
        first.uniform_closed(a.values, 1, 7); second.uniform_closed(v, 1, 7); verify();
        first.uniform_closed(a.values, numeric_limits<T>::min(), numeric_limits<T>::max());
        second.uniform_closed(v, numeric_limits<T>::min(), numeric_limits<T>::max()); verify();
        first.perm(a.values); second.perm(v); verify();
        first.perm(a.values, 1); second.perm(v, 1); verify();
    }
}

void test_native_arrays(Tester& t) {
    test_each_type(tuple<int8_t, int, int64_t, uint64_t, double>{}, [&](auto type) {
        using T = typename decltype(type)::type;
        for (auto seed = uint64_t{0}; seed < 32; ++seed) {
            check_native_array<T, 0>(t, seed);
            check_native_array<T, 1>(t, seed);
            check_native_array<T, 2>(t, seed);
            check_native_array<T, 3>(t, seed);
            check_native_array<T, 17>(t, seed);
        }
    });
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
    test_each_type(tuple<signed char, unsigned char, short, unsigned short, int, unsigned int,
               long, unsigned long, long long, unsigned long long,
               char, wchar_t, char16_t, char32_t>{}, [&](auto type) {
        using T = typename decltype(type)::type; test_integer<T>(t, data); });
    test_all_8bit_intervals<int8_t>(t);
    test_all_8bit_intervals<uint8_t>(t);
    test_all_8bit_deltas<int8_t>(t);
    test_all_8bit_deltas<uint8_t>(t);
    test_delta_sequences(t);
    test_double(t, data);
    test_array_apis(t, data);
    test_native_arrays(t);
    test_weighted(t, data);
    test_uniform_sum(t, data);
    test_normal(t, data);

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

    static inline uint64_t test_rotl(uint64_t x, int k) noexcept {
        return (x << k) | (x >> (64 - k));
    }

    inline result_type operator()() noexcept {
        auto result = test_rotl(s[0] + s[3], 23) + s[0];
        auto t = s[1] << 17;
        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];
        s[2] ^= t;
        s[3] = test_rotl(s[3], 45);
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
    if constexpr (is_same_v<invoke_result_t<F, R&, uint64_t>, double>) {
        auto a = 0.0, b = 0.0, c = 0.0, d = 0.0;
        auto i = uint64_t{0};
        for (; i + 4 <= count; i += 4) {
            a += fn(rng, i); b += fn(rng, i + 1);
            c += fn(rng, i + 2); d += fn(rng, i + 3);
        }
        for (; i < count; ++i) a += fn(rng, i);
        return test_bit_cast<uint64_t>((a + b) + (c + d));
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

// 配列は計測前に確保し、全ての書き込みを最適化で消させない
// バリアは配列 1 本につき 1 回だけ、チェックサムの計算は計測区間外
// 時間は配列を繰り返し生成したバッチの平均で、単発の最大遅延ではない
template<class T, class F>
[[gnu::noinline]] Sample measure_array(vector<T>& a, uint64_t arrays, uint64_t seed_value, F fn) {
    FastRng rng(seed_value);
    auto start = chrono::steady_clock::now();
    for (auto run = uint64_t{0}; run < arrays; ++run) {
        fn(rng, a, run);
        asm volatile("" : : "g"(a.data()) : "memory");
    }
    asm volatile("" : "+r"(rng.state) : : "memory");
    auto end = chrono::steady_clock::now();
    auto checksum = rng.state;
    for (auto x : a) {
        if constexpr (is_same_v<T, double>) checksum = test_rotl(checksum, 1) ^ test_bit_cast<uint64_t>(x);
        else checksum = test_rotl(checksum, 1) ^ (uint64_t)x;
    }
    bench_sink = bench_sink ^ checksum;
    return {chrono::duration<double, milli>(end - start).count(), checksum};
}

// 2 方式を交互の順番で測る、fill は系列と消費回数の一致も各反復で確認する
template<class T, class F, class G>
void array_benchmark_pair(const char* first_name, const char* second_name, size_t size,
                          uint64_t count, int repeat, bool same_sequence, F first, G second) {
    vector<T> a(size);
    auto arrays = max(uint64_t{1}, count / size);
    measure_array(a, min(arrays, uint64_t{100}), 123, first);
    measure_array(a, min(arrays, uint64_t{100}), 123, second);
    array<double, 2> sum{}, worst{};
    array<uint64_t, 2> checksum{};
    for (auto rep = 0; rep < repeat; ++rep) {
        array<Sample, 2> samples{};
        if ((rep & 1) == 0) {
            samples[0] = measure_array(a, arrays, (uint64_t)(rep + 1000), first);
            samples[1] = measure_array(a, arrays, (uint64_t)(rep + 1000), second);
        } else {
            samples[1] = measure_array(a, arrays, (uint64_t)(rep + 1000), second);
            samples[0] = measure_array(a, arrays, (uint64_t)(rep + 1000), first);
        }
        if (same_sequence && samples[0].checksum != samples[1].checksum) {
            cerr << "[BENCH FAILED] array/scalar checksum mismatch\n";
            exit(1);
        }
        for (auto i = size_t{0}; i < 2; ++i) {
            sum[i] += samples[i].ms;
            worst[i] = max(worst[i], samples[i].ms);
            checksum[i] ^= samples[i].checksum;
        }
    }
    for (auto i = size_t{0}; i < 2; ++i) {
        auto average = sum[i] / repeat;
        auto elements = (double)size * (double)arrays;
        cout << left << setw(29) << (i == 0 ? first_name : second_name) << right
             << setw(9) << size << setw(13) << arrays
             << fixed << setprecision(3) << setw(13) << average << setw(15) << worst[i]
             << setw(15) << average * 1e6 / (double)arrays
             << setw(14) << average * 1e6 / elements << setw(22) << worst[i] * 1e6 / elements
             << setw(23) << checksum[i] << '\n';
    }
}

void run_array_benchmark(uint64_t count, int repeat) {
    cout << "\n=== FastRng array benchmark ===\n";
    cout << left << setw(29) << "operation" << right << setw(9) << "size" << setw(13) << "arrays/run"
         << setw(13) << "avg_run_ms" << setw(15) << "worst_run_ms" << setw(15) << "avg_ns/array"
         << setw(14) << "avg_ns/elem" << setw(22) << "worst_batch_ns/elem" << setw(23) << "checksum_xor" << '\n';
    for (auto size : {size_t{32}, size_t{1024}, size_t{65536}}) {
        array_benchmark_pair<int>("fill uniform<int>", "loop uniform<int>", size, count, repeat, true,
            [](FastRng& rng, auto& a, uint64_t) { rng.uniform(a, -1000, 1001); },
            [](FastRng& rng, auto& a, uint64_t) { for (auto& x : a) x = rng.uniform(-1000, 1001); });
        array_benchmark_pair<int>("fill closed<int>", "loop closed<int>", size, count, repeat, true,
            [](FastRng& rng, auto& a, uint64_t) { rng.uniform_closed(a, -1000, 1000); },
            [](FastRng& rng, auto& a, uint64_t) { for (auto& x : a) x = rng.uniform_closed(-1000, 1000); });
        array_benchmark_pair<int64_t>("fill closed<i64>", "loop closed<i64>", size, count, repeat, true,
            [](FastRng& rng, auto& a, uint64_t) { rng.uniform_closed(a, -1000000000000LL, 1000000000000LL); },
            [](FastRng& rng, auto& a, uint64_t) { for (auto& x : a) x = rng.uniform_closed<int64_t>(-1000000000000LL, 1000000000000LL); });
        array_benchmark_pair<uint64_t>("fill uniform<u64>", "loop uniform<u64>", size, count, repeat, true,
            [](FastRng& rng, auto& a, uint64_t) { rng.uniform(a, (1ULL << 63) + 12345); },
            [](FastRng& rng, auto& a, uint64_t) { for (auto& x : a) x = rng.uniform<uint64_t>((1ULL << 63) + 12345); });
        array_benchmark_pair<double>("fill uniform()", "loop uniform()", size, count, repeat, true,
            [](FastRng& rng, auto& a, uint64_t) { rng.uniform(a); },
            [](FastRng& rng, auto& a, uint64_t) { for (auto& x : a) x = rng.uniform(); });
        array_benchmark_pair<double>("fill uniform<double>", "loop uniform<double>", size, count, repeat, true,
            [](FastRng& rng, auto& a, uint64_t) { rng.uniform(a, -3.5, 7.25); },
            [](FastRng& rng, auto& a, uint64_t) { for (auto& x : a) x = rng.uniform(-3.5, 7.25); });
        array_benchmark_pair<int>("fill runtime bounds", "loop runtime bounds", size, count, repeat, true,
            [](FastRng& rng, auto& a, uint64_t run) { auto r = 1 + (int)(run % 2001); rng.uniform(a, -r, r); },
            [](FastRng& rng, auto& a, uint64_t run) { auto r = 1 + (int)(run % 2001); for (auto& x : a) x = rng.uniform(-r, r); });

        // 1 パス方式と、連番の初期化後に後ろから交換する 2 パス方式を比較する
        array_benchmark_pair<int>("perm (1-pass)", "perm (2-pass)", size, count, repeat, false,
            [](FastRng& rng, auto& a, uint64_t) { rng.perm(a); },
            [](FastRng& rng, auto& a, uint64_t) {
                for (auto i = size_t{0}; i < a.size(); ++i) a[i] = (int)i;
                for (auto n = a.size(); n > 1; --n) swap(a[n - 1], a[rng.uniform(n)]);
            });
        // 標準 shuffle は範囲縮小・乱数消費も異なるため、系列一致は要求しない
        array_benchmark_pair<int>("perm (1-pass)", "iota + std::shuffle", size, count, repeat, false,
            [](FastRng& rng, auto& a, uint64_t) { rng.perm(a); },
            [](FastRng& rng, auto& a, uint64_t) { iota(a.begin(), a.end(), 0); shuffle(a.begin(), a.end(), rng); });
    }
}

// 3 要素の折り畳み・一般の指数方式・ソート方式を交互に測る、確保・結果集計は計測区間外
void run_sum_benchmark(uint64_t count, int repeat) {
    cout << "\n=== FastRng uniform_sum benchmark ===\n";
    cout << left << setw(29) << "operation" << right << setw(9) << "size" << setw(13) << "arrays/run"
         << setw(13) << "avg_run_ms" << setw(15) << "worst_run_ms" << setw(15) << "avg_ns/array"
         << setw(14) << "avg_ns/elem" << setw(22) << "worst_batch_ns/elem" << setw(23) << "checksum_xor" << '\n';
    for (auto n : {size_t{2}, size_t{3}, size_t{4}, size_t{8}, size_t{16}, size_t{32},
                   size_t{128}, size_t{1024}, size_t{65536}}) {
        array_benchmark_pair<double>("uniform_sum", "exponential", n, count, repeat, n != 3,
            [](FastRng& rng, auto& a, uint64_t) { rng.uniform_sum(a, 100.0); },
            [](FastRng& rng, auto& a, uint64_t) { sum_exponential(rng, a, 100.0); });
        array_benchmark_pair<double>("uniform_sum", "sorted cuts", n, count, repeat, false,
            [](FastRng& rng, auto& a, uint64_t) { rng.uniform_sum(a, 100.0); },
            [](FastRng& rng, auto& a, uint64_t) { sum_sorted_cuts(rng, a, 100.0); });
    }
}

// 比較用の準備済み累積和、重み 0 を飛ばすため upper_bound を使う
struct WeightedCdf {
    vector<double> prefix;
    explicit WeightedCdf(const vector<double>& weights) : prefix(weights.size()) {
        partial_sum(weights.begin(), weights.end(), prefix.begin());
    }
    int32_t sample(FastRng& rng) const {
        auto x = rng.uniform(prefix.back());
        return (int32_t)(upper_bound(prefix.begin(), prefix.end(), x) - prefix.begin());
    }
};

// 各方式は測定の外で切り替え、同じ seed と操作回数で実行順を巡回させる
// worst_batch_ns/op は最遅バッチの平均であり、単一操作の最大時間ではない
template<class... F>
void weighted_compare_rows(const array<const char*, sizeof...(F)>& labels, uint64_t count, int repeat, F... fn) {
    constexpr auto methods = sizeof...(F);
    auto functions = tuple{fn...};
    array<double, methods> sum{}, worst{};
    array<uint64_t, methods> checksum{};
    auto run = [&](size_t choice, uint64_t seed_value, uint64_t ops) {
        Sample result{};
        auto index = size_t{0};
        apply([&](auto&... f) {
            ((choice == index++ ? (void)(result = measure<FastRng>(seed_value, ops, f)) : (void)0), ...);
        }, functions);
        return result;
    };
    for (auto m = size_t{0}; m < methods; ++m) run(m, 123, min(count, uint64_t{1000}));
    for (auto rep = 0; rep < repeat; ++rep) {
        for (auto step = size_t{0}; step < methods; ++step) {
            auto m = (step + (size_t)rep) % methods;
            auto result = run(m, 3100 + (uint64_t)rep, count);
            sum[m] += result.ms;
            worst[m] = max(worst[m], result.ms);
            checksum[m] ^= result.checksum;
        }
    }
    for (auto m = size_t{0}; m < methods; ++m) {
        auto average = sum[m] / repeat;
        cout << left << setw(30) << labels[m] << right << setw(12) << count
             << fixed << setprecision(3) << setw(14) << average << setw(16) << worst[m]
             << setw(14) << average * 1e6 / (double)count << setw(20) << worst[m] * 1e6 / (double)count
             << setw(23) << checksum[m] << '\n';
    }
}

// 正規乱数は同じ消費処理で比較する、std retained は分布を呼び出し間で保持する参考値
// 配列側は標準分布を配列 1 本につき 1 個だけ構築し、キャッシュを有効に使う
void run_normal_benchmark(uint64_t count, int repeat) {
    benchmark_heading("Normal scalar N(0,1): ns/op includes accumulation");
    weighted_compare_rows(array{"normal()", "std per call", "std retained"}, count, repeat,
        [](FastRng& rng, uint64_t) { return rng.normal(); },
        [](FastRng& rng, uint64_t) { return normal_distribution<double>{}(rng); },
        [dist = normal_distribution<double>{}](FastRng& rng, uint64_t) mutable { return dist(rng); });
    benchmark_heading("Normal scalar N(10,4): mean=10 stddev=2");
    weighted_compare_rows(array{"normal(10,2)", "std per call", "std retained"}, count, repeat,
        [](FastRng& rng, uint64_t) { return rng.normal(10.0, 2.0); },
        [](FastRng& rng, uint64_t) { return normal_distribution<double>{10.0, 2.0}(rng); },
        [dist = normal_distribution<double>{10.0, 2.0}](FastRng& rng, uint64_t) mutable { return dist(rng); });

    // 毎回 seed を指定する処理では、以前の分布のキャッシュを使う方式とは比較しない
    benchmark_heading("Normal reseed + scalar");
    weighted_compare_rows(array{"seed + normal", "seed + std fresh"}, count, repeat,
        [](FastRng& rng, uint64_t i) { rng.seed(123456789 + i); return rng.normal(10.0, 2.0); },
        [](FastRng& rng, uint64_t i) { rng.seed(123456789 + i); return normal_distribution<double>{10.0, 2.0}(rng); });

    array<pair<double, double>, 256> parameters{};
    mt19937_64 data(12345);
    for (auto& [mean, stddev] : parameters) {
        mean = ((double)(data() % 20000) - 10000) * 0.125;
        stddev = 0.01 + (double)(data() % 10000) * 0.03125;
    }
    benchmark_heading("Normal scalar with runtime parameters");
    weighted_compare_rows(array{"normal runtime", "std per call runtime", "std retained runtime"}, count, repeat,
        [&](FastRng& rng, uint64_t i) { auto [mean, sd] = parameters[i & 255]; return rng.normal(mean, sd); },
        [&](FastRng& rng, uint64_t i) { auto [mean, sd] = parameters[i & 255]; return normal_distribution<double>{mean, sd}(rng); },
        [&, dist = normal_distribution<double>{}](FastRng& rng, uint64_t i) mutable {
            auto [mean, sd] = parameters[i & 255];
            return dist(rng, normal_distribution<double>::param_type{mean, sd});
        });

    cout << "\n=== Normal array benchmark (mean=10 stddev=2) ===\n";
    cout << left << setw(29) << "operation" << right << setw(9) << "size" << setw(13) << "arrays/run"
         << setw(13) << "avg_run_ms" << setw(15) << "worst_run_ms" << setw(15) << "avg_ns/array"
         << setw(14) << "avg_ns/elem" << setw(22) << "worst_batch_ns/elem" << setw(23) << "checksum_xor" << '\n';
    for (auto n : {size_t{1}, size_t{2}, size_t{3}, size_t{32}, size_t{127}, size_t{1024}, size_t{65536}}) {
        array_benchmark_pair<double>("normal array", "std per array", n, count, repeat, false,
            [](FastRng& rng, auto& a, uint64_t) { rng.normal(a, 10.0, 2.0); },
            [](FastRng& rng, auto& a, uint64_t) {
                normal_distribution<double> dist(10.0, 2.0);
                for (auto& x : a) x = dist(rng);
            });
    }
    cout << "\n=== Normal array with runtime parameters ===\n";
    for (auto n : {size_t{3}, size_t{32}, size_t{1024}}) {
        array_benchmark_pair<double>("normal runtime array", "std runtime per array", n, count, repeat, false,
            [&](FastRng& rng, auto& a, uint64_t i) { auto [mean, sd] = parameters[i & 255]; rng.normal(a, mean, sd); },
            [&](FastRng& rng, auto& a, uint64_t i) {
                auto [mean, sd] = parameters[i & 255];
                normal_distribution<double> dist(mean, sd);
                for (auto& x : a) x = dist(rng);
            });
    }
}

// 同じサイズで内容の違う重みを用意し、固定の既知配列への最適化を避ける
vector<vector<double>> weighted_datasets(size_t n, int shape) {
    mt19937_64 data(0x923fb54dULL + n + (uint64_t)shape * 9173);
    vector<vector<double>> sets(16, vector<double>(n));
    for (auto& weights : sets) {
        for (auto& w : weights) {
            w = 1.0 + (double)(data() % 1000);
            if (shape == 1 && data() % 8 != 0) w = 0;
        }
        weights.back() += 1;
        if (shape == 2) weights.back() += 100 * accumulate(weights.begin(), weights.end(), 0.0);
    }
    return sets;
}

// 構築のみ・構築後の抽選・構築込みの反復数別を分けて計測する
void run_weighted_benchmark(uint64_t count, int repeat) {
    for (auto n : {size_t{4}, size_t{16}, size_t{64}, size_t{256}, size_t{1024}}) {
        for (auto shape = 0; shape < 3; ++shape) {
            auto weights = weighted_datasets(n, shape);
            vector<FastRng::WeightedTable> tables;
            vector<WeightedCdf> cdfs;
            vector<discrete_distribution<int32_t>> standard;
            for (const auto& w : weights) {
                tables.emplace_back(w);
                cdfs.emplace_back(w);
                standard.emplace_back(w.begin(), w.end());
            }
            const auto* pattern = shape == 0 ? "dense" : shape == 1 ? "sparse" : "skew-last";
            auto title = string("Weighted draws n=") + to_string(n) + " " + pattern;
            benchmark_heading(title.c_str());
            // 各表から連続して抽選し、256 回ごとに同形状の別の表へ切り替える
            weighted_compare_rows(array{"alias prepared", "cdf prepared", "std prepared"}, count, repeat,
                [&](FastRng& rng, uint64_t i) { return rng.weighted_index(tables[(i >> 8) & 15]); },
                [&](FastRng& rng, uint64_t i) { return cdfs[(i >> 8) & 15].sample(rng); },
                [&](FastRng& rng, uint64_t i) { return standard[(i >> 8) & 15](rng); });
            auto scans = max(uint64_t{1000}, count / (uint64_t)n);
            weighted_compare_rows(array{"one-shot"}, scans, repeat,
                [&](FastRng& rng, uint64_t i) { return rng.weighted_index(weights[(i >> 8) & 15]); });

            title = string("Weighted construction n=") + to_string(n) + " " + pattern;
            benchmark_heading(title.c_str());
            auto builds = max(uint64_t{64}, count / ((uint64_t)n * 8));
            weighted_compare_rows(array{"alias build", "cdf build", "std build"}, builds, repeat,
                [&](FastRng&, uint64_t i) {
                    FastRng::WeightedTable table(weights[i & 15]);
                    asm volatile("" : : "g"(&table) : "memory");
                    return i;
                },
                [&](FastRng&, uint64_t i) {
                    WeightedCdf table(weights[i & 15]);
                    asm volatile("" : : "g"(&table) : "memory");
                    return i;
                },
                [&](FastRng&, uint64_t i) {
                    const auto& w = weights[i & 15];
                    discrete_distribution<int32_t> table(w.begin(), w.end());
                    asm volatile("" : : "g"(&table) : "memory");
                    return i;
                });
        }
    }

    // 1 操作を「新規構築＋指定回数の抽選」として、初期化の償却も比較する
    for (auto n : {size_t{4}, size_t{64}, size_t{1024}}) {
        auto weights = weighted_datasets(n, 0);
        for (auto draws : {uint64_t{1}, uint64_t{8}, uint64_t{64}, uint64_t{1024}}) {
            auto groups = max(uint64_t{16}, min(uint64_t{10000}, count / ((uint64_t)n * draws)));
            auto title = string("Weighted build+draw n=") + to_string(n) + " draws=" + to_string(draws);
            benchmark_heading(title.c_str());
            weighted_compare_rows(array{"one-shot x draws", "alias build+draw", "cdf build+draw", "std build+draw"}, groups, repeat,
                [&](FastRng& rng, uint64_t i) {
                    auto sum = uint64_t{0};
                    for (auto j = uint64_t{0}; j < draws; ++j) sum += (uint64_t)rng.weighted_index(weights[i & 15]);
                    return sum;
                },
                [&](FastRng& rng, uint64_t i) {
                    FastRng::WeightedTable table(weights[i & 15]);
                    auto sum = uint64_t{0};
                    for (auto j = uint64_t{0}; j < draws; ++j) sum += (uint64_t)rng.weighted_index(table);
                    return sum;
                },
                [&](FastRng& rng, uint64_t i) {
                    WeightedCdf table(weights[i & 15]);
                    auto sum = uint64_t{0};
                    for (auto j = uint64_t{0}; j < draws; ++j) sum += (uint64_t)table.sample(rng);
                    return sum;
                },
                [&](FastRng& rng, uint64_t i) {
                    const auto& w = weights[i & 15];
                    discrete_distribution<int32_t> table(w.begin(), w.end());
                    auto sum = uint64_t{0};
                    for (auto j = uint64_t{0}; j < draws; ++j) sum += (uint64_t)table(rng);
                    return sum;
                });
        }
    }
}

}

// 引数なしはテストと全ベンチ、--tests-only はテストのみ
// ベンチは --bench-only / --api-bench-only / --rng-bench-only / --array-bench-only / --weighted-bench-only / --sum-bench-only / --normal-bench-only [回数 [反復数]]
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
          mode == "--api-bench-only" || mode == "--rng-bench-only" || mode == "--array-bench-only" || mode == "--weighted-bench-only" || mode == "--sum-bench-only" || mode == "--normal-bench-only")) {
        cerr << "Usage: rng [--tests-only|--bench-only|--api-bench-only|--rng-bench-only|--array-bench-only|--weighted-bench-only|--sum-bench-only|--normal-bench-only] [count [repeat]]\n";
        return 2;
    }
    cout << "Compiler: GCC " << __VERSION__ << '\n';
    if (mode.empty() || mode == "--tests-only") run_tests();
    if (mode.empty() || mode == "--bench-only" || mode == "--rng-bench-only") run_rng_benchmark(count, repeat);
    if (mode.empty() || mode == "--bench-only" || mode == "--api-bench-only") run_api_benchmark(count, repeat);
    if (mode.empty() || mode == "--bench-only" || mode == "--array-bench-only") run_array_benchmark(count, repeat);
    if (mode.empty() || mode == "--bench-only" || mode == "--weighted-bench-only") run_weighted_benchmark(count, repeat);
    if (mode.empty() || mode == "--bench-only" || mode == "--sum-bench-only") run_sum_benchmark(count, repeat);
    if (mode.empty() || mode == "--bench-only" || mode == "--normal-bench-only") run_normal_benchmark(count, repeat);
    if (mode != "--tests-only") cout << "\nfinal_sink = " << bench_sink << '\n';
}
#endif
