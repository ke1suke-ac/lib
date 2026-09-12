/*
 * capacitated_graph_labeling: 容量制約付きグラフラベリングを近似的に解くヒューリスティックsolver
 *
 * 各頂点へラベルを1つ割り当て、頂点とラベルの単項コスト、および辺の両端ラベルに
 * 依存する二項コストの合計を最小化する。ラベルごとに需要量と頂点数の下限・上限、頂点ごとに
 * 固定ラベル・許可ラベルを指定できる。容量を考慮した複数回のgreedy構築後、recolorとswapを
 * 混合した焼きなましで改善し、常に最良の実行可能解を保持する。
 *
 * 主な用途:
 *   - balanced graph partition / capacitated clustering
 *   - 容量付きMax-Cut・グラフ彩色の近似
 *   - 頂点を日・担当者・設備へ割り当て、関係する頂点間の相性も評価する問題
 *   - AHC本体の指定頂点だけを変更し、境界辺と全体容量を保つ部分repair
 *
 * 選別済みの標準探索:
 *   - 頂点は辺端点／全頂点から50%ずつ、近傍はrecolor 55%／swap 45%で選ぶ
 *   - 1提案でrecolorを最大4候補、swapを最大8候補比較する
 *   - Potts型の乱数候補は隣接誘導87.5%／一様12.5%を混合し、一般二項コストは一様とする
 *   - Potts swapの隣接走査を1本の64-bit Bloom filterで抑える
 *   - 最良解更新時は、前回の最良解から実際に変更された頂点だけを同期する
 *   - 固定確率分岐は上位乱数bitを直接比較し、不要な実数変換を行わない
 *
 * 構成:
 *   - 公開データ型と5つの関数から利用し、探索エンジンと補助型はproblemのprivate内部に置く
 *   - このヘッダ自体をコンパイルした場合だけ、main内の自己テストを実行する
 *
 * コスト型:
 *   - capacitated_potts_cost: 辺重み * [両端ラベルが異なる]
 *   - capacitated_label_matrix_cost: 辺重み * 共通ラベル間コスト行列
 *   - 任意のpair_cost(edge_id, edge, label_u, label_v)を持つ関数オブジェクト
 *
 * 注意:
 *   - 需要量と容量はlong long、目的値はテンプレート引数Costで管理する
 *   - Costの演算、long longの総需要量・容量計算、intの添字が範囲内に収まる入力を使う
 *   - 浮動小数点の目的値には差分加算による丸め誤差がある。NaN・無限大のコストは使わない
 *   - 一般の重み付き需要・許可ラベル付き実行可能性判定自体が難しいため、実行可能解を
 *     構築できなかった場合はresult.feasible=falseを返す。この組合せが厳しい場合は、
 *     問題固有手法で作った実行可能解をimprove_capacitated_graph_labelingへ渡す
 *   - 部分repairはimprove_capacitated_graph_labeling_subsetへ変更可能頂点列を渡す
 *   - Potts専用キャッシュは通常O(NK + N)、部分repairではO(|S|K + |S|)メモリを使う
 *   - Potts型のgreedy構築は、各頂点の隣接辺をラベル別に一度集計して高速化する
 */
#pragma once

#include <bits/stdc++.h>

template <class Cost>
struct capacitated_graph_labeling_edge {
    int u = 0;
    int v = 0;
    Cost weight = Cost{1};
    int type = 0;
};

template <class Cost = long long>
struct capacitated_potts_cost {
    inline static constexpr bool is_potts = true;

    // 辺の両端ラベルに対するPottsコストを返す。O(1)
    Cost operator()(int, const capacitated_graph_labeling_edge<Cost>& edge,
                    int label_u, int label_v) const {
        return label_u == label_v ? Cost{} : edge.weight;
    }
};

template <class Cost = long long>
struct capacitated_label_matrix_cost {
    inline static constexpr bool is_potts = false;

    int label_count = 0;
    const std::vector<Cost>* matrix = nullptr;

    // 辺重みと共有ラベル間行列から二項コストを返す。O(1)
    Cost operator()(int, const capacitated_graph_labeling_edge<Cost>& edge,
                    int label_u, int label_v) const {
        return edge.weight * (*matrix)[label_u * label_count + label_v];
    }
};

struct capacitated_graph_labeling_options {
    double time_limit_ms = 1000.0;
    long long iteration_limit = -1;
    std::uint64_t seed = 1;
    int initial_trials = 8;
    double initial_temperature = -1.0;
    double final_temperature = -1.0;
    std::size_t potts_cache_max_bytes = 256ULL << 20;
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::time_point::max();
};

enum class capacitated_graph_labeling_status : unsigned char {
    success,
    infeasible_initial_solution,
    construction_failed,
    invalid_repair_scope,
};

template <class Cost = long long>
struct capacitated_graph_labeling_result {
    std::vector<int> label;
    std::vector<long long> load;
    std::vector<int> count;
    Cost objective = Cost{};
    Cost initial_objective = Cost{};
    long long iterations = 0;
    double elapsed_ms = 0.0;
    bool feasible = false;
    bool used_potts_cache = false;
    capacitated_graph_labeling_status status =
        capacitated_graph_labeling_status::construction_failed;
};

template <class Cost = long long>
struct capacitated_graph_labeling_repair_result {
    std::vector<int> changed_vertices;
    std::vector<int> new_labels;
    Cost objective_delta = Cost{};
    long long iterations = 0;
    double elapsed_ms = 0.0;
    bool used_potts_cache = false;
    capacitated_graph_labeling_status status =
        capacitated_graph_labeling_status::invalid_repair_scope;
};

template <class Cost = long long>
struct capacitated_graph_labeling_problem {
    static_assert(std::numeric_limits<Cost>::is_signed,
                  "Costは差分を負値で表せる符号付き型に限る");

    using edge_type = capacitated_graph_labeling_edge<Cost>;

    int vertex_count = 0;
    int label_count = 0;
    std::vector<long long> demand;
    std::vector<long long> lower_capacity;
    std::vector<long long> upper_capacity;
    std::vector<int> lower_count;
    std::vector<int> upper_count;
    std::vector<Cost> unary_cost;
    std::vector<edge_type> edges;
    std::vector<int> fixed_label;
    std::vector<unsigned char> allowed;

private:
    // 入力の解釈と探索の実装は非公開。公開データだけのaggregate初期化は維持する。
    template <class PairCost> class solver;

    template <class C, class P>
    friend C evaluate_capacitated_graph_labeling(const capacitated_graph_labeling_problem<C>&,
                                                 const std::vector<int>&, const P&);
    template <class C>
    friend bool is_feasible_capacitated_graph_labeling(const capacitated_graph_labeling_problem<C>&,
                                                       const std::vector<int>&);
    template <class C, class P>
    friend capacitated_graph_labeling_result<C> solve_capacitated_graph_labeling(
        const capacitated_graph_labeling_problem<C>&, const P&, const capacitated_graph_labeling_options&);
    template <class C, class P>
    friend capacitated_graph_labeling_result<C> improve_capacitated_graph_labeling(
        const capacitated_graph_labeling_problem<C>&, const std::vector<int>&, const P&,
        const capacitated_graph_labeling_options&);
    template <class C, class P>
    friend capacitated_graph_labeling_repair_result<C> improve_capacitated_graph_labeling_subset(
        const capacitated_graph_labeling_problem<C>&, const std::vector<int>&, const std::vector<int>&,
        const P&, const capacitated_graph_labeling_options&);
#if __INCLUDE_LEVEL__ == 0
    friend int main();
#endif

    long long get_demand(int v) const {
        return demand.empty() ? 1LL : demand[v];
    }

    long long get_lower(int label) const {
        return lower_capacity.empty() ? 0LL : lower_capacity[label];
    }

    long long get_upper(int label, long long total_demand) const {
        return upper_capacity.empty() ? total_demand : upper_capacity[label];
    }

    int get_lower_count(int label) const {
        return lower_count.empty() ? 0 : lower_count[label];
    }

    int get_upper_count(int label) const {
        return upper_count.empty() ? vertex_count : upper_count[label];
    }

    Cost get_unary(int v, int label) const {
        if (unary_cost.empty()) return Cost{};
        return unary_cost[v * label_count + label];
    }

    bool is_allowed(int v, int label) const {
        if (!fixed_label.empty() && fixed_label[v] >= 0) {
            return fixed_label[v] == label;
        }
        return allowed.empty() || allowed[v * label_count + label] != 0;
    }

    void validate() const {
        const int n = vertex_count;
        const int k = label_count;
        assert(n >= 0);
        assert(k > 0);
        assert(demand.empty() || static_cast<int>(demand.size()) == n);
        assert(lower_capacity.empty() || static_cast<int>(lower_capacity.size()) == k);
        assert(upper_capacity.empty() || static_cast<int>(upper_capacity.size()) == k);
        assert(lower_count.empty() || static_cast<int>(lower_count.size()) == k);
        assert(upper_count.empty() || static_cast<int>(upper_count.size()) == k);
        assert(unary_cost.empty() ||
               static_cast<long long>(unary_cost.size()) == 1LL * n * k);
        assert(fixed_label.empty() || static_cast<int>(fixed_label.size()) == n);
        assert(allowed.empty() ||
               static_cast<long long>(allowed.size()) == 1LL * n * k);

        for (int v = 0; v < n; ++v) {
            assert(get_demand(v) >= 0);
            if (!fixed_label.empty()) {
                assert(fixed_label[v] >= -1 && fixed_label[v] < k);
            }
        }
        for (int label = 0; label < k; ++label) {
            assert(get_lower(label) >= 0);
            if (!upper_capacity.empty()) {
                assert(get_lower(label) <= upper_capacity[label]);
            }
            assert(get_lower_count(label) >= 0);
            assert(get_upper_count(label) <= n);
            assert(get_lower_count(label) <= get_upper_count(label));
        }
        for (const auto& edge : edges) {
            assert(edge.u >= 0 && edge.u < n);
            assert(edge.v >= 0 && edge.v < n);
            (void)edge;
        }
    }

    template <class PairCost>
    Cost evaluate(const std::vector<int>& label, const PairCost& pair_cost) const {
        Cost result{};
        for (int v = 0; v < vertex_count; ++v) {
            result += get_unary(v, label[v]);
        }
        for (int edge_id = 0; edge_id < static_cast<int>(edges.size()); ++edge_id) {
            const auto& edge = edges[edge_id];
            result += pair_cost(edge_id, edge, label[edge.u], label[edge.v]);
        }
        return result;
    }

