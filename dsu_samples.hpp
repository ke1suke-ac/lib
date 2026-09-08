#pragma once

#include <bits/stdc++.h>
#include <atcoder/dsu>

using namespace std;

// atcoder::dsu を利用した典型用途向け solver 集
// 無向連結性、成分サイズ寄与、Kruskal、offline しきい値、逆順削除、グリッド連結、二値制約などを扱う
// 各 solver は単体でコピペしやすいよう、共通 helper に依存せず独立している
// 頂点番号はすべて 0-indexed とする

// ============================================================
// DsuOnlineConnectivity
// ============================================================
// 【用途】辺追加だけの無向グラフで、連結性・成分数・最大成分サイズを管理する基本 solver
// 【典型】辺追加クエリ、同じグループかの判定、全体が連結になったかの判定、閉路を作る辺の検出
// 【使い方】DsuOnlineConnectivity g(n); と作り、g.merge(u,v) で辺追加、g.same(u,v) で連結判定を行う
// 【取得】g.components が現在の連結成分数、g.max_size が最大成分サイズ。merge は実際に併合したとき true
// 【注意】頂点番号は 0-indexed。削除・undo・有向到達性は扱わない
struct DsuOnlineConnectivity {
    atcoder::dsu uf;
    int components;
    int max_size;

    // n 頂点で初期化する O(N)
    DsuOnlineConnectivity(int n) : uf(n), components(n), max_size(n > 0 ? 1 : 0) {}

    // 辺を追加し、異なる成分を併合したかを返す ならし O(α(N))
    bool merge(int u, int v) {
        if (uf.same(u, v)) return false;
        int r = uf.merge(u, v);
        components--;
        max_size = max(max_size, uf.size(r));
        return true;
    }

    // u と v が同じ成分かを返す ならし O(α(N))
    bool same(int u, int v) {
        return uf.same(u, v);
    }

    // v を含む成分の代表元を返す ならし O(α(N))
    int leader(int v) {
        return uf.leader(v);
    }

    // v を含む成分の頂点数を返す ならし O(α(N))
    int size(int v) {
        return uf.size(v);
    }

    // 全頂点が 1 つの成分なら true を返す O(1)
    bool connected_all() const {
        return components <= 1;
    }
};

// ============================================================
// DsuComponentPairCounter
// ============================================================
// 【用途】辺追加に伴う連結ペア数 connected_pairs と非連結ペア数 disconnected_pairs を管理する solver
// 【典型】道路追加後の不便さ、連結ペア数のスコア、成分併合時の size(a)*size(b) 寄与計算
// 【使い方】DsuComponentPairCounter g(n); と作り、辺追加ごとに g.merge(u,v) を呼ぶ
// 【取得】g.connected_pairs は同じ成分に属する unordered な頂点ペア数、g.disconnected_pairs は異なる成分のペア数
// 【注意】同じ成分内の辺を追加してもペア数は変わらず、merge は false を返す
struct DsuComponentPairCounter {
    atcoder::dsu uf;
    long long connected_pairs;
    long long disconnected_pairs;

    // n 頂点で初期化する O(N)
    DsuComponentPairCounter(int n)
        : uf(n), connected_pairs(0), disconnected_pairs(1LL * n * (n - 1) / 2) {}

    // 辺を追加し、連結ペア数と非連結ペア数を更新して、併合したかを返す ならし O(α(N))
    bool merge(int u, int v) {
        if (uf.same(u, v)) return false;
        long long delta = 1LL * uf.size(u) * uf.size(v);
        uf.merge(u, v);
        connected_pairs += delta;
        disconnected_pairs -= delta;
        return true;
    }

    // u と v が同じ成分かを返す ならし O(α(N))
    bool same(int u, int v) {
        return uf.same(u, v);
    }

    // v を含む成分の頂点数を返す ならし O(α(N))
    int size(int v) {
        return uf.size(v);
    }
};

// ============================================================
// DsuComponentEdgeInfo
// ============================================================
// 【用途】無向グラフの各連結成分について、頂点数に加えて成分内の辺数も管理する solver
// 【典型】木判定、森判定、閉路検出、余分な辺数、完全グラフにするための不足辺数の計算
// 【使い方】DsuComponentEdgeInfo g(n); と作り、実際の入力辺ごとに g.add_edge(u,v) を呼ぶ
// 【取得】g.size(v) が頂点数、g.edges(v) が辺数、g.is_tree(v) や g.extra_edges(v) で成分の性質を取得
// 【注意】add_edge は多重辺・自己ループも 1 本の辺として数える。addable_edges_inside は単純無向グラフ前提
struct DsuComponentEdgeInfo {
    atcoder::dsu uf;
    vector<long long> edge_count;
    int components;

    // n 頂点で初期化する O(N)
    DsuComponentEdgeInfo(int n) : uf(n), edge_count(n, 0), components(n) {}

    // 辺を 1 本追加し、異なる成分を併合したかを返す ならし O(α(N))
    bool add_edge(int u, int v) {
        int ru = uf.leader(u);
        int rv = uf.leader(v);

        // 同じ成分内の辺は、その成分の辺数だけを増やす
        if (ru == rv) {
            edge_count[ru]++;
            return false;
        }

        // 異なる成分を結ぶ辺は、両成分の辺数と追加辺をまとめて新代表へ移す
        long long merged_edges = edge_count[ru] + edge_count[rv] + 1;
        int r = uf.merge(ru, rv);
        edge_count[r] = merged_edges;
        components--;
        return true;
    }

    // u と v が同じ成分かを返す ならし O(α(N))
    bool same(int u, int v) {
        return uf.same(u, v);
    }

    // v を含む成分の代表元を返す ならし O(α(N))
    int leader(int v) {
        return uf.leader(v);
    }

    // v を含む成分の頂点数を返す ならし O(α(N))
    int size(int v) {
        return uf.size(v);
    }

    // v を含む成分の辺数を返す ならし O(α(N))
    long long edges(int v) {
        return edge_count[uf.leader(v)];
    }

    // v を含む成分が木なら true を返す ならし O(α(N))
    bool is_tree(int v) {
        return edges(v) == size(v) - 1;
    }

    // v を含む成分が辺数=頂点数の連結成分なら true を返す ならし O(α(N))
    bool is_unicyclic(int v) {
        return edges(v) == size(v);
    }

    // v を含む成分の余分な辺数 e-v+1 を返す ならし O(α(N))
    long long extra_edges(int v) {
        return edges(v) - size(v) + 1;
    }

    // 単純無向グラフとして成分内にさらに追加できる辺数を返す ならし O(α(N))
    long long addable_edges_inside(int v) {
        long long s = size(v);
        return s * (s - 1) / 2 - edges(v);
    }
};

// ============================================================
// DsuComponentSum
// ============================================================
// 【用途】各頂点に初期値があるとき、連結成分ごとの合計値を merge と同時に管理する solver
// 【典型】友達グループの能力値合計、島の資源量合計、成分スコアの合算
// 【使い方】DsuComponentSum g(a); と初期値配列で作り、g.merge(u,v) 後に g.get(v) で成分合計を得る
// 【取得】g.get(v) は v を含む成分の合計値、g.size(v) は成分サイズ
// 【注意】sum 専用の最小実装。min/max/xor/or が欲しい場合はこの struct をコピーして更新式だけ変える
struct DsuComponentSum {
    atcoder::dsu uf;
    vector<long long> sum;

    // 初期値配列 a で初期化する O(N)
    DsuComponentSum(const vector<long long>& a) : uf((int)a.size()), sum(a) {}

    // 成分を併合し、合計値を新代表へ移して、併合したかを返す ならし O(α(N))
    bool merge(int u, int v) {
        int ru = uf.leader(u);
        int rv = uf.leader(v);
        if (ru == rv) return false;
        long long merged_sum = sum[ru] + sum[rv];
        int r = uf.merge(ru, rv);
        sum[r] = merged_sum;
        return true;
    }

    // v を含む成分の合計値を返す ならし O(α(N))
    long long get(int v) {
        return sum[uf.leader(v)];
    }

