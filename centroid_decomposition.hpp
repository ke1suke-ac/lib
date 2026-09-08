#pragma once
#include <bits/stdc++.h>

#include <atcoder/fenwicktree>
#include <atcoder/segtree>
#include <atcoder/convolution>

// 木の重心分解を用いる競技プログラミング向けデータ構造と代表的ユースケースをまとめたヘッダ
// Fenwick Tree・Segment Tree・畳み込みはAtCoder Libraryを利用し、削除可能priority queueだけを共通実装する

using namespace std;
using namespace atcoder;


// ============================================================================
// 共通データ構造
// ============================================================================

// 同値要素の重複を扱える遅延削除付きpriority queue
template <class T, class Compare = less<T>>
struct DeletablePriorityQueue {
    priority_queue<T, vector<T>, Compare> added;
    priority_queue<T, vector<T>, Compare> removed;
    long long element_count = 0;

    // 要素を追加する O(log n)
    void push(const T& value) {
        added.push(value);
        ++element_count;
    }

    // 現在含まれる要素を1個削除予約する O(log n)
    void erase(const T& value) {
        assert(element_count > 0);
        removed.push(value);
        --element_count;
    }

    // 優先度最大の要素を返す 償却O(log n)
    T top() {
        normalize();
        assert(element_count > 0);
        return added.top();
    }

    // 優先度最大の要素を1個削除する 償却O(log n)
    void pop() {
        normalize();
        assert(element_count > 0);
        added.pop();
        --element_count;
    }

    // 論理的に空か判定する O(1)
    bool empty() const {
        return element_count == 0;
    }

    // 論理要素数を返す O(1)
    long long size() const {
        return element_count;
    }

    // 全要素を削除する O(n)
    void clear() {
        added = {};
        removed = {};
        element_count = 0;
    }

private:
    void normalize() {
        while (!added.empty() && !removed.empty() && added.top() == removed.top()) {
            added.pop();
            removed.pop();
        }
    }
};

// ============================================================================
// データ構造
// ============================================================================

// 重心分解木の親・深さ・構築順を求めるデータ構造
struct CentroidDecompositionTree {
    int n;
    int edge_count = 0;
    vector<vector<int>> graph;
    vector<int> centroid_parent;
    vector<int> centroid_depth;
    vector<int> centroid_order;

    // n頂点の空の木を構築する O(n)
    explicit CentroidDecompositionTree(int vertex_count)
        : n(vertex_count), graph(vertex_count), centroid_parent(vertex_count, -1), centroid_depth(vertex_count, -1),
          removed(vertex_count, false), parent_work(vertex_count, -1), subtree_size(vertex_count, 0) {}

    // 無向辺u-vを追加する O(1)
    void add_edge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        graph[u].push_back(v);
        graph[v].push_back(u);
        ++edge_count;
    }

    // 重心分解木を構築する O(n log n)
    void build() {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        fill(centroid_parent.begin(), centroid_parent.end(), -1);
        fill(centroid_depth.begin(), centroid_depth.end(), -1);
        centroid_order.clear();
        if (n == 0) return;

        // 未処理成分を再帰的に分解する
        auto decompose = [&](auto&& self, int start, int parent_centroid) -> void {
            if (removed[start]) return;

            // 現在成分の重心を確定し、分解木へ登録する
            int centroid = find_centroid(start);
            centroid_parent[centroid] = parent_centroid;
            centroid_depth[centroid] = parent_centroid == -1 ? 0 : centroid_depth[parent_centroid] + 1;
            centroid_order.push_back(centroid);
            removed[centroid] = true;

            // 重心を除いてできる各成分を再帰的に処理する
            for (int next_start : graph[centroid]) {
                if (!removed[next_start]) self(self, next_start, centroid);
            }
        };
        decompose(decompose, 0, -1);
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (int to : graph[v]) {
                if (removed[to] || to == parent) continue;
                self(self, to, v);
                subtree_size[v] += subtree_size[to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (int to : graph[v]) {
                if (!removed[to] && parent_work[to] == v) max_part = max(max_part, subtree_size[to]);
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }
};

// 各頂点から全重心祖先への距離と枝番号を構築する低レベルデータ構造
struct CentroidDecompositionPathIndex {
    using ll = long long;

    struct Edge {
        int to;
        ll weight;
    };

    struct PathEntry {
        int centroid;
        int branch;
        ll distance;
    };

    int n;
    int edge_count = 0;
    vector<vector<Edge>> graph;
    vector<int> centroid_parent;
    vector<int> centroid_depth;
    vector<vector<PathEntry>> path;

    // n頂点の空の重み付き木を構築する O(n)
    explicit CentroidDecompositionPathIndex(int vertex_count)
        : n(vertex_count), graph(vertex_count), centroid_parent(vertex_count, -1), centroid_depth(vertex_count, -1), path(vertex_count),
          removed(vertex_count, false), parent_work(vertex_count, -1), subtree_size(vertex_count, 0) {}

    // 重みweightの無向辺u-vを追加する O(1)
    void add_edge(int u, int v, ll weight = 1) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        graph[u].push_back({v, weight});
        graph[v].push_back({u, weight});
        ++edge_count;
    }

    // 重心祖先・距離・枝番号を構築する O(n log n)
    void build() {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        fill(centroid_parent.begin(), centroid_parent.end(), -1);
        fill(centroid_depth.begin(), centroid_depth.end(), -1);
        for (auto& entries : path) entries.clear();
        if (n == 0) return;

        // 未処理成分を再帰的に分解する
        auto decompose = [&](auto&& self, int start, int parent_centroid) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            centroid_parent[centroid] = parent_centroid;
            centroid_depth[centroid] = parent_centroid == -1 ? 0 : centroid_depth[parent_centroid] + 1;
            removed[centroid] = true;

            // 重心自身と各枝内の頂点へ、この重心に対応する情報を追加する
            path[centroid].push_back({centroid, -1, 0});
            int branch = 0;
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                add_branch_entries(centroid, first.to, branch, first.weight);
                ++branch;
            }
            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to, centroid);
            }
        };
        decompose(decompose, 0, -1);
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    void add_branch_entries(int centroid, int start, int branch, ll initial_distance) {
        auto dfs = [&](auto&& self, int v, int parent, ll distance) -> void {
            path[v].push_back({centroid, branch, distance});
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v, distance + edge.weight);
            }
        };
        dfs(dfs, start, centroid, initial_distance);
    }
};

// 動的な有効頂点集合への最近傍距離を管理するデータ構造
struct CentroidDecompositionNearestActive {
    using ll = long long;
    static constexpr ll INF = numeric_limits<ll>::max() / 4;

    struct Edge {
        int to;
        ll weight;
    };

    struct PathEntry {
        int centroid;
        ll distance;
    };

    int n;
    int edge_count = 0;
    vector<vector<Edge>> graph;
    vector<vector<PathEntry>> path;

    // n頂点の全頂点無効な重み付き木を構築する O(n)
    explicit CentroidDecompositionNearestActive(int vertex_count)
        : n(vertex_count), graph(vertex_count), path(vertex_count), removed(vertex_count, false), parent_work(vertex_count, -1),
          subtree_size(vertex_count, 0), active(vertex_count, false), nearest(vertex_count) {}

    // 非負重みweightの無向辺u-vを追加する O(1)
    void add_edge(int u, int v, ll weight = 1) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v && weight >= 0);
        graph[u].push_back({v, weight});
        graph[v].push_back({u, weight});
        ++edge_count;
    }

    // 重心祖先情報を構築して全頂点を無効化する O(n log n)
    void build() {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        fill(active.begin(), active.end(), false);
        for (auto& entries : path) entries.clear();
        for (auto& queue : nearest) queue.clear();
        if (n == 0) return;

        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            path[centroid].push_back({centroid, 0});
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                add_branch_entries(centroid, first.to, first.weight);
            }
            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to);
            }
        };
        decompose(decompose, 0);
    }

    // 頂点vを有効化する O(log^2 n)
    void activate(int v) {
        assert(0 <= v && v < n);
        if (active[v]) return;
        active[v] = true;
        for (const PathEntry& entry : path[v]) nearest[entry.centroid].push(entry.distance);
    }

    // 頂点vを無効化する O(log^2 n)
    void deactivate(int v) {
        assert(0 <= v && v < n);
        if (!active[v]) return;
        active[v] = false;
        for (const PathEntry& entry : path[v]) nearest[entry.centroid].erase(entry.distance);
    }

    // 頂点vの有効・無効を反転する O(log^2 n)
    void toggle(int v) {
        assert(0 <= v && v < n);
        if (active[v]) deactivate(v);
        else activate(v);
    }

    // 頂点vから最近傍の有効頂点までの距離を返し、存在しなければ-1を返す 償却O(log^2 n)
    ll nearest_distance(int v) {
        assert(0 <= v && v < n);
        ll answer = INF;
        for (const PathEntry& entry : path[v]) {
            auto& queue = nearest[entry.centroid];
            if (!queue.empty()) answer = min(answer, entry.distance + queue.top());
        }
        return answer == INF ? -1 : answer;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;
    vector<char> active;
    vector<DeletablePriorityQueue<ll, greater<ll>>> nearest;

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    void add_branch_entries(int centroid, int start, ll initial_distance) {
        auto dfs = [&](auto&& self, int v, int parent, ll distance) -> void {
            path[v].push_back({centroid, distance});
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v, distance + edge.weight);
            }
        };
        dfs(dfs, start, centroid, initial_distance);
    }
};

// 動的な有効頂点数を距離条件付きで数えるデータ構造
struct CentroidDecompositionActiveDistanceCount {
    using ll = long long;

    struct Edge {
        int to;
        ll weight;
    };

    struct PathEntry {
        int centroid;
        int branch;
        ll distance;
    };

    struct Bucket {
        vector<ll> all_coordinates;
        vector<vector<ll>> branch_coordinates;
        fenwick_tree<int> all_count;
        vector<fenwick_tree<int>> branch_count;
    };

    int n;
    int edge_count = 0;
    vector<vector<Edge>> graph;
    vector<vector<PathEntry>> path;

    // n頂点の全頂点無効な重み付き木を構築する O(n)
    explicit CentroidDecompositionActiveDistanceCount(int vertex_count)
        : n(vertex_count), graph(vertex_count), path(vertex_count), buckets(vertex_count), removed(vertex_count, false), parent_work(vertex_count, -1),
          subtree_size(vertex_count, 0), active(vertex_count, false) {}

    // 非負重みweightの無向辺u-vを追加する O(1)
    void add_edge(int u, int v, ll weight = 1) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v && weight >= 0);
        graph[u].push_back({v, weight});
        graph[v].push_back({u, weight});
        ++edge_count;
    }

    // 距離座標と重心祖先情報を構築して全頂点を無効化する O(n log^2 n)
    void build() {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        fill(active.begin(), active.end(), false);
        for (auto& entries : path) entries.clear();
        for (Bucket& bucket : buckets) {
            bucket.all_coordinates.clear();
            bucket.branch_coordinates.clear();
            bucket.all_count = fenwick_tree<int>(0);
            bucket.branch_count.clear();
        }
        if (n == 0) return;

        // 各重心について全体距離列と枝別距離列を収集する
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            path[centroid].push_back({centroid, -1, 0});
            buckets[centroid].all_coordinates.push_back(0);
            int branch = 0;
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                buckets[centroid].branch_coordinates.emplace_back();
                add_branch_entries(centroid, first.to, branch, first.weight);
                ++branch;
            }
            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to);
            }
        };
        decompose(decompose, 0);

        // 座標圧縮後の長さでFenwick Treeを初期化する
        for (Bucket& bucket : buckets) {
            sort_unique(bucket.all_coordinates);
            bucket.all_count = fenwick_tree<int>(static_cast<int>(bucket.all_coordinates.size()));
            bucket.branch_count.reserve(bucket.branch_coordinates.size());
            for (auto& coordinates : bucket.branch_coordinates) {
                sort_unique(coordinates);
                bucket.branch_count.emplace_back(static_cast<int>(coordinates.size()));
            }
        }
    }

    // 頂点vを有効化する O(log^2 n)
    void activate(int v) {
        assert(0 <= v && v < n);
        if (active[v]) return;
        active[v] = true;
        update_vertex(v, 1);
    }

    // 頂点vを無効化する O(log^2 n)
    void deactivate(int v) {
        assert(0 <= v && v < n);
        if (!active[v]) return;
        active[v] = false;
        update_vertex(v, -1);
    }

    // 頂点vの有効・無効を反転する O(log^2 n)
    void toggle(int v) {
        assert(0 <= v && v < n);
        if (active[v]) deactivate(v);
        else activate(v);
    }

    // 頂点vから距離max_distance以内の有効頂点数を返す O(log^2 n)
    long long count_within(int v, ll max_distance) {
        assert(0 <= v && v < n);
        long long answer = 0;
        for (const PathEntry& entry : path[v]) {
            ll remain = max_distance - entry.distance;
            Bucket& bucket = buckets[entry.centroid];
            answer += prefix_count(bucket.all_coordinates, bucket.all_count, remain);
            if (entry.branch != -1) {
                answer -= prefix_count(bucket.branch_coordinates[entry.branch],
                                       bucket.branch_count[entry.branch], remain);
            }
        }
        return answer;
    }

    // 頂点vから距離[min_distance, max_distance]の有効頂点数を返す O(log^2 n)
    long long count_in_range(int v, ll min_distance, ll max_distance) {
        assert(0 <= v && v < n);
        if (min_distance > max_distance) return 0;
        long long below = min_distance == numeric_limits<ll>::min()
                              ? 0
                              : count_within(v, min_distance - 1);
        return count_within(v, max_distance) - below;
    }

    // 頂点vから距離distanceちょうどの有効頂点数を返す O(log^2 n)
    long long count_at_distance(int v, ll distance) {
        return count_in_range(v, distance, distance);
    }

private:
    vector<Bucket> buckets;
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;
    vector<char> active;

    static void sort_unique(vector<ll>& values) {
        sort(values.begin(), values.end());
        values.erase(unique(values.begin(), values.end()), values.end());
    }

    static long long prefix_count(const vector<ll>& coordinates, fenwick_tree<int>& bit, ll value) {
        int right = static_cast<int>(upper_bound(coordinates.begin(), coordinates.end(), value) - coordinates.begin());
        return bit.sum(0, right);
    }

    void update_vertex(int v, int delta) {
        for (const PathEntry& entry : path[v]) {
            Bucket& bucket = buckets[entry.centroid];
            int all_index = static_cast<int>(
                lower_bound(bucket.all_coordinates.begin(), bucket.all_coordinates.end(), entry.distance)
                - bucket.all_coordinates.begin());
            bucket.all_count.add(all_index, delta);
            if (entry.branch != -1) {
                auto& coordinates = bucket.branch_coordinates[entry.branch];
                int branch_index = static_cast<int>(
                    lower_bound(coordinates.begin(), coordinates.end(), entry.distance) - coordinates.begin());
                bucket.branch_count[entry.branch].add(branch_index, delta);
            }
        }
    }

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    void add_branch_entries(int centroid, int start, int branch, ll initial_distance) {
        auto dfs = [&](auto&& self, int v, int parent, ll distance) -> void {
            path[v].push_back({centroid, branch, distance});
            buckets[centroid].all_coordinates.push_back(distance);
            buckets[centroid].branch_coordinates[branch].push_back(distance);
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v, distance + edge.weight);
            }
        };
        dfs(dfs, start, centroid, initial_distance);
    }
};

// 固定色ごとに動的な有効頂点への最近傍距離を管理するデータ構造
struct CentroidDecompositionNearestActiveByColor {
    using ll = long long;
    static constexpr ll INF = numeric_limits<ll>::max() / 4;

    struct Edge {
        int to;
        ll weight;
    };

    struct PathEntry {
        int centroid;
        ll distance;
        int bucket;
    };

    int n;
    int edge_count = 0;
    vector<int> color;
    vector<vector<Edge>> graph;
    vector<vector<PathEntry>> path;

    // 固定色colorsを持つn頂点の全頂点無効な木を構築する O(n)
    explicit CentroidDecompositionNearestActiveByColor(const vector<int>& colors)
        : n(static_cast<int>(colors.size())), color(colors), graph(n), path(n), removed(n, false),
          parent_work(n, -1), subtree_size(n, 0), active(n, false) {}

    // 非負重みweightの無向辺u-vを追加する O(1)
    void add_edge(int u, int v, ll weight = 1) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v && weight >= 0);
        graph[u].push_back({v, weight});
        graph[v].push_back({u, weight});
        ++edge_count;
    }

    // 色別バケットと重心祖先情報を構築して全頂点を無効化する O(n log^2 n)
    void build() {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        fill(active.begin(), active.end(), false);
        for (auto& entries : path) entries.clear();
        bucket_keys.clear();
        buckets.clear();
        if (n == 0) return;

        // 各頂点の重心祖先と距離を列挙する
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            path[centroid].push_back({centroid, 0, -1});
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                add_branch_entries(centroid, first.to, first.weight);
            }
            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to);
            }
        };
        decompose(decompose, 0);

        // (重心,色)をソートして連続したバケット番号へ変換する
        struct Ref {
            int centroid;
            int color;
            int vertex;
            int position;
        };
        vector<Ref> refs;
        for (int v = 0; v < n; ++v) {
            for (int i = 0; i < static_cast<int>(path[v].size()); ++i) {
                refs.push_back({path[v][i].centroid, color[v], v, i});
            }
        }
        sort(refs.begin(), refs.end(), [](const Ref& a, const Ref& b) {
            return tie(a.centroid, a.color) < tie(b.centroid, b.color);
        });
        pair<int, int> previous = {-1, 0};
        int bucket = -1;
        for (const Ref& ref : refs) {
            pair<int, int> key = {ref.centroid, ref.color};
            if (bucket == -1 || key != previous) {
                previous = key;
                bucket_keys.push_back(key);
                buckets.emplace_back();
                ++bucket;
            }
            path[ref.vertex][ref.position].bucket = bucket;
        }
    }

    // 頂点vを有効化する O(log^2 n)
    void activate(int v) {
        assert(0 <= v && v < n);
        if (active[v]) return;
        active[v] = true;
        for (const PathEntry& entry : path[v]) buckets[entry.bucket].push(entry.distance);
    }

    // 頂点vを無効化する O(log^2 n)
    void deactivate(int v) {
        assert(0 <= v && v < n);
        if (!active[v]) return;
        active[v] = false;
        for (const PathEntry& entry : path[v]) buckets[entry.bucket].erase(entry.distance);
    }

    // 頂点vの有効・無効を反転する O(log^2 n)
    void toggle(int v) {
        assert(0 <= v && v < n);
        if (active[v]) deactivate(v);
        else activate(v);
    }

    // 頂点vから指定色target_colorの最近傍有効頂点までの距離を返し、存在しなければ-1を返す 償却O(log^2 n)
    ll nearest_distance(int v, int target_color) {
        assert(0 <= v && v < n);
        ll answer = INF;
        for (const PathEntry& entry : path[v]) {
            int bucket = find_bucket(entry.centroid, target_color);
            if (bucket != -1 && !buckets[bucket].empty()) {
                answer = min(answer, entry.distance + buckets[bucket].top());
            }
        }
        return answer == INF ? -1 : answer;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;
    vector<char> active;
    vector<pair<int, int>> bucket_keys;
    vector<DeletablePriorityQueue<ll, greater<ll>>> buckets;

    int find_bucket(int centroid, int target_color) const {
        pair<int, int> key = {centroid, target_color};
        auto it = lower_bound(bucket_keys.begin(), bucket_keys.end(), key);
        if (it == bucket_keys.end() || *it != key) return -1;
        return static_cast<int>(it - bucket_keys.begin());
    }

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    void add_branch_entries(int centroid, int start, ll initial_distance) {
        auto dfs = [&](auto&& self, int v, int parent, ll distance) -> void {
            path[v].push_back({centroid, distance, -1});
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v, distance + edge.weight);
            }
        };
        dfs(dfs, start, centroid, initial_distance);
    }
};

