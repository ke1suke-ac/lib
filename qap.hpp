/*
 * qap_solver v0.9.1: 二次割当問題を制限時間内に近似的に解く単一ヘッダー
 *
 * 物体 i を位置 p[i] に一対一で割り当て、次の値を最小化する。
 *   sum_i unary[i][p[i]] + sum_i sum_j flow[i][j] * distance[p[i]][p[j]]
 *
 * 主な機能:
 * - qap_problem / qap_result: 問題・配置・目的値を保持し、全評価と swap 差分を計算
 * - qap_result は cout/cerr へ直接出力可能。配置は先頭20要素まで、128bit整数の費用も表示
 * - solve_qap / improve_qap: 初期解生成・既存解改善。Threshold Accepting と Robust Tabu Search を使用
 * - qap_prepared: 対称性判定と疎 flow の CSR を再利用して solve / improve / swap_delta を実行
 * - qap_swap_state: 置換と目的値を保持し、独自探索から swap 差分計算・交換を実行
 * - make_qap_subproblem: 可動物体だけの厳密な縮約 QAP を作り、restore で完全配置へ復元
 * - improve_qap_subset: 指定外の物体を固定し、可動物体が現在使う位置集合内で改善
 * - 非対称、負値、非ゼロ対角、単項コスト、__int128_t の目的値型に対応
 * - 疎 flow の隣接リスト、対称・ゼロ対角の簡略差分式を自動利用
 * - 支配的な単項では、交互道による割当候補と時間制限付きの最小費用割当を利用
 * - 差分表構築が残り時間に収まらない場合は Threshold Accepting を継続
 *
 * 使用例:
 *   qap_problem<int, long long> problem(n);
 *   problem.flow[i * n + j] = flow_ij;
 *   problem.distance[x * n + y] = distance_xy;
 *   problem.unary.assign(n * n, 0);  // 単項コストが必要な場合だけ確保
 *   problem.unary[i * n + x] = unary_ix;
 *   const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1900);
 *   auto result = solve_qap(problem, deadline, 12345);
 *   auto improved = improve_qap(problem, result.location_of, deadline, 12345);
 *   auto repaired = improve_qap_subset(problem, result.location_of, movable_objects,
 *                                      result.cost, deadline, 12345);
 *   // result.location_of[i]: 物体 i の位置、result.cost: 目的関数値
 *   std::cerr << result << '\n';  // 改行は呼出し側で指定
 *
 * 前提・注意:
 * - GCC / C++20、シングルスレッド、標準ライブラリのみ。外部ヘッダーへの依存なし
 * - 積・総和・差分表の補正を含む全中間値は Score に収める。オーバーフロー検査は行わない
 * - 最大化は flow と unary の符号を反転する。空き位置は 0 コストのダミー物体で表現できる
 * - 一般の容量制約・配置禁止は直接扱わない。固定配置には部分問題 API を使える
 * - prepared の参照先 problem、および state の参照先 prepared は利用中に変更・破棄しない
 * - 単項の予備判定はヒューリスティック。禁止配置を必ず解消する保証はない
 * - 追加割当の最悪計算量 O(n^3)、一時メモリ O(n^2)。時間枠は残りの1/4が目安
 * - 既知 cost は正しい値を渡す。deadline は終了目安であり、厳密な停止時刻ではない
 * - 直接コンパイルすると全自己テストを実行する。include 時はテストと main が無効になる
 */
#pragma once

#include <bits/stdc++.h>

template <class Weight, class Score> struct qap_problem;
template <class Score> struct qap_result;
template <class Weight, class Score> struct qap_subproblem;
template <class Weight, class Score> class qap_prepared;

template <class Weight, class Score>
qap_result<Score> solve_qap(const qap_problem<Weight, Score>&,
    std::chrono::steady_clock::time_point, std::uint64_t seed = 1);
template <class Weight, class Score>
qap_result<Score> improve_qap(const qap_problem<Weight, Score>&,
    const std::vector<int>&, std::chrono::steady_clock::time_point, std::uint64_t seed = 1);
template <class Weight, class Score>
qap_result<Score> improve_qap_subset(const qap_problem<Weight, Score>&,
    const std::vector<int>&, const std::vector<int>&, Score,
    std::chrono::steady_clock::time_point, std::uint64_t seed = 1);

template <class Weight = int, class Score = long long>
struct qap_problem {
    int n = 0;
    std::vector<Weight> flow;
    std::vector<Weight> distance;
    std::vector<Score> unary;

    // 空の問題を構築する。O(1)
    qap_problem() = default;

    // n 物体・n 位置の 0 初期化された問題を構築する。O(n^2)
    explicit qap_problem(int n_)
        : n(n_), flow(n * n), distance(n * n) {}

    // 行列を move して問題を構築する。O(1)
    qap_problem(int n_, std::vector<Weight> flow_, std::vector<Weight> distance_,
                std::vector<Score> unary_ = {})
        : n(n_), flow(std::move(flow_)), distance(std::move(distance_)),
          unary(std::move(unary_)) {
        assert_shape();
    }

    // permutation が 0..n-1 の置換なら true を返す。O(n)
    bool is_valid_permutation(const std::vector<int>& permutation) const {
        if (static_cast<int>(permutation.size()) != n) return false;
        if (permutation.empty()) return true;
        std::vector<unsigned char> used(n);
        for (int location : permutation) {
            if (location < 0 || location >= n || used[location]) return false;
            used[location] = 1;
        }
        return true;
    }

    // permutation の目的関数値を全再計算する。O(n^2)
    Score evaluate(const std::vector<int>& permutation) const {
        assert(is_valid_permutation(permutation));
        Score result = 0;
        if (!unary.empty()) {
            for (int i = 0; i < n; ++i) result += unary[i * n + permutation[i]];
        }

        // 各 flow と対応する配置間 distance の積を、向き付き二重和として加算する。
        for (int i = 0; i < n; ++i) {
            const int pi = permutation[i];
            const Weight* flow_row = flow.data() + i * n;
            const Weight* distance_row = distance.data() + pi * n;
            for (int j = 0; j < n; ++j) {
                result += Score(flow_row[j]) * Score(distance_row[permutation[j]]);
            }
        }
        return result;
    }

    // 物体 r, s の位置を交換したときの「交換後 - 交換前」を返す。O(n)
    Score swap_delta(const std::vector<int>& permutation, int r, int s) const {
        assert(is_valid_permutation(permutation));
        assert(0 <= r && r < n && 0 <= s && s < n);
        if (r == s) return 0;
        const int pr = permutation[r];
        const int ps = permutation[s];
        const Weight* flow_r = flow.data() + r * n;
        const Weight* flow_s = flow.data() + s * n;
        const Weight* distance_pr = distance.data() + pr * n;
        const Weight* distance_ps = distance.data() + ps * n;
        Score delta = 0;

        // 単項コストと、r, s 以外の物体との相互作用を更新する。
        if (!unary.empty()) {
            delta += unary[r * n + ps] - unary[r * n + pr];
            delta += unary[s * n + pr] - unary[s * n + ps];
        }
        const auto add_interaction = [&](int k) {
            const int pk = permutation[k];
            const Weight* flow_k = flow.data() + k * n;
            const Weight* distance_pk = distance.data() + pk * n;
            delta += Score(flow_r[k]) *
                     (Score(distance_ps[pk]) - Score(distance_pr[pk]));
            delta += Score(flow_s[k]) *
                     (Score(distance_pr[pk]) - Score(distance_ps[pk]));
            delta += Score(flow_k[r]) *
                     (Score(distance_pk[ps]) - Score(distance_pk[pr]));
            delta += Score(flow_k[s]) *
                     (Score(distance_pk[pr]) - Score(distance_pk[ps]));
        };
        const int first = std::min(r, s);
        const int second = std::max(r, s);
        for (int k = 0; k < first; ++k) add_interaction(k);
        for (int k = first + 1; k < second; ++k) add_interaction(k);
        for (int k = second + 1; k < n; ++k) add_interaction(k);

        // 自己相互作用と r-s 間の相互作用は、上のループに含まれないので個別に更新する。
        delta += Score(flow_r[r]) *
                 (Score(distance_ps[ps]) - Score(distance_pr[pr]));
        delta += Score(flow_s[s]) *
                 (Score(distance_pr[pr]) - Score(distance_ps[ps]));
        delta += Score(flow_r[s]) *
                 (Score(distance_ps[pr]) - Score(distance_pr[ps]));
        delta += Score(flow_s[r]) *
                 (Score(distance_pr[ps]) - Score(distance_ps[pr]));
        return delta;
    }
private:
    template <class, class> friend class qap_prepared;
    template <class W, class S>
    friend qap_result<S> solve_qap(const qap_problem<W, S>&,
        std::chrono::steady_clock::time_point, std::uint64_t);
    template <class W, class S>
    friend qap_subproblem<W, S> make_qap_subproblem(const qap_problem<W, S>&,
        const std::vector<int>&, const std::vector<int>&, S);
    template <class W, class S>
    friend qap_result<S> improve_qap_subset(const qap_problem<W, S>&,
        const std::vector<int>&, const std::vector<int>&, S,
        std::chrono::steady_clock::time_point, std::uint64_t);

    void assert_shape() const {
#ifndef NDEBUG
        assert(n >= 0);
        const long long size = static_cast<long long>(n) * n;
        assert(static_cast<long long>(flow.size()) == size);
        assert(static_cast<long long>(distance.size()) == size);
        assert(unary.empty() ||
               static_cast<long long>(unary.size()) == size);
#endif
    }

    void assert_movable_objects(const std::vector<int>& movable_objects) const {
#ifndef NDEBUG
        std::vector<unsigned char> used(n);
        for (int object : movable_objects) {
            assert(0 <= object && object < n);
            assert(!used[object]);
            used[object] = 1;
        }
#else
        (void)movable_objects;
#endif
    }

};

