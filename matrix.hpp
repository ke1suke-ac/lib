/*
 * matrix: 競技プログラミング向けの行列データ構造・公開関数・ユースケースソルバー集
 * 通常行列、固定長行列、演算をテンプレート指定する汎用半環行列、min-plus / max-plus 行列、
 * GF(2)・boolean 用ビット圧縮行列を提供し、累乗、経路DP、掃き出し法、行列式、行列木定理を扱う
 * 各グループは「データ構造、公開関数群、そのデータ構造を使う solver 群」の順に配置する
 * solver 固有の補助処理は関数内 lambda として持ち、solver 単位でコピーしやすい構成とする
 * 厳密な体、実数、整数、合成数 mod、GF(2) は前提が異なるため用途別 solver に分離する
 * min-plus / max-plus の有限値は絶対値 4e18 未満を前提とし、加算はその範囲へ飽和させる
 */
#pragma once

#include <bits/stdc++.h>

// ============================================================================
// matrix<T>: 通常の加算・乗算を使う動的密行列
// ============================================================================

// ==================== matrix<T> データ構造 ここから ====================
// コピー依存: <bits/stdc++.h>

// 行優先の連続領域に格納する動的な長方形密行列
template <class T>
struct matrix {
private:
    static_assert(!std::is_same_v<std::remove_cv_t<T>, bool>,
                  "bool 行列には bit_matrix を使用する");

    int height_ = 0;
    int width_ = 0;
    std::vector<T> data_;

public:
    // 0 × 0 行列を構築する。O(1)
    matrix() = default;

    // height × width 行列を value で初期化する。O(height * width)
    matrix(int height, int width, const T& value = T{})
        : height_(height), width_(width) {
        assert(height >= 0);
        assert(width >= 0);
        data_.assign(
            static_cast<std::size_t>(height) * width,
            value);
    }

    // 行数を返す。O(1)
    int rows() const { return height_; }

    // 列数を返す。O(1)
    int cols() const { return width_; }

    // 要素が 1 個も存在しないなら true を返す。O(1)
    bool empty() const { return height_ == 0 || width_ == 0; }

    // 連続領域の先頭ポインタを返す。O(1)
    T* data() { return data_.data(); }

    // 連続領域の先頭ポインタを返す。O(1)
    const T* data() const { return data_.data(); }

    // row 行目の先頭ポインタを返す。O(1)
    T* operator[](int row) {
        assert(0 <= row && row < height_);
        if (width_ == 0) return data_.data();
        return data_.data() + static_cast<std::size_t>(row) * width_;
    }

    // row 行目の先頭ポインタを返す。O(1)
    const T* operator[](int row) const {
        assert(0 <= row && row < height_);
        if (width_ == 0) return data_.data();
        return data_.data() + static_cast<std::size_t>(row) * width_;
    }

    // 2 行を交換する。O(width)
    void swap_rows(int row_a, int row_b) {
        assert(0 <= row_a && row_a < height_);
        assert(0 <= row_b && row_b < height_);
        if (row_a == row_b) return;
        for (int col = 0; col < width_; ++col) std::swap((*this)[row_a][col], (*this)[row_b][col]);
    }

    // size 次単位行列を返す。O(size^2)
    static matrix identity(int size) {
        assert(size >= 0);
        matrix result(size, size, T{});
        for (int index = 0; index < size; ++index) result[index][index] = T{1};
        return result;
    }
};

// ==================== matrix<T> データ構造 ここまで ====================

// ==================== matrix<T> 公開関数群 ここから ====================
// コピー依存: matrix<T>

// rhs を要素ごとに加算する。O(rows * cols)
template <class T>
matrix<T>& operator+=(matrix<T>& lhs, const matrix<T>& rhs) {
    assert(lhs.rows() == rhs.rows());
    assert(lhs.cols() == rhs.cols());
    const std::size_t size = static_cast<std::size_t>(lhs.rows()) * lhs.cols();
    for (std::size_t index = 0; index < size; ++index) lhs.data()[index] += rhs.data()[index];
    return lhs;
}

// rhs を要素ごとに減算する。O(rows * cols)
template <class T>
matrix<T>& operator-=(matrix<T>& lhs, const matrix<T>& rhs) {
    assert(lhs.rows() == rhs.rows());
    assert(lhs.cols() == rhs.cols());
    const std::size_t size = static_cast<std::size_t>(lhs.rows()) * lhs.cols();
    for (std::size_t index = 0; index < size; ++index) lhs.data()[index] -= rhs.data()[index];
    return lhs;
}

// 2 行列を要素ごとに加算する。O(rows * cols)
template <class T>
matrix<T> operator+(matrix<T> lhs, const matrix<T>& rhs) {
    lhs += rhs;
    return lhs;
}

// 2 行列を要素ごとに減算する。O(rows * cols)
template <class T>
matrix<T> operator-(matrix<T> lhs, const matrix<T>& rhs) {
    lhs -= rhs;
    return lhs;
}

// lhs と rhs の行列積を返す。O(lhs.rows * lhs.cols * rhs.cols)
template <class T>
matrix<T> operator*(const matrix<T>& lhs, const matrix<T>& rhs) {
    assert(lhs.cols() == rhs.rows());
    matrix<T> result(lhs.rows(), rhs.cols(), T{});

    // 内側を連続アクセスにして、キャッシュ効率と自動 SIMD を優先する
    for (int row = 0; row < lhs.rows(); ++row) {
        T* const result_row = result[row];
        const T* const lhs_row = lhs[row];
        for (int middle = 0; middle < lhs.cols(); ++middle) {
            const T value = lhs_row[middle];
            const T* const rhs_row = rhs[middle];
            for (int col = 0; col < rhs.cols(); ++col) result_row[col] += value * rhs_row[col];
        }
    }
    return result;
}

// 行列を列ベクトルへ適用する。O(rows * cols)
template <class T>
std::vector<T> matrix_apply(const matrix<T>& a, const std::vector<T>& vector) {
    assert(a.cols() == std::ssize(vector));
    std::vector<T> result(a.rows(), T{});
    for (int row = 0; row < a.rows(); ++row) {
        T value{};
        for (int col = 0; col < a.cols(); ++col) value += a[row][col] * vector[col];
        result[row] = value;
    }
    return result;
}

// 正方行列の非負整数乗を返す。O(n^3 log exponent)
template <class T>
matrix<T> matrix_power(matrix<T> base, std::uint64_t exponent) {
    assert(base.rows() == base.cols());
    matrix<T> result = matrix<T>::identity(base.rows());
    while (exponent != 0) {
        if ((exponent & 1U) != 0U) result = result * base;
        exponent >>= 1U;
        if (exponent != 0) base = base * base;
    }
    return result;
}

// ---------- 行列累乗のベクトル適用・前方和 ----------

// 想定ユースケース: 状態遷移、線形漸化式、確率遷移などで A^exponent 自体は不要で、最終状態だけ欲しい場合
// 処理概要: 列ベクトルを状態とみなし、二分累乗中に指数の bit が立っている行列だけを状態へ左から適用する
// 考え方: 通常の行列累乗で結果行列へ掛ける部分を行列ベクトル積へ置き換え、選択 bit ごとの計算を O(n^2) にする
// 注意: base[row][col] は col 状態から row 状態への係数として扱い、exponent == 0 では入力ベクトルをそのまま返す
// 正方行列の非負整数乗を列ベクトルへ適用する。O(n^3 log exponent + n^2 popcount(exponent))
template <class T>
std::vector<T> matrix_power_apply(
    matrix<T> base,
    std::uint64_t exponent,
    std::vector<T> vector) {
    assert(base.rows() == base.cols());
    assert(base.cols() == std::ssize(vector));

    // 指数の立っている bit だけベクトルへ適用し、結果行列の構築を省く
    while (exponent != 0) {
        if ((exponent & 1U) != 0U) vector = matrix_apply(base, vector);
        exponent >>= 1U;
        if (exponent != 0) base = base * base;
    }
    return vector;
}

template <class T>
struct matrix_power_prefix_sum_result {
    matrix<T> power;
    matrix<T> prefix_sum;
};

// 想定ユースケース: 長さが一定値未満の walk 数、遷移結果の累積、行列版の等比級数をまとめて求める場合
// 処理概要: 長さ L の区間を power=A^L と prefix_sum=sum_{i=0}^{L-1} A^i の組で保持して二分累乗する
// 考え方: 左区間の後に右区間を連結すると、累乗は P_left P_right、和は S_left + P_left S_right になる
// 注意: 対象範囲は半開区間 [0, count) であり、count == 0 では power=I、prefix_sum=0 を返す
// A^count と I + A + ... + A^(count-1) を返す。O(n^3 log count)
template <class T>
matrix_power_prefix_sum_result<T> matrix_power_prefix_sum(
    matrix<T> base,
    std::uint64_t count) {
    assert(base.rows() == base.cols());
    const int size = base.rows();
    matrix_power_prefix_sum_result<T> result{
        matrix<T>::identity(size),
        matrix<T>(size, size, T{})};
    matrix_power_prefix_sum_result<T> block{
        std::move(base),
        matrix<T>::identity(size)};

    // 連続する 2 区間を power と prefix_sum の組として結合する
    const auto combine = [](const matrix_power_prefix_sum_result<T>& left,
                            const matrix_power_prefix_sum_result<T>& right) {
        return matrix_power_prefix_sum_result<T>{
            left.power * right.power,
            left.prefix_sum + left.power * right.prefix_sum};
    };

    // 区間長を二進展開し、通常の二分累乗と同じ順序で結合する
    while (count != 0) {
        if ((count & 1U) != 0U) result = combine(result, block);
        count >>= 1U;
        if (count != 0) block = combine(block, block);
    }
    return result;
}

// ==================== matrix<T> 公開関数群 ここまで ====================

// ==================== matrix<T> solver 群 ここから ====================
// コピー依存: matrix<T> と必要な公開関数

// ==================== 線形漸化式 ここから ====================
// コピー依存: matrix<T>, matrix_power_apply

// 想定ユースケース: Fibonacci 型を含む低次数の線形漸化式について、非常に大きい添字の項を求める場合
// 処理概要: d=initial.size() とし、直近 d 項を並べた状態を companion 行列で 1 項ずつ進める
// 考え方: 状態 [a_t, a_(t-1), ..., a_(t-d+1)] の先頭行へ係数を置き、残りは 1 つ前の項をずらす
// 係数規約: coefficient[j] は a_(i-1-j) に掛かり、index < d の場合は initial[index] を直接返す
// 注意: 次数 d が大きい場合は Kitamasa 法などの O(d^2 log index) の専用手法が適する
// a_i = sum coefficient[j] * a_(i-1-j) で定まる数列の index 項を返す。O(d^3 log index)
template <class T>
T linear_recurrence_nth(
    const std::vector<T>& initial,
    const std::vector<T>& coefficient,
    std::uint64_t index) {
    assert(!initial.empty());
    assert(initial.size() == coefficient.size());
    assert(initial.size() <= std::numeric_limits<int>::max());
    const int degree = static_cast<int>(initial.size());
    if (index < static_cast<std::uint64_t>(degree)) return initial[index];

    // [a_t, a_(t-1), ..., a_(t-d+1)] を次時刻へ進める companion 行列を作る
    matrix<T> transition(degree, degree, T{});
    for (int col = 0; col < degree; ++col) transition[0][col] = coefficient[col];
    for (int row = 1; row < degree; ++row) transition[row][row - 1] = T{1};

    // 最後の初期項を先頭にした状態へ必要回数だけ遷移を適用する
    std::vector<T> state(degree);
    for (int offset = 0; offset < degree; ++offset) {
        state[offset] = initial[degree - 1 - offset];
    }
    const std::uint64_t exponent = index - (degree - 1);
    return matrix_power_apply(std::move(transition), exponent, std::move(state))[0];
}

// ==================== 線形漸化式 ここまで ====================

// ==================== 経路数 ここから ====================
// コピー依存: matrix<T>, matrix_power_apply, matrix_power_prefix_sum

// 想定ユースケース: 頂点数が小さく、辺数だけが非常に大きい経路数 DP やオートマトンの受理数を求める場合
// 処理概要: transition[to][from] に辺数を格納し、A^edge_count を source の単位ベクトルへ適用する
// 考え方: 行列積の中間添字が経由頂点に対応し、A^k[to][from] が長さ k の walk 数になる
// 注意: 単純路ではなく頂点・辺の再利用を許す walk を数え、多重辺はそれぞれ別の遷移として加算する
// 型の選択: T には通常 long long より、問題の法に対応した modint を使うことが多い
// 有向グラフで source から destination への長さ edge_count の walk 数を返す。O(V^3 log edge_count)
template <class T>
T count_walks_exactly_k(
    int vertex_count,
    const std::vector<std::pair<int, int>>& directed_edges,
    int source,
    int destination,
    std::uint64_t edge_count) {
    assert(vertex_count >= 0);
    assert(0 <= source && source < vertex_count);
    assert(0 <= destination && destination < vertex_count);
    matrix<T> transition(vertex_count, vertex_count, T{});
    for (const auto& [from, to] : directed_edges) {
        assert(0 <= from && from < vertex_count);
        assert(0 <= to && to < vertex_count);
        transition[to][from] += T{1};
    }
    std::vector<T> state(vertex_count, T{});
    state[source] = T{1};
    return matrix_power_apply(std::move(transition), edge_count, std::move(state))[
        destination];
}

// 想定ユースケース: 長さ 0,1,...,K-1 の経路数総和や、K 回未満で到達する方法数をまとめて求める場合
// 処理概要: I+A+...+A^(length_limit-1) を source の単位ベクトルへ適用する
// 考え方: 各 A^k が長さ k の walk 数を表すため、行列の前方和が辺数ごとの答えの総和になる
// 注意: length_limit > 0 なら長さ 0 の空 walk を含むため、source == destination の答えへ 1 が加わる
// 境界: length_limit == 0 では許される長さがなく、答えは 0 になる
// 有向グラフで長さが length_limit 未満の source-destination walk 数を返す。O(V^3 log length_limit)
template <class T>
T count_walks_with_length_less_than(
    int vertex_count,
    const std::vector<std::pair<int, int>>& directed_edges,
    int source,
    int destination,
    std::uint64_t length_limit) {
    assert(vertex_count >= 0);
    assert(0 <= source && source < vertex_count);
    assert(0 <= destination && destination < vertex_count);
    matrix<T> transition(vertex_count, vertex_count, T{});
    for (const auto& [from, to] : directed_edges) {
        assert(0 <= from && from < vertex_count);
        assert(0 <= to && to < vertex_count);
        transition[to][from] += T{1};
    }
    const auto powers = matrix_power_prefix_sum(std::move(transition), length_limit);
    std::vector<T> state(vertex_count, T{});
    state[source] = T{1};
    return matrix_apply(powers.prefix_sum, state)[destination];
}

// ==================== 経路数 ここまで ====================

// ==================== マルコフ連鎖 ここから ====================
// コピー依存: matrix<long double>, matrix_power_apply

// 想定ユースケース: 状態数の小さい時不変マルコフ連鎖で、非常に多いステップ後の確率分布を求める場合
// 処理概要: transition[to][from] に遷移確率を足し、遷移行列の累乗を初期分布へ適用する
// 考え方: 分布を列ベクトルとして扱うため、各 from 列の確率が遷移先 to の成分へ流れる
// 注意: 各列の確率和や非負性は検証せず、入力が確率遷移として妥当であることを呼び出し側が保証する
// 数値誤差: long double でも累乗・加算誤差は蓄積するため、必要なら最終結果を許容誤差付きで扱う
// 初期分布へ確率遷移を steps 回適用した分布を返す。O(S^3 log steps)
inline std::vector<long double> markov_distribution_after_steps(
    int state_count,
    const std::vector<std::tuple<int, int, long double>>& transitions,
    const std::vector<long double>& initial_distribution,
    std::uint64_t steps) {
    assert(state_count >= 0);
    assert(std::ssize(initial_distribution) == state_count);
    matrix<long double> transition(state_count, state_count, 0.0L);
    for (const auto& [from, to, probability] : transitions) {
        assert(0 <= from && from < state_count);
        assert(0 <= to && to < state_count);
        transition[to][from] += probability;
    }
    return matrix_power_apply(std::move(transition), steps, initial_distribution);
}

// ==================== マルコフ連鎖 ここまで ====================

// ==================== 厳密な体上の掃き出し法 ここから ====================
// コピー依存: matrix<T>

struct field_gauss_jordan_result {
    int rank;
    std::vector<int> pivot_columns;
};

// 想定ユースケース: rank、逆行列、連立一次方程式に加え、拡大係数行列を自分で構成する特殊な掃き出し法
// 処理概要: 左から非零ピボットを選び、ピボット行を 1 に正規化して他の全行から対象列を消去する
// 考え方: pivot_column_count より右の列をピボット候補から外すことで、定数列や付加行列を行操作だけ追従させる
// 型の前提: T は非零要素が必ず逆元を持ち、== T{} で厳密に零判定できる体でなければならない
// 利用不可: int、long long、一般の合成数 mod、double などには使用しない
// 副作用: a を破壊的に RREF 化し、rank と各ピボット列を返す
// 厳密な体上で先頭 pivot_column_count 列をピボット候補として RREF 化する。O(H * W * min(H, pivot_column_count))
template <class T>
field_gauss_jordan_result field_gauss_jordan_inplace(
    matrix<T>& a,
    int pivot_column_count) {
    assert(0 <= pivot_column_count && pivot_column_count <= a.cols());
    int rank = 0;
    std::vector<int> pivot_columns;
    pivot_columns.reserve(std::min(a.rows(), pivot_column_count));

    // 左から順に非零ピボットを探し、ピボット行を 1 へ正規化する
    for (int col = 0; col < pivot_column_count && rank < a.rows(); ++col) {
        int pivot_row = rank;
        while (pivot_row < a.rows() && a[pivot_row][col] == T{}) ++pivot_row;
        if (pivot_row == a.rows()) continue;
        a.swap_rows(rank, pivot_row);

        const T inverse = T{1} / a[rank][col];
        for (int target_col = col; target_col < a.cols(); ++target_col) {
            a[rank][target_col] *= inverse;
        }
        a[rank][col] = T{1};

        // ピボット行以外から対象列を消去し、ピボット列を単位ベクトルにする
        for (int row = 0; row < a.rows(); ++row) {
            if (row == rank || a[row][col] == T{}) continue;
            const T factor = a[row][col];
            for (int target_col = col; target_col < a.cols(); ++target_col) {
                a[row][target_col] -= factor * a[rank][target_col];
            }
            a[row][col] = T{};
        }
        pivot_columns.push_back(col);
        ++rank;
    }
    return {rank, std::move(pivot_columns)};
}

// 想定ユースケース: ベクトル集合の一次独立性、線形写像の像の次元、制約式の独立本数を求める場合
// 処理概要: 入力行列をコピーして全列を対象にガウス・ジョルダン消去し、得られたピボット数を返す
// 考え方: 体上では非零ピボットごとに独立な行・列が 1 つ確定し、その総数が rank になる
// 型の前提: 素数 modint や有理数などの厳密な体を使用する
// 厳密な体上で行列の rank を返す。O(H * W * min(H, W))
template <class T>
int field_rank(matrix<T> a) {
    // この solver 単体でコピーできるよう、必要な掃き出し処理をローカルに持つ
    const auto gauss_jordan = [](matrix<T>& target, int pivot_column_count) {
        int rank = 0;
        std::vector<int> pivot_columns;
        pivot_columns.reserve(std::min(target.rows(), pivot_column_count));
        for (int col = 0; col < pivot_column_count && rank < target.rows(); ++col) {
            int pivot_row = rank;
            while (pivot_row < target.rows() && target[pivot_row][col] == T{}) ++pivot_row;
            if (pivot_row == target.rows()) continue;
            target.swap_rows(rank, pivot_row);

            const T inverse = T{1} / target[rank][col];
            for (int target_col = col; target_col < target.cols(); ++target_col) {
                target[rank][target_col] *= inverse;
            }
            target[rank][col] = T{1};
            for (int row = 0; row < target.rows(); ++row) {
                if (row == rank || target[row][col] == T{}) continue;
                const T factor = target[row][col];
                for (int target_col = col; target_col < target.cols(); ++target_col) {
                    target[row][target_col] -= factor * target[rank][target_col];
                }
                target[row][col] = T{};
            }
            pivot_columns.push_back(col);
            ++rank;
        }
        return field_gauss_jordan_result{rank, std::move(pivot_columns)};
    };

    return gauss_jordan(a, a.cols()).rank;
}

// 想定ユースケース: 正則性判定、行列木定理、線形変換の体積係数などを厳密に求める場合
// 処理概要: 前進消去で上三角化し、行交換の符号と消去前のピボット値の積を追跡する
// 考え方: 行の定数倍を行わず、他行への倍加だけで消去するため、行列式は符号と対角積から復元できる
// 型の前提: 除算が常に正しく行える体を使用し、合成数 mod や整数には専用関数を使う
// 境界: 0 次正方行列の行列式は空積として 1 を返す
// 厳密な体上で正方行列の行列式を返す。O(n^3)
template <class T>
T field_determinant(matrix<T> a) {
    assert(a.rows() == a.cols());
    const int size = a.rows();
    T determinant{1};

    // 前進消去だけを行い、行交換の符号とピボット積を追跡する
    for (int col = 0; col < size; ++col) {
        int pivot_row = col;
        while (pivot_row < size && a[pivot_row][col] == T{}) ++pivot_row;
        if (pivot_row == size) return T{};
        if (pivot_row != col) {
            a.swap_rows(col, pivot_row);
            determinant = T{} - determinant;
        }

        const T pivot = a[col][col];
        determinant *= pivot;
        for (int row = col + 1; row < size; ++row) {
            if (a[row][col] == T{}) continue;
            const T factor = a[row][col] / pivot;
            a[row][col] = T{};
            for (int target_col = col + 1; target_col < size; ++target_col) {
                a[row][target_col] -= factor * a[col][target_col];
            }
        }
    }
    return determinant;
}

// 想定ユースケース: 同じ係数行列へ多数の右辺を適用する場合や、線形変換を逆向きに戻す場合
// 処理概要: 拡大行列 [A | I] を作り、左半分だけをピボット候補として RREF 化する
// 考え方: A を I へ変形する行基本変形を右側の I にも適用すると、その右側が A^{-1} になる
// 返り値: rank が n 未満なら逆行列は存在しないため nullopt を返す
// 型の前提: 素数 modint や有理数などの厳密な体を使用する
// 厳密な体上で正方行列の逆行列を返し、特異なら nullopt を返す。O(n^3)
template <class T>
std::optional<matrix<T>> field_inverse(matrix<T> a) {
    assert(a.rows() == a.cols());
    const int size = a.rows();

    // この solver 内で完結する厳密なガウス・ジョルダン消去
    const auto gauss_jordan = [](matrix<T>& target, int pivot_column_count) {
        int rank = 0;
        std::vector<int> pivot_columns;
        pivot_columns.reserve(std::min(target.rows(), pivot_column_count));
        for (int col = 0; col < pivot_column_count && rank < target.rows(); ++col) {
            int pivot_row = rank;
            while (pivot_row < target.rows() && target[pivot_row][col] == T{}) ++pivot_row;
            if (pivot_row == target.rows()) continue;
            target.swap_rows(rank, pivot_row);

            const T inverse = T{1} / target[rank][col];
            for (int target_col = col; target_col < target.cols(); ++target_col) {
                target[rank][target_col] *= inverse;
            }
            target[rank][col] = T{1};
            for (int row = 0; row < target.rows(); ++row) {
                if (row == rank || target[row][col] == T{}) continue;
                const T factor = target[row][col];
                for (int target_col = col; target_col < target.cols(); ++target_col) {
                    target[row][target_col] -= factor * target[rank][target_col];
                }
                target[row][col] = T{};
            }
            pivot_columns.push_back(col);
            ++rank;
        }
        return field_gauss_jordan_result{rank, std::move(pivot_columns)};
    };

    // [A | I] を構築して左半分だけをピボット候補として掃き出す
    matrix<T> augmented(size, size * 2, T{});
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) augmented[row][col] = a[row][col];
        augmented[row][size + row] = T{1};
    }
    const auto elimination = gauss_jordan(augmented, size);
    if (elimination.rank != size) return std::nullopt;

    // RREF 化後の右半分を逆行列として取り出す
    matrix<T> inverse(size, size, T{});
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) inverse[row][col] = augmented[row][size + col];
    }
    return inverse;
}

enum class field_linear_system_status {
    no_solution,
    unique_solution,
    multiple_solutions
};

template <class T>
struct field_linear_system_result {
    field_linear_system_status status;
    int rank;
    std::vector<T> particular_solution;
    matrix<T> kernel_basis;
};