// 動的な有効頂点集合の直径を管理するデータ構造
struct CentroidDecompositionActiveDiameter {
    using ll = long long;

    struct Edge {
        int to;
        ll weight;
    };

    struct PathEntry {
        int centroid;
        int group;
        ll distance;
    };

    struct Bucket {
        vector<DeletablePriorityQueue<ll>> group_values;
        DeletablePriorityQueue<pair<ll, int>> group_best;
        ll candidate = -1;
    };

    int n;
    int edge_count = 0;
    vector<vector<Edge>> graph;
    vector<vector<PathEntry>> path;

    // n頂点の全頂点無効な重み付き木を構築する O(n)
    explicit CentroidDecompositionActiveDiameter(int vertex_count)
        : n(vertex_count), graph(vertex_count), path(vertex_count), buckets(vertex_count), removed(vertex_count, false), parent_work(vertex_count, -1),
          subtree_size(vertex_count, 0), active(vertex_count, false) {}

    // 非負重みweightの無向辺u-vを追加する O(1)
    void add_edge(int u, int v, ll weight = 1) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v && weight >= 0);
        graph[u].push_back({v, weight});
        graph[v].push_back({u, weight});
        ++edge_count;
    }

    // 枝別最大距離と重心祖先情報を構築して全頂点を無効化する O(n log n)
    void build() {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        fill(active.begin(), active.end(), false);
        active_count = 0;
        global_candidates.clear();
        for (auto& entries : path) entries.clear();
        for (Bucket& bucket : buckets) {
            bucket.group_values.clear();
            bucket.group_best.clear();
            bucket.candidate = -1;
        }
        if (n == 0) return;

        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            buckets[centroid].group_values.emplace_back();
            path[centroid].push_back({centroid, 0, 0});
            int group = 1;
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                buckets[centroid].group_values.emplace_back();
                add_branch_entries(centroid, first.to, group, first.weight);
                ++group;
            }
            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to);
            }
        };
        decompose(decompose, 0);
    }

    // 頂点vを有効化する 償却O(log^2 n)
    void activate(int v) {
        assert(0 <= v && v < n);
        if (active[v]) return;
        active[v] = true;
        ++active_count;
        update_vertex(v, true);
    }

    // 頂点vを無効化する 償却O(log^2 n)
    void deactivate(int v) {
        assert(0 <= v && v < n);
        if (!active[v]) return;
        active[v] = false;
        --active_count;
        update_vertex(v, false);
    }

    // 頂点vの有効・無効を反転する 償却O(log^2 n)
    void toggle(int v) {
        assert(0 <= v && v < n);
        if (active[v]) deactivate(v);
        else activate(v);
    }

    // 有効頂点集合の直径を返し、空集合なら-1を返す 償却O(log n)
    ll diameter() {
        if (active_count == 0) return -1;
        if (active_count == 1) return 0;
        assert(!global_candidates.empty());
        return global_candidates.top().first;
    }

private:
    vector<Bucket> buckets;
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;
    vector<char> active;
    int active_count = 0;
    DeletablePriorityQueue<pair<ll, int>> global_candidates;

    static ll calculate_candidate(Bucket& bucket) {
        if (bucket.group_best.size() < 2) return -1;
        pair<ll, int> first = bucket.group_best.top();
        bucket.group_best.pop();
        pair<ll, int> second = bucket.group_best.top();
        bucket.group_best.push(first);
        return first.first + second.first;
    }

    void update_vertex(int v, bool add) {
        // 各重心で変更前候補を除き、所属枝の最大距離を更新する
        for (const PathEntry& entry : path[v]) {
            Bucket& bucket = buckets[entry.centroid];
            if (bucket.candidate != -1) global_candidates.erase({bucket.candidate, entry.centroid});
            auto& group_queue = bucket.group_values[entry.group];
            if (!group_queue.empty()) bucket.group_best.erase({group_queue.top(), entry.group});
            if (add) group_queue.push(entry.distance);
            else group_queue.erase(entry.distance);
            if (!group_queue.empty()) bucket.group_best.push({group_queue.top(), entry.group});

            // 異なる二枝の上位値から変更後候補を再計算する
            bucket.candidate = calculate_candidate(bucket);
            if (bucket.candidate != -1) global_candidates.push({bucket.candidate, entry.centroid});
        }
    }

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    void add_branch_entries(int centroid, int start, int group, ll initial_distance) {
        auto dfs = [&](auto&& self, int v, int parent, ll distance) -> void {
            path[v].push_back({centroid, group, distance});
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v, distance + edge.weight);
            }
        };
        dfs(dfs, start, centroid, initial_distance);
    }
};

// 動的な有効頂点集合の異なる二頂点間最短距離を管理するデータ構造
struct CentroidDecompositionActiveClosestPair {
    using ll = long long;
    static constexpr ll INF = numeric_limits<ll>::max() / 4;

    struct Edge {
        int to;
        ll weight;
    };

    struct PathEntry {
        int centroid;
        int group;
        ll distance;
    };

    struct Bucket {
        vector<DeletablePriorityQueue<ll, greater<ll>>> group_values;
        DeletablePriorityQueue<pair<ll, int>, greater<pair<ll, int>>> group_best;
        ll candidate = INF;
    };

    int n;
    int edge_count = 0;
    vector<vector<Edge>> graph;
    vector<vector<PathEntry>> path;

    // n頂点の全頂点無効な重み付き木を構築する O(n)
    explicit CentroidDecompositionActiveClosestPair(int vertex_count)
        : n(vertex_count), graph(vertex_count), path(vertex_count), buckets(vertex_count), removed(vertex_count, false), parent_work(vertex_count, -1),
          subtree_size(vertex_count, 0), active(vertex_count, false) {}

    // 非負重みweightの無向辺u-vを追加する O(1)
    void add_edge(int u, int v, ll weight = 1) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v && weight >= 0);
        graph[u].push_back({v, weight});
        graph[v].push_back({u, weight});
        ++edge_count;
    }

    // 枝別最小距離と重心祖先情報を構築して全頂点を無効化する O(n log n)
    void build() {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        fill(active.begin(), active.end(), false);
        active_count = 0;
        global_candidates.clear();
        for (auto& entries : path) entries.clear();
        for (Bucket& bucket : buckets) {
            bucket.group_values.clear();
            bucket.group_best.clear();
            bucket.candidate = INF;
        }
        if (n == 0) return;

        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            buckets[centroid].group_values.emplace_back();
            path[centroid].push_back({centroid, 0, 0});
            int group = 1;
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                buckets[centroid].group_values.emplace_back();
                add_branch_entries(centroid, first.to, group, first.weight);
                ++group;
            }
            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to);
            }
        };
        decompose(decompose, 0);
    }

    // 頂点vを有効化する 償却O(log^2 n)
    void activate(int v) {
        assert(0 <= v && v < n);
        if (active[v]) return;
        active[v] = true;
        ++active_count;
        update_vertex(v, true);
    }

    // 頂点vを無効化する 償却O(log^2 n)
    void deactivate(int v) {
        assert(0 <= v && v < n);
        if (!active[v]) return;
        active[v] = false;
        --active_count;
        update_vertex(v, false);
    }

    // 頂点vの有効・無効を反転する 償却O(log^2 n)
    void toggle(int v) {
        assert(0 <= v && v < n);
        if (active[v]) deactivate(v);
        else activate(v);
    }

    // 有効な異なる二頂点間の最短距離を返し、2頂点未満なら-1を返す 償却O(log n)
    ll minimum_pair_distance() {
        if (active_count < 2) return -1;
        assert(!global_candidates.empty());
        return global_candidates.top().first;
    }

    // 距離max_distance以下の有効な異なる頂点対が存在するか返す 償却O(log n)
    bool exists_pair_within(ll max_distance) {
        ll distance = minimum_pair_distance();
        return distance != -1 && distance <= max_distance;
    }

private:
    vector<Bucket> buckets;
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;
    vector<char> active;
    int active_count = 0;
    DeletablePriorityQueue<pair<ll, int>, greater<pair<ll, int>>> global_candidates;

    static ll calculate_candidate(Bucket& bucket) {
        if (bucket.group_best.size() < 2) return INF;
        pair<ll, int> first = bucket.group_best.top();
        bucket.group_best.pop();
        pair<ll, int> second = bucket.group_best.top();
        bucket.group_best.push(first);
        return first.first + second.first;
    }

    void update_vertex(int v, bool add) {
        // 各重心で変更前候補を除き、所属枝の最小距離を更新する
        for (const PathEntry& entry : path[v]) {
            Bucket& bucket = buckets[entry.centroid];
            if (bucket.candidate != INF) global_candidates.erase({bucket.candidate, entry.centroid});
            auto& group_queue = bucket.group_values[entry.group];
            if (!group_queue.empty()) bucket.group_best.erase({group_queue.top(), entry.group});
            if (add) group_queue.push(entry.distance);
            else group_queue.erase(entry.distance);
            if (!group_queue.empty()) bucket.group_best.push({group_queue.top(), entry.group});

            // 異なる二枝の下位値から変更後候補を再計算する
            bucket.candidate = calculate_candidate(bucket);
            if (bucket.candidate != INF) global_candidates.push({bucket.candidate, entry.centroid});
        }
    }

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    void add_branch_entries(int centroid, int start, int group, ll initial_distance) {
        auto dfs = [&](auto&& self, int v, int parent, ll distance) -> void {
            path[v].push_back({centroid, group, distance});
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v, distance + edge.weight);
            }
        };
        dfs(dfs, start, centroid, initial_distance);
    }
};

// 固定色が異なる有効二頂点間の最大距離を管理するデータ構造
struct CentroidDecompositionActiveDifferentColorDiameter {
    using ll = long long;
    static constexpr ll NEG_INF = numeric_limits<ll>::lowest() / 4;

    struct Edge {
        int to;
        ll weight;
    };

    struct PathEntry {
        int centroid;
        int group;
        ll distance;
        int cell;
    };

    struct Endpoint {
        ll value = NEG_INF;
        int group = -1;
    };

    struct SegmentNode {
        Endpoint first;
        Endpoint second;
        ll best_pair = NEG_INF;
    };

    static SegmentNode merge_nodes(SegmentNode left, SegmentNode right);
    static SegmentNode empty_segment_node();

    struct Cell {
        int color_index;
        int group;
        DeletablePriorityQueue<ll> distances;
    };

    struct Bucket {
        vector<int> colors;
        vector<Cell> cells;
        vector<DeletablePriorityQueue<pair<ll, int>>> color_best;
        segtree<SegmentNode, merge_nodes, empty_segment_node> segment;
        ll candidate = NEG_INF;
    };

    int n;
    int edge_count = 0;
    vector<int> color;
    vector<vector<Edge>> graph;
    vector<vector<PathEntry>> path;

    // 固定色colorsを持つn頂点の全頂点無効な木を構築する O(n)
    explicit CentroidDecompositionActiveDifferentColorDiameter(const vector<int>& colors)
        : n(static_cast<int>(colors.size())), color(colors), graph(n), path(n), buckets(n),
          removed(n, false), parent_work(n, -1), subtree_size(n, 0), active(n, false) {}

    // 非負重みweightの無向辺u-vを追加する O(1)
    void add_edge(int u, int v, ll weight = 1) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v && weight >= 0);
        graph[u].push_back({v, weight});
        graph[v].push_back({u, weight});
        ++edge_count;
    }

    // 色・枝別バケットと重心祖先情報を構築して全頂点を無効化する O(n log^2 n)
    void build() {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        fill(active.begin(), active.end(), false);
        global_candidates.clear();
        for (auto& entries : path) entries.clear();
        for (Bucket& bucket : buckets) {
            bucket.colors.clear();
            bucket.cells.clear();
            bucket.color_best.clear();
            bucket.segment = segtree<SegmentNode, merge_nodes, empty_segment_node>();
            bucket.candidate = NEG_INF;
        }
        if (n == 0) return;

        // 各頂点について重心・枝・距離を列挙する
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            path[centroid].push_back({centroid, 0, 0, -1});
            int group = 1;
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                add_branch_entries(centroid, first.to, group, first.weight);
                ++group;
            }
            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to);
            }
        };
        decompose(decompose, 0);

        // (重心,色,枝)ごとに削除可能priority queueを表すセルを割り当てる
        struct Ref {
            int centroid;
            int color;
            int group;
            int vertex;
            int position;
        };
        vector<Ref> refs;
        for (int v = 0; v < n; ++v) {
            for (int i = 0; i < static_cast<int>(path[v].size()); ++i) {
                const PathEntry& entry = path[v][i];
                refs.push_back({entry.centroid, color[v], entry.group, v, i});
            }
        }
        sort(refs.begin(), refs.end(), [](const Ref& a, const Ref& b) {
            return tie(a.centroid, a.color, a.group) < tie(b.centroid, b.color, b.group);
        });

        int previous_centroid = -1;
        int previous_color = 0;
        int previous_group = -1;
        int color_index = -1;
        int cell_index = -1;
        for (const Ref& ref : refs) {
            Bucket& bucket = buckets[ref.centroid];
            if (ref.centroid != previous_centroid) {
                previous_centroid = ref.centroid;
                previous_color = ref.color;
                previous_group = -1;
                color_index = 0;
                cell_index = -1;
                bucket.colors.push_back(ref.color);
            } else if (ref.color != previous_color) {
                previous_color = ref.color;
                previous_group = -1;
                ++color_index;
                bucket.colors.push_back(ref.color);
            }
            if (ref.group != previous_group) {
                previous_group = ref.group;
                bucket.cells.push_back({color_index, ref.group, {}});
                ++cell_index;
            }
            path[ref.vertex][ref.position].cell = cell_index;
        }

        // 各重心に色セグメント木を構築する
        for (Bucket& bucket : buckets) {
            if (bucket.colors.empty()) continue;
            bucket.color_best.resize(bucket.colors.size());
            bucket.segment = segtree<SegmentNode, merge_nodes, empty_segment_node>(
                static_cast<int>(bucket.colors.size()));
        }
    }

    // 頂点vを有効化する 償却O(log^3 n)
    void activate(int v) {
        assert(0 <= v && v < n);
        if (active[v]) return;
        active[v] = true;
        update_vertex(v, true);
    }

    // 頂点vを無効化する 償却O(log^3 n)
    void deactivate(int v) {
        assert(0 <= v && v < n);
        if (!active[v]) return;
        active[v] = false;
        update_vertex(v, false);
    }

    // 頂点vの有効・無効を反転する 償却O(log^3 n)
    void toggle(int v) {
        assert(0 <= v && v < n);
        if (active[v]) deactivate(v);
        else activate(v);
    }

    // 色が異なる有効二頂点間の最大距離を返し、存在しなければ-1を返す 償却O(log n)
    ll diameter_between_different_colors() {
        if (global_candidates.empty()) return -1;
        return global_candidates.top().first;
    }

private:
    vector<Bucket> buckets;
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;
    vector<char> active;
    DeletablePriorityQueue<pair<ll, int>> global_candidates;

    static void consider_endpoint(Endpoint candidate, Endpoint& first, Endpoint& second) {
        if (candidate.group == -1) return;
        if (first.group == candidate.group) {
            if (candidate.value > first.value) first = candidate;
            return;
        }
        if (candidate.value > first.value) {
            second = first;
            first = candidate;
        } else if (candidate.group != first.group && candidate.value > second.value) {
            second = candidate;
        }
    }


    static SegmentNode make_leaf(DeletablePriorityQueue<pair<ll, int>>& queue) {
        SegmentNode node;
        if (queue.empty()) return node;
        pair<ll, int> first = queue.top();
        node.first = {first.first, first.second};
        if (queue.size() >= 2) {
            queue.pop();
            pair<ll, int> second = queue.top();
            queue.push(first);
            node.second = {second.first, second.second};
        }
        return node;
    }

    void rebuild_color(Bucket& bucket, int color_index) {
        bucket.segment.set(color_index, make_leaf(bucket.color_best[color_index]));
    }

    void update_vertex(int v, bool add) {
        // 各重心で該当する(色,枝)セルを更新する
        for (const PathEntry& entry : path[v]) {
            Bucket& bucket = buckets[entry.centroid];
            if (bucket.candidate != NEG_INF) global_candidates.erase({bucket.candidate, entry.centroid});
            Cell& cell = bucket.cells[entry.cell];
            auto& color_queue = bucket.color_best[cell.color_index];
            if (!cell.distances.empty()) color_queue.erase({cell.distances.top(), cell.group});
            if (add) cell.distances.push(entry.distance);
            else cell.distances.erase(entry.distance);
            if (!cell.distances.empty()) color_queue.push({cell.distances.top(), cell.group});

            // 変更色から根まで再構築し、重心の最良候補を更新する
            rebuild_color(bucket, cell.color_index);
            bucket.candidate = bucket.segment.all_prod().best_pair;
            if (bucket.candidate != NEG_INF) global_candidates.push({bucket.candidate, entry.centroid});
        }
    }

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    void add_branch_entries(int centroid, int start, int group, ll initial_distance) {
        auto dfs = [&](auto&& self, int v, int parent, ll distance) -> void {
            path[v].push_back({centroid, group, distance, -1});
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v, distance + edge.weight);
            }
        };
        dfs(dfs, start, centroid, initial_distance);
    }
};

inline CentroidDecompositionActiveDifferentColorDiameter::SegmentNode
CentroidDecompositionActiveDifferentColorDiameter::merge_nodes(SegmentNode left, SegmentNode right) {
    SegmentNode result;
    result.best_pair = max(left.best_pair, right.best_pair);

    // 左右は異なる色範囲なので、枝が異なる端点同士だけを比較する
    array<Endpoint, 2> left_endpoints = {left.first, left.second};
    array<Endpoint, 2> right_endpoints = {right.first, right.second};
    for (const Endpoint& a : left_endpoints) {
        for (const Endpoint& b : right_endpoints) {
            if (a.group != -1 && b.group != -1 && a.group != b.group) {
                result.best_pair = max(result.best_pair, a.value + b.value);
            }
        }
    }

    // 外部との結合に必要な、枝が異なる上位二端点を残す
    consider_endpoint(left.first, result.first, result.second);
    consider_endpoint(left.second, result.first, result.second);
    consider_endpoint(right.first, result.first, result.second);
    consider_endpoint(right.second, result.first, result.second);
    return result;
}

