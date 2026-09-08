/*
 * btree_multiset_cnt: 競技プログラミング向けの順序付き multiset 実装。
 * 同じ key を Entry{key, cnt} として圧縮して複数のソート済み vector ブロックに保持し、ブロック最大値と
 * Fenwick Tree 風の累積個数で lower_bound / upper_bound / rank / kth を高速に処理する。size や rank は重複込みで数える。
 * erase(x) は同じ key を全削除し、erase_one(x) は 1 要素だけ削除する。
 *
 * テンプレートパラメータ:
 *   T     : 要素型。operator< で比較できる型を想定。
 *   load1 : 1 ブロックに入れる distinct key 数の目安。大きいほど検索寄り、小さいほど新規 key の insert / erase 寄り。
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
struct btree_multiset_cnt {
private:
    struct Entry {
        T key;
        std::size_t cnt;
    };

    using const_iterator = typename std::vector<Entry>::const_iterator;

public:
    // 空の multiset を遅延初期化で構築する。O(1)
    btree_multiset_cnt() = default;

    // 配列から重複を圧縮した順序付き multiset を構築する。O(n log n)
    explicit btree_multiset_cnt(std::vector<T> a) {
        std::sort(a.begin(), a.end());
        n = a.size();
        if (n == 0) return;

        std::vector<Entry> entries;
        entries.reserve(a.size());
        for (const T& x : a) {
            if (entries.empty() || !key_equal(entries.back().key, x)) {
                entries.push_back(Entry{x, 1});
            } else {
                ++entries.back().cnt;
            }
        }

        blocks.reserve(entries.size() / load1 + 2);
        bit_cnt.reserve(entries.size() / load1 + 2);
        blk_max.reserve(entries.size() / load1 + 2);
        make_sentinel();

        for (std::size_t l = 0; l < entries.size(); l += load1) {
            const std::size_t r = std::min(l + load1, entries.size());
            ensure_storage(block_len + 1);
            blocks[block_len].assign(entries.begin() + static_cast<std::ptrdiff_t>(l),
                                     entries.begin() + static_cast<std::ptrdiff_t>(r));
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

    // 要素 x を 1 個挿入する。O(log d + load1 + load2)
    void insert(T x) {
        if (seg_size.empty()) {
            make_sentinel();
            ensure_storage(2);
            blocks[1].clear();
            blocks[1].push_back(Entry{std::move(x), 1});
            bit_cnt[1] = 1;
            blk_max[1] = blocks[1].back().key;
            block_len = 2;
            ++n;
            seg_size.assign(1, 1);
            return;
        }

        auto [bi, it] = this->template bound<false>(x);
        if (it != blocks[bi].end() && key_equal(it->key, x)) {
            ++blocks[bi][static_cast<std::size_t>(it - blocks[bi].begin())].cnt;
            add_block_count(bi, 1);
            ++n;
            return;
        }

        ++n;
        add_block_count(bi, 1);

        const std::size_t segi = (bi - 1) >> log;
        const std::size_t bj = (segi << log) | seg_size[segi];
        const std::size_t bn = (segi + 1) << log;
        const bool x_in_block_end = (it == blocks[bi].end());
        blocks[bi].insert(it, Entry{std::move(x), 1});

        if (blocks[bi].size() >= load1x) {
            if (block_len <= bn) {
                ensure_storage(block_len + 1);
                for (std::size_t idx = block_len; idx > bi + 1; --idx)
                    blocks[idx] = std::move(blocks[idx - 1]);
                blocks[bi + 1].assign(blocks[bi].begin() + static_cast<std::ptrdiff_t>(load1),
                                      blocks[bi].end());
                bit_cnt[block_len] = 0;
                blk_max[block_len].reset();
                ++block_len;
            } else {
                blocks[bj + 1].clear();
                for (std::size_t idx = bj + 1; idx > bi + 1; --idx)
                    blocks[idx] = std::move(blocks[idx - 1]);
                blocks[bi + 1].assign(blocks[bi].begin() + static_cast<std::ptrdiff_t>(load1),
                                      blocks[bi].end());
            }
            blocks[bi].resize(load1);
            if (++seg_size[segi] == load2x)
                expand();
            else
                range_bit_modify((segi << log) | 1, std::min(bn, block_len - 1));
        } else if (x_in_block_end) {
            blk_max[bi] = blocks[bi].back().key;
            if (bi == bj)
                for (std::size_t i = bi; i < std::min(bj + 1, block_len); i += -i & i)
                    blk_max[i] = blk_max[bi];
        }
    }

    // x と等しい要素を全て削除する。存在しない場合は何もしない。O(log d + load1 + load2)
    void erase(T x) { erase_impl<false>(x); }

    // x と等しい要素を 1 個だけ削除する。存在しない場合は何もしない。O(log d + load1 + load2)
    void erase_one(T x) { erase_impl<true>(x); }

    // 現在の重複込み要素数を返す。O(1)
    std::size_t size() const { return n; }

    // multiset が空なら true を返す。O(1)
    bool empty() const { return n == 0; }

    // 最小要素への参照を返す。空の場合の動作は未定義。O(1)
    const T& min() const { return blocks[1].front().key; }

    // 最大要素への参照を返す。空の場合の動作は未定義。O(1)
    const T& max() const { return blocks[block_len - 1].back().key; }

    // 昇順に重複込み全要素へ callback を適用する。bool 変換可能な戻り値が true なら中断する。O(n)
    template <class F>
    void for_each(F&& callback) const {
        for_each_impl<false>(callback);
    }

    // 降順に重複込み全要素へ callback を適用する。bool 変換可能な戻り値が true なら中断する。O(n)
    template <class F>
    void for_each_r(F&& callback) const {
        for_each_impl<true>(callback);
    }

    // 0-indexed で k 番目に小さい重複込み要素を返す。O(log d + load1)
    T operator[](std::size_t k) const {
        std::size_t bi = 0;
        if (block_len > 1) {
            const std::size_t top = floor_log2_size(block_len - 1);
            for (std::size_t i = std::size_t(1) << top; i; i >>= 1)
                if (((bi | i) < block_len) && (k >= bit_cnt[bi | i]))
                    k -= (bit_cnt[bi |= i]);
        }

        for (const Entry& e : blocks[bi + 1]) {
            if (k < e.cnt) return e.key;
            k -= e.cnt;
        }
        return blocks[bi + 1].back().key;
    }

    // x より小さい重複込み要素数を返す。O(log d + load1)
    std::size_t rank(T x) const {
        if (seg_size.empty()) return 0;

        auto [bi, it] = this->template bound<false>(x);
        std::size_t rk = block_prefix_count(blocks[bi].begin(), it);
        for (std::size_t i = bi - 1; i; i ^= (-i & i)) rk += bit_cnt[i];
        return rk;
    }

    // x 以下の重複込み要素数を返す。O(log d + load1)
    std::size_t rank2(T x) const {
        if (seg_size.empty()) return 0;

        auto [bi, it] = this->template bound<true>(x);
        std::size_t rk = block_prefix_count(blocks[bi].begin(), it);
        for (std::size_t i = bi - 1; i; i ^= (-i & i)) rk += bit_cnt[i];
        return rk;
    }

    // x が存在するなら true を返す。O(log d)
    bool contains(T x) const {
        if (seg_size.empty()) return false;
        auto [bi, it] = this->template bound<false>(x);
        return it != blocks[bi].end() && key_equal(it->key, x);
    }

    // x と等しい要素数を返す。O(log d)
    std::size_t count(T x) const {
        if (seg_size.empty()) return 0;
        auto [bi, it] = this->template bound<false>(x);
        return (it != blocks[bi].end() && key_equal(it->key, x)) ? it->cnt : 0;
    }

    // x 以下の最大要素を返す。存在しない場合は std::nullopt。O(log d)
    std::optional<T> less_equal(T x) const {
        if (seg_size.empty()) return std::nullopt;
        auto [bi, it] = this->template bound<true>(x);
        if (!(bi > 1 || (bi == 1 && it > blocks[bi].begin()))) return std::nullopt;
        if (it > blocks[bi].begin()) return std::prev(it)->key;
        if ((bi & (load2x - 1)) != 1) return blocks[bi - 1].back().key;
        const std::size_t segi = (bi - 1) >> log;
        bi = ((segi - 1) << log) | seg_size[segi - 1];
        return blocks[bi].back().key;
    }

    // x 以上の最小要素を返す。存在しない場合は std::nullopt。O(log d)
    std::optional<T> greater_equal(T x) const {
        if (seg_size.empty()) return std::nullopt;
        auto [bi, it] = this->template bound<false>(x);
        if (it == blocks[bi].end()) return std::nullopt;
        return it->key;
    }

private:
    static std::size_t floor_log2_size(const std::size_t x) { return static_cast<std::size_t>(63 - __builtin_clzll(x)); }

    static constexpr std::size_t load1x = load1 * 2;
    static constexpr std::size_t load2x = load2 * 2;
    static constexpr std::size_t log = static_cast<std::size_t>(std::bit_width(load2x) - 1);

    std::vector<std::vector<Entry>> blocks;
    std::vector<std::size_t> bit_cnt;
    std::vector<std::optional<T>> blk_max;
    std::vector<std::size_t> seg_size;
    std::size_t n = 0;
    std::size_t block_len = 0;

    static bool key_equal(const T& a, const T& b) { return !(a < b) && !(b < a); }

    static bool entry_key_less(const Entry& e, const T& x) { return e.key < x; }

    static bool key_less_entry(const T& x, const Entry& e) { return x < e.key; }

    static std::size_t block_weight(const std::vector<Entry>& block) {
        std::size_t s = 0;
        for (const Entry& e : block) s += e.cnt;
        return s;
    }

    static std::size_t block_prefix_count(const_iterator first, const_iterator last) {
        std::size_t s = 0;
        for (; first != last; ++first) s += first->cnt;
        return s;
    }

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

    void add_block_count(const std::size_t bi, const std::size_t delta) {
        for (std::size_t i = bi; i < block_len; i += (-i & i)) bit_cnt[i] += delta;
    }

    void sub_block_count(const std::size_t bi, const std::size_t delta) {
        for (std::size_t i = bi; i < block_len; i += (-i & i)) bit_cnt[i] -= delta;
    }

    template <bool One>
    void erase_impl(T x) {
        if (seg_size.empty()) return;

        auto [bi, it] = this->template bound<false>(x);
        if (it == blocks[bi].end() || !key_equal(it->key, x)) return;

        if constexpr (One) {
            if (it->cnt > 1) {
                --blocks[bi][static_cast<std::size_t>(it - blocks[bi].begin())].cnt;
                sub_block_count(bi, 1);
                --n;
                return;
            }
        }

        const std::size_t removed = it->cnt;
        n -= removed;
        sub_block_count(bi, removed);

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
            blk_max[bi] = blocks[bi].back().key;
            if (bi == bj)
                for (std::size_t i = bi; i < std::min(bn + 1, block_len); i += (-i & i))
                    blk_max[i] = blk_max[bi];
        }
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
                for (std::size_t bi = last + 1; bi-- > first;) {
                    for (auto it = blocks[bi].rbegin(); it != blocks[bi].rend(); ++it) {
                        for (std::size_t c = 0; c < it->cnt; ++c)
                            if (invoke_callback(callback, it->key)) return;
                    }
                }
            }
        } else {
            for (std::size_t segi = 0; segi < seg_size.size(); ++segi) {
                const std::size_t first = (segi << log) | 1;
                const std::size_t last = (segi << log) | seg_size[segi];
                for (std::size_t bi = first; bi <= last; ++bi) {
                    for (const Entry& e : blocks[bi]) {
                        for (std::size_t c = 0; c < e.cnt; ++c)
                            if (invoke_callback(callback, e.key)) return;
                    }
                }
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
                    (Upper ? !(x < blk_max[idx].value()) : blk_max[idx].value() < x))
                    bi |= i;
            }
        }

        if (bi + 1 < block_len) ++bi;
        auto it = Upper ? std::upper_bound(blocks[bi].begin(), blocks[bi].end(), x, key_less_entry)
                        : std::lower_bound(blocks[bi].begin(), blocks[bi].end(), x, entry_key_less);
        return {bi, it};
    }

    void range_bit_modify(const std::size_t b1, const std::size_t b2) {
        std::fill(bit_cnt.data() + b1, bit_cnt.data() + b2 + 1, 0);

        std::optional<T> mx;
        for (std::size_t i = b1; i <= b2; ++i) {
            if (!blocks[i].empty()) {
                bit_cnt[i] += block_weight(blocks[i]);
                blk_max[i] = mx = blocks[i].back().key;
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
        std::vector<std::vector<Entry>> blocks_old = std::move(blocks);
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
                auto mx = block.back().key;
                ++seg_size[i >> (log - 1)];
                ensure_storage(block_len + 1);
                blocks[block_len] = std::move(block);
                bit_cnt[block_len] = block_weight(blocks[block_len]);
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

namespace btree_multiset_cnt_self_test {

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

std::optional<int> multiset_less_equal(const std::multiset<int>& ss, int x) {
    auto it = ss.upper_bound(x);
    if (it == ss.begin()) return std::nullopt;
    --it;
    return *it;
}

std::optional<int> multiset_greater_equal(const std::multiset<int>& ss, int x) {
    auto it = ss.lower_bound(x);
    if (it == ss.end()) return std::nullopt;
    return *it;
}

std::vector<int> candidate_values(const std::multiset<int>& ss) {
    std::vector<int> qs = {
        std::numeric_limits<int>::min(), std::numeric_limits<int>::min() + 1,
        -1000000007, -1000, -101, -100, -99, -10, -1, 0, 1, 2, 10,
        99, 100, 101, 1000, 1000000007,
        std::numeric_limits<int>::max() - 1, std::numeric_limits<int>::max()
    };
    for (auto it = ss.begin(); it != ss.end();) {
        const int x = *it;
        qs.push_back(x);
        if (x != std::numeric_limits<int>::min()) qs.push_back(x - 1);
        if (x != std::numeric_limits<int>::max()) qs.push_back(x + 1);
        it = ss.upper_bound(x);
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
void check_all_apis(const Set& xs, const std::multiset<int>& ss, const std::string& label) {
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
        require_eq(xs.contains(q), ss.find(q) != ss.end(), label + ": contains");
        require_eq(xs.count(q), ss.count(q), label + ": count");
        require_eq_opt(xs.less_equal(q), multiset_less_equal(ss, q), label + ": less_equal");
        require_eq_opt(xs.greater_equal(q), multiset_greater_equal(ss, q), label + ": greater_equal");
    }
}

void test_empty_singleton_clear() {
    btree_multiset_cnt<int> xs;
    std::multiset<int> ss;
    check_all_apis(xs, ss, "empty");

    xs.insert(7);
    ss.insert(7);
    check_all_apis(xs, ss, "single insert");

    xs.insert(7);
    ss.insert(7);
    check_all_apis(xs, ss, "duplicate insert");

    xs.erase_one(7);
    ss.erase(ss.find(7));
    check_all_apis(xs, ss, "erase_one leaves one");

    xs.erase(7);
    ss.erase(7);
    check_all_apis(xs, ss, "erase all empty");

    xs.clear();
    ss.clear();
    check_all_apis(xs, ss, "clear empty");

    xs.insert(-1);
    xs.insert(-1);
    ss.insert(-1);
    ss.insert(-1);
    check_all_apis(xs, ss, "reuse after clear");
    std::cout << "empty_singleton_clear: ok\n";
}

void test_constructor_duplicates_boundaries() {
    std::vector<int> a = {
        5, 5, 5, -3, -3, 0, std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max(), std::numeric_limits<int>::min(), 42, 42, 42, 42
    };
    btree_multiset_cnt<int> xs(a);
    std::multiset<int> ss(a.begin(), a.end());
    check_all_apis(xs, ss, "constructor duplicates boundaries");

    xs.erase_one(std::numeric_limits<int>::min());
    auto it = ss.find(std::numeric_limits<int>::min());
    require(it != ss.end(), "min exists before erase_one");
    ss.erase(it);
    check_all_apis(xs, ss, "erase_one int min");

    xs.erase(std::numeric_limits<int>::max());
    ss.erase(std::numeric_limits<int>::max());
    check_all_apis(xs, ss, "erase all int max");
    std::cout << "constructor_duplicates_boundaries: ok\n";
}

void test_iteration_callbacks() {
    btree_multiset_cnt<int> xs({1, 1, 2, 3, 3, 3, 4});

    std::vector<int> got;
    xs.for_each([&](const int& x) { got.push_back(x); });
    require_vec_eq(got, std::vector<int>{1, 1, 2, 3, 3, 3, 4}, "void callback");

    got.clear();
    xs.for_each([&](const int& x) {
        got.push_back(x);
        return x == 3;
    });
    require_vec_eq(got, std::vector<int>{1, 1, 2, 3}, "bool break");

    got.clear();
    xs.for_each_r([&](const int& x) {
        got.push_back(x);
        return x == 2 ? 1 : 0;
    });
    require_vec_eq(got, std::vector<int>{4, 3, 3, 3, 2}, "int break reverse");

    got.clear();
    xs.for_each([&](const int& x) -> std::optional<int> {
        got.push_back(x);
        if (x == 2) return 1;
        return std::nullopt;
    });
    require_vec_eq(got, std::vector<int>{1, 1, 2}, "optional break");
    std::cout << "iteration_callbacks: ok\n";
}

void test_explicit_btree_set_compatible_api() {
    btree_multiset_cnt<int> xs({3, 1, 2, 1});
    const btree_multiset_cnt<int>& cx = xs;

    require(!cx.empty(), "api parity empty");
    require_eq(cx.size(), std::size_t{4}, "api parity size");
    require_eq(cx.min(), 1, "api parity min");
    require_eq(cx.max(), 3, "api parity max");
    require_eq(cx[0], 1, "api parity operator[] 0");
    require_eq(cx[1], 1, "api parity operator[] duplicate");
    require_eq(cx.rank(2), std::size_t{2}, "api parity rank");
    require_eq(cx.rank2(1), std::size_t{2}, "api parity rank2");
    require(cx.contains(1), "api parity contains true");
    require(!cx.contains(9), "api parity contains false");
    require_eq(cx.count(1), std::size_t{2}, "api parity count");
    require_eq_opt(cx.less_equal(2), std::optional<int>{2}, "api parity less_equal");
    require_eq_opt(cx.greater_equal(2), std::optional<int>{2}, "api parity greater_equal");

    std::vector<int> forward;
    cx.for_each([&](const int& x) { forward.push_back(x); });
    require_vec_eq(forward, std::vector<int>{1, 1, 2, 3}, "api parity for_each");

    std::vector<int> reverse;
    cx.for_each_r([&](const int& x) { reverse.push_back(x); });
    require_vec_eq(reverse, std::vector<int>{3, 2, 1, 1}, "api parity for_each_r");

    xs.insert(2);
    require_eq(xs.count(2), std::size_t{2}, "api parity insert duplicate");
    xs.erase_one(2);
    require_eq(xs.count(2), std::size_t{1}, "api parity erase_one");
    xs.erase(1);
    require(!xs.contains(1), "api parity erase all");
    xs.clear();
    require(xs.empty(), "api parity clear");
    std::cout << "explicit_btree_set_compatible_api: ok\n";
}

void test_deterministic_update_edges() {
    btree_multiset_cnt<int, 4, 2> xs;
    std::multiset<int> ss;

    for (int rep = 0; rep < 3; ++rep) {
        for (int x = -20; x <= 20; ++x) {
            xs.insert(x);
            ss.insert(x);
        }
    }
    check_all_apis(xs, ss, "after many duplicates");

    for (int x = -20; x <= 20; x += 2) {
        xs.erase_one(x);
        auto it = ss.find(x);
        require(it != ss.end(), "erase_one deterministic exists");
        ss.erase(it);
    }
    check_all_apis(xs, ss, "after erase_one evens");

    for (int x = -15; x <= 15; x += 3) {
        xs.erase(x);
        ss.erase(x);
    }
    check_all_apis(xs, ss, "after erase all multiples");

    for (int x = 100; x < 180; ++x) {
        xs.insert(x);
        ss.insert(x);
    }
    for (int x = 100; x < 180; x += 2) {
        xs.erase(x);
        ss.erase(x);
    }
    check_all_apis(xs, ss, "after split compact edges");
    std::cout << "deterministic_update_edges: ok\n";
}

template <class Set>
void apply_random_operation(Set& xs, std::multiset<int>& ss, std::mt19937_64& rng,
                            int value_mod, int op) {
    const int x = static_cast<int>(rng() % static_cast<std::uint64_t>(value_mod)) - value_mod / 2;
    if (op < 35) {
        xs.insert(x);
        ss.insert(x);
    } else if (op < 50) {
        xs.erase_one(x);
        auto it = ss.find(x);
        if (it != ss.end()) ss.erase(it);
    } else if (op < 62) {
        xs.erase(x);
        ss.erase(x);
    } else if (op < 66) {
        xs.clear();
        ss.clear();
    } else {
        require_eq(xs.count(x), ss.count(x), "random count quick");
        require_eq(xs.contains(x), ss.find(x) != ss.end(), "random contains quick");
        if (!ss.empty()) {
            const std::size_t k = static_cast<std::size_t>(rng() % ss.size());
            auto it = ss.begin();
            std::advance(it, static_cast<std::ptrdiff_t>(k));
            require_eq(xs[k], *it, "random kth quick");
        }
    }
}

void test_random_small_load_full_check() {
    btree_multiset_cnt<int, 4, 2> xs;
    std::multiset<int> ss;
    std::mt19937_64 rng(123456789);

    for (int step = 0; step < 100000; ++step) {
        const int op = static_cast<int>(rng() % 100);
        apply_random_operation(xs, ss, rng, 80, op);
        if (step % 100 == 0) check_all_apis(xs, ss, "random small step " + std::to_string(step));
    }
    check_all_apis(xs, ss, "random small final");
    std::cout << "random_small_load_full_check_100000_ops: ok\n";
}

void test_random_default_sampled() {
    btree_multiset_cnt<int> xs;
    std::multiset<int> ss;
    std::mt19937_64 rng(987654321);

    for (int step = 0; step < 200000; ++step) {
        const int op = static_cast<int>(rng() % 100);
        apply_random_operation(xs, ss, rng, 2000, op);
        if (step % 5000 == 0) check_all_apis(xs, ss, "random default step " + std::to_string(step));
    }
    check_all_apis(xs, ss, "random default final");
    std::cout << "random_default_sampled_200000_ops: ok\n";
}

volatile std::uint64_t bench_sink = 0;

struct BenchResult {
    std::string api;
    std::size_t n = 0;
    std::size_t q = 0;
    std::size_t btree_ops = 0;
    std::size_t std_ops = 0;
    double btree_ms = 0.0;
    double std_ms = -1.0;
    std::uint64_t btree_checksum = 0;
    std::uint64_t std_checksum = 0;
    std::string status;
};

struct SplitMix64 {
    std::uint64_t x;
    explicit SplitMix64(std::uint64_t seed) : x(seed) {}

    std::uint64_t next() {
        std::uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }
};

struct Timer {
    std::chrono::steady_clock::time_point t0;
    Timer() : t0(std::chrono::steady_clock::now()) {}
    double ms() const {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }
};

void consume_benchmark(std::uint64_t x) {
    const std::uint64_t cur = bench_sink;
    bench_sink = cur ^ (x + 0x9e3779b97f4a7c15ULL + (cur << 6) + (cur >> 2));
}

std::vector<int> shuffled_unique_values(std::size_t n, std::uint64_t seed, int mul = 2, int add = 0) {
    std::vector<int> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<int>(i) * mul + add;
    SplitMix64 rng(seed);
    for (std::size_t i = n; i > 1; --i) {
        std::swap(v[i - 1], v[static_cast<std::size_t>(rng.next() % i)]);
    }
    return v;
}

std::vector<int> duplicated_values(std::size_t n, std::uint64_t seed, std::uint32_t value_mod) {
    std::vector<int> v(n);
    SplitMix64 rng(seed);
    for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<int>(rng.next() % value_mod);
    return v;
}

std::vector<int> query_values(std::size_t q, std::uint64_t seed, std::uint32_t value_mod) {
    std::vector<int> v(q);
    SplitMix64 rng(seed);
    for (std::size_t i = 0; i < q; ++i) v[i] = static_cast<int>(rng.next() % value_mod);
    return v;
}

std::vector<std::size_t> query_indices(std::size_t q, std::size_t n, std::uint64_t seed) {
    std::vector<std::size_t> v(q);
    SplitMix64 rng(seed);
    for (std::size_t i = 0; i < q; ++i) v[i] = static_cast<std::size_t>(rng.next() % n);
    return v;
}

template <class F>
BenchResult median_of(int reps, F&& f) {
    std::vector<BenchResult> rows;
    rows.reserve(static_cast<std::size_t>(reps));
    for (int rep = 0; rep < reps; ++rep) rows.push_back(f(rep));

    BenchResult out = rows.front();
    {
        auto tmp = rows;
        std::sort(tmp.begin(), tmp.end(),
                  [](const BenchResult& a, const BenchResult& b) { return a.btree_ms < b.btree_ms; });
        out.btree_ms = tmp[tmp.size() / 2].btree_ms;
        out.btree_checksum = tmp[tmp.size() / 2].btree_checksum;
    }
    if (out.std_ms >= 0.0) {
        auto tmp = rows;
        std::sort(tmp.begin(), tmp.end(),
                  [](const BenchResult& a, const BenchResult& b) { return a.std_ms < b.std_ms; });
        out.std_ms = tmp[tmp.size() / 2].std_ms;
        out.std_checksum = tmp[tmp.size() / 2].std_checksum;
    }
    return out;
}

BenchResult bench_default_construct(std::size_t q) {
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    std::vector<btree_multiset_cnt<int>> xs(q);
    cb += xs.size();
    const double mb = tb.ms();
    Timer ts;
    std::vector<std::multiset<int>> ss(q);
    cs += ss.size();
    const double ms = ts.ms();
    return {"default_construct", 0, q, q, q, mb, ms, cb, cs, "ok"};
}

BenchResult bench_construct_vector(std::size_t n, std::uint64_t seed) {
    const auto a = duplicated_values(n, seed, 65536U);
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    btree_multiset_cnt<int> xs(a);
    cb += xs.size();
    const double mb = tb.ms();
    Timer ts;
    std::multiset<int> ss(a.begin(), a.end());
    cs += ss.size();
    const double ms = ts.ms();
    return {"construct(vector)", n, 1, 1, 1, mb, ms, cb, cs, "ok"};
}

BenchResult bench_clear(std::size_t n, std::size_t q, std::uint64_t seed) {
    const auto a = duplicated_values(n, seed, 65536U);
    std::vector<btree_multiset_cnt<int>> xs_list;
    std::vector<std::multiset<int>> ss_list;
    xs_list.reserve(q);
    ss_list.reserve(q);
    for (std::size_t i = 0; i < q; ++i) {
        xs_list.emplace_back(a);
        ss_list.emplace_back(a.begin(), a.end());
    }
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    for (auto& xs : xs_list) {
        xs.clear();
        cb += xs.size();
    }
    const double mb = tb.ms();
    Timer ts;
    for (auto& ss : ss_list) {
        ss.clear();
        cs += ss.size();
    }
    const double ms = ts.ms();
    return {"clear", n, q, q, q, mb, ms, cb, cs, "ok"};
}

BenchResult bench_insert_unique_empty(std::size_t q, std::uint64_t seed) {
    const auto values = shuffled_unique_values(q, seed);
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    btree_multiset_cnt<int> xs;
    for (int x : values) xs.insert(x);
    cb += xs.size();
    const double mb = tb.ms();
    Timer ts;
    std::multiset<int> ss;
    for (int x : values) ss.insert(x);
    cs += ss.size();
    const double ms = ts.ms();
    return {"insert(unique,empty)", q, q, q, q, mb, ms, cb, cs, "ok"};
}

BenchResult bench_insert_missing(std::size_t n, std::size_t q, std::uint64_t seed) {
    const auto initial = shuffled_unique_values(n, seed, 4, 0);
    const auto values = shuffled_unique_values(q, seed + 1, 4, 1);
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    btree_multiset_cnt<int> xs(initial);
    for (int x : values) xs.insert(x);
    cb += xs.size();
    const double mb = tb.ms();
    Timer ts;
    std::multiset<int> ss(initial.begin(), initial.end());
    for (int x : values) ss.insert(x);
    cs += ss.size();
    const double ms = ts.ms();
    return {"insert(missing)", n, q, q, q, mb, ms, cb, cs, "ok"};
}

BenchResult bench_insert_duplicate(std::size_t n, std::size_t q, std::uint64_t seed) {
    const auto initial = shuffled_unique_values(n, seed);
    const int key = initial[n / 2];
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    btree_multiset_cnt<int> xs(initial);
    for (std::size_t i = 0; i < q; ++i) xs.insert(key);
    cb += xs.count(key);
    const double mb = tb.ms();
    Timer ts;
    std::multiset<int> ss(initial.begin(), initial.end());
    for (std::size_t i = 0; i < q; ++i) ss.insert(key);
    cs += ss.count(key);
    const double ms = ts.ms();
    return {"insert(duplicate)", n, q, q, q, mb, ms, cb, cs, "ok"};
}

BenchResult bench_erase_present(std::size_t n, std::uint64_t seed) {
    const auto values = shuffled_unique_values(n, seed);
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    btree_multiset_cnt<int> xs(values);
    for (int x : values) xs.erase(x);
    cb += xs.size();
    const double mb = tb.ms();
    Timer ts;
    std::multiset<int> ss(values.begin(), values.end());
    for (int x : values) ss.erase(x);
    cs += ss.size();
    const double ms = ts.ms();
    return {"erase(present)", n, n, n, n, mb, ms, cb, cs, "ok"};
}

BenchResult bench_erase_one_present(std::size_t n, std::uint64_t seed) {
    const auto values = shuffled_unique_values(n, seed);
    std::vector<int> duplicated;
    duplicated.reserve(n * 2);
    for (int x : values) {
        duplicated.push_back(x);
        duplicated.push_back(x);
    }
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    btree_multiset_cnt<int> xs(duplicated);
    for (int x : values) xs.erase_one(x);
    cb += xs.size();
    const double mb = tb.ms();
    Timer ts;
    std::multiset<int> ss(duplicated.begin(), duplicated.end());
    for (int x : values) {
        auto it = ss.find(x);
        ss.erase(it);
    }
    cs += ss.size();
    const double ms = ts.ms();
    return {"erase_one(present)", n * 2, n, n, n, mb, ms, cb, cs, "ok"};
}

BenchResult bench_erase_missing(std::size_t n, std::size_t q, std::uint64_t seed) {
    const auto initial = shuffled_unique_values(n, seed, 2, 0);
    const auto values = shuffled_unique_values(q, seed + 1, 2, 1);
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    btree_multiset_cnt<int> xs(initial);
    for (int x : values) xs.erase(x);
    cb += xs.size();
    const double mb = tb.ms();
    Timer ts;
    std::multiset<int> ss(initial.begin(), initial.end());
    for (int x : values) ss.erase(x);
    cs += ss.size();
    const double ms = ts.ms();
    return {"erase(missing)", n, q, q, q, mb, ms, cb, cs, "ok"};
}

BenchResult bench_erase_one_missing(std::size_t n, std::size_t q, std::uint64_t seed) {
    const auto initial = shuffled_unique_values(n, seed, 2, 0);
    const auto values = shuffled_unique_values(q, seed + 1, 2, 1);
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    btree_multiset_cnt<int> xs(initial);
    for (int x : values) xs.erase_one(x);
    cb += xs.size();
    const double mb = tb.ms();
    Timer ts;
    std::multiset<int> ss(initial.begin(), initial.end());
    for (int x : values) {
        auto it = ss.find(x);
        if (it != ss.end()) ss.erase(it);
    }
    cs += ss.size();
    const double ms = ts.ms();
    return {"erase_one(missing)", n, q, q, q, mb, ms, cb, cs, "ok"};
}

std::vector<unsigned char> query_bits(std::size_t q, std::uint64_t seed) {
    std::vector<unsigned char> bits(q);
    SplitMix64 rng(seed);
    for (std::size_t i = 0; i < q; ++i) bits[i] = static_cast<unsigned char>(rng.next() & 1ULL);
    return bits;
}

BenchResult bench_size(std::size_t n, std::size_t q, std::uint64_t seed) {
    auto initial = duplicated_values(n, seed, 65536U);
    auto initial2 = initial;
    initial2.push_back(70000);
    const auto bits = query_bits(q, seed + 1);
    btree_multiset_cnt<int> xs0(initial);
    btree_multiset_cnt<int> xs1(initial2);
    std::multiset<int> ss0(initial.begin(), initial.end());
    std::multiset<int> ss1(initial2.begin(), initial2.end());
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) cb += (bits[i] ? xs1.size() : xs0.size()) ^ (bench_sink & 1ULL);
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) cs += (bits[i] ? ss1.size() : ss0.size()) ^ (bench_sink & 1ULL);
    const double ms = ts.ms();
    return {"size", n, q, q, q, mb, ms, cb, cs, "ok"};
}

BenchResult bench_empty(std::size_t n, std::size_t q, std::uint64_t seed) {
    const auto initial = duplicated_values(n, seed, 65536U);
    const auto bits = query_bits(q, seed + 1);
    btree_multiset_cnt<int> xs0;
    btree_multiset_cnt<int> xs1(initial);
    std::multiset<int> ss0;
    std::multiset<int> ss1(initial.begin(), initial.end());
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) cb += ((bits[i] ? xs1.empty() : xs0.empty()) ? 1ULL : 0ULL) ^ (bench_sink & 1ULL);
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) cs += ((bits[i] ? ss1.empty() : ss0.empty()) ? 1ULL : 0ULL) ^ (bench_sink & 1ULL);
    const double ms = ts.ms();
    return {"empty", n, q, q, q, mb, ms, cb, cs, "ok"};
}

BenchResult bench_min(std::size_t n, std::size_t q, std::uint64_t seed) {
    const auto initial = duplicated_values(n, seed, 65536U);
    btree_multiset_cnt<int> xs(initial);
    std::multiset<int> ss(initial.begin(), initial.end());
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) cb += static_cast<std::uint64_t>(xs.min()) ^ (bench_sink & 1ULL);
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) cs += static_cast<std::uint64_t>(*ss.begin()) ^ (bench_sink & 1ULL);
    const double ms = ts.ms();
    return {"min", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchResult bench_max(std::size_t n, std::size_t q, std::uint64_t seed) {
    const auto initial = duplicated_values(n, seed, 65536U);
    btree_multiset_cnt<int> xs(initial);
    std::multiset<int> ss(initial.begin(), initial.end());
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) cb += static_cast<std::uint64_t>(xs.max()) ^ (bench_sink & 1ULL);
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) cs += static_cast<std::uint64_t>(*ss.rbegin()) ^ (bench_sink & 1ULL);
    const double ms = ts.ms();
    return {"max", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchResult bench_for_each(std::size_t n, std::uint64_t seed) {
    const auto initial = duplicated_values(n, seed, 65536U);
    btree_multiset_cnt<int> xs(initial);
    std::multiset<int> ss(initial.begin(), initial.end());
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    xs.for_each([&](const int& x) { cb += static_cast<std::uint64_t>(x); });
    const double mb = tb.ms();
    Timer ts;
    for (int x : ss) cs += static_cast<std::uint64_t>(x);
    const double ms = ts.ms();
    return {"for_each", n, n, n, n, mb, ms, cb, cs, "ok"};
}

BenchResult bench_for_each_r(std::size_t n, std::uint64_t seed) {
    const auto initial = duplicated_values(n, seed, 65536U);
    btree_multiset_cnt<int> xs(initial);
    std::multiset<int> ss(initial.begin(), initial.end());
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    xs.for_each_r([&](const int& x) { cb += static_cast<std::uint64_t>(x); });
    const double mb = tb.ms();
    Timer ts;
    for (auto it = ss.rbegin(); it != ss.rend(); ++it) cs += static_cast<std::uint64_t>(*it);
    const double ms = ts.ms();
    return {"for_each_r", n, n, n, n, mb, ms, cb, cs, "ok"};
}

BenchResult bench_for_each_break(std::size_t n, std::uint64_t seed) {
    const auto initial = duplicated_values(n, seed, 65536U);
    btree_multiset_cnt<int> xs(initial);
    std::multiset<int> ss(initial.begin(), initial.end());
    const std::size_t limit = n / 2;
    std::size_t seen_b = 0;
    std::size_t seen_s = 0;
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    xs.for_each([&](const int& x) {
        cb += static_cast<std::uint64_t>(x);
        return ++seen_b >= limit;
    });
    const double mb = tb.ms();
    Timer ts;
    for (int x : ss) {
        cs += static_cast<std::uint64_t>(x);
        if (++seen_s >= limit) break;
    }
    const double ms = ts.ms();
    return {"for_each(break)", n, limit, limit, limit, mb, ms, cb, cs, "ok"};
}

BenchResult bench_for_each_r_break(std::size_t n, std::uint64_t seed) {
    const auto initial = duplicated_values(n, seed, 65536U);
    btree_multiset_cnt<int> xs(initial);
    std::multiset<int> ss(initial.begin(), initial.end());
    const std::size_t limit = n / 2;
    std::size_t seen_b = 0;
    std::size_t seen_s = 0;
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    xs.for_each_r([&](const int& x) {
        cb += static_cast<std::uint64_t>(x);
        return ++seen_b >= limit;
    });
    const double mb = tb.ms();
    Timer ts;
    for (auto it = ss.rbegin(); it != ss.rend(); ++it) {
        cs += static_cast<std::uint64_t>(*it);
        if (++seen_s >= limit) break;
    }
    const double ms = ts.ms();
    return {"for_each_r(break)", n, limit, limit, limit, mb, ms, cb, cs, "ok"};
}

BenchResult bench_operator_index(std::size_t n, std::size_t q, std::uint64_t seed) {
    const auto initial = duplicated_values(n, seed, 65536U);
    const auto indices = query_indices(q, n, seed + 1);
    btree_multiset_cnt<int> xs(initial);
    std::uint64_t cb = 0;
    Timer tb;
    for (std::size_t k : indices) cb += static_cast<std::uint64_t>(xs[k]);
    const double mb = tb.ms();
    return {"operator[]", n, q, q, 0, mb, -1.0, cb, 0, "btree_only"};
}

BenchResult bench_rank(std::size_t n, std::size_t q, std::uint64_t seed) {
    const auto initial = duplicated_values(n, seed, 65536U);
    const auto qs = query_values(q, seed + 1, 80000U);
    btree_multiset_cnt<int> xs(initial);
    std::uint64_t cb = 0;
    Timer tb;
    for (int x : qs) cb += xs.rank(x);
    const double mb = tb.ms();
    return {"rank", n, q, q, 0, mb, -1.0, cb, 0, "btree_only"};
}

BenchResult bench_rank2(std::size_t n, std::size_t q, std::uint64_t seed) {
    const auto initial = duplicated_values(n, seed, 65536U);
    const auto qs = query_values(q, seed + 1, 80000U);
    btree_multiset_cnt<int> xs(initial);
    std::uint64_t cb = 0;
    Timer tb;
    for (int x : qs) cb += xs.rank2(x);
    const double mb = tb.ms();
    return {"rank2", n, q, q, 0, mb, -1.0, cb, 0, "btree_only"};
}

BenchResult bench_contains(std::size_t n, std::size_t q, std::uint64_t seed) {
    const auto initial = duplicated_values(n, seed, 65536U);
    const auto qs = query_values(q, seed + 1, 80000U);
    btree_multiset_cnt<int> xs(initial);
    std::multiset<int> ss(initial.begin(), initial.end());
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    for (int x : qs) cb += xs.contains(x) ? 1ULL : 0ULL;
    const double mb = tb.ms();
    Timer ts;
    for (int x : qs) cs += ss.find(x) != ss.end() ? 1ULL : 0ULL;
    const double ms = ts.ms();
    return {"contains", n, q, q, q, mb, ms, cb, cs, "ok"};
}

BenchResult bench_count(std::size_t n, std::size_t q, std::uint64_t seed) {
    const auto initial = duplicated_values(n, seed, 65536U);
    const auto qs = query_values(q, seed + 1, 80000U);
    btree_multiset_cnt<int> xs(initial);
    std::multiset<int> ss(initial.begin(), initial.end());
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    for (int x : qs) cb += xs.count(x);
    const double mb = tb.ms();
    Timer ts;
    for (int x : qs) cs += ss.count(x);
    const double ms = ts.ms();
    return {"count", n, q, q, q, mb, ms, cb, cs, "ok"};
}

BenchResult bench_less_equal(std::size_t n, std::size_t q, std::uint64_t seed) {
    const auto initial = duplicated_values(n, seed, 65536U);
    const auto qs = query_values(q, seed + 1, 80000U);
    btree_multiset_cnt<int> xs(initial);
    std::multiset<int> ss(initial.begin(), initial.end());
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    for (int x : qs) {
        const auto v = xs.less_equal(x);
        cb += v ? static_cast<std::uint64_t>(*v) : 0x517cc1b727220a95ULL;
    }
    const double mb = tb.ms();
    Timer ts;
    for (int x : qs) {
        auto it = ss.upper_bound(x);
        if (it == ss.begin()) {
            cs += 0x517cc1b727220a95ULL;
        } else {
            --it;
            cs += static_cast<std::uint64_t>(*it);
        }
    }
    const double ms = ts.ms();
    return {"less_equal", n, q, q, q, mb, ms, cb, cs, "ok"};
}

BenchResult bench_greater_equal(std::size_t n, std::size_t q, std::uint64_t seed) {
    const auto initial = duplicated_values(n, seed, 65536U);
    const auto qs = query_values(q, seed + 1, 80000U);
    btree_multiset_cnt<int> xs(initial);
    std::multiset<int> ss(initial.begin(), initial.end());
    std::uint64_t cb = 0;
    std::uint64_t cs = 0;
    Timer tb;
    for (int x : qs) {
        const auto v = xs.greater_equal(x);
        cb += v ? static_cast<std::uint64_t>(*v) : 0x517cc1b727220a95ULL;
    }
    const double mb = tb.ms();
    Timer ts;
    for (int x : qs) {
        auto it = ss.lower_bound(x);
        cs += it == ss.end() ? 0x517cc1b727220a95ULL : static_cast<std::uint64_t>(*it);
    }
    const double ms = ts.ms();
    return {"greater_equal", n, q, q, q, mb, ms, cb, cs, "ok"};
}

void run_benchmarks() {
    constexpr int reps = 3;
    constexpr std::size_t n = 200000;
    constexpr std::size_t q = 300000;
    constexpr std::size_t q_fast = 5000000;
    constexpr std::size_t n_small = 50000;
    constexpr std::size_t q_small = 100000;

    std::vector<BenchResult> rows;
    rows.push_back(median_of(reps, [&](int) { return bench_default_construct(q_small); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_construct_vector(n, 100 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_clear(n, 5, 200 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_insert_unique_empty(n, 300 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_insert_missing(n, q, 400 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_insert_duplicate(n, q, 500 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_erase_present(n, 600 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_erase_one_present(n, 650 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_erase_missing(n, q, 700 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_erase_one_missing(n, q, 750 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_size(n, q_fast, 800 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_empty(n, q_fast, 850 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_min(n, q_fast, 900 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_max(n, q_fast, 1000 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_for_each(n, 1100 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_for_each_r(n, 1200 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_for_each_break(n, 1300 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_for_each_r_break(n, 1400 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_operator_index(n_small, q_small, 1500 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_rank(n, q, 1600 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_rank2(n, q, 1700 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_contains(n, q, 1800 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_count(n, q, 1850 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_less_equal(n, q, 1900 + static_cast<std::uint64_t>(i)); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_greater_equal(n, q, 2000 + static_cast<std::uint64_t>(i)); }));

    std::cout << "benchmarks begin\n";
    std::cout << "compiler," << __VERSION__ << "\n";
    std::cout << "flags,-std=c++20 -O2 -march=native -DNDEBUG\n";
    std::cout << "repetitions," << reps << "\n";
    std::cout << "api,N,Q,btree_ms,std_multiset_ms,btree_ns_per_op,std_multiset_ns_per_op,std_over_btree,status\n";
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
        consume_benchmark(r.btree_checksum);
        consume_benchmark(r.std_checksum);
    }
    std::cerr << "bench_sink=" << bench_sink << '\n';
}
} // namespace btree_multiset_cnt_self_test

int main() {
    using namespace btree_multiset_cnt_self_test;
    test_empty_singleton_clear();
    test_constructor_duplicates_boundaries();
    test_iteration_callbacks();
    test_explicit_btree_set_compatible_api();
    test_deterministic_update_edges();
    test_random_small_load_full_check();
    test_random_default_sampled();
    std::cout << "all tests passed\n";
    run_benchmarks();
}
#endif

// 実行結果(atcoder)
// empty_singleton_clear: ok
// constructor_duplicates_boundaries: ok
// iteration_callbacks: ok
// explicit_btree_set_compatible_api: ok
// deterministic_update_edges: ok
// random_small_load_full_check_100000_ops: ok
// random_default_sampled_200000_ops: ok
// all tests passed
// benchmarks begin
// compiler,15.2.0
// flags,-std=c++20 -O2 -march=native -DNDEBUG
// repetitions,3
// api,N,Q,btree_ms,std_multiset_ms,btree_ns_per_op,std_multiset_ns_per_op,std_over_btree,status
// default_construct,0,100000,3.332,1.410,33.322,14.100,0.423,ok
// construct(vector),200000,1,13.472,40.112,13472193.000,40111670.000,2.977,ok
// clear,200000,5,0.009,57.370,1719.200,11474002.400,6674.036,ok
// insert(unique,empty),200000,200000,36.773,40.033,183.865,200.166,1.089,ok
// insert(missing),200000,300000,96.080,128.216,320.266,427.386,1.334,ok
// insert(duplicate),200000,300000,22.613,80.687,75.377,268.956,3.568,ok
// erase(present),200000,200000,39.246,91.325,196.230,456.627,2.327,ok
// erase_one(present),400000,200000,50.748,148.620,253.738,743.098,2.929,ok
// erase(missing),200000,300000,39.918,99.964,133.061,333.213,2.504,ok
// erase_one(missing),200000,300000,40.249,102.772,134.164,342.573,2.553,ok
// size,200000,5000000,3.090,3.091,0.618,0.618,1.000,ok
// empty,200000,5000000,3.084,3.087,0.617,0.617,1.001,ok
// min,200000,5000000,1.539,1.534,0.308,0.307,0.997,ok
// max,200000,5000000,1.538,1.532,0.308,0.306,0.996,ok
// for_each,200000,200000,0.076,8.725,0.381,43.625,114.414,ok
// for_each_r,200000,200000,0.121,8.994,0.607,44.971,74.029,ok
// for_each(break),200000,100000,0.306,4.471,3.058,44.708,14.622,ok
// for_each_r(break),200000,100000,0.310,4.439,3.100,44.391,14.319,ok
// operator[],50000,100000,8.004,NA,80.041,NA,NA,btree_only
// rank,200000,300000,35.738,NA,119.127,NA,NA,btree_only
// rank2,200000,300000,34.608,NA,115.359,NA,NA,btree_only
// contains,200000,300000,23.600,73.955,78.666,246.517,3.134,ok
// count,200000,300000,23.861,89.702,79.536,299.006,3.759,ok
// less_equal,200000,300000,23.119,78.391,77.065,261.304,3.391,ok
// greater_equal,200000,300000,23.811,75.839,79.371,252.796,3.185,ok
