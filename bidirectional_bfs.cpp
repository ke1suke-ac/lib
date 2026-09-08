#include <bits/stdc++.h>
using namespace std;

/*======================================================================
  BidirectionalBFS
  ----------------------------------------------------------------------
  単一ペア (s, t) の最短距離と経路を「双方向 BFS」で高速に取得するユーティリティ。
  内部バッファを再利用するため、同一インスタンスを使い回すと動的確保コストが抑えられる。
  ──────────────────────────────────────────────────────────
  ■ 使い方例
      BidirectionalBFS<int> solver;
      int dist = solver.bfs(G, R, s, t);     // 距離を取得
      if (dist != -1) {
          const auto& p = solver.path();     // 経路 vector<int>
      }
  ■ 特徴
    * 世代カウンタ方式により配列全初期化 O(|V|) を回避
    * 無向グラフでは R = G を渡せばよい
    * スレッドセーフではないので並列利用時はインスタンスを分割
======================================================================*/
template<std::integral T>
struct BidirectionalBFS {
private:
    /* sentinel 定数 */
    static constexpr T SENTINEL = std::numeric_limits<T>::max();
    static constexpr T INF_DIST = std::numeric_limits<T>::max();

    /* 再利用バッファ */
    vector<T>        parent_fw, parent_bw;
    vector<T>        dist_fw,   dist_bw;
    vector<uint32_t> visit_fw,  visit_bw;
    vector<T>        result_path;

    uint32_t cur_stamp = 1;           // 世代カウンタ（0 は未訪問）
    T        meet_vertex = SENTINEL;  // 交差点（経路復元用）

    /* オーバーフロー時に visit 配列をリセット */
    void maybe_reset_stamp() {
        if (cur_stamp != 0xFFFFFFFFu) return;
        std::fill(visit_fw.begin(), visit_fw.end(), 0);
        std::fill(visit_bw.begin(), visit_bw.end(), 0);
        cur_stamp = 1;
    }

    /* 必要に応じてバッファを拡張 */
    void ensure_size(size_t n) {
        if (parent_fw.size() >= n) return;
        parent_fw.resize(n, SENTINEL);
        parent_bw.resize(n, SENTINEL);
        dist_fw.resize(n, INF_DIST);
        dist_bw.resize(n, INF_DIST);
        visit_fw.resize(n, 0);
        visit_bw.resize(n, 0);
    }

public:
    /**
     * @brief 双方向 BFS を実行し最短距離を返す
     * @param G 順方向隣接リスト
     * @param R 逆方向隣接リスト（無向なら G と同じ）
     * @param s 始点 (0-indexed)
     * @param t 終点 (0-indexed)
     * @return 最短距離。到達不能なら -1
     *
     * 実行後に path() を呼ぶことで経路も取得可能。
     * 探索済み判定は世代カウンタ方式で行い、最短経路長が短い場合でも
     * 計算量は O(探索頂点数) に近づく。
     */
    int bfs(const vector<vector<T>>& G,
            const vector<vector<T>>& R,
            T s, T t)
    {
        const size_t n = G.size();
        ensure_size(n);
        maybe_reset_stamp();
        ++cur_stamp;

        visit_fw[s] = visit_bw[t] = cur_stamp;
        dist_fw[s]  = dist_bw[t]  = 0;
        parent_fw[s] = s;
        parent_bw[t] = t;
        meet_vertex  = SENTINEL;

        if (s == t) { meet_vertex = s; return 0; }

        deque<T> q_fw{ s }, q_bw{ t };

        while (!q_fw.empty() && !q_bw.empty()) {
            if (q_fw.size() <= q_bw.size()) {
                const int layer = static_cast<int>(q_fw.size());
                for (int i = 0; i < layer; ++i) {
                    T u = q_fw.front(); q_fw.pop_front();
                    for (T v : G[u]) {
                        if (visit_fw[v] == cur_stamp) continue;
                        visit_fw[v]  = cur_stamp;
                        dist_fw[v]   = dist_fw[u] + 1;
                        parent_fw[v] = u;
                        if (visit_bw[v] == cur_stamp) {
                            meet_vertex = v;
                            return static_cast<int>(dist_fw[v] + dist_bw[v]);
                        }
                        q_fw.push_back(v);
                    }
                }
            } else {
                const int layer = static_cast<int>(q_bw.size());
                for (int i = 0; i < layer; ++i) {
                    T u = q_bw.front(); q_bw.pop_front();
                    for (T v : R[u]) {
                        if (visit_bw[v] == cur_stamp) continue;
                        visit_bw[v]  = cur_stamp;
                        dist_bw[v]   = dist_bw[u] + 1;
                        parent_bw[v] = u;
                        if (visit_fw[v] == cur_stamp) {
                            meet_vertex = v;
                            return static_cast<int>(dist_fw[v] + dist_bw[v]);
                        }
                        q_bw.push_back(v);
                    }
                }
            }
        }
        return -1;  // 到達不能
    }