// デバッグ表示の内部補助。標準型そのものの operator<< は追加しない。
namespace qap_debug_detail {
template <class T>
std::ostream& write(std::ostream& out, const T& value) {
    if constexpr (std::is_same_v<T, __int128_t> || std::is_same_v<T, __uint128_t>) {
        bool negative = false;
        __uint128_t magnitude;
        if constexpr (std::is_same_v<T, __int128_t>) {
            negative = value < 0;
            magnitude = negative ? __uint128_t(-(value + 1)) + 1 : __uint128_t(value);
        } else {
            magnitude = value;
        }
        char buffer[41];
        char* const end = buffer + sizeof(buffer);
        char* first = end;
        do {
            *--first = static_cast<char>('0' + magnitude % 10);
            magnitude /= 10;
        } while (magnitude != 0);
        if (negative) *--first = '-';
        return out << std::string_view(first, static_cast<std::size_t>(end - first));
    } else if constexpr (std::is_same_v<T, bool>) {
        return out << (value ? "true" : "false");
    } else if constexpr (std::is_integral_v<T>) {
        return out << +value;  // 小さい整数型も文字ではなく数値として表示
    } else {
        return out << value;
    }
}

inline std::ostream& write(std::ostream& out, const std::vector<int>& values) {
    out << '[';
    const std::size_t shown = std::min<std::size_t>(values.size(), 20);
    for (std::size_t i = 0; i < shown; ++i) {
        if (i != 0) out << ", ";
        out << values[i];
    }
    if (shown < values.size()) out << ", ...";
    out << ']';
    if (shown < values.size()) out << " (size=" << values.size() << ')';
    return out;
}
}  // namespace qap_debug_detail

template <class Score = long long>
struct qap_result {
    std::vector<int> location_of;
    Score cost = 0;

    // 費用と物体→位置の配列を表示。データ・書式フラグは変更せず、改行しない。
    friend std::ostream& operator<<(std::ostream& out, const qap_result& result) {
        out << "qap_result{cost=";
        qap_debug_detail::write(out, result.cost);
        out << ", location_of=";
        qap_debug_detail::write(out, result.location_of);
        return out << '}';
    }
};

// 同じ問題の前処理と探索を所有する。problem の内容・寿命は利用中に維持する。
template <class Weight = int, class Score = long long>
class qap_prepared {
    friend struct qap_test_access;  // 配布テストだけに内部の検証用アクセスを許可する。
    template <class W, class S>
    friend qap_result<S> solve_qap(const qap_problem<W, S>&,
        std::chrono::steady_clock::time_point, std::uint64_t);
    template <class W, class S>
    friend qap_result<S> improve_qap(const qap_problem<W, S>&,
        const std::vector<int>&, std::chrono::steady_clock::time_point, std::uint64_t);

    struct rng64 {
        std::uint64_t state;

        explicit rng64(std::uint64_t seed)
            : state(seed ? seed : 0x9e3779b97f4a7c15ULL) {}

        std::uint64_t operator()() {
            state ^= state << 7;
            state ^= state >> 9;
            return state;
        }

        int uniform_int(int bound) {
            return static_cast<int>((static_cast<__uint128_t>((*this)()) *
                                     static_cast<std::uint64_t>(bound)) >>
                                    64);
        }
    };

    struct evaluator {
        struct sparse_flow {
            std::vector<int> out_offset;
            std::vector<int> out_vertex;
            std::vector<Weight> out_weight;
            std::vector<int> in_offset;
            std::vector<int> in_vertex;
            std::vector<Weight> in_weight;

            sparse_flow(const qap_problem<Weight, Score>& problem,
                        long long nonzero) {
                const int n = problem.n;
                out_offset.resize(n + 1);
                in_offset.resize(n + 1);
                out_vertex.reserve(nonzero);
                out_weight.reserve(nonzero);

                // 行方向 CSR は flow の1走査で直接作り、同時に入次数を集計する。
                for (int i = 0; i < n; ++i) {
                    for (int j = 0; j < n; ++j) {
                        const Weight weight = problem.flow[i * n + j];
                        if (weight == Weight{}) continue;
                        out_vertex.push_back(j);
                        out_weight.push_back(weight);
                        ++in_offset[j + 1];
                    }
                    out_offset[i + 1] = static_cast<int>(out_vertex.size());
                }
                std::partial_sum(in_offset.begin(), in_offset.end(), in_offset.begin());
                in_vertex.resize(in_offset.back());
                in_weight.resize(in_offset.back());

                // 密な flow を再走査せず、行方向 CSR の非ゼロ辺だけから入方向 CSR を作る。
                std::vector<int> in_cursor = in_offset;
                for (int i = 0; i < n; ++i) {
                    for (int out_index = out_offset[i];
                         out_index < out_offset[i + 1]; ++out_index) {
                        const int j = out_vertex[out_index];
                        const int in_index = in_cursor[j]++;
                        in_vertex[in_index] = i;
                        in_weight[in_index] = out_weight[out_index];
                    }
                }
            }
        };

        const qap_problem<Weight, Score>* problem;
        bool symmetric;
        std::optional<sparse_flow> sparse;

        explicit evaluator(const qap_problem<Weight, Score>& problem_)
            : problem(&problem_), symmetric([&] {
                problem_.assert_shape();
                const int n = problem_.n;
                for (int i = 0; i < n; ++i) {
                    const Weight* flow_i = problem_.flow.data() + i * n;
                    const Weight* distance_i = problem_.distance.data() + i * n;
                    if (flow_i[i] != Weight{} || distance_i[i] != Weight{}) {
                        return false;
                    }
                    for (int j = i + 1; j < n; ++j) {
                        if (flow_i[j] != problem_.flow[j * n + i] ||
                            distance_i[j] != problem_.distance[j * n + i]) {
                            return false;
                        }
                    }
                }
                return true;
            }()) {
            long long nonzero = 0;
            for (const Weight& weight : problem_.flow) nonzero += weight != Weight{};
            if (nonzero * 3 < static_cast<long long>(problem_.n) * problem_.n) {
                sparse.emplace(problem_, nonzero);
            }
        }

        Score dense_symmetric_delta(const std::vector<int>& permutation, int r, int s) const {
            const int n = problem->n;
            const int pr = permutation[r];
            const int ps = permutation[s];
            const Weight* flow_r = problem->flow.data() + r * n;
            const Weight* flow_s = problem->flow.data() + s * n;
            const Weight* distance_pr = problem->distance.data() + pr * n;
            const Weight* distance_ps = problem->distance.data() + ps * n;
            Score delta = 0;
            if (!problem->unary.empty()) {
                delta += problem->unary[r * n + ps] - problem->unary[r * n + pr];
                delta += problem->unary[s * n + pr] - problem->unary[s * n + ps];
            }
            const auto add_interaction = [&](int k) {
                const int pk = permutation[k];
                delta += Score(2) * (Score(flow_r[k]) - Score(flow_s[k])) *
                         (Score(distance_ps[pk]) - Score(distance_pr[pk]));
            };
            const int first = std::min(r, s);
            const int second = std::max(r, s);
            for (int k = 0; k < first; ++k) add_interaction(k);
            for (int k = first + 1; k < second; ++k) add_interaction(k);
            for (int k = second + 1; k < n; ++k) add_interaction(k);
            return delta;
        }

        Score sparse_symmetric_delta(const std::vector<int>& permutation, int r, int s) const {
            const int n = problem->n;
            const int pr = permutation[r];
            const int ps = permutation[s];
            const auto& graph = *sparse;
            const Weight* distance_pr = problem->distance.data() + pr * n;
            const Weight* distance_ps = problem->distance.data() + ps * n;
            Score delta = 0;
            if (!problem->unary.empty()) {
                delta += problem->unary[r * n + ps] - problem->unary[r * n + pr];
                delta += problem->unary[s * n + pr] - problem->unary[s * n + ps];
            }

            // 対称行列では r, s の出辺だけで、入辺側を含む寄与を 2 倍して計算できる。
            for (int index = graph.out_offset[r]; index < graph.out_offset[r + 1]; ++index) {
                const int k = graph.out_vertex[index];
                if (k == r || k == s) continue;
                const int pk = permutation[k];
                delta += Score(2) * Score(graph.out_weight[index]) *
                         (Score(distance_ps[pk]) - Score(distance_pr[pk]));
            }
            for (int index = graph.out_offset[s]; index < graph.out_offset[s + 1]; ++index) {
                const int k = graph.out_vertex[index];
                if (k == r || k == s) continue;
                const int pk = permutation[k];
                delta += Score(2) * Score(graph.out_weight[index]) *
                         (Score(distance_pr[pk]) - Score(distance_ps[pk]));
            }
            return delta;
        }