// 想定ユースケース: 一意解だけでなく、不定解の自由度や全解の表現まで必要な線形制約問題
// 処理概要: [A | b] を RREF 化し、矛盾行を検出した後、自由変数を 0 とした特殊解を作る
// 考え方: 各自由変数を 1 つだけ 1 にしてピボット変数を逆算すると、ker(A) の基底が得られる
// 結果の解釈: 全解は particular_solution と kernel_basis の各行の任意線形結合で表せる
// 状態: 解なし、一意解、複数解を status で区別し、kernel_basis の行数は variable_count-rank になる
// 型の前提: 厳密な体を使用し、浮動小数点には real_solve_linear_system を使う
// 厳密な体上で Ax=b を解き、特殊解と斉次解空間の基底を返す。O(H * W * min(H, W))
template <class T>
field_linear_system_result<T> field_solve_linear_system(
    matrix<T> coefficient,
    const std::vector<T>& constant) {
    assert(coefficient.rows() == std::ssize(constant));
    const int equation_count = coefficient.rows();
    const int variable_count = coefficient.cols();

    // この solver 内で完結する厳密なガウス・ジョルダン消去
    const auto gauss_jordan = [](matrix<T>& target, int pivot_column_count) {
        int rank = 0;
        std::vector<int> pivot_columns;
        pivot_columns.reserve(std::min(target.rows(), pivot_column_count));
        for (int col = 0; col < pivot_column_count && rank < target.rows(); ++col) {
            int pivot_row = rank;
            while (pivot_row < target.rows() && target[pivot_row][col] == T{}) ++pivot_row;
            if (pivot_row == target.rows()) continue;
            target.swap_rows(rank, pivot_row);

            const T inverse = T{1} / target[rank][col];
            for (int target_col = col; target_col < target.cols(); ++target_col) {
                target[rank][target_col] *= inverse;
            }
            target[rank][col] = T{1};
            for (int row = 0; row < target.rows(); ++row) {
                if (row == rank || target[row][col] == T{}) continue;
                const T factor = target[row][col];
                for (int target_col = col; target_col < target.cols(); ++target_col) {
                    target[row][target_col] -= factor * target[rank][target_col];
                }
                target[row][col] = T{};
            }
            pivot_columns.push_back(col);
            ++rank;
        }
        return field_gauss_jordan_result{rank, std::move(pivot_columns)};
    };

    // 係数行列と定数列を 1 個の拡大係数行列へまとめて RREF 化する
    matrix<T> augmented(equation_count, variable_count + 1, T{});
    for (int row = 0; row < equation_count; ++row) {
        for (int col = 0; col < variable_count; ++col) augmented[row][col] = coefficient[row][col];
        augmented[row][variable_count] = constant[row];
    }
    const auto elimination = gauss_jordan(augmented, variable_count);

    // 係数がすべて 0 で定数だけが非零の行があれば矛盾する
    for (int row = 0; row < equation_count; ++row) {
        bool all_zero = true;
        for (int col = 0; col < variable_count; ++col) {
            if (augmented[row][col] != T{}) {
                all_zero = false;
                break;
            }
        }
        if (all_zero && augmented[row][variable_count] != T{}) {
            return {
                field_linear_system_status::no_solution,
                elimination.rank,
                {},
                matrix<T>(0, variable_count, T{})};
        }
    }

    // 自由変数を 0 と置いた特殊解を構築する
    std::vector<T> particular(variable_count, T{});
    for (int row = 0; row < elimination.rank; ++row) {
        const int pivot_col = elimination.pivot_columns[row];
        particular[pivot_col] = augmented[row][variable_count];
    }

    // 各自由変数を 1 個ずつ 1 にして斉次解空間の基底を構築する
    std::vector<bool> is_pivot(variable_count, false);
    for (const int pivot_col : elimination.pivot_columns) is_pivot[pivot_col] = true;
    const int free_count = variable_count - elimination.rank;
    matrix<T> kernel_basis(free_count, variable_count, T{});
    int basis_row = 0;
    for (int free_col = 0; free_col < variable_count; ++free_col) {
        if (is_pivot[free_col]) continue;
        kernel_basis[basis_row][free_col] = T{1};
        for (int row = 0; row < elimination.rank; ++row) {
            const int pivot_col = elimination.pivot_columns[row];
            kernel_basis[basis_row][pivot_col] = T{} - augmented[row][free_col];
        }
        ++basis_row;
    }

    const field_linear_system_status status =
        free_count == 0 ? field_linear_system_status::unique_solution
                        : field_linear_system_status::multiple_solutions;
    return {status, elimination.rank, std::move(particular), std::move(kernel_basis)};
}

// ==================== 厳密な体上の掃き出し法 ここまで ====================

// ==================== 実数上の掃き出し法 ここから ====================
// コピー依存: matrix<T>

template <std::floating_point T>
struct real_gauss_jordan_result {
    int rank;
    std::vector<int> pivot_columns;
};

// 想定ユースケース: 幾何計算や数値係数の連立方程式で、近似的な rank や解空間を調べる場合
// 処理概要: 各列で絶対値最大のピボットを選び、eps 以下を 0 とみなしてガウス・ジョルダン消去する
// 考え方: 大きいピボットを選ぶ部分ピボット法により、極端に小さい値で割ることによる誤差増幅を抑える
// eps の意味: 絶対誤差による零判定であり、入力のスケールに合わせて呼び出し側が指定する
// 副作用: a を近似的な RREF へ破壊的に変形し、微小値を 0 へ丸める
// 実数上で部分ピボット選択を行い、先頭 pivot_column_count 列を RREF 化する。O(H * W * min(H, pivot_column_count))
template <std::floating_point T>
real_gauss_jordan_result<T> real_gauss_jordan_inplace(
    matrix<T>& a,
    int pivot_column_count,
    T eps) {
    assert(0 <= pivot_column_count && pivot_column_count <= a.cols());
    assert(eps >= T{});
    int rank = 0;
    std::vector<int> pivot_columns;
    pivot_columns.reserve(std::min(a.rows(), pivot_column_count));

    // 各列で絶対値最大のピボットを選び、微小な列は従属として飛ばす
    for (int col = 0; col < pivot_column_count && rank < a.rows(); ++col) {
        int pivot_row = rank;
        for (int row = rank + 1; row < a.rows(); ++row) {
            if (std::abs(a[row][col]) > std::abs(a[pivot_row][col])) pivot_row = row;
        }
        if (std::abs(a[pivot_row][col]) <= eps) continue;
        a.swap_rows(rank, pivot_row);

        const T pivot = a[rank][col];
        for (int target_col = col; target_col < a.cols(); ++target_col) a[rank][target_col] /= pivot;
        a[rank][col] = T{1};

        // ピボット行以外を消去し、演算で生じた eps 以下の値を 0 へ丸める
        for (int row = 0; row < a.rows(); ++row) {
            if (row == rank || std::abs(a[row][col]) <= eps) {
                if (row != rank) a[row][col] = T{};
                continue;
            }
            const T factor = a[row][col];
            for (int target_col = col; target_col < a.cols(); ++target_col) {
                a[row][target_col] -= factor * a[rank][target_col];
                if (std::abs(a[row][target_col]) <= eps) a[row][target_col] = T{};
            }
            a[row][col] = T{};
        }
        pivot_columns.push_back(col);
        ++rank;
    }
    return {rank, std::move(pivot_columns)};
}

// 想定ユースケース: 数値ベクトルの独立性、幾何的な次元、実数係数制約の有効本数を近似判定する場合
// 処理概要: 入力をコピーし、全列を対象に部分ピボット付き掃き出し法を適用してピボット数を返す
// 考え方: eps より大きいピボットを持つ列だけを独立とみなし、得られたピボット数を数値的な rank とする
// 注意: rank は eps と入力スケールに依存し、数学的に同じ行列でも丸め誤差により結果が変わり得る
// 実数行列の eps に基づく rank を返す。O(H * W * min(H, W))
template <std::floating_point T>
int real_rank(matrix<T> a, T eps) {
    assert(eps >= T{});

    // rank 判定に必要な部分ピボット付き掃き出しをローカルに持つ
    const auto gauss_jordan = [eps](matrix<T>& target, int pivot_column_count) {
        int rank = 0;
        std::vector<int> pivot_columns;
        pivot_columns.reserve(std::min(target.rows(), pivot_column_count));
        for (int col = 0; col < pivot_column_count && rank < target.rows(); ++col) {
            int pivot_row = rank;
            for (int row = rank + 1; row < target.rows(); ++row) {
                if (std::abs(target[row][col]) > std::abs(target[pivot_row][col])) pivot_row = row;
            }
            if (std::abs(target[pivot_row][col]) <= eps) continue;
            target.swap_rows(rank, pivot_row);

            const T pivot = target[rank][col];
            for (int target_col = col; target_col < target.cols(); ++target_col) {
                target[rank][target_col] /= pivot;
            }
            target[rank][col] = T{1};
            for (int row = 0; row < target.rows(); ++row) {
                if (row == rank || std::abs(target[row][col]) <= eps) {
                    if (row != rank) target[row][col] = T{};
                    continue;
                }
                const T factor = target[row][col];
                for (int target_col = col; target_col < target.cols(); ++target_col) {
                    target[row][target_col] -= factor * target[rank][target_col];
                    if (std::abs(target[row][target_col]) <= eps) target[row][target_col] = T{};
                }
                target[row][col] = T{};
            }
            pivot_columns.push_back(col);
            ++rank;
        }
        return real_gauss_jordan_result<T>{rank, std::move(pivot_columns)};
    };

    return gauss_jordan(a, a.cols()).rank;
}

// 想定ユースケース: 実数係数行列の正則性の目安や、幾何変換の向き・面積体積倍率を求める場合
// 処理概要: 絶対値最大のピボットを選びながら前進消去し、行交換の符号とピボット積を追跡する
// 考え方: RREF まで進めず上三角化だけに留め、逆行列や連立方程式より少ない演算で行列式を得る
// 注意: ピボットの絶対値が eps 以下なら 0 を返すため、悪条件行列では厳密な判定にはならない
// 実数正方行列の行列式を部分ピボット選択で返す。O(n^3)
template <std::floating_point T>
T real_determinant(matrix<T> a, T eps) {
    assert(a.rows() == a.cols());
    assert(eps >= T{});
    const int size = a.rows();
    T determinant{1};

    // 絶対値最大ピボットによる前進消去で、符号とピボット積を追跡する
    for (int col = 0; col < size; ++col) {
        int pivot_row = col;
        for (int row = col + 1; row < size; ++row) {
            if (std::abs(a[row][col]) > std::abs(a[pivot_row][col])) pivot_row = row;
        }
        if (std::abs(a[pivot_row][col]) <= eps) return T{};
        if (pivot_row != col) {
            a.swap_rows(col, pivot_row);
            determinant = -determinant;
        }

        const T pivot = a[col][col];
        determinant *= pivot;
        for (int row = col + 1; row < size; ++row) {
            const T factor = a[row][col] / pivot;
            a[row][col] = T{};
            for (int target_col = col + 1; target_col < size; ++target_col) {
                a[row][target_col] -= factor * a[col][target_col];
            }
        }
    }
    return determinant;
}

// 想定ユースケース: 実数の線形変換を逆に適用する場合や、同じ係数行列で複数の右辺を解く場合
// 処理概要: [A | I] を部分ピボット付きで RREF 化し、左側が単位行列になれば右側を返す
// 考え方: A へ施した行基本変形を同時に I へ施すと、A が I になった時点で I 側が A^{-1} になる
// 判定: eps の下で rank が n 未満なら数値的に特異とみなし nullopt を返す
// 注意: 得られた逆行列には丸め誤差があるため、高精度が必要なら A*inverse と I の残差を確認する
// 実数正方行列の逆行列を返し、eps の下で特異なら nullopt を返す。O(n^3)
template <std::floating_point T>
std::optional<matrix<T>> real_inverse(matrix<T> a, T eps) {
    assert(a.rows() == a.cols());
    assert(eps >= T{});
    const int size = a.rows();

    // 逆行列計算に必要な部分ピボット付き掃き出しをローカルに持つ
    const auto gauss_jordan = [eps](matrix<T>& target, int pivot_column_count) {
        int rank = 0;
        std::vector<int> pivot_columns;
        pivot_columns.reserve(std::min(target.rows(), pivot_column_count));
        for (int col = 0; col < pivot_column_count && rank < target.rows(); ++col) {
            int pivot_row = rank;
            for (int row = rank + 1; row < target.rows(); ++row) {
                if (std::abs(target[row][col]) > std::abs(target[pivot_row][col])) pivot_row = row;
            }
            if (std::abs(target[pivot_row][col]) <= eps) continue;
            target.swap_rows(rank, pivot_row);

            const T pivot = target[rank][col];
            for (int target_col = col; target_col < target.cols(); ++target_col) {
                target[rank][target_col] /= pivot;
            }
            target[rank][col] = T{1};
            for (int row = 0; row < target.rows(); ++row) {
                if (row == rank || std::abs(target[row][col]) <= eps) {
                    if (row != rank) target[row][col] = T{};
                    continue;
                }
                const T factor = target[row][col];
                for (int target_col = col; target_col < target.cols(); ++target_col) {
                    target[row][target_col] -= factor * target[rank][target_col];
                    if (std::abs(target[row][target_col]) <= eps) target[row][target_col] = T{};
                }
                target[row][col] = T{};
            }
            pivot_columns.push_back(col);
            ++rank;
        }
        return real_gauss_jordan_result<T>{rank, std::move(pivot_columns)};
    };

    // [A | I] を構築して左半分だけをピボット候補として掃き出す
    matrix<T> augmented(size, size * 2, T{});
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) augmented[row][col] = a[row][col];
        augmented[row][size + row] = T{1};
    }
    const auto elimination = gauss_jordan(augmented, size);
    if (elimination.rank != size) return std::nullopt;

    matrix<T> inverse(size, size, T{});
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) inverse[row][col] = augmented[row][size + col];
    }
    return inverse;
}

enum class real_linear_system_status {
    no_solution,
    unique_solution,
    multiple_solutions
};

template <std::floating_point T>
struct real_linear_system_result {
    real_linear_system_status status;
    int rank;
    std::vector<T> particular_solution;
    matrix<T> kernel_basis;
};

// 想定ユースケース: 実数係数の連立方程式で、解の有無・一意性・自由度までまとめて判定する場合
// 処理概要: [A | b] を部分ピボット付きで RREF 化し、矛盾行、特殊解、斉次解基底を順に構築する
// 考え方: 自由変数を 0 とした値を特殊解とし、各自由変数を 1 にした解を ker(A) の基底とする
// eps の意味: 係数と定数の絶対値が eps 以下なら 0 とみなすため、入力スケールに応じた指定が必要
// 注意: status は数値的な判定であり、悪条件な入力では数学的な解の分類と異なる可能性がある
// 実数上で Ax=b を解き、eps に基づく特殊解と斉次解空間の基底を返す。O(H * W * min(H, W))
template <std::floating_point T>
real_linear_system_result<T> real_solve_linear_system(
    matrix<T> coefficient,
    const std::vector<T>& constant,
    T eps) {
    assert(coefficient.rows() == std::ssize(constant));
    assert(eps >= T{});
    const int equation_count = coefficient.rows();
    const int variable_count = coefficient.cols();

    // この solver 内で完結する部分ピボット付きガウス・ジョルダン消去
    const auto gauss_jordan = [eps](matrix<T>& target, int pivot_column_count) {
        int rank = 0;
        std::vector<int> pivot_columns;
        pivot_columns.reserve(std::min(target.rows(), pivot_column_count));
        for (int col = 0; col < pivot_column_count && rank < target.rows(); ++col) {
            int pivot_row = rank;
            for (int row = rank + 1; row < target.rows(); ++row) {
                if (std::abs(target[row][col]) > std::abs(target[pivot_row][col])) pivot_row = row;
            }
            if (std::abs(target[pivot_row][col]) <= eps) continue;
            target.swap_rows(rank, pivot_row);

            const T pivot = target[rank][col];
            for (int target_col = col; target_col < target.cols(); ++target_col) {
                target[rank][target_col] /= pivot;
            }
            target[rank][col] = T{1};
            for (int row = 0; row < target.rows(); ++row) {
                if (row == rank || std::abs(target[row][col]) <= eps) {
                    if (row != rank) target[row][col] = T{};
                    continue;
                }
                const T factor = target[row][col];
                for (int target_col = col; target_col < target.cols(); ++target_col) {
                    target[row][target_col] -= factor * target[rank][target_col];
                    if (std::abs(target[row][target_col]) <= eps) target[row][target_col] = T{};
                }
                target[row][col] = T{};
            }
            pivot_columns.push_back(col);
            ++rank;
        }
        return real_gauss_jordan_result<T>{rank, std::move(pivot_columns)};
    };

    // 拡大係数行列を構築し、係数列だけをピボット候補として RREF 化する
    matrix<T> augmented(equation_count, variable_count + 1, T{});
    for (int row = 0; row < equation_count; ++row) {
        for (int col = 0; col < variable_count; ++col) augmented[row][col] = coefficient[row][col];
        augmented[row][variable_count] = constant[row];
    }
    const auto elimination = gauss_jordan(augmented, variable_count);

    // 係数が eps 以下で定数だけが有意な行を矛盾として検出する
    for (int row = 0; row < equation_count; ++row) {
        bool all_zero = true;
        for (int col = 0; col < variable_count; ++col) {
            if (std::abs(augmented[row][col]) > eps) {
                all_zero = false;
                break;
            }
        }
        if (all_zero && std::abs(augmented[row][variable_count]) > eps) {
            return {
                real_linear_system_status::no_solution,
                elimination.rank,
                {},
                matrix<T>(0, variable_count, T{})};
        }
    }

    // 自由変数を 0 とした特殊解を構築する
    std::vector<T> particular(variable_count, T{});
    for (int row = 0; row < elimination.rank; ++row) {
        const int pivot_col = elimination.pivot_columns[row];
        particular[pivot_col] = augmented[row][variable_count];
    }

    // 各自由変数に対応する斉次解基底を構築する
    std::vector<bool> is_pivot(variable_count, false);
    for (const int pivot_col : elimination.pivot_columns) is_pivot[pivot_col] = true;
    const int free_count = variable_count - elimination.rank;
    matrix<T> kernel_basis(free_count, variable_count, T{});
    int basis_row = 0;
    for (int free_col = 0; free_col < variable_count; ++free_col) {
        if (is_pivot[free_col]) continue;
        kernel_basis[basis_row][free_col] = T{1};
        for (int row = 0; row < elimination.rank; ++row) {
            const int pivot_col = elimination.pivot_columns[row];
            kernel_basis[basis_row][pivot_col] = -augmented[row][free_col];
        }
        ++basis_row;
    }

    const real_linear_system_status status =
        free_count == 0 ? real_linear_system_status::unique_solution
                        : real_linear_system_status::multiple_solutions;
    return {status, elimination.rank, std::move(particular), std::move(kernel_basis)};
}

// ==================== 実数上の掃き出し法 ここまで ====================

// ==================== 整数・任意 mod の行列式 ここから ====================
// コピー依存: matrix<T>

// 想定ユースケース: 整数行列の行列式や、行列木定理による全域木数を剰余を取らず厳密に求める場合
// 処理概要: 分数なしガウス消去で、各更新を直前のピボットで正確に割りながら次数を 1 ずつ下げる
// 考え方: Bareiss 更新では数学的に除算が割り切れ、中間値が通常の有理数消去より抑えられる
// 型の前提: Int は加減乗除と零比較を持ち、途中値と最終行列式を表現できる整数型であること
// 注意: long long や __int128_t でも中間値が範囲を超える場合があり、任意精度整数には対応しない
// 境界: 0 次正方行列の行列式は 1 を返す
// 整数環上で Bareiss 法により正確な行列式を返す。O(n^3) 回の整数演算
template <class Int>
Int integer_determinant_bareiss(matrix<Int> a) {
    assert(a.rows() == a.cols());
    const int size = a.rows();
    if (size == 0) return Int{1};
    Int previous_pivot{1};
    Int sign{1};

    // 分数を発生させない Bareiss 更新を、必要に応じて行交換しながら進める
    for (int pivot_index = 0; pivot_index + 1 < size; ++pivot_index) {
        int pivot_row = pivot_index;
        while (pivot_row < size && a[pivot_row][pivot_index] == Int{}) ++pivot_row;
        if (pivot_row == size) return Int{};
        if (pivot_row != pivot_index) {
            a.swap_rows(pivot_index, pivot_row);
            sign = Int{} - sign;
        }

        const Int pivot = a[pivot_index][pivot_index];
        for (int row = pivot_index + 1; row < size; ++row) {
            for (int col = pivot_index + 1; col < size; ++col) {
                a[row][col] =
                    (a[row][col] * pivot - a[row][pivot_index] * a[pivot_index][col]) /
                    previous_pivot;
            }
            a[row][pivot_index] = Int{};
        }
        previous_pivot = pivot;
    }
    return sign * a[size - 1][size - 1];
}

// 想定ユースケース: modulus が素数とは限らず、非零ピボットの逆元が存在しない可能性がある行列式計算
// 処理概要: Euclid 型の行操作と行交換を繰り返して上三角化し、最後に対角積と符号を求める
// 考え方: pivot 行から lower 行の整数倍を引く操作は行列式を変えず、互除法のように対象列を 0 へできる
// 安全性: 乗算・減算の中間値は __int128_t で計算し、各操作後に [0, modulus) へ正規化する
// 注意: modulus は正でなければならず、modulus == 1 では全要素が 0 なので答えも 0 になる
// 合成数を含む正の modulus 上で逆元を使わずに行列式を返す。O(n^3 log modulus)
inline long long determinant_mod_any(matrix<long long> a, long long modulus) {
    assert(a.rows() == a.cols());
    assert(modulus > 0);
    if (modulus == 1) return 0;
    const int size = a.rows();

    const auto normalize = [modulus](__int128_t value) {
        value %= modulus;
        if (value < 0) value += modulus;
        return static_cast<long long>(value);
    };
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) a[row][col] = normalize(a[row][col]);
    }

    bool negative = false;
    // Euclid 型の行基本変形で各列の下側を 0 にし、逆元なしで上三角化する
    for (int col = 0; col < size; ++col) {
        for (int row = col + 1; row < size; ++row) {
            while (a[row][col] != 0) {
                const long long quotient = a[col][col] / a[row][col];
                for (int target_col = col; target_col < size; ++target_col) {
                    a[col][target_col] = normalize(
                        a[col][target_col] -
                        static_cast<__int128_t>(quotient) * a[row][target_col]);
                }
                a.swap_rows(col, row);
                negative = !negative;
            }
        }
        if (a[col][col] == 0) return 0;
    }

    // 上三角行列の対角積へ行交換の符号を反映する
    long long determinant = 1 % modulus;
    for (int index = 0; index < size; ++index) {
        determinant = normalize(static_cast<__int128_t>(determinant) * a[index][index]);
    }
    if (negative && determinant != 0) determinant = modulus - determinant;
    return determinant;
}

// ==================== 整数・任意 mod の行列式 ここまで ====================

// ==================== 行列木定理 ここから ====================
// コピー依存: matrix<T>

// 想定ユースケース: 無向グラフの全域木数、または各木の辺重み積の総和を素数 mod などで求める場合
// 処理概要: 重み付き Laplacian 行列を作り、任意の 1 行 1 列を除いた余因子の行列式を計算する
// 考え方: Kirchhoff の行列木定理により、その余因子は全域木ごとの辺重み積の総和に一致する
// 辺の扱い: 多重辺は寄与を加算し、自己ループは全域木に使われないため無視する
// 型の前提: T は厳密な体であり、通常の個数なら各辺重みを T{1} とする
// 重み付き無向グラフの全域木重み積総和を厳密な体上で返す。O(V^3 + E)
template <class T>
T count_undirected_spanning_trees(
    int vertex_count,
    const std::vector<std::tuple<int, int, T>>& weighted_edges) {
    assert(vertex_count >= 1);
    if (vertex_count == 1) return T{1};

    // 余因子の行列式計算を solver 内へ置き、単体コピー時の依存を matrix<T> だけにする
    const auto determinant = [](matrix<T> target) {
        const int size = target.rows();
        T result{1};
        for (int col = 0; col < size; ++col) {
            int pivot_row = col;
            while (pivot_row < size && target[pivot_row][col] == T{}) ++pivot_row;
            if (pivot_row == size) return T{};
            if (pivot_row != col) {
                target.swap_rows(col, pivot_row);
                result = T{} - result;
            }
            const T pivot = target[col][col];
            result *= pivot;
            for (int row = col + 1; row < size; ++row) {
                if (target[row][col] == T{}) continue;
                const T factor = target[row][col] / pivot;
                target[row][col] = T{};
                for (int target_col = col + 1; target_col < size; ++target_col) {
                    target[row][target_col] -= factor * target[col][target_col];
                }
            }
        }
        return result;
    };

    matrix<T> laplacian(vertex_count, vertex_count, T{});
    for (const auto& [vertex_a, vertex_b, weight] : weighted_edges) {
        assert(0 <= vertex_a && vertex_a < vertex_count);
        assert(0 <= vertex_b && vertex_b < vertex_count);
        if (vertex_a == vertex_b) continue;
        laplacian[vertex_a][vertex_a] += weight;
        laplacian[vertex_b][vertex_b] += weight;
        laplacian[vertex_a][vertex_b] -= weight;
        laplacian[vertex_b][vertex_a] -= weight;
    }

    matrix<T> minor(vertex_count - 1, vertex_count - 1, T{});
    for (int row = 0; row + 1 < vertex_count; ++row) {
        for (int col = 0; col + 1 < vertex_count; ++col) minor[row][col] = laplacian[row][col];
    }
    return determinant(std::move(minor));
}