inline CentroidDecompositionActiveDifferentColorDiameter::SegmentNode
CentroidDecompositionActiveDifferentColorDiameter::empty_segment_node() {
    return SegmentNode{};
}

// 辺XOR条件を満たす動的な有効頂点数を管理するデータ構造
struct CentroidDecompositionActivePathXorCount {
    struct Edge {
        int to;
        int value;
    };

    struct PathEntry {
        int centroid;
        int branch;
        int path_xor;
    };

    struct Bucket {
        vector<int> all_coordinates;
        vector<vector<int>> branch_coordinates;
        fenwick_tree<int> all_count;
        vector<fenwick_tree<int>> branch_count;
    };

    int n;
    int edge_count = 0;
    vector<vector<Edge>> graph;
    vector<vector<PathEntry>> path;

    // n頂点の全頂点無効な辺XOR木を構築する O(n)
    explicit CentroidDecompositionActivePathXorCount(int vertex_count)
        : n(vertex_count), graph(vertex_count), path(vertex_count), buckets(vertex_count), removed(vertex_count, false), parent_work(vertex_count, -1),
          subtree_size(vertex_count, 0), active(vertex_count, false) {}

    // 値valueを持つ無向辺u-vを追加する O(1)
    void add_edge(int u, int v, int value) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        graph[u].push_back({v, value});
        graph[v].push_back({u, value});
        ++edge_count;
    }

    // XOR座標と重心祖先情報を構築して全頂点を無効化する O(n log^2 n)
    void build() {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        fill(active.begin(), active.end(), false);
        for (auto& entries : path) entries.clear();
        for (Bucket& bucket : buckets) {
            bucket.all_coordinates.clear();
            bucket.branch_coordinates.clear();
            bucket.all_count = fenwick_tree<int>(0);
            bucket.branch_count.clear();
        }
        if (n == 0) return;

        // 各重心からのXORを全体・枝別に収集する
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            path[centroid].push_back({centroid, -1, 0});
            buckets[centroid].all_coordinates.push_back(0);
            int branch = 0;
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                buckets[centroid].branch_coordinates.emplace_back();
                add_branch_entries(centroid, first.to, branch, first.value);
                ++branch;
            }
            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to);
            }
        };
        decompose(decompose, 0);

        // 各座標列を圧縮してFenwick Treeを構築する
        for (Bucket& bucket : buckets) {
            sort_unique(bucket.all_coordinates);
            bucket.all_count = fenwick_tree<int>(static_cast<int>(bucket.all_coordinates.size()));
            for (auto& coordinates : bucket.branch_coordinates) {
                sort_unique(coordinates);
                bucket.branch_count.emplace_back(static_cast<int>(coordinates.size()));
            }
        }
    }

    // 頂点vを有効化する O(log^2 n)
    void activate(int v) {
        assert(0 <= v && v < n);
        if (active[v]) return;
        active[v] = true;
        update_vertex(v, 1);
    }

    // 頂点vを無効化する O(log^2 n)
    void deactivate(int v) {
        assert(0 <= v && v < n);
        if (!active[v]) return;
        active[v] = false;
        update_vertex(v, -1);
    }

    // 頂点vの有効・無効を反転する O(log^2 n)
    void toggle(int v) {
        assert(0 <= v && v < n);
        if (active[v]) deactivate(v);
        else activate(v);
    }

    // vとのパス辺XORがtarget_xorとなる有効頂点数を返す O(log^2 n)
    long long count_path_xor(int v, int target_xor) {
        assert(0 <= v && v < n);
        long long answer = 0;
        for (const PathEntry& entry : path[v]) {
            int required = target_xor ^ entry.path_xor;
            Bucket& bucket = buckets[entry.centroid];
            answer += exact_count(bucket.all_coordinates, bucket.all_count, required);
            if (entry.branch != -1) {
                answer -= exact_count(bucket.branch_coordinates[entry.branch],
                                      bucket.branch_count[entry.branch], required);
            }
        }
        return answer;
    }

private:
    vector<Bucket> buckets;
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;
    vector<char> active;

    static void sort_unique(vector<int>& values) {
        sort(values.begin(), values.end());
        values.erase(unique(values.begin(), values.end()), values.end());
    }

    static int exact_count(const vector<int>& coordinates, fenwick_tree<int>& bit, int value) {
        auto it = lower_bound(coordinates.begin(), coordinates.end(), value);
        if (it == coordinates.end() || *it != value) return 0;
        int index = static_cast<int>(it - coordinates.begin());
        return bit.sum(index, index + 1);
    }

    void update_vertex(int v, int delta) {
        for (const PathEntry& entry : path[v]) {
            Bucket& bucket = buckets[entry.centroid];
            int all_index = static_cast<int>(
                lower_bound(bucket.all_coordinates.begin(), bucket.all_coordinates.end(), entry.path_xor)
                - bucket.all_coordinates.begin());
            bucket.all_count.add(all_index, delta);
            if (entry.branch != -1) {
                auto& coordinates = bucket.branch_coordinates[entry.branch];
                int branch_index = static_cast<int>(
                    lower_bound(coordinates.begin(), coordinates.end(), entry.path_xor) - coordinates.begin());
                bucket.branch_count[entry.branch].add(branch_index, delta);
            }
        }
    }

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    void add_branch_entries(int centroid, int start, int branch, int initial_xor) {
        auto dfs = [&](auto&& self, int v, int parent, int path_xor) -> void {
            path[v].push_back({centroid, branch, path_xor});
            buckets[centroid].all_coordinates.push_back(path_xor);
            buckets[centroid].branch_coordinates[branch].push_back(path_xor);
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v, path_xor ^ edge.value);
            }
        };
        dfs(dfs, start, centroid, initial_xor);
    }
};

// 有効頂点へのパス上最小辺を最大化する動的データ構造
struct CentroidDecompositionMaximumBottleneckToActive {
    using ll = long long;
    static constexpr ll INF = numeric_limits<ll>::max() / 4;

    struct Edge {
        int to;
        ll weight;
    };

    struct PathEntry {
        int centroid;
        ll bottleneck;
    };

    int n;
    int edge_count = 0;
    vector<vector<Edge>> graph;
    vector<vector<PathEntry>> path;

    // n頂点の全頂点無効な容量付き木を構築する O(n)
    explicit CentroidDecompositionMaximumBottleneckToActive(int vertex_count)
        : n(vertex_count), graph(vertex_count), path(vertex_count), removed(vertex_count, false), parent_work(vertex_count, -1),
          subtree_size(vertex_count, 0), active(vertex_count, false), best(n) {}

    // 0以上INF未満の重みweightを持つ無向辺u-vを追加する O(1)
    void add_edge(int u, int v, ll weight) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v && 0 <= weight && weight < INF);
        graph[u].push_back({v, weight});
        graph[v].push_back({u, weight});
        ++edge_count;
    }

    // 半パスボトルネックと重心祖先情報を構築して全頂点を無効化する O(n log n)
    void build() {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        fill(active.begin(), active.end(), false);
        for (auto& entries : path) entries.clear();
        for (auto& queue : best) queue.clear();
        if (n == 0) return;

        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            path[centroid].push_back({centroid, INF});
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                add_branch_entries(centroid, first.to, first.weight);
            }
            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to);
            }
        };
        decompose(decompose, 0);
    }

    // 頂点vを有効化する O(log^2 n)
    void activate(int v) {
        assert(0 <= v && v < n);
        if (active[v]) return;
        active[v] = true;
        for (const PathEntry& entry : path[v]) best[entry.centroid].push(entry.bottleneck);
    }

    // 頂点vを無効化する O(log^2 n)
    void deactivate(int v) {
        assert(0 <= v && v < n);
        if (!active[v]) return;
        active[v] = false;
        for (const PathEntry& entry : path[v]) best[entry.centroid].erase(entry.bottleneck);
    }

    // 頂点vの有効・無効を反転する O(log^2 n)
    void toggle(int v) {
        assert(0 <= v && v < n);
        if (active[v]) deactivate(v);
        else activate(v);
    }

    // vから有効頂点へのパス上最小辺の最大値を返し、存在しなければ-1を返す 償却O(log^2 n)
    ll maximum_bottleneck(int v) {
        assert(0 <= v && v < n);
        ll answer = -1;
        for (const PathEntry& entry : path[v]) {
            auto& queue = best[entry.centroid];
            if (!queue.empty()) answer = max(answer, min(entry.bottleneck, queue.top()));
        }
        return answer;
    }

    // v自身が有効なら真となるINF規約で、ボトルネックthreshold以上の有効頂点が存在するか返す 償却O(log^2 n)
    bool exists_with_bottleneck_at_least(int v, ll threshold) {
        return maximum_bottleneck(v) >= threshold;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;
    vector<char> active;
    vector<DeletablePriorityQueue<ll>> best;

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    void add_branch_entries(int centroid, int start, ll initial_bottleneck) {
        auto dfs = [&](auto&& self, int v, int parent, ll bottleneck) -> void {
            path[v].push_back({centroid, bottleneck});
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v, min(bottleneck, edge.weight));
            }
        };
        dfs(dfs, start, centroid, initial_bottleneck);
    }
};

// 有効頂点へのパス上最大辺を最小化する動的データ構造
struct CentroidDecompositionMinimumMaximumEdgeToActive {
    using ll = long long;
    static constexpr ll INF = numeric_limits<ll>::max() / 4;

    struct Edge {
        int to;
        ll weight;
    };

    struct PathEntry {
        int centroid;
        ll maximum_edge;
    };

    int n;
    int edge_count = 0;
    vector<vector<Edge>> graph;
    vector<vector<PathEntry>> path;

    // n頂点の全頂点無効な重み付き木を構築する O(n)
    explicit CentroidDecompositionMinimumMaximumEdgeToActive(int vertex_count)
        : n(vertex_count), graph(vertex_count), path(vertex_count), removed(vertex_count, false), parent_work(vertex_count, -1),
          subtree_size(vertex_count, 0), active(vertex_count, false), best(n) {}

    // 非負重みweightの無向辺u-vを追加する O(1)
    void add_edge(int u, int v, ll weight) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v && 0 <= weight && weight < INF);
        graph[u].push_back({v, weight});
        graph[v].push_back({u, weight});
        ++edge_count;
    }

    // 半パス最大辺と重心祖先情報を構築して全頂点を無効化する O(n log n)
    void build() {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        fill(active.begin(), active.end(), false);
        for (auto& entries : path) entries.clear();
        for (auto& queue : best) queue.clear();
        if (n == 0) return;

        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            path[centroid].push_back({centroid, 0});
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                add_branch_entries(centroid, first.to, first.weight);
            }
            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to);
            }
        };
        decompose(decompose, 0);
    }

    // 頂点vを有効化する O(log^2 n)
    void activate(int v) {
        assert(0 <= v && v < n);
        if (active[v]) return;
        active[v] = true;
        for (const PathEntry& entry : path[v]) best[entry.centroid].push(entry.maximum_edge);
    }

    // 頂点vを無効化する O(log^2 n)
    void deactivate(int v) {
        assert(0 <= v && v < n);
        if (!active[v]) return;
        active[v] = false;
        for (const PathEntry& entry : path[v]) best[entry.centroid].erase(entry.maximum_edge);
    }

    // 頂点vの有効・無効を反転する O(log^2 n)
    void toggle(int v) {
        assert(0 <= v && v < n);
        if (active[v]) deactivate(v);
        else activate(v);
    }

    // vから有効頂点へのパス上最大辺の最小値を返し、存在しなければ-1を返す 償却O(log^2 n)
    ll minimum_maximum_edge(int v) {
        assert(0 <= v && v < n);
        ll answer = INF;
        for (const PathEntry& entry : path[v]) {
            auto& queue = best[entry.centroid];
            if (!queue.empty()) answer = min(answer, max(entry.maximum_edge, queue.top()));
        }
        return answer == INF ? -1 : answer;
    }

    // 最大辺threshold以下で到達できる有効頂点が存在するか返す 償却O(log^2 n)
    bool exists_with_maximum_edge_at_most(int v, ll threshold) {
        ll value = minimum_maximum_edge(v);
        return value != -1 && value <= threshold;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;
    vector<char> active;
    vector<DeletablePriorityQueue<ll, greater<ll>>> best;

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    void add_branch_entries(int centroid, int start, ll initial_maximum) {
        auto dfs = [&](auto&& self, int v, int parent, ll maximum_edge) -> void {
            path[v].push_back({centroid, maximum_edge});
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v, max(maximum_edge, edge.weight));
            }
        };
        dfs(dfs, start, centroid, initial_maximum);
    }
};

// 辺重みパス和が指定条件を満たす異なる順序なし頂点対を数えるデータ構造
struct CentroidDecompositionPathSumCount {
    using ll = long long;

    struct Edge {
        int to;
        ll weight;
    };

    int n;
    int edge_count = 0;
    vector<vector<Edge>> graph;

    // n頂点の空の重み付き木を構築する O(n)
    explicit CentroidDecompositionPathSumCount(int vertex_count)
        : n(vertex_count), graph(vertex_count), removed(vertex_count, false), parent_work(vertex_count, -1), subtree_size(vertex_count, 0) {}

    // 重みweightの無向辺u-vを追加する O(1)
    void add_edge(int u, int v, ll weight = 1) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        graph[u].push_back({v, weight});
        graph[v].push_back({u, weight});
        ++edge_count;
    }

    // パス辺重み和がtargetと等しい異なる順序なし頂点対数を返す O(n log^2 n)
    long long count_exact(ll target) {
        return decompose_and_count(target, true);
    }

    // パス辺重み和がlimit以下の異なる順序なし頂点対数を返す O(n log^2 n)
    long long count_at_most(ll limit) {
        return decompose_and_count(limit, false);
    }

    // パス辺重み和が[lower, upper]に入る異なる順序なし頂点対数を返す O(n log^2 n)
    long long count_in_range(ll lower, ll upper) {
        if (lower > upper) return 0;
        long long below = lower == numeric_limits<ll>::min() ? 0 : count_at_most(lower - 1);
        return count_at_most(upper) - below;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;

    static long long count_exact_pairs(vector<ll> values, ll target) {
        sort(values.begin(), values.end());
        long long answer = 0;
        int left = 0;
        int right = static_cast<int>(values.size()) - 1;
        while (left < right) {
            __int128 sum = static_cast<__int128>(values[left]) + values[right];
            if (sum < target) {
                ++left;
            } else if (sum > target) {
                --right;
            } else if (values[left] == values[right]) {
                long long count = right - left + 1;
                answer += count * (count - 1) / 2;
                break;
            } else {
                int next_left = left + 1;
                int next_right = right - 1;
                while (next_left <= right && values[next_left] == values[left]) ++next_left;
                while (next_right >= left && values[next_right] == values[right]) --next_right;
                answer += 1LL * (next_left - left) * (right - next_right);
                left = next_left;
                right = next_right;
            }
        }
        return answer;
    }

    static long long count_at_most_pairs(vector<ll> values, ll limit) {
        sort(values.begin(), values.end());
        long long answer = 0;
        int left = 0;
        int right = static_cast<int>(values.size()) - 1;
        while (left < right) {
            __int128 sum = static_cast<__int128>(values[left]) + values[right];
            if (sum <= limit) {
                answer += right - left;
                ++left;
            } else {
                --right;
            }
        }
        return answer;
    }

    long long decompose_and_count(ll value, bool exact) {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        if (n == 0) return 0;
        long long answer = 0;

        // 各重心を通るパスを全体から同一枝内の組を引いて数える
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            vector<ll> all_distances = {0};
            vector<vector<ll>> branch_distances;
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                branch_distances.push_back(collect_distances(centroid, first.to, first.weight));
                all_distances.insert(all_distances.end(), branch_distances.back().begin(), branch_distances.back().end());
            }

            // 全組の数からパスが重心を通らない各枝内の組を除く
            auto count_function = [&](vector<ll> distances) {
                return exact ? count_exact_pairs(move(distances), value)
                             : count_at_most_pairs(move(distances), value);
            };
            answer += count_function(move(all_distances));
            for (auto& distances : branch_distances) answer -= count_function(move(distances));


            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to);
            }
        };
        decompose(decompose, 0);
        return answer;
    }

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    vector<ll> collect_distances(int centroid, int start, ll initial_distance) const {
        vector<ll> distances;
        auto dfs = [&](auto&& self, int v, int parent, ll distance) -> void {
            distances.push_back(distance);
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v, distance + edge.weight);
            }
        };
        dfs(dfs, start, centroid, initial_distance);
        return distances;
    }
};

// 辺重みパス和が指定値となる異なる頂点対の最小辺数を求めるデータ構造
struct CentroidDecompositionExactPathSumMinEdges {
    using ll = long long;

    struct Edge {
        int to;
        ll weight;
    };

    struct State {
        ll sum;
        int depth;
    };

    int n;
    int edge_count = 0;
    vector<vector<Edge>> graph;

    // n頂点の空の重み付き木を構築する O(n)
    explicit CentroidDecompositionExactPathSumMinEdges(int vertex_count)
        : n(vertex_count), graph(vertex_count), removed(vertex_count, false), parent_work(vertex_count, -1), subtree_size(vertex_count, 0) {}

    // 重みweightの無向辺u-vを追加する O(1)
    void add_edge(int u, int v, ll weight) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        graph[u].push_back({v, weight});
        graph[v].push_back({u, weight});
        ++edge_count;
    }

    // パス辺重み和がtargetとなる異なる頂点対の最小辺数を返し、存在しなければ-1を返す O(n log^2 n)
    int minimum_edges(ll target) {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        if (n == 0) return -1;
        int answer = numeric_limits<int>::max();

        // 各重心で枝を問い合わせ後にマージし、同一枝内の組を除外する
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            vector<vector<State>> branches;
            vector<ll> coordinates = {0};
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                branches.push_back(collect_states(centroid, first.to, first.weight));
                for (const State& state : branches.back()) coordinates.push_back(state.sum);
            }
            sort(coordinates.begin(), coordinates.end());
            coordinates.erase(unique(coordinates.begin(), coordinates.end()), coordinates.end());
            vector<int> best(coordinates.size(), numeric_limits<int>::max());
            best[static_cast<int>(lower_bound(coordinates.begin(), coordinates.end(), 0) - coordinates.begin())] = 0;

            // 現在枝と過去枝または重心自身を結ぶ候補を評価する
            for (const auto& branch : branches) {
                for (const State& state : branch) {
                    __int128 required128 = static_cast<__int128>(target) - state.sum;
                    if (required128 < numeric_limits<ll>::min() || required128 > numeric_limits<ll>::max()) continue;
                    ll required = static_cast<ll>(required128);
                    auto it = lower_bound(coordinates.begin(), coordinates.end(), required);
                    if (it != coordinates.end() && *it == required) {
                        int previous_depth = best[static_cast<int>(it - coordinates.begin())];
                        if (previous_depth != numeric_limits<int>::max()) {
                            answer = min(answer, state.depth + previous_depth);
                        }
                    }
                }
                for (const State& state : branch) {
                    int index = static_cast<int>(lower_bound(coordinates.begin(), coordinates.end(), state.sum) - coordinates.begin());
                    best[index] = min(best[index], state.depth);
                }
            }
            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to);
            }
        };
        decompose(decompose, 0);
        return answer == numeric_limits<int>::max() ? -1 : answer;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    vector<State> collect_states(int centroid, int start, ll initial_sum) const {
        vector<State> states;
        auto dfs = [&](auto&& self, int v, int parent, ll sum, int depth) -> void {
            states.push_back({sum, depth});
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v, sum + edge.weight, depth + 1);
            }
        };
        dfs(dfs, start, centroid, initial_sum, 1);
        return states;
    }
};

