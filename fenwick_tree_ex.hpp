#pragma once
#include <bits/stdc++.h>

// Fenwick tree関連の軽量ライブラリ
// prefix min/max、xor、区間加算系、頻度配列による順序統計、密な2次元/任意次元Fenwick treeを提供する
// いずれも0-indexed、区間は半開区間[l, r)として扱う

// ============================================================
// ACL Fenwickを使う派生Fenwick
// ============================================================
#include <atcoder/fenwicktree>

// 差分配列をACLのFenwick treeで管理する区間加算・一点取得用の軽量構造体
template <class T>
struct fenwick_range_add_point_get {
  public:
    // 空の構造体を構築する O(1)
    fenwick_range_add_point_get() : _n(0), bit(0) {}

    // サイズnの構造体をゼロ初期化で構築する O(n)
    explicit fenwick_range_add_point_get(int n) : _n(0), bit(0) {
        assign(n);
    }

    // 要素数を返す O(1)
    int size() const {
        return _n;
    }

    // 要素数が0かを返す O(1)
    bool empty() const {
        return _n == 0;
    }

    // サイズnでゼロ初期化し直す O(n)
    void assign(int n) {
        assert(0 <= n);
        _n = n;
        bit = atcoder::fenwick_tree<T>(n);
    }

    // 現在のサイズを保って全要素をゼロにする O(n)
    void clear() {
        bit = atcoder::fenwick_tree<T>(_n);
    }

    // 区間[l, r)にxを加算する O(log n)
    void add(int l, int r, T x) {
        assert(0 <= l && l <= r && r <= _n);
        if (l == r) return;
        bit.add(l, x);
        if (r < _n) bit.add(r, -x);
    }

    // a[p]を返す O(log n)
    T get(int p) {
        assert(0 <= p && p < _n);
        return bit.sum(0, p + 1);
    }

  private:
    int _n;
    atcoder::fenwick_tree<T> bit;
};

// 2本のACL Fenwick treeで区間加算・区間和取得を行う軽量構造体
template <class T>
struct fenwick_range_add_range_sum {
  public:
    // 空の構造体を構築する O(1)
    fenwick_range_add_range_sum() : _n(0), bit0(0), bit1(0) {}

    // サイズnの構造体をゼロ初期化で構築する O(n)
    explicit fenwick_range_add_range_sum(int n) : _n(0), bit0(0), bit1(0) {
        assign(n);
    }

    // 要素数を返す O(1)
    int size() const {
        return _n;
    }

    // 要素数が0かを返す O(1)
    bool empty() const {
        return _n == 0;
    }

    // サイズnでゼロ初期化し直す O(n)
    void assign(int n) {
        assert(0 <= n);
        _n = n;
        bit0 = atcoder::fenwick_tree<T>(n);
        bit1 = atcoder::fenwick_tree<T>(n);
    }

    // 現在のサイズを保って全要素をゼロにする O(n)
    void clear() {
        bit0 = atcoder::fenwick_tree<T>(_n);
        bit1 = atcoder::fenwick_tree<T>(_n);
    }

    // 区間[l, r)にxを加算する O(log n)
    void add(int l, int r, T x) {
        assert(0 <= l && l <= r && r <= _n);
        if (l == r) return;

        // prefix_sum(r) = bit0.sum(0, r) * r + bit1.sum(0, r)となるように係数を更新する
        bit0.add(l, x);
        if (r < _n) bit0.add(r, -x);
        bit1.add(l, -x * T(l));
        if (r < _n) bit1.add(r, x * T(r));
    }

    // 区間[0, r)の和を返す O(log n)
    T prefix_sum(int r) {
        assert(0 <= r && r <= _n);
        return bit0.sum(0, r) * T(r) + bit1.sum(0, r);
    }

    // 区間[l, r)の和を返す O(log n)
    T sum(int l, int r) {
        assert(0 <= l && l <= r && r <= _n);
        return prefix_sum(r) - prefix_sum(l);
    }

    // a[p]を返す O(log n)
    T get(int p) {
        assert(0 <= p && p < _n);
        return bit0.sum(0, p + 1);
    }

  private:
    int _n;
    atcoder::fenwick_tree<T> bit0;
    atcoder::fenwick_tree<T> bit1;
};

// ============================================================
// 独自実装の派生Fenwick
// ============================================================


// 点chminとprefix最小値取得に特化したFenwick tree
// 更新後の値を大きく戻す操作、任意区間最小値取得には対応しない
template <class T>
struct fenwick_prefix_min {
  public:
    // 空のFenwick treeを構築する O(1)
    fenwick_prefix_min() : _n(0), _inf(std::numeric_limits<T>::max()) {}

    // サイズn、単位元infのFenwick treeを構築する O(n)
    explicit fenwick_prefix_min(int n, T inf = std::numeric_limits<T>::max()) : _n(0), _inf(inf) {
        assign(n, inf);
    }

    // 要素数を返す O(1)
    int size() const {
        return _n;
    }

    // 要素数が0かを返す O(1)
    bool empty() const {
        return _n == 0;
    }

    // サイズn、単位元infで初期化し直す O(n)
    void assign(int n, T inf = std::numeric_limits<T>::max()) {
        assert(0 <= n);
        _n = n;
        _inf = inf;
        data.assign(n, inf);
    }

    // 現在のサイズを保って全要素を単位元に戻す O(n)
    void clear() {
        std::fill(data.begin(), data.end(), _inf);
    }

    // a[p]をmin(a[p], x)で更新する O(log n)
    void chmin(int p, T x) {
        assert(0 <= p && p < _n);
        p++;
        while (p <= _n) {
            data[p - 1] = std::min(data[p - 1], x);
            p += p & -p;
        }
    }

    // 区間[0, r)の最小値を返す O(log n)
    T prefix_min(int r) const {
        assert(0 <= r && r <= _n);
        T res = _inf;
        while (r > 0) {
            res = std::min(res, data[r - 1]);
            r -= r & -r;
        }
        return res;
    }

  private:
    int _n;
    T _inf;
    std::vector<T> data;
};

// 点chmaxとprefix最大値取得に特化したFenwick tree
// 更新後の値を小さく戻す操作、任意区間最大値取得には対応しない
template <class T>
struct fenwick_prefix_max {
  public:
    // 空のFenwick treeを構築する O(1)
    fenwick_prefix_max() : _n(0), _neg_inf(std::numeric_limits<T>::lowest()) {}

    // サイズn、単位元neg_infのFenwick treeを構築する O(n)
    explicit fenwick_prefix_max(int n, T neg_inf = std::numeric_limits<T>::lowest()) : _n(0), _neg_inf(neg_inf) {
        assign(n, neg_inf);
    }

    // 要素数を返す O(1)
    int size() const {
        return _n;
    }

    // 要素数が0かを返す O(1)
    bool empty() const {
        return _n == 0;
    }

    // サイズn、単位元neg_infで初期化し直す O(n)
    void assign(int n, T neg_inf = std::numeric_limits<T>::lowest()) {
        assert(0 <= n);
        _n = n;
        _neg_inf = neg_inf;
        data.assign(n, neg_inf);
    }

    // 現在のサイズを保って全要素を単位元に戻す O(n)
    void clear() {
        std::fill(data.begin(), data.end(), _neg_inf);
    }

    // a[p]をmax(a[p], x)で更新する O(log n)
    void chmax(int p, T x) {
        assert(0 <= p && p < _n);
        p++;
        while (p <= _n) {
            data[p - 1] = std::max(data[p - 1], x);
            p += p & -p;
        }
    }

    // 区間[0, r)の最大値を返す O(log n)
    T prefix_max(int r) const {
        assert(0 <= r && r <= _n);
        T res = _neg_inf;
        while (r > 0) {
            res = std::max(res, data[r - 1]);
            r -= r & -r;
        }
        return res;
    }

  private:
    int _n;
    T _neg_inf;
    std::vector<T> data;
};

// 点xor更新・区間xor取得に特化したFenwick tree
// ACLのfenwick_treeでは加減算以外の群演算を扱えないため、xor専用で提供する
template <class T>
struct fenwick_tree_xor {
  public:
    // 空のFenwick treeを構築する O(1)
    fenwick_tree_xor() : _n(0) {}

    // サイズnのFenwick treeをゼロ初期化で構築する O(n)
    explicit fenwick_tree_xor(int n) : _n(0) {
        assign(n);
    }

    // 要素数を返す O(1)
    int size() const {
        return _n;
    }

    // 要素数が0かを返す O(1)
    bool empty() const {
        return _n == 0;
    }