// 想定ユースケース: 各頂点から辺の向きに沿って root へ到達する有向全域木の個数・重み総和を求める場合
// 処理概要: from 側の対角へ重みを加える out-degree Laplacian を作り、root の行と列を除いた余因子を取る
// 考え方: root 以外の各頂点が外向き辺を 1 本選び、最終的に root へ収束する木を有向行列木定理で数える
// 辺の扱い: タプルは from,to,weight の順で、多重辺は加算し、自己ループは無視する
// 型の前提: T は厳密な体を使用する
// root へ向かう重み付き有向全域木の重み積総和を厳密な体上で返す。O(V^3 + E)
template <class T>
T count_directed_in_arborescences(
    int vertex_count,
    int root,
    const std::vector<std::tuple<int, int, T>>& weighted_edges) {
    assert(vertex_count >= 1);
    assert(0 <= root && root < vertex_count);
    if (vertex_count == 1) return T{1};

    // 有向 Laplacian の余因子を評価する行列式処理をローカルに持つ
    const auto determinant = [](matrix<T> target) {
        const int size = target.rows();
        T result{1};
        for (int col = 0; col < size; ++col) {
            int pivot_row = col;
            while (pivot_row < size && target[pivot_row][col] == T{}) ++pivot_row;
            if (pivot_row == size) return T{};
            if (pivot_row != col) {
                target.swap_rows(col, pivot_row);
                result = T{} - result;
            }
            const T pivot = target[col][col];
            result *= pivot;
            for (int row = col + 1; row < size; ++row) {
                if (target[row][col] == T{}) continue;
                const T factor = target[row][col] / pivot;
                target[row][col] = T{};
                for (int target_col = col + 1; target_col < size; ++target_col) {
                    target[row][target_col] -= factor * target[col][target_col];
                }
            }
        }
        return result;
    };

    matrix<T> laplacian(vertex_count, vertex_count, T{});
    for (const auto& [from, to, weight] : weighted_edges) {
        assert(0 <= from && from < vertex_count);
        assert(0 <= to && to < vertex_count);
        if (from == to) continue;
        laplacian[from][from] += weight;
        laplacian[from][to] -= weight;
    }

    matrix<T> minor(vertex_count - 1, vertex_count - 1, T{});
    int minor_row = 0;
    for (int row = 0; row < vertex_count; ++row) {
        if (row == root) continue;
        int minor_col = 0;
        for (int col = 0; col < vertex_count; ++col) {
            if (col == root) continue;
            minor[minor_row][minor_col] = laplacian[row][col];
            ++minor_col;
        }
        ++minor_row;
    }
    return determinant(std::move(minor));
}

// 想定ユースケース: root から辺の向きに沿って全頂点へ到達する有向全域木を数える場合
// 処理概要: 全辺の向きを反転し、関数内 lambda で root へ向かう有向全域木を数える
// 考え方: root 外向き木の各辺を反転すると、重みを保ったまま root 内向き木と 1 対 1 に対応する
// 型の前提: T は厳密な体を使用し、辺タプルの重みは反転しても変更しない
// root から外へ向かう重み付き有向全域木の重み積総和を厳密な体上で返す。O(V^3 + E)
template <class T>
T count_directed_out_arborescences(
    int vertex_count,
    int root,
    const std::vector<std::tuple<int, int, T>>& weighted_edges) {
    assert(vertex_count >= 1);
    assert(0 <= root && root < vertex_count);

    // 反転後の辺から root 向き有向全域木を数える処理を、この solver 内の lambda として持つ
    const auto count_in_arborescences = [&](
        const std::vector<std::tuple<int, int, T>>& edges) {
        if (vertex_count == 1) return T{1};

        const auto determinant = [](matrix<T> target) {
            const int size = target.rows();
            T result{1};
            for (int col = 0; col < size; ++col) {
                int pivot_row = col;
                while (pivot_row < size && target[pivot_row][col] == T{}) ++pivot_row;
                if (pivot_row == size) return T{};
                if (pivot_row != col) {
                    target.swap_rows(col, pivot_row);
                    result = T{} - result;
                }
                const T pivot = target[col][col];
                result *= pivot;
                for (int row = col + 1; row < size; ++row) {
                    if (target[row][col] == T{}) continue;
                    const T factor = target[row][col] / pivot;
                    target[row][col] = T{};
                    for (int target_col = col + 1; target_col < size; ++target_col) {
                        target[row][target_col] -= factor * target[col][target_col];
                    }
                }
            }
            return result;
        };

        matrix<T> laplacian(vertex_count, vertex_count, T{});
        for (const auto& [from, to, weight] : edges) {
            assert(0 <= from && from < vertex_count);
            assert(0 <= to && to < vertex_count);
            if (from == to) continue;
            laplacian[from][from] += weight;
            laplacian[from][to] -= weight;
        }

        matrix<T> minor(vertex_count - 1, vertex_count - 1, T{});
        int minor_row = 0;
        for (int row = 0; row < vertex_count; ++row) {
            if (row == root) continue;
            int minor_col = 0;
            for (int col = 0; col < vertex_count; ++col) {
                if (col == root) continue;
                minor[minor_row][minor_col] = laplacian[row][col];
                ++minor_col;
            }
            ++minor_row;
        }
        return determinant(std::move(minor));
    };

    std::vector<std::tuple<int, int, T>> reversed;
    reversed.reserve(weighted_edges.size());
    for (const auto& [from, to, weight] : weighted_edges) reversed.emplace_back(to, from, weight);
    return count_in_arborescences(reversed);
}

// 想定ユースケース: 全域木数や整数重み付き総和を剰余なしで正確に得たい小規模グラフ
// 処理概要: 整数 Laplacian の余因子を作り、Bareiss 法で分数を発生させず行列式を求める
// 考え方: Kirchhoff の行列木定理自体は整数上で成り立つため、体上の除算を使わない行列式へ置き換える
// 注意: 答えだけでなく Bareiss 法の中間値も Int の範囲内である必要がある
// 辺の扱い: 多重辺は加算し、自己ループは無視する
// 重み付き無向グラフの全域木重み積総和を整数上で返す。O(V^3 + E) 回の整数演算
template <class Int>
Int count_undirected_spanning_trees_integer(
    int vertex_count,
    const std::vector<std::tuple<int, int, Int>>& weighted_edges) {
    assert(vertex_count >= 1);
    if (vertex_count == 1) return Int{1};

    // Bareiss 法を solver 内へ置き、整数行列式 solver の別コピーを不要にする
    const auto determinant_bareiss = [](matrix<Int> target) {
        const int size = target.rows();
        if (size == 0) return Int{1};
        Int previous_pivot{1};
        Int sign{1};
        for (int pivot_index = 0; pivot_index + 1 < size; ++pivot_index) {
            int pivot_row = pivot_index;
            while (pivot_row < size && target[pivot_row][pivot_index] == Int{}) ++pivot_row;
            if (pivot_row == size) return Int{};
            if (pivot_row != pivot_index) {
                target.swap_rows(pivot_index, pivot_row);
                sign = Int{} - sign;
            }
            const Int pivot = target[pivot_index][pivot_index];
            for (int row = pivot_index + 1; row < size; ++row) {
                for (int col = pivot_index + 1; col < size; ++col) {
                    target[row][col] =
                        (target[row][col] * pivot -
                         target[row][pivot_index] * target[pivot_index][col]) /
                        previous_pivot;
                }
                target[row][pivot_index] = Int{};
            }
            previous_pivot = pivot;
        }
        return sign * target[size - 1][size - 1];
    };

    matrix<Int> laplacian(vertex_count, vertex_count, Int{});
    for (const auto& [vertex_a, vertex_b, weight] : weighted_edges) {
        assert(0 <= vertex_a && vertex_a < vertex_count);
        assert(0 <= vertex_b && vertex_b < vertex_count);
        if (vertex_a == vertex_b) continue;
        laplacian[vertex_a][vertex_a] += weight;
        laplacian[vertex_b][vertex_b] += weight;
        laplacian[vertex_a][vertex_b] -= weight;
        laplacian[vertex_b][vertex_a] -= weight;
    }

    matrix<Int> minor(vertex_count - 1, vertex_count - 1, Int{});
    for (int row = 0; row + 1 < vertex_count; ++row) {
        for (int col = 0; col + 1 < vertex_count; ++col) minor[row][col] = laplacian[row][col];
    }
    return determinant_bareiss(std::move(minor));
}

// 想定ユースケース: 全域木数を合成数を含む任意の法で求め、素数法の逆元を仮定できない場合
// 処理概要: Laplacian の各更新を modulus で正規化し、関数内 lambda で余因子の行列式を求める
// 考え方: 逆元なしの Euclid 型行列式をローカルに持ち、合成数 mod でも同じ行列木定理を適用する
// 安全性: 辺重みの加減算は __int128_t を介して正規化し、long long の符号付きオーバーフローを避ける
// 注意: modulus == 1 では答えは 0、頂点数 1 では空の全域木を 1 と数えて法を取る
// 重み付き無向グラフの全域木重み積総和を任意の正の modulus で返す。O(V^3 log modulus + E)
inline long long count_undirected_spanning_trees_mod_any(
    int vertex_count,
    const std::vector<std::tuple<int, int, long long>>& weighted_edges,
    long long modulus) {
    assert(vertex_count >= 1);
    assert(modulus > 0);
    if (modulus == 1) return 0;
    if (vertex_count == 1) return 1 % modulus;

    const auto normalize = [modulus](__int128_t value) {
        value %= modulus;
        if (value < 0) value += modulus;
        return static_cast<long long>(value);
    };

    // 逆元を使わない任意 mod 行列式を solver 内へ置く
    const auto determinant_mod = [modulus](matrix<long long> target) {
        const auto normalize_local = [modulus](__int128_t value) {
            value %= modulus;
            if (value < 0) value += modulus;
            return static_cast<long long>(value);
        };
        const int size = target.rows();
        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) {
                target[row][col] = normalize_local(target[row][col]);
            }
        }

        bool negative = false;
        for (int col = 0; col < size; ++col) {
            for (int row = col + 1; row < size; ++row) {
                while (target[row][col] != 0) {
                    const long long quotient = target[col][col] / target[row][col];
                    for (int target_col = col; target_col < size; ++target_col) {
                        target[col][target_col] = normalize_local(
                            target[col][target_col] -
                            static_cast<__int128_t>(quotient) * target[row][target_col]);
                    }
                    target.swap_rows(col, row);
                    negative = !negative;
                }
            }
            if (target[col][col] == 0) return 0LL;
        }

        long long result = 1 % modulus;
        for (int index = 0; index < size; ++index) {
            result = normalize_local(static_cast<__int128_t>(result) * target[index][index]);
        }
        if (negative && result != 0) result = modulus - result;
        return result;
    };

    matrix<long long> laplacian(vertex_count, vertex_count, 0);
    for (const auto& [vertex_a, vertex_b, raw_weight] : weighted_edges) {
        assert(0 <= vertex_a && vertex_a < vertex_count);
        assert(0 <= vertex_b && vertex_b < vertex_count);
        if (vertex_a == vertex_b) continue;
        const long long weight = normalize(raw_weight);
        laplacian[vertex_a][vertex_a] = normalize(
            static_cast<__int128_t>(laplacian[vertex_a][vertex_a]) + weight);
        laplacian[vertex_b][vertex_b] = normalize(
            static_cast<__int128_t>(laplacian[vertex_b][vertex_b]) + weight);
        laplacian[vertex_a][vertex_b] = normalize(
            static_cast<__int128_t>(laplacian[vertex_a][vertex_b]) - weight);
        laplacian[vertex_b][vertex_a] = normalize(
            static_cast<__int128_t>(laplacian[vertex_b][vertex_a]) - weight);
    }

    matrix<long long> minor(vertex_count - 1, vertex_count - 1, 0);
    for (int row = 0; row + 1 < vertex_count; ++row) {
        for (int col = 0; col + 1 < vertex_count; ++col) minor[row][col] = laplacian[row][col];
    }
    return determinant_mod(std::move(minor));
}

// ==================== 行列木定理 ここまで ====================

// ==================== matrix<T> solver 群 ここまで ====================

// ============================================================================
// fixed_matrix<T, N>: 小さい固定サイズ正方行列
// ============================================================================

// ==================== fixed_matrix<T, N> データ構造 ここから ====================
// コピー依存: <bits/stdc++.h>

// 小さい固定サイズ正方行列
template <class T, int N>
struct fixed_matrix {
private:
    static_assert(N > 0, "fixed_matrix のサイズは正でなければならない");
    static_assert(!std::is_same_v<std::remove_cv_t<T>, bool>,
                  "bool 行列には bit_matrix を使用する");

    std::array<T, N * N> data_{};

public:
    // 全要素を値初期化して構築する。O(N^2)
    fixed_matrix() = default;

    // 全要素を value で初期化する。O(N^2)
    explicit fixed_matrix(const T& value) { data_.fill(value); }

    // 行数を返す。O(1)
    static constexpr int rows() { return N; }

    // 列数を返す。O(1)
    static constexpr int cols() { return N; }

    // 連続領域の先頭ポインタを返す。O(1)
    T* data() { return data_.data(); }

    // 連続領域の先頭ポインタを返す。O(1)
    const T* data() const { return data_.data(); }

    // row 行目の先頭ポインタを返す。O(1)
    T* operator[](int row) {
        assert(0 <= row && row < N);
        return data_.data() + row * N;
    }

    // row 行目の先頭ポインタを返す。O(1)
    const T* operator[](int row) const {
        assert(0 <= row && row < N);
        return data_.data() + row * N;
    }

    // N 次単位行列を返す。O(N^2)
    static fixed_matrix identity() {
        fixed_matrix result;
        for (int index = 0; index < N; ++index) result[index][index] = T{1};
        return result;
    }
};

// ==================== fixed_matrix<T, N> データ構造 ここまで ====================

// ==================== fixed_matrix<T, N> 公開関数群 ここから ====================
// コピー依存: fixed_matrix<T, N>

// 2 行列を要素ごとに加算する。O(N^2)
template <class T, int N>
fixed_matrix<T, N> operator+(const fixed_matrix<T, N>& lhs, const fixed_matrix<T, N>& rhs) {
    fixed_matrix<T, N> result;
    constexpr std::size_t size = N * N;
    for (std::size_t index = 0; index < size; ++index) result.data()[index] = lhs.data()[index] + rhs.data()[index];
    return result;
}

// 2 行列を要素ごとに減算する。O(N^2)
template <class T, int N>
fixed_matrix<T, N> operator-(const fixed_matrix<T, N>& lhs, const fixed_matrix<T, N>& rhs) {
    fixed_matrix<T, N> result;
    constexpr std::size_t size = N * N;
    for (std::size_t index = 0; index < size; ++index) result.data()[index] = lhs.data()[index] - rhs.data()[index];
    return result;
}

// lhs と rhs の行列積を返す。O(N^3)
template <class T, int N>
fixed_matrix<T, N> operator*(const fixed_matrix<T, N>& lhs, const fixed_matrix<T, N>& rhs) {
    fixed_matrix<T, N> result;

    // 内側を連続アクセスにして、固定長ループの展開をコンパイラへ促す
    for (int row = 0; row < N; ++row) {
        T* const result_row = result[row];
        const T* const lhs_row = lhs[row];
        for (int middle = 0; middle < N; ++middle) {
            const T value = lhs_row[middle];
            const T* const rhs_row = rhs[middle];
            for (int col = 0; col < N; ++col) result_row[col] += value * rhs_row[col];
        }
    }
    return result;
}

// 行列を列ベクトルへ適用する。O(N^2)
template <class T, int N>
std::array<T, N> matrix_apply(
    const fixed_matrix<T, N>& a,
    const std::array<T, static_cast<std::size_t>(N)>& vector) {
    std::array<T, N> result{};
    for (int row = 0; row < N; ++row) {
        T value{};
        for (int col = 0; col < N; ++col) value += a[row][col] * vector[col];
        result[row] = value;
    }
    return result;
}

// 正方行列の非負整数乗を返す。O(N^3 log exponent)
template <class T, int N>
fixed_matrix<T, N> matrix_power(fixed_matrix<T, N> base, std::uint64_t exponent) {
    fixed_matrix<T, N> result = fixed_matrix<T, N>::identity();
    while (exponent != 0) {
        if ((exponent & 1U) != 0U) result = result * base;
        exponent >>= 1U;
        if (exponent != 0) base = base * base;
    }
    return result;
}

// ==================== fixed_matrix<T, N> 公開関数群 ここまで ====================

// ==================== fixed_matrix<T, N> solver 群 ここから ====================

// ==================== アフィン変換の反復 ここから ====================
// コピー依存: fixed_matrix<T, N>, matrix_power, matrix_apply

// 想定ユースケース: 座標の回転・平行移動、定数項を持つ漸化式、同一のアフィン操作を大量反復する場合
// 処理概要: 末尾成分を 1 とする同次座標へ変換し、Dimension+1 次の通常行列として累乗する
// 考え方: [[linear, shift], [0, 1]] を掛けると、線形変換と平行移動を 1 回の行列積で同時に表せる
// 注意: 列ベクトル規約であり、count == 0 では point をそのまま返す
// x -> linear * x + shift を count 回適用する。O(Dimension^3 log count)
template <class T, int Dimension>
std::array<T, Dimension> repeat_affine_transform(
    const fixed_matrix<T, Dimension>& linear,
    const std::array<T, static_cast<std::size_t>(Dimension)>& shift,
    std::uint64_t count,
    const std::array<T, static_cast<std::size_t>(Dimension)>& point) {
    static_assert(Dimension > 0, "アフィン変換の次元は正でなければならない");

    // 同次座標を 1 次元追加し、アフィン変換を通常の行列積へ変換する
    fixed_matrix<T, Dimension + 1> homogeneous;
    for (int row = 0; row < Dimension; ++row) {
        for (int col = 0; col < Dimension; ++col) homogeneous[row][col] = linear[row][col];
        homogeneous[row][Dimension] = shift[row];
    }
    homogeneous[Dimension][Dimension] = T{1};

    // 末尾を 1 とした点へ累乗済み変換を適用する
    std::array<T, Dimension + 1> state{};
    for (int index = 0; index < Dimension; ++index) state[index] = point[index];
    state[Dimension] = T{1};
    const auto transformed = matrix_apply(matrix_power(homogeneous, count), state);

    std::array<T, Dimension> result{};
    for (int index = 0; index < Dimension; ++index) result[index] = transformed[index];
    return result;
}

// ==================== アフィン変換の反復 ここまで ====================

// ==================== fixed_matrix<T, N> solver 群 ここまで ====================

// ============================================================================
// semiring_matrix<T, Zero, One, Add, Mul>: 演算をコンパイル時指定する汎用半環行列
// ============================================================================

// ==================== semiring_matrix データ構造 ここから ====================
// コピー依存: <bits/stdc++.h>
//
// 使用例:
// constexpr int add(int a, int b) { return std::max(a, b); }
// constexpr int mul(int a, int b) { return std::min(a, b); }
// using mat = semiring_matrix<int, -1, 1'000'000'000, add, mul>;
//
// Zero と One には T へ変換できる値、または T を返す引数なし関数を渡す
// Add と Mul にはそれぞれ T add(T, T)、T mul(T, T) として呼べる関数を渡す
// 半環則、零元の吸収則、演算結果の値域は呼び出し側が保証する

// 行優先の連続領域に格納し、零元・単位元・加算・乗算をテンプレート引数で指定する動的半環行列
template <class T, auto Zero, auto One, auto Add, auto Mul>
struct semiring_matrix {
private:
    static_assert(!std::is_same_v<std::remove_cv_t<T>, bool>,
                  "bool 要素には std::uint8_t または bit_matrix を使用する");

    int height_ = 0;
    int width_ = 0;
    std::vector<T> data_;

public:
    // 0 × 0 行列を構築する。O(1)
    semiring_matrix() = default;

    // height × width 行列を半環の零元で初期化する。O(height * width)
    semiring_matrix(int height, int width)
        : height_(height), width_(width) {
        assert(height >= 0);
        assert(width >= 0);
        data_.assign(static_cast<std::size_t>(height) * width, zero());
    }

    // height × width 行列を value で初期化する。O(height * width)
    semiring_matrix(int height, int width, const T& value)
        : height_(height), width_(width) {
        assert(height >= 0);
        assert(width >= 0);
        data_.assign(static_cast<std::size_t>(height) * width, value);
    }

    // 行数を返す。O(1)
    int rows() const { return height_; }

    // 列数を返す。O(1)
    int cols() const { return width_; }

    // 要素が 1 個も存在しないなら true を返す。O(1)
    bool empty() const { return height_ == 0 || width_ == 0; }

    // 連続領域の先頭ポインタを返す。O(1)
    T* data() { return data_.data(); }

    // 連続領域の先頭ポインタを返す。O(1)
    const T* data() const { return data_.data(); }

    // row 行目の先頭ポインタを返す。O(1)
    T* operator[](int row) {
        assert(0 <= row && row < height_);
        if (width_ == 0) return data_.data();
        return data_.data() + static_cast<std::size_t>(row) * width_;
    }

    // row 行目の先頭ポインタを返す。O(1)
    const T* operator[](int row) const {
        assert(0 <= row && row < height_);
        if (width_ == 0) return data_.data();
        return data_.data() + static_cast<std::size_t>(row) * width_;
    }

    // 半環の零元を返す。O(1)
    static T zero() {
        if constexpr (std::invocable<decltype(Zero)>) {
            return Zero();
        } else {
            return static_cast<T>(Zero);
        }
    }

    // 半環の乗法単位元を返す。O(1)
    static T one() {
        if constexpr (std::invocable<decltype(One)>) {
            return One();
        } else {
            return static_cast<T>(One);
        }
    }

    // 半環の加算 Add(lhs, rhs) を返す。O(1)
    static T add(const T& lhs, const T& rhs) {
        return Add(lhs, rhs);
    }

    // 半環の乗算 Mul(lhs, rhs) を返す。O(1)
    static T multiply(const T& lhs, const T& rhs) {
        return Mul(lhs, rhs);
    }

    // size 次単位行列を返す。O(size^2)
    static semiring_matrix identity(int size) {
        assert(size >= 0);
        semiring_matrix result(size, size);
        for (int index = 0; index < size; ++index) result[index][index] = one();
        return result;
    }
};

// ==================== semiring_matrix データ構造 ここまで ====================

// ==================== semiring_matrix 公開関数群 ここから ====================
// コピー依存: semiring_matrix

// 2 行列を半環の加算で要素ごとに足す。O(rows * cols)
template <class T, auto Zero, auto One, auto Add, auto Mul>
semiring_matrix<T, Zero, One, Add, Mul> operator+(
    const semiring_matrix<T, Zero, One, Add, Mul>& lhs,
    const semiring_matrix<T, Zero, One, Add, Mul>& rhs) {
    assert(lhs.rows() == rhs.rows());
    assert(lhs.cols() == rhs.cols());
    using matrix_type = semiring_matrix<T, Zero, One, Add, Mul>;
    matrix_type result(lhs.rows(), lhs.cols());
    const std::size_t size = static_cast<std::size_t>(lhs.rows()) * lhs.cols();
    for (std::size_t index = 0; index < size; ++index) {
        result.data()[index] = matrix_type::add(lhs.data()[index], rhs.data()[index]);
    }
    return result;
}

// lhs と rhs の半環行列積を返す。O(lhs.rows * lhs.cols * rhs.cols)
template <class T, auto Zero, auto One, auto Add, auto Mul>
semiring_matrix<T, Zero, One, Add, Mul> operator*(
    const semiring_matrix<T, Zero, One, Add, Mul>& lhs,
    const semiring_matrix<T, Zero, One, Add, Mul>& rhs) {
    assert(lhs.cols() == rhs.rows());
    using matrix_type = semiring_matrix<T, Zero, One, Add, Mul>;
    matrix_type result(lhs.rows(), rhs.cols());

    // 内側の列方向を連続アクセスし、各内積を Add と Mul だけで評価する
    for (int row = 0; row < lhs.rows(); ++row) {
        T* const result_row = result[row];
        const T* const lhs_row = lhs[row];
        for (int middle = 0; middle < lhs.cols(); ++middle) {
            const T lhs_value = lhs_row[middle];
            const T* const rhs_row = rhs[middle];
            for (int col = 0; col < rhs.cols(); ++col) {
                result_row[col] = matrix_type::add(
                    result_row[col],
                    matrix_type::multiply(lhs_value, rhs_row[col]));
            }
        }
    }
    return result;
}

// 半環行列を列ベクトルへ適用する。O(rows * cols)
template <class T, auto Zero, auto One, auto Add, auto Mul>
std::vector<T> semiring_matrix_apply(
    const semiring_matrix<T, Zero, One, Add, Mul>& a,
    const std::vector<T>& vector) {
    assert(a.cols() == std::ssize(vector));
    using matrix_type = semiring_matrix<T, Zero, One, Add, Mul>;
    std::vector<T> result(a.rows(), matrix_type::zero());
    for (int row = 0; row < a.rows(); ++row) {
        T value = matrix_type::zero();
        for (int col = 0; col < a.cols(); ++col) {
            value = matrix_type::add(
                value,
                matrix_type::multiply(a[row][col], vector[col]));
        }
        result[row] = std::move(value);
    }
    return result;
}

// 正方半環行列の非負整数乗を返す。O(n^3 log exponent)
template <class T, auto Zero, auto One, auto Add, auto Mul>
semiring_matrix<T, Zero, One, Add, Mul> semiring_matrix_power(
    semiring_matrix<T, Zero, One, Add, Mul> base,
    std::uint64_t exponent) {
    using matrix_type = semiring_matrix<T, Zero, One, Add, Mul>;
    assert(base.rows() == base.cols());
    matrix_type result = matrix_type::identity(base.rows());
    while (exponent != 0) {
        if ((exponent & 1U) != 0U) result = result * base;
        exponent >>= 1U;
        if (exponent != 0) base = base * base;
    }
    return result;
}

