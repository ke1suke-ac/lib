/*
 * btree_set: 競技プログラミング向けの順序付き set 実装。
 * 要素を複数のソート済み vector ブロックに分けて保持し、ブロック最大値と Fenwick Tree 風の累積個数で
 * lower_bound / upper_bound / rank / kth を高速に処理し、min / max / 全要素走査も提供する。重複要素は保持しない。
 *
 * テンプレートパラメータ:
 *   T     : 要素型。operator< / operator<= / operator> などで比較できる型を想定。
 *   load1 : 1 ブロックに入れる要素数の目安。大きいほど検索寄り、小さいほど insert / erase 寄り。
 *   load2 : 1 セグメントに入れるブロック数の目安。2 の冪を推奨。大きいほど rebuild が減り、小さいほど局所更新が軽い。
 */
#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

template <typename T, std::size_t load1 = 200, std::size_t load2 = 64>
struct btree_set {
    using const_iterator = typename std::vector<T>::const_iterator;

    // 空の集合を遅延初期化で構築する。O(1)
    btree_set() = default;

    // 配列から重複を除いた順序付き集合を構築する。O(n log n)
    explicit btree_set(std::vector<T> a) {
        std::sort(a.begin(), a.end());
        a.erase(std::unique(a.begin(), a.end()), a.end());
        n = a.size();
        if (n == 0) return;

        blocks.reserve(n / load1 + 2);
        bit_cnt.reserve(n / load1 + 2);
        blk_max.reserve(n / load1 + 2);
        make_sentinel();

        for (std::size_t l = 0; l < n; l += load1) {
            std::size_t r = std::min(l + load1, n);
            ensure_storage(block_len + 1);
            blocks[block_len].assign(a.data() + l, a.data() + r);
            ++block_len;
        }

        expand();
    }

    // 全要素を削除して内部 capacity を保持する。O(保持ブロック数)
    void clear() {
        for (auto& block : blocks) block.clear();
        seg_size.clear();
        block_len = 0;
        n = 0;
    }

    // 要素 x を挿入する。既に存在する場合は何もしない。O(log n + load1 + load2)
    void insert(T x) {
        if (seg_size.empty()) {
            make_sentinel();
            ensure_storage(2);
            blocks[1].clear();
            blocks[1].push_back(x);
            bit_cnt[1] = 1;
            blk_max[1] = x;
            block_len = 2;
            ++n;
            seg_size.assign(1, 1);
            return;
        }

        auto [bi, it] = this->template bound<false>(x);
        if (it != blocks[bi].end() && *it == x) return;

        ++n;
        for (std::size_t i = bi; i < block_len; i += (-i & i)) ++bit_cnt[i];

        const std::size_t segi = (bi - 1) >> log;
        const std::size_t bj = (segi << log) | seg_size[segi];
        const std::size_t bn = (segi + 1) << log;
        const bool x_in_block_end = (it == blocks[bi].end());
        blocks[bi].insert(it, x);

        if (blocks[bi].size() >= load1x) {
            if (block_len <= bn) {
                ensure_storage(block_len + 1);
                for (std::size_t idx = block_len; idx > bi + 1; --idx)
                    blocks[idx] = std::move(blocks[idx - 1]);
                blocks[bi + 1].assign(blocks[bi].begin() + load1, blocks[bi].end());
                bit_cnt[block_len] = 0;
                blk_max[block_len].reset();
                ++block_len;
            } else {
                blocks[bj + 1].clear();
                for (std::size_t idx = bj + 1; idx > bi + 1; --idx)
                    blocks[idx] = std::move(blocks[idx - 1]);
                blocks[bi + 1].assign(blocks[bi].begin() + load1, blocks[bi].end());
            }
            blocks[bi].resize(load1);
            if (++seg_size[segi] == load2x)
                expand();
            else
                range_bit_modify((segi << log) | 1, std::min(bn, block_len - 1));
        } else if (x_in_block_end) {
            blk_max[bi] = blocks[bi].back();
            if (bi == bj)
                for (std::size_t i = bi; i < std::min(bj + 1, block_len); i += -i & i)
                    blk_max[i] = blk_max[bi];
        }
    }