    /**
     * @brief 直前の bfs() が見つけた経路を返す
     * @return 始点 → 終点 の頂点列。到達不能だった場合は空 vector
     *
     * bfs() を呼び出さずに連続で呼ぶと直前の結果が再度返る点に注意。
     */
    const vector<T>& path() {
        if (meet_vertex == SENTINEL) return result_path;
        result_path.clear();

        vector<T> left;
        for (T v = meet_vertex;; v = parent_fw[v]) {
            left.push_back(v);
            if (v == parent_fw[v]) break;
        }
        reverse(left.begin(), left.end());

        vector<T> right;
        for (T v = meet_vertex; v != parent_bw[v]; ) {
            v = parent_bw[v];
            right.push_back(v);
        }

        result_path.reserve(left.size() + right.size());
        result_path.insert(result_path.end(), left.begin(), left.end());
        result_path.insert(result_path.end(), right.begin(), right.end());
        return result_path;
    }
};

#if 1   // ===================== テスト & ベンチマーク =====================
#include <chrono>
#include <random>

/* 単方向 BFS（正解確認用） */
static int bfs_single(const vector<vector<int>>& G, int s, int t) {
    int n = static_cast<int>(G.size());
    vector<int> dist(n, -1);
    queue<int> q;
    dist[s] = 0; q.push(s);
    while (!q.empty()) {
        int u = q.front(); q.pop();
        if (u == t) return dist[u];
        for (int v : G[u]) if (dist[v] == -1) {
            dist[v] = dist[u] + 1;
            q.push(v);
        }
    }
    return -1;
}

/* 連結無向ランダムグラフ生成 */
static vector<vector<int>>
gen_random_connected_graph(int n, int extra_edges, std::mt19937& rng) {
    vector<vector<int>> g(n);
    std::uniform_int_distribution<int> dis(0, n - 1);
    for (int i = 1; i < n; ++i) {       // ランダム木
        int p = dis(rng) % i;
        g[i].push_back(p);
        g[p].push_back(i);
    }
    for (int e = 0; e < extra_edges; ++e) {  // 追加無向辺
        int u = dis(rng), v = dis(rng);
        if (u == v) continue;
        g[u].push_back(v);
        g[v].push_back(u);
    }
    return g;
}

/* 経路妥当性チェック */
static void verify_path(const vector<vector<int>>& G,
                        const vector<int>& path,
                        int s, int t, int dist_expected)
{
    if (dist_expected == -1) { assert(path.empty()); return; }
    assert(!path.empty());
    assert(static_cast<int>(path.size()) == dist_expected + 1);
    assert(path.front() == s && path.back() == t);

    for (size_t i = 0; i + 1 < path.size(); ++i) {
        int u = path[i], v = path[i + 1];
        bool ok = false;
        for (int x : G[u]) if (x == v) { ok = true; break; }
        assert(ok);
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    std::mt19937 rng(123456);

    /* エッジケース */
    {
        vector<vector<int>> g(1);
        BidirectionalBFS<int> solver;
        int d = solver.bfs(g, g, 0, 0);
        verify_path(g, solver.path(), 0, 0, d);
        cout << "Edge case test 1 passed.\n";
    }
    {
        vector<vector<int>> g(2);
        BidirectionalBFS<int> solver;
        int d = solver.bfs(g, g, 0, 1);
        verify_path(g, solver.path(), 0, 1, d);
        cout << "Edge case test 2 passed.\n";
    }

    /* 整合性テスト */
    for (int n : {10, 50, 100}) {
        for (int iter = 0; iter < 100; ++iter) {
            auto g = gen_random_connected_graph(n, n, rng);
            int s = rng() % n, t = rng() % n;
            BidirectionalBFS<int> solver;
            int d_bi = solver.bfs(g, g, s, t);
            int d_single = bfs_single(g, s, t);
            assert(d_bi == d_single);
            verify_path(g, solver.path(), s, t, d_bi);
        }
    }
    cout << "Random consistency tests (with path) passed.\n";

    /* ベンチマーク */
    for (int n : {100, 1000, 10000}) {
        int iterations = (n == 100 ? 500 : (n == 1000 ? 200 : 20));
        auto g = gen_random_connected_graph(n, n * 2, rng);

        BidirectionalBFS<int> solver;
        std::uniform_int_distribution<int> dis(0, n - 1);
        long long total_time_bi = 0, total_time_single = 0;
        long long total_len_bi = 0, total_len_single = 0;

        for (int it = 0; it < iterations; ++it) {
            int s = dis(rng), t = dis(rng);

            auto t0 = chrono::steady_clock::now();
            int d_bi = solver.bfs(g, g, s, t);
            const auto& pvec = solver.path();
            auto t1 = chrono::steady_clock::now();
            int d_single = bfs_single(g, s, t);
            auto t2 = chrono::steady_clock::now();

            verify_path(g, pvec, s, t, d_bi);

            total_time_bi     += chrono::duration_cast<chrono::microseconds>(t1 - t0).count();
            total_time_single += chrono::duration_cast<chrono::microseconds>(t2 - t1).count();
            total_len_bi      += d_bi;
            total_len_single  += d_single;
        }

        cout << "n=" << n << " iterations=" << iterations << '\n';
        cout << "  Bidirectional BFS average time (us): "
             << (double)total_time_bi / iterations
             << "  average path length: "
             << (double)total_len_bi / iterations << '\n';
        cout << "  Single BFS        average time (us): "
             << (double)total_time_single / iterations
             << "  average path length: "
             << (double)total_len_single / iterations << '\n';
    }
    return 0;
}
#endif  // テスト & ベンチマーク