        Score sparse_general_delta(const std::vector<int>& permutation, int r, int s) const {
            const int n = problem->n;
            const int pr = permutation[r];
            const int ps = permutation[s];
            const auto& graph = *sparse;
            const Weight* distance_pr = problem->distance.data() + pr * n;
            const Weight* distance_ps = problem->distance.data() + ps * n;
            Score delta = 0;
            if (!problem->unary.empty()) {
                delta += problem->unary[r * n + ps] - problem->unary[r * n + pr];
                delta += problem->unary[s * n + pr] - problem->unary[s * n + ps];
            }

            // r, s の出辺と入辺を別々に走査し、非ゼロ flow に関係する項だけを更新する。
            for (int index = graph.out_offset[r]; index < graph.out_offset[r + 1]; ++index) {
                const int k = graph.out_vertex[index];
                if (k == r || k == s) continue;
                const int pk = permutation[k];
                delta += Score(graph.out_weight[index]) *
                         (Score(distance_ps[pk]) - Score(distance_pr[pk]));
            }
            for (int index = graph.out_offset[s]; index < graph.out_offset[s + 1]; ++index) {
                const int k = graph.out_vertex[index];
                if (k == r || k == s) continue;
                const int pk = permutation[k];
                delta += Score(graph.out_weight[index]) *
                         (Score(distance_pr[pk]) - Score(distance_ps[pk]));
            }
            for (int index = graph.in_offset[r]; index < graph.in_offset[r + 1]; ++index) {
                const int k = graph.in_vertex[index];
                if (k == r || k == s) continue;
                const int pk = permutation[k];
                const Weight* distance_pk = problem->distance.data() + pk * n;
                delta += Score(graph.in_weight[index]) *
                         (Score(distance_pk[ps]) - Score(distance_pk[pr]));
            }
            for (int index = graph.in_offset[s]; index < graph.in_offset[s + 1]; ++index) {
                const int k = graph.in_vertex[index];
                if (k == r || k == s) continue;
                const int pk = permutation[k];
                const Weight* distance_pk = problem->distance.data() + pk * n;
                delta += Score(graph.in_weight[index]) *
                         (Score(distance_pk[pr]) - Score(distance_pk[ps]));
            }

            // 自己相互作用と r-s 間の相互作用は CSR 走査から除外したため個別に加算する。
            const Weight* flow_r = problem->flow.data() + r * n;
            const Weight* flow_s = problem->flow.data() + s * n;
            delta += Score(flow_r[r]) *
                     (Score(distance_ps[ps]) - Score(distance_pr[pr]));
            delta += Score(flow_s[s]) *
                     (Score(distance_pr[pr]) - Score(distance_ps[ps]));
            delta += Score(flow_r[s]) *
                     (Score(distance_ps[pr]) - Score(distance_pr[ps]));
            delta += Score(flow_s[r]) *
                     (Score(distance_pr[ps]) - Score(distance_ps[pr]));
            return delta;
        }

        Score swap_delta(const std::vector<int>& permutation, int r, int s) const {
            if (sparse) {
                return symmetric ? sparse_symmetric_delta(permutation, r, s)
                                 : sparse_general_delta(permutation, r, s);
            }
            return symmetric ? dense_symmetric_delta(permutation, r, s)
                             : problem->swap_delta(permutation, r, s);
        }

    };

    const qap_problem<Weight, Score>* problem_;
    evaluator evaluator_;

    struct delta_table {
        const qap_problem<Weight, Score>* problem;
        const evaluator* eval;
        std::vector<int> permutation;
        std::vector<Score> delta;
        Score cost;
        std::vector<Score> update_work;
        bool valid = true;

        delta_table(const qap_problem<Weight, Score>& problem_,
                    const evaluator& eval_, std::vector<int> permutation_,
                    Score initial_cost,
                    std::chrono::steady_clock::time_point deadline)
            : problem(&problem_), eval(&eval_), permutation(std::move(permutation_)),
              delta(problem_.n * problem_.n), cost(initial_cost), update_work(4 * problem_.n) {
            const int n = problem->n;
            for (int i = 0; i < n; ++i) {
                for (int j = i + 1; j < n; ++j) {
                    delta[i * n + j] = eval->swap_delta(permutation, i, j);
                }
                if ((i & 3) == 3 && std::chrono::steady_clock::now() >= deadline) {
                    valid = false;
                    return;
                }
            }
        }

        void apply(int r, int s) {
            if (r > s) std::swap(r, s);
            const int n = problem->n;
            const Score applied_delta = delta[r * n + s];
            cost += applied_delta;
            std::swap(permutation[r], permutation[s]);

            // 変更された2物体への係数を O(n) でまとめ、差分表を連続走査する。
            Score* out_flow = update_work.data();
            Score* out_distance = out_flow + n;
            Score* in_flow = out_distance + n;
            Score* in_distance = in_flow + n;
            const int pr = permutation[r];
            const int ps = permutation[s];
            for (int i = 0; i < n; ++i) {
                const int pi = permutation[i];
                out_flow[i] = Score(problem->flow[i * n + r]) - Score(problem->flow[i * n + s]);
                out_distance[i] = Score(problem->distance[pi * n + pr]) - Score(problem->distance[pi * n + ps]);
            }
            if (eval->symmetric) {
                for (int i = 0; i < n; ++i) {
                    if (i == r || i == s) continue;
                    Score* row = delta.data() + i * n;
                    for (int j = i + 1; j < n; ++j) {
                        if (j == r || j == s) continue;
                        row[j] += Score(2) * (out_flow[i] - out_flow[j]) * (out_distance[j] - out_distance[i]);
                    }
                }
            } else {
                for (int i = 0; i < n; ++i) {
                    const int pi = permutation[i];
                    in_flow[i] = Score(problem->flow[r * n + i]) - Score(problem->flow[s * n + i]);
                    in_distance[i] = Score(problem->distance[pr * n + pi]) - Score(problem->distance[ps * n + pi]);
                }
                for (int i = 0; i < n; ++i) {
                    if (i == r || i == s) continue;
                    Score* row = delta.data() + i * n;
                    for (int j = i + 1; j < n; ++j) {
                        if (j == r || j == s) continue;
                        row[j] += (out_flow[i] - out_flow[j]) * (out_distance[j] - out_distance[i])
                                + (in_flow[i] - in_flow[j]) * (in_distance[j] - in_distance[i]);
                    }
                }
            }

            // 3物体の6置換の交代和から、片方の差分を O(1) で復元する。
            // 単項項・対角項・外部物体の寄与は打ち消し合い、有向3辺の積だけが残る。
            for (int i = 0; i < n; ++i) {
                if (i == r || i == s) continue;
                const int ri = std::min(r, i) * n + std::max(r, i);
                const int si = std::min(s, i) * n + std::max(s, i);
                Score sum = delta[ri] + delta[si] - applied_delta;
                if (!eval->symmetric) {
                    const int pi = permutation[i];
                    const auto& f = problem->flow;
                    const auto& d = problem->distance;
                    const Score flow_cycle = Score(f[r*n+s]) - Score(f[s*n+r])
                                           + Score(f[s*n+i]) - Score(f[i*n+s])
                                           + Score(f[i*n+r]) - Score(f[r*n+i]);
                    const Score distance_cycle = Score(d[ps*n+pr]) - Score(d[pr*n+ps])
                                               + Score(d[pr*n+pi]) - Score(d[pi*n+pr])
                                               + Score(d[pi*n+ps]) - Score(d[ps*n+pi]);
                    sum += flow_cycle * distance_cycle;
                }
                delta[ri] = eval->swap_delta(permutation, r, i);
                delta[si] = sum - delta[ri];
            }
            delta[r * n + s] = -applied_delta;
        }
    };

    qap_result<Score> threshold_search(
        const std::vector<int>& initial,
        Score initial_cost,
        std::chrono::steady_clock::time_point deadline,
        std::uint64_t seed) const {
        const auto& problem = *problem_;
        const auto& eval = evaluator_;
        rng64 rng(seed);
        std::vector<int> permutation = initial;
        Score current = initial_cost;
        qap_result<Score> result{permutation, current};
        const int n = problem.n;
        if (std::chrono::steady_clock::now() >= deadline) return result;

        // 正の swap 差分の中央値から、問題の目的値スケールに合う初期閾値を決める。
        std::vector<Score> uphill;
        uphill.reserve(256);
        for (int trial = 0; trial < 256; ++trial) {
            int i = rng.uniform_int(n);
            int j = rng.uniform_int(n - 1);
            if (j >= i) ++j;
            const Score delta = eval.swap_delta(permutation, i, j);
            if (delta > 0) uphill.push_back(delta);
            if ((trial & 31) == 31 && std::chrono::steady_clock::now() >= deadline) return result;
        }
        Score scale = 1;
        if (!uphill.empty()) {
            std::nth_element(uphill.begin(), uphill.begin() + uphill.size() / 2, uphill.end());
            scale = std::max<Score>(1, uphill[uphill.size() / 2]);
        }
        const long double start_threshold = static_cast<long double>(scale) * 0.1171875L;
        const auto start = std::chrono::steady_clock::now();
        const long double total_seconds =
            std::chrono::duration<long double>(deadline - start).count();
        long double threshold = start_threshold;
        long long iteration = 0;

        // 正の差分中央値の0.1171875倍から、各フェーズ内で閾値を線形に0まで下げる。最良解は別に保存する。
        while (true) {
            if ((iteration & 255) == 0) {
                const auto now = std::chrono::steady_clock::now();
                if (now >= deadline) break;
                const long double progress =
                    std::chrono::duration<long double>(now - start).count() / total_seconds;
                threshold = start_threshold * (1.0L - progress);
            }
            int i = rng.uniform_int(n);
            int j = rng.uniform_int(n - 1);
            if (j >= i) ++j;
            const Score delta = eval.swap_delta(permutation, i, j);
            if (static_cast<long double>(delta) <= threshold) {
                std::swap(permutation[i], permutation[j]);
                current += delta;
                if (current < result.cost) {
                    result.cost = current;
                    result.location_of = permutation;
                }
            }
            ++iteration;
        }
        return result;
    }