// 正方半環行列の非負整数乗を列ベクトルへ適用する。O(n^3 log exponent + n^2 popcount(exponent))
template <class T, auto Zero, auto One, auto Add, auto Mul>
std::vector<T> semiring_matrix_power_apply(
    semiring_matrix<T, Zero, One, Add, Mul> base,
    std::uint64_t exponent,
    std::vector<T> vector) {
    assert(base.rows() == base.cols());
    assert(base.cols() == std::ssize(vector));
    while (exponent != 0) {
        if ((exponent & 1U) != 0U) vector = semiring_matrix_apply(base, vector);
        exponent >>= 1U;
        if (exponent != 0) base = base * base;
    }
    return vector;
}

template <class T, auto Zero, auto One, auto Add, auto Mul>
struct semiring_matrix_power_prefix_sum_result {
    semiring_matrix<T, Zero, One, Add, Mul> power;
    semiring_matrix<T, Zero, One, Add, Mul> prefix_sum;
};

// A^count と I + A + ... + A^(count-1) を半環上で返す。O(n^3 log count)
template <class T, auto Zero, auto One, auto Add, auto Mul>
semiring_matrix_power_prefix_sum_result<T, Zero, One, Add, Mul>
semiring_matrix_power_prefix_sum(
    semiring_matrix<T, Zero, One, Add, Mul> base,
    std::uint64_t count) {
    using matrix_type = semiring_matrix<T, Zero, One, Add, Mul>;
    using result_type = semiring_matrix_power_prefix_sum_result<T, Zero, One, Add, Mul>;
    assert(base.rows() == base.cols());
    const int size = base.rows();
    result_type result{matrix_type::identity(size), matrix_type(size, size)};
    result_type block{std::move(base), matrix_type::identity(size)};

    // 連続する区間の累乗と前方和を半環の積・和で結合する
    const auto combine = [](const result_type& left, const result_type& right) {
        return result_type{
            left.power * right.power,
            left.prefix_sum + left.power * right.prefix_sum};
    };
    while (count != 0) {
        if ((count & 1U) != 0U) result = combine(result, block);
        count >>= 1U;
        if (count != 0) block = combine(block, block);
    }
    return result;
}

// ==================== semiring_matrix 公開関数群 ここまで ====================

// ==================== semiring_matrix solver 群 ここから ====================
// コピー依存: semiring_matrix とその公開関数群

// 想定ユースケース: 通常の経路数、min-plus、max-plus、boolean、max-min などを同じ遷移 DP として解く場合
// 処理概要: transition[to][from] に 1 ステップの重みを Add で集約し、初期状態へ遷移行列を K 回適用する
// 考え方: 行列積の Add が経由状態の候補統合、Mul が連続する遷移の結合を表すため、任意半環の DP を累乗できる
// 入力規約: 同じ from-to の複数遷移は Add でまとめ、initial_state は各開始状態の初期値を直接指定する
// 例: (+,*) の経路和、(min,+) の最短路、(max,min) のボトルネック、独自型での最適値と個数の同時計算に使える
// 初期状態からちょうど step_count 回遷移した各状態の半環 DP 値を返す。O(S^3 log step_count)
template <class T, auto Zero, auto One, auto Add, auto Mul>
std::vector<T> semiring_transition_dp_exactly_k_steps(
    int state_count,
    const std::vector<std::tuple<int, int, T>>& transitions,
    const std::vector<T>& initial_state,
    std::uint64_t step_count) {
    using matrix_type = semiring_matrix<T, Zero, One, Add, Mul>;
    assert(state_count >= 0);
    assert(std::ssize(initial_state) == state_count);

    // 問題固有の遷移列から、列ベクトル規約の遷移行列を構築する
    const auto build_transition = [&]() {
        matrix_type transition(state_count, state_count);
        for (const auto& [from, to, weight] : transitions) {
            assert(0 <= from && from < state_count);
            assert(0 <= to && to < state_count);
            transition[to][from] = matrix_type::add(transition[to][from], weight);
        }
        return transition;
    };

    return semiring_matrix_power_apply(
        build_transition(),
        step_count,
        initial_state);
}

// 想定ユースケース: 0 回から K-1 回までの任意の遷移回数を許し、全長の候補を半環の Add で集約する場合
// 処理概要: I+A+...+A^(K-1) を初期状態へ適用し、各終了時刻の DP 値をまとめて返す
// 考え方: 通常加算なら長さ別の総和、min/max なら最良値、OR ならいずれかの長さでの到達可能性になる
// 境界: step_count_limit == 0 では候補となる遷移回数がないため、全状態が半環の零元になる
// 注意: Add が冪等でない半環では同じ経路を異なる長さとしてそれぞれ加算することに意味があるか確認する
// 初期状態から step_count_limit 未満の遷移回数で得る各状態の半環 DP 値を返す。O(S^3 log step_count_limit)
template <class T, auto Zero, auto One, auto Add, auto Mul>
std::vector<T> semiring_transition_dp_with_fewer_than_k_steps(
    int state_count,
    const std::vector<std::tuple<int, int, T>>& transitions,
    const std::vector<T>& initial_state,
    std::uint64_t step_count_limit) {
    using matrix_type = semiring_matrix<T, Zero, One, Add, Mul>;
    assert(state_count >= 0);
    assert(std::ssize(initial_state) == state_count);

    // 同じ from-to の遷移を Add でまとめた 1 ステップ行列を構築する
    const auto build_transition = [&]() {
        matrix_type transition(state_count, state_count);
        for (const auto& [from, to, weight] : transitions) {
            assert(0 <= from && from < state_count);
            assert(0 <= to && to < state_count);
            transition[to][from] = matrix_type::add(transition[to][from], weight);
        }
        return transition;
    };

    const auto powers = semiring_matrix_power_prefix_sum(
        build_transition(),
        step_count_limit);
    return semiring_matrix_apply(powers.prefix_sum, initial_state);
}

// ==================== semiring_matrix solver 群 ここまで ====================

// ============================================================================
// min_plus_matrix: min-plus 半環の最短路行列
// ============================================================================

// ==================== min_plus_matrix データ構造 ここから ====================
// コピー依存: <bits/stdc++.h>

// 最短路DP用の min-plus 正方行列
struct min_plus_matrix {
    inline static constexpr long long infinity = 4'000'000'000'000'000'000LL;

private:
    int size_ = 0;
    std::vector<long long> data_;

public:
    // 0 次行列を構築する。O(1)
    min_plus_matrix() = default;

    // size 次行列を value で初期化する。O(size^2)
    explicit min_plus_matrix(int size, long long value = infinity)
        : size_(size) {
        assert(size >= 0);
        data_.assign(
            static_cast<std::size_t>(size) * size,
            value);
    }

    // 次数を返す。O(1)
    int size() const { return size_; }

    // 連続領域の先頭ポインタを返す。O(1)
    long long* data() { return data_.data(); }

    // 連続領域の先頭ポインタを返す。O(1)
    const long long* data() const { return data_.data(); }

    // row 行目の先頭ポインタを返す。O(1)
    long long* operator[](int row) {
        assert(0 <= row && row < size_);
        if (size_ == 0) return data_.data();
        return data_.data() + static_cast<std::size_t>(row) * size_;
    }

    // row 行目の先頭ポインタを返す。O(1)
    const long long* operator[](int row) const {
        assert(0 <= row && row < size_);
        if (size_ == 0) return data_.data();
        return data_.data() + static_cast<std::size_t>(row) * size_;
    }

    // 有限値の和を表現範囲へ飽和させ、到達不能を伝播する。O(1)
    static long long saturated_add(long long lhs, long long rhs) {
        if (lhs == infinity || rhs == infinity) return infinity;
        const __int128_t sum = static_cast<__int128_t>(lhs) + rhs;
        if (sum > infinity) return infinity;
        if (sum < -infinity) return -infinity;
        return static_cast<long long>(sum);
    }

    // min-plus 乗法単位行列を返す。O(size^2)
    static min_plus_matrix identity(int size) {
        min_plus_matrix result(size);
        for (int index = 0; index < size; ++index) result[index][index] = 0;
        return result;
    }
};

// ==================== min_plus_matrix データ構造 ここまで ====================

// ==================== min_plus_matrix 公開関数群 ここから ====================
// コピー依存: min_plus_matrix と公開関数群

// 2 行列を要素ごとの最小値で集約する。O(n^2)
inline min_plus_matrix operator+(const min_plus_matrix& lhs, const min_plus_matrix& rhs) {
    assert(lhs.size() == rhs.size());
    min_plus_matrix result(lhs.size());
    const std::size_t count = static_cast<std::size_t>(lhs.size()) * lhs.size();
    for (std::size_t index = 0; index < count; ++index) result.data()[index] = std::min(lhs.data()[index], rhs.data()[index]);
    return result;
}

// 2 行列の min-plus 積を返す。O(n^3)
inline min_plus_matrix operator*(const min_plus_matrix& lhs, const min_plus_matrix& rhs) {
    assert(lhs.size() == rhs.size());
    const int size = lhs.size();
    min_plus_matrix result(size);

    // 到達不能な中間状態を飛ばし、有限値だけを連続走査する
    for (int row = 0; row < size; ++row) {
        long long* const result_row = result[row];
        for (int middle = 0; middle < size; ++middle) {
            const long long left = lhs[row][middle];
            if (left == min_plus_matrix::infinity) continue;
            const long long* const rhs_row = rhs[middle];
            for (int col = 0; col < size; ++col) {
                if (rhs_row[col] == min_plus_matrix::infinity) continue;
                result_row[col] = std::min(result_row[col], min_plus_matrix::saturated_add(left, rhs_row[col]));
            }
        }
    }
    return result;
}

// min-plus 行列を列ベクトルへ適用する。O(n^2)
inline std::vector<long long> matrix_apply(const min_plus_matrix& a, const std::vector<long long>& vector) {
    assert(a.size() == std::ssize(vector));
    std::vector<long long> result(a.size(), min_plus_matrix::infinity);
    for (int row = 0; row < a.size(); ++row) {
        for (int col = 0; col < a.size(); ++col) {
            if (a[row][col] == min_plus_matrix::infinity ||
                vector[col] == min_plus_matrix::infinity) {
                continue;
            }
            result[row] = std::min(
                result[row],
                min_plus_matrix::saturated_add(a[row][col], vector[col]));
        }
    }
    return result;
}

// min-plus 行列の非負整数乗を返す。O(n^3 log exponent)
inline min_plus_matrix matrix_power(min_plus_matrix base, std::uint64_t exponent) {
    min_plus_matrix result = min_plus_matrix::identity(base.size());
    while (exponent != 0) {
        if ((exponent & 1U) != 0U) result = result * base;
        exponent >>= 1U;
        if (exponent != 0) base = base * base;
    }
    return result;
}

// ==================== min_plus_matrix 公開関数群 ここまで ====================

// ==================== min_plus_matrix solver 群 ここから ====================

// ==================== min-plus ユースケース ここから ====================
// コピー依存: min_plus_matrix

// 想定ユースケース: 1 回の遷移コスト行列を非常に多い回数だけ適用し、ちょうどその回数後の最小コストを求める場合
// 処理概要: 加算を min、乗算をコスト加算とする min-plus 半環上で二分累乗を行う
// 考え方: vector[col] を現在コスト、base[row][col] を col から row への追加コストとして最小値を取る
// 注意: 到達不能は infinity で表し、有限値の絶対値は infinity 未満であることを前提とする
// min-plus 行列の非負整数乗を列ベクトルへ適用する。O(n^3 log exponent)
inline std::vector<long long> matrix_power_apply(
    min_plus_matrix base,
    std::uint64_t exponent,
    std::vector<long long> vector) {
    assert(base.size() == std::ssize(vector));
    while (exponent != 0) {
        if ((exponent & 1U) != 0U) vector = matrix_apply(base, vector);
        exponent >>= 1U;
        if (exponent != 0) base = base * base;
    }
    return vector;
}

struct min_plus_power_prefix_result {
    min_plus_matrix power;
    min_plus_matrix prefix_minimum;
};

// 想定ユースケース: ちょうど count 回の最小コストと、count 回未満の最小コストを同時に扱う場合
// 処理概要: power=A^L と prefix_minimum=min_{i=0}^{L-1} A^i の組を min-plus 上で二分合成する
// 考え方: 右区間の経路を使う前に左区間を通るため、前方最小値は min(S_left, P_left*S_right) になる
// 注意: 対象回数は [0, count) で、count == 0 の prefix_minimum は全要素が到達不能になる
// A^count と I min A min ... min A^(count-1) を返す。O(n^3 log count)
inline min_plus_power_prefix_result min_plus_power_prefix(
    min_plus_matrix base,
    std::uint64_t count) {
    const int size = base.size();
    min_plus_power_prefix_result result{
        min_plus_matrix::identity(size),
        min_plus_matrix(size)};
    min_plus_power_prefix_result block{
        std::move(base),
        min_plus_matrix::identity(size)};

    // min-plus 上で連続する 2 区間の累乗と前方最小値を結合する
    const auto combine = [](const min_plus_power_prefix_result& left,
                            const min_plus_power_prefix_result& right) {
        return min_plus_power_prefix_result{
            left.power * right.power,
            left.prefix_minimum + left.power * right.prefix_minimum};
    };

    while (count != 0) {
        if ((count & 1U) != 0U) result = combine(result, block);
        count >>= 1U;
        if (count != 0) block = combine(block, block);
    }
    return result;
}

// 想定ユースケース: 辺数が厳密に K 本という制約付き最短路で、K が非常に大きい場合
// 処理概要: 辺重みを min-plus 遷移行列へ格納し、source の距離 0 の状態へ K 回分の遷移を適用する
// 考え方: min-plus 行列積は中間頂点を選ぶ最短路の結合に一致し、A^K がちょうど K 辺の最短距離を表す
// 注意: 負辺や閉路があっても辺数が固定なので利用でき、多重辺は最小重みだけを残す
// 返り値: 到達不能な頂点は min_plus_matrix::infinity のまま返す
// source から各頂点へのちょうど edge_count 辺の最短距離を返す。O(V^3 log edge_count)
inline std::vector<long long> shortest_distances_exactly_k_edges(
    int vertex_count,
    const std::vector<std::tuple<int, int, long long>>& directed_edges,
    int source,
    std::uint64_t edge_count) {
    assert(vertex_count >= 0);
    assert(0 <= source && source < vertex_count);
    min_plus_matrix transition(vertex_count);
    for (const auto& [from, to, weight] : directed_edges) {
        assert(0 <= from && from < vertex_count);
        assert(0 <= to && to < vertex_count);
        assert(-min_plus_matrix::infinity < weight && weight < min_plus_matrix::infinity);
        transition[to][from] = std::min(transition[to][from], weight);
    }
    std::vector<long long> distance(vertex_count, min_plus_matrix::infinity);
    distance[source] = 0;
    return matrix_power_apply(std::move(transition), edge_count, std::move(distance));
}

// 想定ユースケース: 使用できる辺数に上限がある最短路や、少ない遷移回数も含めて最良値を求める場合
// 処理概要: I min A min ... min A^(edge_count_limit-1) を初期距離ベクトルへ適用する
// 考え方: 各冪がちょうどその辺数の最短距離なので、要素ごとの min が許される辺数全体の最短距離になる
// 注意: edge_count_limit == 1 では 0 辺だけを許し、source のみ 0、その他は到達不能になる
// 境界: edge_count_limit == 0 では許される辺数がないため、全頂点が到達不能になる
// source から各頂点への edge_count_limit 未満の辺数での最短距離を返す。O(V^3 log edge_count_limit)
inline std::vector<long long> shortest_distances_with_fewer_than_k_edges(
    int vertex_count,
    const std::vector<std::tuple<int, int, long long>>& directed_edges,
    int source,
    std::uint64_t edge_count_limit) {
    assert(vertex_count >= 0);
    assert(0 <= source && source < vertex_count);
    min_plus_matrix transition(vertex_count);
    for (const auto& [from, to, weight] : directed_edges) {
        assert(0 <= from && from < vertex_count);
        assert(0 <= to && to < vertex_count);
        assert(-min_plus_matrix::infinity < weight && weight < min_plus_matrix::infinity);
        transition[to][from] = std::min(transition[to][from], weight);
    }
    const auto powers = min_plus_power_prefix(std::move(transition), edge_count_limit);
    std::vector<long long> distance(vertex_count, min_plus_matrix::infinity);
    distance[source] = 0;
    return matrix_apply(powers.prefix_minimum, distance);
}

// ==================== min-plus ユースケース ここまで ====================

// ==================== min_plus_matrix solver 群 ここまで ====================

// ============================================================================
// max_plus_matrix: max-plus 半環の最大得点行列
// ============================================================================

// ==================== max_plus_matrix データ構造 ここから ====================
// コピー依存: <bits/stdc++.h>

// 最大得点DP用の max-plus 正方行列
struct max_plus_matrix {
    inline static constexpr long long negative_infinity = -4'000'000'000'000'000'000LL;
    inline static constexpr long long positive_limit = 4'000'000'000'000'000'000LL;

private:
    int size_ = 0;
    std::vector<long long> data_;

public:
    // 0 次行列を構築する。O(1)
    max_plus_matrix() = default;

    // size 次行列を value で初期化する。O(size^2)
    explicit max_plus_matrix(int size, long long value = negative_infinity)
        : size_(size) {
        assert(size >= 0);
        data_.assign(
            static_cast<std::size_t>(size) * size,
            value);
    }

    // 次数を返す。O(1)
    int size() const { return size_; }

    // 連続領域の先頭ポインタを返す。O(1)
    long long* data() { return data_.data(); }

    // 連続領域の先頭ポインタを返す。O(1)
    const long long* data() const { return data_.data(); }

    // row 行目の先頭ポインタを返す。O(1)
    long long* operator[](int row) {
        assert(0 <= row && row < size_);
        if (size_ == 0) return data_.data();
        return data_.data() + static_cast<std::size_t>(row) * size_;
    }

    // row 行目の先頭ポインタを返す。O(1)
    const long long* operator[](int row) const {
        assert(0 <= row && row < size_);
        if (size_ == 0) return data_.data();
        return data_.data() + static_cast<std::size_t>(row) * size_;
    }

    // 有限値の和を表現範囲へ飽和させ、遷移不能を伝播する。O(1)
    static long long saturated_add(long long lhs, long long rhs) {
        if (lhs == negative_infinity || rhs == negative_infinity) return negative_infinity;
        const __int128_t sum = static_cast<__int128_t>(lhs) + rhs;
        if (sum > positive_limit) return positive_limit;
        if (sum < negative_infinity) return negative_infinity;
        return static_cast<long long>(sum);
    }

    // max-plus 乗法単位行列を返す。O(size^2)
    static max_plus_matrix identity(int size) {
        max_plus_matrix result(size);
        for (int index = 0; index < size; ++index) result[index][index] = 0;
        return result;
    }
};

// ==================== max_plus_matrix データ構造 ここまで ====================

// ==================== max_plus_matrix 公開関数群 ここから ====================
// コピー依存: max_plus_matrix と公開関数群

// 2 行列を要素ごとの最大値で集約する。O(n^2)
inline max_plus_matrix operator+(const max_plus_matrix& lhs, const max_plus_matrix& rhs) {
    assert(lhs.size() == rhs.size());
    max_plus_matrix result(lhs.size());
    const std::size_t count = static_cast<std::size_t>(lhs.size()) * lhs.size();
    for (std::size_t index = 0; index < count; ++index) result.data()[index] = std::max(lhs.data()[index], rhs.data()[index]);
    return result;
}

// 2 行列の max-plus 積を返す。O(n^3)
inline max_plus_matrix operator*(const max_plus_matrix& lhs, const max_plus_matrix& rhs) {
    assert(lhs.size() == rhs.size());
    const int size = lhs.size();
    max_plus_matrix result(size);

    // 遷移不能な中間状態を飛ばし、有限値だけを連続走査する
    for (int row = 0; row < size; ++row) {
        long long* const result_row = result[row];
        for (int middle = 0; middle < size; ++middle) {
            const long long left = lhs[row][middle];
            if (left == max_plus_matrix::negative_infinity) continue;
            const long long* const rhs_row = rhs[middle];
            for (int col = 0; col < size; ++col) {
                if (rhs_row[col] == max_plus_matrix::negative_infinity) continue;
                result_row[col] = std::max(result_row[col], max_plus_matrix::saturated_add(left, rhs_row[col]));
            }
        }
    }
    return result;
}

// max-plus 行列を列ベクトルへ適用する。O(n^2)
inline std::vector<long long> matrix_apply(const max_plus_matrix& a, const std::vector<long long>& vector) {
    assert(a.size() == std::ssize(vector));
    std::vector<long long> result(a.size(), max_plus_matrix::negative_infinity);
    for (int row = 0; row < a.size(); ++row) {
        for (int col = 0; col < a.size(); ++col) {
            if (a[row][col] == max_plus_matrix::negative_infinity ||
                vector[col] == max_plus_matrix::negative_infinity) {
                continue;
            }
            result[row] = std::max(
                result[row],
                max_plus_matrix::saturated_add(a[row][col], vector[col]));
        }
    }
    return result;
}

// max-plus 行列の非負整数乗を返す。O(n^3 log exponent)
inline max_plus_matrix matrix_power(max_plus_matrix base, std::uint64_t exponent) {
    max_plus_matrix result = max_plus_matrix::identity(base.size());
    while (exponent != 0) {
        if ((exponent & 1U) != 0U) result = result * base;
        exponent >>= 1U;
        if (exponent != 0) base = base * base;
    }
    return result;
}

// ==================== max_plus_matrix 公開関数群 ここまで ====================

// ==================== max_plus_matrix solver 群 ここから ====================

// ==================== max-plus ユースケース ここから ====================
// コピー依存: max_plus_matrix

// 想定ユースケース: 1 回ごとの得点付き遷移を非常に多い回数行い、ちょうどその回数後の最大得点を求める場合
// 処理概要: 加算を max、乗算を得点加算とする max-plus 半環上で二分累乗を行う
// 考え方: vector[col] を現在得点、base[row][col] を col から row への追加得点として最大値を取る
// 注意: 遷移不能は negative_infinity で表し、有限値は表現範囲内であることを前提とする
// max-plus 行列の非負整数乗を列ベクトルへ適用する。O(n^3 log exponent)
inline std::vector<long long> matrix_power_apply(
    max_plus_matrix base,
    std::uint64_t exponent,
    std::vector<long long> vector) {
    assert(base.size() == std::ssize(vector));
    while (exponent != 0) {
        if ((exponent & 1U) != 0U) vector = matrix_apply(base, vector);
        exponent >>= 1U;
        if (exponent != 0) base = base * base;
    }
    return vector;
}

struct max_plus_power_prefix_result {
    max_plus_matrix power;
    max_plus_matrix prefix_maximum;
};

// 想定ユースケース: ちょうど count 回の最大得点と、count 回未満で終了してよい最大得点を同時に扱う場合
// 処理概要: power=A^L と prefix_maximum=max_{i=0}^{L-1} A^i の組を max-plus 上で二分合成する
// 考え方: 右区間を選ぶ候補は左区間の累乗を経由するため、max(S_left, P_left*S_right) で結合できる
// 注意: 対象回数は [0, count) で、count == 0 の prefix_maximum は全要素が遷移不能になる
// A^count と I max A max ... max A^(count-1) を返す。O(n^3 log count)
inline max_plus_power_prefix_result max_plus_power_prefix(
    max_plus_matrix base,
    std::uint64_t count) {
    const int size = base.size();
    max_plus_power_prefix_result result{
        max_plus_matrix::identity(size),
        max_plus_matrix(size)};
    max_plus_power_prefix_result block{
        std::move(base),
        max_plus_matrix::identity(size)};

    // max-plus 上で連続する 2 区間の累乗と前方最大値を結合する
    const auto combine = [](const max_plus_power_prefix_result& left,
                            const max_plus_power_prefix_result& right) {
        return max_plus_power_prefix_result{
            left.power * right.power,
            left.prefix_maximum + left.power * right.prefix_maximum};
    };

    while (count != 0) {
        if ((count & 1U) != 0U) result = combine(result, block);
        count >>= 1U;
        if (count != 0) block = combine(block, block);
    }
    return result;
}

// 想定ユースケース: K ターン後、K 個の辺を選んだ後など、遷移回数が厳密に固定された最大化 DP
// 処理概要: transition[to][from] に 1 回の得点を置き、initial_state の得点 0 から K 回進める
// 考え方: max-plus 積が経由状態を選ぶ DP の max と得点加算をそのまま表す
// 注意: 負得点や閉路にも対応し、多重遷移は同じ from-to 間で最大得点だけを残す
// 返り値: 到達不能な状態は max_plus_matrix::negative_infinity のまま返す
// initial_state から各状態へのちょうど step_count 回遷移した最大得点を返す。O(S^3 log step_count)
inline std::vector<long long> maximum_scores_exactly_k_steps(
    int state_count,
    const std::vector<std::tuple<int, int, long long>>& transitions,
    int initial_state,
    std::uint64_t step_count) {
    assert(state_count >= 0);
    assert(0 <= initial_state && initial_state < state_count);
    max_plus_matrix transition(state_count);
    for (const auto& [from, to, score] : transitions) {
        assert(0 <= from && from < state_count);
        assert(0 <= to && to < state_count);
        assert(max_plus_matrix::negative_infinity < score && score < max_plus_matrix::positive_limit);
        transition[to][from] = std::max(transition[to][from], score);
    }
    std::vector<long long> best(state_count, max_plus_matrix::negative_infinity);
    best[initial_state] = 0;
    return matrix_power_apply(std::move(transition), step_count, std::move(best));
}

