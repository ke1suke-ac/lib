#pragma once

#include <bits/stdc++.h>
using namespace std;

using ll = long long;

// 一般グラフの最大重みマッチング - Edmonds の Blossom アルゴリズムの重み付き版
// ◆注意点
// 1-indexed
// 重み0の辺や、負辺は扱えない
// ◆最大マッチングの中で最小重みを求めるには
// LARGE = max_cost * n + 1
// として、add_edge時の各重みを LARGE - w とする
// この時、最小コストは以下のように求まる
// auto [total_w_prime, pair_count] = matching_solver.solve();  min_total_cost = 1LL * pair_count * LARGE -
// total_w_prime;
template <int MAX_N> struct WeightedMatch {
    // 無向グラフの辺を表す構造体
    struct Edge {
        int u;
        int v;
        int w;
    };

    Edge edge_matrix[MAX_N * 2][MAX_N * 2];
    int node_count;
    int extended_node_count;
    int label[MAX_N * 2];
    int mate[MAX_N * 2];  // 結果が入る(1-indexed)。0はペアなし
    int slack[MAX_N * 2];
    int blossom_rep[MAX_N * 2];
    int parent[MAX_N * 2];
    int blossom_from[MAX_N * 2][MAX_N];
    int state[MAX_N * 2];
    int visited_token[MAX_N * 2];
    vector<int> blossom_vertices[MAX_N * 2];
    queue<int> bfs_queue;

    // グラフを初期化する。頂点数 n (1..n) をセットし、全ての辺の重みを 0 にする。
    void init_graph(int n) {
        assert(n < MAX_N);

        node_count = n;
        for (int u = 1; u <= node_count; ++u) {
            for (int v = 1; v <= node_count; ++v) {
                edge_matrix[u][v] = {u, v, 0};
            }
        }
    }

    // 頂点 u, v 間に重み w の無向辺を追加する（頂点は 1-indexed）。w > 0 (0以下だとその辺は無視される)
    void add_edge(int u, int v, int w) {
        assert(u > 0 && v > 0);

        edge_matrix[u][v].w = w;
        edge_matrix[v][u].w = w;
    }

    // 最大重みマッチングを計算し、{合計重み, 辺数} を返す。
    pair<ll, int> solve() {
        extended_node_count = node_count;
        blossom_rep[0] = 0;
        for (int i = 1; i <= 2 * node_count; ++i) {
            visited_token[i] = 0;
        }
        for (int i = 1; i <= node_count; ++i) {
            mate[i] = 0;
            blossom_rep[i] = i;
            blossom_vertices[i].clear();
        }

        int max_weight = 0;
        for (int u = 1; u <= node_count; ++u) {
            for (int v = 1; v <= node_count; ++v) {
                blossom_from[u][v] = (u == v ? u : 0);
                max_weight = max(max_weight, edge_matrix[u][v].w);
            }
        }
        for (int u = 1; u <= node_count; ++u) {
            label[u] = max_weight;
        }

        int matched_edge_count = 0;
        ll total_weight = 0;

        while (find_augmenting_path()) {
            ++matched_edge_count;
        }

        for (int u = 1; u <= node_count; ++u) {
            if (mate[u] && mate[u] < u) {
                total_weight += edge_matrix[u][mate[u]].w;
            }
        }

        return {total_weight, matched_edge_count};
    }

   private:
    // 辺 e に対するラベルからの余裕量（tight なとき 0）を計算する。
    int edge_delta(const Edge& e) { return label[e.u] + label[e.v] - edge_matrix[e.u][e.v].w * 2; }

    // 頂点 u から blossom/頂点 x への slack を更新する。
    void update_slack(int u, int x) {
        if (!slack[x] || edge_delta(edge_matrix[u][x]) < edge_delta(edge_matrix[slack[x]][x])) {
            slack[x] = u;
        }
    }

    // blossom/頂点 x について、現在のラベルに対する最小 slack を再計算する。
    void recompute_slack(int x) {
        slack[x] = 0;
        for (int u = 1; u <= node_count; ++u) {
            if (edge_matrix[u][x].w > 0 && blossom_rep[u] != x && state[blossom_rep[u]] == 0) {
                update_slack(u, x);
            }
        }
    }

    // 頂点または blossom x を BFS キューに追加する。
    void enqueue_vertex(int x) {
        if (x <= node_count) {
            bfs_queue.push(x);
        } else {
            for (int t : blossom_vertices[x]) {
                enqueue_vertex(t);
            }
        }
    }

    // 頂点 x の代表 blossom を b に設定する。
    void set_blossom_rep(int x, int b) {
        blossom_rep[x] = b;
        if (x > node_count) {
            for (int t : blossom_vertices[x]) {
                set_blossom_rep(t, b);
            }
        }
    }

    // blossom b 内で頂点 xr が現れる偶数位置（0-indexed）を取得する。
    int get_blossom_position(int b, int xr) {
        auto& vertices = blossom_vertices[b];
        int pos = static_cast<int>(find(vertices.begin(), vertices.end(), xr) - vertices.begin());
        if (pos & 1) {
            reverse(vertices.begin() + 1, vertices.end());
            return static_cast<int>(vertices.size()) - pos;
        }
        return pos;
    }

    // 頂点 u と v をマッチさせ、必要に応じて blossom 内部のマッチングを再配置する。
    void set_mate(int u, int v) {
        Edge e = edge_matrix[u][v];
        mate[u] = e.v;
        if (u <= node_count) {
            return;
        }
        int xr = blossom_from[u][e.u];
        int pos = get_blossom_position(u, xr);
        auto& vertices = blossom_vertices[u];
        for (int i = 0; i < pos; ++i) {
            int x = vertices[i];
            int y = vertices[i ^ 1];
            set_mate(x, y);
        }
        set_mate(xr, v);
        rotate(vertices.begin(), vertices.begin() + pos, vertices.end());
    }

    // u-v を含む増加路でマッチングを反転させる。
    void augment_matching(int u, int v) {
        while (true) {
            int next = blossom_rep[mate[u]];
            set_mate(u, v);
            if (!next) {
                return;
            }
            int parent_vertex = blossom_rep[parent[next]];
            set_mate(next, parent_vertex);
            u = parent_vertex;
            v = next;
        }
    }

    // 2 つの頂点 u, v の最近共通祖先（blossom の基点）を求める。
    int find_lca(int u, int v) {
        thread_local static int current_token = 0;
        ++current_token;
        while (u || v) {
            if (u) {
                if (visited_token[u] == current_token) {
                    return u;
                }
                visited_token[u] = current_token;
                u = blossom_rep[mate[u]];
                if (u) {
                    u = blossom_rep[parent[u]];
                }
            }
            swap(u, v);
        }
        return 0;
    }

    // u-anc-v の奇サイクルから新しい blossom を構築する。
    void add_blossom(int u, int anc, int v) {
        int b = node_count + 1;
        while (b <= extended_node_count && blossom_rep[b]) {
            ++b;
        }
        if (b > extended_node_count) {
            ++extended_node_count;
        }

        label[b] = 0;
        state[b] = 0;
        mate[b] = mate[anc];
        auto& vertices = blossom_vertices[b];
        vertices.clear();
        vertices.push_back(anc);

        auto build_path = [&](int x) {
            for (int y; x != anc; x = blossom_rep[parent[y]]) {
                vertices.push_back(x);
                y = blossom_rep[mate[x]];
                vertices.push_back(y);
                enqueue_vertex(y);
            }
        };

        build_path(u);
        if (vertices.size() > 1) {
            reverse(vertices.begin() + 1, vertices.end());
        }
        build_path(v);
        set_blossom_rep(b, b);

        for (int x = 1; x <= extended_node_count; ++x) {
            edge_matrix[b][x].w = 0;
            edge_matrix[x][b].w = 0;
        }
        for (int x = 1; x <= node_count; ++x) {
            blossom_from[b][x] = 0;
        }

        for (int xs : vertices) {
            for (int x = 1; x <= extended_node_count; ++x) {
                if (edge_matrix[b][x].w == 0 || edge_delta(edge_matrix[xs][x]) < edge_delta(edge_matrix[b][x])) {
                    edge_matrix[b][x] = edge_matrix[xs][x];
                    edge_matrix[x][b] = edge_matrix[x][xs];
                }
            }
            for (int x = 1; x <= node_count; ++x) {
                if (blossom_from[xs][x]) {
                    blossom_from[b][x] = xs;
                }
            }
        }

        recompute_slack(b);
    }

    // blossom b を展開し、元の頂点に戻す。
    void expand_blossom(int b) {
        auto& vertices = blossom_vertices[b];
        for (int t : vertices) {
            set_blossom_rep(t, t);
        }

        int xr = blossom_from[b][edge_matrix[b][parent[b]].u];
        int pos = get_blossom_position(b, xr);

        for (int i = 0; i < pos; i += 2) {
            int xs = vertices[i];
            int xns = vertices[i + 1];
            parent[xs] = edge_matrix[xns][xs].u;
            state[xs] = 1;
            state[xns] = 0;
            slack[xs] = 0;
            slack[xns] = 0;
            enqueue_vertex(xns);
        }

        state[xr] = 1;
        parent[xr] = parent[b];

        for (int i = pos + 1; i < static_cast<int>(vertices.size()); ++i) {
            int xs = vertices[i];
            state[xs] = -1;
            recompute_slack(xs);
        }

        blossom_rep[b] = 0;
    }

    // tight な辺 e を見つけたときの処理を行う。
    bool on_found_edge(const Edge& e) {
        int u = blossom_rep[e.u];
        int v = blossom_rep[e.v];

        if (state[v] == -1) {
            parent[v] = e.u;
            state[v] = 1;
            slack[v] = 0;
            int matched_vertex = blossom_rep[mate[v]];
            state[matched_vertex] = 0;
            slack[matched_vertex] = 0;
            enqueue_vertex(matched_vertex);
        } else if (state[v] == 0) {
            int anc = find_lca(u, v);
            if (!anc) {
                augment_matching(u, v);
                augment_matching(v, u);
                return true;
            }
            add_blossom(u, anc, v);
        }
        return false;
    }

    // 1 回の BFS / ラベル更新で増加路を 1 本見つけ、マッチングを更新する。
    bool find_augmenting_path() {
        bfs_queue = queue<int>();

        for (int x = 1; x <= extended_node_count; ++x) {
            state[x] = -1;
            slack[x] = 0;
        }

        for (int x = 1; x <= extended_node_count; ++x) {
            if (blossom_rep[x] == x && !mate[x]) {
                parent[x] = 0;
                state[x] = 0;
                enqueue_vertex(x);
            }
        }

        if (bfs_queue.empty()) {
            return false;
        }

        while (true) {
            while (!bfs_queue.empty()) {
                int u = bfs_queue.front();
                bfs_queue.pop();
                if (state[blossom_rep[u]] == 1) {
                    continue;
                }
                for (int v = 1; v <= node_count; ++v) {
                    if (edge_matrix[u][v].w > 0 && blossom_rep[u] != blossom_rep[v]) {
                        if (edge_delta(edge_matrix[u][v]) == 0) {
                            if (on_found_edge(edge_matrix[u][v])) {
                                return true;
                            }
                        } else {
                            update_slack(u, blossom_rep[v]);
                        }
                    }
                }
            }

            int d = INT_MAX;
            for (int b = node_count + 1; b <= extended_node_count; ++b) {
                if (blossom_rep[b] == b && state[b] == 1) {
                    d = min(d, label[b] / 2);
                }
            }
            for (int x = 1; x <= extended_node_count; ++x) {
                if (blossom_rep[x] == x && slack[x]) {
                    if (state[x] == -1) {
                        d = min(d, edge_delta(edge_matrix[slack[x]][x]));
                    } else if (state[x] == 0) {
                        d = min(d, edge_delta(edge_matrix[slack[x]][x]) / 2);
                    }
                }
            }

            for (int u = 1; u <= node_count; ++u) {
                if (state[blossom_rep[u]] == 0) {
                    if (label[u] <= d) {
                        return false;
                    }
                    label[u] -= d;
                } else if (state[blossom_rep[u]] == 1) {
                    label[u] += d;
                }
            }

            for (int b = node_count + 1; b <= extended_node_count; ++b) {
                if (blossom_rep[b] == b && state[b] != -1) {
                    label[b] += (state[b] == 0 ? 1 : -1) * d * 2;
                }
            }

            bfs_queue = queue<int>();

            for (int x = 1; x <= extended_node_count; ++x) {
                if (blossom_rep[x] == x && slack[x] && blossom_rep[slack[x]] != x &&
                    edge_delta(edge_matrix[slack[x]][x]) == 0) {
                    if (on_found_edge(edge_matrix[slack[x]][x])) {
                        return true;
                    }
                }
            }

            for (int b = node_count + 1; b <= extended_node_count; ++b) {
                if (blossom_rep[b] == b && state[b] == 1 && label[b] == 0) {
                    expand_blossom(b);
                }
            }
        }

        return false;
    }
};

#if 0
// 最大重みマッチングのソルバ。
// init_graph(), add_edge(), solve() を順に呼び出して利用する。
constexpr int MAX_NODE_COUNT = 501;
WeightedMatch<MAX_NODE_COUNT> matching_solver;

// メイン関数。入力されたグラフに対して最大重みマッチングを実行し、結果を出力する。
int main() {
    cin.tie(0)->sync_with_stdio(0);

    int node_count, edge_count;
    cin >> node_count >> edge_count;

    matching_solver.init_graph(node_count);

    for (int i = 0; i < edge_count; ++i) {
        int from, to, weight;
        cin >> from >> to >> weight;
        ++from;
        ++to;
        matching_solver.add_edge(from, to, weight);
    }

    auto [total_weight, matching_size] = matching_solver.solve();
    cout << matching_size << ' ' << total_weight << '\n';

    for (int i = 1; i <= node_count; ++i) {
        int mate_vertex = matching_solver.mate[i];
        if (mate_vertex && i < mate_vertex) {
            cout << (i - 1) << ' ' << (mate_vertex - 1) << '\n';
        }
    }

    return 0;
}
#endif