    // 要素 x を削除する。存在しない場合は何もしない。O(log n + load1 + load2)
    void erase(T x) {
        if (seg_size.empty()) return;

        auto [bi, it] = this->template bound<false>(x);
        if (it == blocks[bi].end() || *it > x) return;

        --n;
        for (std::size_t i = bi; i < block_len; i += (-i & i)) --bit_cnt[i];

        const std::size_t segi = (bi - 1) >> log;
        const std::size_t bj = (segi << log) | seg_size[segi];
        const std::size_t bn = (segi + 1) << log;
        const bool x_in_block_end = (std::next(it) == blocks[bi].end());
        blocks[bi].erase(it);

        if (blocks[bi].empty()) {
            if (block_len <= bn) {
                for (std::size_t idx = bi; idx + 1 < block_len; ++idx)
                    blocks[idx] = std::move(blocks[idx + 1]);
                if (block_len > 0) {
                    blocks[block_len - 1].clear();
                    bit_cnt[block_len - 1] = 0;
                    blk_max[block_len - 1].reset();
                    --block_len;
                }
                if (--seg_size[segi] == 0)
                    seg_size.pop_back();
                else
                    range_bit_modify((segi << log) | 1, block_len - 1);
            } else if (--seg_size[segi] == 0) {
                expand();
            } else {
                blocks[bi].clear();
                for (std::size_t idx = bi; idx < bj; ++idx)
                    blocks[idx] = std::move(blocks[idx + 1]);
                blocks[bj].clear();
                range_bit_modify((segi << log) | 1, bn);
            }
            while (block_len > 1 && blocks[block_len - 1].empty()) {
                bit_cnt[block_len - 1] = 0;
                blk_max[block_len - 1].reset();
                --block_len;
            }
        } else if (x_in_block_end) {
            blk_max[bi] = blocks[bi].back();
            if (bi == bj)
                for (std::size_t i = bi; i < std::min(bn + 1, block_len); i += (-i & i))
                    blk_max[i] = blk_max[bi];
        }
    }

    // 現在の要素数を返す。O(1)
    std::size_t size() const { return n; }

    // 集合が空なら true を返す。O(1)
    bool empty() const { return n == 0; }

    // 最小要素への参照を返す。空の場合の動作は未定義。O(1)
    const T& min() const { return blocks[1].front(); }

    // 最大要素への参照を返す。空の場合の動作は未定義。O(1)
    const T& max() const { return blocks[block_len - 1].back(); }

    // 昇順に全要素へ callback を適用する。bool 変換可能な戻り値が true なら中断する。O(n)
    template <class F>
    void for_each(F&& callback) const {
        for_each_impl<false>(callback);
    }

    // 降順に全要素へ callback を適用する。bool 変換可能な戻り値が true なら中断する。O(n)
    template <class F>
    void for_each_r(F&& callback) const {
        for_each_impl<true>(callback);
    }

    // 0-indexed で k 番目に小さい要素を返す。O(log n)
    T operator[](std::size_t k) const {
        std::size_t bi = 0;
        if (block_len > 1) {
            const std::size_t top = floor_log2_size(block_len - 1);
            for (std::size_t i = std::size_t(1) << top; i; i >>= 1)
                if (((bi | i) < block_len) && (k >= bit_cnt[bi | i]))
                    k -= (bit_cnt[bi |= i]);
        }

        return blocks[bi + 1][k];
    }

    // x より小さい要素数を返す。O(log n)
    std::size_t rank(T x) const {
        if (seg_size.empty()) return 0;

        auto [bi, it] = this->template bound<false>(x);
        std::size_t rk = it - blocks[bi].begin();
        for (std::size_t i = bi - 1; i; i ^= (-i & i)) rk += bit_cnt[i];
        return rk;
    }

    // x 以下の要素数を返す。O(log n)
    std::size_t rank2(T x) const {
        if (seg_size.empty()) return 0;

        auto [bi, it] = this->template bound<true>(x);
        std::size_t rk = it - blocks[bi].begin();
        for (std::size_t i = bi - 1; i; i ^= (-i & i)) rk += bit_cnt[i];
        return rk;
    }

    // x が存在するなら true を返す。O(log n)
    bool contains(T x) const {
        if (seg_size.empty()) return false;
        auto [bi, it] = this->template bound<false>(x);
        return it != blocks[bi].end() && *it == x;
    }

    // x が存在するかを 0 または 1 で返す。O(log n)
    std::size_t count(T x) const { return contains(x); }

    // x 以下の最大要素を返す。存在しない場合は std::nullopt。O(log n)
    std::optional<T> less_equal(T x) const {
        if (seg_size.empty()) return std::nullopt;
        auto [bi, it] = this->template bound<true>(x);
        if (!(bi > 1 || (bi == 1 && it > blocks[bi].begin()))) return std::nullopt;
        if (it > blocks[bi].begin()) return *std::prev(it);
        if ((bi & (load2x - 1)) != 1) return blocks[bi - 1].back();
        const std::size_t segi = (bi - 1) >> log;
        bi = ((segi - 1) << log) | seg_size[segi - 1];
        return blocks[bi].back();
    }

    // x 以上の最小要素を返す。存在しない場合は std::nullopt。O(log n)
    std::optional<T> greater_equal(T x) const {
        if (seg_size.empty()) return std::nullopt;
        auto [bi, it] = this->template bound<false>(x);
        if (it == blocks[bi].end()) return std::nullopt;
        return *it;
    }

private:
    static std::size_t floor_log2_size(const std::size_t x) { return static_cast<std::size_t>(63 - __builtin_clzll(x)); }

