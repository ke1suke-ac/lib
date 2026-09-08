/*
 * difference_constraints: 差分制約系（牛ゲー）を扱う4種類の実装。
 *
 * 共通して扱う制約:
 *   x[to] - x[from] <= upper
 * この制約を from -> to、重み upper の有向辺とみなす。
 * 制約全体が実行可能であることは、グラフ全体に負閉路が存在しないことと同値。
 * 実行可能かつ from から to へ到達可能なら、x[to] - x[from] の最大値は
 * from -> to 最短路長に一致する。到達不能なら最大値は上に非有界。
 *
 * 実装の選び方:
 *   difference_constraints_bellman_ford
 *     ACL非依存の基本形。全辺走査型Bellman-Fordを使う。
 *     最悪 O(VE)。小規模、最小構成、正解比較用に向く。
 *
 *   difference_constraints_scc_bellman_ford
 *     ACLのscc_graphで強連結成分分解し、各SCC内だけBellman-Fordを行う。
 *     O(V + E + sum_C |V_C||E_C|)。DAGに近い場合や小さなSCCが多い場合に強い。
 *     グラフ全体が1つのSCCなら、最悪計算量は基本形と同じ O(VE)。
 *
 *   difference_constraints_spfa
 *     更新された頂点だけをdequeで処理し、SLF順序を使うSPFA版。
 *     最悪 O(VE) だが、更新が局所的な非敵対的入力では高速になりやすい。
 *
 *   difference_constraints_floyd_warshall
 *     小規模・密グラフ・全点対問い合わせ向け。
 *     初回構築 O(V^3)、問い合わせ O(1)、構築後の制約追加 O(V^2)、メモリ O(V^2)。
 *
 * 4構造体の公開APIと結果型は同じ。各構造体は他の構造体や共通基底に依存せず、
 * struct全体だけを個別にコピペできる。SCC版だけは <atcoder/scc> を必要とする。
 *
 * 典型的な制約の追加:
 *   DC dc(n);
 *   dc.add_upper_bound(u, v, r);    // x[v] - x[u] <= r
 *   dc.add_lower_bound(u, v, l);    // l <= x[v] - x[u]
 *   dc.add_bounds(u, v, l, r);      // l <= x[v] - x[u] <= r
 *   dc.add_equal(u, v, d);          // x[v] - x[u] == d
 *   dc.add_abs_upper_bound(u, v, d);// |x[v] - x[u]| <= d
 *   dc.add_upper_bound(v, r);       // x[v] <= r
 *   dc.add_lower_bound(v, l);       // l <= x[v]
 *   dc.add_bounds(v, l, r);         // l <= x[v] <= r
 *   dc.add_equal(v, a);             // x[v] == a
 *
 * 典型的な問い合わせ:
 *   if (!dc.feasible()) { ... }                 // 制約全体の実行可能性
 *   auto a = dc.feasible_assignment();          // 任意の実行可能解
 *   auto mx = dc.maximum_difference(s, t);      // max x[t] - x[s]
 *   auto mn = dc.minimum_difference(s, t);      // min x[t] - x[s]
 *   auto range = dc.difference_bounds(s, t);    // x[t] - x[s] の上下限
 *   auto sol = dc.maximum_difference_solution(s, t); // 最大値を達成する解
 *
 * 典型ユースケース:
 *   ・累積和 P を変数にして、区間和 l <= P[r] - P[l] <= r を表す
 *   ・開始時刻を変数にして、start[v] - start[u] >= duration を表す
 *   ・座標間の最小距離・最大距離・固定された並び順を表す
 *   ・数列の上下限と |a[i + 1] - a[i]| <= d を組み合わせる
 *   ・残余グラフの辺から最小費用流の双対ポテンシャルを復元する
 *
 * 適用できない代表例:
 *   ・x[i] + x[j] <= c、2*x[i] - x[j] <= c など係数が +1,-1 以外の制約
 *   ・|x[i] - x[j]| >= d のような選言を含む制約
 *   ・任意の重み付き総和、積、max/minなどを目的関数にする問題
 *   ・制約削除を含む完全動的問題
 * 等式制約しかない場合は、ポテンシャル付きUnion-Findの方が高速。
 *
 * 数値型:
 *   Weight は辺重みの保存型、Calc はポテンシャル・最短距離の計算型。
 *   通常は両方 long long でよい。経路重みの和が long long を超える場合は、
 *   difference_constraints_...(long long, __int128_t) の形で Calc を広げられる。
 *   オーバーフロー検査は行わない。重みの符号反転を含む全中間値が各型に収まること。
 */
#pragma once

#include <bits/stdc++.h>

// ACLに依存しない、全辺走査型Bellman-Fordによる差分制約系
template <class Weight = long long, class Calc = Weight>
struct difference_constraints_bellman_ford {
    static_assert(std::numeric_limits<Weight>::is_signed, "Weight は符号付き数値型にする");
    static_assert(std::numeric_limits<Calc>::is_signed, "Calc は符号付き数値型にする");

    enum class status {
        infeasible,
        unbounded,
        finite,
    };

    struct bound_result {
        status state = status::infeasible;
        Calc value{};
    };

    struct bounds_result {
        bool feasible = false;
        std::optional<Calc> lower;
        std::optional<Calc> upper;
    };

    struct solution_result {
        status state = status::infeasible;
        Calc value{};
        std::vector<Calc> values;
    };

private:
    struct Edge {
        int from;
        int to;
        Weight weight;
    };

    int n;
    int vertex_count;
    int zero_vertex;
    std::vector<Edge> edges;
    std::vector<std::vector<int>> graph;
    std::vector<Calc> potential;
    bool solved = false;
    bool feasible_cache = false;

    static constexpr Calc infinity() { return std::numeric_limits<Calc>::max(); }

    void invalidate() {
        solved = false;
        feasible_cache = false;
        graph.clear();
        potential.clear();
    }

    void add_edge(int from, int to, Weight weight) {
        assert(0 <= from && from < vertex_count);
        assert(0 <= to && to < vertex_count);
        edges.push_back({from, to, weight});
        invalidate();
    }

    void build_graph() {
        graph.assign(vertex_count, {});
        std::vector<int> degree(vertex_count, 0);
        for (const Edge& edge : edges) ++degree[edge.from];
        for (int v = 0; v < vertex_count; ++v) graph[v].reserve(degree[v]);
        for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
            graph[edges[edge_id].from].push_back(
                static_cast<int>(edge_id));
        }
    }

    bool solve_internal() {
        if (solved) return feasible_cache;

        // 全頂点へ重み0の辺を持つ超始点を置く代わりに、全ポテンシャルを0で初期化する
        potential.assign(vertex_count, Calc{});
        bool updated = false;
        for (int iteration = 0; iteration < vertex_count; ++iteration) {
            updated = false;
            for (const Edge& edge : edges) {
                const Calc candidate = potential[edge.from] +
                                       static_cast<Calc>(edge.weight);
                Calc& destination = potential[edge.to];
                if (destination <= candidate) continue;
                destination = candidate;
                updated = true;
            }
            if (!updated) break;
        }

        // V回目にも更新された場合、その更新を生む負閉路がどこかに存在する
        feasible_cache = !updated;
        solved = true;
        if (feasible_cache) build_graph();
        return feasible_cache;
    }

    std::vector<Calc> reduced_distances(int source) const {
        std::vector<Calc> distance(vertex_count, infinity());
        using QueueEntry = std::pair<Calc, int>;
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> queue;
        distance[source] = Calc{};
        queue.emplace(Calc{}, source);

        // 実行可能ポテンシャルにより全辺の簡約重みが非負なのでDijkstraを使える
        while (!queue.empty()) {
            const auto [current_distance, from] = queue.top();
            queue.pop();
            if (distance[from] != current_distance) continue;

            for (int edge_id : graph[from]) {
                const Edge& edge = edges[edge_id];
                const Calc reduced_weight = static_cast<Calc>(edge.weight) +
                                            potential[edge.from] -
                                            potential[edge.to];
                const Calc candidate = current_distance + reduced_weight;
                Calc& destination = distance[edge.to];
                if (destination <= candidate) continue;
                destination = candidate;
                queue.emplace(candidate, edge.to);
            }
        }
        return distance;
    }

    bound_result maximum_difference_internal(int from, int to) {
        if (!solve_internal()) return {status::infeasible, Calc{}};
        const std::vector<Calc> distance = reduced_distances(from);
        if (distance[to] == infinity()) {
            return {status::unbounded, Calc{}};
        }
        const Calc value = distance[to] +
                           potential[to] -
                           potential[from];
        return {status::finite, value};
    }

    solution_result maximum_difference_solution_internal(int from, int to) {
        if (!solve_internal()) return {status::infeasible, Calc{}, {}};
        const std::vector<Calc> distance = reduced_distances(from);
        if (distance[to] == infinity()) {
            return {status::unbounded, Calc{}, {}};
        }

        // 到達不能頂点には、到達可能頂点の最大簡約距離と同じオフセットを与える
        Calc unreachable_offset{};
        for (Calc value : distance) {
            if (value != infinity() && unreachable_offset < value) unreachable_offset = value;
        }
        std::vector<Calc> all_values(vertex_count);
        for (int v = 0; v < vertex_count; ++v) {
            const Calc offset = distance[v] == infinity()
                                    ? unreachable_offset
                                    : distance[v];
            all_values[v] =
                potential[v] + offset;
        }

        // 内部ZERO頂点が0になるよう平行移動し、ユーザー変数だけを返す
        const Calc base = all_values[zero_vertex];
        std::vector<Calc> values(n);
        for (int v = 0; v < n; ++v) {
            values[v] = all_values[v] - base;
        }
        const Calc optimum = distance[to] +
                             potential[to] -
                             potential[from];
        return {status::finite, optimum, std::move(values)};
    }

