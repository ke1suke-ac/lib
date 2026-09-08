/*
 * mod_matrix / dynamic_mod_matrix: 固定法の正方行列を高速に乗算・累乗し、列ベクトルへ作用させるライブラリ。
 * mod_matrix はコンパイル時次数、dynamic_mod_matrix は入力で決まる実行時次数を扱う。
 * dynamic_mod_matrix の FastDimensions に候補次数を列挙すると、その次数だけ固定長相当の専用 kernel を生成する。
 * bounded_dynamic_mod_matrix は 1 から指定上限までの候補列挙を簡略化する。
 * 要素を row-major の uint32_t 配列へ連続配置し、積和を uint64_t に安全な範囲まで遅延してから剰余を取る。
 * 小さい行列は転置済み右辺との内積、大きい行列は複数行ブロックの i-k-j 順で処理し、キャッシュ効率と自動 SIMD を高める。
 * 累乗は二進法と幅 2・3 の sliding-window 法を指数ごとに比較し、行列累乗後のベクトルだけが必要な場合は
 * pow_apply により、行列積を平方だけへ減らして A^exponent * vector を直接計算する。
 * operator() / data() から直接書き込む行列・ベクトル要素は [0, Mod) でなければならない。
 *
 * 【行列と状態ベクトルの規約】
 * - ベクトルは列ベクトルとして扱い、1 回の状態遷移を next = transition * current と書く
 * - transition(to, from) には、状態 from から状態 to へ加える係数を入れる
 * - よって transition.pow(k) * initial と transition.pow_apply(k, initial) は k 回遷移後の状態を返す
 * - (lhs * rhs) * vector では rhs が先、lhs が後に作用する
 * - pow(0) は単位行列、pow_apply(0, vector) は入力 vector をそのまま返す
 * - 通常の加算・乗算を法 Mod で行う行列専用であり、min-plus、max-plus、論理半環には対応しない
 *
 * 【型の選び方】
 * 1. 次数 N がコンパイル時に決まる場合
 *      using Matrix = mod_matrix<998244353, N>;
 *    固定長配列と N 専用 kernel を使えるため、原則として最も高速
 *
 * 2. 次数 n が入力で決まる場合
 *      using Matrix = dynamic_mod_matrix<998244353>;
 *      Matrix matrix(n);
 *    任意の正の n を扱える汎用版
 *
 * 3. 入力次数の候補が少数に限られる場合
 *      using Matrix = dynamic_mod_matrix<998244353, 2, 4, 8, 16, 32>;
 *      Matrix matrix(n);
 *    候補に一致する n では固定長相当の専用 kernel を使い、候補外では汎用 kernel へ戻る
 *
 * 4. 入力次数が 1 以上 MaxN 以下であることだけ分かる場合
 *      using Matrix = bounded_dynamic_mod_matrix<998244353, MaxN>;
 *      Matrix matrix(n);
 *    1..MaxN をすべて特殊化するため、MaxN を大きくするとコンパイル時間とコードサイズが増える
 *
 * 【基本操作】
 * 固定次数版の vector_type は std::array<uint32_t, N>、動的次数版は std::vector<uint32_t> になる。
 *
 *      constexpr std::uint32_t MOD = 998244353;
 *      using Matrix = mod_matrix<MOD, 3>;
 *
 *      Matrix a;                          // 3x3 零行列
 *      a(0, 0) = 1;                      // 直接代入する値は [0, MOD)
 *      a.set(0, 1, large_value);         // large_value % MOD を格納
 *      a.set_signed(0, 2, signed_value); // 負数も正規化して格納
 *
 *      const Matrix identity = Matrix::identity();
 *      const Matrix product = a * identity;
 *      const Matrix powered = a.pow(k);
 *
 *      Matrix::vector_type vector{1, 2, 3};
 *      const auto one_step = a * vector;
 *      const auto k_steps = a.pow_apply(k, vector);
 *
 * 【pow と pow_apply の使い分け】
 * - A^k * v を 1 個だけ求める場合は A.pow_apply(k, v) を使う
 *   通常は累乗行列全体を完成させず、指数の立っている bit を行列ベクトル積で反映する
 *   固定次数版の 2x2 だけは実測に基づく専用分岐により、pow 後に 1 回ベクトルへ掛ける
 * - A^k 自体が必要な場合、または同じ A^k を複数のベクトルへ使う場合は一度だけ pow する
 *      const Matrix powered = a.pow(k);
 *      const auto answer1 = powered * vector1;
 *      const auto answer2 = powered * vector2;
 *
 * 【主要ユースケース 1: Fibonacci 数列】
 * state_n = {F_(n+1), F_n} とすると、次の遷移で F_n を O(log n) で求められる。
 *
 *      std::uint32_t fibonacci(std::uint64_t n) {
 *          using Matrix = mod_matrix<998244353, 2>;
 *          Matrix transition;
 *          transition(0, 0) = 1;
 *          transition(0, 1) = 1;
 *          transition(1, 0) = 1;
 *          const Matrix::vector_type initial{1, 0}; // {F_1, F_0}
 *          return transition.pow_apply(n, initial)[1];
 *      }
 *
 * 【主要ユースケース 2: d 項線形漸化式】
 * x_t = c[0] x_(t-1) + c[1] x_(t-2) + ... + c[d-1] x_(t-d) を考える。
 * target < d は初期値を直接返し、target >= d では companion matrix を使う。
 *
 *      using Matrix = dynamic_mod_matrix<998244353>;
 *      Matrix transition(d);
 *      for (std::size_t j = 0; j < d; ++j) transition.set(0, j, coefficient[j]);
 *      for (std::size_t i = 1; i < d; ++i) transition(i, i - 1) = 1;
 *
 *      Matrix::vector_type state(d);
 *      for (std::size_t i = 0; i < d; ++i) state[i] = initial[d - 1 - i];
 *      const auto result = transition.pow_apply(target - (d - 1), state);
 *      const std::uint32_t answer = result[0];
 *
 * state は {x_(d-1), x_(d-2), ..., x_0} と並べる。d が定数なら mod_matrix<Mod, d> の方が速い。
 *
 * 【主要ユースケース 3: 長さ k の walk 数・重み付き状態遷移】
 * adjacency(to, from) に有向辺 from -> to の本数または重みを入れる。
 * このとき powered(to, from) が、from から to への長さ k の walk の重み総和になる。
 *
 *      using Matrix = dynamic_mod_matrix<998244353>;
 *      Matrix adjacency(n);
 *      for (const auto& edge : edges) {
 *          const std::size_t from = edge.from;
 *          const std::size_t to = edge.to;
 *          const std::uint64_t current = adjacency(to, from);
 *          const std::uint64_t add = edge.weight % Matrix::mod;
 *          adjacency.set(to, from, current + add); // 多重辺も加算
 *      }
 *
 *      const Matrix powered = adjacency.pow(k);
 *      const std::uint32_t all_pairs_answer = powered(goal, start);
 *
 * 特定の始点集合から各頂点への個数だけが必要なら pow_apply を使う。
 *
 *      Matrix::vector_type initial(n, 0);
 *      initial[start] = 1;
 *      const auto count = adjacency.pow_apply(k, initial);
 *      const std::uint32_t answer = count[goal];
 *
 * 無向辺 u-v は adjacency(v, u) と adjacency(u, v) の両方へ加える。
 *
 * 【主要ユースケース 4: 有限状態 DP・オートマトン】
 * 1 ステップの DP が next[to] += transition(to, from) * current[from] と線形に書けるなら、
 * k ステップを transition.pow_apply(k, initial) でまとめられる。
 * 状態には、直前数文字、余り、使用済みフラグ、オートマトン頂点などをまとめて番号付けする。
 * 遷移が時刻によらず一定であることが必要で、時刻ごとに変わる場合は区間ごとの行列積などを使う。
 *
 * 【主要ユースケース 5: アフィン変換・累積和】
 * x' = a*x + b のような定数項付き変換は、末尾に常に 1 の成分を追加して線形化する。
 * a、b、x は [0, Mod) に正規化済みの uint32_t とする。
 *
 *      using Matrix = mod_matrix<998244353, 2>;
 *      Matrix affine;
 *      affine(0, 0) = a;
 *      affine(0, 1) = b;
 *      affine(1, 1) = 1;
 *      const Matrix::vector_type initial{x, 1};
 *      const std::uint32_t after_k = affine.pow_apply(k, initial)[0];
 *
 * 多変数の x' = M*x + b も、(変数数 + 1) 次の行列 [[M, b], [0, 1]] で同様に処理できる。
 * 数列の累積和は sum を状態へ追加し、次状態で sum' = sum + next_term となる行を加える。
 *
 * 【入力値と制約】
 * - Mod はコンパイル時定数で、2 以上 UINT32_MAX 以下を指定する
 * - 行列は 1 次以上の正方行列だけを扱う
 * - operator() / data() へ直接代入する行列要素は [0, Mod) に正規化する
 * - 未正規化の非負値は set、負数を含む値は set_signed で行列へ設定する
 * - 入力ベクトルの各要素も [0, Mod) に正規化してから渡す
 * - exponent は uint64_t で、0 以上 2^64 - 1 以下を扱える
 *
 * 【計算量】
 * - 行列積 A * B         : O(N^3)
 * - 行列ベクトル積 A * v : O(N^2)
 * - 行列累乗 A.pow(k)     : O(N^3 log k)
 * - A.pow_apply(k, v)     : O(N^3 log k) だが、1 ベクトルだけなら pow 後の積より通常高速
 *
 * テンプレートパラメータ:
 *   Mod            : 2 以上 UINT32_MAX 以下の法
 *   N              : mod_matrix で使う 1 以上の正方行列の次数
 *   FastDimensions : dynamic_mod_matrix で定数化して高速化する任意個の候補次数
 */
#pragma once

#include <bits/stdc++.h>

namespace mod_matrix_detail {

// 符号なし型からの縮小変換を挟まず、0 以上 64 以下のビット長を int で返す
constexpr int bit_length_u64(std::uint64_t value) noexcept {
    return std::numeric_limits<std::uint64_t>::digits - std::countl_zero(value);
}

} // namespace mod_matrix_detail

template <std::uint32_t Mod, std::size_t N>
struct mod_matrix {
    static_assert(Mod >= 2, "mod_matrix の法は 2 以上でなければならない");
    static_assert(N >= 1, "mod_matrix の次数は 1 以上でなければならない");
    static_assert(N <= std::numeric_limits<std::size_t>::max() / N,
                  "mod_matrix の要素数が size_t の範囲を超える");

