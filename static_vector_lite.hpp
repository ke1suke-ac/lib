#pragma once
#include <bits/stdc++.h>
using namespace std;

// ArrayVector<T, Capacity>: std::arrayと有効長による固定容量の可変長配列
// コンテナ自身はヒープを使わない。Capacity > 0、要素はデフォルト構築・コピー代入可能かつ自明に破棄可能
// int、double、pair<int,int>、軽量構造体などが対象。string/vector等の資源管理型は対象外
// 全スロットのTが存在し、clear/pop/eraseでは寿命を終了しない。Boostの完全互換ではない
// Tのデフォルト構築に処理がある場合、空の構築でもCapacity個分の処理が発生する
// コピーとムーブ相当の操作は有効部分だけをコピーし、コピー元は変更しない
// 容量超過・範囲外アクセスはassertで検出。NDEBUG時は呼び出し側が前提を守る
template<class T, size_t Capacity>
class ArrayVector {
    static_assert(Capacity > 0);
    static_assert(is_default_constructible_v<T> && is_copy_assignable_v<T> && is_trivially_destructible_v<T>);
    array<T, Capacity> values_;
    size_t size_ = 0;

public:
    using value_type = T;
    using size_type = size_t;
    using iterator = T*;
    using const_iterator = const T*;

    // 空で構築する。スカラーの未使用領域は初期化しない: O(1)、Tの初期化が必要ならO(Capacity)
    ArrayVector() {}

    // count個をvalueで初期化する: O(count)、Tの初期化が必要ならさらにO(Capacity)
    explicit ArrayVector(size_t count, const T& value = T{}) { assign(count, value); }

    // 初期化リストから構築する: O(要素数)、Tの初期化が必要ならさらにO(Capacity)
    ArrayVector(initializer_list<T> values) { assign(values.begin(), values.end()); }

    // イテレータ区間から構築する: O(要素数)、Tの初期化が必要ならさらにO(Capacity)
    template<input_iterator It>
    ArrayVector(It first, It last) { assign(first, last); }

    // 有効部分をコピーする。右辺値もこのコンストラクタでコピーする: O(size)、Tの初期化が必要ならさらにO(Capacity)
    ArrayVector(const ArrayVector& other) : size_(other.size_) {
        copy_n(other.begin(), size_, begin());
    }

    // 有効部分をコピー代入する。右辺値もこの演算子を使う: O(other.size)
    ArrayVector& operator=(const ArrayVector& other) {
        if (this != &other) {
            copy_n(other.begin(), other.size_, begin());
            size_ = other.size_;
        }
        return *this;
    }

    // 初期化リストを代入する: O(要素数)
    ArrayVector& operator=(initializer_list<T> values) {
        assign(values.begin(), values.end());
        return *this;
    }

    // 有効要素数を返す: O(1)
    size_t size() const { return size_; }
    // 最大要素数を返す: O(1)
    static constexpr size_t capacity() { return Capacity; }
    // 空かを返す: O(1)
    bool empty() const { return size_ == 0; }
    // 容量内であることだけを確認する。確保は行わない: O(1)
    void reserve(size_t count) const { assert(count <= Capacity); (void)count; }

    // 先頭ポインタを返す。有効区間は[data(), data()+size()): O(1)
    T* data() { return values_.data(); }
    // 読み取り用の先頭ポインタを返す: O(1)
    const T* data() const { return values_.data(); }
    // 有効区間の先頭を返す: O(1)
    iterator begin() { return data(); }
    // 読み取り用の先頭を返す: O(1)
    const_iterator begin() const { return data(); }
    // 有効区間の終端を返す: O(1)
    iterator end() { return data() + size_; }
    // 読み取り用の終端を返す: O(1)
    const_iterator end() const { return data() + size_; }
    // 読み取り用の先頭を返す: O(1)
    const_iterator cbegin() const { return begin(); }
    // 読み取り用の終端を返す: O(1)
    const_iterator cend() const { return end(); }
    // 逆順区間の先頭を返す: O(1)
    auto rbegin() { return make_reverse_iterator(end()); }
    // 読み取り用の逆順区間の先頭を返す: O(1)
    auto rbegin() const { return make_reverse_iterator(end()); }
    // 逆順区間の終端を返す: O(1)
    auto rend() { return make_reverse_iterator(begin()); }
    // 読み取り用の逆順区間の終端を返す: O(1)
    auto rend() const { return make_reverse_iterator(begin()); }

    // index番目を参照する: O(1)
    T& operator[](size_t index) { assert(index < size_); return values_[index]; }
    // index番目を読み取り参照する: O(1)
    const T& operator[](size_t index) const { assert(index < size_); return values_[index]; }
    // 先頭要素を参照する。空では使えない: O(1)
    T& front() { return (*this)[0]; }
    // 先頭要素を読み取り参照する。空では使えない: O(1)
    const T& front() const { return (*this)[0]; }
    // 末尾要素を参照する。空では使えない: O(1)
    T& back() { assert(size_ != 0); return values_[size_ - 1]; }
    // 末尾要素を読み取り参照する。空では使えない: O(1)
    const T& back() const { assert(size_ != 0); return values_[size_ - 1]; }

