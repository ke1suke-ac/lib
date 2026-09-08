/*
 * hash_map: 競技プログラミング向けの整数系 key 特化 HashMap 実装。
 * bool を除く整数型、またはそれらを要素に持つ std::pair / std::tuple を key として扱い、
 * 軽量な乗算 hash、7-bit fingerprint 付き control byte、open addressing により平均 O(1) の検索・更新を行う。
 * 反復順序は未規定で、rehash を伴う更新後は value への参照・ポインタが無効になる場合がある。
 * デフォルト構築ではヒープ確保を行わず、オブジェクト本体はテーブルへのポインタ 1 個だけを保持する。
 *
 * テンプレートパラメータ:
 *   K : bool を除く整数型、またはそれらを要素に持つ std::pair / std::tuple 型
 *   V : value 型。move / copy が軽量な型ほど更新・rehash が高速
 */
#pragma once

#include <bits/stdc++.h>

template <class K, class V>
struct hash_map {
private:
    template <class T>
    inline static constexpr bool is_key_integer_v =
        !std::is_same_v<std::remove_cv_t<T>, bool> &&
        (std::is_integral_v<std::remove_cv_t<T>> ||
         std::is_same_v<std::remove_cv_t<T>, __int128_t> ||
         std::is_same_v<std::remove_cv_t<T>, __uint128_t>);

    template <class T>
    struct is_valid_key : std::bool_constant<is_key_integer_v<T>> {};

    template <class A, class B>
    struct is_valid_key<std::pair<A, B>>
        : std::bool_constant<is_key_integer_v<A> && is_key_integer_v<B>> {};

    template <class... Ts>
    struct is_valid_key<std::tuple<Ts...>>
        : std::bool_constant<(sizeof...(Ts) > 0) && (is_key_integer_v<Ts> && ...)> {};

    static_assert(is_valid_key<std::remove_cv_t<K>>::value,
                  "hash_map の key は bool を除く整数型、またはそれらを要素に持つ pair / tuple に限る");

    inline static constexpr std::uint64_t hash_mul = 0x9e3779b97f4a7c15ULL;
    inline static constexpr std::uint64_t hash_add = 0x517cc1b727220a95ULL;

    // 単一整数 key は pair / tuple 合成より短い経路で hash する
    static std::uint64_t mix64(std::uint64_t x) {
        // 競技プログラミング向けの軽量 mix。強い乱択耐性より定数倍を優先する
        x ^= x >> 32;
        return x * hash_mul;
    }

    static void append_word(std::uint64_t& h, std::uint64_t x) {
        // boost::hash_combine 風に、1 component あたり軽い整数演算だけで混ぜる
        h ^= mix64(x) + hash_add + (h << 6) + (h >> 2);
    }

    template <class T>
    requires is_key_integer_v<T>
    static void append_integer(std::uint64_t& h, const T& value) {
        using R = std::remove_cv_t<T>;
        if constexpr (std::is_same_v<R, __int128_t> || std::is_same_v<R, __uint128_t>) {
            const __uint128_t x = static_cast<__uint128_t>(value);
            append_word(h, static_cast<std::uint64_t>(x));
            append_word(h, static_cast<std::uint64_t>(x >> 64));
        } else {
            using U = std::make_unsigned_t<R>;
            append_word(h, static_cast<U>(value));
        }
    }

    template <class T>
    requires is_key_integer_v<T>
    static std::uint64_t integer_hash(const T& value) {
        using R = std::remove_cv_t<T>;
        if constexpr (std::is_same_v<R, __int128_t> || std::is_same_v<R, __uint128_t>) {
            const __uint128_t x = static_cast<__uint128_t>(value);
            const std::uint64_t lo = static_cast<std::uint64_t>(x);
            const std::uint64_t hi = static_cast<std::uint64_t>(x >> 64);
            return mix64(lo ^ std::rotl(hi, 32));
        } else {
            using U = std::make_unsigned_t<R>;
            return mix64(static_cast<U>(value));
        }
    }

    static std::uint64_t hash_key(const K& key) {
        if constexpr (is_key_integer_v<K>) {
            return integer_hash(key);
        } else {
            std::uint64_t h = 0x243f6a8885a308d3ULL;
            std::apply([&](const auto&... xs) { (append_integer(h, xs), ...); }, key);
            return mix64(h);
        }
    }

public:
    using key_type = K;
    using mapped_type = V;
    using value_type = std::pair<K, V>;

private:
    struct Entry {
        K key;
        V value;
    };

    // 通常の競技プログラミング用途に絞り、over-aligned value はサポートしない
    static_assert(alignof(Entry) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
                  "hash_map は over-aligned value をサポートしない");

    struct Table {
        std::size_t size;
        std::size_t mask;
        std::size_t growth_left;
    };

    struct ProbeResult {
        std::size_t slot;
        bool found;
        bool uses_empty;
    };

    static constexpr std::uint8_t empty_ctrl = 0x80;
    static constexpr std::uint8_t deleted_ctrl = 0xfe;
    static constexpr std::size_t group_width = 8;
    static constexpr std::size_t min_capacity = 8;
    static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();
    static constexpr std::uint64_t byte_ones = 0x0101010101010101ULL;
    static constexpr std::uint64_t byte_high_bits = 0x8080808080808080ULL;

    Table* table = nullptr;

public:
    // 空の map をヒープ確保なしで構築する。O(1)
    hash_map() = default;

    // 配列から map を構築する。同じ key は入力で最初に現れた値を残す。期待 O(n)
    explicit hash_map(std::vector<value_type> a) {
        reserve(a.size());
        for (auto& [key, value] : a) insert(std::move(key), std::move(value));
    }

    // map を複製する。期待 O(n)
    hash_map(const hash_map& rhs) {
        reserve(rhs.size());
        rhs.for_each([&](const K& key, const V& value) { insert(key, value); });
    }

    // map の所有権を移動する。O(1)
    hash_map(hash_map&& rhs) noexcept : table(std::exchange(rhs.table, nullptr)) {}

    // map を複製して代入する。期待 O(n + capacity)
    hash_map& operator=(const hash_map& rhs) {
        if (this == &rhs) return *this;
        hash_map tmp(rhs);
        std::swap(table, tmp.table);
        return *this;
    }

    // map の所有権を移動して代入する。O(capacity)
    hash_map& operator=(hash_map&& rhs) noexcept {
        if (this == &rhs) return *this;
        release();
        table = std::exchange(rhs.table, nullptr);
        return *this;
    }

    // 全要素と内部メモリを破棄する。O(capacity)
    ~hash_map() { release(); }

    // 全要素を削除して内部 capacity を保持する。O(capacity)
    void clear() {
        if (!table) return;
        destroy_entries(table);
        const std::size_t cap = capacity(table);
        std::memset(ctrl(table), empty_ctrl, cap + group_width);
        table->size = 0;
        table->growth_left = max_occupied(cap);
    }

    // expected_size 要素を rehash なしで保持できる capacity を確保する。変更時は期待 O(n)、変更なしなら O(1)
    void reserve(std::size_t expected_size) {
        if (expected_size == 0 || (table && max_occupied(capacity(table)) >= expected_size)) return;
        rehash(capacity_for_size(expected_size));
    }

    // key が存在しなければ key-value を挿入する。既に存在する場合は何もしない。期待償却 O(1)
    void insert(K key, V value) { insert_impl(std::move(key), std::move(value), false); }

    // key-value を挿入する。既に存在する場合は value を代入する。期待償却 O(1)
    void insert_or_assign(K key, V value) { insert_impl(std::move(key), std::move(value), true); }

    // key を削除する。存在しない場合は何もしない。期待 O(1)
    void erase(const K& key) {
        if (!table) return;
        const std::uint64_t hash = hash_key(key);
        const std::size_t slot = probe_slot<false>(table, key, hash);
        if (slot == npos) return;
        std::destroy_at(entries(table) + slot);
        set_ctrl(table, slot, deleted_ctrl);
        --table->size;
    }

    // 現在の要素数を返す。O(1)
    std::size_t size() const { return table ? table->size : 0; }

    // map が空なら true を返す。O(1)
    bool empty() const { return !table || table->size == 0; }

    // 未規定順で全 key-value へ callback(key, value) を適用する。bool 変換可能な戻り値が true なら中断する。O(capacity)
    template <class F>
    void for_each(F&& callback) {
        if (!table) return;
        for_each_impl(*this, callback);
    }

