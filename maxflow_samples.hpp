#pragma once
#include <bits/stdc++.h>
#include <atcoder/maxflow>
using namespace std;
using atcoder::mf_graph;

// ACL の mf_graph を利用して、競技プログラミングで頻出する最大流・最小カット変換を単体 solver として集約する
// 各 solver は独立して使えるよう、他 solver や共通 helper に依存しない形で実装する
// 基本的な使い方は「入力を追加する、solve/flow を一度呼ぶ、Result を見る」

// 最大二部マッチングを ACL maxflow で解く solver
//
// 使いどころ:
//   左集合 L と右集合 R の要素を、許可された組だけで 1 対 1 に対応させる最大数を求める
//   人-仕事、学生-研究室、駒-目的地、条件付き permutation、グリッド市松マッチングなど
//
// 使い方:
//   BipartiteMatching bm(L, R);
//   bm.add_edge(l, r);              // l と r をマッチ可能にする
//   auto res = bm.solve();          // 最大マッチングを求める
//
// 結果の見方:
//   res.size                 最大マッチングサイズ
//   res.left_to_right[l]     左 l の相手。未マッチなら -1
//   res.right_to_left[r]     右 r の相手。未マッチなら -1
//   res.pairs                採用された (l, r) の一覧
//   左側を全てマッチしたいなら res.size == L を確認する
struct BipartiteMatching {
    struct Result {
        int size = 0;
        vector<int> left_to_right;
        vector<int> right_to_left;
        vector<pair<int, int>> pairs;
    };

    int left_n, right_n;
    vector<pair<int, int>> edges;

    // 左頂点数 left_n と右頂点数 right_n で初期化する O(1)
    BipartiteMatching(int left_count, int right_count) : left_n(left_count), right_n(right_count) {}

    // 左 l と右 r の間にマッチ可能辺を追加する O(1)
    int add_edge(int l, int r) {
        assert(0 <= l && l < left_n);
        assert(0 <= r && r < right_n);
        edges.push_back({l, r});
        return (int)edges.size() - 1;
    }

    // 最大マッチングを求め、対応表とペア一覧を返す O(FE)
    Result solve() {
        int s = left_n + right_n;
        int t = s + 1;
        mf_graph<int> g(t + 1);

        // 左右の頂点を容量 1 で source/sink へ接続する
        for (int i = 0; i < left_n; i++) g.add_edge(s, i, 1);
        for (int j = 0; j < right_n; j++) g.add_edge(left_n + j, t, 1);

        // 入力された二部辺を容量 1 で張り、復元用に辺番号を保持する
        vector<int> edge_id(edges.size());
        for (int k = 0; k < (int)edges.size(); k++) {
            auto [l, r] = edges[k];
            edge_id[k] = g.add_edge(l, left_n + r, 1);
        }

        Result res;
        res.size = g.flow(s, t);
        res.left_to_right.assign(left_n, -1);
        res.right_to_left.assign(right_n, -1);

        // 流量 1 の二部辺が実際に採用されたマッチング辺になる
        for (int k = 0; k < (int)edges.size(); k++) {
            auto e = g.get_edge(edge_id[k]);
            if (e.flow == 1) {
                auto [l, r] = edges[k];
                res.left_to_right[l] = r;
                res.right_to_left[r] = l;
                res.pairs.push_back({l, r});
            }
        }
        return res;
    }
};

// 二部グラフ上の容量付き b-matching を ACL maxflow で解く solver
//
// 使いどころ:
//   左右の各頂点が複数単位を受け持てる割当を最大化する
//   例: 1 人が複数仕事を担当、各仕事に複数人必要、各曜日・各カテゴリに定員がある割当
//
// 使い方:
//   BipartiteBMatching bm(L, R);
//   bm.set_left_cap(l, cap);        // 左 l から出せる総量。初期値は 1
//   bm.set_right_cap(r, cap);       // 右 r が受け取れる総量。初期値は 1
//   bm.add_edge(l, r, cap);         // l から r へ最大 cap 流せる
//   auto res = bm.solve();
//
// 結果の見方:
//   res.flow            最大に割り当てられた総量
//   res.edge_flow[k]    k 番目に追加した候補辺の流量
//   res.used_edges      正の流量を持つ (l, r, flow) の一覧
//   「全需要を満たせたか」は呼び出し側で res.flow == 必要総量 を判定する
struct BipartiteBMatching {
    using Cap = long long;

    struct Edge {
        int l, r;
        Cap cap;
    };

    struct Result {
        Cap flow = 0;
        vector<Cap> edge_flow;
        vector<tuple<int, int, Cap>> used_edges;
    };

    int left_n, right_n;
    vector<Cap> left_cap, right_cap;
    vector<Edge> edges;

    // 左頂点数 left_n と右頂点数 right_n で、各頂点容量 1 として初期化する O(L+R)
    BipartiteBMatching(int left_count, int right_count)
        : left_n(left_count), right_n(right_count), left_cap(left_count, 1), right_cap(right_count, 1) {}

    // 左 l の使用可能量を cap に設定する O(1)
    void set_left_cap(int l, Cap cap) {
        assert(0 <= l && l < left_n);
        assert(0 <= cap);
        left_cap[l] = cap;
    }

    // 右 r の使用可能量を cap に設定する O(1)
    void set_right_cap(int r, Cap cap) {
        assert(0 <= r && r < right_n);
        assert(0 <= cap);
        right_cap[r] = cap;
    }

    // 左 l から右 r へ最大 cap だけ対応できる辺を追加する O(1)
    int add_edge(int l, int r, Cap cap = 1) {
        assert(0 <= l && l < left_n);
        assert(0 <= r && r < right_n);
        assert(0 <= cap);
        edges.push_back({l, r, cap});
        return (int)edges.size() - 1;
    }

    // 容量制約内で最大流量の b-matching を求める O(FE)
    Result solve() {
        int s = left_n + right_n;
        int t = s + 1;
        mf_graph<Cap> g(t + 1);

        // 左右の頂点容量を source/sink との辺容量で表す
        for (int i = 0; i < left_n; i++) g.add_edge(s, i, left_cap[i]);
        for (int j = 0; j < right_n; j++) g.add_edge(left_n + j, t, right_cap[j]);

        // 各候補辺にも容量を持たせ、復元用に辺番号を保持する
        vector<int> edge_id(edges.size());
        for (int k = 0; k < (int)edges.size(); k++) {
            const auto &e = edges[k];
            edge_id[k] = g.add_edge(e.l, left_n + e.r, e.cap);
        }

        Result res;
        res.flow = g.flow(s, t);
        res.edge_flow.assign(edges.size(), 0);

        // 正の流量を持つ候補辺だけを used_edges に入れる
        for (int k = 0; k < (int)edges.size(); k++) {
            auto e = g.get_edge(edge_id[k]);
            res.edge_flow[k] = e.flow;
            if (e.flow > 0) {
                res.used_edges.push_back({edges[k].l, edges[k].r, e.flow});
            }
        }
        return res;
    }
};

// 供給側から需要側への辺上限付き exact transportation を判定・復元する solver
//
// 使いどころ:
//   供給側の総量を、需要側の要求量へちょうど配れるかを判定する
//   倉庫-店舗、サーバー-クライアント、在庫配分、sparse な整数行列構成など
//
// 使い方:
//   TransportationFeasibility tr(S, D);
//   tr.set_supply(i, amount);       // 供給 i の供給量
//   tr.set_demand(j, amount);       // 需要 j の要求量
//   tr.add_edge(i, j, upper);       // i から j へ最大 upper 送れる
//   auto res = tr.solve();
//
// 結果の見方:
//   res == nullopt       総供給 != 総需要、または辺容量不足で実現不能
//   res->total           実現できた総流量
//   res->edge_flow[k]    k 番目に追加した供給-需要辺の流量
//   res->used_edges      正の流量を持つ (supply, demand, flow) の一覧
struct TransportationFeasibility {
    using Cap = long long;

    struct Edge {
        int supply, demand;
        Cap upper;
    };

    struct Result {
        Cap total = 0;
        vector<Cap> edge_flow;
        vector<tuple<int, int, Cap>> used_edges;
    };

    int supply_n, demand_n;
    vector<Cap> supply, demand;
    vector<Edge> edges;

    // 供給側 supply_n 個、需要側 demand_n 個で初期化する O(S+D)
    TransportationFeasibility(int supply_count, int demand_count)
        : supply_n(supply_count), demand_n(demand_count), supply(supply_count, 0), demand(demand_count, 0) {}

    // 供給 i の供給量を x に設定する O(1)
    void set_supply(int i, Cap x) {
        assert(0 <= i && i < supply_n);
        assert(0 <= x);
        supply[i] = x;
    }

    // 需要 j の需要量を x に設定する O(1)
    void set_demand(int j, Cap x) {
        assert(0 <= j && j < demand_n);
        assert(0 <= x);
        demand[j] = x;
    }

    // 供給 i から需要 j へ最大 upper だけ送れる辺を追加する O(1)
    int add_edge(int i, int j, Cap upper) {
        assert(0 <= i && i < supply_n);
        assert(0 <= j && j < demand_n);
        assert(0 <= upper);
        edges.push_back({i, j, upper});
        return (int)edges.size() - 1;
    }

    // 全供給と全需要をちょうど満たせるなら流量を復元する O(FE)
    optional<Result> solve() {
        Cap sum_supply = 0, sum_demand = 0;
        for (Cap x : supply) sum_supply += x;
        for (Cap x : demand) sum_demand += x;
        if (sum_supply != sum_demand) return nullopt;

        int s = supply_n + demand_n;
        int t = s + 1;
        mf_graph<Cap> g(t + 1);

        // source から供給、需要から sink へ必要量を容量として張る
        for (int i = 0; i < supply_n; i++) g.add_edge(s, i, supply[i]);
        for (int j = 0; j < demand_n; j++) g.add_edge(supply_n + j, t, demand[j]);

        // 供給から需要への候補辺を張る
        vector<int> edge_id(edges.size());
        for (int k = 0; k < (int)edges.size(); k++) {
            const auto &e = edges[k];
            edge_id[k] = g.add_edge(e.supply, supply_n + e.demand, e.upper);
        }

        Cap f = g.flow(s, t);
        if (f != sum_supply) return nullopt;

        Result res;
        res.total = f;
        res.edge_flow.assign(edges.size(), 0);
        for (int k = 0; k < (int)edges.size(); k++) {
            auto e = g.get_edge(edge_id[k]);
            res.edge_flow[k] = e.flow;
            if (e.flow > 0) {
                res.used_edges.push_back({edges[k].supply, edges[k].demand, e.flow});
            }
        }
        return res;
    }
};

// 行和・列和を満たす 0-1 行列を ACL maxflow で構成する solver
//
// 使いどころ:
//   各行の 1 の個数と各列の 1 の個数が決まっている 0-1 表を作る
//   禁止セル付きの選択表、参加可能日表、行列制約付きグリッド選択など
//
// 使い方:
//   BinaryMatrixBySums mat(row_sum, col_sum);
//   mat.add_cell(i, j);             // セル (i,j) を 1 にしてよい候補として追加
//   auto res = mat.solve();
//
// 注意:
//   add_cell したセルだけが 1 になれる。全セル許可なら全ての (i,j) を add_cell する
//   同じセルを複数回 add_cell しても候補は 1 本だけになる
//
// 結果の見方:
//   res == nullopt       条件を満たす 0-1 行列が存在しない
//   res->a[i][j]         構成された値。0 または 1
struct BinaryMatrixBySums {
    struct Result {
        vector<vector<int>> a;
    };

    int h, w;
    vector<int> row_sum, col_sum;
    vector<pair<int, int>> cells;
    vector<vector<char>> allowed;