    // サイズnでゼロ初期化し直す O(n)
    void assign(int n) {
        assert(0 <= n);
        _n = n;
        data.assign(n, T(0));
    }

    // 現在のサイズを保って全要素をゼロにする O(n)
    void clear() {
        std::fill(data.begin(), data.end(), T(0));
    }

    // a[p]にxをxorする O(log n)
    void apply(int p, T x) {
        assert(0 <= p && p < _n);
        p++;
        while (p <= _n) {
            data[p - 1] ^= x;
            p += p & -p;
        }
    }

    // 区間[0, r)のxorを返す O(log n)
    T prefix_xor(int r) const {
        assert(0 <= r && r <= _n);
        T res = T(0);
        while (r > 0) {
            res ^= data[r - 1];
            r -= r & -r;
        }
        return res;
    }

    // 区間[l, r)のxorを返す O(log n)
    T prod(int l, int r) const {
        assert(0 <= l && l <= r && r <= _n);
        return prefix_xor(l) ^ prefix_xor(r);
    }

  private:
    int _n;
    std::vector<T> data;
};

// 非負頻度を個数配列とFenwick treeで管理する値域固定の順序付き多重集合
struct fenwick_ordered_multiset {
  public:
    // 空の多重集合を構築する O(1)
    fenwick_ordered_multiset() : _n(0), _bit(0), _size(0) {}

    // 値域[0, n)の空の多重集合を構築する O(n)
    explicit fenwick_ordered_multiset(int n) : _n(0), _bit(0), _size(0) {
        assign(n);
    }

    // 値域サイズを返す O(1)
    int value_size() const {
        return _n;
    }

    // 格納されている要素数を返す O(1)
    long long size() const {
        return _size;
    }

    // 要素数が0かを返す O(1)
    bool empty() const {
        return _size == 0;
    }

    // 値域[0, n)の空の多重集合に初期化し直す O(n)
    void assign(int n) {
        assert(0 <= n);
        _n = n;
        _bit = max_bit(n);
        _size = 0;
        cnts.assign(n, 0);
        data.assign(n, 0);
    }

    // 現在の値域を保って全要素を削除する O(n)
    void clear() {
        _size = 0;
        std::fill(cnts.begin(), cnts.end(), 0);
        std::fill(data.begin(), data.end(), 0);
    }

    // 値xをcnt個追加する O(log n)
    void add(int x, long long cnt = 1) {
        assert(0 <= x && x < _n);
        assert(0 <= cnt);
        if (cnt == 0) return;
        _size += cnt;
        cnts[x] += cnt;
        add_count(x, cnt);
    }

    // 値xをcnt個削除する O(log n)
    void erase(int x, long long cnt = 1) {
        assert(0 <= x && x < _n);
        assert(0 <= cnt);
        assert(cnt <= cnts[x]);
        if (cnt == 0) return;
        _size -= cnt;
        cnts[x] -= cnt;
        add_count(x, -cnt);
    }

    // 値xを1個削除する O(log n)
    void erase_one(int x) {
        erase(x, 1);
    }

    // 値xの個数を返す O(1)
    long long count(int x) const {
        assert(0 <= x && x < _n);
        return cnts[x];
    }

    // 値xが存在するかを返す O(1)
    bool contains(int x) const {
        return count(x) > 0;
    }

    // x未満の要素数を返す O(log n)
    long long count_less(int x) const {
        assert(0 <= x && x <= _n);
        return prefix_count(x);
    }

    // x以下の要素数を返す O(log n)
    long long count_leq(int x) const {
        assert(0 <= x && x < _n);
        return prefix_count(x + 1);
    }

    // 区間[l, r)に含まれる要素数を返す O(log n)
    long long count_range(int l, int r) const {
        assert(0 <= l && l <= r && r <= _n);
        return prefix_count(r) - prefix_count(l);
    }

    // 0-indexedでk番目の要素を返す O(log n)
    int kth(long long k) const {
        assert(0 <= k && k < _size);
        return lower_bound(k + 1);
    }

    // 最小要素を返す O(log n)
    int min_element() const {
        assert(!empty());
        return kth(0);
    }

    // 最大要素を返す O(log n)
    int max_element() const {
        assert(!empty());
        return kth(_size - 1);
    }

  private:
    int _n;
    int _bit;
    long long _size;
    std::vector<long long> cnts;
    std::vector<long long> data;

    static int max_bit(int n) {
        int k = 1;
        while (k <= n / 2) k <<= 1;
        return n == 0 ? 0 : k;
    }

    void add_count(int p, long long cnt) {
        p++;
        while (p <= _n) {
            data[p - 1] += cnt;
            p += p & -p;
        }
    }

    long long prefix_count(int r) const {
        long long s = 0;
        while (r > 0) {
            s += data[r - 1];
            r -= r & -r;
        }
        return s;
    }

    int lower_bound(long long w) const {
        // xは条件を満たさないprefix長、sはそのprefix和として管理する
        int x = 0;
        long long s = 0;
        int k = _bit;

        // 大きい2冪から順に試し、prefix_count(x) < wを保つ最大のxを求める
        while (k > 0) {
            int next = x + k;
            if (next <= _n && s + data[next - 1] < w) {
                s += data[next - 1];
                x = next;
            }
            k >>= 1;
        }
        return x;
    }
};

// 2次元配列に対する点加算・矩形和取得を行う密なFenwick tree
template <class T>
struct fenwick_tree_2d {
  private:
    template <class X, bool B = std::is_integral_v<X> && std::is_signed_v<X>>
    struct to_unsigned_impl {
        using type = X;
    };
    template <class X>
    struct to_unsigned_impl<X, true> {
        using type = std::make_unsigned_t<X>;
    };

    using U = typename to_unsigned_impl<T>::type;

  public:
    // 空の2次元Fenwick treeを構築する O(1)
    fenwick_tree_2d() : _h(0), _w(0) {}

    // 高さh、幅wの2次元Fenwick treeをゼロ初期化で構築する O(hw)
    fenwick_tree_2d(int h, int w) : _h(0), _w(0) {
        assign(h, w);
    }

    // 高さを返す O(1)
    int height() const {
        return _h;
    }

    // 幅を返す O(1)
    int width() const {
        return _w;
    }

    // 高さまたは幅が0かを返す O(1)
    bool empty() const {
        return _h == 0 || _w == 0;
    }

    // 高さh、幅wでゼロ初期化し直す O(hw)
    void assign(int h, int w) {
        assert(0 <= h && 0 <= w);
        _h = h;
        _w = w;
        data.assign(size_t(h) * size_t(w), U(0));
    }

    // 現在のサイズを保って全要素をゼロにする O(hw)
    void clear() {
        std::fill(data.begin(), data.end(), U(0));
    }

    // a[x][y]にvを加算する O(log h log w)
    void add(int x, int y, T v) {
        assert(0 <= x && x < _h);
        assert(0 <= y && y < _w);
        x++;
        y++;

        // x方向のFenwickノードごとに、対応するy方向Fenwickノードを更新する
        int xi = x;
        while (xi <= _h) {
            int yi = y;
            size_t base = size_t(xi - 1) * size_t(_w);
            while (yi <= _w) {
                data[base + size_t(yi - 1)] += U(v);
                yi += yi & -yi;
            }
            xi += xi & -xi;
        }
    }

    // 矩形[0, x) * [0, y)の和を返す O(log h log w)
    T prefix_sum(int x, int y) const {
        assert(0 <= x && x <= _h);
        assert(0 <= y && y <= _w);

        U s = U(0);

        // x方向に降りながら、各ノードのy方向prefixを足し込む
        while (x > 0) {
            int yi = y;
            size_t base = size_t(x - 1) * size_t(_w);
            while (yi > 0) {
                s += data[base + size_t(yi - 1)];
                yi -= yi & -yi;
            }
            x -= x & -x;
        }
        return T(s);
    }

    // 矩形[xl, xr) * [yl, yr)の和を返す O(log h log w)
    T sum(int xl, int xr, int yl, int yr) const {
        assert(0 <= xl && xl <= xr && xr <= _h);
        assert(0 <= yl && yl <= yr && yr <= _w);
        return prefix_sum(xr, yr) - prefix_sum(xl, yr) - prefix_sum(xr, yl) + prefix_sum(xl, yl);
    }

  private:
    int _h;
    int _w;
    std::vector<U> data;
};



// 任意次元の密な配列に対する点加算・超直方体和取得を行うFenwick tree
// 次元数Dをテンプレートパラメータで固定する汎用版で、2次元だけならfenwick_tree_2dの方が軽い
template <class T, size_t D>
struct fenwick_tree_nd {
  private:
    static_assert(0 < D, "fenwick_tree_nd requires positive dimension");

