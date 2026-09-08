#include <bitset>
#include <chrono>
#include <iostream>
#include <queue>
#include <string>
#include <vector>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <random>

using namespace std;
using namespace std::chrono;

// ----- 効率化BFS（ビット演算による実装） -----
template <int W, int H>
int bfs(const bitset<W * H>& grid, int start_x, int start_y, int goal_x, int goal_y) {
    constexpr int N = W * H;
    // 境界マスク作成（シフト時のラップアラウンド防止用）
    bitset<N> topBoundary, bottomBoundary, leftBoundary, rightBoundary;
    for (int i = 0; i < N; i++) {
        int x = i % W;
        int y = i / W;
        if (y == 0)        topBoundary.set(i);
        if (y == H - 1)    bottomBoundary.set(i);
        if (x == 0)        leftBoundary.set(i);
        if (x == W - 1)    rightBoundary.set(i);
    }
    // 通路のみ進入可能（grid は 1==壁 なので反転）
    bitset<N> freeMask = ~grid;
    
    int start_index = start_y * W + start_x;
    int goal_index  = goal_y * W + goal_x;
    if (!freeMask.test(start_index) || !freeMask.test(goal_index))
        return -1;
    
    bitset<N> current;
    current.set(start_index);
    bitset<N> visited = current;
    int steps = 0;
    
    while (true) {
        if (current.test(goal_index))
            return steps;
        bitset<N> up    = (current & ~topBoundary)    >> W;
        bitset<N> down  = (current & ~bottomBoundary) << W;
        bitset<N> left  = (current & ~leftBoundary)   >> 1;
        bitset<N> right = (current & ~rightBoundary)  << 1;
        
        bitset<N> next = (up | down | left | right) & freeMask & ~visited;
        if (next.none())
            return -1;
        
        visited |= next;
        current = next;
        steps++;
    }
}

// ----- 一般的なBFS（キューを用いた実装） -----
template <int W, int H>
int bfs_standard(const bitset<W * H>& grid, int start_x, int start_y, int goal_x, int goal_y) {
    constexpr int N = W * H;
    vector<bool> visited(N, false);
    queue<int> q;
    int start_index = start_y * W + start_x;
    int goal_index = goal_y * W + goal_x;
    if (grid.test(start_index) || grid.test(goal_index))
        return -1;
    visited[start_index] = true;
    q.push(start_index);
    int steps = 0;
    
    while (!q.empty()) {
        size_t qs = q.size(); // size_t に変更
        for (size_t i = 0; i < qs; i++) {
            int cur = q.front();
            q.pop();
            if (cur == goal_index)
                return steps;
            int cx = cur % W;
            int cy = cur / W;
            int dx[4] = {-1, 1, 0, 0};
            int dy[4] = {0, 0, -1, 1};
            for (int j = 0; j < 4; j++) {
                int nx = cx + dx[j];
                int ny = cy + dy[j];
                if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                int nindex = ny * W + nx;
                if (grid.test(nindex) || visited[nindex])
                    continue;
                visited[nindex] = true;
                q.push(nindex);
            }
        }
        steps++;
    }
    return -1;
}

// ----- A*アルゴリズム -----
// ヒューリスティックとしてマンハッタン距離を利用
template <int W, int H>
int astar(const bitset<W * H>& grid, int start_x, int start_y, int goal_x, int goal_y) {
    constexpr int N = W * H;
    int start_index = start_y * W + start_x;
    int goal_index = goal_y * W + goal_x;
    if (grid.test(start_index) || grid.test(goal_index))
        return -1;
    const int INF = 1e9;
    vector<int> cost(N, INF);
    struct Node { int f, g, index; };
    struct Compare {
        bool operator()(const Node& a, const Node& b) const {
            return a.f > b.f;
        }
    };
    priority_queue<Node, vector<Node>, Compare> pq;
    cost[start_index] = 0;
    int h = abs(start_x - goal_x) + abs(start_y - goal_y);
    pq.push(Node{h, 0, start_index});
    
    while (!pq.empty()) {
        Node cur = pq.top();
        pq.pop();
        if (cur.index == goal_index)
            return cur.g;
        if (cur.g > cost[cur.index])
            continue;
        int cx = cur.index % W;
        int cy = cur.index / W;
        int directions[4][2] = { {-1,0}, {1,0}, {0,-1}, {0,1} };
        for (int i = 0; i < 4; i++) {
            int nx = cx + directions[i][0];
            int ny = cy + directions[i][1];
            if (nx < 0 || nx >= W || ny < 0 || ny >= H)
                continue;
            int nindex = ny * W + nx;
            if (grid.test(nindex))
                continue;
            int ng = cur.g + 1;
            if (ng < cost[nindex]) {
                cost[nindex] = ng;
                int nh = abs(nx - goal_x) + abs(ny - goal_y);
                pq.push(Node{ng + nh, ng, nindex});
            }
        }
    }
    return -1;
}