    // 末尾に追加する: O(1)
    void push_back(const T& value) {
        assert(size_ < Capacity);
        values_[size_] = value;
        ++size_;
    }

    // 引数から値を作り、末尾スロットに代入して参照を返す: O(Tの構築・代入)
    template<class... Args>
    T& emplace_back(Args&&... args) {
        assert(size_ < Capacity);
        values_[size_] = T(forward<Args>(args)...);
        return values_[size_++];
    }

    // 末尾を削除する。空では使えない: O(1)
    void pop_back() { assert(size_ != 0); --size_; }
    // 有効要素数を0にする: O(1)
    void clear() { size_ = 0; }

    // 要素数を変更する。増加部分だけvalueで埋める: O(max(0, count-size))
    void resize(size_t count, T value = T{}) {
        assert(count <= Capacity);
        if (count > size_) fill(begin() + size_, begin() + count, value);
        size_ = count;
    }

    // count個のvalueで置き換える: O(count)
    void assign(size_t count, T value) {
        assert(count <= Capacity);
        fill_n(begin(), count, value);
        size_ = count;
    }

    // 区間で置き換える。自身の有効部分の部分区間も指定可能: O(要素数)
    template<input_iterator It>
    void assign(It first, It last) {
        clear();
        for (; first != last; ++first) push_back(*first);
    }

    // 初期化リストで置き換える: O(要素数)
    void assign(initializer_list<T> values) { assign(values.begin(), values.end()); }

    // posの前に1要素を挿入する。valueが自身の要素でもよい: O(end-pos)
    iterator insert(const_iterator pos, T value) {
        const size_t index = (size_t)(pos - data());
        assert(index <= size_ && size_ < Capacity);
        copy_backward(begin() + index, end(), end() + 1);
        values_[index] = value;
        ++size_;
        return begin() + index;
    }

    // [first,last)を削除し、後続要素の位置を返す: O(end-last)
    iterator erase(const_iterator first, const_iterator last) {
        const size_t index = (size_t)(first - data());
        const size_t finish = (size_t)(last - data());
        assert(index <= finish && finish <= size_);
        if (index != finish) copy(begin() + finish, end(), begin() + index);
        size_ -= finish - index;
        return begin() + index;
    }

    // posの1要素を削除し、後続要素の位置を返す: O(end-pos)
    iterator erase(const_iterator pos) {
        assert(pos >= begin() && pos < end());
        return erase(pos, pos + 1);
    }

    // 末尾要素で埋めてindex番目を削除する。順序は保たない: O(1)
    void erase_unordered(size_t index) {
        assert(index < size_);
        values_[index] = values_[size_ - 1];
        --size_;
    }

    // 有効部分と要素数を交換する。未使用領域は読まない: O(max(size, other.size))
    void swap(ArrayVector& other) {
        if (this == &other) return;
        // 両方に存在する部分を交換し、長い側の残りを短い側へコピーする
        const size_t common = min(size_, other.size_);
        swap_ranges(begin(), begin() + common, other.begin());
        if (size_ > common) copy(begin() + common, end(), other.begin() + common);
        else if (other.size_ > common) copy(other.begin() + common, other.end(), begin() + common);
        std::swap(size_, other.size_);
    }

    // ADLによるswapで有効部分を交換する: O(max(a.size, b.size))
    friend void swap(ArrayVector& a, ArrayVector& b) { a.swap(b); }

    // 有効部分の一致を調べる: O(size)
    bool operator==(const ArrayVector& other) const {
        return size_ == other.size_ && equal(begin(), end(), other.begin());
    }
};

#if __INCLUDE_LEVEL__ == 0

// 確認条件はNDEBUGでも実行し、ライブラリ側のassertと独立させる
void check(bool condition) { if (!condition) abort(); }