    // 未規定順で全 key-value へ callback(key, value) を適用する。bool 変換可能な戻り値が true なら中断する。O(capacity)
    template <class F>
    void for_each(F&& callback) const {
        if (!table) return;
        for_each_impl(*this, callback);
    }

    // key が存在すれば value へのポインタを返す。存在しない場合は nullptr。期待 O(1)
    V* find(const K& key) {
        if (!table) return nullptr;
        const std::size_t slot = probe_slot<false>(table, key, hash_key(key));
        return slot == npos ? nullptr : &entries(table)[slot].value;
    }

    // key が存在すれば value へのポインタを返す。存在しない場合は nullptr。期待 O(1)
    const V* find(const K& key) const {
        if (!table) return nullptr;
        const std::size_t slot = probe_slot<false>(table, key, hash_key(key));
        return slot == npos ? nullptr : &entries(table)[slot].value;
    }

    // key に対応する value への参照を返す。存在しない場合は value を値初期化して挿入する。期待償却 O(1)
    V& operator[](const K& key) {
        // 既存要素と挿入候補を 1 回の probe で決定する
        if (!table) table = allocate_table(min_capacity);
        const std::uint64_t hash = hash_key(key);
        ProbeResult result = probe_slot<true>(table, key, hash);
        if (result.found) return entries(table)[result.slot].value;
        prepare_absent_insert(key, hash, result);

        // 空き slot へ key と値初期化した value を構築する
        Entry* slot = entries(table) + result.slot;
        std::construct_at(slot, Entry{key, V{}});
        finish_insert(result.slot, fingerprint(hash), result.uses_empty);
        return slot->value;
    }

    // key が存在するなら true を返す。期待 O(1)
    bool contains(const K& key) const {
        if (!table) return false;
        return probe_slot<false>(table, key, hash_key(key)) != npos;
    }

    // key が存在するかを 0 または 1 で返す。期待 O(1)
    std::size_t count(const K& key) const { return contains(key); }

private:
    static constexpr std::size_t capacity(const Table* t) { return t->mask + 1; }

    static constexpr std::size_t max_occupied(std::size_t cap) { return cap - cap / 8; }

    static std::size_t capacity_for_size(std::size_t expected_size) {
        [[maybe_unused]] constexpr std::size_t max_cap =
            std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1);
        assert(expected_size <= max_occupied(max_cap));
        const std::size_t need = expected_size + (expected_size + 6) / 7;
        return std::bit_ceil(std::max(min_capacity, need));
    }

    static std::size_t entries_offset(std::size_t cap) {
        const std::size_t raw = sizeof(Table) + cap + group_width;
        return (raw + alignof(Entry) - 1) & ~(alignof(Entry) - 1);
    }

    template <class TablePtr>
    static auto* ctrl(TablePtr* t) {
        using Byte = std::conditional_t<std::is_const_v<TablePtr>, const std::uint8_t, std::uint8_t>;
        using Raw = std::conditional_t<std::is_const_v<TablePtr>, const std::byte, std::byte>;
        return reinterpret_cast<Byte*>(reinterpret_cast<Raw*>(t) + sizeof(Table));
    }

    template <class TablePtr>
    static auto* entries(TablePtr* t) {
        using Slot = std::conditional_t<std::is_const_v<TablePtr>, const Entry, Entry>;
        using Raw = std::conditional_t<std::is_const_v<TablePtr>, const std::byte, std::byte>;
        return reinterpret_cast<Slot*>(reinterpret_cast<Raw*>(t) + entries_offset(capacity(t)));
    }

    static Table* allocate_table(std::size_t cap) {
        // control byte と Entry 配列を 1 回の allocation で確保する
        const std::size_t offset = entries_offset(cap);
        assert(cap <= (std::numeric_limits<std::size_t>::max() - offset) / sizeof(Entry));
        const std::size_t bytes = offset + cap * sizeof(Entry);
        void* raw = ::operator new(bytes);
        // 全 control byte と折り返し読み込み用の複製領域を empty にする
        Table* t = std::construct_at(static_cast<Table*>(raw), Table{0, cap - 1, max_occupied(cap)});
        std::memset(ctrl(t), empty_ctrl, cap + group_width);
        return t;
    }

    static void free_storage(Table* t) {
        std::destroy_at(t);
        ::operator delete(t);
    }

    static std::uint64_t load_group(const std::uint8_t* p) {
        std::uint64_t x;
        std::memcpy(&x, p, sizeof(x));
        return x;
    }

    static std::uint64_t match_byte_candidates(std::uint64_t word, std::uint8_t byte) {
        const std::uint64_t x = word ^ (byte_ones * byte);
        return (x - byte_ones) & ~x & byte_high_bits;
    }

    static std::uint8_t fingerprint(std::uint64_t hash) { return static_cast<std::uint8_t>(hash & 0x7f); }

    static void set_ctrl(Table* t, std::size_t slot, std::uint8_t value) {
        std::uint8_t* c = ctrl(t);
        c[slot] = value;
        if (slot < group_width) c[capacity(t) + slot] = value;
    }

    static bool is_occupied(std::uint8_t value) { return value < empty_ctrl; }

    static std::size_t first_exact_byte(const std::uint8_t* group, std::uint64_t candidates,
                                        std::uint8_t value) {
        while (candidates) {
            const std::size_t idx = __builtin_ctzll(candidates) >> 3;
            if (group[idx] == value) return idx;
            candidates &= candidates - 1;
        }
        return npos;
    }

    template <bool ForInsert>
    static auto probe_slot(const Table* t, const K& key, std::uint64_t hash) {
        const std::size_t mask = t->mask;
        const std::uint8_t h2 = fingerprint(hash);
        const std::uint8_t* c = ctrl(t);
        const Entry* es = entries(t);
        const std::size_t start = (hash >> 7) & mask;
        std::size_t first_deleted = npos;
        std::size_t offset = 0;
        std::size_t step = 0;

        for (;;) {
            // fingerprint が一致する候補だけ key 本体を比較する
            const std::size_t pos = (start + offset) & mask;
            const std::uint64_t word = load_group(c + pos);
            std::uint64_t matches = match_byte_candidates(word, h2);
            while (matches) {
                const std::size_t idx = __builtin_ctzll(matches) >> 3;
                const std::size_t slot = (pos + idx) & mask;
                if (c[pos + idx] == h2 && es[slot].key == key) {
                    if constexpr (ForInsert) return ProbeResult{slot, true, false};
                    else return slot;
                }
                matches &= matches - 1;
            }

            const std::uint64_t empty_candidates = match_byte_candidates(word, empty_ctrl);
            const std::size_t empty_idx = first_exact_byte(c + pos, empty_candidates, empty_ctrl);
            if constexpr (ForInsert) {
                // empty より手前にある最初の tombstone だけを再利用候補にする
                if (first_deleted == npos) {
                    const std::uint64_t deleted_candidates = match_byte_candidates(word, deleted_ctrl);
                    const std::size_t deleted_idx = first_exact_byte(c + pos, deleted_candidates, deleted_ctrl);
                    if (deleted_idx != npos && (empty_idx == npos || deleted_idx < empty_idx))
                        first_deleted = (pos + deleted_idx) & mask;
                }
                if (empty_idx != npos) {
                    if (first_deleted != npos) return ProbeResult{first_deleted, false, false};
                    return ProbeResult{(pos + empty_idx) & mask, false, true};
                }
            } else if (empty_idx != npos) {
                // 一度も使われていない slot があれば、この先に対象 key は存在しない
                return npos;
            }

            step += group_width;
            offset += step;
        }
    }

    void prepare_absent_insert(const K& key, std::uint64_t hash, ProbeResult& result) {
        if (table->growth_left != 0) return;

        // tombstone が少なく、その場へ挿入できる場合は rehash を避ける
        const std::size_t cap = capacity(table);
        const std::size_t deleted = max_occupied(cap) - table->size;
        if (!result.uses_empty && deleted <= cap / 8) return;

        // tombstone が多ければ同容量で整理し、少なければ容量を拡大する
        std::size_t next_cap = cap;
        if (deleted <= cap / 8) {
            assert(cap <= std::numeric_limits<std::size_t>::max() / 2);
            next_cap = cap * 2;
        }
        rehash(next_cap);
        result = probe_slot<true>(table, key, hash);
    }

    void finish_insert(std::size_t slot, std::uint8_t h2, bool used_empty) {
        set_ctrl(table, slot, h2);
        ++table->size;
        if (used_empty) --table->growth_left;
    }

    void insert_impl(K key, V value, bool assign_if_exists) {
        // 同一 key の検索と挿入位置の決定を 1 回の probe で行う
        if (!table) table = allocate_table(min_capacity);
        const std::uint64_t hash = hash_key(key);
        ProbeResult result = probe_slot<true>(table, key, hash);
        if (result.found) {
            if (assign_if_exists) entries(table)[result.slot].value = std::move(value);
            return;
        }

        // 必要なら rehash した後、未構築 slot へ Entry を直接構築する
        prepare_absent_insert(key, hash, result);
        std::construct_at(entries(table) + result.slot,
                          Entry{std::move(key), std::move(value)});
        finish_insert(result.slot, fingerprint(hash), result.uses_empty);
    }

    void rehash(std::size_t new_capacity) {
        // 新しい control byte と Entry 配列を 1 領域で確保する
        Table* old = table;
        Table* fresh = allocate_table(new_capacity);

        if (old) {
            // 使用中 Entry だけを再 hash し、value を move して詰め直す
            Entry* old_entries = entries(old);
            const std::uint8_t* old_ctrl = ctrl(old);
            const std::size_t old_capacity = capacity(old);
            for (std::size_t i = 0; i < old_capacity; ++i) {
                if (!is_occupied(old_ctrl[i])) continue;
                Entry& old_entry = old_entries[i];
                const std::uint64_t hash = hash_key(old_entry.key);
                const std::size_t slot = probe_slot<true>(fresh, old_entry.key, hash).slot;
                std::construct_at(entries(fresh) + slot,
                                  Entry{std::move(old_entry.key), std::move(old_entry.value)});
                set_ctrl(fresh, slot, fingerprint(hash));
                ++fresh->size;
                --fresh->growth_left;
                std::destroy_at(&old_entry);
            }
            // 全 Entry の移動完了後に旧領域をまとめて解放する
            free_storage(old);
        }

        table = fresh;
    }

    static void destroy_entries(Table* t) {
        if constexpr (!std::is_trivially_destructible_v<Entry>) {
            Entry* es = entries(t);
            const std::uint8_t* c = ctrl(t);
            const std::size_t cap = capacity(t);
            for (std::size_t i = 0; i < cap; ++i)
                if (is_occupied(c[i])) std::destroy_at(es + i);
        }
    }

    void release() {
        if (!table) return;
        destroy_entries(table);
        free_storage(table);
        table = nullptr;
    }

    template <class Self, class F>
    static void for_each_impl(Self& self, F& callback) {
        using SelfEntry = std::conditional_t<std::is_const_v<Self>, const Entry, Entry>;
        SelfEntry* es = entries(self.table);
        const std::uint8_t* c = ctrl(self.table);
        const std::size_t cap = capacity(self.table);

        // control byte を 8 個ずつ読み、使用中 slot だけ callback へ渡す
        for (std::size_t base = 0; base < cap; base += group_width) {
            const std::uint64_t word = load_group(c + base);
            std::uint64_t occupied = (~word) & byte_high_bits;
            while (occupied) {
                const std::size_t idx = __builtin_ctzll(occupied) >> 3;
                SelfEntry& e = es[base + idx];
                if (invoke_callback(callback, e.key, e.value)) return;
                occupied &= occupied - 1;
            }
        }
    }

    template <class F, class ValueRef>
    static bool invoke_callback(F& callback, const K& key, ValueRef&& value) {
        if constexpr (std::is_void_v<std::invoke_result_t<F&, const K&, ValueRef>>) {
            std::invoke(callback, key, std::forward<ValueRef>(value));
            return false;
        } else {
            // void 以外の戻り値は bool 互換であることを要求する
            auto&& ret = std::invoke(callback, key, std::forward<ValueRef>(value));
            return static_cast<bool>(ret);
        }
    }
};