    // v を含む成分の頂点数を返す ならし O(α(N))
    int size(int v) {
        return uf.size(v);
    }

    // v を含む成分の代表元を返す ならし O(α(N))
    int leader(int v) {
        return uf.leader(v);
    }
};

// ============================================================
// DsuKruskalMst
// ============================================================
// 【用途】重み付き無向グラフから Kruskal 法で最小全域森、連結なら最小全域木を作る solver
// 【典型】MST のコスト、採用辺 id、グラフを連結にする最小コスト、サイクルを作らない辺選択
// 【使い方】Edge{u,v,w,id} の vector を用意し、DsuKruskalMst::solve(n, edges) を呼ぶ
// 【取得】Result.cost が採用辺重み合計、used_edge_ids が採用辺 id、connected が全頂点連結か
// 【注意】非連結グラフでは最小全域森を返す。最大全域木は重みを反転するか比較関数を降順に変える
struct DsuKruskalMst {
    struct Edge {
        int u, v;
        long long w;
        int id;
    };

    struct Result {
        long long cost;
        vector<int> used_edge_ids;
        int components;
        bool connected;
    };

    // Kruskal 法で最小全域森を作る O(M log M)
    static Result solve(int n, vector<Edge> edges) {
        sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
            if (a.w != b.w) return a.w < b.w;
            return a.id < b.id;
        });

        atcoder::dsu uf(n);
        long long cost = 0;
        vector<int> used;
        int components = n;

        // 軽い辺から順に、閉路を作らない辺だけを採用する
        for (const Edge& e : edges) {
            if (uf.same(e.u, e.v)) continue;
            uf.merge(e.u, e.v);
            cost += e.w;
            used.push_back(e.id);
            components--;
        }

        return {cost, used, components, components <= 1};
    }
};

// ============================================================
// DsuKruskalPairContribution
// ============================================================
// 【用途】辺を重み順に追加し、併合時の size(u)*size(v) を重み付きで足すペア寄与 solver
// 【典型】木上の全頂点ペアについて、パス最大辺重みの総和やパス最小辺重みの総和を求める
// 【使い方】Edge{u,v,w} を渡し、最大値寄与なら sum_path_max、最小値寄与なら sum_path_min を呼ぶ
// 【取得】戻り値は merge された全ペアに対する w*ペア数 の合計
// 【注意】主に木で使う。一般グラフでは Kruskal で実際に併合されたペアへの寄与として動く
struct DsuKruskalPairContribution {
    struct Edge {
        int u, v;
        long long w;
    };

    // 辺を昇順に追加し、各併合辺をパス最大値として寄与させる O(M log M)
    static long long sum_path_max(int n, vector<Edge> edges) {
        sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
            return a.w < b.w;
        });

        atcoder::dsu uf(n);
        long long ans = 0;

        // u 側成分と v 側成分の全ペアが、この辺で初めて連結になる
        for (const Edge& e : edges) {
            if (uf.same(e.u, e.v)) continue;
            ans += e.w * 1LL * uf.size(e.u) * uf.size(e.v);
            uf.merge(e.u, e.v);
        }
        return ans;
    }

    // 辺を降順に追加し、各併合辺をパス最小値として寄与させる O(M log M)
    static long long sum_path_min(int n, vector<Edge> edges) {
        sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
            return a.w > b.w;
        });

        atcoder::dsu uf(n);
        long long ans = 0;

        // 降順に見ると、初めて連結になるペアに対してこの辺がパス最小値になる
        for (const Edge& e : edges) {
            if (uf.same(e.u, e.v)) continue;
            ans += e.w * 1LL * uf.size(e.u) * uf.size(e.v);
            uf.merge(e.u, e.v);
        }
        return ans;
    }
};

// ============================================================
// DsuOfflineThresholdConnectivity
// ============================================================
// 【用途】重み・時刻・高さなどのしきい値付き連結性クエリを、ソートして一括処理する solver
// 【典型】w<=x の辺だけで u,v が連結か、時刻 t までに追加済みの辺だけで連結かを多数答える
// 【使い方】Edge{u,v,w} と Query{u,v,limit,id} を作り、solve_leq または solve_lt を呼ぶ
// 【取得】返り値 ans は query 数と同じ長さで、ans[id] にそのクエリの答えが入る
// 【注意】solve_leq は w<=limit、solve_lt は w<limit。Query::id は 0..Q-1 を想定する
struct DsuOfflineThresholdConnectivity {
    struct Edge {
        int u, v;
        long long w;
    };

    struct Query {
        int u, v;
        long long limit;
        int id;
    };

    // w <= limit の辺だけを使った連結性クエリをまとめて解く O((M+Q) log(M+Q))
    static vector<bool> solve_leq(int n, vector<Edge> edges, vector<Query> queries) {
        sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
            return a.w < b.w;
        });
        sort(queries.begin(), queries.end(), [](const Query& a, const Query& b) {
            return a.limit < b.limit;
        });

        atcoder::dsu uf(n);
        vector<bool> ans(queries.size());
        int ei = 0;

        // クエリの limit が増える順に、使える辺を前から追加する
        for (const Query& q : queries) {
            while (ei < (int)edges.size() && edges[ei].w <= q.limit) {
                uf.merge(edges[ei].u, edges[ei].v);
                ei++;
            }
            ans[q.id] = uf.same(q.u, q.v);
        }
        return ans;
    }

    // w < limit の辺だけを使った連結性クエリをまとめて解く O((M+Q) log(M+Q))
    static vector<bool> solve_lt(int n, vector<Edge> edges, vector<Query> queries) {
        sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
            return a.w < b.w;
        });
        sort(queries.begin(), queries.end(), [](const Query& a, const Query& b) {
            return a.limit < b.limit;
        });

        atcoder::dsu uf(n);
        vector<bool> ans(queries.size());
        int ei = 0;

        // strict 版では、limit と等しい重みの辺はまだ追加しない
        for (const Query& q : queries) {
            while (ei < (int)edges.size() && edges[ei].w < q.limit) {
                uf.merge(edges[ei].u, edges[ei].v);
                ei++;
            }
            ans[q.id] = uf.same(q.u, q.v);
        }
        return ans;
    }
};

// ============================================================
// DsuFirstConnectByWeight
// ============================================================
// 【用途】辺を重み昇順に追加し、特定の連結条件が初めて満たされる重みを求める solver
// 【典型】s と t が連結になる最小しきい値、全頂点が連結になる最小しきい値、最小ボトルネック接続
// 【使い方】between(n,edges,s,t) または all_connected(n,edges) を呼ぶ
// 【取得】条件を満たした瞬間の辺重みを optional<long long> で返し、最後まで無理なら nullopt
// 【注意】s==t や n<=1 の自明ケースでは 0 を返す
struct DsuFirstConnectByWeight {
    struct Edge {
        int u, v;
        long long w;
    };

    // 辺を昇順に追加し、s と t が初めて連結になる重みを返す O(M log M)
    static optional<long long> between(int n, vector<Edge> edges, int s, int t) {
        if (s == t) return 0LL;
        sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
            return a.w < b.w;
        });

        atcoder::dsu uf(n);
        for (const Edge& e : edges) {
            uf.merge(e.u, e.v);
            if (uf.same(s, t)) return e.w;
        }
        return nullopt;
    }

    // 辺を昇順に追加し、全頂点が初めて連結になる重みを返す O(M log M)
    static optional<long long> all_connected(int n, vector<Edge> edges) {
        if (n <= 1) return 0LL;
        sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
            return a.w < b.w;
        });

        atcoder::dsu uf(n);
        int components = n;
        for (const Edge& e : edges) {
            if (uf.same(e.u, e.v)) continue;
            uf.merge(e.u, e.v);
            components--;
            if (components == 1) return e.w;
        }
        return nullopt;
    }
};