// 辺重みパス和が指定値となる異なる頂点対の頂点スコア和最大値を求めるデータ構造
struct CentroidDecompositionExactPathSumMaxVertexScore {
    using ll = long long;
    static constexpr ll NO_SOLUTION = numeric_limits<ll>::lowest() / 4;

    struct Edge {
        int to;
        ll weight;
    };

    struct State {
        ll sum;
        ll score;
    };

    int n;
    int edge_count = 0;
    vector<ll> vertex_score;
    vector<vector<Edge>> graph;

    // 頂点スコアscoresを持つn頂点の空の重み付き木を構築する O(n)
    explicit CentroidDecompositionExactPathSumMaxVertexScore(const vector<ll>& scores)
        : n(static_cast<int>(scores.size())), vertex_score(scores), graph(n), removed(n, false),
          parent_work(n, -1), subtree_size(n, 0) {}

    // 重みweightの無向辺u-vを追加する O(1)
    void add_edge(int u, int v, ll weight) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        graph[u].push_back({v, weight});
        graph[v].push_back({u, weight});
        ++edge_count;
    }

    // パス辺重み和がtargetとなる異なる頂点対の最大頂点スコア和を返す O(n log^2 n)
    ll maximum_vertex_score(ll target) {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        if (n == 0) return NO_SOLUTION;
        ll answer = NO_SOLUTION;

        // 各重心で同じ和の半パスについて最大スコアだけを保持する
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            vector<vector<State>> branches;
            vector<ll> coordinates = {0};
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                branches.push_back(collect_states(centroid, first.to, first.weight,
                                                  vertex_score[centroid] + vertex_score[first.to]));
                for (const State& state : branches.back()) coordinates.push_back(state.sum);
            }
            sort(coordinates.begin(), coordinates.end());
            coordinates.erase(unique(coordinates.begin(), coordinates.end()), coordinates.end());
            vector<ll> best(coordinates.size(), NO_SOLUTION);
            best[static_cast<int>(lower_bound(coordinates.begin(), coordinates.end(), 0) - coordinates.begin())] = vertex_score[centroid];

            // 問い合わせ後に枝をマージして異なる枝または重心との組だけを作る
            for (const auto& branch : branches) {
                for (const State& state : branch) {
                    __int128 required128 = static_cast<__int128>(target) - state.sum;
                    if (required128 < numeric_limits<ll>::min() || required128 > numeric_limits<ll>::max()) continue;
                    ll required = static_cast<ll>(required128);
                    auto it = lower_bound(coordinates.begin(), coordinates.end(), required);
                    if (it != coordinates.end() && *it == required && best[static_cast<int>(it - coordinates.begin())] != NO_SOLUTION) {
                        answer = max(answer, state.score + best[static_cast<int>(it - coordinates.begin())] - vertex_score[centroid]);
                    }
                }
                for (const State& state : branch) {
                    int index = static_cast<int>(lower_bound(coordinates.begin(), coordinates.end(), state.sum) - coordinates.begin());
                    best[index] = max(best[index], state.score);
                }
            }
            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to);
            }
        };
        decompose(decompose, 0);
        return answer;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    vector<State> collect_states(int centroid, int start, ll initial_sum, ll initial_score) const {
        vector<State> states;
        auto dfs = [&](auto&& self, int v, int parent, ll sum, ll score) -> void {
            states.push_back({sum, score});
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v, sum + edge.weight, score + vertex_score[edge.to]);
            }
        };
        dfs(dfs, start, centroid, initial_sum, initial_score);
        return states;
    }
};

// 正の頂点値の積が上限以下となる最長パスを求めるデータ構造
struct CentroidDecompositionPathProductMaxEdges {
    using ll = long long;

    struct State {
        ll product;
        int depth;
    };

    int n;
    int edge_count = 0;
    vector<ll> value;
    vector<vector<int>> graph;

    static int max_depth_op(int left, int right) {
        return max(left, right);
    }

    static int max_depth_e() {
        return -1;
    }

    // 正の頂点値valuesを持つn頂点の空の木を構築する O(n)
    explicit CentroidDecompositionPathProductMaxEdges(const vector<ll>& values)
        : n(static_cast<int>(values.size())), value(values), graph(n), removed(n, false),
          parent_work(n, -1), subtree_size(n, 0) {
        for (ll x : value) { (void)x; assert(x > 0); }
    }

