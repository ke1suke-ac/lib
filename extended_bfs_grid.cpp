#include <iostream>
#include <vector>
#include <deque>
#include <queue>
#include <limits>
#include <chrono>
#include <random>
#include <algorithm>
#include <string>
#include <utility>
using namespace std;

const int INF = numeric_limits<int>::max();

//--------------------------------------------
// extended_bfs_grid : リングバッファ方式（Dial’s法風）の実装
// grid は vector<string> など、各セルのコストは weights[ cell ] で与えられる。
// 移動は上下左右の4方向。
// 最短距離配列と parent 配列を返し、goal に到達したら途中で return する。
// 経路復元が不要なら、parent周りの処理削除で微高速化
// idから座標への復元 auto y = id / W, x = id % W;
template<typename Grid>
pair<vector<int>, vector<int>> extended_bfs_grid(const Grid &grid, pair<int,int> start, pair<int,int> goal, int max_weight, const int weights[]) {
    int R = static_cast<int>(grid.size()), C = static_cast<int>(grid[0].size());
    int V = R * C;
    vector<int> dist(V, INF), parent(V, -1);
    auto idx = [C](int r, int c) { return r * C + c; };
    int start_index = idx(start.first, start.second);
    int goal_index = idx(goal.first, goal.second);
    dist[start_index] = 0;
    
    // buckets をリングバッファ的に利用
    vector<deque<int>> buckets(max_weight + 1);
    buckets[0].push_back(start_index);
    int remaining = 1; // 現在バケット内にあるノード数
    int cur = 0;
    
    int dr[4] = {1, -1, 0, 0};
    int dc[4] = {0, 0, 1, -1};
    
    while (remaining > 0) {
        int bucket_index = cur % (max_weight + 1);
        // 現在のバケットが空なら、cur をインクリメントして次のバケットへ
        while (buckets[bucket_index].empty()) {
            cur++;
            bucket_index = cur % (max_weight + 1);
        }
        int u = buckets[bucket_index].front();
        buckets[bucket_index].pop_front();
        remaining--;
        if (dist[u] > cur)
            cur = dist[u];
        // goal 到達なら早期 return
        if (u == goal_index)
            return {dist, parent};
        int r = u / C, c = u % C;
        for (int i = 0; i < 4; i++) {
            int nr = r + dr[i], nc = c + dc[i];
            if (nr < 0 || nr >= R || nc < 0 || nc >= C)
                continue;
            int v = idx(nr, nc);
            int new_cost = dist[u] + weights[static_cast<unsigned char>(grid[nr][nc])];
            if (new_cost < dist[v]) {
                dist[v] = new_cost;
                parent[v] = u;
                int new_bucket = new_cost % (max_weight + 1);
                buckets[new_bucket].push_back(v);
                remaining++;
            }
        }
    }
    return {dist, parent};
}

