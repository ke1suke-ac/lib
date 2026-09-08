#pragma once
#include <bits/stdc++.h>
using namespace std;

/*
累積和とimosのヘッダオンリーライブラリ
1次元、2次元、3次元、コンパイル時固定N次元に対応する
範囲はすべて半開区間で扱い、CumSumは点加算後のbuildに対応し、N次元版の座標は array<int, D> で受け取る
*/

// 1次元配列に対して、点加算後に半開区間 [l, r) の総和を O(1) で求める累積和
// サイズ指定して add で値を入れてから build する使い方と、配列から直接 build する使い方に対応する
template<class T>
struct CumSum1D {
    int n = 0;
    bool built = true;
    vector<T> s;

    // 空の累積和を作る / O(1)
    CumSum1D() : s(1, T{}) {}

    // 長さ n の未構築累積和を作る / O(n)
    explicit CumSum1D(int n_) {
        init(n_);
    }

    // 配列 a から累積和を作る / O(n)
    explicit CumSum1D(const vector<T>& a) {
        build(a);
    }

    // 長さ n の未構築累積和を作り直す / O(n)
    void init(int n_) {
        assert(n_ >= 0);
        n = n_;
        built = false;
        s.assign(n + 1, T{});
    }

    // 位置 i に value を加算する / O(1)
    void add(int i, T value) {
        assert(!built);
        assert(0 <= i && i < n);
        s[i + 1] += value;
    }

    // add 済みの値から累積和を構築する / O(n)
    void build() {
        assert(!built);
        for (int i = 0; i < n; i++) {
            s[i + 1] += s[i];
        }
        built = true;
    }

    // 配列 a から累積和を作り直す / O(n)
    void build(const vector<T>& a) {
        init(static_cast<int>(a.size()));
        copy(a.begin(), a.end(), s.begin() + 1);
        build();
    }

    // 半開区間 [l, r) の総和を返す / O(1)
    T sum(int l, int r) const {
        assert(built);
        assert(0 <= l && l <= r && r <= n);
        return s[r] - s[l];
    }

    // 元配列の長さを返す / O(1)
    int size() const {
        return n;
    }
};

// 2次元グリッドに対して、点加算後に半開長方形 [x1, x2) × [y1, y2) の総和を O(1) で求める累積和
// サイズ指定して add で値を入れてから build する使い方と、グリッドから直接 build する使い方に対応する
template<class T>
struct CumSum2D {
    int h = 0;
    int w = 0;
    bool built = true;
    vector<T> s;

    // 空の累積和を作る / O(1)
    CumSum2D() : s(1, T{}) {}

    // H×W の未構築累積和を作る / O(HW)
    CumSum2D(int h_, int w_) {
        init(h_, w_);
    }

    // グリッド a から累積和を作る / O(HW)
    explicit CumSum2D(const vector<vector<T>>& a) {
        build(a);
    }

    // H×W の未構築累積和を作り直す / O(HW)
    void init(int h_, int w_) {
        assert(h_ >= 0 && w_ >= 0);
        h = h_;
        w = w_;
        built = false;
        s.assign(static_cast<size_t>(h + 1) * static_cast<size_t>(w + 1), T{});
    }

    // 位置 (x, y) に value を加算する / O(1)
    void add(int x, int y, T value) {
        assert(!built);
        assert(0 <= x && x < h);
        assert(0 <= y && y < w);
        const int ww = w + 1;
        s[(x + 1) * ww + (y + 1)] += value;
    }

    // add 済みの値から累積和を構築する / O(HW)
    void build() {
        assert(!built);

        // 行内累積と1つ上の行の累積和を合わせて、1回の走査で2次元累積和を構築する
        const int ww = w + 1;
        for (int x = 1; x <= h; x++) {
            const int row = x * ww;
            const int prev = row - ww;
            T row_sum{};
            for (int y = 1; y <= w; y++) {
                row_sum += s[row + y];
                s[row + y] = row_sum + s[prev + y];
            }
        }
        built = true;
    }

    // グリッド a から累積和を作り直す / O(HW)
    void build(const vector<vector<T>>& a) {
        // 入力が矩形であることを確認する
        const int h_ = static_cast<int>(a.size());
        const int w_ = (h_ == 0 ? 0 : static_cast<int>(a[0].size()));
        for (int i = 0; i < h_; i++) {
            assert(static_cast<int>(a[i].size()) == w_);
        }

        // 値を番兵付き配列に入れてから累積する
        init(h_, w_);
        const int ww = w + 1;
        for (int x = 0; x < h; x++) {
            const int row = (x + 1) * ww + 1;
            copy(a[x].begin(), a[x].end(), s.begin() + row);
        }
        build();
    }

    // 半開長方形 [x1, x2) × [y1, y2) の総和を返す / O(1)
    T sum(int x1, int x2, int y1, int y2) const {
        assert(built);
        assert(0 <= x1 && x1 <= x2 && x2 <= h);
        assert(0 <= y1 && y1 <= y2 && y2 <= w);
        const int ww = w + 1;
        const int row1 = x1 * ww;
        const int row2 = x2 * ww;
        return s[row2 + y2] - s[row1 + y2] - s[row2 + y1] + s[row1 + y1];
    }

    // 高さを返す / O(1)
    int height() const {
        return h;
    }

    // 幅を返す / O(1)
    int width() const {
        return w;
    }
};

// 3次元グリッドに対して、点加算後に半開直方体 [x1, x2) × [y1, y2) × [z1, z2) の総和を O(1) で求める累積和
// サイズ指定して add で値を入れてから build する使い方と、3次元グリッドから直接 build する使い方に対応する
template<class T>
struct CumSum3D {
    int h = 0;
    int w = 0;
    int d = 0;
    bool built = true;
    vector<T> s;

    // 空の累積和を作る / O(1)
    CumSum3D() : s(1, T{}) {}

    // H×W×D の未構築累積和を作る / O(HWD)
    CumSum3D(int h_, int w_, int d_) {
        init(h_, w_, d_);
    }

    // 3次元グリッド a から累積和を作る / O(HWD)
    explicit CumSum3D(const vector<vector<vector<T>>>& a) {
        build(a);
    }

    // H×W×D の未構築累積和を作り直す / O(HWD)
    void init(int h_, int w_, int d_) {
        assert(h_ >= 0 && w_ >= 0 && d_ >= 0);
        h = h_;
        w = w_;
        d = d_;
        built = false;
        s.assign(static_cast<size_t>(h + 1) * static_cast<size_t>(w + 1) * static_cast<size_t>(d + 1), T{});
    }

    // 位置 (x, y, z) に value を加算する / O(1)
    void add(int x, int y, int z, T value) {
        assert(!built);
        assert(0 <= x && x < h);
        assert(0 <= y && y < w);
        assert(0 <= z && z < d);
        const int ww = w + 1;
        const int dd = d + 1;
        s[((x + 1) * ww + (y + 1)) * dd + (z + 1)] += value;
    }

    // add 済みの値から累積和を構築する / O(HWD)
    void build() {
        assert(!built);

        // z軸、y軸、x軸の順に累積して3次元累積和を構築する
        const int ww = w + 1;
        const int dd = d + 1;
        const int plane = ww * dd;
        for (int x = 1; x <= h; x++) {
            for (int y = 1; y <= w; y++) {
                const int base = (x * ww + y) * dd;
                for (int z = 1; z <= d; z++) {
                    s[base + z] += s[base + z - 1];
                }
            }
        }
        for (int x = 1; x <= h; x++) {
            for (int y = 1; y <= w; y++) {
                const int base = (x * ww + y) * dd;
                const int prev = base - dd;
                for (int z = 1; z <= d; z++) {
                    s[base + z] += s[prev + z];
                }
            }
        }
        for (int x = 1; x <= h; x++) {
            const int base_x = x * plane;
            const int prev_x = base_x - plane;
            for (int y = 1; y <= w; y++) {
                const int base = base_x + y * dd;
                const int prev = prev_x + y * dd;
                for (int z = 1; z <= d; z++) {
                    s[base + z] += s[prev + z];
                }
            }
        }
        built = true;
    }