public:
    // n個の変数を持つ空の差分制約系を構築する。O(n)
    explicit difference_constraints_bellman_ford(int n_)
        : n(n_), vertex_count(n_ + 1), zero_vertex(n_) {
        assert(n_ >= 0);
    }

    // ユーザー変数の個数を返す。O(1)
    int size() const { return n; }

    // expected_edges本の内部有向辺を再確保なしで格納できるよう予約する。O(E)になる場合がある
    void reserve_edges(std::size_t expected_edges) { edges.reserve(expected_edges); }

    // x[to] - x[from] <= upper を追加する。償却 O(1)
    void add_upper_bound(int from, int to, Weight upper) { add_edge(from, to, upper); }

    // x[to] - x[from] >= lower を追加する。償却 O(1)
    void add_lower_bound(int from, int to, Weight lower) {
        add_edge(to, from, static_cast<Weight>(-static_cast<Calc>(lower)));
    }

    // lower <= x[to] - x[from] <= upper を追加する。償却 O(1)
    void add_bounds(int from, int to, Weight lower, Weight upper) {
        add_upper_bound(from, to, upper);
        add_lower_bound(from, to, lower);
    }

    // x[to] - x[from] == difference を追加する。償却 O(1)
    void add_equal(int from, int to, Weight difference) {
        add_bounds(from, to, difference, difference);
    }

    // |x[to] - x[from]| <= limit を追加する。償却 O(1)
    void add_abs_upper_bound(int from, int to, Weight limit) {
        add_bounds(from, to, static_cast<Weight>(-static_cast<Calc>(limit)), limit);
    }

    // x[variable] <= upper を追加する。償却 O(1)
    void add_upper_bound(int variable, Weight upper) {
        add_edge(zero_vertex, variable, upper);
    }

    // x[variable] >= lower を追加する。償却 O(1)
    void add_lower_bound(int variable, Weight lower) {
        add_edge(variable, zero_vertex, static_cast<Weight>(-static_cast<Calc>(lower)));
    }

    // lower <= x[variable] <= upper を追加する。償却 O(1)
    void add_bounds(int variable, Weight lower, Weight upper) {
        add_upper_bound(variable, upper);
        add_lower_bound(variable, lower);
    }

    // x[variable] == value を追加する。償却 O(1)
    void add_equal(int variable, Weight value) { add_bounds(variable, value, value); }

    // 全制約を同時に満たす値が存在するか判定する。初回 O(VE)、変更がなければ O(1)
    bool feasible() { return solve_internal(); }

    // 任意の実行可能解を返し、矛盾時はnulloptを返す。初回 O(VE)、以後 O(V)
    std::optional<std::vector<Calc>> feasible_assignment() {
        if (!solve_internal()) return std::nullopt;
        const Calc base = potential[zero_vertex];
        std::vector<Calc> values(n);
        for (int v = 0; v < n; ++v) {
            values[v] = potential[v] - base;
        }
        return values;
    }

    // x[to] - x[from] の最大値を返す。初回 O(VE)、実行可能性計算済みなら O((V+E)log V)
    bound_result maximum_difference(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        return maximum_difference_internal(from, to);
    }

    // x[to] - x[from] の最小値を返す。初回 O(VE)、実行可能性計算済みなら O((V+E)log V)
    bound_result minimum_difference(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        bound_result result = maximum_difference_internal(to, from);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }

    // x[to] - x[from] の下限と上限を返す。初回 O(VE)、実行可能性計算済みなら O((V+E)log V)
    bounds_result difference_bounds(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        if (!solve_internal()) return {};
        const bound_result upper_result = maximum_difference_internal(from, to);
        const bound_result reverse_result = maximum_difference_internal(to, from);
        bounds_result result;
        result.feasible = true;
        if (reverse_result.state == status::finite) result.lower = -reverse_result.value;
        if (upper_result.state == status::finite) result.upper = upper_result.value;
        return result;
    }

    // x[variable] の最大値を返す。初回 O(VE)、実行可能性計算済みなら O((V+E)log V)
    bound_result maximum_value(int variable) {
        assert(0 <= variable && variable < n);
        return maximum_difference_internal(zero_vertex, variable);
    }

    // x[variable] の最小値を返す。初回 O(VE)、実行可能性計算済みなら O((V+E)log V)
    bound_result minimum_value(int variable) {
        assert(0 <= variable && variable < n);
        bound_result result = maximum_difference_internal(variable, zero_vertex);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }

    // x[variable] の下限と上限を返す。初回 O(VE)、実行可能性計算済みなら O((V+E)log V)
    bounds_result value_bounds(int variable) {
        assert(0 <= variable && variable < n);
        if (!solve_internal()) return {};
        const bound_result upper_result = maximum_difference_internal(zero_vertex, variable);
        const bound_result reverse_result = maximum_difference_internal(variable, zero_vertex);
        bounds_result result;
        result.feasible = true;
        if (reverse_result.state == status::finite) result.lower = -reverse_result.value;
        if (upper_result.state == status::finite) result.upper = upper_result.value;
        return result;
    }

    // x[to] - x[from] の最大値と、それを達成する解を返す。O(VE + (V+E)log V)
    solution_result maximum_difference_solution(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        return maximum_difference_solution_internal(from, to);
    }

    // x[to] - x[from] の最小値と、それを達成する解を返す。O(VE + (V+E)log V)
    solution_result minimum_difference_solution(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        solution_result result = maximum_difference_solution_internal(to, from);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }

    // x[variable] の最大値と、それを達成する解を返す。O(VE + (V+E)log V)
    solution_result maximum_value_solution(int variable) {
        assert(0 <= variable && variable < n);
        return maximum_difference_solution_internal(zero_vertex, variable);
    }

    // x[variable] の最小値と、それを達成する解を返す。O(VE + (V+E)log V)
    solution_result minimum_value_solution(int variable) {
        assert(0 <= variable && variable < n);
        solution_result result = maximum_difference_solution_internal(variable, zero_vertex);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }
};

#include <atcoder/scc>

// ACLのSCC分解後に各成分内でBellman-Fordを行う差分制約系
template <class Weight = long long, class Calc = Weight>
struct difference_constraints_scc_bellman_ford {
    static_assert(std::numeric_limits<Weight>::is_signed, "Weight は符号付き数値型にする");
    static_assert(std::numeric_limits<Calc>::is_signed, "Calc は符号付き数値型にする");

    enum class status {
        infeasible,
        unbounded,
        finite,
    };

    struct bound_result {
        status state = status::infeasible;
        Calc value{};
    };

    struct bounds_result {
        bool feasible = false;
        std::optional<Calc> lower;
        std::optional<Calc> upper;
    };

    struct solution_result {
        status state = status::infeasible;
        Calc value{};
        std::vector<Calc> values;
    };

private:
    struct Edge {
        int from;
        int to;
        Weight weight;
    };

    int n;
    int vertex_count;
    int zero_vertex;
    std::vector<Edge> edges;
    std::vector<std::vector<int>> graph;
    std::vector<Calc> potential;
    bool solved = false;
    bool feasible_cache = false;

    static constexpr Calc infinity() { return std::numeric_limits<Calc>::max(); }

    void invalidate() {
        solved = false;
        feasible_cache = false;
        graph.clear();
        potential.clear();
    }

    void add_edge(int from, int to, Weight weight) {
        assert(0 <= from && from < vertex_count);
        assert(0 <= to && to < vertex_count);
        edges.push_back({from, to, weight});
        invalidate();
    }

    void build_graph() {
        graph.assign(vertex_count, {});
        std::vector<int> degree(vertex_count, 0);
        for (const Edge& edge : edges) ++degree[edge.from];
        for (int v = 0; v < vertex_count; ++v) graph[v].reserve(degree[v]);
        for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
            graph[edges[edge_id].from].push_back(
                static_cast<int>(edge_id));
        }
    }

    bool solve_internal() {
        if (solved) return feasible_cache;

        // 負閉路は必ず1つのSCC内に収まるため、内部辺だけを成分ごとに緩和する
        atcoder::scc_graph scc_graph(vertex_count);
        for (const Edge& edge : edges) scc_graph.add_edge(edge.from, edge.to);
        const std::vector<std::vector<int>> groups = scc_graph.scc();
        const int component_count = static_cast<int>(groups.size());
        std::vector<int> component(vertex_count);
        for (int component_id = 0; component_id < component_count; ++component_id) {
            for (int v : groups[component_id]) {
                component[v] = component_id;
            }
        }

        std::vector<std::vector<int>> internal_edges(component_count);
        std::vector<std::vector<int>> outgoing_edges(component_count);
        for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
            const Edge& edge = edges[edge_id];
            const int from_component = component[edge.from];
            const int to_component = component[edge.to];
            if (from_component == to_component) {
                internal_edges[from_component].push_back(
                    static_cast<int>(edge_id));
            } else {
                outgoing_edges[from_component].push_back(
                    static_cast<int>(edge_id));
            }
        }

        potential.assign(vertex_count, Calc{});
        for (int component_id = 0; component_id < component_count; ++component_id) {
            const int component_size =
                static_cast<int>(groups[component_id].size());
            bool updated = false;
            for (int iteration = 0; iteration < component_size; ++iteration) {
                updated = false;
                for (int edge_id : internal_edges[component_id]) {
                    const Edge& edge = edges[edge_id];
                    const Calc candidate = potential[edge.from] +
                                           static_cast<Calc>(edge.weight);
                    Calc& destination = potential[edge.to];
                    if (destination <= candidate) continue;
                    destination = candidate;
                    updated = true;
                }
                if (!updated) break;
            }
            if (updated) {
                solved = true;
                feasible_cache = false;
                return false;
            }
        }

        // ACLのSCCはトポロジカル順なので、成分全体のオフセットを前から確定できる
        std::vector<Calc> component_offset(component_count, Calc{});
        for (int component_id = 0; component_id < component_count; ++component_id) {
            for (int edge_id : outgoing_edges[component_id]) {
                const Edge& edge = edges[edge_id];
                const int to_component = component[edge.to];
                const Calc candidate = component_offset[component_id] +
                                       potential[edge.from] +
                                       static_cast<Calc>(edge.weight) -
                                       potential[edge.to];
                Calc& destination = component_offset[to_component];
                if (destination > candidate) destination = candidate;
            }
        }
        for (int v = 0; v < vertex_count; ++v) {
            potential[v] +=
                component_offset[component[v]];
        }

        feasible_cache = true;
        solved = true;
        build_graph();
        return true;
    }

    std::vector<Calc> reduced_distances(int source) const {
        std::vector<Calc> distance(vertex_count, infinity());
        using QueueEntry = std::pair<Calc, int>;
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> queue;
        distance[source] = Calc{};
        queue.emplace(Calc{}, source);

        // 実行可能ポテンシャルにより全辺の簡約重みが非負なのでDijkstraを使える
        while (!queue.empty()) {
            const auto [current_distance, from] = queue.top();
            queue.pop();
            if (distance[from] != current_distance) continue;

            for (int edge_id : graph[from]) {
                const Edge& edge = edges[edge_id];
                const Calc reduced_weight = static_cast<Calc>(edge.weight) +
                                            potential[edge.from] -
                                            potential[edge.to];
                const Calc candidate = current_distance + reduced_weight;
                Calc& destination = distance[edge.to];
                if (destination <= candidate) continue;
                destination = candidate;
                queue.emplace(candidate, edge.to);
            }
        }
        return distance;
    }

    bound_result maximum_difference_internal(int from, int to) {
        if (!solve_internal()) return {status::infeasible, Calc{}};
        const std::vector<Calc> distance = reduced_distances(from);
        if (distance[to] == infinity()) {
            return {status::unbounded, Calc{}};
        }
        const Calc value = distance[to] +
                           potential[to] -
                           potential[from];
        return {status::finite, value};
    }

    solution_result maximum_difference_solution_internal(int from, int to) {
        if (!solve_internal()) return {status::infeasible, Calc{}, {}};
        const std::vector<Calc> distance = reduced_distances(from);
        if (distance[to] == infinity()) {
            return {status::unbounded, Calc{}, {}};
        }

        // 到達不能頂点には、到達可能頂点の最大簡約距離と同じオフセットを与える
        Calc unreachable_offset{};
        for (Calc value : distance) {
            if (value != infinity() && unreachable_offset < value) unreachable_offset = value;
        }
        std::vector<Calc> all_values(vertex_count);
        for (int v = 0; v < vertex_count; ++v) {
            const Calc offset = distance[v] == infinity()
                                    ? unreachable_offset
                                    : distance[v];
            all_values[v] =
                potential[v] + offset;
        }

        // 内部ZERO頂点が0になるよう平行移動し、ユーザー変数だけを返す
        const Calc base = all_values[zero_vertex];
        std::vector<Calc> values(n);
        for (int v = 0; v < n; ++v) {
            values[v] = all_values[v] - base;
        }
        const Calc optimum = distance[to] +
                             potential[to] -
                             potential[from];
        return {status::finite, optimum, std::move(values)};
    }