    template <class X, bool B = std::is_integral_v<X> && std::is_signed_v<X>>
    struct to_unsigned_impl {
        using type = X;
    };
    template <class X>
    struct to_unsigned_impl<X, true> {
        using type = std::make_unsigned_t<X>;
    };

    using U = typename to_unsigned_impl<T>::type;

  public:
    // 空のD次元Fenwick treeを構築する O(D)
    fenwick_tree_nd() : _total_size(0) {
        _sizes.fill(0);
        _strides.fill(1);
    }

    // 各次元のサイズを指定してゼロ初期化で構築する O(Πn_i)
    explicit fenwick_tree_nd(const std::array<int, D>& sizes) : fenwick_tree_nd() {
        assign(sizes);
    }

    // 次元数を返す O(1)
    int dim() const {
        return int(D);
    }

    // 各次元のサイズを返す O(1)
    const std::array<int, D>& sizes() const {
        return _sizes;
    }

    // axis次元のサイズを返す O(1)
    int size(int axis) const {
        assert(0 <= axis && axis < int(D));
        return _sizes[size_t(axis)];
    }

    // いずれかの次元サイズが0かを返す O(1)
    bool empty() const {
        return _total_size == 0;
    }

    // 各次元のサイズを指定してゼロ初期化し直す O(Πn_i)
    void assign(const std::array<int, D>& sizes) {
        _sizes = sizes;

        // 後ろの次元からstrideと全体サイズを計算する
        size_t prod = 1;
        for (int i = int(D) - 1; i >= 0; i--) {
            assert(0 <= _sizes[size_t(i)]);
            _strides[size_t(i)] = prod;
            if (_sizes[size_t(i)] == 0) {
                prod = 0;
            } else if (prod != 0) {
                assert(size_t(_sizes[size_t(i)]) <= std::numeric_limits<size_t>::max() / prod);
                prod *= size_t(_sizes[size_t(i)]);
            }
        }
        _total_size = prod;
        data.assign(_total_size, U(0));
    }

    // 現在のサイズを保って全要素をゼロにする O(Πn_i)
    void clear() {
        std::fill(data.begin(), data.end(), U(0));
    }

    // 点pにvを加算する O(D * Πlog n_i)
    void add(const std::array<int, D>& p, T v) {
        std::array<int, D> idx;
        for (int i = 0; i < int(D); i++) {
            assert(0 <= p[size_t(i)] && p[size_t(i)] < _sizes[size_t(i)]);
            idx[size_t(i)] = p[size_t(i)] + 1;
        }

        // 各次元のFenwickノードの直積を、末尾次元から順に進めて更新する
        while (true) {
            data[flat_index(idx)] += U(v);
            int d = int(D) - 1;
            while (d >= 0) {
                idx[size_t(d)] += idx[size_t(d)] & -idx[size_t(d)];
                if (idx[size_t(d)] <= _sizes[size_t(d)]) {
                    for (int e = d + 1; e < int(D); e++) idx[size_t(e)] = p[size_t(e)] + 1;
                    break;
                }
                d--;
            }
            if (d < 0) break;
        }
    }

    // 超直方体[0, r_0) * ... * [0, r_{D-1})の和を返す O(D * Πlog n_i)
    T prefix_sum(const std::array<int, D>& r) const {
        for (int i = 0; i < int(D); i++) assert(0 <= r[size_t(i)] && r[size_t(i)] <= _sizes[size_t(i)]);
        return T(prefix_sum_raw(r));
    }

    // 超直方体[l_0, r_0) * ... * [l_{D-1}, r_{D-1})の和を返す O(2^D * D * Πlog n_i)
    T sum(const std::array<int, D>& l, const std::array<int, D>& r) const {
        for (int i = 0; i < int(D); i++) assert(0 <= l[size_t(i)] && l[size_t(i)] <= r[size_t(i)] && r[size_t(i)] <= _sizes[size_t(i)]);

        // 包除原理で2^D個のprefix和を足し引きする
        U res = U(0);
        std::array<int, D> p;
        std::array<unsigned char, D> use_l{};
        while (true) {
            int parity = 0;
            for (int i = 0; i < int(D); i++) {
                if (use_l[size_t(i)]) {
                    p[size_t(i)] = l[size_t(i)];
                    parity++;
                } else {
                    p[size_t(i)] = r[size_t(i)];
                }
            }
            U v = prefix_sum_raw(p);
            if (parity & 1) res -= v;
            else res += v;

            int d = int(D) - 1;
            while (d >= 0 && use_l[size_t(d)]) {
                use_l[size_t(d)] = 0;
                d--;
            }
            if (d < 0) break;
            use_l[size_t(d)] = 1;
        }
        return T(res);
    }

  private:
    size_t _total_size;
    std::array<int, D> _sizes;
    std::array<size_t, D> _strides;
    std::vector<U> data;

    size_t flat_index(const std::array<int, D>& idx) const {
        size_t res = 0;
        for (int i = 0; i < int(D); i++) res += size_t(idx[size_t(i)] - 1) * _strides[size_t(i)];
        return res;
    }

    U prefix_sum_raw(const std::array<int, D>& r) const {
        for (int i = 0; i < int(D); i++) {
            if (r[size_t(i)] == 0) return U(0);
        }

        // 各次元のFenwickノードを降りる直積を、末尾次元から順に進めて集約する
        U s = U(0);
        std::array<int, D> idx = r;
        while (true) {
            s += data[flat_index(idx)];
            int d = int(D) - 1;
            while (d >= 0) {
                idx[size_t(d)] -= idx[size_t(d)] & -idx[size_t(d)];
                if (idx[size_t(d)] > 0) {
                    for (int e = d + 1; e < int(D); e++) idx[size_t(e)] = r[size_t(e)];
                    break;
                }
                d--;
            }
            if (d < 0) break;
        }
        return s;
    }
};

// ============================================================
// テスト
// ============================================================
#if __INCLUDE_LEVEL__ == 0
#include <atcoder/lazysegtree>
#include <atcoder/segtree>

using ll = long long;

ll seg_sum_op(ll a, ll b) { return a + b; }
ll seg_sum_e() { return 0; }

ll seg_min_op(ll a, ll b) { return std::min(a, b); }
ll seg_min_e() { return std::numeric_limits<ll>::max(); }
ll seg_max_op(ll a, ll b) { return std::max(a, b); }
ll seg_max_e() { return std::numeric_limits<ll>::lowest(); }
unsigned long long seg_xor_op(unsigned long long a, unsigned long long b) { return a ^ b; }
unsigned long long seg_xor_e() { return 0; }

struct lazy_sum_node {
    ll sum;
    int len;
};

lazy_sum_node lazy_sum_op(lazy_sum_node a, lazy_sum_node b) {
    return {a.sum + b.sum, a.len + b.len};
}

lazy_sum_node lazy_sum_e() {
    return {0, 0};
}

lazy_sum_node lazy_sum_mapping(ll f, lazy_sum_node x) {
    return {x.sum + f * x.len, x.len};
}

ll lazy_sum_composition(ll f, ll g) {
    return f + g;
}

ll lazy_sum_id() {
    return 0;
}

ll benchmark_sink = 0;

void require_true(bool ok, const std::string& message) {
    if (!ok) {
        std::cerr << "test failed: " << message << '\n';
        std::abort();
    }
}

ll naive_sum_1d(const std::vector<ll>& a, int l, int r) {
    ll s = 0;
    for (int i = l; i < r; i++) s += a[i];
    return s;
}

void test_fenwick_range_add_point_get() {
    {
        fenwick_range_add_point_get<ll> fw;
        require_true(fw.size() == 0, "range_add_point_get default size");
        require_true(fw.empty(), "range_add_point_get default empty");
    }
    {
        int n = 5;
        std::vector<ll> a(n, 0);
        fenwick_range_add_point_get<ll> fw(n);
        fw.add(0, 5, 3);
        for (int i = 0; i < n; i++) a[i] += 3;
        fw.add(2, 4, -7);
        for (int i = 2; i < 4; i++) a[i] -= 7;
        fw.add(1, 1, 100);
        for (int i = 0; i < n; i++) require_true(fw.get(i) == a[i], "range_add_point_get fixed get");
        fw.clear();
        std::fill(a.begin(), a.end(), 0);
        for (int i = 0; i < n; i++) require_true(fw.get(i) == a[i], "range_add_point_get clear");
        fw.assign(3);
        require_true(fw.size() == 3, "range_add_point_get assign size");
    }
    {
        std::mt19937_64 rng(3);
        for (int n : {1, 2, 7, 32}) {
            std::vector<ll> a(n, 0);
            fenwick_range_add_point_get<ll> fw(n);
            for (int q = 0; q < 5000; q++) {
                int op = int(rng() % 2);
                if (op == 0) {
                    int l = int(rng() % (n + 1));
                    int r = int(rng() % (n + 1));
                    if (l > r) std::swap(l, r);
                    ll x = ll(rng() % 201) - 100;
                    fw.add(l, r, x);
                    for (int i = l; i < r; i++) a[i] += x;
                } else {
                    int p = int(rng() % n);
                    require_true(fw.get(p) == a[p], "range_add_point_get random get");
                }
            }
            for (int i = 0; i < n; i++) require_true(fw.get(i) == a[i], "range_add_point_get final get");
        }
    }
}