    // 3次元グリッド a から累積和を作り直す / O(HWD)
    void build(const vector<vector<vector<T>>>& a) {
        // 入力が直方体であることを確認する
        const int h_ = static_cast<int>(a.size());
        const int w_ = (h_ == 0 ? 0 : static_cast<int>(a[0].size()));
        const int d_ = (h_ == 0 || w_ == 0 ? 0 : static_cast<int>(a[0][0].size()));
        for (int x = 0; x < h_; x++) {
            assert(static_cast<int>(a[x].size()) == w_);
            for (int y = 0; y < w_; y++) {
                assert(static_cast<int>(a[x][y].size()) == d_);
            }
        }

        // 値を番兵付き配列に入れてから累積する
        init(h_, w_, d_);
        const int ww = w + 1;
        const int dd = d + 1;
        for (int x = 0; x < h; x++) {
            for (int y = 0; y < w; y++) {
                const int base = ((x + 1) * ww + (y + 1)) * dd + 1;
                copy(a[x][y].begin(), a[x][y].end(), s.begin() + base);
            }
        }
        build();
    }

    // 半開直方体 [x1, x2) × [y1, y2) × [z1, z2) の総和を返す / O(1)
    T sum(int x1, int x2, int y1, int y2, int z1, int z2) const {
        assert(built);
        assert(0 <= x1 && x1 <= x2 && x2 <= h);
        assert(0 <= y1 && y1 <= y2 && y2 <= w);
        assert(0 <= z1 && z1 <= z2 && z2 <= d);

        // 8個の角を包除原理で合成する
        const int ww = w + 1;
        const int dd = d + 1;
        const int x1_base = x1 * ww * dd;
        const int x2_base = x2 * ww * dd;
        const int y1_base = y1 * dd;
        const int y2_base = y2 * dd;
        return s[x2_base + y2_base + z2]
            - s[x1_base + y2_base + z2]
            - s[x2_base + y1_base + z2]
            - s[x2_base + y2_base + z1]
            + s[x1_base + y1_base + z2]
            + s[x1_base + y2_base + z1]
            + s[x2_base + y1_base + z1]
            - s[x1_base + y1_base + z1];
    }

    // 高さを返す / O(1)
    int height() const {
        return h;
    }

    // 幅を返す / O(1)
    int width() const {
        return w;
    }

    // 奥行きを返す / O(1)
    int depth() const {
        return d;
    }
};

// コンパイル時固定N次元配列に対して、点加算後に半開超直方体の総和を O(2^D) で求める累積和
// shape を指定して add で値を入れてから build する使い方と、flatten配列から直接 build する使い方に対応する
template<class T, int D>
struct CumSumND {
    static_assert(D >= 1, "D must be positive");
    static_assert(D < static_cast<int>(sizeof(size_t) * CHAR_BIT), "D is too large");
    static constexpr size_t corner_count = size_t{1} << D;

    array<int, D> shape{};
    array<size_t, D> stride{};
    bool built = true;
    vector<T> s;

    // 空の累積和を作る / O(D)
    CumSumND() {
        make_stride();
        s.assign(1, T{});
    }

    // shape の未構築累積和を作る / O(DΠ(shape[i] + 1))
    explicit CumSumND(const array<int, D>& shape_) {
        init(shape_);
    }

    // shape とrow-major順の配列 a から累積和を作る / O(DΠ(shape[i] + 1))
    CumSumND(const array<int, D>& shape_, const vector<T>& a) {
        build(shape_, a);
    }

    // shape の未構築累積和を作り直す / O(DΠ(shape[i] + 1))
    void init(const array<int, D>& shape_) {
        shape = shape_;
        make_stride();
        built = false;
        s.assign(internal_count(), T{});
    }

    // 位置 coord に value を加算する / O(D)
    void add(const array<int, D>& coord, T value) {
        assert(!built);
        size_t index = 0;
        for (int i = 0; i < D; i++) {
            assert(0 <= coord[i] && coord[i] < shape[i]);
            index += static_cast<size_t>(coord[i] + 1) * stride[i];
        }
        s[index] += value;
    }

    // add 済みの値から累積和を構築する / O(DΠ(shape[i] + 1))
    void build() {
        assert(!built);

        // 各軸方向に順番に累積し、下位次元の連続ブロックを内側でまとめて処理する
        for (int axis = 0; axis < D; axis++) {
            const size_t step = stride[axis];
            const size_t len = static_cast<size_t>(shape[axis] + 1);
            const size_t block = step * len;
            for (size_t base = 0; base < s.size(); base += block) {
                for (size_t pos = 1; pos < len; pos++) {
                    const size_t cur = base + pos * step;
                    const size_t prev = cur - step;
                    for (size_t offset = 0; offset < step; offset++) {
                        s[cur + offset] += s[prev + offset];
                    }
                }
            }
        }
        built = true;
    }

    // shape とrow-major順の配列 a から累積和を作り直す / O(DΠ(shape[i] + 1))
    void build(const array<int, D>& shape_, const vector<T>& a) {
        init(shape_);
        const size_t input_total = element_count();
        assert(a.size() == input_total);

        // row-major順の最下位次元の連続部分を、番兵付き配列へ行単位でコピーする
        array<int, D> coord{};
        size_t src = 0;
        const size_t last_len = static_cast<size_t>(shape[D - 1]);
        while (src < input_total) {
            size_t dst = stride[D - 1];
            for (int i = 0; i < D - 1; i++) {
                dst += static_cast<size_t>(coord[i] + 1) * stride[i];
            }
            copy_n(a.begin() + src, last_len, s.begin() + dst);
            src += last_len;
            for (int i = D - 2; i >= 0; i--) {
                coord[i]++;
                if (coord[i] < shape[i]) {
                    break;
                }
                coord[i] = 0;
            }
        }
        build();
    }

    // 半開超直方体 [low[0], high[0]) × ... × [low[D-1], high[D-1]) の総和を返す / O(2^D)
    T sum(const array<int, D>& low, const array<int, D>& high) const {
        assert(built);

        // high側の角を基準にし、low側を選ぶ次元の差分だけ添字を戻す
        size_t base = 0;
        array<size_t, D> delta{};
        for (int i = 0; i < D; i++) {
            assert(0 <= low[i] && low[i] <= high[i] && high[i] <= shape[i]);
            base += static_cast<size_t>(high[i]) * stride[i];
            delta[i] = static_cast<size_t>(high[i] - low[i]) * stride[i];
        }

        // Gray code順に角をたどり、変化した1次元の添字だけを更新する
        T res = s[base];
        size_t index = base;
        size_t prev_gray = 0;
        for (size_t t = 1; t < corner_count; t++) {
            const size_t gray = t ^ (t >> 1);
            const size_t changed = gray ^ prev_gray;
            const int bit = __builtin_ctzll(static_cast<unsigned long long>(changed));
            if ((gray & changed) != 0) {
                index -= delta[bit];
            } else {
                index += delta[bit];
            }
            if ((t & 1) == 0) {
                res += s[index];
            } else {
                res -= s[index];
            }
            prev_gray = gray;
        }
        return res;
    }

    // 各次元のサイズを返す / O(1)
    const array<int, D>& sizes() const {
        return shape;
    }

private:
    size_t element_count() const {
        size_t total = 1;
        for (int i = 0; i < D; i++) {
            assert(shape[i] >= 0);
            total *= static_cast<size_t>(shape[i]);
        }
        return total;
    }

    size_t internal_count() const {
        size_t total = 1;
        for (int i = 0; i < D; i++) {
            total *= static_cast<size_t>(shape[i] + 1);
        }
        return total;
    }

    void make_stride() {
        for (int i = 0; i < D; i++) {
            assert(shape[i] >= 0);
        }
        stride[D - 1] = 1;
        for (int i = D - 2; i >= 0; i--) {
            stride[i] = stride[i + 1] * static_cast<size_t>(shape[i + 1] + 1);
        }
    }

};

// 1次元配列に対して、半開区間 [l, r) への範囲加算をまとめて適用するimos
template<class T>
struct Imos1D {
    int n = 0;
    vector<T> diff;

    // 空のimosを作る / O(1)
    Imos1D() = default;

    // 長さ n のimosを作る / O(n)
    explicit Imos1D(int n_) {
        init(n_);
    }

    // 長さ n のimosを作り直す / O(n)
    void init(int n_) {
        assert(n_ >= 0);
        n = n_;
        diff.assign(n + 1, T{});
    }

    // 半開区間 [l, r) に value を加算する / O(1)
    void add(int l, int r, T value) {
        assert(0 <= l && l <= r && r <= n);
        if (l == r) {
            return;
        }
        diff[l] += value;
        diff[r] -= value;
    }

    // 範囲加算後の配列を返す / O(n)
    vector<T> build() const {
        vector<T> res(n);
        T cur{};
        for (int i = 0; i < n; i++) {
            cur += diff[i];
            res[i] = cur;
        }
        return res;
    }

    // 長さを返す / O(1)
    int size() const {
        return n;
    }
};

// 2次元グリッドに対して、半開長方形への範囲加算をまとめて適用するimos
template<class T>
struct Imos2D {
    int h = 0;
    int w = 0;
    vector<T> diff;