template<size_t Capacity>
void random_test(uint64_t seed) {
    mt19937_64 rng(seed);
    ArrayVector<int, Capacity> a, b;
    vector<int> x, y;
    auto verify = [](const auto& actual, const auto& expected) {
        check(actual.size() == expected.size());
        check(equal(actual.begin(), actual.end(), expected.begin()));
        check(actual.empty() == expected.empty());
    };
    for (int step = 0; step < 3000; ++step) {
        const int op = (int)(rng() % 16);
        const int value = (int)(rng() % 2001) - 1000;
        const size_t count = (size_t)(rng() % (Capacity + 1));
        if (op == 0 && x.size() < Capacity) { a.push_back(value); x.push_back(value); }
        else if (op == 1 && !x.empty()) { a.pop_back(); x.pop_back(); }
        else if (op == 2) { a.resize(count, value); x.resize(count, value); }
        else if (op == 3) { a.assign(count, value); x.assign(count, value); }
        else if (op == 4 && x.size() < Capacity) {
            const size_t at = (size_t)(rng() % (x.size() + 1));
            auto p = a.insert(a.begin() + at, value);
            x.insert(x.begin() + (ptrdiff_t)at, value);
            check(p == a.begin() + at);
        } else if (op == 5 && !x.empty()) {
            const size_t at = (size_t)(rng() % x.size());
            auto p = a.erase(a.begin() + at);
            x.erase(x.begin() + (ptrdiff_t)at);
            check(p == a.begin() + at);
        } else if (op == 6) {
            size_t l = (size_t)(rng() % (x.size() + 1));
            size_t r = (size_t)(rng() % (x.size() + 1));
            if (l > r) std::swap(l, r);
            a.erase(a.begin() + l, a.begin() + r);
            x.erase(x.begin() + (ptrdiff_t)l, x.begin() + (ptrdiff_t)r);
        } else if (op == 7) { b = a; y = x; }
        else if (op == 8) { a.swap(b); x.swap(y); }
        else if (op == 9) { a.clear(); x.clear(); }
        else if (op == 10 && !x.empty()) {
            const size_t at = (size_t)(rng() % x.size());
            a.erase_unordered(at); x[at] = x.back(); x.pop_back();
        } else if (op == 11) { sort(a.begin(), a.end()); sort(x.begin(), x.end()); }
        else if (op == 12) { reverse(a.begin(), a.end()); reverse(x.begin(), x.end()); }
        else if (op == 13 && x.size() < Capacity && !x.empty()) {
            const size_t at = (size_t)(rng() % (x.size() + 1));
            const size_t from = (size_t)(rng() % x.size());
            const int saved = x[from];
            a.insert(a.begin() + at, a[from]); x.insert(x.begin() + (ptrdiff_t)at, saved);
        } else if (op == 14) {
            const auto copy = a;
            verify(copy, x);
            ArrayVector<int, Capacity> moved(std::move(a));
            verify(moved, x); verify(a, x);
            a = a; a.swap(a);
        } else if (op == 15 && !x.empty()) {
            a.resize(count, a.front());
            const int saved = x.front(); x.resize(count, saved);
        }
        verify(a, x); verify(b, y);
    }
}

int main() {
    // 空、満杯、再利用、自己参照、constアクセス、標準アルゴリズムを確認する
    ArrayVector<int, 4> a;
    check(a.empty() && a.begin() == a.end() && a.capacity() == 4);
    a.erase(a.begin(), a.end());
    a.reserve(4);
    for (int i = 0; i < 4; ++i) a.push_back(i);
    a.assign(a.begin() + 1, a.end());
    check((a == ArrayVector<int, 4>{1, 2, 3}));
    a.assign(a.begin(), a.end());
    a.push_back(a.front());
    a.clear(); a.resize(4);
    check(all_of(a.begin(), a.end(), [](int v) { return v == 0; }));
    a = {1, 2, 3};
    const auto& ca = a;
    check(ca.front() == 1 && ca.back() == 3 && *ca.rbegin() == 3);
    check(ca.cend() - ca.cbegin() == 3 && ca.rend() - ca.rbegin() == 3);
    span<int> view(a);
    view[1] = 8;
    check(a[1] == 8 && a.data() == a.begin());

    // pair、構造体、bool、double、過剰アラインメント、入力イテレータを確認する
    ArrayVector<pair<int, int>, 4> pairs;
    pairs.emplace_back(3, 7);
    check(pairs.back() == make_pair(3, 7));
    struct Item { int x = 5; double y = 2; };
    ArrayVector<Item, 3> items;
    items.emplace_back(4, 1.5);
    items.resize(3);
    check(items[0].x == 4 && items[2].x == 5);
    ArrayVector<bool, 2> bits{true, false};
    bool& bit = bits[1]; bit = true;
    check(bits.back());
    ArrayVector<double, 2> reals{1.25, -2.5};
    reals.erase_unordered(0); check(reals.front() == -2.5);
    struct alignas(64) Aligned { long long value; };
    ArrayVector<Aligned, 2> aligned;
    aligned.emplace_back(7);
    check((uintptr_t)aligned.data() % 64 == 0 && aligned[0].value == 7);
    istringstream input("3 1 4");
    ArrayVector<int, 8> stream_values{istream_iterator<int>(input), istream_iterator<int>()};
    check(stream_values.size() == 3 && stream_values.back() == 4);

    // 2群200seed、各容量3000操作をstd::vectorと逐次比較する
    for (uint64_t seed = 0; seed < 200; ++seed) {
        random_test<1>(seed); random_test<8>(seed);
        random_test<64>(seed); random_test<257>(seed);
    }
    cout << "PASS: 2400000 randomized operations + edge/type tests\n";
}
#endif