// ----- 複雑な迷路生成（再帰的バックトラッキング） -----
// ここでは、外周は壁とし、内部は奇数座標にセルを配置
template <int W, int H>
bitset<W * H> generate_maze() {
    constexpr int N = W * H;
    bitset<N> maze;
    // 全体を壁で初期化
    maze.set();
    
    // セルグリッドのサイズ
    int cell_rows = (H - 2) / 2;
    int cell_cols = (W - 2) / 2;
    vector<vector<bool>> visited(cell_rows, vector<bool>(cell_cols, false));
    
    // ランダムエンジン（再現性のため固定シード）
    mt19937 rng(42);
    
    // DFS用のスタック。セル座標 (r, c) で管理。
    vector<pair<int,int>> stack;
    
    auto carve = [&](int r, int c) {
        // 実際の座標に変換: (2*r+1, 2*c+1)
        int rr = 2 * r + 1;
        int cc = 2 * c + 1;
        maze.reset(rr * W + cc); // 通路にする (0)
    };
    
    // 初期セル (0,0) → (1,1) に対応
    visited[0][0] = true;
    carve(0, 0);
    stack.push_back({0, 0});
    
    // 隣接セル方向（上下左右）
    vector<pair<int,int>> directions = {{-1,0}, {1,0}, {0,-1}, {0,1}};
    
    while (!stack.empty()) {
        auto [r, c] = stack.back();
        // 利用可能な隣接セルを集める
        vector<pair<int,int>> neighbors;
        for (auto [dr, dc] : directions) {
            int nr = r + dr, nc = c + dc;
            if (nr < 0 || nr >= cell_rows || nc < 0 || nc >= cell_cols)
                continue;
            if (!visited[nr][nc])
                neighbors.push_back({nr, nc});
        }
        if (!neighbors.empty()) {
            // 下記で neighbors.size() の型変換を明示的に行う
            int ub = static_cast<int>(neighbors.size()) - 1;
            uniform_int_distribution<int> dist(0, ub);
            auto [nr, nc] = neighbors[dist(rng)];
            // 壁を取り除く: 現在のセルと隣接セルの間の壁
            int wall_r = 2 * r + 1 + (nr - r);
            int wall_c = 2 * c + 1 + (nc - c);
            maze.reset(wall_r * W + wall_c);
            // 隣接セルを通路に
            carve(nr, nc);
            visited[nr][nc] = true;
            stack.push_back({nr, nc});
        } else {
            stack.pop_back();
        }
    }
    return maze;
}