    // 空のimosを作る / O(1)
    Imos2D() = default;

    // H×W のimosを作る / O(HW)
    Imos2D(int h_, int w_) {
        init(h_, w_);
    }

    // H×W のimosを作り直す / O(HW)
    void init(int h_, int w_) {
        assert(h_ >= 0 && w_ >= 0);
        h = h_;
        w = w_;
        diff.assign(static_cast<size_t>(h + 1) * static_cast<size_t>(w + 1), T{});
    }

    // 半開長方形 [x1, x2) × [y1, y2) に value を加算する / O(1)
    void add(int x1, int x2, int y1, int y2, T value) {
        assert(0 <= x1 && x1 <= x2 && x2 <= h);
        assert(0 <= y1 && y1 <= y2 && y2 <= w);
        if (x1 == x2 || y1 == y2) {
            return;
        }
        const int wp = w + 1;
        const int row1 = x1 * wp;
        const int row2 = x2 * wp;
        diff[row1 + y1] += value;
        diff[row2 + y1] -= value;
        diff[row1 + y2] -= value;
        diff[row2 + y2] += value;
    }

    // 範囲加算後のグリッドを返す / O(HW)
    vector<vector<T>> build() const {
        vector<T> cur = diff;
        const int wp = w + 1;

        // 横方向、縦方向の順に累積して差分配列を値配列へ変換する
        for (int x = 0; x <= h; x++) {
            const int base = x * wp;
            for (int y = 1; y <= w; y++) {
                cur[base + y] += cur[base + y - 1];
            }
        }
        for (int x = 1; x <= h; x++) {
            const int cur_row = x * wp;
            const int prev_row = cur_row - wp;
            for (int y = 0; y <= w; y++) {
                cur[cur_row + y] += cur[prev_row + y];
            }
        }

        // 番兵行と番兵列を除いた H×W 部分だけを返す
        vector<vector<T>> res(h, vector<T>(w));
        for (int x = 0; x < h; x++) {
            copy_n(cur.begin() + x * wp, w, res[x].begin());
        }
        return res;
    }

    // 高さを返す / O(1)
    int height() const {
        return h;
    }

    // 幅を返す / O(1)
    int width() const {
        return w;
    }
};

// 3次元グリッドに対して、半開直方体への範囲加算をまとめて適用するimos
template<class T>
struct Imos3D {
    int h = 0;
    int w = 0;
    int d = 0;
    vector<T> diff;

    // 空のimosを作る / O(1)
    Imos3D() = default;

    // H×W×D のimosを作る / O(HWD)
    Imos3D(int h_, int w_, int d_) {
        init(h_, w_, d_);
    }

    // H×W×D のimosを作り直す / O(HWD)
    void init(int h_, int w_, int d_) {
        assert(h_ >= 0 && w_ >= 0 && d_ >= 0);
        h = h_;
        w = w_;
        d = d_;
        diff.assign(static_cast<size_t>(h + 1) * static_cast<size_t>(w + 1) * static_cast<size_t>(d + 1), T{});
    }

    // 半開直方体 [x1, x2) × [y1, y2) × [z1, z2) に value を加算する / O(1)
    void add(int x1, int x2, int y1, int y2, int z1, int z2, T value) {
        assert(0 <= x1 && x1 <= x2 && x2 <= h);
        assert(0 <= y1 && y1 <= y2 && y2 <= w);
        assert(0 <= z1 && z1 <= z2 && z2 <= d);
        if (x1 == x2 || y1 == y2 || z1 == z2) {
            return;
        }
        const int wp = w + 1;
        const int dp = d + 1;
        const int x1_base = x1 * wp * dp;
        const int x2_base = x2 * wp * dp;
        const int y1_base = y1 * dp;
        const int y2_base = y2 * dp;

        // 8個の角に符号付きで差分を入れる
        diff[x1_base + y1_base + z1] += value;
        diff[x2_base + y1_base + z1] -= value;
        diff[x1_base + y2_base + z1] -= value;
        diff[x1_base + y1_base + z2] -= value;
        diff[x2_base + y2_base + z1] += value;
        diff[x2_base + y1_base + z2] += value;
        diff[x1_base + y2_base + z2] += value;
        diff[x2_base + y2_base + z2] -= value;
    }

    // 範囲加算後の3次元グリッドを返す / O(HWD)
    vector<vector<vector<T>>> build() const {
        vector<T> cur = diff;
        const int wp = w + 1;
        const int dp = d + 1;
        const int plane = wp * dp;

        // z軸、y軸、x軸の順に累積して差分配列を値配列へ変換する
        for (int x = 0; x <= h; x++) {
            for (int y = 0; y <= w; y++) {
                const int base = (x * wp + y) * dp;
                for (int z = 1; z <= d; z++) {
                    cur[base + z] += cur[base + z - 1];
                }
            }
        }
        for (int x = 0; x <= h; x++) {
            for (int y = 1; y <= w; y++) {
                const int cur_base = (x * wp + y) * dp;
                const int prev_base = cur_base - dp;
                for (int z = 0; z <= d; z++) {
                    cur[cur_base + z] += cur[prev_base + z];
                }
            }
        }
        for (int x = 1; x <= h; x++) {
            const int cur_base = x * plane;
            const int prev_base = cur_base - plane;
            for (int yz = 0; yz < plane; yz++) {
                cur[cur_base + yz] += cur[prev_base + yz];
            }
        }

        // 番兵部分を除いた H×W×D 部分だけを返す
        vector<vector<vector<T>>> res(h, vector<vector<T>>(w, vector<T>(d)));
        for (int x = 0; x < h; x++) {
            for (int y = 0; y < w; y++) {
                copy_n(cur.begin() + (x * wp + y) * dp, d, res[x][y].begin());
            }
        }
        return res;
    }

    // 高さを返す / O(1)
    int height() const {
        return h;
    }

    // 幅を返す / O(1)
    int width() const {
        return w;
    }

    // 奥行きを返す / O(1)
    int depth() const {
        return d;
    }
};

// コンパイル時固定N次元配列に対して、半開超直方体への範囲加算をまとめて適用するimos
template<class T, int D>
struct ImosND {
    static_assert(D >= 1, "D must be positive");
    static_assert(D < static_cast<int>(sizeof(size_t) * CHAR_BIT), "D is too large");
    static constexpr size_t corner_count = size_t{1} << D;

    array<int, D> shape{};
    array<size_t, D> stride{};
    vector<T> diff;

    // 空のimosを作る / O(D)
    ImosND() {
        make_stride();
        diff.assign(1, T{});
    }

    // shape のimosを作る / O(DΠ(shape[i] + 1))
    explicit ImosND(const array<int, D>& shape_) {
        init(shape_);
    }

    // shape のimosを作り直す / O(DΠ(shape[i] + 1))
    void init(const array<int, D>& shape_) {
        shape = shape_;
        make_stride();
        diff.assign(internal_count(), T{});
    }

    // 半開超直方体 [low[0], high[0]) × ... × [low[D-1], high[D-1]) に value を加算する / O(2^D)
    void add(const array<int, D>& low, const array<int, D>& high, T value) {
        // low側の角を基準にし、high側を選ぶ次元の差分だけ添字を進める
        bool empty = false;
        size_t base = 0;
        array<size_t, D> delta{};
        for (int i = 0; i < D; i++) {
            assert(0 <= low[i] && low[i] <= high[i] && high[i] <= shape[i]);
            if (low[i] == high[i]) {
                empty = true;
            }
            base += static_cast<size_t>(low[i]) * stride[i];
            delta[i] = static_cast<size_t>(high[i] - low[i]) * stride[i];
        }
        if (empty) {
            return;
        }

        // Gray code順に角をたどり、変化した1次元の添字だけを更新する
        size_t index = base;
        diff[index] += value;
        size_t prev_gray = 0;
        for (size_t t = 1; t < corner_count; t++) {
            const size_t gray = t ^ (t >> 1);
            const size_t changed = gray ^ prev_gray;
            const int bit = __builtin_ctzll(static_cast<unsigned long long>(changed));
            if ((gray & changed) != 0) {
                index += delta[bit];
            } else {
                index -= delta[bit];
            }
            if ((t & 1) == 0) {
                diff[index] += value;
            } else {
                diff[index] -= value;
            }
            prev_gray = gray;
        }
    }