public:
    // n個の変数を持つ空の差分制約系を構築する。O(n)
    explicit difference_constraints_scc_bellman_ford(int n_)
        : n(n_), vertex_count(n_ + 1), zero_vertex(n_) {
        assert(n_ >= 0);
    }

    // ユーザー変数の個数を返す。O(1)
    int size() const { return n; }

    // expected_edges本の内部有向辺を再確保なしで格納できるよう予約する。O(E)になる場合がある
    void reserve_edges(std::size_t expected_edges) { edges.reserve(expected_edges); }

    // x[to] - x[from] <= upper を追加する。償却 O(1)
    void add_upper_bound(int from, int to, Weight upper) { add_edge(from, to, upper); }

    // x[to] - x[from] >= lower を追加する。償却 O(1)
    void add_lower_bound(int from, int to, Weight lower) {
        add_edge(to, from, static_cast<Weight>(-static_cast<Calc>(lower)));
    }

    // lower <= x[to] - x[from] <= upper を追加する。償却 O(1)
    void add_bounds(int from, int to, Weight lower, Weight upper) {
        add_upper_bound(from, to, upper);
        add_lower_bound(from, to, lower);
    }

    // x[to] - x[from] == difference を追加する。償却 O(1)
    void add_equal(int from, int to, Weight difference) {
        add_bounds(from, to, difference, difference);
    }

    // |x[to] - x[from]| <= limit を追加する。償却 O(1)
    void add_abs_upper_bound(int from, int to, Weight limit) {
        add_bounds(from, to, static_cast<Weight>(-static_cast<Calc>(limit)), limit);
    }

    // x[variable] <= upper を追加する。償却 O(1)
    void add_upper_bound(int variable, Weight upper) {
        add_edge(zero_vertex, variable, upper);
    }

    // x[variable] >= lower を追加する。償却 O(1)
    void add_lower_bound(int variable, Weight lower) {
        add_edge(variable, zero_vertex, static_cast<Weight>(-static_cast<Calc>(lower)));
    }

    // lower <= x[variable] <= upper を追加する。償却 O(1)
    void add_bounds(int variable, Weight lower, Weight upper) {
        add_upper_bound(variable, upper);
        add_lower_bound(variable, lower);
    }

    // x[variable] == value を追加する。償却 O(1)
    void add_equal(int variable, Weight value) { add_bounds(variable, value, value); }

    // 全制約を同時に満たす値が存在するか判定する。初回 O(V+E+sum |V_C||E_C|)、変更がなければ O(1)
    bool feasible() { return solve_internal(); }

    // 任意の実行可能解を返し、矛盾時はnulloptを返す。初回 O(V+E+sum |V_C||E_C|)、以後 O(V)
    std::optional<std::vector<Calc>> feasible_assignment() {
        if (!solve_internal()) return std::nullopt;
        const Calc base = potential[zero_vertex];
        std::vector<Calc> values(n);
        for (int v = 0; v < n; ++v) {
            values[v] = potential[v] - base;
        }
        return values;
    }

    // x[to] - x[from] の最大値を返す。初回 O(V+E+sum |V_C||E_C|)、計算済みなら O((V+E)log V)
    bound_result maximum_difference(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        return maximum_difference_internal(from, to);
    }

    // x[to] - x[from] の最小値を返す。初回 O(V+E+sum |V_C||E_C|)、計算済みなら O((V+E)log V)
    bound_result minimum_difference(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        bound_result result = maximum_difference_internal(to, from);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }

    // x[to] - x[from] の下限と上限を返す。初回 O(V+E+sum |V_C||E_C|)、計算済みなら O((V+E)log V)
    bounds_result difference_bounds(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        if (!solve_internal()) return {};
        const bound_result upper_result = maximum_difference_internal(from, to);
        const bound_result reverse_result = maximum_difference_internal(to, from);
        bounds_result result;
        result.feasible = true;
        if (reverse_result.state == status::finite) result.lower = -reverse_result.value;
        if (upper_result.state == status::finite) result.upper = upper_result.value;
        return result;
    }

    // x[variable] の最大値を返す。初回 O(V+E+sum |V_C||E_C|)、計算済みなら O((V+E)log V)
    bound_result maximum_value(int variable) {
        assert(0 <= variable && variable < n);
        return maximum_difference_internal(zero_vertex, variable);
    }

    // x[variable] の最小値を返す。初回 O(V+E+sum |V_C||E_C|)、計算済みなら O((V+E)log V)
    bound_result minimum_value(int variable) {
        assert(0 <= variable && variable < n);
        bound_result result = maximum_difference_internal(variable, zero_vertex);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }

    // x[variable] の下限と上限を返す。初回 O(V+E+sum |V_C||E_C|)、計算済みなら O((V+E)log V)
    bounds_result value_bounds(int variable) {
        assert(0 <= variable && variable < n);
        if (!solve_internal()) return {};
        const bound_result upper_result = maximum_difference_internal(zero_vertex, variable);
        const bound_result reverse_result = maximum_difference_internal(variable, zero_vertex);
        bounds_result result;
        result.feasible = true;
        if (reverse_result.state == status::finite) result.lower = -reverse_result.value;
        if (upper_result.state == status::finite) result.upper = upper_result.value;
        return result;
    }

    // x[to] - x[from] の最大値と、それを達成する解を返す。O(V+E+sum |V_C||E_C|+(V+E)log V)
    solution_result maximum_difference_solution(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        return maximum_difference_solution_internal(from, to);
    }

    // x[to] - x[from] の最小値と、それを達成する解を返す。O(V+E+sum |V_C||E_C|+(V+E)log V)
    solution_result minimum_difference_solution(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        solution_result result = maximum_difference_solution_internal(to, from);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }

    // x[variable] の最大値と、それを達成する解を返す。O(V+E+sum |V_C||E_C|+(V+E)log V)
    solution_result maximum_value_solution(int variable) {
        assert(0 <= variable && variable < n);
        return maximum_difference_solution_internal(zero_vertex, variable);
    }

    // x[variable] の最小値と、それを達成する解を返す。O(V+E+sum |V_C||E_C|+(V+E)log V)
    solution_result minimum_value_solution(int variable) {
        assert(0 <= variable && variable < n);
        solution_result result = maximum_difference_solution_internal(variable, zero_vertex);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }
};

// ACLに依存しない、dequeとSLF順序を使うSPFAによる差分制約系
template <class Weight = long long, class Calc = Weight>
struct difference_constraints_spfa {
    static_assert(std::numeric_limits<Weight>::is_signed, "Weight は符号付き数値型にする");
    static_assert(std::numeric_limits<Calc>::is_signed, "Calc は符号付き数値型にする");

    enum class status {
        infeasible,
        unbounded,
        finite,
    };

    struct bound_result {
        status state = status::infeasible;
        Calc value{};
    };

    struct bounds_result {
        bool feasible = false;
        std::optional<Calc> lower;
        std::optional<Calc> upper;
    };

    struct solution_result {
        status state = status::infeasible;
        Calc value{};
        std::vector<Calc> values;
    };

private:
    struct Edge {
        int from;
        int to;
        Weight weight;
    };

    int n;
    int vertex_count;
    int zero_vertex;
    std::vector<Edge> edges;
    std::vector<std::vector<int>> graph;
    std::vector<Calc> potential;
    bool solved = false;
    bool feasible_cache = false;

    static constexpr Calc infinity() { return std::numeric_limits<Calc>::max(); }

    void invalidate() {
        solved = false;
        feasible_cache = false;
        graph.clear();
        potential.clear();
    }

    void add_edge(int from, int to, Weight weight) {
        assert(0 <= from && from < vertex_count);
        assert(0 <= to && to < vertex_count);
        edges.push_back({from, to, weight});
        invalidate();
    }

