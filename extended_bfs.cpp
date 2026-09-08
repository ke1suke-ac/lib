#include <iostream>
#include <vector>
#include <deque>
#include <queue>
#include <limits>
#include <chrono>
#include <random>
#include <tuple>
#include <algorithm>
using namespace std;

const int INF = numeric_limits<int>::max();

//--------------------------------------------
// 隣接リスト形式のグラフ生成（必ず 0→…→n-1 の連結パスを含む）
// 各辺は {到達先, 重み} を表す。
vector<vector<pair<int,int>>> generate_graph(int n, int m, int max_weight, mt19937 &gen) {
    vector<vector<pair<int,int>>> graph(n);
    uniform_int_distribution<int> weight_dist(1, max_weight);
    // 連結性を保証するため、0→1→…→n-1 のパスを追加
    if(n > 1) {
        for (int i = 0; i < n - 1; i++) {
            int w = weight_dist(gen);
            graph[i].push_back({i + 1, w});
        }
    }
    int current_edges = (n > 1 ? n - 1 : 0);
    uniform_int_distribution<int> vertex_dist(0, n - 1);
    while(current_edges < m) {
        int u = vertex_dist(gen);
        int v = vertex_dist(gen);
        if(u == v)
            continue;
        int w = weight_dist(gen);
        graph[u].push_back({v, w});
        current_edges++;
    }
    return graph;
}

//--------------------------------------------
// extended_bfs : リングバッファ方式（Dial’s法風）の実装
// dist と parent の両方を返す。
// 引数 max_weight は辺重みの上限（例: 10）。
// goal が -1 でない場合、goal が buckets から取り出された時点で早期 return する。
// 経路復元が不要なら、parent周りの処理削除で微高速化
pair<vector<int>, vector<int>> extended_bfs(const vector<vector<pair<int,int>>> &graph, int start, int max_weight, int goal = -1) {
    int n = static_cast<int>(graph.size());
    vector<int> dist(n, INF), parent(n, -1);
    dist[start] = 0;
    vector<deque<int>> buckets(max_weight + 1);
    buckets[0].push_back(start);
    int remaining = 1;  // 現在、バケット内に入っているノード数
    int cur = 0;
    
    while (remaining > 0) {
        int bucket_index = cur % (max_weight + 1);
        while (buckets[bucket_index].empty()) {
            cur++;
            bucket_index = cur % (max_weight + 1);
        }
        int u = buckets[bucket_index].front();
        buckets[bucket_index].pop_front();
        remaining--;
        if (dist[u] > cur)
            cur = dist[u];
        if (goal != -1 && u == goal)
            return {dist, parent};
        for (const auto &edge : graph[u]) {
            int v = edge.first, w = edge.second;
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                parent[v] = u;
                int new_bucket = dist[v] % (max_weight + 1);
                buckets[new_bucket].push_back(v);
                remaining++;
            }
        }
    }
    return {dist, parent};
}

//--------------------------------------------
// dijkstra_heap : 優先度付きキューを用いた標準的ダイクストラ法（隣接リスト形式）
// goal が指定されている場合、goal が取り出された時点で早期 return する。
vector<int> dijkstra_heap(const vector<vector<pair<int,int>>> &graph, int start, int goal = -1) {
    int n = static_cast<int>(graph.size());
    vector<int> dist(n, INF);
    dist[start] = 0;
    // {距離, 頂点}
    priority_queue<pair<int,int>, vector<pair<int,int>>, greater<pair<int,int>>> pq;
    pq.push({0, start});
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d != dist[u])
            continue;
        if (goal != -1 && u == goal)
            return dist;
        for (const auto &edge : graph[u]) {
            int v = edge.first, w = edge.second;
            if (d + w < dist[v]) {
                dist[v] = d + w;
                pq.push({dist[v], v});
            }
        }
    }
    return dist;
}

//--------------------------------------------
// 隣接行列への変換
// 無い辺は INF、自己ループは 0 とする。
vector<vector<int>> convert_to_matrix(const vector<vector<pair<int,int>>> &graph) {
    int n = static_cast<int>(graph.size());
    vector<vector<int>> mat(n, vector<int>(n, INF));
    for (int i = 0; i < n; i++) {
        mat[i][i] = 0;
        for (const auto &edge : graph[i]) {
            int v = edge.first, w = edge.second;
            if (w < mat[i][v])
                mat[i][v] = w;
        }
    }
    return mat;
}

