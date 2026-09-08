/*
 * SPFA: 負辺を含む重み付き有向グラフの最短路を、更新が起きた頂点だけ再走査して求めるライブラリ
 *
 * 主な用途:
 *   1. 単一始点・多始点最短路
 *   2. 到達可能な負閉路の検出と復元
 *   3. 到達不能・有限距離・負閉路から到達可能な -INF 頂点の分類
 *   4. 差分制約 x[left] - x[right] <= upper の充足判定と実行可能解の構築
 *   5. Johnson 法や最小費用流で使う実行可能ポテンシャルの構築
 *   6. AHC で同じグラフを繰り返し解く、途中停止・再開する、辺追加・重み減少を差分伝播する
 *   7. 遷移をその場で列挙する暗黙グラフ、std::unordered_map で状態を管理する疎な暗黙グラフ
 *   8. CSR に固定した大規模グラフで、異なる始点から最短路を繰り返し計算する
 *
 * 選択指針:
 *   - 全辺が非負なら通常は Dijkstra 法、重みが 0/1 なら 0-1 BFS、DAG ならトポロジカル順 DP を優先する
 *   - SPFA は負辺がある疎グラフ、または少数頂点からの差分伝播が多い AHC で特に使いやすい
 *   - 既定の FIFO はランダムな固定グラフで SLF より高速だった候補順序で、最悪 O(VE) の上界も持つ
 *   - SLF が有利な入力もあるため、既知の入力分布で速い場合は spfa_queue_policy::slf を明示する
 *   - 固定グラフでSLFを繰り返す場合は、全ての辺重みを確定した後に
 *     graph.sort_edges_by_cost_for_slf() を1回呼ぶと、同じ始点からの距離候補が小さい順に並びやすい
 *     並べ替え自体は O(E + Σ deg(v) log deg(v)) なので、反復回数が少ない場合は実測して選ぶ
 *   - 辺追加がない固定グラフを繰り返し解く場合は、連続走査できる spfa_csr_graph を優先する
 *   - spfa_graph は構築途中で add_edge する場合、辺追加を含む差分更新、1回だけの計算に向く
 *   - spfa_csr_graph は全ての辺を確定した後、同じトポロジーを始点や重みを変えて何度も解く場合に向く
 *     全出辺が1本の配列に並ぶため、特に辺数が多く辺走査が支配的な反復計算でキャッシュ効率がよい
 *   - CSRへの変換は O(V+E) の時間と追加メモリを使うため、1回だけの計算や小さいグラフでは
 *     構築費を回収できないことがある。迷う場合は spfa_graph を使い、反復計算がボトルネックならCSRを試す
 *   - spfa_csr_graph は辺追加・削除はできないが、set_cost による既存辺の重み変更はできる
 *   - 負辺を双方向に張った無向グラフには、その辺を往復する負閉路が存在する
 *
 * 重要な仕様:
 *   - 頂点番号は 0 以上 n 未満、辺数・頂点数は int に収まる前提とし、範囲検査は行わない
 *   - Cost は辺重みと距離の両方に使う。全ての計算途中の有限な経路和が Cost に収まる必要がある
 *   - inf は有限な経路長より十分大きい値を指定する。オーバーフロー検査は行わない
 *   - SPFA では target がキューから出た時点の距離は確定しない。正確な結果にはキューを空にする必要がある
 *   - paused の距離は実在する発見済み経路の長さだが、最短性は保証されない
 *   - 差分更新が正確なのは辺追加・辺重み減少・始点ラベル減少だけ。辺削除・重み増加は全再計算する
 *   - run_unchecked 後は unchecked 名の notify/propagate だけを使い、安全版へ戻す場合は run で再計算する
 *
 * 基本例:
 *   spfa_graph<long long> graph(n);
 *   graph.add_edge(0, 1, 5);
 *   graph.add_edge(1, 2, -3);
 *   auto result = spfa_shortest_paths(graph, 0);  // 既定は FIFO
 *   if (result.status == spfa_status::completed) {
 *       long long distance = result.dist[2];
 *       std::vector<int> path = result.restore_vertex_path(2);
 *   }
 *
 * 固定グラフを繰り返し解く例:
 *   // 1. まず spfa_graph で全ての辺を追加する。この後は辺を追加・削除しない
 *   spfa_graph<long long> building_graph(n);
 *   building_graph.add_edge(0, 1, 5);
 *   building_graph.add_edge(1, 2, -3);
 *   // ... 残りの辺も追加する
 *
 *   // 2. O(V+E) でCSRへ変換する。辺IDは building_graph と同じ値に保たれる
 *   spfa_csr_graph<long long> fixed_graph(building_graph);
 *   // 最初から辺リストを持つ場合は、次のように直接構築でき、input_edges の添字が辺IDになる
 *   // using fixed_graph_type = spfa_csr_graph<long long>;
 *   // std::vector<fixed_graph_type::input_edge> input_edges{{0, 1, 5}, {1, 2, -3}};
 *   // fixed_graph_type fixed_graph(n, input_edges);
 *
 *   // 3. 距離だけ必要なら StoreParent=false とし、同じWorkspaceの配列を使い回す。以下はFIFO版
 *   spfa_workspace<long long, spfa_queue_policy::fifo, false> workspace(n);
 *   // SLFを実測で選ぶ場合は、上の1行を次の2行に置き換える
 *   // fixed_graph.sort_edges_by_cost_for_slf();  // 全ての重みを確定した後に1回
 *   // spfa_workspace<long long, spfa_queue_policy::slf, false> workspace(n);
 *   for (int source : sources) {
 *       // 負閉路がないと保証できる場合は run_unchecked、保証できない場合は run を使う
 *       workspace.run_unchecked(fixed_graph, source);
 *       long long distance = workspace.distance(target);
 *       // distance を評価値などに使う
 *   }
 *   // 経路復元も必要なら StoreParent=true にする。既存辺の重みだけなら fixed_graph.set_cost で変更できる
 *
 * AHC の差分更新例:
 *   // 1. 変更前のグラフについて一度だけ正確な距離を求める
 *   spfa_workspace<long long, spfa_queue_policy::fifo, false> workspace(n);
 *   workspace.run(graph, source);
 *
 *   // 2-a. 既存辺の重みを減らす。必ず graph を先に変更してから notify する
 *   long long old_cost = graph.set_cost(edge_id, smaller_cost);  // smaller_cost <= old_cost
 *   if (workspace.notify_edge_added_or_decreased(graph, edge_id)) {
 *       // 改善した頂点から先だけを再走査する。局所的な変更なら全再計算より速い
 *       spfa_status update_status = workspace.propagate(graph);
 *       // accept_candidate() は呼び出し側の評価処理を表す
 *       if (update_status == spfa_status::negative_cycle || !accept_candidate()) {
 *           // 重みを戻すだけでは距離は戻らないので、グラフを戻した後に全再計算する
 *           graph.set_cost(edge_id, old_cost);
 *           workspace.run(graph, source);
 *       }
 *   }
 *   // spfa_csr_graph でも set_cost と notify による重み減少は同じ手順で行える
 *
 *   // 2-b. 辺追加も同様。追加直後の辺IDをnotifyへ渡す
 *   int added_edge_id = graph.add_edge(from, to, cost);
 *   workspace.notify_edge_added_or_decreased(graph, added_edge_id);
 *   workspace.propagate(graph);
 *
 *   // 2-c. 仮想始点から vertex への初期距離を減らす場合
 *   workspace.notify_source_decreased(vertex, smaller_initial_distance);
 *   workspace.propagate(graph);
 *
 *   // 複数の辺を一度に変更する場合は、各辺を変更して notify した後、最後に1回だけ propagate してよい
 *   // 辺削除・重み増加では距離が大きくなる可能性があり差分伝播できないため、run で全再計算する
 *   // 頻繁に候補を棄却する場合は、変更前にWorkspaceをコピーし、棄却時にコピーから復元する方法もある
 *   // ただしWorkspaceのコピーは O(V) なので、全再計算・コピー・変更履歴による自前ロールバックを実測比較する
 */
#pragma once

#include <bits/stdc++.h>

enum class spfa_status : std::uint8_t {
    completed,
    negative_cycle,
    paused,
    state_limit_reached,
};

enum class spfa_queue_policy : std::uint8_t {
    fifo,
    slf,
};

template <class Cost>
struct spfa_seed {
    int vertex;
    Cost distance;
};

template <class Cost>
constexpr Cost spfa_default_inf() {
    return std::numeric_limits<Cost>::max() / Cost{4};
}

/*
 * 辺ID付きの可変隣接リスト
 * 出辺を頂点ごとの vector に直接格納するため走査時の局所性が高く、辺IDから重みをO(1)で変更できる
 */
template <class Cost>
struct spfa_graph {
    using cost_type = Cost;

    struct edge {
        int to;
        int id;
        Cost cost;

        edge(int to_, Cost cost_, int id_) : to(to_), id(id_), cost(cost_) {}
    };

    struct edge_info {
        int from;
        int to;
        Cost cost;
        int id;
    };

private:
    std::vector<std::vector<edge>> graph_;
    std::vector<std::pair<int, int>> edge_positions_;

public:
    // 頂点数0の空グラフを構築する。O(1)
    spfa_graph() = default;

    // 頂点数 n の辺なしグラフを構築する。O(n)
    explicit spfa_graph(int n) : graph_(n) {}

    // 頂点数を返す。O(1)
    int size() const { return static_cast<int>(graph_.size()); }

    // 辺数を返す。O(1)
    int edge_count() const { return static_cast<int>(edge_positions_.size()); }

    // 頂点 from の出辺領域を expected_count 本分確保する。再確保時 O(outdegree)、変更なしなら O(1)
    void reserve_out_edges(int from, int expected_count) {
        graph_[from].reserve(expected_count);
    }

    // 有向辺を追加して辺IDを返す。償却 O(1)
    int add_edge(int from, int to, Cost cost) {
        const int id = static_cast<int>(edge_positions_.size());
        auto& edges = graph_[from];
        const int index = static_cast<int>(edges.size());
        edge_positions_.push_back({from, index});
        edges.emplace_back(to, cost, id);
        return id;
    }

    // 同じ重みの双方向辺を追加して {from->to, to->from} の辺IDを返す。償却 O(1)
    std::pair<int, int> add_undirected_edge(int from, int to, Cost cost) {
        const int forward = add_edge(from, to, cost);
        const int backward = add_edge(to, from, cost);
        return {forward, backward};
    }

    // 頂点 vertex の出辺を現在の格納順で返す。通常は追加順、SLF向け整列後は重み順。O(1)
    const std::vector<edge>& edges_from(int vertex) const {
        return graph_[vertex];
    }

    // 頂点 vertex の出辺を現在の格納順で返す。通常は追加順、SLF向け整列後は重み順。O(1)
    const std::vector<edge>& operator[](int vertex) const { return edges_from(vertex); }

    // 辺IDに対応する始点・終点・重みを返す。O(1)
    edge_info get_edge(int edge_id) const {
        const auto [from, index] = edge_positions_[edge_id];
        const edge& e = graph_[from][index];
        return edge_info{from, e.to, e.cost, e.id};
    }

    // 辺IDに対応する重みを変更して変更前の重みを返す。O(1)
    Cost set_cost(int edge_id, Cost new_cost) {
        const auto [from, index] = edge_positions_[edge_id];
        edge& e = graph_[from][index];
        Cost old_cost = e.cost;
        e.cost = new_cost;
        return old_cost;
    }

    // SLF向けに各頂点の出辺を重み昇順へ並べ、辺IDを維持する。add_edge/set_cost後は再実行する。O(E + Σ deg(v) log deg(v))
    void sort_edges_by_cost_for_slf() {
        for (int from = 0; from < size(); ++from) {
            auto& edges = graph_[from];
            std::ranges::sort(edges, [](const edge& lhs, const edge& rhs) {
                if (lhs.cost < rhs.cost) return true;
                if (rhs.cost < lhs.cost) return false;
                return lhs.id < rhs.id;
            });
            for (int index = 0; index < static_cast<int>(edges.size()); ++index) {
                edge_positions_[edges[index].id] = {from, index};
            }
        }
    }
};

/*
 * トポロジー固定グラフ向けのCSR隣接リスト
 * 全出辺を1本の連続領域に置き、vector<vector<edge>> の各vectorをたどる処理をなくすことで、
 * 同じ大規模グラフを異なる始点や重みで繰り返し解く場合の辺走査をキャッシュ効率よく行う
 * 全辺が確定した後に構築する用途向けで、辺追加・削除はできないが、辺IDを保った重み変更には対応する
 * 1回だけの計算では O(V+E) の構築費が勝つ場合があるため、その場合は spfa_graph を使う
 */
template <class Cost>
struct spfa_csr_graph {
    using cost_type = Cost;

    struct input_edge {
        int from;
        int to;
        Cost cost;
    };

    struct edge {
        int to = 0;
        int id = 0;
        Cost cost{};

        edge() = default;
        edge(int to_, Cost cost_, int id_) : to(to_), id(id_), cost(cost_) {}
    };

    struct edge_info {
        int from;
        int to;
        Cost cost;
        int id;
    };

private:
    std::vector<int> start_;
    std::vector<edge> edges_;
    std::vector<int> edge_positions_;
    std::vector<int> edge_from_;