// 想定ユースケース: 最大 K-1 ターンまでの任意の時点で終了できるゲーム DP や報酬最大化
// 処理概要: I max A max ... max A^(step_count_limit-1) を初期得点ベクトルへ適用する
// 考え方: 各冪がちょうどその回数の最大得点なので、要素ごとの max で終了時刻も同時に選べる
// 注意: step_count_limit > 0 では 0 回遷移も候補となり、initial_state の得点 0 が含まれる
// 境界: step_count_limit == 0 では候補回数がなく、全状態が遷移不能になる
// initial_state から各状態への step_count_limit 未満の遷移回数での最大得点を返す。O(S^3 log step_count_limit)
inline std::vector<long long> maximum_scores_with_fewer_than_k_steps(
    int state_count,
    const std::vector<std::tuple<int, int, long long>>& transitions,
    int initial_state,
    std::uint64_t step_count_limit) {
    assert(state_count >= 0);
    assert(0 <= initial_state && initial_state < state_count);
    max_plus_matrix transition(state_count);
    for (const auto& [from, to, score] : transitions) {
        assert(0 <= from && from < state_count);
        assert(0 <= to && to < state_count);
        assert(max_plus_matrix::negative_infinity < score && score < max_plus_matrix::positive_limit);
        transition[to][from] = std::max(transition[to][from], score);
    }
    const auto powers = max_plus_power_prefix(std::move(transition), step_count_limit);
    std::vector<long long> best(state_count, max_plus_matrix::negative_infinity);
    best[initial_state] = 0;
    return matrix_apply(powers.prefix_maximum, best);
}

// ==================== max-plus ユースケース ここまで ====================

// ==================== max_plus_matrix solver 群 ここまで ====================

// ============================================================================
// bit_matrix: GF(2)・boolean 用ビット圧縮行列
// ============================================================================

// ==================== bit_matrix データ構造 ここから ====================
// コピー依存: <bits/stdc++.h>

// GF(2) と boolean 演算で共有する行優先ビット圧縮行列
struct bit_matrix {
private:
    int height_ = 0;
    int width_ = 0;
    int blocks_per_row_ = 0;
    std::vector<std::uint64_t> data_;

public:
    // 0 × 0 行列を構築する。O(1)
    bit_matrix() = default;

    // height × width のゼロ行列を構築する。O(height * ceil(width / 64))
    bit_matrix(int height, int width)
        : height_(height), width_(width) {
        assert(height >= 0);
        assert(width >= 0);
        blocks_per_row_ = width / 64 + (width % 64 != 0 ? 1 : 0);
        data_.assign(
            static_cast<std::size_t>(height) * blocks_per_row_,
            0);
    }

    // 行数を返す。O(1)
    int rows() const { return height_; }

    // 列数を返す。O(1)
    int cols() const { return width_; }

    // 1 行当たりの 64-bit block 数を返す。O(1)
    int blocks_per_row() const { return blocks_per_row_; }

    // row 行目の block 配列先頭を返す。O(1)
    std::uint64_t* row_data(int row) {
        assert(0 <= row && row < height_);
        if (blocks_per_row_ == 0) return data_.data();
        return data_.data() + static_cast<std::size_t>(row) * blocks_per_row_;
    }

    // row 行目の block 配列先頭を返す。O(1)
    const std::uint64_t* row_data(int row) const {
        assert(0 <= row && row < height_);
        if (blocks_per_row_ == 0) return data_.data();
        return data_.data() + static_cast<std::size_t>(row) * blocks_per_row_;
    }

    // 指定要素を返す。O(1)
    bool get(int row, int col) const {
        assert(0 <= row && row < height_);
        assert(0 <= col && col < width_);
        const int block = col >> 6;
        const int offset = col & 63;
        return ((row_data(row)[block] >> offset) & 1U) != 0U;
    }

    // 指定要素を value に更新する。O(1)
    void set(int row, int col, bool value = true) {
        assert(0 <= row && row < height_);
        assert(0 <= col && col < width_);
        const int block = col >> 6;
        const int offset = col & 63;
        const std::uint64_t mask = std::uint64_t{1} << offset;
        if (value) {
            row_data(row)[block] |= mask;
        } else {
            row_data(row)[block] &= ~mask;
        }
    }

    // 指定要素を反転する。O(1)
    void flip(int row, int col) {
        assert(0 <= row && row < height_);
        assert(0 <= col && col < width_);
        row_data(row)[col >> 6] ^= std::uint64_t{1} << (col & 63);
    }

    // 2 行を交換する。O(ceil(width / 64))
    void swap_rows(int row_a, int row_b) {
        assert(0 <= row_a && row_a < height_);
        assert(0 <= row_b && row_b < height_);
        if (row_a == row_b) return;
        for (int block = 0; block < blocks_per_row_; ++block) {
            std::swap(row_data(row_a)[block], row_data(row_b)[block]);
        }
    }

    // source 行を destination 行へ XOR する。O(ceil(width / 64) - first_block)
    void xor_rows(int destination, int source, int first_block = 0) {
        assert(0 <= destination && destination < height_);
        assert(0 <= source && source < height_);
        assert(0 <= first_block && first_block <= blocks_per_row_);
        std::uint64_t* const destination_row = row_data(destination);
        const std::uint64_t* const source_row = row_data(source);
        for (int block = first_block; block < blocks_per_row_; ++block) destination_row[block] ^= source_row[block];
    }

    // source 行を destination 行へ OR する。O(ceil(width / 64) - first_block)
    void or_rows(int destination, int source, int first_block = 0) {
        assert(0 <= destination && destination < height_);
        assert(0 <= source && source < height_);
        assert(0 <= first_block && first_block <= blocks_per_row_);
        std::uint64_t* const destination_row = row_data(destination);
        const std::uint64_t* const source_row = row_data(source);
        for (int block = first_block; block < blocks_per_row_; ++block) destination_row[block] |= source_row[block];
    }

    // size 次単位行列を返す。O(size)
    static bit_matrix identity(int size) {
        assert(size >= 0);
        bit_matrix result(size, size);
        for (int index = 0; index < size; ++index) result.set(index, index);
        return result;
    }
};

// ==================== bit_matrix データ構造 ここまで ====================

// ==================== bit_matrix 公開関数群 ここから ====================
// コピー依存: bit_matrix

// ---------- GF(2) 行列演算 ----------

// 想定ユースケース: XOR 線形変換の合成、パリティ DP、GF(2) 上の遷移をまとめる場合
// 処理概要: 右辺の列を bit 行へ転置し、左辺の行との AND の popcount 偶奇を各要素にする
// 考え方: GF(2) の内積は積の総和を 2 で割った余りなので、共通する 1 bit 数の偶奇だけ分かればよい
// 高速化: 64 要素を 1 block として AND と popcount で処理し、通常の 3 重ループを bit 並列化する
// 注意: 1 個でも経路があれば真とする boolean 積とは異なり、偶数個なら 0 になる
// GF(2) 上の行列積を返す。O(lhs.rows * rhs.cols * ceil(lhs.cols / 64))
inline bit_matrix gf2_matrix_product(const bit_matrix& lhs, const bit_matrix& rhs) {
    assert(lhs.cols() == rhs.rows());
    bit_matrix transposed(rhs.cols(), rhs.rows());

    // 右辺の各列を連続した bit 列へ転置し、内積を block 単位で計算できる形にする
    for (int row = 0; row < rhs.rows(); ++row) {
        for (int col = 0; col < rhs.cols(); ++col) {
            if (rhs.get(row, col)) transposed.set(col, row);
        }
    }

    // popcount の偶奇を GF(2) 内積として各要素へ格納する
    bit_matrix result(lhs.rows(), rhs.cols());
    for (int row = 0; row < lhs.rows(); ++row) {
        const std::uint64_t* const left_row = lhs.row_data(row);
        for (int col = 0; col < rhs.cols(); ++col) {
            const std::uint64_t* const right_col = transposed.row_data(col);
            int parity = 0;
            for (int block = 0; block < lhs.blocks_per_row(); ++block) {
                parity ^= std::popcount(left_row[block] & right_col[block]) & 1;
            }
            if (parity != 0) result.set(row, col);
        }
    }
    return result;
}

// 想定ユースケース: XOR で表される線形変換を 1 回適用する場合や、GF(2) 累乗の内部処理
// 処理概要: 入力 vector を 64-bit block へ詰め、各行との AND の popcount 偶奇を出力 bit にする
// 考え方: 各出力は a[row][col] と vector[col] の積を XOR した値に等しい
// 注意: vector の各要素は 0 または 1 を前提とし、それ以外は assert で検出する
// GF(2) 行列を 0/1 列ベクトルへ適用する。O(rows * ceil(cols / 64))
inline std::vector<std::uint8_t> gf2_matrix_apply(
    const bit_matrix& a,
    const std::vector<std::uint8_t>& vector) {
    assert(a.cols() == std::ssize(vector));
    std::vector<std::uint64_t> packed(a.blocks_per_row(), 0);
    for (int col = 0; col < a.cols(); ++col) {
        assert(vector[col] <= 1U);
        if (vector[col] != 0U) {
            packed[col >> 6] |= std::uint64_t{1} << (col & 63);
        }
    }

    std::vector<std::uint8_t> result(a.rows(), 0);
    for (int row = 0; row < a.rows(); ++row) {
        int parity = 0;
        const std::uint64_t* const matrix_row = a.row_data(row);
        for (int block = 0; block < a.blocks_per_row(); ++block) {
            parity ^= std::popcount(matrix_row[block] & packed[block]) & 1;
        }
        result[row] = parity != 0;
    }
    return result;
}

// 想定ユースケース: XOR 線形変換や線形フィードバック処理を非常に多い回数繰り返し、変換全体が必要な場合
// 処理概要: gf2_matrix_product を乗算として通常の二分累乗を行う
// 考え方: GF(2) も加法・乗法について行列積の結合則を満たすため、通常の累乗と同じ手順を使える
// 注意: exponent == 0 では GF(2) の単位行列を返す
// GF(2) 正方行列の非負整数乗を返す。O(n^2 ceil(n / 64) log exponent)
inline bit_matrix gf2_matrix_power(bit_matrix base, std::uint64_t exponent) {
    assert(base.rows() == base.cols());
    bit_matrix result = bit_matrix::identity(base.rows());
    while (exponent != 0) {
        if ((exponent & 1U) != 0U) result = gf2_matrix_product(result, base);
        exponent >>= 1U;
        if (exponent != 0) base = gf2_matrix_product(base, base);
    }
    return result;
}

// 想定ユースケース: XOR 遷移を K 回適用した最終状態だけが必要で、A^K 全体を保持する必要がない場合
// 処理概要: 指数の bit が立っているときだけ現在の行列を vector へ適用し、行列側は毎回自乗する
// 考え方: 結果行列との積を行列ベクトル積へ置き換え、選択 bit の処理を軽くする
// 注意: vector は 0/1 値を前提とし、exponent == 0 では入力をそのまま返す
// GF(2) 正方行列の非負整数乗を 0/1 列ベクトルへ適用する。O(n^2 ceil(n / 64) log exponent)
inline std::vector<std::uint8_t> gf2_matrix_power_apply(
    bit_matrix base,
    std::uint64_t exponent,
    std::vector<std::uint8_t> vector) {
    assert(base.rows() == base.cols());
    assert(base.cols() == std::ssize(vector));
    while (exponent != 0) {
        if ((exponent & 1U) != 0U) vector = gf2_matrix_apply(base, vector);
        exponent >>= 1U;
        if (exponent != 0) base = gf2_matrix_product(base, base);
    }
    return vector;
}

struct gf2_gauss_jordan_result {
    int rank;
    std::vector<int> pivot_columns;
};

// 想定ユースケース: XOR 方程式、XOR 基底、GF(2) rank、逆行列、および拡大係数行列の直接操作
// 処理概要: 各列で 1 のある行をピボットに選び、他行へピボット行を XOR して対象列を消去する
// 考え方: GF(2) では非零値が 1 だけなので正規化は不要で、減算も加算も行 XOR になる
// 高速化: 現在列を含む block 以降だけを 64-bit 単位で XOR し、不要な前方 block を触らない
// 副作用: a を破壊的に RREF 化し、pivot_column_count より右の付加列も行操作へ追従させる
// GF(2) 上で先頭 pivot_column_count 列をピボット候補として RREF 化する。O(H * ceil(W / 64) * min(H, pivot_column_count))
inline gf2_gauss_jordan_result gf2_gauss_jordan_inplace(
    bit_matrix& a,
    int pivot_column_count) {
    assert(0 <= pivot_column_count && pivot_column_count <= a.cols());
    int rank = 0;
    std::vector<int> pivot_columns;
    pivot_columns.reserve(std::min(a.rows(), pivot_column_count));

    // 左から順に 1 のある行を選び、行 XOR でピボット列を単位ベクトルにする
    for (int col = 0; col < pivot_column_count && rank < a.rows(); ++col) {
        int pivot_row = rank;
        while (pivot_row < a.rows() && !a.get(pivot_row, col)) ++pivot_row;
        if (pivot_row == a.rows()) continue;
        a.swap_rows(rank, pivot_row);

        const int first_block = col >> 6;
        for (int row = 0; row < a.rows(); ++row) {
            if (row != rank && a.get(row, col)) a.xor_rows(row, rank, first_block);
        }
        pivot_columns.push_back(col);
        ++rank;
    }
    return {rank, std::move(pivot_columns)};
}

// ---------- boolean 行列演算 ----------

// 想定ユースケース: 到達可能性関係の合成、オートマトン遷移、経路の存在だけを調べる場合
// 処理概要: 右辺の列を bit 行へ転置し、左辺行との AND が 1 bit でもあれば結果を 1 にする
// 考え方: boolean 積は OR_k(lhs[i][k] AND rhs[k][j]) であり、共通する中間状態の存在判定に一致する
// 高速化: 64 個の中間状態を一括で AND し、非零 block を見つけた時点で走査を打ち切る
// 注意: 共通 bit 数の偶奇を取る GF(2) 積とは意味が異なる
// boolean 半環上の行列積を返す。O(lhs.rows * rhs.cols * ceil(lhs.cols / 64))
inline bit_matrix boolean_matrix_product(const bit_matrix& lhs, const bit_matrix& rhs) {
    assert(lhs.cols() == rhs.rows());
    bit_matrix transposed(rhs.cols(), rhs.rows());

    // 右辺の各列を連続した bit 列へ転置し、共通 bit の存在判定へ変換する
    for (int row = 0; row < rhs.rows(); ++row) {
        for (int col = 0; col < rhs.cols(); ++col) {
            if (rhs.get(row, col)) transposed.set(col, row);
        }
    }

    bit_matrix result(lhs.rows(), rhs.cols());
    for (int row = 0; row < lhs.rows(); ++row) {
        const std::uint64_t* const left_row = lhs.row_data(row);
        for (int col = 0; col < rhs.cols(); ++col) {
            const std::uint64_t* const right_col = transposed.row_data(col);
            bool reachable = false;
            for (int block = 0; block < lhs.blocks_per_row(); ++block) {
                if ((left_row[block] & right_col[block]) != 0U) {
                    reachable = true;
                    break;
                }
            }
            if (reachable) result.set(row, col);
        }
    }
    return result;
}

// 想定ユースケース: 現在到達可能な状態集合から、1 回の関係適用後に到達できる状態集合を求める場合
// 処理概要: active な入力状態を bit block へ詰め、各行との共通 bit が存在するかを判定する
// 考え方: 出力 row は、a[row][col] が真かつ vector[col] が真となる col が 1 個でもあれば真になる
// 注意: vector の各要素は 0 または 1 を前提とする
// boolean 行列を 0/1 列ベクトルへ適用する。O(rows * ceil(cols / 64))
inline std::vector<std::uint8_t> boolean_matrix_apply(
    const bit_matrix& a,
    const std::vector<std::uint8_t>& vector) {
    assert(a.cols() == std::ssize(vector));
    std::vector<std::uint64_t> packed(a.blocks_per_row(), 0);
    for (int col = 0; col < a.cols(); ++col) {
        assert(vector[col] <= 1U);
        if (vector[col] != 0U) {
            packed[col >> 6] |= std::uint64_t{1} << (col & 63);
        }
    }

    std::vector<std::uint8_t> result(a.rows(), 0);
    for (int row = 0; row < a.rows(); ++row) {
        const std::uint64_t* const matrix_row = a.row_data(row);
        for (int block = 0; block < a.blocks_per_row(); ++block) {
            if ((matrix_row[block] & packed[block]) != 0U) {
                result[row] = 1;
                break;
            }
        }
    }
    return result;
}

// 想定ユースケース: ちょうど K 回の遷移で到達可能かを、全始点・全終点についてまとめて求める場合
// 処理概要: boolean_matrix_product を乗算として二分累乗する
// 考え方: boolean 行列積が到達関係の連結に対応するため、A^K[i][j] は K 回遷移の存在を表す
// 注意: exponent == 0 では 0 回遷移を表す単位行列を返す
// boolean 正方行列の非負整数乗を返す。O(n^2 ceil(n / 64) log exponent)
inline bit_matrix boolean_matrix_power(bit_matrix base, std::uint64_t exponent) {
    assert(base.rows() == base.cols());
    bit_matrix result = bit_matrix::identity(base.rows());
    while (exponent != 0) {
        if ((exponent & 1U) != 0U) result = boolean_matrix_product(result, base);
        exponent >>= 1U;
        if (exponent != 0) base = boolean_matrix_product(base, base);
    }
    return result;
}

// 想定ユースケース: 始点集合が限られており、ちょうど K 回後の到達状態だけが必要な場合
// 処理概要: 指数の bit が立っているときだけ現在の到達関係を状態集合へ適用し、関係側は自乗する
// 考え方: 全組の A^K を最終結果として構築せず、選択 bit では boolean 行列ベクトル積を使う
// 注意: exponent == 0 では入力された状態集合をそのまま返す
// boolean 正方行列の非負整数乗を 0/1 列ベクトルへ適用する。O(n^2 ceil(n / 64) log exponent)
inline std::vector<std::uint8_t> boolean_matrix_power_apply(
    bit_matrix base,
    std::uint64_t exponent,
    std::vector<std::uint8_t> vector) {
    assert(base.rows() == base.cols());
    assert(base.cols() == std::ssize(vector));
    while (exponent != 0) {
        if ((exponent & 1U) != 0U) vector = boolean_matrix_apply(base, vector);
        exponent >>= 1U;
        if (exponent != 0) base = boolean_matrix_product(base, base);
    }
    return vector;
}

// 想定ユースケース: 異なる長さ・条件で得た到達可能性行列を統合する場合や、boolean 前方和の内部処理
// 処理概要: 対応する 64-bit block を OR し、どちらか一方で真の要素を真にする
// 考え方: boolean 半環における行列の加算は要素ごとの OR なので、経路候補集合の和集合に一致する
// 2 個の同サイズ bit 行列を要素ごとの OR で集約する。O(rows * ceil(cols / 64))
inline bit_matrix boolean_matrix_union(const bit_matrix& lhs, const bit_matrix& rhs) {
    assert(lhs.rows() == rhs.rows());
    assert(lhs.cols() == rhs.cols());
    bit_matrix result(lhs.rows(), lhs.cols());
    for (int row = 0; row < lhs.rows(); ++row) {
        std::uint64_t* const result_row = result.row_data(row);
        const std::uint64_t* const left_row = lhs.row_data(row);
        const std::uint64_t* const right_row = rhs.row_data(row);
        for (int block = 0; block < lhs.blocks_per_row(); ++block) {
            result_row[block] = left_row[block] | right_row[block];
        }
    }
    return result;
}

struct boolean_power_prefix_result {
    bit_matrix power;
    bit_matrix prefix_or;
};

// 想定ユースケース: 0 回以上 count 回未満のいずれかの遷移回数で到達可能かを全頂点対で求める場合
// 処理概要: power=A^L と prefix_or=OR_{i=0}^{L-1} A^i の組を boolean 半環上で二分合成する
// 考え方: 右区間の長さを選ぶ経路は左区間の累乗を先に通るため、S_left OR P_left*S_right で結合する
// 注意: 対象範囲は [0, count) で、count == 0 の prefix_or は全要素 false になる
// A^count と I OR A OR ... OR A^(count-1) を返す。O(n^2 ceil(n / 64) log count)
inline boolean_power_prefix_result boolean_power_prefix(
    bit_matrix base,
    std::uint64_t count) {
    assert(base.rows() == base.cols());
    const int size = base.rows();
    boolean_power_prefix_result result{
        bit_matrix::identity(size),
        bit_matrix(size, size)};
    boolean_power_prefix_result block{
        std::move(base),
        bit_matrix::identity(size)};

    // boolean 半環上で連続する 2 区間の累乗と前方 OR を結合する
    const auto combine = [](const boolean_power_prefix_result& left,
                            const boolean_power_prefix_result& right) {
        return boolean_power_prefix_result{
            boolean_matrix_product(left.power, right.power),
            boolean_matrix_union(
                left.prefix_or,
                boolean_matrix_product(left.power, right.prefix_or))};
    };

    while (count != 0) {
        if ((count & 1U) != 0U) result = combine(result, block);
        count >>= 1U;
        if (count != 0) block = combine(block, block);
    }
    return result;
}

// ==================== bit_matrix 公開関数群 ここまで ====================

// ==================== bit_matrix solver 群 ここから ====================
// コピー依存: bit_matrix と必要な公開関数

// ---------- GF(2) 線形代数 solver ----------

// 想定ユースケース: XOR ベクトル集合の独立本数、線形基底の次元、パリティ制約の自由度を求める場合
// 処理概要: 入力をコピーして全列を GF(2) 掃き出しし、得られたピボット数を返す
// 考え方: 各ピボット列が 1 個の独立な方向に対応し、その個数が GF(2) 上の rank になる
// GF(2) 上で行列の rank を返す。O(H * ceil(W / 64) * min(H, W))
inline int gf2_rank(bit_matrix a) {
    // rank solver 単体でコピーできるよう、GF(2) 掃き出しをローカルに持つ
    const auto gauss_jordan = [](bit_matrix& target, int pivot_column_count) {
        int rank = 0;
        std::vector<int> pivot_columns;
        pivot_columns.reserve(std::min(target.rows(), pivot_column_count));
        for (int col = 0; col < pivot_column_count && rank < target.rows(); ++col) {
            int pivot_row = rank;
            while (pivot_row < target.rows() && !target.get(pivot_row, col)) ++pivot_row;
            if (pivot_row == target.rows()) continue;
            target.swap_rows(rank, pivot_row);

            const int first_block = col >> 6;
            for (int row = 0; row < target.rows(); ++row) {
                if (row != rank && target.get(row, col)) {
                    target.xor_rows(row, rank, first_block);
                }
            }
            pivot_columns.push_back(col);
            ++rank;
        }
        return gf2_gauss_jordan_result{rank, std::move(pivot_columns)};
    };
    return gauss_jordan(a, a.cols()).rank;
}

// 想定ユースケース: 可逆な XOR 線形変換を元に戻す場合や、同じ GF(2) 係数行列で多数の右辺を解く場合
// 処理概要: bit_matrix で [A | I] を構築し、左半分を GF(2) 掃き出しして右半分を取り出す
// 考え方: 左側を I にする行 XOR 操作の積が右側へ記録され、その結果が A^{-1} になる
// 返り値: rank が n 未満なら変換は可逆でないため nullopt を返す
// GF(2) 上で正方行列の逆行列を返し、特異なら nullopt を返す。O(n^3 / 64)
inline std::optional<bit_matrix> gf2_inverse(const bit_matrix& a) {
    assert(a.rows() == a.cols());
    const int size = a.rows();

    // 逆行列 solver 内で完結する GF(2) 掃き出し
    const auto gauss_jordan = [](bit_matrix& target, int pivot_column_count) {
        int rank = 0;
        std::vector<int> pivot_columns;
        pivot_columns.reserve(std::min(target.rows(), pivot_column_count));
        for (int col = 0; col < pivot_column_count && rank < target.rows(); ++col) {
            int pivot_row = rank;
            while (pivot_row < target.rows() && !target.get(pivot_row, col)) ++pivot_row;
            if (pivot_row == target.rows()) continue;
            target.swap_rows(rank, pivot_row);

            const int first_block = col >> 6;
            for (int row = 0; row < target.rows(); ++row) {
                if (row != rank && target.get(row, col)) {
                    target.xor_rows(row, rank, first_block);
                }
            }
            pivot_columns.push_back(col);
            ++rank;
        }
        return gf2_gauss_jordan_result{rank, std::move(pivot_columns)};
    };

    bit_matrix augmented(size, size * 2);
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            if (a.get(row, col)) augmented.set(row, col);
        }
        augmented.set(row, size + row);
    }
    const auto elimination = gauss_jordan(augmented, size);
    if (elimination.rank != size) return std::nullopt;

    bit_matrix inverse(size, size);
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            if (augmented.get(row, size + col)) inverse.set(row, col);
        }
    }
    return inverse;
}

enum class gf2_linear_system_status {
    no_solution,
    unique_solution,
    multiple_solutions
};

struct gf2_linear_system_result {
    gf2_linear_system_status status;
    int rank;
    std::vector<std::uint8_t> particular_solution;
    bit_matrix kernel_basis;
};