    static constexpr std::size_t load1x = load1 * 2;
    static constexpr std::size_t load2x = load2 * 2;
    static constexpr std::size_t log = static_cast<std::size_t>(std::bit_width(load2x) - 1);

    std::vector<std::vector<T>> blocks;
    std::vector<std::size_t> bit_cnt;
    std::vector<std::optional<T>> blk_max;
    std::vector<std::size_t> seg_size;
    std::size_t n = 0;
    std::size_t block_len = 0;

    void ensure_storage(const std::size_t sz) {
        if (blocks.size() < sz) blocks.resize(sz);
        if (bit_cnt.size() < sz) bit_cnt.resize(sz);
        if (blk_max.size() < sz) blk_max.resize(sz);
    }

    void make_sentinel() {
        if (block_len != 0) return;
        ensure_storage(1);
        blocks[0].clear();
        bit_cnt[0] = 0;
        blk_max[0].reset();
        block_len = 1;
    }

    void reset_to_sentinel() {
        ensure_storage(1);
        blocks[0].clear();
        bit_cnt[0] = 0;
        blk_max[0].reset();
        seg_size.clear();
        block_len = 1;
        n = 0;
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
            for (std::size_t segi = seg_size.size(); segi-- > 0;) {
                const std::size_t first = (segi << log) | 1;
                const std::size_t last = (segi << log) | seg_size[segi];
                for (std::size_t bi = last + 1; bi-- > first;)
                    for (auto it = blocks[bi].rbegin(); it != blocks[bi].rend(); ++it)
                        if (invoke_callback(callback, *it)) return;
            }
        } else {
            for (std::size_t segi = 0; segi < seg_size.size(); ++segi) {
                const std::size_t first = (segi << log) | 1;
                const std::size_t last = (segi << log) | seg_size[segi];
                for (std::size_t bi = first; bi <= last; ++bi)
                    for (const T& x : blocks[bi])
                        if (invoke_callback(callback, x)) return;
            }
        }
    }

    template <bool Upper>
    std::pair<std::size_t, const_iterator> bound(T x) const {
        if (seg_size.empty()) return {0, const_iterator{}};

        std::size_t bi = 0;
        if (block_len > 1) {
            const std::size_t top = floor_log2_size(block_len - 1);
            for (std::size_t i = std::size_t(1) << top; i; i >>= 1) {
                const std::size_t idx = bi | i;
                if (idx < block_len && blk_max[idx].has_value() &&
                    (Upper ? blk_max[idx].value() <= x : blk_max[idx].value() < x))
                    bi |= i;
            }
        }

        if (bi + 1 < block_len) ++bi;
        auto it = Upper ? std::upper_bound(blocks[bi].begin(), blocks[bi].end(), x)
                        : std::lower_bound(blocks[bi].begin(), blocks[bi].end(), x);
        return {bi, it};
    }

    void range_bit_modify(const std::size_t b1, const std::size_t b2) {
        std::fill(bit_cnt.data() + b1, bit_cnt.data() + b2 + 1, 0);

        std::optional<T> mx;
        for (std::size_t i = b1; i <= b2; ++i) {
            if (!blocks[i].empty()) {
                bit_cnt[i] += blocks[i].size();
                blk_max[i] = mx = blocks[i].back();
            } else {
                blk_max[i] = mx;
            }

            if (const std::size_t next = i + (-i & i); (next <= b2) && bit_cnt[i])
                bit_cnt[next] += bit_cnt[i];
        }

        for (std::size_t lowb = ((-b2) & b2) / 2; lowb >= load2x; lowb >>= 1)
            bit_cnt[b2] += bit_cnt[b2 - lowb];
    }

    void expand() {
        std::vector<std::vector<T>> blocks_old = std::move(blocks);
        const std::size_t old_block_len = block_len;

        std::size_t c = 0;
        for (std::size_t i = 0; i < old_block_len; ++i)
            if (!blocks_old[i].empty()) ++c;

        const std::size_t segn = (c + load2 - 1) / load2;
        blocks.reserve(segn * load2x + 1);
        bit_cnt.reserve(segn * load2x + 1);
        blk_max.reserve(segn * load2x + 1);

        const std::size_t ec = n;
        reset_to_sentinel();

        n = ec;
        seg_size.assign(segn, 0);

        std::size_t i = 0;
        for (std::size_t old_i = 0; old_i < old_block_len; ++old_i) {
            auto& block = blocks_old[old_i];
            if (!block.empty()) {
                auto mx = block.back();
                ++seg_size[i >> (log - 1)];
                ensure_storage(block_len + 1);
                blocks[block_len] = std::move(block);
                bit_cnt[block_len] = blocks[block_len].size();
                blk_max[block_len] = mx;
                ++block_len;
                ++i;

                if (((i & (load2 - 1)) == 0) && (i < c)) {
                    const std::size_t old_len = block_len;
                    ensure_storage(block_len + load2);
                    for (std::size_t j = old_len; j < old_len + load2; ++j) {
                        blocks[j].clear();
                        bit_cnt[j] = 0;
                        blk_max[j] = mx;
                    }
                    block_len += load2;
                }
            }
        }

        for (std::size_t j = 1; j < block_len; ++j)
            if (const std::size_t next = j + (-j & j); next < block_len)
                bit_cnt[next] += bit_cnt[j];
    }
};