#if __INCLUDE_LEVEL__ == 0
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <unordered_map>

namespace hash_map_selftest {

[[noreturn]] void fail(const std::string& label) {
    std::cerr << "FAILED: " << label << '\n';
    std::abort();
}

void require(bool condition, const std::string& label) {
    if (!condition) fail(label);
}

template <class A, class B>
void require_equal(const A& actual, const B& expected, const std::string& label) {
    if (!(actual == expected)) fail(label);
}

template <class K>
struct shared_hasher {
    std::size_t operator()(const K& key) const {
        if constexpr (std::is_integral_v<std::remove_cv_t<K>>) {
            return static_cast<std::size_t>(key);
        } else {
            std::uint64_t h = 0x243f6a8885a308d3ULL;
            std::apply([&](const auto&... xs) {
                ((h ^= static_cast<std::uint64_t>(
                           std::hash<std::remove_cvref_t<decltype(xs)>>{}(xs)) +
                       0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2)),
                 ...);
            }, key);
            return static_cast<std::size_t>(h);
        }
    }
};

template <class K, class V, class RefMap>
void check_all_apis(const hash_map<K, V>& xs, const RefMap& ref, const std::string& label) {
    require_equal(xs.size(), ref.size(), label + ": size");
    require_equal(xs.empty(), ref.empty(), label + ": empty");

    std::set<K> seen;
    std::size_t visited = 0;
    xs.for_each([&](const K& key, const V& value) {
        auto it = ref.find(key);
        require(it != ref.end(), label + ": for_each key");
        require_equal(value, it->second, label + ": for_each value");
        require(seen.insert(key).second, label + ": for_each duplicate");
        ++visited;
    });
    require_equal(visited, ref.size(), label + ": for_each count");

    for (const auto& [key, value] : ref) {
        const V* p = xs.find(key);
        require(p != nullptr, label + ": find existing");
        require_equal(*p, value, label + ": find value");
        require(xs.contains(key), label + ": contains existing");
        require_equal(xs.count(key), std::size_t{1}, label + ": count existing");
    }
}

struct NotBoolConvertible {
    int value = 0;
};

struct alignas(64) WideValue {
    std::array<std::uint64_t, 8> data{};

    bool operator==(const WideValue&) const = default;
};

template <class T>
inline constexpr bool test_is_key_integer_v =
    !std::is_same_v<std::remove_cv_t<T>, bool> &&
    (std::is_integral_v<std::remove_cv_t<T>> ||
     std::is_same_v<std::remove_cv_t<T>, __int128_t> ||
     std::is_same_v<std::remove_cv_t<T>, __uint128_t>);

template <class T>
struct test_is_valid_key : std::bool_constant<test_is_key_integer_v<T>> {};

template <class A, class B>
struct test_is_valid_key<std::pair<A, B>>
    : std::bool_constant<test_is_key_integer_v<A> && test_is_key_integer_v<B>> {};

template <class... Ts>
struct test_is_valid_key<std::tuple<Ts...>>
    : std::bool_constant<(sizeof...(Ts) > 0) && (test_is_key_integer_v<Ts> && ...)> {};

template <class T>
inline constexpr bool test_is_valid_key_v = test_is_valid_key<std::remove_cv_t<T>>::value;

void test_empty_singleton_clear() {
    static_assert(sizeof(hash_map<int, int>) == sizeof(void*));
    static_assert(test_is_valid_key_v<int>);
    static_assert(test_is_valid_key_v<std::pair<int, unsigned>>);
    static_assert(test_is_valid_key_v<std::tuple<int, unsigned, long long>>);
    static_assert(!test_is_valid_key_v<std::pair<int, std::tuple<int, int>>>);
    static_assert(!test_is_valid_key_v<bool>);
    static_assert(!test_is_valid_key_v<std::pair<int, bool>>);
    static_assert(!test_is_valid_key_v<std::tuple<int, bool, unsigned>>);
    static_assert(!test_is_valid_key_v<double>);
    static_assert(!test_is_valid_key_v<std::string>);

    hash_map<int, bool> bool_value_ok;
    require(bool_value_ok.empty(), "bool value is allowed");

    hash_map<int, int> xs;
    std::unordered_map<int, int> ref;
    check_all_apis(xs, ref, "empty");
    require(xs.find(0) == nullptr, "empty find");
    require(!xs.contains(0), "empty contains");
    require_equal(xs.count(0), std::size_t{0}, "empty count");

    int calls = 0;
    xs.for_each([&](const int&, const int&) { ++calls; });
    require_equal(calls, 0, "empty for_each");
    xs.clear();
    check_all_apis(xs, ref, "empty clear");

    xs.insert(7, 70);
    ref.emplace(7, 70);
    check_all_apis(xs, ref, "singleton insert");

    xs.insert(7, 700);
    ref.emplace(7, 700);
    check_all_apis(xs, ref, "duplicate insert");

    xs.insert_or_assign(7, 701);
    ref.insert_or_assign(7, 701);
    check_all_apis(xs, ref, "existing assign");

    xs.erase(100);
    ref.erase(100);
    check_all_apis(xs, ref, "missing erase");

    xs[8] += 5;
    ref[8] += 5;
    xs[7] += 2;
    ref[7] += 2;
    check_all_apis(xs, ref, "operator[]");

    xs.erase(7);
    ref.erase(7);
    check_all_apis(xs, ref, "present erase");

    xs.clear();
    ref.clear();
    check_all_apis(xs, ref, "nonempty clear");
    xs.insert(9, 90);
    ref.emplace(9, 90);
    check_all_apis(xs, ref, "reuse after clear");
}