    void build_graph() {
        graph.assign(vertex_count, {});
        std::vector<int> degree(vertex_count, 0);
        for (const Edge& edge : edges) ++degree[edge.from];
        for (int v = 0; v < vertex_count; ++v) graph[v].reserve(degree[v]);
        for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
            graph[edges[edge_id].from].push_back(
                static_cast<int>(edge_id));
        }
    }

    bool solve_internal() {
        if (solved) return feasible_cache;
        build_graph();
        potential.assign(vertex_count, Calc{});
        std::vector<int> path_length(vertex_count, 0);
        std::vector<unsigned char> in_queue(vertex_count, 0);
        std::deque<int> queue;

        // 初期ポテンシャル0が違反し得るのは負辺だけなので、その始点だけを起点にする
        for (const Edge& edge : edges) {
            if (edge.weight >= Weight{} || in_queue[edge.from] != 0) {
                continue;
            }
            in_queue[edge.from] = 1;
            queue.push_back(edge.from);
        }

        // 更新頂点だけを処理し、小さいラベルをdeque前方へ置くSLFで平均速度を改善する
        while (!queue.empty()) {
            const int from = queue.front();
            queue.pop_front();
            in_queue[from] = 0;

            for (int edge_id : graph[from]) {
                const Edge& edge = edges[edge_id];
                const Calc candidate = potential[edge.from] +
                                       static_cast<Calc>(edge.weight);
                Calc& destination = potential[edge.to];
                if (destination <= candidate) continue;
                destination = candidate;
                path_length[edge.to] =
                    path_length[edge.from] + 1;
                if (path_length[edge.to] >= vertex_count) {
                    solved = true;
                    feasible_cache = false;
                    return false;
                }
                if (in_queue[edge.to] != 0) continue;

                in_queue[edge.to] = 1;
                if (!queue.empty() && destination < potential[queue.front()]) {
                    queue.push_front(edge.to);
                } else {
                    queue.push_back(edge.to);
                }
            }
        }

        feasible_cache = true;
        solved = true;
        return true;
    }

    std::vector<Calc> reduced_distances(int source) const {
        std::vector<Calc> distance(vertex_count, infinity());
        using QueueEntry = std::pair<Calc, int>;
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> queue;
        distance[source] = Calc{};
        queue.emplace(Calc{}, source);

        // 実行可能ポテンシャルにより全辺の簡約重みが非負なのでDijkstraを使える
        while (!queue.empty()) {
            const auto [current_distance, from] = queue.top();
            queue.pop();
            if (distance[from] != current_distance) continue;

            for (int edge_id : graph[from]) {
                const Edge& edge = edges[edge_id];
                const Calc reduced_weight = static_cast<Calc>(edge.weight) +
                                            potential[edge.from] -
                                            potential[edge.to];
                const Calc candidate = current_distance + reduced_weight;
                Calc& destination = distance[edge.to];
                if (destination <= candidate) continue;
                destination = candidate;
                queue.emplace(candidate, edge.to);
            }
        }
        return distance;
    }

    bound_result maximum_difference_internal(int from, int to) {
        if (!solve_internal()) return {status::infeasible, Calc{}};
        const std::vector<Calc> distance = reduced_distances(from);
        if (distance[to] == infinity()) {
            return {status::unbounded, Calc{}};
        }
        const Calc value = distance[to] +
                           potential[to] -
                           potential[from];
        return {status::finite, value};
    }

    solution_result maximum_difference_solution_internal(int from, int to) {
        if (!solve_internal()) return {status::infeasible, Calc{}, {}};
        const std::vector<Calc> distance = reduced_distances(from);
        if (distance[to] == infinity()) {
            return {status::unbounded, Calc{}, {}};
        }

        // 到達不能頂点には、到達可能頂点の最大簡約距離と同じオフセットを与える
        Calc unreachable_offset{};
        for (Calc value : distance) {
            if (value != infinity() && unreachable_offset < value) unreachable_offset = value;
        }
        std::vector<Calc> all_values(vertex_count);
        for (int v = 0; v < vertex_count; ++v) {
            const Calc offset = distance[v] == infinity()
                                    ? unreachable_offset
                                    : distance[v];
            all_values[v] =
                potential[v] + offset;
        }

        // 内部ZERO頂点が0になるよう平行移動し、ユーザー変数だけを返す
        const Calc base = all_values[zero_vertex];
        std::vector<Calc> values(n);
        for (int v = 0; v < n; ++v) {
            values[v] = all_values[v] - base;
        }
        const Calc optimum = distance[to] +
                             potential[to] -
                             potential[from];
        return {status::finite, optimum, std::move(values)};
    }

public:
    // n個の変数を持つ空の差分制約系を構築する。O(n)
    explicit difference_constraints_spfa(int n_)
        : n(n_), vertex_count(n_ + 1), zero_vertex(n_) {
        assert(n_ >= 0);
    }

    // ユーザー変数の個数を返す。O(1)
    int size() const { return n; }

    // expected_edges本の内部有向辺を再確保なしで格納できるよう予約する。O(E)になる場合がある
    void reserve_edges(std::size_t expected_edges) { edges.reserve(expected_edges); }

    // x[to] - x[from] <= upper を追加する。償却 O(1)
    void add_upper_bound(int from, int to, Weight upper) { add_edge(from, to, upper); }

    // x[to] - x[from] >= lower を追加する。償却 O(1)
    void add_lower_bound(int from, int to, Weight lower) {
        add_edge(to, from, static_cast<Weight>(-static_cast<Calc>(lower)));
    }

    // lower <= x[to] - x[from] <= upper を追加する。償却 O(1)
    void add_bounds(int from, int to, Weight lower, Weight upper) {
        add_upper_bound(from, to, upper);
        add_lower_bound(from, to, lower);
    }

    // x[to] - x[from] == difference を追加する。償却 O(1)
    void add_equal(int from, int to, Weight difference) {
        add_bounds(from, to, difference, difference);
    }

    // |x[to] - x[from]| <= limit を追加する。償却 O(1)
    void add_abs_upper_bound(int from, int to, Weight limit) {
        add_bounds(from, to, static_cast<Weight>(-static_cast<Calc>(limit)), limit);
    }

    // x[variable] <= upper を追加する。償却 O(1)
    void add_upper_bound(int variable, Weight upper) {
        add_edge(zero_vertex, variable, upper);
    }

    // x[variable] >= lower を追加する。償却 O(1)
    void add_lower_bound(int variable, Weight lower) {
        add_edge(variable, zero_vertex, static_cast<Weight>(-static_cast<Calc>(lower)));
    }

    // lower <= x[variable] <= upper を追加する。償却 O(1)
    void add_bounds(int variable, Weight lower, Weight upper) {
        add_upper_bound(variable, upper);
        add_lower_bound(variable, lower);
    }

    // x[variable] == value を追加する。償却 O(1)
    void add_equal(int variable, Weight value) { add_bounds(variable, value, value); }

    // 全制約を同時に満たす値が存在するか判定する。平均的に高速、最悪 O(VE)、変更がなければ O(1)
    bool feasible() { return solve_internal(); }

    // 任意の実行可能解を返し、矛盾時はnulloptを返す。初回は最悪 O(VE)、以後 O(V)
    std::optional<std::vector<Calc>> feasible_assignment() {
        if (!solve_internal()) return std::nullopt;
        const Calc base = potential[zero_vertex];
        std::vector<Calc> values(n);
        for (int v = 0; v < n; ++v) {
            values[v] = potential[v] - base;
        }
        return values;
    }

    // x[to] - x[from] の最大値を返す。初回は最悪 O(VE)、計算済みなら O((V+E)log V)
    bound_result maximum_difference(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        return maximum_difference_internal(from, to);
    }

    // x[to] - x[from] の最小値を返す。初回は最悪 O(VE)、計算済みなら O((V+E)log V)
    bound_result minimum_difference(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        bound_result result = maximum_difference_internal(to, from);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }

    // x[to] - x[from] の下限と上限を返す。初回は最悪 O(VE)、計算済みなら O((V+E)log V)
    bounds_result difference_bounds(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        if (!solve_internal()) return {};
        const bound_result upper_result = maximum_difference_internal(from, to);
        const bound_result reverse_result = maximum_difference_internal(to, from);
        bounds_result result;
        result.feasible = true;
        if (reverse_result.state == status::finite) result.lower = -reverse_result.value;
        if (upper_result.state == status::finite) result.upper = upper_result.value;
        return result;
    }

    // x[variable] の最大値を返す。初回は最悪 O(VE)、計算済みなら O((V+E)log V)
    bound_result maximum_value(int variable) {
        assert(0 <= variable && variable < n);
        return maximum_difference_internal(zero_vertex, variable);
    }

    // x[variable] の最小値を返す。初回は最悪 O(VE)、計算済みなら O((V+E)log V)
    bound_result minimum_value(int variable) {
        assert(0 <= variable && variable < n);
        bound_result result = maximum_difference_internal(variable, zero_vertex);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }

    // x[variable] の下限と上限を返す。初回は最悪 O(VE)、計算済みなら O((V+E)log V)
    bounds_result value_bounds(int variable) {
        assert(0 <= variable && variable < n);
        if (!solve_internal()) return {};
        const bound_result upper_result = maximum_difference_internal(zero_vertex, variable);
        const bound_result reverse_result = maximum_difference_internal(variable, zero_vertex);
        bounds_result result;
        result.feasible = true;
        if (reverse_result.state == status::finite) result.lower = -reverse_result.value;
        if (upper_result.state == status::finite) result.upper = upper_result.value;
        return result;
    }

    // x[to] - x[from] の最大値と、それを達成する解を返す。最悪 O(VE + (V+E)log V)
    solution_result maximum_difference_solution(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        return maximum_difference_solution_internal(from, to);
    }

    // x[to] - x[from] の最小値と、それを達成する解を返す。最悪 O(VE + (V+E)log V)
    solution_result minimum_difference_solution(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        solution_result result = maximum_difference_solution_internal(to, from);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }

    // x[variable] の最大値と、それを達成する解を返す。最悪 O(VE + (V+E)log V)
    solution_result maximum_value_solution(int variable) {
        assert(0 <= variable && variable < n);
        return maximum_difference_solution_internal(zero_vertex, variable);
    }

    // x[variable] の最小値と、それを達成する解を返す。最悪 O(VE + (V+E)log V)
    solution_result minimum_value_solution(int variable) {
        assert(0 <= variable && variable < n);
        solution_result result = maximum_difference_solution_internal(variable, zero_vertex);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }
};

