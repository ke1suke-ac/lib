#pragma once
#include <bits/stdc++.h>
using namespace std;

/*
 * static_vector_v2.hpp
 *
 * 競技プログラミング向けの固定容量 vector。
 * - 型 T と最大要素数 Capacity をテンプレート引数で指定する。
 * - 内部ストレージは常に未初期化バイト列で持ち、push/emplace された範囲だけ T の寿命を開始する。
 * - 動的メモリ確保は行わない。
 * - コピー処理は std::is_trivially_copyable_v<T> の場合だけ memcpy で高速化する。
 * - std::span への変換と span() を提供する。
 * - 例外安全性は競技用途向けに簡略化し、構築中の例外に対するロールバックは行わない。
 *
 * 使い方:
 *   StaticVector<int, 32> v;
 *   v.push_back_unchecked(10);       // 容量を呼び出し側で保証する高速版
 *   v.emplace_back(20);              // assert 付き版
 *   std::span<int> s = v;            // または v.span()
 *
 * 注意:
 * - *_unchecked は容量・空判定を行わない。ホットパスで使う前提。
 * - checked 版も assert のみで、例外送出などは行わない。
 * - data()[0, size()) のみ有効範囲として扱う。
 */

template <class T, size_t Capacity, class SizeT = uint32_t>
class StaticVector {
    static_assert(is_object_v<T>, "T must be an object type");
    static_assert(!is_const_v<T>, "T must not be const");
    static_assert(!is_volatile_v<T>, "T must not be volatile");
    static_assert(is_integral_v<SizeT> && is_unsigned_v<SizeT>, "SizeT must be an unsigned integer type");
    static_assert(Capacity <= static_cast<size_t>(numeric_limits<SizeT>::max()), "Capacity does not fit in SizeT");
    static_assert(Capacity == 0 || Capacity <= numeric_limits<size_t>::max() / sizeof(T), "Capacity is too large");
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
private:
    static constexpr size_t kStorageBytes = Capacity == 0 ? 1 : sizeof(T) * Capacity;
    SizeT size_ = 0;
    alignas(T) std::byte storage_[kStorageBytes];
    T* raw_data() noexcept { return reinterpret_cast<T*>(storage_); }
    const T* raw_data() const noexcept { return reinterpret_cast<const T*>(storage_); }
    T* ptr_at(size_t i) noexcept { return std::launder(raw_data() + i); }
    const T* ptr_at(size_t i) const noexcept { return std::launder(raw_data() + i); }
    void destroy_all() noexcept {
        if constexpr (!is_trivially_destructible_v<T>) {
            for (size_t i = size(); i > 0; --i) std::destroy_at(ptr_at(i - 1));
        }
        size_ = 0;
    }
    void copy_construct_from(const StaticVector& other) noexcept(is_trivially_copyable_v<T>) {
        const size_t n = other.size();
        if constexpr (is_trivially_copyable_v<T>) {
            if (n != 0) std::memcpy(static_cast<void*>(storage_), static_cast<const void*>(other.storage_), sizeof(T) * n);
            size_ = static_cast<SizeT>(n);
        } else {
            size_ = 0;
            for (const T& x : other) emplace_back_unchecked(x);
        }
    }
    void move_construct_from(StaticVector&& other) noexcept(is_trivially_copyable_v<T> || is_nothrow_move_constructible_v<T>) {
        const size_t n = other.size();
        if constexpr (is_trivially_copyable_v<T>) {
            if (n != 0) std::memcpy(static_cast<void*>(storage_), static_cast<const void*>(other.storage_), sizeof(T) * n);
            size_ = static_cast<SizeT>(n);
            other.size_ = 0;
        } else {
            size_ = 0;
            for (T& x : other) emplace_back_unchecked(std::move(x));
            other.clear();
        }
    }
public:
    StaticVector() noexcept = default;
    StaticVector(initializer_list<T> init) requires is_copy_constructible_v<T> {
        assert(init.size() <= Capacity);
        for (const T& x : init) emplace_back_unchecked(x);
    }
    ~StaticVector() noexcept requires is_trivially_destructible_v<T> = default;
    ~StaticVector() noexcept requires (!is_trivially_destructible_v<T>) { destroy_all(); }
    StaticVector(const StaticVector& other) noexcept(is_trivially_copyable_v<T>) requires is_copy_constructible_v<T> { copy_construct_from(other); }
    StaticVector(const StaticVector& other) requires (!is_copy_constructible_v<T>) = delete;
    StaticVector& operator=(const StaticVector& other) noexcept(is_trivially_copyable_v<T>) requires is_copy_constructible_v<T> {
        if (this == &other) return *this;
        destroy_all();
        copy_construct_from(other);
        return *this;
    }
    StaticVector& operator=(const StaticVector& other) requires (!is_copy_constructible_v<T>) = delete;
    StaticVector(StaticVector&& other) noexcept(is_trivially_copyable_v<T> || is_nothrow_move_constructible_v<T>) requires is_move_constructible_v<T> { move_construct_from(std::move(other)); }
    StaticVector(StaticVector&& other) requires (!is_move_constructible_v<T>) = delete;
    StaticVector& operator=(StaticVector&& other) noexcept(is_trivially_copyable_v<T> || is_nothrow_move_constructible_v<T>) requires is_move_constructible_v<T> {
        if (this == &other) return *this;
        destroy_all();
        move_construct_from(std::move(other));
        return *this;
    }
    StaticVector& operator=(StaticVector&& other) requires (!is_move_constructible_v<T>) = delete;
    [[nodiscard]] size_t size() const noexcept { return static_cast<size_t>(size_); }
    [[nodiscard]] static constexpr size_t capacity() noexcept { return Capacity; }
    [[nodiscard]] static constexpr size_t max_size() noexcept { return Capacity; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] bool full() const noexcept { return size() == Capacity; }
    T* data() noexcept { return raw_data(); }
    const T* data() const noexcept { return raw_data(); }
    iterator begin() noexcept { return data(); }
    const_iterator begin() const noexcept { return data(); }
    const_iterator cbegin() const noexcept { return data(); }
    iterator end() noexcept { return data() + size(); }
    const_iterator end() const noexcept { return data() + size(); }
    const_iterator cend() const noexcept { return data() + size(); }
    reference operator[](size_t i) noexcept { return data()[i]; }
    const_reference operator[](size_t i) const noexcept { return data()[i]; }
    reference front() noexcept { return data()[0]; }
    const_reference front() const noexcept { return data()[0]; }
    reference back() noexcept { return data()[size() - 1]; }
    const_reference back() const noexcept { return data()[size() - 1]; }
    template <class... Args>
    reference emplace_back_unchecked(Args&&... args) {
        T* p = raw_data() + size();
        std::construct_at(p, std::forward<Args>(args)...);
        ++size_;
        return *p;
    }
    template <class... Args>
    reference emplace_back(Args&&... args) {
        assert(size() < Capacity);
        return emplace_back_unchecked(std::forward<Args>(args)...);
    }
    reference push_back_unchecked(const T& value) { return emplace_back_unchecked(value); }
    reference push_back_unchecked(T&& value) { return emplace_back_unchecked(std::move(value)); }
    reference push_back(const T& value) {
        assert(size() < Capacity);
        return push_back_unchecked(value);
    }
    reference push_back(T&& value) {
        assert(size() < Capacity);
        return push_back_unchecked(std::move(value));
    }
    void pop_back_unchecked() noexcept {
        --size_;
        if constexpr (!is_trivially_destructible_v<T>) std::destroy_at(ptr_at(size()));
    }
    void pop_back() noexcept {
        assert(!empty());
        pop_back_unchecked();
    }
    void clear() noexcept { destroy_all(); }
    [[nodiscard]] std::span<T> span() noexcept { return std::span<T>(data(), size()); }
    [[nodiscard]] std::span<const T> span() const noexcept { return std::span<const T>(data(), size()); }
};

