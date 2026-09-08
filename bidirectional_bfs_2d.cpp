#include <bits/stdc++.h>
using namespace std;

/*======================================================================
  BidirectionalBFS2D
  ----------------------------------------------------------------------
  2 点間最短経路を、スタート側／ゴール側から同時に BFS を進めて探索する
  “双方向 BFS” の軽量実装。訪問配列は世代カウンタで再利用し、
  グリッド全体サイズより実際の探索幅に近い計算量で動作する。
 ======================================================================*/
struct BidirectionalBFS2D {
    /*---------------------------- 型 ----------------------------*/
    using Cord = pair<int, int>;  ///< 座標 (row, col)

    /*------------------------- 使い方概観 ------------------------
      BidirectionalBFS2D solver;
      int dist = solver.bfs(grid, s, t, neigh, neigh); // 最短距離
      auto path = solver.path();                       // 座標列
    -------------------------------------------------------------*/

   private:
    /* 動的バッファ（確保したら使い回す） */
    size_t R = 0, C = 0;
    vector<int> dist_fw, dist_bk;    // 距離
    vector<int> prev_fw, prev_bk;    // 親インデックス
    vector<int> stamp_fw, stamp_bk;  // 訪問世代
    int cur_fw = 1, cur_bk = 1;      // 現在世代
    vector<Cord> path_buf;           // 経路バッファ
    Cord meet{-1, -1};               // 合流セル
    bool has_path = false;           // 経路が存在したか

    /* 座標⇔1 次元 index 変換 */
    inline size_t idx(const Cord& p) const noexcept {
        return static_cast<size_t>(p.first) * C + static_cast<size_t>(p.second);
    }
    inline Cord cell(size_t id) const noexcept { return {static_cast<int>(id / C), static_cast<int>(id % C)}; }

    /* バッファ確保 */
    void ensure_capacity(size_t N) {
        auto reserve_if = [N](vector<int>& v) {
            if (v.size() < N) v.resize(N, 0);
        };
        reserve_if(dist_fw);
        reserve_if(dist_bk);
        reserve_if(prev_fw);
        reserve_if(prev_bk);
        reserve_if(stamp_fw);
        reserve_if(stamp_bk);
    }

    /* 世代 wrap 対策（INT_MAX 付近でリセット） */
    static constexpr int GEN_LIMIT = INT_MAX - 10;
    void bump_generation(int& g, vector<int>& stamps) {
        if (g >= GEN_LIMIT) {
            fill(stamps.begin(), stamps.end(), 0);
            g = 1;
        } else
            ++g;
    }

   public:
    BidirectionalBFS2D() = default;