    qap_result<Score> continue_hybrid_search(
        qap_result<Score> result,
        std::chrono::steady_clock::time_point deadline,
        std::uint64_t seed) const {
        const auto& problem = *problem_;
        const auto& eval = evaluator_;
        // 単項が支配的なときだけ追加候補を作る。完全な置換を全評価して比較する。
        // 時間枠の後にも、未割当の補完と全評価には別途 O(n^2) かかる。
        const auto unary_initial = [&]() -> std::vector<int> {
            if (problem.unary.empty()) return {};
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) return {};
            const auto seed_deadline = now + (deadline - now) / 4;
            const int size = problem.n;

            // 少数の行の差だけで弱い単項を安価に除外する。行・列の定数費用を相殺する。
            // O(n) のヒューリスティックなので、観測しない行や浮動小数点の丸めで見逃しうる。
            long double sampled_flow = 0;
            Weight sampled_min = problem.distance[0], sampled_max = sampled_min;
            for (int sample = 0; sample < 16; ++sample) {
                const int index = static_cast<int>(static_cast<long long>(sample) * size * size / 16);
                sampled_flow = std::max(sampled_flow, std::abs(static_cast<long double>(problem.flow[index])));
                sampled_min = std::min(sampled_min, problem.distance[index]);
                sampled_max = std::max(sampled_max, problem.distance[index]);
            }
            const long double cutoff = 8.0L * size * sampled_flow *
                (static_cast<long double>(sampled_max) - static_cast<long double>(sampled_min));
            bool promising = false;
            for (int part = 1; part < 4 && !promising; ++part) {
                const int row = size * part / 4;
                long double low = static_cast<long double>(problem.unary[row * size])
                    - static_cast<long double>(problem.unary[0]);
                long double high = low;
                for (int j = 1; j < size; ++j) {
                    const long double value = static_cast<long double>(problem.unary[row * size + j])
                        - static_cast<long double>(problem.unary[j]);
                    low = std::min(low, value);
                    high = std::max(high, value);
                    if (high - low > cutoff) { promising = true; break; }
                }
            }
            if (!promising) return {};
            // 追加の差・累積・双対変数が Score に収まる保守的な範囲だけ扱う。
            // 範囲外は追加処理を省き、元のQAP探索を続ける。
            const Score bound = static_cast<Score>(std::numeric_limits<Score>::max() / (8LL * size));
            for (Score value : problem.unary) {
                if (value < -bound || value > bound) return {};
            }

            // 二次項の全変動上界を求め、単項の全変動でも届かない場合は追加処理を省く。
            long double flow_sum = 0;
            Weight minimum_distance = problem.distance[0], maximum_distance = problem.distance[0];
            for (int i = 0; i < size * size; ++i) {
                flow_sum += std::abs(static_cast<long double>(problem.flow[i]));
                minimum_distance = std::min(minimum_distance, problem.distance[i]);
                maximum_distance = std::max(maximum_distance, problem.distance[i]);
            }
            const long double quadratic_range = flow_sum *
                (static_cast<long double>(maximum_distance) - static_cast<long double>(minimum_distance));
            const auto [smallest, largest] = std::minmax_element(problem.unary.begin(), problem.unary.end());
            if (static_cast<long double>(*largest - *smallest) * size <= quadratic_range) return {};

            // 列平均を引いた単項で改善余地を測る。n倍して整数のまま列の定数成分を除く。
            // 各行の最小値も引くため、行・列ごとの定数費用の追加は判定を変えない。
            std::vector<Score> column_sum(size);
            for (int i = 0; i < size; ++i) for (int j = 0; j < size; ++j) {
                column_sum[j] += problem.unary[i * size + j];
            }
            long double unary_gap = 0;
            for (int i = 0; i < size; ++i) {
                const Score* row = problem.unary.data() + i * size;
                Score minimum_value = Score(size) * row[0] - column_sum[0];
                for (int j = 1; j < size; ++j) {
                    minimum_value = std::min(minimum_value, Score(size) * row[j] - column_sum[j]);
                }
                const int location = result.location_of[i];
                unary_gap += static_cast<long double>(
                    (Score(size) * row[location] - column_sum[location]) - minimum_value);
            }
            if (unary_gap <= quadratic_range * size) return {};
            if (std::chrono::steady_clock::now() >= seed_deadline) return {};

            // 各行の最安値から二次項の変動幅以内の辺を使い、候補の少ない行から割り当てる。
            // 最小費用割当ではない。完全な候補ができたときだけ、目的関数全体で比較する。
            const auto matching_seed = [&]() -> std::vector<int> {
                std::vector<std::vector<int>> edges(size);
                std::vector<int> order(size), at(size, -1), permutation(size, -1);
                std::iota(order.begin(), order.end(), 0);
                for (int i = 0; i < size; ++i) {
                    const Score* costs = problem.unary.data() + i * size;
                    const Score low = *std::min_element(costs, costs + size);
                    for (int j = 0; j < size; ++j) {
                        if (static_cast<long double>(costs[j] - low) <= quadratic_range) edges[i].push_back(j);
                    }
                    if (std::chrono::steady_clock::now() >= seed_deadline) return {};
                }
                std::sort(order.begin(), order.end(), [&](int a, int b) {
                    if (edges[a].size() != edges[b].size()) return edges[a].size() < edges[b].size();
                    return a < b;
                });
                std::vector<int> previous(size), queue(size);
                for (int row : order) {
                    std::fill(previous.begin(), previous.end(), -1);
                    int head = 0, tail = 1, free_column = -1;
                    queue[0] = row;
                    while (head < tail && free_column < 0) {
                        const int active = queue[head++];
                        for (int column : edges[active]) if (previous[column] < 0) {
                            previous[column] = active;
                            if (at[column] < 0) { free_column = column; break; }
                            queue[tail++] = at[column];
                        }
                        if (std::chrono::steady_clock::now() >= seed_deadline) return {};
                    }
                    if (free_column < 0) return {};
                    while (free_column >= 0) {
                        const int active = previous[free_column];
                        const int next = permutation[active];
                        permutation[active] = free_column;
                        at[free_column] = active;
                        free_column = next;
                    }
                }
                return permutation;
            }();
            if (!matching_seed.empty()) return matching_seed;
            // 割当が完成しない場合は、残った時間で単項の最小費用割当を試す。
            std::vector<Score> row_potential(size + 1), column_potential(size + 1);
            std::vector<Score> minimum(size + 1);
            std::vector<int> row_at(size + 1), previous(size + 1);
            std::vector<unsigned char> used(size + 1);
            bool timeout = false;
            for (int row = 1; row <= size; ++row) {
                row_at[0] = row;
                std::fill(used.begin(), used.end(), 0);
                std::fill(minimum.begin(), minimum.end(), std::numeric_limits<Score>::max());
                int column = 0;
                do {
                    used[column] = 1;
                    const int active_row = row_at[column];
                    Score step = std::numeric_limits<Score>::max();
                    int next = 0;
                    for (int j = 1; j <= size; ++j) if (!used[j]) {
                        const Score reduced = problem.unary[(active_row - 1) * size + j - 1]
                            - row_potential[active_row] - column_potential[j];
                        if (reduced < minimum[j]) {
                            minimum[j] = reduced;
                            previous[j] = column;
                        }
                        if (minimum[j] < step || (minimum[j] == step && row_at[j] == 0)) { step = minimum[j]; next = j; }
                    }
                    for (int j = 0; j <= size; ++j) {
                        if (used[j]) {
                            row_potential[row_at[j]] += step;
                            if (j != 0) column_potential[j] -= step;
                        } else if (j != 0) minimum[j] -= step;
                    }
                    column = next;
                    if (std::chrono::steady_clock::now() >= seed_deadline) { timeout = true; break; }
                } while (row_at[column] != 0);
                if (timeout) break;
                // 空き位置までの交互道に沿って割当を入れ替える。
                do {
                    const int next = previous[column];
                    row_at[column] = row_at[next];
                    column = next;
                } while (column != 0);
            }
            std::vector<int> permutation(size, -1);
            std::fill(used.begin(), used.end(), 0);
            for (int j = 1; j <= size; ++j) if (row_at[j] != 0) {
                permutation[row_at[j] - 1] = j - 1;
                used[j] = 1;
            }
            // 時間切れなら未割当の行だけを貪欲に補完し、完成済みの割当を生かす。
            for (int i = 0; i < size; ++i) if (permutation[i] < 0) {
                int best = 0;
                for (int j = 1; j <= size; ++j) if (!used[j]) {
                    if (best == 0 || problem.unary[i * size + j - 1] < problem.unary[i * size + best - 1]) best = j;
                }
                permutation[i] = best - 1;
                used[best] = 1;
            }
            return permutation;
        }();

        if (!unary_initial.empty()) {
            const Score unary_cost = problem.evaluate(unary_initial);
            if (unary_cost < result.cost) result = {unary_initial, unary_cost};
        }
        const auto start = std::chrono::steady_clock::now();
        if (start >= deadline) return result;
        const auto duration = deadline - start;

        const auto first_deadline = start + duration / 4;
        const auto second_deadline = start + duration / 2;

        // Threshold Accepting を 2 周期に分け、前半の最良解から閾値を再設定する。
        auto first = threshold_search(result.location_of, result.cost,
                                      first_deadline, seed);
        if (first.cost < result.cost) result = std::move(first);
        auto second = threshold_search(result.location_of, result.cost,
                                       second_deadline,
                                       seed + 0x9e3779b97f4a7c15ULL);
        if (second.cost < result.cost) result = std::move(second);
        if (std::chrono::steady_clock::now() >= deadline) return result;