//--------------------------------------------
// dijkstra_naive : 隣接行列形式のグラフに対して、O(V^2) の標準ダイクストラ法
// INF との加算を防ぐため、mat[u][v] != INF の場合のみ更新。
// goal が指定されている場合、goal が選ばれたら早期終了する。
vector<int> dijkstra_naive(const vector<vector<int>> &mat, int start, int goal = -1) {
    int n = static_cast<int>(mat.size());
    vector<int> dist(n, INF);
    vector<bool> used(n, false);
    dist[start] = 0;
    for (int i = 0; i < n; i++) {
        int u = -1;
        for (int j = 0; j < n; j++) {
            if (!used[j] && (u == -1 || dist[j] < dist[u]))
                u = j;
        }
        if (u == -1)
            break;
        used[u] = true;
        if (goal != -1 && u == goal)
            break;
        for (int v = 0; v < n; v++) {
            if (mat[u][v] != INF && dist[u] + mat[u][v] < dist[v])
                dist[v] = dist[u] + mat[u][v];
        }
    }
    return dist;
}

//--------------------------------------------
// reconstruct_path : 経路復元用関数
// parent 配列から、start から goal までの経路（頂点列）を復元して返す。
// 経路が見つからなければ空の vector を返す。
vector<int> reconstruct_path(const vector<int>& parent, int start, int goal) {
    vector<int> path;
    if (goal < 0 || goal >= static_cast<int>(parent.size()) || (goal != start && parent[goal] == -1))
        return path;
    for (int cur = goal; cur != -1; cur = parent[cur])
        path.push_back(cur);
    reverse(path.begin(), path.end());
    if (path.front() == start)
        return path;
    return vector<int>(); // 到達不可の場合
}

//--------------------------------------------
// ベンチマークテスト用メイン
// 各テストケースは {頂点数 n, 辺数 m, iterations}。
// 各アルゴリズムで 0 から n-1 までの最短経路を求め、
// 経路復元結果も extended_bfs で確認する。
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    vector<tuple<int, int, int>> test_cases = {
        {10, 20, 10000},
        {10, 100, 10000},
        {100, 200, 1000},
        {100, 10000, 100},
        {1000, 2000, 10},
        {1000, 1000000, 1},
        {2000, 4000, 10},
        {4000, 8000, 10},
        {8000, 16000, 5},
        {16000, 32000, 5},
    };
    int max_weight = 10;
    random_device rd;
    mt19937 gen(rd());

    cout << "Benchmarking algorithms...\n";
    for (auto [n, m, iters] : test_cases) {
        cout << "\nGraph: n = " << n << ", m = " << m << ", iterations = " << iters << "\n";
        // 隣接リスト形式のグラフ生成（0 から n-1 まで到達可能）
        auto graph = generate_graph(n, m, max_weight, gen);
        int start = 0, goal = n - 1;
        // 基準結果は dijkstra_heap（早期 return あり）によるもの
        vector<int> ref = dijkstra_heap(graph, start, goal);
        int ref_distance = ref[goal];

        // dijkstra_naive 用に隣接行列形式に変換（変換時間は測定対象外）
        vector<vector<int>> mat = convert_to_matrix(graph);

        double time_extended = 0, time_naive = 0, time_heap = 0;
        pair<vector<int>, vector<int>> ext_result;
        vector<int> res;
        
        // extended_bfs の計測（goal 指定あり）
        {
            auto t0 = chrono::high_resolution_clock::now();
            for (int i = 0; i < iters; i++) {
                ext_result = extended_bfs(graph, start, max_weight, goal);
            }
            auto t1 = chrono::high_resolution_clock::now();
            time_extended = chrono::duration<double, micro>(t1 - t0).count() / iters;
            if (ext_result.first[goal] != ref_distance)
                cout << "Mismatch in extended_bfs: expected " << ref_distance << ", got " << ext_result.first[goal] << "\n";
        }
        
        // dijkstra_naive の計測（隣接行列版、goal 指定あり）
        {
            auto t0 = chrono::high_resolution_clock::now();
            for (int i = 0; i < iters; i++) {
                res = dijkstra_naive(mat, start, goal);
            }
            auto t1 = chrono::high_resolution_clock::now();
            time_naive = chrono::duration<double, micro>(t1 - t0).count() / iters;
            if (res[goal] != ref_distance)
                cout << "Mismatch in dijkstra_naive: expected " << ref_distance << ", got " << res[goal] << "\n";
        }
        
        // dijkstra_heap の計測（goal 指定あり）
        {
            auto t0 = chrono::high_resolution_clock::now();
            for (int i = 0; i < iters; i++) {
                res = dijkstra_heap(graph, start, goal);
            }
            auto t1 = chrono::high_resolution_clock::now();
            time_heap = chrono::duration<double, micro>(t1 - t0).count() / iters;
            if (res[goal] != ref_distance)
                cout << "Mismatch in dijkstra_heap: expected " << ref_distance << ", got " << res[goal] << "\n";
        }
        
        cout << "Shortest path from " << start << " to " << goal << ": " << ref_distance << "\n";
        cout << "Reconstructed path (extended_bfs): ";
        vector<int> path = reconstruct_path(ext_result.second, start, goal);
        for (auto v : path)
            cout << v << " ";
        cout << "\n";
        cout << "Average time per iteration (microseconds):\n";
        cout << "  extended_bfs: " << time_extended << "\n";
        cout << "  dijkstra_naive (matrix): " << time_naive << "\n";
        cout << "  dijkstra_heap: " << time_heap << "\n";
        cout << "--------------------------------------\n";
    }
    
    return 0;
}