    void build(int n, const std::vector<input_edge>& input_edges) {
        start_.assign(n + 1, 0);
        const int edge_count = static_cast<int>(input_edges.size());

        // 各頂点の出次数を数え、累積和でCSR上の出辺区間を決定する
        for (const auto& input_value : input_edges) {
            ++start_[input_value.from + 1];
        }
        for (int vertex = 0; vertex < n; ++vertex) {
            start_[vertex + 1] +=
                start_[vertex];
        }

        // 入力添字を辺IDとして保ちつつ、始点ごとの連続領域へ辺を配置する
        edges_.resize(input_edges.size());
        edge_positions_.resize(input_edges.size());
        edge_from_.resize(input_edges.size());
        std::vector<int> next = start_;
        for (int id = 0; id < edge_count; ++id) {
            const auto& input_value = input_edges[id];
            const int position = next[input_value.from]++;
            edges_[position] =
                edge{input_value.to, input_value.cost, id};
            edge_positions_[id] = position;
            edge_from_[id] = input_value.from;
        }
    }

public:
    // 頂点数0の空グラフを構築する。O(1)
    spfa_csr_graph() = default;

    // 入力順を辺IDとして頂点数 n のCSRグラフを構築する。O(V+E)
    spfa_csr_graph(int n, const std::vector<input_edge>& input_edges) {
        build(n, input_edges);
    }

    // 可変隣接リストから辺IDを保ったCSRグラフを構築する。O(V+E)
    explicit spfa_csr_graph(const spfa_graph<Cost>& graph) {
        std::vector<input_edge> input_edges;
        input_edges.reserve(graph.edge_count());
        for (int id = 0; id < graph.edge_count(); ++id) {
            const auto source_edge = graph.get_edge(id);
            input_edges.push_back(
                input_edge{source_edge.from, source_edge.to, source_edge.cost});
        }
        build(graph.size(), input_edges);
    }

    // 頂点数を返す。O(1)
    int size() const {
        return start_.empty() ? 0 : static_cast<int>(start_.size()) - 1;
    }

    // 辺数を返す。O(1)
    int edge_count() const { return static_cast<int>(edges_.size()); }

    // 頂点 vertex の連続した出辺領域を返す。O(1)
    std::span<const edge> edges_from(int vertex) const {
        const int first = start_[vertex];
        const int count = start_[vertex + 1] - start_[vertex];
        return std::span<const edge>(edges_).subspan(first, count);
    }

    // 頂点 vertex の連続した出辺領域を返す。O(1)
    std::span<const edge> operator[](int vertex) const { return edges_from(vertex); }

    // 辺IDに対応する始点・終点・重みを返す。O(1)
    edge_info get_edge(int edge_id) const {
        const int position = edge_positions_[edge_id];
        const edge& source_edge = edges_[position];
        return edge_info{edge_from_[edge_id],
                         source_edge.to, source_edge.cost, edge_id};
    }

    // 辺IDに対応する重みを変更して変更前の重みを返す。O(1)
    Cost set_cost(int edge_id, Cost new_cost) {
        const int position = edge_positions_[edge_id];
        edge& target_edge = edges_[position];
        Cost old_cost = target_edge.cost;
        target_edge.cost = new_cost;
        return old_cost;
    }

    // SLF向けに各頂点の出辺を重み昇順へ並べ、辺IDを維持する。set_cost後は再実行する。O(E + Σ deg(v) log deg(v))
    void sort_edges_by_cost_for_slf() {
        for (int vertex = 0; vertex < size(); ++vertex) {
            std::sort(edges_.begin() + start_[vertex],
                      edges_.begin() + start_[vertex + 1],
                      [](const edge& lhs, const edge& rhs) {
                          if (lhs.cost < rhs.cost) return true;
                          if (rhs.cost < lhs.cost) return false;
                          return lhs.id < rhs.id;
                      });
        }
        for (int position = 0; position < edge_count(); ++position) {
            edge_positions_[edges_[position].id] = position;
        }
    }
};

/*
 * 所有権を持つSPFA実行結果
 * status が completed なら dist は正確で、negative_cycle なら負閉路列を保持する
 */
template <class Cost>
struct spfa_result {
    spfa_status status = spfa_status::completed;
    Cost inf = spfa_default_inf<Cost>();
    std::vector<Cost> dist;
    std::vector<int> parent_vertex;
    std::vector<int> parent_edge;
    std::vector<int> negative_cycle_vertices;
    std::vector<int> negative_cycle_edges;

    // vertex が発見済みなら true を返す。O(1)
    bool reachable(int vertex) const {
        return dist[vertex] != inf;
    }

    // 発見済み経路の頂点列を返し、未到達または親が閉路を作る場合は空列を返す。O(V)
    std::vector<int> restore_vertex_path(int target) const {
        if (!reachable(target) || parent_vertex.empty()) return {};
        const int n = static_cast<int>(dist.size());
        std::vector<int> path;
        int vertex = target;
        for (int step = 0; step <= n; ++step) {
            path.push_back(vertex);
            vertex = parent_vertex[vertex];
            if (vertex == -1) {
                std::reverse(path.begin(), path.end());
                return path;
            }
        }
        return {};
    }

    // 発見済み経路の辺ID列を返し、未到達または親が閉路を作る場合は空列を返す。O(V)
    std::vector<int> restore_edge_path(int target) const {
        if (!reachable(target) || parent_vertex.empty()) return {};
        const int n = static_cast<int>(dist.size());
        std::vector<int> path;
        int vertex = target;
        for (int step = 0; step <= n; ++step) {
            const int parent = parent_vertex[vertex];
            if (parent == -1) {
                std::reverse(path.begin(), path.end());
                return path;
            }
            path.push_back(parent_edge[vertex]);
            vertex = parent;
        }
        return {};
    }
};

/*
 * 配列とキューを再利用するSPFA実行器
 * ランダム固定グラフで速かった FIFO を既定とし、StoreParent=false では親配列を持たず反復計算を軽量化する
 */
template <class Cost, spfa_queue_policy QueuePolicy = spfa_queue_policy::fifo,
          bool StoreParent = true>
struct spfa_workspace {
private:
    struct ring_deque {
        std::vector<int> data;
        int head = 0;
        int tail = 0;
        int count = 0;

        explicit ring_deque(int capacity = 0)
            : data(capacity + 1) {}

        bool empty() const { return count == 0; }
        int front() const { return data[head]; }

        void clear() {
            head = 0;
            tail = 0;
            count = 0;
        }

        void push_front(int value) {
            const int capacity = static_cast<int>(data.size());
            head = head == 0 ? capacity - 1 : head - 1;
            data[head] = value;
            ++count;
        }

        void push_back(int value) {
            const int capacity = static_cast<int>(data.size());
            data[tail] = value;
            ++tail;
            if (tail == capacity) tail = 0;
            ++count;
        }

        int pop_front() {
            const int capacity = static_cast<int>(data.size());
            const int value = data[head];
            ++head;
            if (head == capacity) head = 0;
            --count;
            return value;
        }
    };

    int n_ = 0;
    Cost inf_ = spfa_default_inf<Cost>();
    std::vector<Cost> dist_;
    std::vector<int> parent_vertex_;
    std::vector<int> parent_edge_;
    std::vector<int> path_edge_count_;
    std::vector<unsigned char> in_queue_;
    std::vector<int> touched_vertices_;
    ring_deque queue_;
    spfa_status status_ = spfa_status::completed;
    int negative_cycle_trigger_ = -1;
    bool path_edge_counts_valid_ = true;

    void touch(int vertex) {
        if (dist_[vertex] != inf_) return;
        touched_vertices_.push_back(vertex);
    }

    void enqueue(int vertex) {
        if (in_queue_[vertex] != 0) return;
        if constexpr (QueuePolicy == spfa_queue_policy::slf) {
            if (!queue_.empty() &&
                dist_[vertex] <
                    dist_[queue_.front()]) {
                queue_.push_front(vertex);
            } else {
                queue_.push_back(vertex);
            }
        } else {
            queue_.push_back(vertex);
        }
        in_queue_[vertex] = 1;
    }

    void set_parent(int vertex, int parent, int edge_id) {
        if constexpr (StoreParent) {
            parent_vertex_[vertex] = parent;
            parent_edge_[vertex] = edge_id;
        }
    }

    bool relax_vertex(int from, int to, Cost candidate, int edge_id,
                      bool detect_negative_cycle) {
        if (!(candidate < dist_[to])) return false;
        touch(to);
        dist_[to] = candidate;
        set_parent(to, from, edge_id);

        if (detect_negative_cycle) {
            path_edge_count_[to] =
                path_edge_count_[from] + 1;
            if (path_edge_count_[to] >= n_) {
                status_ = spfa_status::negative_cycle;
                negative_cycle_trigger_ = to;
                return true;
            }
        }

        enqueue(to);
        status_ = spfa_status::paused;
        return true;
    }

    template <bool DetectNegativeCycle, bool CheckStop, bool CheckPopLimit,
              class Graph, class StopPredicate>
    spfa_status resume_impl(const Graph& graph,
                            StopPredicate&& should_stop,
                            int check_interval,
                            std::uint64_t pop_limit) {
        assert(graph.size() == n_);
        if (status_ == spfa_status::negative_cycle) return status_;
        if constexpr (DetectNegativeCycle) {
            assert(path_edge_counts_valid_);
        } else {
            path_edge_counts_valid_ = false;
        }
        assert(check_interval > 0);
        std::uint64_t processed = 0;
        int until_check = 0;

        // キューが空になるか、負閉路または利用者指定の打ち切り条件を検出するまで緩和を続ける
        while (!queue_.empty()) {
            if constexpr (CheckPopLimit) {
                if (processed >= pop_limit) {
                    status_ = spfa_status::paused;
                    return status_;
                }
            }
            if constexpr (CheckStop) {
                if (until_check == 0) {
                    if (std::invoke(should_stop)) {
                        status_ = spfa_status::paused;
                        return status_;
                    }
                    until_check = check_interval;
                }
                --until_check;
            }

            const int from = queue_.pop_front();
            in_queue_[from] = 0;
            ++processed;
            const Cost from_distance = dist_[from];

            // from の現在ラベルから全出辺を緩和し、改善した終点だけを再び候補キューへ入れる
            for (const auto& edge : graph[from]) {
                const Cost candidate = from_distance + edge.cost;
                const bool changed =
                    relax_vertex(from, edge.to, candidate, edge.id, DetectNegativeCycle);
                if (changed && status_ == spfa_status::negative_cycle) return status_;
            }
        }

        status_ = spfa_status::completed;
        return status_;
    }

    std::pair<std::vector<int>, std::vector<int>> build_negative_cycle() const {
        if constexpr (!StoreParent) {
            return {};
        } else {
            if (negative_cycle_trigger_ == -1) return {};
            int vertex = negative_cycle_trigger_;
            for (int step = 0; step < n_; ++step) {
                vertex = parent_vertex_[vertex];
                if (vertex == -1) return {};
            }

            // 親をたどると辺と逆向きになるため、閉路頂点列を収集した後に反転する
            const int cycle_start = vertex;
            std::vector<int> vertices;
            do {
                vertices.push_back(vertex);
                vertex = parent_vertex_[vertex];
                if (vertex == -1 || static_cast<int>(vertices.size()) > n_) return {};
            } while (vertex != cycle_start);
            std::reverse(vertices.begin(), vertices.end());

            // 頂点列 vertices[i] -> vertices[i+1] に対応する親辺IDを順に並べる
            std::vector<int> edges;
            edges.reserve(vertices.size());
            const int cycle_size = static_cast<int>(vertices.size());
            for (int i = 0; i < cycle_size; ++i) {
                const int next = vertices[(i + 1) % cycle_size];
                edges.push_back(parent_edge_[next]);
            }
            return {std::move(vertices), std::move(edges)};
        }
    }

public:
    // 頂点数0のWorkspaceを構築する。O(1)
    spfa_workspace() = default;

    // 頂点数 n の配列と最大 n 要素のリングdequeを確保する。O(n)
    explicit spfa_workspace(int n, Cost inf = spfa_default_inf<Cost>())
        : n_(n),
          inf_(inf),
          dist_(n, inf),
          parent_vertex_(StoreParent ? n : 0, -1),
          parent_edge_(StoreParent ? n : 0, -1),
          path_edge_count_(n, 0),
          in_queue_(n, 0),
          queue_(n) {
        touched_vertices_.reserve(n);
    }

    // 頂点数を返す。O(1)
    int size() const { return n_; }

    // 到達不能を表す値を返す。O(1)
    Cost inf() const { return inf_; }

    // 直前の実行状態を返す。O(1)
    spfa_status status() const { return status_; }

    // vertex の現在距離を返す。O(1)
    Cost distance(int vertex) const { return dist_[vertex]; }

    // vertex が現在発見済みなら true を返す。O(1)
    bool reachable(int vertex) const { return distance(vertex) != inf_; }

    // 現在距離の配列を参照で返す。O(1)
    const std::vector<Cost>& distances() const { return dist_; }

    // 現在の親頂点配列を参照で返す。StoreParent=true のときだけ n 要素を持つ。O(1)
    const std::vector<int>& parent_vertices() const { return parent_vertex_; }