    bool feasible(const std::vector<int>& label,
                  std::vector<long long>* output_load = nullptr,
                  std::vector<int>* output_count = nullptr) const {
        const int n = vertex_count;
        const int k = label_count;
        if (static_cast<int>(label.size()) != n) return false;

        long long total_demand = 0;
        for (int v = 0; v < n; ++v) total_demand += get_demand(v);
        std::vector<long long> load(k, 0);
        const bool track_count = output_count != nullptr ||
                                 !lower_count.empty() || !upper_count.empty();
        std::vector<int> count;
        if (track_count) count.assign(k, 0);
        for (int v = 0; v < n; ++v) {
            if (label[v] < 0 || label[v] >= k || !is_allowed(v, label[v])) return false;
            load[label[v]] += get_demand(v);
            if (track_count) ++count[label[v]];
        }
        for (int label_id = 0; label_id < k; ++label_id) {
            if (load[label_id] < get_lower(label_id) ||
                load[label_id] > get_upper(label_id, total_demand)) {
                return false;
            }
            if (track_count &&
                (count[label_id] < get_lower_count(label_id) ||
                 count[label_id] > get_upper_count(label_id))) return false;
        }
        if (output_load != nullptr) *output_load = std::move(load);
        if (output_count != nullptr) *output_count = std::move(count);
        return true;
    }
};

template <class Cost>
template <class PairCost>
class capacitated_graph_labeling_problem<Cost>::solver {
private:
    struct fast_random {
        std::uint64_t state;
        bool use_multiply_high = false;

        std::uint64_t next_u64() {
            state += 0x9e3779b97f4a7c15ULL;
            std::uint64_t z = state;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return z ^ (z >> 31);
        }

        int next_int(int bound) {
            assert(bound > 0);
            if (!use_multiply_high) {
                return static_cast<int>(next_u64() % static_cast<std::uint64_t>(bound));
            }
            const std::uint64_t unsigned_bound = static_cast<std::uint64_t>(bound);
            std::uint64_t value = next_u64();
            auto product = static_cast<__uint128_t>(value) * unsigned_bound;
            std::uint64_t low = static_cast<std::uint64_t>(product);

            // Lemire法。通常は乗算1回で終わり、剰余除算より高速に一様整数を得る。
            if (low < unsigned_bound) {
                const std::uint64_t threshold = -unsigned_bound % unsigned_bound;
                while (low < threshold) {
                    value = next_u64();
                    product = static_cast<__uint128_t>(value) * unsigned_bound;
                    low = static_cast<std::uint64_t>(product);
                }
            }
            return static_cast<int>(product >> 64);
        }

    };

    inline static constexpr bool is_potts_policy = [] {
        if constexpr (requires { PairCost::is_potts; }) {
            return static_cast<bool>(PairCost::is_potts);
        } else {
            return false;
        }
    }();

#if __INCLUDE_LEVEL__ == 0
    friend int main();
#endif
    using problem_type = capacitated_graph_labeling_problem<Cost>;
    using edge_type = capacitated_graph_labeling_edge<Cost>;
    using clock_type = std::chrono::steady_clock;

    struct arc {
        int other;
        int edge_id;
    };

    enum class move_type : unsigned char { none, recolor, swap };

    struct move {
        move_type type = move_type::none;
        int u = -1;
        int v = -1;
        int target_label = -1;
        Cost delta{};
    };

    const problem_type& problem;
    const PairCost& pair_cost;
    const capacitated_graph_labeling_options& options;
    const int n;
    const int k;
    const int m;
    long long total_demand = 0;
    fast_random random;
    clock_type::time_point start_time;
    clock_type::time_point end_time = clock_type::time_point::max();
    double time_budget_ms = -1.0;

    std::vector<int> offset;
    std::vector<arc> arcs;
    std::vector<int> movable_vertices;
    std::vector<int> movable_index;
    std::vector<int> active_edge_ids;
    std::vector<int> label;
    std::vector<long long> load;
    std::vector<int> label_vertex_count;
    std::vector<std::vector<int>> vertices_by_label;
    std::vector<int> position_in_label;
    std::vector<Cost> potts_neighbor_sum;
    std::vector<std::uint64_t> potts_neighbor_bloom;
    Cost objective{};
    bool uses_potts_cache = false;

    std::vector<int> best_label;
    std::vector<long long> best_load;
    std::vector<int> best_count;
    std::vector<unsigned char> dirty_since_best;
    std::vector<int> vertices_dirty_since_best;
    Cost best_objective{};
    Cost initial_objective{};
    long long iterations = 0;
    bool repair_scope_valid = true;
    bool all_vertices_movable = false;
    bool uses_count_bounds = false;

    bool time_expired() const {
        return end_time != clock_type::time_point::max() && clock_type::now() >= end_time;
    }

    double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(clock_type::now() - start_time).count();
    }

    bool fixed_vertex(int v) const {
        return !problem.fixed_label.empty() && problem.fixed_label[v] >= 0;
    }

    bool movable_vertex(int v) const {
        return all_vertices_movable || movable_index[v] >= 0;
    }

    int active_index_of(int v) const { return all_vertices_movable ? v : movable_index[v]; }

    int movable_count() const {
        return all_vertices_movable ? n : static_cast<int>(movable_vertices.size());
    }

    // Swap=false: targetは移動先ラベル。Swap=true: targetは交換相手の頂点。
    // swapの相手を変更済みとみなし、v側の寄与をprefixへ同じ順序で加算する。
    template <bool Swap = false>
    Cost vertex_delta_generic(int v, int target, Cost prefix_delta = Cost()) const {
        const int* const labels = label.data();
        const int* const offsets = offset.data();
        const arc* const incident = arcs.data();
        const edge_type* const edges = problem.edges.data();
        const int old_label = labels[v];
        const int target_label = Swap ? labels[target] : target;
        Cost delta = Swap
                         ? prefix_delta + problem.get_unary(v, target_label) - problem.get_unary(v, old_label)
                         : problem.get_unary(v, target_label) - problem.get_unary(v, old_label);
        for (int arc_index = offsets[v]; arc_index < offsets[v + 1]; ++arc_index) {
            const arc current_arc = incident[arc_index];
            const edge_type& edge = edges[current_arc.edge_id];
            if (edge.u == edge.v) {
                delta += pair_cost(current_arc.edge_id, edge, target_label, target_label) -
                         pair_cost(current_arc.edge_id, edge, old_label, old_label);
                continue;
            }
            const int neighbor = current_arc.other;
            const int neighbor_label = Swap && neighbor == target ? old_label : labels[neighbor];
            const bool from_u = edge.u == v;
            const int old_u = from_u ? old_label : neighbor_label;
            const int old_v = from_u ? neighbor_label : old_label;
            const int new_u = from_u ? target_label : neighbor_label;
            const int new_v = from_u ? neighbor_label : target_label;
            delta += pair_cost(current_arc.edge_id, edge, new_u, new_v) -
                     pair_cost(current_arc.edge_id, edge, old_u, old_v);
        }
        return delta;
    }

    int random_target_label(int v) {
        if (k <= 1) return label[v];
        if constexpr (is_potts_policy) {
            if (offset[v] < offset[v + 1] && (random.next_u64() & 7U) != 0) {
                const int arc_index = offset[v] + random.next_int(offset[v + 1] - offset[v]);
                const arc selected_arc = arcs[arc_index];
                const edge_type& edge = problem.edges[selected_arc.edge_id];
                if (edge.weight < Cost{}) {
                    int target = random.next_int(k - 1);
                    const int avoided = label[selected_arc.other];
                    if (target >= avoided) ++target;
                    return target;
                }
                return label[selected_arc.other];
            }
        }
        int target = random.next_int(k - 1);
        if (target >= label[v]) ++target;
        return target;
    }

    move propose_move() {
        const auto choose_vertex = [this]() -> int {
            const int active_edge_count = all_vertices_movable
                                            ? m
                                            : static_cast<int>(active_edge_ids.size());
            if (active_edge_count > 0 && (random.next_u64() >> 63) == 0) {
                const int selected = random.next_int(active_edge_count);
                const edge_type& edge = problem.edges[
                    all_vertices_movable ? selected : active_edge_ids[selected]];
                const bool u_movable = movable_vertex(edge.u);
                const bool v_movable = movable_vertex(edge.v);
                if (u_movable && v_movable) {
                    return random.next_int(2) == 0 ? edge.u : edge.v;
                }
                return u_movable ? edge.u : edge.v;
            }
            if (all_vertices_movable) return random.next_int(n);
            return movable_vertices[random.next_int(static_cast<int>(movable_vertices.size()))];
        };

        const auto propose_swap = [this](int u) -> move {
            const auto swap_delta = [this](int swap_u, int swap_v, Cost u_delta) -> Cost {
                if constexpr (is_potts_policy) {
                    if (uses_potts_cache) {
                        const int label_u = label[swap_u];
                        const int label_v = label[swap_v];
                        const std::size_t base_u = static_cast<std::size_t>(active_index_of(swap_u)) * k;
                        const std::size_t base_v = static_cast<std::size_t>(active_index_of(swap_v)) * k;
                        Cost delta = problem.get_unary(swap_u, label_v) - problem.get_unary(swap_u, label_u) +
                                     problem.get_unary(swap_v, label_u) - problem.get_unary(swap_v, label_v) +
                                     potts_neighbor_sum[base_u + label_u] -
                                     potts_neighbor_sum[base_u + label_v] +
                                     potts_neighbor_sum[base_v + label_v] -
                                     potts_neighbor_sum[base_v + label_u];
                        Cost between{};
                        const auto bit = static_cast<unsigned>(swap_v) & 63U;
                        bool may_be_adjacent = potts_neighbor_bloom.empty() ||
                            (potts_neighbor_bloom[active_index_of(swap_u)] &
                             (std::uint64_t{1} << bit)) != 0;
                        if (may_be_adjacent) {
                            // 無向の補正和なので、次数が小さい側を走査する
                            if (offset[swap_u + 1] - offset[swap_u] > offset[swap_v + 1] - offset[swap_v]) std::swap(swap_u, swap_v);
                            for (int arc_index = offset[swap_u]; arc_index < offset[swap_u + 1]; ++arc_index) {
                                const arc current_arc = arcs[arc_index];
                                const edge_type& edge = problem.edges[current_arc.edge_id];
                                if (edge.u != edge.v && current_arc.other == swap_v) between += edge.weight;
                            }
                        }
                        delta += between + between;
                        return delta;
                    }
                }
                return vertex_delta_generic<true>(swap_v, swap_u, u_delta);
            };

            move best;
            const int target_label = random_target_label(u);
            if (target_label == label[u] || vertices_by_label[target_label].empty()) return best;

            const bool source_allowed = problem.is_allowed(u, target_label);
            const int label_u = label[u];
            const long long demand_u = problem.get_demand(u);
            const long long source_load = load[label_u] - demand_u;
            const long long source_lower = problem.get_lower(label_u);
            const long long source_upper = problem.get_upper(label_u, total_demand);
            const auto& candidates = vertices_by_label[target_label];
            const int trials = std::min(static_cast<int>(candidates.size()), 8);
            Cost u_delta{};
            bool have_u_delta = false;
            for (int trial = 0; trial < trials; ++trial) {
                const int v = candidates[random.next_int(static_cast<int>(candidates.size()))];
                // ラベル別リストから選んだ候補なので、移動先ラベルは既知
                const int label_v = target_label;
                // u側の許可条件は提案ごとに確認済み
                if (!source_allowed || !problem.is_allowed(v, label_u)) continue;
                const long long demand_v = problem.get_demand(v);
                const long long new_load_u = source_load + demand_v;
                const long long new_load_v = load[label_v] - demand_v + demand_u;
                const bool capacity_ok = new_load_u >= source_lower && new_load_u <= source_upper &&
                       new_load_v >= problem.get_lower(label_v) && new_load_v <= problem.get_upper(label_v, total_demand);
                if (!capacity_ok) continue;
                if (!uses_potts_cache && !have_u_delta) {
                    u_delta = vertex_delta_generic(u, target_label);
                    have_u_delta = true;
                }
                const Cost delta = swap_delta(u, v, u_delta);
                if (best.type == move_type::none || delta < best.delta) {
                    best = {move_type::swap, u, v, target_label, delta};
                }
            }
            return best;
        };

        const auto propose_recolor = [this](int u) -> move {
            const auto recolor_delta = [this](int v, int target) -> Cost {
                if constexpr (is_potts_policy) {
                    if (uses_potts_cache) {
                        const int old_label = label[v];
                        const std::size_t base = static_cast<std::size_t>(active_index_of(v)) * k;
                        return problem.get_unary(v, target) - problem.get_unary(v, old_label) +
                               potts_neighbor_sum[base + old_label] -
                               potts_neighbor_sum[base + target];
                    }
                }
                return vertex_delta_generic(v, target);
            };

            move best;
            // 移動元だけで実行不能と分かる場合、移動先の提案は不要
            const int source = label[u];
            const long long demand = problem.get_demand(u);
            if (load[source] - demand < problem.get_lower(source) ||
                (uses_count_bounds && label_vertex_count[source] - 1 < problem.get_lower_count(source))) return best;
            const int trials = std::min(k - 1, 4);
            for (int trial = 0; trial < trials; ++trial) {
                const int target = random_target_label(u);
                if (target == source || !problem.is_allowed(u, target) ||
                    load[target] + demand > problem.get_upper(target, total_demand) ||
                    (uses_count_bounds && label_vertex_count[target] + 1 > problem.get_upper_count(target))) continue;
                const Cost delta = recolor_delta(u, target);
                if (best.type == move_type::none || delta < best.delta) {
                    best = {move_type::recolor, u, -1, target, delta};
                }
            }
            return best;
        };

        const int u = choose_vertex();
        // next_double() < 0.45と同じ上位53-bit境界を整数で判定する
        if ((random.next_u64() >> 11) < 0xe666666666667ULL) {
            move candidate = propose_swap(u);
            if (candidate.type != move_type::none) return candidate;
            return propose_recolor(u);
        }
        move candidate = propose_recolor(u);
        if (candidate.type != move_type::none) return candidate;
        return propose_swap(u);
    }