// 実行結果
// Benchmarking algorithms...

// Graph: n = 10, m = 20, iterations = 10000
// Shortest path from 0 to 9: 19
// Reconstructed path (extended_bfs): 0 7 8 9 
// Average time per iteration (microseconds):
//   extended_bfs: 0.582816
//   dijkstra_naive (matrix): 0.190944
//   dijkstra_heap: 0.12482
// --------------------------------------

// Graph: n = 10, m = 100, iterations = 10000
// Shortest path from 0 to 9: 3
// Reconstructed path (extended_bfs): 0 9 
// Average time per iteration (microseconds):
//   extended_bfs: 0.63824
//   dijkstra_naive (matrix): 0.127232
//   dijkstra_heap: 0.184262
// --------------------------------------

// Graph: n = 100, m = 200, iterations = 1000
// Shortest path from 0 to 99: 34
// Reconstructed path (extended_bfs): 0 80 95 96 97 98 99 
// Average time per iteration (microseconds):
//   extended_bfs: 1.80852
//   dijkstra_naive (matrix): 16.4968
//   dijkstra_heap: 1.33518
// --------------------------------------

// Graph: n = 100, m = 10000, iterations = 100
// Shortest path from 0 to 99: 2
// Reconstructed path (extended_bfs): 0 44 99 
// Average time per iteration (microseconds):
//   extended_bfs: 5.24107
//   dijkstra_naive (matrix): 21.8802
//   dijkstra_heap: 8.40926
// --------------------------------------

// Graph: n = 1000, m = 2000, iterations = 10
// Shortest path from 0 to 999: 62
// Reconstructed path (extended_bfs): 0 1 241 242 918 919 606 372 373 374 997 998 999 
// Average time per iteration (microseconds):
//   extended_bfs: 21.9605
//   dijkstra_naive (matrix): 2195.16
//   dijkstra_heap: 69.0811
// --------------------------------------

// Graph: n = 1000, m = 1000000, iterations = 1
// Shortest path from 0 to 999: 1
// Reconstructed path (extended_bfs): 0 999 
// Average time per iteration (microseconds):
//   extended_bfs: 62.131
//   dijkstra_naive (matrix): 617.432
//   dijkstra_heap: 188.728
// --------------------------------------

// Graph: n = 2000, m = 4000, iterations = 10
// Shortest path from 0 to 1999: 58
// Reconstructed path (extended_bfs): 0 1 2 617 1354 1355 1356 1468 1469 1470 1471 1472 1998 1999 
// Average time per iteration (microseconds):
//   extended_bfs: 29.7374
//   dijkstra_naive (matrix): 6088.19
//   dijkstra_heap: 94.4055
// --------------------------------------

// Graph: n = 4000, m = 8000, iterations = 10
// Shortest path from 0 to 3999: 54
// Reconstructed path (extended_bfs): 0 1 2 3 4 907 908 2516 3997 3998 3999 
// Average time per iteration (microseconds):
//   extended_bfs: 23.4365
//   dijkstra_naive (matrix): 10301.2
//   dijkstra_heap: 72.2123
// --------------------------------------

// Graph: n = 8000, m = 16000, iterations = 5
// Shortest path from 0 to 7999: 62
// Reconstructed path (extended_bfs): 0 1 615 616 6464 6465 3833 3834 6964 6965 6966 6967 5761 4383 4384 7999 
// Average time per iteration (microseconds):
//   extended_bfs: 86.7008
//   dijkstra_naive (matrix): 54639.5
//   dijkstra_heap: 263.358
// --------------------------------------

// Graph: n = 16000, m = 32000, iterations = 5
// Shortest path from 0 to 15999: 69
// Reconstructed path (extended_bfs): 0 1 8755 14429 14430 14431 11879 11880 2649 11005 5732 4259 13466 13467 13468 13469 15998 15999 
// Average time per iteration (microseconds):
//   extended_bfs: 396.01
//   dijkstra_naive (matrix): 369234
//   dijkstra_heap: 997.991
// --------------------------------------
