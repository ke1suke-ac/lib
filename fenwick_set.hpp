/*
 * fenwick_set: 座標圧縮済み index 用の競技プログラミング向け順序付き set 実装。
 * 値域 [0, value_range) 上の 0/1 集合を Fenwick Tree と存在 bitset で保持し、rank / kth / 前駆 / 後継を高速に処理する。
 * 重複要素は保持しない。座標圧縮、および元の値との相互変換はこのライブラリの責任範囲外。
 * insert する値は 0 <= x < value_range の圧縮済み index を想定する。値域外の insert / erase は何もしない。
 * 内部の Fenwick Tree のカウンタは uint32_t で保持するため、value_range は UINT32_MAX 以下を想定する。
 * rank / rank2 / count / less_equal / greater_equal は値域外の問い合わせにも自然に動作する。
 *
 * テンプレートパラメータ:
 *   T : 要素型。整数型を想定。実際の値は圧縮済み index として扱われる。
 */
#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

template <typename T = std::size_t>
struct fenwick_set {
    static_assert(std::is_integral_v<T>, "fenwick_set<T>: T must be an integral type");

    using count_type = std::uint32_t;

    // 空の集合を構築する。O(1)
    fenwick_set() = default;

    // 値域 [0, value_range) の空集合を構築する。O(value_range)
    explicit fenwick_set(std::size_t range) { assign_range(range); }

    // 配列から重複を除いた順序付き集合を構築する。O(value_range + n)
    explicit fenwick_set(std::vector<T> a) {
        std::size_t range = 0;
        for (T x : a) {
            if (is_negative(x)) continue;
            const std::size_t idx = static_cast<std::size_t>(x);
            if (idx == std::numeric_limits<std::size_t>::max()) continue;
            range = std::max(range, idx + 1);
        }
        assign_range(range);
        build_from_values(a);
    }

    // 値域 [0, value_range) と配列から重複を除いた順序付き集合を構築する。O(value_range + n)
    fenwick_set(std::size_t range, std::vector<T> a) {
        assign_range(range);
        build_from_values(a);
    }

    // 全要素を削除して空にする。O(value_range)
    void clear() {
        std::fill(bit.begin(), bit.end(), count_type{0});
        std::fill(words.begin(), words.end(), std::uint64_t{0});
        n = 0;
        min_idx = max_idx = 0;
    }

    // 要素 x を挿入する。既に存在する場合は何もしない。O(log value_range)
    void insert(T x) {
        if (!in_range(x)) return;
        const std::size_t idx = static_cast<std::size_t>(x);
        if (test_bit(idx)) return;
        set_bit(idx);
        fenwick_add(idx, +1);
        if (n == 0) {
            min_idx = max_idx = idx;
        } else {
            if (idx < min_idx) min_idx = idx;
            if (idx > max_idx) max_idx = idx;
        }
        ++n;
    }

    // 要素 x を削除する。存在しない場合は何もしない。O(log value_range)
    void erase(T x) {
        if (!in_range(x)) return;
        const std::size_t idx = static_cast<std::size_t>(x);
        if (!test_bit(idx)) return;
        reset_bit(idx);
        fenwick_add(idx, -1);
        --n;
        if (n == 0) {
            min_idx = max_idx = 0;
            return;
        }
        if (idx == min_idx) min_idx = kth_index(0);
        if (idx == max_idx) max_idx = kth_index(n - 1);
    }

    // 現在の要素数を返す。O(1)
    std::size_t size() const { return n; }

    // 集合が空なら true を返す。O(1)
    bool empty() const { return n == 0; }

    // 最小要素を返す。空なら std::nullopt。O(1)
    std::optional<T> min() const {
        if (n == 0) return std::nullopt;
        return static_cast<T>(min_idx);
    }

    // 最大要素を返す。空なら std::nullopt。O(1)
    std::optional<T> max() const {
        if (n == 0) return std::nullopt;
        return static_cast<T>(max_idx);
    }

    // 昇順に全要素へ callback を適用する。bool 変換可能な戻り値が true なら中断する。O(value_range / 64 + n)
    template <class F>
    void for_each(F&& callback) const {
        for_each_impl<false>(callback);
    }

    // 降順に全要素へ callback を適用する。bool 変換可能な戻り値が true なら中断する。O(value_range / 64 + n)
    template <class F>
    void for_each_r(F&& callback) const {
        for_each_impl<true>(callback);
    }

    // 0-indexed で k 番目に小さい要素を返す。O(log value_range)
    T operator[](std::size_t k) const {
        assert(k < n);
        return static_cast<T>(kth_index(k));
    }

    // x より小さい要素数を返す。O(log value_range)
    std::size_t rank(T x) const {
        if (n == 0 || is_negative(x)) return 0;
        const std::size_t idx = static_cast<std::size_t>(x);
        if (idx >= value_range) return n;
        return prefix_sum(idx);
    }

    // x 以下の要素数を返す。O(log value_range)
    std::size_t rank2(T x) const {
        if (n == 0 || is_negative(x)) return 0;
        const std::size_t idx = static_cast<std::size_t>(x);
        if (idx + 1 >= value_range || idx == std::numeric_limits<std::size_t>::max()) return n;
        return prefix_sum(idx + 1);
    }

    // x が存在するなら true を返す。O(1)
    bool contains(T x) const {
        return in_range(x) && test_bit(static_cast<std::size_t>(x));
    }

    // x が存在するかを 0 または 1 で返す。O(1)
    std::size_t count(T x) const { return contains(x); }