    // 現在の親辺ID配列を参照で返す。StoreParent=true のときだけ n 要素を持つ。O(1)
    const std::vector<int>& parent_edges() const { return parent_edge_; }

    // 現在の発見済み頂点列を参照で返す。O(1)
    const std::vector<int>& touched_vertices() const { return touched_vertices_; }

    // 前回発見した頂点だけを未到達へ戻し、キューと状態を初期化する。O(k), k は前回の発見頂点数
    void clear() {
        // 発見済み頂点だけを戻し、到達範囲が小さい反復実行で全頂点初期化を避ける
        for (const int vertex : touched_vertices_) {
            const int index = vertex;
            dist_[index] = inf_;
            path_edge_count_[index] = 0;
            in_queue_[index] = 0;
            if constexpr (StoreParent) {
                parent_vertex_[index] = -1;
                parent_edge_[index] = -1;
            }
        }
        touched_vertices_.clear();
        queue_.clear();
        status_ = spfa_status::completed;
        negative_cycle_trigger_ = -1;
        path_edge_counts_valid_ = true;
    }

    // 単一始点で実行を開始し、まだ緩和せず paused にする。O(k), k は前回の発見頂点数
    void start(int source, Cost initial_distance = Cost{}) {
        clear();
        touch(source);
        dist_[source] = initial_distance;
        path_edge_count_[source] = 0;
        set_parent(source, -1, -1);
        enqueue(source);
        status_ = spfa_status::paused;
    }

    // 多始点・任意初期距離で実行を開始し、まだ緩和せず paused または completed にする。O(k+s)
    void start(std::span<const spfa_seed<Cost>> seeds) {
        clear();
        for (const auto& seed : seeds) {
            const int index = seed.vertex;
            if (!(seed.distance < dist_[index])) continue;
            touch(seed.vertex);
            dist_[index] = seed.distance;
            path_edge_count_[index] = 0;
            set_parent(seed.vertex, -1, -1);
            enqueue(seed.vertex);
        }
        status_ = queue_.empty() ? spfa_status::completed : spfa_status::paused;
    }

    // 単一始点から負閉路検出付きSLF/FIFOを完了まで実行する。時間 O(R)、空間 O(V+E)、R は実際の辺走査数
    template <class Graph>
    spfa_status run(const Graph& graph, int source,
                    Cost initial_distance = Cost{}) {
        start(source, initial_distance);
        return resume(graph);
    }

    // 多始点から負閉路検出付きSLF/FIFOを完了まで実行する。時間 O(R)、空間 O(V+E)、R は実際の辺走査数
    template <class Graph>
    spfa_status run(const Graph& graph,
                    std::span<const spfa_seed<Cost>> seeds) {
        start(seeds);
        return resume(graph);
    }

    // 負閉路が存在しない前提で単一始点から完了まで実行する。時間 O(R)、空間 O(V+E)、R は実際の辺走査数
    template <class Graph>
    spfa_status run_unchecked(const Graph& graph, int source,
                              Cost initial_distance = Cost{}) {
        start(source, initial_distance);
        return resume_unchecked(graph);
    }

    // 負閉路が存在しない前提で多始点から完了まで実行する。時間 O(R)、空間 O(V+E)、R は実際の辺走査数
    template <class Graph>
    spfa_status run_unchecked(const Graph& graph,
                              std::span<const spfa_seed<Cost>> seeds) {
        start(seeds);
        return resume_unchecked(graph);
    }

    // 現在の候補キューを負閉路検出付きで空になるまで処理する。O(R)、R は実際の辺走査数
    template <class Graph>
    spfa_status resume(const Graph& graph) {
        auto never_stop = [] { return false; };
        return resume_impl<true, false, false>(
            graph, never_stop, 1, std::numeric_limits<std::uint64_t>::max());
    }

    // 負閉路が存在しない前提で現在の候補キューを空になるまで処理する。O(R)、R は実際の辺走査数
    template <class Graph>
    spfa_status resume_unchecked(const Graph& graph) {
        auto never_stop = [] { return false; };
        return resume_impl<false, false, false>(
            graph, never_stop, 1, std::numeric_limits<std::uint64_t>::max());
    }

    // check_interval 頂点ごとに should_stop() を調べ、true なら頂点境界で paused にする。O(R+P)、P はpop数
    template <class Graph, class StopPredicate>
    spfa_status resume_until(const Graph& graph,
                             StopPredicate&& should_stop,
                             int check_interval = 64) {
        return resume_impl<true, true, false>(
            graph, std::forward<StopPredicate>(should_stop), check_interval,
            std::numeric_limits<std::uint64_t>::max());
    }

    // 最大 pop_limit 頂点を処理し、候補が残れば paused にする。O(R)、R は実際の辺走査数
    template <class Graph>
    spfa_status resume_for_pops(const Graph& graph,
                                std::uint64_t pop_limit) {
        auto never_stop = [] { return false; };
        return resume_impl<true, false, true>(graph, never_stop, 1, pop_limit);
    }

    // 始点ラベルを減少させて差分伝播の候補に追加する。償却 O(1)
    bool notify_source_decreased(int vertex, Cost new_distance) {
        assert(status_ != spfa_status::negative_cycle);
        assert(path_edge_counts_valid_);
        if (!(new_distance < dist_[vertex])) return false;
        touch(vertex);
        dist_[vertex] = new_distance;
        path_edge_count_[vertex] = 0;
        set_parent(vertex, -1, -1);
        enqueue(vertex);
        status_ = spfa_status::paused;
        negative_cycle_trigger_ = -1;
        return true;
    }

    // 追加または重み減少済みの辺を一度緩和して差分伝播の候補に追加する。償却 O(1)
    template <class Graph>
    bool notify_edge_added_or_decreased(const Graph& graph, int edge_id) {
        assert(status_ != spfa_status::negative_cycle);
        assert(path_edge_counts_valid_);
        const auto edge = graph.get_edge(edge_id);
        if (!reachable(edge.from)) return false;
        const Cost candidate = distance(edge.from) + edge.cost;
        return relax_vertex(edge.from, edge.to, candidate, edge.id, true);
    }

    // 負閉路がない前提で始点ラベルを減少させて差分伝播の候補に追加する。償却 O(1)
    bool notify_source_decreased_unchecked(int vertex, Cost new_distance) {
        assert(status_ != spfa_status::negative_cycle);
        if (!(new_distance < dist_[vertex])) return false;
        touch(vertex);
        dist_[vertex] = new_distance;
        set_parent(vertex, -1, -1);
        enqueue(vertex);
        status_ = spfa_status::paused;
        negative_cycle_trigger_ = -1;
        path_edge_counts_valid_ = false;
        return true;
    }

    // 負閉路がない前提で追加または重み減少済みの辺を一度緩和する。償却 O(1)
    template <class Graph>
    bool notify_edge_added_or_decreased_unchecked(const Graph& graph,
                                                   int edge_id) {
        assert(status_ != spfa_status::negative_cycle);
        const auto edge = graph.get_edge(edge_id);
        if (!reachable(edge.from)) return false;
        const Cost candidate = distance(edge.from) + edge.cost;
        path_edge_counts_valid_ = false;
        return relax_vertex(edge.from, edge.to, candidate, edge.id, false);
    }

    // notify_* で追加した候補から負閉路検出付き差分伝播を完了する。O(R)、R は実際の辺走査数
    template <class Graph>
    spfa_status propagate(const Graph& graph) { return resume(graph); }

    // 負閉路がない前提で notify_* の候補から差分伝播を完了する。O(R)、R は実際の辺走査数
    template <class Graph>
    spfa_status propagate_unchecked(const Graph& graph) {
        return resume_unchecked(graph);
    }

    // 発見済み経路の頂点列を返し、未到達または親を保持しない場合は空列を返す。O(V)
    std::vector<int> restore_vertex_path(int target) const {
        if constexpr (!StoreParent) {
            return {};
        } else {
            if (!reachable(target)) return {};
            std::vector<int> path;
            int vertex = target;
            for (int step = 0; step <= n_; ++step) {
                path.push_back(vertex);
                vertex = parent_vertex_[vertex];
                if (vertex == -1) {
                    std::reverse(path.begin(), path.end());
                    return path;
                }
            }
            return {};
        }
    }

    // 発見済み経路の辺ID列を返し、未到達または親を保持しない場合は空列を返す。O(V)
    std::vector<int> restore_edge_path(int target) const {
        if constexpr (!StoreParent) {
            return {};
        } else {
            if (!reachable(target)) return {};
            std::vector<int> path;
            int vertex = target;
            for (int step = 0; step <= n_; ++step) {
                const int parent = parent_vertex_[vertex];
                if (parent == -1) {
                    std::reverse(path.begin(), path.end());
                    return path;
                }
                path.push_back(parent_edge_[vertex]);
                vertex = parent;
            }
            return {};
        }
    }

    // 検出した負閉路の頂点列を返し、未検出または親を保持しない場合は空列を返す。O(V)
    std::vector<int> negative_cycle_vertices() const {
        return build_negative_cycle().first;
    }

    // 検出した負閉路の辺ID列を返し、未検出または親を保持しない場合は空列を返す。O(V)
    std::vector<int> negative_cycle_edges() const {
        return build_negative_cycle().second;
    }

    // 配列を所有する結果へ移してWorkspaceを消費する。O(V)
    spfa_result<Cost> take_result() && {
        auto [cycle_vertices, cycle_edges] = build_negative_cycle();
        spfa_result<Cost> result;
        result.status = status_;
        result.inf = inf_;
        result.dist = std::move(dist_);
        result.parent_vertex = std::move(parent_vertex_);
        result.parent_edge = std::move(parent_edge_);
        result.negative_cycle_vertices = std::move(cycle_vertices);
        result.negative_cycle_edges = std::move(cycle_edges);
        return result;
    }
};

/*
 * 用途: 明示グラフの単一始点最短路、経路復元、始点から到達可能な負閉路検出
 * 入力: graph、source、全有限経路長より大きい inf
 * 出力: 完了時は距離と親、負閉路検出時は status と負閉路頂点・辺ID列
 * 処理量: 時間 O(R)、空間 O(V+E)、R は選択したキュー順序で実際に走査した辺数
 */
template <class Cost, spfa_queue_policy QueuePolicy = spfa_queue_policy::fifo,
          template <class> class Graph>
spfa_result<Cost> spfa_shortest_paths(
    const Graph<Cost>& graph, int source,
    Cost inf = spfa_default_inf<Cost>()) {
    spfa_workspace<Cost, QueuePolicy, true> workspace(graph.size(), inf);
    workspace.run(graph, source);
    return std::move(workspace).take_result();
}

/*
 * 用途: 多始点最短路、始点ごとに初期コストが異なる最短路
 * 入力: graph と {vertex, distance} の seeds。各seedは仮想超始点からの辺に相当する
 * 出力: 完了時は距離と親、負閉路検出時は status と負閉路頂点・辺ID列
 * 処理量: 時間 O(R)、空間 O(V+E)、R は選択したキュー順序で実際に走査した辺数
 */
template <class Cost, spfa_queue_policy QueuePolicy = spfa_queue_policy::fifo,
          template <class> class Graph>
spfa_result<Cost> spfa_shortest_paths(
    const Graph<Cost>& graph,
    std::span<const spfa_seed<Cost>> seeds,
    Cost inf = spfa_default_inf<Cost>()) {
    spfa_workspace<Cost, QueuePolicy, true> workspace(graph.size(), inf);
    workspace.run(graph, seeds);
    return std::move(workspace).take_result();
}

/* vectorで多始点を渡す簡便版。処理内容はspan版と同じ */
template <class Cost, spfa_queue_policy QueuePolicy = spfa_queue_policy::fifo,
          template <class> class Graph>
spfa_result<Cost> spfa_shortest_paths(
    const Graph<Cost>& graph,
    const std::vector<spfa_seed<Cost>>& seeds,
    Cost inf = spfa_default_inf<Cost>()) {
    return spfa_shortest_paths<Cost, QueuePolicy>(
        graph, std::span<const spfa_seed<Cost>>(seeds), inf);
}

/*
 * 用途: グラフの連結性に関係なく任意の負閉路を探す
 * 入力: 明示グラフ。内部で全頂点を距離0の始点として扱う
 * 出力: 負閉路があればその頂点・辺ID列、なければ実行可能ポテンシャルにも使える距離
 * 処理量: 時間 O(R)、空間 O(V+E)、R は実際に走査した辺数
 */
template <class Cost, spfa_queue_policy QueuePolicy = spfa_queue_policy::fifo,
          template <class> class Graph>
spfa_result<Cost> spfa_find_negative_cycle_anywhere(
    const Graph<Cost>& graph,
    Cost inf = spfa_default_inf<Cost>()) {
    std::vector<spfa_seed<Cost>> seeds;
    seeds.reserve(graph.size());
    for (int vertex = 0; vertex < graph.size(); ++vertex) {
        seeds.push_back(spfa_seed<Cost>{vertex, Cost{}});
    }
    spfa_workspace<Cost, QueuePolicy, true> workspace(graph.size(), inf);
    workspace.run(graph, std::span<const spfa_seed<Cost>>(seeds));
    return std::move(workspace).take_result();
}