public:
    // 隣接情報・可変頂点範囲・終了時刻を初期化する。O(N + M + K)
    solver(const problem_type& problem_, const PairCost& pair_cost_,
           const capacitated_graph_labeling_options& options_,
           const std::vector<int>* requested_vertices = nullptr)
        : problem(problem_), pair_cost(pair_cost_), options(options_),
          n(problem.vertex_count), k(problem.label_count),
          m(static_cast<int>(problem.edges.size())),
          random{options.seed},
          start_time(clock_type::now()) {
        const auto build_movable_scope = [&]() -> void {
            if (requested_vertices == nullptr && problem.fixed_label.empty()) {
                all_vertices_movable = true;
                return;
            }
            movable_index.assign(n, -1);
            movable_vertices.reserve(requested_vertices == nullptr
                                         ? n
                                         : static_cast<int>(requested_vertices->size()));
            if (requested_vertices == nullptr) {
                for (int v = 0; v < n; ++v) {
                    if (fixed_vertex(v)) continue;
                    movable_index[v] = static_cast<int>(movable_vertices.size());
                    movable_vertices.push_back(v);
                }
            } else {
                for (const int v : *requested_vertices) {
                    if (v < 0 || v >= n || fixed_vertex(v) || movable_index[v] >= 0) {
                        repair_scope_valid = false;
                        return;
                    }
                    movable_index[v] = static_cast<int>(movable_vertices.size());
                    movable_vertices.push_back(v);
                }
            }

            all_vertices_movable = static_cast<int>(movable_vertices.size()) == n;
            if (all_vertices_movable) {
                movable_vertices.clear();
                movable_index.clear();
                return;
            }

            active_edge_ids.reserve(problem.edges.size());
            for (int edge_id = 0; edge_id < m; ++edge_id) {
                const edge_type& edge = problem.edges[edge_id];
                if (movable_vertex(edge.u) || movable_vertex(edge.v)) {
                    active_edge_ids.push_back(edge_id);
                }
            }
        };

        problem.validate();
        {
            end_time = options.deadline;
            if (options.time_limit_ms >= 0.0) {
                // 整数tickへ変換する前に上限を確認し、時計に収まらない相対期限は飽和させる
                const auto relative_duration = std::chrono::duration<long double, std::milli>(
                    options.time_limit_ms);
                if (relative_duration < clock_type::time_point::max() - start_time) {
                    const auto relative_deadline =
                        start_time + std::chrono::duration_cast<clock_type::duration>(relative_duration);
                    end_time = std::min(end_time, relative_deadline);
                }
            }
            if (end_time != clock_type::time_point::max()) {
                // 過去の期限は差を取らず0とし、time_point::min()でも減算をあふれさせない
                time_budget_ms = end_time <= start_time ? 0.0 :
                    std::chrono::duration<double, std::milli>(end_time - start_time).count();
            }
        }
        uses_count_bounds = !problem.lower_count.empty() || !problem.upper_count.empty();
        for (int v = 0; v < n; ++v) total_demand += problem.get_demand(v);
        {
            // 自己辺は1本、それ以外は両端に1本ずつarcを置く
            offset.assign(n + 1, 0);
            for (const auto& edge : problem.edges) {
                ++offset[edge.u + 1];
                if (edge.v != edge.u) ++offset[edge.v + 1];
            }
            for (int v = 0; v < n; ++v) offset[v + 1] += offset[v];

            arcs.resize(offset[n]);
            std::vector<int> cursor = offset;
            for (int edge_id = 0; edge_id < m; ++edge_id) {
                const auto& edge = problem.edges[edge_id];
                arcs[cursor[edge.u]++] = {edge.v, edge_id};
                if (edge.v != edge.u) arcs[cursor[edge.v]++] = {edge.u, edge_id};
            }
        }
        build_movable_scope();
    }

    // 初期化・探索後に最良解を返す。O(R(N log N + NK + KM + K) + T(D + K))、R=max(1, 構築回数)、T=反復数、D=最大次数
    void run(const std::vector<int>* initial,
             capacitated_graph_labeling_result<Cost>* full_result,
             capacitated_graph_labeling_repair_result<Cost>* repair_result) {
        const auto search = [&]() -> void {
            const auto build_potts_cache = [&]() -> void {
                if constexpr (!is_potts_policy) return;
                const std::size_t active_count = static_cast<std::size_t>(movable_count());
                const std::size_t cells = active_count * k;
                if (k != 0 && cells / k != active_count) return;
                if (cells > options.potts_cache_max_bytes / sizeof(Cost)) return;

                // S[active_index(v)][label] = labelを持つ隣接頂点との辺重み和
                potts_neighbor_sum.assign(cells, Cost{});
                const auto add_neighbor_sum = [&](const edge_type& edge) {
                    if (edge.u == edge.v) return;
                    if (movable_vertex(edge.u)) {
                        const std::size_t base = static_cast<std::size_t>(active_index_of(edge.u)) * k;
                        potts_neighbor_sum[base + label[edge.v]] += edge.weight;
                    }
                    if (movable_vertex(edge.v)) {
                        const std::size_t base = static_cast<std::size_t>(active_index_of(edge.v)) * k;
                        potts_neighbor_sum[base + label[edge.u]] += edge.weight;
                    }
                };
                if (all_vertices_movable) {
                    for (const edge_type& edge : problem.edges) add_neighbor_sum(edge);
                } else {
                    for (const int edge_id : active_edge_ids) {
                        add_neighbor_sum(problem.edges[edge_id]);
                    }
                }

                // swap候補が非隣接ならu-v間補正の隣接走査を省く。偽陽性だけを許すため差分は厳密。
                const std::size_t cache_bytes = cells * sizeof(Cost);
                const std::size_t bloom_bytes = active_count * sizeof(std::uint64_t);
                if (bloom_bytes <= options.potts_cache_max_bytes - cache_bytes) {
                    potts_neighbor_bloom.assign(active_count, 0);
                    const auto add_bloom_edge = [&](const edge_type& edge) {
                        if (edge.u == edge.v) return;
                        if (movable_vertex(edge.u)) {
                            potts_neighbor_bloom[active_index_of(edge.u)] |=
                                std::uint64_t{1} << (static_cast<unsigned>(edge.v) & 63U);
                        }
                        if (movable_vertex(edge.v)) {
                            potts_neighbor_bloom[active_index_of(edge.v)] |=
                                std::uint64_t{1} << (static_cast<unsigned>(edge.u) & 63U);
                        }
                    };
                    if (all_vertices_movable) {
                        for (const edge_type& edge : problem.edges) add_bloom_edge(edge);
                    } else {
                        for (const int edge_id : active_edge_ids) {
                            add_bloom_edge(problem.edges[edge_id]);
                        }
                    }
                }
                uses_potts_cache = true;
            };

            const auto calibrate_temperature = [&]() -> std::pair<double, double> {
                std::vector<double> magnitudes;
                magnitudes.reserve(128);
                for (int sample = 0; sample < 256 && static_cast<int>(magnitudes.size()) < 128; ++sample) {
                    const move candidate = propose_move();
                    if (candidate.type == move_type::none || candidate.delta == Cost{}) continue;
                    magnitudes.push_back(std::abs(static_cast<double>(candidate.delta)));
                }
                double scale = 1.0;
                if (!magnitudes.empty()) {
                    const std::size_t middle = magnitudes.size() / 2;
                    std::nth_element(magnitudes.begin(), magnitudes.begin() +
                                     static_cast<std::ptrdiff_t>(middle), magnitudes.end());
                    scale = std::max(1e-12, magnitudes[middle]);
                }
                const double start_temperature = options.initial_temperature >= 0.0
                                                     ? options.initial_temperature
                                                     : scale * 0.5;
                const double final_temperature = options.final_temperature >= 0.0
                                                     ? options.final_temperature
                                                     : scale * 0.03;
                return {std::max(1e-12, start_temperature), std::max(1e-12, final_temperature)};
            };

            const auto update_cache_for_recolor = [&](int v, int old_label, int new_label) -> void {
                if (!uses_potts_cache) return;
                for (int arc_index = offset[v]; arc_index < offset[v + 1]; ++arc_index) {
                    const arc current_arc = arcs[arc_index];
                    const edge_type& edge = problem.edges[current_arc.edge_id];
                    if (edge.u == edge.v || !movable_vertex(current_arc.other)) continue;
                    const std::size_t base =
                        static_cast<std::size_t>(active_index_of(current_arc.other)) * k;
                    potts_neighbor_sum[base + old_label] -= edge.weight;
                    potts_neighbor_sum[base + new_label] += edge.weight;
                }
            };

            const auto apply_move = [&](const move& selected_move) -> void {
                if (selected_move.type == move_type::recolor) {
                    const int v = selected_move.u;
                    const int old_label = label[v];
                    const int new_label = selected_move.target_label;
                    update_cache_for_recolor(v, old_label, new_label);

                    // 元ラベルの末尾要素で穴を埋め、新ラベルの末尾へ頂点を追加する
                    auto& old_vertices = vertices_by_label[old_label];
                    const int old_position = position_in_label[v];
                    const int moved_vertex = old_vertices.back();
                    old_vertices[old_position] = moved_vertex;
                    position_in_label[moved_vertex] = old_position;
                    old_vertices.pop_back();
                    position_in_label[v] = static_cast<int>(vertices_by_label[new_label].size());
                    vertices_by_label[new_label].push_back(v);

                    const long long vertex_demand = problem.get_demand(v);
                    load[old_label] -= vertex_demand;
                    load[new_label] += vertex_demand;
                    if (uses_count_bounds) {
                        --label_vertex_count[old_label];
                        ++label_vertex_count[new_label];
                    }
                    label[v] = new_label;
                    objective += selected_move.delta;
                } else {
                    const int u = selected_move.u;
                    const int v = selected_move.v;
                    const int label_u = label[u];
                    const int label_v = label[v];
                    update_cache_for_recolor(u, label_u, label_v);
                    update_cache_for_recolor(v, label_v, label_u);

                    const int position_u = position_in_label[u];
                    const int position_v = position_in_label[v];
                    vertices_by_label[label_u][position_u] = v;
                    vertices_by_label[label_v][position_v] = u;
                    position_in_label[v] = position_u;
                    position_in_label[u] = position_v;

                    const long long demand_u = problem.get_demand(u);
                    const long long demand_v = problem.get_demand(v);
                    load[label_u] += demand_v - demand_u;
                    load[label_v] += demand_u - demand_v;
                    label[u] = label_v;
                    label[v] = label_u;
                    objective += selected_move.delta;
                }

                const auto mark_dirty = [&](int v) {
                    const int active_index = active_index_of(v);
                    if (dirty_since_best[active_index] != 0) return;
                    dirty_since_best[active_index] = 1;
                    vertices_dirty_since_best.push_back(v);
                };
                mark_dirty(selected_move.u);
                if (selected_move.type == move_type::swap) mark_dirty(selected_move.v);
            };

            if (movable_count() == 0 || k <= 1 ||
                options.iteration_limit == 0 || time_expired()) return;
            {
                // 変更可能なswap候補だけをラベルからO(1)で抽出できる形にする
                vertices_by_label.assign(k, {});
                position_in_label.assign(n, -1);
                if (all_vertices_movable) {
                    for (int v = 0; v < n; ++v) {
                        position_in_label[v] = static_cast<int>(vertices_by_label[label[v]].size());
                        vertices_by_label[label[v]].push_back(v);
                    }
                } else {
                    for (const int v : movable_vertices) {
                        position_in_label[v] = static_cast<int>(vertices_by_label[label[v]].size());
                        vertices_by_label[label[v]].push_back(v);
                    }
                }
            }
            build_potts_cache();
            dirty_since_best.assign(movable_count(), 0);
            vertices_dirty_since_best.clear();
            vertices_dirty_since_best.reserve(movable_count());
            random.use_multiply_high = uses_potts_cache;
            const auto [start_temperature, final_temperature] = calibrate_temperature();
            const double temperature_ratio = final_temperature / start_temperature;
            int update_interval = 1024;
            if (options.iteration_limit > 0) {
                const long long adaptive = std::max(1LL, options.iteration_limit / 256);
                update_interval = static_cast<int>(std::min<long long>(update_interval, adaptive));
            }
            int iterations_until_update = 0;
            double temperature = start_temperature;

            // 温度と時刻はまとめて間欠更新し、正差分はMetropolis則で受理する
            while (options.iteration_limit < 0 || iterations < options.iteration_limit) {
                if (iterations_until_update == 0) {
                    double elapsed = 0.0;
                    if (time_budget_ms >= 0.0) {
                        const auto now = clock_type::now();
                        if (now >= end_time) break;
                        elapsed = std::chrono::duration<double, std::milli>(now - start_time).count();
                    }
                    double progress = 0.0;
                    if (options.iteration_limit > 0) {
                        progress = static_cast<double>(iterations) /
                                   static_cast<double>(options.iteration_limit);
                    }
                    if (time_budget_ms > 0.0) {
                        progress = std::max(progress, elapsed / time_budget_ms);
                    }
                    progress = std::clamp(progress, 0.0, 1.0);
                    temperature = start_temperature * std::pow(temperature_ratio, progress);
                    iterations_until_update = update_interval;
                }
                --iterations_until_update;
                const move candidate = propose_move();
                ++iterations;
                if (candidate.type == move_type::none) continue;

                bool accept = candidate.delta <= Cost{};
                if (!accept) {
                    const double probability = std::exp(
                        -static_cast<double>(candidate.delta) / temperature);
                    accept = (static_cast<double>(random.next_u64() >> 11) * 0x1.0p-53) < probability;
                }
                if (!accept) continue;

                apply_move(candidate);
                if (objective < best_objective) {
                    best_objective = objective;
                    for (const int v : vertices_dirty_since_best) {
                        best_label[v] = label[v];
                        dirty_since_best[active_index_of(v)] = 0;
                    }
                    vertices_dirty_since_best.clear();
                    best_load = load;
                    if (uses_count_bounds) best_count = label_vertex_count;
                }
            }
        };

        const auto load_initial = [&](const std::vector<int>& initial_label, bool active_objective_only) -> bool {
            const auto evaluate_active_objective = [&]() -> Cost {
                Cost result{};
                if (all_vertices_movable) return problem.evaluate(label, pair_cost);
                {
                    for (const int v : movable_vertices) result += problem.get_unary(v, label[v]);
                    for (const int edge_id : active_edge_ids) {
                        const edge_type& edge = problem.edges[edge_id];
                        result += pair_cost(edge_id, edge, label[edge.u], label[edge.v]);
                    }
                }
                return result;
            };

            if (!problem.feasible(initial_label, &load,
                          uses_count_bounds ? &label_vertex_count : nullptr)) return false;
            label = initial_label;
            objective = active_objective_only
                            ? evaluate_active_objective()
                            : problem.evaluate(label, pair_cost);
            best_label = label;
            best_load = load;
            if (uses_count_bounds) best_count = label_vertex_count;
            best_objective = objective;
            initial_objective = objective;
            return true;
        };

        const auto construct_initial = [&]() -> bool {
            std::vector<int> allowed_label_counts;
            const auto construct_once = [&](int trial, std::vector<int>& output_label, Cost& output_objective) -> bool {
                const auto make_bfs_rank = [&]() -> std::vector<int> {
                    // 連結成分ごとにランダムな根からBFSし、近い頂点が連続する構築順を作る
                    std::vector<int> rank(n, -1);
                    std::vector<int> queue;
                    queue.reserve(n);
                    int rank_value = 0;
                    int first_unused = 0;

                    for (int component = 0; component < n; ++component) {
                        int root = random.next_int(n);
                        if (rank[root] >= 0) {
                            // 既訪問の接頭辞を再走査せず、全成分を通じてO(N)で根を探す
                            while (first_unused < n && rank[first_unused] >= 0) ++first_unused;
                            if (first_unused == n) break;
                            root = first_unused;
                        }
                        rank[root] = rank_value++;
                        queue.clear();
                        queue.push_back(root);
                        std::size_t head = 0;
                        while (head < queue.size()) {
                            const int v = queue[head++];
                            for (int arc_index = offset[v]; arc_index < offset[v + 1]; ++arc_index) {
                                const int to = arcs[arc_index].other;
                                if (rank[to] < 0) {
                                    rank[to] = rank_value++;
                                    queue.push_back(to);
                                }
                            }
                        }
                    }
                    return rank;
                };

                const auto allowed_count = [&](int v) -> int {
                    if (fixed_vertex(v)) return 1;
                    if (problem.allowed.empty()) return k;
                    return allowed_label_counts[v];
                };

                // 固定頂点を先に割り当て、残り容量を確定する
                label.assign(n, -1);
                load.assign(k, 0);
                if (uses_count_bounds) label_vertex_count.assign(k, 0);

                for (int v = 0; v < n; ++v) {
                    if (!fixed_vertex(v)) continue;
                    const int target = problem.fixed_label[v];
                    if (load[target] + problem.get_demand(v) > problem.get_upper(target, total_demand) ||
                        (uses_count_bounds &&
                         label_vertex_count[target] + 1 > problem.get_upper_count(target))) return false;
                    label[v] = target;
                    load[target] += problem.get_demand(v);
                    if (uses_count_bounds) ++label_vertex_count[target];
                }

                std::vector<int> order;
                order.reserve(n);
                for (int v = 0; v < n; ++v) {
                    if (!fixed_vertex(v)) order.push_back(v);
                }
                std::vector<std::uint64_t> tie_key(n);
                for (int v = 0; v < n; ++v) tie_key[v] = random.next_u64();

                std::vector<int> bfs_rank;
                if (trial % 4 == 1 && !order.empty()) bfs_rank = make_bfs_rank();
                std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
                    const int lhs_allowed = allowed_count(lhs), rhs_allowed = allowed_count(rhs);
                    if (lhs_allowed != rhs_allowed) return lhs_allowed < rhs_allowed;
                    if (problem.get_demand(lhs) != problem.get_demand(rhs)) return problem.get_demand(lhs) > problem.get_demand(rhs);
                    if (!bfs_rank.empty()) return bfs_rank[lhs] < bfs_rank[rhs];
                    if (trial % 4 == 0) {
                        const int lhs_degree = offset[lhs + 1] - offset[lhs];
                        const int rhs_degree = offset[rhs + 1] - offset[rhs];
                        if (lhs_degree != rhs_degree) return lhs_degree > rhs_degree;
                    }
                    return tie_key[lhs] < tie_key[rhs];
                });

                long long remaining_demand = 0;
                for (const int v : order) remaining_demand += problem.get_demand(v);
                int remaining_vertices = static_cast<int>(order.size());
                const bool capacity_priority = trial % 4 == 2;
                std::vector<Cost> assigned_neighbor_sum;
                if constexpr (is_potts_policy) {
                    assigned_neighbor_sum.resize(static_cast<std::size_t>(k));
                }
                for (const int v : order) {
                    // 残需要が容量下限の不足量と同じになったら、不足ラベルだけを候補にする
                    long long total_deficit = 0;
                    for (int label_id = 0; label_id < k; ++label_id) {
                        total_deficit += std::max(0LL, problem.get_lower(label_id) - load[label_id]);
                    }
                    const bool must_fill_deficit = remaining_demand <= total_deficit;
                    int total_count_deficit = 0;
                    if (uses_count_bounds) {
                        for (int label_id = 0; label_id < k; ++label_id) {
                            total_count_deficit +=
                                std::max(0, problem.get_lower_count(label_id) -
                                            label_vertex_count[label_id]);
                        }
                    }
                    const bool must_fill_count_deficit =
                        uses_count_bounds && remaining_vertices <= total_count_deficit;

                    if constexpr (is_potts_policy) {
                        std::fill(assigned_neighbor_sum.begin(), assigned_neighbor_sum.end(), Cost{});
                        for (int arc_index = offset[v]; arc_index < offset[v + 1]; ++arc_index) {
                            const arc current_arc = arcs[arc_index];
                            const edge_type& edge = problem.edges[current_arc.edge_id];
                            if (edge.u == edge.v || label[current_arc.other] < 0) continue;
                            assigned_neighbor_sum[label[current_arc.other]] += edge.weight;
                        }
                    }

                    int best_target = -1;
                    Cost best_cost{};
                    long long best_residual = 0;
                    std::uint64_t best_tie = 0;
                    for (int target = 0; target < k; ++target) {
                        if (!problem.is_allowed(v, target) || load[target] + problem.get_demand(v) > problem.get_upper(target, total_demand)) continue;
                        // 需要0の頂点は不足容量を消費しないため、充足済みラベルにも置ける
                        if (must_fill_deficit && problem.get_demand(v) > 0 &&
                            load[target] >= problem.get_lower(target)) continue;
                        if (uses_count_bounds &&
                            label_vertex_count[target] + 1 > problem.get_upper_count(target)) continue;
                        if (must_fill_count_deficit &&
                            label_vertex_count[target] >= problem.get_lower_count(target)) continue;
                        Cost candidate_cost{};
                        if constexpr (is_potts_policy) {
                            candidate_cost = problem.get_unary(v, target) -
                                             assigned_neighbor_sum[target];
                        } else {
                            candidate_cost = problem.get_unary(v, target);
                            for (int arc_index = offset[v]; arc_index < offset[v + 1]; ++arc_index) {
                                const arc current_arc = arcs[arc_index];
                                const edge_type& edge = problem.edges[current_arc.edge_id];
                                if (edge.u == edge.v) {
                                    candidate_cost += pair_cost(current_arc.edge_id, edge, target, target);
                                } else if (label[current_arc.other] >= 0) {
                                    const int label_u = edge.u == v ? target : label[edge.u];
                                    const int label_v = edge.v == v ? target : label[edge.v];
                                    candidate_cost += pair_cost(current_arc.edge_id, edge, label_u, label_v);
                                }
                            }
                        }
                        const long long candidate_residual =
                            problem.get_upper(target, total_demand) - load[target] - problem.get_demand(v);
                        const std::uint64_t candidate_tie = random.next_u64();
                        // 4回に1回は容量を詰め切りやすいbest-fitを目的値より優先する
                        const bool better_capacity = capacity_priority &&
                            (best_target < 0 || candidate_residual < best_residual);
                        const bool equal_capacity = !capacity_priority ||
                            (best_target >= 0 && candidate_residual == best_residual);
                        if (better_capacity ||
                            (equal_capacity &&
                             (best_target < 0 || candidate_cost < best_cost ||
                              (candidate_cost == best_cost && candidate_tie < best_tie)))) {
                            best_target = target;
                            best_cost = candidate_cost;
                            best_residual = candidate_residual;
                            best_tie = candidate_tie;
                        }
                    }
                    if (best_target < 0) return false;
                    label[v] = best_target;
                    load[best_target] += problem.get_demand(v);
                    if (uses_count_bounds) ++label_vertex_count[best_target];
                    remaining_demand -= problem.get_demand(v);
                    --remaining_vertices;
                }

                if (!problem.feasible(label)) return false;
                output_label = label;
                output_objective = problem.evaluate(label, pair_cost);
                return true;
            };

            // 許可表がある初期構築だけで1回集計する。既存解改善では表を作らない
            if (!problem.allowed.empty()) {
                allowed_label_counts.assign(n, 0);
                for (int v = 0; v < n; ++v) {
                    for (int a = 0; a < k; ++a) allowed_label_counts[v] += problem.allowed[v * k + a] != 0;
                }
            }
            // 時間内で複数のgreedy解を作り、目的値が最小の実行可能解だけを残す
            bool found = false;
            std::vector<int> candidate_label;
            Cost candidate_objective{};
            const int trials = std::max(1, options.initial_trials);
            for (int trial = 0; trial < trials; ++trial) {
                if (trial > 0 && time_expired()) break;
                if (!construct_once(trial, candidate_label, candidate_objective)) continue;
                if (!found || candidate_objective < best_objective) {
                    found = true;
                    best_label = candidate_label;
                    best_objective = candidate_objective;
                }
            }
            if (!found) return false;
            label = best_label;
            objective = best_objective;
            problem.feasible(label, &load,
                     uses_count_bounds ? &label_vertex_count : nullptr);
            initial_objective = objective;
            best_load = load;
            if (uses_count_bounds) best_count = label_vertex_count;
            return true;
        };

        // 呼出し側が必要な出力だけを用意する。探索実装は通常/subsetで共有する。
        const auto fail = [&](capacitated_graph_labeling_status status) {
            const auto set_failure = [&](auto& result) {
                result.elapsed_ms = elapsed_ms();
                result.status = status;
            };
            if (repair_result != nullptr) set_failure(*repair_result);
            else set_failure(*full_result);
        };
        if (!repair_scope_valid) {
            fail(capacitated_graph_labeling_status::invalid_repair_scope);
            return;
        }
        const bool initialized = initial == nullptr
                                   ? construct_initial()
                                   : load_initial(*initial, repair_result != nullptr);
        if (!initialized) {
            fail(initial == nullptr
                     ? capacitated_graph_labeling_status::construction_failed
                     : capacitated_graph_labeling_status::infeasible_initial_solution);
            return;
        }
        search();
        const auto finish = [&](auto& result) {
            result.iterations = iterations;
            result.elapsed_ms = elapsed_ms();
            result.used_potts_cache = uses_potts_cache;
            result.status = capacitated_graph_labeling_status::success;
        };
        if (repair_result != nullptr) {
            auto& result = *repair_result;
            if (best_objective < initial_objective) {
                result.changed_vertices.reserve(static_cast<std::size_t>(movable_count()));
                result.new_labels.reserve(static_cast<std::size_t>(movable_count()));
                const auto append_change = [&](int v) {
                    if (best_label[v] == (*initial)[v]) return;
                    result.changed_vertices.push_back(v);
                    result.new_labels.push_back(best_label[v]);
                };
                if (all_vertices_movable) {
                    for (int v = 0; v < n; ++v) append_change(v);
                } else {
                    for (const int v : movable_vertices) append_change(v);
                }
            }
            result.objective_delta = best_objective - initial_objective;
            finish(result);
        } else {
            auto& result = *full_result;
            result.label = std::move(best_label);
            result.load = std::move(best_load);
            result.count = std::move(best_count);
            result.objective = best_objective;
            result.initial_objective = initial_objective;
            result.feasible = true;
            finish(result);
        }
    }

};