    // x 以下の最大要素を返す。存在しない場合は std::nullopt。O(log value_range)
    std::optional<T> less_equal(T x) const {
        if (n == 0 || is_negative(x)) return std::nullopt;
        const std::size_t idx = static_cast<std::size_t>(x);
        if (idx >= value_range) return static_cast<T>(max_idx);
        const std::size_t c = prefix_sum(idx + 1);
        if (c == 0) return std::nullopt;
        return static_cast<T>(kth_index(c - 1));
    }

    // x 以上の最小要素を返す。存在しない場合は std::nullopt。O(log value_range)
    std::optional<T> greater_equal(T x) const {
        if (n == 0) return std::nullopt;
        if (is_negative(x)) return static_cast<T>(min_idx);
        const std::size_t idx = static_cast<std::size_t>(x);
        if (idx >= value_range) return std::nullopt;
        const std::size_t before = prefix_sum(idx);
        if (before == n) return std::nullopt;
        return static_cast<T>(kth_index(before));
    }

private:
    std::size_t value_range = 0;
    std::size_t n = 0;
    std::size_t min_idx = 0;
    std::size_t max_idx = 0;
    std::vector<count_type> bit;
    std::vector<std::uint64_t> words;

    static bool is_negative(T x) {
        if constexpr (std::is_signed_v<T>) {
            return x < 0;
        } else {
            (void)x;
            return false;
        }
    }

    bool in_range(T x) const {
        return !is_negative(x) && static_cast<std::size_t>(x) < value_range;
    }

    void assign_range(std::size_t range) {
        assert(range <= static_cast<std::size_t>(std::numeric_limits<count_type>::max()));
        value_range = range;
        bit.assign(value_range + 1, count_type{0});
        words.assign((value_range + 63) >> 6, std::uint64_t{0});
        n = 0;
        min_idx = max_idx = 0;
    }

    void build_from_values(const std::vector<T>& a) {
        for (T x : a) {
            if (!in_range(x)) continue;
            const std::size_t idx = static_cast<std::size_t>(x);
            if (test_bit(idx)) continue;
            set_bit(idx);
            ++n;
        }
        build_fenwick_from_words();
        if (n != 0) {
            min_idx = kth_index(0);
            max_idx = kth_index(n - 1);
        }
    }

    bool test_bit(std::size_t idx) const { return ((words[idx >> 6] >> (idx & 63)) & 1ULL) != 0; }
    void set_bit(std::size_t idx) { words[idx >> 6] |= std::uint64_t{1} << (idx & 63); }
    void reset_bit(std::size_t idx) { words[idx >> 6] &= ~(std::uint64_t{1} << (idx & 63)); }

    void build_fenwick_from_words() {
        std::fill(bit.begin(), bit.end(), count_type{0});
        for (std::size_t wi = 0; wi < words.size(); ++wi) {
            std::uint64_t w = words[wi];
            while (w) {
                const int b = __builtin_ctzll(w);
                const std::size_t idx = (wi << 6) + static_cast<std::size_t>(b);
                if (idx < value_range) bit[idx + 1] = 1;
                w &= w - 1;
            }
        }
        for (std::size_t i = 1; i <= value_range; ++i) {
            const std::size_t j = i + (i & -i);
            if (j <= value_range) bit[j] += bit[i];
        }
    }

    void fenwick_add(std::size_t idx, int delta) {
        for (++idx; idx <= value_range; idx += idx & -idx) {
            if (delta > 0) ++bit[idx];
            else --bit[idx];
        }
    }

    std::size_t prefix_sum(std::size_t end) const {
        if (end > value_range) end = value_range;
        std::size_t s = 0;
        for (std::size_t i = end; i; i -= i & -i) s += bit[i];
        return s;
    }

    std::size_t kth_index(std::size_t k) const {
        assert(k < n);
        std::size_t idx = 0;
        std::size_t acc = 0;
        for (std::size_t step = std::bit_floor(value_range); step; step >>= 1) {
            const std::size_t next = idx + step;
            if (next <= value_range && acc + bit[next] <= k) {
                idx = next;
                acc += bit[next];
            }
        }
        return idx;
    }

    template <class F>
    static bool invoke_callback(F& callback, const T& x) {
        if constexpr (std::is_void_v<std::invoke_result_t<F&, const T&>>) {
            std::invoke(callback, x);
            return false;
        } else {
            return static_cast<bool>(std::invoke(callback, x));
        }
    }

    template <bool Reverse, class F>
    void for_each_impl(F& callback) const {
        if constexpr (Reverse) {
            for (std::size_t wi = words.size(); wi-- > 0;) {
                std::uint64_t w = words[wi];
                while (w) {
                    const int b = 63 - __builtin_clzll(w);
                    const std::size_t idx = (wi << 6) + static_cast<std::size_t>(b);
                    if (idx < value_range && invoke_callback(callback, static_cast<T>(idx))) return;
                    w &= ~(std::uint64_t{1} << b);
                }
            }
        } else {
            for (std::size_t wi = 0; wi < words.size(); ++wi) {
                std::uint64_t w = words[wi];
                while (w) {
                    const int b = __builtin_ctzll(w);
                    const std::size_t idx = (wi << 6) + static_cast<std::size_t>(b);
                    if (idx < value_range && invoke_callback(callback, static_cast<T>(idx))) return;
                    w &= w - 1;
                }
            }
        }
    }
};

#if __INCLUDE_LEVEL__ == 0
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <string>