void test_constructor_duplicates_boundaries() {
    using Pair = std::pair<int, int>;
    std::vector<Pair> init = {
        {5, 50}, {3, 30}, {5, 500}, {-1, -10}, {0, 0}, {3, 300},
        {std::numeric_limits<int>::min(), -1},
        {std::numeric_limits<int>::max(), 1},
        {42, 420}, {42, 421}, {-1, -11}
    };
    hash_map<int, int> xs(init);
    std::unordered_map<int, int> ref;
    for (const auto& [key, value] : init) ref.emplace(key, value);
    check_all_apis(xs, ref, "constructor duplicates and boundaries");

    hash_map<int, int> empty(std::vector<Pair>{});
    check_all_apis(empty, std::unordered_map<int, int>{}, "empty vector constructor");
}

void test_iteration_callbacks() {
    hash_map<int, int> xs;
    std::unordered_map<int, int> ref;
    for (int i = 0; i < 20; ++i) {
        xs.insert(i, i * 10);
        ref.emplace(i, i * 10);
    }

    int calls = 0;
    xs.for_each([&](const int&, const int&) -> bool { return ++calls == 3; });
    require_equal(calls, 3, "for_each bool break");

    calls = 0;
    xs.for_each([&](const int&, const int&) -> int { return ++calls == 5; });
    require_equal(calls, 5, "for_each int break");

    calls = 0;
    xs.for_each([&](const int&, const int&) -> std::optional<int> {
        ++calls;
        return calls == 7 ? std::optional<int>{1} : std::nullopt;
    });
    require_equal(calls, 7, "for_each optional break");

    calls = 0;
    xs.for_each([&](const int&, const int&) {
        ++calls;
    });
    require_equal(calls, 20, "for_each void return");

    xs.for_each([](const int& key, int& value) { value += key; });
    for (auto& [key, value] : ref) value += key;
    check_all_apis(xs, ref, "mutable for_each");

    const auto& cxs = xs;
    calls = 0;
    cxs.for_each([&](const int&, const int&) { ++calls; });
    require_equal(calls, 20, "const for_each");
}

void test_reserve_tombstones_copy_move() {
    hash_map<int, std::string> xs;
    std::unordered_map<int, std::string> ref;
    xs.reserve(5000);
    ref.reserve(5000);

    for (int i = 0; i < 5000; ++i) {
        xs.insert(i, std::to_string(i));
        ref.emplace(i, std::to_string(i));
    }
    for (int i = 0; i < 5000; i += 2) {
        xs.erase(i);
        ref.erase(i);
    }
    for (int i = 5000; i < 10000; ++i) {
        xs.insert_or_assign(i, std::to_string(i * 3));
        ref.insert_or_assign(i, std::to_string(i * 3));
    }
    check_all_apis(xs, ref, "tombstone reuse");

    hash_map<int, std::string> copied(xs);
    check_all_apis(copied, ref, "copy constructor");
    hash_map<int, std::string> assigned;
    assigned = xs;
    check_all_apis(assigned, ref, "copy assignment");
    assigned = assigned;
    check_all_apis(assigned, ref, "self copy assignment");

    hash_map<int, std::string> moved(std::move(copied));
    check_all_apis(moved, ref, "move constructor");
    require_equal(copied.size(), std::size_t{0}, "moved-from constructor source");
    hash_map<int, std::string> move_assigned;
    move_assigned = std::move(moved);
    check_all_apis(move_assigned, ref, "move assignment");
    require_equal(moved.size(), std::size_t{0}, "moved-from assignment source");

    for (int cycle = 0; cycle < 12; ++cycle) {
        xs.clear();
        ref.clear();
        for (int i = 0; i < 4096; ++i) {
            const int key = cycle * 100000 + i;
            xs.insert(key, std::to_string(key));
            ref.emplace(key, std::to_string(key));
        }
        for (int i = 0; i < 4096; ++i) {
            if ((i + cycle) % 3 != 0) continue;
            const int key = cycle * 100000 + i;
            xs.erase(key);
            ref.erase(key);
        }
        check_all_apis(xs, ref, "clear and tombstone cycle");
    }
}

void test_key_types_and_value_lifetimes() {
    {
        using Key = std::pair<int, unsigned>;
        hash_map<Key, long long> xs;
        std::unordered_map<Key, long long, shared_hasher<Key>> ref;
        for (int i = -100; i <= 100; ++i) {
            Key key{i, static_cast<unsigned>(i * i)};
            xs.insert(key, static_cast<long long>(i) * i * i);
            ref.emplace(key, static_cast<long long>(i) * i * i);
        }
        check_all_apis(xs, ref, "pair key");
    }
    {
        using Key = std::tuple<int, unsigned, long long>;
        hash_map<Key, int> xs;
        std::unordered_map<Key, int, shared_hasher<Key>> ref;
        for (int i = 0; i < 500; ++i) {
            Key key{i - 250, static_cast<unsigned>(i * 17), static_cast<long long>(i) * i};
            xs.insert_or_assign(key, i);
            ref.insert_or_assign(key, i);
        }
        check_all_apis(xs, ref, "tuple key");
    }
    {
        static_assert(!test_is_valid_key_v<std::pair<int, std::tuple<unsigned, long long>>>);
        static_assert(!test_is_valid_key_v<std::tuple<int, std::pair<unsigned, long long>>>);
    }
    {
        hash_map<__int128_t, int> xs;
        const __int128_t a = (static_cast<__int128_t>(1) << 100) + 123;
        const __int128_t b = -a;
        xs.insert(a, 1);
        xs.insert(b, 2);
        require(xs.find(a) && *xs.find(a) == 1, "int128 positive");
        require(xs.find(b) && *xs.find(b) == 2, "int128 negative");
    }
    {
        hash_map<int, std::unique_ptr<int>> xs;
        xs.reserve(2000);
        for (int i = 0; i < 2000; ++i) xs.insert(i, std::make_unique<int>(i * 2));
        for (int i = 0; i < 2000; ++i)
            require(xs.find(i) && **xs.find(i) == i * 2, "move-only value");
        xs.clear();
        require_equal(xs.size(), std::size_t{0}, "move-only clear");
    }
    {
        static_assert(alignof(WideValue) > __STDCPP_DEFAULT_NEW_ALIGNMENT__);
        hash_map<int, std::array<std::uint64_t, 8>> xs;
        for (int i = 0; i < 1000; ++i) {
            std::array<std::uint64_t, 8> value{};
            value[0] = static_cast<std::uint64_t>(i);
            value[7] = static_cast<std::uint64_t>(i * 3);
            xs.insert(i, value);
        }
        for (int i = 0; i < 1000; ++i) {
            const auto* p = xs.find(i);
            require(p && (*p)[0] == static_cast<std::uint64_t>(i), "large value");
        }
    }
}