void test_fenwick_range_add_range_sum() {
    {
        fenwick_range_add_range_sum<ll> fw;
        require_true(fw.size() == 0, "range_add_range_sum default size");
        require_true(fw.empty(), "range_add_range_sum default empty");
    }
    {
        int n = 6;
        std::vector<ll> a(n, 0);
        fenwick_range_add_range_sum<ll> fw(n);
        auto check = [&]() {
            for (int r = 0; r <= n; r++) {
                require_true(fw.prefix_sum(r) == naive_sum_1d(a, 0, r), "range_add_range_sum fixed prefix");
            }
            for (int l = 0; l <= n; l++) {
                for (int r = l; r <= n; r++) {
                    require_true(fw.sum(l, r) == naive_sum_1d(a, l, r), "range_add_range_sum fixed sum");
                }
            }
            for (int i = 0; i < n; i++) require_true(fw.get(i) == a[i], "range_add_range_sum fixed get");
        };
        fw.add(0, 6, 2);
        for (int i = 0; i < n; i++) a[i] += 2;
        fw.add(1, 5, -4);
        for (int i = 1; i < 5; i++) a[i] -= 4;
        fw.add(3, 3, 100);
        check();
        fw.clear();
        std::fill(a.begin(), a.end(), 0);
        check();
        fw.assign(4);
        require_true(fw.size() == 4, "range_add_range_sum assign size");
    }
    {
        std::mt19937_64 rng(4);
        for (int n : {1, 2, 8, 35}) {
            std::vector<ll> a(n, 0);
            fenwick_range_add_range_sum<ll> fw(n);
            for (int q = 0; q < 5000; q++) {
                int op = int(rng() % 4);
                if (op == 0) {
                    int l = int(rng() % (n + 1));
                    int r = int(rng() % (n + 1));
                    if (l > r) std::swap(l, r);
                    ll x = ll(rng() % 201) - 100;
                    fw.add(l, r, x);
                    for (int i = l; i < r; i++) a[i] += x;
                } else if (op == 1) {
                    int r = int(rng() % (n + 1));
                    require_true(fw.prefix_sum(r) == naive_sum_1d(a, 0, r), "range_add_range_sum random prefix");
                } else if (op == 2) {
                    int l = int(rng() % (n + 1));
                    int r = int(rng() % (n + 1));
                    if (l > r) std::swap(l, r);
                    require_true(fw.sum(l, r) == naive_sum_1d(a, l, r), "range_add_range_sum random sum");
                } else {
                    int p = int(rng() % n);
                    require_true(fw.get(p) == a[p], "range_add_range_sum random get");
                }
            }
        }
    }
}


void test_fenwick_prefix_min_max() {
    {
        fenwick_prefix_min<ll> mn(6);
        fenwick_prefix_max<ll> mx(6);
        std::vector<ll> a_min(6, std::numeric_limits<ll>::max());
        std::vector<ll> a_max(6, std::numeric_limits<ll>::lowest());
        std::mt19937_64 rng(31);
        for (int q = 0; q < 5000; q++) {
            int p = int(rng() % 6);
            ll x = ll(rng() % 2001) - 1000;
            mn.chmin(p, x);
            mx.chmax(p, x);
            a_min[p] = std::min(a_min[p], x);
            a_max[p] = std::max(a_max[p], x);
            int r = int(rng() % 7);
            ll expected_min = std::numeric_limits<ll>::max();
            ll expected_max = std::numeric_limits<ll>::lowest();
            for (int i = 0; i < r; i++) {
                expected_min = std::min(expected_min, a_min[i]);
                expected_max = std::max(expected_max, a_max[i]);
            }
            require_true(mn.prefix_min(r) == expected_min, "fenwick_prefix_min random");
            require_true(mx.prefix_max(r) == expected_max, "fenwick_prefix_max random");
        }
        mn.clear();
        mx.clear();
        require_true(mn.prefix_min(6) == std::numeric_limits<ll>::max(), "fenwick_prefix_min clear");
        require_true(mx.prefix_max(6) == std::numeric_limits<ll>::lowest(), "fenwick_prefix_max clear");
    }
}

void test_fenwick_tree_xor() {
    std::mt19937_64 rng(32);
    for (int n : {1, 2, 7, 31}) {
        fenwick_tree_xor<unsigned long long> fw(n);
        std::vector<unsigned long long> a(n, 0);
        for (int q = 0; q < 5000; q++) {
            if ((rng() & 1) == 0) {
                int p = int(rng() % n);
                unsigned long long x = rng();
                fw.apply(p, x);
                a[p] ^= x;
            } else {
                int l = int(rng() % (n + 1));
                int r = int(rng() % (n + 1));
                if (l > r) std::swap(l, r);
                unsigned long long expected = 0;
                for (int i = l; i < r; i++) expected ^= a[i];
                require_true(fw.prod(l, r) == expected, "fenwick_tree_xor random prod");
            }
        }
        fw.clear();
        require_true(fw.prod(0, n) == 0, "fenwick_tree_xor clear");
    }
}

void test_fenwick_ordered_multiset() {
    {
        fenwick_ordered_multiset ms;
        require_true(ms.value_size() == 0, "ordered_multiset default value_size");
        require_true(ms.empty(), "ordered_multiset default empty");
    }
    {
        fenwick_ordered_multiset ms(5);
        ms.add(2, 2);
        ms.add(0);
        ms.add(4, 3);
        require_true(ms.size() == 6, "ordered_multiset fixed size");
        require_true(ms.count(0) == 1, "ordered_multiset fixed count0");
        require_true(ms.count(2) == 2, "ordered_multiset fixed count2");
        require_true(ms.count(4) == 3, "ordered_multiset fixed count4");
        require_true(ms.contains(2), "ordered_multiset fixed contains true");
        require_true(!ms.contains(1), "ordered_multiset fixed contains false");
        require_true(ms.count_less(2) == 1, "ordered_multiset fixed count_less");
        require_true(ms.count_leq(2) == 3, "ordered_multiset fixed count_leq");
        require_true(ms.count_range(2, 5) == 5, "ordered_multiset fixed count_range");
        std::vector<int> expected = {0, 2, 2, 4, 4, 4};
        for (int k = 0; k < int(expected.size()); k++) require_true(ms.kth(k) == expected[k], "ordered_multiset fixed kth");
        require_true(ms.min_element() == 0, "ordered_multiset fixed min");
        require_true(ms.max_element() == 4, "ordered_multiset fixed max");
        ms.erase(4, 2);
        require_true(ms.count(4) == 1, "ordered_multiset fixed erase");
        ms.erase_one(2);
        require_true(ms.count(2) == 1, "ordered_multiset fixed erase_one");
        ms.clear();
        require_true(ms.empty(), "ordered_multiset fixed clear");
        ms.assign(3);
        require_true(ms.value_size() == 3 && ms.empty(), "ordered_multiset fixed assign");
    }
    {
        std::mt19937_64 rng(5);
        int n = 40;
        std::vector<ll> cnt(n, 0);
        fenwick_ordered_multiset ms(n);
        auto count_less_naive = [&](int x) {
            ll s = 0;
            for (int i = 0; i < x; i++) s += cnt[i];
            return s;
        };
        auto kth_naive = [&](ll k) {
            ll s = 0;
            for (int i = 0; i < n; i++) {
                s += cnt[i];
                if (s >= k + 1) return i;
            }
            return n;
        };
        for (int q = 0; q < 10000; q++) {
            int op = int(rng() % 8);
            int x = int(rng() % n);
            if (op <= 2) {
                ll c = ll(rng() % 4);
                ms.add(x, c);
                cnt[x] += c;
            } else if (op == 3) {
                if (cnt[x] > 0) {
                    ll c = 1 + ll(rng() % cnt[x]);
                    ms.erase(x, c);
                    cnt[x] -= c;
                }
            } else if (op == 4) {
                require_true(ms.count(x) == cnt[x], "ordered_multiset random count");
                require_true(ms.contains(x) == (cnt[x] > 0), "ordered_multiset random contains");
            } else if (op == 5) {
                int y = int(rng() % (n + 1));
                require_true(ms.count_less(y) == count_less_naive(y), "ordered_multiset random count_less");
            } else if (op == 6) {
                int l = int(rng() % (n + 1));
                int r = int(rng() % (n + 1));
                if (l > r) std::swap(l, r);
                require_true(ms.count_range(l, r) == count_less_naive(r) - count_less_naive(l), "ordered_multiset random range");
            } else {
                ll total = std::accumulate(cnt.begin(), cnt.end(), 0LL);
                require_true(ms.size() == total, "ordered_multiset random size");
                if (total > 0) {
                    ll k = ll(rng() % total);
                    require_true(ms.kth(k) == kth_naive(k), "ordered_multiset random kth");
                    require_true(ms.min_element() == kth_naive(0), "ordered_multiset random min");
                    require_true(ms.max_element() == kth_naive(total - 1), "ordered_multiset random max");
                }
            }
        }
    }
}

