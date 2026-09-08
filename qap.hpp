/*
 * qap_solver v0.7.2: 二次割当問題を制限時間内に近似的に解く単一ヘッダー
 *
 * 物体 i を位置 p[i] に一対一で割り当て、次の値を最小化する。
 *   sum_i unary[i][p[i]] + sum_i sum_j flow[i][j] * distance[p[i]][p[j]]
 *
 * 主な機能:
 * - qap_problem / qap_result: 問題・配置・目的値を保持し、全評価と swap 差分を計算
 * - solve_qap / improve_qap: 初期解生成・既存解改善。Threshold Accepting と Robust Tabu Search を使用
 * - qap_prepared: 対称性判定と疎 flow の CSR を再利用して solve / improve / swap_delta を実行
 * - qap_swap_state: 置換と目的値を保持し、独自探索から swap 差分計算・交換を実行
 * - make_qap_subproblem: 可動物体だけの厳密な縮約 QAP を作り、restore で完全配置へ復元
 * - improve_qap_subset: 指定外の物体を固定し、可動物体が現在使う位置集合内で改善
 * - 非対称、負値、非ゼロ対角、単項コスト、__int128_t の目的値型に対応
 * - 疎 flow の隣接リスト、対称・ゼロ対角の簡略差分式を自動利用
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
 *
 * 前提・注意:
 * - GCC / C++20、シングルスレッド、標準ライブラリのみ。外部ヘッダーへの依存なし
 * - 積・総和・差分表の補正を含む全中間値は Score に収める。オーバーフロー検査は行わない
 * - 最大化は flow と unary の符号を反転する。空き位置は 0 コストのダミー物体で表現できる
 * - 一般の容量制約・配置禁止は直接扱わない。固定配置には部分問題 API を使える
 * - prepared の参照先 problem、および state の参照先 prepared は利用中に変更・破棄しない
 * - 既知 cost は正しい値を渡す。deadline は終了目安であり、厳密な停止時刻ではない
 * - 直接コンパイルすると全自己テストを実行する。include 時はテストと main が無効になる
 */
#pragma once

#include <bits/stdc++.h>

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
        assert(n >= 0);
        assert(static_cast<long long>(flow.size()) == static_cast<long long>(n) * n);
        assert(static_cast<long long>(distance.size()) == static_cast<long long>(n) * n);
        assert(unary.empty() ||
               static_cast<long long>(unary.size()) == static_cast<long long>(n) * n);
    }

    // permutation が 0..n-1 の置換なら true を返す。O(n)
    bool is_valid_permutation(const std::vector<int>& permutation) const {
        if (static_cast<int>(permutation.size()) != n) return false;
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
};

template <class Score = long long>
struct qap_result {
    std::vector<int> location_of;
    Score cost = 0;
};

namespace qap_internal {

template <class Score>
Score score_abs(Score value) {
    return value < 0 ? -value : value;
}

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

template <class Weight, class Score>
bool is_symmetric_zero_diagonal(const qap_problem<Weight, Score>& problem) {
    const int n = problem.n;
    for (int i = 0; i < n; ++i) {
        const Weight* flow_i = problem.flow.data() + i * n;
        const Weight* distance_i = problem.distance.data() + i * n;
        if (flow_i[i] != Weight{} || distance_i[i] != Weight{}) {
            return false;
        }
        for (int j = i + 1; j < n; ++j) {
            if (flow_i[j] != problem.flow[j * n + i] ||
                distance_i[j] != problem.distance[j * n + i]) {
                return false;
            }
        }
    }
    return true;
}

template <class Weight>
struct sparse_flow {
    std::vector<int> out_offset;
    std::vector<int> out_vertex;
    std::vector<Weight> out_weight;
    std::vector<int> in_offset;
    std::vector<int> in_vertex;
    std::vector<Weight> in_weight;

    template <class Score>
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

template <class Weight, class Score>
struct evaluator {
    const qap_problem<Weight, Score>* problem;
    bool symmetric;
    std::optional<sparse_flow<Weight>> sparse;

    explicit evaluator(const qap_problem<Weight, Score>& problem_)
        : problem(&problem_), symmetric(is_symmetric_zero_diagonal(problem_)) {
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

template <class Score>
struct search_result {
    std::vector<int> permutation;
    Score cost;
};

template <class Weight, class Score>
search_result<Score> threshold_search(
    const qap_problem<Weight, Score>& problem,
    const evaluator<Weight, Score>& eval,
    const std::vector<int>& initial,
    Score initial_cost,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed) {
    rng64 rng(seed);
    std::vector<int> permutation = initial;
    Score current = initial_cost;
    search_result<Score> result{permutation, current};
    const int n = problem.n;
    if (n < 2 || std::chrono::steady_clock::now() >= deadline) return result;

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
                total_seconds > 0.0L
                    ? std::min(1.0L,
                               std::chrono::duration<long double>(now - start).count() /
                                   total_seconds)
                    : 1.0L;
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
                result.permutation = permutation;
            }
        }
        ++iteration;
    }
    return result;
}

template <class Weight, class Score>
struct delta_table {
    const qap_problem<Weight, Score>* problem;
    const evaluator<Weight, Score>* eval;
    std::vector<int> permutation;
    std::vector<Score> delta;
    Score cost;
    std::vector<Score> update_work;
    bool valid = true;