    using value_type = std::uint32_t;
    using vector_type = std::array<std::uint32_t, N>;
    inline static constexpr std::uint32_t mod = Mod;
    inline static constexpr std::size_t dimension = N;
    inline static constexpr std::size_t element_count = N * N;

private:
    struct uninitialized_t {};

    struct PowerPlan {
        int width;
        unsigned max_odd_index;
        int multiplication_count;
    };

    std::array<std::uint32_t, element_count> values;

    explicit mod_matrix(uninitialized_t) noexcept {}

    static constexpr std::size_t safe_accumulation_terms() {
        constexpr std::uint64_t max_value = Mod - 1;
        constexpr std::uint64_t max_product = max_value * max_value;
        return (std::numeric_limits<std::uint64_t>::max() - max_value) / max_product;
    }

    static constexpr std::size_t accumulation_chunk_size() {
        // 最少の剰余回数を保ったまま各 chunk を均等化し、固定長 loop の最適化を促す
        constexpr std::size_t safe = safe_accumulation_terms();
        constexpr std::size_t chunks = 1 + (N - 1) / safe;
        return 1 + (N - 1) / chunks;
    }

    static unsigned extract_window(std::uint64_t exponent, int high, int width,
                                   int& low) {
        low = std::max(0, high - width + 1);
        while (((exponent >> low) & 1ULL) == 0) ++low;
        const int length = high - low + 1;
        return static_cast<unsigned>((exponent >> low) & ((1ULL << length) - 1));
    }

    static PowerPlan evaluate_power_plan(std::uint64_t exponent, int width) {
        const int exponent_bits = mod_matrix_detail::bit_length_u64(exponent);
        if (width == 1) {
            return PowerPlan{1, 0, exponent_bits + std::popcount(exponent) - 2};
        }
        int first_window_length = 0;
        int window_count = 0;
        unsigned max_odd_index = 0;
        std::uint64_t remaining = exponent;

        // 0 の連続区間をまとめて飛ばし、非零 window の個数と事前計算範囲だけを求める
        while (remaining != 0) {
            const int high = mod_matrix_detail::bit_length_u64(remaining) - 1;
            int low = 0;
            const unsigned window = extract_window(remaining, high, width, low);
            const int length = high - low + 1;
            if (window_count == 0) first_window_length = length;
            ++window_count;
            max_odd_index = std::max(max_odd_index, window >> 1);
            remaining = low == 0 ? 0 : remaining & ((1ULL << low) - 1);
        }

        // 先頭 window 以外の各 bit は平方 1 回、各非零 window は乗算 1 回を要する
        int multiplication_count = exponent_bits - first_window_length + window_count - 1;
        if (max_odd_index != 0) multiplication_count += max_odd_index + 1;
        return PowerPlan{width, max_odd_index, multiplication_count};
    }

    static PowerPlan select_power_plan(std::uint64_t exponent) {
        PowerPlan best = evaluate_power_plan(exponent, 1);
        for (int width = 2; width <= 3; ++width) {
            const PowerPlan candidate = evaluate_power_plan(exponent, width);
            if (candidate.multiplication_count < best.multiplication_count) best = candidate;
        }
        return best;
    }

    static void multiply_dot(mod_matrix& out, const mod_matrix& lhs,
                             const mod_matrix& rhs) {
        std::array<std::uint32_t, element_count> rhs_transposed;
        for (std::size_t row = 0; row < N; ++row) {
            for (std::size_t col = 0; col < N; ++col) {
                rhs_transposed[col * N + row] = rhs.values[row * N + col];
            }
        }

        // 両方の入力を連続読み込みし、各出力要素の剰余回数を最小化する
        constexpr std::size_t chunk = accumulation_chunk_size();
        for (std::size_t row = 0; row < N; ++row) {
            const std::uint32_t* lhs_row = lhs.values.data() + row * N;
            for (std::size_t col = 0; col < N; ++col) {
                const std::uint32_t* rhs_row = rhs_transposed.data() + col * N;
                std::uint64_t sum = 0;
                for (std::size_t begin = 0; begin < N; begin += chunk) {
                    const std::size_t end = std::min(N, begin + chunk);
                    for (std::size_t k = begin; k < end; ++k) {
                        sum += static_cast<std::uint64_t>(lhs_row[k]) * rhs_row[k];
                    }
                    sum %= Mod;
                }
                out.values[row * N + col] = static_cast<std::uint32_t>(sum);
            }
        }
    }

    template <std::size_t RowBlock>
    static void multiply_blocked(mod_matrix& out, const mod_matrix& lhs,
                                 const mod_matrix& rhs) {
        constexpr std::size_t chunk = accumulation_chunk_size();
        std::array<std::uint64_t, RowBlock * N> sums;

        // 複数の出力行を同時処理し、右辺の各行を L1 cache 上で再利用する
        for (std::size_t row_begin = 0; row_begin < N; row_begin += RowBlock) {
            const std::size_t rows = std::min(RowBlock, N - row_begin);
            std::fill(sums.begin(), sums.end(), 0);

            // overflow しない k 範囲ごとに積和し、chunk 境界だけで剰余を取る
            for (std::size_t k_begin = 0; k_begin < N; k_begin += chunk) {
                const std::size_t k_end = std::min(N, k_begin + chunk);
                for (std::size_t k = k_begin; k < k_end; ++k) {
                    const std::uint32_t* __restrict__ rhs_row = rhs.values.data() + k * N;
                    for (std::size_t inner_row = 0; inner_row < rows; ++inner_row) {
                        const std::uint64_t lhs_value =
                            lhs.values[(row_begin + inner_row) * N + k];
                        std::uint64_t* __restrict__ sum_row = sums.data() + inner_row * N;
                        for (std::size_t col = 0; col < N; ++col) {
                            sum_row[col] += lhs_value * rhs_row[col];
                        }
                    }
                }

                for (std::size_t inner_row = 0; inner_row < rows; ++inner_row) {
                    std::uint64_t* sum_row = sums.data() + inner_row * N;
                    for (std::size_t col = 0; col < N; ++col) sum_row[col] %= Mod;
                }
            }

            // uint64_t の作業領域から正規化済み uint32_t 配列へ書き戻す
            for (std::size_t inner_row = 0; inner_row < rows; ++inner_row) {
                const std::uint64_t* sum_row = sums.data() + inner_row * N;
                std::uint32_t* out_row = out.values.data() + (row_begin + inner_row) * N;
                for (std::size_t col = 0; col < N; ++col) {
                    out_row[col] = static_cast<std::uint32_t>(sum_row[col]);
                }
            }
        }
    }

    static void multiply_to(mod_matrix& out, const mod_matrix& lhs,
                            const mod_matrix& rhs) {
        if constexpr (N == 1) {
            out.values[0] =
                static_cast<std::uint64_t>(lhs.values[0]) * rhs.values[0] % Mod;
        } else if constexpr (N <= 7) {
            multiply_dot(out, lhs, rhs);
        } else if constexpr (N <= 192) {
            multiply_blocked<8>(out, lhs, rhs);
        } else if constexpr (N <= 512) {
            multiply_blocked<4>(out, lhs, rhs);
        } else {
            multiply_blocked<2>(out, lhs, rhs);
        }
    }

    static void multiply_vector_to(vector_type& out, const mod_matrix& matrix,
                                   const vector_type& vector) {
        constexpr std::size_t chunk = accumulation_chunk_size();
        const std::uint32_t* __restrict__ input = vector.data();
        std::uint32_t* __restrict__ output = out.data();

        // 行列の各行と列ベクトルを連続読み込みし、chunk 境界だけで剰余を取る
        for (std::size_t row = 0; row < N; ++row) {
            const std::uint32_t* __restrict__ matrix_row = matrix.values.data() + row * N;
            std::uint64_t sum = 0;
            for (std::size_t begin = 0; begin < N; begin += chunk) {
                const std::size_t end = std::min(N, begin + chunk);
                for (std::size_t col = begin; col < end; ++col) {
                    sum += static_cast<std::uint64_t>(matrix_row[col]) * input[col];
                }
                sum %= Mod;
            }
            output[row] = static_cast<std::uint32_t>(sum);
        }
    }

    mod_matrix pow_binary(std::uint64_t exponent) const {
        mod_matrix result = *this;
        mod_matrix scratch(uninitialized_t{});
        mod_matrix* current = &result;
        mod_matrix* next = &scratch;
        int bit = mod_matrix_detail::bit_length_u64(exponent) - 2;

        // 出力先を 2 面化し、各行列積の後に N^2 要素を代入し直す処理を避ける
        for (; bit >= 0; --bit) {
            multiply_to(*next, *current, *current);
            std::swap(current, next);
            if (((exponent >> bit) & 1ULL) != 0) {
                multiply_to(*next, *current, *this);
                std::swap(current, next);
            }
        }

        if (current != &result) result.values = current->values;
        return result;
    }

    mod_matrix pow_window_with_storage(std::uint64_t exponent, const PowerPlan& plan,
                                       mod_matrix* odd_powers) const {
        odd_powers[0] = *this;

        // 必要な最大奇数冪までだけを A^2 ずつ掛けて事前計算する
        if (plan.max_odd_index != 0) {
            mod_matrix squared(uninitialized_t{});
            multiply_to(squared, *this, *this);
            for (unsigned index = 1; index <= plan.max_odd_index; ++index) {
                multiply_to(odd_powers[index], odd_powers[index - 1], squared);
            }
        }

        // 最上位の非零 window は単なる代入とし、恒等行列との不要な乗算を省く
        int high = mod_matrix_detail::bit_length_u64(exponent) - 1;
        int low = 0;
        const unsigned first_window = extract_window(exponent, high, plan.width, low);
        mod_matrix result = odd_powers[first_window >> 1];
        mod_matrix scratch(uninitialized_t{});
        mod_matrix* current = &result;
        mod_matrix* next = &scratch;
        high = low - 1;

        // 0 bit は平方 1 回、非零 window は bit 数回の平方と奇数冪 1 回で進める
        while (high >= 0) {
            if (((exponent >> high) & 1ULL) == 0) {
                multiply_to(*next, *current, *current);
                std::swap(current, next);
                --high;
                continue;
            }

            const unsigned window = extract_window(exponent, high, plan.width, low);
            const int square_count = high - low + 1;
            for (int count = 0; count < square_count; ++count) {
                multiply_to(*next, *current, *current);
                std::swap(current, next);
            }
            multiply_to(*next, *current, odd_powers[window >> 1]);
            std::swap(current, next);
            high = low - 1;
        }

        if (current != &result) result.values = current->values;
        return result;
    }