        // 差分表構築が残り時間に収まる場合だけ tabu を行い、大規模時は閾値探索を継続する。
        const auto table_is_affordable = [&] {
            const int n = problem.n;
            const long long pairs = static_cast<long long>(n) * (n - 1) / 2;
            const int samples = static_cast<int>(std::min<long long>(pairs, std::max(16, n)));
            rng64 rng(seed ^ 0x94d049bb133111ebULL);
            // 符号付き Score の加算オーバーフローを避けつつ、計測対象の計算が消去されないようにする。
            std::uint64_t checksum = 0;
            const auto sample_start = std::chrono::steady_clock::now();
            for (int sample = 0; sample < samples; ++sample) {
                int i = rng.uniform_int(n);
                int j = rng.uniform_int(n - 1);
                if (j >= i) ++j;
                checksum += static_cast<std::uint64_t>(eval.swap_delta(result.location_of, i, j));
            }
            // 測定した計算の消去を防ぎ、checksum の値を探索方式の選択には使わない
            asm volatile("" : : "g"(checksum) : "memory");
            const auto finish = std::chrono::steady_clock::now();
            const auto remaining = deadline - finish;
            if (remaining <= std::chrono::steady_clock::duration::zero()) return false;
            const long double multiplier = static_cast<long double>(pairs) / samples;
            const auto estimated = std::chrono::duration<long double>(finish - sample_start) * multiplier;
            return estimated * 2.0L < std::chrono::duration<long double>(remaining);
        };
        if (table_is_affordable()) {
            delta_table table(
                problem, eval, result.location_of, result.cost, deadline);
            if (!table.valid) return result;
            const int n = problem.n;
            rng64 rng(seed ^ 0xd1b54a32d192ed03ULL);
            std::vector<long long> tabu_until(n * n);
            long long iteration = 0;
            std::chrono::steady_clock::duration previous_apply{};

            while (std::chrono::steady_clock::now() < deadline) {
                Score best_delta = 0;
                int best_i = -1;
                int best_j = -1;

                // 全 swap から、tabu でない最良手または最良値を更新する aspiration 手を選ぶ。
                for (int i = 0; i < n; ++i) {
                    const int pi = table.permutation[i];
                    const Score* delta_row = table.delta.data() + i * n;
                    const long long* tabu_row = tabu_until.data() + i * n;
                    for (int j = i + 1; j < n; ++j) {
                        const Score delta = delta_row[j];
                        // Score の最大値も有効なので、候補の有無は添字で判定する
                        if (best_i >= 0 && delta >= best_delta) continue;
                        const bool allowed =
                            tabu_row[table.permutation[j]] <= iteration &&
                            tabu_until[j * n + pi] <= iteration;
                        if (allowed || table.cost + delta < result.cost) {
                            best_delta = delta;
                            best_i = i;
                            best_j = j;
                        }
                    }
                }
                const auto before_apply = std::chrono::steady_clock::now();
                if (best_i < 0 || before_apply >= deadline ||
                    (previous_apply != std::chrono::steady_clock::duration::zero() &&
                     before_apply + previous_apply + previous_apply / 4 >= deadline)) {
                    break;
                }

                // 元の位置への即時復帰を、0.1n～0.3n 反復のランダムな期間だけ禁止する。
                const int old_i = table.permutation[best_i];
                const int old_j = table.permutation[best_j];
                table.apply(best_i, best_j);
                const int low = std::max(1, n / 10);
                const int high = std::max(low, 3 * n / 10);
                const int tenure = low + rng.uniform_int(high - low + 1);
                tabu_until[best_i * n + old_i] = iteration + tenure;
                tabu_until[best_j * n + old_j] = iteration + tenure;
                ++iteration;
                previous_apply = std::chrono::steady_clock::now() - before_apply;
                if (table.cost < result.cost) {
                    result.cost = table.cost;
                    result.location_of = table.permutation;
                }
            }
        } else {
            auto tail = threshold_search(result.location_of, result.cost,
                                         deadline,
                                         seed ^ 0xe7037ed1a0b428dbULL);
            if (tail.cost < result.cost) result = std::move(tail);
        }
        return result;
    }

    static std::vector<int> initial_permutation(const qap_problem<Weight, Score>& problem,
                                         std::uint64_t seed) {
        const int n = problem.n;
        rng64 rng(seed);
        std::vector<int> random(n);
        std::iota(random.begin(), random.end(), 0);
        for (int i = n - 1; i > 0; --i) std::swap(random[i], random[rng.uniform_int(i + 1)]);
        if (n < 2) return random;

        // 相互作用総量の大きい物体を、他位置への距離和が小さい中央寄りの位置へ割り当てる。
        std::vector<std::pair<Score, int>> facilities;
        std::vector<std::pair<Score, int>> locations;
        facilities.reserve(n);
        locations.reserve(n);
        std::vector<Score> strength(n), centrality(n);
        for (int i = 0; i < n; ++i) {
            Score row_strength = 0;
            Score row_centrality = 0;
            for (int j = 0; j < n; ++j) {
                const Score value = Score(problem.flow[i * n + j]);
                const Score flow = value < 0 ? -value : value;
                const Score distance = Score(problem.distance[i * n + j]);
                row_strength += flow;
                row_centrality += distance;
                strength[j] += flow;
                centrality[j] += distance;
            }
            strength[i] += row_strength;
            centrality[i] += row_centrality;
        }
        for (int i = 0; i < n; ++i) {
            facilities.emplace_back(-strength[i], i);
            locations.emplace_back(centrality[i], i);
        }
        std::sort(facilities.begin(), facilities.end());
        std::sort(locations.begin(), locations.end());
        std::vector<int> central(n);
        for (int i = 0; i < n; ++i) central[facilities[i].second] = locations[i].second;
        return problem.evaluate(central) < problem.evaluate(random) ? central : random;
    }

public:
    // 問題への参照と再利用用の前処理を構築する。O(n^2)
    explicit qap_prepared(const qap_problem<Weight, Score>& problem)
        : problem_(&problem), evaluator_(problem) {}

    // 前処理の参照先の問題を返す。O(1)
    const qap_problem<Weight, Score>& problem() const noexcept {
        return *problem_;
    }

    // 配置の目的値を全再計算する。O(n^2)
    Score evaluate(const std::vector<int>& location_of) const {
        return problem_->evaluate(location_of);
    }

    // location_of が有効な置換であることは呼び出し側の事前条件。O(1) の検査だけ行う。
    // 交換後と交換前の目的値の差を返す。O(n)、疎 flow では交換物体の入出次数の和
    Score swap_delta(const std::vector<int>& location_of, int i, int j) const {
        assert(static_cast<int>(location_of.size()) == problem_->n);
        assert(0 <= i && i < problem_->n && 0 <= j && j < problem_->n);
        if (i == j) return 0;
        return evaluator_.swap_delta(location_of, i, j);
    }

    // 初期配置を作り deadline まで探索する。前処理 O(n^2)、メモリ O(n^2)
    qap_result<Score> solve(
        std::chrono::steady_clock::time_point deadline,
        std::uint64_t seed = 1) const {
        return improve(initial_permutation(*problem_, seed), deadline, seed);
    }

    // 初期配置を保持候補に含めて探索する。初期評価 O(n^2)、メモリ O(n^2)
    qap_result<Score> improve(
        const std::vector<int>& initial,
        std::chrono::steady_clock::time_point deadline,
        std::uint64_t seed = 1) const {
        qap_result<Score> result{initial, problem_->evaluate(initial)};
        if (problem_->n < 2 || std::chrono::steady_clock::now() >= deadline) return result;
        return continue_hybrid_search(std::move(result), deadline, seed);
    }

};

// 与えられた置換を保持候補に含め、deadline まで QAP 解を改善する。O(n^2) memory
template <class Weight, class Score>
qap_result<Score> improve_qap(const qap_problem<Weight, Score>& problem,
    const std::vector<int>& initial, std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed) {
    qap_result<Score> result{initial, problem.evaluate(initial)};
    if (problem.n < 2 || std::chrono::steady_clock::now() >= deadline) return result;
    qap_prepared<Weight, Score> prepared(problem);
    return prepared.continue_hybrid_search(std::move(result), deadline, seed);
}

// 初期解を内部生成し、deadline まで QAP を近似的に解く。O(n^2) memory
template <class Weight, class Score>
qap_result<Score> solve_qap(const qap_problem<Weight, Score>& problem,
    std::chrono::steady_clock::time_point deadline, std::uint64_t seed) {
    problem.assert_shape();
    return improve_qap(problem,
        qap_prepared<Weight, Score>::initial_permutation(problem, seed), deadline, seed);
}

// 外側の探索が置換と目的値を保持しながら、QAP の swap 近傍だけを利用するための状態。
template <class Weight = int, class Score = long long>
class qap_swap_state {
public:
    // 初期配置と全評価した目的値を保持する。O(n^2)
    qap_swap_state(const qap_prepared<Weight, Score>& prepared,
                   std::vector<int> location_of)
        : prepared_(&prepared), location_of_(std::move(location_of)),
          cost_(prepared.evaluate(location_of_)) {}

    // 正しい既知目的値を使って初期配置を保持する。O(n)（配置の値渡し・検査を含む）
    qap_swap_state(const qap_prepared<Weight, Score>& prepared,
                   std::vector<int> location_of, Score known_cost)
        : prepared_(&prepared), location_of_(std::move(location_of)),
          cost_(known_cost) {
        assert(prepared.problem().is_valid_permutation(location_of_));
    }

    // 現在の物体ごとの配置位置を参照で返す。O(1)
    const std::vector<int>& location_of() const noexcept {
        return location_of_;
    }

    // 保持している現在の目的値を返す。O(1)
    Score cost() const noexcept {
        return cost_;
    }

    // 現在配置の交換差分を返す。O(n)、疎 flow では交換物体の入出次数の和
    Score swap_delta(int i, int j) const {
        return prepared_->swap_delta(location_of_, i, j);
    }

    // 交換して配置・目的値を更新し、差分を返す。O(n)、疎 flow では交換物体の入出次数の和
    Score apply_swap(int i, int j) {
        const Score delta = swap_delta(i, j);
        std::swap(location_of_[i], location_of_[j]);
        cost_ += delta;
        return delta;
    }

private:
    const qap_prepared<Weight, Score>* prepared_;
    std::vector<int> location_of_;
    Score cost_;
};