    delta_table(const qap_problem<Weight, Score>& problem_,
                const evaluator<Weight, Score>& eval_, std::vector<int> permutation_,
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

template <class Weight, class Score>
bool tabu_table_is_affordable(const qap_problem<Weight, Score>& problem,
                              const evaluator<Weight, Score>& eval,
                              const std::vector<int>& permutation,
                              std::chrono::steady_clock::time_point deadline,
                              std::uint64_t seed) {
    const int n = problem.n;
    const long long pairs = static_cast<long long>(n) * (n - 1) / 2;
    if (pairs == 0) return false;
    const int samples = static_cast<int>(std::min<long long>(pairs, std::max(16, n)));
    rng64 rng(seed);
    // 符号付き Score の加算オーバーフローを避けつつ、計測対象の計算が消去されないようにする。
    std::uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int sample = 0; sample < samples; ++sample) {
        int i = rng.uniform_int(n);
        int j = rng.uniform_int(n - 1);
        if (j >= i) ++j;
        checksum += static_cast<std::uint64_t>(eval.swap_delta(permutation, i, j));
    }
    const auto finish = std::chrono::steady_clock::now();
    if (checksum == std::numeric_limits<std::uint64_t>::max()) return false;
    const auto remaining = deadline - finish;
    if (remaining <= std::chrono::steady_clock::duration::zero()) return false;
    const long double multiplier = static_cast<long double>(pairs) / samples;
    const auto estimated = std::chrono::duration<long double>(finish - start) * multiplier;
    return estimated * 2.0L < std::chrono::duration<long double>(remaining);
}

template <class Weight, class Score>
search_result<Score> tabu_search(const qap_problem<Weight, Score>& problem,
                                 const evaluator<Weight, Score>& eval,
                                 const std::vector<int>& initial,
                                 Score initial_cost,
                                 std::chrono::steady_clock::time_point deadline,
                                 std::uint64_t seed) {
    delta_table<Weight, Score> table(
        problem, eval, initial, initial_cost, deadline);
    search_result<Score> result{initial, initial_cost};
    if (!table.valid || problem.n < 2) return result;
    const int n = problem.n;
    rng64 rng(seed);
    std::vector<long long> tabu_until(n * n);
    long long iteration = 0;
    std::chrono::steady_clock::duration previous_apply{};

    while (std::chrono::steady_clock::now() < deadline) {
        Score best_delta = std::numeric_limits<Score>::max();
        int best_i = -1;
        int best_j = -1;

        // 全 swap から、tabu でない最良手または最良値を更新する aspiration 手を選ぶ。
        for (int i = 0; i < n; ++i) {
            const int pi = table.permutation[i];
            const Score* delta_row = table.delta.data() + i * n;
            const long long* tabu_row = tabu_until.data() + i * n;
            for (int j = i + 1; j < n; ++j) {
                const Score delta = delta_row[j];
                if (delta >= best_delta) continue;
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
            result.permutation = table.permutation;
        }
    }
    return result;
}

template <class Weight, class Score>
search_result<Score> continue_hybrid_search(
    const qap_problem<Weight, Score>& problem,
    const evaluator<Weight, Score>& eval,
    search_result<Score> result,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed) {
    const auto start = std::chrono::steady_clock::now();
    if (start >= deadline) return result;
    const auto duration = deadline - start;

    const auto first_deadline = start + duration / 4;
    const auto second_deadline = start + duration / 2;

    // Threshold Accepting を 2 周期に分け、前半の最良解から閾値を再設定する。
    auto first = threshold_search(problem, eval, result.permutation, result.cost,
                                  first_deadline, seed);
    if (first.cost < result.cost) result = first;
    auto second = threshold_search(problem, eval, result.permutation, result.cost,
                                   second_deadline,
                                   seed + 0x9e3779b97f4a7c15ULL);
    if (second.cost < result.cost) result = std::move(second);
    if (std::chrono::steady_clock::now() >= deadline) return result;

    // 差分表構築が残り時間に収まる場合だけ tabu を行い、大規模時は閾値探索を継続する。
    if (tabu_table_is_affordable(problem, eval, result.permutation, deadline,
                                 seed ^ 0x94d049bb133111ebULL)) {
        auto tabu = tabu_search(problem, eval, result.permutation, result.cost,
                                deadline,
                                seed ^ 0xd1b54a32d192ed03ULL);
        if (tabu.cost < result.cost) result = std::move(tabu);
    } else {
        auto tail = threshold_search(problem, eval, result.permutation, result.cost,
                                     deadline,
                                     seed ^ 0xe7037ed1a0b428dbULL);
        if (tail.cost < result.cost) result = std::move(tail);
    }
    return result;
}

template <class Weight, class Score>
search_result<Score> hybrid_search(const qap_problem<Weight, Score>& problem,
                                   const std::vector<int>& initial,
                                   std::chrono::steady_clock::time_point deadline,
                                   std::uint64_t seed) {
    search_result<Score> result{initial, problem.evaluate(initial)};
    if (problem.n < 2 || std::chrono::steady_clock::now() >= deadline) return result;
    evaluator<Weight, Score> eval(problem);
    return continue_hybrid_search(
        problem, eval, std::move(result), deadline, seed);
}

template <class Weight, class Score>
std::vector<int> initial_permutation(const qap_problem<Weight, Score>& problem,
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
            const Score flow = score_abs(Score(problem.flow[i * n + j]));
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

template <class Weight, class Score>
void assert_problem_shape(const qap_problem<Weight, Score>& problem) {
#ifndef NDEBUG
    assert(problem.n >= 0);
    const long long size = static_cast<long long>(problem.n) * problem.n;
    assert(static_cast<long long>(problem.flow.size()) == size);
    assert(static_cast<long long>(problem.distance.size()) == size);
    assert(problem.unary.empty() ||
           static_cast<long long>(problem.unary.size()) == size);
#else
    (void)problem;
#endif
}

}  // namespace qap_internal

// 初期解を内部生成し、deadline まで QAP を近似的に解く。O(n^2) memory
template <class Weight, class Score>
qap_result<Score> solve_qap(const qap_problem<Weight, Score>& problem,
                            std::chrono::steady_clock::time_point deadline,
                            std::uint64_t seed = 1) {
    qap_internal::assert_problem_shape(problem);
    std::vector<int> initial = qap_internal::initial_permutation(problem, seed);
    auto result = qap_internal::hybrid_search(problem, initial, deadline, seed);
    return {std::move(result.permutation), result.cost};
}

// 与えられた置換を保持候補に含め、deadline まで QAP 解を改善する。O(n^2) memory
template <class Weight, class Score>
qap_result<Score> improve_qap(const qap_problem<Weight, Score>& problem,
                              const std::vector<int>& initial,
                              std::chrono::steady_clock::time_point deadline,
                              std::uint64_t seed = 1) {
    assert(problem.is_valid_permutation(initial));
    auto result = qap_internal::hybrid_search(problem, initial, deadline, seed);
    return {std::move(result.permutation), result.cost};
}

namespace qap_internal {

// 構築済み evaluator を受け取り、共通のハイブリッド探索を行う。
// qap_prepared から繰り返し呼ぶことで、対称性判定と CSR 構築を省略できる。
template <class Weight, class Score>
search_result<Score> hybrid_search_prepared(
    const qap_problem<Weight, Score>& problem,
    const evaluator<Weight, Score>& eval,
    const std::vector<int>& initial,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed) {
    search_result<Score> result{initial, problem.evaluate(initial)};
    if (problem.n < 2 || std::chrono::steady_clock::now() >= deadline) return result;
    return continue_hybrid_search(
        problem, eval, std::move(result), deadline, seed);
}

template <class Weight, class Score>
void assert_movable_objects(const qap_problem<Weight, Score>& problem,
                            const std::vector<int>& movable_objects) {
#ifndef NDEBUG
    std::vector<unsigned char> used(problem.n);
    for (int object : movable_objects) {
        assert(0 <= object && object < problem.n);
        assert(!used[object]);
        used[object] = 1;
    }
#else
    (void)problem;
    (void)movable_objects;
#endif
}

}  // namespace qap_internal

// 同じ問題に対する多数の差分評価・探索で、O(n^2) の前処理を再利用する。
// problem はこのオブジェクトより長く生存し、利用中に変更されてはならない。
template <class Weight = int, class Score = long long>
class qap_prepared {
public:
    // 問題への参照と再利用用の前処理を構築する。O(n^2)
    explicit qap_prepared(const qap_problem<Weight, Score>& problem)
        : problem_(&problem), evaluator_(problem) {
        qap_internal::assert_problem_shape(problem);
    }

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
        std::vector<int> initial =
            qap_internal::initial_permutation(*problem_, seed);
        auto result = qap_internal::hybrid_search_prepared(
            *problem_, evaluator_, initial, deadline, seed);
        return {std::move(result.permutation), result.cost};
    }

    // 初期配置を保持候補に含めて探索する。初期評価 O(n^2)、メモリ O(n^2)
    qap_result<Score> improve(
        const std::vector<int>& initial,
        std::chrono::steady_clock::time_point deadline,
        std::uint64_t seed = 1) const {
        assert(problem_->is_valid_permutation(initial));
        auto result = qap_internal::hybrid_search_prepared(
            *problem_, evaluator_, initial, deadline, seed);
        return {std::move(result.permutation), result.cost};
    }

private:
    const qap_problem<Weight, Score>* problem_;
    qap_internal::evaluator<Weight, Score> evaluator_;
};

// 外側の探索が置換と目的値を保持しながら、QAP の swap 近傍だけを利用するための状態。
template <class Weight = int, class Score = long long>
class qap_swap_state {
public:
    // 初期配置と全評価した目的値を保持する。O(n^2)
    qap_swap_state(const qap_prepared<Weight, Score>& prepared,
                   std::vector<int> location_of)
        : prepared_(&prepared), location_of_(std::move(location_of)),
          cost_(prepared.evaluate(location_of_)) {
        assert(prepared.problem().is_valid_permutation(location_of_));
    }

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

namespace qap_internal {

template <class Weight, class Score>
qap_subproblem<Weight, Score> make_qap_subproblem_impl(
    const qap_problem<Weight, Score>& full_problem,
    const std::vector<int>& full_initial,
    const std::vector<int>& movable_objects,
    Score full_initial_cost) {
    assert_problem_shape(full_problem);
    assert(full_problem.is_valid_permutation(full_initial));
    assert_movable_objects(full_problem, movable_objects);

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
    for (int a = 0; a < k; ++a) {
        if (full_problem.unary.empty()) break;
        for (int x = 0; x < k; ++x) {
            unary[a * k + x] = full_problem.unary[subproblem.object[a] * n + subproblem.location[x]];
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
    bool has_nonzero_unary = false;
    for (const Score& value : unary) has_nonzero_unary = has_nonzero_unary || value != Score{};
    if (has_nonzero_unary) subproblem.problem.unary = std::move(unary);

    subproblem.constant_offset =
        full_initial_cost -
        subproblem.problem.evaluate(subproblem.initial_location_of);
    return subproblem;
}

}  // namespace qap_internal

// 完全目的値を再計算し、k 物体の厳密な部分 QAP を作る。O(n^2 + k^2 n)
template <class Weight, class Score>
qap_subproblem<Weight, Score> make_qap_subproblem(
    const qap_problem<Weight, Score>& problem,
    const std::vector<int>& full_initial,
    const std::vector<int>& movable_objects) {
    const Score initial_cost = problem.evaluate(full_initial);
    return qap_internal::make_qap_subproblem_impl(
        problem, full_initial, movable_objects, initial_cost);
}

// 正しい既知目的値から厳密な部分 QAP を作る。O(n + k^2 n)
template <class Weight, class Score>
qap_subproblem<Weight, Score> make_qap_subproblem(
    const qap_problem<Weight, Score>& problem,
    const std::vector<int>& full_initial,
    const std::vector<int>& movable_objects,
    Score full_initial_cost) {
    return qap_internal::make_qap_subproblem_impl(
        problem, full_initial, movable_objects, full_initial_cost);
}

// 正しい既知目的値を使い可動物体だけを改善する。前処理 O(n + k^2 n)、メモリ O(n + k^2)（k < n）
template <class Weight, class Score>
qap_result<Score> improve_qap_subset(
    const qap_problem<Weight, Score>& problem,
    const std::vector<int>& full_initial,
    const std::vector<int>& movable_objects,
    Score full_initial_cost,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    qap_internal::assert_problem_shape(problem);
    assert(problem.is_valid_permutation(full_initial));
    qap_internal::assert_movable_objects(problem, movable_objects);
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
        qap_internal::evaluator<int, long long> eval(problem);
        assert(eval.symmetric);
        std::vector<int> permutation(n);
        std::iota(permutation.begin(), permutation.end(), 0);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        qap_internal::delta_table<int, long long> table(
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
        qap_internal::evaluator<int, long long> eval(problem);
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(10);
        qap_internal::delta_table<int, long long> table(
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

int main() {
    qap_test::test_edge_cases();
    qap_test::test_random_delta();
    qap_test::test_internal_delta_variants();
    qap_test::test_general_delta_table();
    qap_test::test_solver();
    qap_test::test_dummy_and_score_type();
    qap_subset_test::test_subproblem_identity();
    qap_subset_test::test_prepared_and_state();
    qap_subset_test::test_subset_solver();
    qap_subset_test::test_wide_score();
    std::cout << "All QAP v0.7.2 tests passed (10 suites)\n";
}

#endif