    mod_matrix pow_window(std::uint64_t exponent, const PowerPlan& plan) const {
        // 小さい行列は固定長領域で allocation を避け、大きい行列だけ heap を使う
        if constexpr (element_count <= 1024) {
            std::array<mod_matrix, 4> odd_powers;
            return pow_window_with_storage(exponent, plan, odd_powers.data());
        } else {
            std::vector<mod_matrix> odd_powers(plan.max_odd_index + 1);
            return pow_window_with_storage(exponent, plan, odd_powers.data());
        }
    }

public:
    // 全要素が 0 の行列を構築する。O(N^2)
    mod_matrix() noexcept : values{} {}

    // 単位行列を返す。O(N^2)
    static mod_matrix identity() {
        mod_matrix result;
        for (std::size_t index = 0; index < N; ++index) result.values[index * N + index] = 1;
        return result;
    }

    // 正規化済みの (row, col) 要素への参照を返す。O(1)
    std::uint32_t& operator()(std::size_t row, std::size_t col) {
        assert(row < N && col < N);
        return values[row * N + col];
    }

    // (row, col) 要素を返す。O(1)
    const std::uint32_t& operator()(std::size_t row, std::size_t col) const {
        assert(row < N && col < N);
        return values[row * N + col];
    }

    // row-major 配列の先頭を返す。O(1)
    std::uint32_t* data() noexcept { return values.data(); }

    // row-major 配列の先頭を返す。O(1)
    const std::uint32_t* data() const noexcept { return values.data(); }

    // 非負値を法 Mod で正規化して (row, col) に設定する。O(1)
    void set(std::size_t row, std::size_t col, std::uint64_t value) {
        assert(row < N && col < N);
        values[row * N + col] = static_cast<std::uint32_t>(value % Mod);
    }

    // 符号付き値を法 Mod で正規化して (row, col) に設定する。O(1)
    void set_signed(std::size_t row, std::size_t col, std::int64_t value) {
        assert(row < N && col < N);
        std::int64_t reduced = value % Mod;
        if (reduced < 0) reduced += Mod;
        values[row * N + col] = static_cast<std::uint32_t>(reduced);
    }

    // 右辺との行列積を返す。O(N^3)
    mod_matrix operator*(const mod_matrix& rhs) const {
        mod_matrix result(uninitialized_t{});
        multiply_to(result, *this, rhs);
        return result;
    }

    // 右辺との行列積を自身へ代入する。O(N^3)
    mod_matrix& operator*=(const mod_matrix& rhs) {
        *this = *this * rhs;
        return *this;
    }

    // 列ベクトル vector との積を返す。O(N^2)
    vector_type operator*(const vector_type& vector) const {
        vector_type result;
        multiply_vector_to(result, *this, vector);
        return result;
    }

    // A^exponent * vector を累乗行列を構築せず返す。O(N^3 log exponent)
    vector_type pow_apply(std::uint64_t exponent, const vector_type& vector) const {
        if (exponent == 0) return vector;
        if (exponent == 1) return *this * vector;

        // 2x2 は行列積自体が極小で、専用 matvec を重ねるより累乗後に 1 回掛ける方が速い
        if constexpr (N == 2) return pow(exponent) * vector;

        // 1x1 は scalar 累乗 1 回と scalar 乗算 1 回だけで処理する
        if constexpr (N == 1) {
            std::uint64_t scalar_result = 1;
            std::uint64_t scalar_base = values[0];
            std::uint64_t remaining = exponent;
            while (remaining != 0) {
                if ((remaining & 1ULL) != 0) {
                    scalar_result = scalar_result * scalar_base % Mod;
                }
                remaining >>= 1;
                if (remaining != 0) scalar_base = scalar_base * scalar_base % Mod;
            }
            return vector_type{static_cast<std::uint32_t>(scalar_result * vector[0] % Mod)};
        } else {
            vector_type result = vector;
            vector_type vector_scratch;
            vector_type* current_vector = &result;
            vector_type* next_vector = &vector_scratch;
            mod_matrix square_a(uninitialized_t{});
            mod_matrix square_b(uninitialized_t{});
            const mod_matrix* current_matrix = this;
            mod_matrix* next_matrix = &square_a;
            std::uint64_t remaining = exponent;

            // 各 bit の行列冪を平方だけで作り、立っている bit は O(N^2) の行列ベクトル積で反映する
            while (remaining != 0) {
                if ((remaining & 1ULL) != 0) {
                    multiply_vector_to(*next_vector, *current_matrix, *current_vector);
                    std::swap(current_vector, next_vector);
                }
                remaining >>= 1;
                if (remaining == 0) break;
                multiply_to(*next_matrix, *current_matrix, *current_matrix);
                current_matrix = next_matrix;
                next_matrix = next_matrix == &square_a ? &square_b : &square_a;
            }

            if (current_vector != &result) result = *current_vector;
            return result;
        }
    }

    // 自身の exponent 乗を返す。O(N^3 log exponent)
    mod_matrix pow(std::uint64_t exponent) const {
        if (exponent == 0) return identity();
        if (exponent == 1) return *this;

        // 1x1 は通常の scalar 累乗、2x2 は window 管理コストを避けた二進法が最速
        if constexpr (N == 1) {
            mod_matrix result(uninitialized_t{});
            std::uint64_t scalar_result = 1;
            std::uint64_t scalar_base = values[0];
            while (exponent != 0) {
                if ((exponent & 1ULL) != 0) scalar_result = scalar_result * scalar_base % Mod;
                exponent >>= 1;
                if (exponent != 0) scalar_base = scalar_base * scalar_base % Mod;
            }
            result.values[0] = static_cast<std::uint32_t>(scalar_result);
            return result;
        } else if constexpr (N == 2) {
            mod_matrix result = *this;
            for (int bit = mod_matrix_detail::bit_length_u64(exponent) - 2; bit >= 0; --bit) {
                result = result * result;
                if (((exponent >> bit) & 1ULL) != 0) result = result * *this;
            }
            return result;
        } else {
            const PowerPlan plan = select_power_plan(exponent);
            return plan.width == 1 ? pow_binary(exponent) : pow_window(exponent, plan);
        }
    }

    // 全要素が等しい場合に true を返す。O(N^2)
    friend bool operator==(const mod_matrix&, const mod_matrix&) = default;
};


template <std::uint32_t Mod, std::size_t... FastDimensions>
class dynamic_mod_matrix {
    static_assert(Mod >= 2, "dynamic_mod_matrix の法は 2 以上でなければならない");
    static_assert(((FastDimensions >= 1) && ...),
                  "dynamic_mod_matrix の高速化対象次数は 1 以上でなければならない");

public:
    using value_type = std::uint32_t;
    using vector_type = std::vector<std::uint32_t>;
    inline static constexpr std::uint32_t mod = Mod;

private:
    struct uninitialized_t {};

    struct PowerPlan {
        int width;
        unsigned max_odd_index;
        int multiplication_count;
    };

    struct MultiplyWorkspace {
        std::vector<std::uint64_t> sums;
        std::vector<std::uint32_t> transposed;
    };

    std::size_t dimension_;
    std::vector<std::uint32_t> values;

    static std::size_t checked_element_count(std::size_t dimension) {
        if (dimension == 0 || dimension > std::numeric_limits<std::size_t>::max() / dimension) {
            throw std::length_error("dynamic_mod_matrix の次数が不正");
        }
        return dimension * dimension;
    }

    explicit dynamic_mod_matrix(std::size_t dimension, uninitialized_t)
        : dimension_(dimension), values(checked_element_count(dimension)) {}

    static constexpr std::uint64_t safe_accumulation_terms() {
        constexpr std::uint64_t max_value = Mod - 1;
        constexpr std::uint64_t max_product = max_value * max_value;
        return (std::numeric_limits<std::uint64_t>::max() - max_value) / max_product;
    }

    static std::size_t accumulation_chunk_size(std::size_t dimension) {
        constexpr std::uint64_t safe = safe_accumulation_terms();
        if (dimension <= safe) return dimension;
        const std::size_t safe_size = safe;
        const std::size_t chunks = 1 + (dimension - 1) / safe_size;
        return 1 + (dimension - 1) / chunks;
    }

    static unsigned extract_window(std::uint64_t exponent, int high, int width,
                                   int& low) {
        low = std::max(0, high - width + 1);
        while (((exponent >> low) & 1ULL) == 0) ++low;
        const int length = high - low + 1;
        return static_cast<unsigned>((exponent >> low) & ((1ULL << length) - 1));
    }

    static PowerPlan evaluate_power_plan(std::uint64_t exponent, int width) {
        const int exponent_bits = mod_matrix_detail::bit_length_u64(exponent);
        if (width == 1) {
            return PowerPlan{1, 0, exponent_bits + std::popcount(exponent) - 2};
        }
        int first_window_length = 0;
        int window_count = 0;
        unsigned max_odd_index = 0;
        std::uint64_t remaining = exponent;

        // 非零 window の個数、先頭 window 長、必要な最大奇数冪だけを数える
        while (remaining != 0) {
            const int high = mod_matrix_detail::bit_length_u64(remaining) - 1;
            int low = 0;
            const unsigned window = extract_window(remaining, high, width, low);
            const int length = high - low + 1;
            if (window_count == 0) first_window_length = length;
            ++window_count;
            max_odd_index = std::max(max_odd_index, window >> 1);
            remaining = low == 0 ? 0 : remaining & ((1ULL << low) - 1);
        }

        int multiplication_count = exponent_bits - first_window_length + window_count - 1;
        if (max_odd_index != 0) multiplication_count += max_odd_index + 1;
        return PowerPlan{width, max_odd_index, multiplication_count};
    }

    static PowerPlan select_power_plan(std::uint64_t exponent) {
        PowerPlan best = evaluate_power_plan(exponent, 1);
        for (int width = 2; width <= 3; ++width) {
            const PowerPlan candidate = evaluate_power_plan(exponent, width);
            if (candidate.multiplication_count < best.multiplication_count) best = candidate;
        }
        return best;
    }