// full_initial のうち object に列挙した物体だけを、現在使っている位置集合内で
// 並べ替える厳密な縮約問題。固定物体との二次項は problem.unary に畳み込まれる。
template <class Weight = int, class Score = long long>
struct qap_subproblem {
    qap_problem<Weight, Score> problem;
    std::vector<int> base_location_of;
    std::vector<int> object;    // 縮約物体 -> 元の物体
    std::vector<int> location;  // 縮約位置 -> 元の位置
    std::vector<int> initial_location_of;
    Score constant_offset = 0;

    // 縮約解を完全配置へ戻し定数コストを加算する。O(n + k)
    qap_result<Score> restore(const qap_result<Score>& reduced) const {
        assert(problem.is_valid_permutation(reduced.location_of));
        std::vector<int> restored = base_location_of;
        for (int i = 0; i < problem.n; ++i) {
            restored[object[i]] = location[reduced.location_of[i]];
        }
        return {std::move(restored), constant_offset + reduced.cost};
    }
};

// 正しい既知目的値から厳密な部分 QAP を作る。O(n + k^2 n)
template <class Weight, class Score>
qap_subproblem<Weight, Score> make_qap_subproblem(
    const qap_problem<Weight, Score>& full_problem,
    const std::vector<int>& full_initial,
    const std::vector<int>& movable_objects,
    Score full_initial_cost) {
    full_problem.assert_shape();
    assert(full_problem.is_valid_permutation(full_initial));
    full_problem.assert_movable_objects(movable_objects);

    const int n = full_problem.n;
    const int k = static_cast<int>(movable_objects.size());
    qap_subproblem<Weight, Score> subproblem;
    subproblem.problem = qap_problem<Weight, Score>(k);
    subproblem.base_location_of = full_initial;
    subproblem.object = movable_objects;
    subproblem.location.resize(k);
    subproblem.initial_location_of.resize(k);
    std::iota(subproblem.initial_location_of.begin(),
              subproblem.initial_location_of.end(), 0);

    std::vector<unsigned char> is_movable(n);
    for (int a = 0; a < k; ++a) {
        is_movable[movable_objects[a]] = 1;
        subproblem.location[a] = full_initial[movable_objects[a]];
    }
    std::vector<int> fixed_objects;
    fixed_objects.reserve(n - k);
    for (int i = 0; i < n; ++i) {
        if (!is_movable[i]) fixed_objects.push_back(i);
    }

    for (int a = 0; a < k; ++a) {
        const int global_i = subproblem.object[a];
        for (int b = 0; b < k; ++b) {
            const int global_j = subproblem.object[b];
            subproblem.problem.flow[a * k + b] =
                full_problem.flow[global_i * n + global_j];
            subproblem.problem.distance[a * k + b] =
                full_problem.distance[subproblem.location[a] * n +
                                      subproblem.location[b]];
        }
    }

    // 固定物体を外側で処理し、必要な距離の並べ替えを O(k) の作業領域で共有する。
    std::vector<Score> unary(static_cast<std::size_t>(k) * k);
    if (!full_problem.unary.empty()) {
        for (int a = 0; a < k; ++a) {
            for (int x = 0; x < k; ++x) {
                unary[a * k + x] = full_problem.unary[subproblem.object[a] * n + subproblem.location[x]];
            }
        }
    }
    std::vector<Weight> outgoing_distance(k), incoming_distance(k);
    for (int fixed : fixed_objects) {
        bool ready = false;
        for (int a = 0; a < k; ++a) {
            const int global_i = subproblem.object[a];
            const Weight outgoing = full_problem.flow[global_i * n + fixed];
            const Weight incoming = full_problem.flow[fixed * n + global_i];
            if (outgoing == Weight{} && incoming == Weight{}) continue;
            if (!ready) {
                const int fixed_location = full_initial[fixed];
                for (int x = 0; x < k; ++x) {
                    const int global_x = subproblem.location[x];
                    outgoing_distance[x] = full_problem.distance[global_x * n + fixed_location];
                    incoming_distance[x] = full_problem.distance[fixed_location * n + global_x];
                }
                ready = true;
            }
            Score* unary_row = unary.data() + a * k;
            for (int x = 0; x < k; ++x) {
                unary_row[x] += Score(outgoing) * Score(outgoing_distance[x]);
                unary_row[x] += Score(incoming) * Score(incoming_distance[x]);
            }
        }
    }
    if (std::any_of(unary.begin(), unary.end(), [](Score value) { return value != Score{}; })) {
        subproblem.problem.unary = std::move(unary);
    }

    subproblem.constant_offset =
        full_initial_cost -
        subproblem.problem.evaluate(subproblem.initial_location_of);
    return subproblem;
}

// 完全目的値を再計算し、k 物体の厳密な部分 QAP を作る。O(n^2 + k^2 n)
template <class Weight, class Score>
qap_subproblem<Weight, Score> make_qap_subproblem(
    const qap_problem<Weight, Score>& problem,
    const std::vector<int>& full_initial,
    const std::vector<int>& movable_objects) {
    const Score initial_cost = problem.evaluate(full_initial);
    return make_qap_subproblem(
        problem, full_initial, movable_objects, initial_cost);
}

// 正しい既知目的値を使い可動物体だけを改善する。前処理 O(n + k^2 n)、メモリ O(n + k^2)（k < n）
template <class Weight, class Score>
qap_result<Score> improve_qap_subset(
    const qap_problem<Weight, Score>& problem,
    const std::vector<int>& full_initial,
    const std::vector<int>& movable_objects,
    Score full_initial_cost,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed) {
    problem.assert_shape();
    assert(problem.is_valid_permutation(full_initial));
    problem.assert_movable_objects(movable_objects);
    const int k = static_cast<int>(movable_objects.size());
    if (k < 2 || std::chrono::steady_clock::now() >= deadline) {
        return {full_initial, full_initial_cost};
    }
    if (k == problem.n) {
        return improve_qap(problem, full_initial, deadline, seed);
    }

    auto subproblem = make_qap_subproblem(
        problem, full_initial, movable_objects, full_initial_cost);
    qap_prepared<Weight, Score> prepared(subproblem.problem);
    const auto reduced = prepared.improve(
        subproblem.initial_location_of, deadline, seed);
    return subproblem.restore(reduced);
}

// 完全目的値を再計算し可動物体だけを改善する。前処理 O(n^2 + k^2 n)、メモリ O(n + k^2)（k < n）
template <class Weight, class Score>
qap_result<Score> improve_qap_subset(
    const qap_problem<Weight, Score>& problem,
    const std::vector<int>& full_initial,
    const std::vector<int>& movable_objects,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    const Score initial_cost = problem.evaluate(full_initial);
    return improve_qap_subset(problem, full_initial, movable_objects,
                              initial_cost, deadline, seed);
}

#if __INCLUDE_LEVEL__ == 0

// 検証専用。通常のライブラリ利用時には include しない。
struct qap_test_access {
    template <class Weight, class Score>
    using evaluator = typename qap_prepared<Weight, Score>::evaluator;
    template <class Weight, class Score>
    using delta_table = typename qap_prepared<Weight, Score>::delta_table;
    using rng64 = qap_prepared<int, long long>::rng64;
    template <class Weight, class Score>
    static std::vector<int> initial_permutation(
        const qap_problem<Weight, Score>& problem, std::uint64_t seed) {
        return qap_prepared<Weight, Score>::initial_permutation(problem, seed);
    }
};