void test_random_int_against_unordered_map() {
    hash_map<int, int> xs;
    std::unordered_map<int, int> ref;
    std::mt19937_64 rng(0x123456789abcdef0ULL);
    std::uniform_int_distribution<int> key_dist(-100000, 100000);
    std::uniform_int_distribution<int> value_dist(-1000000000, 1000000000);

    constexpr int operations = 500000;
    for (int step = 1; step <= operations; ++step) {
        const int op = static_cast<int>(rng() % 100);
        const int key = key_dist(rng);
        const int value = value_dist(rng);
        if (op < 24) {
            xs.insert(key, value);
            ref.emplace(key, value);
        } else if (op < 44) {
            xs.insert_or_assign(key, value);
            ref.insert_or_assign(key, value);
        } else if (op < 59) {
            xs.erase(key);
            ref.erase(key);
        } else if (op < 69) {
            xs[key] ^= value;
            ref[key] ^= value;
        } else if (op < 81) {
            int* p = xs.find(key);
            auto it = ref.find(key);
            require((p == nullptr) == (it == ref.end()), "random int find nullness");
            if (p) require_equal(*p, it->second, "random int find value");
        } else if (op < 91) {
            require_equal(xs.contains(key), ref.contains(key), "random int contains");
            require_equal(xs.count(key), ref.count(key), "random int count");
        } else if (op < 94) {
            const std::size_t expected = static_cast<std::size_t>(rng() % 250001);
            xs.reserve(expected);
            ref.reserve(expected);
        } else if (op == 94 && step % 997 == 0) {
            xs.clear();
            ref.clear();
        } else {
            if (int* p = xs.find(key)) ++*p;
            if (auto it = ref.find(key); it != ref.end()) ++it->second;
        }

        if ((step & 8191) == 0) check_all_apis(xs, ref, "random int periodic");
    }
    check_all_apis(xs, ref, "random int final");
}

void test_random_pair_tuple() {
    {
        using Key = std::pair<int, int>;
        hash_map<Key, long long> xs;
        std::unordered_map<Key, long long, shared_hasher<Key>> ref;
        std::mt19937_64 rng(0x3141592653589793ULL);
        for (int step = 1; step <= 250000; ++step) {
            const Key key{static_cast<int>(rng() % 2001) - 1000,
                          static_cast<int>(rng() % 2001) - 1000};
            const long long value = static_cast<long long>(rng() % 2000000001ULL) - 1000000000LL;
            const int op = static_cast<int>(rng() % 10);
            if (op < 3) {
                xs.insert(key, value);
                ref.emplace(key, value);
            } else if (op < 6) {
                xs.insert_or_assign(key, value);
                ref.insert_or_assign(key, value);
            } else if (op < 8) {
                xs.erase(key);
                ref.erase(key);
            } else if (op == 8) {
                xs[key] += value;
                ref[key] += value;
            } else {
                const long long* p = xs.find(key);
                auto it = ref.find(key);
                require((p == nullptr) == (it == ref.end()), "random pair find nullness");
                if (p) require_equal(*p, it->second, "random pair find value");
            }
            if ((step & 16383) == 0) check_all_apis(xs, ref, "random pair periodic");
        }
        check_all_apis(xs, ref, "random pair final");
    }
    {
        using Key = std::tuple<int, unsigned, long long>;
        hash_map<Key, int> xs;
        std::unordered_map<Key, int, shared_hasher<Key>> ref;
        std::mt19937_64 rng(0x2718281828459045ULL);
        for (int step = 1; step <= 200000; ++step) {
            const Key key{static_cast<int>(rng() % 401) - 200,
                          static_cast<unsigned>(rng() % 401),
                          static_cast<long long>(rng() % 401) - 200};
            const int value = static_cast<int>(rng());
            const int op = static_cast<int>(rng() % 9);
            if (op < 3) {
                xs.insert(key, value);
                ref.emplace(key, value);
            } else if (op < 6) {
                xs.insert_or_assign(key, value);
                ref.insert_or_assign(key, value);
            } else if (op < 8) {
                xs.erase(key);
                ref.erase(key);
            } else {
                const int* p = xs.find(key);
                auto it = ref.find(key);
                require((p == nullptr) == (it == ref.end()), "random tuple find nullness");
                if (p) require_equal(*p, it->second, "random tuple find value");
            }
            if ((step & 16383) == 0) check_all_apis(xs, ref, "random tuple periodic");
        }
        check_all_apis(xs, ref, "random tuple final");
    }
}

void test_erase_heavy() {
    hash_map<std::uint64_t, std::uint64_t> xs;
    std::unordered_map<std::uint64_t, std::uint64_t> ref;
    constexpr std::size_t width = 32768;
    xs.reserve(width);
    ref.reserve(width);

    for (std::size_t cycle = 0; cycle < 20; ++cycle) {
        for (std::size_t i = 0; i < width; ++i) {
            const std::uint64_t key = (cycle * width + i) * 0x9e3779b97f4a7c15ULL;
            xs.insert(key, key ^ 0xabcdef0123456789ULL);
            ref.emplace(key, key ^ 0xabcdef0123456789ULL);
        }
        for (std::size_t i = 0; i < width; ++i) {
            const std::uint64_t key = (cycle * width + i) * 0x9e3779b97f4a7c15ULL;
            xs.erase(key);
            ref.erase(key);
        }
        require_equal(xs.size(), std::size_t{0}, "erase-heavy empty size");
        require_equal(ref.size(), std::size_t{0}, "erase-heavy reference size");
    }
    xs.insert(123, 456);
    ref.emplace(123, 456);
    check_all_apis(xs, ref, "erase-heavy final reuse");
}

void run_tests() {
    test_empty_singleton_clear();
    std::cout << "edge_empty_singleton_clear: ok\n";
    test_constructor_duplicates_boundaries();
    std::cout << "edge_constructor_duplicates_boundaries: ok\n";
    test_iteration_callbacks();
    std::cout << "edge_iteration_callbacks: ok\n";
    test_reserve_tombstones_copy_move();
    std::cout << "reserve_tombstones_copy_move: ok\n";
    test_key_types_and_value_lifetimes();
    std::cout << "key_types_and_value_lifetimes: ok\n";
    test_random_int_against_unordered_map();
    std::cout << "random_int_500000_ops: ok\n";
    test_random_pair_tuple();
    std::cout << "random_pair_tuple_450000_ops: ok\n";
    test_erase_heavy();
    std::cout << "erase_heavy_655360_pairs: ok\n";
    std::cout << "all tests passed\n";
}

#ifndef HASH_MAP_SKIP_BENCHMARK
struct BenchmarkResult {
    std::string api;
    std::size_t n;
    std::size_t q;
    double hash_ms;
    std::optional<double> std_ms;
    std::size_t hash_ops;
    std::size_t std_ops;
    bool ok;
    std::string status;
};

volatile std::uint64_t benchmark_sink = 0;