/*
 * 用途: Johnson 法や最小費用流の初期ポテンシャル構築
 * 入力: 負辺を許す明示グラフ
 * 出力: 全縮約重み cost+potential[from]-potential[to] を非負にする dist。負閉路時は構築不能
 * 処理量: 時間 O(R)、空間 O(V+E)、R は実際に走査した辺数
 */
template <class Cost, spfa_queue_policy QueuePolicy = spfa_queue_policy::fifo,
          template <class> class Graph>
spfa_result<Cost> spfa_feasible_potential(
    const Graph<Cost>& graph,
    Cost inf = spfa_default_inf<Cost>()) {
    std::vector<spfa_seed<Cost>> seeds;
    seeds.reserve(graph.size());
    for (int vertex = 0; vertex < graph.size(); ++vertex) {
        seeds.push_back(spfa_seed<Cost>{vertex, Cost{}});
    }
    spfa_workspace<Cost, QueuePolicy, true> workspace(graph.size(), inf);
    workspace.run(graph, std::span<const spfa_seed<Cost>>(seeds));
    return std::move(workspace).take_result();
}

enum class spfa_distance_state : std::uint8_t {
    unreachable,
    finite,
    negative_infinity,
};

/*
 * 各頂点を到達不能・有限距離・負閉路から到達可能な -INF に分類した結果
 * finite 頂点だけ dist と親経路が最短路として有効になる
 */
template <class Cost>
struct spfa_classified_result {
    Cost inf = spfa_default_inf<Cost>();
    std::vector<Cost> dist;
    std::vector<spfa_distance_state> state;
    std::vector<int> parent_vertex;
    std::vector<int> parent_edge;

    // vertex が有限距離なら true を返す。O(1)
    bool finite(int vertex) const {
        return state[vertex] == spfa_distance_state::finite;
    }

    // vertex が負閉路から到達可能なら true を返す。O(1)
    bool negative_infinite(int vertex) const {
        return state[vertex] ==
               spfa_distance_state::negative_infinity;
    }

    // 有限最短路の頂点列を返し、有限距離でなければ空列を返す。O(V)
    std::vector<int> restore_vertex_path(int target) const {
        if (!finite(target)) return {};
        const int n = static_cast<int>(dist.size());
        std::vector<int> path;
        int vertex = target;
        for (int step = 0; step <= n; ++step) {
            path.push_back(vertex);
            vertex = parent_vertex[vertex];
            if (vertex == -1) {
                std::reverse(path.begin(), path.end());
                return path;
            }
        }
        return {};
    }

    // 有限最短路の辺ID列を返し、有限距離でなければ空列を返す。O(V)
    std::vector<int> restore_edge_path(int target) const {
        if (!finite(target)) return {};
        const int n = static_cast<int>(dist.size());
        std::vector<int> path;
        int vertex = target;
        for (int step = 0; step <= n; ++step) {
            const int parent = parent_vertex[vertex];
            if (parent == -1) {
                std::reverse(path.begin(), path.end());
                return path;
            }
            path.push_back(parent_edge[vertex]);
            vertex = parent;
        }
        return {};
    }
};

/*
 * 用途: 最短路問題で各頂点を到達不能・有限距離・負閉路由来の -INF に分類する
 * 入力: graph と任意初期距離の seeds
 * 出力: 各頂点のstate、有限頂点の最短距離と親
 * 処理量: 時間 O(R+V+E)、空間 O(V+E)、R は負閉路の影響を切り離すまでの辺走査数
 */
template <class Cost, spfa_queue_policy QueuePolicy = spfa_queue_policy::fifo,
          template <class> class Graph>
spfa_classified_result<Cost> spfa_shortest_paths_with_negative_infinity(
    const Graph<Cost>& graph,
    std::span<const spfa_seed<Cost>> seeds,
    Cost inf = spfa_default_inf<Cost>()) {
    const int n = graph.size();
    std::vector<Cost> dist(n, inf);
    std::vector<int> parent_vertex(n, -1);
    std::vector<int> parent_edge(n, -1);
    std::vector<int> path_edge_count(n, 0);
    std::vector<unsigned char> in_queue(n, 0);
    std::vector<unsigned char> negative(n, 0);
    std::deque<int> queue;

    auto enqueue = [&](int vertex) {
        if (in_queue[vertex] != 0) return;
        if constexpr (QueuePolicy == spfa_queue_policy::slf) {
            if (!queue.empty() &&
                dist[vertex] <
                    dist[queue.front()]) {
                queue.push_front(vertex);
            } else {
                queue.push_back(vertex);
            }
        } else {
            queue.push_back(vertex);
        }
        in_queue[vertex] = 1;
    };

    // 仮想超始点からの辺に相当する初期ラベルを設定する
    for (const auto& seed : seeds) {
        const int index = seed.vertex;
        if (!(seed.distance < dist[index])) continue;
        dist[index] = seed.distance;
        parent_vertex[index] = -1;
        parent_edge[index] = -1;
        path_edge_count[index] = 0;
        enqueue(seed.vertex);
    }

    // n辺以上の改善経路が現れた頂点を負閉路の影響下として数値緩和から切り離す
    while (!queue.empty()) {
        const int from = queue.front();
        queue.pop_front();
        in_queue[from] = 0;
        if (negative[from] != 0) continue;

        for (const auto& edge : graph[from]) {
            if (negative[edge.to] != 0) continue;
            const Cost candidate = dist[from] + edge.cost;
            if (!(candidate < dist[edge.to])) continue;

            const int to_index = edge.to;
            dist[to_index] = candidate;
            parent_vertex[to_index] = from;
            parent_edge[to_index] = edge.id;
            path_edge_count[to_index] =
                path_edge_count[from] + 1;
            if (path_edge_count[to_index] >= n) {
                negative[to_index] = 1;
            } else {
                enqueue(edge.to);
            }
        }
    }

    // 検出済み頂点から到達できる全頂点は、負閉路を任意回通って距離を -INF にできる
    std::queue<int> negative_queue;
    for (int vertex = 0; vertex < n; ++vertex) {
        if (negative[vertex] != 0) negative_queue.push(vertex);
    }
    while (!negative_queue.empty()) {
        const int from = negative_queue.front();
        negative_queue.pop();
        for (const auto& edge : graph[from]) {
            const int to_index = edge.to;
            if (negative[to_index] != 0) continue;
            negative[to_index] = 1;
            negative_queue.push(edge.to);
        }
    }

    spfa_classified_result<Cost> result;
    result.inf = inf;
    result.dist = std::move(dist);
    result.parent_vertex = std::move(parent_vertex);
    result.parent_edge = std::move(parent_edge);
    result.state.resize(n);
    for (int vertex = 0; vertex < n; ++vertex) {
        const int index = vertex;
        if (negative[index] != 0) {
            result.state[index] = spfa_distance_state::negative_infinity;
        } else if (result.dist[index] == inf) {
            result.state[index] = spfa_distance_state::unreachable;
        } else {
            result.state[index] = spfa_distance_state::finite;
        }
    }
    return result;
}

/* 単一始点版。入出力と性質は多始点版と同じ */
template <class Cost, spfa_queue_policy QueuePolicy = spfa_queue_policy::fifo,
          template <class> class Graph>
spfa_classified_result<Cost> spfa_shortest_paths_with_negative_infinity(
    const Graph<Cost>& graph, int source,
    Cost inf = spfa_default_inf<Cost>()) {
    const std::array<spfa_seed<Cost>, 1> seeds{spfa_seed<Cost>{source, Cost{}}};
    return spfa_shortest_paths_with_negative_infinity<Cost, QueuePolicy>(
        graph, std::span<const spfa_seed<Cost>>(seeds), inf);
}

/* vectorで多始点を渡す簡便版。処理内容はspan版と同じ */
template <class Cost, spfa_queue_policy QueuePolicy = spfa_queue_policy::fifo,
          template <class> class Graph>
spfa_classified_result<Cost> spfa_shortest_paths_with_negative_infinity(
    const Graph<Cost>& graph,
    const std::vector<spfa_seed<Cost>>& seeds,
    Cost inf = spfa_default_inf<Cost>()) {
    return spfa_shortest_paths_with_negative_infinity<Cost, QueuePolicy>(
        graph, std::span<const spfa_seed<Cost>>(seeds), inf);
}

template <class Cost>
struct spfa_difference_constraint {
    int left;
    int right;
    Cost upper;
};

template <class Cost>
struct spfa_difference_constraints_result {
    bool feasible = false;
    std::vector<Cost> value;
    std::vector<int> negative_cycle_constraint_ids;
};

/*
 * 用途: 制約 x[left]-x[right]<=upper の充足判定と実行可能な変数値の構築
 * 入力: 変数数と制約列。等式 x[a]-x[b]=d は {a,b,d} と {b,a,-d} の2制約にする
 * 出力: feasible=true なら value、false なら矛盾を作る負閉路上の入力制約添字
 * 処理量: 時間 O(R)、空間 O(V+C)、R は実際の辺走査数、C は制約数
 */
template <class Cost, spfa_queue_policy QueuePolicy = spfa_queue_policy::fifo>
spfa_difference_constraints_result<Cost> solve_difference_constraints_spfa(
    int variable_count,
    const std::vector<spfa_difference_constraint<Cost>>& constraints,
    Cost inf = spfa_default_inf<Cost>()) {
    spfa_graph<Cost> graph(variable_count);
    for (const auto& constraint : constraints) {
        graph.add_edge(constraint.right, constraint.left, constraint.upper);
    }

    std::vector<spfa_seed<Cost>> seeds;
    seeds.reserve(variable_count);
    for (int variable = 0; variable < variable_count; ++variable) {
        seeds.push_back(spfa_seed<Cost>{variable, Cost{}});
    }

    spfa_workspace<Cost, QueuePolicy, true> workspace(variable_count, inf);
    const spfa_status status =
        workspace.run(graph, std::span<const spfa_seed<Cost>>(seeds));
    spfa_difference_constraints_result<Cost> result;
    result.feasible = status == spfa_status::completed;
    if (result.feasible) {
        result.value = workspace.distances();
    } else {
        result.negative_cycle_constraint_ids = workspace.negative_cycle_edges();
    }
    return result;
}

/*
 * 用途: 頂点ID範囲は既知だが、遷移を保存せず必要時に生成したい暗黙グラフの最短路
 * 入力: 頂点数、seeds、各遷移で relax(to,cost) を呼ぶ expand(from,relax)
 * 出力: 距離、親頂点、負閉路頂点列。暗黙辺には辺IDがないため親辺IDは -1
 * 処理量: 時間 O(X)、空間 O(V)、X は再展開を含む全遷移列挙数
 */
template <class Cost, spfa_queue_policy QueuePolicy = spfa_queue_policy::fifo,
          class Expand>
spfa_result<Cost> spfa_implicit_shortest_paths(
    int vertex_count,
    std::span<const spfa_seed<Cost>> seeds,
    Expand&& expand,
    Cost inf = spfa_default_inf<Cost>()) {
    const int n = vertex_count;
    std::vector<Cost> dist(n, inf);
    std::vector<int> parent_vertex(n, -1);
    std::vector<int> parent_edge(n, -1);
    std::vector<int> path_edge_count(n, 0);
    std::vector<unsigned char> in_queue(n, 0);
    std::deque<int> queue;
    int negative_cycle_trigger = -1;

    auto enqueue = [&](int vertex) {
        if (in_queue[vertex] != 0) return;
        if constexpr (QueuePolicy == spfa_queue_policy::slf) {
            if (!queue.empty() &&
                dist[vertex] <
                    dist[queue.front()]) {
                queue.push_front(vertex);
            } else {
                queue.push_back(vertex);
            }
        } else {
            queue.push_back(vertex);
        }
        in_queue[vertex] = 1;
    };

    for (const auto& seed : seeds) {
        const int index = seed.vertex;
        if (!(seed.distance < dist[index])) continue;
        dist[index] = seed.distance;
        enqueue(seed.vertex);
    }

    // 遷移列挙callbackへ軽量な relax lambda を渡し、明示グラフ版と同じSLF/FIFO緩和を行う
    while (!queue.empty() && negative_cycle_trigger == -1) {
        const int from = queue.front();
        queue.pop_front();
        in_queue[from] = 0;

        auto relax = [&](int to, Cost cost) {
            if (negative_cycle_trigger != -1) return;
            const Cost candidate = dist[from] + cost;
            if (!(candidate < dist[to])) return;
            const int to_index = to;
            dist[to_index] = candidate;
            parent_vertex[to_index] = from;
            parent_edge[to_index] = -1;
            path_edge_count[to_index] =
                path_edge_count[from] + 1;
            if (path_edge_count[to_index] >= n) {
                negative_cycle_trigger = to;
                return;
            }
            enqueue(to);
        };
        std::invoke(expand, from, relax);
    }

    spfa_result<Cost> result;
    result.status = negative_cycle_trigger == -1 ? spfa_status::completed
                                                  : spfa_status::negative_cycle;
    result.inf = inf;
    result.dist = std::move(dist);
    result.parent_vertex = std::move(parent_vertex);
    result.parent_edge = std::move(parent_edge);

    // 暗黙辺には辺IDがないため、頂点列だけ負閉路として復元する
    if (negative_cycle_trigger != -1) {
        int vertex = negative_cycle_trigger;
        for (int step = 0; step < n && vertex != -1; ++step) {
            vertex = result.parent_vertex[vertex];
        }
        if (vertex != -1) {
            const int cycle_start = vertex;
            do {
                result.negative_cycle_vertices.push_back(vertex);
                vertex = result.parent_vertex[vertex];
            } while (vertex != -1 && vertex != cycle_start &&
                     static_cast<int>(result.negative_cycle_vertices.size()) <= n);
            if (vertex == cycle_start) {
                std::reverse(result.negative_cycle_vertices.begin(),
                             result.negative_cycle_vertices.end());
            } else {
                result.negative_cycle_vertices.clear();
            }
        }
    }
    return result;
}