    /*------------------------------------------------------------------
      bfs
      ------------------------------------------------------------------
      @tparam ROW 行の型（string / vector<char> などランダムアクセス可能なもの）
      @tparam FW  順方向の隣接セルを列挙するラムダ
      @tparam BK  逆方向の隣接セルを列挙するラムダ

      @param grid      2D グリッド：障害物 '#' ※テンプレートパラメータで指定
      @param s, t      始点・終点座標 (row, col)
      @param cands_fw  ラムダ式 [](Cord cur, vector<Cord> out) ※順方向の遷移先候補を out に設定する
      @param cands_bk  ラムダ式 [](Cord cur, vector<Cord> out) ※逆方向の遷移先候補を out に設定する

      @return 最短経路長（到達不能なら -1）
     ------------------------------------------------------------------*/
    template <char WALL = '#', class ROW, class FW, class BK>
    int bfs(const vector<ROW>& grid, Cord s, Cord t, const FW& cands_fw, const BK& cands_bk) {
        R = grid.size();
        if (R == 0) return -1;
        C = grid[0].size();
        const size_t N = R * C;
        ensure_capacity(N);
        bump_generation(cur_fw, stamp_fw);
        bump_generation(cur_bk, stamp_bk);
        const int gen_fw = cur_fw, gen_bk = cur_bk;

        has_path = false;
        meet = {-1, -1};

        const size_t sid = idx(s), tid = idx(t);
        if (s == t) {
            has_path = true;
            meet = s;
            stamp_fw[sid] = gen_fw;
            stamp_bk[tid] = gen_bk;
            dist_fw[sid] = dist_bk[tid] = 0;
            path_buf = {s};
            return 0;
        }

        deque<size_t> q_fw{sid}, q_bk{tid};
        stamp_fw[sid] = gen_fw;
        dist_fw[sid] = 0;
        prev_fw[sid] = -1;
        stamp_bk[tid] = gen_bk;
        dist_bk[tid] = 0;
        prev_bk[tid] = -1;

        auto in_bounds = [this](const Cord& p) -> bool {
            return static_cast<size_t>(p.first) < R && static_cast<size_t>(p.second) < C;
        };

        vector<Cord> nxts;
        while (!q_fw.empty() && !q_bk.empty()) {
            if (q_fw.size() <= q_bk.size()) {  // 小さい側から展開
                const size_t cur = q_fw.front();
                q_fw.pop_front();
                Cord cur_cell = cell(cur);
                
                nxts.clear();
                cands_fw(cur_cell, nxts);
                const int dcur = dist_fw[cur];
                for (const Cord& nb : nxts) {
                    if (!in_bounds(nb)) continue;
                    const size_t id = idx(nb);
                    if (stamp_fw[id] == gen_fw || grid[nb.first][nb.second] == WALL) continue;
                    stamp_fw[id] = gen_fw;
                    dist_fw[id] = dcur + 1;
                    prev_fw[id] = static_cast<int>(cur);
                    q_fw.push_back(id);
                    if (stamp_bk[id] == gen_bk) {
                        meet = nb;
                        has_path = true;
                        return dist_fw[id] + dist_bk[id];
                    }
                }
            } else {  // 逆方向側
                const size_t cur = q_bk.front();
                q_bk.pop_front();
                Cord cur_cell = cell(cur);
                
                nxts.clear();
                cands_bk(cur_cell, nxts);
                const int dcur = dist_bk[cur];
                for (const Cord& nb : nxts) {
                    if (!in_bounds(nb)) continue;
                    const size_t id = idx(nb);
                    if (stamp_bk[id] == gen_bk || grid[nb.first][nb.second] == WALL) continue;
                    stamp_bk[id] = gen_bk;
                    dist_bk[id] = dcur + 1;
                    prev_bk[id] = static_cast<int>(cur);
                    q_bk.push_back(id);
                    if (stamp_fw[id] == gen_fw) {
                        meet = nb;
                        has_path = true;
                        return dist_fw[id] + dist_bk[id];
                    }
                }
            }
        }
        return -1;  // 到達不能
    }

    /*------------------------------------------------------------------
      path
      ------------------------------------------------------------------
      前回 bfs() が見つけた経路を復元して返す（内部バッファを再利用）。
      @return 経路の座標列（始点→終点）。bfs が失敗した場合は空 vector。
     ------------------------------------------------------------------*/
    const vector<Cord>& path() {
        static const vector<Cord> empty;
        if (!has_path) return empty;

        path_buf.clear();
        const size_t mid = idx(meet);

        for (size_t cur = mid; cur != static_cast<size_t>(-1); cur = static_cast<size_t>(prev_fw[cur]))
            path_buf.push_back(cell(cur));
        reverse(path_buf.begin(), path_buf.end());

        for (size_t cur = static_cast<size_t>(prev_bk[mid]); cur != static_cast<size_t>(-1);
             cur = static_cast<size_t>(prev_bk[cur]))
            path_buf.push_back(cell(cur));

        return path_buf;
    }
};

/*======================================================================
  ----------------------  テスト & ベンチマーク ----------------------
  `#if 1 ... #endif` を 0 にするとビルド時にまとめて無効化できる。
 ======================================================================*/