namespace qap_test {

struct test_rng {
    std::uint64_t state = 1;
    std::uint64_t operator()() {
        state ^= state << 7;
        state ^= state >> 9;
        return state;
    }
    int next_int(int bound) {
        return static_cast<int>((static_cast<__uint128_t>((*this)()) *
                                 static_cast<std::uint64_t>(bound)) >>
                                64);
    }
};

void test_edge_cases() {
    qap_problem<int, long long> empty(0);
    auto empty_result = solve_qap(empty, std::chrono::steady_clock::now(), 1);
    assert(empty_result.location_of.empty());
    assert(empty_result.cost == 0);

    qap_problem<int, long long> one(1);
    one.flow[0] = 3;
    one.distance[0] = 7;
    one.unary = {5};
    assert(one.evaluate({0}) == 26);
    assert(one.swap_delta({0}, 0, 0) == 0);
}

void test_score_limits() {
    const auto check = []<class Score>() {
        const Score maximum = std::numeric_limits<Score>::max();
        const Score minimum = std::numeric_limits<Score>::lowest();
        for (const Score base : {Score(0), minimum}) {
            // 2配置の差を最大値ちょうどにし、正負の差分と最小値側の目的値も確認する
            qap_problem<int, Score> problem(2);
            problem.unary = {base, Score(base + maximum), 0, 0};
            const std::vector<int> initial{0, 1}, swapped{1, 0};
            assert(problem.evaluate(initial) == base);
            assert(problem.evaluate(swapped) == base + maximum);
            assert(problem.swap_delta(initial, 0, 1) == maximum);
            assert(problem.swap_delta(swapped, 0, 1) == -maximum);

            qap_prepared<int, Score> prepared(problem);
            qap_swap_state<int, Score> state(prepared, initial, base);
            assert(state.apply_swap(0, 1) == maximum);
            assert(state.cost() == base + maximum);
            assert(state.apply_swap(1, 0) == -maximum);
            assert(state.cost() == base);

            // 差分表を往復更新し、最大値を持つ差分も通常の値として保持する
            qap_test_access::evaluator<int, Score> eval(problem);
            qap_test_access::delta_table<int, Score> table(
                problem, eval, initial, base,
                std::chrono::steady_clock::now() + std::chrono::seconds(10));
            assert(table.valid && table.delta[1] == maximum);
            table.apply(0, 1);
            assert(table.cost == base + maximum && table.delta[1] == -maximum);
            table.apply(1, 0);
            assert(table.cost == base && table.delta[1] == maximum);

            // 縮約と探索でも、境界値の目的値・置換の整合性を保つ
            const auto sub = make_qap_subproblem(problem, initial, std::vector<int>{1, 0}, base);
            const auto restored = sub.restore({swapped, sub.problem.evaluate(swapped)});
            assert(restored.cost == problem.evaluate(restored.location_of));
            const auto result = prepared.improve(
                initial, std::chrono::steady_clock::now() + std::chrono::milliseconds(5));
            assert(result.cost == base && result.location_of == initial);
        }
    };
    check.operator()<int>();
    check.operator()<long long>();
    check.operator()<__int128_t>();
}

void test_random_delta() {
    test_rng rng;
    for (int n = 2; n <= 24; ++n) {
        for (int instance = 0; instance < 20; ++instance) {
            qap_problem<int, long long> problem(n);
            problem.unary.resize(n * n);
            for (int& value : problem.flow) value = rng.next_int(31) - 7;
            for (int& value : problem.distance) value = rng.next_int(41) - 11;
            for (long long& value : problem.unary) value = rng.next_int(29) - 9;
            std::vector<int> permutation(n);
            std::iota(permutation.begin(), permutation.end(), 0);
            for (int i = n - 1; i > 0; --i) {
                std::swap(permutation[i], permutation[rng.next_int(i + 1)]);
            }
            long long cost = problem.evaluate(permutation);

            // 多数のランダム交換について、差分加算後の値と全再計算値を照合する。
            for (int operation = 0; operation < 1000; ++operation) {
                int i = rng.next_int(n);
                int j = rng.next_int(n - 1);
                if (j >= i) ++j;
                const long long delta = problem.swap_delta(permutation, i, j);
                std::swap(permutation[i], permutation[j]);
                cost += delta;
                assert(cost == problem.evaluate(permutation));
            }
        }
    }
}

void test_internal_delta_variants() {
    test_rng rng;
    for (int n = 2; n <= 36; ++n) {
        qap_problem<int, long long> problem(n);
        problem.unary.resize(n * n);
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                const int flow = rng.next_int(5) == 0 ? rng.next_int(100) + 1 : 0;
                const int distance = rng.next_int(100) + 1;
                problem.flow[i * n + j] = problem.flow[j * n + i] = flow;
                problem.distance[i * n + j] = problem.distance[j * n + i] = distance;
            }
        }
        for (long long& value : problem.unary) value = rng.next_int(50);
        qap_test_access::evaluator<int, long long> eval(problem);
        assert(eval.symmetric);
        std::vector<int> permutation(n);
        std::iota(permutation.begin(), permutation.end(), 0);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        qap_test_access::delta_table<int, long long> table(
            problem, eval, permutation, problem.evaluate(permutation), deadline);
        assert(table.valid);

        // 疎・対称の直接差分と全差分表を、一般式および全再計算に照合する。
        for (int operation = 0; operation < 400; ++operation) {
            int i = rng.next_int(n);
            int j = rng.next_int(n - 1);
            if (j >= i) ++j;
            if (i > j) std::swap(i, j);
            const long long expected = problem.swap_delta(table.permutation, i, j);
            assert(eval.swap_delta(table.permutation, i, j) == expected);
            assert(table.delta[i * n + j] == expected);
            table.apply(i, j);
            assert(table.cost == problem.evaluate(table.permutation));
        }
    }
}

void test_general_delta_table() {
    test_rng rng;
    for (int n = 2; n <= 20; ++n) {
        qap_problem<int, long long> problem(n);
        problem.unary.resize(n * n);
        for (int& value : problem.flow) value = rng.next_int(31) - 7;
        for (int& value : problem.distance) value = rng.next_int(41) - 11;
        for (long long& value : problem.unary) value = rng.next_int(29) - 9;
        std::vector<int> permutation(n);
        std::iota(permutation.begin(), permutation.end(), 0);
        for (int i = n - 1; i > 0; --i) {
            std::swap(permutation[i], permutation[rng.next_int(i + 1)]);
        }
        qap_test_access::evaluator<int, long long> eval(problem);
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(10);
        qap_test_access::delta_table<int, long long> table(
            problem, eval, permutation, problem.evaluate(permutation), deadline);
        assert(table.valid);
        for (int operation = 0; operation < 300; ++operation) {
            int i = rng.next_int(n);
            int j = rng.next_int(n - 1);
            if (j >= i) ++j;
            if (i > j) std::swap(i, j);
            assert(table.delta[i * n + j] ==
                   problem.swap_delta(table.permutation, i, j));
            table.apply(i, j);
            assert(table.cost == problem.evaluate(table.permutation));
        }
    }
}

void test_solver() {
    test_rng rng;
    for (int kind = 0; kind < 4; ++kind) {
        const int n = 28;
        qap_problem<int, long long> problem(n);
        if (kind == 3) problem.unary.resize(n * n);
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (i == j) continue;
                if (kind == 0 || rng.next_int(100) < 15) {
                    problem.flow[i * n + j] = rng.next_int(100) + 1;
                }
                problem.distance[i * n + j] = rng.next_int(100) + 1;
            }
        }
        if (kind == 1 || kind == 2) {
            for (int i = 0; i < n; ++i) {
                for (int j = i + 1; j < n; ++j) {
                    problem.flow[j * n + i] = problem.flow[i * n + j];
                    problem.distance[j * n + i] = problem.distance[i * n + j];
                }
            }
        }
        for (long long& value : problem.unary) value = rng.next_int(200);
        std::vector<int> initial(n);
        std::iota(initial.begin(), initial.end(), 0);
        const long long initial_cost = problem.evaluate(initial);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(25);
        auto result = improve_qap(problem, initial, deadline, static_cast<std::uint64_t>(kind + 1));
        assert(problem.is_valid_permutation(result.location_of));
        assert(result.cost == problem.evaluate(result.location_of));
        assert(result.cost <= initial_cost);
    }
}

void test_dummy_and_score_type() {
    // 3 個の実物体と 1 個の 0-flow ダミー物体により、4 位置中 1 位置を空きにする。
    qap_problem<int, long long> problem(4);
    problem.flow[0 * 4 + 1] = problem.flow[1 * 4 + 0] = 10;
    problem.flow[1 * 4 + 2] = problem.flow[2 * 4 + 1] = 20;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) problem.distance[i * 4 + j] = std::abs(i - j);
    }
    auto result = solve_qap(problem,
                            std::chrono::steady_clock::now() + std::chrono::milliseconds(20), 9);
    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    qap_problem<long long, __int128_t> wide(2);
    wide.flow = {0, 1'000'000'000'000LL, 1'000'000'000'000LL, 0};
    wide.distance = {0, 1'000'000'000'000LL, 1'000'000'000'000LL, 0};
    assert(wide.evaluate({0, 1}) == static_cast<__int128_t>(2) * 1'000'000'000'000LL *
                                        1'000'000'000'000LL);
    auto wide_result = solve_qap(wide, std::chrono::steady_clock::now(), 3);
    assert(wide.is_valid_permutation(wide_result.location_of));
}

void test_result_output() {
    const auto render = [](const auto& result) {
        std::ostringstream out;
        assert(&(out << result) == &out);
        return out.str();
    };
    static_assert(std::is_aggregate_v<qap_result<long long>>);
    assert(render(qap_result<>{}) == "qap_result{cost=0, location_of=[]}");
    assert(render(qap_result<int>{{2, 0, 1}, -17}) ==
           "qap_result{cost=-17, location_of=[2, 0, 1]}");
    assert(render(qap_result<long long>{{0}, std::numeric_limits<long long>::min()}) ==
           "qap_result{cost=-9223372036854775808, location_of=[0]}");
    const __int128_t maximum = static_cast<__int128_t>((__uint128_t{1} << 127) - 1);
    assert(render(qap_result<__int128_t>{{}, maximum}) ==
           "qap_result{cost=170141183460469231731687303715884105727, location_of=[]}");
    assert(render(qap_result<__int128_t>{{}, -maximum - 1}) ==
           "qap_result{cost=-170141183460469231731687303715884105728, location_of=[]}");
    assert(render(qap_result<__int128_t>{}) == "qap_result{cost=0, location_of=[]}");
    assert(render(qap_result<__uint128_t>{{}, ~__uint128_t{0}}) ==
           "qap_result{cost=340282366920938463463374607431768211455, location_of=[]}");

    std::vector<int> values(20);
    std::iota(values.begin(), values.end(), 0);
    const std::string first_twenty =
        "0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19";
    assert(render(qap_result<>{values, 7}) ==
           "qap_result{cost=7, location_of=[" + first_twenty + "]}");
    for (int count : {21, 100000}) {
        values.resize(count, 987654321);
        const qap_result<> result{values, 7};
        assert(render(result) == "qap_result{cost=7, location_of=[" + first_twenty +
               ", ...] (size=" + std::to_string(count) + ")}");
        assert(result.location_of == values && result.cost == 7);
    }
    std::ostringstream out;
    out << std::hex << std::showbase << std::boolalpha;
    const auto flags = out.flags();
    out << qap_result<int>{{0}, 15} << ' ' << 16 << ' ' << true;
    assert(out.flags() == flags);
    assert(out.str() == "qap_result{cost=0xf, location_of=[0]} 0x10 true");
}

}  // namespace qap_test