// ============================================================
// DsuReverseEdgeDeletionPairs
// ============================================================
// 【用途】辺削除列を逆から見て辺追加に変換し、各削除後の非連結ペア数を求める solver
// 【典型】橋が順に壊れる問題、道路削除後の不便さ、ABC120 D 型
// 【使い方】全辺 edges と、時系列順の deleted_edge_ids を disconnected_pairs_after_each_deletion に渡す
// 【取得】返り値 ans[i] は i 番目の削除を行った直後の非連結ペア数
// 【注意】deleted_edge_ids に同じ辺 id が重複して出るケースは想定していない
struct DsuReverseEdgeDeletionPairs {
    struct Edge {
        int u, v;
    };

    // 指定された辺を順に削除した直後の非連結ペア数を返す O((M+Q)α(N))
    static vector<long long> disconnected_pairs_after_each_deletion(
        int n,
        const vector<Edge>& edges,
        const vector<int>& deleted_edge_ids
    ) {
        int m = (int)edges.size();
        int q = (int)deleted_edge_ids.size();
        vector<char> deleted(m, 0);
        for (int id : deleted_edge_ids) deleted[id] = 1;

        atcoder::dsu uf(n);
        long long disconnected = 1LL * n * (n - 1) / 2;

        auto add_edge = [&](int id) {
            int u = edges[id].u;
            int v = edges[id].v;
            if (uf.same(u, v)) return;
            disconnected -= 1LL * uf.size(u) * uf.size(v);
            uf.merge(u, v);
        };

        // すべての削除が終わった後の最終状態を先に作る
        for (int i = 0; i < m; i++) {
            if (!deleted[i]) add_edge(i);
        }

        vector<long long> ans(q);

        // 逆順に見ると、削除は辺追加になる
        for (int i = q - 1; i >= 0; i--) {
            ans[i] = disconnected;
            add_edge(deleted_edge_ids[i]);
        }
        return ans;
    }
};

// ============================================================
// DsuStaticGridConnectivity
// ============================================================
// 【用途】固定グリッドで、指定文字 passable のマスだけを 4 近傍で連結成分化する solver
// 【典型】迷路の通路連結性、島のサイズ、スタートとゴールが同じ領域かの事前計算
// 【使い方】DsuStaticGridConnectivity g(grid, '.'); のように作り、g.same(r1,c1,r2,c2) や g.size(r,c) を呼ぶ
// 【取得】same は両マスが有効で同じ成分なら true、size は有効マスなら成分サイズ、無効マスなら 0
// 【注意】4 近傍固定。8 近傍が必要な場合はコンストラクタ内の方向配列を差し替える
struct DsuStaticGridConnectivity {
    int h, w;
    atcoder::dsu uf;
    vector<char> active;

    // passable と等しいマスを 4 近傍で連結して初期化する O(HWα(HW))
    DsuStaticGridConnectivity(const vector<string>& grid, char passable)
        : h((int)grid.size()), w(h == 0 ? 0 : (int)grid[0].size()), uf(h * w), active(h * w, 0) {
        for (int r = 0; r < h; r++) {
            for (int c = 0; c < w; c++) {
                active[id(r, c)] = (grid[r][c] == passable);
            }
        }

        // 右と下だけを見れば、4 近傍の辺を重複なく追加できる
        const int dr[2] = {1, 0};
        const int dc[2] = {0, 1};
        for (int r = 0; r < h; r++) {
            for (int c = 0; c < w; c++) {
                if (!is_active(r, c)) continue;
                for (int k = 0; k < 2; k++) {
                    int nr = r + dr[k];
                    int nc = c + dc[k];
                    if (inside(nr, nc) && is_active(nr, nc)) {
                        uf.merge(id(r, c), id(nr, nc));
                    }
                }
            }
        }
    }

    // マス座標を頂点番号に変換する O(1)
    int id(int r, int c) const {
        return r * w + c;
    }

    // マスがグリッド内なら true を返す O(1)
    bool inside(int r, int c) const {
        return 0 <= r && r < h && 0 <= c && c < w;
    }

    // マスがグリッド内かつ有効なら true を返す O(1)
    bool is_active(int r, int c) const {
        return inside(r, c) && active[id(r, c)];
    }

    // 2 マスがともに有効で同じ連結成分なら true を返す ならし O(α(HW))
    bool same(int r1, int c1, int r2, int c2) {
        if (!is_active(r1, c1) || !is_active(r2, c2)) return false;
        return uf.same(id(r1, c1), id(r2, c2));
    }

    // マスが有効なら属する成分サイズ、無効なら 0 を返す ならし O(α(HW))
    int size(int r, int c) {
        if (!is_active(r, c)) return 0;
        return uf.size(id(r, c));
    }
};

// ============================================================
// DsuGridActivationConnectivity
// ============================================================
// 【用途】最初は全マス無効で、activate(r,c) によりマスを有効化しながら 4 近傍連結を管理する solver
// 【典型】赤く塗ったマス同士の連結性、島の数、セル削除問題を逆順にしてセル追加として解く問題
// 【使い方】DsuGridActivationConnectivity g(h,w); と作り、g.activate(r,c) の後に g.same や g.size を呼ぶ
// 【取得】g.components が現在の有効マス連結成分数。activate は新規有効化できたとき true
// 【注意】境界仮想頂点や 8 近傍は持たない。必要な問題ではこの struct をコピーして最小修正する
struct DsuGridActivationConnectivity {
    int h, w;
    atcoder::dsu uf;
    vector<char> active;
    int components;

    // h*w マスをすべて無効状態で初期化する O(HW)
    DsuGridActivationConnectivity(int height, int width) : h(height), w(width), uf(height * width), active(height * width, 0), components(0) {}

    // マス座標を頂点番号に変換する O(1)
    int id(int r, int c) const {
        return r * w + c;
    }

    // マスがグリッド内なら true を返す O(1)
    bool inside(int r, int c) const {
        return 0 <= r && r < h && 0 <= c && c < w;
    }

    // マスがグリッド内かつ有効なら true を返す O(1)
    bool is_active(int r, int c) const {
        return inside(r, c) && active[id(r, c)];
    }

    // マスを有効化し、新規有効化できたかを返す ならし O(α(HW))
    bool activate(int r, int c) {
        if (!inside(r, c)) return false;
        int x = id(r, c);
        if (active[x]) return false;

        active[x] = 1;
        components++;

        // 4 近傍の有効マスとだけ併合する
        const int dr[4] = {1, -1, 0, 0};
        const int dc[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
            int nr = r + dr[k];
            int nc = c + dc[k];
            if (!is_active(nr, nc)) continue;
            int y = id(nr, nc);
            if (uf.same(x, y)) continue;
            uf.merge(x, y);
            components--;
        }
        return true;
    }

    // 2 マスがともに有効で同じ連結成分なら true を返す ならし O(α(HW))
    bool same(int r1, int c1, int r2, int c2) {
        if (!is_active(r1, c1) || !is_active(r2, c2)) return false;
        return uf.same(id(r1, c1), id(r2, c2));
    }

    // マスが有効なら属する成分サイズ、無効なら 0 を返す ならし O(α(HW))
    int size(int r, int c) {
        if (!is_active(r, c)) return 0;
        return uf.size(id(r, c));
    }
};

// ============================================================
// DsuOpposite
// ============================================================
// 【用途】2N 頂点展開で、二値変数の「同じ」「反対」制約を管理する solver
// 【典型】x==y、x!=y、味方・敵、同じチーム・違うチーム、白黒制約、XOR 0/1 の矛盾検出
// 【使い方】DsuOpposite g(n); と作り、g.add_same(x,y) または g.add_diff(x,y) で制約を追加する
// 【取得】g.ok がこれまでの制約全体の整合性。g.same(x,y)、g.diff(x,y) で確定関係を確認できる
// 【注意】入力する x,y は 0..n-1 の元変数番号を想定する。x の反対は内部で x+n
struct DsuOpposite {
    int n;
    atcoder::dsu uf;
    bool ok;

    // n 個の二値変数を 2N 頂点で初期化する O(N)
    DsuOpposite(int count) : n(count), uf(2 * count), ok(true) {}

    // x の反対側の頂点番号を返す O(1)
    int neg(int x) const {
        return x < n ? x + n : x - n;
    }