#if 1

#include <chrono>
#include <iomanip>
#include <random>

/* 4 近傍列挙ラムダ（無向グラフなら fwd/bwd 共通で使う） */
static const auto neigh4 = [](const pair<int, int>& cur, vector<pair<int, int>>& out) noexcept {
    static const int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
    out.clear();
    for (int k = 0; k < 4; ++k) out.emplace_back(cur.first + dr[k], cur.second + dc[k]);
};

/* 片方向 BFS（正しさ検証用の簡易実装） */
template <class ROW, class FW>
int bfs_ref(const vector<ROW>& grid, pair<int, int> s, pair<int, int> t, const FW& cands) {
    const size_t R = grid.size(), C = grid[0].size();
    const size_t N = static_cast<size_t>(R) * C;
    auto idx = [C](int r, int c) { return static_cast<size_t>(r) * C + c; };
    vector<int> d(N, -1);
    queue<size_t> q;
    d[idx(s.first, s.second)] = 0;
    q.push(idx(s.first, s.second));
    auto inb = [R, C](int r, int c) { return 0 <= r && r < (int)R && 0 <= c && c < (int)C; };
    while (!q.empty()) {
        size_t cur = q.front();
        q.pop();
        int r = int(cur / C), c = int(cur % C);
        if (r == t.first && c == t.second) return d[cur];
        vector<pair<int, int>> nxt;
        cands({r, c}, nxt);
        for (auto& p : nxt) {
            if (!inb(p.first, p.second) || grid[p.first][p.second] == '#') continue;
            size_t id = idx(p.first, p.second);
            if (d[id] != -1) continue;
            d[id] = d[cur] + 1;
            q.push(id);
        }
    }
    return -1;
}

/* グリッド＋経路 ASCII 描画 */
template <class ROW>
void draw_path(const vector<ROW>& grid, const vector<pair<int, int>>& path, pair<int, int> s, pair<int, int> t) {
    vector<string> g;
    g.reserve(grid.size());
    for (auto& row : grid) g.emplace_back(row.begin(), row.end());
    for (auto p : path)
        if (p != s && p != t) g[p.first][p.second] = 'o';
    g[s.first][s.second] = 'S';
    g[t.first][t.second] = 'T';
    for (auto& row : g) cout << row << '\n';
}

/* 乱数ユーティリティ */
struct RNG {
    mt19937_64 eng;
    uniform_real_distribution<double> prob{0, 1};
    explicit RNG(uint64_t seed = 123456789ULL) : eng(seed) {}
    bool coin(double p) { return prob(eng) < p; }
    int ui(int l, int r) { return uniform_int_distribution<int>(l, r)(eng); }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    RNG rng(43);
    BidirectionalBFS2D solver;

    /*---- 1. エッジケース ----*/
    solver.bfs(vector<string>{"S"}, {0, 0}, {0, 0}, neigh4, neigh4);
    solver.bfs(vector<string>{"S#T"}, {0, 0}, {0, 2}, neigh4, neigh4);
    solver.bfs(vector<string>{"S..", "...", "..T"}, {0, 0}, {2, 2}, neigh4, neigh4);

    /*---- 2. ランダム検証 (200 ケース) ----*/
    for (int it = 0; it < 200; ++it) {
        int R = rng.ui(5, 40), C = rng.ui(5, 40);
        vector<string> g(R, string(C, '.'));
        for (int r = 0; r < R; ++r)
            for (int c = 0; c < C; ++c)
                if (rng.coin(0.25)) g[r][c] = '#';
        pair<int, int> s{rng.ui(0, R - 1), rng.ui(0, C - 1)}, t{rng.ui(0, R - 1), rng.ui(0, C - 1)};
        g[s.first][s.second] = '.';
        g[t.first][t.second] = '.';
        int d1 = solver.bfs(g, s, t, neigh4, neigh4);
        int d2 = bfs_ref(g, s, t, neigh4);
        if (d1 != d2) {
            cerr << "Mismatch\n";
            draw_path(g, solver.path(), s, t);
            return 1;
        }
    }
    cerr << "[OK] random tests passed\n";