namespace qap_subset_test {

struct rng {
    std::uint64_t state = 1;
    std::uint64_t operator()() {
        state ^= state << 7;
        state ^= state >> 9;
        return state;
    }
    int uniform_int(int bound) {
        return static_cast<int>((static_cast<__uint128_t>((*this)()) *
                                 static_cast<std::uint64_t>(bound)) >>
                                64);
    }
};

std::vector<int> random_permutation(int n, rng& random) {
    std::vector<int> permutation(n);
    std::iota(permutation.begin(), permutation.end(), 0);
    for (int i = n - 1; i > 0; --i) {
        std::swap(permutation[i], permutation[random.uniform_int(i + 1)]);
    }
    return permutation;
}

qap_problem<int, long long> random_problem(int n, rng& random, int kind) {
    qap_problem<int, long long> problem(n);
    if (kind != 2) problem.unary.resize(n * n);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (kind == 1 && random.uniform_int(100) >= 12) continue;
            problem.flow[i * n + j] = random.uniform_int(31) - 9;
            problem.distance[i * n + j] = random.uniform_int(41) - 13;
        }
    }
    if (kind == 2) {
        for (int i = 0; i < n; ++i) {
            problem.flow[i * n + i] = 0;
            problem.distance[i * n + i] = 0;
            for (int j = i + 1; j < n; ++j) {
                const int flow = random.uniform_int(100) < 15
                                     ? random.uniform_int(30) + 1
                                     : 0;
                const int distance = random.uniform_int(40) + 1;
                problem.flow[i * n + j] = problem.flow[j * n + i] = flow;
                problem.distance[i * n + j] =
                    problem.distance[j * n + i] = distance;
            }
        }
    }
    for (long long& value : problem.unary) {
        value = random.uniform_int(29) - 10;
    }
    return problem;
}

void test_subproblem_identity() {
    rng random;
    for (int kind = 0; kind < 3; ++kind) {
        for (int n = 0; n <= 22; ++n) {
            for (int instance = 0; instance < 8; ++instance) {
                const auto problem = random_problem(n, random, kind);
                const auto initial = random_permutation(n, random);
                const long long initial_cost = problem.evaluate(initial);
                std::vector<int> order(n);
                std::iota(order.begin(), order.end(), 0);
                for (int i = n - 1; i > 0; --i) {
                    std::swap(order[i], order[random.uniform_int(i + 1)]);
                }
                const int k = n == 0 ? 0 : random.uniform_int(n + 1);
                order.resize(k);
                const auto a = make_qap_subproblem(problem, initial, order);
                const auto b = make_qap_subproblem(
                    problem, initial, order, initial_cost);
                assert(a.constant_offset == b.constant_offset);
                assert(a.problem.flow == b.problem.flow);
                assert(a.problem.distance == b.problem.distance);
                assert(a.problem.unary == b.problem.unary);
                assert(a.restore({a.initial_location_of,
                                  a.problem.evaluate(a.initial_location_of)})
                           .cost == initial_cost);

                for (int trial = 0; trial < 20; ++trial) {
                    const auto reduced_location = random_permutation(k, random);
                    const long long reduced_cost =
                        a.problem.evaluate(reduced_location);
                    const auto restored =
                        a.restore({reduced_location, reduced_cost});
                    assert(problem.is_valid_permutation(restored.location_of));
                    assert(restored.cost == problem.evaluate(restored.location_of));
                    for (int i = 0; i < n; ++i) {
                        if (std::find(order.begin(), order.end(), i) ==
                            order.end()) {
                            assert(restored.location_of[i] == initial[i]);
                        }
                    }
                }
            }
        }
    }
}

void test_prepared_and_state() {
    rng random;
    for (int kind = 0; kind < 3; ++kind) {
        for (int n = 2; n <= 28; ++n) {
            const auto problem = random_problem(n, random, kind);
            qap_prepared<int, long long> prepared(problem);
            auto permutation = random_permutation(n, random);
            qap_swap_state<int, long long> state(
                prepared, permutation, problem.evaluate(permutation));
            for (int operation = 0; operation < 300; ++operation) {
                int i = random.uniform_int(n);
                int j = random.uniform_int(n - 1);
                if (j >= i) ++j;
                const long long expected =
                    problem.swap_delta(state.location_of(), i, j);
                assert(prepared.swap_delta(state.location_of(), i, j) == expected);
                assert(state.apply_swap(i, j) == expected);
                assert(state.cost() == problem.evaluate(state.location_of()));
            }
            if (n == 28) {
                const auto solved = prepared.solve(
                    std::chrono::steady_clock::now(),
                    static_cast<std::uint64_t>(kind + 1));
                assert(problem.is_valid_permutation(solved.location_of));
                assert(solved.cost == problem.evaluate(solved.location_of));
            }
        }
    }
}

void test_subset_solver() {
    rng random;
    for (int kind = 0; kind < 3; ++kind) {
        const int n = 30;
        const auto problem = random_problem(n, random, kind);
        const auto initial = random_permutation(n, random);
        const long long initial_cost = problem.evaluate(initial);
        for (int k : {0, 1, 7, 30}) {
            std::vector<int> movable(n);
            std::iota(movable.begin(), movable.end(), 0);
            for (int i = n - 1; i > 0; --i) {
                std::swap(movable[i], movable[random.uniform_int(i + 1)]);
            }
            movable.resize(k);
            const auto result = improve_qap_subset(
                problem, initial, movable, initial_cost,
                std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(8),
                static_cast<std::uint64_t>(kind * 100 + k + 1));
            assert(problem.is_valid_permutation(result.location_of));
            assert(result.cost == problem.evaluate(result.location_of));
            assert(result.cost <= initial_cost);
            std::vector<unsigned char> is_movable(n);
            for (int object : movable) is_movable[object] = 1;
            for (int i = 0; i < n; ++i) {
                if (!is_movable[i]) assert(result.location_of[i] == initial[i]);
            }
            if (k == 7) {
                const auto unknown_cost_result = improve_qap_subset(
                    problem, initial, movable,
                    std::chrono::steady_clock::now(), 999);
                assert(unknown_cost_result.location_of == initial);
                assert(unknown_cost_result.cost == initial_cost);
            }
        }
    }
}

void test_wide_score() {
    qap_problem<long long, __int128_t> problem(3);
    problem.flow = {7, -1'000'000'000'000LL, 4,
                    3, 5, 1'000'000'000'000LL,
                    -2, 8, 6};
    problem.distance = {9, 1'000'000'000'000LL, -3,
                        2, 7, -1'000'000'000'000LL,
                        5, 1, 4};
    problem.unary = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    const std::vector<int> initial{2, 0, 1};
    const auto subproblem = make_qap_subproblem(
        problem, initial, std::vector<int>{2, 0}, problem.evaluate(initial));
    const std::vector<int> reduced{1, 0};
    const auto restored = subproblem.restore(
        {reduced, subproblem.problem.evaluate(reduced)});
    assert(restored.cost == problem.evaluate(restored.location_of));
}

}  // namespace qap_subset_test

// 単項が支配する問題と短い期限でも、配置・目的値・最良解の契約を保つことを確かめる
template<class Score>
void test_unary_initial_contract() {
    std::mt19937_64 rng(7312845);
    for (int n : {2, 7, 32, 96}) for (int budget : {-1, 20, 200, 2000}) {
        qap_problem<int, Score> problem(n);
        problem.unary.resize(n * n);
        for (Score& value : problem.unary) value = Score(static_cast<int>(rng() % 101) - 50);
        std::vector<int> initial(n);
        std::iota(initial.begin(), initial.end(), 0);
        std::shuffle(initial.begin(), initial.end(), rng);
        const Score initial_cost = problem.evaluate(initial);

        // 途中までの割当を補完した場合も、初期解より悪い解は返さない
        const auto result = improve_qap(problem, initial,
            std::chrono::steady_clock::now() + std::chrono::microseconds(budget), 1);
        assert(problem.is_valid_permutation(result.location_of));
        assert(problem.evaluate(result.location_of) == result.cost);
        assert(result.cost <= initial_cost);
        if (budget < 0) assert(result.location_of == initial);
    }

    // 単項だけの小問題は全列挙とも照合する。制限時間内の最適性は一般には保証しない
    for (int n = 2; n <= 5; ++n) {
        qap_problem<int, Score> problem(n);
        problem.unary.resize(n * n);
        for (Score& value : problem.unary) value = Score(static_cast<int>(rng() % 21) - 10);
        std::vector<int> initial(n);
        std::iota(initial.begin(), initial.end(), 0);
        auto permutation = initial;
        Score optimum = problem.evaluate(initial);
        do { optimum = std::min(optimum, problem.evaluate(permutation)); }
        while (std::next_permutation(permutation.begin(), permutation.end()));
        const auto result = solve_qap(problem,
            std::chrono::steady_clock::now() + std::chrono::milliseconds(10), 1);
        assert(result.cost >= optimum && result.cost <= problem.evaluate(initial));
        assert(problem.evaluate(result.location_of) == result.cost);
    }
}

int main() {
    test_unary_initial_contract<int>();
    test_unary_initial_contract<long long>();
    test_unary_initial_contract<__int128_t>();
    qap_test::test_edge_cases();
    qap_test::test_score_limits();
    qap_test::test_random_delta();
    qap_test::test_internal_delta_variants();
    qap_test::test_general_delta_table();
    qap_test::test_solver();
    qap_test::test_dummy_and_score_type();
    qap_test::test_result_output();
    qap_subset_test::test_subproblem_identity();
    qap_subset_test::test_prepared_and_state();
    qap_subset_test::test_subset_solver();
    qap_subset_test::test_wide_score();
    std::cout << "All QAP v0.9.1 tests passed (13 suites)\n";
}

#endif