// 想定ユースケース: スイッチ問題、パリティ条件、XOR 制約で解の存在・個数・一般形まで必要な場合
// 処理概要: [A | b] を bit 行列として RREF 化し、矛盾行、特殊解、自由変数ごとの基底を構築する
// 考え方: 自由変数をすべて 0 とした解を 1 つ取り、各自由変数を 1 にした斉次解を基底にする
// 結果の解釈: 全解は particular_solution と kernel_basis 各行の XOR 組合せで表せる
// 解の個数: 解が存在する場合は 2^(variable_count-rank) 個になる
// 注意: constant の各要素は 0 または 1 を前提とする
// GF(2) 上で Ax=b を解き、特殊解と斉次解空間の基底を返す。O(H * ceil(W / 64) * min(H, W))
inline gf2_linear_system_result gf2_solve_linear_system(
    const bit_matrix& coefficient,
    const std::vector<std::uint8_t>& constant) {
    assert(coefficient.rows() == std::ssize(constant));
    const int equation_count = coefficient.rows();
    const int variable_count = coefficient.cols();

    // XOR 方程式 solver 内で完結する GF(2) 掃き出し
    const auto gauss_jordan = [](bit_matrix& target, int pivot_column_count) {
        int rank = 0;
        std::vector<int> pivot_columns;
        pivot_columns.reserve(std::min(target.rows(), pivot_column_count));
        for (int col = 0; col < pivot_column_count && rank < target.rows(); ++col) {
            int pivot_row = rank;
            while (pivot_row < target.rows() && !target.get(pivot_row, col)) ++pivot_row;
            if (pivot_row == target.rows()) continue;
            target.swap_rows(rank, pivot_row);

            const int first_block = col >> 6;
            for (int row = 0; row < target.rows(); ++row) {
                if (row != rank && target.get(row, col)) {
                    target.xor_rows(row, rank, first_block);
                }
            }
            pivot_columns.push_back(col);
            ++rank;
        }
        return gf2_gauss_jordan_result{rank, std::move(pivot_columns)};
    };

    bit_matrix augmented(equation_count, variable_count + 1);
    for (int row = 0; row < equation_count; ++row) {
        for (int col = 0; col < variable_count; ++col) {
            if (coefficient.get(row, col)) augmented.set(row, col);
        }
        assert(constant[row] <= 1U);
        if (constant[row] != 0U) augmented.set(row, variable_count);
    }
    const auto elimination = gauss_jordan(augmented, variable_count);

    for (int row = 0; row < equation_count; ++row) {
        bool all_zero = true;
        for (int col = 0; col < variable_count; ++col) {
            if (augmented.get(row, col)) {
                all_zero = false;
                break;
            }
        }
        if (all_zero && augmented.get(row, variable_count)) {
            return {
                gf2_linear_system_status::no_solution,
                elimination.rank,
                {},
                bit_matrix(0, variable_count)};
        }
    }

    std::vector<std::uint8_t> particular(variable_count, 0);
    for (int row = 0; row < elimination.rank; ++row) {
        const int pivot_col = elimination.pivot_columns[row];
        particular[pivot_col] = augmented.get(row, variable_count);
    }

    std::vector<std::uint8_t> is_pivot(variable_count, 0);
    for (const int pivot_col : elimination.pivot_columns) is_pivot[pivot_col] = 1;
    const int free_count = variable_count - elimination.rank;
    bit_matrix kernel_basis(free_count, variable_count);
    int basis_row = 0;
    for (int free_col = 0; free_col < variable_count; ++free_col) {
        if (is_pivot[free_col] != 0U) continue;
        kernel_basis.set(basis_row, free_col);
        for (int row = 0; row < elimination.rank; ++row) {
            if (augmented.get(row, free_col)) {
                const int pivot_col = elimination.pivot_columns[row];
                kernel_basis.set(basis_row, pivot_col);
            }
        }
        ++basis_row;
    }

    const gf2_linear_system_status status =
        free_count == 0 ? gf2_linear_system_status::unique_solution
                        : gf2_linear_system_status::multiple_solutions;
    return {status, elimination.rank, std::move(particular), std::move(kernel_basis)};
}

// ---------- boolean 到達可能性 solver ----------

// 想定ユースケース: 辺数が厳密に K 本の経路の存在、オートマトンを K 文字読んだ後の状態集合を求める場合
// 処理概要: transition[to][from] を真にし、source だけが active な列ベクトルへ boolean 累乗を適用する
// 考え方: boolean 行列の K 乗が K 回の関係合成を表すため、結果 vector が到達可能頂点集合になる
// 注意: at most K ではなく exactly K であり、step_count == 0 では source だけが到達可能になる
// 辺の扱い: 多重辺は存在判定では同一なので結果に影響しない
// 有向グラフで source からちょうど step_count 回で到達可能な頂点を返す。O(V^3 log step_count / 64)
inline std::vector<std::uint8_t> reachable_exactly_k_steps(
    int vertex_count,
    const std::vector<std::pair<int, int>>& directed_edges,
    int source,
    std::uint64_t step_count) {
    assert(vertex_count >= 0);
    assert(0 <= source && source < vertex_count);

    // この solver 内でだけ使う boolean 二分累乗のベクトル適用
    const auto power_apply = [](
        bit_matrix base,
        std::uint64_t exponent,
        std::vector<std::uint8_t> state) {
        while (exponent != 0) {
            if ((exponent & 1U) != 0U) state = boolean_matrix_apply(base, state);
            exponent >>= 1U;
            if (exponent != 0) base = boolean_matrix_product(base, base);
        }
        return state;
    };

    bit_matrix transition(vertex_count, vertex_count);
    for (const auto& [from, to] : directed_edges) {
        assert(0 <= from && from < vertex_count);
        assert(0 <= to && to < vertex_count);
        transition.set(to, from);
    }
    std::vector<std::uint8_t> state(vertex_count, 0);
    state[source] = 1;
    return power_apply(std::move(transition), step_count, std::move(state));
}

// 想定ユースケース: 0,1,...,K-1 回のいずれかで到達できる頂点を、K が非常に大きい場合に求める
// 処理概要: I OR A OR ... OR A^(step_count_limit-1) を source の状態ベクトルへ適用する
// 考え方: 各冪がちょうどその回数の到達関係なので、OR により許された全回数をまとめられる
// 注意: step_count_limit > 0 なら 0 回遷移を含み source 自身が真になり、0 なら全頂点 false になる
// 有向グラフで source から step_count_limit 未満の回数で到達可能な頂点を返す。O(V^3 log step_count_limit / 64)
inline std::vector<std::uint8_t> reachable_with_fewer_than_k_steps(
    int vertex_count,
    const std::vector<std::pair<int, int>>& directed_edges,
    int source,
    std::uint64_t step_count_limit) {
    assert(vertex_count >= 0);
    assert(0 <= source && source < vertex_count);

    // [0, count) の boolean 累乗和を求める処理を、この solver 内の lambda として持つ
    const auto power_prefix = [](bit_matrix base, std::uint64_t count) {
        using pair_type = std::pair<bit_matrix, bit_matrix>;
        const int size = base.rows();
        pair_type result{bit_matrix::identity(size), bit_matrix(size, size)};
        pair_type block{std::move(base), bit_matrix::identity(size)};
        const auto combine = [](const pair_type& left, const pair_type& right) {
            return pair_type{
                boolean_matrix_product(left.first, right.first),
                boolean_matrix_union(
                    left.second,
                    boolean_matrix_product(left.first, right.second))};
        };
        while (count != 0) {
            if ((count & 1U) != 0U) result = combine(result, block);
            count >>= 1U;
            if (count != 0) block = combine(block, block);
        }
        return result;
    };

    bit_matrix transition(vertex_count, vertex_count);
    for (const auto& [from, to] : directed_edges) {
        assert(0 <= from && from < vertex_count);
        assert(0 <= to && to < vertex_count);
        transition.set(to, from);
    }
    const auto powers = power_prefix(std::move(transition), step_count_limit);
    std::vector<std::uint8_t> state(vertex_count, 0);
    state[source] = 1;
    return boolean_matrix_apply(powers.second, state);
}

// 想定ユースケース: 全頂点対の到達可能性、部分順序の推移関係、強連結性判定の前処理を行う場合
// 処理概要: 行を始点、列を終点として隣接関係を格納し、Warshall 法の行 OR を 64-bit 単位で行う
// 考え方: from から middle へ到達できるなら、middle から到達できる全頂点を from の行へ追加できる
// 表現上の注意: 他の遷移 solver の transition[to][from] とは異なり、返り値は reachable[from][to] で読む
// reflexive: true なら長さ 0 の到達も含めて対角を立て、false でも閉路があれば対角が立つことがある
// 有向グラフの推移閉包を返す。O(V^3 / 64 + E)
inline bit_matrix directed_transitive_closure(
    int vertex_count,
    const std::vector<std::pair<int, int>>& directed_edges,
    bool reflexive) {
    assert(vertex_count >= 0);
    bit_matrix reachable(vertex_count, vertex_count);
    for (const auto& [from, to] : directed_edges) {
        assert(0 <= from && from < vertex_count);
        assert(0 <= to && to < vertex_count);
        reachable.set(from, to);
    }
    if (reflexive) {
        for (int vertex = 0; vertex < vertex_count; ++vertex) reachable.set(vertex, vertex);
    }

    // middle へ到達できる各行へ middle 行を OR し、Warshall 法を bit 並列化する
    for (int middle = 0; middle < vertex_count; ++middle) {
        for (int from = 0; from < vertex_count; ++from) {
            if (reachable.get(from, middle)) reachable.or_rows(from, middle);
        }
    }
    return reachable;
}

// ==================== bit_matrix solver 群 ここまで ====================

// ============================================================================
// テスト
// ============================================================================

#if __INCLUDE_LEVEL__ == 0

namespace {

[[noreturn]] void test_fail(const std::string& message) {
    throw std::runtime_error(message);
}

void require(bool condition, const std::string& message) {
    if (!condition) test_fail(message);
}

template <class T>
void require_equal(const T& actual, const T& expected, const std::string& message) {
    if (!(actual == expected)) test_fail(message);
}

void require_near(long double actual, long double expected, long double tolerance, const std::string& message) {
    if (std::abs(actual - expected) > tolerance) test_fail(message);
}

struct test_distance_count {
    long long distance;
    long long ways;

    friend bool operator==(const test_distance_count&, const test_distance_count&) = default;
};

constexpr test_distance_count test_distance_count_zero() {
    return {1'000'000'000LL, 0};
}

constexpr test_distance_count test_distance_count_one() {
    return {0, 1};
}

constexpr test_distance_count test_distance_count_add(
    const test_distance_count& lhs,
    const test_distance_count& rhs) {
    if (lhs.distance < rhs.distance) return lhs;
    if (rhs.distance < lhs.distance) return rhs;
    return {lhs.distance, lhs.ways + rhs.ways};
}

constexpr test_distance_count test_distance_count_mul(
    const test_distance_count& lhs,
    const test_distance_count& rhs) {
    if (lhs.ways == 0 || rhs.ways == 0) return test_distance_count_zero();
    return {lhs.distance + rhs.distance, lhs.ways * rhs.ways};
}

constexpr long long test_semiring_sum_add(long long lhs, long long rhs) {
    return lhs + rhs;
}

constexpr long long test_semiring_sum_mul(long long lhs, long long rhs) {
    return lhs * rhs;
}

constexpr int test_semiring_bottleneck_zero() {
    return -1;
}

constexpr int test_semiring_bottleneck_one() {
    return 1'000'000'000;
}

constexpr int test_semiring_bottleneck_add(int lhs, int rhs) {
    return std::max(lhs, rhs);
}

constexpr int test_semiring_bottleneck_mul(int lhs, int rhs) {
    return std::min(lhs, rhs);
}

template <int Modulus>
struct test_modint {
    static_assert(Modulus >= 2);
    int value = 0;

    test_modint() = default;

    test_modint(long long raw_value) {
        raw_value %= Modulus;
        if (raw_value < 0) raw_value += Modulus;
        value = static_cast<int>(raw_value);
    }

    test_modint& operator+=(const test_modint& rhs) {
        value += rhs.value;
        if (value >= Modulus) value -= Modulus;
        return *this;
    }

    test_modint& operator-=(const test_modint& rhs) {
        value -= rhs.value;
        if (value < 0) value += Modulus;
        return *this;
    }

    test_modint& operator*=(const test_modint& rhs) {
        value = static_cast<int>((static_cast<long long>(value) * rhs.value) % Modulus);
        return *this;
    }

    static test_modint power(test_modint base, int exponent) {
        test_modint result{1};
        while (exponent != 0) {
            if ((exponent & 1) != 0) result *= base;
            exponent >>= 1;
            if (exponent != 0) base *= base;
        }
        return result;
    }

    test_modint& operator/=(const test_modint& rhs) {
        assert(rhs.value != 0);
        return *this *= power(rhs, Modulus - 2);
    }

    friend test_modint operator+(test_modint lhs, const test_modint& rhs) { return lhs += rhs; }
    friend test_modint operator-(test_modint lhs, const test_modint& rhs) { return lhs -= rhs; }
    friend test_modint operator*(test_modint lhs, const test_modint& rhs) { return lhs *= rhs; }
    friend test_modint operator/(test_modint lhs, const test_modint& rhs) { return lhs /= rhs; }
    friend test_modint operator-(const test_modint& x) { return test_modint{} - x; }
    friend bool operator==(const test_modint&, const test_modint&) = default;
};

template <class T>
bool matrices_equal(const matrix<T>& lhs, const matrix<T>& rhs) {
    if (lhs.rows() != rhs.rows() || lhs.cols() != rhs.cols()) return false;
    for (int row = 0; row < lhs.rows(); ++row) {
        for (int col = 0; col < lhs.cols(); ++col) {
            if (!(lhs[row][col] == rhs[row][col])) return false;
        }
    }
    return true;
}

bool bit_matrices_equal(const bit_matrix& lhs, const bit_matrix& rhs) {
    if (lhs.rows() != rhs.rows() || lhs.cols() != rhs.cols()) return false;
    for (int row = 0; row < lhs.rows(); ++row) {
        for (int col = 0; col < lhs.cols(); ++col) {
            if (lhs.get(row, col) != rhs.get(row, col)) return false;
        }
    }
    return true;
}

template <class T>
matrix<T> naive_matrix_product(const matrix<T>& lhs, const matrix<T>& rhs) {
    require(lhs.cols() == rhs.rows(), "naive matrix dimensions");
    matrix<T> result(lhs.rows(), rhs.cols(), T{});
    for (int row = 0; row < lhs.rows(); ++row) {
        for (int col = 0; col < rhs.cols(); ++col) {
            for (int middle = 0; middle < lhs.cols(); ++middle) {
                result[row][col] += lhs[row][middle] * rhs[middle][col];
            }
        }
    }
    return result;
}

template <class T>
matrix<T> naive_matrix_power(const matrix<T>& base, int exponent) {
    require(base.rows() == base.cols(), "naive power dimensions");
    matrix<T> result = matrix<T>::identity(base.rows());
    for (int iteration = 0; iteration < exponent; ++iteration) result = naive_matrix_product(result, base);
    return result;
}

template <class T>
T naive_determinant_permutation(const matrix<T>& a) {
    require(a.rows() == a.cols(), "naive determinant dimensions");
    const int size = a.rows();
    std::vector<int> permutation(size);
    std::iota(permutation.begin(), permutation.end(), 0);
    T determinant{};
    do {
        T product{1};
        int inversions = 0;
        for (int row = 0; row < size; ++row) {
            product *= a[row][permutation[row]];
            for (int previous = 0; previous < row; ++previous) {
                if (permutation[previous] >
                    permutation[row]) {
                    ++inversions;
                }
            }
        }
        if ((inversions & 1) == 0) {
            determinant += product;
        } else {
            determinant -= product;
        }
    } while (std::next_permutation(permutation.begin(), permutation.end()));
    return determinant;
}

template <class T>
int naive_rank_by_minors(const matrix<T>& a) {
    const int max_rank = std::min(a.rows(), a.cols());
    for (int candidate = max_rank; candidate >= 1; --candidate) {
        const std::uint64_t row_limit = std::uint64_t{1} << a.rows();
        const std::uint64_t col_limit = std::uint64_t{1} << a.cols();
        for (std::uint64_t row_mask = 0; row_mask < row_limit; ++row_mask) {
            if (std::popcount(row_mask) != candidate) continue;
            for (std::uint64_t col_mask = 0; col_mask < col_limit; ++col_mask) {
                if (std::popcount(col_mask) != candidate) continue;
                matrix<T> minor(candidate, candidate, T{});
                int minor_row = 0;
                for (int row = 0; row < a.rows(); ++row) {
                    if (((row_mask >> row) & 1U) == 0U) continue;
                    int minor_col = 0;
                    for (int col = 0; col < a.cols(); ++col) {
                        if (((col_mask >> col) & 1U) == 0U) continue;
                        minor[minor_row][minor_col] = a[row][col];
                        ++minor_col;
                    }
                    ++minor_row;
                }
                if (naive_determinant_permutation(minor) != T{}) return candidate;
            }
        }
    }
    return 0;
}

long long normalize_mod128(__int128_t value, long long modulus) {
    value %= modulus;
    if (value < 0) value += modulus;
    return static_cast<long long>(value);
}

std::vector<std::vector<std::uint8_t>> unpack_bits(const bit_matrix& a) {
    std::vector<std::vector<std::uint8_t>> result(
        a.rows(),
        std::vector<std::uint8_t>(a.cols(), 0));
    for (int row = 0; row < a.rows(); ++row) {
        for (int col = 0; col < a.cols(); ++col) {
            result[row][col] = a.get(row, col);
        }
    }
    return result;
}

std::vector<std::vector<std::uint8_t>> naive_gf2_product(
    const std::vector<std::vector<std::uint8_t>>& lhs,
    const std::vector<std::vector<std::uint8_t>>& rhs) {
    const int rows = static_cast<int>(lhs.size());
    const int middle_count = rows == 0 ? static_cast<int>(rhs.size()) : static_cast<int>(lhs[0].size());
    const int cols = rhs.empty() ? 0 : static_cast<int>(rhs[0].size());
    std::vector<std::vector<std::uint8_t>> result(
        rows,
        std::vector<std::uint8_t>(cols, 0));
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            std::uint8_t value = 0;
            for (int middle = 0; middle < middle_count; ++middle) {
                value = (value != 0U) !=
                        ((lhs[row][middle] & rhs[middle][col]) != 0U);
            }
            result[row][col] = value;
        }
    }
    return result;
}

std::vector<std::vector<std::uint8_t>> naive_boolean_product(
    const std::vector<std::vector<std::uint8_t>>& lhs,
    const std::vector<std::vector<std::uint8_t>>& rhs) {
    const int rows = static_cast<int>(lhs.size());
    const int middle_count = rows == 0 ? static_cast<int>(rhs.size()) : static_cast<int>(lhs[0].size());
    const int cols = rhs.empty() ? 0 : static_cast<int>(rhs[0].size());
    std::vector<std::vector<std::uint8_t>> result(
        rows,
        std::vector<std::uint8_t>(cols, 0));
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            for (int middle = 0; middle < middle_count; ++middle) {
                if ((lhs[row][middle] &
                     rhs[middle][col]) != 0U) {
                    result[row][col] = 1;
                    break;
                }
            }
        }
    }
    return result;
}

int naive_gf2_rank(std::vector<std::vector<std::uint8_t>> a) {
    const int rows = static_cast<int>(a.size());
    const int cols = rows == 0 ? 0 : static_cast<int>(a[0].size());
    int rank = 0;
    for (int col = 0; col < cols && rank < rows; ++col) {
        int pivot_row = rank;
        while (pivot_row < rows && a[pivot_row][col] == 0U) {
            ++pivot_row;
        }
        if (pivot_row == rows) continue;
        std::swap(a[rank], a[pivot_row]);
        for (int row = 0; row < rows; ++row) {
            if (row == rank || a[row][col] == 0U) continue;
            for (int target_col = col; target_col < cols; ++target_col) {
                a[row][target_col] ^=
                    a[rank][target_col];
            }
        }
        ++rank;
    }
    return rank;
}

void test_matrix_basic_and_power() {
    {
        matrix<long long> zero(0, 0, 0);
        const auto identity = matrix<long long>::identity(0);
        require(matrices_equal(zero, identity), "matrix 0x0 identity");
        require(matrices_equal(matrix_power(zero, 0), identity), "matrix 0x0 power");
    }
    {
        matrix<long long> lhs(2, 3, 0);
        lhs[0][0] = 1;
        lhs[0][1] = 2;
        lhs[0][2] = 3;
        lhs[1][0] = 4;
        lhs[1][1] = 5;
        lhs[1][2] = 6;
        matrix<long long> rhs(3, 2, 0);
        rhs[0][0] = 7;
        rhs[0][1] = 8;
        rhs[1][0] = 9;
        rhs[1][1] = 10;
        rhs[2][0] = 11;
        rhs[2][1] = 12;
        const auto product = lhs * rhs;
        require_equal(product[0][0], 58LL, "matrix known product 00");
        require_equal(product[0][1], 64LL, "matrix known product 01");
        require_equal(product[1][0], 139LL, "matrix known product 10");
        require_equal(product[1][1], 154LL, "matrix known product 11");
        lhs.swap_rows(0, 1);
        require_equal(lhs[0][0], 4LL, "matrix row swap");
    }
    {
        matrix<long long> lhs(2, 2, 0);
        matrix<long long> rhs(2, 2, 0);
        lhs[0][0] = 3;
        lhs[1][1] = -4;
        rhs[0][1] = 5;
        rhs[1][0] = 7;
        const auto sum = lhs + rhs;
        const auto restored = sum - rhs;
        require(matrices_equal(restored, lhs), "matrix addition subtraction");
    }
    {
        matrix<long long> fibonacci(2, 2, 0);
        fibonacci[0][0] = 1;
        fibonacci[0][1] = 1;
        fibonacci[1][0] = 1;
        for (const int exponent : {0, 1, 2, 63, 64, 65}) {
            require(
                matrices_equal(
                    matrix_power(fibonacci, exponent),
                    naive_matrix_power(fibonacci, exponent)),
                "matrix exponent bit boundary");
        }
    }

    std::mt19937_64 rng(0x6d61747269785f31ULL);
    for (int iteration = 0; iteration < 600; ++iteration) {
        const int rows = static_cast<int>(rng() % 6U);
        const int middle = static_cast<int>(rng() % 6U);
        const int cols = static_cast<int>(rng() % 6U);
        matrix<long long> lhs(rows, middle, 0);
        matrix<long long> rhs(middle, cols, 0);
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < middle; ++col) {
                lhs[row][col] = static_cast<long long>(rng() % 9U) - 4;
            }
        }
        for (int row = 0; row < middle; ++row) {
            for (int col = 0; col < cols; ++col) {
                rhs[row][col] = static_cast<long long>(rng() % 9U) - 4;
            }
        }
        require(matrices_equal(lhs * rhs, naive_matrix_product(lhs, rhs)), "matrix random product");
    }

    for (int iteration = 0; iteration < 400; ++iteration) {
        const int size = static_cast<int>(rng() % 5U);
        const int exponent = static_cast<int>(rng() % 8U);
        matrix<long long> base(size, size, 0);
        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) {
                base[row][col] = static_cast<long long>(rng() % 5U) - 2;
            }
        }
        const auto expected_power = naive_matrix_power(base, exponent);
        require(matrices_equal(matrix_power(base, exponent), expected_power),
                "matrix random power");

        std::vector<long long> vector(size, 0);
        for (long long& value : vector) value = static_cast<long long>(rng() % 7U) - 3;
        require_equal(
            matrix_power_apply(base, exponent, vector),
            matrix_apply(expected_power, vector),
            "matrix power apply");

        const auto prefix = matrix_power_prefix_sum(base, exponent);
        matrix<long long> expected_sum(size, size, 0);
        matrix<long long> current = matrix<long long>::identity(size);
        for (int power_index = 0; power_index < exponent; ++power_index) {
            expected_sum += current;
            current = current * base;
        }
        require(matrices_equal(prefix.power, expected_power), "matrix prefix power");
        require(matrices_equal(prefix.prefix_sum, expected_sum), "matrix prefix sum");
    }
}

void test_fixed_matrix_and_affine() {
    std::mt19937_64 rng(0x66697865645f6d31ULL);
    for (int iteration = 0; iteration < 400; ++iteration) {
        fixed_matrix<long long, 3> fixed;
        matrix<long long> dynamic(3, 3, 0);
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                const long long value = static_cast<long long>(rng() % 5U) - 2;
                fixed[row][col] = value;
                dynamic[row][col] = value;
            }
        }
        const int exponent = static_cast<int>(rng() % 8U);
        const auto fixed_power = matrix_power(fixed, exponent);
        const auto dynamic_power = matrix_power(dynamic, exponent);
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                require_equal(fixed_power[row][col], dynamic_power[row][col], "fixed power comparison");
            }
        }

        std::array<long long, 3> vector{};
        for (long long& value : vector) value = static_cast<long long>(rng() % 7U) - 3;
        const auto fixed_sum = fixed + fixed;
        const auto fixed_restored = fixed_sum - fixed;
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                require_equal(fixed_restored[row][col], fixed[row][col], "fixed addition subtraction");
            }
        }

        const auto fixed_applied = matrix_apply(fixed, vector);
        const std::vector<long long> dynamic_vector(vector.begin(), vector.end());
        const auto dynamic_applied = matrix_apply(dynamic, dynamic_vector);
        for (int index = 0; index < 3; ++index) {
            require_equal(
                fixed_applied[index],
                dynamic_applied[index],
                "fixed apply comparison");
        }
    }

    fixed_matrix<long long, 2> linear;
    linear[0][0] = 1;
    linear[0][1] = 1;
    linear[1][1] = 1;
    const std::array<long long, 2> shift{2, -1};
    const std::array<long long, 2> initial{3, 4};
    for (std::uint64_t count = 0; count <= 30; ++count) {
        std::array<long long, 2> expected = initial;
        for (std::uint64_t iteration = 0; iteration < count; ++iteration) {
            expected = {
                expected[0] + expected[1] + shift[0],
                expected[1] + shift[1]};
        }
        require_equal(
            repeat_affine_transform(linear, shift, count, initial),
            expected,
            "affine repetition");
    }
}