/* 単一始点版。expand(from, relax) の仕様は多始点版と同じ */
template <class Cost, spfa_queue_policy QueuePolicy = spfa_queue_policy::fifo,
          class Expand>
spfa_result<Cost> spfa_implicit_shortest_paths(
    int vertex_count, int source, Expand&& expand,
    Cost inf = spfa_default_inf<Cost>()) {
    const std::array<spfa_seed<Cost>, 1> seeds{spfa_seed<Cost>{source, Cost{}}};
    return spfa_implicit_shortest_paths<Cost, QueuePolicy>(
        vertex_count, std::span<const spfa_seed<Cost>>(seeds),
        std::forward<Expand>(expand), inf);
}

/*
 * std::unordered_map で発見状態を連番ID化する疎な暗黙グラフの結果
 * states[id]、dist[id]、parent_id[id] を対応させ、状態値からも期待O(1)で検索できる
 */
template <class State, class Cost, class Hash = std::hash<State>,
          class KeyEqual = std::equal_to<State>>
struct spfa_sparse_result {
    spfa_status status = spfa_status::completed;
    Cost inf = spfa_default_inf<Cost>();
    std::vector<State> states;
    std::vector<Cost> dist;
    std::vector<int> parent_id;
    std::vector<int> negative_cycle_state_ids;
    std::unordered_map<State, int, Hash, KeyEqual> state_to_id;

    // 空の疎状態結果と指定hash/equalのunordered_mapを構築する。O(1)
    spfa_sparse_result(Cost inf_value, const Hash& hash, const KeyEqual& equal)
        : inf(inf_value), state_to_id(0, hash, equal) {}

    // state が発見済みなら true を返す。期待 O(1)
    bool contains(const State& state) const {
        return state_to_id.find(state) != state_to_id.end();
    }

    // state の距離へのポインタを返し、未発見なら nullptr を返す。期待 O(1)
    const Cost* find_distance(const State& state) const {
        const auto it = state_to_id.find(state);
        if (it == state_to_id.end()) return nullptr;
        return &dist[it->second];
    }

    // 発見済みstateへの経路を返し、未発見または親が閉路を作る場合は空列を返す。期待 O(経路長)
    std::vector<State> restore_state_path(const State& target) const {
        const auto it = state_to_id.find(target);
        if (it == state_to_id.end()) return {};
        const int state_count = static_cast<int>(states.size());
        std::vector<State> path;
        int id = it->second;
        for (int step = 0; step <= state_count; ++step) {
            path.push_back(states[id]);
            id = parent_id[id];
            if (id == -1) {
                std::reverse(path.begin(), path.end());
                return path;
            }
        }
        return {};
    }
};

/*
 * 用途: 状態を密な整数IDにできず、到達状態だけ生成したい疎な暗黙グラフの最短路
 * 入力: 状態と初期距離のseeds、到達可能状態総数以上のupper_bound、expand(state,relax)
 * 出力: 状態ID対応表・距離・親。上限を超える新状態を見つけた場合は state_limit_reached
 * 処理量: 期待 O(X)、空間 O(S)、X は再展開を含む遷移列挙数、S は発見状態数
 */
template <class State, class Cost, spfa_queue_policy QueuePolicy = spfa_queue_policy::fifo,
          class Expand, class Hash = std::hash<State>,
          class KeyEqual = std::equal_to<State>>
spfa_sparse_result<State, Cost, Hash, KeyEqual>
spfa_sparse_implicit_shortest_paths(
    const std::vector<std::pair<State, Cost>>& seeds,
    int state_count_upper_bound,
    Expand&& expand,
    std::size_t expected_state_count = 0,
    Cost inf = spfa_default_inf<Cost>(),
    Hash hash = Hash{},
    KeyEqual equal = KeyEqual{}) {
    spfa_sparse_result<State, Cost, Hash, KeyEqual> result(inf, hash, equal);
    if (expected_state_count != 0) result.state_to_id.reserve(expected_state_count);
    std::vector<int> path_edge_count;
    std::vector<unsigned char> in_queue;
    std::deque<int> queue;
    int negative_cycle_trigger = -1;
    bool state_limit_reached = false;

    auto get_or_add_id = [&](const State& state) -> int {
        const auto found = result.state_to_id.find(state);
        if (found != result.state_to_id.end()) return found->second;
        if (static_cast<int>(result.states.size()) >= state_count_upper_bound) {
            state_limit_reached = true;
            return -1;
        }
        const int id = static_cast<int>(result.states.size());
        result.state_to_id.emplace(state, id);
        result.states.push_back(state);
        result.dist.push_back(inf);
        result.parent_id.push_back(-1);
        path_edge_count.push_back(0);
        in_queue.push_back(0);
        return id;
    };

    auto enqueue = [&](int id) {
        if (in_queue[id] != 0) return;
        if constexpr (QueuePolicy == spfa_queue_policy::slf) {
            if (!queue.empty() &&
                result.dist[id] <
                    result.dist[queue.front()]) {
                queue.push_front(id);
            } else {
                queue.push_back(id);
            }
        } else {
            queue.push_back(id);
        }
        in_queue[id] = 1;
    };

    // 始点状態をID化し、同じ状態が複数あれば最小の初期距離だけを残す
    for (const auto& [state, initial_distance] : seeds) {
        const int id = get_or_add_id(state);
        if (id == -1) break;
        const int index = id;
        if (!(initial_distance < result.dist[index])) continue;
        result.dist[index] = initial_distance;
        result.parent_id[index] = -1;
        path_edge_count[index] = 0;
        enqueue(id);
    }

    // vector再確保で参照が無効にならないよう、展開対象Stateは値としてコピーしてからcallbackへ渡す
    while (!queue.empty() && negative_cycle_trigger == -1 && !state_limit_reached) {
        const int from = queue.front();
        queue.pop_front();
        in_queue[from] = 0;
        const State current_state = result.states[from];

        auto relax = [&](const State& next_state, Cost cost) {
            if (negative_cycle_trigger != -1 || state_limit_reached) return;
            const int to = get_or_add_id(next_state);
            if (to == -1) return;
            const Cost candidate = result.dist[from] + cost;
            if (!(candidate < result.dist[to])) return;
            const int to_index = to;
            result.dist[to_index] = candidate;
            result.parent_id[to_index] = from;
            path_edge_count[to_index] =
                path_edge_count[from] + 1;
            if (path_edge_count[to_index] >= state_count_upper_bound) {
                negative_cycle_trigger = to;
                return;
            }
            enqueue(to);
        };
        std::invoke(expand, current_state, relax);
    }

    if (state_limit_reached) {
        result.status = spfa_status::state_limit_reached;
    } else if (negative_cycle_trigger != -1) {
        result.status = spfa_status::negative_cycle;
    } else {
        result.status = spfa_status::completed;
    }

    // 親ID列をたどり、暗黙グラフ上の負閉路状態ID列を順方向に復元する
    if (negative_cycle_trigger != -1) {
        int id = negative_cycle_trigger;
        for (int step = 0; step < state_count_upper_bound && id != -1; ++step) {
            id = result.parent_id[id];
        }
        if (id != -1) {
            const int cycle_start = id;
            do {
                result.negative_cycle_state_ids.push_back(id);
                id = result.parent_id[id];
            } while (id != -1 && id != cycle_start &&
                     static_cast<int>(result.negative_cycle_state_ids.size()) <=
                         state_count_upper_bound);
            if (id == cycle_start) {
                std::reverse(result.negative_cycle_state_ids.begin(),
                             result.negative_cycle_state_ids.end());
            } else {
                result.negative_cycle_state_ids.clear();
            }
        }
    }
    return result;
}

/* 単一始点版。Stateにstd::hashがない場合はHashを明示する */
template <class State, class Cost, spfa_queue_policy QueuePolicy = spfa_queue_policy::fifo,
          class Expand, class Hash = std::hash<State>,
          class KeyEqual = std::equal_to<State>>
spfa_sparse_result<State, Cost, Hash, KeyEqual>
spfa_sparse_implicit_shortest_paths(
    const State& source,
    int state_count_upper_bound,
    Expand&& expand,
    Cost initial_distance = Cost{},
    std::size_t expected_state_count = 0,
    Cost inf = spfa_default_inf<Cost>(),
    Hash hash = Hash{},
    KeyEqual equal = KeyEqual{}) {
    const std::vector<std::pair<State, Cost>> seeds{{source, initial_distance}};
    return spfa_sparse_implicit_shortest_paths<State, Cost, QueuePolicy>(
        seeds, state_count_upper_bound, std::forward<Expand>(expand),
        expected_state_count, inf, std::move(hash), std::move(equal));
}

#if __INCLUDE_LEVEL__ == 0
#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

namespace spfa_selftest {

using test_cost = long long;
constexpr test_cost test_inf = std::numeric_limits<test_cost>::max() / 4;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    std::abort();
}

void require(bool condition, const std::string& message) {
    if (!condition) fail(message);
}

template <class A, class B>
void require_equal(const A& actual, const B& expected, const std::string& message) {
    if (!(actual == expected)) {
        std::ostringstream out;
        out << message << " actual=" << actual << " expected=" << expected;
        fail(out.str());
    }
}

struct reference_result {
    std::vector<test_cost> dist;
    std::vector<unsigned char> negative;
};

reference_result bellman_ford_reference(
    const spfa_graph<test_cost>& graph,
    std::span<const spfa_seed<test_cost>> seeds) {
    const int n = graph.size();
    std::vector<test_cost> dist(n, test_inf);
    std::vector<unsigned char> negative(n, 0);
    for (const auto& seed : seeds) {
        test_cost& value = dist[seed.vertex];
        if (seed.distance < value) value = seed.distance;
    }

    // n 回目にも改善された頂点は到達可能な負閉路の影響下にある
    for (int iteration = 0; iteration < n; ++iteration) {
        bool changed = false;
        for (int edge_id = 0; edge_id < graph.edge_count(); ++edge_id) {
            const auto edge = graph.get_edge(edge_id);
            if (dist[edge.from] == test_inf) continue;
            const test_cost candidate =
                dist[edge.from] + edge.cost;
            test_cost& destination = dist[edge.to];
            if (!(candidate < destination)) continue;
            destination = candidate;
            changed = true;
            if (iteration == n - 1) {
                negative[edge.to] = 1;
            }
        }
        if (!changed) break;
    }

    // 負閉路の影響を全到達先へ伝播する
    std::queue<int> queue;
    for (int vertex = 0; vertex < n; ++vertex) {
        if (negative[vertex] != 0) queue.push(vertex);
    }
    while (!queue.empty()) {
        const int from = queue.front();
        queue.pop();
        for (const auto& edge : graph[from]) {
            const int to_index = edge.to;
            if (negative[to_index] != 0) continue;
            negative[to_index] = 1;
            queue.push(edge.to);
        }
    }
    return reference_result{std::move(dist), std::move(negative)};
}

bool has_negative_vertex(const reference_result& result) {
    return std::ranges::any_of(result.negative, [](unsigned char value) {
        return value != 0;
    });
}

template <class Graph>
void validate_negative_cycle(const Graph& graph,
                             const spfa_result<test_cost>& result,
                             const std::string& label) {
    require(result.status == spfa_status::negative_cycle,
            label + ": negative-cycle status");
    require(!result.negative_cycle_vertices.empty(), label + ": cycle vertices");
    require_equal(result.negative_cycle_edges.size(),
                  result.negative_cycle_vertices.size(),
                  label + ": cycle edge count");

    test_cost total_cost = 0;
    const int cycle_size = static_cast<int>(result.negative_cycle_vertices.size());
    for (int i = 0; i < cycle_size; ++i) {
        const auto edge = graph.get_edge(
            result.negative_cycle_edges[i]);
        const int from = result.negative_cycle_vertices[i];
        const int to = result.negative_cycle_vertices[
            (i + 1) % cycle_size];
        require_equal(edge.from, from, label + ": cycle edge from");
        require_equal(edge.to, to, label + ": cycle edge to");
        total_cost += edge.cost;
    }
    require(total_cost < 0, label + ": cycle cost must be negative");
}