namespace fenwick_set_self_test {

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

void require(bool condition, const std::string& message) {
    if (!condition) fail(message);
}

template <class A, class B>
void require_eq(const A& actual, const B& expected, const std::string& message) {
    if (!(actual == expected)) {
        std::ostringstream oss;
        oss << message << " actual=" << actual << " expected=" << expected;
        fail(oss.str());
    }
}

std::string optional_to_string(const std::optional<int>& x) {
    if (!x) return "nullopt";
    return std::to_string(*x);
}

void require_eq_opt(const std::optional<int>& actual, const std::optional<int>& expected,
                    const std::string& message) {
    if (actual != expected) {
        fail(message + " actual=" + optional_to_string(actual) +
             " expected=" + optional_to_string(expected));
    }
}

template <class A, class B>
void require_vec_eq(const std::vector<A>& actual, const std::vector<B>& expected,
                    const std::string& message) {
    if (actual.size() != expected.size()) {
        std::ostringstream oss;
        oss << message << " size actual=" << actual.size() << " expected=" << expected.size();
        fail(oss.str());
    }
    for (std::size_t i = 0; i < actual.size(); ++i) {
        if (!(actual[i] == expected[i])) {
            std::ostringstream oss;
            oss << message << " at " << i << " actual=" << actual[i]
                << " expected=" << expected[i];
            fail(oss.str());
        }
    }
}

std::optional<int> set_less_equal(const std::set<int>& ss, int x) {
    auto it = ss.upper_bound(x);
    if (it == ss.begin()) return std::nullopt;
    --it;
    return *it;
}

std::optional<int> set_greater_equal(const std::set<int>& ss, int x) {
    auto it = ss.lower_bound(x);
    if (it == ss.end()) return std::nullopt;
    return *it;
}

std::vector<int> candidate_values(const std::set<int>& ss, int value_range) {
    std::vector<int> qs = {
        std::numeric_limits<int>::min(), -1000000007, -10, -1, 0, 1, 2, 3,
        value_range / 2 - 1, value_range / 2, value_range / 2 + 1,
        value_range - 2, value_range - 1, value_range, value_range + 1,
        1000000007, std::numeric_limits<int>::max()
    };
    for (int x : ss) {
        qs.push_back(x);
        if (x != std::numeric_limits<int>::min()) qs.push_back(x - 1);
        if (x != std::numeric_limits<int>::max()) qs.push_back(x + 1);
    }
    std::sort(qs.begin(), qs.end());
    qs.erase(std::unique(qs.begin(), qs.end()), qs.end());
    return qs;
}

template <class Set>
std::vector<int> collect_forward(const Set& xs) {
    std::vector<int> got;
    xs.for_each([&](const int& x) { got.push_back(x); });
    return got;
}

template <class Set>
std::vector<int> collect_reverse(const Set& xs) {
    std::vector<int> got;
    xs.for_each_r([&](const int& x) { got.push_back(x); });
    return got;
}

template <class Set>
void check_all_apis(const Set& xs, const std::set<int>& ss, int value_range,
                    const std::string& label) {
    const std::vector<int> expected(ss.begin(), ss.end());
    std::vector<int> expected_r = expected;
    std::reverse(expected_r.begin(), expected_r.end());

    require_eq(xs.size(), ss.size(), label + ": size");
    require_eq(xs.empty(), ss.empty(), label + ": empty");
    require_eq_opt(xs.min(), ss.empty() ? std::optional<int>{} : std::optional<int>{*ss.begin()},
                   label + ": min");
    require_eq_opt(xs.max(), ss.empty() ? std::optional<int>{} : std::optional<int>{*ss.rbegin()},
                   label + ": max");
    require_vec_eq(collect_forward(xs), expected, label + ": for_each order");
    require_vec_eq(collect_reverse(xs), expected_r, label + ": for_each_r order");

    for (std::size_t k = 0; k < expected.size(); ++k) {
        require_eq(xs[k], expected[k], label + ": operator[]");
    }

    for (int q : candidate_values(ss, value_range)) {
        const std::size_t rk = static_cast<std::size_t>(
            std::lower_bound(expected.begin(), expected.end(), q) - expected.begin());
        const std::size_t rk2 = static_cast<std::size_t>(
            std::upper_bound(expected.begin(), expected.end(), q) - expected.begin());
        require_eq(xs.rank(q), rk, label + ": rank");
        require_eq(xs.rank2(q), rk2, label + ": rank2");
        require_eq(xs.contains(q), ss.contains(q), label + ": contains");
        require_eq(xs.count(q), ss.count(q), label + ": count");
        require_eq_opt(xs.less_equal(q), set_less_equal(ss, q), label + ": less_equal");
        require_eq_opt(xs.greater_equal(q), set_greater_equal(ss, q), label + ": greater_equal");
    }
}

struct NotBoolConvertible {};

void test_empty_and_singleton() {
    constexpr int range = 32;
    fenwick_set<int> xs(range);
    std::set<int> ss;
    check_all_apis(xs, ss, range, "empty with range");

    fenwick_set<int> zero;
    check_all_apis(zero, ss, 0, "empty default zero range");

    int calls = 0;
    xs.for_each([&](const int&) { ++calls; });
    xs.for_each_r([&](const int&) { ++calls; return true; });
    require_eq(calls, 0, "empty iteration callbacks are not called");

    xs.erase(123);
    xs.erase(-1);
    xs.clear();
    check_all_apis(xs, ss, range, "empty after erase/clear");

    xs.insert(10);
    ss.insert(10);
    check_all_apis(xs, ss, range, "singleton insert");

    xs.insert(10);
    ss.insert(10);
    xs.erase(11);
    ss.erase(11);
    check_all_apis(xs, ss, range, "singleton duplicate and missing erase");

    xs.erase(10);
    ss.erase(10);
    check_all_apis(xs, ss, range, "singleton erased");

    xs.insert(7);
    ss.insert(7);
    xs.clear();
    ss.clear();
    check_all_apis(xs, ss, range, "clear nonempty");

    xs.insert(7);
    ss.insert(7);
    check_all_apis(xs, ss, range, "reuse after clear");
}

void test_constructor_duplicates_and_boundaries() {
    std::vector<int> init = {5, 3, 5, 1, 0, 3, 99, 42, 42, 1};
    fenwick_set<int> xs(100, init);
    std::set<int> ss(init.begin(), init.end());
    check_all_apis(xs, ss, 100, "constructor duplicates and value boundaries");

    fenwick_set<int> auto_range(init);
    check_all_apis(auto_range, ss, 100, "auto range vector constructor");

    fenwick_set<int> empty_from_vector(std::vector<int>{});
    check_all_apis(empty_from_vector, std::set<int>{}, 0, "empty vector constructor");
}

void test_iteration_break_modes() {
    constexpr int range = 64;
    std::vector<int> init = {0, 2, 4, 8, 13, 20, 31, 63};
    fenwick_set<int> xs(range, init);
    std::set<int> ss(init.begin(), init.end());
    check_all_apis(xs, ss, range, "iteration source");

    std::vector<int> got;
    xs.for_each([&](const int& x) -> bool {
        got.push_back(x);
        return x >= 13;
    });
    require_vec_eq(got, std::vector<int>({0, 2, 4, 8, 13}), "for_each bool break");

    got.clear();
    xs.for_each([&](const int& x) -> int {
        got.push_back(x);
        return x == 20;
    });
    require_vec_eq(got, std::vector<int>({0, 2, 4, 8, 13, 20}), "for_each int break");

    got.clear();
    xs.for_each([&](const int& x) -> std::optional<int> {
        got.push_back(x);
        if (x == 8) return 1;
        return std::nullopt;
    });
    require_vec_eq(got, std::vector<int>({0, 2, 4, 8}), "for_each optional break");

    got.clear();
    xs.for_each([&](const int& x) -> const int* {
        got.push_back(x);
        return x == 13 ? &got.back() : nullptr;
    });
    require_vec_eq(got, std::vector<int>({0, 2, 4, 8, 13}), "for_each pointer break");

    got.clear();
    xs.for_each_r([&](const int& x) -> bool {
        got.push_back(x);
        return x <= 8;
    });
    require_vec_eq(got, std::vector<int>({63, 31, 20, 13, 8}), "for_each_r bool break");

    got.clear();
    xs.for_each_r([&](const int& x) -> void { got.push_back(x); });
    std::vector<int> expected_r(init.rbegin(), init.rend());
    require_vec_eq(got, expected_r, "for_each_r void full scan");
}

void test_deterministic_updates() {
    constexpr int range = 128;
    fenwick_set<int> xs(range);
    std::set<int> ss;
    const std::vector<int> values = {
        64, 63, 65, 0, 127, 1, 126, 2, 125, 3, 124, 4, 123,
        5, 122, 6, 121, 7, 120, 8, 119, 9, 118, 10, 117
    };
    for (int x : values) {
        xs.insert(x);
        ss.insert(x);
        check_all_apis(xs, ss, range, "deterministic insert");
    }
    for (int x : {0, 127, 64, 200, -1, 5, 122, 1, 126, 10, 117, 2, 125}) {
        xs.erase(x);
        ss.erase(x);
        check_all_apis(xs, ss, range, "deterministic erase/missing erase");
    }
    for (int x : values) {
        xs.insert(x);
        ss.insert(x);
    }
    check_all_apis(xs, ss, range, "deterministic refill");
}

void test_random_exhaustive() {
    constexpr int range = 181;
    fenwick_set<int> xs(range);
    std::set<int> ss;
    std::mt19937 rng(123456789u);
    std::uniform_int_distribution<int> value_dist(0, range - 1);
    std::uniform_int_distribution<int> query_dist(-10, range + 10);
    std::uniform_int_distribution<int> op_dist(0, 11);

    for (int step = 0; step < 100000; ++step) {
        const int op = op_dist(rng);
        const int x = (op <= 5 ? value_dist(rng) : query_dist(rng));
        if (op <= 2) {
            xs.insert(x);
            ss.insert(x);
        } else if (op <= 5) {
            xs.erase(x);
            ss.erase(x);
        } else if (op == 6) {
            require_eq(xs.count(x), ss.count(x), "random count");
        } else if (op == 7) {
            const std::vector<int> v(ss.begin(), ss.end());
            const std::size_t expected = static_cast<std::size_t>(
                std::lower_bound(v.begin(), v.end(), x) - v.begin());
            require_eq(xs.rank(x), expected, "random rank");
        } else if (op == 8) {
            const std::vector<int> v(ss.begin(), ss.end());
            const std::size_t expected = static_cast<std::size_t>(
                std::upper_bound(v.begin(), v.end(), x) - v.begin());
            require_eq(xs.rank2(x), expected, "random rank2");
        } else if (op == 9) {
            require_eq_opt(xs.less_equal(x), set_less_equal(ss, x), "random less_equal");
        } else if (op == 10) {
            require_eq_opt(xs.greater_equal(x), set_greater_equal(ss, x), "random greater_equal");
        } else {
            require_eq_opt(xs.min(), ss.empty() ? std::optional<int>{} : std::optional<int>{*ss.begin()},
                           "random min");
            require_eq_opt(xs.max(), ss.empty() ? std::optional<int>{} : std::optional<int>{*ss.rbegin()},
                           "random max");
            if (!ss.empty()) {
                std::uniform_int_distribution<std::size_t> kth_dist(0, ss.size() - 1);
                const std::size_t k = kth_dist(rng);
                const std::vector<int> v(ss.begin(), ss.end());
                require_eq(xs[k], v[k], "random kth");
            }
        }
        if ((step % 257) == 0) check_all_apis(xs, ss, range, "random periodic full check");
    }
    check_all_apis(xs, ss, range, "random final full check");
}

void check_large_sample(const fenwick_set<int>& xs, const std::set<int>& ss, int value_range,
                        std::mt19937& rng, const std::string& label) {
    const std::vector<int> v(ss.begin(), ss.end());
    require_eq(xs.size(), ss.size(), label + ": size");
    require_eq_opt(xs.min(), ss.empty() ? std::optional<int>{} : std::optional<int>{*ss.begin()},
                   label + ": min");
    require_eq_opt(xs.max(), ss.empty() ? std::optional<int>{} : std::optional<int>{*ss.rbegin()},
                   label + ": max");
    require_vec_eq(collect_forward(xs), v, label + ": for_each full order");
    std::vector<int> rv = v;
    std::reverse(rv.begin(), rv.end());
    require_vec_eq(collect_reverse(xs), rv, label + ": for_each_r full order");

    if (!v.empty()) {
        std::uniform_int_distribution<std::size_t> kth_dist(0, v.size() - 1);
        for (int i = 0; i < 128; ++i) {
            const std::size_t k = kth_dist(rng);
            require_eq(xs[k], v[k], label + ": sampled kth");
        }
    }

    std::uniform_int_distribution<int> query_dist(-1000, value_range + 1000);
    std::vector<int> qs = {
        std::numeric_limits<int>::min(), -1, 0, 1, value_range / 2,
        value_range - 1, value_range, value_range + 1, std::numeric_limits<int>::max()
    };
    for (int i = 0; i < 256; ++i) qs.push_back(query_dist(rng));
    if (!v.empty()) {
        std::uniform_int_distribution<std::size_t> idx_dist(0, v.size() - 1);
        for (int i = 0; i < 128; ++i) {
            const int y = v[idx_dist(rng)];
            qs.push_back(y);
            if (y != std::numeric_limits<int>::min()) qs.push_back(y - 1);
            if (y != std::numeric_limits<int>::max()) qs.push_back(y + 1);
        }
    }
    for (int q : qs) {
        const std::size_t rk = static_cast<std::size_t>(
            std::lower_bound(v.begin(), v.end(), q) - v.begin());
        const std::size_t rk2 = static_cast<std::size_t>(
            std::upper_bound(v.begin(), v.end(), q) - v.begin());
        require_eq(xs.rank(q), rk, label + ": sampled rank");
        require_eq(xs.rank2(q), rk2, label + ": sampled rank2");
        require_eq(xs.count(q), ss.count(q), label + ": sampled count");
        require_eq_opt(xs.less_equal(q), set_less_equal(ss, q), label + ": sampled less_equal");
        require_eq_opt(xs.greater_equal(q), set_greater_equal(ss, q), label + ": sampled greater_equal");
    }
}

void test_large_random_against_std_set() {
    constexpr int range = 400001;
    fenwick_set<int> xs(range);
    std::set<int> ss;
    std::mt19937 rng(987654321u);
    std::uniform_int_distribution<int> value_dist(0, range - 1);
    std::uniform_int_distribution<int> query_dist(-1000, range + 1000);
    std::uniform_int_distribution<int> op_dist(0, 9);

    constexpr int operations = 300000;
    for (int step = 1; step <= operations; ++step) {
        const int op = op_dist(rng);
        const int x = (op <= 6 ? value_dist(rng) : query_dist(rng));
        if (op <= 3) {
            xs.insert(x);
            ss.insert(x);
        } else if (op <= 6) {
            xs.erase(x);
            ss.erase(x);
        } else if (op == 7) {
            require_eq(xs.count(x), ss.count(x), "large random count immediate");
        } else if (op == 8) {
            require_eq_opt(xs.less_equal(x), set_less_equal(ss, x), "large random less_equal immediate");
        } else {
            require_eq_opt(xs.greater_equal(x), set_greater_equal(ss, x), "large random greater_equal immediate");
        }

        if ((step % 5000) == 0) {
            check_large_sample(xs, ss, range, rng, "large random sample step " + std::to_string(step));
        }
    }
    check_large_sample(xs, ss, range, rng, "large random final");
}

void run_all_tests() {
    test_empty_and_singleton();
    test_constructor_duplicates_and_boundaries();
    test_iteration_break_modes();
    test_deterministic_updates();
    test_random_exhaustive();
    test_large_random_against_std_set();
}

} // namespace fenwick_set_self_test