    // 範囲加算後のrow-major順flatten配列を返す / O(DΠ(shape[i]))
    vector<T> build() const {
        const size_t total = element_count();
        vector<T> res(total);
        if (total == 0) {
            return res;
        }

        // 番兵部分は出力に不要なので、元shape部分だけを行単位で取り出す
        array<int, D> coord{};
        size_t src = 0;
        size_t dst = 0;
        const size_t last_len = static_cast<size_t>(shape[D - 1]);
        const size_t rows = total / last_len;
        for (size_t row = 0; row < rows; row++) {
            copy_n(diff.begin() + src, last_len, res.begin() + dst);
            dst += last_len;

            for (int i = D - 2; i >= 0; i--) {
                coord[i]++;
                src += stride[i];
                if (coord[i] < shape[i]) {
                    break;
                }
                coord[i] = 0;
                src -= static_cast<size_t>(shape[i]) * stride[i];
            }
        }

        // 元shapeのコンパクトな配列上で各軸方向に累積して値配列へ変換する
        array<size_t, D> out_stride{};
        out_stride[D - 1] = 1;
        for (int i = D - 2; i >= 0; i--) {
            out_stride[i] = out_stride[i + 1] * static_cast<size_t>(shape[i + 1]);
        }
        for (int axis = 0; axis < D; axis++) {
            const size_t step = out_stride[axis];
            const size_t len = static_cast<size_t>(shape[axis]);
            const size_t block = step * len;
            for (size_t base = 0; base < res.size(); base += block) {
                for (size_t pos = 1; pos < len; pos++) {
                    const size_t cur_index = base + pos * step;
                    const size_t prev_index = cur_index - step;
                    for (size_t offset = 0; offset < step; offset++) {
                        res[cur_index + offset] += res[prev_index + offset];
                    }
                }
            }
        }
        return res;
    }

    // 各次元のサイズを返す / O(1)
    const array<int, D>& sizes() const {
        return shape;
    }

private:
    size_t element_count() const {
        size_t total = 1;
        for (int i = 0; i < D; i++) {
            assert(shape[i] >= 0);
            total *= static_cast<size_t>(shape[i]);
        }
        return total;
    }

    size_t internal_count() const {
        size_t total = 1;
        for (int i = 0; i < D; i++) {
            total *= static_cast<size_t>(shape[i] + 1);
        }
        return total;
    }

    void make_stride() {
        for (int i = 0; i < D; i++) {
            assert(shape[i] >= 0);
        }
        stride[D - 1] = 1;
        for (int i = D - 2; i >= 0; i--) {
            stride[i] = stride[i + 1] * static_cast<size_t>(shape[i + 1] + 1);
        }
    }

};

#if __INCLUDE_LEVEL__ == 0

long long naive_sum_1d(const vector<long long>& a, int l, int r) {
    long long res = 0;
    for (int i = l; i < r; i++) {
        res += a[i];
    }
    return res;
}

long long naive_sum_2d(const vector<vector<long long>>& a, int x1, int x2, int y1, int y2) {
    long long res = 0;
    for (int x = x1; x < x2; x++) {
        for (int y = y1; y < y2; y++) {
            res += a[x][y];
        }
    }
    return res;
}

long long naive_sum_3d(const vector<vector<vector<long long>>>& a, int x1, int x2, int y1, int y2, int z1, int z2) {
    long long res = 0;
    for (int x = x1; x < x2; x++) {
        for (int y = y1; y < y2; y++) {
            for (int z = z1; z < z2; z++) {
                res += a[x][y][z];
            }
        }
    }
    return res;
}

template<int D>
size_t flat_count(const array<int, D>& shape) {
    size_t total = 1;
    for (int i = 0; i < D; i++) {
        assert(shape[i] >= 0);
        total *= static_cast<size_t>(shape[i]);
    }
    return total;
}

template<int D>
array<int, D> flat_to_coord(size_t flat, const array<int, D>& shape) {
    array<int, D> coord{};
    for (int i = D - 1; i >= 0; i--) {
        coord[i] = static_cast<int>(flat % static_cast<size_t>(shape[i]));
        flat /= static_cast<size_t>(shape[i]);
    }
    return coord;
}

template<int D>
long long naive_sum_nd(const array<int, D>& shape, const vector<long long>& a, const array<int, D>& low, const array<int, D>& high) {
    long long res = 0;
    const size_t total = flat_count<D>(shape);
    for (size_t flat = 0; flat < total; flat++) {
        const array<int, D> coord = flat_to_coord<D>(flat, shape);
        bool inside = true;
        for (int i = 0; i < D; i++) {
            if (coord[i] < low[i] || high[i] <= coord[i]) {
                inside = false;
            }
        }
        if (inside) {
            res += a[flat];
        }
    }
    return res;
}

template<int D>
void add_naive_nd(const array<int, D>& shape, vector<long long>& a, const array<int, D>& low, const array<int, D>& high, long long value) {
    const size_t total = flat_count<D>(shape);
    for (size_t flat = 0; flat < total; flat++) {
        const array<int, D> coord = flat_to_coord<D>(flat, shape);
        bool inside = true;
        for (int i = 0; i < D; i++) {
            if (coord[i] < low[i] || high[i] <= coord[i]) {
                inside = false;
            }
        }
        if (inside) {
            a[flat] += value;
        }
    }
}

void check_cumsum_1d(const vector<long long>& a, const CumSum1D<long long>& cs) {
    const int n = static_cast<int>(a.size());
    assert(cs.size() == n);
    for (int l = 0; l <= n; l++) {
        for (int r = l; r <= n; r++) {
            assert(cs.sum(l, r) == naive_sum_1d(a, l, r));
        }
    }
}

void test_cumsum_1d() {
    vector<vector<long long>> cases = {
        {},
        {5},
        {-3},
        {1, -2, 3, -4, 5},
        {0, 0, 0, 0}
    };
    for (const auto& a : cases) {
        CumSum1D<long long> cs(a);
        check_cumsum_1d(a, cs);

        CumSum1D<long long> add_cs(static_cast<int>(a.size()));
        for (int i = 0; i < static_cast<int>(a.size()); i++) {
            add_cs.add(i, a[i]);
            add_cs.add(i, 7);
            add_cs.add(i, -7);
        }
        add_cs.build();
        check_cumsum_1d(a, add_cs);
    }

    mt19937 rng(1);
    for (int tc = 0; tc < 200; tc++) {
        const int n = static_cast<int>(rng() % 30);
        vector<long long> a(n);
        for (int i = 0; i < n; i++) {
            a[i] = static_cast<int>(rng() % 101) - 50;
        }
        CumSum1D<long long> cs(a);
        check_cumsum_1d(a, cs);

        CumSum1D<long long> add_cs;
        add_cs.init(n);
        for (int i = 0; i < n; i++) {
            const long long first = a[i] / 2;
            add_cs.add(i, first);
            add_cs.add(i, a[i] - first);
        }
        add_cs.build();
        check_cumsum_1d(a, add_cs);
    }
}

void check_cumsum_2d(const vector<vector<long long>>& a, const CumSum2D<long long>& cs) {
    const int h = static_cast<int>(a.size());
    const int w = (h == 0 ? 0 : static_cast<int>(a[0].size()));
    assert(cs.height() == h && cs.width() == w);
    for (int x1 = 0; x1 <= h; x1++) {
        for (int x2 = x1; x2 <= h; x2++) {
            for (int y1 = 0; y1 <= w; y1++) {
                for (int y2 = y1; y2 <= w; y2++) {
                    assert(cs.sum(x1, x2, y1, y2) == naive_sum_2d(a, x1, x2, y1, y2));
                }
            }
        }
    }
}

void test_cumsum_2d() {
    vector<pair<int, int>> shapes = {{0, 0}, {5, 0}, {1, 1}, {1, 6}, {6, 1}, {4, 5}};
    for (const auto& [h, w] : shapes) {
        vector<vector<long long>> a(h, vector<long long>(w));
        for (int x = 0; x < h; x++) {
            for (int y = 0; y < w; y++) {
                a[x][y] = x * 7 - y * 5 + 3;
            }
        }
        CumSum2D<long long> cs(a);
        check_cumsum_2d(a, cs);

        CumSum2D<long long> add_cs(h, w);
        for (int x = 0; x < h; x++) {
            for (int y = 0; y < w; y++) {
                add_cs.add(x, y, a[x][y]);
                add_cs.add(x, y, 11);
                add_cs.add(x, y, -11);
            }
        }
        add_cs.build();
        check_cumsum_2d(a, add_cs);
    }

    mt19937 rng(2);
    for (int tc = 0; tc < 80; tc++) {
        const int h = static_cast<int>(rng() % 7);
        const int w = (h == 0 ? 0 : static_cast<int>(rng() % 7));
        vector<vector<long long>> a(h, vector<long long>(w));
        for (int x = 0; x < h; x++) {
            for (int y = 0; y < w; y++) {
                a[x][y] = static_cast<int>(rng() % 101) - 50;
            }
        }
        CumSum2D<long long> cs(a);
        check_cumsum_2d(a, cs);

        CumSum2D<long long> add_cs;
        add_cs.init(h, w);
        for (int x = 0; x < h; x++) {
            for (int y = 0; y < w; y++) {
                const long long first = a[x][y] / 2;
                add_cs.add(x, y, first);
                add_cs.add(x, y, a[x][y] - first);
            }
        }
        add_cs.build();
        check_cumsum_2d(a, add_cs);
    }
}