template <class Graph>
void validate_finite_paths(const Graph& graph,
                           const spfa_result<test_cost>& result,
                           const std::string& label) {
    require(result.status == spfa_status::completed, label + ": completed");
    const int n = graph.size();
    for (int target = 0; target < n; ++target) {
        if (!result.reachable(target)) continue;
        const auto vertices = result.restore_vertex_path(target);
        const auto edges = result.restore_edge_path(target);
        require(!vertices.empty(), label + ": restored vertices");
        require_equal(vertices.back(), target, label + ": restored target");
        require_equal(edges.size() + 1, vertices.size(), label + ": path sizes");
        for (std::size_t i = 0; i < edges.size(); ++i) {
            const auto edge = graph.get_edge(edges[i]);
            require_equal(edge.from, vertices[i], label + ": path edge from");
            require_equal(edge.to, vertices[i + 1], label + ": path edge to");
            require_equal(result.dist[edge.from] + edge.cost,
                          result.dist[edge.to],
                          label + ": parent distance equality");
        }
    }
}

void test_graph_and_basic_shortest_paths() {
    spfa_graph<test_cost> graph(6);
    graph.reserve_out_edges(0, 3);
    const int edge_01 = graph.add_edge(0, 1, 4);
    graph.add_edge(0, 2, 5);
    graph.add_edge(1, 2, -2);
    graph.add_edge(2, 3, 3);
    graph.add_edge(1, 3, 10);
    graph.add_edge(3, 4, 1);
    graph.add_edge(0, 1, 7);

    require_equal(graph.size(), 6, "graph size");
    require_equal(graph.edge_count(), 7, "graph edge count");
    require_equal(graph.get_edge(edge_01).cost, test_cost{4}, "graph get edge");
    require_equal(graph.set_cost(edge_01, 4), test_cost{4}, "graph set cost old");

    spfa_graph<test_cost> ordered_graph(2);
    const int ordered_edge_5 = ordered_graph.add_edge(0, 1, 5);
    const int ordered_edge_minus_1 = ordered_graph.add_edge(0, 1, -1);
    const int ordered_edge_3 = ordered_graph.add_edge(0, 1, 3);
    spfa_csr_graph<test_cost> ordered_csr(ordered_graph);
    ordered_graph.sort_edges_by_cost_for_slf();
    ordered_csr.sort_edges_by_cost_for_slf();
    require_equal(ordered_graph[0][0].id, ordered_edge_minus_1,
                  "graph SLF sort first edge");
    require_equal(ordered_graph[0][1].id, ordered_edge_3,
                  "graph SLF sort middle edge");
    require_equal(ordered_csr[0][2].id, ordered_edge_5,
                  "CSR SLF sort last edge");
    require_equal(ordered_graph.get_edge(ordered_edge_5).cost, test_cost{5},
                  "graph SLF sort keeps edge IDs");
    require_equal(ordered_csr.get_edge(ordered_edge_minus_1).cost, test_cost{-1},
                  "CSR SLF sort keeps edge IDs");
    ordered_csr.set_cost(ordered_edge_5, -2);
    ordered_csr.sort_edges_by_cost_for_slf();
    require_equal(ordered_csr[0][0].id, ordered_edge_5,
                  "CSR SLF resort after set_cost");

    const auto fifo = spfa_shortest_paths(graph, 0);
    const auto slf =
        spfa_shortest_paths<test_cost, spfa_queue_policy::slf>(graph, 0);
    const std::vector<test_cost> expected{0, 4, 2, 5, 6, test_inf};
    require(slf.dist == expected, "basic SLF distances");
    require(fifo.dist == expected, "basic FIFO distances");
    validate_finite_paths(graph, slf, "basic SLF paths");

    static_assert(sizeof(spfa_graph<test_cost>::edge) == 16);
    static_assert(sizeof(spfa_csr_graph<test_cost>::edge) == 16);
    spfa_csr_graph<test_cost> csr_graph(graph);
    const auto csr_result = spfa_shortest_paths(csr_graph, 0);
    require(csr_result.dist == expected, "basic CSR distances");
    validate_finite_paths(csr_graph, csr_result, "basic CSR paths");
    require_equal(csr_graph.get_edge(edge_01).cost, test_cost{4},
                  "CSR keeps edge IDs");
    require_equal(csr_graph.set_cost(edge_01, 3), test_cost{4},
                  "CSR set cost old");
    require_equal(spfa_shortest_paths(csr_graph, 0).dist[1], test_cost{3},
                  "CSR changed cost");
    csr_graph.set_cost(edge_01, 4);

    spfa_workspace<test_cost> csr_workspace(csr_graph.size());
    csr_workspace.run(csr_graph, 0);
    csr_graph.set_cost(edge_01, 1);
    require(csr_workspace.notify_edge_added_or_decreased(csr_graph, edge_01),
            "CSR notify decreased edge");
    csr_workspace.propagate(csr_graph);
    require(csr_workspace.distances() == spfa_shortest_paths(csr_graph, 0).dist,
            "CSR incremental equals full rerun");
    csr_graph.set_cost(edge_01, 4);

    using csr_type = spfa_csr_graph<test_cost>;
    const std::vector<csr_type::input_edge> direct_edges{{0, 1, 2},
                                                          {1, 2, -1}};
    const csr_type direct_csr(3, direct_edges);
    require_equal(spfa_shortest_paths(direct_csr, 0).dist[2], test_cost{1},
                  "CSR direct construction");

    const std::vector<spfa_seed<test_cost>> seeds{{0, 10}, {4, -3}, {0, 2}};
    const auto multi = spfa_shortest_paths(
        graph, std::span<const spfa_seed<test_cost>>(seeds));
    const auto multi_direct = spfa_shortest_paths(graph, seeds);
    require_equal(multi.dist[0], test_cost{2}, "multi-source duplicate seed");
    require_equal(multi.dist[4], test_cost{-3}, "multi-source independent seed");
    require(multi_direct.dist == multi.dist, "vector seed one-shot overload");

    spfa_workspace<test_cost> direct_vector_workspace(graph.size());
    direct_vector_workspace.run(graph, seeds);
    require(direct_vector_workspace.distances() == multi.dist,
            "vector implicitly converts to seed span");

    spfa_graph<__int128_t> wide_graph(2);
    wide_graph.add_edge(0, 1, -7);
    const auto wide_result = spfa_shortest_paths(wide_graph, 0);
    require(wide_result.dist[1] == -7,
            "__int128 cost support");

    const auto [forward, backward] = graph.add_undirected_edge(4, 5, 8);
    require_equal(graph.get_edge(forward).from, 4, "undirected forward");
    require_equal(graph.get_edge(backward).from, 5, "undirected backward");
}

void test_negative_cycles_and_classification() {
    spfa_graph<test_cost> graph(7);
    graph.add_edge(0, 1, 2);
    graph.add_edge(1, 2, -5);
    graph.add_edge(2, 1, 1);
    graph.add_edge(2, 3, 4);
    graph.add_edge(4, 5, -2);
    graph.add_edge(5, 4, 1);

    const auto reachable_cycle = spfa_shortest_paths(graph, 0);
    validate_negative_cycle(graph, reachable_cycle, "reachable negative cycle");
    const spfa_csr_graph<test_cost> csr_graph(graph);
    validate_negative_cycle(csr_graph, spfa_shortest_paths(csr_graph, 0),
                            "CSR reachable negative cycle");

    const auto isolated_source = spfa_shortest_paths(graph, 6);
    require(isolated_source.status == spfa_status::completed,
            "unreachable negative cycles ignored");

    const auto anywhere = spfa_find_negative_cycle_anywhere(graph);
    validate_negative_cycle(graph, anywhere, "negative cycle anywhere");
    validate_negative_cycle(csr_graph,
                            spfa_find_negative_cycle_anywhere(csr_graph),
                            "CSR negative cycle anywhere");

    const auto classified = spfa_shortest_paths_with_negative_infinity(graph, 0);
    require(classified.negative_infinite(1), "cycle vertex -INF");
    require(classified.negative_infinite(2), "cycle vertex2 -INF");
    require(classified.negative_infinite(3), "cycle descendant -INF");
    require(classified.state[0] == spfa_distance_state::finite, "finite source");
    require(classified.state[4] == spfa_distance_state::unreachable,
            "unreachable separate cycle");

    spfa_graph<test_cost> self_loop(1);
    self_loop.add_edge(0, 0, -1);
    validate_negative_cycle(self_loop, spfa_shortest_paths(self_loop, 0),
                            "negative self loop");

    spfa_graph<test_cost> zero_cycle(2);
    zero_cycle.add_edge(0, 1, 0);
    zero_cycle.add_edge(1, 0, 0);
    require(spfa_shortest_paths(zero_cycle, 0).status == spfa_status::completed,
            "zero cycle is finite");

    spfa_graph<test_cost> negative_undirected(2);
    negative_undirected.add_undirected_edge(0, 1, -1);
    require(spfa_shortest_paths(negative_undirected, 0).status ==
                spfa_status::negative_cycle,
            "negative undirected edge creates cycle");

    spfa_graph<test_cost> empty(0);
    require(spfa_find_negative_cycle_anywhere(empty).status == spfa_status::completed,
            "empty graph");
}

void test_potential_and_difference_constraints() {
    spfa_graph<test_cost> graph(4);
    graph.add_edge(0, 1, -4);
    graph.add_edge(1, 2, 7);
    graph.add_edge(0, 2, 8);
    graph.add_edge(2, 3, -2);
    graph.add_edge(3, 1, 3);
    const auto potential = spfa_feasible_potential(graph);
    require(potential.status == spfa_status::completed, "feasible potential status");
    const spfa_csr_graph<test_cost> csr_graph(graph);
    const auto csr_potential = spfa_feasible_potential(csr_graph);
    require(csr_potential.dist == potential.dist, "CSR feasible potential");
    for (int edge_id = 0; edge_id < graph.edge_count(); ++edge_id) {
        const auto edge = graph.get_edge(edge_id);
        const test_cost reduced = edge.cost + potential.dist[edge.from] -
                                  potential.dist[edge.to];
        require(reduced >= 0, "nonnegative reduced cost");
    }

    std::vector<spfa_difference_constraint<test_cost>> feasible{
        {1, 0, 5},   // x1 - x0 <= 5
        {2, 1, 3},   // x2 - x1 <= 3
        {0, 2, -2},  // x0 - x2 <= -2
        {3, 2, 4},
        {2, 3, -4},  // x3 - x2 = 4 の逆向き
    };
    const auto feasible_result = solve_difference_constraints_spfa(4, feasible);
    require(feasible_result.feasible, "difference constraints feasible");
    for (const auto& constraint : feasible) {
        require(feasible_result.value[constraint.left] -
                    feasible_result.value[constraint.right] <=
                    constraint.upper,
                "difference constraint satisfied");
    }

    const std::vector<spfa_difference_constraint<test_cost>> infeasible{
        {0, 1, -1},
        {1, 0, -1},
    };
    const auto infeasible_result = solve_difference_constraints_spfa(2, infeasible);
    require(!infeasible_result.feasible, "difference constraints infeasible");
    require(!infeasible_result.negative_cycle_constraint_ids.empty(),
            "infeasible constraint witness");
}

void test_workspace_pause_reuse_and_incremental() {
    spfa_graph<test_cost> chain(100);
    for (int vertex = 0; vertex + 1 < chain.size(); ++vertex) {
        chain.add_edge(vertex, vertex + 1, 1);
    }

    spfa_workspace<test_cost> workspace(chain.size());
    workspace.start(0);
    require(workspace.resume_for_pops(chain, 3) == spfa_status::paused,
            "pop-limited pause");
    require(workspace.reachable(3), "paused labels remain usable");
    require(workspace.resume(chain) == spfa_status::completed, "resume completion");
    require_equal(workspace.distance(99), test_cost{99}, "resumed distance");

    workspace.start(0);
    int checks = 0;
    const auto stopped = workspace.resume_until(
        chain,
        [&] {
            ++checks;
            return checks == 2;
        },
        4);
    require(stopped == spfa_status::paused, "predicate pause");
    require(workspace.resume(chain) == spfa_status::completed,
            "predicate resume completion");

    workspace.run(chain, 50);
    require(!workspace.reachable(0), "reuse clears previous reachability");
    require_equal(workspace.distance(99), test_cost{49}, "reuse new source");

    spfa_workspace<test_cost, spfa_queue_policy::slf, false> no_parent(chain.size());
    require(no_parent.run_unchecked(chain, 0) == spfa_status::completed,
            "unchecked run");
    require(no_parent.parent_vertices().empty(), "StoreParent false");

    spfa_graph<test_cost> dynamic_graph(5);
    const int edge_01 = dynamic_graph.add_edge(0, 1, 10);
    dynamic_graph.add_edge(1, 2, 10);
    dynamic_graph.add_edge(2, 3, 10);
    dynamic_graph.add_edge(3, 4, 10);
    dynamic_graph.add_edge(0, 4, 100);
    spfa_workspace<test_cost> incremental(5);
    incremental.run(dynamic_graph, 0);
    require_equal(incremental.distance(4), test_cost{40}, "incremental baseline");

    dynamic_graph.set_cost(edge_01, 1);
    require(incremental.notify_edge_added_or_decreased(dynamic_graph, edge_01),
            "notify decreased edge");
    require(incremental.propagate(dynamic_graph) == spfa_status::completed,
            "incremental propagate");
    require_equal(incremental.distance(4), test_cost{31}, "decreased edge result");

    const int edge_04 = dynamic_graph.add_edge(0, 4, 3);
    incremental.notify_edge_added_or_decreased(dynamic_graph, edge_04);
    incremental.propagate(dynamic_graph);
    require_equal(incremental.distance(4), test_cost{3}, "added edge result");

    require(incremental.notify_source_decreased(2, -20), "source label decrease");
    incremental.propagate(dynamic_graph);
    require_equal(incremental.distance(4), test_cost{0}, "source decrease result");
}