#if __INCLUDE_LEVEL__ == 0
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <sstream>
#include <string>

namespace btree_set_self_test {

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

std::vector<int> candidate_values(const std::set<int>& ss) {
    std::vector<int> qs = {
        std::numeric_limits<int>::min(), std::numeric_limits<int>::min() + 1,
        -1000000007, -1000, -101, -100, -99, -10, -1, 0, 1, 2, 10,
        99, 100, 101, 1000, 1000000007,
        std::numeric_limits<int>::max() - 1, std::numeric_limits<int>::max()
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
void check_all_apis(const Set& xs, const std::set<int>& ss, const std::string& label) {
    const std::vector<int> expected(ss.begin(), ss.end());
    std::vector<int> expected_r = expected;
    std::reverse(expected_r.begin(), expected_r.end());

    require_eq(xs.size(), ss.size(), label + ": size");
    require_eq(xs.empty(), ss.empty(), label + ": empty");
    if (!ss.empty()) {
        require_eq(xs.min(), *ss.begin(), label + ": min");
        require_eq(xs.max(), *ss.rbegin(), label + ": max");
    }
    require_vec_eq(collect_forward(xs), expected, label + ": for_each order");
    require_vec_eq(collect_reverse(xs), expected_r, label + ": for_each_r order");

    for (std::size_t k = 0; k < expected.size(); ++k) {
        require_eq(xs[k], expected[k], label + ": operator[]");
    }

    for (int q : candidate_values(ss)) {
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
    btree_set<int, 4, 2> xs;
    std::set<int> ss;
    check_all_apis(xs, ss, "empty default");

    int calls = 0;
    xs.for_each([&](const int&) { ++calls; });
    xs.for_each_r([&](const int&) { ++calls; return true; });
    require_eq(calls, 0, "empty iteration callbacks are not called");

    xs.erase(123);
    xs.clear();
    check_all_apis(xs, ss, "empty after erase/clear");

    xs.insert(10);
    ss.insert(10);
    check_all_apis(xs, ss, "singleton insert");

    xs.insert(10);
    ss.insert(10);
    xs.erase(11);
    ss.erase(11);
    check_all_apis(xs, ss, "singleton duplicate and missing erase");

    xs.erase(10);
    ss.erase(10);
    check_all_apis(xs, ss, "singleton erased");

    xs.insert(-5);
    ss.insert(-5);
    xs.clear();
    ss.clear();
    check_all_apis(xs, ss, "clear nonempty");

    xs.insert(7);
    ss.insert(7);
    check_all_apis(xs, ss, "reuse after clear");
}

void test_constructor_duplicates_and_boundaries() {
    std::vector<int> init = {
        5, 3, 5, -1, 0, 3, std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max(), 42, 42, -1
    };
    btree_set<int, 4, 2> xs(init);
    std::set<int> ss(init.begin(), init.end());
    check_all_apis(xs, ss, "constructor duplicates and int boundaries");

    btree_set<int, 4, 2> empty_from_vector(std::vector<int>{});
    check_all_apis(empty_from_vector, std::set<int>{}, "empty vector constructor");
}

void test_iteration_break_modes() {
    std::vector<int> init = {-8, -3, -1, 0, 2, 4, 9, 20};
    btree_set<int, 4, 2> xs(init);
    std::set<int> ss(init.begin(), init.end());
    check_all_apis(xs, ss, "iteration source");

    std::vector<int> got;
    xs.for_each([&](const int& x) -> bool {
        got.push_back(x);
        return x >= 2;
    });
    require_vec_eq(got, std::vector<int>({-8, -3, -1, 0, 2}), "for_each bool break");

    got.clear();
    xs.for_each([&](const int& x) -> int {
        got.push_back(x);
        return x == 4;
    });
    require_vec_eq(got, std::vector<int>({-8, -3, -1, 0, 2, 4}), "for_each int break");

    got.clear();
    xs.for_each([&](const int& x) -> std::optional<int> {
        got.push_back(x);
        if (x == 0) return 1;
        return std::nullopt;
    });
    require_vec_eq(got, std::vector<int>({-8, -3, -1, 0}), "for_each optional break");

    got.clear();
    xs.for_each([&](const int& x) -> const int* {
        got.push_back(x);
        return x == 9 ? &got.back() : nullptr;
    });
    require_vec_eq(got, std::vector<int>({-8, -3, -1, 0, 2, 4, 9}), "for_each pointer break");

    got.clear();
    xs.for_each_r([&](const int& x) -> bool {
        got.push_back(x);
        return x <= 0;
    });
    require_vec_eq(got, std::vector<int>({20, 9, 4, 2, 0}), "for_each_r bool break");

    got.clear();
    xs.for_each_r([&](const int& x) -> void { got.push_back(x); });
    std::vector<int> expected_r(init.rbegin(), init.rend());
    require_vec_eq(got, expected_r, "for_each_r void full scan");
}

void test_deterministic_updates() {
    btree_set<int, 4, 2> xs;
    std::set<int> ss;
    const std::vector<int> values = {
        0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6, -6, 7, -7,
        8, -8, 9, -9, 10, -10, 11, -11, 12, -12, 13, -13
    };
    for (int x : values) {
        xs.insert(x);
        ss.insert(x);
        check_all_apis(xs, ss, "deterministic insert");
    }
    for (int x : {0, 13, -13, 100, -100, 5, -5, 1, -1, 12, -12, 2, -2}) {
        xs.erase(x);
        ss.erase(x);
        check_all_apis(xs, ss, "deterministic erase/missing erase");
    }
    for (int x : values) {
        xs.insert(x);
        ss.insert(x);
    }
    check_all_apis(xs, ss, "deterministic refill");
}

void test_small_load_random_exhaustive() {
    btree_set<int, 4, 2> xs;
    std::set<int> ss;
    std::mt19937 rng(123456789u);
    std::uniform_int_distribution<int> value_dist(-90, 90);
    std::uniform_int_distribution<int> op_dist(0, 11);

    for (int step = 0; step < 100000; ++step) {
        const int op = op_dist(rng);
        const int x = value_dist(rng);
        if (op <= 2) {
            xs.insert(x);
            ss.insert(x);
        } else if (op <= 5) {
            xs.erase(x);
            ss.erase(x);
        } else if (op == 6) {
            require_eq(xs.count(x), ss.count(x), "small random count");
        } else if (op == 7) {
            const std::vector<int> v(ss.begin(), ss.end());
            const std::size_t expected = static_cast<std::size_t>(
                std::lower_bound(v.begin(), v.end(), x) - v.begin());
            require_eq(xs.rank(x), expected, "small random rank");
        } else if (op == 8) {
            const std::vector<int> v(ss.begin(), ss.end());
            const std::size_t expected = static_cast<std::size_t>(
                std::upper_bound(v.begin(), v.end(), x) - v.begin());
            require_eq(xs.rank2(x), expected, "small random rank2");
        } else if (op == 9) {
            require_eq_opt(xs.less_equal(x), set_less_equal(ss, x), "small random less_equal");
        } else if (op == 10) {
            require_eq_opt(xs.greater_equal(x), set_greater_equal(ss, x), "small random greater_equal");
        } else {
            if (!ss.empty()) {
                require_eq(xs.min(), *ss.begin(), "small random min");
                require_eq(xs.max(), *ss.rbegin(), "small random max");
            }
            if (!ss.empty()) {
                std::uniform_int_distribution<std::size_t> kth_dist(0, ss.size() - 1);
                const std::size_t k = kth_dist(rng);
                const std::vector<int> v(ss.begin(), ss.end());
                require_eq(xs[k], v[k], "small random kth");
            }
        }
        if ((step % 257) == 0) check_all_apis(xs, ss, "small random periodic full check");
    }
    check_all_apis(xs, ss, "small random final full check");
}

void check_large_sample(const btree_set<int>& xs, const std::set<int>& ss, std::mt19937& rng,
                        const std::string& label) {
    const std::vector<int> v(ss.begin(), ss.end());
    require_eq(xs.size(), ss.size(), label + ": size");
    if (!ss.empty()) {
        require_eq(xs.min(), *ss.begin(), label + ": min");
        require_eq(xs.max(), *ss.rbegin(), label + ": max");
    }
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

    std::uniform_int_distribution<int> query_dist(-250000, 250000);
    std::vector<int> qs = {
        std::numeric_limits<int>::min(), std::numeric_limits<int>::min() + 1,
        -250001, -250000, -1, 0, 1, 250000, 250001,
        std::numeric_limits<int>::max() - 1, std::numeric_limits<int>::max()
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
    btree_set<int> xs;
    std::set<int> ss;
    std::mt19937 rng(987654321u);
    std::uniform_int_distribution<int> value_dist(-200000, 200000);
    std::uniform_int_distribution<int> op_dist(0, 9);

    constexpr int operations = 300000;
    for (int step = 1; step <= operations; ++step) {
        const int op = op_dist(rng);
        const int x = value_dist(rng);
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
            check_large_sample(xs, ss, rng, "large random sample step " + std::to_string(step));
        }
    }
    check_large_sample(xs, ss, rng, "large random final");
}

void run_all_tests() {
    test_empty_and_singleton();
    test_constructor_duplicates_and_boundaries();
    test_iteration_break_modes();
    test_deterministic_updates();
    test_small_load_random_exhaustive();
    test_large_random_against_std_set();
}

} // namespace btree_set_self_test


namespace btree_set_self_benchmark {

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
    std::size_t btree_ops = 0;
    std::size_t std_ops = 0;
    double btree_ms = 0.0;
    double std_ms = -1.0;
    u64 btree_checksum = 0;
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
        std::vector<btree_set<u32>> v(q);
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

BenchRow bench_construct_vector(std::size_t n, u64 seed) {
    const auto vals = shuffled_unique_values(n, 2, seed);
    u64 cb = 0, cs = 0;
    Timer tb;
    btree_set<u32> bs(vals);
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
    std::vector<btree_set<u32>> bv;
    std::vector<std::set<u32>> sv;
    bv.reserve(q);
    sv.reserve(q);
    for (std::size_t i = 0; i < q; ++i) {
        bv.emplace_back(vals);
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
    btree_set<u32> bs;
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
    btree_set<u32> bs(init);
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
    btree_set<u32> bs(init);
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
    btree_set<u32> bs(init);
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
    btree_set<u32> bs(init);
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
    btree_set<u32> bs(init);
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
    btree_set<u32> bs(init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cb += bs.empty();
    }
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cs += ss.empty();
    }
    const double ms = ts.ms();
    return {"empty", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_min(std::size_t n, std::size_t q, u64 seed) {
    const auto init = shuffled_unique_values(n, 2, seed);
    btree_set<u32> bs(init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        const auto& y = bs.min();
        cb += y + 1u;
    }
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cs += *ss.begin() + 1u;
    }
    const double ms = ts.ms();
    return {"min", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_max(std::size_t n, std::size_t q, u64 seed) {
    const auto init = shuffled_unique_values(n, 2, seed);
    btree_set<u32> bs(init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        const auto& y = bs.max();
        cb += y + 1u;
    }
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cs += *ss.rbegin() + 1u;
    }
    const double ms = ts.ms();
    return {"max", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_for_each(std::size_t n, u64 seed) {
    const auto init = shuffled_unique_values(n, 2, seed);
    btree_set<u32> bs(init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    bs.for_each([&](const u32& x) { cb += x + 1u; });
    const double mb = tb.ms();
    Timer ts;
    for (const u32& x : ss) cs += x + 1u;
    const double ms = ts.ms();
    return {"for_each", n, n, n, n, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_for_each_r(std::size_t n, u64 seed) {
    const auto init = shuffled_unique_values(n, 2, seed);
    btree_set<u32> bs(init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    bs.for_each_r([&](const u32& x) { cb += x + 1u; });
    const double mb = tb.ms();
    Timer ts;
    for (auto it = ss.rbegin(); it != ss.rend(); ++it) cs += *it + 1u;
    const double ms = ts.ms();
    return {"for_each_r", n, n, n, n, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_for_each_break(std::size_t n, u64 seed) {
    const auto init = shuffled_unique_values(n, 2, seed);
    btree_set<u32> bs(init);
    std::set<u32> ss(init.begin(), init.end());
    const u32 limit = static_cast<u32>(n);
    u64 cb = 0, cs = 0;
    std::size_t bv = 0, sv = 0;
    Timer tb;
    bs.for_each([&](const u32& x) -> bool {
        ++bv;
        cb += x + 1u;
        return x >= limit;
    });
    const double mb = tb.ms();
    Timer ts;
    for (const u32& x : ss) {
        ++sv;
        cs += x + 1u;
        if (x >= limit) break;
    }
    const double ms = ts.ms();
    return {"for_each(break)", n, sv, bv, sv, mb, ms, cb, cs, (cb == cs && bv == sv) ? "ok" : "wrong"};
}

BenchRow bench_for_each_r_break(std::size_t n, u64 seed) {
    const auto init = shuffled_unique_values(n, 2, seed);
    btree_set<u32> bs(init);
    std::set<u32> ss(init.begin(), init.end());
    const u32 limit = static_cast<u32>(n);
    u64 cb = 0, cs = 0;
    std::size_t bv = 0, sv = 0;
    Timer tb;
    bs.for_each_r([&](const u32& x) -> bool {
        ++bv;
        cb += x + 1u;
        return x <= limit;
    });
    const double mb = tb.ms();
    Timer ts;
    for (auto it = ss.rbegin(); it != ss.rend(); ++it) {
        ++sv;
        cs += *it + 1u;
        if (*it <= limit) break;
    }
    const double ms = ts.ms();
    return {"for_each_r(break)", n, sv, bv, sv, mb, ms, cb, cs, (cb == cs && bv == sv) ? "ok" : "wrong"};
}

BenchRow bench_count(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(4 * n), seed);
    btree_set<u32> bs(init);
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
    btree_set<u32> bs(init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : qs) cb += bs.contains(x);
    const double mb = tb.ms();
    Timer ts;
    for (u32 x : qs) cs += ss.contains(x);
    const double ms = ts.ms();
    return {"contains", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_less_equal(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(4 * n), seed);
    btree_set<u32> bs(init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : qs) {
        auto y = bs.less_equal(x);
        cb += y ? (*y + 1u) : 0u;
    }
    const double mb = tb.ms();
    Timer ts;
    for (u32 x : qs) {
        auto it = ss.upper_bound(x);
        if (it != ss.begin()) {
            --it;
            cs += *it + 1u;
        }
    }
    const double ms = ts.ms();
    return {"less_equal", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_greater_equal(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(4 * n), seed);
    btree_set<u32> bs(init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : qs) {
        auto y = bs.greater_equal(x);
        cb += y ? (*y + 1u) : 0u;
    }
    const double mb = tb.ms();
    Timer ts;
    for (u32 x : qs) {
        auto it = ss.lower_bound(x);
        cs += it == ss.end() ? 0u : (*it + 1u);
    }
    const double ms = ts.ms();
    return {"greater_equal", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_operator_index(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_indices(q, n, seed);
    btree_set<u32> bs(init);
    u64 cb = 0;
    Timer tb;
    for (std::size_t k : qs) cb += bs[k];
    const double mb = tb.ms();
    return {"operator[]", n, q, q, 0, mb, -1.0, cb, 0, "btree_only"};
}

BenchRow bench_rank(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(4 * n), seed);
    btree_set<u32> bs(init);
    u64 cb = 0;
    Timer tb;
    for (u32 x : qs) cb += bs.rank(x);
    const double mb = tb.ms();
    return {"rank", n, q, q, 0, mb, -1.0, cb, 0, "btree_only"};
}

BenchRow bench_rank2(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(4 * n), seed);
    btree_set<u32> bs(init);
    u64 cb = 0;
    Timer tb;
    for (u32 x : qs) cb += bs.rank2(x);
    const double mb = tb.ms();
    return {"rank2", n, q, q, 0, mb, -1.0, cb, 0, "btree_only"};
}

template <class F>
BenchRow median_of(int reps, F&& f) {
    std::vector<BenchRow> rows;
    rows.reserve(static_cast<std::size_t>(reps));
    for (int i = 0; i < reps; ++i) rows.push_back(f(i));
    BenchRow out = rows.front();

    auto b_rows = rows;
    std::nth_element(b_rows.begin(), b_rows.begin() + reps / 2, b_rows.end(),
                     [](const BenchRow& a, const BenchRow& b) { return a.btree_ms < b.btree_ms; });
    out.btree_ms = b_rows[reps / 2].btree_ms;
    out.btree_ops = b_rows[reps / 2].btree_ops;

    if (rows.front().std_ms >= 0.0) {
        auto s_rows = rows;
        std::nth_element(s_rows.begin(), s_rows.begin() + reps / 2, s_rows.end(),
                         [](const BenchRow& a, const BenchRow& b) { return a.std_ms < b.std_ms; });
        out.std_ms = s_rows[reps / 2].std_ms;
        out.std_ops = s_rows[reps / 2].std_ops;
    }

    bool ok = true;
    for (const auto& r : rows) {
        if (r.status == "wrong") ok = false;
        if (r.std_ms >= 0.0 && r.btree_checksum != r.std_checksum) ok = false;
    }
    if (out.status != "btree_only") out.status = ok ? "ok" : "wrong";
    out.btree_checksum = rows.back().btree_checksum;
    out.std_checksum = rows.back().std_checksum;
    return out;
}

void run_all_benchmarks() {
    constexpr int reps = 3;
    constexpr std::size_t n = 200000;
    constexpr std::size_t q = 500000;
    constexpr std::size_t q_fast = 5000000;
    constexpr std::size_t n_small = 50000;
    constexpr std::size_t q_small = 100000;

    std::vector<BenchRow> rows;
    rows.push_back(median_of(reps, [&](int) { return bench_default_construct(q_small); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_construct_vector(n, 100 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_clear(100000, 5, 200 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_insert_unique_empty(n, 300 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_insert_missing(n, q, 400 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_insert_duplicate(n, q, 500 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_erase_present(n, 600 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_erase_missing(n, q, 700 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_size(n, q_fast, 800 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_empty(n, q_fast, 850 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_min(n, q_fast, 900 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_max(n, q_fast, 1000 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_for_each(n, 1100 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_for_each_r(n, 1200 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_for_each_break(n, 1300 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_for_each_r_break(n, 1400 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_operator_index(n_small, q_small, 1500 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_rank(n, q, 1600 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_rank2(n, q, 1700 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_contains(n, q, 1800 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_count(n, q, 1800 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_less_equal(n, q, 1900 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_greater_equal(n, q, 2000 + i); }));

    std::cout << "benchmarks begin\n";
    std::cout << "compiler," << __VERSION__ << "\n";
    std::cout << "flags,-std=c++20 -O2 -march=native -DNDEBUG\n";
    std::cout << "repetitions," << reps << "\n";
    std::cout << "api,N,Q,btree_ms,std_set_ms,btree_ns_per_op,std_set_ns_per_op,std_over_btree,status\n";
    std::cout << std::fixed << std::setprecision(3);
    for (const auto& r : rows) {
        const double b_ns = r.btree_ms * 1e6 / static_cast<double>(std::max<std::size_t>(1, r.btree_ops));
        std::cout << r.api << ',' << r.n << ',' << r.q << ',' << r.btree_ms << ',';
        if (r.std_ms >= 0.0) {
            const double s_ns = r.std_ms * 1e6 / static_cast<double>(std::max<std::size_t>(1, r.std_ops));
            const double ratio = r.std_ms / r.btree_ms;
            std::cout << r.std_ms << ',' << b_ns << ',' << s_ns << ',' << ratio << ',';
        } else {
            std::cout << "NA," << b_ns << ",NA,NA,";
        }
        std::cout << r.status << '\n';
        benchmark_sink ^= r.btree_checksum + 0x9e3779b97f4a7c15ULL + (benchmark_sink << 6) + (benchmark_sink >> 2);
        benchmark_sink ^= r.std_checksum + 0x9e3779b97f4a7c15ULL + (benchmark_sink << 6) + (benchmark_sink >> 2);
    }
    std::cerr << "benchmark_sink=" << benchmark_sink << '\n';
}

} // namespace btree_set_self_benchmark

int main() {
    using namespace btree_set_self_test;

    test_empty_and_singleton();
    std::cout << "edge_empty_singleton_clear: ok\n";

    test_constructor_duplicates_and_boundaries();
    std::cout << "edge_constructor_duplicates_boundaries: ok\n";

    test_iteration_break_modes();
    std::cout << "edge_iteration_callbacks: ok\n";

    test_deterministic_updates();
    std::cout << "deterministic_update_edges: ok\n";

    test_small_load_random_exhaustive();
    std::cout << "random_small_load_full_check_100000_ops: ok\n";

    test_large_random_against_std_set();
    std::cout << "random_default_load_sampled_300000_ops: ok\n";

    std::cout << "all tests passed\n";

    btree_set_self_benchmark::run_all_benchmarks();
    return 0;
}
#endif

// 実行結果(atcoder)
// edge_empty_singleton_clear: ok
// edge_constructor_duplicates_boundaries: ok
// edge_iteration_callbacks: ok
// deterministic_update_edges: ok
// random_small_load_full_check_100000_ops: ok
// random_default_load_sampled_300000_ops: ok
// all tests passed
// benchmarks begin
// compiler,15.2.0
// flags,-std=c++20 -O2 -march=native -DNDEBUG
// repetitions,3
// api,N,Q,btree_ms,std_set_ms,btree_ns_per_op,std_set_ns_per_op,std_over_btree,status
// default_construct,0,100000,1.975,0.163,19.748,1.635,0.083,ok
// construct(vector),200000,1,11.303,36.901,11302969.000,36900771.000,3.265,ok
// clear,100000,5,0.012,23.111,2310.600,4622247.200,2000.453,ok
// insert(unique,empty),200000,200000,18.894,36.519,94.468,182.595,1.933,ok
// insert(missing),200000,500000,60.999,182.791,121.999,365.583,2.997,ok
// insert(duplicate),200000,500000,38.420,74.852,76.841,149.704,1.948,ok
// erase(present),200000,200000,17.995,53.788,89.977,268.938,2.989,ok
// erase(missing),200000,500000,22.499,74.395,44.997,148.791,3.307,ok
// size,200000,5000000,1.419,1.332,0.284,0.266,0.939,ok
// empty,200000,5000000,1.357,1.324,0.271,0.265,0.976,ok
// min,200000,5000000,1.625,1.325,0.325,0.265,0.815,ok
// max,200000,5000000,2.748,13.561,0.550,2.712,4.934,ok
// for_each,200000,200000,0.061,8.818,0.305,44.088,144.592,ok
// for_each_r,200000,200000,0.056,8.628,0.278,43.140,155.195,ok
// for_each(break),200000,100001,0.054,4.444,0.541,44.444,82.214,ok
// for_each_r(break),200000,100000,0.051,4.338,0.515,43.376,84.276,ok
// operator[],50000,100000,0.983,NA,9.828,NA,NA,btree_only
// rank,200000,500000,27.123,NA,54.246,NA,NA,btree_only
// rank2,200000,500000,26.852,NA,53.704,NA,NA,btree_only
// contains,200000,500000,23.848,100.198,47.696,200.396,4.201,ok
// count,200000,500000,24.473,99.414,48.945,198.829,4.062,ok
// less_equal,200000,500000,23.933,101.027,47.866,202.054,4.221,ok
// greater_equal,200000,500000,23.835,97.844,47.670,195.687,4.105,ok