    // 行和 row_sum と列和 col_sum で初期化する O(HW)
    BinaryMatrixBySums(vector<int> row_sum_input, vector<int> col_sum_input)
        : h((int)row_sum_input.size()), w((int)col_sum_input.size()), row_sum(row_sum_input), col_sum(col_sum_input), allowed(h, vector<char>(w, 0)) {}

    // 1 にしてよいセル (i,j) を追加する O(1)
    void add_cell(int i, int j) {
        assert(0 <= i && i < h);
        assert(0 <= j && j < w);
        if (!allowed[i][j]) {
            allowed[i][j] = 1;
            cells.push_back({i, j});
        }
    }

    // 条件を満たす 0-1 行列が存在すれば構成して返す O(FE)
    optional<Result> solve() {
        long long rs = 0, cs = 0;
        for (int x : row_sum) {
            assert(0 <= x);
            rs += x;
        }
        for (int x : col_sum) {
            assert(0 <= x);
            cs += x;
        }
        if (rs != cs) return nullopt;

        int s = h + w;
        int t = s + 1;
        mf_graph<int> g(t + 1);

        // 行と列を二部グラフにし、行和・列和を source/sink 辺で表す
        for (int i = 0; i < h; i++) g.add_edge(s, i, row_sum[i]);
        for (int j = 0; j < w; j++) g.add_edge(h + j, t, col_sum[j]);

        // 許可セルは容量 1 の辺にする
        vector<int> edge_id(cells.size());
        for (int k = 0; k < (int)cells.size(); k++) {
            auto [i, j] = cells[k];
            edge_id[k] = g.add_edge(i, h + j, 1);
        }

        int f = g.flow(s, t);
        if ((long long)f != rs) return nullopt;

        Result res;
        res.a.assign(h, vector<int>(w, 0));
        for (int k = 0; k < (int)cells.size(); k++) {
            auto e = g.get_edge(edge_id[k]);
            if (e.flow == 1) {
                auto [i, j] = cells[k];
                res.a[i][j] = 1;
            }
        }
        return res;
    }
};

// 行和・列和を満たす非負整数行列を ACL maxflow で構成する solver
//
// 使いどころ:
//   各行和・各列和を満たす非負整数行列を、セルごとの上限付きで構成する
//   輸送表、生産量割当、在庫配分、行列形式の供給需要配分など
//
// 使い方:
//   IntegerMatrixBySums mat(row_sum, col_sum);
//   mat.add_cell(i, j, upper);      // a[i][j] を 0..upper にできる
//   auto res = mat.solve();
//
// 注意:
//   add_cell したセルだけが正になれる
//   同じセルを複数回 add_cell すると upper は加算される
//
// 結果の見方:
//   res == nullopt       条件を満たす整数行列が存在しない
//   res->a[i][j]         構成された非負整数値
struct IntegerMatrixBySums {
    using Cap = long long;

    struct Edge {
        int i, j;
        Cap upper;
    };

    struct Result {
        vector<vector<Cap>> a;
    };

    int h, w;
    vector<Cap> row_sum, col_sum;
    vector<Edge> cells;
    vector<vector<int>> pos;

    // 行和 row_sum と列和 col_sum で初期化する O(HW)
    IntegerMatrixBySums(vector<Cap> row_sum_input, vector<Cap> col_sum_input)
        : h((int)row_sum_input.size()), w((int)col_sum_input.size()), row_sum(row_sum_input), col_sum(col_sum_input), pos(h, vector<int>(w, -1)) {}

    // セル (i,j) に最大 upper まで入れられるようにする O(1)
    void add_cell(int i, int j, Cap upper) {
        assert(0 <= i && i < h);
        assert(0 <= j && j < w);
        assert(0 <= upper);
        if (pos[i][j] == -1) {
            pos[i][j] = (int)cells.size();
            cells.push_back({i, j, upper});
        } else {
            cells[pos[i][j]].upper += upper;
        }
    }

    // 条件を満たす非負整数行列が存在すれば構成して返す O(FE)
    optional<Result> solve() {
        Cap rs = 0, cs = 0;
        for (Cap x : row_sum) {
            assert(0 <= x);
            rs += x;
        }
        for (Cap x : col_sum) {
            assert(0 <= x);
            cs += x;
        }
        if (rs != cs) return nullopt;

        int s = h + w;
        int t = s + 1;
        mf_graph<Cap> g(t + 1);

        // 行と列を二部グラフにし、行和・列和を source/sink 辺で表す
        for (int i = 0; i < h; i++) g.add_edge(s, i, row_sum[i]);
        for (int j = 0; j < w; j++) g.add_edge(h + j, t, col_sum[j]);

        // 各セル上限を行から列への容量として張る
        vector<int> edge_id(cells.size());
        for (int k = 0; k < (int)cells.size(); k++) {
            const auto &e = cells[k];
            edge_id[k] = g.add_edge(e.i, h + e.j, e.upper);
        }

        Cap f = g.flow(s, t);
        if (f != rs) return nullopt;

        Result res;
        res.a.assign(h, vector<Cap>(w, 0));
        for (int k = 0; k < (int)cells.size(); k++) {
            auto e = g.get_edge(edge_id[k]);
            res.a[cells[k].i][cells[k].j] = e.flow;
        }
        return res;
    }
};

// 障害物付きグリッド上で 1x2 ドミノの最大配置を求める solver
//
// 使いどころ:
//   空マスを上下左右に隣接する 2 マスずつペアにし、重ならないペア数を最大化する
//   最大ドミノ配置、隣接ペア最大化、空マスの最大被覆など
//
// 使い方:
//   GridDominoMatching gd(grid, '.');
//   auto res = gd.solve();
//
// 入力の意味:
//   grid[i][j] == empty のマスだけを使用可能マスとして扱う
//   empty 以外は障害物で、ドミノは置けない
//
// 結果の見方:
//   res.count     置けるドミノの最大枚数
//   res.pairs     各ドミノが覆う隣接 2 マスのペア一覧
//   出力用の矢印グリッドなどは res.pairs から必要に応じて作る
struct GridDominoMatching {
    struct Result {
        int count = 0;
        vector<pair<pair<int, int>, pair<int, int>>> pairs;
    };

    vector<string> grid;
    char empty;
    int h, w;

    // グリッドと空マス文字 empty で初期化する O(HW)
    GridDominoMatching(vector<string> grid_input, char empty_cell = '.')
        : grid(grid_input), empty(empty_cell), h((int)grid_input.size()), w(grid_input.empty() ? 0 : (int)grid_input[0].size()) {}

    // 最大枚数のドミノ配置を隣接ペアとして返す O(FE)
    Result solve() {
        int n = h * w;
        int s = n;
        int t = s + 1;
        mf_graph<int> g(t + 1);

        auto id = [&](int i, int j) { return i * w + j; };
        const int di[4] = {1, -1, 0, 0};
        const int dj[4] = {0, 0, 1, -1};

        // 市松模様で黒側を source、白側を sink に接続する
        for (int i = 0; i < h; i++) {
            assert((int)grid[i].size() == w);
            for (int j = 0; j < w; j++) {
                if (grid[i][j] != empty) continue;
                if (((i + j) & 1) == 0) g.add_edge(s, id(i, j), 1);
                else g.add_edge(id(i, j), t, 1);
            }
        }

        // 黒マスから隣接する白マスへ辺を張る
        vector<int> edge_id;
        vector<pair<pair<int, int>, pair<int, int>>> edge_cell;
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                if (grid[i][j] != empty || ((i + j) & 1)) continue;
                for (int dir = 0; dir < 4; dir++) {
                    int ni = i + di[dir], nj = j + dj[dir];
                    if (ni < 0 || ni >= h || nj < 0 || nj >= w) continue;
                    if (grid[ni][nj] != empty) continue;
                    edge_id.push_back(g.add_edge(id(i, j), id(ni, nj), 1));
                    edge_cell.push_back({{i, j}, {ni, nj}});
                }
            }
        }

        Result res;
        res.count = g.flow(s, t);
        for (int k = 0; k < (int)edge_id.size(); k++) {
            auto e = g.get_edge(edge_id[k]);
            if (e.flow == 1) res.pairs.push_back(edge_cell[k]);
        }
        return res;
    }
};

// 障害物付き盤面で互いに攻撃しないルークの最大配置を求める solver
//
// 使いどころ:
//   障害物で視線が遮られる盤面に、同じ横区間・縦区間を共有しないよう最大数のルークを置く
//   行列の行・列干渉、レーザー配置、障害物付き非攻撃配置など
//
// 使い方:
//   GridRookPlacement rp(grid, '.');
//   auto res = rp.solve();
//
// 入力の意味:
//   grid[i][j] == empty のマスにだけ置ける
//   empty 以外のマスは障害物で、ルークの攻撃線をそこで遮る
//
// 結果の見方:
//   res.count     置けるルークの最大個数
//   res.cells     ルークを置くマス一覧
struct GridRookPlacement {
    struct Result {
        int count = 0;
        vector<pair<int, int>> cells;
    };

    vector<string> grid;
    char empty;
    int h, w;

    // グリッドと空マス文字 empty で初期化する O(HW)
    GridRookPlacement(vector<string> grid_input, char empty_cell = '.')
        : grid(grid_input), empty(empty_cell), h((int)grid_input.size()), w(grid_input.empty() ? 0 : (int)grid_input[0].size()) {}

    // 攻撃し合わない最大数のルーク配置を返す O(FE+HW)
    Result solve() {
        vector<vector<int>> hid(h, vector<int>(w, -1));
        vector<vector<int>> vid(h, vector<int>(w, -1));
        int hn = 0, vn = 0;

        // 横方向の連続空白区間を番号付けする
        for (int i = 0; i < h; i++) {
            assert((int)grid[i].size() == w);
            int j = 0;
            while (j < w) {
                if (grid[i][j] != empty) {
                    j++;
                    continue;
                }
                int cur = hn++;
                while (j < w && grid[i][j] == empty) hid[i][j++] = cur;
            }
        }

        // 縦方向の連続空白区間を番号付けする
        for (int j = 0; j < w; j++) {
            int i = 0;
            while (i < h) {
                if (grid[i][j] != empty) {
                    i++;
                    continue;
                }
                int cur = vn++;
                while (i < h && grid[i][j] == empty) vid[i++][j] = cur;
            }
        }

        int s = hn + vn;
        int t = s + 1;
        mf_graph<int> g(t + 1);
        for (int i = 0; i < hn; i++) g.add_edge(s, i, 1);
        for (int j = 0; j < vn; j++) g.add_edge(hn + j, t, 1);

        // 各空マスは「横区間と縦区間の交点」として二部辺になる
        vector<int> edge_id;
        vector<pair<int, int>> edge_cell;
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                if (grid[i][j] != empty) continue;
                edge_id.push_back(g.add_edge(hid[i][j], hn + vid[i][j], 1));
                edge_cell.push_back({i, j});
            }
        }

        Result res;
        res.count = g.flow(s, t);
        for (int k = 0; k < (int)edge_id.size(); k++) {
            auto e = g.get_edge(edge_id[k]);
            if (e.flow == 1) res.cells.push_back(edge_cell[k]);
        }
        return res;
    }
};

// 障害物付き盤面で互いに攻撃しないビショップの最大配置を求める solver
//
// 使いどころ:
//   障害物で斜め視線が遮られる盤面に、同じ斜め区間を共有しないよう最大数のビショップを置く
//   2 種類の対角線制約を持つ配置、斜めレーザー配置など
//
// 使い方:
//   GridBishopPlacement bp(grid, '.');
//   auto res = bp.solve();
//
// 入力の意味:
//   grid[i][j] == empty のマスにだけ置ける
//   empty 以外のマスは障害物で、ビショップの斜め攻撃線をそこで遮る
//
// 結果の見方:
//   res.count     置けるビショップの最大個数
//   res.cells     ビショップを置くマス一覧
struct GridBishopPlacement {
    struct Result {
        int count = 0;
        vector<pair<int, int>> cells;
    };