    // x == y の制約を追加し、矛盾していなければ true を返す ならし O(α(N))
    bool add_same(int x, int y) {
        uf.merge(x, y);
        uf.merge(neg(x), neg(y));
        if (uf.same(x, neg(x)) || uf.same(y, neg(y))) ok = false;
        return ok;
    }

    // x != y の制約を追加し、矛盾していなければ true を返す ならし O(α(N))
    bool add_diff(int x, int y) {
        uf.merge(x, neg(y));
        uf.merge(neg(x), y);
        if (uf.same(x, neg(x)) || uf.same(y, neg(y))) ok = false;
        return ok;
    }

    // x と y が同じ値だと確定しているなら true を返す ならし O(α(N))
    bool same(int x, int y) {
        return uf.same(x, y);
    }

    // x と y が異なる値だと確定しているなら true を返す ならし O(α(N))
    bool diff(int x, int y) {
        return uf.same(x, neg(y));
    }
};

// ============================================================
// DsuModRelation
// ============================================================
// 【用途】K*N 頂点展開で、小さい mod K の状態差制約を ACL dsu だけで扱う solver
// 【典型】mod 3 の食物連鎖、じゃんけん関係、小さい有限状態、state[y]=state[x]+d mod K の矛盾検出
// 【使い方】DsuModRelation g(n,k); と作り、g.add_relation(x,y,d) で制約を追加する
// 【取得】g.ok が整合性。g.same_state(x,rx,y,ry) で状態同士の同一視を確認できる
// 【注意】K が小さい場合専用。大きい K や一般のポテンシャル差分は重み付き Union-Find を使う
struct DsuModRelation {
    int n, k;
    atcoder::dsu uf;
    bool ok;

    // n 個の変数と k 個の状態を K*N 頂点で初期化する O(NK)
    DsuModRelation(int count, int states) : n(count), k(states), uf(count * states), ok(true) {}

    // 変数 x が状態 r であることを表す頂点番号を返す O(1)
    int id(int x, int r) const {
        return x * k + r;
    }

    // state[y] = state[x] + d mod k の制約を追加し、矛盾していなければ true を返す O(K^2α(NK))
    bool add_relation(int x, int y, int d) {
        d %= k;
        if (d < 0) d += k;

        // x が r なら y は r+d である、という同一視を全状態について追加する
        for (int r = 0; r < k; r++) {
            uf.merge(id(x, r), id(y, (r + d) % k));
        }

        // 同じ変数の異なる状態が同一視されたら矛盾
        for (int r = 0; r < k; r++) {
            for (int s = r + 1; s < k; s++) {
                if (uf.same(id(x, r), id(x, s)) || uf.same(id(y, r), id(y, s))) {
                    ok = false;
                }
            }
        }
        return ok;
    }

    // x が rx、y が ry である状態同士が同一視されているかを返す ならし O(α(NK))
    bool same_state(int x, int rx, int y, int ry) {
        return uf.same(id(x, rx), id(y, ry));
    }
};

// ============================================================
// DsuTwoLayerPairCount
// ============================================================
// 【用途】2 種類の無向グラフで、両方の連結成分が同じ頂点数を各頂点について求める solver
// 【典型】道路と鉄道の両方で連結、赤辺と青辺の両方で同じ成分、2 レイヤー連結性の分類
// 【使い方】edges1, edges2 を渡して DsuTwoLayerPairCount::count_same_pairs(n, edges1, edges2) を呼ぶ
// 【取得】返り値 ans[v] は、v と同じ (layer1 成分, layer2 成分) に属する頂点数
// 【注意】内部では leader ペアを vector に積んで sort する。map/unordered_map は使わない
struct DsuTwoLayerPairCount {
    struct Edge {
        int u, v;
    };

    // 2 つの連結性で leader ペアが同じ頂点数を各頂点について返す O((N+M)α(N)+N log N)
    static vector<int> count_same_pairs(
        int n,
        const vector<Edge>& edges1,
        const vector<Edge>& edges2
    ) {
        atcoder::dsu uf1(n), uf2(n);
        for (const Edge& e : edges1) uf1.merge(e.u, e.v);
        for (const Edge& e : edges2) uf2.merge(e.u, e.v);

        vector<pair<pair<int, int>, int>> keys;
        keys.reserve(n);
        for (int v = 0; v < n; v++) {
            keys.push_back({{uf1.leader(v), uf2.leader(v)}, v});
        }
        sort(keys.begin(), keys.end());

        vector<int> ans(n);

        // 同じ leader ペアが連続するので、その長さを各頂点へ割り当てる
        for (int l = 0; l < n;) {
            int r = l + 1;
            while (r < n && keys[r].first == keys[l].first) r++;
            int cnt = r - l;
            for (int i = l; i < r; i++) ans[keys[i].second] = cnt;
            l = r;
        }
        return ans;
    }
};

// ============================================================
// DsuCompressor
// ============================================================
// 【用途】既に作った dsu の連結成分を 0-indexed の成分 ID に振り直し、成分間グラフを作る solver
// 【典型】0 コスト辺で先に縮約、等式制約で変数縮約、同色領域縮約、縮約後グラフで BFS/DP
// 【使い方】必要な merge 済みの atcoder::dsu と元グラフの edges を DsuCompressor::build に渡す
// 【取得】comp_id[v] が圧縮後 ID、comp_size が成分サイズ、comp_edges が重複なし成分間辺
// 【注意】comp_edges は無向辺として u<v に正規化され、同一成分内の辺は捨てる
struct DsuCompressor {
    struct Edge {
        int u, v;
    };

    struct Result {
        int comp_count;
        vector<int> comp_id;
        vector<int> comp_size;
        vector<Edge> comp_edges;
    };

    // dsu の成分を 0-indexed に振り直し、成分間辺を重複なしで作る O((N+M)α(N)+M log M)
    static Result build(int n, atcoder::dsu& uf, const vector<Edge>& edges) {
        vector<int> leader_to_id(n, -1);
        vector<int> comp_id(n, -1);
        vector<int> comp_size;

        // 頂点番号の昇順に初登場した leader へ成分 ID を割り当てる
        for (int v = 0; v < n; v++) {
            int r = uf.leader(v);
            if (leader_to_id[r] == -1) {
                leader_to_id[r] = (int)comp_size.size();
                comp_size.push_back(uf.size(r));
            }
            comp_id[v] = leader_to_id[r];
        }

        vector<Edge> comp_edges;
        comp_edges.reserve(edges.size());

        // 元の辺を成分間辺へ変換し、自己ループは捨てる
        for (const Edge& e : edges) {
            int a = comp_id[e.u];
            int b = comp_id[e.v];
            if (a == b) continue;
            if (a > b) swap(a, b);
            comp_edges.push_back({a, b});
        }

        sort(comp_edges.begin(), comp_edges.end(), [](const Edge& a, const Edge& b) {
            if (a.u != b.u) return a.u < b.u;
            return a.v < b.v;
        });
        comp_edges.erase(unique(comp_edges.begin(), comp_edges.end(), [](const Edge& a, const Edge& b) {
            return a.u == b.u && a.v == b.v;
        }), comp_edges.end());

        return {(int)comp_size.size(), comp_id, comp_size, comp_edges};
    }
};

// ============================================================
// DsuExtraEdgeReconnector
// ============================================================
// 【用途】閉路を作る余分な辺を付け替えて、全体を連結にする操作列を作る solver
// 【典型】余ったケーブルの再配線、冗長辺を使った成分接続、連結化に必要な付け替え操作の出力
// 【使い方】edges を渡して DsuExtraEdgeReconnector::solve(n, edges) を呼び、possible と operations を見る
// 【取得】Operation{edge_id, old_endpoint, new_endpoint} は、その辺の old_endpoint 側を new_endpoint へ付け替える意味
// 【注意】操作の出力形式は問題ごとに違うため、1-indexed 出力や端点順は呼び出し側で合わせる
struct DsuExtraEdgeReconnector {
    struct Edge {
        int u, v;
    };

    struct Operation {
        int edge_id;
        int old_endpoint;
        int new_endpoint;
    };

