/******************************************************************************
 *  最小全域木 (MST) を利用したシングルリンククラスタリング – コピペ即利用サンプル
 *
 *  ────────────────────────────────────────────────────────────────
 *  ▼ 概要
 *    1. Prim 法で MST（ユークリッド距離²を重み）を構築
 *    2. MST のうち重い辺を (K-1) 本だけ削除
 *    3. 残った辺で得られる連結成分が K 個 ⇒ シングルリンク法のクラスタ
 *
 *  ▼ do_mst_clustering<T,DIM>() の使い方
 *      ┌ 戻り値 : 削除した辺数 (= クラスタ数増加数)
 *      ├ 第 1 引数 : vector<array<T,DIM>>  入力点列
 *      ├ 第 2 引数 : unsigned short        要求クラスタ数 K (1‥N)
 *      └ 第 3 引数 : vector<int>&          出力クラスタID (0‥K-1 の連番)
 *
 *    - 計算量 O(N²·DIM) : N が大きい場合は kd-tree 等で高速化を検討
 *    - K >= N なら各点が独立クラスタ / K==0 なら何もしない
 *
 *  ▼ サンプル main() の構成
 *      [A] 問題設定    : ①一様乱数 ②ガウスクラスタ の 2 ユースケースを生成
 *      [B] 問題解決    : do_mst_clustering を呼び assignments を取得
 *      [C] 結果表示    : ASCII グリッドにクラスタ文字を可視化＋SSE 表示
 *
 *    run_case() 内のコメントをそのままコピペすれば任意データで再利用できます。
 *****************************************************************************/

#include <array>
#include <vector>
#include <utility>
#include <limits>
#include <random>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cassert>

using namespace std;

// ============================================================================
// DSU (Disjoint Set Union) ★親に負サイズを格納する軽量実装
// ============================================================================
class DSU {
    vector<int> p;                     // root : -size,  non-root : parent index
public:
    explicit DSU(int n) : p(n, -1) {}
    int find(int x) {                  // 根を取得 (2 段パス圧縮)
        while (p[x] >= 0) {
            if (p[p[x]] >= 0) p[x] = p[p[x]];
            x = p[x];
        }
        return x;
    }
    bool unite(int a, int b) {         // 異集合なら統合して true
        a = find(a); b = find(b);
        if (a == b) return false;
        if (-p[a] < -p[b]) swap(a, b); // サイズ大を root に
        p[a] += p[b];
        p[b]  = a;
        return true;
    }
    int size(int x) { return -p[find(x)]; }
};

// ============================================================================
// do_mst_clustering  ★MST-based シングルリンククラスタリング
//   戻り値 : 削除した辺数 (= N - 実クラスタ数)
// ============================================================================
template <std::floating_point T, size_t DIM>
int do_mst_clustering(const vector<array<T, DIM>>& data,
                      unsigned short K,
                      vector<int>& assignments) {
    const size_t N = data.size();
    assignments.assign(N, 0);
    if (N == 0 || K == 0) return 0;            // 空入力
    if (K >= N) {                              // 各点が独立クラスタ
        iota(assignments.begin(), assignments.end(), 0);
        return 0;
    }

    // ---- [1] Prim 法で MST 構築 (距離²で高速) ----------------------------
    struct Edge { T w; int u, v; };
    vector<T>   best(N, numeric_limits<T>::max());
    vector<int> parent(N, -1);
    vector<char> used(N, 0);
    best[0] = 0;

    for (size_t it = 0; it < N; ++it) {
        size_t u = N; T bw = numeric_limits<T>::max();
        for (size_t i = 0; i < N; ++i)
            if (!used[i] && best[i] < bw) { bw = best[i]; u = i; }
        assert(u != N);
        used[u] = 1;
        for (size_t v = 0; v < N; ++v) if (!used[v]) {
            T dist2 = 0;
            for (size_t d = 0; d < DIM; ++d) {
                T diff = data[u][d] - data[v][d];
                dist2 += diff * diff;
            }
            if (dist2 < best[v]) { best[v] = dist2; parent[v] = static_cast<int>(u); }
        }
    }

    // ---- [2] MST 辺集合を配列化 → nth_element で最重量 (K-1) 本をカット --
    vector<Edge> edges; edges.reserve(N);
    for (size_t v = 1; v < N; ++v) edges.push_back({best[v], parent[v], static_cast<int>(v)});

    auto heavier = [](const Edge& a, const Edge& b){ return a.w > b.w; };
    size_t cut = (K > 0 ? K - 1 : 0);
    if (cut > 0 && cut < edges.size())
        nth_element(edges.begin(), edges.begin() + cut, edges.end(), heavier);

    // ---- [3] 残った辺で連結しクラスタ分割 ---------------------------------
    DSU dsu(static_cast<int>(N));
    for (size_t i = cut; i < edges.size(); ++i) dsu.unite(edges[i].u, edges[i].v);

    // ---- [4] クラスタ ID を 0..K-1 にリマップ ----------------------------
    vector<int> root2id(N, -1); int cid = 0;
    for (size_t i = 0; i < N; ++i) {
        int r = dsu.find(static_cast<int>(i));
        if (root2id[r] == -1) root2id[r] = cid++;
        assignments[i] = root2id[r];
    }
    return static_cast<int>(N - cid);          // 切断した辺数
}