ll naive_sum_2d(const std::vector<std::vector<ll>>& a, int xl, int xr, int yl, int yr) {
    ll s = 0;
    for (int i = xl; i < xr; i++) {
        for (int j = yl; j < yr; j++) s += a[i][j];
    }
    return s;
}

void test_fenwick_tree_2d() {
    {
        fenwick_tree_2d<ll> fw;
        require_true(fw.height() == 0 && fw.width() == 0, "fenwick_tree_2d default size");
        require_true(fw.empty(), "fenwick_tree_2d default empty");
    }
    {
        int h = 3, w = 4;
        std::vector<std::vector<ll>> a(h, std::vector<ll>(w, 0));
        fenwick_tree_2d<ll> fw(h, w);
        fw.add(0, 0, 5); a[0][0] += 5;
        fw.add(2, 3, -2); a[2][3] -= 2;
        fw.add(1, 2, 7); a[1][2] += 7;
        for (int xl = 0; xl <= h; xl++) {
            for (int xr = xl; xr <= h; xr++) {
                for (int yl = 0; yl <= w; yl++) {
                    for (int yr = yl; yr <= w; yr++) {
                        require_true(fw.sum(xl, xr, yl, yr) == naive_sum_2d(a, xl, xr, yl, yr), "fenwick_tree_2d fixed sum");
                    }
                }
            }
        }
        fw.clear();
        for (auto& row : a) std::fill(row.begin(), row.end(), 0);
        require_true(fw.sum(0, h, 0, w) == 0, "fenwick_tree_2d clear");
        fw.assign(2, 5);
        require_true(fw.height() == 2 && fw.width() == 5, "fenwick_tree_2d assign");
    }
    {
        std::mt19937_64 rng(6);
        int h = 8, w = 9;
        std::vector<std::vector<ll>> a(h, std::vector<ll>(w, 0));
        fenwick_tree_2d<ll> fw(h, w);
        for (int q = 0; q < 5000; q++) {
            int op = int(rng() % 3);
            if (op == 0) {
                int x = int(rng() % h);
                int y = int(rng() % w);
                ll v = ll(rng() % 201) - 100;
                fw.add(x, y, v);
                a[x][y] += v;
            } else if (op == 1) {
                int x = int(rng() % (h + 1));
                int y = int(rng() % (w + 1));
                require_true(fw.prefix_sum(x, y) == naive_sum_2d(a, 0, x, 0, y), "fenwick_tree_2d random prefix");
            } else {
                int xl = int(rng() % (h + 1));
                int xr = int(rng() % (h + 1));
                int yl = int(rng() % (w + 1));
                int yr = int(rng() % (w + 1));
                if (xl > xr) std::swap(xl, xr);
                if (yl > yr) std::swap(yl, yr);
                require_true(fw.sum(xl, xr, yl, yr) == naive_sum_2d(a, xl, xr, yl, yr), "fenwick_tree_2d random sum");
            }
        }
    }
}


template <size_t D>
size_t test_flat_index_nd(const std::array<int, D>& p, const std::array<int, D>& sizes) {
    size_t res = 0;
    for (int i = 0; i < int(D); i++) {
        res *= size_t(sizes[size_t(i)]);
        res += size_t(p[size_t(i)]);
    }
    return res;
}

template <size_t D>
ll naive_sum_nd(const std::vector<ll>& a, const std::array<int, D>& sizes, const std::array<int, D>& l, const std::array<int, D>& r) {
    for (int i = 0; i < int(D); i++) {
        if (l[size_t(i)] == r[size_t(i)]) return 0;
    }

    ll s = 0;
    std::array<int, D> idx = l;
    while (true) {
        s += a[test_flat_index_nd(idx, sizes)];
        int k = int(D) - 1;
        while (k >= 0) {
            idx[size_t(k)]++;
            if (idx[size_t(k)] < r[size_t(k)]) break;
            idx[size_t(k)] = l[size_t(k)];
            k--;
        }
        if (k < 0) break;
    }
    return s;
}

void test_fenwick_tree_nd() {
    {
        fenwick_tree_nd<ll, 3> fw;
        require_true(fw.dim() == 3, "fenwick_tree_nd default dim");
        require_true(fw.empty(), "fenwick_tree_nd default empty");
        require_true(fw.sizes() == std::array<int, 3>{0, 0, 0}, "fenwick_tree_nd default sizes");
    }
    {
        std::array<int, 3> sizes = {3, 4, 2};
        std::vector<ll> a(size_t(3 * 4 * 2), 0);
        fenwick_tree_nd<ll, 3> fw(sizes);
        require_true(fw.dim() == 3, "fenwick_tree_nd fixed dim");
        require_true(fw.size(0) == 3 && fw.size(1) == 4 && fw.size(2) == 2, "fenwick_tree_nd fixed sizes");
        require_true(fw.sizes() == sizes, "fenwick_tree_nd fixed sizes array");

        auto add = [&](std::array<int, 3> p, ll v) {
            fw.add(p, v);
            a[test_flat_index_nd(p, sizes)] += v;
        };
        add({0, 0, 0}, 5);
        add({2, 3, 1}, -2);
        add({1, 2, 0}, 7);
        add({1, 2, 0}, -3);

        for (int x1 = 0; x1 <= sizes[0]; x1++) {
            for (int x2 = x1; x2 <= sizes[0]; x2++) {
                for (int y1 = 0; y1 <= sizes[1]; y1++) {
                    for (int y2 = y1; y2 <= sizes[1]; y2++) {
                        for (int z1 = 0; z1 <= sizes[2]; z1++) {
                            for (int z2 = z1; z2 <= sizes[2]; z2++) {
                                std::array<int, 3> l = {x1, y1, z1};
                                std::array<int, 3> r = {x2, y2, z2};
                                require_true(fw.sum(l, r) == naive_sum_nd(a, sizes, l, r), "fenwick_tree_nd fixed sum");
                            }
                        }
                    }
                }
            }
        }
        require_true(fw.prefix_sum({3, 4, 2}) == naive_sum_nd(a, sizes, std::array<int, 3>{0, 0, 0}, std::array<int, 3>{3, 4, 2}), "fenwick_tree_nd fixed prefix all");
        fw.clear();
        std::fill(a.begin(), a.end(), 0);
        require_true(fw.sum({0, 0, 0}, {3, 4, 2}) == 0, "fenwick_tree_nd clear");
        fw.assign({2, 5, 3});
        require_true(fw.size(0) == 2 && fw.size(1) == 5 && fw.size(2) == 3, "fenwick_tree_nd assign");
    }
    {
        fenwick_tree_nd<ll, 4> fw({2, 5, 3, 2});
        require_true(fw.dim() == 4 && fw.size(1) == 5, "fenwick_tree_nd 4d construct");
    }
    {
        std::mt19937_64 rng(7);
        std::array<int, 4> sizes = {3, 4, 5, 2};
        size_t total = 1;
        for (int x : sizes) total *= size_t(x);
        std::vector<ll> a(total, 0);
        fenwick_tree_nd<ll, 4> fw(sizes);
        for (int q = 0; q < 5000; q++) {
            int op = int(rng() % 3);
            if (op == 0) {
                std::array<int, 4> p{};
                for (int i = 0; i < int(p.size()); i++) p[size_t(i)] = int(rng() % sizes[size_t(i)]);
                ll v = ll(rng() % 201) - 100;
                fw.add(p, v);
                a[test_flat_index_nd(p, sizes)] += v;
            } else {
                std::array<int, 4> l{}, r{};
                for (int i = 0; i < int(l.size()); i++) {
                    int x = int(rng() % (sizes[size_t(i)] + 1));
                    int y = int(rng() % (sizes[size_t(i)] + 1));
                    if (x > y) std::swap(x, y);
                    l[size_t(i)] = x;
                    r[size_t(i)] = y;
                }
                if (op == 1) {
                    std::array<int, 4> zero{};
                    require_true(fw.prefix_sum(r) == naive_sum_nd(a, sizes, zero, r), "fenwick_tree_nd random prefix");
                } else {
                    require_true(fw.sum(l, r) == naive_sum_nd(a, sizes, l, r), "fenwick_tree_nd random sum");
                }
            }
        }
    }
}