    // 無向辺u-vを追加する O(1)
    void add_edge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        graph[u].push_back(v);
        graph[v].push_back(u);
        ++edge_count;
    }

    // 頂点値積がproduct_limit以下となるパスの最大辺数を返し、存在しなければ-1を返す O(n log^2 n)
    int maximum_edges(ll product_limit) {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        if (n == 0 || product_limit < 1) return -1;
        int answer = -1;

        // 積が単調非減少であることを利用し、上限超過した半パスを打ち切る
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            if (value[centroid] > product_limit) {
                for (int next_start : graph[centroid]) {
                    if (!removed[next_start]) self(self, next_start);
                }
                return;
            }
            answer = max(answer, 0);
            ll side_limit = product_limit / value[centroid];

            // 各枝の半パス積を列挙し、積座標上の最大深さをセグメント木で管理する
            vector<vector<State>> branches;
            vector<ll> coordinates = {1};
            for (int first : graph[centroid]) {
                if (removed[first]) continue;
                branches.push_back(collect_states(centroid, first, side_limit));
                for (const State& state : branches.back()) coordinates.push_back(state.product);
            }
            sort(coordinates.begin(), coordinates.end());
            coordinates.erase(unique(coordinates.begin(), coordinates.end()), coordinates.end());
            segtree<int, max_depth_op, max_depth_e> segment(static_cast<int>(coordinates.size()));
            int zero_index = static_cast<int>(lower_bound(coordinates.begin(), coordinates.end(), 1) - coordinates.begin());
            segment.set(zero_index, max(segment.get(zero_index), 0));

            // 現在枝を問い合わせてから追加し、重心を通るパスだけを評価する
            for (const auto& branch : branches) {
                for (const State& state : branch) {
                    ll maximum_other = side_limit / state.product;
                    int right = static_cast<int>(upper_bound(coordinates.begin(), coordinates.end(), maximum_other) - coordinates.begin());
                    int other_depth = segment.prod(0, right);
                    if (other_depth != -1) answer = max(answer, state.depth + other_depth);
                }
                for (const State& state : branch) {
                    int index = static_cast<int>(lower_bound(coordinates.begin(), coordinates.end(), state.product) - coordinates.begin());
                    segment.set(index, max(segment.get(index), state.depth));
                }
            }
            for (int next_start : graph[centroid]) {
                if (!removed[next_start]) self(self, next_start);
            }
        };
        decompose(decompose, 0);
        return answer;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;


    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (int to : graph[v]) {
                if (removed[to] || to == parent) continue;
                self(self, to, v);
                subtree_size[v] += subtree_size[to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (int to : graph[v]) {
                if (!removed[to] && parent_work[to] == v) max_part = max(max_part, subtree_size[to]);
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    vector<State> collect_states(int centroid, int start, ll side_limit) const {
        vector<State> states;
        if (value[start] > side_limit) return states;
        auto dfs = [&](auto&& self, int v, int parent, ll product, int depth) -> void {
            states.push_back({product, depth});
            for (int to : graph[v]) {
                if (removed[to] || to == parent) continue;
                if (product <= side_limit / value[to]) self(self, to, v, product * value[to], depth + 1);
            }
        };
        dfs(dfs, start, centroid, value[start], 1);
        return states;
    }
};

// 頂点マスクのパスXORが許可集合に属するパスを数えるデータ構造
struct CentroidDecompositionVertexMaskPathCount {
    struct State {
        int path_mask;
    };

    int n;
    int edge_count = 0;
    int bit_count;
    vector<int> vertex_mask;
    vector<vector<int>> graph;

    // bit_countビットの頂点マスクmasksを持つ空の木を構築する O(n)
    CentroidDecompositionVertexMaskPathCount(const vector<int>& masks, int bit_count_)
        : n(static_cast<int>(masks.size())), bit_count(bit_count_), vertex_mask(masks), graph(n),
          removed(n, false), parent_work(n, -1), subtree_size(n, 0) {
        assert(0 <= bit_count && bit_count <= 20);
        int mask_limit = 1 << bit_count;
        (void)mask_limit;
        for (int mask : vertex_mask) { (void)mask; assert(0 <= mask && mask < mask_limit); }
    }

    // 無向辺u-vを追加する O(1)
    void add_edge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        graph[u].push_back(v);
        graph[v].push_back(u);
        ++edge_count;
    }

    // パス頂点マスクXORがallowed_xorsのいずれかとなる単一頂点を含む順序なしパス数を返す O(A n log n)
    long long count_allowed_xors(vector<int> allowed_xors) {
        assert(n == 0 || edge_count == n - 1);
        int mask_limit = 1 << bit_count;
        for (int mask : allowed_xors) { (void)mask; assert(0 <= mask && mask < mask_limit); }
        sort(allowed_xors.begin(), allowed_xors.end());
        allowed_xors.erase(unique(allowed_xors.begin(), allowed_xors.end()), allowed_xors.end());
        fill(removed.begin(), removed.end(), false);
        if (n == 0) return 0;
        long long answer = 0;
        vector<int> frequency(mask_limit, 0);
        vector<int> touched;

        // 各重心で問い合わせ後に枝をマージし、パスを一度だけ数える
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            vector<vector<State>> branches;
            for (int first : graph[centroid]) {
                if (removed[first]) continue;
                branches.push_back(collect_states(centroid, first,
                                                  vertex_mask[centroid] ^ vertex_mask[first]));
            }

            // 重心単独パスを加え、重心を片端とする半パスを初期状態にする
            if (binary_search(allowed_xors.begin(), allowed_xors.end(), vertex_mask[centroid])) ++answer;
            add_frequency(frequency, touched, vertex_mask[centroid]);
            for (const auto& branch : branches) {
                for (const State& state : branch) {
                    for (int target : allowed_xors) {
                        int required = state.path_mask ^ vertex_mask[centroid] ^ target;
                        answer += frequency[required];
                    }
                }
                for (const State& state : branch) add_frequency(frequency, touched, state.path_mask);
            }
            for (int mask : touched) frequency[mask] = 0;
            touched.clear();
            for (int next_start : graph[centroid]) {
                if (!removed[next_start]) self(self, next_start);
            }
        };
        decompose(decompose, 0);
        return answer;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;

    static void add_frequency(vector<int>& frequency, vector<int>& touched, int mask) {
        if (frequency[mask] == 0) touched.push_back(mask);
        ++frequency[mask];
    }

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (int to : graph[v]) {
                if (removed[to] || to == parent) continue;
                self(self, to, v);
                subtree_size[v] += subtree_size[to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (int to : graph[v]) {
                if (!removed[to] && parent_work[to] == v) max_part = max(max_part, subtree_size[to]);
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    vector<State> collect_states(int centroid, int start, int initial_mask) const {
        vector<State> states;
        auto dfs = [&](auto&& self, int v, int parent, int path_mask) -> void {
            states.push_back({path_mask});
            for (int to : graph[v]) {
                if (removed[to] || to == parent) continue;
                self(self, to, v, path_mask ^ vertex_mask[to]);
            }
        };
        dfs(dfs, start, centroid, initial_mask);
        return states;
    }
};

// 各頂点を通る回文並べ替え可能パス数を求めるデータ構造
struct CentroidDecompositionPalindromePathThroughVertexCount {
    struct State {
        int vertex;
        int parent;
        int path_mask;
        long long outside_count = 0;
    };

    int n;
    int edge_count = 0;
    int alphabet_size;
    vector<int> label;
    vector<int> vertex_mask;
    vector<vector<int>> graph;

    // 0以上alphabet_size未満の頂点ラベルlabelsを持つ空の木を構築する O(n)
    CentroidDecompositionPalindromePathThroughVertexCount(const vector<int>& labels, int alphabet_size_)
        : n(static_cast<int>(labels.size())), alphabet_size(alphabet_size_), label(labels),
          vertex_mask(n), graph(n), removed(n, false), parent_work(n, -1),
          subtree_size(n, 0), subtree_contribution(n, 0) {
        assert(1 <= alphabet_size && alphabet_size <= 20);
        for (int v = 0; v < n; ++v) {
            assert(0 <= label[v] && label[v] < alphabet_size);
            vertex_mask[v] = 1 << label[v];
        }
    }

    // 無向辺u-vを追加する O(1)
    void add_edge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        graph[u].push_back(v);
        graph[v].push_back(u);
        ++edge_count;
    }

    // 各頂点を通る回文並べ替え可能パス数を単一頂点パス込みで返す O(B n log n)
    vector<long long> count_through_each_vertex() {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        vector<long long> answer(n, 0);
        if (n == 0) return answer;
        int mask_limit = 1 << alphabet_size;
        vector<int> frequency(mask_limit, 0);
        vector<int> touched;

        // 各重心を最高位の分離点とする有効パスの頂点別寄与を加算する
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            vector<vector<State>> branches;
            for (int first : graph[centroid]) {
                if (removed[first]) continue;
                branches.push_back(collect_states(centroid, first,
                                                  vertex_mask[centroid] ^ vertex_mask[first]));
            }

            // 問い合わせ後マージで重心を通る有効パス総数を数える
            long long centroid_paths = 1;
            add_frequency(frequency, touched, vertex_mask[centroid], 1);
            for (const auto& branch : branches) {
                for (const State& state : branch) {
                    centroid_paths += compatible_count(frequency, state.path_mask,
                                                       vertex_mask[centroid], alphabet_size);
                }
                for (const State& state : branch) add_frequency(frequency, touched, state.path_mask, 1);
            }
            answer[centroid] += centroid_paths;
            for (int mask : touched) frequency[mask] = 0;
            touched.clear();

            // 全端点頻度から同じ枝を除き、各端点と外側端点の有効組数を求める
            add_frequency(frequency, touched, vertex_mask[centroid], 1);
            for (const auto& branch : branches) {
                for (const State& state : branch) add_frequency(frequency, touched, state.path_mask, 1);
            }
            for (auto& branch : branches) {
                for (const State& state : branch) add_frequency(frequency, touched, state.path_mask, -1);
                for (State& state : branch) {
                    state.outside_count = compatible_count(frequency, state.path_mask,
                                                           vertex_mask[centroid], alphabet_size);
                    subtree_contribution[state.vertex] = state.outside_count;
                }
                for (const State& state : branch) add_frequency(frequency, touched, state.path_mask, 1);

                // 端点寄与を枝内で下から上へ累積し、パス上の全頂点へ加える
                for (int i = static_cast<int>(branch.size()) - 1; i >= 0; --i) {
                    const State& state = branch[i];
                    answer[state.vertex] += subtree_contribution[state.vertex];
                    if (state.parent != centroid) {
                        subtree_contribution[state.parent] += subtree_contribution[state.vertex];
                    }
                }
            }
            for (int mask : touched) frequency[mask] = 0;
            touched.clear();
            for (int next_start : graph[centroid]) {
                if (!removed[next_start]) self(self, next_start);
            }
        };
        decompose(decompose, 0);
        return answer;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;
    vector<long long> subtree_contribution;

    static void add_frequency(vector<int>& frequency, vector<int>& touched, int mask, int delta) {
        if (delta > 0 && frequency[mask] == 0) touched.push_back(mask);
        frequency[mask] += delta;
        assert(frequency[mask] >= 0);
    }

    static long long compatible_count(const vector<int>& frequency, int path_mask,
                                      int centroid_mask, int alphabet_size) {
        int base = path_mask ^ centroid_mask;
        long long result = frequency[base];
        for (int bit = 0; bit < alphabet_size; ++bit) result += frequency[base ^ (1 << bit)];
        return result;
    }

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (int to : graph[v]) {
                if (removed[to] || to == parent) continue;
                self(self, to, v);
                subtree_size[v] += subtree_size[to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (int to : graph[v]) {
                if (!removed[to] && parent_work[to] == v) max_part = max(max_part, subtree_size[to]);
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    vector<State> collect_states(int centroid, int start, int initial_mask) const {
        vector<State> states;
        auto dfs = [&](auto&& self, int v, int parent, int path_mask) -> void {
            states.push_back({v, parent, path_mask, 0});
            for (int to : graph[v]) {
                if (removed[to] || to == parent) continue;
                self(self, to, v, path_mask ^ vertex_mask[to]);
            }
        };
        dfs(dfs, start, centroid, initial_mask);
        return states;
    }
};

// 辺数字を連結した有向パス整数が法modで0となる順序付き頂点対を数えるデータ構造
struct CentroidDecompositionDigitModuloCount {
    using ll = long long;

    struct Edge {
        int to;
        int digit;
    };

    struct State {
        int up;
        int down;
        int depth;
    };

    int n;
    int edge_count = 0;
    int mod;
    vector<vector<Edge>> graph;

    // 法modで評価するn頂点の空の数字辺付き木を構築する O(n)
    CentroidDecompositionDigitModuloCount(int vertex_count, int modulus)
        : n(vertex_count), mod(modulus), graph(vertex_count), removed(vertex_count, false), parent_work(vertex_count, -1),
          subtree_size(vertex_count, 0) {
        assert(mod >= 1);
        assert(mod == 1 || std::gcd(mod, 10) == 1);
    }

    // 数字digitを持つ無向辺u-vを追加する O(1)
    void add_edge(int u, int v, int digit) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        assert(0 <= digit && digit <= 9);
        graph[u].push_back({v, digit});
        graph[v].push_back({u, digit});
        ++edge_count;
    }

    // 連結整数がmodで割り切れる異なる頂点の順序付き対数を返す O(n log^2 n)
    long long count_divisible_ordered_pairs() {
        assert(n == 0 || edge_count == n - 1);
        if (mod == 1) return 1LL * n * (n - 1);
        fill(removed.begin(), removed.end(), false);
        if (n == 0) return 0;

        // 10の冪と逆冪を全深さ分前計算する
        int inverse_ten = modular_inverse(10, mod);
        power_ten.assign(n + 1, 1 % mod);
        inverse_power_ten.assign(n + 1, 1 % mod);
        for (int i = 1; i <= n; ++i) {
            power_ten[i] = multiply_mod(power_ten[i - 1], 10);
            inverse_power_ten[i] = multiply_mod(inverse_power_ten[i - 1], inverse_ten);
        }
        long long answer = 0;

        // 全状態の順序付き組から同じ枝内の組を引く
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            vector<State> all_states = {{0, 0, 0}};
            vector<vector<State>> branches;
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                branches.push_back(collect_states(centroid, first.to, first.digit));
                all_states.insert(all_states.end(), branches.back().begin(), branches.back().end());
            }
            answer += count_ordered_pairs(all_states);
            for (const auto& branch : branches) answer -= count_ordered_pairs(branch);
            --answer;  // 重心自身から重心自身への空パスを除く
            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to);
            }
        };
        decompose(decompose, 0);
        return answer;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;
    vector<int> power_ten;
    vector<int> inverse_power_ten;

    static int extended_gcd(int a, int b, long long& x, long long& y) {
        if (b == 0) {
            x = 1;
            y = 0;
            return a;
        }
        long long next_x, next_y;
        int g = extended_gcd(b, a % b, next_x, next_y);
        x = next_y;
        y = next_x - 1LL * (a / b) * next_y;
        return g;
    }

    static int modular_inverse(int value, int modulus) {
        long long x, y;
        int g = extended_gcd(value, modulus, x, y);
        (void)g;
        assert(g == 1);
        x %= modulus;
        if (x < 0) x += modulus;
        return static_cast<int>(x);
    }

    int multiply_mod(int a, int b) const {
        return static_cast<int>(static_cast<__int128>(a) * b % mod);
    }

    int normalize(long long value) const {
        value %= mod;
        if (value < 0) value += mod;
        return static_cast<int>(value);
    }

    long long count_ordered_pairs(const vector<State>& states) const {
        vector<int> up_values;
        up_values.reserve(states.size());
        for (const State& state : states) up_values.push_back(state.up);
        sort(up_values.begin(), up_values.end());
        long long answer = 0;

        // v側の長さを逆冪で消し、必要なup値を二分探索する
        for (const State& state : states) {
            int scaled_down = multiply_mod(state.down, inverse_power_ten[state.depth]);
            int required_up = normalize(-scaled_down);
            auto range = equal_range(up_values.begin(), up_values.end(), required_up);
            answer += range.second - range.first;
        }
        return answer;
    }

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    vector<State> collect_states(int centroid, int start, int initial_digit) const {
        int digit = initial_digit % mod;
        vector<State> states;
        auto dfs = [&](auto&& self, int v, int parent, int up, int down, int depth) -> void {
            states.push_back({up, down, depth});
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                int next_depth = depth + 1;
                int next_down = normalize(1LL * down * 10 + edge.digit);
                int next_up = normalize(1LL * edge.digit * power_ten[depth] + up);
                self(self, edge.to, v, next_up, next_down, next_depth);
            }
        };
        dfs(dfs, start, centroid, digit, digit, 1);
        return states;
    }
};

// 頂点値のパスGCDごとの単一頂点を含む順序なしパス数を求めるデータ構造
struct CentroidDecompositionPathGcdFrequency {
    int n;
    int edge_count = 0;
    vector<int> value;
    vector<vector<int>> graph;

    // 非負頂点値valuesを持つn頂点の空の木を構築する O(n)
    explicit CentroidDecompositionPathGcdFrequency(const vector<int>& values)
        : n(static_cast<int>(values.size())), value(values), graph(n), removed(n, false),
          parent_work(n, -1), subtree_size(n, 0) {
        for (int x : value) { (void)x; assert(x >= 0); }
    }

    // 無向辺u-vを追加する O(1)
    void add_edge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        graph[u].push_back(v);
        graph[v].push_back(u);
        ++edge_count;
    }

    // indexをGCD値とするパス数配列を返す O(n log n + ΣS_c^2)
    vector<long long> frequency() {
        assert(n == 0 || edge_count == n - 1);
        int maximum_value = n == 0 ? 0 : *max_element(value.begin(), value.end());
        vector<long long> answer(maximum_value + 1, 0);
        fill(removed.begin(), removed.end(), false);
        if (n == 0) return answer;

        // 各重心で圧縮した半パスGCD状態を枝順に結合する
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            ++answer[value[centroid]];
            vector<pair<int, long long>> previous = {{value[centroid], 1}};
            for (int first : graph[centroid]) {
                if (removed[first]) continue;
                vector<pair<int, long long>> branch = collect_frequency(centroid, first,
                                                                       std::gcd(value[centroid], value[first]));
                for (auto [left_value, left_count] : branch) {
                    for (auto [right_value, right_count] : previous) {
                        answer[std::gcd(left_value, right_value)] += left_count * right_count;
                    }
                }
                previous = merge_frequency(move(previous), branch);
            }
            for (int next_start : graph[centroid]) {
                if (!removed[next_start]) self(self, next_start);
            }
        };
        decompose(decompose, 0);
        return answer;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;

    static vector<pair<int, long long>> compress_values(vector<int> values) {
        sort(values.begin(), values.end());
        vector<pair<int, long long>> result;
        for (int x : values) {
            if (result.empty() || result.back().first != x) result.push_back({x, 1});
            else ++result.back().second;
        }
        return result;
    }

    static vector<pair<int, long long>> merge_frequency(vector<pair<int, long long>> left,
                                                        const vector<pair<int, long long>>& right) {
        left.insert(left.end(), right.begin(), right.end());
        sort(left.begin(), left.end());
        vector<pair<int, long long>> result;
        for (auto [x, count] : left) {
            if (result.empty() || result.back().first != x) result.push_back({x, count});
            else result.back().second += count;
        }
        return result;
    }

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (int to : graph[v]) {
                if (removed[to] || to == parent) continue;
                self(self, to, v);
                subtree_size[v] += subtree_size[to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (int to : graph[v]) {
                if (!removed[to] && parent_work[to] == v) max_part = max(max_part, subtree_size[to]);
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    vector<pair<int, long long>> collect_frequency(int centroid, int start, int initial_gcd) const {
        vector<int> values;
        auto dfs = [&](auto&& self, int v, int parent, int aggregate) -> void {
            values.push_back(aggregate);
            for (int to : graph[v]) {
                if (removed[to] || to == parent) continue;
                self(self, to, v, std::gcd(aggregate, value[to]));
            }
        };
        dfs(dfs, start, centroid, initial_gcd);
        return compress_values(move(values));
    }
};

// 小さいビット幅で頂点値のパスANDごとの単一頂点を含む順序なしパス数を求めるデータ構造
struct CentroidDecompositionPathAndFrequencySmallState {
    int n;
    int edge_count = 0;
    int bit_count;
    vector<int> value;
    vector<vector<int>> graph;

    // bit_countビットの頂点値valuesを持つn頂点の空の木を構築する O(n)
    CentroidDecompositionPathAndFrequencySmallState(const vector<int>& values, int bit_count_)
        : n(static_cast<int>(values.size())), bit_count(bit_count_), value(values), graph(n),
          removed(n, false), parent_work(n, -1), subtree_size(n, 0) {
        assert(0 <= bit_count && bit_count <= 20);
        int limit = 1 << bit_count;
        (void)limit;
        for (int x : value) { (void)x; assert(0 <= x && x < limit); }
    }

    // 無向辺u-vを追加する O(1)
    void add_edge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        graph[u].push_back(v);
        graph[v].push_back(u);
        ++edge_count;
    }

    // indexをAND値とするパス数配列を返す O(n log n + ΣS_c^2)
    vector<long long> frequency() {
        assert(n == 0 || edge_count == n - 1);
        vector<long long> answer(1 << bit_count, 0);
        fill(removed.begin(), removed.end(), false);
        if (n == 0) return answer;
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            ++answer[value[centroid]];
            vector<pair<int, long long>> previous = {{value[centroid], 1}};
            for (int first : graph[centroid]) {
                if (removed[first]) continue;
                auto branch = collect_frequency(centroid, first, value[centroid] & value[first]);
                for (auto [a, count_a] : branch) {
                    for (auto [b, count_b] : previous) answer[a & b] += count_a * count_b;
                }
                previous = merge_frequency(move(previous), branch);
            }
            for (int next_start : graph[centroid]) {
                if (!removed[next_start]) self(self, next_start);
            }
        };
        decompose(decompose, 0);
        return answer;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;

    static vector<pair<int, long long>> compress_values(vector<int> values) {
        sort(values.begin(), values.end());
        vector<pair<int, long long>> result;
        for (int x : values) {
            if (result.empty() || result.back().first != x) result.push_back({x, 1});
            else ++result.back().second;
        }
        return result;
    }

    static vector<pair<int, long long>> merge_frequency(vector<pair<int, long long>> left,
                                                        const vector<pair<int, long long>>& right) {
        left.insert(left.end(), right.begin(), right.end());
        sort(left.begin(), left.end());
        vector<pair<int, long long>> result;
        for (auto [x, count] : left) {
            if (result.empty() || result.back().first != x) result.push_back({x, count});
            else result.back().second += count;
        }
        return result;
    }

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (int to : graph[v]) {
                if (removed[to] || to == parent) continue;
                self(self, to, v);
                subtree_size[v] += subtree_size[to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (int to : graph[v]) {
                if (!removed[to] && parent_work[to] == v) max_part = max(max_part, subtree_size[to]);
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    vector<pair<int, long long>> collect_frequency(int centroid, int start, int initial) const {
        vector<int> values;
        auto dfs = [&](auto&& self, int v, int parent, int aggregate) -> void {
            values.push_back(aggregate);
            for (int to : graph[v]) {
                if (removed[to] || to == parent) continue;
                self(self, to, v, aggregate & value[to]);
            }
        };
        dfs(dfs, start, centroid, initial);
        return compress_values(move(values));
    }
};

// 小さいビット幅で頂点値のパスORごとの単一頂点を含む順序なしパス数を求めるデータ構造
struct CentroidDecompositionPathOrFrequencySmallState {
    int n;
    int edge_count = 0;
    int bit_count;
    vector<int> value;
    vector<vector<int>> graph;

    // bit_countビットの頂点値valuesを持つn頂点の空の木を構築する O(n)
    CentroidDecompositionPathOrFrequencySmallState(const vector<int>& values, int bit_count_)
        : n(static_cast<int>(values.size())), bit_count(bit_count_), value(values), graph(n),
          removed(n, false), parent_work(n, -1), subtree_size(n, 0) {
        assert(0 <= bit_count && bit_count <= 20);
        int limit = 1 << bit_count;
        (void)limit;
        for (int x : value) { (void)x; assert(0 <= x && x < limit); }
    }

    // 無向辺u-vを追加する O(1)
    void add_edge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        graph[u].push_back(v);
        graph[v].push_back(u);
        ++edge_count;
    }

    // indexをOR値とするパス数配列を返す O(n log n + ΣS_c^2)
    vector<long long> frequency() {
        assert(n == 0 || edge_count == n - 1);
        vector<long long> answer(1 << bit_count, 0);
        fill(removed.begin(), removed.end(), false);
        if (n == 0) return answer;
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            ++answer[value[centroid]];
            vector<pair<int, long long>> previous = {{value[centroid], 1}};
            for (int first : graph[centroid]) {
                if (removed[first]) continue;
                auto branch = collect_frequency(centroid, first, value[centroid] | value[first]);
                for (auto [a, count_a] : branch) {
                    for (auto [b, count_b] : previous) answer[a | b] += count_a * count_b;
                }
                previous = merge_frequency(move(previous), branch);
            }
            for (int next_start : graph[centroid]) {
                if (!removed[next_start]) self(self, next_start);
            }
        };
        decompose(decompose, 0);
        return answer;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;

    static vector<pair<int, long long>> compress_values(vector<int> values) {
        sort(values.begin(), values.end());
        vector<pair<int, long long>> result;
        for (int x : values) {
            if (result.empty() || result.back().first != x) result.push_back({x, 1});
            else ++result.back().second;
        }
        return result;
    }

    static vector<pair<int, long long>> merge_frequency(vector<pair<int, long long>> left,
                                                        const vector<pair<int, long long>>& right) {
        left.insert(left.end(), right.begin(), right.end());
        sort(left.begin(), left.end());
        vector<pair<int, long long>> result;
        for (auto [x, count] : left) {
            if (result.empty() || result.back().first != x) result.push_back({x, count});
            else result.back().second += count;
        }
        return result;
    }

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (int to : graph[v]) {
                if (removed[to] || to == parent) continue;
                self(self, to, v);
                subtree_size[v] += subtree_size[to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (int to : graph[v]) {
                if (!removed[to] && parent_work[to] == v) max_part = max(max_part, subtree_size[to]);
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    vector<pair<int, long long>> collect_frequency(int centroid, int start, int initial) const {
        vector<int> values;
        auto dfs = [&](auto&& self, int v, int parent, int aggregate) -> void {
            values.push_back(aggregate);
            for (int to : graph[v]) {
                if (removed[to] || to == parent) continue;
                self(self, to, v, aggregate | value[to]);
            }
        };
        dfs(dfs, start, centroid, initial);
        return compress_values(move(values));
    }
};

// ラベルが等しい異なる頂点対の個数と距離総和を求めるデータ構造
struct CentroidDecompositionSameLabelDistance {
    using ll = long long;

    struct Edge {
        int to;
        ll weight;
    };

    struct State {
        int vertex;
        ll distance;
    };

    struct Result {
        long long pair_count = 0;
        long long distance_sum = 0;
    };

    int n;
    int edge_count = 0;
    vector<int> label;
    vector<vector<Edge>> graph;

    // 任意整数ラベルlabelsを持つn頂点の空の重み付き木を構築する O(n log n)
    explicit CentroidDecompositionSameLabelDistance(const vector<int>& labels)
        : n(static_cast<int>(labels.size())), label(labels), graph(n), removed(n, false),
          parent_work(n, -1), subtree_size(n, 0) {
        vector<int> coordinates = label;
        sort(coordinates.begin(), coordinates.end());
        coordinates.erase(unique(coordinates.begin(), coordinates.end()), coordinates.end());
        for (int& x : label) {
            x = static_cast<int>(lower_bound(coordinates.begin(), coordinates.end(), x) - coordinates.begin());
        }
        label_count = static_cast<int>(coordinates.size());
    }

    // 重みweightの無向辺u-vを追加する O(1)
    void add_edge(int u, int v, ll weight = 1) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        graph[u].push_back({v, weight});
        graph[v].push_back({u, weight});
        ++edge_count;
    }

    // 同ラベル異頂点対の個数と距離総和を返す O(n log n)
    Result solve() {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        Result answer;
        if (n == 0) return answer;
        vector<long long> count(label_count, 0);
        vector<long long> distance_sum(label_count, 0);
        vector<int> touched;

        // 各重心で問い合わせ後に枝をマージし、同一枝内の組を再帰へ委ねる
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            count[label[centroid]] = 1;
            distance_sum[label[centroid]] = 0;
            touched.push_back(label[centroid]);
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                vector<State> branch = collect_states(centroid, first.to, first.weight);
                for (const State& state : branch) {
                    int current_label = label[state.vertex];
                    answer.pair_count += count[current_label];
                    answer.distance_sum += distance_sum[current_label] + count[current_label] * state.distance;
                }
                for (const State& state : branch) {
                    int current_label = label[state.vertex];
                    if (count[current_label] == 0) touched.push_back(current_label);
                    ++count[current_label];
                    distance_sum[current_label] += state.distance;
                }
            }
            for (int current_label : touched) {
                count[current_label] = 0;
                distance_sum[current_label] = 0;
            }
            touched.clear();
            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to);
            }
        };
        decompose(decompose, 0);
        return answer;
    }

private:
    int label_count = 0;
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    vector<State> collect_states(int centroid, int start, ll initial_distance) const {
        vector<State> states;
        auto dfs = [&](auto&& self, int v, int parent, ll distance) -> void {
            states.push_back({v, distance});
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v, distance + edge.weight);
            }
        };
        dfs(dfs, start, centroid, initial_distance);
        return states;
    }
};

// 小さいクラス数についてクラス組別の異頂点対数と距離総和を求めるデータ構造
struct CentroidDecompositionSmallClassDistance {
    using ll = long long;

    struct Edge {
        int to;
        ll weight;
    };

    struct State {
        int vertex;
        ll distance;
    };

    struct Result {
        vector<vector<long long>> pair_count;
        vector<vector<long long>> distance_sum;
    };

    int n;
    int edge_count = 0;
    int class_count;
    vector<int> vertex_class;
    vector<vector<Edge>> graph;

    // 0以上class_count未満の固定クラスclassesを持つ空の重み付き木を構築する O(n)
    CentroidDecompositionSmallClassDistance(const vector<int>& classes, int class_count_)
        : n(static_cast<int>(classes.size())), class_count(class_count_), vertex_class(classes), graph(n),
          removed(n, false), parent_work(n, -1), subtree_size(n, 0) {
        assert(class_count >= 0);
        for (int x : vertex_class) { (void)x; assert(0 <= x && x < class_count); }
    }

    // 重みweightの無向辺u-vを追加する O(1)
    void add_edge(int u, int v, ll weight = 1) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        graph[u].push_back({v, weight});
        graph[v].push_back({u, weight});
        ++edge_count;
    }

    // 対称行列[class_a][class_b]へ異頂点対数と距離総和を格納して返す O(C n log n + C^2)
    Result solve() {
        assert(n == 0 || edge_count == n - 1);
        fill(removed.begin(), removed.end(), false);
        Result answer{vector<vector<long long>>(class_count, vector<long long>(class_count, 0)),
                      vector<vector<long long>>(class_count, vector<long long>(class_count, 0))};
        if (n == 0) return answer;
        vector<long long> count(class_count, 0);
        vector<long long> distance_sum(class_count, 0);

        // 各重心で現在枝の各頂点を過去枝の全クラスと結合する
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            fill(count.begin(), count.end(), 0);
            fill(distance_sum.begin(), distance_sum.end(), 0);
            count[vertex_class[centroid]] = 1;
            for (const Edge& first : graph[centroid]) {
                if (removed[first.to]) continue;
                vector<State> branch = collect_states(centroid, first.to, first.weight);
                for (const State& state : branch) {
                    int a = vertex_class[state.vertex];
                    for (int b = 0; b < class_count; ++b) {
                        if (count[b] == 0) continue;
                        long long pair_add = count[b];
                        long long distance_add = distance_sum[b] + count[b] * state.distance;
                        answer.pair_count[a][b] += pair_add;
                        answer.distance_sum[a][b] += distance_add;
                        if (a != b) {
                            answer.pair_count[b][a] += pair_add;
                            answer.distance_sum[b][a] += distance_add;
                        }
                    }
                }
                for (const State& state : branch) {
                    int a = vertex_class[state.vertex];
                    ++count[a];
                    distance_sum[a] += state.distance;
                }
            }
            for (const Edge& edge : graph[centroid]) {
                if (!removed[edge.to]) self(self, edge.to);
            }
        };
        decompose(decompose, 0);
        return answer;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v);
                subtree_size[v] += subtree_size[edge.to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (const Edge& edge : graph[v]) {
                if (!removed[edge.to] && parent_work[edge.to] == v) {
                    max_part = max(max_part, subtree_size[edge.to]);
                }
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    vector<State> collect_states(int centroid, int start, ll initial_distance) const {
        vector<State> states;
        auto dfs = [&](auto&& self, int v, int parent, ll distance) -> void {
            states.push_back({v, distance});
            for (const Edge& edge : graph[v]) {
                if (removed[edge.to] || edge.to == parent) continue;
                self(self, edge.to, v, distance + edge.weight);
            }
        };
        dfs(dfs, start, centroid, initial_distance);
        return states;
    }
};

// 非重み付き木の距離ごとの異なる順序なし頂点対数を求めるデータ構造
struct CentroidDecompositionDistanceFrequency {
    using ll = long long;

    int n;
    int edge_count = 0;
    vector<vector<int>> graph;

    // n頂点の空の非重み付き木を構築する O(n)
    explicit CentroidDecompositionDistanceFrequency(int vertex_count)
        : n(vertex_count), graph(vertex_count), removed(vertex_count, false), parent_work(vertex_count, -1), subtree_size(vertex_count, 0) {}

    // 無向辺u-vを追加する O(1)
    void add_edge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n && u != v);
        graph[u].push_back(v);
        graph[v].push_back(u);
        ++edge_count;
    }

    // frequency[d]を距離dの異なる順序なし頂点対数として返す O(n log^2 n)
    vector<long long> frequency() {
        assert(n == 0 || edge_count == n - 1);
        vector<long long> answer(n, 0);
        fill(removed.begin(), removed.end(), false);
        if (n == 0) return answer;

        // 各重心で全深さ頻度の自己畳み込みから枝別自己畳み込みを引く
        auto decompose = [&](auto&& self, int start) -> void {
            if (removed[start]) return;
            int centroid = find_centroid(start);
            removed[centroid] = true;
            vector<ll> all_frequency(1, 1);
            vector<vector<ll>> branch_frequency;
            for (int first : graph[centroid]) {
                if (removed[first]) continue;
                branch_frequency.push_back(collect_depth_frequency(centroid, first));
                if (all_frequency.size() < branch_frequency.back().size()) {
                    all_frequency.resize(branch_frequency.back().size(), 0);
                }
                for (int depth = 0; depth < static_cast<int>(branch_frequency.back().size()); ++depth) {
                    all_frequency[depth] += branch_frequency.back()[depth];
                }
            }

            // 順序付き組の差を2で割り、現在重心で初めて分離される頂点対を加える
            vector<ll> contribution = convolution_exact(all_frequency, all_frequency);
            for (const auto& branch : branch_frequency) {
                vector<ll> same_branch = convolution_exact(branch, branch);
                for (int i = 0; i < static_cast<int>(same_branch.size()); ++i) contribution[i] -= same_branch[i];
            }
            int maximum_distance = min(n - 1, static_cast<int>(contribution.size()) - 1);
            for (int distance = 1; distance <= maximum_distance; ++distance) {
                assert(contribution[distance] >= 0 && contribution[distance] % 2 == 0);
                answer[distance] += contribution[distance] / 2;
            }
            for (int next_start : graph[centroid]) {
                if (!removed[next_start]) self(self, next_start);
            }
        };
        decompose(decompose, 0);
        return answer;
    }