    static void multiply_dot(dynamic_mod_matrix& out, const dynamic_mod_matrix& lhs,
                             const dynamic_mod_matrix& rhs, MultiplyWorkspace& workspace) {
        const std::size_t dimension = lhs.dimension_;
        workspace.transposed.resize(dimension * dimension);
        std::uint32_t* transposed = workspace.transposed.data();
        for (std::size_t row = 0; row < dimension; ++row) {
            for (std::size_t col = 0; col < dimension; ++col) {
                transposed[col * dimension + row] = rhs.values[row * dimension + col];
            }
        }

        // 両入力を連続読み込みし、各出力要素の剰余回数を最小化する
        const std::size_t chunk = accumulation_chunk_size(dimension);
        for (std::size_t row = 0; row < dimension; ++row) {
            const std::uint32_t* __restrict__ lhs_row =
                lhs.values.data() + row * dimension;
            for (std::size_t col = 0; col < dimension; ++col) {
                const std::uint32_t* __restrict__ rhs_row = transposed + col * dimension;
                std::uint64_t sum = 0;
                for (std::size_t begin = 0; begin < dimension; begin += chunk) {
                    const std::size_t end = std::min(dimension, begin + chunk);
                    for (std::size_t k = begin; k < end; ++k) {
                        sum += static_cast<std::uint64_t>(lhs_row[k]) * rhs_row[k];
                    }
                    sum %= Mod;
                }
                out.values[row * dimension + col] = static_cast<std::uint32_t>(sum);
            }
        }
    }

    template <std::size_t RowBlock>
    static void multiply_blocked(dynamic_mod_matrix& out, const dynamic_mod_matrix& lhs,
                                 const dynamic_mod_matrix& rhs,
                                 MultiplyWorkspace& workspace) {
        const std::size_t dimension = lhs.dimension_;
        const std::size_t chunk = accumulation_chunk_size(dimension);
        workspace.sums.resize(RowBlock * dimension);
        std::uint64_t* sums = workspace.sums.data();
        std::size_t row_begin = 0;

        // 完全な行ブロックでは RowBlock をコンパイル時定数に保ち、行 loop の展開を促す
        for (; row_begin + RowBlock <= dimension; row_begin += RowBlock) {
            std::fill_n(sums, RowBlock * dimension, 0ULL);
            for (std::size_t k_begin = 0; k_begin < dimension; k_begin += chunk) {
                const std::size_t k_end = std::min(dimension, k_begin + chunk);
                for (std::size_t k = k_begin; k < k_end; ++k) {
                    const std::uint32_t* __restrict__ rhs_row =
                        rhs.values.data() + k * dimension;
                    for (std::size_t inner_row = 0; inner_row < RowBlock; ++inner_row) {
                        const std::uint64_t lhs_value =
                            lhs.values[(row_begin + inner_row) * dimension + k];
                        std::uint64_t* __restrict__ sum_row = sums + inner_row * dimension;
                        for (std::size_t col = 0; col < dimension; ++col) {
                            sum_row[col] += lhs_value * rhs_row[col];
                        }
                    }
                }

                for (std::size_t inner_row = 0; inner_row < RowBlock; ++inner_row) {
                    std::uint64_t* sum_row = sums + inner_row * dimension;
                    for (std::size_t col = 0; col < dimension; ++col) sum_row[col] %= Mod;
                }
            }

            for (std::size_t inner_row = 0; inner_row < RowBlock; ++inner_row) {
                const std::uint64_t* sum_row = sums + inner_row * dimension;
                std::uint32_t* out_row =
                    out.values.data() + (row_begin + inner_row) * dimension;
                for (std::size_t col = 0; col < dimension; ++col) {
                    out_row[col] = static_cast<std::uint32_t>(sum_row[col]);
                }
            }
        }

        // 最後の端数行だけは実行時の行数で処理する
        if (row_begin < dimension) {
            const std::size_t rows = dimension - row_begin;
            std::fill_n(sums, rows * dimension, 0ULL);
            for (std::size_t k_begin = 0; k_begin < dimension; k_begin += chunk) {
                const std::size_t k_end = std::min(dimension, k_begin + chunk);
                for (std::size_t k = k_begin; k < k_end; ++k) {
                    const std::uint32_t* __restrict__ rhs_row =
                        rhs.values.data() + k * dimension;
                    for (std::size_t inner_row = 0; inner_row < rows; ++inner_row) {
                        const std::uint64_t lhs_value =
                            lhs.values[(row_begin + inner_row) * dimension + k];
                        std::uint64_t* __restrict__ sum_row = sums + inner_row * dimension;
                        for (std::size_t col = 0; col < dimension; ++col) {
                            sum_row[col] += lhs_value * rhs_row[col];
                        }
                    }
                }

                for (std::size_t inner_row = 0; inner_row < rows; ++inner_row) {
                    std::uint64_t* sum_row = sums + inner_row * dimension;
                    for (std::size_t col = 0; col < dimension; ++col) sum_row[col] %= Mod;
                }
            }

            for (std::size_t inner_row = 0; inner_row < rows; ++inner_row) {
                const std::uint64_t* sum_row = sums + inner_row * dimension;
                std::uint32_t* out_row =
                    out.values.data() + (row_begin + inner_row) * dimension;
                for (std::size_t col = 0; col < dimension; ++col) {
                    out_row[col] = static_cast<std::uint32_t>(sum_row[col]);
                }
            }
        }
    }

    template <std::size_t Dimension>
    static constexpr std::size_t specialized_chunk_size() {
        static_assert(Dimension >= 1);
        static_assert(Dimension <= std::numeric_limits<std::size_t>::max() / Dimension);
        constexpr std::size_t safe = safe_accumulation_terms();
        constexpr std::size_t chunks = 1 + (Dimension - 1) / safe;
        return 1 + (Dimension - 1) / chunks;
    }