template <class F>
double measure_ms(F&& f) {
    auto start = std::chrono::steady_clock::now();
    ll acc = f();
    auto finish = std::chrono::steady_clock::now();
    benchmark_sink += acc;
    return std::chrono::duration<double, std::milli>(finish - start).count();
}

struct bench_row_segtree_2d {
    int h;
    int w;
    int size;
    std::vector<atcoder::segtree<ll, seg_sum_op, seg_sum_e>> seg;

    bench_row_segtree_2d(int h_, int w_) : h(h_), w(w_), size(1) {
        while (size < h) size <<= 1;
        seg.reserve(size_t(2 * size));
        for (int i = 0; i < 2 * size; i++) seg.emplace_back(w);
    }

    void add(int x, int y, ll v) {
        int p = x + size;
        seg[p].set(y, seg[p].get(y) + v);
        p >>= 1;
        while (p > 0) {
            seg[p].set(y, seg[p << 1].get(y) + seg[p << 1 | 1].get(y));
            p >>= 1;
        }
    }

    ll sum(int xl, int xr, int yl, int yr) {
        ll res = 0;
        xl += size;
        xr += size;
        while (xl < xr) {
            if (xl & 1) res += seg[xl++].prod(yl, yr);
            if (xr & 1) res += seg[--xr].prod(yl, yr);
            xl >>= 1;
            xr >>= 1;
        }
        return res;
    }
};


struct bench_row_segtree_3d {
    int x_size;
    int y_size;
    int z_size;
    int size;
    std::vector<bench_row_segtree_2d> seg;

    bench_row_segtree_3d(int x_, int y_, int z_) : x_size(x_), y_size(y_), z_size(z_), size(1) {
        while (size < x_size) size <<= 1;
        seg.reserve(size_t(2 * size));
        for (int i = 0; i < 2 * size; i++) seg.emplace_back(y_size, z_size);
    }

    void add(int x, int y, int z, ll v) {
        int p = x + size;
        while (p > 0) {
            seg[p].add(y, z, v);
            p >>= 1;
        }
    }

    ll sum(int xl, int xr, int yl, int yr, int zl, int zr) {
        ll res = 0;
        xl += size;
        xr += size;
        while (xl < xr) {
            if (xl & 1) res += seg[xl++].sum(yl, yr, zl, zr);
            if (xr & 1) res += seg[--xr].sum(yl, yr, zl, zr);
            xl >>= 1;
            xr >>= 1;
        }
        return res;
    }
};

void print_bench_header(const std::string& compared_name) {
    std::cout << std::left << std::setw(34) << "method"
              << std::right << std::setw(16) << "fenwick(ns/op)"
              << std::setw(16) << compared_name + "(ns/op)"
              << std::setw(10) << "seg/fw" << '\n';
}

void print_bench_row(const std::string& name, double fw_ms, double seg_ms, int operation_count) {
    assert(0 < operation_count);
    const double fw_ns = fw_ms * 1000000.0 / double(operation_count);
    const double seg_ns = seg_ms * 1000000.0 / double(operation_count);
    std::cout << std::left << std::setw(34) << name
              << std::right << std::setw(16) << std::fixed << std::setprecision(2) << fw_ns
              << std::setw(16) << seg_ns
              << std::setw(10) << std::setprecision(2) << (seg_ns / fw_ns) << "x\n";
}

std::vector<lazy_sum_node> make_lazy_init(int n) {
    std::vector<lazy_sum_node> v(n);
    for (int i = 0; i < n; i++) v[i] = {0, 1};
    return v;
}


void benchmark_prefix_min_max_and_xor() {
    const int n = 1 << 20;
    const int q = 1 << 20;
    std::mt19937_64 rng(33);
    std::vector<int> p(q), r(q), l(q);
    std::vector<ll> x(q);
    std::vector<unsigned long long> ux(q);
    for (int i = 0; i < q; i++) {
        p[i] = int(rng() % n);
        r[i] = int(rng() % (n + 1));
        l[i] = int(rng() % (n + 1));
        if (l[i] > r[i]) std::swap(l[i], r[i]);
        x[i] = ll(rng() % 1000000001ULL) - 500000000LL;
        ux[i] = rng();
    }

    std::cout << "\n[fenwick_prefix_min vs ACL segtree]\n";
    print_bench_header("segtree");
    {
        fenwick_prefix_min<ll> fw(n);
        atcoder::segtree<ll, seg_min_op, seg_min_e> seg(n);
        double fw_ms = measure_ms([&]() {
            for (int i = 0; i < q; i++) fw.chmin(p[i], x[i]);
            return fw.prefix_min(n);
        });
        double seg_ms = measure_ms([&]() {
            for (int i = 0; i < q; i++) {
                ll old = seg.get(p[i]);
                if (x[i] < old) seg.set(p[i], x[i]);
            }
            return seg.all_prod();
        });
        print_bench_row("chmin", fw_ms, seg_ms, q);
        fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc ^= fw.prefix_min(r[i]);
            return acc;
        });
        seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc ^= seg.prod(0, r[i]);
            return acc;
        });
        print_bench_row("prefix_min", fw_ms, seg_ms, q);
    }

    std::cout << "\n[fenwick_prefix_max vs ACL segtree]\n";
    print_bench_header("segtree");
    {
        fenwick_prefix_max<ll> fw(n);
        atcoder::segtree<ll, seg_max_op, seg_max_e> seg(n);
        double fw_ms = measure_ms([&]() {
            for (int i = 0; i < q; i++) fw.chmax(p[i], x[i]);
            return fw.prefix_max(n);
        });
        double seg_ms = measure_ms([&]() {
            for (int i = 0; i < q; i++) {
                ll old = seg.get(p[i]);
                if (old < x[i]) seg.set(p[i], x[i]);
            }
            return seg.all_prod();
        });
        print_bench_row("chmax", fw_ms, seg_ms, q);
        fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc ^= fw.prefix_max(r[i]);
            return acc;
        });
        seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc ^= seg.prod(0, r[i]);
            return acc;
        });
        print_bench_row("prefix_max", fw_ms, seg_ms, q);
    }

    std::cout << "\n[fenwick_tree_xor vs ACL segtree]\n";
    print_bench_header("segtree");
    {
        fenwick_tree_xor<unsigned long long> fw(n);
        atcoder::segtree<unsigned long long, seg_xor_op, seg_xor_e> seg(n);
        double fw_ms = measure_ms([&]() {
            for (int i = 0; i < q; i++) fw.apply(p[i], ux[i]);
            return ll(fw.prefix_xor(n));
        });
        double seg_ms = measure_ms([&]() {
            for (int i = 0; i < q; i++) seg.set(p[i], seg.get(p[i]) ^ ux[i]);
            return ll(seg.all_prod());
        });
        print_bench_row("apply", fw_ms, seg_ms, q);
        fw_ms = measure_ms([&]() {
            unsigned long long acc = 0;
            for (int i = 0; i < q; i++) acc ^= fw.prod(l[i], r[i]);
            return ll(acc);
        });
        seg_ms = measure_ms([&]() {
            unsigned long long acc = 0;
            for (int i = 0; i < q; i++) acc ^= seg.prod(l[i], r[i]);
            return ll(acc);
        });
        print_bench_row("prod", fw_ms, seg_ms, q);
    }
}