// 小規模・密グラフ・全点対問い合わせ向けのFloyd-Warshall差分制約系
template <class Weight = long long, class Calc = Weight>
struct difference_constraints_floyd_warshall {
    static_assert(std::numeric_limits<Weight>::is_signed, "Weight は符号付き数値型にする");
    static_assert(std::numeric_limits<Calc>::is_signed, "Calc は符号付き数値型にする");

    enum class status {
        infeasible,
        unbounded,
        finite,
    };

    struct bound_result {
        status state = status::infeasible;
        Calc value{};
    };

    struct bounds_result {
        bool feasible = false;
        std::optional<Calc> lower;
        std::optional<Calc> upper;
    };

    struct solution_result {
        status state = status::infeasible;
        Calc value{};
        std::vector<Calc> values;
    };

private:
    int n;
    int vertex_count;
    int zero_vertex;
    std::vector<Calc> distance;
    std::vector<Calc> potential;
    bool solved = false;
    bool feasible_cache = false;
    bool potential_ready = false;

    static constexpr Calc infinity() { return std::numeric_limits<Calc>::max(); }

    std::size_t index(int from, int to) const {
        // 乗算をsize_tで行い、intの範囲での積のオーバーフローを避ける。
        return static_cast<std::size_t>(from) * vertex_count + to;
    }

    void add_edge_after_solve(int from, int to, Calc weight) {
        if (!feasible_cache || distance[index(from, to)] <= weight) return;

        // 新辺を含む負閉路は、toからfromへの既存最短路と新辺の組で検出できる
        const Calc reverse_distance = distance[index(to, from)];
        if (reverse_distance != infinity() && reverse_distance + weight < Calc{}) {
            feasible_cache = false;
            potential_ready = false;
            return;
        }

        // 新辺を高々1回使う経路 d[i][from] + weight + d[to][j] で全点対距離を更新する
        std::vector<Calc> distance_to_from(vertex_count);
        std::vector<Calc> distance_from_to(vertex_count);
        for (int v = 0; v < vertex_count; ++v) {
            distance_to_from[v] = distance[index(v, from)];
            distance_from_to[v] = distance[index(to, v)];
        }
        for (int i = 0; i < vertex_count; ++i) {
            const Calc left = distance_to_from[i];
            if (left == infinity()) continue;
            for (int j = 0; j < vertex_count; ++j) {
                const Calc right = distance_from_to[j];
                if (right == infinity()) continue;
                const Calc candidate = left + weight + right;
                Calc& destination = distance[index(i, j)];
                if (destination > candidate) destination = candidate;
            }
        }
        potential_ready = false;
    }

    void add_edge(int from, int to, Weight weight) {
        assert(0 <= from && from < vertex_count);
        assert(0 <= to && to < vertex_count);
        const Calc calc_weight = static_cast<Calc>(weight);
        if (solved) {
            add_edge_after_solve(from, to, calc_weight);
            return;
        }
        Calc& direct = distance[index(from, to)];
        if (direct > calc_weight) direct = calc_weight;
    }

    bool solve_internal() {
        if (solved) return feasible_cache;

        // 中継頂点kを順に許可し、全点対最短距離を計算する
        for (int k = 0; k < vertex_count; ++k) {
            for (int i = 0; i < vertex_count; ++i) {
                const Calc left = distance[index(i, k)];
                if (left == infinity()) continue;
                for (int j = 0; j < vertex_count; ++j) {
                    const Calc right = distance[index(k, j)];
                    if (right == infinity()) continue;
                    const Calc candidate = left + right;
                    Calc& destination = distance[index(i, j)];
                    if (destination > candidate) destination = candidate;
                }
            }
        }

        // 対角成分が負なら、その頂点を含む負閉路が存在する
        feasible_cache = true;
        for (int v = 0; v < vertex_count; ++v) {
            if (distance[index(v, v)] < Calc{}) {
                feasible_cache = false;
                break;
            }
        }
        solved = true;
        potential_ready = false;
        return feasible_cache;
    }

    void build_potential() {
        if (potential_ready || !feasible_cache) return;

        // 全頂点へ重み0の辺を持つ超始点からの最短距離を、全行の列ごとの最小値で得る
        potential.assign(vertex_count, Calc{});
        for (int from = 0; from < vertex_count; ++from) {
            for (int to = 0; to < vertex_count; ++to) {
                const Calc value = distance[index(from, to)];
                Calc& destination = potential[to];
                if (value != infinity() && destination > value) destination = value;
            }
        }
        potential_ready = true;
    }

    bound_result maximum_difference_internal(int from, int to) {
        if (!solve_internal()) return {status::infeasible, Calc{}};
        const Calc value = distance[index(from, to)];
        if (value == infinity()) return {status::unbounded, Calc{}};
        return {status::finite, value};
    }

    solution_result maximum_difference_solution_internal(int from, int to) {
        if (!solve_internal()) return {status::infeasible, Calc{}, {}};
        const Calc optimum = distance[index(from, to)];
        if (optimum == infinity()) return {status::unbounded, Calc{}, {}};
        build_potential();

        // 元の最短距離を簡約距離へ直し、疎グラフ版と同じ方法で最適解を構成する
        std::vector<Calc> reduced_distance(vertex_count, infinity());
        Calc unreachable_offset{};
        for (int v = 0; v < vertex_count; ++v) {
            const Calc original_distance = distance[index(from, v)];
            if (original_distance == infinity()) continue;
            const Calc reduced = original_distance +
                                 potential[from] -
                                 potential[v];
            reduced_distance[v] = reduced;
            if (unreachable_offset < reduced) unreachable_offset = reduced;
        }
        std::vector<Calc> all_values(vertex_count);
        for (int v = 0; v < vertex_count; ++v) {
            const Calc offset = reduced_distance[v] == infinity()
                                    ? unreachable_offset
                                    : reduced_distance[v];
            all_values[v] =
                potential[v] + offset;
        }

        // 内部ZERO頂点が0になるよう平行移動し、ユーザー変数だけを返す
        const Calc base = all_values[zero_vertex];
        std::vector<Calc> values(n);
        for (int v = 0; v < n; ++v) {
            values[v] = all_values[v] - base;
        }
        return {status::finite, optimum, std::move(values)};
    }

public:
    // 行列要素数の乗算はsize_tで行う。
    // n個の変数を持つ空の差分制約系を構築する。O(n^2)
    explicit difference_constraints_floyd_warshall(int n_)
        : n(n_), vertex_count(n_ + 1), zero_vertex(n_),
          distance(static_cast<std::size_t>(n_ + 1) * (n_ + 1), infinity()) {
        assert(n_ >= 0);
        for (int v = 0; v < vertex_count; ++v) distance[index(v, v)] = Calc{};
    }

    // ユーザー変数の個数を返す。O(1)
    int size() const { return n; }

    // API互換用で、行列実装では何もしない。O(1)
    void reserve_edges(std::size_t expected_edges) { (void)expected_edges; }

    // x[to] - x[from] <= upper を追加する。構築前 O(1)、構築後 O(V^2)
    void add_upper_bound(int from, int to, Weight upper) { add_edge(from, to, upper); }

    // x[to] - x[from] >= lower を追加する。構築前 O(1)、構築後 O(V^2)
    void add_lower_bound(int from, int to, Weight lower) {
        add_edge(to, from, static_cast<Weight>(-static_cast<Calc>(lower)));
    }

    // lower <= x[to] - x[from] <= upper を追加する。構築前 O(1)、構築後 O(V^2)
    void add_bounds(int from, int to, Weight lower, Weight upper) {
        add_upper_bound(from, to, upper);
        add_lower_bound(from, to, lower);
    }

    // x[to] - x[from] == difference を追加する。構築前 O(1)、構築後 O(V^2)
    void add_equal(int from, int to, Weight difference) {
        add_bounds(from, to, difference, difference);
    }

    // |x[to] - x[from]| <= limit を追加する。構築前 O(1)、構築後 O(V^2)
    void add_abs_upper_bound(int from, int to, Weight limit) {
        add_bounds(from, to, static_cast<Weight>(-static_cast<Calc>(limit)), limit);
    }

    // x[variable] <= upper を追加する。構築前 O(1)、構築後 O(V^2)
    void add_upper_bound(int variable, Weight upper) {
        add_edge(zero_vertex, variable, upper);
    }

    // x[variable] >= lower を追加する。構築前 O(1)、構築後 O(V^2)
    void add_lower_bound(int variable, Weight lower) {
        add_edge(variable, zero_vertex, static_cast<Weight>(-static_cast<Calc>(lower)));
    }

    // lower <= x[variable] <= upper を追加する。構築前 O(1)、構築後 O(V^2)
    void add_bounds(int variable, Weight lower, Weight upper) {
        add_upper_bound(variable, upper);
        add_lower_bound(variable, lower);
    }

    // x[variable] == value を追加する。構築前 O(1)、構築後 O(V^2)
    void add_equal(int variable, Weight value) { add_bounds(variable, value, value); }

    // 全制約を同時に満たす値が存在するか判定する。初回 O(V^3)、変更がなければ O(1)
    bool feasible() { return solve_internal(); }

    // 任意の実行可能解を返し、矛盾時はnulloptを返す。初回 O(V^3)、構築後 O(V^2)
    std::optional<std::vector<Calc>> feasible_assignment() {
        if (!solve_internal()) return std::nullopt;
        build_potential();
        const Calc base = potential[zero_vertex];
        std::vector<Calc> values(n);
        for (int v = 0; v < n; ++v) {
            values[v] = potential[v] - base;
        }
        return values;
    }

    // x[to] - x[from] の最大値を返す。初回 O(V^3)、構築後 O(1)
    bound_result maximum_difference(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        return maximum_difference_internal(from, to);
    }

    // x[to] - x[from] の最小値を返す。初回 O(V^3)、構築後 O(1)
    bound_result minimum_difference(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        bound_result result = maximum_difference_internal(to, from);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }

    // x[to] - x[from] の下限と上限を返す。初回 O(V^3)、構築後 O(1)
    bounds_result difference_bounds(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        if (!solve_internal()) return {};
        bounds_result result;
        result.feasible = true;
        const Calc upper = distance[index(from, to)];
        const Calc reverse = distance[index(to, from)];
        if (reverse != infinity()) result.lower = -reverse;
        if (upper != infinity()) result.upper = upper;
        return result;
    }