    struct Result {
        bool possible;
        vector<Operation> operations;
    };

    struct SpareEdge {
        int id;
        int u, v;
    };

    // 閉路辺を付け替えて全体を連結にする操作列を作る O((N+M)α(N)+M)
    static Result solve(int n, const vector<Edge>& edges) {
        atcoder::dsu uf(n);
        vector<SpareEdge> spare_edges;

        // まず通常の Union-Find で連結成分を作り、閉路を作る辺を余りとして集める
        for (int i = 0; i < (int)edges.size(); i++) {
            int u = edges[i].u;
            int v = edges[i].v;
            if (uf.same(u, v)) {
                spare_edges.push_back({i, u, v});
            } else {
                uf.merge(u, v);
            }
        }

        vector<int> reps;
        for (int v = 0; v < n; v++) {
            if (uf.leader(v) == v) reps.push_back(v);
        }
        int comp_count = (int)reps.size();
        if (comp_count <= 1) return {true, {}};
        if ((int)spare_edges.size() < comp_count - 1) return {false, {}};

        vector<vector<SpareEdge>> spares_by_root(n);
        for (const SpareEdge& e : spare_edges) {
            spares_by_root[uf.leader(e.u)].push_back(e);
        }

        int start = -1;
        for (int r : reps) {
            if (!spares_by_root[r].empty()) {
                start = r;
                break;
            }
        }
        if (start == -1) return {false, {}};

        vector<int> with_spares;
        vector<int> without_spares;
        for (int r : reps) {
            if (r == start) continue;
            if (!spares_by_root[r].empty()) with_spares.push_back(r);
            else without_spares.push_back(r);
        }

        vector<SpareEdge> available = spares_by_root[start];
        vector<Operation> ops;
        ops.reserve(comp_count - 1);

        auto connect_target = [&](int target) -> bool {
            if (available.empty()) return false;
            SpareEdge e = available.back();
            available.pop_back();

            // e.v 側を現在の連結済み成分に残し、e.u 側を target へ付け替える
            ops.push_back({e.id, e.u, target});

            // target 成分が連結済み側へ加わるので、その成分内の余り辺も以後使える
            for (const SpareEdge& ne : spares_by_root[target]) {
                available.push_back(ne);
            }
            return true;
        };

        // 余り辺を持つ成分を先につなぐと、利用可能な余り辺が増えて安定する
        for (int target : with_spares) {
            if (!connect_target(target)) return {false, {}};
        }
        for (int target : without_spares) {
            if (!connect_target(target)) return {false, {}};
        }

        return {true, ops};
    }
};

// ============================================================
// DsuSwappableArrayChecker
// ============================================================
// 【用途】swap 可能な index を辺で与え、配列 a を配列 b に変換できるか判定する solver
// 【典型】同じ連結成分内なら自由に並べ替え可能、グラフ上 swap、成分ごとの multiset 比較
// 【使い方】DsuSwappableArrayChecker::can_transform(a, b, edges) を呼ぶ。edges は swap 可能な index 間の無向辺
// 【取得】変換可能なら true。各成分で a 側と b 側の値 multiset が一致すれば可能
// 【注意】値型 T は sort と == が使える必要がある。実際の swap 手順は作らない
struct DsuSwappableArrayChecker {
    struct Edge {
        int u, v;
    };

    // swap 可能な成分ごとの multiset が一致するかを判定する O((N+M)α(N)+N log N)
    template<class T>
    static bool can_transform(
        const vector<T>& a,
        const vector<T>& b,
        const vector<Edge>& edges
    ) {
        if (a.size() != b.size()) return false;
        int n = (int)a.size();
        atcoder::dsu uf(n);
        for (const Edge& e : edges) uf.merge(e.u, e.v);

        vector<pair<int, T>> left, right;
        left.reserve(n);
        right.reserve(n);
        for (int i = 0; i < n; i++) {
            int r = uf.leader(i);
            left.push_back({r, a[i]});
            right.push_back({r, b[i]});
        }

        sort(left.begin(), left.end());
        sort(right.begin(), right.end());
        return left == right;
    }
};

// ============================================================
// DsuSwappableStringMinimizer
// ============================================================
// 【用途】swap 可能な index の連結成分ごとに文字を並べ替え、辞書順最小文字列を作る solver
// 【典型】グラフ上 swap で文字列最小化、成分内で自由に並べ替えられる文字の最適配置
// 【使い方】DsuSwappableStringMinimizer::lexicographically_minimum(s, edges) を呼ぶ
// 【取得】戻り値が作れる辞書順最小文字列。各成分で小さい index に小さい文字を割り当てる
// 【注意】同じ成分内では任意の並べ替えが可能、という条件の問題で使う
struct DsuSwappableStringMinimizer {
    struct Edge {
        int u, v;
    };

    // swap 可能な成分内で自由に並べ替えて辞書順最小文字列を作る O((N+M)α(N)+N log N)
    static string lexicographically_minimum(string s, const vector<Edge>& edges) {
        int n = (int)s.size();
        atcoder::dsu uf(n);
        for (const Edge& e : edges) uf.merge(e.u, e.v);

        vector<pair<int, int>> keyed_indices;
        keyed_indices.reserve(n);
        for (int i = 0; i < n; i++) {
            keyed_indices.push_back({uf.leader(i), i});
        }
        sort(keyed_indices.begin(), keyed_indices.end());

        string ans = s;

        // 同じ成分の index と文字を集め、どちらも昇順にして小さい index へ小さい文字を置く
        for (int l = 0; l < n;) {
            int r = l + 1;
            while (r < n && keyed_indices[r].first == keyed_indices[l].first) r++;

            vector<int> indices;
            vector<char> chars;
            indices.reserve(r - l);
            chars.reserve(r - l);
            for (int i = l; i < r; i++) {
                int idx = keyed_indices[i].second;
                indices.push_back(idx);
                chars.push_back(s[idx]);
            }
            sort(indices.begin(), indices.end());
            sort(chars.begin(), chars.end());
            for (int i = 0; i < (int)indices.size(); i++) {
                ans[indices[i]] = chars[i];
            }

            l = r;
        }
        return ans;
    }
};

#if __INCLUDE_LEVEL__ == 0

// テスト用に 0 以上 upper_exclusive 未満の整数乱数を返す O(1)
static int test_rand_int(mt19937& rng, int upper_exclusive) {
    assert(upper_exclusive > 0);
    uniform_int_distribution<int> dist(0, upper_exclusive - 1);
    return dist(rng);
}