    vector<string> grid;
    char empty;
    int h, w;

    // グリッドと空マス文字 empty で初期化する O(HW)
    GridBishopPlacement(vector<string> grid_input, char empty_cell = '.')
        : grid(grid_input), empty(empty_cell), h((int)grid_input.size()), w(grid_input.empty() ? 0 : (int)grid_input[0].size()) {}

    // 攻撃し合わない最大数のビショップ配置を返す O(FE+HW)
    Result solve() {
        vector<vector<int>> d1(h, vector<int>(w, -1));
        vector<vector<int>> d2(h, vector<int>(w, -1));
        int d1n = 0, d2n = 0;

        // 左上から右下へ伸びる対角線区間を番号付けする
        for (int sj0 = 0; sj0 < w; sj0++) {
            int i = 0, j = sj0;
            while (i < h && j < w) {
                if (grid[i][j] != empty) {
                    i++;
                    j++;
                    continue;
                }
                int cur = d1n++;
                while (i < h && j < w && grid[i][j] == empty) {
                    d1[i][j] = cur;
                    i++;
                    j++;
                }
            }
        }
        for (int si0 = 1; si0 < h; si0++) {
            int i = si0, j = 0;
            while (i < h && j < w) {
                if (grid[i][j] != empty) {
                    i++;
                    j++;
                    continue;
                }
                int cur = d1n++;
                while (i < h && j < w && grid[i][j] == empty) {
                    d1[i][j] = cur;
                    i++;
                    j++;
                }
            }
        }

        // 右上から左下へ伸びる対角線区間を番号付けする
        for (int sj0 = 0; sj0 < w; sj0++) {
            int i = 0, j = sj0;
            while (i < h && j >= 0) {
                if (grid[i][j] != empty) {
                    i++;
                    j--;
                    continue;
                }
                int cur = d2n++;
                while (i < h && j >= 0 && grid[i][j] == empty) {
                    d2[i][j] = cur;
                    i++;
                    j--;
                }
            }
        }
        for (int si0 = 1; si0 < h; si0++) {
            int i = si0, j = w - 1;
            while (i < h && j >= 0) {
                if (grid[i][j] != empty) {
                    i++;
                    j--;
                    continue;
                }
                int cur = d2n++;
                while (i < h && j >= 0 && grid[i][j] == empty) {
                    d2[i][j] = cur;
                    i++;
                    j--;
                }
            }
        }

        int s = d1n + d2n;
        int t = s + 1;
        mf_graph<int> g(t + 1);
        for (int i = 0; i < d1n; i++) g.add_edge(s, i, 1);
        for (int j = 0; j < d2n; j++) g.add_edge(d1n + j, t, 1);

        // 各空マスは 2 種類の対角線区間の交点として二部辺になる
        vector<int> edge_id;
        vector<pair<int, int>> edge_cell;
        for (int i = 0; i < h; i++) {
            assert((int)grid[i].size() == w);
            for (int j = 0; j < w; j++) {
                if (grid[i][j] != empty) continue;
                edge_id.push_back(g.add_edge(d1[i][j], d1n + d2[i][j], 1));
                edge_cell.push_back({i, j});
            }
        }

        Result res;
        res.count = g.flow(s, t);
        for (int k = 0; k < (int)edge_id.size(); k++) {
            auto e = g.get_edge(edge_id[k]);
            if (e.flow == 1) res.cells.push_back(edge_cell[k]);
        }
        return res;
    }
};

// s-t 最小カット値とカット辺を ACL maxflow で求める solver
//
// 使いどころ:
//   有向辺に「切るコスト」があるグラフで、s から t へ行けなくする最小コストを求める
//   道路封鎖、通信網切断、辺削除 mincut、source 側集合の復元など
//
// 使い方:
//   STMinCut mc(n);
//   int id = mc.add_edge(u, v, cost);       // 有向辺
//   mc.add_undirected_edge(u, v, cost);     // 無向容量辺は両向きに追加
//   auto res = mc.solve(s, t);
//
// 結果の見方:
//   res.value          s-t 最小カット値
//   res.side[v]        1 なら最大流後の残余グラフで s から到達可能、0 なら到達不能
//   res.cut_edges      元の有向辺番号のうち side[from]=1, side[to]=0 となる辺
//   add_undirected_edge は内部的に 2 本の有向辺を追加する点に注意
struct STMinCut {
    using Cap = long long;

    struct Edge {
        int from, to;
        Cap cap;
    };

    struct Result {
        Cap value = 0;
        vector<int> side;
        vector<int> cut_edges;
    };

    int n;
    vector<Edge> edges;

    // 頂点数 n で初期化する O(1)
    STMinCut(int vertex_count) : n(vertex_count) {}

    // 有向辺 u から v へ容量 cap で追加する O(1)
    int add_edge(int u, int v, Cap cap) {
        assert(0 <= u && u < n);
        assert(0 <= v && v < n);
        assert(0 <= cap);
        edges.push_back({u, v, cap});
        return (int)edges.size() - 1;
    }

    // 無向容量辺を両向きの有向辺として追加する O(1)
    void add_undirected_edge(int u, int v, Cap cap) {
        add_edge(u, v, cap);
        add_edge(v, u, cap);
    }

    // s-t 最小カット値、source 側、source 側から sink 側へ出る辺番号を返す O(FE)
    Result solve(int s, int t) {
        assert(0 <= s && s < n);
        assert(0 <= t && t < n);
        mf_graph<Cap> g(n);

        // 元辺をそのまま maxflow グラフへ張り、辺番号を保持する
        vector<int> edge_id(edges.size());
        for (int k = 0; k < (int)edges.size(); k++) {
            const auto &e = edges[k];
            edge_id[k] = g.add_edge(e.from, e.to, e.cap);
        }

        Result res;
        res.value = g.flow(s, t);
        auto cut = g.min_cut(s);
        res.side.assign(n, 0);
        for (int v = 0; v < n; v++) res.side[v] = cut[v] ? 1 : 0;

        // source 側から sink 側へ跨る元辺がカット辺になる
        for (int k = 0; k < (int)edges.size(); k++) {
            const auto &e = edges[k];
            if (e.cap > 0 && res.side[e.from] && !res.side[e.to]) res.cut_edges.push_back(k);
        }
        return res;
    }
};

// 頂点容量付き最大流を頂点分割で解く solver
//
// 使いどころ:
//   辺容量だけでなく、各頂点を通過できる流量にも上限がある最大流を解く
//   点素パス、各マスを高々 1 回だけ使う経路、中継点・部屋・駅の処理容量など
//
// 使い方:
//   VertexCapacityMaxflow vf(n, default_cap);
//   vf.set_vertex_cap(v, cap);              // 頂点 v の通過容量
//   vf.add_edge(u, v, cap);                 // u から v への有向辺容量
//   auto ans = vf.flow(s, t);
//
// 注意:
//   始点 s や終点 t の容量も通常の頂点と同じように制限される
//   s/t を制限したくない場合は set_vertex_cap(s, INF), set_vertex_cap(t, INF) を明示する
//   add_undirected_edge は両向きの有向辺を追加する
struct VertexCapacityMaxflow {
    using Cap = long long;

    struct Edge {
        int from, to;
        Cap cap;
    };

    int n;
    vector<Cap> vertex_cap;
    vector<Edge> edges;

    // 頂点数 n と全頂点の初期容量 default_vertex_cap で初期化する O(N)
    VertexCapacityMaxflow(int vertex_count, Cap default_vertex_cap) : n(vertex_count), vertex_cap(vertex_count, default_vertex_cap) {
        assert(0 <= default_vertex_cap);
    }

    // 頂点 v の通過容量を cap に設定する O(1)
    void set_vertex_cap(int v, Cap cap) {
        assert(0 <= v && v < n);
        assert(0 <= cap);
        vertex_cap[v] = cap;
    }

    // 有向辺 u から v へ容量 cap で追加する O(1)
    int add_edge(int u, int v, Cap cap) {
        assert(0 <= u && u < n);
        assert(0 <= v && v < n);
        assert(0 <= cap);
        edges.push_back({u, v, cap});
        return (int)edges.size() - 1;
    }

    // 無向辺を両向きの有向辺として追加する O(1)
    void add_undirected_edge(int u, int v, Cap cap) {
        add_edge(u, v, cap);
        add_edge(v, u, cap);
    }

    // 頂点容量を考慮した s から t への最大流を返す O(FE)
    Cap flow(int s, int t) {
        assert(0 <= s && s < n);
        assert(0 <= t && t < n);
        mf_graph<Cap> g(2 * n);

        // 各頂点 v を v_in -> v_out の容量辺で表す
        for (int v = 0; v < n; v++) {
            g.add_edge(2 * v, 2 * v + 1, vertex_cap[v]);
        }

        // 元グラフの辺 u -> v は u_out -> v_in に変換する
        for (const auto &e : edges) {
            g.add_edge(2 * e.from + 1, 2 * e.to, e.cap);
        }
        return g.flow(2 * s, 2 * t + 1);
    }
};

// 頂点削除コスト付き s-t 最小頂点カットを頂点分割で求める solver
//
// 使いどころ:
//   頂点を削除するコストを払い、sources 側から sinks 側へ到達不能にする最小コストを求める
//   点カット、都市・中継点・部屋の破壊、マス封鎖の一般グラフ版など
//
// 使い方:
//   VertexMinCut vc(n, INF);
//   vc.set_cost(v, cost);                   // 頂点 v を削除するコスト。初期値は 1
//   vc.add_edge(u, v);                      // u から v へ移動可能
//   auto res = vc.solve({s}, {t});          // 複数 source/sink も指定可能
//
// 注意:
//   solve に渡した sources と sinks は削除不可として扱われる
//   inf は「絶対に切りたくない辺・頂点」の容量なので、答えの上限より十分大きくする
//
// 結果の見方:
//   res.value               最小削除コスト
//   res.removed_vertices    削除する頂点一覧
//   res.side[v]             分断後の source 側なら 1
struct VertexMinCut {
    using Cap = long long;

    struct Result {
        Cap value = 0;
        vector<int> removed_vertices;
        vector<int> side;
    };

    int n;
    Cap inf;
    vector<Cap> cost;
    vector<pair<int, int>> edges;

    // 頂点数 n、無限大容量 inf、初期削除コスト 1 で初期化する O(N)
    VertexMinCut(int vertex_count, Cap inf_value) : n(vertex_count), inf(inf_value), cost(vertex_count, 1) {}

    // 頂点 v の削除コストを c に設定する O(1)
    void set_cost(int v, Cap c) {
        assert(0 <= v && v < n);
        assert(0 <= c);
        cost[v] = c;
    }

    // 有向辺 u から v への通行を追加する O(1)
    void add_edge(int u, int v) {
        assert(0 <= u && u < n);
        assert(0 <= v && v < n);
        edges.push_back({u, v});
    }

    // 無向辺を両向きの有向辺として追加する O(1)
    void add_undirected_edge(int u, int v) {
        add_edge(u, v);
        add_edge(v, u);
    }