    /*---- 3. 既存ベンチ (60x60, 300 回) ----*/
    const int BR = 60, BC = 60, BEN = 300;
    vector<string> bg(BR, string(BC, '.'));
    for (int r = 0; r < BR; ++r)
        for (int c = 0; c < BC; ++c)
            if (rng.coin(0.3)) bg[r][c] = '#';
    double ms = 0;
    long long len = 0;
    int cnt = 0;
    for (int it = 0; it < BEN; ++it) {
        pair<int, int> s{rng.ui(0, BR - 1), rng.ui(0, BC - 1)}, t{rng.ui(0, BR - 1), rng.ui(0, BC - 1)};
        bg[s.first][s.second] = '.';
        bg[t.first][t.second] = '.';
        auto st = chrono::high_resolution_clock::now();
        int l = solver.bfs(bg, s, t, neigh4, neigh4);
        ms += chrono::duration<double, milli>(chrono::high_resolution_clock::now() - st).count();
        if (l != -1) {
            len += l;
            ++cnt;
        }
    }
    cout << "Benchmark (60x60) : " << BEN << " queries\n";
    cout << "Avg time (biBFS)  : " << ms / BEN << " ms\n";
    cout << "Avg length        : " << (cnt ? static_cast<double>(len) / cnt : 0) << "\n";

    /*---- 4. サイズ別片方向 vs 双方向ベンチ ----*/
    cout << "\n=== One-way vs Two-way ===\n";
    for (int SZ : {10, 20, 50, 100, 200}) {
        vector<string> g(SZ, string(SZ, '.'));
        for (int r = 0; r < SZ; ++r)
            for (int c = 0; c < SZ; ++c)
                if (rng.coin(0.3)) g[r][c] = '#';
        double t_bi = 0, t_uni = 0;
        const int REP = 100;
        for (int i = 0; i < REP; ++i) {
            pair<int, int> s{rng.ui(0, SZ - 1), rng.ui(0, SZ - 1)}, t{rng.ui(0, SZ - 1), rng.ui(0, SZ - 1)};
            g[s.first][s.second] = '.';
            g[t.first][t.second] = '.';
            auto tb = chrono::high_resolution_clock::now();
            solver.bfs(g, s, t, neigh4, neigh4);
            t_bi += chrono::duration<double, micro>(chrono::high_resolution_clock::now() - tb).count();
            auto ub = chrono::high_resolution_clock::now();
            bfs_ref(g, s, t, neigh4);
            t_uni += chrono::duration<double, micro>(chrono::high_resolution_clock::now() - ub).count();
        }
        cout << "Grid " << setw(3) << SZ << "x" << setw(3) << SZ << " | one-way " << setw(8) << fixed << setprecision(3)
             << t_uni / REP << " µs"
             << " | two-way " << setw(8) << t_bi / REP << " µs"
             << " | ×" << setw(5) << setprecision(2) << t_uni / t_bi << "\n";
    }

    /*---- 5. サンプル可視化 ----*/
    {
        int R = 15, C = 30;
        vector<string> g(R, string(C, '.'));
        for (int r = 0; r < R; ++r)
            for (int c = 0; c < C; ++c)
                if (rng.coin(0.25)) g[r][c] = '#';
        pair<int, int> s{0, 0}, t{R - 1, C - 1};
        g[s.first][s.second] = '.';
        g[t.first][t.second] = '.';
        int l = solver.bfs(g, s, t, neigh4, neigh4);
        cout << "\n=== SAMPLE (" << R << "x" << C << ") length=" << l << " ===\n";
        draw_path(g, solver.path(), s, t);
    }
    return 0;
}
#endif