void check_cumsum_3d(const vector<vector<vector<long long>>>& a, const CumSum3D<long long>& cs) {
    const int h = static_cast<int>(a.size());
    const int w = (h == 0 ? 0 : static_cast<int>(a[0].size()));
    const int d = (h == 0 || w == 0 ? 0 : static_cast<int>(a[0][0].size()));
    assert(cs.height() == h && cs.width() == w && cs.depth() == d);
    for (int x1 = 0; x1 <= h; x1++) {
        for (int x2 = x1; x2 <= h; x2++) {
            for (int y1 = 0; y1 <= w; y1++) {
                for (int y2 = y1; y2 <= w; y2++) {
                    for (int z1 = 0; z1 <= d; z1++) {
                        for (int z2 = z1; z2 <= d; z2++) {
                            assert(cs.sum(x1, x2, y1, y2, z1, z2) == naive_sum_3d(a, x1, x2, y1, y2, z1, z2));
                        }
                    }
                }
            }
        }
    }
}

void test_cumsum_3d() {
    vector<array<int, 3>> shapes = {{{0, 0, 0}}, {{1, 1, 1}}, {{1, 2, 3}}, {{3, 1, 2}}, {{2, 3, 2}}};
    for (const auto& sh : shapes) {
        const int h = sh[0];
        const int w = sh[1];
        const int d = sh[2];
        vector<vector<vector<long long>>> a(h, vector<vector<long long>>(w, vector<long long>(d)));
        for (int x = 0; x < h; x++) {
            for (int y = 0; y < w; y++) {
                for (int z = 0; z < d; z++) {
                    a[x][y][z] = x * 11 - y * 7 + z * 5 - 3;
                }
            }
        }
        CumSum3D<long long> cs(a);
        check_cumsum_3d(a, cs);

        CumSum3D<long long> add_cs(h, w, d);
        for (int x = 0; x < h; x++) {
            for (int y = 0; y < w; y++) {
                for (int z = 0; z < d; z++) {
                    add_cs.add(x, y, z, a[x][y][z]);
                    add_cs.add(x, y, z, 13);
                    add_cs.add(x, y, z, -13);
                }
            }
        }
        add_cs.build();
        check_cumsum_3d(a, add_cs);
    }

    mt19937 rng(3);
    for (int tc = 0; tc < 40; tc++) {
        const int h = static_cast<int>(rng() % 5);
        const int w = (h == 0 ? 0 : static_cast<int>(rng() % 5));
        const int d = (h == 0 || w == 0 ? 0 : static_cast<int>(rng() % 5));
        vector<vector<vector<long long>>> a(h, vector<vector<long long>>(w, vector<long long>(d)));
        for (int x = 0; x < h; x++) {
            for (int y = 0; y < w; y++) {
                for (int z = 0; z < d; z++) {
                    a[x][y][z] = static_cast<int>(rng() % 101) - 50;
                }
            }
        }
        CumSum3D<long long> cs(a);
        CumSum3D<long long> add_cs;
        add_cs.init(h, w, d);
        for (int x = 0; x < h; x++) {
            for (int y = 0; y < w; y++) {
                for (int z = 0; z < d; z++) {
                    const long long first = a[x][y][z] / 2;
                    add_cs.add(x, y, z, first);
                    add_cs.add(x, y, z, a[x][y][z] - first);
                }
            }
        }
        add_cs.build();
        for (int rep = 0; rep < 300; rep++) {
            int x1 = static_cast<int>(rng() % static_cast<unsigned>(h + 1));
            int x2 = static_cast<int>(rng() % static_cast<unsigned>(h + 1));
            int y1 = static_cast<int>(rng() % static_cast<unsigned>(w + 1));
            int y2 = static_cast<int>(rng() % static_cast<unsigned>(w + 1));
            int z1 = static_cast<int>(rng() % static_cast<unsigned>(d + 1));
            int z2 = static_cast<int>(rng() % static_cast<unsigned>(d + 1));
            if (x1 > x2) swap(x1, x2);
            if (y1 > y2) swap(y1, y2);
            if (z1 > z2) swap(z1, z2);
            const long long want = naive_sum_3d(a, x1, x2, y1, y2, z1, z2);
            assert(cs.sum(x1, x2, y1, y2, z1, z2) == want);
            assert(add_cs.sum(x1, x2, y1, y2, z1, z2) == want);
        }
    }
}

template<int D>
void test_cumsum_nd_case(const array<int, D>& shape) {
    const size_t total = flat_count<D>(shape);
    vector<long long> a(total);
    for (size_t i = 0; i < total; i++) {
        a[i] = static_cast<long long>(i % 23) - 11;
    }
    CumSumND<long long, D> cs(shape, a);
    assert(cs.sizes() == shape);

    CumSumND<long long, D> add_cs(shape);
    for (size_t flat = 0; flat < total; flat++) {
        const array<int, D> coord = flat_to_coord<D>(flat, shape);
        add_cs.add(coord, a[flat]);
        add_cs.add(coord, 17);
        add_cs.add(coord, -17);
    }
    add_cs.build();
    assert(add_cs.sizes() == shape);

    mt19937 rng(100 + D);
    for (int rep = 0; rep < 1000; rep++) {
        array<int, D> low{};
        array<int, D> high{};
        for (int i = 0; i < D; i++) {
            low[i] = static_cast<int>(rng() % static_cast<unsigned>(shape[i] + 1));
            high[i] = static_cast<int>(rng() % static_cast<unsigned>(shape[i] + 1));
            if (low[i] > high[i]) {
                swap(low[i], high[i]);
            }
        }
        const long long want = naive_sum_nd<D>(shape, a, low, high);
        assert(cs.sum(low, high) == want);
        assert(add_cs.sum(low, high) == want);
    }
}

void test_cumsum_nd() {
    test_cumsum_nd_case<1>(array<int, 1>{7});
    test_cumsum_nd_case<2>(array<int, 2>{4, 5});
    test_cumsum_nd_case<3>(array<int, 3>{3, 4, 2});
    test_cumsum_nd_case<4>(array<int, 4>{3, 2, 4, 2});
    test_cumsum_nd_case<4>(array<int, 4>{0, 2, 3, 4});
    test_cumsum_nd_case<6>(array<int, 6>{2, 2, 2, 2, 2, 2});
}

void test_imos_1d() {
    for (int n = 0; n <= 20; n++) {
        Imos1D<long long> im(n);
        vector<long long> naive(n);
        im.add(0, 0, 10);
        if (n > 0) {
            im.add(0, n, 3);
            for (int i = 0; i < n; i++) {
                naive[i] += 3;
            }
            im.add(n / 2, n / 2, 100);
        }
        assert(im.build() == naive);
    }

    mt19937 rng(4);
    for (int tc = 0; tc < 100; tc++) {
        const int n = static_cast<int>(rng() % 40);
        Imos1D<long long> im(n);
        vector<long long> naive(n);
        for (int op = 0; op < 300; op++) {
            int l = static_cast<int>(rng() % static_cast<unsigned>(n + 1));
            int r = static_cast<int>(rng() % static_cast<unsigned>(n + 1));
            if (l > r) {
                swap(l, r);
            }
            const long long v = static_cast<int>(rng() % 101) - 50;
            im.add(l, r, v);
            for (int i = l; i < r; i++) {
                naive[i] += v;
            }
        }
        assert(im.build() == naive);
    }
}

void test_imos_2d() {
    mt19937 rng(5);
    for (int tc = 0; tc < 80; tc++) {
        const int h = static_cast<int>(rng() % 9);
        const int w = static_cast<int>(rng() % 9);
        Imos2D<long long> im(h, w);
        vector<vector<long long>> naive(h, vector<long long>(w));
        for (int op = 0; op < 200; op++) {
            int x1 = static_cast<int>(rng() % static_cast<unsigned>(h + 1));
            int x2 = static_cast<int>(rng() % static_cast<unsigned>(h + 1));
            int y1 = static_cast<int>(rng() % static_cast<unsigned>(w + 1));
            int y2 = static_cast<int>(rng() % static_cast<unsigned>(w + 1));
            if (x1 > x2) swap(x1, x2);
            if (y1 > y2) swap(y1, y2);
            const long long v = static_cast<int>(rng() % 101) - 50;
            im.add(x1, x2, y1, y2, v);
            for (int x = x1; x < x2; x++) {
                for (int y = y1; y < y2; y++) {
                    naive[x][y] += v;
                }
            }
        }
        assert(im.height() == h && im.width() == w);
        assert(im.build() == naive);
    }
}