    // sources と sinks を削除不可として最小頂点カットを返す O(FE)
    Result solve(vector<int> sources, vector<int> sinks) {
        vector<char> terminal(n, 0);
        for (int v : sources) {
            assert(0 <= v && v < n);
            terminal[v] = 1;
        }
        for (int v : sinks) {
            assert(0 <= v && v < n);
            terminal[v] = 1;
        }

        int ss = 2 * n;
        int tt = ss + 1;
        mf_graph<Cap> g(tt + 1);

        // 頂点削除は v_in -> v_out を切ることに対応する
        vector<int> vertex_edge_id(n);
        for (int v = 0; v < n; v++) {
            Cap c = terminal[v] ? inf : cost[v];
            vertex_edge_id[v] = g.add_edge(2 * v, 2 * v + 1, c);
        }

        // 元の通行辺は無限大容量で張り、切れない辺として扱う
        for (auto [u, v] : edges) {
            g.add_edge(2 * u + 1, 2 * v, inf);
        }
        for (int v : sources) g.add_edge(ss, 2 * v, inf);
        for (int v : sinks) g.add_edge(2 * v + 1, tt, inf);

        Result res;
        res.value = g.flow(ss, tt);
        auto cut = g.min_cut(ss);
        res.side.assign(n, 0);

        // v_in が source 側で v_out が sink 側なら、頂点 v を削除するカットになる
        for (int v = 0; v < n; v++) {
            res.side[v] = cut[2 * v] ? 1 : 0;
            if (!terminal[v] && cut[2 * v] && !cut[2 * v + 1]) {
                res.removed_vertices.push_back(v);
            }
        }
        return res;
    }
};

// グリッド上のマス削除コスト付き最小頂点カットを求める solver
//
// 使いどころ:
//   グリッド上でマスを塞ぐコストを払い、source マス群と sink マス群を分断する
//   壁配置、入口と出口の分離、敵味方の分断、複数入口・複数出口の封鎖など
//
// 使い方:
//   GridVertexMinCut gc(grid, cost, '.', INF);
//   gc.add_source(si, sj);
//   gc.add_sink(ti, tj);
//   auto res = gc.solve();
//
// 入力の意味:
//   grid[i][j] == empty のマスだけが通行可能で、削除対象にもなる
//   cost[i][j] はそのマスを塞ぐコスト
//   source/sink に指定したマスは削除不可として扱われる
//
// 結果の見方:
//   res.value           最小削除コスト
//   res.removed_cells   塞ぐマス一覧
//   res.side[i][j]      空マスなら source 側 1 / sink 側 0、障害物なら -1
struct GridVertexMinCut {
    using Cap = long long;

    struct Result {
        Cap value = 0;
        vector<pair<int, int>> removed_cells;
        vector<vector<int>> side;
    };

    vector<string> grid;
    vector<vector<Cap>> cost;
    char empty;
    Cap inf;
    int h, w;
    vector<pair<int, int>> sources, sinks;

    // グリッド、削除コスト、空マス文字 empty、無限大容量 inf で初期化する O(HW)
    GridVertexMinCut(vector<string> grid_input, vector<vector<Cap>> cost_input, char empty_cell = '.', Cap inf_value = (1LL << 60))
        : grid(grid_input), cost(cost_input), empty(empty_cell), inf(inf_value), h((int)grid_input.size()), w(grid_input.empty() ? 0 : (int)grid_input[0].size()) {}

    // source 側のマス (i,j) を追加する O(1)
    void add_source(int i, int j) {
        assert(0 <= i && i < h);
        assert(0 <= j && j < w);
        assert(grid[i][j] == empty);
        sources.push_back({i, j});
    }

    // sink 側のマス (i,j) を追加する O(1)
    void add_sink(int i, int j) {
        assert(0 <= i && i < h);
        assert(0 <= j && j < w);
        assert(grid[i][j] == empty);
        sinks.push_back({i, j});
    }

    // sources と sinks を分断する最小削除マス集合を返す O(FE+HW)
    Result solve() {
        int cell_n = h * w;
        int ss = 2 * cell_n;
        int tt = ss + 1;
        mf_graph<Cap> g(tt + 1);
        auto id = [&](int i, int j) { return i * w + j; };

        vector<char> terminal(cell_n, 0);
        for (auto [i, j] : sources) terminal[id(i, j)] = 1;
        for (auto [i, j] : sinks) terminal[id(i, j)] = 1;

        // 各空マスを in/out に分け、削除コストを in -> out の容量にする
        for (int i = 0; i < h; i++) {
            assert((int)grid[i].size() == w);
            assert((int)cost[i].size() == w);
            for (int j = 0; j < w; j++) {
                if (grid[i][j] != empty) continue;
                int v = id(i, j);
                Cap c = terminal[v] ? inf : cost[i][j];
                assert(0 <= c);
                g.add_edge(2 * v, 2 * v + 1, c);
            }
        }

        // 隣接空マス間は無限大容量の移動辺として張る
        const int di[4] = {1, -1, 0, 0};
        const int dj[4] = {0, 0, 1, -1};
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                if (grid[i][j] != empty) continue;
                int v = id(i, j);
                for (int dir = 0; dir < 4; dir++) {
                    int ni = i + di[dir], nj = j + dj[dir];
                    if (ni < 0 || ni >= h || nj < 0 || nj >= w) continue;
                    if (grid[ni][nj] != empty) continue;
                    int to = id(ni, nj);
                    g.add_edge(2 * v + 1, 2 * to, inf);
                }
            }
        }

        for (auto [i, j] : sources) g.add_edge(ss, 2 * id(i, j), inf);
        for (auto [i, j] : sinks) g.add_edge(2 * id(i, j) + 1, tt, inf);

        Result res;
        res.value = g.flow(ss, tt);
        auto cut = g.min_cut(ss);
        res.side.assign(h, vector<int>(w, -1));

        // in が source 側で out が sink 側の空マスが削除対象になる
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                if (grid[i][j] != empty) continue;
                int v = id(i, j);
                res.side[i][j] = cut[2 * v] ? 1 : 0;
                if (!terminal[v] && cut[2 * v] && !cut[2 * v + 1]) {
                    res.removed_cells.push_back({i, j});
                }
            }
        }
        return res;
    }
};

// 最大閉包問題を最小カットに変換して解く solver
//
// 使いどころ:
//   各要素に利益・損失があり、「u を選ぶなら v も選ぶ」という依存制約の下で利益最大集合を選ぶ
//   プロジェクト選択、前提クエスト、設備投資、依存機能、DAG 上の選択問題など
//
// 使い方:
//   MaxClosure cl(n);
//   cl.add_weight(v, w);                    // 選ぶと得る重み。負なら選ぶと損
//   cl.add_implication(u, v);               // u を選ぶなら v も必ず選ぶ
//   auto res = cl.solve();
//
// 結果の見方:
//   res.value           含意制約を満たす選択集合の最大重み和
//   res.selected        選ばれた頂点一覧
//   res.is_selected[v]  選ばれたら 1
//
// 注意:
//   内部 INF は重み絶対値総和 + 1。重み絶対値総和が大きすぎる場合は long long overflow に注意
struct MaxClosure {
    using Cap = long long;

    struct Result {
        Cap value = 0;
        vector<int> selected;
        vector<int> is_selected;
    };

    int n;
    vector<Cap> weight;
    vector<pair<int, int>> implications;

    // 頂点数 n で初期化する O(N)
    MaxClosure(int vertex_count) : n(vertex_count), weight(vertex_count, 0) {}

    // 頂点 v の重みに w を加算する O(1)
    void add_weight(int v, Cap w) {
        assert(0 <= v && v < n);
        weight[v] += w;
    }

    // u を選ぶなら v も選ぶという含意制約を追加する O(1)
    void add_implication(int u, int v) {
        assert(0 <= u && u < n);
        assert(0 <= v && v < n);
        implications.push_back({u, v});
    }

    // 含意制約を満たす重み最大の閉集合を返す O(FE)
    Result solve() {
        Cap positive_sum = 0;
        Cap abs_sum = 0;
        for (Cap w : weight) {
            if (w > 0) positive_sum += w;
            abs_sum += w >= 0 ? w : -w;
        }
        assert(abs_sum < (1LL << 60));
        Cap inf = abs_sum + 1;

        int s = n;
        int t = s + 1;
        mf_graph<Cap> g(t + 1);

        // 正の重みは source 側に置きたい利益、負の重みは選ぶと払う損失として表す
        for (int v = 0; v < n; v++) {
            if (weight[v] > 0) g.add_edge(s, v, weight[v]);
            else if (weight[v] < 0) g.add_edge(v, t, -weight[v]);
        }

        // 含意 u => v は u が source 側で v が sink 側になるカットを禁止する辺で表す
        for (auto [u, v] : implications) g.add_edge(u, v, inf);

        Cap cut_value = g.flow(s, t);
        auto cut = g.min_cut(s);

        Result res;
        res.value = positive_sum - cut_value;
        res.is_selected.assign(n, 0);
        for (int v = 0; v < n; v++) {
            if (cut[v]) {
                res.is_selected[v] = 1;
                res.selected.push_back(v);
            }
        }
        return res;
    }
};

// 二値ラベルの unary cost と単純な pairwise penalty を最小カットで解く solver
//
// 使いどころ:
//   各変数を 0/1 に分類し、単体コストと単純な関係ペナルティの合計を最小化する
//   2 色塗り、領域分割、同じグループにしたいペア、含意違反ペナルティ、強制固定など
//
// 使い方:
//   BinaryLabelingCut bl(n, INF);
//   bl.add_cost(v, cost0, cost1);           // v=0 / v=1 の単体コストを加算
//   bl.add_diff_cost(u, v, c);              // label[u] != label[v] のとき c
//   bl.add_penalty_10(u, v, c);             // u=1 かつ v=0 のとき c
//   bl.force0(v); bl.force1(v);             // ラベル固定
//   auto res = bl.solve();
//
// 結果の見方:
//   res.cost       最小コスト
//   res.label[v]   0 または 1。内部 mincut の source 側を 1 として返す
//
// 注意:
//   コストは非負前提
//   一般の任意 2 変数コストではなく、上記の単純な形だけを扱う
struct BinaryLabelingCut {
    using Cap = long long;

    struct DirectedPenalty {
        int from, to;
        Cap cost;
    };

    struct Result {
        Cap cost = 0;
        vector<int> label;
    };

    int n;
    Cap inf;
    vector<Cap> cost0, cost1;
    vector<DirectedPenalty> penalties;

    // 変数数 n と固定用の無限大容量 inf で初期化する O(N)
    BinaryLabelingCut(int vertex_count, Cap inf_value = (1LL << 60)) : n(vertex_count), inf(inf_value), cost0(vertex_count, 0), cost1(vertex_count, 0) {}

    // v=0 のコスト cost0_add、v=1 のコスト cost1_add を加算する O(1)
    void add_cost(int v, Cap cost0_add, Cap cost1_add) {
        assert(0 <= v && v < n);
        assert(0 <= cost0_add && 0 <= cost1_add);
        cost0[v] += cost0_add;
        cost1[v] += cost1_add;
    }

    // label[u] != label[v] のとき cost を払う無向差分ペナルティを追加する O(1)
    void add_diff_cost(int u, int v, Cap cost) {
        assert(0 <= u && u < n);
        assert(0 <= v && v < n);
        assert(0 <= cost);
        penalties.push_back({u, v, cost});
        penalties.push_back({v, u, cost});
    }

    // label[u]=1 かつ label[v]=0 のとき cost を払う有向ペナルティを追加する O(1)
    void add_penalty_10(int u, int v, Cap cost) {
        assert(0 <= u && u < n);
        assert(0 <= v && v < n);
        assert(0 <= cost);
        penalties.push_back({u, v, cost});
    }

    // v を 0 に固定する O(1)
    void force0(int v) {
        add_cost(v, 0, inf);
    }

    // v を 1 に固定する O(1)
    void force1(int v) {
        add_cost(v, inf, 0);
    }

    // 最小コストの 0/1 ラベルを返す O(FE)
    Result solve() {
        int s = n;
        int t = s + 1;
        mf_graph<Cap> g(t + 1);

        // S -> v は v=0 のコスト、v -> T は v=1 のコストとしてカットされる
        for (int v = 0; v < n; v++) {
            g.add_edge(s, v, cost0[v]);
            g.add_edge(v, t, cost1[v]);
        }

        // 有向辺 u -> v は label[u]=1, label[v]=0 の場合だけカットされる
        for (const auto &p : penalties) {
            g.add_edge(p.from, p.to, p.cost);
        }

        Result res;
        res.cost = g.flow(s, t);
        auto cut = g.min_cut(s);
        res.label.assign(n, 0);
        for (int v = 0; v < n; v++) res.label[v] = cut[v] ? 1 : 0;
        return res;
    }
};