//--------------------------------------------
// dijkstra_heap_grid : 優先度付きキューを用いた標準的なダイクストラ法（グリッド版）
// grid は vector<string> など、各セルのコストは weights[ cell ] で与えられる。
// goal に到達したら早期 return し、最短距離配列と parent 配列を返す。
template<typename Grid>
pair<vector<int>, vector<int>> dijkstra_heap_grid(const Grid &grid, pair<int,int> start, pair<int,int> goal, const int weights[]) {
    int R = static_cast<int>(grid.size()), C = static_cast<int>(grid[0].size());
    int V = R * C;
    vector<int> dist(V, INF), parent(V, -1);
    auto idx = [C](int r, int c) { return r * C + c; };
    int start_index = idx(start.first, start.second);
    int goal_index = idx(goal.first, goal.second);
    dist[start_index] = 0;
    
    // {distance, vertex}
    priority_queue<pair<int,int>, vector<pair<int,int>>, greater<pair<int,int>>> pq;
    pq.push({0, start_index});
    int dr[4] = {1, -1, 0, 0};
    int dc[4] = {0, 0, 1, -1};
    
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d != dist[u])
            continue;
        // goal 到達なら早期 return
        if (u == goal_index)
            return {dist, parent};
        int r = u / C, c = u % C;
        for (int i = 0; i < 4; i++) {
            int nr = r + dr[i], nc = c + dc[i];
            if (nr < 0 || nr >= R || nc < 0 || nc >= C)
                continue;
            int v = idx(nr, nc);
            int new_cost = d + weights[static_cast<unsigned char>(grid[nr][nc])];
            if (new_cost < dist[v]) {
                dist[v] = new_cost;
                parent[v] = u;
                pq.push({new_cost, v});
            }
        }
    }
    return {dist, parent};
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
// generate_random_grid : (0,0) から (n-1,n-1) まで到達可能な単純パスを保証した上で、
// ' ' と '#' のセルからなるグリッドをランダム生成する。
// なお、ここでは ' ' への移動はコスト 1、'#' への移動は max_weight とする。
vector<string> generate_random_grid(int n, double spaceProbability, char openChar, char blockChar, mt19937 &gen) {
    vector<string> grid(n, string(n, blockChar));
    int r = 0, c = 0;
    grid[r][c] = openChar;
    while (r < n - 1 || c < n - 1) {
        if (r == n - 1) {
            c++;
        } else if (c == n - 1) {
            r++;
        } else {
            uniform_int_distribution<int> coin(0, 1);
            if (coin(gen) == 0)
                c++;
            else
                r++;
        }
        grid[r][c] = openChar;
    }
    uniform_real_distribution<double> prob(0.0, 1.0);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (grid[i][j] != openChar) {
                grid[i][j] = (prob(gen) < spaceProbability ? openChar : blockChar);
            }
        }
    }
    return grid;
}

//--------------------------------------------
// ベンチマークテスト用メイン
// テストケース：各グリッドサイズと反復回数を指定して、
// extended_bfs_grid と dijkstra_heap_grid の性能と結果を比較する。
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    // '#' への移動コストを max_weight (例: 10) とする
    int max_weight = 10;
    int weights[128] = {0};
    weights[' '] = 1;
    weights['#'] = max_weight;
    
    // テストケース：{グリッドサイズ, 反復回数}
    vector<pair<int, int>> test_cases = {
        {10, 1000},
        {20, 500},
        {35, 300},
        {50, 100},
        {100, 10},
        {200, 5},
    };
    
    random_device rd;
    mt19937 gen(rd());
    double spaceProbability = 0.7;
    char openChar = ' ';
    char blockChar = '#';
    
    cout << "Benchmarking grid-based algorithms...\n";
    for (auto [n, iters] : test_cases) {
        cout << "\nGrid: " << n << "x" << n << ", iterations = " << iters << "\n";
        auto grid = generate_random_grid(n, spaceProbability, openChar, blockChar, gen);
        pair<int,int> start = {0, 0};
        pair<int,int> goal = {n - 1, n - 1};
        
        // 基準結果は dijkstra_heap_grid によるもの
        auto ref_result = dijkstra_heap_grid(grid, start, goal, weights);
        int ref_distance = ref_result.first[(n * n) - 1]; // goal の距離
        
        double time_extended = 0, time_heap = 0;
        pair<vector<int>, vector<int>> ext_result;
        pair<vector<int>, vector<int>> heap_result;
        
        // extended_bfs_grid の計測
        {
            auto t0 = chrono::high_resolution_clock::now();
            for (int i = 0; i < iters; i++) {
                ext_result = extended_bfs_grid(grid, start, goal, max_weight, weights);
            }
            auto t1 = chrono::high_resolution_clock::now();
            time_extended = chrono::duration<double, micro>(t1 - t0).count() / iters;
            if (ext_result.first[(n * n) - 1] != ref_distance)
                cout << "Mismatch in extended_bfs_grid: expected " << ref_distance << ", got " << ext_result.first[(n * n) - 1] << "\n";
        }
        
        // dijkstra_heap_grid の計測
        {
            auto t0 = chrono::high_resolution_clock::now();
            for (int i = 0; i < iters; i++) {
                heap_result = dijkstra_heap_grid(grid, start, goal, weights);
            }
            auto t1 = chrono::high_resolution_clock::now();
            time_heap = chrono::duration<double, micro>(t1 - t0).count() / iters;
            if (heap_result.first[(n * n) - 1] != ref_distance)
                cout << "Mismatch in dijkstra_heap_grid: expected " << ref_distance << ", got " << heap_result.first[(n * n) - 1] << "\n";
        }
        
        cout << "Shortest path from (0,0) to (" << n - 1 << "," << n - 1 << "): " << ref_distance << "\n";
        vector<int> path = reconstruct_path(ext_result.second, 0, (n * n) - 1);
        cout << "Reconstructed path (extended_bfs_grid): ";
        for (auto v : path)
            cout << v << " ";
        cout << "\n";
        cout << "Average time per iteration (microseconds):\n";
        cout << "  extended_bfs_grid : " << time_extended << "\n";
        cout << "  dijkstra_heap_grid: " << time_heap << "\n";
        cout << "--------------------------------------\n";
    }
    
    return 0;
}