private:
    vector<char> removed;
    vector<int> parent_work;
    vector<int> subtree_size;
    vector<int> component;

    static vector<ll> convolution_exact(const vector<ll>& left, const vector<ll>& right) {
        if (left.empty() || right.empty()) return {};
        return convolution_ll(left, right);
    }

    int find_centroid(int start) {
        component.clear();

        // 現在成分をDFSで列挙しながら部分木サイズを求める
        auto dfs = [&](auto&& self, int v, int parent) -> void {
            parent_work[v] = parent;
            component.push_back(v);
            subtree_size[v] = 1;
            for (int to : graph[v]) {
                if (removed[to] || to == parent) continue;
                self(self, to, v);
                subtree_size[v] += subtree_size[to];
            }
        };
        dfs(dfs, start, -1);

        int component_size = static_cast<int>(component.size());
        int centroid = -1;
        int best_max_part = component_size + 1;
        for (int v : component) {
            int max_part = component_size - subtree_size[v];
            for (int to : graph[v]) {
                if (!removed[to] && parent_work[to] == v) max_part = max(max_part, subtree_size[to]);
            }
            if (max_part < best_max_part || (max_part == best_max_part && v < centroid)) {
                best_max_part = max_part;
                centroid = v;
            }
        }
        return centroid;
    }

    vector<ll> collect_depth_frequency(int centroid, int start) const {
        vector<ll> frequency(2, 0);
        auto dfs = [&](auto&& self, int v, int parent, int depth) -> void {
            if (frequency.size() <= static_cast<size_t>(depth)) frequency.resize(depth + 1, 0);
            ++frequency[depth];
            for (int to : graph[v]) {
                if (removed[to] || to == parent) continue;
                self(self, to, v, depth + 1);
            }
        };
        dfs(dfs, start, centroid, 1);
        return frequency;
    }
};

// ============================================================================
// ユースケースソルバー関数
// ============================================================================

// 重心分解木の親配列を返す利用例 O(n log n)
inline vector<int> solve_centroid_tree_parent(int n, const vector<pair<int, int>>& edges) {
    CentroidDecompositionTree decomposition(n);
    for (auto [u, v] : edges) decomposition.add_edge(u, v);
    decomposition.build();
    return decomposition.centroid_parent;
}

// 重心分解木の深さ配列を返す利用例 O(n log n)
inline vector<int> solve_centroid_tree_depth(int n, const vector<pair<int, int>>& edges) {
    CentroidDecompositionTree decomposition(n);
    for (auto [u, v] : edges) decomposition.add_edge(u, v);
    decomposition.build();
    return decomposition.centroid_depth;
}

// 各頂点の重心祖先・枝・距離を返す利用例 O(n log n)
inline vector<vector<CentroidDecompositionPathIndex::PathEntry>>
solve_centroid_path_index(int n, const vector<tuple<int, int, long long>>& edges) {
    CentroidDecompositionPathIndex decomposition(n);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    decomposition.build();
    return decomposition.path;
}

struct NearestActiveOperation {
    int type;    // 0: toggle, 1: query
    int vertex;
};

// toggleと最近傍距離問い合わせを処理する利用例 O((n+q) log^2 n)
inline vector<long long> solve_nearest_active_queries(
    int n, const vector<tuple<int, int, long long>>& edges,
    const vector<NearestActiveOperation>& operations) {
    CentroidDecompositionNearestActive decomposition(n);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    decomposition.build();
    vector<long long> answers;
    for (const auto& operation : operations) {
        if (operation.type == 0) decomposition.toggle(operation.vertex);
        else answers.push_back(decomposition.nearest_distance(operation.vertex));
    }
    return answers;
}

struct ActiveCountWithinOperation {
    int type;    // 0: toggle, 1: query
    int vertex;
    long long distance;
};

// toggleと距離上限付き有効頂点数問い合わせを処理する利用例 O((n+q) log^2 n)
inline vector<long long> solve_active_count_within_distance_queries(
    int n, const vector<tuple<int, int, long long>>& edges,
    const vector<ActiveCountWithinOperation>& operations) {
    CentroidDecompositionActiveDistanceCount decomposition(n);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    decomposition.build();
    vector<long long> answers;
    for (const auto& operation : operations) {
        if (operation.type == 0) decomposition.toggle(operation.vertex);
        else answers.push_back(decomposition.count_within(operation.vertex, operation.distance));
    }
    return answers;
}

struct ActiveCountAtDistanceOperation {
    int type;    // 0: toggle, 1: query
    int vertex;
    long long distance;
};

// toggleと距離一致有効頂点数問い合わせを処理する利用例 O((n+q) log^2 n)
inline vector<long long> solve_active_count_at_distance_queries(
    int n, const vector<tuple<int, int, long long>>& edges,
    const vector<ActiveCountAtDistanceOperation>& operations) {
    CentroidDecompositionActiveDistanceCount decomposition(n);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    decomposition.build();
    vector<long long> answers;
    for (const auto& operation : operations) {
        if (operation.type == 0) decomposition.toggle(operation.vertex);
        else answers.push_back(decomposition.count_at_distance(operation.vertex, operation.distance));
    }
    return answers;
}

struct ActiveCountRangeOperation {
    int type;    // 0: toggle, 1: query
    int vertex;
    long long lower;
    long long upper;
};

// toggleと距離区間有効頂点数問い合わせを処理する利用例 O((n+q) log^2 n)
inline vector<long long> solve_active_count_in_distance_range_queries(
    int n, const vector<tuple<int, int, long long>>& edges,
    const vector<ActiveCountRangeOperation>& operations) {
    CentroidDecompositionActiveDistanceCount decomposition(n);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    decomposition.build();
    vector<long long> answers;
    for (const auto& operation : operations) {
        if (operation.type == 0) decomposition.toggle(operation.vertex);
        else answers.push_back(decomposition.count_in_range(operation.vertex, operation.lower, operation.upper));
    }
    return answers;
}

struct NearestActiveByColorOperation {
    int type;    // 0: toggle, 1: query
    int vertex;
    int target_color;
};

// toggleと指定色最近傍距離問い合わせを処理する利用例 O((n+q) log^2 n)
inline vector<long long> solve_nearest_active_by_color_queries(
    const vector<int>& colors, const vector<tuple<int, int, long long>>& edges,
    const vector<NearestActiveByColorOperation>& operations) {
    CentroidDecompositionNearestActiveByColor decomposition(colors);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    decomposition.build();
    vector<long long> answers;
    for (const auto& operation : operations) {
        if (operation.type == 0) decomposition.toggle(operation.vertex);
        else answers.push_back(decomposition.nearest_distance(operation.vertex, operation.target_color));
    }
    return answers;
}

struct ActiveDiameterOperation {
    int type;    // 0: toggle, 1: query
    int vertex;
};

// toggleと有効頂点集合直径問い合わせを処理する利用例 O((n+q) log^2 n)
inline vector<long long> solve_active_diameter_queries(
    int n, const vector<tuple<int, int, long long>>& edges,
    const vector<ActiveDiameterOperation>& operations) {
    CentroidDecompositionActiveDiameter decomposition(n);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    decomposition.build();
    vector<long long> answers;
    for (const auto& operation : operations) {
        if (operation.type == 0) decomposition.toggle(operation.vertex);
        else answers.push_back(decomposition.diameter());
    }
    return answers;
}

struct ActiveClosestPairOperation {
    int type;    // 0: toggle, 1: query
    int vertex;
};

// toggleと有効頂点間最短距離問い合わせを処理する利用例 O((n+q) log^2 n)
inline vector<long long> solve_active_closest_pair_queries(
    int n, const vector<tuple<int, int, long long>>& edges,
    const vector<ActiveClosestPairOperation>& operations) {
    CentroidDecompositionActiveClosestPair decomposition(n);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    decomposition.build();
    vector<long long> answers;
    for (const auto& operation : operations) {
        if (operation.type == 0) decomposition.toggle(operation.vertex);
        else answers.push_back(decomposition.minimum_pair_distance());
    }
    return answers;
}

struct ActivePairWithinOperation {
    int type;    // 0: toggle, 1: query
    int vertex;
    long long maximum_distance;
};

// toggleと距離上限内の有効頂点対存在判定を処理する利用例 O((n+q) log^2 n)
inline vector<char> solve_exists_active_pair_within_distance_queries(
    int n, const vector<tuple<int, int, long long>>& edges,
    const vector<ActivePairWithinOperation>& operations) {
    CentroidDecompositionActiveClosestPair decomposition(n);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    decomposition.build();
    vector<char> answers;
    for (const auto& operation : operations) {
        if (operation.type == 0) decomposition.toggle(operation.vertex);
        else answers.push_back(decomposition.exists_pair_within(operation.maximum_distance));
    }
    return answers;
}

struct ActiveDifferentColorDiameterOperation {
    int type;    // 0: toggle, 1: query
    int vertex;
};

// toggleと異色有効頂点間最大距離問い合わせを処理する利用例 O((n+q) log^3 n)
inline vector<long long> solve_active_different_color_diameter_queries(
    const vector<int>& colors, const vector<tuple<int, int, long long>>& edges,
    const vector<ActiveDifferentColorDiameterOperation>& operations) {
    CentroidDecompositionActiveDifferentColorDiameter decomposition(colors);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    decomposition.build();
    vector<long long> answers;
    for (const auto& operation : operations) {
        if (operation.type == 0) decomposition.toggle(operation.vertex);
        else answers.push_back(decomposition.diameter_between_different_colors());
    }
    return answers;
}

struct ActivePathXorOperation {
    int type;    // 0: toggle, 1: query
    int vertex;
    int target_xor;
};

// toggleとパス辺XOR一致数問い合わせを処理する利用例 O((n+q) log^2 n)
inline vector<long long> solve_active_path_xor_count_queries(
    int n, const vector<tuple<int, int, int>>& edges,
    const vector<ActivePathXorOperation>& operations) {
    CentroidDecompositionActivePathXorCount decomposition(n);
    for (auto [u, v, value] : edges) decomposition.add_edge(u, v, value);
    decomposition.build();
    vector<long long> answers;
    for (const auto& operation : operations) {
        if (operation.type == 0) decomposition.toggle(operation.vertex);
        else answers.push_back(decomposition.count_path_xor(operation.vertex, operation.target_xor));
    }
    return answers;
}

// パス辺XORがtarget_xorとなる異なる順序なし頂点対数を返す利用例 O(n log^2 n)
inline long long solve_static_path_xor_exact_count(
    int n, const vector<tuple<int, int, int>>& edges, int target_xor) {
    CentroidDecompositionActivePathXorCount decomposition(n);
    for (auto [u, v, value] : edges) decomposition.add_edge(u, v, value);
    decomposition.build();
    for (int v = 0; v < n; ++v) decomposition.activate(v);
    long long ordered = 0;
    for (int v = 0; v < n; ++v) ordered += decomposition.count_path_xor(v, target_xor);
    if (target_xor == 0) ordered -= n;
    assert(ordered % 2 == 0);
    return ordered / 2;
}

struct MaximumBottleneckOperation {
    int type;    // 0: toggle, 1: query
    int vertex;
};

// toggleと有効頂点への最大ボトルネック問い合わせを処理する利用例 O((n+q) log^2 n)
inline vector<long long> solve_maximum_bottleneck_to_active_queries(
    int n, const vector<tuple<int, int, long long>>& edges,
    const vector<MaximumBottleneckOperation>& operations) {
    CentroidDecompositionMaximumBottleneckToActive decomposition(n);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    decomposition.build();
    vector<long long> answers;
    for (const auto& operation : operations) {
        if (operation.type == 0) decomposition.toggle(operation.vertex);
        else answers.push_back(decomposition.maximum_bottleneck(operation.vertex));
    }
    return answers;
}

struct MinimumMaximumEdgeOperation {
    int type;    // 0: toggle, 1: query
    int vertex;
};

// toggleと有効頂点への最小minimax辺問い合わせを処理する利用例 O((n+q) log^2 n)
inline vector<long long> solve_minimum_max_edge_to_active_queries(
    int n, const vector<tuple<int, int, long long>>& edges,
    const vector<MinimumMaximumEdgeOperation>& operations) {
    CentroidDecompositionMinimumMaximumEdgeToActive decomposition(n);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    decomposition.build();
    vector<long long> answers;
    for (const auto& operation : operations) {
        if (operation.type == 0) decomposition.toggle(operation.vertex);
        else answers.push_back(decomposition.minimum_maximum_edge(operation.vertex));
    }
    return answers;
}

// パス辺重み和がtargetとなる異なる順序なし頂点対数を返す利用例 O(n log^2 n)
inline long long solve_path_sum_exact_count(
    int n, const vector<tuple<int, int, long long>>& edges, long long target) {
    CentroidDecompositionPathSumCount decomposition(n);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    return decomposition.count_exact(target);
}

// パス辺重み和がlimit以下となる異なる順序なし頂点対数を返す利用例 O(n log^2 n)
inline long long solve_path_sum_at_most_count(
    int n, const vector<tuple<int, int, long long>>& edges, long long limit) {
    CentroidDecompositionPathSumCount decomposition(n);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    return decomposition.count_at_most(limit);
}

// パス辺重み和が[lower,upper]となる異なる順序なし頂点対数を返す利用例 O(n log^2 n)
inline long long solve_path_sum_range_count(
    int n, const vector<tuple<int, int, long long>>& edges, long long lower, long long upper) {
    CentroidDecompositionPathSumCount decomposition(n);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    return decomposition.count_in_range(lower, upper);
}

// パス辺重み和がtargetとなる異なる頂点対の最小辺数を返す利用例 O(n log^2 n)
inline int solve_exact_path_sum_min_edges(
    int n, const vector<tuple<int, int, long long>>& edges, long long target) {
    CentroidDecompositionExactPathSumMinEdges decomposition(n);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    return decomposition.minimum_edges(target);
}

// パス辺重み和がtargetとなる異なる頂点対の最大辺数を返す利用例 O(n log^2 n)
inline int solve_exact_path_sum_max_edges(
    int n, const vector<tuple<int, int, long long>>& edges, long long target) {
    vector<long long> score(n, 1);
    CentroidDecompositionExactPathSumMaxVertexScore decomposition(score);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    long long vertices = decomposition.maximum_vertex_score(target);
    if (vertices == CentroidDecompositionExactPathSumMaxVertexScore::NO_SOLUTION) return -1;
    return static_cast<int>(vertices - 1);
}

// パス辺重み和がtargetとなる異なる頂点対の最大利益を返す利用例 O(n log^2 n)
inline long long solve_exact_path_sum_max_profit(
    const vector<long long>& profit, const vector<tuple<int, int, long long>>& edges, long long target) {
    CentroidDecompositionExactPathSumMaxVertexScore decomposition(profit);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    return decomposition.maximum_vertex_score(target);
}

// 頂点値積がlimit以下となるパスの最大辺数を返す利用例 O(n log^2 n)
inline int solve_path_product_at_most_max_edges(
    const vector<long long>& values, const vector<pair<int, int>>& edges, long long limit) {
    CentroidDecompositionPathProductMaxEdges decomposition(values);
    for (auto [u, v] : edges) decomposition.add_edge(u, v);
    return decomposition.maximum_edges(limit);
}

// 頂点マスクXORが許可集合に属するパス数を返す利用例 O(A n log n)
inline long long solve_allowed_vertex_mask_path_count(
    const vector<int>& masks, int bit_count, const vector<pair<int, int>>& edges,
    const vector<int>& allowed_xors) {
    CentroidDecompositionVertexMaskPathCount decomposition(masks, bit_count);
    for (auto [u, v] : edges) decomposition.add_edge(u, v);
    return decomposition.count_allowed_xors(allowed_xors);
}

// 回文に並べ替え可能な単一頂点を含むパス数を返す利用例 O(B n log n)
inline long long solve_palindrome_permutable_path_count(
    const vector<int>& labels, int alphabet_size, const vector<pair<int, int>>& edges) {
    vector<int> masks(labels.size());
    for (int v = 0; v < static_cast<int>(labels.size()); ++v) masks[v] = 1 << labels[v];
    vector<int> allowed = {0};
    for (int bit = 0; bit < alphabet_size; ++bit) allowed.push_back(1 << bit);
    return solve_allowed_vertex_mask_path_count(masks, alphabet_size, edges, allowed);
}

// 奇数回現れるラベルがexactly_k種類の単一頂点を含むパス数を返す利用例 O(C(B,k) n log n)
inline long long solve_paths_with_exactly_k_odd_labels(
    const vector<int>& labels, int alphabet_size, const vector<pair<int, int>>& edges,
    int exactly_k) {
    assert(0 <= exactly_k && exactly_k <= alphabet_size);
    vector<int> masks(labels.size());
    for (int v = 0; v < static_cast<int>(labels.size()); ++v) masks[v] = 1 << labels[v];
    vector<int> allowed;
    for (int mask = 0; mask < (1 << alphabet_size); ++mask) {
        if (popcount(static_cast<unsigned>(mask)) == exactly_k) allowed.push_back(mask);
    }
    return solve_allowed_vertex_mask_path_count(masks, alphabet_size, edges, allowed);
}