// 二部グラフの最小重み頂点被覆と最大重み独立集合を最小カットで求める solver
//
// 使いどころ:
//   二部グラフの全ての辺を、端点の少なくとも一方で覆う最小重み集合を求める
//   また、その補集合として最大重み独立集合も同時に得る
//   衝突辺を全て潰す最小削除、行列の行・列被覆、二部グラフ上の競合しない最大価値選択など
//
// 使い方:
//   WeightedBipartiteVertexCover vc(L, R);
//   vc.set_left_weight(l, w);               // 左頂点の重み。初期値は 1
//   vc.set_right_weight(r, w);              // 右頂点の重み。初期値は 1
//   vc.add_edge(l, r);                      // 覆う必要がある二部辺
//   auto res = vc.solve();
//
// 結果の見方:
//   res.cover_weight          最小重み頂点被覆の重み
//   res.left_cover/right_cover        被覆に入る頂点
//   res.independent_weight    最大重み独立集合の重み
//   res.left_independent/right_independent  独立集合に入る頂点
//
// 注意:
//   入力グラフは二部グラフ前提。一般グラフの頂点被覆・独立集合には使えない
struct WeightedBipartiteVertexCover {
    using Cap = long long;

    struct Result {
        Cap cover_weight = 0;
        Cap independent_weight = 0;
        vector<int> left_cover;
        vector<int> right_cover;
        vector<int> left_independent;
        vector<int> right_independent;
    };

    int left_n, right_n;
    vector<Cap> left_weight, right_weight;
    vector<pair<int, int>> edges;

    // 左頂点数 left_n と右頂点数 right_n で、各重み 1 として初期化する O(L+R)
    WeightedBipartiteVertexCover(int left_count, int right_count)
        : left_n(left_count), right_n(right_count), left_weight(left_count, 1), right_weight(right_count, 1) {}

    // 左頂点 i の重みを w に設定する O(1)
    void set_left_weight(int i, Cap w) {
        assert(0 <= i && i < left_n);
        assert(0 <= w);
        left_weight[i] = w;
    }

    // 右頂点 j の重みを w に設定する O(1)
    void set_right_weight(int j, Cap w) {
        assert(0 <= j && j < right_n);
        assert(0 <= w);
        right_weight[j] = w;
    }

    // 覆う必要がある二部辺 l-r を追加する O(1)
    void add_edge(int l, int r) {
        assert(0 <= l && l < left_n);
        assert(0 <= r && r < right_n);
        edges.push_back({l, r});
    }

    // 最小重み頂点被覆とその補集合である最大重み独立集合を返す O(FE)
    Result solve() {
        Cap total = 0;
        for (Cap w : left_weight) total += w;
        for (Cap w : right_weight) total += w;
        assert(total < (1LL << 60));
        Cap inf = total + 1;

        int s = left_n + right_n;
        int t = s + 1;
        mf_graph<Cap> g(t + 1);

        // 左重みは S -> L、右重みは R -> T、二部辺は INF として張る
        for (int i = 0; i < left_n; i++) g.add_edge(s, i, left_weight[i]);
        for (int j = 0; j < right_n; j++) g.add_edge(left_n + j, t, right_weight[j]);
        for (auto [l, r] : edges) g.add_edge(l, left_n + r, inf);

        Result res;
        res.cover_weight = g.flow(s, t);
        res.independent_weight = total - res.cover_weight;
        auto cut = g.min_cut(s);

        // 最小頂点被覆は「左の非到達頂点」と「右の到達頂点」
        for (int i = 0; i < left_n; i++) {
            if (!cut[i]) res.left_cover.push_back(i);
            else res.left_independent.push_back(i);
        }
        for (int j = 0; j < right_n; j++) {
            if (cut[left_n + j]) res.right_cover.push_back(j);
            else res.right_independent.push_back(j);
        }
        return res;
    }
};

// 4 近傍グリッド上の最大重み独立集合を最小カットで求める solver
//
// 使いどころ:
//   上下左右に隣接する 2 マスを同時に選べない条件で、選ぶマスの重み和を最大化する
//   隣接不可なマス選択、4 近傍グリッドの最大独立集合、重みなし最大個数選択など
//
// 使い方:
//   GridWeightedIndependentSet is(grid, weight, '.');
//   auto res = is.solve();
//
// 入力の意味:
//   grid[i][j] == empty のマスだけが選択候補
//   weight[i][j] は非負重み。重みなしなら候補マスを 1 にする
//
// 結果の見方:
//   res.value          選んだマスの最大重み和
//   res.cells          選ばれたマス一覧
//   res.selected[i][j] 選ばれたら 1
//
// 注意:
//   4 近傍グリッドが二部グラフであることを利用している。斜め隣接制約は対象外
struct GridWeightedIndependentSet {
    using Cap = long long;

    struct Result {
        Cap value = 0;
        vector<pair<int, int>> cells;
        vector<vector<int>> selected;
    };

    vector<string> grid;
    vector<vector<Cap>> weight;
    char empty;
    int h, w;

    // グリッド、非負重み、空マス文字 empty で初期化する O(HW)
    GridWeightedIndependentSet(vector<string> grid_input, vector<vector<Cap>> weight_input, char empty_cell = '.')
        : grid(grid_input), weight(weight_input), empty(empty_cell), h((int)grid_input.size()), w(grid_input.empty() ? 0 : (int)grid_input[0].size()) {}

    // 4 近傍で隣接しない重み最大のマス集合を返す O(FE+HW)
    Result solve() {
        int n = h * w;
        int s = n;
        int t = s + 1;
        mf_graph<Cap> g(t + 1);
        auto id = [&](int i, int j) { return i * w + j; };
        Cap total = 0;

        // 市松模様の片側を left とし、頂点重みを source/sink 辺で表す
        for (int i = 0; i < h; i++) {
            assert((int)grid[i].size() == w);
            assert((int)weight[i].size() == w);
            for (int j = 0; j < w; j++) {
                if (grid[i][j] != empty) continue;
                assert(0 <= weight[i][j]);
                int v = id(i, j);
                total += weight[i][j];
                if (((i + j) & 1) == 0) g.add_edge(s, v, weight[i][j]);
                else g.add_edge(v, t, weight[i][j]);
            }
        }

        assert(total < (1LL << 60));
        Cap inf = total + 1;
        const int di[4] = {1, -1, 0, 0};
        const int dj[4] = {0, 0, 1, -1};

        // 隣接する 2 マスを同時に選ぶことを INF 辺で禁止する
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                if (grid[i][j] != empty || ((i + j) & 1)) continue;
                int v = id(i, j);
                for (int dir = 0; dir < 4; dir++) {
                    int ni = i + di[dir], nj = j + dj[dir];
                    if (ni < 0 || ni >= h || nj < 0 || nj >= w) continue;
                    if (grid[ni][nj] != empty) continue;
                    g.add_edge(v, id(ni, nj), inf);
                }
            }
        }

        Cap cover = g.flow(s, t);
        auto cut = g.min_cut(s);

        Result res;
        res.value = total - cover;
        res.selected.assign(h, vector<int>(w, 0));

        // 最大独立集合は「left の到達頂点」と「right の非到達頂点」
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                if (grid[i][j] != empty) continue;
                bool take = (((i + j) & 1) == 0) ? cut[id(i, j)] : !cut[id(i, j)];
                if (take) {
                    res.selected[i][j] = 1;
                    res.cells.push_back({i, j});
                }
            }
        }
        return res;
    }
};

// 下限付き feasible circulation を ACL maxflow への標準変換で判定する solver
//
// 使いどころ:
//   各辺に lower <= flow <= upper がある有向グラフで、全頂点の流量保存を満たす循環が存在するか判定する
//   下限付き割当、最低人数制約、lower/upper 付き表構成、feasible circulation など
//
// 使い方:
//   LowerBoundFlowFeasibility lb(n);
//   int id = lb.add_edge(u, v, lower, upper);
//   auto res = lb.solve();
//
// 結果の見方:
//   res == nullopt       条件を満たす circulation が存在しない
//   res->flow[k]         k 番目に追加した元辺の実際の流量
//
// 注意:
//   この solver は「全頂点で流量保存する循環」の feasible 判定に絞っている
//   下限付き s-t flow を判定したい場合は、呼び出し側で t -> s に十分大きい容量の辺を追加して循環にする
struct LowerBoundFlowFeasibility {
    using Cap = long long;

    struct Edge {
        int from, to;
        Cap lower, upper;
    };

    struct Result {
        vector<Cap> flow;
    };

    int n;
    vector<Edge> edges;

    // 頂点数 n で初期化する O(1)
    LowerBoundFlowFeasibility(int vertex_count) : n(vertex_count) {}

    // lower <= f <= upper を満たす有向辺 u から v を追加する O(1)
    int add_edge(int u, int v, Cap lower, Cap upper) {
        assert(0 <= u && u < n);
        assert(0 <= v && v < n);
        assert(0 <= lower && lower <= upper);
        edges.push_back({u, v, lower, upper});
        return (int)edges.size() - 1;
    }

    // 全頂点で流量保存を満たす circulation が存在すれば各元辺の流量を返す O(FE)
    optional<Result> solve() {
        int ss = n;
        int tt = ss + 1;
        mf_graph<Cap> g(tt + 1);
        vector<Cap> balance(n, 0);
        vector<int> edge_id(edges.size());

        // 下限分を先に流したものとして、残り容量 upper-lower をグラフへ張る
        for (int k = 0; k < (int)edges.size(); k++) {
            const auto &e = edges[k];
            edge_id[k] = g.add_edge(e.from, e.to, e.upper - e.lower);
            balance[e.from] -= e.lower;
            balance[e.to] += e.lower;
        }

        // 正の balance は追加で流入が必要、負の balance は追加で流出が必要
        Cap need = 0;
        for (int v = 0; v < n; v++) {
            if (balance[v] > 0) {
                g.add_edge(ss, v, balance[v]);
                need += balance[v];
            } else if (balance[v] < 0) {
                g.add_edge(v, tt, -balance[v]);
            }
        }

        if (g.flow(ss, tt) != need) return nullopt;

        Result res;
        res.flow.assign(edges.size(), 0);
        for (int k = 0; k < (int)edges.size(); k++) {
            auto e = g.get_edge(edge_id[k]);
            res.flow[k] = edges[k].lower + e.flow;
        }
        return res;
    }
};

// DAG の最小パス被覆を二部マッチングで求める solver
//
// 使いどころ:
//   DAG の全頂点を、頂点を重複しない有向パスの集合で覆うときの最小パス数を求める
//   順序制約付きタスク列への分解、半順序のチェーン分解、N - 最大マッチング型の典型など
//
// 使い方:
//   DagMinPathCover pc(n);
//   pc.add_edge(u, v);              // パス内で u の次に v を置ける有向辺
//   auto res = pc.solve();
//
// 結果の見方:
//   res.count       最小パス数
//   res.paths       復元されたパス一覧
//   res.next[v]     同じパスで v の次の頂点。なければ -1
//   res.prev[v]     同じパスで v の前の頂点。なければ -1
//
// 注意:
//   入力グラフは DAG 前提。必要なら呼び出し側でトポロジカル順や閉路なしを確認する
//   推移的に到達可能なら同じパスで繋いでよい問題では、必要に応じて到達可能辺を追加して使う
struct DagMinPathCover {
    struct Result {
        int count = 0;
        vector<vector<int>> paths;
        vector<int> next;
        vector<int> prev;
    };