std::uint64_t splitmix_permutation(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

std::vector<std::uint64_t> make_u64_keys(std::size_t n, std::uint64_t offset) {
    std::vector<std::uint64_t> keys(n);
    for (std::size_t i = 0; i < n; ++i) keys[i] = splitmix_permutation(offset + i);
    return keys;
}

template <class F>
double measure_ms(F&& f) {
    const auto begin = std::chrono::steady_clock::now();
    std::invoke(std::forward<F>(f));
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

double median3(std::array<double, 3> values) {
    std::sort(values.begin(), values.end());
    return values[1];
}

template <class HashRun, class StdRun>
BenchmarkResult benchmark_pair(const std::string& api, std::size_t n, std::size_t q,
                               std::size_t hash_ops, std::size_t std_ops,
                               HashRun&& hash_run, StdRun&& std_run) {
    std::array<double, 3> hash_times{};
    std::array<double, 3> std_times{};
    std::array<std::uint64_t, 3> hash_checks{};
    std::array<std::uint64_t, 3> std_checks{};
    for (int rep = 0; rep < 3; ++rep) {
        auto [hash_time, hash_check] = hash_run();
        auto [std_time, std_check] = std_run();
        hash_times[rep] = hash_time;
        std_times[rep] = std_time;
        hash_checks[rep] = hash_check;
        std_checks[rep] = std_check;
        benchmark_sink = hash_check ^ std_check;
    }
    const bool ok = hash_checks[0] == std_checks[0] && hash_checks[1] == std_checks[1] &&
                    hash_checks[2] == std_checks[2];
    return {api, n, q, median3(hash_times), median3(std_times), hash_ops, std_ops, ok,
            ok ? "ok" : "checksum_mismatch"};
}

template <class HashRun>
BenchmarkResult benchmark_hash_only(const std::string& api, std::size_t n, std::size_t q,
                                    std::size_t hash_ops, HashRun&& hash_run) {
    std::array<double, 3> times{};
    for (int rep = 0; rep < 3; ++rep) {
        auto [time, check] = hash_run();
        times[rep] = time;
        benchmark_sink = check;
    }
    return {api, n, q, median3(times), std::nullopt, hash_ops, 0, true, "hash_only"};
}

BenchmarkResult bench_default_construct() {
    constexpr std::size_t q = 100000;
    return benchmark_pair(
        "default_construct", 0, q, q, q,
        [] {
            using M = hash_map<std::uint64_t, std::uint64_t>;
            std::allocator<M> alloc;
            M* storage = alloc.allocate(q);
            const double ms = measure_ms([&] {
                for (std::size_t i = 0; i < q; ++i) std::construct_at(storage + i);
            });
            for (std::size_t i = 0; i < q; ++i) std::destroy_at(storage + i);
            alloc.deallocate(storage, q);
            return std::pair{ms, static_cast<std::uint64_t>(q)};
        },
        [] {
            using M = std::unordered_map<std::uint64_t, std::uint64_t>;
            std::allocator<M> alloc;
            M* storage = alloc.allocate(q);
            const double ms = measure_ms([&] {
                for (std::size_t i = 0; i < q; ++i) std::construct_at(storage + i);
            });
            for (std::size_t i = 0; i < q; ++i) std::destroy_at(storage + i);
            alloc.deallocate(storage, q);
            return std::pair{ms, static_cast<std::uint64_t>(q)};
        });
}

BenchmarkResult bench_construct_vector(const std::vector<std::uint64_t>& keys) {
    using Pair = std::pair<std::uint64_t, std::uint64_t>;
    std::vector<Pair> source;
    source.reserve(keys.size());
    for (std::uint64_t key : keys) source.emplace_back(key, key ^ 0x123456789abcdef0ULL);

    return benchmark_pair(
        "construct(vector)", keys.size(), 1, 1, 1,
        [&] {
            using Map = hash_map<std::uint64_t, std::uint64_t>;
            auto input = source;
            alignas(Map) std::byte storage[sizeof(Map)];
            Map* holder = nullptr;
            const double ms = measure_ms([&] {
                holder = std::construct_at(reinterpret_cast<Map*>(storage), std::move(input));
            });
            const std::uint64_t check = holder->size();
            std::destroy_at(holder);
            return std::pair{ms, check};
        },
        [&] {
            using Map = std::unordered_map<std::uint64_t, std::uint64_t>;
            auto input = source;
            alignas(Map) std::byte storage[sizeof(Map)];
            Map* holder = nullptr;
            const double ms = measure_ms([&] {
                holder = std::construct_at(reinterpret_cast<Map*>(storage));
                holder->reserve(input.size());
                for (auto& [key, value] : input) holder->emplace(std::move(key), std::move(value));
            });
            const std::uint64_t check = holder->size();
            std::destroy_at(holder);
            return std::pair{ms, check};
        });
}

BenchmarkResult bench_reserve() {
    constexpr std::size_t n = 200000;
    return benchmark_pair(
        "reserve", n, 1, 1, 1,
        [] {
            hash_map<std::uint64_t, std::uint64_t> xs;
            const double ms = measure_ms([&] { xs.reserve(n); });
            return std::pair{ms, static_cast<std::uint64_t>(xs.size())};
        },
        [] {
            std::unordered_map<std::uint64_t, std::uint64_t> xs;
            const double ms = measure_ms([&] { xs.reserve(n); });
            return std::pair{ms, static_cast<std::uint64_t>(xs.size())};
        });
}

template <class Map>
void fill_map(Map& xs, const std::vector<std::uint64_t>& keys) {
    if constexpr (requires { xs.reserve(keys.size()); }) xs.reserve(keys.size());
    for (std::uint64_t key : keys) xs.emplace(key, key ^ 0x123456789abcdef0ULL);
}

template <class V>
void fill_map(hash_map<std::uint64_t, V>& xs, const std::vector<std::uint64_t>& keys) {
    xs.reserve(keys.size());
    for (std::uint64_t key : keys) xs.insert(key, static_cast<V>(key ^ 0x123456789abcdef0ULL));
}

BenchmarkResult bench_clear(const std::vector<std::uint64_t>& keys) {
    constexpr std::size_t q = 5;
    return benchmark_pair(
        "clear", keys.size(), q, q, q,
        [&] {
            std::vector<hash_map<std::uint64_t, std::uint64_t>> maps(q);
            for (auto& xs : maps) fill_map(xs, keys);
            const double ms = measure_ms([&] {
                for (auto& xs : maps) xs.clear();
            });
            std::uint64_t check = 0;
            for (const auto& xs : maps) check += xs.size();
            return std::pair{ms, check};
        },
        [&] {
            std::vector<std::unordered_map<std::uint64_t, std::uint64_t>> maps(q);
            for (auto& xs : maps) fill_map(xs, keys);
            const double ms = measure_ms([&] {
                for (auto& xs : maps) xs.clear();
            });
            std::uint64_t check = 0;
            for (const auto& xs : maps) check += xs.size();
            return std::pair{ms, check};
        });
}

BenchmarkResult bench_insert_unique(const std::vector<std::uint64_t>& keys, bool reserved) {
    return benchmark_pair(
        reserved ? "insert(unique,reserved)" : "insert(unique,empty)",
        keys.size(), keys.size(), keys.size(), keys.size(),
        [&] {
            hash_map<std::uint64_t, std::uint64_t> xs;
            if (reserved) xs.reserve(keys.size());
            const double ms = measure_ms([&] {
                for (std::uint64_t key : keys) xs.insert(key, key);
            });
            return std::pair{ms, static_cast<std::uint64_t>(xs.size())};
        },
        [&] {
            std::unordered_map<std::uint64_t, std::uint64_t> xs;
            if (reserved) xs.reserve(keys.size());
            const double ms = measure_ms([&] {
                for (std::uint64_t key : keys) xs.emplace(key, key);
            });
            return std::pair{ms, static_cast<std::uint64_t>(xs.size())};
        });
}

BenchmarkResult bench_insert_missing(const std::vector<std::uint64_t>& base,
                                     const std::vector<std::uint64_t>& missing) {
    return benchmark_pair(
        "insert(missing,reserved)", base.size(), missing.size(), missing.size(), missing.size(),
        [&] {
            hash_map<std::uint64_t, std::uint64_t> xs;
            xs.reserve(base.size() + missing.size());
            for (auto key : base) xs.insert(key, key);
            const double ms = measure_ms([&] {
                for (auto key : missing) xs.insert(key, key);
            });
            return std::pair{ms, static_cast<std::uint64_t>(xs.size())};
        },
        [&] {
            std::unordered_map<std::uint64_t, std::uint64_t> xs;
            xs.reserve(base.size() + missing.size());
            for (auto key : base) xs.emplace(key, key);
            const double ms = measure_ms([&] {
                for (auto key : missing) xs.emplace(key, key);
            });
            return std::pair{ms, static_cast<std::uint64_t>(xs.size())};
        });
}

BenchmarkResult bench_insert_duplicate(const std::vector<std::uint64_t>& base,
                                       const std::vector<std::uint64_t>& queries) {
    return benchmark_pair(
        "insert(duplicate)", base.size(), queries.size(), queries.size(), queries.size(),
        [&] {
            hash_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, base);
            const double ms = measure_ms([&] {
                for (auto key : queries) xs.insert(key, key + 1);
            });
            std::uint64_t check = xs.size();
            for (std::size_t i = 0; i < 64; ++i) check ^= *xs.find(base[i]);
            return std::pair{ms, check};
        },
        [&] {
            std::unordered_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, base);
            const double ms = measure_ms([&] {
                for (auto key : queries) xs.emplace(key, key + 1);
            });
            std::uint64_t check = xs.size();
            for (std::size_t i = 0; i < 64; ++i) check ^= xs.find(base[i])->second;
            return std::pair{ms, check};
        });
}

BenchmarkResult bench_insert_or_assign(const std::vector<std::uint64_t>& base,
                                       const std::vector<std::uint64_t>& queries) {
    return benchmark_pair(
        "insert_or_assign(present)", base.size(), queries.size(), queries.size(), queries.size(),
        [&] {
            hash_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, base);
            std::uint64_t value = 1;
            const double ms = measure_ms([&] {
                for (auto key : queries) xs.insert_or_assign(key, ++value);
            });
            std::uint64_t check = xs.size();
            for (std::size_t i = 0; i < 64; ++i) check ^= *xs.find(base[i]);
            return std::pair{ms, check};
        },
        [&] {
            std::unordered_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, base);
            std::uint64_t value = 1;
            const double ms = measure_ms([&] {
                for (auto key : queries) xs.insert_or_assign(key, ++value);
            });
            std::uint64_t check = xs.size();
            for (std::size_t i = 0; i < 64; ++i) check ^= xs.find(base[i])->second;
            return std::pair{ms, check};
        });
}