static void test_fixed_cases() {
    {
        DsuOnlineConnectivity g(5);
        assert(g.components == 5);
        assert(g.max_size == 1);
        assert(g.merge(0, 1));
        assert(!g.merge(0, 1));
        assert(g.same(0, 1));
        assert(g.size(0) == 2);
        assert(g.components == 4);
        assert(g.max_size == 2);
        g.merge(2, 3);
        g.merge(1, 2);
        assert(g.size(3) == 4);
        assert(g.max_size == 4);
        assert(!g.connected_all());
        g.merge(3, 4);
        assert(g.connected_all());
    }

    {
        DsuComponentPairCounter g(4);
        assert(g.connected_pairs == 0);
        assert(g.disconnected_pairs == 6);
        assert(g.merge(0, 1));
        assert(g.connected_pairs == 1);
        assert(g.disconnected_pairs == 5);
        assert(g.merge(2, 3));
        assert(g.connected_pairs == 2);
        assert(g.disconnected_pairs == 4);
        assert(g.merge(1, 2));
        assert(g.connected_pairs == 6);
        assert(g.disconnected_pairs == 0);
        assert(!g.merge(0, 3));
    }

    {
        DsuComponentEdgeInfo g(4);
        assert(g.add_edge(0, 1));
        assert(g.add_edge(1, 2));
        assert(g.is_tree(0));
        assert(g.edges(0) == 2);
        assert(g.size(0) == 3);
        assert(!g.add_edge(2, 0));
        assert(!g.is_tree(0));
        assert(g.is_unicyclic(0));
        assert(g.extra_edges(0) == 1);
        assert(g.addable_edges_inside(0) == 0);
        assert(g.is_tree(3));
    }

    {
        vector<long long> a = {1, 2, 3};
        DsuComponentSum g(a);
        assert(g.get(0) == 1);
        assert(g.merge(0, 2));
        assert(g.get(0) == 4);
        assert(g.merge(0, 1));
        assert(g.get(2) == 6);
        assert(!g.merge(1, 2));
    }

    {
        vector<DsuKruskalMst::Edge> edges = {
            {0, 1, 1, 0}, {1, 2, 2, 1}, {2, 3, 3, 2}, {0, 3, 10, 3}, {0, 2, 5, 4}
        };
        auto res = DsuKruskalMst::solve(4, edges);
        assert(res.cost == 6);
        assert(res.connected);
        assert(res.components == 1);
        sort(res.used_edge_ids.begin(), res.used_edge_ids.end());
        assert((res.used_edge_ids == vector<int>{0, 1, 2}));

        vector<DsuKruskalMst::Edge> edges2 = {{0, 1, 7, 0}, {2, 3, 8, 1}};
        auto res2 = DsuKruskalMst::solve(4, edges2);
        assert(res2.cost == 15);
        assert(!res2.connected);
        assert(res2.components == 2);
    }

    {
        vector<DsuKruskalPairContribution::Edge> tree = {
            {0, 1, 1}, {1, 2, 2}, {1, 3, 4}
        };
        assert(DsuKruskalPairContribution::sum_path_max(4, tree) == 17);
        assert(DsuKruskalPairContribution::sum_path_min(4, tree) == 11);
    }

    {
        vector<DsuOfflineThresholdConnectivity::Edge> edges = {
            {0, 1, 2}, {1, 2, 4}, {2, 3, 6}
        };
        vector<DsuOfflineThresholdConnectivity::Query> queries = {
            {0, 1, 3, 0}, {0, 2, 3, 1}, {0, 2, 4, 2}, {0, 3, 6, 3}
        };
        auto leq = DsuOfflineThresholdConnectivity::solve_leq(4, edges, queries);
        assert((leq == vector<bool>{true, false, true, true}));
        auto lt = DsuOfflineThresholdConnectivity::solve_lt(4, edges, queries);
        assert((lt == vector<bool>{true, false, false, false}));
    }

    {
        vector<DsuFirstConnectByWeight::Edge> edges = {
            {0, 1, 2}, {1, 2, 4}, {2, 3, 6}
        };
        assert(DsuFirstConnectByWeight::between(4, edges, 0, 3).value() == 6);
        assert(DsuFirstConnectByWeight::all_connected(4, edges).value() == 6);
        vector<DsuFirstConnectByWeight::Edge> bad = {{0, 1, 1}, {2, 3, 1}};
        assert(!DsuFirstConnectByWeight::between(4, bad, 0, 3).has_value());
    }

    {
        vector<DsuReverseEdgeDeletionPairs::Edge> edges = {
            {0, 1}, {1, 2}, {2, 3}
        };
        vector<int> del = {0, 1, 2};
        auto ans = DsuReverseEdgeDeletionPairs::disconnected_pairs_after_each_deletion(4, edges, del);
        assert((ans == vector<long long>{3, 5, 6}));
    }

    {
        vector<string> grid = {".#.", "..."};
        DsuStaticGridConnectivity g(grid, '.');
        assert(g.is_active(0, 0));
        assert(!g.is_active(0, 1));
        assert(g.same(0, 0, 0, 2));
        assert(g.size(0, 0) == 5);
        assert(g.size(0, 1) == 0);
    }

    {
        DsuGridActivationConnectivity g(2, 2);
        assert(g.components == 0);
        assert(g.activate(0, 0));
        assert(g.components == 1);
        assert(g.activate(0, 1));
        assert(g.components == 1);
        assert(g.activate(1, 1));
        assert(g.components == 1);
        assert(!g.activate(0, 1));
        assert(g.same(0, 0, 1, 1));
        assert(g.size(0, 0) == 3);
        assert(!g.same(0, 0, 1, 0));
    }

    {
        DsuOpposite g(3);
        assert(g.add_diff(0, 1));
        assert(g.add_same(1, 2));
        assert(g.diff(0, 2));
        assert(!g.add_same(0, 2));
        assert(!g.ok);
    }

    {
        DsuModRelation g(3, 3);
        assert(g.add_relation(0, 1, 1));
        assert(g.add_relation(1, 2, 1));
        assert(g.same_state(0, 0, 2, 2));
        assert(!g.add_relation(0, 2, 1));
        assert(!g.ok);
    }

    {
        vector<DsuTwoLayerPairCount::Edge> road = {{0, 1}, {2, 3}};
        vector<DsuTwoLayerPairCount::Edge> rail = {{0, 1}, {1, 2}};
        auto ans = DsuTwoLayerPairCount::count_same_pairs(4, road, rail);
        assert((ans == vector<int>{2, 2, 1, 1}));
    }

    {
        atcoder::dsu uf(4);
        uf.merge(0, 1);
        vector<DsuCompressor::Edge> edges = {{0, 2}, {1, 2}, {2, 3}, {0, 1}};
        auto res = DsuCompressor::build(4, uf, edges);
        assert(res.comp_count == 3);
        assert(res.comp_id[0] == res.comp_id[1]);
        assert(res.comp_id[2] != res.comp_id[3]);
        assert((res.comp_size == vector<int>{2, 1, 1}));
        assert(res.comp_edges.size() == 2);
        assert(res.comp_edges[0].u == 0 && res.comp_edges[0].v == 1);
        assert(res.comp_edges[1].u == 1 && res.comp_edges[1].v == 2);
    }

    {
        vector<DsuExtraEdgeReconnector::Edge> edges = {{0, 1}, {1, 2}, {2, 0}};
        auto res = DsuExtraEdgeReconnector::solve(4, edges);
        assert(res.possible);
        assert(res.operations.size() == 1);

        vector<pair<int, int>> changed;
        for (auto e : edges) changed.push_back({e.u, e.v});
        for (auto op : res.operations) {
            auto& e = changed[op.edge_id];
            if (e.first == op.old_endpoint) e.first = op.new_endpoint;
            else if (e.second == op.old_endpoint) e.second = op.new_endpoint;
            else assert(false);
        }
        atcoder::dsu uf(4);
        for (auto [u, v] : changed) uf.merge(u, v);
        assert(uf.size(0) == 4);

        vector<DsuExtraEdgeReconnector::Edge> bad = {{0, 1}, {2, 3}};
        assert(!DsuExtraEdgeReconnector::solve(4, bad).possible);
    }

    {
        vector<int> a = {1, 2, 3, 4};
        vector<int> b = {2, 1, 4, 3};
        vector<DsuSwappableArrayChecker::Edge> edges = {{0, 1}, {2, 3}};
        assert(DsuSwappableArrayChecker::can_transform(a, b, edges));
        vector<int> c = {2, 3, 1, 4};
        assert(!DsuSwappableArrayChecker::can_transform(a, c, edges));
    }

    {
        string s = "dcab";
        vector<DsuSwappableStringMinimizer::Edge> edges = {{0, 3}, {1, 2}};
        assert(DsuSwappableStringMinimizer::lexicographically_minimum(s, edges) == "bacd");
    }
}