// 指定ラベル列の目的値を全頂点・全辺から再計算する。O(N + M)
template <class Cost, class PairCost>
Cost evaluate_capacitated_graph_labeling(
    const capacitated_graph_labeling_problem<Cost>& problem,
    const std::vector<int>& label, const PairCost& pair_cost) {
    assert(static_cast<int>(label.size()) == problem.vertex_count);
    return problem.evaluate(label, pair_cost);
}

// 指定ラベル列が許可ラベルと全容量制約を満たすか判定する。O(N + M + K)
template <class Cost>
bool is_feasible_capacitated_graph_labeling(
    const capacitated_graph_labeling_problem<Cost>& problem,
    const std::vector<int>& label) {
    problem.validate();
    return problem.feasible(label);
}

// 計算量表記のRは構築回数、Tは探索反復数、Dは最大次数。最良解の負荷・件数保存は1回O(K)
// Potts型の構築は1回O(N log N + NK + M + K)、一般型では辺評価がO(KM)になる
// 容量対応greedyで初期解を構築し、焼きなましで改善する。O(R(N log N + NK + KM + K) + T(D + K))
template <class Cost, class PairCost>
capacitated_graph_labeling_result<Cost> solve_capacitated_graph_labeling(
    const capacitated_graph_labeling_problem<Cost>& problem,
    const PairCost& pair_cost,
    const capacitated_graph_labeling_options& options = {}) {
    typename capacitated_graph_labeling_problem<Cost>::template solver<PairCost> solver(
        problem, pair_cost, options);
    capacitated_graph_labeling_result<Cost> result;
    solver.run(nullptr, &result, nullptr);
    return result;
}