BenchmarkResult bench_erase_present(const std::vector<std::uint64_t>& keys) {
    return benchmark_pair(
        "erase(present)", keys.size(), keys.size(), keys.size(), keys.size(),
        [&] {
            hash_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, keys);
            const double ms = measure_ms([&] {
                for (auto key : keys) xs.erase(key);
            });
            return std::pair{ms, static_cast<std::uint64_t>(xs.size())};
        },
        [&] {
            std::unordered_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, keys);
            const double ms = measure_ms([&] {
                for (auto key : keys) xs.erase(key);
            });
            return std::pair{ms, static_cast<std::uint64_t>(xs.size())};
        });
}

BenchmarkResult bench_erase_missing(const std::vector<std::uint64_t>& base,
                                    const std::vector<std::uint64_t>& missing) {
    return benchmark_pair(
        "erase(missing)", base.size(), missing.size(), missing.size(), missing.size(),
        [&] {
            hash_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, base);
            const double ms = measure_ms([&] {
                for (auto key : missing) xs.erase(key);
            });
            return std::pair{ms, static_cast<std::uint64_t>(xs.size())};
        },
        [&] {
            std::unordered_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, base);
            const double ms = measure_ms([&] {
                for (auto key : missing) xs.erase(key);
            });
            return std::pair{ms, static_cast<std::uint64_t>(xs.size())};
        });
}

void benchmark_memory_barrier(const void* p) {
    asm volatile("" : : "g"(p) : "memory");
}

BenchmarkResult bench_size(const std::vector<std::uint64_t>& keys) {
    constexpr std::size_t q = 5000000;
    return benchmark_pair(
        "size", keys.size(), q, q, q,
        [&] {
            hash_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, keys);
            std::uint64_t check = 0;
            const double ms = measure_ms([&] {
                for (std::size_t i = 0; i < q; ++i) {
                    benchmark_memory_barrier(&xs);
                    check += xs.size();
                }
            });
            return std::pair{ms, check};
        },
        [&] {
            std::unordered_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, keys);
            std::uint64_t check = 0;
            const double ms = measure_ms([&] {
                for (std::size_t i = 0; i < q; ++i) {
                    benchmark_memory_barrier(&xs);
                    check += xs.size();
                }
            });
            return std::pair{ms, check};
        });
}

BenchmarkResult bench_empty(const std::vector<std::uint64_t>& keys) {
    constexpr std::size_t q = 5000000;
    return benchmark_pair(
        "empty", keys.size(), q, q, q,
        [&] {
            hash_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, keys);
            std::uint64_t check = 0;
            const double ms = measure_ms([&] {
                for (std::size_t i = 0; i < q; ++i) {
                    benchmark_memory_barrier(&xs);
                    check += xs.empty();
                }
            });
            return std::pair{ms, check};
        },
        [&] {
            std::unordered_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, keys);
            std::uint64_t check = 0;
            const double ms = measure_ms([&] {
                for (std::size_t i = 0; i < q; ++i) {
                    benchmark_memory_barrier(&xs);
                    check += xs.empty();
                }
            });
            return std::pair{ms, check};
        });
}

BenchmarkResult bench_for_each(const std::vector<std::uint64_t>& keys, bool break_half) {
    const std::size_t visits = break_half ? keys.size() / 2 : keys.size();
    return benchmark_pair(
        break_half ? "for_each(break)" : "for_each", keys.size(), visits, visits, visits,
        [&] {
            hash_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, keys);
            std::uint64_t check = 0;
            std::size_t count = 0;
            const double ms = measure_ms([&] {
                xs.for_each([&](const auto& key, const auto& value) {
                    check += key ^ value;
                    ++count;
                    return break_half && count == visits;
                });
            });
            return std::pair{ms, check ^ count};
        },
        [&] {
            std::unordered_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, keys);
            std::uint64_t check = 0;
            std::size_t count = 0;
            const double ms = measure_ms([&] {
                for (const auto& [key, value] : xs) {
                    check += key ^ value;
                    ++count;
                    if (break_half && count == visits) break;
                }
            });
            return std::pair{ms, check ^ count};
        });
}

BenchmarkResult bench_lookup(const std::string& name,
                             const std::vector<std::uint64_t>& base,
                             const std::vector<std::uint64_t>& queries,
                             int mode) {
    return benchmark_pair(
        name, base.size(), queries.size(), queries.size(), queries.size(),
        [&] {
            hash_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, base);
            std::uint64_t check = 0;
            const double ms = measure_ms([&] {
                if (mode == 0) {
                    for (auto key : queries) check += xs.count(key);
                } else if (mode == 1) {
                    for (auto key : queries) {
                        const auto* p = xs.find(key);
                        check += p ? *p : 1;
                    }
                } else if (mode == 2) {
                    for (auto key : queries) check += xs[key];
                } else {
                    for (auto key : queries) check += xs.contains(key);
                }
            });
            return std::pair{ms, check};
        },
        [&] {
            std::unordered_map<std::uint64_t, std::uint64_t> xs;
            fill_map(xs, base);
            std::uint64_t check = 0;
            const double ms = measure_ms([&] {
                if (mode == 0) {
                    for (auto key : queries) check += xs.count(key);
                } else if (mode == 1) {
                    for (auto key : queries) {
                        const auto it = xs.find(key);
                        check += it == xs.end() ? 1 : it->second;
                    }
                } else if (mode == 2) {
                    for (auto key : queries) check += xs[key];
                } else {
                    for (auto key : queries) check += xs.contains(key);
                }
            });
            return std::pair{ms, check};
        });
}

BenchmarkResult bench_operator_missing(const std::vector<std::uint64_t>& missing) {
    return benchmark_pair(
        "operator[](missing,reserved)", 0, missing.size(), missing.size(), missing.size(),
        [&] {
            hash_map<std::uint64_t, std::uint64_t> xs;
            xs.reserve(missing.size());
            std::uint64_t check = 0;
            const double ms = measure_ms([&] {
                for (auto key : missing) check += (xs[key] = key);
            });
            return std::pair{ms, check ^ xs.size()};
        },
        [&] {
            std::unordered_map<std::uint64_t, std::uint64_t> xs;
            xs.reserve(missing.size());
            std::uint64_t check = 0;
            const double ms = measure_ms([&] {
                for (auto key : missing) check += (xs[key] = key);
            });
            return std::pair{ms, check ^ xs.size()};
        });
}

BenchmarkResult bench_sequential_int_find() {
    constexpr std::size_t n = 200000;
    constexpr std::size_t q = 500000;
    std::vector<int> keys(n);
    for (std::size_t i = 0; i < n; ++i) keys[i] = static_cast<int>(i * 2 + 1);
    std::vector<int> queries(q);
    for (std::size_t i = 0; i < q; ++i)
        queries[i] = i & 1 ? keys[i % n] : static_cast<int>((i % n) * 2);

    return benchmark_pair(
        "find(sequential_int,mixed)", n, q, q, q,
        [&] {
            hash_map<int, int> xs;
            xs.reserve(n);
            for (int key : keys) xs.insert(key, key);
            std::uint64_t check = 0;
            const double ms = measure_ms([&] {
                for (int key : queries) {
                    const int* p = xs.find(key);
                    check += p ? static_cast<std::uint64_t>(*p) : 1;
                }
            });
            return std::pair{ms, check};
        },
        [&] {
            std::unordered_map<int, int> xs;
            xs.reserve(n);
            for (int key : keys) xs.emplace(key, key);
            std::uint64_t check = 0;
            const double ms = measure_ms([&] {
                for (int key : queries) {
                    const auto it = xs.find(key);
                    check += it == xs.end() ? 1 : static_cast<std::uint64_t>(it->second);
                }
            });
            return std::pair{ms, check};
        });
}

template <class Key>
BenchmarkResult bench_composite_find(const std::string& name,
                                     const std::vector<Key>& keys,
                                     const std::vector<Key>& queries) {
    return benchmark_pair(
        name, keys.size(), queries.size(), queries.size(), queries.size(),
        [&] {
            hash_map<Key, std::uint64_t> xs;
            xs.reserve(keys.size());
            for (std::size_t i = 0; i < keys.size(); ++i) xs.insert(keys[i], i + 1);
            std::uint64_t check = 0;
            const double ms = measure_ms([&] {
                for (const Key& key : queries) {
                    const auto* p = xs.find(key);
                    check += p ? *p : 1;
                }
            });
            return std::pair{ms, check};
        },
        [&] {
            std::unordered_map<Key, std::uint64_t, shared_hasher<Key>> xs;
            xs.reserve(keys.size());
            for (std::size_t i = 0; i < keys.size(); ++i) xs.emplace(keys[i], i + 1);
            std::uint64_t check = 0;
            const double ms = measure_ms([&] {
                for (const Key& key : queries) {
                    const auto it = xs.find(key);
                    check += it == xs.end() ? 1 : it->second;
                }
            });
            return std::pair{ms, check};
        });
}