    // x[variable] の最大値を返す。初回 O(V^3)、構築後 O(1)
    bound_result maximum_value(int variable) {
        assert(0 <= variable && variable < n);
        return maximum_difference_internal(zero_vertex, variable);
    }

    // x[variable] の最小値を返す。初回 O(V^3)、構築後 O(1)
    bound_result minimum_value(int variable) {
        assert(0 <= variable && variable < n);
        bound_result result = maximum_difference_internal(variable, zero_vertex);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }

    // x[variable] の下限と上限を返す。初回 O(V^3)、構築後 O(1)
    bounds_result value_bounds(int variable) {
        assert(0 <= variable && variable < n);
        if (!solve_internal()) return {};
        bounds_result result;
        result.feasible = true;
        const Calc upper = distance[index(zero_vertex, variable)];
        const Calc reverse = distance[index(variable, zero_vertex)];
        if (reverse != infinity()) result.lower = -reverse;
        if (upper != infinity()) result.upper = upper;
        return result;
    }

    // x[to] - x[from] の最大値と、それを達成する解を返す。初回 O(V^3)、構築後 O(V^2)
    solution_result maximum_difference_solution(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        return maximum_difference_solution_internal(from, to);
    }

    // x[to] - x[from] の最小値と、それを達成する解を返す。初回 O(V^3)、構築後 O(V^2)
    solution_result minimum_difference_solution(int from, int to) {
        assert(0 <= from && from < n && 0 <= to && to < n);
        solution_result result = maximum_difference_solution_internal(to, from);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }

    // x[variable] の最大値と、それを達成する解を返す。初回 O(V^3)、構築後 O(V^2)
    solution_result maximum_value_solution(int variable) {
        assert(0 <= variable && variable < n);
        return maximum_difference_solution_internal(zero_vertex, variable);
    }

    // x[variable] の最小値と、それを達成する解を返す。初回 O(V^3)、構築後 O(V^2)
    solution_result minimum_value_solution(int variable) {
        assert(0 <= variable && variable < n);
        solution_result result = maximum_difference_solution_internal(variable, zero_vertex);
        if (result.state == status::finite) result.value = -result.value;
        return result;
    }
};

#if __INCLUDE_LEVEL__ == 0

struct DifferenceConstraintsTestRunner {
    std::size_t checks = 0;

    void expect(bool condition, const std::string& message) {
        ++checks;
        if (!condition) throw std::runtime_error(message);
    }
};

struct DifferenceConstraintsTestEdge {
    int from;
    int to;
    long long upper;
};

struct DifferenceConstraintsTestOperation {
    int type;
    int from;
    int to;
    long long lower;
    long long upper;
};

template <class Value>
bool difference_constraints_test_satisfies(
    const std::vector<Value>& values,
    int variable_count,
    const std::vector<DifferenceConstraintsTestEdge>& constraints) {
    for (const DifferenceConstraintsTestEdge& constraint : constraints) {
        const __int128_t from_value =
            constraint.from == variable_count ? __int128_t{} : values[constraint.from];
        const __int128_t to_value =
            constraint.to == variable_count ? __int128_t{} : values[constraint.to];
        if (to_value - from_value > constraint.upper) return false;
    }
    return true;
}

template <class ResultA, class ResultB>
void difference_constraints_test_expect_bound_equal(
    DifferenceConstraintsTestRunner& tester,
    const ResultA& actual,
    const ResultB& expected,
    const std::string& message) {
    tester.expect(static_cast<int>(actual.state) == static_cast<int>(expected.state),
                  message + ": status");
    if (static_cast<int>(expected.state) == 2) {
        tester.expect(actual.value == expected.value, message + ": value");
    }
}

template <class ResultA, class ResultB>
void difference_constraints_test_expect_bounds_equal(
    DifferenceConstraintsTestRunner& tester,
    const ResultA& actual,
    const ResultB& expected,
    const std::string& message) {
    tester.expect(actual.feasible == expected.feasible, message + ": feasible");
    if (!expected.feasible) return;
    tester.expect(actual.lower.has_value() == expected.lower.has_value(), message + ": lower state");
    tester.expect(actual.upper.has_value() == expected.upper.has_value(), message + ": upper state");
    if (expected.lower.has_value()) {
        tester.expect(*actual.lower == *expected.lower, message + ": lower value");
    }
    if (expected.upper.has_value()) {
        tester.expect(*actual.upper == *expected.upper, message + ": upper value");
    }
}

template <class DC>
void difference_constraints_test_apply_operation(
    DC& constraints,
    const DifferenceConstraintsTestOperation& operation) {
    switch (operation.type) {
    case 0:
        constraints.add_upper_bound(operation.from, operation.to, operation.upper);
        break;
    case 1:
        constraints.add_lower_bound(operation.from, operation.to, operation.lower);
        break;
    case 2:
        constraints.add_bounds(
            operation.from, operation.to, operation.lower, operation.upper);
        break;
    case 3:
        constraints.add_equal(operation.from, operation.to, operation.lower);
        break;
    case 4:
        constraints.add_abs_upper_bound(operation.from, operation.to, operation.upper);
        break;
    case 5:
        constraints.add_upper_bound(operation.from, operation.upper);
        break;
    case 6:
        constraints.add_lower_bound(operation.from, operation.lower);
        break;
    case 7:
        constraints.add_bounds(operation.from, operation.lower, operation.upper);
        break;
    default:
        constraints.add_equal(operation.from, operation.lower);
        break;
    }
}

void difference_constraints_test_record_operation(
    std::vector<DifferenceConstraintsTestEdge>& constraints,
    int variable_count,
    const DifferenceConstraintsTestOperation& operation) {
    switch (operation.type) {
    case 0:
        constraints.push_back({operation.from, operation.to, operation.upper});
        break;
    case 1:
        constraints.push_back({operation.to, operation.from, -operation.lower});
        break;
    case 2:
        constraints.push_back({operation.from, operation.to, operation.upper});
        constraints.push_back({operation.to, operation.from, -operation.lower});
        break;
    case 3:
        constraints.push_back({operation.from, operation.to, operation.lower});
        constraints.push_back({operation.to, operation.from, -operation.lower});
        break;
    case 4:
        constraints.push_back({operation.from, operation.to, operation.upper});
        constraints.push_back({operation.to, operation.from, operation.upper});
        break;
    case 5:
        constraints.push_back({variable_count, operation.from, operation.upper});
        break;
    case 6:
        constraints.push_back({operation.from, variable_count, -operation.lower});
        break;
    case 7:
        constraints.push_back({variable_count, operation.from, operation.upper});
        constraints.push_back({operation.from, variable_count, -operation.lower});
        break;
    default:
        constraints.push_back({variable_count, operation.from, operation.lower});
        constraints.push_back({operation.from, variable_count, -operation.lower});
        break;
    }
}

template <class DC>
void difference_constraints_test_fixed(DifferenceConstraintsTestRunner& tester,
                                       const std::string& implementation_name) {
    {
        DC constraints(2);
        constraints.reserve_edges(8);
        tester.expect(constraints.size() == 2, implementation_name + ": size");
        tester.expect(constraints.feasible(), implementation_name + ": empty feasible");
        const auto assignment = constraints.feasible_assignment();
        tester.expect(assignment.has_value() && assignment->size() == 2,
                      implementation_name + ": empty assignment");
        tester.expect(static_cast<int>(constraints.maximum_difference(0, 1).state) == 1,
                      implementation_name + ": empty maximum unbounded");
        tester.expect(static_cast<int>(constraints.minimum_difference(0, 1).state) == 1,
                      implementation_name + ": empty minimum unbounded");
        const auto self_maximum = constraints.maximum_difference(0, 0);
        tester.expect(static_cast<int>(self_maximum.state) == 2 && self_maximum.value == 0,
                      implementation_name + ": self difference");
        const auto bounds = constraints.difference_bounds(0, 1);
        tester.expect(bounds.feasible && !bounds.lower.has_value() && !bounds.upper.has_value(),
                      implementation_name + ": empty bounds");
    }
    {
        DC constraints(3);
        constraints.add_bounds(0, 1, 2, 5);
        constraints.add_equal(1, 2, 3);
        constraints.add_equal(0, 1);
        const std::vector<DifferenceConstraintsTestEdge> recorded = {
            {0, 1, 5}, {1, 0, -2}, {1, 2, 3}, {2, 1, -3},
            {3, 0, 1}, {0, 3, -1},
        };
        tester.expect(constraints.feasible(), implementation_name + ": bounded feasible");
        const auto assignment = constraints.feasible_assignment();
        tester.expect(assignment.has_value() &&
                          difference_constraints_test_satisfies(*assignment, 3, recorded),
                      implementation_name + ": bounded feasible assignment");

        const auto maximum = constraints.maximum_difference(0, 2);
        tester.expect(static_cast<int>(maximum.state) == 2 && maximum.value == 8,
                      implementation_name + ": bounded maximum");
        const auto minimum = constraints.minimum_difference(0, 2);
        tester.expect(static_cast<int>(minimum.state) == 2 && minimum.value == 5,
                      implementation_name + ": bounded minimum");
        const auto range = constraints.difference_bounds(0, 2);
        tester.expect(range.feasible && range.lower == 5 && range.upper == 8,
                      implementation_name + ": bounded range");
        const auto value_range = constraints.value_bounds(2);
        tester.expect(value_range.feasible && value_range.lower == 6 && value_range.upper == 9,
                      implementation_name + ": value range");

        const auto maximum_solution = constraints.maximum_difference_solution(0, 2);
        tester.expect(static_cast<int>(maximum_solution.state) == 2 &&
                          maximum_solution.value == 8 &&
                          maximum_solution.values[2] - maximum_solution.values[0] == 8 &&
                          difference_constraints_test_satisfies(
                              maximum_solution.values, 3, recorded),
                      implementation_name + ": maximum solution");
        const auto minimum_solution = constraints.minimum_difference_solution(0, 2);
        tester.expect(static_cast<int>(minimum_solution.state) == 2 &&
                          minimum_solution.value == 5 &&
                          minimum_solution.values[2] - minimum_solution.values[0] == 5 &&
                          difference_constraints_test_satisfies(
                              minimum_solution.values, 3, recorded),
                      implementation_name + ": minimum solution");
        const auto maximum_value_solution = constraints.maximum_value_solution(2);
        tester.expect(static_cast<int>(maximum_value_solution.state) == 2 &&
                          maximum_value_solution.value == 9 &&
                          maximum_value_solution.values[2] == 9,
                      implementation_name + ": maximum value solution");
        const auto minimum_value_solution = constraints.minimum_value_solution(2);
        tester.expect(static_cast<int>(minimum_value_solution.state) == 2 &&
                          minimum_value_solution.value == 6 &&
                          minimum_value_solution.values[2] == 6,
                      implementation_name + ": minimum value solution");
    }
    {
        DC constraints(2);
        constraints.add_equal(0, 0);
        constraints.add_abs_upper_bound(0, 1, 4);
        tester.expect(constraints.maximum_difference(0, 1).value == 4,
                      implementation_name + ": absolute maximum");
        tester.expect(constraints.minimum_difference(0, 1).value == -4,
                      implementation_name + ": absolute minimum");
    }
    {
        DC constraints(4);
        constraints.add_upper_bound(2, 3, -1);
        constraints.add_upper_bound(3, 2, 0);
        tester.expect(!constraints.feasible(), implementation_name + ": disconnected negative cycle");
        tester.expect(static_cast<int>(constraints.maximum_difference(0, 1).state) == 0,
                      implementation_name + ": infeasible query");
        tester.expect(!constraints.feasible_assignment().has_value(),
                      implementation_name + ": infeasible assignment");
    }
    {
        DC constraints(1);
        constraints.add_upper_bound(0, 0, -1);
        tester.expect(!constraints.feasible(), implementation_name + ": negative self loop");
    }
    {
        DC constraints(1);
        constraints.add_bounds(0, 5, 4);
        tester.expect(!constraints.feasible(), implementation_name + ": contradictory unary bounds");
    }
    {
        DC constraints(2);
        tester.expect(static_cast<int>(constraints.maximum_difference(0, 1).state) == 1,
                      implementation_name + ": before adding bound");
        constraints.add_upper_bound(0, 1, 7);
        tester.expect(constraints.maximum_difference(0, 1).value == 7,
                      implementation_name + ": add after solve");
        constraints.add_lower_bound(0, 1, 3);
        const auto range = constraints.difference_bounds(0, 1);
        tester.expect(range.lower == 3 && range.upper == 7,
                      implementation_name + ": add lower after solve");
    }
    {
        DC constraints(0);
        tester.expect(constraints.feasible(), implementation_name + ": zero variables feasible");
        const auto assignment = constraints.feasible_assignment();
        tester.expect(assignment.has_value() && assignment->empty(),
                      implementation_name + ": zero variables assignment");
    }
}