void test_implicit_solvers() {
    std::vector<std::vector<std::pair<int, test_cost>>> graph(5);
    graph[0].push_back({1, 4});
    graph[0].push_back({2, 10});
    graph[1].push_back({2, -3});
    graph[2].push_back({3, 2});
    graph[3].push_back({4, 1});
    auto expand = [&](int from, auto&& relax) {
        for (const auto& [to, cost] : graph[from]) {
            relax(to, cost);
        }
    };
    const auto result = spfa_implicit_shortest_paths<test_cost>(5, 0, expand);
    require(result.status == spfa_status::completed, "implicit completed");
    const std::vector<test_cost> expected{0, 4, 1, 3, 4};
    require(result.dist == expected, "implicit distances");

    graph[4].push_back({1, -10});
    const auto negative = spfa_implicit_shortest_paths<test_cost>(5, 0, expand);
    require(negative.status == spfa_status::negative_cycle,
            "implicit negative cycle");
    require(!negative.negative_cycle_vertices.empty(),
            "implicit negative cycle witness");

    auto sparse_expand = [](const std::string& state, auto&& relax) {
        if (state == "A") {
            relax(std::string{"B"}, test_cost{5});
            relax(std::string{"C"}, test_cost{20});
        } else if (state == "B") {
            relax(std::string{"C"}, test_cost{-2});
        } else if (state == "C") {
            relax(std::string{"D"}, test_cost{4});
        }
    };
    const auto sparse =
        spfa_sparse_implicit_shortest_paths<std::string, test_cost>(
            std::string{"A"}, 4, sparse_expand, 0, 4);
    require(sparse.status == spfa_status::completed, "sparse completed");
    const test_cost* distance_d = sparse.find_distance("D");
    require(distance_d != nullptr, "sparse state discovered");
    require_equal(*distance_d, test_cost{7}, "sparse distance");
    const std::vector<std::string> expected_path{"A", "B", "C", "D"};
    require(sparse.restore_state_path("D") == expected_path, "sparse path");

    const auto limited =
        spfa_sparse_implicit_shortest_paths<std::string, test_cost>(
            std::string{"A"}, 2, sparse_expand);
    require(limited.status == spfa_status::state_limit_reached,
            "sparse state limit");

    auto sparse_negative_expand = [](int state, auto&& relax) {
        if (state == 0) relax(1, test_cost{0});
        if (state == 1) relax(2, test_cost{-2});
        if (state == 2) relax(1, test_cost{1});
    };
    const auto sparse_negative =
        spfa_sparse_implicit_shortest_paths<int, test_cost>(
            0, 3, sparse_negative_expand);
    require(sparse_negative.status == spfa_status::negative_cycle,
            "sparse negative cycle");
    require(!sparse_negative.negative_cycle_state_ids.empty(),
            "sparse negative cycle witness");
}

void test_random_signed_graphs() {
    std::mt19937_64 rng(0x8b8b8b8b12345678ULL);
    constexpr int cases = 10000;
    for (int test_case = 0; test_case < cases; ++test_case) {
        const int n = 1 + static_cast<int>(rng() % 9ULL);
        const int max_edges = n * n;
        const int edge_count = static_cast<int>(rng() % (max_edges + 1));
        spfa_graph<test_cost> graph(n);
        for (int i = 0; i < edge_count; ++i) {
            const int from = static_cast<int>(rng() % n);
            const int to = static_cast<int>(rng() % n);
            const test_cost cost = static_cast<test_cost>(rng() % 19ULL) - 8;
            graph.add_edge(from, to, cost);
        }

        const int seed_count = 1 + static_cast<int>(rng() % n);
        std::vector<spfa_seed<test_cost>> seeds;
        seeds.reserve(seed_count);
        for (int i = 0; i < seed_count; ++i) {
            const int vertex = static_cast<int>(rng() % n);
            const test_cost distance = static_cast<test_cost>(rng() % 11ULL) - 5;
            seeds.push_back(spfa_seed<test_cost>{vertex, distance});
        }

        const auto seed_span = std::span<const spfa_seed<test_cost>>(seeds);
        const auto reference = bellman_ford_reference(graph, seed_span);
        const bool has_negative_cycle = has_negative_vertex(reference);
        graph.sort_edges_by_cost_for_slf();
        const auto fifo = spfa_shortest_paths(graph, seed_span);
        const auto slf =
            spfa_shortest_paths<test_cost, spfa_queue_policy::slf>(graph, seed_span);
        spfa_csr_graph<test_cost> csr_graph(graph);
        csr_graph.sort_edges_by_cost_for_slf();
        const auto csr = spfa_shortest_paths(csr_graph, seed_span);

        const std::string label = "random signed case " + std::to_string(test_case);
        if (has_negative_cycle) {
            validate_negative_cycle(graph, slf, label + " SLF");
            validate_negative_cycle(graph, fifo, label + " FIFO");
            validate_negative_cycle(csr_graph, csr, label + " CSR");
        } else {
            require(slf.status == spfa_status::completed, label + " SLF status");
            require(fifo.status == spfa_status::completed, label + " FIFO status");
            require(slf.dist == reference.dist, label + " SLF distance");
            require(fifo.dist == reference.dist, label + " FIFO distance");
            require(csr.status == spfa_status::completed, label + " CSR status");
            require(csr.dist == reference.dist, label + " CSR distance");
            validate_finite_paths(graph, slf, label + " paths");
        }

        const auto classified =
            spfa_shortest_paths_with_negative_infinity(graph, seed_span);
        const auto csr_classified =
            spfa_shortest_paths_with_negative_infinity(csr_graph, seed_span);
        for (int vertex = 0; vertex < n; ++vertex) {
            const int index = vertex;
            if (reference.negative[index] != 0) {
                require(classified.state[index] ==
                            spfa_distance_state::negative_infinity,
                        label + " classified -INF");
                require(csr_classified.state[index] ==
                            spfa_distance_state::negative_infinity,
                        label + " CSR classified -INF");
            } else if (reference.dist[index] == test_inf) {
                require(classified.state[index] == spfa_distance_state::unreachable,
                        label + " classified unreachable");
                require(csr_classified.state[index] ==
                            spfa_distance_state::unreachable,
                        label + " CSR classified unreachable");
            } else {
                require(classified.state[index] == spfa_distance_state::finite,
                        label + " classified finite");
                require_equal(classified.dist[index], reference.dist[index],
                              label + " classified distance");
                require(csr_classified.state[index] == spfa_distance_state::finite,
                        label + " CSR classified finite");
                require_equal(csr_classified.dist[index], reference.dist[index],
                              label + " CSR classified distance");
            }
        }

        if (test_case % 8 == 0) {
            std::vector<spfa_seed<test_cost>> all_seeds;
            all_seeds.reserve(n);
            for (int vertex = 0; vertex < n; ++vertex) {
                all_seeds.push_back(spfa_seed<test_cost>{vertex, 0});
            }
            const auto global_reference = bellman_ford_reference(
                graph, std::span<const spfa_seed<test_cost>>(all_seeds));
            const auto global_result = spfa_find_negative_cycle_anywhere(graph);
            require((global_result.status == spfa_status::negative_cycle) ==
                        has_negative_vertex(global_reference),
                    label + " global cycle status");
            if (global_result.status == spfa_status::negative_cycle) {
                validate_negative_cycle(graph, global_result, label + " global cycle");
            }
        }
    }
}

void test_random_incremental_updates() {
    std::mt19937_64 rng(0x1234fedc5678ba90ULL);
    constexpr int graph_cases = 80;
    constexpr int updates_per_graph = 120;
    for (int graph_case = 0; graph_case < graph_cases; ++graph_case) {
        const int n = 8 + static_cast<int>(rng() % 17ULL);
        spfa_graph<test_cost> graph(n);
        for (int vertex = 0; vertex + 1 < n; ++vertex) {
            graph.add_edge(vertex, vertex + 1, 1 + rng() % 20ULL);
        }
        const int extra_edges = n * 3;
        for (int i = 0; i < extra_edges; ++i) {
            const int from = static_cast<int>(rng() % n);
            const int to = static_cast<int>(rng() % n);
            const test_cost cost = 1 + rng() % 30ULL;
            graph.add_edge(from, to, cost);
        }

        spfa_workspace<test_cost, spfa_queue_policy::slf, false> incremental(n);
        incremental.run_unchecked(graph, 0);
        for (int update = 0; update < updates_per_graph; ++update) {
            int changed_edge = -1;
            if (update % 7 == 0) {
                const int from = static_cast<int>(rng() % n);
                const int to = static_cast<int>(rng() % n);
                const test_cost cost = rng() % 8ULL;
                changed_edge = graph.add_edge(from, to, cost);
            } else {
                changed_edge = static_cast<int>(rng() % graph.edge_count());
                const auto edge = graph.get_edge(changed_edge);
                const test_cost decrease = rng() % 5ULL;
                const test_cost new_cost = std::max(test_cost{0}, edge.cost - decrease);
                graph.set_cost(changed_edge, new_cost);
            }
            incremental.notify_edge_added_or_decreased_unchecked(graph, changed_edge);
            incremental.propagate_unchecked(graph);

            const auto full = spfa_shortest_paths(graph, 0);
            require(full.status == spfa_status::completed,
                    "random incremental full status");
            require(incremental.distances() == full.dist,
                    "random incremental equals full recomputation");
        }
    }
}

std::uint64_t benchmark_sink = 0;

spfa_graph<test_cost> make_potential_graph(int n, int edge_count,
                                           std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<test_cost> potential(n);
    for (test_cost& value : potential) {
        value = static_cast<test_cost>(rng() % 2001ULL) - 1000;
    }
    spfa_graph<test_cost> graph(n);

    auto add_reduced_edge = [&](int from, int to, test_cost reduced_cost) {
        const test_cost cost = reduced_cost - potential[from] +
                               potential[to];
        graph.add_edge(from, to, cost);
    };
    for (int vertex = 0; vertex < n; ++vertex) {
        add_reduced_edge(vertex, (vertex + 1) % n, rng() % 31ULL);
    }
    while (graph.edge_count() < edge_count) {
        const int from = static_cast<int>(rng() % n);
        const int to = static_cast<int>(rng() % n);
        add_reduced_edge(from, to, rng() % 101ULL);
    }
    return graph;
}

spfa_graph<test_cost> make_queue_order_stress_graph(int n) {
    spfa_graph<test_cost> graph(n);
    graph.reserve_out_edges(0, n - 1);
    for (int vertex = 1; vertex < n; ++vertex) {
        graph.add_edge(0, vertex, n - vertex);
    }
    for (int vertex = 2; vertex < n; ++vertex) {
        graph.add_edge(vertex, vertex - 1, 0);
    }
    return graph;
}

template <spfa_queue_policy QueuePolicy, class Graph>
long long benchmark_queue_policy(const Graph& graph,
                                 const std::vector<int>& sources) {
    spfa_workspace<test_cost, QueuePolicy, false> workspace(graph.size());
    const auto start = std::chrono::steady_clock::now();
    std::uint64_t checksum = 0;
    for (const int source : sources) {
        workspace.run_unchecked(graph, source);
        const test_cost value = workspace.distance((source * 8191 + 17) % graph.size());
        checksum ^= value + 0x9e3779b97f4a7c15ULL;
    }
    const auto finish = std::chrono::steady_clock::now();
    benchmark_sink = benchmark_sink * 0x9e3779b97f4a7c15ULL + checksum;
    return std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count();
}

template <spfa_queue_policy QueuePolicy, class Graph>
long long median_queue_benchmark(const Graph& graph,
                                 const std::vector<int>& sources,
                                 int repetitions) {
    std::vector<long long> times;
    times.reserve(repetitions);
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        times.push_back(benchmark_queue_policy<QueuePolicy>(graph, sources));
    }
    std::ranges::sort(times);
    return times[repetitions / 2];
}

struct queue_profile {
    std::uint64_t pops = 0;
    std::uint64_t edge_scans = 0;
    std::uint64_t successful_relaxations = 0;
};

template <spfa_queue_policy QueuePolicy>
queue_profile profile_queue_policy(const spfa_graph<test_cost>& graph,
                                   const std::vector<int>& sources) {
    const int n = graph.size();
    std::vector<test_cost> dist(n, test_inf);
    std::vector<unsigned char> in_queue(n, 0);
    std::deque<int> queue;
    queue_profile profile;

    for (const int source : sources) {
        std::ranges::fill(dist, test_inf);
        std::ranges::fill(in_queue, 0);
        queue.clear();
        dist[source] = 0;
        queue.push_back(source);
        in_queue[source] = 1;

        auto enqueue = [&](int vertex) {
            if (in_queue[vertex] != 0) return;
            if constexpr (QueuePolicy == spfa_queue_policy::slf) {
                if (!queue.empty() &&
                    dist[vertex] <
                        dist[queue.front()]) {
                    queue.push_front(vertex);
                } else {
                    queue.push_back(vertex);
                }
            } else {
                queue.push_back(vertex);
            }
            in_queue[vertex] = 1;
        };

        while (!queue.empty()) {
            const int from = queue.front();
            queue.pop_front();
            in_queue[from] = 0;
            ++profile.pops;
            for (const auto& edge : graph[from]) {
                ++profile.edge_scans;
                const test_cost candidate = dist[from] +
                                            edge.cost;
                if (!(candidate < dist[edge.to])) continue;
                dist[edge.to] = candidate;
                ++profile.successful_relaxations;
                enqueue(edge.to);
            }
        }
    }
    return profile;
}