void benchmark_range_add_point_get() {
    const int n = 1 << 20;
    const int q = 200000;
    std::mt19937_64 rng(12);
    std::vector<int> p(q), l(q), r(q);
    std::vector<ll> x(q);
    for (int i = 0; i < q; i++) {
        p[i] = int(rng() % n);
        int a = int(rng() % (n + 1));
        int b = int(rng() % (n + 1));
        if (a > b) std::swap(a, b);
        l[i] = a;
        r[i] = b;
        x[i] = ll(rng() % 11) - 5;
    }

    std::cout << "\n[fenwick_range_add_point_get vs ACL lazy_segtree]\n";
    print_bench_header("lazyseg");

    {
        double fw_ms;
        {
            fenwick_range_add_point_get<ll> fw(n);
            fw_ms = measure_ms([&]() {
                for (int i = 0; i < q; i++) fw.add(l[i], r[i], x[i]);
                return fw.get(p[0]);
            });
        }
        double seg_ms;
        {
            auto init = make_lazy_init(n);
            atcoder::lazy_segtree<lazy_sum_node, lazy_sum_op, lazy_sum_e, ll, lazy_sum_mapping, lazy_sum_composition, lazy_sum_id> seg(init);
            seg_ms = measure_ms([&]() {
                for (int i = 0; i < q; i++) seg.apply(l[i], r[i], x[i]);
                return seg.get(p[0]).sum;
            });
        }
        print_bench_row("add(l, r, x)", fw_ms, seg_ms, q);
    }
    {
        fenwick_range_add_point_get<ll> fw(n);
        auto init = make_lazy_init(n);
        atcoder::lazy_segtree<lazy_sum_node, lazy_sum_op, lazy_sum_e, ll, lazy_sum_mapping, lazy_sum_composition, lazy_sum_id> seg(init);
        for (int i = 0; i < q / 5; i++) {
            fw.add(l[i], r[i], x[i]);
            seg.apply(l[i], r[i], x[i]);
        }
        double fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += fw.get(p[i]);
            return acc;
        });
        double seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += seg.get(p[i]).sum;
            return acc;
        });
        print_bench_row("get", fw_ms, seg_ms, q);
    }
}

void benchmark_range_add_range_sum() {
    const int n = 1 << 20;
    const int q = 200000;
    std::mt19937_64 rng(13);
    std::vector<int> p(q), l(q), r(q);
    std::vector<ll> x(q);
    for (int i = 0; i < q; i++) {
        p[i] = int(rng() % n);
        int a = int(rng() % (n + 1));
        int b = int(rng() % (n + 1));
        if (a > b) std::swap(a, b);
        l[i] = a;
        r[i] = b;
        x[i] = ll(rng() % 11) - 5;
    }

    std::cout << "\n[fenwick_range_add_range_sum vs ACL lazy_segtree]\n";
    print_bench_header("lazyseg");

    {
        double fw_ms;
        {
            fenwick_range_add_range_sum<ll> fw(n);
            fw_ms = measure_ms([&]() {
                for (int i = 0; i < q; i++) fw.add(l[i], r[i], x[i]);
                return fw.sum(0, n);
            });
        }
        double seg_ms;
        {
            auto init = make_lazy_init(n);
            atcoder::lazy_segtree<lazy_sum_node, lazy_sum_op, lazy_sum_e, ll, lazy_sum_mapping, lazy_sum_composition, lazy_sum_id> seg(init);
            seg_ms = measure_ms([&]() {
                for (int i = 0; i < q; i++) seg.apply(l[i], r[i], x[i]);
                return seg.all_prod().sum;
            });
        }
        print_bench_row("add(l, r, x)", fw_ms, seg_ms, q);
    }

    fenwick_range_add_range_sum<ll> fw(n);
    atcoder::lazy_segtree<lazy_sum_node, lazy_sum_op, lazy_sum_e, ll, lazy_sum_mapping, lazy_sum_composition, lazy_sum_id> seg(make_lazy_init(n));
    for (int i = 0; i < q / 5; i++) {
        fw.add(l[i], r[i], x[i]);
        seg.apply(l[i], r[i], x[i]);
    }
    {
        double fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += fw.prefix_sum(r[i]);
            return acc;
        });
        double seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += seg.prod(0, r[i]).sum;
            return acc;
        });
        print_bench_row("prefix_sum", fw_ms, seg_ms, q);
    }
    {
        double fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += fw.sum(l[i], r[i]);
            return acc;
        });
        double seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += seg.prod(l[i], r[i]).sum;
            return acc;
        });
        print_bench_row("sum", fw_ms, seg_ms, q);
    }
    {
        double fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += fw.get(p[i]);
            return acc;
        });
        double seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += seg.get(p[i]).sum;
            return acc;
        });
        print_bench_row("get", fw_ms, seg_ms, q);
    }
}

void benchmark_ordered_multiset() {
    const int n = 1 << 18;
    const int q = 150000;
    std::mt19937_64 rng(14);
    std::vector<int> p(q), l(q), r(q);
    std::vector<ll> k(q);
    for (int i = 0; i < q; i++) {
        p[i] = int(rng() % n);
        int a = int(rng() % (n + 1));
        int b = int(rng() % (n + 1));
        if (a > b) std::swap(a, b);
        l[i] = a;
        r[i] = b;
        k[i] = ll(rng() % n);
    }

    std::cout << "\n[fenwick_ordered_multiset vs ACL segtree]\n";
    print_bench_header("segtree");

    {
        double fw_ms;
        {
            fenwick_ordered_multiset ms(n);
            fw_ms = measure_ms([&]() {
                for (int i = 0; i < q; i++) ms.add(p[i]);
                return ms.size();
            });
        }
        double seg_ms;
        {
            atcoder::segtree<ll, seg_sum_op, seg_sum_e> seg(n);
            seg_ms = measure_ms([&]() {
                for (int i = 0; i < q; i++) seg.set(p[i], seg.get(p[i]) + 1);
                return seg.all_prod();
            });
        }
        print_bench_row("add", fw_ms, seg_ms, q);
    }
    {
        double fw_ms;
        {
            fenwick_ordered_multiset ms(n);
            for (int i = 0; i < n; i++) ms.add(i, 1);
            fw_ms = measure_ms([&]() {
                for (int i = 0; i < q; i++) ms.erase(i);
                return ms.size();
            });
        }
        double seg_ms;
        {
            std::vector<ll> erase_init(n, 1);
            atcoder::segtree<ll, seg_sum_op, seg_sum_e> erase_seg(erase_init);
            seg_ms = measure_ms([&]() {
                for (int i = 0; i < q; i++) erase_seg.set(i, erase_seg.get(i) - 1);
                return erase_seg.all_prod();
            });
        }
        print_bench_row("erase", fw_ms, seg_ms, q);
    }

    fenwick_ordered_multiset ms(n);
    std::vector<ll> init(n, 1);
    for (int i = 0; i < n; i++) ms.add(i);
    atcoder::segtree<ll, seg_sum_op, seg_sum_e> seg(init);
    {
        double fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += ms.count(p[i]);
            return acc;
        });
        double seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += seg.get(p[i]);
            return acc;
        });
        print_bench_row("count", fw_ms, seg_ms, q);
    }
    {
        double fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += ms.contains(p[i]);
            return acc;
        });
        double seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += (seg.get(p[i]) > 0);
            return acc;
        });
        print_bench_row("contains", fw_ms, seg_ms, q);
    }
    {
        double fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += ms.count_less(p[i]);
            return acc;
        });
        double seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += seg.prod(0, p[i]);
            return acc;
        });
        print_bench_row("count_less", fw_ms, seg_ms, q);
    }
    {
        double fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += ms.count_range(l[i], r[i]);
            return acc;
        });
        double seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += seg.prod(l[i], r[i]);
            return acc;
        });
        print_bench_row("count_range", fw_ms, seg_ms, q);
    }
    {
        double fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += ms.kth(k[i]);
            return acc;
        });
        double seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) {
                ll target = k[i] + 1;
                acc += seg.max_right(0, [&](ll s) { return s < target; });
            }
            return acc;
        });
        print_bench_row("kth", fw_ms, seg_ms, q);
    }
    {
        double fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += ms.min_element();
            return acc;
        });
        double seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += seg.max_right(0, [](ll s) { return s < 1; });
            return acc;
        });
        print_bench_row("min_element", fw_ms, seg_ms, q);
    }
    {
        double fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += ms.max_element();
            return acc;
        });
        double seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) {
                ll target = seg.all_prod();
                acc += seg.max_right(0, [&](ll s) { return s < target; });
            }
            return acc;
        });
        print_bench_row("max_element", fw_ms, seg_ms, q);
    }
}