template <class DC>
void difference_constraints_test_against_oracle(
    DifferenceConstraintsTestRunner& tester,
    DC& actual,
    difference_constraints_floyd_warshall<long long>& oracle,
    int variable_count,
    const std::vector<DifferenceConstraintsTestEdge>& recorded,
    int selected_from,
    int selected_to,
    const std::string& implementation_name) {
    const bool expected_feasible = oracle.feasible();
    tester.expect(actual.feasible() == expected_feasible, implementation_name + ": random feasible");
    if (!expected_feasible) {
        tester.expect(static_cast<int>(actual.maximum_difference(0, 0).state) == 0,
                      implementation_name + ": random infeasible query");
        return;
    }

    const auto assignment = actual.feasible_assignment();
    tester.expect(assignment.has_value() &&
                      difference_constraints_test_satisfies(
                          *assignment, variable_count, recorded),
                  implementation_name + ": random assignment");
    for (int from = 0; from < variable_count; ++from) {
        for (int to = 0; to < variable_count; ++to) {
            difference_constraints_test_expect_bound_equal(
                tester,
                actual.maximum_difference(from, to),
                oracle.maximum_difference(from, to),
                implementation_name + ": random maximum");
            difference_constraints_test_expect_bound_equal(
                tester,
                actual.minimum_difference(from, to),
                oracle.minimum_difference(from, to),
                implementation_name + ": random minimum");
            difference_constraints_test_expect_bounds_equal(
                tester,
                actual.difference_bounds(from, to),
                oracle.difference_bounds(from, to),
                implementation_name + ": random bounds");
        }
    }
    for (int variable = 0; variable < variable_count; ++variable) {
        difference_constraints_test_expect_bound_equal(
            tester,
            actual.maximum_value(variable),
            oracle.maximum_value(variable),
            implementation_name + ": random maximum value");
        difference_constraints_test_expect_bound_equal(
            tester,
            actual.minimum_value(variable),
            oracle.minimum_value(variable),
            implementation_name + ": random minimum value");
        difference_constraints_test_expect_bounds_equal(
            tester,
            actual.value_bounds(variable),
            oracle.value_bounds(variable),
            implementation_name + ": random value bounds");
    }

    const auto maximum_solution = actual.maximum_difference_solution(selected_from, selected_to);
    const auto expected_maximum = oracle.maximum_difference(selected_from, selected_to);
    difference_constraints_test_expect_bound_equal(
        tester, maximum_solution, expected_maximum, implementation_name + ": random max solution result");
    if (static_cast<int>(maximum_solution.state) == 2) {
        tester.expect(difference_constraints_test_satisfies(
                          maximum_solution.values, variable_count, recorded) &&
                          maximum_solution.values[selected_to] -
                                  maximum_solution.values[selected_from] ==
                              maximum_solution.value,
                      implementation_name + ": random max solution values");
    } else {
        tester.expect(maximum_solution.values.empty(),
                      implementation_name + ": random max solution empty");
    }

    const auto minimum_solution = actual.minimum_difference_solution(selected_from, selected_to);
    const auto expected_minimum = oracle.minimum_difference(selected_from, selected_to);
    difference_constraints_test_expect_bound_equal(
        tester, minimum_solution, expected_minimum, implementation_name + ": random min solution result");
    if (static_cast<int>(minimum_solution.state) == 2) {
        tester.expect(difference_constraints_test_satisfies(
                          minimum_solution.values, variable_count, recorded) &&
                          minimum_solution.values[selected_to] -
                                  minimum_solution.values[selected_from] ==
                              minimum_solution.value,
                      implementation_name + ": random min solution values");
    } else {
        tester.expect(minimum_solution.values.empty(),
                      implementation_name + ": random min solution empty");
    }
}

void difference_constraints_test_random(DifferenceConstraintsTestRunner& tester) {
    std::mt19937_64 random_engine(0x6a09e667f3bcc909ULL);
    auto random_int = [&random_engine](int lower, int upper) {
        const std::uint64_t width = upper - lower + 1;
        return lower + static_cast<int>(random_engine() % width);
    };

    constexpr int test_cases = 1200;
    for (int test_case = 0; test_case < test_cases; ++test_case) {
        const int variable_count = random_int(1, 7);
        const int operation_count = random_int(0, 22);
        difference_constraints_bellman_ford<long long> basic(variable_count);
        difference_constraints_scc_bellman_ford<long long> scc(variable_count);
        difference_constraints_spfa<long long> spfa(variable_count);
        difference_constraints_floyd_warshall<long long> floyd(variable_count);
        std::vector<DifferenceConstraintsTestEdge> recorded;

        for (int operation_index = 0; operation_index < operation_count; ++operation_index) {
            const int type = random_int(0, 8);
            const int from = random_int(0, variable_count - 1);
            const int to = random_int(0, variable_count - 1);
            long long lower = random_int(-8, 8);
            long long upper = random_int(-8, 8);
            if (lower > upper) std::swap(lower, upper);
            if (type == 4) {
                lower = 0;
                upper = random_int(0, 8);
            } else if (type == 3 || type == 8) {
                upper = lower;
            }
            const DifferenceConstraintsTestOperation operation = {
                type, from, to, lower, upper,
            };
            difference_constraints_test_apply_operation(basic, operation);
            difference_constraints_test_apply_operation(scc, operation);
            difference_constraints_test_apply_operation(spfa, operation);
            difference_constraints_test_apply_operation(floyd, operation);
            difference_constraints_test_record_operation(recorded, variable_count, operation);
        }

        const int selected_from = random_int(0, variable_count - 1);
        const int selected_to = random_int(0, variable_count - 1);
        const bool floyd_feasible = floyd.feasible();
        if (floyd_feasible) {
            const auto floyd_assignment = floyd.feasible_assignment();
            tester.expect(floyd_assignment.has_value() &&
                              difference_constraints_test_satisfies(
                                  *floyd_assignment, variable_count, recorded),
                          "floyd: random assignment");
            const auto floyd_solution =
                floyd.maximum_difference_solution(selected_from, selected_to);
            if (static_cast<int>(floyd_solution.state) == 2) {
                tester.expect(difference_constraints_test_satisfies(
                                  floyd_solution.values, variable_count, recorded) &&
                                  floyd_solution.values[selected_to] -
                                          floyd_solution.values[
                                              selected_from] ==
                                      floyd_solution.value,
                              "floyd: random maximum solution");
            }
        }

        difference_constraints_test_against_oracle(
            tester, basic, floyd, variable_count, recorded, selected_from, selected_to, "basic");
        difference_constraints_test_against_oracle(
            tester, scc, floyd, variable_count, recorded, selected_from, selected_to, "scc");
        difference_constraints_test_against_oracle(
            tester, spfa, floyd, variable_count, recorded, selected_from, selected_to, "spfa");
    }
    std::cout << "random differential tests: " << test_cases << " cases passed\n";
}