    int n;
    vector<pair<int, int>> edges;

    // 頂点数 n で初期化する O(1)
    DagMinPathCover(int vertex_count) : n(vertex_count) {}

    // DAG の有向辺 u から v を追加する O(1)
    void add_edge(int u, int v) {
        assert(0 <= u && u < n);
        assert(0 <= v && v < n);
        edges.push_back({u, v});
    }

    // DAG の全頂点を覆う最小個数のパス列を返す O(FE)
    Result solve() {
        int s = 2 * n;
        int t = s + 1;
        mf_graph<int> g(t + 1);

        // 各頂点を left/right にコピーし、DAG 辺を left -> right の二部辺にする
        for (int i = 0; i < n; i++) {
            g.add_edge(s, i, 1);
            g.add_edge(n + i, t, 1);
        }
        vector<int> edge_id(edges.size());
        for (int k = 0; k < (int)edges.size(); k++) {
            auto [u, v] = edges[k];
            edge_id[k] = g.add_edge(u, n + v, 1);
        }

        int matching = g.flow(s, t);
        Result res;
        res.count = n - matching;
        res.next.assign(n, -1);
        res.prev.assign(n, -1);

        // マッチされた u -> v を同じパス内の次頂点として復元する
        for (int k = 0; k < (int)edges.size(); k++) {
            auto e = g.get_edge(edge_id[k]);
            if (e.flow == 1) {
                auto [u, v] = edges[k];
                res.next[u] = v;
                res.prev[v] = u;
            }
        }

        // prev がない頂点から next を辿ればパス被覆になる
        vector<int> seen(n, 0);
        for (int start = 0; start < n; start++) {
            if (res.prev[start] != -1) continue;
            vector<int> path;
            int v = start;
            while (v != -1) {
                path.push_back(v);
                seen[v] = 1;
                v = res.next[v];
            }
            res.paths.push_back(path);
        }
        return res;
    }
};

// 無向辺を向き付けて各頂点の入次数を指定値にする solver
//
// 使いどころ:
//   各無向辺の向きを決め、各頂点の入次数を指定値に一致させる
//   試合結果の勝者割当、各頂点が何本受け取るか決まっている向き付け、二択配分など
//
// 使い方:
//   EdgeOrientationIndegree eo(n);
//   int id = eo.add_edge(u, v);     // 無向辺 u-v
//   eo.set_indegree(v, deg);        // 頂点 v の目標入次数
//   auto res = eo.solve();
//
// 結果の見方:
//   res == nullopt                  条件を満たす向き付けが存在しない
//   res->directed_edges[k]          k 番目の元辺の向き。pair は (tail, head)
//
// 注意:
//   目標入次数の総和は辺数と一致している必要がある
//   自己ループは扱わない
struct EdgeOrientationIndegree {
    struct Result {
        vector<pair<int, int>> directed_edges;
    };

    int n;
    vector<pair<int, int>> edges;
    vector<int> indegree;

    // 頂点数 n で初期入次数 0 として初期化する O(N)
    EdgeOrientationIndegree(int vertex_count) : n(vertex_count), indegree(vertex_count, 0) {}

    // 無向辺 u-v を追加する O(1)
    int add_edge(int u, int v) {
        assert(0 <= u && u < n);
        assert(0 <= v && v < n);
        assert(u != v);
        edges.push_back({u, v});
        return (int)edges.size() - 1;
    }

    // 頂点 v の目標入次数を deg に設定する O(1)
    void set_indegree(int v, int deg) {
        assert(0 <= v && v < n);
        assert(0 <= deg);
        indegree[v] = deg;
    }

    // 入次数条件を満たす向き付けがあれば元辺順で返す O(FE)
    optional<Result> solve() {
        int m = (int)edges.size();
        int s = m + n;
        int t = s + 1;
        mf_graph<int> g(t + 1);

        int sum_deg = 0;
        for (int d : indegree) sum_deg += d;
        if (sum_deg != m) return nullopt;

        // 各辺を 1 単位のイベントノードにし、流れた先を head とする
        vector<int> to_u_id(m), to_v_id(m);
        for (int k = 0; k < m; k++) {
            auto [u, v] = edges[k];
            g.add_edge(s, k, 1);
            to_u_id[k] = g.add_edge(k, m + u, 1);
            to_v_id[k] = g.add_edge(k, m + v, 1);
        }
        for (int v = 0; v < n; v++) g.add_edge(m + v, t, indegree[v]);

        if (g.flow(s, t) != m) return nullopt;

        Result res;
        res.directed_edges.assign(m, {-1, -1});
        for (int k = 0; k < m; k++) {
            auto [u, v] = edges[k];
            auto eu = g.get_edge(to_u_id[k]);
            auto ev = g.get_edge(to_v_id[k]);
            if (eu.flow == 1) res.directed_edges[k] = {v, u};
            else if (ev.flow == 1) res.directed_edges[k] = {u, v};
            else return nullopt;
        }
        return res;
    }
};

// 各イベント量を 2 つの候補先へ容量内で分配する solver
//
// 使いどころ:
//   各イベントの量 amount を、2 つの候補先 first/second のどちらか、または両方へ容量内で分ける
//   残り試合の勝敗配分、二択の資源配分、容量付き向き付けの一般化など
//
// 使い方:
//   TwoChoiceDistribution td(B);
//   td.set_cap(b, cap);                     // 受け皿 b の容量
//   int id = td.add_choice(a, b, amount);   // amount を a/b へ分配可能
//   auto res = td.solve();
//
// 結果の見方:
//   res == nullopt        全イベント量を容量内で分配できない
//   res->total            分配した総量
//   res->to_first[k]      k 番目のイベントから first へ送った量
//   res->to_second[k]     k 番目のイベントから second へ送った量
//
// 注意:
//   amount > 1 のイベントは first と second に分割されうる
//   「完全にどちらか一方へ選ぶ」二択にしたい場合は amount=1 のイベントとして扱う
struct TwoChoiceDistribution {
    using Cap = long long;

    struct Choice {
        int first, second;
        Cap amount;
    };

    struct Result {
        Cap total = 0;
        vector<Cap> to_first;
        vector<Cap> to_second;
    };

    int bin_n;
    vector<Cap> cap;
    vector<Choice> choices;

    // 受け皿数 bin_n で各容量 0 として初期化する O(B)
    TwoChoiceDistribution(int bin_count) : bin_n(bin_count), cap(bin_count, 0) {}

    // 受け皿 bin の容量を c に設定する O(1)
    void set_cap(int bin, Cap c) {
        assert(0 <= bin && bin < bin_n);
        assert(0 <= c);
        cap[bin] = c;
    }

    // amount を first または second へ分配できるイベントを追加する O(1)
    int add_choice(int first, int second, Cap amount) {
        assert(0 <= first && first < bin_n);
        assert(0 <= second && second < bin_n);
        assert(first != second);
        assert(0 <= amount);
        choices.push_back({first, second, amount});
        return (int)choices.size() - 1;
    }

    // 全イベント量を容量内で分配できれば各イベントの分配量を返す O(FE)
    optional<Result> solve() {
        int m = (int)choices.size();
        int s = m + bin_n;
        int t = s + 1;
        mf_graph<Cap> g(t + 1);

        Cap total = 0;
        vector<int> first_id(m), second_id(m);

        // 各イベントから 2 つの候補先へ、最大 amount まで流せるようにする
        for (int k = 0; k < m; k++) {
            const auto &c = choices[k];
            total += c.amount;
            g.add_edge(s, k, c.amount);
            first_id[k] = g.add_edge(k, m + c.first, c.amount);
            second_id[k] = g.add_edge(k, m + c.second, c.amount);
        }
        for (int b = 0; b < bin_n; b++) g.add_edge(m + b, t, cap[b]);

        if (g.flow(s, t) != total) return nullopt;

        Result res;
        res.total = total;
        res.to_first.assign(m, 0);
        res.to_second.assign(m, 0);
        for (int k = 0; k < m; k++) {
            res.to_first[k] = g.get_edge(first_id[k]).flow;
            res.to_second[k] = g.get_edge(second_id[k]).flow;
        }
        return res;
    }
};

#if __INCLUDE_LEVEL__ == 0

static int brute_bipartite_matching(int l, int r, const vector<pair<int, int>> &edges) {
    vector<vector<int>> ok(l, vector<int>(r, 0));
    for (auto [a, b] : edges) ok[a][b] = 1;
    vector<int> used(r, 0);
    int best = 0;
    function<void(int, int)> dfs = [&](int i, int cur) {
        if (i == l) {
            best = max(best, cur);
            return;
        }
        dfs(i + 1, cur);
        for (int j = 0; j < r; j++) if (ok[i][j] && !used[j]) {
            used[j] = 1;
            dfs(i + 1, cur + 1);
            used[j] = 0;
        }
    };
    dfs(0, 0);
    return best;
}

static int brute_domino(vector<string> grid) {
    int h = (int)grid.size();
    int w = h ? (int)grid[0].size() : 0;
    int best = 0;
    function<void(int, int)> dfs = [&](int pos, int cur) {
        while (pos < h * w && grid[pos / w][pos % w] != '.') pos++;
        if (pos == h * w) {
            best = max(best, cur);
            return;
        }
        int i = pos / w, j = pos % w;
        grid[i][j] = '#';
        dfs(pos + 1, cur);
        const int di[2] = {1, 0};
        const int dj[2] = {0, 1};
        for (int d = 0; d < 2; d++) {
            int ni = i + di[d], nj = j + dj[d];
            if (ni < h && nj < w && grid[ni][nj] == '.') {
                grid[ni][nj] = '#';
                dfs(pos + 1, cur + 1);
                grid[ni][nj] = '.';
            }
        }
        grid[i][j] = '.';
    };
    dfs(0, 0);
    return best;
}

static int brute_rook(vector<string> grid) {
    int h = (int)grid.size();
    int w = h ? (int)grid[0].size() : 0;
    vector<pair<int, int>> cells;
    for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) if (grid[i][j] == '.') cells.push_back({i, j});
    int n = (int)cells.size(), best = 0;
    for (int mask = 0; mask < (1 << n); mask++) {
        bool ok = true;
        for (int a = 0; a < n && ok; a++) if (mask >> a & 1) {
            for (int b = a + 1; b < n && ok; b++) if (mask >> b & 1) {
                auto [i1, j1] = cells[a];
                auto [i2, j2] = cells[b];
                if (i1 == i2) {
                    bool block = false;
                    for (int j = min(j1, j2) + 1; j < max(j1, j2); j++) if (grid[i1][j] != '.') block = true;
                    if (!block) ok = false;
                }
                if (j1 == j2) {
                    bool block = false;
                    for (int i = min(i1, i2) + 1; i < max(i1, i2); i++) if (grid[i][j1] != '.') block = true;
                    if (!block) ok = false;
                }
            }
        }
        if (ok) best = max(best, __builtin_popcount((unsigned)mask));
    }
    return best;
}

static int brute_bishop(vector<string> grid) {
    int h = (int)grid.size();
    int w = h ? (int)grid[0].size() : 0;
    vector<pair<int, int>> cells;
    for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) if (grid[i][j] == '.') cells.push_back({i, j});
    int n = (int)cells.size(), best = 0;
    for (int mask = 0; mask < (1 << n); mask++) {
        bool ok = true;
        for (int a = 0; a < n && ok; a++) if (mask >> a & 1) {
            for (int b = a + 1; b < n && ok; b++) if (mask >> b & 1) {
                auto [i1, j1] = cells[a];
                auto [i2, j2] = cells[b];
                if (abs(i1 - i2) == abs(j1 - j2)) {
                    int di = (i2 > i1) ? 1 : -1;
                    int dj = (j2 > j1) ? 1 : -1;
                    bool block = false;
                    for (int i = i1 + di, j = j1 + dj; i != i2; i += di, j += dj) if (grid[i][j] != '.') block = true;
                    if (!block) ok = false;
                }
            }
        }
        if (ok) best = max(best, __builtin_popcount((unsigned)mask));
    }
    return best;
}