void benchmark_fenwick_tree_2d() {
    const int h = 512;
    const int w = 512;
    const int q = 80000;
    std::mt19937_64 rng(15);
    std::vector<int> x(q), y(q), xl(q), xr(q), yl(q), yr(q);
    std::vector<ll> v(q);
    for (int i = 0; i < q; i++) {
        x[i] = int(rng() % h);
        y[i] = int(rng() % w);
        int a = int(rng() % (h + 1));
        int b = int(rng() % (h + 1));
        if (a > b) std::swap(a, b);
        xl[i] = a;
        xr[i] = b;
        a = int(rng() % (w + 1));
        b = int(rng() % (w + 1));
        if (a > b) std::swap(a, b);
        yl[i] = a;
        yr[i] = b;
        v[i] = ll(rng() % 11) - 5;
    }

    std::cout << "\n[fenwick_tree_2d vs ACL segtree based 2D segtree]\n";
    print_bench_header("segtree");

    {
        double fw_ms;
        {
            fenwick_tree_2d<ll> fw(h, w);
            fw_ms = measure_ms([&]() {
                for (int i = 0; i < q; i++) fw.add(x[i], y[i], v[i]);
                return fw.sum(0, h, 0, w);
            });
        }
        double seg_ms;
        {
            bench_row_segtree_2d seg(h, w);
            seg_ms = measure_ms([&]() {
                for (int i = 0; i < q; i++) seg.add(x[i], y[i], v[i]);
                return seg.sum(0, h, 0, w);
            });
        }
        print_bench_row("add", fw_ms, seg_ms, q);
    }

    fenwick_tree_2d<ll> fw(h, w);
    bench_row_segtree_2d seg(h, w);
    for (int i = 0; i < q / 5; i++) {
        fw.add(x[i], y[i], v[i]);
        seg.add(x[i], y[i], v[i]);
    }
    {
        double fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += fw.prefix_sum(xr[i], yr[i]);
            return acc;
        });
        double seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += seg.sum(0, xr[i], 0, yr[i]);
            return acc;
        });
        print_bench_row("prefix_sum", fw_ms, seg_ms, q);
    }
    {
        double fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += fw.sum(xl[i], xr[i], yl[i], yr[i]);
            return acc;
        });
        double seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += seg.sum(xl[i], xr[i], yl[i], yr[i]);
            return acc;
        });
        print_bench_row("sum", fw_ms, seg_ms, q);
    }
}


void benchmark_fenwick_tree_nd() {
    const int x_size = 64;
    const int y_size = 64;
    const int z_size = 64;
    const int q = 30000;
    std::mt19937_64 rng(16);
    std::vector<int> x(q), y(q), z(q), xl(q), xr(q), yl(q), yr(q), zl(q), zr(q);
    std::vector<std::array<int, 3>> point(q), left(q), right(q), zero_to_right(q);
    std::vector<ll> v(q);
    for (int i = 0; i < q; i++) {
        x[i] = int(rng() % x_size);
        y[i] = int(rng() % y_size);
        z[i] = int(rng() % z_size);
        int a = int(rng() % (x_size + 1));
        int b = int(rng() % (x_size + 1));
        if (a > b) std::swap(a, b);
        xl[i] = a;
        xr[i] = b;
        a = int(rng() % (y_size + 1));
        b = int(rng() % (y_size + 1));
        if (a > b) std::swap(a, b);
        yl[i] = a;
        yr[i] = b;
        a = int(rng() % (z_size + 1));
        b = int(rng() % (z_size + 1));
        if (a > b) std::swap(a, b);
        zl[i] = a;
        zr[i] = b;
        point[i] = {x[i], y[i], z[i]};
        left[i] = {xl[i], yl[i], zl[i]};
        right[i] = {xr[i], yr[i], zr[i]};
        zero_to_right[i] = {xr[i], yr[i], zr[i]};
        v[i] = ll(rng() % 11) - 5;
    }

    std::cout << "\n[fenwick_tree_nd<3> vs ACL segtree based 3D segtree]\n";
    print_bench_header("segtree");

    {
        double fw_ms;
        {
            fenwick_tree_nd<ll, 3> fw({x_size, y_size, z_size});
            fw_ms = measure_ms([&]() {
                for (int i = 0; i < q; i++) fw.add(point[i], v[i]);
                return fw.sum({0, 0, 0}, {x_size, y_size, z_size});
            });
        }
        double seg_ms;
        {
            bench_row_segtree_3d seg(x_size, y_size, z_size);
            seg_ms = measure_ms([&]() {
                for (int i = 0; i < q; i++) seg.add(x[i], y[i], z[i], v[i]);
                return seg.sum(0, x_size, 0, y_size, 0, z_size);
            });
        }
        print_bench_row("add", fw_ms, seg_ms, q);
    }

    fenwick_tree_nd<ll, 3> fw({x_size, y_size, z_size});
    bench_row_segtree_3d seg(x_size, y_size, z_size);
    for (int i = 0; i < q / 5; i++) {
        fw.add(point[i], v[i]);
        seg.add(x[i], y[i], z[i], v[i]);
    }
    {
        double fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += fw.prefix_sum(zero_to_right[i]);
            return acc;
        });
        double seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += seg.sum(0, xr[i], 0, yr[i], 0, zr[i]);
            return acc;
        });
        print_bench_row("prefix_sum", fw_ms, seg_ms, q);
    }
    {
        double fw_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += fw.sum(left[i], right[i]);
            return acc;
        });
        double seg_ms = measure_ms([&]() {
            ll acc = 0;
            for (int i = 0; i < q; i++) acc += seg.sum(xl[i], xr[i], yl[i], yr[i], zl[i], zr[i]);
            return acc;
        });
        print_bench_row("sum", fw_ms, seg_ms, q);
    }
}

void run_benchmarks() {
    std::cout << "\nbenchmark unit: ns/op, smaller is faster\n";
    benchmark_prefix_min_max_and_xor();
    benchmark_range_add_point_get();
    benchmark_range_add_range_sum();
    benchmark_ordered_multiset();
    benchmark_fenwick_tree_2d();
    benchmark_fenwick_tree_nd();
    std::cout << "\nbenchmark_sink=" << benchmark_sink << '\n';
}

int main() {
    test_fenwick_prefix_min_max();
    test_fenwick_tree_xor();
    test_fenwick_range_add_point_get();
    test_fenwick_range_add_range_sum();
    test_fenwick_ordered_multiset();
    test_fenwick_tree_2d();
    test_fenwick_tree_nd();
    std::cout << "all correctness tests passed\n";
    run_benchmarks();
    return 0;
}
#endif

// 実行結果(atcoder)
// all correctness tests passed

// benchmark unit: ns/op, smaller is faster

// [fenwick_prefix_min vs ACL segtree]
// method                              fenwick(ns/op)  segtree(ns/op)    seg/fw
// chmin                                        37.58          112.84      3.00x
// prefix_min                                   25.23           90.61      3.59x

// [fenwick_prefix_max vs ACL segtree]
// method                              fenwick(ns/op)  segtree(ns/op)    seg/fw
// chmax                                        36.86          112.21      3.04x
// prefix_max                                   22.83           90.18      3.95x

// [fenwick_tree_xor vs ACL segtree]
// method                              fenwick(ns/op)  segtree(ns/op)    seg/fw
// apply                                        38.10          127.69      3.35x
// prod                                         51.17          136.39      2.67x

// [fenwick_range_add_point_get vs ACL lazy_segtree]
// method                              fenwick(ns/op)  lazyseg(ns/op)    seg/fw
// add(l, r, x)                                 69.01          514.58      7.46x
// get                                          24.94          164.00      6.57x

// [fenwick_range_add_range_sum vs ACL lazy_segtree]
// method                              fenwick(ns/op)  lazyseg(ns/op)    seg/fw
// add(l, r, x)                                159.24          509.41      3.20x
// prefix_sum                                   56.94          222.09      3.90x
// sum                                         114.44          394.01      3.44x
// get                                          27.53          180.92      6.57x

// [fenwick_ordered_multiset vs ACL segtree]
// method                              fenwick(ns/op)  segtree(ns/op)    seg/fw
// add                                          27.96          114.15      4.08x
// erase                                         7.63          112.43     14.73x
// count                                         1.61            2.18      1.36x
// contains                                      2.12            2.43      1.15x
// count_less                                   15.77           80.56      5.11x
// count_range                                  29.17          126.76      4.34x
// kth                                          86.89           88.39      1.02x
// min_element                                  42.81           45.40      1.06x
// max_element                                  34.74           45.47      1.31x

// [fenwick_tree_2d vs ACL segtree based 2D segtree]
// method                              fenwick(ns/op)  segtree(ns/op)    seg/fw
// add                                          48.07          556.72     11.58x
// prefix_sum                                   31.87          221.57      6.95x
// sum                                         107.61          427.18      3.97x

// [fenwick_tree_nd<3> vs ACL segtree based 3D segtree]
// method                              fenwick(ns/op)  segtree(ns/op)    seg/fw
// add                                         342.62         2049.81      5.98x
// prefix_sum                                  146.15          347.41      2.38x
// sum                                         935.14          716.38      0.77x

// benchmark_sink=-4944985218527331972