std::vector<BenchmarkResult> run_benchmarks() {
    constexpr std::size_t n = 200000;
    constexpr std::size_t q = 500000;
    const auto base = make_u64_keys(n, 0);
    const auto missing = make_u64_keys(300000, 1000000000ULL);

    std::vector<std::uint64_t> present_queries(q);
    std::vector<std::uint64_t> mixed_queries(q);
    std::vector<std::uint64_t> missing_queries(q);
    for (std::size_t i = 0; i < q; ++i) {
        present_queries[i] = base[(i * 11995408973635179863ULL) % n];
        missing_queries[i] = missing[i % missing.size()];
        mixed_queries[i] = (i & 1) ? present_queries[i] : missing_queries[i];
    }

    std::vector<BenchmarkResult> results;
    results.push_back(bench_default_construct());
    results.push_back(bench_construct_vector(base));
    results.push_back(bench_reserve());
    results.push_back(bench_clear(std::vector<std::uint64_t>(base.begin(), base.begin() + 100000)));
    results.push_back(bench_insert_unique(base, false));
    results.push_back(bench_insert_unique(base, true));
    results.push_back(bench_insert_missing(base, missing));
    results.push_back(bench_insert_duplicate(base, present_queries));
    results.push_back(bench_insert_or_assign(base, present_queries));
    results.push_back(bench_erase_present(base));
    results.push_back(bench_erase_missing(base, missing_queries));
    results.push_back(bench_size(base));
    results.push_back(bench_empty(base));
    results.push_back(bench_for_each(base, false));
    results.push_back(bench_for_each(base, true));
    results.push_back(bench_lookup("contains(mixed)", base, mixed_queries, 3));
    results.push_back(bench_lookup("count(mixed)", base, mixed_queries, 0));
    results.push_back(bench_lookup("find(mixed_random_u64)", base, mixed_queries, 1));
    results.push_back(bench_lookup("operator[](existing)", base, present_queries, 2));
    results.push_back(bench_operator_missing(missing));
    results.push_back(bench_sequential_int_find());

    using PairKey = std::pair<std::uint32_t, std::uint32_t>;
    std::vector<PairKey> pair_keys(100000);
    std::vector<PairKey> pair_queries(300000);
    for (std::size_t i = 0; i < pair_keys.size(); ++i)
        pair_keys[i] = {static_cast<std::uint32_t>(splitmix_permutation(i)),
                        static_cast<std::uint32_t>(splitmix_permutation(i + 7000000) >> 32)};
    for (std::size_t i = 0; i < pair_queries.size(); ++i)
        pair_queries[i] = i & 1 ? pair_keys[i % pair_keys.size()]
                                : PairKey{static_cast<std::uint32_t>(splitmix_permutation(i + 9000000)),
                                          static_cast<std::uint32_t>(splitmix_permutation(i + 12000000) >> 32)};
    results.push_back(bench_composite_find("find(pair_u32,mixed)", pair_keys, pair_queries));

    using TupleKey = std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>;
    std::vector<TupleKey> tuple_keys(100000);
    std::vector<TupleKey> tuple_queries(300000);
    for (std::size_t i = 0; i < tuple_keys.size(); ++i)
        tuple_keys[i] = {static_cast<std::uint32_t>(splitmix_permutation(i + 15000000)),
                         static_cast<std::uint32_t>(splitmix_permutation(i + 18000000)),
                         static_cast<std::uint32_t>(splitmix_permutation(i + 21000000))};
    for (std::size_t i = 0; i < tuple_queries.size(); ++i)
        tuple_queries[i] = i & 1 ? tuple_keys[i % tuple_keys.size()]
                                 : TupleKey{static_cast<std::uint32_t>(splitmix_permutation(i + 24000000)),
                                            static_cast<std::uint32_t>(splitmix_permutation(i + 27000000)),
                                            static_cast<std::uint32_t>(splitmix_permutation(i + 30000000))};
    results.push_back(bench_composite_find("find(tuple_u32x3,mixed)", tuple_keys, tuple_queries));
    return results;
}

void print_benchmarks(const std::vector<BenchmarkResult>& results) {
    std::cout << "benchmarks begin\n";
    std::cout << "compiler," << __VERSION__ << '\n';
    std::cout << "flags,-std=c++20 -O2 -march=native -DNDEBUG\n";
    std::cout << "repetitions,3\n";
    std::cout << "api,n,q,hash_ms,std_unordered_map_ms,hash_ns_per_op,std_ns_per_op,std_over_hash,status\n";
    std::cout << std::fixed << std::setprecision(3);
    for (const auto& r : results) {
        const double hash_ns = r.hash_ops ? r.hash_ms * 1000000.0 / static_cast<double>(r.hash_ops) : 0.0;
        std::cout << r.api << ',' << r.n << ',' << r.q << ',' << r.hash_ms << ',';
        if (r.std_ms) {
            const double std_ns = r.std_ops ? *r.std_ms * 1000000.0 / static_cast<double>(r.std_ops) : 0.0;
            const double ratio = r.hash_ms > 0.0 ? *r.std_ms / r.hash_ms : 0.0;
            std::cout << *r.std_ms << ',' << hash_ns << ',' << std_ns << ',' << ratio << ',';
        } else {
            std::cout << "NA," << hash_ns << ",NA,NA,";
        }
        std::cout << r.status << '\n';
    }
}

#endif

} // namespace hash_map_selftest

int main() {
    hash_map_selftest::run_tests();
#ifndef HASH_MAP_SKIP_BENCHMARK
    hash_map_selftest::print_benchmarks(hash_map_selftest::run_benchmarks());
#endif
    return 0;
}
#endif

// 実行結果(atcoder)
// benchmarks begin
// compiler,15.2.0
// flags,-std=c++20 -O2 -march=native -DNDEBUG
// repetitions,3
// api,n,q,hash_ms,std_unordered_map_ms,hash_ns_per_op,std_ns_per_op,std_over_hash,status
// default_construct,0,100000,0.229,1.574,2.291,15.739,6.870,ok
// construct(vector),200000,1,2.789,9.953,2788789.000,9952913.000,3.569,ok
// reserve,200000,1,0.005,0.034,5425.000,33520.000,6.179,ok
// clear,100000,5,0.021,4.610,4148.800,921919.200,222.213,ok
// insert(unique,empty),200000,200000,5.082,18.459,25.408,92.293,3.632,ok
// insert(unique,reserved),200000,200000,2.747,9.937,13.733,49.687,3.618,ok
// insert(missing,reserved),200000,300000,5.083,22.082,16.942,73.607,4.345,ok
// insert(duplicate),200000,500000,5.408,11.674,10.816,23.348,2.159,ok
// insert_or_assign(present),200000,500000,5.877,12.114,11.754,24.228,2.061,ok
// erase(present),200000,200000,1.726,8.133,8.628,40.664,4.713,ok
// erase(missing),200000,500000,6.104,16.263,12.207,32.527,2.665,ok
// size,200000,5000000,3.164,3.163,0.633,0.633,1.000,ok
// empty,200000,5000000,3.163,3.161,0.633,0.632,0.999,ok
// for_each,200000,200000,0.393,2.578,1.964,12.890,6.563,ok
// for_each(break),200000,100000,0.208,0.962,2.078,9.620,4.629,ok
// contains(mixed),200000,500000,5.280,13.400,10.559,26.800,2.538,ok
// count(mixed),200000,500000,5.267,13.235,10.533,26.469,2.513,ok
// find(mixed_random_u64),200000,500000,5.859,13.400,11.718,26.800,2.287,ok
// operator[](existing),200000,500000,5.934,11.899,11.869,23.797,2.005,ok
// operator[](missing,reserved),0,300000,3.573,17.090,11.910,56.966,4.783,ok
// find(sequential_int,mixed),200000,500000,3.853,2.374,7.705,4.749,0.616,ok
// find(pair_u32,mixed),100000,300000,3.855,8.386,12.850,27.953,2.175,ok
// find(tuple_u32x3,mixed),100000,300000,4.747,9.968,15.824,33.228,2.100,ok