static long long brute_st_cut(int n, int s, int t, const vector<STMinCut::Edge> &edges) {
    long long best = (1LL << 60);
    for (int mask = 0; mask < (1 << n); mask++) {
        if (!(mask >> s & 1)) continue;
        if (mask >> t & 1) continue;
        long long val = 0;
        for (auto e : edges) if ((mask >> e.from & 1) && !(mask >> e.to & 1)) val += e.cap;
        best = min(best, val);
    }
    return best;
}

int main() {
    mt19937 rng(1);
    int passed = 0;
    auto rnd = [&rng](int mod) -> int {
        assert(0 < mod);
        return static_cast<int>(rng() % static_cast<unsigned int>(mod));
    };

    {
        for (int tc = 0; tc < 200; tc++) {
            int l = 1 + rnd(5), r = 1 + rnd(5);
            BipartiteMatching bm(l, r);
            vector<pair<int, int>> es;
            for (int i = 0; i < l; i++) for (int j = 0; j < r; j++) if (rnd(2)) {
                bm.add_edge(i, j);
                es.push_back({i, j});
            }
            auto res = bm.solve();
            assert(res.size == brute_bipartite_matching(l, r, es));
            vector<int> used_l(l, 0), used_r(r, 0);
            for (auto [a, b] : res.pairs) {
                assert(0 <= a && a < l && 0 <= b && b < r);
                assert(!used_l[a] && !used_r[b]);
                used_l[a] = used_r[b] = 1;
            }
        }
        cout << "BipartiteMatching ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 100; tc++) {
            int l = 1 + rnd(3), r = 1 + rnd(3);
            BipartiteBMatching bm(l, r);
            vector<int> lc(l), rc(r);
            for (int i = 0; i < l; i++) bm.set_left_cap(i, lc[i] = rnd(3));
            for (int j = 0; j < r; j++) bm.set_right_cap(j, rc[j] = rnd(3));
            struct BE { int a, b, c; };
            vector<BE> es;
            for (int i = 0; i < l; i++) for (int j = 0; j < r; j++) if (rnd(2)) {
                int c = rnd(3);
                bm.add_edge(i, j, c);
                es.push_back({i, j, c});
            }
            int m = (int)es.size();
            int best = 0;
            vector<int> lf(l), rf(r);
            function<void(int, int)> dfs = [&](int k, int cur) {
                if (k == m) {
                    best = max(best, cur);
                    return;
                }
                for (int x = 0; x <= es[k].c; x++) {
                    if (lf[es[k].a] + x <= lc[es[k].a] && rf[es[k].b] + x <= rc[es[k].b]) {
                        lf[es[k].a] += x;
                        rf[es[k].b] += x;
                        dfs(k + 1, cur + x);
                        lf[es[k].a] -= x;
                        rf[es[k].b] -= x;
                    }
                }
            };
            dfs(0, 0);
            auto res = bm.solve();
            assert(res.flow == best);
        }
        cout << "BipartiteBMatching ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 100; tc++) {
            int a = 1 + rnd(3), b = 1 + rnd(3);
            TransportationFeasibility tf(a, b);
            vector<int> sup(a), dem(b);
            for (int i = 0; i < a; i++) tf.set_supply(i, sup[i] = rnd(4));
            for (int j = 0; j < b; j++) tf.set_demand(j, dem[j] = rnd(4));
            struct E { int i, j, u; };
            vector<E> es;
            for (int i = 0; i < a; i++) for (int j = 0; j < b; j++) if (rnd(2)) {
                int u = rnd(4);
                tf.add_edge(i, j, u);
                es.push_back({i, j, u});
            }
            int m = (int)es.size();
            bool brute = false;
            vector<int> sf(a), df(b);
            function<void(int)> dfs = [&](int k) {
                if (brute) return;
                if (k == m) {
                    brute = (sf == sup && df == dem);
                    return;
                }
                for (int x = 0; x <= es[k].u; x++) {
                    sf[es[k].i] += x;
                    df[es[k].j] += x;
                    dfs(k + 1);
                    sf[es[k].i] -= x;
                    df[es[k].j] -= x;
                }
            };
            dfs(0);
            auto res = tf.solve();
            assert((bool)res == brute);
            if (res) {
                vector<long long> ss(a), dd(b);
                for (int k = 0; k < m; k++) {
                    assert(0 <= res->edge_flow[k] && res->edge_flow[k] <= es[k].u);
                    ss[es[k].i] += res->edge_flow[k];
                    dd[es[k].j] += res->edge_flow[k];
                }
                for (int i = 0; i < a; i++) assert(ss[i] == sup[i]);
                for (int j = 0; j < b; j++) assert(dd[j] == dem[j]);
            }
        }
        cout << "TransportationFeasibility ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 100; tc++) {
            int h = 1 + rnd(4), w = 1 + rnd(4);
            vector<int> rs(h), cs(w);
            for (int i = 0; i < h; i++) rs[i] = rnd((w + 1));
            for (int j = 0; j < w; j++) cs[j] = rnd((h + 1));
            BinaryMatrixBySums bm(rs, cs);
            vector<pair<int, int>> cells;
            for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) if (rnd(2)) {
                bm.add_cell(i, j);
                cells.push_back({i, j});
            }
            int m = (int)cells.size();
            bool brute = false;
            vector<int> rr(h), cc(w);
            function<void(int)> dfs = [&](int k) {
                if (brute) return;
                if (k == m) {
                    brute = (rr == rs && cc == cs);
                    return;
                }
                dfs(k + 1);
                auto [i, j] = cells[k];
                rr[i]++; cc[j]++;
                dfs(k + 1);
                rr[i]--; cc[j]--;
            };
            dfs(0);
            auto res = bm.solve();
            assert((bool)res == brute);
            if (res) {
                vector<int> rr2(h), cc2(w);
                for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) {
                    assert(res->a[i][j] == 0 || res->a[i][j] == 1);
                    rr2[i] += res->a[i][j];
                    cc2[j] += res->a[i][j];
                }
                assert(rr2 == rs && cc2 == cs);
            }
        }
        cout << "BinaryMatrixBySums ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 100; tc++) {
            int h = 1 + rnd(3), w = 1 + rnd(3);
            vector<long long> rs(h), cs(w);
            for (int i = 0; i < h; i++) rs[i] = rnd(4);
            for (int j = 0; j < w; j++) cs[j] = rnd(4);
            IntegerMatrixBySums im(rs, cs);
            struct E { int i, j, u; };
            vector<E> es;
            for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) if (rnd(2)) {
                int u = rnd(4);
                im.add_cell(i, j, u);
                es.push_back({i, j, u});
            }
            int m = (int)es.size();
            bool brute = false;
            vector<long long> rr(h), cc(w);
            function<void(int)> dfs = [&](int k) {
                if (brute) return;
                if (k == m) {
                    brute = (rr == rs && cc == cs);
                    return;
                }
                for (int x = 0; x <= es[k].u; x++) {
                    rr[es[k].i] += x;
                    cc[es[k].j] += x;
                    dfs(k + 1);
                    rr[es[k].i] -= x;
                    cc[es[k].j] -= x;
                }
            };
            dfs(0);
            auto res = im.solve();
            assert((bool)res == brute);
            if (res) {
                vector<long long> rr2(h), cc2(w);
                for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) {
                    rr2[i] += res->a[i][j];
                    cc2[j] += res->a[i][j];
                }
                assert(rr2 == rs && cc2 == cs);
            }
        }
        cout << "IntegerMatrixBySums ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 100; tc++) {
            int h = 1 + rnd(4), w = 1 + rnd(4);
            vector<string> grid(h, string(w, '.'));
            for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) if (rnd(4) == 0) grid[i][j] = '#';
            GridDominoMatching gd(grid);
            auto res = gd.solve();
            assert(res.count == brute_domino(grid));
            assert((int)res.pairs.size() == res.count);
        }
        cout << "GridDominoMatching ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 80; tc++) {
            int h = 1 + rnd(4), w = 1 + rnd(4);
            vector<string> grid(h, string(w, '.'));
            for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) if (rnd(4) == 0) grid[i][j] = '#';
            GridRookPlacement gr(grid);
            auto res = gr.solve();
            assert(res.count == brute_rook(grid));
        }
        cout << "GridRookPlacement ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 80; tc++) {
            int h = 1 + rnd(4), w = 1 + rnd(4);
            vector<string> grid(h, string(w, '.'));
            for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) if (rnd(4) == 0) grid[i][j] = '#';
            GridBishopPlacement gb(grid);
            auto res = gb.solve();
            assert(res.count == brute_bishop(grid));
        }
        cout << "GridBishopPlacement ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 150; tc++) {
            int n = 2 + rnd(5);
            int s = 0, t = n - 1;
            STMinCut mc(n);
            for (int e = 0; e < 8; e++) {
                int u = rnd(n), v = rnd(n);
                long long c = rnd(6);
                mc.add_edge(u, v, c);
            }
            auto res = mc.solve(s, t);
            assert(res.value == brute_st_cut(n, s, t, mc.edges));
        }
        cout << "STMinCut ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 100; tc++) {
            int n = 2 + rnd(5);
            vector<long long> vc(n);
            VertexCapacityMaxflow solver(n, 0);
            for (int i = 0; i < n; i++) {
                vc[i] = rnd(4);
                solver.set_vertex_cap(i, vc[i]);
            }
            struct E { int u, v; long long c; };
            vector<E> es;
            for (int e = 0; e < 8; e++) {
                int u = rnd(n), v = rnd(n);
                long long c = rnd(4);
                solver.add_edge(u, v, c);
                es.push_back({u, v, c});
            }
            int s = 0, t = n - 1;
            mf_graph<long long> raw(2 * n);
            for (int v = 0; v < n; v++) raw.add_edge(2 * v, 2 * v + 1, vc[v]);
            for (auto e : es) raw.add_edge(2 * e.u + 1, 2 * e.v, e.c);
            assert(solver.flow(s, t) == raw.flow(2 * s, 2 * t + 1));
        }
        cout << "VertexCapacityMaxflow ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 80; tc++) {
            int n = 2 + rnd(6);
            int s = 0, t = n - 1;
            VertexMinCut vm(n, 1000000000LL);
            vector<long long> cost(n);
            for (int i = 0; i < n; i++) {
                cost[i] = 1 + rnd(5);
                vm.set_cost(i, cost[i]);
            }
            vector<pair<int, int>> und;
            for (int e = 0; e < 8; e++) {
                int u = rnd(n), v = rnd(n);
                if (u == v) continue;
                vm.add_undirected_edge(u, v);
                und.push_back({u, v});
            }
            long long best = 1000000000LL;
            for (int mask = 0; mask < (1 << n); mask++) {
                if ((mask >> s & 1) || (mask >> t & 1)) continue;
                vector<int> seen(n, 0);
                queue<int> q;
                seen[s] = 1; q.push(s);
                while (!q.empty()) {
                    int v = q.front(); q.pop();
                    for (auto [a, b] : und) {
                        int to = -1;
                        if (a == v) to = b;
                        if (b == v) to = a;
                        if (to != -1 && !(mask >> to & 1) && !seen[to]) {
                            seen[to] = 1;
                            q.push(to);
                        }
                    }
                }
                if (!seen[t]) {
                    long long val = 0;
                    for (int i = 0; i < n; i++) if (mask >> i & 1) val += cost[i];
                    best = min(best, val);
                }
            }
            auto res = vm.solve({s}, {t});
            assert(res.value == best);
        }
        cout << "VertexMinCut ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 60; tc++) {
            int h = 2 + rnd(3), w = 2 + rnd(3);
            vector<string> grid(h, string(w, '.'));
            for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) if ((i || j) && (i != h - 1 || j != w - 1) && rnd(5) == 0) grid[i][j] = '#';
            if (grid[h - 1][w - 1] == '#') grid[h - 1][w - 1] = '.';
            vector<vector<long long>> cost(h, vector<long long>(w));
            for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) cost[i][j] = 1 + rnd(5);
            GridVertexMinCut gm(grid, cost, '.', 1000000000LL);
            gm.add_source(0, 0);
            gm.add_sink(h - 1, w - 1);
            long long best = 1000000000LL;
            int n = h * w;
            for (int mask = 0; mask < (1 << n); mask++) {
                if ((mask & 1) || (mask >> (n - 1) & 1)) continue;
                vector<vector<int>> seen(h, vector<int>(w));
                queue<pair<int, int>> q;
                if (grid[0][0] == '.') { seen[0][0] = 1; q.push({0, 0}); }
                const int di[4] = {1, -1, 0, 0};
                const int dj[4] = {0, 0, 1, -1};
                while (!q.empty()) {
                    auto [i, j] = q.front(); q.pop();
                    for (int d = 0; d < 4; d++) {
                        int ni = i + di[d], nj = j + dj[d];
                        if (ni < 0 || ni >= h || nj < 0 || nj >= w) continue;
                        int id = ni * w + nj;
                        if (grid[ni][nj] == '.' && !(mask >> id & 1) && !seen[ni][nj]) {
                            seen[ni][nj] = 1;
                            q.push({ni, nj});
                        }
                    }
                }
                if (!seen[h - 1][w - 1]) {
                    long long val = 0;
                    for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) if (mask >> (i * w + j) & 1) val += cost[i][j];
                    best = min(best, val);
                }
            }
            auto res = gm.solve();
            assert(res.value == best);
        }
        cout << "GridVertexMinCut ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 120; tc++) {
            int n = 1 + rnd(10);
            MaxClosure mc(n);
            vector<long long> w(n);
            for (int i = 0; i < n; i++) {
                w[i] = (int)(rnd(11)) - 5;
                mc.add_weight(i, w[i]);
            }
            vector<pair<int, int>> imp;
            for (int e = 0; e < 15; e++) {
                int u = rnd(n), v = rnd(n);
                if (rnd(3) == 0) {
                    mc.add_implication(u, v);
                    imp.push_back({u, v});
                }
            }
            long long best = LLONG_MIN;
            for (int mask = 0; mask < (1 << n); mask++) {
                bool ok = true;
                for (auto [u, v] : imp) if ((mask >> u & 1) && !(mask >> v & 1)) ok = false;
                if (!ok) continue;
                long long val = 0;
                for (int i = 0; i < n; i++) if (mask >> i & 1) val += w[i];
                best = max(best, val);
            }
            auto res = mc.solve();
            assert(res.value == best);
            for (auto [u, v] : imp) assert(!res.is_selected[u] || res.is_selected[v]);
        }
        cout << "MaxClosure ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 120; tc++) {
            int n = 1 + rnd(9);
            BinaryLabelingCut bl(n, 1000000000LL);
            vector<long long> c0(n), c1(n);
            struct P { int u, v; long long c; };
            vector<P> pen;
            for (int i = 0; i < n; i++) {
                long long a = rnd(5), b = rnd(5);
                c0[i] += a; c1[i] += b;
                bl.add_cost(i, a, b);
            }
            for (int e = 0; e < 12; e++) {
                int u = rnd(n), v = rnd(n);
                long long c = rnd(5);
                if (rnd(2)) {
                    bl.add_penalty_10(u, v, c);
                    pen.push_back({u, v, c});
                } else {
                    bl.add_diff_cost(u, v, c);
                    pen.push_back({u, v, c});
                    pen.push_back({v, u, c});
                }
            }
            if (n >= 2) {
                bl.force0(0); c1[0] += 1000000000LL;
                bl.force1(1); c0[1] += 1000000000LL;
            }
            long long best = (1LL << 60);
            for (int mask = 0; mask < (1 << n); mask++) {
                long long val = 0;
                for (int i = 0; i < n; i++) val += (mask >> i & 1) ? c1[i] : c0[i];
                for (auto p : pen) if ((mask >> p.u & 1) && !(mask >> p.v & 1)) val += p.c;
                best = min(best, val);
            }
            auto res = bl.solve();
            assert(res.cost == best);
        }
        cout << "BinaryLabelingCut ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 100; tc++) {
            int l = 1 + rnd(5), r = 1 + rnd(5);
            WeightedBipartiteVertexCover vc(l, r);
            vector<int> lw(l), rw(r);
            for (int i = 0; i < l; i++) vc.set_left_weight(i, lw[i] = rnd(6));
            for (int j = 0; j < r; j++) vc.set_right_weight(j, rw[j] = rnd(6));
            vector<pair<int, int>> es;
            for (int i = 0; i < l; i++) for (int j = 0; j < r; j++) if (rnd(2)) {
                vc.add_edge(i, j);
                es.push_back({i, j});
            }
            long long best = (1LL << 60);
            for (int lm = 0; lm < (1 << l); lm++) for (int rm = 0; rm < (1 << r); rm++) {
                bool ok = true;
                for (auto [a, b] : es) if (!(lm >> a & 1) && !(rm >> b & 1)) ok = false;
                if (!ok) continue;
                long long val = 0;
                for (int i = 0; i < l; i++) if (lm >> i & 1) val += lw[i];
                for (int j = 0; j < r; j++) if (rm >> j & 1) val += rw[j];
                best = min(best, val);
            }
            auto res = vc.solve();
            assert(res.cover_weight == best);
        }
        cout << "WeightedBipartiteVertexCover ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 70; tc++) {
            int h = 1 + rnd(4), w = 1 + rnd(4);
            vector<string> grid(h, string(w, '.'));
            vector<vector<long long>> weight(h, vector<long long>(w));
            for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) {
                if (rnd(5) == 0) grid[i][j] = '#';
                weight[i][j] = rnd(6);
            }
            GridWeightedIndependentSet gi(grid, weight);
            long long best = 0;
            int n = h * w;
            for (int mask = 0; mask < (1 << n); mask++) {
                bool ok = true;
                long long val = 0;
                for (int i = 0; i < h; i++) for (int j = 0; j < w; j++) if (mask >> (i * w + j) & 1) {
                    if (grid[i][j] != '.') ok = false;
                    val += weight[i][j];
                    if (i + 1 < h && (mask >> ((i + 1) * w + j) & 1)) ok = false;
                    if (j + 1 < w && (mask >> (i * w + j + 1) & 1)) ok = false;
                }
                if (ok) best = max(best, val);
            }
            auto res = gi.solve();
            assert(res.value == best);
        }
        cout << "GridWeightedIndependentSet ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 100; tc++) {
            int n = 1 + rnd(4);
            LowerBoundFlowFeasibility lb(n);
            struct E { int u, v, l, r; };
            vector<E> es;
            for (int e = 0; e < 6; e++) {
                int u = rnd(n), v = rnd(n);
                int l = rnd(3), r = l + (int)(rnd(3));
                lb.add_edge(u, v, l, r);
                es.push_back({u, v, l, r});
            }
            int m = (int)es.size();
            bool brute = false;
            vector<int> bal(n);
            function<void(int)> dfs = [&](int k) {
                if (brute) return;
                if (k == m) {
                    brute = true;
                    for (int x : bal) if (x != 0) brute = false;
                    return;
                }
                for (int f = es[k].l; f <= es[k].r; f++) {
                    bal[es[k].u] -= f;
                    bal[es[k].v] += f;
                    dfs(k + 1);
                    bal[es[k].u] += f;
                    bal[es[k].v] -= f;
                }
            };
            dfs(0);
            auto res = lb.solve();
            assert((bool)res == brute);
            if (res) {
                vector<long long> b(n);
                for (int k = 0; k < m; k++) {
                    assert(es[k].l <= res->flow[k] && res->flow[k] <= es[k].r);
                    b[es[k].u] -= res->flow[k];
                    b[es[k].v] += res->flow[k];
                }
                for (long long x : b) assert(x == 0);
            }
        }
        cout << "LowerBoundFlowFeasibility ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 100; tc++) {
            int n = 1 + rnd(8);
            DagMinPathCover pc(n);
            vector<pair<int, int>> es;
            for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (rnd(3) == 0) {
                pc.add_edge(i, j);
                es.push_back({i, j});
            }
            int match = brute_bipartite_matching(n, n, es);
            auto res = pc.solve();
            assert(res.count == n - match);
            vector<int> seen(n, 0);
            for (auto &p : res.paths) {
                for (int v : p) {
                    assert(0 <= v && v < n);
                    assert(!seen[v]);
                    seen[v] = 1;
                }
            }
            for (int v = 0; v < n; v++) assert(seen[v]);
        }
        cout << "DagMinPathCover ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 100; tc++) {
            int n = 2 + rnd(5);
            EdgeOrientationIndegree eo(n);
            vector<pair<int, int>> es;
            int m = rnd(7);
            for (int k = 0; k < m; k++) {
                int u = rnd(n), v = rnd(n);
                if (u == v) v = (v + 1) % n;
                eo.add_edge(u, v);
                es.push_back({u, v});
            }
            vector<int> deg(n);
            for (int v = 0; v < n; v++) {
                deg[v] = rnd((m + 1));
                eo.set_indegree(v, deg[v]);
            }
            bool brute = false;
            for (int mask = 0; mask < (1 << m); mask++) {
                vector<int> d(n);
                for (int k = 0; k < m; k++) {
                    auto [u, v] = es[k];
                    if (mask >> k & 1) d[u]++;
                    else d[v]++;
                }
                if (d == deg) brute = true;
            }
            auto res = eo.solve();
            assert((bool)res == brute);
            if (res) {
                vector<int> d(n);
                for (auto [u, v] : res->directed_edges) d[v]++;
                assert(d == deg);
            }
        }
        cout << "EdgeOrientationIndegree ok\n";
        passed++;
    }

    {
        for (int tc = 0; tc < 100; tc++) {
            int b = 2 + rnd(3);
            TwoChoiceDistribution td(b);
            vector<int> cap(b);
            for (int i = 0; i < b; i++) td.set_cap(i, cap[i] = rnd(6));
            struct C { int a, b, x; };
            vector<C> cs;
            int m = rnd(6);
            for (int k = 0; k < m; k++) {
                int a = rnd(b), c = rnd(b);
                if (a == c) c = (c + 1) % b;
                int x = rnd(4);
                td.add_choice(a, c, x);
                cs.push_back({a, c, x});
            }
            bool brute = false;
            vector<int> use(b);
            function<void(int)> dfs = [&](int k) {
                if (brute) return;
                if (k == m) {
                    brute = true;
                    for (int i = 0; i < b; i++) if (use[i] > cap[i]) brute = false;
                    return;
                }
                for (int x = 0; x <= cs[k].x; x++) {
                    use[cs[k].a] += x;
                    use[cs[k].b] += cs[k].x - x;
                    dfs(k + 1);
                    use[cs[k].a] -= x;
                    use[cs[k].b] -= cs[k].x - x;
                }
            };
            dfs(0);
            auto res = td.solve();
            assert((bool)res == brute);
            if (res) {
                vector<long long> use2(b);
                for (int k = 0; k < m; k++) {
                    assert(res->to_first[k] + res->to_second[k] == cs[k].x);
                    use2[cs[k].a] += res->to_first[k];
                    use2[cs[k].b] += res->to_second[k];
                }
                for (int i = 0; i < b; i++) assert(use2[i] <= cap[i]);
            }
        }
        cout << "TwoChoiceDistribution ok\n";
        passed++;
    }

    cout << "All tests passed: " << passed << " solver groups\n";
    return 0;
}
#endif