// ============================================================================
// ヘルパ: SSE (Sum-of-Squared-Errors) を計算するだけの関数
// ============================================================================
template <std::floating_point T, size_t DIM>
T calc_sse(const vector<array<T, DIM>>& data,
           unsigned short K,
           const vector<int>& assignments) {
    const size_t N = data.size();
    vector<array<T, DIM>> centroids(K); for (auto& c : centroids) c.fill(T(0));
    vector<int> counts(K, 0);

    for (size_t i = 0; i < N; ++i) {
        int k = assignments[i]; ++counts[k];
        for (size_t d = 0; d < DIM; ++d) centroids[k][d] += data[i][d];
    }
    for (int k = 0; k < K; ++k) if (counts[k]) {
        T inv = T(1) / counts[k];
        for (size_t d = 0; d < DIM; ++d) centroids[k][d] *= inv;
    }

    T sse = 0;
    for (size_t i = 0; i < N; ++i) {
        int k = assignments[i]; T d2 = 0;
        for (size_t d = 0; d < DIM; ++d) {
            T diff = data[i][d] - centroids[k][d];
            d2 += diff * diff;
        }
        sse += d2;
    }
    return sse;
}

// ============================================================================
// main() – 2 ユースケースで do_mst_clustering を実演
// ============================================================================
int main() {
    // [A-共通設定] ###########################################################
    constexpr size_t ROWS = 20, COLS = 80;   // ASCII グリッドサイズ
    constexpr size_t DIM  = 2;               // 座標次元
    constexpr int    N    = 120;             // 点数
    constexpr unsigned short K = 5;          // 要求クラスタ数

    mt19937_64 rng(123456789ULL);
    uniform_real_distribution<double> ur_row(0.0, ROWS - 1);
    uniform_real_distribution<double> ur_col(0.0, COLS - 1);

    /*-----------------------------------------------------------------------
     *  run_case() : 任意の点列で
     *      [B] do_mst_clustering 呼び出し → assignments / SSE 取得
     *      [C] ASCII グリッドにクラスタ文字を描画
     *------------------------------------------------------------------------*/
    auto run_case = [&](const vector<array<double, DIM>>& pts, const char* title) {
        //------------------------- [B] 問題解決 -------------------------
        vector<int> asg;
        int cuts = do_mst_clustering<double, DIM>(pts, K, asg);
        double sse = calc_sse<double, DIM>(pts, K, asg);

        //------------------------- [C] 結果表示 -------------------------
        vector<string> grid(ROWS, string(COLS, '.'));
        for (size_t i = 0; i < pts.size(); ++i) {
            int r = static_cast<int>(pts[i][0] + 0.5);
            int c = static_cast<int>(pts[i][1] + 0.5);
            if (0 <= r && r < int(ROWS) && 0 <= c && c < int(COLS))
                grid[r][c] = static_cast<char>('A' + (asg[i] % 26));
        }
        cout << "==== " << title << " ====\n";
        cout << "clusters=" << K
             << "  cuts=" << cuts
             << "  SSE=" << fixed << setprecision(2) << sse << "\n\n";
        for (const auto& row : grid) cout << row << '\n';
        cout << '\n';
    };

    // ------------------------- [A-1] 一様乱数 ------------------------------
    vector<array<double, DIM>> uniform_pts(N);
    for (auto& p : uniform_pts) { p[0] = ur_row(rng); p[1] = ur_col(rng); }
    run_case(uniform_pts, "Uniform Random");

    // ------------------------- [A-2] ガウスクラスタ ------------------------
    uniform_real_distribution<double> uc_row(2.0, ROWS - 3.0);
    uniform_real_distribution<double> uc_col(4.0, COLS - 5.0);
    vector<array<double, DIM>> centers;
    for (int i = 0; i < 4; ++i) centers.push_back({uc_row(rng), uc_col(rng)});

    normal_distribution<double> nr(0.0, 2.0);
    normal_distribution<double> nc(0.0, 4.0);
    vector<array<double, DIM>> cluster_pts(N);
    for (int i = 0; i < N; ++i) {
        const auto& cen = centers[i % 4];
        double r = clamp(cen[0] + nr(rng), 0.0, double(ROWS - 1));
        double c = clamp(cen[1] + nc(rng), 0.0, double(COLS - 1));
        cluster_pts[i] = {r, c};
    }
    run_case(cluster_pts, "Cluster-friendly Random");

    return 0;
}