// 与えられた実行可能解を焼きなましで改善し、初期解以下の目的値を返す。O(N + M + K + NK + T(D + K))
template <class Cost, class PairCost>
capacitated_graph_labeling_result<Cost> improve_capacitated_graph_labeling(
    const capacitated_graph_labeling_problem<Cost>& problem,
    const std::vector<int>& initial_label,
    const PairCost& pair_cost,
    const capacitated_graph_labeling_options& options = {}) {
    typename capacitated_graph_labeling_problem<Cost>::template solver<PairCost> solver(
        problem, pair_cost, options);
    capacitated_graph_labeling_result<Cost> result;
    solver.run(&initial_label, &result, nullptr);
    return result;
}

// 範囲外頂点との境界辺と全体容量を考慮し、変更点と目的値差分だけを返す
// 指定頂点集合Sだけを変更して実行可能解を改善する。O(N + M + K + |S|K + T(D + K))
template <class Cost, class PairCost>
capacitated_graph_labeling_repair_result<Cost>
improve_capacitated_graph_labeling_subset(
    const capacitated_graph_labeling_problem<Cost>& problem,
    const std::vector<int>& initial_label,
    const std::vector<int>& mutable_vertices,
    const PairCost& pair_cost,
    const capacitated_graph_labeling_options& options = {}) {
    typename capacitated_graph_labeling_problem<Cost>::template solver<PairCost> solver(
        problem, pair_cost, options, &mutable_vertices);
    capacitated_graph_labeling_repair_result<Cost> result;
    solver.run(&initial_label, nullptr, &result);
    return result;
}

#if __INCLUDE_LEVEL__ == 0