    template <std::size_t Dimension>
    static void multiply_specialized(dynamic_mod_matrix& out,
                                     const dynamic_mod_matrix& lhs,
                                     const dynamic_mod_matrix& rhs) {
        constexpr std::size_t count = Dimension * Dimension;
        if constexpr (Dimension == 1) {
            out.values[0] = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(lhs.values[0]) * rhs.values[0] % Mod);
        } else if constexpr (Dimension <= 7) {
            std::array<std::uint32_t, count> transposed;
            for (std::size_t row = 0; row < Dimension; ++row) {
                for (std::size_t col = 0; col < Dimension; ++col) {
                    transposed[col * Dimension + row] =
                        rhs.values[row * Dimension + col];
                }
            }

            // 次数を定数化した連続内積により、小行列の loop 管理コストを除く
            constexpr std::size_t chunk = specialized_chunk_size<Dimension>();
            for (std::size_t row = 0; row < Dimension; ++row) {
                const std::uint32_t* lhs_row = lhs.values.data() + row * Dimension;
                for (std::size_t col = 0; col < Dimension; ++col) {
                    const std::uint32_t* rhs_row = transposed.data() + col * Dimension;
                    std::uint64_t sum = 0;
                    for (std::size_t begin = 0; begin < Dimension; begin += chunk) {
                        const std::size_t end = std::min(Dimension, begin + chunk);
                        for (std::size_t k = begin; k < end; ++k) {
                            sum += static_cast<std::uint64_t>(lhs_row[k]) * rhs_row[k];
                        }
                        sum %= Mod;
                    }
                    out.values[row * Dimension + col] = static_cast<std::uint32_t>(sum);
                }
            }
        } else {
            constexpr std::size_t preferred_block =
                Dimension <= 192 ? 8 : (Dimension <= 512 ? 4 : 2);
            constexpr std::size_t row_block =
                Dimension % preferred_block == 0 ? preferred_block :
                (Dimension % 4 == 0 ? 4 : (Dimension % 2 == 0 ? 2 : 1));
            constexpr std::size_t chunk = specialized_chunk_size<Dimension>();
            std::array<std::uint64_t, row_block * Dimension> sums;

            // 次数と行ブロック幅を定数化し、固定長版と同じ自動 SIMD 向け loop を生成する
            for (std::size_t row_begin = 0; row_begin < Dimension;
                 row_begin += row_block) {
                std::fill(sums.begin(), sums.end(), 0ULL);
                for (std::size_t k_begin = 0; k_begin < Dimension; k_begin += chunk) {
                    const std::size_t k_end = std::min(Dimension, k_begin + chunk);
                    for (std::size_t k = k_begin; k < k_end; ++k) {
                        const std::uint32_t* __restrict__ rhs_row =
                            rhs.values.data() + k * Dimension;
                        for (std::size_t inner_row = 0; inner_row < row_block; ++inner_row) {
                            const std::uint64_t lhs_value =
                                lhs.values[(row_begin + inner_row) * Dimension + k];
                            std::uint64_t* __restrict__ sum_row =
                                sums.data() + inner_row * Dimension;
                            for (std::size_t col = 0; col < Dimension; ++col) {
                                sum_row[col] += lhs_value * rhs_row[col];
                            }
                        }
                    }
                    for (std::size_t inner_row = 0; inner_row < row_block; ++inner_row) {
                        std::uint64_t* sum_row = sums.data() + inner_row * Dimension;
                        for (std::size_t col = 0; col < Dimension; ++col) {
                            sum_row[col] %= Mod;
                        }
                    }
                }

                for (std::size_t inner_row = 0; inner_row < row_block; ++inner_row) {
                    const std::uint64_t* sum_row = sums.data() + inner_row * Dimension;
                    std::uint32_t* out_row =
                        out.values.data() + (row_begin + inner_row) * Dimension;
                    for (std::size_t col = 0; col < Dimension; ++col) {
                        out_row[col] = static_cast<std::uint32_t>(sum_row[col]);
                    }
                }
            }
        }
    }

    template <std::size_t Dimension>
    static bool try_multiply_specialized(dynamic_mod_matrix& out,
                                         const dynamic_mod_matrix& lhs,
                                         const dynamic_mod_matrix& rhs) {
        if (lhs.dimension_ != Dimension) return false;
        multiply_specialized<Dimension>(out, lhs, rhs);
        return true;
    }

    static bool try_requested_multiply(dynamic_mod_matrix& out,
                                       const dynamic_mod_matrix& lhs,
                                       const dynamic_mod_matrix& rhs) {
        if constexpr (sizeof...(FastDimensions) == 0) {
            return false;
        } else {
            return (try_multiply_specialized<FastDimensions>(out, lhs, rhs) || ...);
        }
    }

    static void multiply_to(dynamic_mod_matrix& out, const dynamic_mod_matrix& lhs,
                            const dynamic_mod_matrix& rhs, MultiplyWorkspace& workspace) {
        assert(out.dimension_ == lhs.dimension_ && lhs.dimension_ == rhs.dimension_);
        if (try_requested_multiply(out, lhs, rhs)) return;

        const std::size_t dimension = lhs.dimension_;
        if (dimension == 1) {
            out.values[0] = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(lhs.values[0]) * rhs.values[0] % Mod);
        } else if (dimension <= 7) {
            multiply_dot(out, lhs, rhs, workspace);
        } else {
            // 実行時次数では 1 行ずつの単純な kernel が複数行版より一貫して高速だった
            multiply_blocked<1>(out, lhs, rhs, workspace);
        }
    }

    static void multiply_vector_generic_to(std::uint32_t* out,
                                           const dynamic_mod_matrix& matrix,
                                           const std::uint32_t* vector) {
        const std::size_t dimension = matrix.dimension_;
        const std::size_t chunk = accumulation_chunk_size(dimension);
        std::uint32_t* __restrict__ output = out;
        const std::uint32_t* __restrict__ input = vector;

        // 行列の各行と列ベクトルを連続読み込みし、chunk 境界だけで剰余を取る
        for (std::size_t row = 0; row < dimension; ++row) {
            const std::uint32_t* __restrict__ matrix_row =
                matrix.values.data() + row * dimension;
            std::uint64_t sum = 0;
            for (std::size_t begin = 0; begin < dimension; begin += chunk) {
                const std::size_t end = std::min(dimension, begin + chunk);
                for (std::size_t col = begin; col < end; ++col) {
                    sum += static_cast<std::uint64_t>(matrix_row[col]) * input[col];
                }
                sum %= Mod;
            }
            output[row] = static_cast<std::uint32_t>(sum);
        }
    }

    template <std::size_t Dimension>
    static void multiply_vector_specialized_to(std::uint32_t* out,
                                                const dynamic_mod_matrix& matrix,
                                                const std::uint32_t* vector) {
        constexpr std::size_t chunk = specialized_chunk_size<Dimension>();
        std::uint32_t* __restrict__ output = out;
        const std::uint32_t* __restrict__ input = vector;

        // 次数を定数化し、内積 loop の展開と定数剰余最適化を促す
        for (std::size_t row = 0; row < Dimension; ++row) {
            const std::uint32_t* __restrict__ matrix_row =
                matrix.values.data() + row * Dimension;
            std::uint64_t sum = 0;
            for (std::size_t begin = 0; begin < Dimension; begin += chunk) {
                const std::size_t end = std::min(Dimension, begin + chunk);
                for (std::size_t col = begin; col < end; ++col) {
                    sum += static_cast<std::uint64_t>(matrix_row[col]) * input[col];
                }
                sum %= Mod;
            }
            output[row] = static_cast<std::uint32_t>(sum);
        }
    }

    template <std::size_t Dimension>
    static bool try_multiply_vector_specialized(std::uint32_t* out,
                                                const dynamic_mod_matrix& matrix,
                                                const std::uint32_t* vector) {
        if (matrix.dimension_ != Dimension) return false;
        multiply_vector_specialized_to<Dimension>(out, matrix, vector);
        return true;
    }

    static void multiply_vector_to(std::uint32_t* out, const dynamic_mod_matrix& matrix,
                                   const std::uint32_t* vector) {
        if constexpr (sizeof...(FastDimensions) != 0) {
            if ((try_multiply_vector_specialized<FastDimensions>(out, matrix, vector) || ...)) {
                return;
            }
        }
        multiply_vector_generic_to(out, matrix, vector);
    }

    dynamic_mod_matrix pow_binary(std::uint64_t exponent,
                                  MultiplyWorkspace& workspace) const {
        dynamic_mod_matrix result = *this;
        dynamic_mod_matrix scratch(dimension_, uninitialized_t{});
        dynamic_mod_matrix* current = &result;
        dynamic_mod_matrix* next = &scratch;

        // 出力先を 2 面化し、各行列積後の allocation と N^2 要素代入を避ける
        for (int bit = mod_matrix_detail::bit_length_u64(exponent) - 2; bit >= 0; --bit) {
            multiply_to(*next, *current, *current, workspace);
            std::swap(current, next);
            if (((exponent >> bit) & 1ULL) != 0) {
                multiply_to(*next, *current, *this, workspace);
                std::swap(current, next);
            }
        }

        if (current == &result) return result;
        return scratch;
    }

    dynamic_mod_matrix pow_window(std::uint64_t exponent, const PowerPlan& plan,
                                  MultiplyWorkspace& workspace) const {
        std::vector<dynamic_mod_matrix> odd_powers;
        odd_powers.reserve(plan.max_odd_index + 1);
        for (unsigned index = 0; index <= plan.max_odd_index; ++index) {
            odd_powers.push_back(dynamic_mod_matrix(dimension_, uninitialized_t{}));
        }
        odd_powers[0].values = values;

        // 必要な最大奇数冪までだけを A^2 ずつ掛けて事前計算する
        if (plan.max_odd_index != 0) {
            dynamic_mod_matrix squared(dimension_, uninitialized_t{});
            multiply_to(squared, *this, *this, workspace);
            for (unsigned index = 1; index <= plan.max_odd_index; ++index) {
                multiply_to(odd_powers[index], odd_powers[index - 1], squared, workspace);
            }
        }

        // 最上位の非零 window は代入だけで開始し、恒等行列との不要な乗算を省く
        int high = mod_matrix_detail::bit_length_u64(exponent) - 1;
        int low = 0;
        const unsigned first_window = extract_window(exponent, high, plan.width, low);
        dynamic_mod_matrix result = odd_powers[first_window >> 1];
        dynamic_mod_matrix scratch(dimension_, uninitialized_t{});
        dynamic_mod_matrix* current = &result;
        dynamic_mod_matrix* next = &scratch;
        high = low - 1;

        // 0 bit は平方 1 回、非零 window は bit 数回の平方と奇数冪 1 回で進める
        while (high >= 0) {
            if (((exponent >> high) & 1ULL) == 0) {
                multiply_to(*next, *current, *current, workspace);
                std::swap(current, next);
                --high;
                continue;
            }

            const unsigned window = extract_window(exponent, high, plan.width, low);
            const int square_count = high - low + 1;
            for (int count = 0; count < square_count; ++count) {
                multiply_to(*next, *current, *current, workspace);
                std::swap(current, next);
            }
            multiply_to(*next, *current, odd_powers[window >> 1], workspace);
            std::swap(current, next);
            high = low - 1;
        }

        if (current == &result) return result;
        return scratch;
    }

public:
    // 指定次数の全要素が 0 の行列を構築する。O(N^2)
    explicit dynamic_mod_matrix(std::size_t dimension)
        : dimension_(dimension), values(checked_element_count(dimension), 0U) {}

    // 指定次数の単位行列を返す。O(N^2)
    static dynamic_mod_matrix identity(std::size_t dimension) {
        dynamic_mod_matrix result(dimension);
        for (std::size_t index = 0; index < dimension; ++index) {
            result.values[index * dimension + index] = 1;
        }
        return result;
    }

    // 行列の次数を返す。O(1)
    std::size_t size() const noexcept { return dimension_; }

    // 正規化済みの (row, col) 要素への参照を返す。O(1)
    std::uint32_t& operator()(std::size_t row, std::size_t col) {
        assert(row < dimension_ && col < dimension_);
        return values[row * dimension_ + col];
    }

    // (row, col) 要素を返す。O(1)
    const std::uint32_t& operator()(std::size_t row, std::size_t col) const {
        assert(row < dimension_ && col < dimension_);
        return values[row * dimension_ + col];
    }

    // row-major 配列の先頭を返す。O(1)
    std::uint32_t* data() noexcept { return values.data(); }

    // row-major 配列の先頭を返す。O(1)
    const std::uint32_t* data() const noexcept { return values.data(); }

    // 非負値を法 Mod で正規化して (row, col) に設定する。O(1)
    void set(std::size_t row, std::size_t col, std::uint64_t value) {
        assert(row < dimension_ && col < dimension_);
        values[row * dimension_ + col] = static_cast<std::uint32_t>(value % Mod);
    }

    // 符号付き値を法 Mod で正規化して (row, col) に設定する。O(1)
    void set_signed(std::size_t row, std::size_t col, std::int64_t value) {
        assert(row < dimension_ && col < dimension_);
        std::int64_t reduced = value % Mod;
        if (reduced < 0) reduced += Mod;
        values[row * dimension_ + col] = static_cast<std::uint32_t>(reduced);
    }

    // 同じ次数の右辺との行列積を返す。O(N^3)
    dynamic_mod_matrix operator*(const dynamic_mod_matrix& rhs) const {
        assert(dimension_ == rhs.dimension_);
        dynamic_mod_matrix result(dimension_, uninitialized_t{});
        MultiplyWorkspace workspace;
        multiply_to(result, *this, rhs, workspace);
        return result;
    }

    // 同じ次数の右辺との行列積を自身へ代入する。O(N^3)
    dynamic_mod_matrix& operator*=(const dynamic_mod_matrix& rhs) {
        *this = *this * rhs;
        return *this;
    }

    // 列ベクトル vector との積を返す。O(N^2)
    vector_type operator*(const vector_type& vector) const {
        assert(vector.size() == dimension_);
        vector_type result(dimension_);
        multiply_vector_to(result.data(), *this, vector.data());
        return result;
    }

    // A^exponent * vector を累乗行列を構築せず返す。O(N^3 log exponent)
    vector_type pow_apply(std::uint64_t exponent, const vector_type& vector) const {
        assert(vector.size() == dimension_);
        if (exponent == 0) return vector;
        if (exponent == 1) return *this * vector;

        // 1x1 は scalar 累乗 1 回と scalar 乗算 1 回だけで処理する
        if (dimension_ == 1) {
            std::uint64_t scalar_result = 1;
            std::uint64_t scalar_base = values[0];
            std::uint64_t remaining = exponent;
            while (remaining != 0) {
                if ((remaining & 1ULL) != 0) {
                    scalar_result = scalar_result * scalar_base % Mod;
                }
                remaining >>= 1;
                if (remaining != 0) scalar_base = scalar_base * scalar_base % Mod;
            }
            return vector_type{
                static_cast<std::uint32_t>(scalar_result * vector[0] % Mod)};
        }

        vector_type result = vector;
        vector_type vector_scratch(dimension_);
        vector_type* current_vector = &result;
        vector_type* next_vector = &vector_scratch;
        dynamic_mod_matrix square_a(dimension_, uninitialized_t{});
        dynamic_mod_matrix square_b(dimension_, uninitialized_t{});
        const dynamic_mod_matrix* current_matrix = this;
        dynamic_mod_matrix* next_matrix = &square_a;
        MultiplyWorkspace workspace;
        std::uint64_t remaining = exponent;

        // 行列冪は bit ごとの平方だけで作り、選択した冪を O(N^2) で列ベクトルへ作用させる
        while (remaining != 0) {
            if ((remaining & 1ULL) != 0) {
                multiply_vector_to(next_vector->data(), *current_matrix,
                                   current_vector->data());
                std::swap(current_vector, next_vector);
            }
            remaining >>= 1;
            if (remaining == 0) break;
            multiply_to(*next_matrix, *current_matrix, *current_matrix, workspace);
            current_matrix = next_matrix;
            next_matrix = next_matrix == &square_a ? &square_b : &square_a;
        }

        if (current_vector == &result) return result;
        return vector_scratch;
    }

    // 自身の exponent 乗を返す。O(N^3 log exponent)
    dynamic_mod_matrix pow(std::uint64_t exponent) const {
        if (exponent == 0) return identity(dimension_);
        if (exponent == 1) return *this;

        // 1x1 は scalar 累乗、2x2 は window 管理コストを避けた二進法で処理する
        if (dimension_ == 1) {
            dynamic_mod_matrix result(1, uninitialized_t{});
            std::uint64_t scalar_result = 1;
            std::uint64_t scalar_base = values[0];
            while (exponent != 0) {
                if ((exponent & 1ULL) != 0) scalar_result = scalar_result * scalar_base % Mod;
                exponent >>= 1;
                if (exponent != 0) scalar_base = scalar_base * scalar_base % Mod;
            }
            result.values[0] = static_cast<std::uint32_t>(scalar_result);
            return result;
        }

        MultiplyWorkspace workspace;
        if (dimension_ == 2) return pow_binary(exponent, workspace);
        const PowerPlan plan = select_power_plan(exponent);
        return plan.width == 1 ? pow_binary(exponent, workspace)
                               : pow_window(exponent, plan, workspace);
    }

    // 次数と全要素が等しい場合に true を返す。O(N^2)
    friend bool operator==(const dynamic_mod_matrix&, const dynamic_mod_matrix&) = default;
};