void test_imos_3d() {
    mt19937 rng(6);
    for (int tc = 0; tc < 50; tc++) {
        const int h = static_cast<int>(rng() % 6);
        const int w = static_cast<int>(rng() % 6);
        const int d = static_cast<int>(rng() % 6);
        Imos3D<long long> im(h, w, d);
        vector<vector<vector<long long>>> naive(h, vector<vector<long long>>(w, vector<long long>(d)));
        for (int op = 0; op < 150; op++) {
            int x1 = static_cast<int>(rng() % static_cast<unsigned>(h + 1));
            int x2 = static_cast<int>(rng() % static_cast<unsigned>(h + 1));
            int y1 = static_cast<int>(rng() % static_cast<unsigned>(w + 1));
            int y2 = static_cast<int>(rng() % static_cast<unsigned>(w + 1));
            int z1 = static_cast<int>(rng() % static_cast<unsigned>(d + 1));
            int z2 = static_cast<int>(rng() % static_cast<unsigned>(d + 1));
            if (x1 > x2) swap(x1, x2);
            if (y1 > y2) swap(y1, y2);
            if (z1 > z2) swap(z1, z2);
            const long long v = static_cast<int>(rng() % 101) - 50;
            im.add(x1, x2, y1, y2, z1, z2, v);
            for (int x = x1; x < x2; x++) {
                for (int y = y1; y < y2; y++) {
                    for (int z = z1; z < z2; z++) {
                        naive[x][y][z] += v;
                    }
                }
            }
        }
        assert(im.height() == h && im.width() == w && im.depth() == d);
        assert(im.build() == naive);
    }
}

template<int D>
void test_imos_nd_case(const array<int, D>& shape) {
    ImosND<long long, D> im(shape);
    vector<long long> naive(flat_count<D>(shape));
    mt19937 rng(200 + D);
    for (int op = 0; op < 500; op++) {
        array<int, D> low{};
        array<int, D> high{};
        for (int i = 0; i < D; i++) {
            low[i] = static_cast<int>(rng() % static_cast<unsigned>(shape[i] + 1));
            high[i] = static_cast<int>(rng() % static_cast<unsigned>(shape[i] + 1));
            if (low[i] > high[i]) {
                swap(low[i], high[i]);
            }
        }
        const long long value = static_cast<int>(rng() % 101) - 50;
        im.add(low, high, value);
        add_naive_nd<D>(shape, naive, low, high, value);
    }
    assert(im.sizes() == shape);
    assert(im.build() == naive);
}

void test_imos_nd() {
    test_imos_nd_case<1>(array<int, 1>{11});
    test_imos_nd_case<2>(array<int, 2>{5, 6});
    test_imos_nd_case<3>(array<int, 3>{4, 3, 5});
    test_imos_nd_case<4>(array<int, 4>{3, 2, 4, 3});
    test_imos_nd_case<4>(array<int, 4>{0, 2, 3, 4});
    test_imos_nd_case<6>(array<int, 6>{2, 2, 2, 2, 2, 2});
}

template<class F>
long long elapsed_us(F&& f) {
    const auto start = chrono::steady_clock::now();
    f();
    const auto finish = chrono::steady_clock::now();
    return chrono::duration_cast<chrono::microseconds>(finish - start).count();
}

struct BenchRow {
    string target;
    string operation;
    size_t cells;
    size_t operations;
    long long us;
};

void print_bench_rows(const vector<BenchRow>& rows) {
    cout << "\n[benchmark]\n";
    cout << left << setw(15) << "target"
         << right << setw(12) << "cells"
         << setw(14) << "operation"
         << setw(14) << "count"
         << setw(12) << "total_us"
         << setw(14) << "ns/op" << '\n';
    for (const BenchRow& row : rows) {
        const double ns_per_op = row.operations == 0 ? 0.0 : static_cast<double>(row.us) * 1000.0 / static_cast<double>(row.operations);
        cout << left << setw(15) << row.target
             << right << setw(12) << row.cells
             << setw(14) << row.operation
             << setw(14) << row.operations
             << setw(12) << row.us
             << setw(14) << fixed << setprecision(2) << ns_per_op << '\n';
    }
}