// 実行結果
// Benchmarking grid-based algorithms...

// Grid: 10x10, iterations = 1000
// Shortest path from (0,0) to (9,9): 18
// Reconstructed path (extended_bfs_grid): 0 10 11 12 22 32 42 52 62 72 73 74 84 85 86 87 88 89 99 
// Average time per iteration (microseconds):
//   extended_bfs_grid : 2.95646
//   dijkstra_heap_grid: 2.2237
// --------------------------------------

// Grid: 20x20, iterations = 500
// Shortest path from (0,0) to (19,19): 38
// Reconstructed path (extended_bfs_grid): 0 20 21 22 23 43 63 83 84 104 124 144 145 165 166 186 206 207 208 228 229 230 231 232 233 253 273 274 275 276 296 316 336 337 338 358 359 379 399 
// Average time per iteration (microseconds):
//   extended_bfs_grid : 10.6625
//   dijkstra_heap_grid: 10.5196
// --------------------------------------

// Grid: 35x35, iterations = 300
// Shortest path from (0,0) to (34,34): 68
// Reconstructed path (extended_bfs_grid): 0 1 2 37 72 107 142 143 144 179 180 215 250 251 286 287 322 357 392 427 428 429 430 431 466 467 468 503 504 505 540 575 576 577 612 613 614 649 650 651 652 687 722 757 792 827 862 897 932 933 934 935 936 971 1006 1007 1008 1009 1044 1079 1114 1149 1150 1185 1220 1221 1222 1223 1224 
// Average time per iteration (microseconds):
//   extended_bfs_grid : 33.786
//   dijkstra_heap_grid: 79.5905
// --------------------------------------

// Grid: 50x50, iterations = 100
// Shortest path from (0,0) to (49,49): 98
// Reconstructed path (extended_bfs_grid): 0 50 100 150 200 250 300 350 400 450 451 501 551 552 602 652 653 703 753 803 853 903 953 954 955 956 1006 1056 1106 1107 1108 1158 1208 1258 1308 1309 1310 1311 1361 1411 1412 1462 1463 1513 1514 1515 1516 1517 1518 1568 1618 1619 1620 1621 1671 1672 1673 1674 1675 1676 1726 1776 1777 1778 1828 1878 1928 1978 1979 1980 2030 2031 2081 2082 2083 2133 2134 2135 2136 2137 2138 2139 2140 2141 2142 2143 2144 2145 2146 2147 2148 2149 2199 2249 2299 2349 2399 2449 2499 
// Average time per iteration (microseconds):
//   extended_bfs_grid : 65.0436
//   dijkstra_heap_grid: 210.434
// --------------------------------------