// ----- シンプルな迷路のベンチマーク -----
// 外周は壁、内部は全て通路とする
template <int W, int H>
void benchmark_simple(int iterations) {
    constexpr int N = W * H;
    bitset<N> grid;
    // 外周は壁、内部は通路 (初期状態は0)
    for (int i = 0; i < N; i++) {
        int x = i % W, y = i / W;
        if (x == 0 || x == W - 1 || y == 0 || y == H - 1)
            grid.set(i);
    }
    int start_x = 1, start_y = 1;
    int goal_x = W - 2, goal_y = H - 2;
    
    volatile int resEff = 0;
    auto startEff = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        resEff = bfs<W,H>(grid, start_x, start_y, goal_x, goal_y);
    }
    auto endEff = high_resolution_clock::now();
    auto durationEff = duration_cast<microseconds>(endEff - startEff).count();
    
    volatile int resStd = 0;
    auto startStd = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        resStd = bfs_standard<W,H>(grid, start_x, start_y, goal_x, goal_y);
    }
    auto endStd = high_resolution_clock::now();
    auto durationStd = duration_cast<microseconds>(endStd - startStd).count();
    
    volatile int resAstar = 0;
    auto startAstar = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        resAstar = astar<W,H>(grid, start_x, start_y, goal_x, goal_y);
    }
    auto endAstar = high_resolution_clock::now();
    auto durationAstar = duration_cast<microseconds>(endAstar - startAstar).count();
    
    cout << "Simple Maze Grid " << W << "x" << H << " (" << iterations << " iterations):\n";
    cout << "  Efficient BFS: Total " << durationEff << " us, Avg " 
         << (double)durationEff / iterations << " us per call.\n";
    cout << "  Standard BFS:  Total " << durationStd << " us, Avg " 
         << (double)durationStd / iterations << " us per call.\n";
    cout << "  A* Algorithm:  Total " << durationAstar << " us, Avg " 
         << (double)durationAstar / iterations << " us per call.\n";
    cout << "  Results: Efficient BFS = " << resEff 
         << ", Standard BFS = " << resStd 
         << ", A* = " << resAstar << "\n\n";
}

// ----- 複雑な迷路のベンチマーク -----
// 複雑な迷路は generate_maze() により生成。
// ※ゴールは、生成された迷路内で必ず通路となる (W-3, H-3) とする。
template <int W, int H>
void benchmark_complex(int iterations) {
    constexpr int N = W * H;
    bitset<N> maze = generate_maze<W,H>();
    
    int start_x = 1, start_y = 1;
    int goal_x = W - 3, goal_y = H - 3;
    volatile int resEff = 0;
    auto startEff = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        resEff = bfs<W,H>(maze, start_x, start_y, goal_x, goal_y);
    }
    auto endEff = high_resolution_clock::now();
    auto durationEff = duration_cast<microseconds>(endEff - startEff).count();
    
    volatile int resStd = 0;
    auto startStd = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        resStd = bfs_standard<W,H>(maze, start_x, start_y, goal_x, goal_y);
    }
    auto endStd = high_resolution_clock::now();
    auto durationStd = duration_cast<microseconds>(endStd - startStd).count();
    
    volatile int resAstar = 0;
    auto startAstar = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        resAstar = astar<W,H>(maze, start_x, start_y, goal_x, goal_y);
    }
    auto endAstar = high_resolution_clock::now();
    auto durationAstar = duration_cast<microseconds>(endAstar - startAstar).count();
    
    cout << "Complex Maze Grid " << W << "x" << H << " (" << iterations << " iterations):\n";
    cout << "  Efficient BFS: Total " << durationEff << " us, Avg " 
         << (double)durationEff / iterations << " us per call.\n";
    cout << "  Standard BFS:  Total " << durationStd << " us, Avg " 
         << (double)durationStd / iterations << " us per call.\n";
    cout << "  A* Algorithm:  Total " << durationAstar << " us, Avg " 
         << (double)durationAstar / iterations << " us per call.\n";
    cout << "  Results: Efficient BFS = " << resEff 
         << ", Standard BFS = " << resStd 
         << ", A* = " << resAstar << "\n\n";
}

// ----- 中間（障害物パターン）迷路の生成 -----
// 境界は壁。内部は全体は通路とし、以下のパターンで障害物を配置：
// ・縦壁：x = mid_x（W/2）に対し、y=1～H-2 の全セルを壁にするが、1箇所だけギャップ（gap_v）を残す。
// ・横壁：y = mid_y（H/2）に対し、x=1～W-2 の全セルを壁にするが、1箇所だけギャップ（gap_h）を残す。
// start=(1,1)、goal=(W-2, H-2) は必ず通路とする。
template <int W, int H>
bitset<W * H> generate_intermediate_grid() {
    constexpr int N = W * H;
    bitset<N> grid;
    // 初期は全体を通路(0)とし、外周は壁にする
    for (int i = 0; i < N; i++) {
        int x = i % W, y = i / W;
        if (x == 0 || x == W - 1 || y == 0 || y == H - 1)
            grid.set(i);
        else
            grid.reset(i);
    }
    // 縦壁：x = mid_x, ただし1箇所のギャップ (gap_v)
    int mid_x = W / 2;
    int gap_v = ((H - 2) / 3) + 1; // 1～H-2の範囲で
    for (int y = 1; y < H - 1; y++) {
        if (y == gap_v) continue;
        grid.set(y * W + mid_x);
    }
    // 横壁：y = mid_y, ただし1箇所のギャップ (gap_h)
    int mid_y = H / 2;
    int gap_h = (((W - 2) * 2) / 3) + 1; // 1～W-2の範囲で
    for (int x = 1; x < W - 1; x++) {
        if (x == gap_h) continue;
        grid.set(mid_y * W + x);
    }
    // 念のため、start と goal は通路にする
    grid.reset(1 * W + 1);
    grid.reset((H - 2) * W + (W - 2));
    return grid;
}