void difference_constraints_test_random_feasible(DifferenceConstraintsTestRunner& tester) {
    std::mt19937_64 random_engine(0x3c6ef372fe94f82bULL);
    auto random_int = [&random_engine](int lower, int upper) {
        const std::uint64_t width = upper - lower + 1;
        return lower + static_cast<int>(random_engine() % width);
    };

    constexpr int test_cases = 600;
    for (int test_case = 0; test_case < test_cases; ++test_case) {
        const int variable_count = random_int(2, 10);
        const int zero_vertex = variable_count;
        std::vector<long long> known_values(variable_count + 1);
        for (int v = 0; v < variable_count; ++v) {
            known_values[v] = random_int(-20, 20);
        }

        difference_constraints_bellman_ford<long long> basic(variable_count);
        difference_constraints_scc_bellman_ford<long long> scc(variable_count);
        difference_constraints_spfa<long long> spfa(variable_count);
        difference_constraints_floyd_warshall<long long> floyd(variable_count);
        std::vector<DifferenceConstraintsTestEdge> recorded;

        auto add_feasible_edge = [&](int from, int to, long long slack) {
            const long long upper = known_values[to] -
                                    known_values[from] + slack;
            if (from == zero_vertex && to == zero_vertex) {
                recorded.push_back({from, to, upper});
                return;
            }
            if (from == zero_vertex) {
                basic.add_upper_bound(to, upper);
                scc.add_upper_bound(to, upper);
                spfa.add_upper_bound(to, upper);
                floyd.add_upper_bound(to, upper);
            } else if (to == zero_vertex) {
                basic.add_lower_bound(from, -upper);
                scc.add_lower_bound(from, -upper);
                spfa.add_lower_bound(from, -upper);
                floyd.add_lower_bound(from, -upper);
            } else {
                basic.add_upper_bound(from, to, upper);
                scc.add_upper_bound(from, to, upper);
                spfa.add_upper_bound(from, to, upper);
                floyd.add_upper_bound(from, to, upper);
            }
            recorded.push_back({from, to, upper});
        };

        // ユーザー変数を1つのSCCにし、さらにZEROとも双方向に接続する
        for (int v = 0; v < variable_count; ++v) {
            const int next = (v + 1) % variable_count;
            add_feasible_edge(v, next, random_int(0, 4));
            add_feasible_edge(next, v, random_int(0, 4));
        }
        add_feasible_edge(zero_vertex, 0, random_int(0, 4));
        add_feasible_edge(0, zero_vertex, random_int(0, 4));

        const int extra_edges = random_int(10, 45);
        for (int edge_index = 0; edge_index < extra_edges; ++edge_index) {
            const int from = random_int(0, variable_count);
            const int to = random_int(0, variable_count);
            add_feasible_edge(from, to, random_int(0, 8));
        }

        const int selected_from = random_int(0, variable_count - 1);
        const int selected_to = random_int(0, variable_count - 1);
        tester.expect(floyd.feasible(), "feasible random: oracle feasible");
        difference_constraints_test_against_oracle(
            tester, basic, floyd, variable_count, recorded, selected_from, selected_to,
            "basic feasible random");
        difference_constraints_test_against_oracle(
            tester, scc, floyd, variable_count, recorded, selected_from, selected_to,
            "scc feasible random");
        difference_constraints_test_against_oracle(
            tester, spfa, floyd, variable_count, recorded, selected_from, selected_to,
            "spfa feasible random");
    }
    std::cout << "known-feasible random tests: " << test_cases << " cases passed\n";
}

void difference_constraints_test_floyd_incremental(DifferenceConstraintsTestRunner& tester) {
    std::mt19937_64 random_engine(0xbb67ae8584caa73bULL);
    auto random_int = [&random_engine](int lower, int upper) {
        const std::uint64_t width = upper - lower + 1;
        return lower + static_cast<int>(random_engine() % width);
    };

    constexpr int test_cases = 250;
    constexpr int additions = 18;
    for (int test_case = 0; test_case < test_cases; ++test_case) {
        const int variable_count = random_int(1, 7);
        difference_constraints_floyd_warshall<long long> incremental(variable_count);
        std::vector<DifferenceConstraintsTestEdge> recorded;

        tester.expect(incremental.feasible(), "incremental floyd: initially feasible");
        for (int addition = 0; addition < additions; ++addition) {
            const int from = random_int(0, variable_count - 1);
            const int to = random_int(0, variable_count - 1);
            const long long upper = random_int(-10, 10);
            incremental.add_upper_bound(from, to, upper);
            recorded.push_back({from, to, upper});

            difference_constraints_floyd_warshall<long long> rebuilt(variable_count);
            for (const DifferenceConstraintsTestEdge& edge : recorded) {
                rebuilt.add_upper_bound(edge.from, edge.to, edge.upper);
            }
            const bool expected_feasible = rebuilt.feasible();
            tester.expect(incremental.feasible() == expected_feasible,
                          "incremental floyd: feasible");
            if (!expected_feasible) continue;
            for (int source = 0; source < variable_count; ++source) {
                for (int target = 0; target < variable_count; ++target) {
                    difference_constraints_test_expect_bound_equal(
                        tester,
                        incremental.maximum_difference(source, target),
                        rebuilt.maximum_difference(source, target),
                        "incremental floyd: maximum");
                }
            }
            const auto assignment = incremental.feasible_assignment();
            tester.expect(assignment.has_value() &&
                              difference_constraints_test_satisfies(
                                  *assignment, variable_count, recorded),
                          "incremental floyd: assignment");
        }
    }
    std::cout << "incremental Floyd-Warshall tests: " << test_cases
              << " cases passed\n";
}

template <class DC>
void difference_constraints_test_int128_one(DifferenceConstraintsTestRunner& tester,
                                            const std::string& implementation_name) {
    constexpr long long edge_weight = 3'000'000'000'000'000'000LL;
    DC constraints(5);
    for (int v = 0; v < 4; ++v) constraints.add_upper_bound(v, v + 1, edge_weight);
    const auto result = constraints.maximum_difference(0, 4);
    const __int128_t expected = static_cast<__int128_t>(edge_weight) * 4;
    tester.expect(static_cast<int>(result.state) == 2 && result.value == expected,
                  implementation_name + ": __int128 path sum");
}

template <class DC, class Builder>
double difference_constraints_benchmark(int variable_count,
                                        int repetitions,
                                        Builder&& builder) {
    std::vector<double> elapsed_times;
    elapsed_times.reserve(repetitions);
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        DC constraints(variable_count);
        builder(constraints);
        const auto start = std::chrono::steady_clock::now();
        if (!constraints.feasible()) throw std::runtime_error("benchmark graph is infeasible");
        const auto finish = std::chrono::steady_clock::now();
        const std::chrono::duration<double, std::milli> elapsed = finish - start;
        elapsed_times.push_back(elapsed.count());
    }
    std::sort(elapsed_times.begin(), elapsed_times.end());
    return elapsed_times[repetitions / 2];
}

void difference_constraints_run_benchmarks() {
    constexpr int repetitions = 5;
    constexpr int dag_vertices = 5000;
    auto build_adverse_dag = [](auto& constraints) {
        constraints.reserve_edges(dag_vertices);
        for (int v = dag_vertices - 2; v >= 1; --v) {
            constraints.add_upper_bound(v, v + 1, 0LL);
        }
        constraints.add_upper_bound(0, 1, -1LL);
    };

    const double dag_basic =
        difference_constraints_benchmark<difference_constraints_bellman_ford<long long>>(
            dag_vertices, repetitions, build_adverse_dag);
    const double dag_scc =
        difference_constraints_benchmark<difference_constraints_scc_bellman_ford<long long>>(
            dag_vertices, repetitions, build_adverse_dag);
    const double dag_spfa =
        difference_constraints_benchmark<difference_constraints_spfa<long long>>(
            dag_vertices, repetitions, build_adverse_dag);

    constexpr int scc_vertices = 30000;
    auto build_one_scc = [](auto& constraints) {
        constraints.reserve_edges(2 * scc_vertices);
        constraints.add_upper_bound(0, 1, -1LL);
        for (int v = 1; v + 1 < scc_vertices; ++v) {
            constraints.add_upper_bound(v, v + 1, 0LL);
        }
        constraints.add_upper_bound(1, 0, 1LL);
        for (int v = 1; v + 1 < scc_vertices; ++v) {
            constraints.add_upper_bound(v + 1, v, 0LL);
        }
    };

    const double one_scc_basic =
        difference_constraints_benchmark<difference_constraints_bellman_ford<long long>>(
            scc_vertices, repetitions, build_one_scc);
    const double one_scc_scc =
        difference_constraints_benchmark<difference_constraints_scc_bellman_ford<long long>>(
            scc_vertices, repetitions, build_one_scc);
    const double one_scc_spfa =
        difference_constraints_benchmark<difference_constraints_spfa<long long>>(
            scc_vertices, repetitions, build_one_scc);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "benchmark median of " << repetitions << " runs (ms)\n";
    std::cout << "  adverse-order DAG, V=" << dag_vertices << ": basic=" << dag_basic
              << ", SCC=" << dag_scc << ", SPFA=" << dag_spfa << '\n';
    std::cout << "  one large SCC, V=" << scc_vertices << ": basic=" << one_scc_basic
              << ", SCC=" << one_scc_scc << ", SPFA=" << one_scc_spfa << '\n';
}

int main() {
    try {
        DifferenceConstraintsTestRunner tester;
        difference_constraints_test_fixed<difference_constraints_bellman_ford<long long>>(
            tester, "basic");
        difference_constraints_test_fixed<difference_constraints_scc_bellman_ford<long long>>(
            tester, "scc");
        difference_constraints_test_fixed<difference_constraints_spfa<long long>>(
            tester, "spfa");
        difference_constraints_test_fixed<difference_constraints_floyd_warshall<long long>>(
            tester, "floyd");
        std::cout << "fixed tests: 4 implementations passed\n";

        difference_constraints_test_random(tester);
        difference_constraints_test_random_feasible(tester);
        difference_constraints_test_floyd_incremental(tester);

        difference_constraints_test_int128_one<
            difference_constraints_bellman_ford<long long, __int128_t>>(tester, "basic");
        difference_constraints_test_int128_one<
            difference_constraints_scc_bellman_ford<long long, __int128_t>>(tester, "scc");
        difference_constraints_test_int128_one<
            difference_constraints_spfa<long long, __int128_t>>(tester, "spfa");
        difference_constraints_test_int128_one<
            difference_constraints_floyd_warshall<long long, __int128_t>>(tester, "floyd");
        std::cout << "__int128 calculation tests: 4 implementations passed\n";

        std::cout << "total assertions: " << tester.checks << '\n';
        if (std::getenv("DIFFERENCE_CONSTRAINTS_SKIP_BENCHMARKS") == nullptr) {
            difference_constraints_run_benchmarks();
        }
        std::cout << "ALL TESTS PASSED\n";
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILED: " << error.what() << '\n';
        return 1;
    }
    return 0;
}

#endif