struct incremental_benchmark_result {
    long long incremental_us;
    long long full_us;
};

incremental_benchmark_result benchmark_incremental_updates() {
    constexpr int n = 2500;
    constexpr int edge_count = 12000;
    constexpr int update_count = 240;
    std::mt19937_64 rng(0xabcdef0123456789ULL);
    spfa_graph<test_cost> base(n);
    for (int vertex = 0; vertex + 1 < n; ++vertex) {
        base.add_edge(vertex, vertex + 1, 10 + rng() % 91ULL);
    }
    while (base.edge_count() < edge_count) {
        const int from = static_cast<int>(rng() % n);
        const int to = static_cast<int>(rng() % n);
        base.add_edge(from, to, 10 + rng() % 991ULL);
    }

    std::vector<std::pair<int, test_cost>> updates;
    updates.reserve(update_count);
    spfa_graph<test_cost> planning = base;
    for (int i = 0; i < update_count; ++i) {
        const int edge_id = static_cast<int>(rng() % planning.edge_count());
        const auto edge = planning.get_edge(edge_id);
        const test_cost new_cost = std::max(test_cost{0}, edge.cost - 1);
        planning.set_cost(edge_id, new_cost);
        updates.push_back({edge_id, new_cost});
    }

    spfa_graph<test_cost> incremental_graph = base;
    spfa_workspace<test_cost, spfa_queue_policy::fifo, false> incremental(n);
    incremental.run_unchecked(incremental_graph, 0);
    const auto incremental_start = std::chrono::steady_clock::now();
    for (const auto& [edge_id, new_cost] : updates) {
        incremental_graph.set_cost(edge_id, new_cost);
        incremental.notify_edge_added_or_decreased_unchecked(incremental_graph, edge_id);
        incremental.propagate_unchecked(incremental_graph);
    }
    const auto incremental_finish = std::chrono::steady_clock::now();

    spfa_graph<test_cost> full_graph = base;
    spfa_workspace<test_cost, spfa_queue_policy::fifo, false> full(n);
    const auto full_start = std::chrono::steady_clock::now();
    for (const auto& [edge_id, new_cost] : updates) {
        full_graph.set_cost(edge_id, new_cost);
        full.run_unchecked(full_graph, 0);
    }
    const auto full_finish = std::chrono::steady_clock::now();
    require(incremental.distances() == full.distances(),
            "benchmark incremental final equality");

    benchmark_sink = benchmark_sink * 0x9e3779b97f4a7c15ULL +
                     incremental.distance(n - 1);
    benchmark_sink = benchmark_sink * 0x9e3779b97f4a7c15ULL +
                     full.distance(n - 1);
    return incremental_benchmark_result{
        std::chrono::duration_cast<std::chrono::microseconds>(
            incremental_finish - incremental_start)
            .count(),
        std::chrono::duration_cast<std::chrono::microseconds>(full_finish - full_start)
            .count(),
    };
}

incremental_benchmark_result benchmark_propagating_chain_updates() {
    constexpr int n = 20000;
    constexpr int update_count = 120;
    spfa_graph<test_cost> base(n);
    for (int vertex = 0; vertex + 1 < n; ++vertex) {
        base.add_edge(vertex, vertex + 1, 100);
    }

    std::vector<std::pair<int, test_cost>> updates;
    updates.reserve(update_count);
    for (int i = 0; i < update_count; ++i) {
        const int edge_id = (i * 7919 + 1237) % (n - 1);
        updates.push_back({edge_id, 99});
    }

    spfa_graph<test_cost> incremental_graph = base;
    spfa_workspace<test_cost, spfa_queue_policy::fifo, false> incremental(n);
    incremental.run_unchecked(incremental_graph, 0);
    const auto incremental_start = std::chrono::steady_clock::now();
    for (const auto& [edge_id, new_cost] : updates) {
        incremental_graph.set_cost(edge_id, new_cost);
        incremental.notify_edge_added_or_decreased_unchecked(incremental_graph,
                                                              edge_id);
        incremental.propagate_unchecked(incremental_graph);
    }
    const auto incremental_finish = std::chrono::steady_clock::now();

    spfa_graph<test_cost> full_graph = base;
    spfa_workspace<test_cost, spfa_queue_policy::fifo, false> full(n);
    const auto full_start = std::chrono::steady_clock::now();
    for (const auto& [edge_id, new_cost] : updates) {
        full_graph.set_cost(edge_id, new_cost);
        full.run_unchecked(full_graph, 0);
    }
    const auto full_finish = std::chrono::steady_clock::now();
    require(incremental.distances() == full.distances(),
            "benchmark propagating incremental equality");

    benchmark_sink = benchmark_sink * 0x9e3779b97f4a7c15ULL +
                     incremental.distance(n - 1);
    benchmark_sink = benchmark_sink * 0x9e3779b97f4a7c15ULL +
                     full.distance(n - 1);
    return incremental_benchmark_result{
        std::chrono::duration_cast<std::chrono::microseconds>(
            incremental_finish - incremental_start)
            .count(),
        std::chrono::duration_cast<std::chrono::microseconds>(full_finish - full_start)
            .count(),
    };
}

void run_all_tests() {
    test_graph_and_basic_shortest_paths();
    test_negative_cycles_and_classification();
    test_potential_and_difference_constraints();
    test_workspace_pause_reuse_and_incremental();
    test_implicit_solvers();
    test_random_signed_graphs();
    test_random_incremental_updates();
}

void run_benchmarks() {
    constexpr int n = 8000;
    constexpr int edge_count = 48000;
    const auto graph = make_potential_graph(n, edge_count, 0x3141592653589793ULL);
    const auto csr_build_start = std::chrono::steady_clock::now();
    const spfa_csr_graph<test_cost> csr_graph(graph);
    const auto csr_build_finish = std::chrono::steady_clock::now();
    const long long csr_build_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            csr_build_finish - csr_build_start)
            .count();
    auto sorted_csr_graph = csr_graph;
    const auto csr_sort_start = std::chrono::steady_clock::now();
    sorted_csr_graph.sort_edges_by_cost_for_slf();
    const auto csr_sort_finish = std::chrono::steady_clock::now();
    const long long csr_sort_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            csr_sort_finish - csr_sort_start)
            .count();
    std::vector<int> sources;
    for (int i = 0; i < 24; ++i) sources.push_back((i * 3253) % n);

    // 初回のページフォルトや分岐予測の影響を小さくするため両方式を一度ずつウォームアップする
    benchmark_queue_policy<spfa_queue_policy::fifo>(graph, sources);
    benchmark_queue_policy<spfa_queue_policy::slf>(graph, sources);
    benchmark_queue_policy<spfa_queue_policy::fifo>(csr_graph, sources);
    benchmark_queue_policy<spfa_queue_policy::slf>(csr_graph, sources);
    benchmark_queue_policy<spfa_queue_policy::slf>(sorted_csr_graph, sources);
    const long long fifo_us =
        median_queue_benchmark<spfa_queue_policy::fifo>(graph, sources, 5);
    const long long slf_us =
        median_queue_benchmark<spfa_queue_policy::slf>(graph, sources, 5);
    const long long csr_fifo_us =
        median_queue_benchmark<spfa_queue_policy::fifo>(csr_graph, sources, 5);
    const long long csr_slf_us =
        median_queue_benchmark<spfa_queue_policy::slf>(csr_graph, sources, 5);
    const long long sorted_csr_slf_us =
        median_queue_benchmark<spfa_queue_policy::slf>(sorted_csr_graph, sources, 5);
    const auto fifo_profile =
        profile_queue_policy<spfa_queue_policy::fifo>(graph, sources);
    const auto slf_profile =
        profile_queue_policy<spfa_queue_policy::slf>(graph, sources);

    constexpr int stress_n = 3000;
    const auto stress_graph = make_queue_order_stress_graph(stress_n);
    const std::vector<int> stress_sources{0};
    benchmark_queue_policy<spfa_queue_policy::fifo>(stress_graph, stress_sources);
    benchmark_queue_policy<spfa_queue_policy::slf>(stress_graph, stress_sources);
    const long long stress_fifo_us =
        median_queue_benchmark<spfa_queue_policy::fifo>(stress_graph,
                                                        stress_sources, 3);
    const long long stress_slf_us =
        median_queue_benchmark<spfa_queue_policy::slf>(stress_graph,
                                                       stress_sources, 3);
    const auto stress_fifo_profile =
        profile_queue_policy<spfa_queue_policy::fifo>(stress_graph, stress_sources);
    const auto stress_slf_profile =
        profile_queue_policy<spfa_queue_policy::slf>(stress_graph, stress_sources);
    const auto incremental = benchmark_incremental_updates();
    const auto propagating_incremental = benchmark_propagating_chain_updates();

    std::cout << "BENCH random_potential_graph n=" << n << " m=" << edge_count
              << " runs=" << sources.size() << '\n';
    std::cout << "  FIFO: " << fifo_us << " us\n";
    std::cout << "  SLF : " << slf_us << " us\n";
    std::cout << "  CSR FIFO: " << csr_fifo_us
              << " us (build " << csr_build_us << " us)\n";
    std::cout << "  CSR SLF: " << csr_slf_us << " us\n";
    std::cout << "  CSR SLF sorted: " << sorted_csr_slf_us
              << " us (sort " << csr_sort_us << " us)\n";
    std::cout << "  FIFO work: pops=" << fifo_profile.pops
              << " scans=" << fifo_profile.edge_scans
              << " relax=" << fifo_profile.successful_relaxations << '\n';
    std::cout << "  SLF  work: pops=" << slf_profile.pops
              << " scans=" << slf_profile.edge_scans
              << " relax=" << slf_profile.successful_relaxations << '\n';
    std::cout << "BENCH queue_order_stress n=" << stress_n
              << " m=" << stress_graph.edge_count() << '\n';
    std::cout << "  FIFO: " << stress_fifo_us << " us\n";
    std::cout << "  SLF : " << stress_slf_us << " us\n";
    std::cout << "  FIFO work: pops=" << stress_fifo_profile.pops
              << " scans=" << stress_fifo_profile.edge_scans
              << " relax=" << stress_fifo_profile.successful_relaxations << '\n';
    std::cout << "  SLF  work: pops=" << stress_slf_profile.pops
              << " scans=" << stress_slf_profile.edge_scans
              << " relax=" << stress_slf_profile.successful_relaxations << '\n';
    std::cout << "BENCH monotone_local_updates updates=240\n";
    std::cout << "  incremental: " << incremental.incremental_us << " us\n";
    std::cout << "  full rerun : " << incremental.full_us << " us\n";
    std::cout << "BENCH propagating_chain_updates n=20000 updates=120\n";
    std::cout << "  incremental: " << propagating_incremental.incremental_us
              << " us\n";
    std::cout << "  full rerun : " << propagating_incremental.full_us << " us\n";
    std::cout << "benchmark checksum: " << benchmark_sink << '\n';
}

}  // namespace spfa_selftest

int main() {
    spfa_selftest::run_all_tests();
    std::cout << "ALL TESTS PASSED\n";
    std::cout << "  deterministic API/edge-case tests: passed\n";
    std::cout << "  random signed graph comparisons: 10000 cases x FIFO/SLF/CSR/classification\n";
    std::cout << "  random monotone incremental comparisons: 80 x 120 updates\n";
    spfa_selftest::run_benchmarks();
}
#endif

// 実行結果(atcoder)
// ALL TESTS PASSED
//   deterministic API/edge-case tests: passed
//   random signed graph comparisons: 10000 cases x FIFO/SLF/CSR/classification
//   random monotone incremental comparisons: 80 x 120 updates
// BENCH random_potential_graph n=8000 m=48000 runs=24
//   FIFO: 22198 us
//   SLF : 36794 us
//   CSR FIFO: 20186 us (build 1096 us)
//   CSR SLF: 33698 us
//   CSR SLF sorted: 30332 us (sort 452 us)
//   FIFO work: pops=496641 scans=2980978 relax=726055
//   SLF  work: pops=719116 scans=4318341 relax=1228096
// BENCH queue_order_stress n=3000 m=5997
//   FIFO: 22671 us
//   SLF : 20 us
//   FIFO work: pops=4498501 scans=4498500 relax=4498500
//   SLF  work: pops=3000 scans=5997 relax=5997
// BENCH monotone_local_updates updates=240
//   incremental: 17 us
//   full rerun : 49226 us
// BENCH propagating_chain_updates n=20000 updates=120
//   incremental: 7720 us
//   full rerun : 18291 us
// benchmark checksum: 5138074156578421226