void test_general_use_case_solvers() {
    {
        const std::vector<long long> initial{0, 1};
        const std::vector<long long> coefficient{1, 1};
        std::vector<long long> fibonacci(45, 0);
        fibonacci[1] = 1;
        for (std::size_t index = 2; index < fibonacci.size(); ++index) {
            fibonacci[index] = fibonacci[index - 1] + fibonacci[index - 2];
        }
        for (std::size_t index = 0; index < fibonacci.size(); ++index) {
            require_equal(
                linear_recurrence_nth(initial, coefficient, index),
                fibonacci[index],
                "fibonacci recurrence");
        }
    }

    using mint = test_modint<101>;
    std::mt19937_64 rng(0x7573655f63617365ULL);
    for (int iteration = 0; iteration < 350; ++iteration) {
        const int degree = 1 + static_cast<int>(rng() % 5U);
        std::vector<mint> initial(degree);
        std::vector<mint> coefficient(degree);
        for (mint& value : initial) value = mint(rng() % 101U);
        for (mint& value : coefficient) value = mint(rng() % 101U);
        std::vector<mint> sequence(60, mint{});
        for (int index = 0; index < degree; ++index) sequence[index] = initial[index];
        for (std::size_t index = degree; index < sequence.size(); ++index) {
            for (int offset = 0; offset < degree; ++offset) {
                sequence[index] += coefficient[offset] *
                                   sequence[index - 1U - offset];
            }
        }
        const std::size_t index = rng() % sequence.size();
        require_equal(
            linear_recurrence_nth(initial, coefficient, index),
            sequence[index],
            "random linear recurrence");
    }

    for (int iteration = 0; iteration < 350; ++iteration) {
        const int vertex_count = 1 + static_cast<int>(rng() % 6U);
        const int edge_total = static_cast<int>(rng() % 14U);
        std::vector<std::pair<int, int>> edges;
        edges.reserve(edge_total);
        for (int edge_index = 0; edge_index < edge_total; ++edge_index) {
            edges.emplace_back(
                rng() % vertex_count,
                rng() % vertex_count);
        }
        const int source = static_cast<int>(rng() % vertex_count);
        const int destination = static_cast<int>(rng() % vertex_count);
        const int limit = static_cast<int>(rng() % 9U);

        std::vector<mint> state(vertex_count, mint{});
        state[source] = mint{1};
        mint prefix{};
        for (int length = 0; length < limit; ++length) {
            prefix += state[destination];
            std::vector<mint> next(vertex_count, mint{});
            for (const auto& [from, to] : edges) next[to] += state[from];
            state = std::move(next);
        }
        require_equal(
            count_walks_exactly_k<mint>(
                vertex_count,
                edges,
                source,
                destination,
                limit),
            state[destination],
            "walk count exact");
        require_equal(
            count_walks_with_length_less_than<mint>(
                vertex_count,
                edges,
                source,
                destination,
                limit),
            prefix,
            "walk count prefix");
    }

    {
        const std::vector<std::tuple<int, int, long double>> transitions{
            {0, 1, 1.0L},
            {1, 0, 1.0L}};
        const auto distribution = markov_distribution_after_steps(2, transitions, {1.0L, 0.0L}, 17);
        require_near(distribution[0], 0.0L, 1e-18L, "markov alternating state 0");
        require_near(distribution[1], 1.0L, 1e-18L, "markov alternating state 1");
    }
    {
        const std::vector<std::tuple<int, int, long double>> transitions{
            {0, 0, 0.5L}, {0, 1, 0.5L}, {1, 0, 0.25L}, {1, 1, 0.75L}};
        const auto distribution = markov_distribution_after_steps(2, transitions, {1.0L, 0.0L}, 2);
        require_near(distribution[0], 0.375L, 1e-18L, "markov probability state 0");
        require_near(distribution[1], 0.625L, 1e-18L, "markov probability state 1");
    }
}

void test_semiring_matrix_and_solvers() {
    using sum_matrix = semiring_matrix<
        long long,
        0LL,
        1LL,
        test_semiring_sum_add,
        test_semiring_sum_mul>;
    using bottleneck_matrix = semiring_matrix<
        int,
        test_semiring_bottleneck_zero,
        test_semiring_bottleneck_one,
        test_semiring_bottleneck_add,
        test_semiring_bottleneck_mul>;

    require_equal(sum_matrix::zero(), 0LL, "semiring literal zero");
    require_equal(sum_matrix::one(), 1LL, "semiring literal one");
    require_equal(bottleneck_matrix::zero(), -1, "semiring function zero");
    require_equal(bottleneck_matrix::one(), 1'000'000'000, "semiring function one");

    // 長方形積、ベクトル適用、空行列を含むデータ構造の基本動作を確認する
    {
        sum_matrix lhs(2, 3);
        sum_matrix rhs(3, 2);
        long long value = 1;
        for (int row = 0; row < lhs.rows(); ++row) {
            for (int col = 0; col < lhs.cols(); ++col) lhs[row][col] = value++;
        }
        value = 7;
        for (int row = 0; row < rhs.rows(); ++row) {
            for (int col = 0; col < rhs.cols(); ++col) rhs[row][col] = value++;
        }
        const auto product = lhs * rhs;
        require_equal(product[0][0], 58LL, "semiring rectangular product 00");
        require_equal(product[0][1], 64LL, "semiring rectangular product 01");
        require_equal(product[1][0], 139LL, "semiring rectangular product 10");
        require_equal(product[1][1], 154LL, "semiring rectangular product 11");
        require_equal(
            semiring_matrix_apply(lhs, std::vector<long long>{1, 2, 3}),
            std::vector<long long>({14, 32}),
            "semiring rectangular apply");

        const sum_matrix empty(0, 0);
        const auto empty_power = semiring_matrix_power(empty, 17);
        require_equal(empty_power.rows(), 0, "semiring empty power rows");
        require_equal(empty_power.cols(), 0, "semiring empty power cols");
        require_equal(
            (semiring_transition_dp_exactly_k_steps<
                long long,
                0LL,
                1LL,
                test_semiring_sum_add,
                test_semiring_sum_mul>(0, {}, {}, 17)),
            std::vector<long long>{},
            "semiring empty transition DP");
    }

    // 通常の (+,*) 半環で、基本演算・累乗・前方和を愚直計算と比較する
    {
        sum_matrix fibonacci(2, 2);
        fibonacci[0][0] = 1;
        fibonacci[0][1] = 1;
        fibonacci[1][0] = 1;
        const auto power = semiring_matrix_power(fibonacci, 10);
        require_equal(power[0][0], 89LL, "semiring Fibonacci power");
        require_equal(power[0][1], 55LL, "semiring Fibonacci power off diagonal");

        const std::vector<long long> initial{1, 0};
        require_equal(
            semiring_matrix_power_apply(fibonacci, 10, initial),
            std::vector<long long>({89, 55}),
            "semiring Fibonacci apply");

        const auto prefix = semiring_matrix_power_prefix_sum(fibonacci, 6);
        sum_matrix expected_power = sum_matrix::identity(2);
        sum_matrix expected_prefix(2, 2);
        for (int exponent = 0; exponent < 6; ++exponent) {
            expected_prefix = expected_prefix + expected_power;
            expected_power = expected_power * fibonacci;
        }
        for (int row = 0; row < 2; ++row) {
            for (int col = 0; col < 2; ++col) {
                require_equal(prefix.power[row][col], expected_power[row][col], "semiring prefix power");
                require_equal(prefix.prefix_sum[row][col], expected_prefix[row][col], "semiring prefix sum");
            }
        }
    }

    std::mt19937_64 rng(0x5E71A11ULL);

    // 通常加算・乗算の重み付き遷移 DP を、ステップごとの愚直 DP と比較する
    for (int iteration = 0; iteration < 350; ++iteration) {
        const int state_count = 1 + static_cast<int>(rng() % 5U);
        const int edge_count = static_cast<int>(rng() % 16U);
        const int steps = static_cast<int>(rng() % 7U);
        std::vector<std::tuple<int, int, long long>> transitions;
        transitions.reserve(edge_count);
        for (int edge_index = 0; edge_index < edge_count; ++edge_index) {
            transitions.emplace_back(
                rng() % state_count,
                rng() % state_count,
                1 + static_cast<long long>(rng() % 3U));
        }
        std::vector<long long> initial(state_count, 0);
        for (long long& value : initial) value = static_cast<long long>(rng() % 4U);

        std::vector<long long> current = initial;
        std::vector<long long> prefix(state_count, 0);
        for (int step = 0; step < steps; ++step) {
            for (int state = 0; state < state_count; ++state) prefix[state] += current[state];
            std::vector<long long> next(state_count, 0);
            for (const auto& [from, to, weight] : transitions) {
                next[to] += weight * current[from];
            }
            current = std::move(next);
        }

        require_equal(
            (semiring_transition_dp_exactly_k_steps<
                long long,
                0LL,
                1LL,
                test_semiring_sum_add,
                test_semiring_sum_mul>(
                    state_count,
                    transitions,
                    initial,
                    steps)),
            current,
            "semiring sum exact transition DP");
        require_equal(
            (semiring_transition_dp_with_fewer_than_k_steps<
                long long,
                0LL,
                1LL,
                test_semiring_sum_add,
                test_semiring_sum_mul>(
                    state_count,
                    transitions,
                    initial,
                    steps)),
            prefix,
            "semiring sum prefix transition DP");
    }

    // (max,min) 半環で、辺数固定・辺数上限付きの最大ボトルネック値を比較する
    for (int iteration = 0; iteration < 350; ++iteration) {
        const int state_count = 1 + static_cast<int>(rng() % 6U);
        const int edge_count = static_cast<int>(rng() % 20U);
        const int source = static_cast<int>(rng() % state_count);
        const int steps = static_cast<int>(rng() % 8U);
        std::vector<std::tuple<int, int, int>> transitions;
        transitions.reserve(edge_count);
        for (int edge_index = 0; edge_index < edge_count; ++edge_index) {
            transitions.emplace_back(
                rng() % state_count,
                rng() % state_count,
                static_cast<int>(rng() % 21U));
        }

        std::vector<int> initial(state_count, bottleneck_matrix::zero());
        initial[source] = bottleneck_matrix::one();
        std::vector<int> current = initial;
        std::vector<int> prefix(state_count, bottleneck_matrix::zero());
        for (int step = 0; step < steps; ++step) {
            for (int state = 0; state < state_count; ++state) {
                prefix[state] = std::max(prefix[state], current[state]);
            }
            std::vector<int> next(state_count, bottleneck_matrix::zero());
            for (const auto& [from, to, capacity] : transitions) {
                next[to] = std::max(next[to], std::min(current[from], capacity));
            }
            current = std::move(next);
        }

        require_equal(
            (semiring_transition_dp_exactly_k_steps<
                int,
                test_semiring_bottleneck_zero,
                test_semiring_bottleneck_one,
                test_semiring_bottleneck_add,
                test_semiring_bottleneck_mul>(
                    state_count,
                    transitions,
                    initial,
                    steps)),
            current,
            "semiring bottleneck exact transition DP");
        require_equal(
            (semiring_transition_dp_with_fewer_than_k_steps<
                int,
                test_semiring_bottleneck_zero,
                test_semiring_bottleneck_one,
                test_semiring_bottleneck_add,
                test_semiring_bottleneck_mul>(
                    state_count,
                    transitions,
                    initial,
                    steps)),
            prefix,
            "semiring bottleneck prefix transition DP");
    }

    // ユーザー定義型で「最小距離と、その最小距離を達成する経路数」を同時に計算する
    using distance_count_matrix = semiring_matrix<
        test_distance_count,
        test_distance_count_zero,
        test_distance_count_one,
        test_distance_count_add,
        test_distance_count_mul>;
    require_equal(
        distance_count_matrix::zero(),
        test_distance_count({1'000'000'000LL, 0}),
        "semiring user-defined zero");
    require_equal(
        distance_count_matrix::one(),
        test_distance_count({0, 1}),
        "semiring user-defined one");

    for (int iteration = 0; iteration < 200; ++iteration) {
        const int state_count = 1 + static_cast<int>(rng() % 5U);
        const int edge_count = static_cast<int>(rng() % 16U);
        const int source = static_cast<int>(rng() % state_count);
        const int steps = static_cast<int>(rng() % 7U);
        std::vector<std::tuple<int, int, test_distance_count>> transitions;
        transitions.reserve(edge_count);
        for (int edge_index = 0; edge_index < edge_count; ++edge_index) {
            transitions.emplace_back(
                rng() % state_count,
                rng() % state_count,
                test_distance_count{static_cast<long long>(rng() % 6U), 1});
        }

        std::vector<test_distance_count> initial(
            state_count,
            test_distance_count_zero());
        initial[source] = test_distance_count_one();
        std::vector<test_distance_count> current = initial;
        std::vector<test_distance_count> prefix(
            state_count,
            test_distance_count_zero());
        for (int step = 0; step < steps; ++step) {
            for (int state = 0; state < state_count; ++state) {
                prefix[state] = test_distance_count_add(prefix[state], current[state]);
            }
            std::vector<test_distance_count> next(
                state_count,
                test_distance_count_zero());
            for (const auto& [from, to, weight] : transitions) {
                next[to] = test_distance_count_add(
                    next[to],
                    test_distance_count_mul(weight, current[from]));
            }
            current = std::move(next);
        }

        require_equal(
            (semiring_transition_dp_exactly_k_steps<
                test_distance_count,
                test_distance_count_zero,
                test_distance_count_one,
                test_distance_count_add,
                test_distance_count_mul>(
                    state_count,
                    transitions,
                    initial,
                    steps)),
            current,
            "semiring user-defined exact transition DP");
        require_equal(
            (semiring_transition_dp_with_fewer_than_k_steps<
                test_distance_count,
                test_distance_count_zero,
                test_distance_count_one,
                test_distance_count_add,
                test_distance_count_mul>(
                    state_count,
                    transitions,
                    initial,
                    steps)),
            prefix,
            "semiring user-defined prefix transition DP");
    }
}

void test_min_plus_and_max_plus() {
    require_equal(
        min_plus_matrix::saturated_add(min_plus_matrix::infinity, 1),
        min_plus_matrix::infinity,
        "min plus infinity propagation");
    require_equal(
        min_plus_matrix::saturated_add(min_plus_matrix::infinity - 3, 10),
        min_plus_matrix::infinity,
        "min plus positive saturation");
    require_equal(
        min_plus_matrix::saturated_add(-min_plus_matrix::infinity + 3, -10),
        -min_plus_matrix::infinity,
        "min plus negative saturation");
    require_equal(
        max_plus_matrix::saturated_add(max_plus_matrix::negative_infinity, 1),
        max_plus_matrix::negative_infinity,
        "max plus infinity propagation");
    require_equal(
        max_plus_matrix::saturated_add(max_plus_matrix::positive_limit - 3, 10),
        max_plus_matrix::positive_limit,
        "max plus positive saturation");
    {
        min_plus_matrix min_base(1);
        min_base[0][0] = 3;
        max_plus_matrix max_base(1);
        max_base[0][0] = 3;
        for (const std::uint64_t exponent : {std::uint64_t{63}, std::uint64_t{64}, std::uint64_t{65}}) {
            require_equal(matrix_power(min_base, exponent)[0][0], static_cast<long long>(exponent) * 3, "min plus exponent boundary");
            require_equal(matrix_power(max_base, exponent)[0][0], static_cast<long long>(exponent) * 3, "max plus exponent boundary");
        }
    }

    std::mt19937_64 rng(0x6d696e6d61785f31ULL);
    for (int iteration = 0; iteration < 500; ++iteration) {
        const int vertex_count = 1 + static_cast<int>(rng() % 6U);
        const int edge_total = static_cast<int>(rng() % 16U);
        std::vector<std::tuple<int, int, long long>> edges;
        edges.reserve(edge_total);
        for (int edge_index = 0; edge_index < edge_total; ++edge_index) {
            edges.emplace_back(
                rng() % vertex_count,
                rng() % vertex_count,
                static_cast<long long>(rng() % 21U) - 10);
        }
        const int source = static_cast<int>(rng() % vertex_count);
        const int limit = static_cast<int>(rng() % 10U);

        std::vector<long long> min_state(
            vertex_count,
            min_plus_matrix::infinity);
        min_state[source] = 0;
        std::vector<long long> min_prefix(
            vertex_count,
            min_plus_matrix::infinity);
        for (int length = 0; length < limit; ++length) {
            for (int vertex = 0; vertex < vertex_count; ++vertex) {
                min_prefix[vertex] = std::min(
                    min_prefix[vertex],
                    min_state[vertex]);
            }
            std::vector<long long> next(
                vertex_count,
                min_plus_matrix::infinity);
            for (const auto& [from, to, weight] : edges) {
                if (min_state[from] == min_plus_matrix::infinity) continue;
                next[to] = std::min(
                    next[to],
                    min_state[from] + weight);
            }
            min_state = std::move(next);
        }
        require_equal(
            shortest_distances_exactly_k_edges(
                vertex_count,
                edges,
                source,
                limit),
            min_state,
            "min plus exact DP");
        require_equal(
            shortest_distances_with_fewer_than_k_edges(
                vertex_count,
                edges,
                source,
                limit),
            min_prefix,
            "min plus prefix DP");

        std::vector<long long> max_state(
            vertex_count,
            max_plus_matrix::negative_infinity);
        max_state[source] = 0;
        std::vector<long long> max_prefix(
            vertex_count,
            max_plus_matrix::negative_infinity);
        for (int length = 0; length < limit; ++length) {
            for (int vertex = 0; vertex < vertex_count; ++vertex) {
                max_prefix[vertex] = std::max(
                    max_prefix[vertex],
                    max_state[vertex]);
            }
            std::vector<long long> next(
                vertex_count,
                max_plus_matrix::negative_infinity);
            for (const auto& [from, to, score] : edges) {
                if (max_state[from] == max_plus_matrix::negative_infinity) continue;
                next[to] = std::max(
                    next[to],
                    max_state[from] + score);
            }
            max_state = std::move(next);
        }
        require_equal(
            maximum_scores_exactly_k_steps(
                vertex_count,
                edges,
                source,
                limit),
            max_state,
            "max plus exact DP");
        require_equal(
            maximum_scores_with_fewer_than_k_steps(
                vertex_count,
                edges,
                source,
                limit),
            max_prefix,
            "max plus prefix DP");
    }
}

void test_field_linear_algebra() {
    using mint = test_modint<101>;
    {
        matrix<mint> empty(0, 0, mint{});
        require_equal(field_determinant(empty), mint{1}, "field empty determinant");
        require_equal(field_rank(empty), 0, "field empty rank");
        const auto inverse = field_inverse(empty);
        require(inverse.has_value(), "field empty inverse exists");
        require(inverse->rows() == 0 && inverse->cols() == 0, "field empty inverse shape");
    }

    std::mt19937_64 rng(0x6669656c645f6d31ULL);
    for (int iteration = 0; iteration < 450; ++iteration) {
        const int size = static_cast<int>(rng() % 6U);
        matrix<mint> a(size, size, mint{});
        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) {
                a[row][col] = mint(rng() % 101U);
            }
        }
        const mint determinant = naive_determinant_permutation(a);
        require_equal(field_determinant(a), determinant, "field determinant permutation");

        const auto inverse = field_inverse(a);
        require_equal(inverse.has_value(), determinant != mint{}, "field inverse existence");
        if (inverse.has_value()) {
            require(
                matrices_equal(a * *inverse, matrix<mint>::identity(size)),
                "field inverse multiplication");
        }
    }

    for (int iteration = 0; iteration < 350; ++iteration) {
        const int rows = static_cast<int>(rng() % 5U);
        const int cols = static_cast<int>(rng() % 5U);
        matrix<mint> a(rows, cols, mint{});
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                a[row][col] = mint(rng() % 101U);
            }
        }
        require_equal(field_rank(a), naive_rank_by_minors(a), "field rank by minors");
    }

    using small_mint = test_modint<5>;
    for (int iteration = 0; iteration < 500; ++iteration) {
        const int equation_count = static_cast<int>(rng() % 6U);
        const int variable_count = static_cast<int>(rng() % 5U);
        matrix<small_mint> coefficient(equation_count, variable_count, small_mint{});
        std::vector<small_mint> constant(equation_count, small_mint{});
        for (int row = 0; row < equation_count; ++row) {
            for (int col = 0; col < variable_count; ++col) {
                coefficient[row][col] = small_mint(rng() % 5U);
            }
            constant[row] = small_mint(rng() % 5U);
        }

        int candidate_count = 1;
        for (int variable = 0; variable < variable_count; ++variable) candidate_count *= 5;
        int solution_count = 0;
        for (int encoded = 0; encoded < candidate_count; ++encoded) {
            int remaining = encoded;
            std::vector<small_mint> candidate(variable_count, small_mint{});
            for (int variable = 0; variable < variable_count; ++variable) {
                candidate[variable] = small_mint(remaining % 5);
                remaining /= 5;
            }
            if (matrix_apply(coefficient, candidate) == constant) ++solution_count;
        }

        const auto solved = field_solve_linear_system(coefficient, constant);
        if (solution_count == 0) {
            require_equal(
                solved.status,
                field_linear_system_status::no_solution,
                "field solve no solution status");
            require(solved.particular_solution.empty(), "field solve no solution vector");
            require(solved.kernel_basis.rows() == 0, "field solve no solution basis");
            continue;
        }

        const field_linear_system_status expected_status =
            solution_count == 1 ? field_linear_system_status::unique_solution
                                : field_linear_system_status::multiple_solutions;
        require_equal(solved.status, expected_status, "field solve status");
        require_equal(matrix_apply(coefficient, solved.particular_solution), constant, "field special solution");
        require_equal(solved.kernel_basis.rows(), variable_count - solved.rank, "field kernel dimension");
        require_equal(field_rank(solved.kernel_basis), solved.kernel_basis.rows(), "field kernel independence");

        for (int basis_row = 0; basis_row < solved.kernel_basis.rows(); ++basis_row) {
            std::vector<small_mint> basis_vector(variable_count, small_mint{});
            for (int col = 0; col < variable_count; ++col) {
                basis_vector[col] = solved.kernel_basis[basis_row][col];
            }
            require_equal(
                matrix_apply(coefficient, basis_vector),
                std::vector<small_mint>(equation_count, small_mint{}),
                "field homogeneous basis");
        }

        int expected_solution_count = 1;
        for (int basis = 0; basis < solved.kernel_basis.rows(); ++basis) expected_solution_count *= 5;
        require_equal(solution_count, expected_solution_count, "field solution space cardinality");
    }

    {
        matrix<small_mint> coefficient(0, 3, small_mint{});
        const auto solved = field_solve_linear_system(coefficient, {});
        require_equal(solved.status, field_linear_system_status::multiple_solutions, "field zero equations");
        require_equal(solved.rank, 0, "field zero equations rank");
        require(solved.kernel_basis.rows() == 3 && solved.kernel_basis.cols() == 3, "field zero equations basis shape");
    }
    {
        matrix<small_mint> coefficient(1, 0, small_mint{});
        const auto solved = field_solve_linear_system(coefficient, {small_mint{1}});
        require_equal(solved.status, field_linear_system_status::no_solution, "field zero variables contradiction");
    }
}