int main() {
    using cost_type = long long;
    using problem_type = capacitated_graph_labeling_problem<cost_type>;
    using random_type = problem_type::solver<capacitated_potts_cost<>>::fast_random;
    static_assert(std::is_aggregate_v<problem_type>);

    const auto require = [&](bool condition, const char* message) -> void {
        if (!condition) {
            std::cerr << "test failed: " << message << '\n';
            std::exit(1);
        }
    };

    const auto brute_force = [&](
        const problem_type& problem, const auto& pair_cost) -> std::pair<cost_type, std::vector<int>> {
        const int n = problem.vertex_count;
        const int k = problem.label_count;
        std::vector<int> label(n, 0);
        std::vector<int> best_label;
        cost_type best = std::numeric_limits<cost_type>::max();

        std::uint64_t cases = 1;
        for (int v = 0; v < n; ++v) cases *= static_cast<std::uint64_t>(k);
        for (std::uint64_t mask = 0; mask < cases; ++mask) {
            std::uint64_t value = mask;
            for (int v = 0; v < n; ++v) {
                label[v] = static_cast<int>(value % static_cast<std::uint64_t>(k));
                value /= static_cast<std::uint64_t>(k);
            }
            if (!is_feasible_capacitated_graph_labeling(problem, label)) continue;
            const cost_type score = evaluate_capacitated_graph_labeling(problem, label, pair_cost);
            if (score < best) {
                best = score;
                best_label = label;
            }
        }
        return {best, best_label};
    };

    const auto test_basic_potts = [&]() -> void {
        problem_type problem;
        problem.vertex_count = 6;
        problem.label_count = 2;
        problem.lower_capacity = {3, 3};
        problem.upper_capacity = {3, 3};
        problem.unary_cost = {
            0, 9, 0, 9, 0, 9,
            9, 0, 9, 0, 9, 0,
        };
        problem.edges = {{0, 1, 4, 0}, {1, 2, 3, 0}, {2, 3, 2, 0},
                         {3, 4, 3, 0}, {4, 5, 4, 0}, {5, 0, 1, 0}};

        capacitated_graph_labeling_options options;
        options.time_limit_ms = -1.0;
        options.iteration_limit = 30000;
        options.seed = 7;
        const capacitated_potts_cost<cost_type> pair_cost;
        const auto result = solve_capacitated_graph_labeling(problem, pair_cost, options);
        require(result.feasible, "basic Potts solution must be feasible");
        require(is_feasible_capacitated_graph_labeling(problem, result.label),
                "basic Potts feasibility mismatch");
        require(result.objective == evaluate_capacitated_graph_labeling(problem, result.label, pair_cost),
                "basic Potts objective mismatch");
        require(result.objective <= result.initial_objective, "solver must retain the initial solution");
    };

    const auto test_matrix_and_constraints = [&]() -> void {
        problem_type problem;
        problem.vertex_count = 5;
        problem.label_count = 3;
        problem.demand = {2, 1, 2, 1, 1};
        problem.lower_capacity = {2, 2, 1};
        problem.upper_capacity = {3, 3, 3};
        problem.fixed_label = {0, -1, -1, 1, -1};
        problem.allowed.assign(15, 1);
        problem.allowed[2 * 3 + 0] = 0;
        problem.allowed[4 * 3 + 2] = 0;
        problem.unary_cost = {
            0, 8, 8, 3, 1, 5, 7, 2, 0, 5, 0, 4, 2, 3, 5,
        };
        problem.edges = {{0, 1, 2, 0}, {1, 2, 1, 0}, {2, 3, 3, 0},
                         {3, 4, 1, 0}, {2, 2, 2, 0}};
        const std::vector<cost_type> matrix = {
            0, 2, 5,
            3, 0, 1,
            4, 2, 1,
        };
        const capacitated_label_matrix_cost<cost_type> pair_cost{3, &matrix};
        const std::vector<int> initial = {0, 0, 2, 1, 1};
        require(is_feasible_capacitated_graph_labeling(problem, initial),
                "matrix initial solution must be feasible");

        capacitated_graph_labeling_options options;
        options.time_limit_ms = -1.0;
        options.iteration_limit = 20000;
        options.seed = 11;
        const auto result = improve_capacitated_graph_labeling(problem, initial, pair_cost, options);
        require(result.feasible, "matrix solution must be feasible");
        require(result.objective == evaluate_capacitated_graph_labeling(problem, result.label, pair_cost),
                "matrix objective mismatch");
        require(result.objective <= evaluate_capacitated_graph_labeling(problem, initial, pair_cost),
                "matrix improve must retain the initial solution");
    };

    const auto test_random_against_bruteforce = [&]() -> void {
        std::mt19937_64 random(1234567);
        const capacitated_potts_cost<cost_type> pair_cost;
        for (int test_case = 0; test_case < 120; ++test_case) {
            problem_type problem;
            problem.vertex_count = 5 + static_cast<int>(random() % 3ULL);
            problem.label_count = 2 + static_cast<int>(random() % 2ULL);
            const int n = problem.vertex_count;
            const int k = problem.label_count;
            problem.lower_capacity.assign(k, n / k);
            problem.upper_capacity.assign(k, (n + k - 1) / k);
            while (std::accumulate(problem.lower_capacity.begin(), problem.lower_capacity.end(), 0LL) > n) {
                --problem.lower_capacity[random() % static_cast<std::uint64_t>(k)];
            }
            problem.unary_cost.resize(n * k);
            for (cost_type& cost : problem.unary_cost) {
                cost = static_cast<cost_type>(static_cast<int>(random() % 13ULL) - 6);
            }
            for (int u = 0; u < n; ++u) {
                for (int v = u; v < n; ++v) {
                    if (random() % 4ULL == 0) {
                        const cost_type weight = static_cast<cost_type>(static_cast<int>(random() % 11ULL) - 5);
                        problem.edges.push_back({u, v, weight, 0});
                    }
                }
            }

            const auto [optimum, optimum_label] = brute_force(problem, pair_cost);
            require(!optimum_label.empty(), "random brute-force instance must be feasible");
            capacitated_graph_labeling_options options;
            options.time_limit_ms = -1.0;
            options.iteration_limit = 5000;
            options.seed = random();
            if (test_case % 2 != 0) {
                options.potts_cache_max_bytes = 0;
            } else if (test_case % 4 == 0) {
                // 差分キャッシュだけが上限内に収まり、隣接フィルタを作らない経路も検査する。
                options.potts_cache_max_bytes =
                    static_cast<std::size_t>(n) * static_cast<std::size_t>(k) * sizeof(cost_type);
            } else if (test_case % 8 == 2) {
                // 差分表と1本の隣接フィルタがちょうど収まる上限を検査する。
                options.potts_cache_max_bytes =
                    static_cast<std::size_t>(n) * static_cast<std::size_t>(k) * sizeof(cost_type) +
                    static_cast<std::size_t>(n) * sizeof(std::uint64_t);
            }
            const auto result = improve_capacitated_graph_labeling(
                problem, optimum_label, pair_cost, options);
            require(result.feasible, "random improve result must be feasible");
            require(result.objective == optimum, "improving an optimum must preserve its value");
            require(result.objective == evaluate_capacitated_graph_labeling(
                                            problem, result.label, pair_cost),
                    "random objective mismatch");
        }
    };

    const auto test_random_matrix_against_bruteforce = [&]() -> void {
        std::mt19937_64 random(7654321);
        for (int test_case = 0; test_case < 60; ++test_case) {
            problem_type problem;
            problem.vertex_count = 5 + static_cast<int>(random() % 2ULL);
            problem.label_count = 2 + static_cast<int>(random() % 2ULL);
            const int n = problem.vertex_count;
            const int k = problem.label_count;
            problem.lower_capacity.assign(k, n / k);
            problem.upper_capacity.assign(k, (n + k - 1) / k);
            problem.unary_cost.resize(n * k);
            for (cost_type& cost : problem.unary_cost) {
                cost = static_cast<cost_type>(static_cast<int>(random() % 9ULL) - 4);
            }
            for (int u = 0; u < n; ++u) {
                for (int v = u; v < n; ++v) {
                    if (random() % 3ULL == 0) {
                        problem.edges.push_back({
                            u, v, 1 + static_cast<cost_type>(random() % 3ULL), 0});
                    }
                }
            }
            std::vector<cost_type> matrix(k * k);
            for (cost_type& cost : matrix) {
                cost = static_cast<cost_type>(static_cast<int>(random() % 9ULL) - 4);
            }
            const capacitated_label_matrix_cost<cost_type> pair_cost{k, &matrix};
            const auto [optimum, optimum_label] = brute_force(problem, pair_cost);
            require(!optimum_label.empty(), "random matrix instance must be feasible");

            capacitated_graph_labeling_options options;
            options.time_limit_ms = -1.0;
            options.iteration_limit = 5000;
            options.seed = random();
            const auto result = improve_capacitated_graph_labeling(
                problem, optimum_label, pair_cost, options);
            require(result.feasible, "random matrix improve result must be feasible");
            require(result.objective == optimum,
                    "improving a matrix optimum must preserve its value");
            require(result.objective == evaluate_capacitated_graph_labeling(
                                            problem, result.label, pair_cost),
                    "random matrix objective mismatch");
        }
    };

    const auto test_weighted_exact_capacity_construction = [&]() -> void {
        std::mt19937_64 random(246813579);
        for (int test_case = 0; test_case < 30; ++test_case) {
            problem_type problem;
            problem.vertex_count = 40;
            problem.label_count = 4;
            problem.demand.resize(40);
            problem.lower_capacity.assign(4, 0);
            problem.upper_capacity.assign(4, 0);
            std::vector<int> hidden(40);
            for (int v = 0; v < 40; ++v) {
                problem.demand[v] = 1 + static_cast<long long>(random() % 7ULL);
                hidden[v] = v % 4;
            }
            std::shuffle(hidden.begin(), hidden.end(), random);
            for (int v = 0; v < 40; ++v) {
                problem.lower_capacity[hidden[v]] += problem.demand[v];
                problem.upper_capacity[hidden[v]] += problem.demand[v];
            }
            problem.unary_cost.resize(160);
            for (cost_type& cost : problem.unary_cost) {
                cost = static_cast<cost_type>(random() % 10ULL);
            }
            for (int v = 1; v < 40; ++v) problem.edges.push_back({v - 1, v, 1, 0});

            capacitated_graph_labeling_options options;
            options.time_limit_ms = -1.0;
            options.iteration_limit = 0;
            options.initial_trials = 8;
            options.seed = random();
            const auto result = solve_capacitated_graph_labeling(
                problem, capacitated_potts_cost<cost_type>{}, options);
            require(result.feasible, "weighted exact-capacity construction must succeed");
            require(is_feasible_capacitated_graph_labeling(problem, result.label),
                    "weighted exact-capacity result must satisfy capacities");
            require(result.objective == evaluate_capacitated_graph_labeling(
                                            problem, result.label,
                                            capacitated_potts_cost<cost_type>{}),
                    "weighted construction objective mismatch");
        }
    };

    const auto test_infeasible = [&]() -> void {
        problem_type problem;
        problem.vertex_count = 3;
        problem.label_count = 2;
        problem.upper_capacity = {1, 1};
        capacitated_graph_labeling_options options;
        options.time_limit_ms = -1.0;
        options.iteration_limit = 0;
        const auto result = solve_capacitated_graph_labeling(
            problem, capacitated_potts_cost<cost_type>{}, options);
        require(!result.feasible, "infeasible instance must be reported");
        require(result.status == capacitated_graph_labeling_status::construction_failed,
                "infeasible construction status mismatch");
    };

    const auto test_subset_repair_and_status = [&]() -> void {
        problem_type problem;
        problem.vertex_count = 4;
        problem.label_count = 2;
        problem.lower_capacity = {2, 2};
        problem.upper_capacity = {2, 2};
        problem.unary_cost = {
            0, 20,
            20, 0,
            100, 0,
            0, 100,
        };
        problem.edges = {
            {0, 2, 3, 0}, {1, 3, 5, 0}, {2, 3, 7, 0},
        };
        const std::vector<int> initial = {0, 1, 0, 1};
        const std::vector<int> mutable_vertices = {2, 3};
        const capacitated_potts_cost<cost_type> pair_cost;
        const cost_type initial_objective =
            evaluate_capacitated_graph_labeling(problem, initial, pair_cost);

        capacitated_graph_labeling_options options;
        options.time_limit_ms = -1.0;
        options.iteration_limit = 20000;
        options.seed = 918273;
        const auto repaired = improve_capacitated_graph_labeling_subset(
            problem, initial, mutable_vertices, pair_cost, options);
        require(repaired.status == capacitated_graph_labeling_status::success,
                "subset repair must succeed");
        require(repaired.changed_vertices.size() == repaired.new_labels.size(),
                "subset repair change arrays must have equal length");

        std::vector<int> updated = initial;
        for (std::size_t i = 0; i < repaired.changed_vertices.size(); ++i) {
            updated[repaired.changed_vertices[i]] = repaired.new_labels[i];
        }
        require(updated[0] == initial[0] && updated[1] == initial[1],
                "subset repair must not change vertices outside the scope");
        require(is_feasible_capacitated_graph_labeling(problem, updated),
                "subset repair result must remain feasible");
        const cost_type updated_objective =
            evaluate_capacitated_graph_labeling(problem, updated, pair_cost);
        require(repaired.objective_delta == updated_objective - initial_objective,
                "subset repair objective delta must include boundary edges exactly");
        require(repaired.objective_delta < 0,
                "subset repair must find the improving swap in the deterministic test");

        const std::vector<int> duplicate_scope = {2, 2};
        const auto invalid_scope = improve_capacitated_graph_labeling_subset(
            problem, initial, duplicate_scope, pair_cost, options);
        require(invalid_scope.status ==
                    capacitated_graph_labeling_status::invalid_repair_scope,
                "duplicate repair vertices must be rejected");

        problem_type fixed_problem = problem;
        fixed_problem.fixed_label = {0, -1, -1, -1};
        const std::vector<int> fixed_scope = {0, 2};
        const auto fixed_scope_result = improve_capacitated_graph_labeling_subset(
            fixed_problem, initial, fixed_scope, pair_cost, options);
        require(fixed_scope_result.status ==
                    capacitated_graph_labeling_status::invalid_repair_scope,
                "permanently fixed repair vertices must be rejected");

        const std::vector<int> infeasible_initial = {0, 0, 0, 1};
        const auto infeasible_repair = improve_capacitated_graph_labeling_subset(
            problem, infeasible_initial, mutable_vertices, pair_cost, options);
        require(infeasible_repair.status ==
                    capacitated_graph_labeling_status::infeasible_initial_solution,
                "infeasible repair initial solution status mismatch");
    };

    const auto test_absolute_deadline = [&]() -> void {
        problem_type problem;
        problem.vertex_count = 4;
        problem.label_count = 2;
        problem.lower_capacity = {2, 2};
        problem.upper_capacity = {2, 2};
        problem.unary_cost = {0, 1, 1, 0, 0, 1, 1, 0};
        const std::vector<int> initial = {0, 1, 0, 1};

        capacitated_graph_labeling_options options;
        options.time_limit_ms = -1.0;
        options.iteration_limit = 100000;
        options.deadline = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
        const auto result = improve_capacitated_graph_labeling(
            problem, initial, capacitated_potts_cost<cost_type>{}, options);
        require(result.status == capacitated_graph_labeling_status::success,
                "expired deadline must still return the valid initial solution");
        require(result.iterations == 0,
                "expired absolute deadline must prevent local-search iterations");
    };

    // 極端な相対時間・過去の絶対期限でも、時計の整数変換と加減算をあふれさせない
    const auto test_time_boundaries = [&]() -> void {
        using clock = std::chrono::steady_clock;
        problem_type p;
        p.vertex_count = 2;
        p.label_count = 2;
        p.unary_cost = {0, 3, 3, 0};
        const std::vector<int> initial = {0, 1};
        const double max_duration_ms =
            std::chrono::duration<double, std::milli>(clock::duration::max()).count();
        const auto past = clock::now() - std::chrono::seconds(1);
        const auto future = clock::now() + std::chrono::hours(1);

        // 巨大な相対時間があっても、明示した絶対期限と反復数上限は有効なまま
        for (const double milliseconds : {-1.0, 0.0, 1000.0,
                                          max_duration_ms * (1.0 - 1e-8),
                                          max_duration_ms, std::numeric_limits<double>::max()}) {
            for (const auto deadline : {clock::time_point::min(), past, future,
                                        clock::time_point::max()}) {
                capacitated_graph_labeling_options o;
                o.time_limit_ms = milliseconds;
                o.deadline = deadline;
                o.iteration_limit = 4;
                const auto r = improve_capacitated_graph_labeling(
                    p, initial, capacitated_potts_cost<>{}, o);
                const bool expired = milliseconds == 0.0 || deadline <= past;
                require(r.feasible && r.status == capacitated_graph_labeling_status::success,
                        "time boundary must preserve a feasible initial solution");
                require(r.iterations == (expired ? 0 : 4), "time boundary iteration limit");
                require(r.objective == 0 && r.label == initial, "time boundary optimum");
            }
        }
    };

    const auto test_subset_repair_with_general_pair_cost = [&]() -> void {
        problem_type problem;
        problem.vertex_count = 4;
        problem.label_count = 2;
        problem.lower_capacity = {2, 2};
        problem.upper_capacity = {2, 2};
        problem.unary_cost = {
            0, 20,
            20, 0,
            50, 0,
            0, 50,
        };
        problem.edges = {
            {0, 2, 3, 0}, {1, 3, 2, 0}, {2, 3, 4, 0}, {2, 2, 5, 0},
        };
        const std::vector<cost_type> matrix = {0, 2, 5, 1};
        const capacitated_label_matrix_cost<cost_type> pair_cost{2, &matrix};
        const std::vector<int> initial = {0, 1, 0, 1};
        const std::vector<int> mutable_vertices = {2, 3};
        const cost_type initial_objective =
            evaluate_capacitated_graph_labeling(problem, initial, pair_cost);

        capacitated_graph_labeling_options options;
        options.time_limit_ms = -1.0;
        options.iteration_limit = 20000;
        options.seed = 271828;
        const auto repaired = improve_capacitated_graph_labeling_subset(
            problem, initial, mutable_vertices, pair_cost, options);
        require(repaired.status == capacitated_graph_labeling_status::success,
                "general-cost subset repair must succeed");
        std::vector<int> updated = initial;
        for (std::size_t i = 0; i < repaired.changed_vertices.size(); ++i) {
            updated[repaired.changed_vertices[i]] = repaired.new_labels[i];
        }
        require(updated[0] == initial[0] && updated[1] == initial[1],
                "general-cost subset repair changed an outside vertex");
        const cost_type updated_objective =
            evaluate_capacitated_graph_labeling(problem, updated, pair_cost);
        require(repaired.objective_delta == updated_objective - initial_objective,
                "general-cost subset objective delta mismatch");
        require(is_feasible_capacitated_graph_labeling(problem, updated),
                "general-cost subset repair must remain feasible");
    };

    const auto test_simultaneous_load_and_count_bounds = [&]() -> void {
        problem_type problem;
        problem.vertex_count = 6;
        problem.label_count = 2;
        problem.demand = {4, 1, 1, 3, 2, 1};
        problem.lower_capacity = {6, 6};
        problem.upper_capacity = {6, 6};
        problem.lower_count = {3, 3};
        problem.upper_count = {3, 3};
        problem.unary_cost = {
            0, 5, 0, 5, 0, 5,
            5, 0, 5, 0, 5, 0,
        };
        const std::vector<int> feasible_label = {0, 0, 0, 1, 1, 1};
        const std::vector<int> count_violating_label = {0, 1, 1, 1, 0, 1};
        require(is_feasible_capacitated_graph_labeling(problem, feasible_label),
                "simultaneous load/count feasible assignment must be accepted");
        require(!is_feasible_capacitated_graph_labeling(problem, count_violating_label),
                "equal-load assignment violating label counts must be rejected");

        capacitated_graph_labeling_options options;
        options.time_limit_ms = -1.0;
        options.iteration_limit = 5000;
        options.seed = 314159;
        const auto result = improve_capacitated_graph_labeling(
            problem, feasible_label, capacitated_potts_cost<cost_type>{}, options);
        require(result.status == capacitated_graph_labeling_status::success,
                "count-bounded improve must succeed");
        require(result.count == std::vector<int>({3, 3}),
                "count-bounded result counts mismatch");
        require(is_feasible_capacitated_graph_labeling(problem, result.label),
                "count-bounded improve must preserve both constraint families");

        options.iteration_limit = 0;
        options.initial_trials = 8;
        const auto constructed = solve_capacitated_graph_labeling(
            problem, capacitated_potts_cost<cost_type>{}, options);
        require(constructed.status == capacitated_graph_labeling_status::success,
                "count-bounded greedy construction must succeed in the planted test");
        require(constructed.count == std::vector<int>({3, 3}),
                "count-bounded construction counts mismatch");
        require(is_feasible_capacitated_graph_labeling(problem, constructed.label),
                "count-bounded construction must satisfy both constraint families");
    };

    const auto test_fast_random_bounds = [&]() -> void {
        random_type random{123456789ULL};
        for (const bool multiply_high : {false, true}) {
            random.use_multiply_high = multiply_high;
            for (const int bound : {1, 2, 3, 17, std::numeric_limits<int>::max()}) {
                for (int sample = 0; sample < 1000; ++sample) {
                    const int value = random.next_int(bound);
                    require(value >= 0 && value < bound, "bounded random must stay in range");
                }
            }
        }

        random_type integer_probability{998244353ULL};
        random_type floating_probability{998244353ULL};
        for (int sample = 0; sample < 10000; ++sample) {
            const bool integer_result =
                (integer_probability.next_u64() >> 11) < 0xe666666666667ULL;
            const bool floating_result = (static_cast<double>(floating_probability.next_u64() >> 11) * 0x1.0p-53) < 0.45;
            require(integer_result == floating_result,
                    "integer 45-percent threshold must match next_double exactly");
        }
    };

    // 非負需要の境界値。需要0を置く時に充足済みラベルを除外しないことを検査する
    const auto test_zero_demand_construction = [&]() -> void {
        for (const auto& demands : {std::vector<long long>{0, 0, 0, 0},
                                    std::vector<long long>{2, 0, 0, 2},
                                    std::vector<long long>{1, 0, 1, 0}}) {
            for (const bool count_bounds : {false, true}) {
                problem_type p;
                p.vertex_count = 4;
                p.label_count = 2;
                p.demand = demands;
                const long long each = std::accumulate(demands.begin(), demands.end(), 0LL) / 2;
                p.lower_capacity = {each, each};
                p.upper_capacity = {each, each};
                if (count_bounds) {
                    p.lower_count = {2, 2};
                    p.upper_count = {2, 2};
                }
                capacitated_graph_labeling_options options;
                options.time_limit_ms = -1;
                options.iteration_limit = 0;
                for (std::uint64_t seed = 0; seed < 30; ++seed) {
                    options.seed = seed;
                    const auto r = solve_capacitated_graph_labeling(p, capacitated_potts_cost<>{}, options);
                    require(r.feasible, "zero-demand construction failed");
                    require(is_feasible_capacitated_graph_labeling(p, r.label), "zero-demand constraints");
                }
            }
        }
    };

    const auto test_extended_random_costs = [&]<class C>() -> void {
        std::mt19937_64 rng(998244353);
        for (int tc = 0; tc < 300; ++tc) {
            capacitated_graph_labeling_problem<C> p;
            // 64-bit Bloomのbit衝突が生じる頂点数でも、通常・部分更新を独立検算する
            const int n = tc >= 240 ? 65 + static_cast<int>(rng() % 64)
                                    : 2 + static_cast<int>(rng() % 6);
            const int k = 1 + static_cast<int>(rng() % 4);
            p.vertex_count = n;
            p.label_count = k;
            std::vector<int> initial(n);
            p.demand.resize(n);
            p.lower_capacity.assign(k, 0);
            p.upper_capacity.assign(k, 0);
            p.lower_count.assign(k, 0);
            p.upper_count.assign(k, 0);
            p.fixed_label.assign(n, -1);
            p.allowed.assign(n * k, 1);
            p.unary_cost.resize(n * k);
            for (int v = 0; v < n; ++v) {
                initial[v] = static_cast<int>(rng() % static_cast<unsigned>(k));
                p.demand[v] = static_cast<long long>(rng() % 5);
                p.upper_capacity[initial[v]] += p.demand[v];
                ++p.upper_count[initial[v]];
                if (rng() % 5 == 0) p.fixed_label[v] = initial[v];
                for (int a = 0; a < k; ++a) {
                    p.allowed[v * k + a] = static_cast<unsigned char>(a == initial[v] || rng() % 3 != 0);
                    p.unary_cost[v * k + a] = static_cast<C>(static_cast<int>(rng() % 23) - 11);
                }
            }
            for (int a = 0; a < k; ++a) {
                p.lower_capacity[a] = std::max(0LL, p.upper_capacity[a] - tc % 4);
                p.upper_capacity[a] += tc % 4;
                p.lower_count[a] = std::max(0, p.upper_count[a] - tc % 3);
                p.upper_count[a] = std::min(n, p.upper_count[a] + tc % 3);
            }
            for (int i = 0; i < n * 4; ++i) {
                C w = static_cast<C>(static_cast<int>(rng() % 17) - 8);
                if constexpr (std::is_floating_point_v<C>) w /= C{7};
                p.edges.push_back({static_cast<int>(rng() % static_cast<unsigned>(n)),
                                   static_cast<int>(rng() % static_cast<unsigned>(n)), w, i % 3});
            }
            std::vector<C> matrix(3 * k * k);
            for (auto& c : matrix) c = static_cast<C>(static_cast<int>(rng() % 13) - 6);
            const auto custom = [&](int id, const auto& e, int a, int b) -> C {
                return e.weight * (matrix[(e.type * k + a) * k + b] +
                                   static_cast<C>(id % 3) * static_cast<C>(a - b));
            };
            const auto check = [&](const auto& policy) {
                // ライブラリの評価・実行可能性関数を使わない参照実装
                const auto reference = [&](const std::vector<int>& labels) {
                    require(static_cast<int>(labels.size()) == n, "extended label size");
                    std::vector<long long> loads(k, 0);
                    std::vector<int> counts(k, 0);
                    long double value = 0;
                    for (int v = 0; v < n; ++v) {
                        const int a = labels[v];
                        require(a >= 0 && a < k, "extended label range");
                        require(p.fixed_label[v] >= 0 ? a == p.fixed_label[v] : p.allowed[v * k + a] != 0,
                                "extended fixed/allowed");
                        loads[a] += p.demand[v];
                        ++counts[a];
                        value += static_cast<long double>(p.unary_cost[v * k + a]);
                    }
                    for (int a = 0; a < k; ++a) {
                        require(loads[a] >= p.lower_capacity[a] && loads[a] <= p.upper_capacity[a],
                                "extended capacity");
                        require(counts[a] >= p.lower_count[a] && counts[a] <= p.upper_count[a],
                                "extended count");
                    }
                    for (int id = 0; id < static_cast<int>(p.edges.size()); ++id) {
                        const auto& e = p.edges[id];
                        value += static_cast<long double>(policy(id, e, labels[e.u], labels[e.v]));
                    }
                    return value;
                };
                const auto near = [&](long double a, long double b) {
                    const long double tolerance = std::is_floating_point_v<C> ?
                                                  1e-8L * std::max(1.0L, std::abs(a)) : 0;
                    return std::abs(a - b) <= tolerance;
                };
                capacitated_graph_labeling_options options;
                options.time_limit_ms = -1;
                options.iteration_limit = 1 + tc * 7;
                options.seed = rng();
                if (tc % 4 < 3) options.potts_cache_max_bytes = tc % 4 == 0 ? 0 :
                    static_cast<std::size_t>(n) * (static_cast<std::size_t>(k) * sizeof(C) +
                                                   (tc % 4 == 2 ? 8U : 0U));
                const auto full = improve_capacitated_graph_labeling(p, initial, policy, options);
                require(full.feasible, "extended warm start status");
                const auto full_exact = reference(full.label);
                require(near(full_exact, static_cast<long double>(full.objective)), "extended full objective");
                require(full_exact <= reference(initial) + 1e-6L, "extended full nonworsening");
                require(full.iterations <= options.iteration_limit, "extended iteration bound");
                std::vector<int> scope;
                for (int v = 0; v < n; ++v) if (p.fixed_label[v] < 0 && rng() % 2 != 0) scope.push_back(v);
                const auto repair = improve_capacitated_graph_labeling_subset(p, initial, scope, policy, options);
                require(repair.status == capacitated_graph_labeling_status::success, "extended subset status");
                std::vector<int> updated = initial;
                std::vector<unsigned char> seen(n, 0);
                require(repair.changed_vertices.size() == repair.new_labels.size(), "extended changes size");
                for (std::size_t i = 0; i < repair.changed_vertices.size(); ++i) {
                    const int v = repair.changed_vertices[i];
                    require(std::find(scope.begin(), scope.end(), v) != scope.end(), "extended outside scope");
                    require(seen[v] == 0, "extended duplicate change");
                    seen[v] = 1;
                    updated[v] = repair.new_labels[i];
                }
                const auto change = reference(updated) - reference(initial);
                require(near(change, static_cast<long double>(repair.objective_delta)), "extended subset delta");
                require(change <= 1e-6L, "extended subset nonworsening");
            };
            check(capacitated_potts_cost<C>{});
            check(capacitated_label_matrix_cost<C>{k, &matrix});
            check(custom);
        }
    };

    const auto test_empty_and_invalid_scopes = [&]() -> void {
        problem_type p;
        p.label_count = 2;
        capacitated_graph_labeling_options options;
        options.time_limit_ms = -1;
        options.iteration_limit = 10;
        const capacitated_potts_cost<> cost;
        const auto empty = solve_capacitated_graph_labeling(p, cost, options);
        require(empty.feasible && empty.label.empty() && empty.objective == 0, "empty graph");
        p.vertex_count = 2;
        const std::vector<int> labels = {0, 1};
        for (const auto& scope : {std::vector<int>{-1}, std::vector<int>{2}, std::vector<int>{0, 0}}) {
            const auto r = improve_capacitated_graph_labeling_subset(p, labels, scope, cost, options);
            require(r.status == capacitated_graph_labeling_status::invalid_repair_scope, "invalid scope rejected");
        }
        const auto r = improve_capacitated_graph_labeling_subset(p, labels, {}, cost, options);
        require(r.status == capacitated_graph_labeling_status::success && r.iterations == 0 &&
                r.changed_vertices.empty() && r.objective_delta == 0, "empty scope no-op");
    };

    // 戻り値の負荷・件数配列も、返却ラベルから独立に再計算して検査する
    const auto test_returned_load_and_count = [&]() -> void {
        for (const bool count_bounds : {false, true}) {
            problem_type p;
            p.vertex_count = 10;
            p.label_count = 3;
            p.demand = {0, 2, 1, 3, 0, 4, 2, 1, 3, 0};
            p.upper_capacity = {20, 20, 20};
            if (count_bounds) p.upper_count = {6, 6, 6};
            p.fixed_label.assign(10, -1);
            p.fixed_label[0] = 0;
            p.allowed.assign(30, 1);
            p.allowed[0] = 0; // 固定ラベルが許可表より優先される
            std::vector<int> initial(10);
            for (int v = 0; v < 10; ++v) {
                initial[v] = v % 3;
                for (int a = 0; a < 3; ++a) p.unary_cost.push_back((v * 7 + a * 11) % 19 - 9);
                p.edges.push_back({v, (v + 1) % 10, v % 5 - 2, 0});
            }
            const std::vector<cost_type> matrix = {0, 3, -1, 2, 1, 4, -2, 5, 0};
            const auto check = [&](const auto& policy) {
                for (int seed = 0; seed < 8; ++seed) {
                    capacitated_graph_labeling_options o;
                    o.time_limit_ms = -1;
                    o.iteration_limit = 3000;
                    o.seed = static_cast<std::uint64_t>(seed);
                    for (const bool warm : {false, true}) {
                        const auto r = warm ? improve_capacitated_graph_labeling(p, initial, policy, o)
                                            : solve_capacitated_graph_labeling(p, policy, o);
                        require(r.feasible, "metadata feasible");
                        std::vector<long long> loads(3, 0);
                        std::vector<int> counts(3, 0);
                        for (int v = 0; v < 10; ++v) {
                            require(r.label[v] >= 0 && r.label[v] < 3, "metadata label range");
                            loads[r.label[v]] += p.demand[v];
                            ++counts[r.label[v]];
                        }
                        require(r.load == loads, "returned load does not match labels");
                        require(count_bounds ? r.count == counts : r.count.empty(), "returned count does not match labels");
                    }
                }
            };
            check(capacitated_potts_cost<>{});
            check(capacitated_label_matrix_cost<>{3, &matrix});
        }
    };

    test_basic_potts();
    test_matrix_and_constraints();
    test_random_against_bruteforce();
    test_random_matrix_against_bruteforce();
    test_weighted_exact_capacity_construction();
    test_infeasible();
    test_subset_repair_and_status();
    test_absolute_deadline();
    test_time_boundaries();
    test_subset_repair_with_general_pair_cost();
    test_simultaneous_load_and_count_bounds();
    test_fast_random_bounds();
    test_zero_demand_construction();
    test_extended_random_costs.template operator()<long long>();
    test_extended_random_costs.template operator()<double>();
    test_empty_and_invalid_scopes();
    test_returned_load_and_count();
    std::cout << "all tests passed\n";
}

#endif