namespace mod_matrix_detail {

template <std::uint32_t Mod, class Sequence>
struct make_bounded_dynamic_mod_matrix;

template <std::uint32_t Mod, std::size_t... Index>
struct make_bounded_dynamic_mod_matrix<Mod, std::index_sequence<Index...>> {
    static_assert(sizeof...(Index) >= 1,
                  "bounded_dynamic_mod_matrix の最大次数は 1 以上でなければならない");
    using type = dynamic_mod_matrix<Mod, (Index + 1)...>;
};

} // namespace mod_matrix_detail

// 1 から MaxFastDimension の各次数を定数化した動的行列型を定義する
// MaxFastDimension を大きくするとコンパイル時間とコードサイズが増える
// 入力次数が範囲外でも汎用 kernel で動作する
template <std::uint32_t Mod, std::size_t MaxFastDimension>
using bounded_dynamic_mod_matrix = typename mod_matrix_detail::make_bounded_dynamic_mod_matrix<
    Mod, std::make_index_sequence<MaxFastDimension>>::type;


#if __INCLUDE_LEVEL__ == 0
#include <chrono>
#include <iostream>
#include <random>
#include <string>

namespace mod_matrix_selftest {

using Clock = std::chrono::steady_clock;
inline volatile std::uint64_t benchmark_sink = 0;

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "test failed: " << message << '\n';
        std::exit(1);
    }
}

template <std::uint32_t Mod, std::size_t N>
mod_matrix<Mod, N> naive_multiply(const mod_matrix<Mod, N>& lhs,
                                  const mod_matrix<Mod, N>& rhs) {
    mod_matrix<Mod, N> out;
    for (std::size_t row = 0; row < N; ++row) {
        for (std::size_t col = 0; col < N; ++col) {
            __uint128_t sum = 0;
            for (std::size_t k = 0; k < N; ++k) {
                sum += static_cast<__uint128_t>(lhs(row, k)) * rhs(k, col);
                sum %= Mod;
            }
            out(row, col) = static_cast<std::uint32_t>(sum);
        }
    }
    return out;
}

template <std::uint32_t Mod, std::size_t N>
mod_matrix<Mod, N> naive_power(mod_matrix<Mod, N> base, std::uint64_t exponent) {
    mod_matrix<Mod, N> result = mod_matrix<Mod, N>::identity();
    while (exponent != 0) {
        if ((exponent & 1ULL) != 0) result = naive_multiply(result, base);
        exponent >>= 1;
        if (exponent != 0) base = naive_multiply(base, base);
    }
    return result;
}


template <std::uint32_t Mod, std::size_t N>
typename mod_matrix<Mod, N>::vector_type naive_multiply_vector(
    const mod_matrix<Mod, N>& matrix,
    const typename mod_matrix<Mod, N>::vector_type& vector) {
    typename mod_matrix<Mod, N>::vector_type result{};
    for (std::size_t row = 0; row < N; ++row) {
        __uint128_t sum = 0;
        for (std::size_t col = 0; col < N; ++col) {
            sum += static_cast<__uint128_t>(matrix(row, col)) * vector[col];
            sum %= Mod;
        }
        result[row] = static_cast<std::uint32_t>(sum);
    }
    return result;
}

template <std::uint32_t Mod>
dynamic_mod_matrix<Mod> naive_dynamic_multiply(const dynamic_mod_matrix<Mod>& lhs,
                                                const dynamic_mod_matrix<Mod>& rhs) {
    const std::size_t dimension = lhs.size();
    dynamic_mod_matrix<Mod> result(dimension);
    for (std::size_t row = 0; row < dimension; ++row) {
        for (std::size_t col = 0; col < dimension; ++col) {
            __uint128_t sum = 0;
            for (std::size_t k = 0; k < dimension; ++k) {
                sum += static_cast<__uint128_t>(lhs(row, k)) * rhs(k, col);
                sum %= Mod;
            }
            result(row, col) = static_cast<std::uint32_t>(sum);
        }
    }
    return result;
}

template <std::uint32_t Mod>
dynamic_mod_matrix<Mod> naive_dynamic_power(dynamic_mod_matrix<Mod> base,
                                             std::uint64_t exponent) {
    dynamic_mod_matrix<Mod> result = dynamic_mod_matrix<Mod>::identity(base.size());
    while (exponent != 0) {
        if ((exponent & 1ULL) != 0) result = naive_dynamic_multiply(result, base);
        exponent >>= 1;
        if (exponent != 0) base = naive_dynamic_multiply(base, base);
    }
    return result;
}

template <std::uint32_t Mod>
typename dynamic_mod_matrix<Mod>::vector_type naive_dynamic_multiply_vector(
    const dynamic_mod_matrix<Mod>& matrix,
    const typename dynamic_mod_matrix<Mod>::vector_type& vector) {
    const std::size_t dimension = matrix.size();
    typename dynamic_mod_matrix<Mod>::vector_type result(dimension);
    for (std::size_t row = 0; row < dimension; ++row) {
        __uint128_t sum = 0;
        for (std::size_t col = 0; col < dimension; ++col) {
            sum += static_cast<__uint128_t>(matrix(row, col)) * vector[col];
            sum %= Mod;
        }
        result[row] = static_cast<std::uint32_t>(sum);
    }
    return result;
}

std::uint64_t splitmix64(std::uint64_t& state) {
    state += 0x9e3779b97f4a7c15ULL;
    std::uint64_t value = state;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

template <std::uint32_t Mod, std::size_t N>
mod_matrix<Mod, N> random_matrix(std::uint64_t& state) {
    mod_matrix<Mod, N> result;
    for (std::size_t index = 0; index < N * N; ++index) {
        result.data()[index] = static_cast<std::uint32_t>(splitmix64(state) % Mod);
    }
    return result;
}


template <std::uint32_t Mod, std::size_t N>
typename mod_matrix<Mod, N>::vector_type random_vector(std::uint64_t& state) {
    typename mod_matrix<Mod, N>::vector_type result{};
    for (std::size_t index = 0; index < N; ++index) {
        result[index] = static_cast<std::uint32_t>(splitmix64(state) % Mod);
    }
    return result;
}

template <std::uint32_t Mod>
dynamic_mod_matrix<Mod> random_dynamic_matrix(std::size_t dimension,
                                               std::uint64_t& state) {
    dynamic_mod_matrix<Mod> result(dimension);
    for (std::size_t index = 0; index < dimension * dimension; ++index) {
        result.data()[index] = static_cast<std::uint32_t>(splitmix64(state) % Mod);
    }
    return result;
}

template <std::uint32_t Mod>
typename dynamic_mod_matrix<Mod>::vector_type random_dynamic_vector(
    std::size_t dimension, std::uint64_t& state) {
    typename dynamic_mod_matrix<Mod>::vector_type result(dimension);
    for (std::uint32_t& value : result) {
        value = static_cast<std::uint32_t>(splitmix64(state) % Mod);
    }
    return result;
}

template <std::uint32_t Mod, std::size_t N>
void test_random_multiplication(std::size_t trials, std::uint64_t& state) {
    for (std::size_t trial = 0; trial < trials; ++trial) {
        const auto lhs = random_matrix<Mod, N>(state);
        const auto rhs = random_matrix<Mod, N>(state);
        require(lhs * rhs == naive_multiply(lhs, rhs),
                "random multiplication Mod=" + std::to_string(Mod) +
                    " N=" + std::to_string(N));
    }
}

void test_bit_length_u64() {
    // 0、境界値、2 の冪、その直後を確認してビット長計算の回帰を防ぐ
    require(mod_matrix_detail::bit_length_u64(0) == 0, "bit length zero");
    require(mod_matrix_detail::bit_length_u64(1) == 1, "bit length one");
    require(mod_matrix_detail::bit_length_u64(2) == 2, "bit length power of two");
    require(mod_matrix_detail::bit_length_u64(3) == 2, "bit length non power of two");
    require(mod_matrix_detail::bit_length_u64(1ULL << 63) == 64,
            "bit length highest bit");
    require(mod_matrix_detail::bit_length_u64(
                std::numeric_limits<std::uint64_t>::max()) == 64,
            "bit length maximum");
}

void test_basic_api() {
    using Matrix = mod_matrix<998244353, 3>;
    Matrix zero;
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t col = 0; col < 3; ++col) require(zero(row, col) == 0, "zero matrix");
    }

    const Matrix identity = Matrix::identity();
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t col = 0; col < 3; ++col) {
            require(identity(row, col) == (row == col ? 1U : 0U), "identity matrix");
        }
    }

    Matrix matrix;
    matrix.set(0, 0, 998244354ULL);
    matrix.set_signed(0, 1, -1);
    matrix(0, 2) = 7;
    require(matrix(0, 0) == 1, "set normalization");
    require(matrix(0, 1) == 998244352, "set_signed normalization");
    require(matrix.data()[2] == 7, "row-major data");
}