void test_real_linear_algebra() {
    constexpr long double eps = 1e-12L;
    {
        matrix<long double> a(3, 3, 0.0L);
        a[0][0] = 0.0L;
        a[0][1] = 2.0L;
        a[0][2] = 1.0L;
        a[1][0] = 1.0L;
        a[1][1] = -2.0L;
        a[1][2] = -3.0L;
        a[2][0] = 3.0L;
        a[2][1] = -1.0L;
        a[2][2] = 2.0L;
        require_near(real_determinant(a, eps), -17.0L, 1e-10L, "real known determinant");
        const auto inverse = real_inverse(a, eps);
        require(inverse.has_value(), "real known inverse exists");
        const auto product = a * *inverse;
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                require_near(product[row][col], row == col ? 1.0L : 0.0L, 1e-10L, "real inverse residual");
            }
        }
    }
    {
        matrix<long double> coefficient(2, 2, 0.0L);
        coefficient[0][0] = 1.0L;
        coefficient[0][1] = 1.0L;
        coefficient[1][0] = 2.0L;
        coefficient[1][1] = 2.0L;
        const auto multiple = real_solve_linear_system(coefficient, {3.0L, 6.0L}, eps);
        require_equal(multiple.status, real_linear_system_status::multiple_solutions, "real multiple status");
        require_equal(multiple.kernel_basis.rows(), 1, "real multiple basis dimension");
        const auto impossible = real_solve_linear_system(coefficient, {3.0L, 7.0L}, eps);
        require_equal(impossible.status, real_linear_system_status::no_solution, "real no solution status");
    }

    std::mt19937_64 rng(0x7265616c5f6d6174ULL);
    int accepted = 0;
    while (accepted < 250) {
        const int size = 1 + static_cast<int>(rng() % 5U);
        matrix<long long> integer_matrix(size, size, 0);
        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) {
                integer_matrix[row][col] = static_cast<long long>(rng() % 7U) - 3;
            }
        }
        const long long exact_determinant = naive_determinant_permutation(integer_matrix);
        if (exact_determinant == 0) continue;
        ++accepted;

        matrix<long double> a(size, size, 0.0L);
        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) a[row][col] = integer_matrix[row][col];
        }
        require_near(
            real_determinant(a, eps),
            exact_determinant,
            1e-8L * std::max(1.0L, std::abs(static_cast<long double>(exact_determinant))),
            "real random determinant");

        std::vector<long double> expected_solution(size, 0.0L);
        for (long double& value : expected_solution) value = static_cast<long long>(rng() % 11U) - 5;
        const auto constant = matrix_apply(a, expected_solution);
        const auto solved = real_solve_linear_system(a, constant, eps);
        require_equal(solved.status, real_linear_system_status::unique_solution, "real random unique status");
        for (int index = 0; index < size; ++index) {
            require_near(
                solved.particular_solution[index],
                expected_solution[index],
                1e-8L,
                "real random solution");
        }
    }

    {
        matrix<long double> near_singular(2, 2, 0.0L);
        near_singular[0][0] = 1.0L;
        near_singular[0][1] = 1.0L;
        near_singular[1][0] = 1.0L;
        near_singular[1][1] = 1.0L + 1e-14L;
        require_equal(real_rank(near_singular, 1e-12L), 1, "real eps rank coarse");
        require_equal(real_rank(near_singular, 1e-16L), 2, "real eps rank fine");
    }
}

void test_integer_and_mod_determinants() {
    {
        matrix<long long> empty(0, 0, 0);
        require_equal(integer_determinant_bareiss(empty), 1LL, "integer empty determinant");
        require_equal(determinant_mod_any(empty, 12), 1LL, "mod empty determinant");
    }
    {
        matrix<__int128_t> a(3, 3, 0);
        a[0][0] = 1'000'000'007;
        a[0][1] = -1'000'000'009;
        a[0][2] = 17;
        a[1][0] = 23;
        a[1][1] = 1'000'000'021;
        a[1][2] = -31;
        a[2][0] = 37;
        a[2][1] = 41;
        a[2][2] = 1'000'000'033;
        require_equal(
            integer_determinant_bareiss(a),
            naive_determinant_permutation(a),
            "Bareiss int128");
    }

    const std::array<long long, 9> moduli{1, 2, 4, 6, 8, 9, 12, 97, 1000};
    std::mt19937_64 rng(0x696e746465745f31ULL);
    for (int iteration = 0; iteration < 500; ++iteration) {
        const int size = static_cast<int>(rng() % 7U);
        matrix<long long> a(size, size, 0);
        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) {
                a[row][col] = static_cast<long long>(rng() % 11U) - 5;
            }
        }
        const long long expected = naive_determinant_permutation(a);
        require_equal(integer_determinant_bareiss(a), expected, "Bareiss permutation comparison");
        for (const long long modulus : moduli) {
            require_equal(
                determinant_mod_any(a, modulus),
                normalize_mod128(expected, modulus),
                "arbitrary modulus determinant");
        }
    }

    {
        const long long modulus = 9'223'372'036'854'775'123LL;
        matrix<long long> a(2, 2, 0);
        a[0][0] = std::numeric_limits<long long>::max() - 2;
        a[0][1] = std::numeric_limits<long long>::min() + 7;
        a[1][0] = std::numeric_limits<long long>::max() - 11;
        a[1][1] = std::numeric_limits<long long>::min() + 19;
        const __int128_t determinant =
            static_cast<__int128_t>(a[0][0]) * a[1][1] -
            static_cast<__int128_t>(a[0][1]) * a[1][0];
        require_equal(
            determinant_mod_any(a, modulus),
            normalize_mod128(determinant, modulus),
            "arbitrary modulus large values");
    }
}

struct test_dsu {
    std::vector<int> parent;

    explicit test_dsu(int size) : parent(size, -1) {}

    int leader(int vertex) {
        if (parent[vertex] < 0) return vertex;
        return parent[vertex] = leader(parent[vertex]);
    }

    bool merge(int vertex_a, int vertex_b) {
        vertex_a = leader(vertex_a);
        vertex_b = leader(vertex_b);
        if (vertex_a == vertex_b) return false;
        if (parent[vertex_a] > parent[vertex_b]) {
            std::swap(vertex_a, vertex_b);
        }
        parent[vertex_a] += parent[vertex_b];
        parent[vertex_b] = vertex_a;
        return true;
    }
};

void test_matrix_tree_theorems() {
    const auto brute_undirected = [](
        int vertex_count,
        const std::vector<std::tuple<int, int, long long>>& edges) {
        if (vertex_count == 1) return 1LL;
        long long total = 0;
        const std::uint64_t subset_limit = std::uint64_t{1} << edges.size();
        for (std::uint64_t mask = 0; mask < subset_limit; ++mask) {
            if (std::popcount(mask) != vertex_count - 1) continue;
            test_dsu dsu(vertex_count);
            bool valid = true;
            long long product = 1;
            for (std::size_t edge_index = 0; edge_index < edges.size(); ++edge_index) {
                if (((mask >> edge_index) & 1U) == 0U) continue;
                const auto [vertex_a, vertex_b, weight] = edges[edge_index];
                if (!dsu.merge(vertex_a, vertex_b)) {
                    valid = false;
                    break;
                }
                product *= weight;
            }
            if (!valid) continue;
            const int root = dsu.leader(0);
            for (int vertex = 1; vertex < vertex_count; ++vertex) {
                if (dsu.leader(vertex) != root) valid = false;
            }
            if (valid) total += product;
        }
        return total;
    };

    const auto brute_in_arborescence = [](
        int vertex_count,
        int root,
        const std::vector<std::tuple<int, int, long long>>& edges) {
        if (vertex_count == 1) return 1LL;
        long long total = 0;
        const std::uint64_t subset_limit = std::uint64_t{1} << edges.size();
        for (std::uint64_t mask = 0; mask < subset_limit; ++mask) {
            if (std::popcount(mask) != vertex_count - 1) continue;
            std::vector<int> next(vertex_count, -1);
            long long product = 1;
            bool valid = true;
            for (std::size_t edge_index = 0; edge_index < edges.size(); ++edge_index) {
                if (((mask >> edge_index) & 1U) == 0U) continue;
                const auto [from, to, weight] = edges[edge_index];
                if (from == root || next[from] != -1) {
                    valid = false;
                    break;
                }
                next[from] = to;
                product *= weight;
            }
            if (!valid || next[root] != -1) continue;
            for (int vertex = 0; vertex < vertex_count && valid; ++vertex) {
                if (vertex == root) continue;
                int current = vertex;
                for (int step = 0; step < vertex_count && current != root; ++step) {
                    if (current < 0 || next[current] == -1) {
                        current = -1;
                        break;
                    }
                    current = next[current];
                }
                if (current != root) valid = false;
            }
            if (valid) total += product;
        }
        return total;
    };

    using mint = test_modint<101>;
    std::mt19937_64 rng(0x747265655f746573ULL);
    for (int iteration = 0; iteration < 350; ++iteration) {
        const int vertex_count = 1 + static_cast<int>(rng() % 6U);
        const int edge_total = static_cast<int>(rng() % 10U);
        std::vector<std::tuple<int, int, long long>> integer_edges;
        integer_edges.reserve(edge_total);
        for (int edge_index = 0; edge_index < edge_total; ++edge_index) {
            integer_edges.emplace_back(
                rng() % vertex_count,
                rng() % vertex_count,
                1 + rng() % 4U);
        }
        const long long expected = brute_undirected(vertex_count, integer_edges);
        require_equal(
            count_undirected_spanning_trees_integer(vertex_count, integer_edges),
            expected,
            "undirected matrix tree integer");
        require_equal(
            count_undirected_spanning_trees_mod_any(vertex_count, integer_edges, 12),
            expected % 12,
            "undirected matrix tree composite modulus");

        std::vector<std::tuple<int, int, mint>> field_edges;
        field_edges.reserve(integer_edges.size());
        for (const auto& [vertex_a, vertex_b, weight] : integer_edges) {
            field_edges.emplace_back(vertex_a, vertex_b, mint{weight});
        }
        require_equal(
            count_undirected_spanning_trees(vertex_count, field_edges),
            mint{expected},
            "undirected matrix tree field");
    }

    for (int iteration = 0; iteration < 350; ++iteration) {
        const int vertex_count = 1 + static_cast<int>(rng() % 6U);
        const int root = static_cast<int>(rng() % vertex_count);
        const int edge_total = static_cast<int>(rng() % 10U);
        std::vector<std::tuple<int, int, long long>> integer_edges;
        integer_edges.reserve(edge_total);
        for (int edge_index = 0; edge_index < edge_total; ++edge_index) {
            integer_edges.emplace_back(
                rng() % vertex_count,
                rng() % vertex_count,
                1 + rng() % 4U);
        }
        std::vector<std::tuple<int, int, mint>> field_edges;
        field_edges.reserve(integer_edges.size());
        for (const auto& [from, to, weight] : integer_edges) field_edges.emplace_back(from, to, mint{weight});

        const long long expected_in = brute_in_arborescence(vertex_count, root, integer_edges);
        require_equal(
            count_directed_in_arborescences(vertex_count, root, field_edges),
            mint{expected_in},
            "directed in arborescence");

        std::vector<std::tuple<int, int, long long>> reversed_integer;
        reversed_integer.reserve(integer_edges.size());
        for (const auto& [from, to, weight] : integer_edges) reversed_integer.emplace_back(to, from, weight);
        const long long expected_out = brute_in_arborescence(vertex_count, root, reversed_integer);
        require_equal(
            count_directed_out_arborescences(vertex_count, root, field_edges),
            mint{expected_out},
            "directed out arborescence");
    }
}

void test_bit_matrix_basic() {
    const std::array<int, 9> widths{0, 1, 2, 63, 64, 65, 127, 128, 129};
    for (const int width : widths) {
        constexpr int height = 4;
        bit_matrix bits(height, width);
        std::vector<std::vector<std::uint8_t>> model(
            height,
            std::vector<std::uint8_t>(width, 0));

        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                const bool value = ((row * 131 + col * 17 + 3) % 5) <= 1;
                bits.set(row, col, value);
                model[row][col] = value;
            }
        }
        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                require_equal(
                    bits.get(row, col),
                    model[row][col] != 0U,
                    "bit matrix set/get");
            }
        }

        if (width > 0) {
            bits.flip(1, width - 1);
            model[1][width - 1] ^= 1U;
            bits.set(2, width / 2, false);
            model[2][width / 2] = 0;
        }
        bits.swap_rows(0, 3);
        std::swap(model[0], model[3]);
        bits.xor_rows(1, 0);
        for (int col = 0; col < width; ++col) model[1][col] ^= model[0][col];
        bits.or_rows(2, 1);
        for (int col = 0; col < width; ++col) model[2][col] |= model[1][col];

        require_equal(unpack_bits(bits), model, "bit matrix row operations");
        if (width != 0 && (width & 63) != 0) {
            const int used = width & 63;
            const std::uint64_t unused_mask = ~((std::uint64_t{1} << used) - 1U);
            for (int row = 0; row < height; ++row) {
                require(
                    (bits.row_data(row)[bits.blocks_per_row() - 1] & unused_mask) == 0U,
                    "bit matrix unused bits");
            }
        }
    }

    const auto identity = bit_matrix::identity(130);
    for (int row = 0; row < 130; ++row) {
        for (int col = 0; col < 130; ++col) {
            require_equal(identity.get(row, col), row == col, "bit matrix identity");
        }
    }
}

void test_gf2_operations() {
    std::mt19937_64 rng(0x6766325f6d617472ULL);
    const std::array<int, 7> middle_sizes{0, 1, 63, 64, 65, 127, 129};
    for (const int middle : middle_sizes) {
        constexpr int rows = 3;
        constexpr int cols = 5;
        bit_matrix lhs(rows, middle);
        bit_matrix rhs(middle, cols);
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < middle; ++col) {
                if ((rng() & 1U) != 0U) lhs.set(row, col);
            }
        }
        for (int row = 0; row < middle; ++row) {
            for (int col = 0; col < cols; ++col) {
                if ((rng() & 1U) != 0U) rhs.set(row, col);
            }
        }
        const auto product = gf2_matrix_product(lhs, rhs);
        if (middle == 0) {
            require_equal(
                unpack_bits(product),
                std::vector<std::vector<std::uint8_t>>(
                    rows,
                    std::vector<std::uint8_t>(cols, 0)),
                "gf2 zero inner product");
        } else {
            require_equal(
                unpack_bits(product),
                naive_gf2_product(unpack_bits(lhs), unpack_bits(rhs)),
                "gf2 boundary product");
        }
    }

    for (int iteration = 0; iteration < 500; ++iteration) {
        const int rows = 1 + static_cast<int>(rng() % 8U);
        const int middle = 1 + static_cast<int>(rng() % 8U);
        const int cols = 1 + static_cast<int>(rng() % 8U);
        bit_matrix lhs(rows, middle);
        bit_matrix rhs(middle, cols);
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < middle; ++col) if ((rng() & 1U) != 0U) lhs.set(row, col);
        }
        for (int row = 0; row < middle; ++row) {
            for (int col = 0; col < cols; ++col) if ((rng() & 1U) != 0U) rhs.set(row, col);
        }
        require_equal(
            unpack_bits(gf2_matrix_product(lhs, rhs)),
            naive_gf2_product(unpack_bits(lhs), unpack_bits(rhs)),
            "gf2 random product");
    }

    {
        bit_matrix swap(2, 2);
        swap.set(0, 1);
        swap.set(1, 0);
        require(
            bit_matrices_equal(gf2_matrix_power(swap, 64), bit_matrix::identity(2)),
            "gf2 exponent 64");
        require(
            bit_matrices_equal(gf2_matrix_power(swap, 65), swap),
            "gf2 exponent 65");
    }

    for (int iteration = 0; iteration < 350; ++iteration) {
        const int size = 1 + static_cast<int>(rng() % 8U);
        const int exponent = static_cast<int>(rng() % 10U);
        bit_matrix base(size, size);
        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) if ((rng() & 1U) != 0U) base.set(row, col);
        }
        auto expected = unpack_bits(bit_matrix::identity(size));
        const auto unpacked_base = unpack_bits(base);
        for (int power_index = 0; power_index < exponent; ++power_index) {
            expected = naive_gf2_product(expected, unpacked_base);
        }
        require_equal(
            unpack_bits(gf2_matrix_power(base, exponent)),
            expected,
            "gf2 random power");

        std::vector<std::uint8_t> vector(size, 0);
        for (std::uint8_t& value : vector) value = (rng() & 1U) != 0U;
        std::vector<std::uint8_t> expected_vector(size, 0);
        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) {
                expected_vector[row] =
                    (expected_vector[row] != 0U) !=
                    ((expected[row][col] & vector[col]) != 0U);
            }
        }
        require_equal(
            gf2_matrix_power_apply(base, exponent, vector),
            expected_vector,
            "gf2 power apply");
    }

    for (int iteration = 0; iteration < 500; ++iteration) {
        const int rows = static_cast<int>(rng() % 9U);
        const int cols = static_cast<int>(rng() % 9U);
        bit_matrix a(rows, cols);
        std::vector<std::vector<std::uint8_t>> model(
            rows,
            std::vector<std::uint8_t>(cols, 0));
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const std::uint8_t value = (rng() & 1U) != 0U;
                model[row][col] = value;
                if (value != 0U) a.set(row, col);
            }
        }
        require_equal(gf2_rank(a), naive_gf2_rank(model), "gf2 random rank");
    }

    for (int iteration = 0; iteration < 350; ++iteration) {
        const int size = static_cast<int>(rng() % 9U);
        bit_matrix a(size, size);
        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) if ((rng() & 1U) != 0U) a.set(row, col);
        }
        const auto inverse = gf2_inverse(a);
        require_equal(inverse.has_value(), gf2_rank(a) == size, "gf2 inverse existence");
        if (inverse.has_value()) {
            require(
                bit_matrices_equal(gf2_matrix_product(a, *inverse), bit_matrix::identity(size)),
                "gf2 inverse product");
        }
    }

    for (int iteration = 0; iteration < 500; ++iteration) {
        const int equation_count = static_cast<int>(rng() % 9U);
        const int variable_count = static_cast<int>(rng() % 9U);
        bit_matrix coefficient(equation_count, variable_count);
        std::vector<std::uint8_t> constant(equation_count, 0);
        for (int row = 0; row < equation_count; ++row) {
            for (int col = 0; col < variable_count; ++col) if ((rng() & 1U) != 0U) coefficient.set(row, col);
            constant[row] = (rng() & 1U) != 0U;
        }

        int solution_count = 0;
        const std::uint64_t candidate_limit = std::uint64_t{1} << variable_count;
        for (std::uint64_t mask = 0; mask < candidate_limit; ++mask) {
            std::vector<std::uint8_t> candidate(variable_count, 0);
            for (int variable = 0; variable < variable_count; ++variable) {
                candidate[variable] = ((mask >> variable) & 1U) != 0U;
            }
            if (gf2_matrix_apply(coefficient, candidate) == constant) ++solution_count;
        }

        const auto solved = gf2_solve_linear_system(coefficient, constant);
        if (solution_count == 0) {
            require_equal(solved.status, gf2_linear_system_status::no_solution, "gf2 no solution status");
            require(solved.particular_solution.empty(), "gf2 no solution vector");
            continue;
        }

        const gf2_linear_system_status expected_status =
            solution_count == 1 ? gf2_linear_system_status::unique_solution
                                : gf2_linear_system_status::multiple_solutions;
        require_equal(solved.status, expected_status, "gf2 solve status");
        require_equal(gf2_matrix_apply(coefficient, solved.particular_solution), constant, "gf2 special solution");
        require_equal(solved.kernel_basis.rows(), variable_count - solved.rank, "gf2 kernel dimension");
        require_equal(gf2_rank(solved.kernel_basis), solved.kernel_basis.rows(), "gf2 kernel independence");
        for (int basis_row = 0; basis_row < solved.kernel_basis.rows(); ++basis_row) {
            std::vector<std::uint8_t> basis_vector(variable_count, 0);
            for (int col = 0; col < variable_count; ++col) {
                basis_vector[col] = solved.kernel_basis.get(basis_row, col);
            }
            require_equal(
                gf2_matrix_apply(coefficient, basis_vector),
                std::vector<std::uint8_t>(equation_count, 0),
                "gf2 homogeneous basis");
        }
        require_equal(
            solution_count,
            1 << solved.kernel_basis.rows(),
            "gf2 solution space cardinality");
    }

    {
        constexpr int rows = 70;
        constexpr int cols = 130;
        bit_matrix a(rows, cols);
        std::vector<std::vector<std::uint8_t>> model(
            rows,
            std::vector<std::uint8_t>(cols, 0));
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const std::uint8_t value = (rng() & 1U) != 0U;
                model[row][col] = value;
                if (value != 0U) a.set(row, col);
            }
        }
        require_equal(gf2_rank(a), naive_gf2_rank(model), "gf2 multi-block rank");
    }
}

void test_boolean_operations() {
    std::mt19937_64 rng(0x626f6f6c5f6d6174ULL);
    const std::array<int, 7> middle_sizes{0, 1, 63, 64, 65, 127, 129};
    for (const int middle : middle_sizes) {
        constexpr int rows = 3;
        constexpr int cols = 5;
        bit_matrix lhs(rows, middle);
        bit_matrix rhs(middle, cols);
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < middle; ++col) if ((rng() & 3U) == 0U) lhs.set(row, col);
        }
        for (int row = 0; row < middle; ++row) {
            for (int col = 0; col < cols; ++col) if ((rng() & 3U) == 0U) rhs.set(row, col);
        }
        const auto product = boolean_matrix_product(lhs, rhs);
        if (middle == 0) {
            require_equal(
                unpack_bits(product),
                std::vector<std::vector<std::uint8_t>>(
                    rows,
                    std::vector<std::uint8_t>(cols, 0)),
                "boolean zero inner product");
        } else {
            require_equal(
                unpack_bits(product),
                naive_boolean_product(unpack_bits(lhs), unpack_bits(rhs)),
                "boolean boundary product");
        }
    }

    {
        bit_matrix swap(2, 2);
        swap.set(0, 1);
        swap.set(1, 0);
        require(
            bit_matrices_equal(boolean_matrix_power(swap, 64), bit_matrix::identity(2)),
            "boolean exponent 64");
        require(
            bit_matrices_equal(boolean_matrix_power(swap, 65), swap),
            "boolean exponent 65");
    }

    for (int iteration = 0; iteration < 400; ++iteration) {
        const int size = 1 + static_cast<int>(rng() % 8U);
        const int exponent = static_cast<int>(rng() % 9U);
        bit_matrix base(size, size);
        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) if ((rng() & 3U) == 0U) base.set(row, col);
        }
        auto expected = unpack_bits(bit_matrix::identity(size));
        const auto unpacked_base = unpack_bits(base);
        for (int power_index = 0; power_index < exponent; ++power_index) {
            expected = naive_boolean_product(expected, unpacked_base);
        }
        require_equal(
            unpack_bits(boolean_matrix_power(base, exponent)),
            expected,
            "boolean random power");
    }

    for (int iteration = 0; iteration < 500; ++iteration) {
        const int vertex_count = 1 + static_cast<int>(rng() % 9U);
        const int edge_total = static_cast<int>(rng() % 22U);
        std::vector<std::pair<int, int>> edges;
        edges.reserve(edge_total);
        for (int edge_index = 0; edge_index < edge_total; ++edge_index) {
            edges.emplace_back(
                rng() % vertex_count,
                rng() % vertex_count);
        }
        const int source = static_cast<int>(rng() % vertex_count);
        const int limit = static_cast<int>(rng() % 11U);

        std::vector<std::uint8_t> state(vertex_count, 0);
        state[source] = 1;
        std::vector<std::uint8_t> prefix(vertex_count, 0);
        for (int length = 0; length < limit; ++length) {
            for (int vertex = 0; vertex < vertex_count; ++vertex) {
                prefix[vertex] |= state[vertex];
            }
            std::vector<std::uint8_t> next(vertex_count, 0);
            for (const auto& [from, to] : edges) {
                if (state[from] != 0U) next[to] = 1;
            }
            state = std::move(next);
        }
        require_equal(
            reachable_exactly_k_steps(vertex_count, edges, source, limit),
            state,
            "boolean exact reachability");
        require_equal(
            reachable_with_fewer_than_k_steps(vertex_count, edges, source, limit),
            prefix,
            "boolean prefix reachability");

        for (const bool reflexive : {false, true}) {
            std::vector<std::vector<std::uint8_t>> closure(
                vertex_count,
                std::vector<std::uint8_t>(vertex_count, 0));
            for (const auto& [from, to] : edges) closure[from][to] = 1;
            if (reflexive) {
                for (int vertex = 0; vertex < vertex_count; ++vertex) {
                    closure[vertex][vertex] = 1;
                }
            }
            for (int middle = 0; middle < vertex_count; ++middle) {
                for (int from = 0; from < vertex_count; ++from) {
                    for (int to = 0; to < vertex_count; ++to) {
                        closure[from][to] =
                            closure[from][to] != 0U ||
                            (closure[from][middle] != 0U &&
                             closure[middle][to] != 0U);
                    }
                }
            }
            require_equal(
                unpack_bits(directed_transitive_closure(vertex_count, edges, reflexive)),
                closure,
                "boolean transitive closure");
        }
    }
}

void run_tests() {
    test_matrix_basic_and_power();
    std::cout << "matrix_basic_power_random_1000: ok\n";
    test_fixed_matrix_and_affine();
    std::cout << "fixed_matrix_affine_random_400: ok\n";
    test_semiring_matrix_and_solvers();
    std::cout << "semiring_matrix_sum_bottleneck_custom_random_900: ok\n";
    test_general_use_case_solvers();
    std::cout << "recurrence_walk_markov_random_700: ok\n";
    test_min_plus_and_max_plus();
    std::cout << "min_plus_max_plus_random_500: ok\n";
    test_field_linear_algebra();
    std::cout << "field_gauss_det_inverse_solve_random_1300: ok\n";
    test_real_linear_algebra();
    std::cout << "real_gauss_det_inverse_solve_random_250: ok\n";
    test_integer_and_mod_determinants();
    std::cout << "bareiss_arbitrary_mod_random_500x9: ok\n";
    test_matrix_tree_theorems();
    std::cout << "matrix_tree_undirected_directed_random_700: ok\n";
    test_bit_matrix_basic();
    std::cout << "bit_matrix_boundaries_0_1_63_64_65_127_128_129: ok\n";
    test_gf2_operations();
    std::cout << "gf2_product_power_rank_inverse_solve_random_2200: ok\n";
    test_boolean_operations();
    std::cout << "boolean_product_power_reachability_closure_random_900: ok\n";
    std::cout << "all tests passed\n";
}

} // namespace

int main() {
    try {
        run_tests();
    } catch (const std::exception& error) {
        std::cerr << "test failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}

#endif