void run_benchmark() {
    vector<BenchRow> rows;
    long long checksum = 0;
    const int query_count = 100000;
    const int build_count = 20;

    {
        const int n = 100000;
        vector<long long> a(n);
        for (int i = 0; i < n; i++) {
            a[i] = i % 97 - 48;
        }

        CumSum1D<long long> add_cs(n);
        const long long add_us = elapsed_us([&]() {
            for (int q = 0; q < query_count; q++) {
                const int i = (q * 37 + 11) % n;
                add_cs.add(i, q % 13 - 6);
            }
        });
        add_cs.build();
        checksum += add_cs.sum(0, n);
        rows.push_back({"CumSum1D", "add", static_cast<size_t>(n), query_count, add_us});

        vector<CumSum1D<long long>> build_targets;
        build_targets.reserve(build_count);
        for (int rep = 0; rep < build_count; rep++) {
            build_targets.emplace_back(n);
            for (int i = 0; i < n; i++) {
                build_targets.back().add(i, a[i]);
            }
        }
        const long long build_us = elapsed_us([&]() {
            for (int rep = 0; rep < build_count; rep++) {
                build_targets[rep].build();
            }
        });
        for (int rep = 0; rep < build_count; rep++) {
            checksum += build_targets[rep].sum(rep, n - rep);
        }
        rows.push_back({"CumSum1D", "build", static_cast<size_t>(n), build_count, build_us});

        CumSum1D<long long> cs(a);
        const long long query_us = elapsed_us([&]() {
            for (int q = 0; q < query_count; q++) {
                int l = q % (n + 1);
                int r = (q * 37 + 11) % (n + 1);
                if (l > r) swap(l, r);
                checksum += cs.sum(l, r);
            }
        });
        rows.push_back({"CumSum1D", "sum", static_cast<size_t>(n), query_count, query_us});
    }

    {
        const int h = 316;
        const int w = 317;
        vector<vector<long long>> a(h, vector<long long>(w));
        for (int x = 0; x < h; x++) {
            for (int y = 0; y < w; y++) {
                a[x][y] = (x * 31 + y * 17) % 101 - 50;
            }
        }
        const size_t cells = static_cast<size_t>(h) * static_cast<size_t>(w);

        CumSum2D<long long> add_cs(h, w);
        const long long add_us = elapsed_us([&]() {
            for (int q = 0; q < query_count; q++) {
                const int x = (q * 37 + 13) % h;
                const int y = (q * 17 + 5) % w;
                add_cs.add(x, y, q % 13 - 6);
            }
        });
        add_cs.build();
        checksum += add_cs.sum(0, h, 0, w);
        rows.push_back({"CumSum2D", "add", cells, query_count, add_us});

        vector<CumSum2D<long long>> build_targets;
        build_targets.reserve(build_count);
        for (int rep = 0; rep < build_count; rep++) {
            build_targets.emplace_back(h, w);
            for (int x = 0; x < h; x++) {
                for (int y = 0; y < w; y++) {
                    build_targets.back().add(x, y, a[x][y]);
                }
            }
        }
        const long long build_us = elapsed_us([&]() {
            for (int rep = 0; rep < build_count; rep++) {
                build_targets[rep].build();
            }
        });
        for (int rep = 0; rep < build_count; rep++) {
            checksum += build_targets[rep].sum(0, h - rep % 5, 0, w - rep % 7);
        }
        rows.push_back({"CumSum2D", "build", cells, build_count, build_us});

        CumSum2D<long long> cs(a);
        const long long query_us = elapsed_us([&]() {
            for (int q = 0; q < query_count; q++) {
                int x1 = q % (h + 1);
                int x2 = (q * 37 + 13) % (h + 1);
                int y1 = (q * 17 + 5) % (w + 1);
                int y2 = (q * 43 + 19) % (w + 1);
                if (x1 > x2) swap(x1, x2);
                if (y1 > y2) swap(y1, y2);
                checksum += cs.sum(x1, x2, y1, y2);
            }
        });
        rows.push_back({"CumSum2D", "sum", cells, query_count, query_us});
    }

    {
        const int h = 46;
        const int w = 46;
        const int d = 46;
        vector<vector<vector<long long>>> a(h, vector<vector<long long>>(w, vector<long long>(d)));
        for (int x = 0; x < h; x++) {
            for (int y = 0; y < w; y++) {
                for (int z = 0; z < d; z++) {
                    a[x][y][z] = (x * 19 + y * 23 + z * 29) % 101 - 50;
                }
            }
        }
        const size_t cells = static_cast<size_t>(h) * static_cast<size_t>(w) * static_cast<size_t>(d);

        CumSum3D<long long> add_cs(h, w, d);
        const long long add_us = elapsed_us([&]() {
            for (int q = 0; q < query_count; q++) {
                const int x = (q * 37 + 13) % h;
                const int y = (q * 17 + 5) % w;
                const int z = (q * 29 + 7) % d;
                add_cs.add(x, y, z, q % 13 - 6);
            }
        });
        add_cs.build();
        checksum += add_cs.sum(0, h, 0, w, 0, d);
        rows.push_back({"CumSum3D", "add", cells, query_count, add_us});

        vector<CumSum3D<long long>> build_targets;
        build_targets.reserve(build_count);
        for (int rep = 0; rep < build_count; rep++) {
            build_targets.emplace_back(h, w, d);
            for (int x = 0; x < h; x++) {
                for (int y = 0; y < w; y++) {
                    for (int z = 0; z < d; z++) {
                        build_targets.back().add(x, y, z, a[x][y][z]);
                    }
                }
            }
        }
        const long long build_us = elapsed_us([&]() {
            for (int rep = 0; rep < build_count; rep++) {
                build_targets[rep].build();
            }
        });
        for (int rep = 0; rep < build_count; rep++) {
            checksum += build_targets[rep].sum(0, h - rep % 5, 0, w - rep % 7, 0, d - rep % 3);
        }
        rows.push_back({"CumSum3D", "build", cells, build_count, build_us});

        CumSum3D<long long> cs(a);
        const long long query_us = elapsed_us([&]() {
            for (int q = 0; q < query_count; q++) {
                int x1 = q % (h + 1);
                int x2 = (q * 37 + 13) % (h + 1);
                int y1 = (q * 17 + 5) % (w + 1);
                int y2 = (q * 43 + 19) % (w + 1);
                int z1 = (q * 29 + 7) % (d + 1);
                int z2 = (q * 31 + 23) % (d + 1);
                if (x1 > x2) swap(x1, x2);
                if (y1 > y2) swap(y1, y2);
                if (z1 > z2) swap(z1, z2);
                checksum += cs.sum(x1, x2, y1, y2, z1, z2);
            }
        });
        rows.push_back({"CumSum3D", "sum", cells, query_count, query_us});
    }

    {
        const array<int, 4> shape = {10, 10, 10, 100};
        const size_t cells = flat_count<4>(shape);
        vector<long long> a(cells);
        for (size_t i = 0; i < cells; i++) {
            a[i] = static_cast<long long>(i % 101) - 50;
        }

        CumSumND<long long, 4> add_cs(shape);
        const long long add_us = elapsed_us([&]() {
            for (int q = 0; q < query_count; q++) {
                const array<int, 4> coord = {
                    q % shape[0],
                    (q * 7 + 1) % shape[1],
                    (q * 11 + 2) % shape[2],
                    (q * 13 + 3) % shape[3]
                };
                add_cs.add(coord, q % 13 - 6);
            }
        });
        add_cs.build();
        checksum += add_cs.sum(array<int, 4>{0, 0, 0, 0}, shape);
        rows.push_back({"CumSumND<T,4>", "add", cells, query_count, add_us});

        vector<CumSumND<long long, 4>> build_targets;
        build_targets.reserve(build_count);
        for (int rep = 0; rep < build_count; rep++) {
            build_targets.emplace_back(shape);
            for (size_t flat = 0; flat < cells; flat++) {
                build_targets.back().add(flat_to_coord<4>(flat, shape), a[flat]);
            }
        }
        const long long build_us = elapsed_us([&]() {
            for (int rep = 0; rep < build_count; rep++) {
                build_targets[rep].build();
            }
        });
        for (int rep = 0; rep < build_count; rep++) {
            array<int, 4> high = shape;
            high[0] -= rep % 3;
            high[1] -= rep % 3;
            high[2] -= rep % 3;
            high[3] -= rep % 11;
            checksum += build_targets[rep].sum(array<int, 4>{0, 0, 0, 0}, high);
        }
        rows.push_back({"CumSumND<T,4>", "build", cells, build_count, build_us});

        CumSumND<long long, 4> cs(shape, a);
        const long long query_us = elapsed_us([&]() {
            for (int q = 0; q < query_count; q++) {
                array<int, 4> low = {
                    q % (shape[0] + 1),
                    (q * 7 + 1) % (shape[1] + 1),
                    (q * 11 + 2) % (shape[2] + 1),
                    (q * 13 + 3) % (shape[3] + 1)
                };
                array<int, 4> high = {
                    (q * 17 + 4) % (shape[0] + 1),
                    (q * 19 + 5) % (shape[1] + 1),
                    (q * 23 + 6) % (shape[2] + 1),
                    (q * 29 + 7) % (shape[3] + 1)
                };
                for (int i = 0; i < 4; i++) {
                    if (low[i] > high[i]) swap(low[i], high[i]);
                }
                checksum += cs.sum(low, high);
            }
        });
        rows.push_back({"CumSumND<T,4>", "sum", cells, query_count, query_us});
    }

    {
        const array<int, 6> shape = {5, 5, 5, 5, 10, 16};
        const size_t cells = flat_count<6>(shape);
        vector<long long> a(cells);
        for (size_t i = 0; i < cells; i++) {
            a[i] = static_cast<long long>((i * 7) % 101) - 50;
        }

        CumSumND<long long, 6> add_cs(shape);
        const long long add_us = elapsed_us([&]() {
            for (int q = 0; q < query_count; q++) {
                const array<int, 6> coord = {
                    q % shape[0],
                    (q * 7 + 1) % shape[1],
                    (q * 11 + 2) % shape[2],
                    (q * 13 + 3) % shape[3],
                    (q * 17 + 4) % shape[4],
                    (q * 19 + 5) % shape[5]
                };
                add_cs.add(coord, q % 13 - 6);
            }
        });
        add_cs.build();
        checksum += add_cs.sum(array<int, 6>{0, 0, 0, 0, 0, 0}, shape);
        rows.push_back({"CumSumND<T,6>", "add", cells, query_count, add_us});

        vector<CumSumND<long long, 6>> build_targets;
        build_targets.reserve(build_count);
        for (int rep = 0; rep < build_count; rep++) {
            build_targets.emplace_back(shape);
            for (size_t flat = 0; flat < cells; flat++) {
                build_targets.back().add(flat_to_coord<6>(flat, shape), a[flat]);
            }
        }
        const long long build_us = elapsed_us([&]() {
            for (int rep = 0; rep < build_count; rep++) {
                build_targets[rep].build();
            }
        });
        for (int rep = 0; rep < build_count; rep++) {
            array<int, 6> high = shape;
            high[0] -= rep % 3;
            high[1] -= rep % 3;
            high[2] -= rep % 3;
            high[3] -= rep % 3;
            high[4] -= rep % 5;
            high[5] -= rep % 7;
            checksum += build_targets[rep].sum(array<int, 6>{0, 0, 0, 0, 0, 0}, high);
        }
        rows.push_back({"CumSumND<T,6>", "build", cells, build_count, build_us});

        CumSumND<long long, 6> cs(shape, a);
        const long long query_us = elapsed_us([&]() {
            for (int q = 0; q < query_count; q++) {
                array<int, 6> low = {
                    q % (shape[0] + 1),
                    (q * 7 + 1) % (shape[1] + 1),
                    (q * 11 + 2) % (shape[2] + 1),
                    (q * 13 + 3) % (shape[3] + 1),
                    (q * 17 + 4) % (shape[4] + 1),
                    (q * 19 + 5) % (shape[5] + 1)
                };
                array<int, 6> high = {
                    (q * 23 + 6) % (shape[0] + 1),
                    (q * 29 + 7) % (shape[1] + 1),
                    (q * 31 + 8) % (shape[2] + 1),
                    (q * 37 + 9) % (shape[3] + 1),
                    (q * 41 + 10) % (shape[4] + 1),
                    (q * 43 + 11) % (shape[5] + 1)
                };
                for (int i = 0; i < 6; i++) {
                    if (low[i] > high[i]) swap(low[i], high[i]);
                }
                checksum += cs.sum(low, high);
            }
        });
        rows.push_back({"CumSumND<T,6>", "sum", cells, query_count, query_us});
    }


    {
        const int n = 100000;
        Imos1D<long long> im(n);
        const long long add_us = elapsed_us([&]() {
            for (int q = 0; q < query_count; q++) {
                int l = q % (n + 1);
                int r = (q * 37 + 11) % (n + 1);
                if (l > r) swap(l, r);
                im.add(l, r, q % 13 - 6);
            }
        });
        rows.push_back({"Imos1D", "add", static_cast<size_t>(n), query_count, add_us});
        vector<long long> res;
        const long long build_us = elapsed_us([&]() {
            for (int rep = 0; rep < build_count; rep++) {
                res = im.build();
            }
        });
        checksum += res.empty() ? 0 : res[res.size() / 2];
        rows.push_back({"Imos1D", "build", static_cast<size_t>(n), build_count, build_us});
    }

    {
        const int h = 316;
        const int w = 317;
        Imos2D<long long> im(h, w);
        const long long add_us = elapsed_us([&]() {
            for (int q = 0; q < query_count; q++) {
                int x1 = q % (h + 1);
                int x2 = (q * 37 + 13) % (h + 1);
                int y1 = (q * 17 + 5) % (w + 1);
                int y2 = (q * 43 + 19) % (w + 1);
                if (x1 > x2) swap(x1, x2);
                if (y1 > y2) swap(y1, y2);
                im.add(x1, x2, y1, y2, q % 13 - 6);
            }
        });
        rows.push_back({"Imos2D", "add", static_cast<size_t>(h) * static_cast<size_t>(w), query_count, add_us});
        vector<vector<long long>> res;
        const long long build_us = elapsed_us([&]() {
            for (int rep = 0; rep < build_count; rep++) {
                res = im.build();
            }
        });
        checksum += res.empty() || res[0].empty() ? 0 : res[h / 2][w / 2];
        rows.push_back({"Imos2D", "build", static_cast<size_t>(h) * static_cast<size_t>(w), build_count, build_us});
    }

    {
        const int h = 46;
        const int w = 46;
        const int d = 46;
        Imos3D<long long> im(h, w, d);
        const long long add_us = elapsed_us([&]() {
            for (int q = 0; q < query_count; q++) {
                int x1 = q % (h + 1);
                int x2 = (q * 37 + 13) % (h + 1);
                int y1 = (q * 17 + 5) % (w + 1);
                int y2 = (q * 43 + 19) % (w + 1);
                int z1 = (q * 29 + 7) % (d + 1);
                int z2 = (q * 31 + 23) % (d + 1);
                if (x1 > x2) swap(x1, x2);
                if (y1 > y2) swap(y1, y2);
                if (z1 > z2) swap(z1, z2);
                im.add(x1, x2, y1, y2, z1, z2, q % 13 - 6);
            }
        });
        rows.push_back({"Imos3D", "add", static_cast<size_t>(h) * static_cast<size_t>(w) * static_cast<size_t>(d), query_count, add_us});
        vector<vector<vector<long long>>> res;
        const long long build_us = elapsed_us([&]() {
            for (int rep = 0; rep < build_count; rep++) {
                res = im.build();
            }
        });
        checksum += res.empty() || res[0].empty() || res[0][0].empty() ? 0 : res[h / 2][w / 2][d / 2];
        rows.push_back({"Imos3D", "build", static_cast<size_t>(h) * static_cast<size_t>(w) * static_cast<size_t>(d), build_count, build_us});
    }

    {
        const array<int, 4> shape = {10, 10, 10, 100};
        const size_t cells = flat_count<4>(shape);
        ImosND<long long, 4> im(shape);
        const long long add_us = elapsed_us([&]() {
            for (int q = 0; q < query_count; q++) {
                array<int, 4> low = {
                    q % (shape[0] + 1),
                    (q * 7 + 1) % (shape[1] + 1),
                    (q * 11 + 2) % (shape[2] + 1),
                    (q * 13 + 3) % (shape[3] + 1)
                };
                array<int, 4> high = {
                    (q * 17 + 4) % (shape[0] + 1),
                    (q * 19 + 5) % (shape[1] + 1),
                    (q * 23 + 6) % (shape[2] + 1),
                    (q * 29 + 7) % (shape[3] + 1)
                };
                for (int i = 0; i < 4; i++) {
                    if (low[i] > high[i]) swap(low[i], high[i]);
                }
                im.add(low, high, q % 13 - 6);
            }
        });
        rows.push_back({"ImosND<T,4>", "add", cells, query_count, add_us});
        vector<long long> res;
        const long long build_us = elapsed_us([&]() {
            for (int rep = 0; rep < build_count; rep++) {
                res = im.build();
            }
        });
        checksum += res.empty() ? 0 : res[res.size() / 2];
        rows.push_back({"ImosND<T,4>", "build", cells, build_count, build_us});
    }

    {
        const array<int, 6> shape = {5, 5, 5, 5, 10, 16};
        const size_t cells = flat_count<6>(shape);
        ImosND<long long, 6> im(shape);
        const long long add_us = elapsed_us([&]() {
            for (int q = 0; q < query_count; q++) {
                array<int, 6> low = {
                    q % (shape[0] + 1),
                    (q * 7 + 1) % (shape[1] + 1),
                    (q * 11 + 2) % (shape[2] + 1),
                    (q * 13 + 3) % (shape[3] + 1),
                    (q * 17 + 4) % (shape[4] + 1),
                    (q * 19 + 5) % (shape[5] + 1)
                };
                array<int, 6> high = {
                    (q * 23 + 6) % (shape[0] + 1),
                    (q * 29 + 7) % (shape[1] + 1),
                    (q * 31 + 8) % (shape[2] + 1),
                    (q * 37 + 9) % (shape[3] + 1),
                    (q * 41 + 10) % (shape[4] + 1),
                    (q * 43 + 11) % (shape[5] + 1)
                };
                for (int i = 0; i < 6; i++) {
                    if (low[i] > high[i]) swap(low[i], high[i]);
                }
                im.add(low, high, q % 13 - 6);
            }
        });
        rows.push_back({"ImosND<T,6>", "add", cells, query_count, add_us});
        vector<long long> res;
        const long long build_us = elapsed_us([&]() {
            for (int rep = 0; rep < build_count; rep++) {
                res = im.build();
            }
        });
        checksum += res.empty() ? 0 : res[res.size() / 2];
        rows.push_back({"ImosND<T,6>", "build", cells, build_count, build_us});
    }


    print_bench_rows(rows);
    cout << "checksum: " << checksum << '\n';
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    test_cumsum_1d();
    test_cumsum_2d();
    test_cumsum_3d();
    test_cumsum_nd();
    test_imos_1d();
    test_imos_2d();
    test_imos_3d();
    test_imos_nd();
    cout << "[test] all tests passed\n";

    run_benchmark();
    return 0;
}