void test_known_multiplication() {
    using Matrix = mod_matrix<1000000007, 2>;
    Matrix lhs;
    Matrix rhs;
    lhs(0, 0) = 1;
    lhs(0, 1) = 2;
    lhs(1, 0) = 3;
    lhs(1, 1) = 4;
    rhs(0, 0) = 5;
    rhs(0, 1) = 6;
    rhs(1, 0) = 7;
    rhs(1, 1) = 8;
    const Matrix product = lhs * rhs;
    require(product(0, 0) == 19 && product(0, 1) == 22 &&
                product(1, 0) == 43 && product(1, 1) == 50,
            "known 2x2 multiplication");

    Matrix assigned = lhs;
    assigned *= rhs;
    require(assigned == product, "operator*=");
}

void test_multiplication() {
    std::uint64_t state = 0x123456789abcdef0ULL;
    test_random_multiplication<2, 1>(200, state);
    test_random_multiplication<2, 8>(100, state);
    test_random_multiplication<17, 3>(200, state);
    test_random_multiplication<998244353, 2>(300, state);
    test_random_multiplication<998244353, 3>(200, state);
    test_random_multiplication<998244353, 7>(100, state);
    test_random_multiplication<998244353, 8>(100, state);
    test_random_multiplication<998244353, 18>(30, state);
    test_random_multiplication<998244353, 19>(30, state);
    test_random_multiplication<998244353, 32>(8, state);
    test_random_multiplication<1000000007, 24>(20, state);
    test_random_multiplication<4294967295U, 2>(200, state);
    test_random_multiplication<4294967295U, 8>(50, state);
}

void test_power() {
    using Matrix = mod_matrix<998244353, 3>;
    std::uint64_t state = 0xfedcba9876543210ULL;
    const Matrix matrix = random_matrix<998244353, 3>(state);
    require(matrix.pow(0) == Matrix::identity(), "power zero");
    require(matrix.pow(1) == matrix, "power one");

    using ScalarMatrix = mod_matrix<1000000007, 1>;
    ScalarMatrix scalar;
    scalar(0, 0) = 123456789;
    require(scalar.pow(std::numeric_limits<std::uint64_t>::max()) ==
                naive_power(scalar, std::numeric_limits<std::uint64_t>::max()),
            "1x1 scalar power");

    // 小さい指数を全走査し、window 境界を含む全 bit pattern を確認する
    for (std::uint64_t exponent = 0; exponent <= 512; ++exponent) {
        require(matrix.pow(exponent) == naive_power(matrix, exponent),
                "power exhaustive exponent=" + std::to_string(exponent));
    }

    // 64 bit の疎・密な指数と端値を naive 二進法で照合する
    const std::array<std::uint64_t, 8> exponents{
        2ULL,
        3ULL,
        0xaaaaaaaaaaaaaaaaULL,
        0xfedcba9876543210ULL,
        (1ULL << 63),
        (1ULL << 63) | 1ULL,
        std::numeric_limits<std::uint64_t>::max() - 1,
        std::numeric_limits<std::uint64_t>::max(),
    };
    for (const std::uint64_t exponent : exponents) {
        require(matrix.pow(exponent) == naive_power(matrix, exponent),
                "power 64-bit exponent=" + std::to_string(exponent));
    }
}


void test_vector_operations() {
    using Matrix = mod_matrix<998244353, 2>;
    Matrix matrix;
    matrix(0, 0) = 1;
    matrix(0, 1) = 2;
    matrix(1, 0) = 3;
    matrix(1, 1) = 4;
    const Matrix::vector_type known_vector{5, 6};
    require(matrix * known_vector == Matrix::vector_type{17, 39},
            "known matrix vector multiplication");
    require(matrix.pow_apply(std::numeric_limits<std::uint64_t>::max(), known_vector) ==
                matrix.pow(std::numeric_limits<std::uint64_t>::max()) * known_vector,
            "fixed 2x2 pow_apply");

    std::uint64_t state = 0x243f6a8885a308d3ULL;
    for (std::size_t trial = 0; trial < 300; ++trial) {
        const auto current_matrix = random_matrix<998244353, 3>(state);
        const auto vector = random_vector<998244353, 3>(state);
        require(current_matrix * vector == naive_multiply_vector(current_matrix, vector),
                "fixed random matrix vector multiplication");
    }

    const auto power_matrix = random_matrix<998244353, 3>(state);
    const auto power_vector = random_vector<998244353, 3>(state);
    for (std::uint64_t exponent = 0; exponent <= 256; ++exponent) {
        require(power_matrix.pow_apply(exponent, power_vector) ==
                    power_matrix.pow(exponent) * power_vector,
                "fixed pow_apply exhaustive exponent=" + std::to_string(exponent));
    }

    const std::array<std::uint64_t, 5> exponents{
        0xaaaaaaaaaaaaaaaaULL,
        0xfedcba9876543210ULL,
        1ULL << 63,
        (1ULL << 63) | 1ULL,
        std::numeric_limits<std::uint64_t>::max(),
    };
    for (const std::uint64_t exponent : exponents) {
        require(power_matrix.pow_apply(exponent, power_vector) ==
                    power_matrix.pow(exponent) * power_vector,
                "fixed pow_apply 64-bit exponent=" + std::to_string(exponent));
    }

    using ScalarMatrix = mod_matrix<4294967295U, 1>;
    ScalarMatrix scalar;
    scalar(0, 0) = 4294967294U;
    const ScalarMatrix::vector_type scalar_vector{4294967294U};
    require(scalar.pow_apply(std::numeric_limits<std::uint64_t>::max(), scalar_vector) ==
                scalar.pow(std::numeric_limits<std::uint64_t>::max()) * scalar_vector,
            "fixed 1x1 pow_apply maximum modulus");
}

template <std::uint32_t Mod>
void test_dynamic_random_multiplication(std::size_t dimension, std::size_t trials,
                                        std::uint64_t& state) {
    for (std::size_t trial = 0; trial < trials; ++trial) {
        const auto lhs = random_dynamic_matrix<Mod>(dimension, state);
        const auto rhs = random_dynamic_matrix<Mod>(dimension, state);
        require(lhs * rhs == naive_dynamic_multiply(lhs, rhs),
                "dynamic random multiplication Mod=" + std::to_string(Mod) +
                    " N=" + std::to_string(dimension));
    }
}

void test_dynamic_matrix() {
    using Matrix = dynamic_mod_matrix<998244353>;
    Matrix zero(3);
    require(zero.size() == 3, "dynamic size");
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t col = 0; col < 3; ++col) {
            require(zero(row, col) == 0, "dynamic zero matrix");
        }
    }

    const Matrix identity = Matrix::identity(3);
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t col = 0; col < 3; ++col) {
            require(identity(row, col) == (row == col ? 1U : 0U),
                    "dynamic identity matrix");
        }
    }

    Matrix normalized(2);
    normalized.set(0, 0, 998244354ULL);
    normalized.set_signed(0, 1, -1);
    require(normalized(0, 0) == 1 && normalized(0, 1) == 998244352U,
            "dynamic normalization");

    bool rejected_zero = false;
    try {
        const Matrix invalid(0);
        (void)invalid;
    } catch (const std::length_error&) {
        rejected_zero = true;
    }
    require(rejected_zero, "dynamic rejects zero dimension");

    std::uint64_t state = 0x13198a2e03707344ULL;
    test_dynamic_random_multiplication<2>(1, 200, state);
    test_dynamic_random_multiplication<17>(3, 150, state);
    test_dynamic_random_multiplication<998244353>(2, 200, state);
    test_dynamic_random_multiplication<998244353>(7, 80, state);
    test_dynamic_random_multiplication<998244353>(8, 80, state);
    test_dynamic_random_multiplication<998244353>(18, 20, state);
    test_dynamic_random_multiplication<998244353>(19, 20, state);
    test_dynamic_random_multiplication<998244353>(32, 5, state);
    test_dynamic_random_multiplication<4294967295U>(8, 40, state);

    const Matrix matrix = random_dynamic_matrix<998244353>(3, state);
    const Matrix::vector_type vector = random_dynamic_vector<998244353>(3, state);
    require(matrix * vector == naive_dynamic_multiply_vector(matrix, vector),
            "dynamic matrix vector multiplication");
    require(matrix.pow(0) == Matrix::identity(3), "dynamic power zero");
    require(matrix.pow(1) == matrix, "dynamic power one");

    // 小さい指数を全走査し、動的版の window 累乗と pow_apply を同時に検証する
    for (std::uint64_t exponent = 0; exponent <= 256; ++exponent) {
        const Matrix expected = naive_dynamic_power(matrix, exponent);
        require(matrix.pow(exponent) == expected,
                "dynamic power exhaustive exponent=" + std::to_string(exponent));
        require(matrix.pow_apply(exponent, vector) == expected * vector,
                "dynamic pow_apply exhaustive exponent=" + std::to_string(exponent));
    }

    const std::array<std::uint64_t, 4> exponents{
        0xaaaaaaaaaaaaaaaaULL,
        0xfedcba9876543210ULL,
        (1ULL << 63) | 1ULL,
        std::numeric_limits<std::uint64_t>::max(),
    };
    for (const std::uint64_t exponent : exponents) {
        const Matrix expected = naive_dynamic_power(matrix, exponent);
        require(matrix.pow(exponent) == expected,
                "dynamic power 64-bit exponent=" + std::to_string(exponent));
        require(matrix.pow_apply(exponent, vector) == expected * vector,
                "dynamic pow_apply 64-bit exponent=" + std::to_string(exponent));
    }

    using ScalarMatrix = dynamic_mod_matrix<1000000007>;
    ScalarMatrix scalar(1);
    scalar(0, 0) = 123456789U;
    const ScalarMatrix::vector_type scalar_vector{987654321U};
    const std::uint64_t exponent = std::numeric_limits<std::uint64_t>::max();
    require(scalar.pow_apply(exponent, scalar_vector) == scalar.pow(exponent) * scalar_vector,
            "dynamic 1x1 pow_apply");
}