// 各頂点を通る回文並べ替え可能パス数を返す利用例 O(B n log n)
inline vector<long long> solve_palindrome_permutable_paths_through_each_vertex(
    const vector<int>& labels, int alphabet_size, const vector<pair<int, int>>& edges) {
    CentroidDecompositionPalindromePathThroughVertexCount decomposition(labels, alphabet_size);
    for (auto [u, v] : edges) decomposition.add_edge(u, v);
    return decomposition.count_through_each_vertex();
}

// 辺数字列がmodで割り切れる異なる頂点の順序付き対数を返す利用例 O(n log^2 n)
inline long long solve_digit_path_divisible_ordered_pairs(
    int n, int mod, const vector<tuple<int, int, int>>& edges) {
    CentroidDecompositionDigitModuloCount decomposition(n, mod);
    for (auto [u, v, digit] : edges) decomposition.add_edge(u, v, digit);
    return decomposition.count_divisible_ordered_pairs();
}

// indexをパスGCDとする単一頂点を含むパス数配列を返す利用例 O(n log n + ΣS_c^2)
inline vector<long long> solve_path_gcd_frequency(
    const vector<int>& values, const vector<pair<int, int>>& edges) {
    CentroidDecompositionPathGcdFrequency decomposition(values);
    for (auto [u, v] : edges) decomposition.add_edge(u, v);
    return decomposition.frequency();
}

// indexをパスANDとする単一頂点を含むパス数配列を返す利用例 O(n log n + ΣS_c^2)
inline vector<long long> solve_path_and_frequency_small_state(
    const vector<int>& values, int bit_count, const vector<pair<int, int>>& edges) {
    CentroidDecompositionPathAndFrequencySmallState decomposition(values, bit_count);
    for (auto [u, v] : edges) decomposition.add_edge(u, v);
    return decomposition.frequency();
}

// indexをパスORとする単一頂点を含むパス数配列を返す利用例 O(n log n + ΣS_c^2)
inline vector<long long> solve_path_or_frequency_small_state(
    const vector<int>& values, int bit_count, const vector<pair<int, int>>& edges) {
    CentroidDecompositionPathOrFrequencySmallState decomposition(values, bit_count);
    for (auto [u, v] : edges) decomposition.add_edge(u, v);
    return decomposition.frequency();
}

// 同ラベル異頂点対数を返す利用例 O(n log n)
inline long long solve_same_label_pair_count(
    const vector<int>& labels, const vector<tuple<int, int, long long>>& edges) {
    CentroidDecompositionSameLabelDistance decomposition(labels);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    return decomposition.solve().pair_count;
}

// 同ラベル異頂点対の距離総和を返す利用例 O(n log n)
inline long long solve_same_label_distance_sum(
    const vector<int>& labels, const vector<tuple<int, int, long long>>& edges) {
    CentroidDecompositionSameLabelDistance decomposition(labels);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    return decomposition.solve().distance_sum;
}

// クラス組別の異頂点対数・距離総和行列を返す利用例 O(C n log n + C^2)
inline CentroidDecompositionSmallClassDistance::Result solve_small_class_pair_distance_matrices(
    const vector<int>& classes, int class_count,
    const vector<tuple<int, int, long long>>& edges) {
    CentroidDecompositionSmallClassDistance decomposition(classes, class_count);
    for (auto [u, v, weight] : edges) decomposition.add_edge(u, v, weight);
    return decomposition.solve();
}

// class 0とclass 1の異頂点対距離総和を返す利用例 O(n log n)
inline long long solve_red_blue_distance_sum(
    const vector<int>& red_blue_class, const vector<tuple<int, int, long long>>& edges) {
    auto result = solve_small_class_pair_distance_matrices(red_blue_class, 2, edges);
    return result.distance_sum[0][1];
}

// 対称なclass_relationを満たす異頂点対の距離総和を返す利用例 O(C n log n + C^2)
inline long long solve_selected_class_relation_distance_sum(
    const vector<int>& classes, int class_count,
    const vector<tuple<int, int, long long>>& edges,
    const vector<vector<char>>& class_relation) {
    assert(static_cast<int>(class_relation.size()) == class_count);
    for (int a = 0; a < class_count; ++a) {
        assert(static_cast<int>(class_relation[a].size()) == class_count);
        for (int b = 0; b < class_count; ++b) assert(class_relation[a][b] == class_relation[b][a]);
    }
    auto result = solve_small_class_pair_distance_matrices(classes, class_count, edges);
    long long answer = 0;
    for (int a = 0; a < class_count; ++a) {
        for (int b = a; b < class_count; ++b) {
            if (class_relation[a][b]) answer += result.distance_sum[a][b];
        }
    }
    return answer;
}

// 非重み付き木の距離頻度表を返す利用例 O(n log^2 n)
inline vector<long long> solve_tree_distance_frequency(
    int n, const vector<pair<int, int>>& edges) {
    CentroidDecompositionDistanceFrequency decomposition(n);
    for (auto [u, v] : edges) decomposition.add_edge(u, v);
    return decomposition.frequency();
}

// 3頂点のどれも他2頂点間パス上にない頂点三つ組数を距離頻度から返す利用例 O(n log^2 n)
inline long long solve_avoid_straight_line_using_distance_frequency(
    int n, const vector<pair<int, int>>& edges) {
    vector<long long> frequency = solve_tree_distance_frequency(n, edges);
    long long collinear = 0;
    for (int distance = 2; distance < n; ++distance) {
        collinear += frequency[distance] * (distance - 1);
    }
    long long all_triples = n < 3 ? 0 : 1LL * n * (n - 1) * (n - 2) / 6;
    return all_triples - collinear;
}

// ============================================================================
// テスト
// ============================================================================

#if __INCLUDE_LEVEL__ == 0

// 閉区間[lower, upper]の乱数をintで返すテスト用ヘルパー O(1)
int centroid_decomposition_test_random_int(mt19937& random, int lower, int upper) {
    assert(lower <= upper);
    return uniform_int_distribution<int>(lower, upper)(random);
}

struct CentroidDecompositionTestEdge {
    int to;
    long long sum_weight;
    long long distance_weight;
    int xor_value;
    int digit;
};

struct CentroidDecompositionTestPathEdge {
    long long sum_weight;
    long long distance_weight;
    int xor_value;
    int digit;
};

struct CentroidDecompositionTestTree {
    int n;
    vector<vector<CentroidDecompositionTestEdge>> graph;

    // n頂点のテスト用木を構築する O(n)
    explicit CentroidDecompositionTestTree(int vertex_count) : n(vertex_count), graph(vertex_count) {}

    // 複数用途の値を持つ無向辺を追加する O(1)
    void add_edge(int u, int v, long long sum_weight, long long distance_weight,
                  int xor_value, int digit) {
        graph[u].push_back({v, sum_weight, distance_weight, xor_value, digit});
        graph[v].push_back({u, sum_weight, distance_weight, xor_value, digit});
    }

    // uからvへの頂点列と向き付き辺列を返す O(n)
    pair<vector<int>, vector<CentroidDecompositionTestPathEdge>> path(int u, int v) const {
        vector<int> parent(n, -2);
        vector<CentroidDecompositionTestEdge> parent_edge(n);
        parent[u] = -1;

        // uを根とするDFSでvまでの親を記録する
        auto dfs = [&](auto&& self, int current) -> bool {
            if (current == v) return true;
            for (const auto& edge : graph[current]) {
                if (parent[edge.to] != -2) continue;
                parent[edge.to] = current;
                parent_edge[edge.to] = edge;
                if (self(self, edge.to)) return true;
            }
            return false;
        };
        dfs(dfs, u);

        // vからuへ復元した列を反転してuからvの順にする
        vector<int> vertices;
        vector<CentroidDecompositionTestPathEdge> edges;
        for (int current = v; current != u; current = parent[current]) {
            const auto& edge = parent_edge[current];
            vertices.push_back(current);
            edges.push_back({edge.sum_weight, edge.distance_weight, edge.xor_value, edge.digit});
        }
        vertices.push_back(u);
        reverse(vertices.begin(), vertices.end());
        reverse(edges.begin(), edges.end());
        return {vertices, edges};
    }
};

template <class Left, class Right>
void centroid_decomposition_test_equal(const Left& actual, const Right& expected, const char* name) {
    if (!(actual == expected)) {
        cerr << "テスト失敗: " << name << '\n';
        abort();
    }
}

void test_common_data_structures() {
    // ACL互換の半開区間和と一点加算を確認する
    fenwick_tree<long long> bit(8);
    vector<long long> values(8, 0);
    for (int i = 0; i < 8; ++i) {
        values[i] = i * i - 3;
        bit.add(i, values[i]);
    }
    for (int left = 0; left <= 8; ++left) {
        for (int right = left; right <= 8; ++right) {
            long long expected = accumulate(values.begin() + left, values.begin() + right, 0LL);
            centroid_decomposition_test_equal(bit.sum(left, right), expected, "fenwick_tree");
        }
    }

    // 重複値と非先頭要素の遅延削除を確認する
    DeletablePriorityQueue<int> maximum_queue;
    maximum_queue.push(3);
    maximum_queue.push(1);
    maximum_queue.push(3);
    maximum_queue.push(2);
    maximum_queue.erase(1);
    maximum_queue.erase(3);
    centroid_decomposition_test_equal(maximum_queue.top(), 3, "削除可能priority queue最大値");
    maximum_queue.pop();
    centroid_decomposition_test_equal(maximum_queue.top(), 2, "削除可能priority queue遅延削除");
    maximum_queue.erase(2);
    centroid_decomposition_test_equal(maximum_queue.empty(), true, "削除可能priority queue空判定");

    DeletablePriorityQueue<int, greater<int>> minimum_queue;
    minimum_queue.push(4);
    minimum_queue.push(2);
    minimum_queue.push(2);
    minimum_queue.erase(2);
    centroid_decomposition_test_equal(minimum_queue.top(), 2, "削除可能priority queue最小値");
}

void test_centroid_base_structures() {
    // 二重心では頂点番号が小さい方を選ぶ決定性を確認する
    CentroidDecompositionTree two_vertices(2);
    two_vertices.add_edge(0, 1);
    two_vertices.build();
    centroid_decomposition_test_equal(two_vertices.centroid_parent[0], -1, "二重心の決定性");
    centroid_decomposition_test_equal(two_vertices.centroid_parent[1], 0, "重心分解木の親");

    // 固定木で分解木の根数・親深さ・重心距離表を確認する
    const int n = 15;
    vector<tuple<int, int, long long>> edges;
    for (int v = 1; v < n; ++v) edges.push_back({(v - 1) / 2, v, v % 4 + 1});
    CentroidDecompositionTree tree(n);
    CentroidDecompositionPathIndex index(n);
    CentroidDecompositionTestTree naive(n);
    for (auto [u, v, weight] : edges) {
        tree.add_edge(u, v);
        index.add_edge(u, v, weight);
        naive.add_edge(u, v, weight, weight, 0, 0);
    }
    tree.build();
    index.build();
    int root_count = 0;
    for (int v = 0; v < n; ++v) {
        if (tree.centroid_parent[v] == -1) {
            ++root_count;
            centroid_decomposition_test_equal(tree.centroid_depth[v], 0, "重心分解木の根深さ");
        } else {
            centroid_decomposition_test_equal(tree.centroid_depth[v],
                                              tree.centroid_depth[tree.centroid_parent[v]] + 1,
                                              "重心分解木の深さ");
        }
        for (const auto& entry : index.path[v]) {
            auto [vertices, path_edges] = naive.path(v, entry.centroid);
            long long expected_distance = 0;
            for (const auto& edge : path_edges) expected_distance += edge.distance_weight;
            centroid_decomposition_test_equal(entry.distance, expected_distance, "重心祖先距離");
        }
    }
    centroid_decomposition_test_equal(root_count, 1, "重心分解木の根数");
}

void test_dynamic_structures() {
    mt19937 random(123456789);
    constexpr int test_cases = 120;
    constexpr int operations = 220;

    for (int test_case = 0; test_case < test_cases; ++test_case) {
        int n = centroid_decomposition_test_random_int(random, 1, 10);
        CentroidDecompositionTestTree naive(n);
        vector<tuple<int, int, long long>> weighted_edges;
        vector<tuple<int, int, int>> xor_edges;
        for (int v = 1; v < n; ++v) {
            int parent = centroid_decomposition_test_random_int(random, 0, v - 1);
            long long weight = centroid_decomposition_test_random_int(random, 0, 5);
            int xor_value = centroid_decomposition_test_random_int(random, 0, 7);
            int digit = centroid_decomposition_test_random_int(random, 0, 9);
            naive.add_edge(parent, v, weight, weight, xor_value, digit);
            weighted_edges.push_back({parent, v, weight});
            xor_edges.push_back({parent, v, xor_value});
        }
        vector<int> colors(n);
        for (int& color : colors) color = centroid_decomposition_test_random_int(random, 0, 3);

        // 全頂点対の距離・XOR・ボトルネックをnaiveに前計算する
        vector<vector<long long>> distance(n, vector<long long>(n));
        vector<vector<long long>> minimum_edge(n, vector<long long>(n));
        vector<vector<long long>> maximum_edge(n, vector<long long>(n));
        vector<vector<int>> path_xor(n, vector<int>(n));
        for (int u = 0; u < n; ++u) {
            for (int v = 0; v < n; ++v) {
                auto [vertices, edges] = naive.path(u, v);
                long long current_distance = 0;
                long long current_minimum = CentroidDecompositionMaximumBottleneckToActive::INF;
                long long current_maximum = 0;
                int current_xor = 0;
                for (const auto& edge : edges) {
                    current_distance += edge.distance_weight;
                    current_minimum = min(current_minimum, edge.distance_weight);
                    current_maximum = max(current_maximum, edge.distance_weight);
                    current_xor ^= edge.xor_value;
                }
                distance[u][v] = current_distance;
                minimum_edge[u][v] = current_minimum;
                maximum_edge[u][v] = current_maximum;
                path_xor[u][v] = current_xor;
            }
        }

        // 同一操作列を全動的構造体へ適用する
        CentroidDecompositionNearestActive nearest(n);
        CentroidDecompositionActiveDistanceCount distance_count(n);
        CentroidDecompositionNearestActiveByColor nearest_by_color(colors);
        CentroidDecompositionActiveDiameter diameter(n);
        CentroidDecompositionActiveClosestPair closest_pair(n);
        CentroidDecompositionActiveDifferentColorDiameter different_color(colors);
        CentroidDecompositionActivePathXorCount xor_count(n);
        CentroidDecompositionMaximumBottleneckToActive maximum_bottleneck(n);
        CentroidDecompositionMinimumMaximumEdgeToActive minimum_maximum_edge(n);
        for (int i = 0; i < static_cast<int>(weighted_edges.size()); ++i) {
            auto [u, v, weight] = weighted_edges[i];
            int xor_value = get<2>(xor_edges[i]);
            nearest.add_edge(u, v, weight);
            distance_count.add_edge(u, v, weight);
            nearest_by_color.add_edge(u, v, weight);
            diameter.add_edge(u, v, weight);
            closest_pair.add_edge(u, v, weight);
            different_color.add_edge(u, v, weight);
            xor_count.add_edge(u, v, xor_value);
            maximum_bottleneck.add_edge(u, v, weight);
            minimum_maximum_edge.add_edge(u, v, weight);
        }
        nearest.build();
        distance_count.build();
        nearest_by_color.build();
        diameter.build();
        closest_pair.build();
        different_color.build();
        xor_count.build();
        maximum_bottleneck.build();
        minimum_maximum_edge.build();
        vector<char> active(n, false);

        for (int operation = 0; operation < operations; ++operation) {
            // 約3分の1で状態を反転し、残りを含む毎回の状態で全種類を照合する
            int changed = centroid_decomposition_test_random_int(random, 0, n - 1);
            if (centroid_decomposition_test_random_int(random, 0, 2) == 0) {
                active[changed] ^= 1;
                nearest.toggle(changed);
                distance_count.toggle(changed);
                nearest_by_color.toggle(changed);
                diameter.toggle(changed);
                closest_pair.toggle(changed);
                different_color.toggle(changed);
                xor_count.toggle(changed);
                maximum_bottleneck.toggle(changed);
                minimum_maximum_edge.toggle(changed);
            }
            int source = centroid_decomposition_test_random_int(random, 0, n - 1);

            long long expected_nearest = numeric_limits<long long>::max();
            for (int v = 0; v < n; ++v) {
                if (active[v]) expected_nearest = min(expected_nearest, distance[source][v]);
            }
            centroid_decomposition_test_equal(
                nearest.nearest_distance(source),
                expected_nearest == numeric_limits<long long>::max() ? -1 : expected_nearest,
                "動的最近傍");

            long long limit = centroid_decomposition_test_random_int(random, -2, 17);
            long long expected_count = 0;
            for (int v = 0; v < n; ++v) {
                if (active[v] && distance[source][v] <= limit) ++expected_count;
            }
            centroid_decomposition_test_equal(distance_count.count_within(source, limit),
                                              expected_count, "動的距離上限個数");
            long long lower = centroid_decomposition_test_random_int(random, -2, 12);
            long long upper = lower + centroid_decomposition_test_random_int(random, 0, 7);
            expected_count = 0;
            for (int v = 0; v < n; ++v) {
                if (active[v] && lower <= distance[source][v] && distance[source][v] <= upper) {
                    ++expected_count;
                }
            }
            centroid_decomposition_test_equal(distance_count.count_in_range(source, lower, upper),
                                              expected_count, "動的距離区間個数");

            int target_color = centroid_decomposition_test_random_int(random, 0, 4);
            long long expected_color_distance = numeric_limits<long long>::max();
            for (int v = 0; v < n; ++v) {
                if (active[v] && colors[v] == target_color) {
                    expected_color_distance = min(expected_color_distance, distance[source][v]);
                }
            }
            centroid_decomposition_test_equal(
                nearest_by_color.nearest_distance(source, target_color),
                expected_color_distance == numeric_limits<long long>::max() ? -1 : expected_color_distance,
                "動的色別最近傍");

            vector<int> active_vertices;
            for (int v = 0; v < n; ++v) if (active[v]) active_vertices.push_back(v);
            long long expected_diameter = -1;
            long long expected_closest = -1;
            long long expected_different_color = -1;
            if (active_vertices.size() == 1) expected_diameter = 0;
            if (active_vertices.size() >= 2) {
                expected_diameter = 0;
                expected_closest = numeric_limits<long long>::max();
                for (int i = 0; i < static_cast<int>(active_vertices.size()); ++i) {
                    for (int j = i + 1; j < static_cast<int>(active_vertices.size()); ++j) {
                        int u = active_vertices[i];
                        int v = active_vertices[j];
                        expected_diameter = max(expected_diameter, distance[u][v]);
                        expected_closest = min(expected_closest, distance[u][v]);
                        if (colors[u] != colors[v]) {
                            expected_different_color = max(expected_different_color, distance[u][v]);
                        }
                    }
                }
            }
            centroid_decomposition_test_equal(diameter.diameter(), expected_diameter, "動的直径");
            centroid_decomposition_test_equal(closest_pair.minimum_pair_distance(),
                                              expected_closest, "動的最近接対");
            centroid_decomposition_test_equal(different_color.diameter_between_different_colors(),
                                              expected_different_color, "動的異色直径");

            int target_xor = centroid_decomposition_test_random_int(random, 0, 7);
            long long expected_xor_count = 0;
            for (int v = 0; v < n; ++v) {
                if (active[v] && path_xor[source][v] == target_xor) ++expected_xor_count;
            }
            centroid_decomposition_test_equal(xor_count.count_path_xor(source, target_xor),
                                              expected_xor_count, "動的パスXOR個数");

            long long expected_bottleneck = -1;
            long long expected_minimax = numeric_limits<long long>::max();
            for (int v = 0; v < n; ++v) {
                if (!active[v]) continue;
                expected_bottleneck = max(expected_bottleneck, minimum_edge[source][v]);
                expected_minimax = min(expected_minimax, maximum_edge[source][v]);
            }
            centroid_decomposition_test_equal(maximum_bottleneck.maximum_bottleneck(source),
                                              expected_bottleneck, "動的最大ボトルネック");
            centroid_decomposition_test_equal(
                minimum_maximum_edge.minimum_maximum_edge(source),
                expected_minimax == numeric_limits<long long>::max() ? -1 : expected_minimax,
                "動的最小最大辺");
        }
    }
}