static void test_random_online_and_pairs() {
    mt19937 rng(1);
    for (int tc = 0; tc < 500; tc++) {
        int n = 1 + test_rand_int(rng, 12);
        int q = 1 + test_rand_int(rng, 80);
        DsuOnlineConnectivity online(n);
        DsuComponentPairCounter pairs(n);
        vector<int> comp(n);
        iota(comp.begin(), comp.end(), 0);

        auto naive_same = [&](int a, int b) {
            return comp[a] == comp[b];
        };
        auto naive_merge = [&](int a, int b) {
            int ca = comp[a], cb = comp[b];
            if (ca == cb) return false;
            for (int i = 0; i < n; i++) if (comp[i] == cb) comp[i] = ca;
            return true;
        };
        auto naive_connected_pairs = [&]() {
            long long ret = 0;
            for (int i = 0; i < n; i++) {
                for (int j = i + 1; j < n; j++) {
                    if (naive_same(i, j)) ret++;
                }
            }
            return ret;
        };

        for (int qi = 0; qi < q; qi++) {
            int u = test_rand_int(rng, n);
            int v = test_rand_int(rng, n);
            bool expected_merge = naive_merge(u, v);
            bool got_online = online.merge(u, v);
            bool got_pairs = pairs.merge(u, v);
            assert(got_online == expected_merge);
            assert(got_pairs == expected_merge);

            long long cp = naive_connected_pairs();
            assert(pairs.connected_pairs == cp);
            assert(pairs.disconnected_pairs == 1LL * n * (n - 1) / 2 - cp);
            for (int a = 0; a < n; a++) {
                for (int b = 0; b < n; b++) {
                    assert(online.same(a, b) == naive_same(a, b));
                    assert(pairs.same(a, b) == naive_same(a, b));
                }
            }
        }
    }
}

static void test_random_component_edge_info() {
    mt19937 rng(2);
    for (int tc = 0; tc < 300; tc++) {
        int n = 1 + test_rand_int(rng, 10);
        int q = 1 + test_rand_int(rng, 50);
        DsuComponentEdgeInfo g(n);
        vector<pair<int, int>> added;

        for (int qi = 0; qi < q; qi++) {
            int u = test_rand_int(rng, n);
            int v2 = test_rand_int(rng, n);
            g.add_edge(u, v2);
            added.push_back({u, v2});

            vector<vector<int>> adj(n);
            for (auto [a, b] : added) {
                adj[a].push_back(b);
                if (a != b) adj[b].push_back(a);
            }

            vector<int> seen(n, 0), comp_id(n, -1), comp_size;
            int cid = 0;
            for (int s = 0; s < n; s++) {
                if (seen[s]) continue;
                queue<int> que;
                que.push(s);
                seen[s] = 1;
                int cnt = 0;
                while (!que.empty()) {
                    int x = que.front();
                    que.pop();
                    comp_id[x] = cid;
                    cnt++;
                    for (int y : adj[x]) {
                        if (!seen[y]) {
                            seen[y] = 1;
                            que.push(y);
                        }
                    }
                }
                comp_size.push_back(cnt);
                cid++;
            }

            vector<long long> comp_edges(cid, 0);
            for (auto [a, b] : added) comp_edges[comp_id[a]]++;
            for (int node = 0; node < n; node++) {
                assert(g.size(node) == comp_size[comp_id[node]]);
                assert(g.edges(node) == comp_edges[comp_id[node]]);
                assert(g.is_tree(node) == (comp_edges[comp_id[node]] == comp_size[comp_id[node]] - 1));
                assert(g.is_unicyclic(node) == (comp_edges[comp_id[node]] == comp_size[comp_id[node]]));
            }
        }
    }
}

static void test_random_kruskal_mst() {
    mt19937 rng(3);
    for (int tc = 0; tc < 200; tc++) {
        int n = 1 + test_rand_int(rng, 6);
        int m = test_rand_int(rng, 12);
        vector<DsuKruskalMst::Edge> edges;
        for (int i = 0; i < m; i++) {
            int u = test_rand_int(rng, n);
            int v = test_rand_int(rng, n);
            long long w = test_rand_int(rng, 21) - 10;
            edges.push_back({u, v, w, i});
        }

        auto got = DsuKruskalMst::solve(n, edges);

        long long best = (1LL << 60);
        int best_comps = n;
        for (int mask = 0; mask < (1 << m); mask++) {
            atcoder::dsu uf(n);
            long long cost = 0;
            int used = 0;
            bool cycle = false;
            for (int i = 0; i < m; i++) {
                if (!(mask >> i & 1)) continue;
                int u = edges[i].u, v = edges[i].v;
                if (uf.same(u, v)) {
                    cycle = true;
                    break;
                }
                uf.merge(u, v);
                cost += edges[i].w;
                used++;
            }
            if (cycle) continue;

            int comps = n - used;
            if (comps < best_comps || (comps == best_comps && cost < best)) {
                best_comps = comps;
                best = cost;
            }
        }
        assert(got.components == best_comps);
        assert(got.cost == best);
    }
}

static void test_random_offline_threshold() {
    mt19937 rng(4);
    for (int tc = 0; tc < 300; tc++) {
        int n = 1 + test_rand_int(rng, 10);
        int m = test_rand_int(rng, 20);
        int q = test_rand_int(rng, 20);
        vector<DsuOfflineThresholdConnectivity::Edge> edges;
        for (int i = 0; i < m; i++) {
            edges.push_back({test_rand_int(rng, n), test_rand_int(rng, n), test_rand_int(rng, 11) - 5});
        }
        vector<DsuOfflineThresholdConnectivity::Query> queries;
        for (int i = 0; i < q; i++) {
            queries.push_back({test_rand_int(rng, n), test_rand_int(rng, n), test_rand_int(rng, 13) - 6, i});
        }

        auto got_leq = DsuOfflineThresholdConnectivity::solve_leq(n, edges, queries);
        auto got_lt = DsuOfflineThresholdConnectivity::solve_lt(n, edges, queries);

        for (const auto& qu : queries) {
            for (int strict = 0; strict < 2; strict++) {
                atcoder::dsu uf(n);
                for (const auto& e : edges) {
                    if ((!strict && e.w <= qu.limit) || (strict && e.w < qu.limit)) {
                        uf.merge(e.u, e.v);
                    }
                }
                bool expected = uf.same(qu.u, qu.v);
                if (strict) assert(got_lt[qu.id] == expected);
                else assert(got_leq[qu.id] == expected);
            }
        }
    }
}

static void test_random_reverse_deletion() {
    mt19937 rng(5);
    for (int tc = 0; tc < 300; tc++) {
        int n = 1 + test_rand_int(rng, 10);
        int m = test_rand_int(rng, 20);
        vector<DsuReverseEdgeDeletionPairs::Edge> edges;
        for (int i = 0; i < m; i++) edges.push_back({test_rand_int(rng, n), test_rand_int(rng, n)});

        vector<int> del(m);
        iota(del.begin(), del.end(), 0);
        shuffle(del.begin(), del.end(), rng);
        int q = test_rand_int(rng, m + 1);
        del.resize(q);

        auto got = DsuReverseEdgeDeletionPairs::disconnected_pairs_after_each_deletion(n, edges, del);
        vector<char> removed(m, 0);
        vector<long long> expected;
        for (int id : del) {
            removed[id] = 1;
            atcoder::dsu uf(n);
            for (int i = 0; i < m; i++) {
                if (!removed[i]) uf.merge(edges[i].u, edges[i].v);
            }
            long long disconnected = 0;
            for (int i = 0; i < n; i++) {
                for (int j = i + 1; j < n; j++) {
                    if (!uf.same(i, j)) disconnected++;
                }
            }
            expected.push_back(disconnected);
        }
        assert(got == expected);
    }
}