void test_dynamic_specialization() {
    using FastMatrix = dynamic_mod_matrix<998244353, 3, 8, 19>;
    std::uint64_t state = 0xa4093822299f31d0ULL;

    // 指定した次数と指定外の次数の双方で汎用版と同じ結果になることを確認する
    for (const std::size_t dimension : {3U, 7U, 8U, 19U}) {
        FastMatrix lhs(dimension);
        FastMatrix rhs(dimension);
        for (std::size_t index = 0; index < dimension * dimension; ++index) {
            lhs.data()[index] = static_cast<std::uint32_t>(splitmix64(state) % 998244353U);
            rhs.data()[index] = static_cast<std::uint32_t>(splitmix64(state) % 998244353U);
        }
        const FastMatrix product = lhs * rhs;
        for (std::size_t row = 0; row < dimension; ++row) {
            for (std::size_t col = 0; col < dimension; ++col) {
                __uint128_t sum = 0;
                for (std::size_t k = 0; k < dimension; ++k) {
                    sum += static_cast<__uint128_t>(lhs(row, k)) * rhs(k, col);
                    sum %= 998244353U;
                }
                require(product(row, col) == sum,
                        "dynamic specialized multiplication N=" +
                            std::to_string(dimension));
            }
        }

        FastMatrix::vector_type vector(dimension);
        for (std::uint32_t& value : vector) {
            value = static_cast<std::uint32_t>(splitmix64(state) % 998244353U);
        }
        constexpr std::uint64_t exponent = 0xfedcba9876543210ULL;
        require(lhs.pow_apply(exponent, vector) == lhs.pow(exponent) * vector,
                "dynamic specialized pow_apply N=" + std::to_string(dimension));
    }

    using BoundedMatrix = bounded_dynamic_mod_matrix<998244353, 16>;
    for (std::size_t dimension = 1; dimension <= 16; ++dimension) {
        BoundedMatrix lhs(dimension);
        BoundedMatrix rhs(dimension);
        BoundedMatrix::vector_type vector(dimension);
        for (std::size_t index = 0; index < dimension * dimension; ++index) {
            lhs.data()[index] =
                static_cast<std::uint32_t>(splitmix64(state) % 998244353U);
            rhs.data()[index] =
                static_cast<std::uint32_t>(splitmix64(state) % 998244353U);
        }
        for (std::uint32_t& value : vector) {
            value = static_cast<std::uint32_t>(splitmix64(state) % 998244353U);
        }

        const BoundedMatrix product = lhs * rhs;
        for (std::size_t row = 0; row < dimension; ++row) {
            for (std::size_t col = 0; col < dimension; ++col) {
                __uint128_t sum = 0;
                for (std::size_t k = 0; k < dimension; ++k) {
                    sum += static_cast<__uint128_t>(lhs(row, k)) * rhs(k, col);
                    sum %= 998244353U;
                }
                require(product(row, col) == sum,
                        "bounded dynamic multiplication N=" +
                            std::to_string(dimension));
            }
        }
        require(lhs.pow_apply(37, vector) == lhs.pow(37) * vector,
                "bounded dynamic pow_apply N=" + std::to_string(dimension));
    }

    using MaximumModMatrix = dynamic_mod_matrix<4294967295U, 8, 10, 15>;
    for (const std::size_t dimension : {8U, 10U, 15U}) {
        MaximumModMatrix lhs(dimension);
        MaximumModMatrix rhs(dimension);
        for (std::size_t index = 0; index < dimension * dimension; ++index) {
            lhs.data()[index] = static_cast<std::uint32_t>(splitmix64(state));
            rhs.data()[index] = static_cast<std::uint32_t>(splitmix64(state));
        }
        const MaximumModMatrix product = lhs * rhs;
        for (std::size_t row = 0; row < dimension; ++row) {
            for (std::size_t col = 0; col < dimension; ++col) {
                __uint128_t sum = 0;
                for (std::size_t k = 0; k < dimension; ++k) {
                    sum += static_cast<__uint128_t>(lhs(row, k)) * rhs(k, col);
                    sum %= 4294967295U;
                }
                require(product(row, col) == sum,
                        "dynamic specialized maximum modulus N=" +
                            std::to_string(dimension));
            }
        }
    }
}

void run_tests() {
    test_bit_length_u64();
    test_basic_api();
    test_known_multiplication();
    test_multiplication();
    test_power();
    test_vector_operations();
    test_dynamic_matrix();
    test_dynamic_specialization();
    std::cout << "all tests passed\n";
}

template <std::uint32_t Mod, std::size_t N>
std::uint64_t checksum(const mod_matrix<Mod, N>& matrix) {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (std::size_t index = 0; index < N * N; ++index) {
        hash ^= matrix.data()[index];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

template <class F>
double median_ns(F&& callback, std::size_t operations) {
    std::array<double, 5> elapsed{};
    for (double& value : elapsed) {
        const auto begin = Clock::now();
        const std::uint64_t result = callback();
        const auto end = Clock::now();
        benchmark_sink ^= result;
        value = std::chrono::duration<double, std::nano>(end - begin).count() /
                static_cast<double>(operations);
    }
    std::sort(elapsed.begin(), elapsed.end());
    return elapsed[elapsed.size() / 2];
}

template <std::size_t N>
mod_matrix<998244353, N> benchmark_baseline_multiply(
    const mod_matrix<998244353, N>& lhs,
    const mod_matrix<998244353, N>& rhs) {
    mod_matrix<998244353, N> out;
    for (std::size_t row = 0; row < N; ++row) {
        for (std::size_t k = 0; k < N; ++k) {
            const std::uint64_t lhs_value = lhs(row, k);
            for (std::size_t col = 0; col < N; ++col) {
                out(row, col) = static_cast<std::uint32_t>(
                    (out(row, col) + lhs_value * rhs(k, col)) % 998244353ULL);
            }
        }
    }
    return out;
}

template <std::size_t N>
void benchmark_multiplication_size(std::uint64_t& state) {
    using Matrix = mod_matrix<998244353, N>;
    const Matrix lhs = random_matrix<998244353, N>(state);
    const Matrix rhs = random_matrix<998244353, N>(state);
    const std::size_t repetitions = std::max<std::size_t>(
        2, std::min<std::size_t>(500000, 20000000ULL / (N * N * N)));

    const double optimized_ns = median_ns(
        [&] {
            Matrix value = lhs;
            for (std::size_t repeat = 0; repeat < repetitions; ++repeat) value = value * rhs;
            return checksum(value);
        },
        repetitions);
    const double baseline_ns = median_ns(
        [&] {
            Matrix value = lhs;
            for (std::size_t repeat = 0; repeat < repetitions; ++repeat) {
                value = benchmark_baseline_multiply(value, rhs);
            }
            return checksum(value);
        },
        repetitions);

    std::cout << N << ',' << repetitions << ',' << optimized_ns << ',' << baseline_ns << ','
              << baseline_ns / optimized_ns << '\n';
}

template <std::size_t N>
mod_matrix<998244353, N> benchmark_binary_power(
    const mod_matrix<998244353, N>& matrix, std::uint64_t exponent) {
    using Matrix = mod_matrix<998244353, N>;
    if (exponent == 0) return Matrix::identity();
    Matrix result = matrix;
    for (int bit = mod_matrix_detail::bit_length_u64(exponent) - 2; bit >= 0; --bit) {
        result = result * result;
        if (((exponent >> bit) & 1ULL) != 0) result = result * matrix;
    }
    return result;
}

template <std::size_t N>
void benchmark_power_size(std::uint64_t& state, std::uint64_t exponent) {
    using Matrix = mod_matrix<998244353, N>;
    const Matrix matrix = random_matrix<998244353, N>(state);
    const std::size_t repetitions = N <= 4 ? 3000 : (N <= 16 ? 100 : 5);

    const double optimized_ns = median_ns(
        [&] {
            std::uint64_t result = 0;
            for (std::size_t repeat = 0; repeat < repetitions; ++repeat) {
                const std::uint64_t current_exponent =
                    (exponent ^ (repeat * 0x9e3779b97f4a7c15ULL)) | (1ULL << 63);
                result ^= checksum(matrix.pow(current_exponent));
            }
            return result;
        },
        repetitions);
    const double binary_ns = median_ns(
        [&] {
            std::uint64_t result = 0;
            for (std::size_t repeat = 0; repeat < repetitions; ++repeat) {
                const std::uint64_t current_exponent =
                    (exponent ^ (repeat * 0x9e3779b97f4a7c15ULL)) | (1ULL << 63);
                result ^= checksum(benchmark_binary_power(matrix, current_exponent));
            }
            return result;
        },
        repetitions);

    std::cout << N << ',' << repetitions << ',' << optimized_ns << ',' << binary_ns << ','
              << binary_ns / optimized_ns << '\n';
}

void run_benchmarks() {
    std::uint64_t state = 0x3141592653589793ULL;
    std::cout << "benchmarks begin\n";
    std::cout << "compiler," << __VERSION__ << '\n';
    std::cout << "flags,-std=c++20 -O2 -march=native -DNDEBUG\n";
    std::cout << "multiply_N,repetitions,optimized_ns,per_term_mod_ns,speedup\n";
    benchmark_multiplication_size<2>(state);
    benchmark_multiplication_size<4>(state);
    benchmark_multiplication_size<8>(state);
    benchmark_multiplication_size<16>(state);
    benchmark_multiplication_size<32>(state);
    benchmark_multiplication_size<64>(state);
    benchmark_multiplication_size<128>(state);

    constexpr std::uint64_t dense_exponent = 0xfedcba9876543210ULL;
    std::cout << "power_N,repetitions,optimized_pow_ns,binary_ns,speedup\n";
    benchmark_power_size<2>(state, dense_exponent);
    benchmark_power_size<3>(state, dense_exponent);
    benchmark_power_size<4>(state, dense_exponent);
    benchmark_power_size<5>(state, dense_exponent);
    benchmark_power_size<7>(state, dense_exponent);
    benchmark_power_size<8>(state, dense_exponent);
    benchmark_power_size<16>(state, dense_exponent);
    benchmark_power_size<32>(state, dense_exponent);
    std::cout << "benchmark_sink," << benchmark_sink << '\n';
}

} // namespace mod_matrix_selftest

int main() {
    mod_matrix_selftest::run_tests();
#ifndef MOD_MATRIX_SKIP_BENCHMARK
    mod_matrix_selftest::run_benchmarks();
#endif
    return 0;
}
#endif

// 実行結果(atcoder)
// all tests passed
// benchmarks begin
// compiler,15.2.0
// flags,-std=c++20 -O2 -march=native -DNDEBUG
// multiply_N,repetitions,optimized_ns,per_term_mod_ns,speedup
// 2,500000,5.01219,13.7853,2.75037
// 4,312500,50.0899,67.1193,1.33998
// 8,39062,109.558,451.397,4.12018
// 16,4882,798.056,3564.15,4.46605
// 32,610,6069.2,28035.2,4.61926
// 64,76,44821.9,224918,5.01805
// 128,9,351170,1.79313e+06,5.10616
// power_N,repetitions,optimized_pow_ns,binary_ns,speedup
// 2,3000,707.371,699.303,0.988594
// 3,3000,2210.66,2512.29,1.13644
// 4,3000,3094.43,4491.33,1.45143
// 5,100,4101.21,5466.57,1.33292
// 7,100,13225.7,15647.7,1.18313
// 8,100,8848.33,10676,1.20656
// 16,100,64754.7,74791.1,1.15499
// 32,5,503502,545231,1.08288
// benchmark_sink,0