namespace fenwick_set_self_benchmark {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

inline volatile u64 benchmark_sink = 0;

struct SplitMix64 {
    u64 x;
    explicit SplitMix64(u64 seed) : x(seed) {}

    u64 next() {
        u64 z = (x += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    u32 next_u32(u32 mod) { return static_cast<u32>(next() % mod); }
};

struct Timer {
    std::chrono::steady_clock::time_point t0;
    Timer() : t0(std::chrono::steady_clock::now()) {}
    double ms() const {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }
};

struct BenchRow {
    std::string api;
    std::size_t n = 0;
    std::size_t q = 0;
    std::size_t fenwick_ops = 0;
    std::size_t std_ops = 0;
    double fenwick_ms = 0.0;
    double std_ms = -1.0;
    u64 fenwick_checksum = 0;
    u64 std_checksum = 0;
    std::string status;
};

std::vector<u32> sorted_unique_values(std::size_t n, u32 mul = 2, u32 add = 0) {
    std::vector<u32> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<u32>(i * mul + add);
    return v;
}

std::vector<u32> shuffled_unique_values(std::size_t n, u32 mul, u64 seed, u32 add = 0) {
    auto v = sorted_unique_values(n, mul, add);
    SplitMix64 rng(seed);
    for (std::size_t i = n; i > 1; --i) {
        std::swap(v[i - 1], v[static_cast<std::size_t>(rng.next() % i)]);
    }
    return v;
}

std::vector<u32> query_values(std::size_t q, u32 mod, u64 seed) {
    SplitMix64 rng(seed);
    std::vector<u32> qs(q);
    for (std::size_t i = 0; i < q; ++i) qs[i] = rng.next_u32(mod);
    return qs;
}

std::vector<std::size_t> query_indices(std::size_t q, std::size_t n, u64 seed) {
    SplitMix64 rng(seed);
    std::vector<std::size_t> qs(q);
    for (std::size_t i = 0; i < q; ++i) qs[i] = static_cast<std::size_t>(rng.next() % n);
    return qs;
}

BenchRow bench_default_construct(std::size_t q) {
    u64 cb = 0, cs = 0;
    double mb = 0.0, ms = 0.0;
    {
        Timer t;
        std::vector<fenwick_set<u32>> v(q);
        cb += v.size();
        mb = t.ms();
    }
    {
        Timer t;
        std::vector<std::set<u32>> v(q);
        cs += v.size();
        ms = t.ms();
    }
    return {"default_construct", 0, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_construct_range(std::size_t n) {
    u64 cb = 0;
    Timer tb;
    fenwick_set<u32> bs(n * 2 + 1);
    const double mb = tb.ms();
    cb = bs.size();
    return {"construct(range)", n, 1, 1, 0, mb, -1.0, cb, 0, cb == 0 ? "fenwick_only" : "wrong"};
}

BenchRow bench_construct_vector(std::size_t n, u64 seed) {
    const auto vals = shuffled_unique_values(n, 2, seed);
    u64 cb = 0, cs = 0;
    Timer tb;
    fenwick_set<u32> bs(vals);
    const double mb = tb.ms();
    cb = bs.size();
    Timer ts;
    std::set<u32> ss(vals.begin(), vals.end());
    const double ms = ts.ms();
    cs = ss.size();
    return {"construct(vector)", n, 1, 1, 1, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_clear(std::size_t n, std::size_t q, u64 seed) {
    const auto vals = shuffled_unique_values(n, 2, seed);
    std::vector<fenwick_set<u32>> bv;
    std::vector<std::set<u32>> sv;
    bv.reserve(q);
    sv.reserve(q);
    for (std::size_t i = 0; i < q; ++i) {
        bv.emplace_back(n * 2 + 1, vals);
        sv.emplace_back(vals.begin(), vals.end());
    }
    u64 cb = 0, cs = 0;
    Timer tb;
    for (auto& x : bv) {
        x.clear();
        cb += x.size();
    }
    const double mb = tb.ms();
    Timer ts;
    for (auto& x : sv) {
        x.clear();
        cs += x.size();
    }
    const double ms = ts.ms();
    return {"clear", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_insert_unique_empty(std::size_t q, u64 seed) {
    const auto vals = shuffled_unique_values(q, 2, seed);
    fenwick_set<u32> bs(q * 2 + 1);
    std::set<u32> ss;
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : vals) bs.insert(x);
    const double mb = tb.ms();
    cb = bs.size();
    Timer ts;
    for (u32 x : vals) ss.insert(x);
    const double ms = ts.ms();
    cs = ss.size();
    return {"insert(unique,empty)", q, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_insert_missing(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto vals = shuffled_unique_values(q, 2, seed, 1);
    const std::size_t range = std::max(n, q) * 2 + 2;
    fenwick_set<u32> bs(range, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : vals) bs.insert(x);
    const double mb = tb.ms();
    cb = bs.size();
    Timer ts;
    for (u32 x : vals) ss.insert(x);
    const double ms = ts.ms();
    cs = ss.size();
    return {"insert(missing)", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_insert_duplicate(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(n), seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 i : qs) {
        bs.insert(init[i]);
        cb += bs.size();
    }
    const double mb = tb.ms();
    Timer ts;
    for (u32 i : qs) {
        ss.insert(init[i]);
        cs += ss.size();
    }
    const double ms = ts.ms();
    return {"insert(duplicate)", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_erase_present(std::size_t n, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    auto vals = init;
    SplitMix64 rng(seed);
    for (std::size_t i = vals.size(); i > 1; --i) {
        std::swap(vals[i - 1], vals[static_cast<std::size_t>(rng.next() % i)]);
    }
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : vals) bs.erase(x);
    const double mb = tb.ms();
    cb = bs.size();
    Timer ts;
    for (u32 x : vals) ss.erase(x);
    const double ms = ts.ms();
    cs = ss.size();
    return {"erase(present)", n, n, n, n, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_erase_missing(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto vals = shuffled_unique_values(q, 2, seed, 1);
    const std::size_t range = std::max(n, q) * 2 + 2;
    fenwick_set<u32> bs(range, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : vals) bs.erase(x);
    const double mb = tb.ms();
    cb = bs.size();
    Timer ts;
    for (u32 x : vals) ss.erase(x);
    const double ms = ts.ms();
    cs = ss.size();
    return {"erase(missing)", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_size(std::size_t n, std::size_t q, u64 seed) {
    const auto init = shuffled_unique_values(n, 2, seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cb += bs.size();
    }
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cs += ss.size();
    }
    const double ms = ts.ms();
    return {"size", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_empty(std::size_t n, std::size_t q, u64 seed) {
    const auto init = shuffled_unique_values(n, 2, seed);
    fenwick_set<u32> fs(2 * n + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cf = 0, cs = 0;
    Timer tf;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cf += fs.empty();
    }
    const double mf = tf.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cs += ss.empty();
    }
    const double ms = ts.ms();
    return {"empty", n, q, q, q, mf, ms, cf, cs, cf == cs ? "ok" : "wrong"};
}

BenchRow bench_min(std::size_t n, std::size_t q, u64 seed) {
    const auto init = shuffled_unique_values(n, 2, seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cb += *bs.min();
    }
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cs += *ss.begin();
    }
    const double ms = ts.ms();
    return {"min", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_max(std::size_t n, std::size_t q, u64 seed) {
    const auto init = shuffled_unique_values(n, 2, seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cb += *bs.max();
    }
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cs += *ss.rbegin();
    }
    const double ms = ts.ms();
    return {"max", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_for_each(std::size_t n, u64 seed, bool rev, bool brk) {
    (void)seed;
    const auto init = sorted_unique_values(n, 2, 0);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    const std::size_t limit = brk ? n / 2 : n;
    u64 cb = 0, cs = 0;
    std::size_t cbn = 0, csn = 0;
    Timer tb;
    if (!rev) {
        bs.for_each([&](const u32& x) -> bool {
            cb += x;
            return ++cbn >= limit;
        });
    } else {
        bs.for_each_r([&](const u32& x) -> bool {
            cb += x;
            return ++cbn >= limit;
        });
    }
    const double mb = tb.ms();
    Timer ts;
    if (!rev) {
        for (u32 x : ss) {
            cs += x;
            if (++csn >= limit) break;
        }
    } else {
        for (auto it = ss.rbegin(); it != ss.rend(); ++it) {
            cs += *it;
            if (++csn >= limit) break;
        }
    }
    const double ms = ts.ms();
    std::string name = rev ? "for_each_r" : "for_each";
    if (brk) name += "(break)";
    return {name, n, limit, cbn, csn, mb, ms, cb, cs, cb == cs && cbn == csn ? "ok" : "wrong"};
}

BenchRow bench_kth(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_indices(q, n, seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    u64 cb = 0;
    Timer tb;
    for (std::size_t k : qs) cb += bs[k];
    const double mb = tb.ms();
    return {"operator[]", n, q, q, 0, mb, -1.0, cb, 0, "fenwick_only"};
}

BenchRow bench_rank_like(std::string name, std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(n * 4 + 1), seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    u64 cb = 0;
    Timer tb;
    if (name == "rank") {
        for (u32 x : qs) cb += bs.rank(x);
    } else {
        for (u32 x : qs) cb += bs.rank2(x);
    }
    const double mb = tb.ms();
    return {name, n, q, q, 0, mb, -1.0, cb, 0, "fenwick_only"};
}

BenchRow bench_count(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(n * 4 + 1), seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : qs) cb += bs.count(x);
    const double mb = tb.ms();
    Timer ts;
    for (u32 x : qs) cs += ss.count(x);
    const double ms = ts.ms();
    return {"count", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_contains(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(4 * n), seed);
    fenwick_set<u32> fs(4 * n + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cf = 0, cs = 0;
    Timer tf;
    for (u32 x : qs) cf += fs.contains(x);
    const double mf = tf.ms();
    Timer ts;
    for (u32 x : qs) cs += ss.contains(x);
    const double ms = ts.ms();
    return {"contains", n, q, q, q, mf, ms, cf, cs, cf == cs ? "ok" : "wrong"};
}

BenchRow bench_less_equal(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(n * 4 + 1), seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : qs) cb += bs.less_equal(x).value_or(0);
    const double mb = tb.ms();
    Timer ts;
    for (u32 x : qs) {
        auto it = ss.upper_bound(x);
        if (it != ss.begin()) {
            --it;
            cs += *it;
        }
    }
    const double ms = ts.ms();
    return {"less_equal", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_greater_equal(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(n * 4 + 1), seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : qs) cb += bs.greater_equal(x).value_or(0);
    const double mb = tb.ms();
    Timer ts;
    for (u32 x : qs) {
        auto it = ss.lower_bound(x);
        if (it != ss.end()) cs += *it;
    }
    const double ms = ts.ms();
    return {"greater_equal", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

template <class F>
BenchRow median_of_three(F&& f) {
    std::vector<BenchRow> rows;
    rows.reserve(3);
    for (int i = 0; i < 3; ++i) rows.push_back(f());
    std::sort(rows.begin(), rows.end(), [](const BenchRow& a, const BenchRow& b) {
        return a.fenwick_ms < b.fenwick_ms;
    });
    benchmark_sink = benchmark_sink + rows[1].fenwick_checksum + rows[1].std_checksum;
    return rows[1];
}

std::vector<BenchRow> run_benchmarks() {
    std::vector<BenchRow> rows;
    rows.push_back(median_of_three([] { return bench_default_construct(100000); }));
    rows.push_back(median_of_three([] { return bench_construct_range(200000); }));
    rows.push_back(median_of_three([] { return bench_construct_vector(200000, 1); }));
    rows.push_back(median_of_three([] { return bench_clear(100000, 5, 2); }));
    rows.push_back(median_of_three([] { return bench_insert_unique_empty(200000, 3); }));
    rows.push_back(median_of_three([] { return bench_insert_missing(200000, 500000, 4); }));
    rows.push_back(median_of_three([] { return bench_insert_duplicate(200000, 500000, 5); }));
    rows.push_back(median_of_three([] { return bench_erase_present(200000, 6); }));
    rows.push_back(median_of_three([] { return bench_erase_missing(200000, 500000, 7); }));
    rows.push_back(median_of_three([] { return bench_size(200000, 5000000, 8); }));
    rows.push_back(median_of_three([] { return bench_empty(200000, 5000000, 81); }));
    rows.push_back(median_of_three([] { return bench_min(200000, 5000000, 9); }));
    rows.push_back(median_of_three([] { return bench_max(200000, 5000000, 10); }));
    rows.push_back(median_of_three([] { return bench_for_each(200000, 11, false, false); }));
    rows.push_back(median_of_three([] { return bench_for_each(200000, 12, true, false); }));
    rows.push_back(median_of_three([] { return bench_for_each(200000, 13, false, true); }));
    rows.push_back(median_of_three([] { return bench_for_each(200000, 14, true, true); }));
    rows.push_back(median_of_three([] { return bench_kth(50000, 100000, 15); }));
    rows.push_back(median_of_three([] { return bench_rank_like("rank", 200000, 500000, 16); }));
    rows.push_back(median_of_three([] { return bench_rank_like("rank2", 200000, 500000, 17); }));
    rows.push_back(median_of_three([] { return bench_contains(200000, 500000, 18); }));
    rows.push_back(median_of_three([] { return bench_count(200000, 500000, 18); }));
    rows.push_back(median_of_three([] { return bench_less_equal(200000, 500000, 19); }));
    rows.push_back(median_of_three([] { return bench_greater_equal(200000, 500000, 20); }));
    return rows;
}

void print_benchmark_table(const std::vector<BenchRow>& rows) {
    std::cout << "api,n,q,fenwick_ms,std_set_ms,fenwick_ns_per_op,std_set_ns_per_op,std_over_fenwick,status\n";
    std::cout << std::fixed << std::setprecision(3);
    for (const auto& r : rows) {
        const double f_ns = r.fenwick_ops ? r.fenwick_ms * 1e6 / static_cast<double>(r.fenwick_ops) : 0.0;
        const double s_ns = (r.std_ms >= 0.0 && r.std_ops) ? r.std_ms * 1e6 / static_cast<double>(r.std_ops) : -1.0;
        const double ratio = (r.std_ms >= 0.0 && r.fenwick_ms > 0.0) ? r.std_ms / r.fenwick_ms : -1.0;
        std::cout << r.api << ',' << r.n << ',' << r.q << ',' << r.fenwick_ms << ',';
        if (r.std_ms >= 0.0) std::cout << r.std_ms; else std::cout << "NA";
        std::cout << ',' << f_ns << ',';
        if (s_ns >= 0.0) std::cout << s_ns; else std::cout << "NA";
        std::cout << ',';
        if (ratio >= 0.0) std::cout << ratio; else std::cout << "NA";
        std::cout << ',' << r.status << '\n';
    }
}

} // namespace fenwick_set_self_benchmark

int main() {
    using namespace fenwick_set_self_test;
    run_all_tests();
    std::cout << "edge_empty_singleton_clear: ok\n";
    std::cout << "edge_constructor_duplicates_boundaries: ok\n";
    std::cout << "edge_iteration_callbacks: ok\n";
    std::cout << "deterministic_update_edges: ok\n";
    std::cout << "random_full_check_100000_ops: ok\n";
    std::cout << "random_default_range_sampled_300000_ops: ok\n";
    std::cout << "all tests passed\n";
    auto rows = fenwick_set_self_benchmark::run_benchmarks();
    fenwick_set_self_benchmark::print_benchmark_table(rows);
    return 0;
}
#endif

// 実行結果(atcoder)
// edge_empty_singleton_clear: ok
// edge_constructor_duplicates_boundaries: ok
// edge_iteration_callbacks: ok
// deterministic_update_edges: ok
// random_full_check_100000_ops: ok
// random_default_range_sampled_300000_ops: ok
// all tests passed
// api,n,q,fenwick_ms,std_set_ms,fenwick_ns_per_op,std_set_ns_per_op,std_over_fenwick,status
// default_construct,0,100000,1.246,0.175,12.462,1.749,0.140,ok
// construct(range),200000,1,0.037,NA,37012.000,NA,NA,fenwick_only
// construct(vector),200000,1,0.666,39.917,666382.000,39917449.000,59.902,ok
// clear,100000,5,0.162,15.182,32464.400,3036492.400,93.533,ok
// insert(unique,empty),200000,200000,3.997,39.240,19.984,196.200,9.818,ok
// insert(missing),200000,500000,12.881,182.029,25.763,364.057,14.131,ok
// insert(duplicate),200000,500000,0.534,78.407,1.069,156.814,146.737,ok
// erase(present),200000,200000,3.981,61.396,19.907,306.980,15.421,ok
// erase(missing),200000,500000,0.357,82.876,0.713,165.753,232.319,ok
// size,200000,5000000,1.529,1.550,0.306,0.310,1.013,ok
// empty,200000,5000000,1.528,1.486,0.306,0.297,0.973,ok
// min,200000,5000000,1.634,1.549,0.327,0.310,0.948,ok
// max,200000,5000000,3.083,13.899,0.617,2.780,4.508,ok
// for_each,200000,200000,0.124,1.359,0.619,6.797,10.981,ok
// for_each_r,200000,200000,0.337,1.560,1.687,7.801,4.624,ok
// for_each(break),200000,100000,0.062,0.677,0.618,6.770,10.964,ok
// for_each_r(break),200000,100000,0.163,0.751,1.626,7.511,4.619,ok
// operator[],50000,100000,3.513,NA,35.130,NA,NA,fenwick_only
// rank,200000,500000,4.754,NA,9.508,NA,NA,fenwick_only
// rank2,200000,500000,4.893,NA,9.786,NA,NA,fenwick_only
// contains,200000,500000,0.360,106.592,0.721,213.184,295.713,ok
// count,200000,500000,2.171,110.690,4.342,221.380,50.987,ok
// less_equal,200000,500000,15.842,111.444,31.684,222.888,7.035,ok
// greater_equal,200000,500000,15.754,109.648,31.509,219.296,6.960,ok