#endif

// 実行結果(atcoder)
// [test] all tests passed

// [benchmark]
// target                cells     operation         count    total_us         ns/op
// CumSum1D             100000           add        100000         213          2.13
// CumSum1D             100000         build            20         803      40150.00
// CumSum1D             100000           sum        100000         174          1.74
// CumSum2D             100172           add        100000         315          3.15
// CumSum2D             100172         build            20         806      40300.00
// CumSum2D             100172           sum        100000         451          4.51
// CumSum3D              97336           add        100000         366          3.66
// CumSum3D              97336         build            20        2116     105800.00
// CumSum3D              97336           sum        100000         797          7.97
// CumSumND<T,4>        100000           add        100000         696          6.96
// CumSumND<T,4>        100000         build            20        7065     353250.00
// CumSumND<T,4>        100000           sum        100000        4612         46.12
// CumSumND<T,6>        100000           add        100000        1093         10.93
// CumSumND<T,6>        100000         build            20       13319     665950.00
// CumSumND<T,6>        100000           sum        100000       10675        106.75
// Imos1D               100000           add        100000         170          1.70
// Imos1D               100000         build            20        1554      77700.00
// Imos2D               100172           add        100000         565          5.65
// Imos2D               100172         build            20        3585     179250.00
// Imos3D                97336           add        100000         879          8.79
// Imos3D                97336         build            20        5468     273400.00
// ImosND<T,4>          100000           add        100000        4314         43.14
// ImosND<T,4>          100000         build            20        6055     302750.00
// ImosND<T,6>          100000           add        100000        2562         25.62
// ImosND<T,6>          100000         build            20        6332     316600.00
// checksum: -371724108
