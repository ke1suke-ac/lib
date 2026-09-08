/*
 * btree_map: 競技プログラミング向けの順序付き map 実装。
 * key-value を複数の key 昇順 vector ブロックに分けて保持し、ブロック最大 key と Fenwick Tree 風の累積個数で
 * lower_bound / upper_bound / rank / kth を高速に処理し、min / max / 全要素走査も提供する。同じ key は 1 つだけ保持する。
 * std::map と異なり、insert / erase で要素が vector 内を移動するため、参照・ポインタの長期安定性は保証しない。
 *
 * テンプレートパラメータ:
 *   K     : key 型。operator< で比較できる型を想定。
 *   V     : value 型。軽量に move / copy できる型ほど有利。
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

template <typename K, typename V, std::size_t load1 = 200, std::size_t load2 = 64>
struct btree_map {
    using key_type = K;
    using mapped_type = V;
    using value_type = std::pair<K, V>;

private:
    struct Entry {
        K key;
        V value;
    };

    using iterator = typename std::vector<Entry>::iterator;
    using const_iterator = typename std::vector<Entry>::const_iterator;

public:
    // 空の map を遅延初期化で構築する。O(1)
    btree_map() = default;

    // 配列から重複 key を除いた順序付き map を構築する。同じ key は入力で最初に現れた値を残す。O(n log n)
    explicit btree_map(std::vector<value_type> a) {
        std::stable_sort(a.begin(), a.end(), [](const value_type& lhs, const value_type& rhs) {
            return lhs.first < rhs.first;
        });
        a.erase(std::unique(a.begin(), a.end(), [](const value_type& lhs, const value_type& rhs) {
                    return key_equal(lhs.first, rhs.first);
                }),
                a.end());
        n = a.size();
        if (n == 0) return;

        blocks.reserve(n / load1 + 2);
        bit_cnt.reserve(n / load1 + 2);
        blk_max.reserve(n / load1 + 2);
        make_sentinel();

        for (std::size_t l = 0; l < n; l += load1) {
            std::size_t r = std::min(l + load1, n);
            ensure_storage(block_len + 1);
            auto& block = blocks[block_len];
            block.clear();
            block.reserve(r - l);
            for (std::size_t i = l; i < r; ++i) {
                block.push_back(Entry{std::move(a[i].first), std::move(a[i].second)});
            }
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

    // key が存在しなければ key-value を挿入する。既に存在する場合は何もしない。O(log n + load1 + load2)
    void insert(K key, V value) { insert_impl<false>(std::move(key), std::move(value)); }

    // key-value を挿入する。既に存在する場合は value を代入する。O(log n + load1 + load2)
    void insert_or_assign(K key, V value) { insert_impl<true>(std::move(key), std::move(value)); }

    // key を削除する。存在しない場合は何もしない。O(log n + load1 + load2)
    void erase(const K& key) {
        if (seg_size.empty()) return;

        auto [bi, it] = lower_bound_mut(key);
        if (it == blocks[bi].end() || !key_equal(it->key, key)) return;

        --n;
        for (std::size_t i = bi; i < block_len; i += (-i & i)) --bit_cnt[i];

        const std::size_t segi = (bi - 1) >> log;
        const std::size_t bj = (segi << log) | seg_size[segi];
        const std::size_t bn = (segi + 1) << log;
        const bool key_in_block_end = (std::next(it) == blocks[bi].end());
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
        } else if (key_in_block_end) {
            blk_max[bi] = blocks[bi].back().key;
            if (bi == bj)
                for (std::size_t i = bi; i < std::min(bn + 1, block_len); i += (-i & i))
                    blk_max[i] = blk_max[bi];
        }
    }

    // 現在の要素数を返す。O(1)
    std::size_t size() const { return n; }

    // map が空なら true を返す。O(1)
    bool empty() const { return n == 0; }

    // 最小 key の key-value 参照ペアを返す。空の場合の動作は未定義。O(1)
    std::pair<const K&, V&> min() {
        Entry& e = blocks[1].front();
        return {e.key, e.value};
    }

    // 最小 key の key-value 参照ペアを返す。空の場合の動作は未定義。O(1)
    std::pair<const K&, const V&> min() const {
        const Entry& e = blocks[1].front();
        return {e.key, e.value};
    }

    // 最大 key の key-value 参照ペアを返す。空の場合の動作は未定義。O(1)
    std::pair<const K&, V&> max() {
        Entry& e = blocks[block_len - 1].back();
        return {e.key, e.value};
    }

    // 最大 key の key-value 参照ペアを返す。空の場合の動作は未定義。O(1)
    std::pair<const K&, const V&> max() const {
        const Entry& e = blocks[block_len - 1].back();
        return {e.key, e.value};
    }

    // 昇順に全 key-value へ callback(key, value) を適用する。bool 変換可能な戻り値が true なら中断する。O(n)
    template <class F>
    void for_each(F&& callback) {
        for_each_impl<false>(*this, callback);
    }

    // 昇順に全 key-value へ callback(key, value) を適用する。bool 変換可能な戻り値が true なら中断する。O(n)
    template <class F>
    void for_each(F&& callback) const {
        for_each_impl<false>(*this, callback);
    }

    // 降順に全 key-value へ callback(key, value) を適用する。bool 変換可能な戻り値が true なら中断する。O(n)
    template <class F>
    void for_each_r(F&& callback) {
        for_each_impl<true>(*this, callback);
    }

    // 降順に全 key-value へ callback(key, value) を適用する。bool 変換可能な戻り値が true なら中断する。O(n)
    template <class F>
    void for_each_r(F&& callback) const {
        for_each_impl<true>(*this, callback);
    }

    // 0-indexed で k 番目に小さい key の key-value を返す。O(log n)
    value_type kth(std::size_t k) const {
        std::size_t bi = 0;
        if (block_len > 1) {
            const std::size_t top = floor_log2_size(block_len - 1);
            for (std::size_t i = std::size_t(1) << top; i; i >>= 1)
                if (((bi | i) < block_len) && (k >= bit_cnt[bi | i]))
                    k -= (bit_cnt[bi |= i]);
        }

        return make_value(blocks[bi + 1][k]);
    }

    // key が存在すれば value へのポインタを返す。存在しない場合は nullptr。O(log n)
    V* find(const K& key) {
        if (seg_size.empty()) return nullptr;
        auto [bi, it] = lower_bound_mut(key);
        if (it == blocks[bi].end() || !key_equal(it->key, key)) return nullptr;
        return &it->value;
    }

    // key が存在すれば value へのポインタを返す。存在しない場合は nullptr。O(log n)
    const V* find(const K& key) const {
        if (seg_size.empty()) return nullptr;
        auto [bi, it] = lower_bound(key);
        if (it == blocks[bi].end() || !key_equal(it->key, key)) return nullptr;
        return &it->value;
    }

    // key に対応する value への参照を返す。存在しない場合は value を値初期化して挿入する。O(log n + load1 + load2)
    V& operator[](const K& key) {
        if (V* p = find(key)) return *p;
        insert(K(key), V{});
        return *find(key);
    }

    // key より小さい key の個数を返す。O(log n)
    std::size_t rank(const K& key) const {
        if (seg_size.empty()) return 0;

        auto [bi, it] = lower_bound(key);
        std::size_t rk = it - blocks[bi].begin();
        for (std::size_t i = bi - 1; i; i ^= (-i & i)) rk += bit_cnt[i];
        return rk;
    }

    // key 以下の key の個数を返す。O(log n)
    std::size_t rank2(const K& key) const {
        if (seg_size.empty()) return 0;

        auto [bi, it] = upper_bound(key);
        std::size_t rk = it - blocks[bi].begin();
        for (std::size_t i = bi - 1; i; i ^= (-i & i)) rk += bit_cnt[i];
        return rk;
    }

    // key が存在するなら true を返す。O(log n)
    bool contains(const K& key) const {
        if (seg_size.empty()) return false;
        auto [bi, it] = lower_bound(key);
        return it != blocks[bi].end() && key_equal(it->key, key);
    }

    // key が存在するかを 0 または 1 で返す。O(log n)
    std::size_t count(const K& key) const { return contains(key); }

    // key 以下の最大 key の key-value を返す。存在しない場合は std::nullopt。O(log n)
    std::optional<value_type> less_equal(const K& key) const {
        if (seg_size.empty()) return std::nullopt;
        auto [bi, it] = upper_bound(key);
        if (!(bi > 1 || (bi == 1 && it > blocks[bi].begin()))) return std::nullopt;
        if (it > blocks[bi].begin()) return make_value(*std::prev(it));
        if ((bi & (load2x - 1)) != 1) return make_value(blocks[bi - 1].back());
        const std::size_t segi = (bi - 1) >> log;
        bi = ((segi - 1) << log) | seg_size[segi - 1];
        return make_value(blocks[bi].back());
    }

    // key 以上の最小 key の key-value を返す。存在しない場合は std::nullopt。O(log n)
    std::optional<value_type> greater_equal(const K& key) const {
        if (seg_size.empty()) return std::nullopt;
        auto [bi, it] = lower_bound(key);
        if (it == blocks[bi].end()) return std::nullopt;
        return make_value(*it);
    }

private:
    static std::size_t floor_log2_size(const std::size_t x) { return static_cast<std::size_t>(63 - __builtin_clzll(x)); }

    static constexpr std::size_t load1x = load1 * 2;
    static constexpr std::size_t load2x = load2 * 2;
    static constexpr std::size_t log = static_cast<std::size_t>(std::bit_width(load2x) - 1);

    std::vector<std::vector<Entry>> blocks;
    std::vector<std::size_t> bit_cnt;
    std::vector<std::optional<K>> blk_max;
    std::vector<std::size_t> seg_size;
    std::size_t n = 0;
    std::size_t block_len = 0;

    static bool key_equal(const K& a, const K& b) { return !(a < b) && !(b < a); }
    static bool entry_key_less(const Entry& e, const K& key) { return e.key < key; }
    static bool key_less_entry(const K& key, const Entry& e) { return key < e.key; }
    static value_type make_value(const Entry& e) { return value_type(e.key, e.value); }

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

    template <class F, class ValueRef>
    static bool invoke_callback(F& callback, const K& key, ValueRef&& value) {
        if constexpr (std::is_void_v<std::invoke_result_t<F&, const K&, ValueRef>>) {
            std::invoke(callback, key, std::forward<ValueRef>(value));
            return false;
        } else {
            return static_cast<bool>(std::invoke(callback, key, std::forward<ValueRef>(value)));
        }
    }

    template <bool Reverse, class Self, class F>
    static void for_each_impl(Self& self, F& callback) {
        if constexpr (Reverse) {
            for (std::size_t segi = self.seg_size.size(); segi-- > 0;) {
                const std::size_t first = (segi << log) | 1;
                const std::size_t last = (segi << log) | self.seg_size[segi];
                for (std::size_t bi = last + 1; bi-- > first;)
                    for (auto it = self.blocks[bi].rbegin(); it != self.blocks[bi].rend(); ++it)
                        if (invoke_callback(callback, it->key, it->value)) return;
            }
        } else {
            for (std::size_t segi = 0; segi < self.seg_size.size(); ++segi) {
                const std::size_t first = (segi << log) | 1;
                const std::size_t last = (segi << log) | self.seg_size[segi];
                for (std::size_t bi = first; bi <= last; ++bi)
                    for (auto& e : self.blocks[bi])
                        if (invoke_callback(callback, e.key, e.value)) return;
            }
        }
    }

    template <bool AssignIfExists>
    void insert_impl(K key, V value) {
        if (seg_size.empty()) {
            make_sentinel();
            ensure_storage(2);
            blocks[1].clear();
            blocks[1].push_back(Entry{std::move(key), std::move(value)});
            bit_cnt[1] = 1;
            blk_max[1] = blocks[1].back().key;
            block_len = 2;
            ++n;
            seg_size.assign(1, 1);
            return;
        }

        auto [bi, it] = lower_bound_mut(key);
        if (it != blocks[bi].end() && key_equal(it->key, key)) {
            if constexpr (AssignIfExists) it->value = std::move(value);
            return;
        }

        ++n;
        for (std::size_t i = bi; i < block_len; i += (-i & i)) ++bit_cnt[i];

        const std::size_t segi = (bi - 1) >> log;
        const std::size_t bj = (segi << log) | seg_size[segi];
        const std::size_t bn = (segi + 1) << log;
        const bool key_in_block_end = (it == blocks[bi].end());
        blocks[bi].insert(it, Entry{std::move(key), std::move(value)});

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
        } else if (key_in_block_end) {
            blk_max[bi] = blocks[bi].back().key;
            if (bi == bj)
                for (std::size_t i = bi; i < std::min(bj + 1, block_len); i += -i & i)
                    blk_max[i] = blk_max[bi];
        }
    }

    std::pair<std::size_t, iterator> lower_bound_mut(const K& key) {
        if (seg_size.empty()) return {0, iterator{}};

        std::size_t bi = 0;
        if (block_len > 1) {
            const std::size_t top = floor_log2_size(block_len - 1);
            for (std::size_t i = std::size_t(1) << top; i; i >>= 1) {
                std::size_t idx = bi | i;
                if (idx < block_len && blk_max[idx].has_value() && blk_max[idx].value() < key) bi |= i;
            }
        }

        if (bi + 1 < block_len) ++bi;
        auto it = std::lower_bound(blocks[bi].begin(), blocks[bi].end(), key, entry_key_less);
        return {bi, it};
    }

    std::pair<std::size_t, const_iterator> lower_bound(const K& key) const {
        if (seg_size.empty()) return {0, const_iterator{}};

        std::size_t bi = 0;
        if (block_len > 1) {
            const std::size_t top = floor_log2_size(block_len - 1);
            for (std::size_t i = std::size_t(1) << top; i; i >>= 1) {
                std::size_t idx = bi | i;
                if (idx < block_len && blk_max[idx].has_value() && blk_max[idx].value() < key) bi |= i;
            }
        }

        if (bi + 1 < block_len) ++bi;
        auto it = std::lower_bound(blocks[bi].begin(), blocks[bi].end(), key, entry_key_less);
        return {bi, it};
    }

    std::pair<std::size_t, const_iterator> upper_bound(const K& key) const {
        if (seg_size.empty()) return {0, const_iterator{}};

        std::size_t bi = 0;
        if (block_len > 1) {
            const std::size_t top = floor_log2_size(block_len - 1);
            for (std::size_t i = std::size_t(1) << top; i; i >>= 1) {
                std::size_t idx = bi | i;
                if (idx < block_len && blk_max[idx].has_value() && ! (key < blk_max[idx].value())) bi |= i;
            }
        }

        if (bi + 1 < block_len) ++bi;
        auto it = std::upper_bound(blocks[bi].begin(), blocks[bi].end(), key, key_less_entry);
        return {bi, it};
    }

    void range_bit_modify(const std::size_t b1, const std::size_t b2) {
        std::fill(bit_cnt.data() + b1, bit_cnt.data() + b2 + 1, 0);

        std::optional<K> mx;
        for (std::size_t i = b1; i <= b2; ++i) {
            if (!blocks[i].empty()) {
                bit_cnt[i] += blocks[i].size();
                blk_max[i] = mx = blocks[i].back().key;
            } else {
                blk_max[i] = mx;
            }

            if (const std::size_t next = i + (-i & i); (next <= b2) && bit_cnt[i]) bit_cnt[next] += bit_cnt[i];
        }

        for (std::size_t lowb = ((-b2) & b2) / 2; lowb >= load2x; lowb >>= 1) bit_cnt[b2] += bit_cnt[b2 - lowb];
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
            if (const std::size_t next = j + (-j & j); next < block_len) bit_cnt[next] += bit_cnt[j];
    }
};

#if __INCLUDE_LEVEL__ == 0
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <sstream>
#include <string>

namespace btree_map_self_test {

using Pair = std::pair<int, int>;

std::ostream& operator<<(std::ostream& os, const Pair& p) {
    return os << '(' << p.first << ',' << p.second << ')';
}

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

std::string pair_to_string(const Pair& p) {
    return "(" + std::to_string(p.first) + "," + std::to_string(p.second) + ")";
}

std::string optional_pair_to_string(const std::optional<Pair>& x) {
    if (!x) return "nullopt";
    return pair_to_string(*x);
}

void require_eq_opt(const std::optional<Pair>& actual, const std::optional<Pair>& expected,
                    const std::string& message) {
    if (actual != expected) {
        fail(message + " actual=" + optional_pair_to_string(actual) +
             " expected=" + optional_pair_to_string(expected));
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
            oss << message << " at " << i;
            fail(oss.str());
        }
    }
}

std::optional<Pair> map_less_equal(const std::map<int, int>& mm, int key) {
    auto it = mm.upper_bound(key);
    if (it == mm.begin()) return std::nullopt;
    --it;
    return Pair{it->first, it->second};
}

std::optional<Pair> map_greater_equal(const std::map<int, int>& mm, int key) {
    auto it = mm.lower_bound(key);
    if (it == mm.end()) return std::nullopt;
    return Pair{it->first, it->second};
}

std::vector<int> candidate_keys(const std::map<int, int>& mm) {
    std::vector<int> qs = {
        std::numeric_limits<int>::min(), std::numeric_limits<int>::min() + 1,
        -1000000007, -1000, -101, -100, -99, -10, -1, 0, 1, 2, 10,
        99, 100, 101, 1000, 1000000007,
        std::numeric_limits<int>::max() - 1, std::numeric_limits<int>::max()
    };
    for (auto [k, v] : mm) {
        (void)v;
        qs.push_back(k);
        if (k != std::numeric_limits<int>::min()) qs.push_back(k - 1);
        if (k != std::numeric_limits<int>::max()) qs.push_back(k + 1);
    }
    std::sort(qs.begin(), qs.end());
    qs.erase(std::unique(qs.begin(), qs.end()), qs.end());
    return qs;
}

template <class Map>
std::vector<Pair> collect_forward(const Map& xs) {
    std::vector<Pair> got;
    xs.for_each([&](const int& k, const int& v) { got.push_back({k, v}); });
    return got;
}

template <class Map>
std::vector<Pair> collect_reverse(const Map& xs) {
    std::vector<Pair> got;
    xs.for_each_r([&](const int& k, const int& v) { got.push_back({k, v}); });
    return got;
}

std::vector<Pair> map_to_vector(const std::map<int, int>& mm) {
    std::vector<Pair> v;
    for (auto [k, val] : mm) v.push_back({k, val});
    return v;
}

template <class Map>
void check_all_apis(const Map& xs, const std::map<int, int>& mm, const std::string& label) {
    const std::vector<Pair> expected = map_to_vector(mm);
    std::vector<Pair> expected_r = expected;
    std::reverse(expected_r.begin(), expected_r.end());

    require_eq(xs.size(), mm.size(), label + ": size");
    require_eq(xs.empty(), mm.empty(), label + ": empty");
    if (!mm.empty()) {
        const auto mn = xs.min();
        const auto mx = xs.max();
        require_eq(Pair{mn.first, mn.second}, Pair{mm.begin()->first, mm.begin()->second}, label + ": min");
        require_eq(Pair{mx.first, mx.second}, Pair{mm.rbegin()->first, mm.rbegin()->second}, label + ": max");
    }
    require_vec_eq(collect_forward(xs), expected, label + ": for_each order");
    require_vec_eq(collect_reverse(xs), expected_r, label + ": for_each_r order");

    for (std::size_t k = 0; k < expected.size(); ++k) require_eq(xs.kth(k), expected[k], label + ": kth");

    for (int q : candidate_keys(mm)) {
        const std::size_t rk = static_cast<std::size_t>(
            std::lower_bound(expected.begin(), expected.end(), q,
                             [](const Pair& p, int key) { return p.first < key; }) - expected.begin());
        const std::size_t rk2 = static_cast<std::size_t>(
            std::upper_bound(expected.begin(), expected.end(), q,
                             [](int key, const Pair& p) { return key < p.first; }) - expected.begin());
        require_eq(xs.rank(q), rk, label + ": rank");
        require_eq(xs.rank2(q), rk2, label + ": rank2");
        require_eq(xs.contains(q), mm.contains(q), label + ": contains");
        require_eq(xs.count(q), mm.count(q), label + ": count");
        const int* p = xs.find(q);
        auto it = mm.find(q);
        require((p == nullptr) == (it == mm.end()), label + ": find nullness");
        if (p) require_eq(*p, it->second, label + ": find value");
        require_eq_opt(xs.less_equal(q), map_less_equal(mm, q), label + ": less_equal");
        require_eq_opt(xs.greater_equal(q), map_greater_equal(mm, q), label + ": greater_equal");
    }
}

struct NotBoolConvertible {};

void test_empty_singleton_clear() {
    btree_map<int, int, 4, 2> xs;
    std::map<int, int> mm;
    check_all_apis(xs, mm, "empty default");

    int calls = 0;
    xs.for_each([&](const int&, const int&) { ++calls; });
    xs.for_each_r([&](const int&, const int&) { ++calls; return true; });
    require_eq(calls, 0, "empty iteration callbacks are not called");

    xs.erase(123);
    xs.clear();
    check_all_apis(xs, mm, "empty after erase/clear");

    xs.insert(10, 100);
    mm.emplace(10, 100);
    check_all_apis(xs, mm, "singleton insert");

    xs.insert(10, 999);
    mm.emplace(10, 999);
    xs.erase(11);
    mm.erase(11);
    check_all_apis(xs, mm, "singleton duplicate insert and missing erase");

    xs.insert_or_assign(10, 1010);
    mm.insert_or_assign(10, 1010);
    check_all_apis(xs, mm, "singleton insert_or_assign existing");

    xs.erase(10);
    mm.erase(10);
    check_all_apis(xs, mm, "singleton erased");

    xs[-5] = 55;
    mm[-5] = 55;
    check_all_apis(xs, mm, "operator[] missing insert");

    xs[-5] = 66;
    mm[-5] = 66;
    check_all_apis(xs, mm, "operator[] existing assign");

    xs.clear();
    mm.clear();
    check_all_apis(xs, mm, "clear nonempty");

    xs.insert(7, 70);
    mm.emplace(7, 70);
    check_all_apis(xs, mm, "reuse after clear");
}

void test_constructor_duplicates_and_boundaries() {
    std::vector<Pair> init = {
        {5, 50}, {3, 30}, {5, 500}, {-1, -10}, {0, 0}, {3, 300},
        {std::numeric_limits<int>::min(), -1}, {std::numeric_limits<int>::max(), 1},
        {42, 420}, {42, 421}, {-1, -11}
    };
    btree_map<int, int, 4, 2> xs(init);
    std::map<int, int> mm;
    for (auto p : init) mm.emplace(p.first, p.second);
    check_all_apis(xs, mm, "constructor duplicates and int boundaries");

    btree_map<int, int, 4, 2> empty_from_vector(std::vector<Pair>{});
    check_all_apis(empty_from_vector, std::map<int, int>{}, "empty vector constructor");
}

void test_iteration_break_modes() {
    std::vector<Pair> init = {{-8, 80}, {-3, 30}, {-1, 10}, {0, 0}, {2, 20}, {4, 40}, {9, 90}, {20, 200}};
    btree_map<int, int, 4, 2> xs(init);
    std::map<int, int> mm(init.begin(), init.end());
    check_all_apis(xs, mm, "iteration source");

    std::vector<int> got;
    xs.for_each([&](const int& k, const int& v) -> bool {
        got.push_back(k + v);
        return k >= 2;
    });
    require_vec_eq(got, std::vector<int>({72, 27, 9, 0, 22}), "for_each bool break");

    got.clear();
    xs.for_each([&](const int& k, const int&) -> int {
        got.push_back(k);
        return k == 4;
    });
    require_vec_eq(got, std::vector<int>({-8, -3, -1, 0, 2, 4}), "for_each int break");

    got.clear();
    xs.for_each([&](const int& k, const int&) -> std::optional<int> {
        got.push_back(k);
        if (k == 0) return 1;
        return std::nullopt;
    });
    require_vec_eq(got, std::vector<int>({-8, -3, -1, 0}), "for_each optional break");

    got.clear();
    xs.for_each([&](const int& k, const int&) -> int {
        got.push_back(k);
        return 0;
    });
    require_vec_eq(got, std::vector<int>({-8, -3, -1, 0, 2, 4, 9, 20}), "for_each int false return");

    got.clear();
    xs.for_each_r([&](const int& k, const int&) -> bool {
        got.push_back(k);
        return k <= 0;
    });
    require_vec_eq(got, std::vector<int>({20, 9, 4, 2, 0}), "for_each_r bool break");

    got.clear();
    xs.for_each_r([&](const int& k, const int&) -> void { got.push_back(k); });
    require_vec_eq(got, std::vector<int>({20, 9, 4, 2, 0, -1, -3, -8}), "for_each_r void full scan");

    xs.for_each([](const int&, int& v) { v += 1; });
    for (auto& [k, v] : mm) ++v;
    check_all_apis(xs, mm, "mutable for_each value update");
}

void test_deterministic_updates() {
    btree_map<int, int, 4, 2> xs;
    std::map<int, int> mm;
    const std::vector<int> values = {
        0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6, -6, 7, -7,
        8, -8, 9, -9, 10, -10, 11, -11, 12, -12, 13, -13
    };
    for (int x : values) {
        xs.insert(x, x * 10 + 1);
        mm.emplace(x, x * 10 + 1);
        check_all_apis(xs, mm, "deterministic insert");
    }
    for (int x : {0, 13, -13, 100, -100, 5, -5, 1, -1, 12, -12, 2, -2}) {
        xs.erase(x);
        mm.erase(x);
        check_all_apis(xs, mm, "deterministic erase/missing erase");
    }
    for (int x : values) {
        xs.insert_or_assign(x, x * 100 + 2);
        mm.insert_or_assign(x, x * 100 + 2);
    }
    check_all_apis(xs, mm, "deterministic refill assign");
}

void test_small_load_random_exhaustive() {
    btree_map<int, int, 4, 2> xs;
    std::map<int, int> mm;
    std::mt19937 rng(123456789u);
    std::uniform_int_distribution<int> key_dist(-90, 90);
    std::uniform_int_distribution<int> val_dist(-100000, 100000);
    std::uniform_int_distribution<int> op_dist(0, 14);

    for (int step = 0; step < 100000; ++step) {
        const int op = op_dist(rng);
        const int key = key_dist(rng);
        const int val = val_dist(rng);
        if (op <= 2) {
            xs.insert(key, val);
            mm.emplace(key, val);
        } else if (op <= 4) {
            xs.insert_or_assign(key, val);
            mm.insert_or_assign(key, val);
        } else if (op <= 6) {
            xs.erase(key);
            mm.erase(key);
        } else if (op == 7) {
            xs[key] += 1;
            mm[key] += 1;
        } else if (op == 8) {
            require_eq(xs.count(key), mm.count(key), "small random count");
        } else if (op == 9) {
            const auto vec = map_to_vector(mm);
            const std::size_t expected = static_cast<std::size_t>(
                std::lower_bound(vec.begin(), vec.end(), key,
                                 [](const Pair& p, int k) { return p.first < k; }) - vec.begin());
            require_eq(xs.rank(key), expected, "small random rank");
        } else if (op == 10) {
            const auto vec = map_to_vector(mm);
            const std::size_t expected = static_cast<std::size_t>(
                std::upper_bound(vec.begin(), vec.end(), key,
                                 [](int k, const Pair& p) { return k < p.first; }) - vec.begin());
            require_eq(xs.rank2(key), expected, "small random rank2");
        } else if (op == 11) {
            require_eq_opt(xs.less_equal(key), map_less_equal(mm, key), "small random less_equal");
        } else if (op == 12) {
            require_eq_opt(xs.greater_equal(key), map_greater_equal(mm, key), "small random greater_equal");
        } else if (op == 13) {
            const int* p = xs.find(key);
            auto it = mm.find(key);
            require((p == nullptr) == (it == mm.end()), "small random find nullness");
            if (p) require_eq(*p, it->second, "small random find value");
        } else {
            if (!mm.empty()) {
                const auto mn = xs.min();
                const auto mx = xs.max();
                require_eq(Pair{mn.first, mn.second}, Pair{mm.begin()->first, mm.begin()->second}, "small random min");
                require_eq(Pair{mx.first, mx.second}, Pair{mm.rbegin()->first, mm.rbegin()->second}, "small random max");
            }
            if (!mm.empty()) {
                std::uniform_int_distribution<std::size_t> kth_dist(0, mm.size() - 1);
                const std::size_t k = kth_dist(rng);
                const auto expected = map_to_vector(mm);
                require_eq(xs.kth(k), expected[k], "small random kth");
            }
        }
        if ((step % 257) == 0) check_all_apis(xs, mm, "small random periodic full check");
    }
    check_all_apis(xs, mm, "small random final full check");
}

void check_large_sample(const btree_map<int, int>& xs, const std::map<int, int>& mm, std::mt19937& rng,
                        const std::string& label) {
    const std::vector<Pair> v = map_to_vector(mm);
    require_eq(xs.size(), mm.size(), label + ": size");
    if (!mm.empty()) {
        const auto mn = xs.min();
        const auto mx = xs.max();
        require_eq(Pair{mn.first, mn.second}, Pair{mm.begin()->first, mm.begin()->second}, label + ": min");
        require_eq(Pair{mx.first, mx.second}, Pair{mm.rbegin()->first, mm.rbegin()->second}, label + ": max");
    }
    require_vec_eq(collect_forward(xs), v, label + ": for_each full order");
    std::vector<Pair> rv = v;
    std::reverse(rv.begin(), rv.end());
    require_vec_eq(collect_reverse(xs), rv, label + ": for_each_r full order");

    if (!v.empty()) {
        std::uniform_int_distribution<std::size_t> kth_dist(0, v.size() - 1);
        for (int i = 0; i < 128; ++i) {
            const std::size_t k = kth_dist(rng);
            require_eq(xs.kth(k), v[k], label + ": sampled kth");
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
            const int y = v[idx_dist(rng)].first;
            qs.push_back(y);
            if (y != std::numeric_limits<int>::min()) qs.push_back(y - 1);
            if (y != std::numeric_limits<int>::max()) qs.push_back(y + 1);
        }
    }
    for (int q : qs) {
        const std::size_t rk = static_cast<std::size_t>(
            std::lower_bound(v.begin(), v.end(), q,
                             [](const Pair& p, int key) { return p.first < key; }) - v.begin());
        const std::size_t rk2 = static_cast<std::size_t>(
            std::upper_bound(v.begin(), v.end(), q,
                             [](int key, const Pair& p) { return key < p.first; }) - v.begin());
        require_eq(xs.rank(q), rk, label + ": sampled rank");
        require_eq(xs.rank2(q), rk2, label + ": sampled rank2");
        require_eq(xs.count(q), mm.count(q), label + ": sampled count");
        const int* p = xs.find(q);
        auto it = mm.find(q);
        require((p == nullptr) == (it == mm.end()), label + ": sampled find nullness");
        if (p) require_eq(*p, it->second, label + ": sampled find value");
        require_eq_opt(xs.less_equal(q), map_less_equal(mm, q), label + ": sampled less_equal");
        require_eq_opt(xs.greater_equal(q), map_greater_equal(mm, q), label + ": sampled greater_equal");
    }
}

void test_large_random_against_std_map() {
    btree_map<int, int> xs;
    std::map<int, int> mm;
    std::mt19937 rng(987654321u);
    std::uniform_int_distribution<int> key_dist(-200000, 200000);
    std::uniform_int_distribution<int> val_dist(-1000000, 1000000);
    std::uniform_int_distribution<int> op_dist(0, 11);

    constexpr int operations = 300000;
    for (int step = 1; step <= operations; ++step) {
        const int op = op_dist(rng);
        const int key = key_dist(rng);
        const int val = val_dist(rng);
        if (op <= 2) {
            xs.insert(key, val);
            mm.emplace(key, val);
        } else if (op <= 4) {
            xs.insert_or_assign(key, val);
            mm.insert_or_assign(key, val);
        } else if (op <= 6) {
            xs.erase(key);
            mm.erase(key);
        } else if (op == 7) {
            xs[key] += 1;
            mm[key] += 1;
        } else if (op == 8) {
            require_eq(xs.count(key), mm.count(key), "large random count immediate");
        } else if (op == 9) {
            const int* p = xs.find(key);
            auto it = mm.find(key);
            require((p == nullptr) == (it == mm.end()), "large random find immediate nullness");
            if (p) require_eq(*p, it->second, "large random find immediate value");
        } else if (op == 10) {
            require_eq_opt(xs.less_equal(key), map_less_equal(mm, key), "large random less_equal immediate");
        } else {
            require_eq_opt(xs.greater_equal(key), map_greater_equal(mm, key), "large random greater_equal immediate");
        }

        if ((step % 5000) == 0) check_large_sample(xs, mm, rng, "large random sample step " + std::to_string(step));
    }
    check_large_sample(xs, mm, rng, "large random final");
}

} // namespace btree_map_self_test

namespace btree_map_self_benchmark {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using Pair = std::pair<u32, u32>;

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

std::vector<Pair> sorted_unique_pairs(std::size_t n, u32 mul = 2, u32 add = 0) {
    std::vector<Pair> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = {static_cast<u32>(i * mul + add), static_cast<u32>((i * 1103515245ULL + 12345ULL) & 0xffffffffULL)};
    return v;
}

std::vector<Pair> shuffled_unique_pairs(std::size_t n, u32 mul, u64 seed, u32 add = 0) {
    auto v = sorted_unique_pairs(n, mul, add);
    SplitMix64 rng(seed);
    for (std::size_t i = n; i > 1; --i) std::swap(v[i - 1], v[static_cast<std::size_t>(rng.next() % i)]);
    return v;
}

std::vector<u32> query_keys(std::size_t q, u32 mod, u64 seed) {
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
    Timer tb;
    std::vector<btree_map<u32, u32>> bv(q);
    cb += bv.size();
    const double mb = tb.ms();
    Timer ts;
    std::vector<std::map<u32, u32>> sv(q);
    cs += sv.size();
    const double ms = ts.ms();
    return {"default_construct", 0, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_construct_vector(std::size_t n, u64 seed) {
    const auto vals = shuffled_unique_pairs(n, 2, seed);
    Timer tb;
    btree_map<u32, u32> bm(vals);
    const double mb = tb.ms();
    u64 cb = bm.size();
    Timer ts;
    std::map<u32, u32> sm(vals.begin(), vals.end());
    const double ms = ts.ms();
    u64 cs = sm.size();
    return {"construct(vector)", n, 1, 1, 1, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_clear(std::size_t n, std::size_t q, u64 seed) {
    const auto vals = shuffled_unique_pairs(n, 2, seed);
    std::vector<btree_map<u32, u32>> bv;
    std::vector<std::map<u32, u32>> sv;
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
    const auto vals = shuffled_unique_pairs(q, 2, seed);
    btree_map<u32, u32> bm;
    std::map<u32, u32> sm;
    Timer tb;
    for (auto [k, v] : vals) bm.insert(k, v);
    const double mb = tb.ms();
    u64 cb = bm.size();
    Timer ts;
    for (auto [k, v] : vals) sm.emplace(k, v);
    const double ms = ts.ms();
    u64 cs = sm.size();
    return {"insert(unique,empty)", q, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_insert_missing(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_pairs(n, 2, 0);
    const auto vals = shuffled_unique_pairs(q, 2, seed, 1);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    Timer tb;
    for (auto [k, v] : vals) bm.insert(k, v);
    const double mb = tb.ms();
    u64 cb = bm.size();
    Timer ts;
    for (auto [k, v] : vals) sm.emplace(k, v);
    const double ms = ts.ms();
    u64 cs = sm.size();
    return {"insert(missing)", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_insert_duplicate(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_pairs(n, 2, 0);
    const auto qs = query_keys(q, static_cast<u32>(n), seed);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 i : qs) {
        bm.insert(init[i].first, init[i].second + 7);
        cb += bm.size();
    }
    const double mb = tb.ms();
    Timer ts;
    for (u32 i : qs) {
        sm.emplace(init[i].first, init[i].second + 7);
        cs += sm.size();
    }
    const double ms = ts.ms();
    return {"insert(duplicate)", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_insert_or_assign_present(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_pairs(n, 2, 0);
    const auto qs = query_keys(q, static_cast<u32>(n), seed);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 i : qs) {
        bm.insert_or_assign(init[i].first, i + 3);
        cb += *bm.find(init[i].first);
    }
    const double mb = tb.ms();
    Timer ts;
    for (u32 i : qs) {
        sm.insert_or_assign(init[i].first, i + 3);
        cs += sm.find(init[i].first)->second;
    }
    const double ms = ts.ms();
    return {"insert_or_assign(present)", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_erase_present(std::size_t n, u64 seed) {
    const auto init = sorted_unique_pairs(n, 2, 0);
    auto vals = init;
    SplitMix64 rng(seed);
    for (std::size_t i = vals.size(); i > 1; --i) std::swap(vals[i - 1], vals[static_cast<std::size_t>(rng.next() % i)]);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    Timer tb;
    for (auto [k, v] : vals) {
        (void)v;
        bm.erase(k);
    }
    const double mb = tb.ms();
    u64 cb = bm.size();
    Timer ts;
    for (auto [k, v] : vals) {
        (void)v;
        sm.erase(k);
    }
    const double ms = ts.ms();
    u64 cs = sm.size();
    return {"erase(present)", n, n, n, n, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_erase_missing(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_pairs(n, 2, 0);
    const auto vals = shuffled_unique_pairs(q, 2, seed, 1);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    Timer tb;
    for (auto [k, v] : vals) {
        (void)v;
        bm.erase(k);
    }
    const double mb = tb.ms();
    u64 cb = bm.size();
    Timer ts;
    for (auto [k, v] : vals) {
        (void)v;
        sm.erase(k);
    }
    const double ms = ts.ms();
    u64 cs = sm.size();
    return {"erase(missing)", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_size(std::size_t n, std::size_t q, u64 seed) {
    const auto init = shuffled_unique_pairs(n, 2, seed);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cb += bm.size();
    }
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cs += sm.size();
    }
    const double ms = ts.ms();
    return {"size", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_empty(std::size_t n, std::size_t q, u64 seed) {
    const auto init = shuffled_unique_pairs(n, 2, seed);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cb += bm.empty();
    }
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cs += sm.empty();
    }
    const double ms = ts.ms();
    return {"empty", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_min(std::size_t n, std::size_t q, u64 seed) {
    const auto init = shuffled_unique_pairs(n, 2, seed);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        const auto y = bm.min();
        cb += y.first + y.second + 1u;
    }
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cs += sm.begin()->first + sm.begin()->second + 1u;
    }
    const double ms = ts.ms();
    return {"min", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_max(std::size_t n, std::size_t q, u64 seed) {
    const auto init = shuffled_unique_pairs(n, 2, seed);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        const auto y = bm.max();
        cb += y.first + y.second + 1u;
    }
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cs += sm.rbegin()->first + sm.rbegin()->second + 1u;
    }
    const double ms = ts.ms();
    return {"max", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_for_each(std::size_t n, u64 seed) {
    const auto init = shuffled_unique_pairs(n, 2, seed);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    bm.for_each([&](const u32& k, const u32& v) { cb += k + v + 1u; });
    const double mb = tb.ms();
    Timer ts;
    for (auto [k, v] : sm) cs += k + v + 1u;
    const double ms = ts.ms();
    return {"for_each", n, n, n, n, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_for_each_r(std::size_t n, u64 seed) {
    const auto init = shuffled_unique_pairs(n, 2, seed);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    bm.for_each_r([&](const u32& k, const u32& v) { cb += k + v + 1u; });
    const double mb = tb.ms();
    Timer ts;
    for (auto it = sm.rbegin(); it != sm.rend(); ++it) cs += it->first + it->second + 1u;
    const double ms = ts.ms();
    return {"for_each_r", n, n, n, n, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_count(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_pairs(n, 2, 0);
    const auto qs = query_keys(q, static_cast<u32>(4 * n), seed);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 k : qs) cb += bm.count(k);
    const double mb = tb.ms();
    Timer ts;
    for (u32 k : qs) cs += sm.count(k);
    const double ms = ts.ms();
    return {"count", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_contains(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_pairs(n);
    const auto qs = query_keys(q, static_cast<u32>(4 * n), seed);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 key : qs) cb += bm.contains(key);
    const double mb = tb.ms();
    Timer ts;
    for (u32 key : qs) cs += sm.contains(key);
    const double ms = ts.ms();
    return {"contains", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_find(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_pairs(n, 2, 0);
    const auto qs = query_keys(q, static_cast<u32>(4 * n), seed);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 k : qs) {
        const u32* p = bm.find(k);
        cb += p ? (*p + 1u) : 0u;
    }
    const double mb = tb.ms();
    Timer ts;
    for (u32 k : qs) {
        auto it = sm.find(k);
        cs += it == sm.end() ? 0u : (it->second + 1u);
    }
    const double ms = ts.ms();
    return {"find", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_operator_existing(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_pairs(n, 2, 0);
    const auto qs = query_keys(q, static_cast<u32>(n), seed);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 i : qs) cb += bm[init[i].first];
    const double mb = tb.ms();
    Timer ts;
    for (u32 i : qs) cs += sm[init[i].first];
    const double ms = ts.ms();
    return {"operator[](existing)", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_less_equal(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_pairs(n, 2, 0);
    const auto qs = query_keys(q, static_cast<u32>(4 * n), seed);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 k : qs) {
        auto y = bm.less_equal(k);
        cb += y ? (y->first + y->second + 1u) : 0u;
    }
    const double mb = tb.ms();
    Timer ts;
    for (u32 k : qs) {
        auto it = sm.upper_bound(k);
        if (it != sm.begin()) {
            --it;
            cs += it->first + it->second + 1u;
        }
    }
    const double ms = ts.ms();
    return {"less_equal", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_greater_equal(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_pairs(n, 2, 0);
    const auto qs = query_keys(q, static_cast<u32>(4 * n), seed);
    btree_map<u32, u32> bm(init);
    std::map<u32, u32> sm(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 k : qs) {
        auto y = bm.greater_equal(k);
        cb += y ? (y->first + y->second + 1u) : 0u;
    }
    const double mb = tb.ms();
    Timer ts;
    for (u32 k : qs) {
        auto it = sm.lower_bound(k);
        cs += it == sm.end() ? 0u : (it->first + it->second + 1u);
    }
    const double ms = ts.ms();
    return {"greater_equal", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_kth(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_pairs(n, 2, 0);
    const auto qs = query_indices(q, n, seed);
    btree_map<u32, u32> bm(init);
    u64 cb = 0;
    Timer tb;
    for (std::size_t k : qs) {
        auto p = bm.kth(k);
        cb += p.first + p.second + 1u;
    }
    const double mb = tb.ms();
    return {"kth", n, q, q, 0, mb, -1.0, cb, 0, "btree_only"};
}

BenchRow bench_rank(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_pairs(n, 2, 0);
    const auto qs = query_keys(q, static_cast<u32>(4 * n), seed);
    btree_map<u32, u32> bm(init);
    u64 cb = 0;
    Timer tb;
    for (u32 k : qs) cb += bm.rank(k);
    const double mb = tb.ms();
    return {"rank", n, q, q, 0, mb, -1.0, cb, 0, "btree_only"};
}

BenchRow bench_rank2(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_pairs(n, 2, 0);
    const auto qs = query_keys(q, static_cast<u32>(4 * n), seed);
    btree_map<u32, u32> bm(init);
    u64 cb = 0;
    Timer tb;
    for (u32 k : qs) cb += bm.rank2(k);
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
    std::nth_element(b_rows.begin(), b_rows.begin() + reps / 2, b_rows.end(), [](const BenchRow& a, const BenchRow& b) {
        return a.btree_ms < b.btree_ms;
    });
    out.btree_ms = b_rows[reps / 2].btree_ms;
    out.btree_ops = b_rows[reps / 2].btree_ops;

    if (rows.front().std_ms >= 0.0) {
        auto s_rows = rows;
        std::nth_element(s_rows.begin(), s_rows.begin() + reps / 2, s_rows.end(), [](const BenchRow& a, const BenchRow& b) {
            return a.std_ms < b.std_ms;
        });
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
    rows.push_back(median_of(reps, [&](int i) { return bench_insert_or_assign_present(n, q, 550 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_erase_present(n, 600 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_erase_missing(n, q, 700 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_size(n, q_fast, 800 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_empty(n, q_fast, 850 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_min(n, q_fast, 900 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_max(n, q_fast, 1000 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_for_each(n, 1100 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_for_each_r(n, 1200 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_kth(n_small, q_small, 1500 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_rank(n, q, 1600 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_rank2(n, q, 1700 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_contains(n, q, 1800 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_count(n, q, 1800 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_find(n, q, 1850 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_operator_existing(n, q, 1875 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_less_equal(n, q, 1900 + i); }));
    rows.push_back(median_of(reps, [&](int i) { return bench_greater_equal(n, q, 2000 + i); }));

    std::cout << "benchmarks begin\n";
    std::cout << "compiler," << __VERSION__ << "\n";
    std::cout << "flags,-std=c++20 -O2 -march=native -DNDEBUG\n";
    std::cout << "repetitions," << reps << "\n";
    std::cout << "api,N,Q,btree_ms,std_map_ms,btree_ns_per_op,std_map_ns_per_op,std_over_btree,status\n";
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

} // namespace btree_map_self_benchmark

int main() {
    using namespace btree_map_self_test;

    test_empty_singleton_clear();
    std::cout << "edge_empty_singleton_clear: ok\n";

    test_constructor_duplicates_and_boundaries();
    std::cout << "edge_constructor_duplicates_boundaries: ok\n";

    test_iteration_break_modes();
    std::cout << "edge_iteration_callbacks: ok\n";

    test_deterministic_updates();
    std::cout << "deterministic_update_edges: ok\n";

    test_small_load_random_exhaustive();
    std::cout << "random_small_load_full_check_100000_ops: ok\n";

    test_large_random_against_std_map();
    std::cout << "random_default_load_sampled_300000_ops: ok\n";

    std::cout << "all tests passed\n";

    btree_map_self_benchmark::run_all_benchmarks();
    return 0;
}
#endif

// 実行結果(atcoder)
// benchmarks begin
// compiler,15.2.0
// flags,-std=c++20 -O2 -march=native -DNDEBUG
// repetitions,3
// api,N,Q,btree_ms,std_map_ms,btree_ns_per_op,std_map_ns_per_op,std_over_btree,status
// default_construct,0,100000,3.347,1.457,33.475,14.574,0.435,ok
// construct(vector),200000,1,15.687,40.635,15687372.000,40635179.000,2.590,ok
// clear,100000,5,0.006,14.551,1240.200,2910178.400,2346.540,ok
// insert(unique,empty),200000,200000,24.548,54.359,122.739,271.794,2.214,ok
// insert(missing),200000,500000,84.780,214.817,169.561,429.634,2.534,ok
// insert(duplicate),200000,500000,50.677,157.602,101.354,315.204,3.110,ok
// insert_or_assign(present),200000,500000,70.248,207.110,140.497,414.220,2.948,ok
// erase(present),200000,200000,22.357,57.548,111.787,287.742,2.574,ok
// erase(missing),200000,500000,27.228,66.303,54.455,132.606,2.435,ok
// size,200000,5000000,1.573,1.581,0.315,0.316,1.005,ok
// empty,200000,5000000,3.161,1.578,0.632,0.316,0.499,ok
// min,200000,5000000,3.154,1.846,0.631,0.369,0.585,ok
// max,200000,5000000,3.178,14.115,0.636,2.823,4.441,ok
// for_each,200000,200000,0.060,8.778,0.300,43.892,146.401,ok
// for_each_r,200000,200000,0.098,8.604,0.491,43.022,87.544,ok
// kth,50000,100000,1.258,NA,12.578,NA,NA,btree_only
// rank,200000,500000,32.405,NA,64.811,NA,NA,btree_only
// rank2,200000,500000,32.285,NA,64.569,NA,NA,btree_only
// contains,200000,500000,29.645,82.773,59.291,165.547,2.792,ok
// count,200000,500000,29.009,100.196,58.018,200.392,3.454,ok
// find,200000,500000,30.743,83.825,61.485,167.651,2.727,ok
// operator[](existing),200000,500000,49.609,158.928,99.218,317.855,3.204,ok
// less_equal,200000,500000,29.384,84.743,58.768,169.485,2.884,ok
// greater_equal,200000,500000,29.125,98.413,58.251,196.826,3.379,ok