template <int W, int H>
void benchmark_intermediate(int iterations) {
    constexpr int N = W * H;
    bitset<N> grid = generate_intermediate_grid<W,H>();
    int start_x = 1, start_y = 1;
    int goal_x = W - 2, goal_y = H - 2;
    
    volatile int resEff = 0;
    auto startEff = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        resEff = bfs<W,H>(grid, start_x, start_y, goal_x, goal_y);
    }
    auto endEff = high_resolution_clock::now();
    auto durationEff = duration_cast<microseconds>(endEff - startEff).count();
    
    volatile int resStd = 0;
    auto startStd = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        resStd = bfs_standard<W,H>(grid, start_x, start_y, goal_x, goal_y);
    }
    auto endStd = high_resolution_clock::now();
    auto durationStd = duration_cast<microseconds>(endStd - startStd).count();
    
    volatile int resAstar = 0;
    auto startAstar = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        resAstar = astar<W,H>(grid, start_x, start_y, goal_x, goal_y);
    }
    auto endAstar = high_resolution_clock::now();
    auto durationAstar = duration_cast<microseconds>(endAstar - startAstar).count();
    
    cout << "Intermediate Maze Grid " << W << "x" << H << " (" << iterations << " iterations):\n";
    cout << "  Efficient BFS: Total " << durationEff << " us, Avg " 
         << (double)durationEff / iterations << " us per call.\n";
    cout << "  Standard BFS:  Total " << durationStd << " us, Avg " 
         << (double)durationStd / iterations << " us per call.\n";
    cout << "  A* Algorithm:  Total " << durationAstar << " us, Avg " 
         << (double)durationAstar / iterations << " us per call.\n";
    cout << "  Results: Efficient BFS = " << resEff 
         << ", Standard BFS = " << resStd 
         << ", A* = " << resAstar << "\n\n";
}

int main(){
    // 各グリッドサイズごとに、イテレーション数をサイズに応じて設定
    cout << "=== Simple Maze Benchmarks ===\n";
    benchmark_simple<8,8>(200000);
    benchmark_simple<16,16>(200000);
    benchmark_simple<32,32>(20000);
    benchmark_simple<64,64>(2000);
    
    cout << "=== Complex Maze Benchmarks ===\n";
    benchmark_complex<8,8>(200000);
    benchmark_complex<16,16>(200000);
    benchmark_complex<32,32>(20000);
    benchmark_complex<64,64>(2000);
    
    cout << "=== Intermediate Maze Benchmarks ===\n";
    benchmark_intermediate<8,8>(200000);
    benchmark_intermediate<16,16>(200000);
    benchmark_intermediate<32,32>(20000);
    benchmark_intermediate<64,64>(2000);
    
    return 0;
}

// 実行結果
// === Simple Maze Benchmarks ===
// Simple Maze Grid 8x8 (200000 iterations):
//   Efficient BFS: Total 19437 us, Avg 0.097185 us per call.
//   Standard BFS:  Total 66521 us, Avg 0.332605 us per call.
//   A* Algorithm:  Total 120405 us, Avg 0.602025 us per call.
//   Results: Efficient BFS = 10, Standard BFS = 10, A* = 10