static void test_random_grid_activation() {
    mt19937 rng(6);
    for (int tc = 0; tc < 300; tc++) {
        int h = 1 + test_rand_int(rng, 5);
        int w = 1 + test_rand_int(rng, 5);
        DsuGridActivationConnectivity g(h, w);
        vector<vector<int>> active(h, vector<int>(w, 0));

        auto naive_component = [&](int sr, int sc) {
            vector<pair<int, int>> cells;
            if (!active[sr][sc]) return cells;
            vector<vector<int>> seen(h, vector<int>(w, 0));
            queue<pair<int, int>> que;
            que.push({sr, sc});
            seen[sr][sc] = 1;
            const int dr[4] = {1, -1, 0, 0};
            const int dc[4] = {0, 0, 1, -1};
            while (!que.empty()) {
                auto [r, c] = que.front();
                que.pop();
                cells.push_back({r, c});
                for (int k = 0; k < 4; k++) {
                    int nr = r + dr[k], nc = c + dc[k];
                    if (nr < 0 || nr >= h || nc < 0 || nc >= w) continue;
                    if (!active[nr][nc] || seen[nr][nc]) continue;
                    seen[nr][nc] = 1;
                    que.push({nr, nc});
                }
            }
            return cells;
        };

        for (int qi = 0; qi < 50; qi++) {
            int r = test_rand_int(rng, h);
            int c = test_rand_int(rng, w);
            bool expected_new = !active[r][c];
            active[r][c] = 1;
            assert(g.activate(r, c) == expected_new);

            int comps = 0;
            vector<vector<int>> seen(h, vector<int>(w, 0));
            for (int sr = 0; sr < h; sr++) {
                for (int sc = 0; sc < w; sc++) {
                    if (!active[sr][sc] || seen[sr][sc]) continue;
                    comps++;
                    auto cells = naive_component(sr, sc);
                    for (auto [x, y] : cells) seen[x][y] = 1;
                }
            }
            assert(g.components == comps);

            for (int a = 0; a < h * w; a++) {
                int r1 = a / w, c1 = a % w;
                auto comp_cells = naive_component(r1, c1);
                assert(g.size(r1, c1) == (int)comp_cells.size());
                for (int b = 0; b < h * w; b++) {
                    int r2 = b / w, c2 = b % w;
                    bool expected_same = false;
                    for (auto [x, y] : comp_cells) {
                        if (x == r2 && y == c2) expected_same = true;
                    }
                    assert(g.same(r1, c1, r2, c2) == expected_same);
                }
            }
        }
    }
}

static void test_random_two_layer_and_swaps() {
    mt19937 rng(7);
    for (int tc = 0; tc < 300; tc++) {
        int n = 1 + test_rand_int(rng, 10);
        int m1 = test_rand_int(rng, 20);
        int m2 = test_rand_int(rng, 20);
        vector<DsuTwoLayerPairCount::Edge> e1, e2;
        vector<DsuSwappableArrayChecker::Edge> sw_edges;
        for (int i = 0; i < m1; i++) {
            int u = test_rand_int(rng, n), v = test_rand_int(rng, n);
            e1.push_back({u, v});
            sw_edges.push_back({u, v});
        }
        for (int i = 0; i < m2; i++) e2.push_back({test_rand_int(rng, n), test_rand_int(rng, n)});

        auto got = DsuTwoLayerPairCount::count_same_pairs(n, e1, e2);
        atcoder::dsu uf1(n), uf2(n);
        for (auto e : e1) uf1.merge(e.u, e.v);
        for (auto e : e2) uf2.merge(e.u, e.v);
        for (int v = 0; v < n; v++) {
            int cnt = 0;
            for (int u = 0; u < n; u++) {
                if (uf1.same(u, v) && uf2.same(u, v)) cnt++;
            }
            assert(got[v] == cnt);
        }

        vector<int> a(n), b(n);
        for (int i = 0; i < n; i++) a[i] = test_rand_int(rng, 5);
        b = a;
        for (int i = 0; i < n; i++) {
            int j = test_rand_int(rng, n);
            if (uf1.same(i, j)) swap(b[i], b[j]);
        }
        assert(DsuSwappableArrayChecker::can_transform(a, b, sw_edges));
    }
}


static void test_random_opposite_mod_extra() {
    mt19937 rng(8);

    // DsuOpposite は全割当ての全探索と照合する
    for (int tc = 0; tc < 300; tc++) {
        int n = 1 + test_rand_int(rng, 6);
        DsuOpposite g(n);
        vector<tuple<int, int, int>> constraints;
        int q = 1 + test_rand_int(rng, 30);
        for (int qi = 0; qi < q; qi++) {
            int type = test_rand_int(rng, 2);
            int x = test_rand_int(rng, n);
            int y = test_rand_int(rng, n);
            constraints.push_back({type, x, y});
            bool got = type == 0 ? g.add_same(x, y) : g.add_diff(x, y);

            bool expected = false;
            for (int mask = 0; mask < (1 << n); mask++) {
                bool ok = true;
                for (auto [t, a, b] : constraints) {
                    int va = (mask >> a) & 1;
                    int vb = (mask >> b) & 1;
                    if (t == 0 && va != vb) ok = false;
                    if (t == 1 && va == vb) ok = false;
                }
                if (ok) expected = true;
            }
            assert(got == expected);
            assert(g.ok == expected);
        }
    }

    // DsuModRelation は小さい K,N で全状態を列挙して矛盾有無を照合する
    for (int tc = 0; tc < 200; tc++) {
        int n = 1 + test_rand_int(rng, 5);
        int k = 2 + test_rand_int(rng, 3);
        DsuModRelation g(n, k);
        vector<tuple<int, int, int>> constraints;
        int q = 1 + test_rand_int(rng, 25);
        for (int qi = 0; qi < q; qi++) {
            int x = test_rand_int(rng, n);
            int y = test_rand_int(rng, n);
            int d = test_rand_int(rng, 2 * k + 1) - k;
            int nd = d % k;
            if (nd < 0) nd += k;
            constraints.push_back({x, y, nd});
            bool got = g.add_relation(x, y, d);

            int total = 1;
            for (int i = 0; i < n; i++) total *= k;
            bool expected = false;
            for (int mask = 0; mask < total; mask++) {
                int tmp = mask;
                vector<int> state(n);
                for (int i = 0; i < n; i++) {
                    state[i] = tmp % k;
                    tmp /= k;
                }

                bool ok = true;
                for (auto [a, b, rel] : constraints) {
                    if (state[b] != (state[a] + rel) % k) {
                        ok = false;
                        break;
                    }
                }
                if (ok) expected = true;
            }
            assert(got == expected);
            assert(g.ok == expected);
        }
    }

    // DsuExtraEdgeReconnector は操作を実際に適用し、最終的に連結になるかを確認する
    for (int tc = 0; tc < 500; tc++) {
        int n = 1 + test_rand_int(rng, 8);
        int m = test_rand_int(rng, 16);
        vector<DsuExtraEdgeReconnector::Edge> edges;
        for (int i = 0; i < m; i++) edges.push_back({test_rand_int(rng, n), test_rand_int(rng, n)});

        auto res = DsuExtraEdgeReconnector::solve(n, edges);
        bool expected_possible = m >= n - 1;
        assert(res.possible == expected_possible);
        if (!res.possible) continue;

        atcoder::dsu before(n);
        for (const auto& e : edges) before.merge(e.u, e.v);
        int comp_count = 0;
        for (int v = 0; v < n; v++) if (before.leader(v) == v) comp_count++;
        assert((int)res.operations.size() == max(0, comp_count - 1));

        vector<pair<int, int>> changed;
        for (auto e : edges) changed.push_back({e.u, e.v});
        for (auto op : res.operations) {
            assert(0 <= op.edge_id && op.edge_id < m);
            auto& e = changed[op.edge_id];
            if (e.first == op.old_endpoint) e.first = op.new_endpoint;
            else if (e.second == op.old_endpoint) e.second = op.new_endpoint;
            else assert(false);
        }
        atcoder::dsu after(n);
        for (auto [u, v] : changed) after.merge(u, v);
        assert(after.size(0) == n);
    }
}

int main() {
    test_fixed_cases();
    test_random_online_and_pairs();
    test_random_component_edge_info();
    test_random_kruskal_mst();
    test_random_offline_threshold();
    test_random_reverse_deletion();
    test_random_grid_activation();
    test_random_two_layer_and_swaps();
    test_random_opposite_mod_extra();

    cout << "All tests passed" << '\n';
    cout << "fixed cases: 18 solver groups" << '\n';
    cout << "random online/pair tests: 500 cases" << '\n';
    cout << "random component edge info tests: 300 cases" << '\n';
    cout << "random kruskal mst tests: 200 cases" << '\n';
    cout << "random offline threshold tests: 300 cases" << '\n';
    cout << "random reverse deletion tests: 300 cases" << '\n';
    cout << "random grid activation tests: 300 cases" << '\n';
    cout << "random two-layer/swap tests: 300 cases" << '\n';
    cout << "random opposite/mod/extra tests: 300+200+500 cases" << '\n';
    return 0;
}
#endif