void test_static_structures() {
    mt19937 random(987654321);
    constexpr int test_cases = 120;

    for (int test_case = 0; test_case < test_cases; ++test_case) {
        int n = centroid_decomposition_test_random_int(random, 1, 10);
        CentroidDecompositionTestTree naive(n);
        vector<pair<int, int>> edges;
        vector<tuple<int, int, long long>> sum_edges;
        vector<tuple<int, int, long long>> distance_edges;
        vector<tuple<int, int, int>> xor_edges;
        vector<tuple<int, int, int>> digit_edges;
        for (int v = 1; v < n; ++v) {
            int parent = centroid_decomposition_test_random_int(random, 0, v - 1);
            long long sum_weight = centroid_decomposition_test_random_int(random, -3, 3);
            long long distance_weight = centroid_decomposition_test_random_int(random, 0, 5);
            int xor_value = centroid_decomposition_test_random_int(random, 0, 7);
            int digit = centroid_decomposition_test_random_int(random, 0, 9);
            naive.add_edge(parent, v, sum_weight, distance_weight, xor_value, digit);
            edges.push_back({parent, v});
            sum_edges.push_back({parent, v, sum_weight});
            distance_edges.push_back({parent, v, distance_weight});
            xor_edges.push_back({parent, v, xor_value});
            digit_edges.push_back({parent, v, digit});
        }

        // 全頂点対のパス列と基本集約値を前計算する
        vector<vector<vector<int>>> path_vertices(n, vector<vector<int>>(n));
        vector<vector<vector<CentroidDecompositionTestPathEdge>>> path_edges(
            n, vector<vector<CentroidDecompositionTestPathEdge>>(n));
        vector<vector<long long>> path_sum(n, vector<long long>(n));
        vector<vector<long long>> path_distance(n, vector<long long>(n));
        vector<vector<int>> path_depth(n, vector<int>(n));
        for (int u = 0; u < n; ++u) {
            for (int v = 0; v < n; ++v) {
                auto path = naive.path(u, v);
                path_vertices[u][v] = path.first;
                path_edges[u][v] = path.second;
                for (const auto& edge : path.second) {
                    path_sum[u][v] += edge.sum_weight;
                    path_distance[u][v] += edge.distance_weight;
                }
                path_depth[u][v] = static_cast<int>(path.second.size());
            }
        }

        // パス和の個数・最小辺数・最大スコアを全頂点対と比較する
        CentroidDecompositionPathSumCount sum_count(n);
        CentroidDecompositionExactPathSumMinEdges minimum_edges(n);
        vector<long long> score(n);
        for (long long& value : score) value = centroid_decomposition_test_random_int(random, -5, 5);
        CentroidDecompositionExactPathSumMaxVertexScore maximum_score(score);
        for (auto [u, v, weight] : sum_edges) {
            sum_count.add_edge(u, v, weight);
            minimum_edges.add_edge(u, v, weight);
            maximum_score.add_edge(u, v, weight);
        }
        for (long long target = -10; target <= 10; ++target) {
            long long expected_exact = 0;
            long long expected_at_most = 0;
            int expected_minimum_edges = -1;
            int expected_maximum_edges = -1;
            long long expected_maximum_score = CentroidDecompositionExactPathSumMaxVertexScore::NO_SOLUTION;
            for (int u = 0; u < n; ++u) {
                for (int v = u + 1; v < n; ++v) {
                    if (path_sum[u][v] <= target) ++expected_at_most;
                    if (path_sum[u][v] != target) continue;
                    ++expected_exact;
                    if (expected_minimum_edges == -1) expected_minimum_edges = path_depth[u][v];
                    else expected_minimum_edges = min(expected_minimum_edges, path_depth[u][v]);
                    expected_maximum_edges = max(expected_maximum_edges, path_depth[u][v]);
                    long long current_score = 0;
                    for (int vertex : path_vertices[u][v]) current_score += score[vertex];
                    expected_maximum_score = max(expected_maximum_score, current_score);
                }
            }
            centroid_decomposition_test_equal(sum_count.count_exact(target), expected_exact,
                                              "静的パス和一致個数");
            centroid_decomposition_test_equal(sum_count.count_at_most(target), expected_at_most,
                                              "静的パス和上限個数");
            centroid_decomposition_test_equal(minimum_edges.minimum_edges(target),
                                              expected_minimum_edges, "静的パス和最小辺数");
            centroid_decomposition_test_equal(maximum_score.maximum_vertex_score(target),
                                              expected_maximum_score, "静的パス和最大スコア");
            centroid_decomposition_test_equal(solve_exact_path_sum_max_edges(n, sum_edges, target),
                                              expected_maximum_edges, "静的パス和最大辺数ソルバー");
        }
        for (int iteration = 0; iteration < 8; ++iteration) {
            long long lower = centroid_decomposition_test_random_int(random, -7, 7);
            long long upper = lower + centroid_decomposition_test_random_int(random, 0, 9);
            long long expected = 0;
            for (int u = 0; u < n; ++u) {
                for (int v = u + 1; v < n; ++v) {
                    if (lower <= path_sum[u][v] && path_sum[u][v] <= upper) ++expected;
                }
            }
            centroid_decomposition_test_equal(sum_count.count_in_range(lower, upper), expected,
                                              "静的パス和区間個数");
        }

        // 正の頂点積の最長パスを全上限で比較する
        vector<long long> product_value(n);
        for (long long& value : product_value) value = centroid_decomposition_test_random_int(random, 1, 4);
        CentroidDecompositionPathProductMaxEdges product(product_value);
        for (auto [u, v] : edges) product.add_edge(u, v);
        for (long long limit = 0; limit <= 40; ++limit) {
            int expected = -1;
            for (int u = 0; u < n; ++u) {
                for (int v = u; v < n; ++v) {
                    long long current_product = 1;
                    for (int vertex : path_vertices[u][v]) current_product *= product_value[vertex];
                    if (current_product <= limit) expected = max(expected, path_depth[u][v]);
                }
            }
            centroid_decomposition_test_equal(product.maximum_edges(limit), expected,
                                              "静的頂点積最長パス");
        }

        // マスク条件・回文条件・頂点別回文通過数を比較する
        constexpr int alphabet_size = 3;
        vector<int> labels(n);
        vector<int> masks(n);
        for (int v = 0; v < n; ++v) {
            labels[v] = centroid_decomposition_test_random_int(random, 0, alphabet_size - 1);
            masks[v] = 1 << labels[v];
        }
        CentroidDecompositionVertexMaskPathCount mask_count(masks, alphabet_size);
        CentroidDecompositionPalindromePathThroughVertexCount palindrome_through(labels, alphabet_size);
        for (auto [u, v] : edges) {
            mask_count.add_edge(u, v);
            palindrome_through.add_edge(u, v);
        }
        for (int iteration = 0; iteration < 8; ++iteration) {
            vector<int> allowed;
            vector<char> is_allowed(1 << alphabet_size, false);
            for (int mask = 0; mask < (1 << alphabet_size); ++mask) {
                if (centroid_decomposition_test_random_int(random, 0, 1) == 1) {
                    allowed.push_back(mask);
                    is_allowed[mask] = true;
                }
            }
            long long expected = 0;
            for (int u = 0; u < n; ++u) {
                for (int v = u; v < n; ++v) {
                    int path_mask = 0;
                    for (int vertex : path_vertices[u][v]) path_mask ^= masks[vertex];
                    if (is_allowed[path_mask]) ++expected;
                }
            }
            centroid_decomposition_test_equal(mask_count.count_allowed_xors(allowed), expected,
                                              "静的頂点マスク条件");
        }
        long long expected_palindrome = 0;
        vector<long long> expected_through(n, 0);
        for (int u = 0; u < n; ++u) {
            for (int v = u; v < n; ++v) {
                int path_mask = 0;
                for (int vertex : path_vertices[u][v]) path_mask ^= masks[vertex];
                if (popcount(static_cast<unsigned>(path_mask)) <= 1) {
                    ++expected_palindrome;
                    for (int vertex : path_vertices[u][v]) ++expected_through[vertex];
                }
            }
        }
        centroid_decomposition_test_equal(
            solve_palindrome_permutable_path_count(labels, alphabet_size, edges),
            expected_palindrome, "静的回文可能パス総数");
        centroid_decomposition_test_equal(palindrome_through.count_through_each_vertex(),
                                          expected_through, "静的頂点別回文可能パス数");
        for (int odd_count = 0; odd_count <= alphabet_size; ++odd_count) {
            long long expected = 0;
            for (int u = 0; u < n; ++u) {
                for (int v = u; v < n; ++v) {
                    int path_mask = 0;
                    for (int vertex : path_vertices[u][v]) path_mask ^= masks[vertex];
                    if (popcount(static_cast<unsigned>(path_mask)) == odd_count) ++expected;
                }
            }
            centroid_decomposition_test_equal(
                solve_paths_with_exactly_k_odd_labels(labels, alphabet_size, edges, odd_count),
                expected, "静的奇数ラベル種類数");
        }

        // 数字列modを全ての有向頂点対と比較する
        constexpr int modulus = 7;
        CentroidDecompositionDigitModuloCount digit_modulo(n, modulus);
        for (auto [u, v, digit] : digit_edges) digit_modulo.add_edge(u, v, digit);
        long long expected_digit_pairs = 0;
        for (int u = 0; u < n; ++u) {
            for (int v = 0; v < n; ++v) {
                if (u == v) continue;
                int value = 0;
                for (const auto& edge : path_edges[u][v]) value = (value * 10 + edge.digit) % modulus;
                if (value == 0) ++expected_digit_pairs;
            }
        }
        centroid_decomposition_test_equal(digit_modulo.count_divisible_ordered_pairs(),
                                          expected_digit_pairs, "静的数字列mod");

        // GCD・AND・ORの全頻度を比較する
        vector<int> gcd_value(n);
        for (int& value : gcd_value) value = centroid_decomposition_test_random_int(random, 0, 12);
        CentroidDecompositionPathGcdFrequency gcd_frequency(gcd_value);
        for (auto [u, v] : edges) gcd_frequency.add_edge(u, v);
        vector<long long> expected_gcd(*max_element(gcd_value.begin(), gcd_value.end()) + 1, 0);
        for (int u = 0; u < n; ++u) {
            for (int v = u; v < n; ++v) {
                int aggregate = 0;
                for (int vertex : path_vertices[u][v]) aggregate = std::gcd(aggregate, gcd_value[vertex]);
                ++expected_gcd[aggregate];
            }
        }
        centroid_decomposition_test_equal(gcd_frequency.frequency(), expected_gcd, "静的パスGCD頻度");

        vector<int> bit_value(n);
        for (int& value : bit_value) value = centroid_decomposition_test_random_int(random, 0, 7);
        CentroidDecompositionPathAndFrequencySmallState and_frequency(bit_value, 3);
        CentroidDecompositionPathOrFrequencySmallState or_frequency(bit_value, 3);
        for (auto [u, v] : edges) {
            and_frequency.add_edge(u, v);
            or_frequency.add_edge(u, v);
        }
        vector<long long> expected_and(8, 0);
        vector<long long> expected_or(8, 0);
        for (int u = 0; u < n; ++u) {
            for (int v = u; v < n; ++v) {
                int aggregate_and = 7;
                int aggregate_or = 0;
                for (int vertex : path_vertices[u][v]) {
                    aggregate_and &= bit_value[vertex];
                    aggregate_or |= bit_value[vertex];
                }
                ++expected_and[aggregate_and];
                ++expected_or[aggregate_or];
            }
        }
        centroid_decomposition_test_equal(and_frequency.frequency(), expected_and, "静的パスAND頻度");
        centroid_decomposition_test_equal(or_frequency.frequency(), expected_or, "静的パスOR頻度");

        // 同ラベル・小クラス行列の個数と距離総和を比較する
        vector<int> same_label(n);
        for (int& value : same_label) value = centroid_decomposition_test_random_int(random, 0, 3);
        CentroidDecompositionSameLabelDistance same_label_distance(same_label);
        for (auto [u, v, weight] : distance_edges) same_label_distance.add_edge(u, v, weight);
        long long expected_same_count = 0;
        long long expected_same_distance = 0;
        for (int u = 0; u < n; ++u) {
            for (int v = u + 1; v < n; ++v) {
                if (same_label[u] == same_label[v]) {
                    ++expected_same_count;
                    expected_same_distance += path_distance[u][v];
                }
            }
        }
        auto same_result = same_label_distance.solve();
        centroid_decomposition_test_equal(same_result.pair_count, expected_same_count,
                                          "静的同ラベル対数");
        centroid_decomposition_test_equal(same_result.distance_sum, expected_same_distance,
                                          "静的同ラベル距離和");

        constexpr int class_count = 3;
        vector<int> classes(n);
        for (int& value : classes) value = centroid_decomposition_test_random_int(random, 0, class_count - 1);
        CentroidDecompositionSmallClassDistance class_distance(classes, class_count);
        for (auto [u, v, weight] : distance_edges) class_distance.add_edge(u, v, weight);
        vector<vector<long long>> expected_class_count(class_count,
                                                        vector<long long>(class_count, 0));
        vector<vector<long long>> expected_class_distance(class_count,
                                                           vector<long long>(class_count, 0));
        for (int u = 0; u < n; ++u) {
            for (int v = u + 1; v < n; ++v) {
                int a = classes[u];
                int b = classes[v];
                ++expected_class_count[a][b];
                expected_class_distance[a][b] += path_distance[u][v];
                if (a != b) {
                    ++expected_class_count[b][a];
                    expected_class_distance[b][a] += path_distance[u][v];
                }
            }
        }
        auto class_result = class_distance.solve();
        centroid_decomposition_test_equal(class_result.pair_count, expected_class_count,
                                          "静的小クラス対数行列");
        centroid_decomposition_test_equal(class_result.distance_sum, expected_class_distance,
                                          "静的小クラス距離行列");

        // 非重み付き距離頻度と非一直線三つ組を比較する
        CentroidDecompositionDistanceFrequency distance_frequency(n);
        for (auto [u, v] : edges) distance_frequency.add_edge(u, v);
        vector<long long> expected_frequency(n, 0);
        for (int u = 0; u < n; ++u) {
            for (int v = u + 1; v < n; ++v) ++expected_frequency[path_depth[u][v]];
        }
        centroid_decomposition_test_equal(distance_frequency.frequency(), expected_frequency,
                                          "静的木距離頻度");
        long long expected_non_collinear = 0;
        for (int a = 0; a < n; ++a) {
            for (int b = a + 1; b < n; ++b) {
                for (int c = b + 1; c < n; ++c) {
                    bool collinear = find(path_vertices[a][b].begin(), path_vertices[a][b].end(), c)
                                         != path_vertices[a][b].end();
                    collinear |= find(path_vertices[a][c].begin(), path_vertices[a][c].end(), b)
                                  != path_vertices[a][c].end();
                    collinear |= find(path_vertices[b][c].begin(), path_vertices[b][c].end(), a)
                                  != path_vertices[b][c].end();
                    if (!collinear) ++expected_non_collinear;
                }
            }
        }
        centroid_decomposition_test_equal(
            solve_avoid_straight_line_using_distance_frequency(n, edges), expected_non_collinear,
            "距離頻度派生の非一直線三つ組");

        // 全頂点有効化を使う静的XORソルバーを比較する
        for (int target = 0; target < 8; ++target) {
            long long expected = 0;
            for (int u = 0; u < n; ++u) {
                for (int v = u + 1; v < n; ++v) {
                    int aggregate = 0;
                    for (const auto& edge : path_edges[u][v]) aggregate ^= edge.xor_value;
                    if (aggregate == target) ++expected;
                }
            }
            centroid_decomposition_test_equal(
                solve_static_path_xor_exact_count(n, xor_edges, target), expected,
                "全有効化による静的パスXOR");
        }
    }
}

void test_acl_convolution_and_recursive_path() {
    // ACL畳み込みを通し、距離dの対数n-dと比較する
    {
        constexpr int n = 200;
        CentroidDecompositionDistanceFrequency frequency(n);
        for (int v = 1; v < n; ++v) frequency.add_edge(v - 1, v);
        vector<long long> expected(n, 0);
        for (int distance = 1; distance < n; ++distance) expected[distance] = n - distance;
        centroid_decomposition_test_equal(frequency.frequency(), expected, "ACL畳み込み距離頻度");
    }

    // 再帰版で扱える範囲のパス木について、重心深さが対数範囲に収まることを確認する
    {
        constexpr int n = 200000;
        CentroidDecompositionTree decomposition(n);
        for (int v = 1; v < n; ++v) decomposition.add_edge(v - 1, v);
        decomposition.build();
        int root = -1;
        int maximum_depth = 0;
        for (int v = 0; v < n; ++v) {
            if (decomposition.centroid_parent[v] == -1) root = v;
            maximum_depth = max(maximum_depth, decomposition.centroid_depth[v]);
        }
        centroid_decomposition_test_equal(root, n / 2 - 1, "再帰パスの重心");
        if (maximum_depth > 18) {
            cerr << "テスト失敗: 再帰パスの重心深さ\n";
            abort();
        }
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    auto start = chrono::steady_clock::now();
    test_common_data_structures();
    cout << "共通データ構造: OK\n";
    test_centroid_base_structures();
    cout << "基礎重心分解: OK\n";
    test_dynamic_structures();
    cout << "動的ユースケース: OK\n";
    test_static_structures();
    cout << "静的ユースケース: OK\n";
    test_acl_convolution_and_recursive_path();
    cout << "ACL畳み込み・再帰パス: OK\n";
    auto elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start);
    cout << "全テスト成功: " << elapsed.count() << " ms\n";
}

#endif