// Simple Maze Grid 16x16 (200000 iterations):
//   Efficient BFS: Total 286517 us, Avg 1.43258 us per call.
//   Standard BFS:  Total 365784 us, Avg 1.82892 us per call.
//   A* Algorithm:  Total 507714 us, Avg 2.53857 us per call.
//   Results: Efficient BFS = 26, Standard BFS = 26, A* = 26

// Simple Maze Grid 32x32 (20000 iterations):
//   Efficient BFS: Total 148829 us, Avg 7.44145 us per call.
//   Standard BFS:  Total 170270 us, Avg 8.5135 us per call.
//   A* Algorithm:  Total 137383 us, Avg 6.86915 us per call.
//   Results: Efficient BFS = 58, Standard BFS = 58, A* = 58

// Simple Maze Grid 64x64 (2000 iterations):
//   Efficient BFS: Total 74065 us, Avg 37.0325 us per call.
//   Standard BFS:  Total 74712 us, Avg 37.356 us per call.
//   A* Algorithm:  Total 134765 us, Avg 67.3825 us per call.
//   Results: Efficient BFS = 122, Standard BFS = 122, A* = 122

// === Complex Maze Benchmarks ===
// Complex Maze Grid 8x8 (200000 iterations):
//   Efficient BFS: Total 18101 us, Avg 0.090505 us per call.
//   Standard BFS:  Total 26913 us, Avg 0.134565 us per call.
//   A* Algorithm:  Total 32038 us, Avg 0.16019 us per call.
//   Results: Efficient BFS = 8, Standard BFS = 8, A* = 8

// Complex Maze Grid 16x16 (200000 iterations):
//   Efficient BFS: Total 302034 us, Avg 1.51017 us per call.
//   Standard BFS:  Total 65702 us, Avg 0.32851 us per call.
//   A* Algorithm:  Total 100796 us, Avg 0.50398 us per call.
//   Results: Efficient BFS = 28, Standard BFS = 28, A* = 28

// Complex Maze Grid 32x32 (20000 iterations):
//   Efficient BFS: Total 344808 us, Avg 17.2404 us per call.
//   Standard BFS:  Total 42026 us, Avg 2.1013 us per call.
//   A* Algorithm:  Total 56165 us, Avg 2.80825 us per call.
//   Results: Efficient BFS = 148, Standard BFS = 148, A* = 148

// Complex Maze Grid 64x64 (2000 iterations):
//   Efficient BFS: Total 559998 us, Avg 279.999 us per call.
//   Standard BFS:  Total 32929 us, Avg 16.4645 us per call.
//   A* Algorithm:  Total 50882 us, Avg 25.441 us per call.
//   Results: Efficient BFS = 980, Standard BFS = 980, A* = 980

// === Intermediate Maze Benchmarks ===
// Intermediate Maze Grid 8x8 (200000 iterations):
//   Efficient BFS: Total 19128 us, Avg 0.09564 us per call.
//   Standard BFS:  Total 46704 us, Avg 0.23352 us per call.
//   A* Algorithm:  Total 62477 us, Avg 0.312385 us per call.
//   Results: Efficient BFS = 10, Standard BFS = 10, A* = 10

// Intermediate Maze Grid 16x16 (200000 iterations):
//   Efficient BFS: Total 284736 us, Avg 1.42368 us per call.
//   Standard BFS:  Total 235410 us, Avg 1.17705 us per call.
//   A* Algorithm:  Total 397071 us, Avg 1.98535 us per call.
//   Results: Efficient BFS = 26, Standard BFS = 26, A* = 26

// Intermediate Maze Grid 32x32 (20000 iterations):
//   Efficient BFS: Total 146904 us, Avg 7.3452 us per call.
//   Standard BFS:  Total 115495 us, Avg 5.77475 us per call.
//   A* Algorithm:  Total 201759 us, Avg 10.0879 us per call.
//   Results: Efficient BFS = 58, Standard BFS = 58, A* = 58

// Intermediate Maze Grid 64x64 (2000 iterations):
//   Efficient BFS: Total 74072 us, Avg 37.036 us per call.
//   Standard BFS:  Total 55418 us, Avg 27.709 us per call.
//   A* Algorithm:  Total 70464 us, Avg 35.232 us per call.
//   Results: Efficient BFS = 122, Standard BFS = 122, A* = 122