#if __INCLUDE_LEVEL__ == 0
struct StaticVectorTestTracker {
    static inline int alive = 0;
    static inline int constructed = 0;
    static inline int destroyed = 0;
    int x = 0;
    StaticVectorTestTracker() : x(0) { ++alive; ++constructed; }
    explicit StaticVectorTestTracker(int v) : x(v) { ++alive; ++constructed; }
    StaticVectorTestTracker(const StaticVectorTestTracker& other) : x(other.x) { ++alive; ++constructed; }
    StaticVectorTestTracker(StaticVectorTestTracker&& other) noexcept : x(other.x) { other.x = -1; ++alive; ++constructed; }
    StaticVectorTestTracker& operator=(const StaticVectorTestTracker& other) { x = other.x; return *this; }
    StaticVectorTestTracker& operator=(StaticVectorTestTracker&& other) noexcept { x = other.x; other.x = -1; return *this; }
    ~StaticVectorTestTracker() { --alive; ++destroyed; }
    static void reset() { alive = 0; constructed = 0; destroyed = 0; }
};
struct StaticVectorNoDefaultTrivial {
    int x;
    StaticVectorNoDefaultTrivial() = delete;
    explicit StaticVectorNoDefaultTrivial(int v) : x(v) {}
};
static_assert(is_trivially_copyable_v<StaticVectorNoDefaultTrivial>);
static_assert(is_trivially_destructible_v<StaticVector<int, 4>>);
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    {
        StaticVector<int, 0> v;
        assert(v.size() == 0);
        assert(v.capacity() == 0);
        assert(v.empty());
        assert(v.span().empty());
    }
    {
        StaticVector<int, 8> v;
        assert(v.empty());
        v.push_back(1);
        v.emplace_back(2);
        v.push_back_unchecked(3);
        assert(v.size() == 3);
        assert(v[0] == 1 && v[1] == 2 && v[2] == 3);
        assert(v.front() == 1 && v.back() == 3);
        std::span<int> s = v;
        s[1] = 20;
        assert(v[1] == 20);
        std::span<const int> cs = v;
        assert(cs.size() == v.size());
        StaticVector<int, 8> a = v;
        assert(a.size() == 3);
        assert(a[0] == 1 && a[1] == 20 && a[2] == 3);
        StaticVector<int, 8> b;
        b = a;
        assert(b.size() == 3);
        assert(b[2] == 3);
        StaticVector<int, 8> c = std::move(b);
        assert(c.size() == 3);
        assert(b.empty());
        c.pop_back();
        assert(c.size() == 2 && c.back() == 20);
        c.clear();
        assert(c.empty());
    }
    {
        StaticVector<int, 4> v = {1, 2, 3};
        assert(v.size() == 3);
        assert(v[0] == 1 && v[1] == 2 && v[2] == 3);
    }
    {
        StaticVector<StaticVectorNoDefaultTrivial, 4> v;
        v.emplace_back(10);
        v.emplace_back(20);
        assert(v.size() == 2);
        assert(v[0].x == 10 && v[1].x == 20);
        auto w = v;
        assert(w.size() == 2);
        assert(w[0].x == 10 && w[1].x == 20);
    }
    {
        StaticVector<unique_ptr<int>, 4> v;
        v.emplace_back(make_unique<int>(123));
        StaticVector<unique_ptr<int>, 4> w = std::move(v);
        assert(v.empty());
        assert(w.size() == 1);
        assert(*w[0] == 123);
    }
    {
        StaticVectorTestTracker::reset();
        {
            StaticVector<StaticVectorTestTracker, 8> v;
            v.emplace_back(1);
            v.emplace_back(2);
            assert(StaticVectorTestTracker::alive == 2);
            {
                StaticVector<StaticVectorTestTracker, 8> w = v;
                assert(w.size() == 2);
                assert(w[0].x == 1 && w[1].x == 2);
                assert(StaticVectorTestTracker::alive == 4);
                StaticVector<StaticVectorTestTracker, 8> z = std::move(w);
                assert(z.size() == 2);
                assert(w.empty());
                assert(StaticVectorTestTracker::alive == 4);
            }
            assert(StaticVectorTestTracker::alive == 2);
            v.pop_back();
            assert(StaticVectorTestTracker::alive == 1);
            v.clear();
            assert(StaticVectorTestTracker::alive == 0);
        }
        assert(StaticVectorTestTracker::alive == 0);
        assert(StaticVectorTestTracker::constructed == StaticVectorTestTracker::destroyed);
    }
    {
        mt19937 rng(1);
        for (int tc = 0; tc < 20000; ++tc) {
            StaticVector<int, 64> sv;
            vector<int> vv;
            for (int step = 0; step < 200; ++step) {
                const int op = static_cast<int>(rng() % 6);
                if (op <= 2) {
                    if (vv.size() < 64) {
                        const int x = static_cast<int>(rng());
                        sv.push_back(x);
                        vv.push_back(x);
                    }
                } else if (op == 3) {
                    if (!vv.empty()) {
                        sv.pop_back();
                        vv.pop_back();
                    }
                } else if (op == 4) {
                    StaticVector<int, 64> cp = sv;
                    assert(cp.size() == vv.size());
                    for (size_t i = 0; i < vv.size(); ++i) assert(cp[i] == vv[i]);
                    sv = cp;
                } else {
                    sv.clear();
                    vv.clear();
                }
                assert(sv.size() == vv.size());
                for (size_t i = 0; i < vv.size(); ++i) assert(sv[i] == vv[i]);
            }
        }
    }
    {
        constexpr int loops = 200000;
        constexpr int cap = 128;
        StaticVector<int, cap> v;
        long long checksum = 0;
        auto total_begin = chrono::steady_clock::now();
        long long max_ns = 0;
        for (int it = 0; it < loops; ++it) {
            auto begin = chrono::steady_clock::now();
            v.clear();
            for (int i = 0; i < cap; ++i) v.push_back_unchecked(i + it);
            for (int i = 0; i < cap / 2; ++i) {
                checksum += v.back();
                v.pop_back_unchecked();
            }
            auto end = chrono::steady_clock::now();
            const long long cur = chrono::duration_cast<chrono::nanoseconds>(end - begin).count();
            max_ns = max(max_ns, cur);
        }
        auto total_end = chrono::steady_clock::now();
        const long long total_ns = chrono::duration_cast<chrono::nanoseconds>(total_end - total_begin).count();
        const double avg_ns = static_cast<double>(total_ns) / static_cast<double>(loops);
        cerr << "StaticVector benchmark: avg_ns=" << avg_ns << " max_ns=" << max_ns << " checksum=" << checksum << '\n';
    }
    cerr << "All StaticVector tests passed.\n";
    return 0;
}
#endif