// Grid: 100x100, iterations = 10
// Shortest path from (0,0) to (99,99): 198
// Reconstructed path (extended_bfs_grid): 0 1 2 3 103 104 105 205 206 306 307 407 507 607 707 807 907 908 1008 1108 1208 1209 1309 1310 1410 1510 1610 1710 1810 1910 1911 1912 1913 2013 2014 2114 2214 2314 2315 2415 2515 2516 2616 2716 2717 2817 2818 2819 2919 3019 3020 3120 3220 3320 3420 3421 3422 3423 3424 3524 3525 3625 3725 3726 3727 3827 3927 3928 3929 3930 3931 3932 3933 3934 4034 4035 4135 4136 4236 4237 4337 4338 4339 4439 4539 4540 4541 4542 4543 4643 4743 4744 4844 4944 4945 5045 5145 5245 5246 5247 5248 5348 5448 5548 5648 5748 5848 5948 5949 5950 5951 5952 6052 6053 6054 6055 6056 6057 6157 6257 6258 6259 6260 6261 6361 6362 6363 6364 6365 6366 6466 6467 6567 6568 6569 6570 6571 6572 6573 6673 6674 6675 6676 6776 6777 6778 6878 6978 7078 7178 7278 7378 7379 7380 7381 7481 7482 7483 7484 7485 7585 7586 7587 7687 7787 7887 7888 7988 8088 8089 8189 8289 8389 8489 8490 8491 8492 8493 8494 8495 8595 8695 8795 8796 8797 8798 8898 8998 9098 9099 9199 9299 9399 9499 9599 9699 9799 9899 9999 
// Average time per iteration (microseconds):
//   extended_bfs_grid : 346.193
//   dijkstra_heap_grid: 956.784
// --------------------------------------

// Grid: 200x200, iterations = 5
// Shortest path from (0,0) to (199,199): 398
// Reconstructed path (extended_bfs_grid): 0 200 400 600 800 1000 1200 1400 1600 1800 1801 1802 1803 2003 2203 2403 2603 2803 3003 3203 3403 3404 3604 3804 4004 4204 4205 4206 4207 4208 4408 4608 4609 4809 5009 5209 5210 5211 5411 5412 5413 5613 5813 6013 6213 6413 6613 6614 6615 6815 6816 6817 6818 7018 7218 7418 7419 7619 7819 8019 8219 8419 8619 8819 9019 9020 9021 9022 9023 9024 9224 9424 9624 9824 9825 9826 9827 10027 10028 10029 10030 10230 10231 10232 10233 10433 10434 10435 10436 10437 10438 10439 10639 10839 11039 11040 11041 11241 11242 11243 11443 11444 11644 11844 12044 12244 12245 12246 12247 12447 12647 12648 12848 13048 13248 13448 13449 13450 13451 13651 13851 14051 14251 14451 14651 14851 15051 15052 15053 15054 15254 15255 15256 15456 15656 15856 16056 16256 16257 16258 16259 16260 16261 16262 16462 16662 16663 16664 16864 17064 17264 17464 17465 17466 17666 17667 17668 17669 17869 18069 18070 18071 18072 18073 18074 18075 18275 18276 18277 18278 18279 18479 18480 18680 18880 19080 19280 19480 19680 19681 19682 19882 19883 20083 20283 20483 20683 20684 20884 21084 21284 21484 21684 21884 21885 21886 21887 22087 22287 22288 22488 22688 22689 22690 22691 22692 22693 22694 22695 22696 22697 22698 22898 22899 23099 23100 23300 23500 23700 23900 24100 24300 24500 24501 24701 24702 24902 25102 25103 25104 25105 25106 25107 25108 25109 25309 25509 25510 25710 25711 25911 26111 26311 26312 26313 26314 26514 26714 26914 27114 27115 27116 27316 27516 27716 27717 27917 27918 27919 27920 27921 27922 27923 28123 28323 28523 28723 28724 28725 28726 28727 28927 28928 29128 29328 29528 29728 29928 29929 30129 30329 30330 30530 30531 30731 30931 31131 31132 31332 31333 31533 31534 31535 31735 31935 32135 32136 32137 32138 32139 32140 32141 32341 32342 32542 32742 32743 32744 32745 32945 33145 33345 33545 33745 33945 34145 34345 34545 34745 34746 34747 34748 34749 34750 34751 34752 34753 34754 34954 35154 35354 35355 35356 35556 35756 35956 36156 36356 36357 36358 36359 36360 36361 36362 36562 36563 36763 36764 36765 36766 36767 36768 36769 36969 37169 37170 37171 37172 37372 37373 37573 37574 37575 37576 37577 37578 37579 37580 37581 37582 37583 37584 37585 37785 37985 37986 38186 38187 38387 38587 38787 38788 38789 38790 38791 38792 38793 38794 38795 38995 39195 39395 39396 39397 39597 39797 39997 39998 39999 
// Average time per iteration (microseconds):
//   extended_bfs_grid : 1430.96
//   dijkstra_heap_grid: 4079.95
// --------------------------------------
