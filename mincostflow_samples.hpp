#pragma once
#include <bits/stdc++.h>
#include <atcoder/mincostflow>

/*
 * ACL mincostflow を典型用途ごとに薄く包んだソルバー集
 *
 * 方針
 * - namespace は使わず、各ソルバーをグローバルな struct として定義する
 * - 各 struct は単体でコピペしやすいよう、必要な型・データ・復元処理を内部に持つ
 * - 共通基底クラスや共通ヘルパーは作らず、用途ごとに最小限の実装にする
 * - solve 系メソッドは毎回 atcoder::mcf_graph を作り直すため、同じソルバーを複数回使える
 * - 最大利益系は cost = base - profit に変換して、ACL の非負コスト制約を満たす
 * - 最小コスト系は add_edge に渡す cost >= 0 を前提とする
 *
 * 主な使い分け
 * - McfMinCostBipartiteMatching: ちょうど k 組の二部マッチングの最小コスト
 * - McfMaxProfitBipartiteMatching: ちょうど/高々 k 組の二部マッチングの最大利益
 * - McfMinCostBipartiteFlow: 容量付き二部グラフで、指定流量の最小コスト
 * - McfMaxProfitBipartiteFlow: 容量付き二部グラフで、指定流量/高々指定流量の最大利益
 * - McfMaxProfitMatrixPick: 行・列容量付きのマス選択の最大利益
 * - McfMaxProfitGridPairing: グリッド隣接ペア選択の最大利益
 * - McfMaxProfitIntervalKSelection: 同時に高々 k 個まで重なる区間選択の最大利益
 * - McfMinCostSupplyDemandFlow: 一般有向グラフ上の供給需要付き最小コスト流
 * - McfMinCostKPaths: 一般有向グラフ上で s から t へ k 単位流す最小コスト
 */


/*
 * McfMinCostBipartiteMatching
 *
 * 左側 L 個・右側 R 個の二部グラフから、互いに端点が重ならないペアをちょうど k 組選び、
 * 選んだ辺コストの合計を最小化するソルバー
 *
 * 代表例
 * - 人と仕事を 1 対 1 で割り当てる最小コスト問題
 * - 希望順位つき割当で、不可能な組み合わせは辺を張らない問題
 * - N 個すべてを割り当てる完全二部マッチング、または k 組だけ作る部分マッチング
 *
 * 使い方
 * 1. McfMinCostBipartiteMatching solver(L, R) を作る
 * 2. 使ってよい組み合わせだけ add_edge(l, r, cost) で追加する
 *    - l は 0..L-1、r は 0..R-1
 *    - cost は 0 以上
 *    - 禁止ペアは add_edge しない
 * 3. solve_exact(k) を呼ぶ
 *    - k=N にすれば完全マッチング型
 *    - k を小さくすれば、ちょうど k 組だけ選ぶ型
 * 4. Result を見る
 *    - feasible=false のとき、ちょうど k 組は作れない
 *    - cost が最小コスト
 *    - selected_ids は add_edge の返り値 ID
 *    - pairs は選ばれた (left, right) の一覧
 *
 * 注意
 * - 各 left / right は高々 1 回しか使われない
 * - 利益最大化をしたい場合は McfMaxProfitBipartiteMatching を使う
 */
struct McfMinCostBipartiteMatching {
    using ll = long long;
    using Graph = atcoder::mcf_graph<ll, ll>;

    struct Result {
        bool feasible = false;
        ll flow = 0;
        ll cost = 0;
        std::vector<int> selected_ids;
        std::vector<std::pair<int, int>> pairs;
    };

    // 左側 left_size 個、右側 right_size 個の二部グラフを初期化する / O(L+R)
    McfMinCostBipartiteMatching(int left_count, int right_count)
        : left_size(left_count), right_size(right_count) {
        assert(0 <= left_count);
        assert(0 <= right_count);
    }

    // 左 l と右 r を結ぶ候補辺を追加し、その候補 ID を返す / O(1)
    int add_edge(int l, int r, ll cost) {
        assert(0 <= l && l < left_size);
        assert(0 <= r && r < right_size);
        assert(0 <= cost);
        int id = (int)edges.size();
        edges.push_back({l, r, cost});
        return id;
    }

    // ちょうど k 組のマッチングを作る最小コストを求める / O(F(V+E)log(V+E))
    Result solve_exact(ll k) const {
        assert(0 <= k);

        // 左右の頂点と始点・終点を作り、通常の二部マッチング用ネットワークを構築する
        int s = left_size + right_size;
        int t = s + 1;
        Graph g(t + 1);
        for (int l = 0; l < left_size; l++) g.add_edge(s, l, 1, 0);
        for (int r = 0; r < right_size; r++) g.add_edge(left_size + r, t, 1, 0);

        std::vector<int> edge_ids(edges.size());
        for (int i = 0; i < (int)edges.size(); i++) {
            const auto &e = edges[i];
            edge_ids[i] = g.add_edge(e.l, left_size + e.r, 1, e.cost);
        }

        auto [flow, cost] = g.flow(s, t, k);
        Result res;
        res.feasible = (flow == k);
        res.flow = flow;
        res.cost = cost;

        // 候補辺の flow を見て、実際に選ばれたペアを復元する
        for (int i = 0; i < (int)edges.size(); i++) {
            auto e = g.get_edge(edge_ids[i]);
            if (e.flow > 0) {
                res.selected_ids.push_back(i);
                res.pairs.push_back({edges[i].l, edges[i].r});
            }
        }
        return res;
    }

private:
    struct Edge {
        int l, r;
        ll cost;
    };

    int left_size, right_size;
    std::vector<Edge> edges;
};


/*
 * McfMaxProfitBipartiteMatching
 *
 * 左側 L 個・右側 R 個の二部グラフから、互いに端点が重ならないペアを選び、
 * 選んだ辺利益の合計を最大化するソルバー
 *
 * 代表例
 * - 最大重み二部マッチング
 * - 人と仕事、選手とポジション、商品と枠などの相性最大割当
 * - ちょうど k 組の最大利益マッチング
 * - 高々 k 組まで選べる最大利益マッチング
 * - サイズごとの最大利益の列挙
 *
 * 使い方
 * 1. McfMaxProfitBipartiteMatching solver(L, R) を作る
 * 2. 使ってよい組み合わせだけ add_edge(l, r, profit) で追加する
 *    - profit は負・0・正のいずれも可
 *    - 禁止ペアは add_edge しない
 * 3. 目的に合わせて solve を選ぶ
 *    - solve_exact(k): ちょうど k 組選ぶ
 *    - solve_at_most(k): 高々 k 組選ぶ。選ばないことも許す
 *    - solve_values(kmax): ちょうど k 組選ぶ最大利益を k=0..kmax で返す
 * 4. Result を見る
 *    - feasible=false のとき、指定条件を満たすマッチングは作れない
 *    - profit が最大利益
 *    - picked が実際に選ばれた組数
 *    - selected_ids / pairs で選ばれた辺を復元できる
 *
 * 注意
 * - solve_at_most(k) は内部でダミー辺へ流して「選ばない」を表現する
 * - 利益 0 の候補と選ばないことが同点の場合、利益最大化としては同じなので、利益 0 の候補が選ばれることがある
 * - solve_values の到達不能な k には NEG_INF が入る
 */
struct McfMaxProfitBipartiteMatching {
    using ll = long long;
    using Graph = atcoder::mcf_graph<ll, ll>;

    static constexpr ll NEG_INF = -(1LL << 60);

    struct Result {
        bool feasible = false;
        ll picked = 0;
        ll profit = 0;
        std::vector<int> selected_ids;
        std::vector<std::pair<int, int>> pairs;
    };

    // 左側 left_size 個、右側 right_size 個の二部グラフを初期化する / O(L+R)
    McfMaxProfitBipartiteMatching(int left_count, int right_count)
        : left_size(left_count), right_size(right_count) {
        assert(0 <= left_count);
        assert(0 <= right_count);
    }

    // 左 l と右 r を結ぶ候補辺を追加し、その候補 ID を返す / O(1)
    int add_edge(int l, int r, ll profit) {
        assert(0 <= l && l < left_size);
        assert(0 <= r && r < right_size);
        int id = (int)edges.size();
        edges.push_back({l, r, profit});
        return id;
    }

    // ちょうど k 組のマッチングを作る最大利益を求める / O(F(V+E)log(V+E))
    Result solve_exact(ll k) const {
        assert(0 <= k);
        ll base = calc_base();

        // 各候補の利益 profit を、非負コスト base-profit に変換して最小費用流にする
        int s = left_size + right_size;
        int t = s + 1;
        Graph g(t + 1);
        for (int l = 0; l < left_size; l++) g.add_edge(s, l, 1, 0);
        for (int r = 0; r < right_size; r++) g.add_edge(left_size + r, t, 1, 0);

        std::vector<int> edge_ids(edges.size());
        for (int i = 0; i < (int)edges.size(); i++) {
            const auto &e = edges[i];
            edge_ids[i] = g.add_edge(e.l, left_size + e.r, 1, base - e.profit);
        }

        auto [flow, cost] = g.flow(s, t, k);
        Result res;
        res.feasible = (flow == k);
        res.picked = flow;
        res.profit = flow * base - cost;

        // 候補辺の flow を見て、実際に選ばれたペアを復元する
        for (int i = 0; i < (int)edges.size(); i++) {
            auto e = g.get_edge(edge_ids[i]);
            if (e.flow > 0) {
                res.selected_ids.push_back(i);
                res.pairs.push_back({edges[i].l, edges[i].r});
            }
        }
        return res;
    }

    // 高々 k 組のマッチングを作る最大利益を求める / O(F(V+E)log(V+E))
    Result solve_at_most(ll k) const {
        assert(0 <= k);
        ll base = calc_base();

        // 選ばない分を S->T のダミー辺へ流し、常に k 単位流す形へ変換する
        int s = left_size + right_size;
        int t = s + 1;
        Graph g(t + 1);
        for (int l = 0; l < left_size; l++) g.add_edge(s, l, 1, 0);
        for (int r = 0; r < right_size; r++) g.add_edge(left_size + r, t, 1, 0);

        std::vector<int> edge_ids(edges.size());
        for (int i = 0; i < (int)edges.size(); i++) {
            const auto &e = edges[i];
            edge_ids[i] = g.add_edge(e.l, left_size + e.r, 1, base - e.profit);
        }
        g.add_edge(s, t, k, base);

        auto [flow, cost] = g.flow(s, t, k);
        Result res;
        res.feasible = (flow == k);
        res.profit = k * base - cost;

        // ダミー辺以外に流れた量を、実際に選ばれた個数として数える
        for (int i = 0; i < (int)edges.size(); i++) {
            auto e = g.get_edge(edge_ids[i]);
            if (e.flow > 0) {
                res.picked += e.flow;
                res.selected_ids.push_back(i);
                res.pairs.push_back({edges[i].l, edges[i].r});
            }
        }
        return res;
    }

    // k=0..kmax の各サイズについて、ちょうど k 組選ぶ最大利益を返す / O(F(V+E)log(V+E)+kmax)
    std::vector<ll> solve_values(int kmax) const {
        assert(0 <= kmax);
        ll base = calc_base();

        // slope は折れ線の端点だけを返すため、各整数流量の値を線形補間で埋める
        int s = left_size + right_size;
        int t = s + 1;
        Graph g(t + 1);
        for (int l = 0; l < left_size; l++) g.add_edge(s, l, 1, 0);
        for (int r = 0; r < right_size; r++) g.add_edge(left_size + r, t, 1, 0);
        for (const auto &e : edges) g.add_edge(e.l, left_size + e.r, 1, base - e.profit);

        std::vector<ll> values(kmax + 1, NEG_INF);
        auto points = g.slope(s, t, (ll)kmax);
        if (!points.empty()) values[0] = 0;
        for (int i = 0; i + 1 < (int)points.size(); i++) {
            ll f0 = points[i].first;
            ll c0 = points[i].second;
            ll f1 = points[i + 1].first;
            ll c1 = points[i + 1].second;
            if (f0 <= kmax) values[(int)f0] = f0 * base - c0;
            if (f0 == f1) continue;
            ll unit_cost = (c1 - c0) / (f1 - f0);
            for (ll f = f0 + 1; f <= f1 && f <= kmax; f++) {
                ll cost = c0 + unit_cost * (f - f0);
                values[(int)f] = f * base - cost;
            }
        }
        return values;
    }

private:
    struct Edge {
        int l, r;
        ll profit;
    };

    int left_size, right_size;
    std::vector<Edge> edges;

    ll calc_base() const {
        ll base = 0;
        for (const auto &e : edges) base = std::max(base, e.profit);
        return base;
    }
};


/*
 * McfMinCostBipartiteFlow
 *
 * 左側 L 個・右側 R 個の二部グラフで、左容量・右容量・辺容量を持つ最小コストフローを解くソルバー
 * 「1 対 1」ではなく、各頂点や各辺に複数単位流せる版
 *
 * 代表例
 * - 工場から店舗への輸送問題
 * - 行和・列和が決まった整数行列の復元
 * - 文字種や値の出現数を、変換コスト最小で別の分布へ変える問題
 * - 人が複数仕事を担当でき、仕事側にも必要人数がある容量付き割当
 *
 * 使い方
 * 1. McfMinCostBipartiteFlow solver(L, R, left_default, right_default) を作る
 *    - デフォルト容量を使わない場合は 0 のまま作り、set_left_cap / set_right_cap で設定する
 * 2. set_left_cap(l, cap), set_right_cap(r, cap) で供給側・需要側の容量を設定する
 * 3. add_edge(l, r, cap, cost) で流せる候補を追加する
 *    - cost は 1 単位あたりの非負コスト
 *    - cap はその候補辺に流せる上限
 * 4. solve_exact(flow_need) で、ちょうど flow_need 流す最小コストを求める
 * 5. Result を見る
 *    - feasible=false のとき、必要流量を流せない
 *    - cost が最小コスト
 *    - edge_flow[id] が add_edge した候補辺に実際に流れた量
 *
 * 注意
 * - cost は 0 以上を前提にしている
 * - 最大利益の容量付き二部フローは McfMaxProfitBipartiteFlow を使う
 */
struct McfMinCostBipartiteFlow {
    using ll = long long;
    using Graph = atcoder::mcf_graph<ll, ll>;

    struct Result {
        bool feasible = false;
        ll flow = 0;
        ll cost = 0;
        std::vector<ll> edge_flow;
    };

    // 左側 left_size 個、右側 right_size 個の容量付き二部グラフを初期化する / O(L+R)
    McfMinCostBipartiteFlow(int left_count, int right_count, ll left_cap_default = 0, ll right_cap_default = 0)
        : left_size(left_count), right_size(right_count),
          left_cap(left_count, left_cap_default), right_cap(right_count, right_cap_default) {
        assert(0 <= left_count);
        assert(0 <= right_count);
        assert(0 <= left_cap_default);
        assert(0 <= right_cap_default);
    }

    // 左 l の容量を設定する / O(1)
    void set_left_cap(int l, ll cap) {
        assert(0 <= l && l < left_size);
        assert(0 <= cap);
        left_cap[l] = cap;
    }

    // 右 r の容量を設定する / O(1)
    void set_right_cap(int r, ll cap) {
        assert(0 <= r && r < right_size);
        assert(0 <= cap);
        right_cap[r] = cap;
    }

    // 左 l から右 r へ流せる候補辺を追加し、その候補 ID を返す / O(1)
    int add_edge(int l, int r, ll cap, ll cost) {
        assert(0 <= l && l < left_size);
        assert(0 <= r && r < right_size);
        assert(0 <= cap);
        assert(0 <= cost);
        int id = (int)edges.size();
        edges.push_back({l, r, cap, cost});
        return id;
    }

    // ちょうど flow_need 流す最小コストを求める / O(F(V+E)log(V+E))
    Result solve_exact(ll flow_need) const {
        assert(0 <= flow_need);

        // 左右容量と候補辺容量をそのまま最小費用流ネットワークへ写す
        int s = left_size + right_size;
        int t = s + 1;
        Graph g(t + 1);
        for (int l = 0; l < left_size; l++) g.add_edge(s, l, left_cap[l], 0);
        for (int r = 0; r < right_size; r++) g.add_edge(left_size + r, t, right_cap[r], 0);

        std::vector<int> edge_ids(edges.size());
        for (int i = 0; i < (int)edges.size(); i++) {
            const auto &e = edges[i];
            edge_ids[i] = g.add_edge(e.l, left_size + e.r, e.cap, e.cost);
        }

        auto [flow, cost] = g.flow(s, t, flow_need);
        Result res;
        res.feasible = (flow == flow_need);
        res.flow = flow;
        res.cost = cost;
        res.edge_flow.assign(edges.size(), 0);

        // 各候補辺に実際に流れた量を、追加順の ID で復元する
        for (int i = 0; i < (int)edges.size(); i++) {
            auto e = g.get_edge(edge_ids[i]);
            res.edge_flow[i] = e.flow;
        }
        return res;
    }

private:
    struct Edge {
        int l, r;
        ll cap, cost;
    };

    int left_size, right_size;
    std::vector<ll> left_cap, right_cap;
    std::vector<Edge> edges;
};


/*
 * McfMaxProfitBipartiteFlow
 *
 * 左側 L 個・右側 R 個の二部グラフで、左容量・右容量・辺容量を持つ最大利益フローを解くソルバー
 * McfMaxProfitBipartiteMatching の容量付き版で、1 つの頂点や候補を複数回使える
 *
 * 代表例
 * - 容量付き二部マッチング、b-matching
 * - 各行・各列に上限があるマス選択
 * - 色とサイズ、ジャンルと作者など、2 種類の属性容量を持つ商品選択
 * - クーポン配布、広告枠割当、複数人を複数枠へ割り当てる最大利益問題
 *
 * 使い方
 * 1. McfMaxProfitBipartiteFlow solver(L, R, left_default, right_default) を作る
 * 2. set_left_cap(l, cap), set_right_cap(r, cap) で左右の容量を設定する
 * 3. add_edge(l, r, cap, profit) で候補辺を追加する
 *    - profit は 1 単位流したときの利益。負・0・正のいずれも可
 * 4. 目的に合わせて solve を選ぶ
 *    - solve_exact(flow_need): ちょうど flow_need 流す最大利益
 *    - solve_at_most(flow_limit): 高々 flow_limit 流す最大利益
 *    - solve_values(kmax): ちょうど f 流す最大利益を f=0..kmax で返す
 * 5. Result を見る
 *    - profit が最大利益
 *    - picked が実際に候補辺へ流れた総量
 *    - edge_flow[id] が各候補辺に流れた量
 *
 * 注意
 * - solve_at_most は選ばない分を内部のダミー辺へ流す
 * - 利益 0 の候補と選ばないことが同点の場合、利益 0 の候補へ流れることがある
 * - solve_values の到達不能な流量には NEG_INF が入る
 */
struct McfMaxProfitBipartiteFlow {
    using ll = long long;
    using Graph = atcoder::mcf_graph<ll, ll>;

    static constexpr ll NEG_INF = -(1LL << 60);

    struct Result {
        bool feasible = false;
        ll picked = 0;
        ll profit = 0;
        std::vector<ll> edge_flow;
    };

    // 左側 left_size 個、右側 right_size 個の容量付き二部グラフを初期化する / O(L+R)
    McfMaxProfitBipartiteFlow(int left_count, int right_count, ll left_cap_default = 0, ll right_cap_default = 0)
        : left_size(left_count), right_size(right_count),
          left_cap(left_count, left_cap_default), right_cap(right_count, right_cap_default) {
        assert(0 <= left_count);
        assert(0 <= right_count);
        assert(0 <= left_cap_default);
        assert(0 <= right_cap_default);
    }

    // 左 l の容量を設定する / O(1)
    void set_left_cap(int l, ll cap) {
        assert(0 <= l && l < left_size);
        assert(0 <= cap);
        left_cap[l] = cap;
    }

    // 右 r の容量を設定する / O(1)
    void set_right_cap(int r, ll cap) {
        assert(0 <= r && r < right_size);
        assert(0 <= cap);
        right_cap[r] = cap;
    }

    // 左 l から右 r へ流せる候補辺を追加し、その候補 ID を返す / O(1)
    int add_edge(int l, int r, ll cap, ll profit) {
        assert(0 <= l && l < left_size);
        assert(0 <= r && r < right_size);
        assert(0 <= cap);
        int id = (int)edges.size();
        edges.push_back({l, r, cap, profit});
        return id;
    }

    // ちょうど flow_need 流す最大利益を求める / O(F(V+E)log(V+E))
    Result solve_exact(ll flow_need) const {
        assert(0 <= flow_need);
        ll base = calc_base();

        // 利益 profit を非負コスト base-profit に変換し、指定流量の最小費用流を解く
        int s = left_size + right_size;
        int t = s + 1;
        Graph g(t + 1);
        for (int l = 0; l < left_size; l++) g.add_edge(s, l, left_cap[l], 0);
        for (int r = 0; r < right_size; r++) g.add_edge(left_size + r, t, right_cap[r], 0);

        std::vector<int> edge_ids(edges.size());
        for (int i = 0; i < (int)edges.size(); i++) {
            const auto &e = edges[i];
            edge_ids[i] = g.add_edge(e.l, left_size + e.r, e.cap, base - e.profit);
        }

        auto [flow, cost] = g.flow(s, t, flow_need);
        Result res;
        res.feasible = (flow == flow_need);
        res.picked = flow;
        res.profit = flow * base - cost;
        res.edge_flow.assign(edges.size(), 0);

        // 各候補辺に実際に流れた量を、追加順の ID で復元する
        for (int i = 0; i < (int)edges.size(); i++) {
            auto e = g.get_edge(edge_ids[i]);
            res.edge_flow[i] = e.flow;
        }
        return res;
    }

    // 高々 flow_limit 流す最大利益を求める / O(F(V+E)log(V+E))
    Result solve_at_most(ll flow_limit) const {
        assert(0 <= flow_limit);
        ll base = calc_base();

        // 選ばない分を S->T のダミー辺へ流し、常に flow_limit 単位流す形へ変換する
        int s = left_size + right_size;
        int t = s + 1;
        Graph g(t + 1);
        for (int l = 0; l < left_size; l++) g.add_edge(s, l, left_cap[l], 0);
        for (int r = 0; r < right_size; r++) g.add_edge(left_size + r, t, right_cap[r], 0);

        std::vector<int> edge_ids(edges.size());
        for (int i = 0; i < (int)edges.size(); i++) {
            const auto &e = edges[i];
            edge_ids[i] = g.add_edge(e.l, left_size + e.r, e.cap, base - e.profit);
        }
        g.add_edge(s, t, flow_limit, base);

        auto [flow, cost] = g.flow(s, t, flow_limit);
        Result res;
        res.feasible = (flow == flow_limit);
        res.profit = flow_limit * base - cost;
        res.edge_flow.assign(edges.size(), 0);

        // ダミー辺以外に流れた量を、実際に選ばれた量として数える
        for (int i = 0; i < (int)edges.size(); i++) {
            auto e = g.get_edge(edge_ids[i]);
            res.edge_flow[i] = e.flow;
            res.picked += e.flow;
        }
        return res;
    }

    // k=0..kmax の各流量について、ちょうど k 流す最大利益を返す / O(F(V+E)log(V+E)+kmax)
    std::vector<ll> solve_values(int kmax) const {
        assert(0 <= kmax);
        ll base = calc_base();

        // slope の折れ線端点から、各整数流量の最適値を復元する
        int s = left_size + right_size;
        int t = s + 1;
        Graph g(t + 1);
        for (int l = 0; l < left_size; l++) g.add_edge(s, l, left_cap[l], 0);
        for (int r = 0; r < right_size; r++) g.add_edge(left_size + r, t, right_cap[r], 0);
        for (const auto &e : edges) g.add_edge(e.l, left_size + e.r, e.cap, base - e.profit);

        std::vector<ll> values(kmax + 1, NEG_INF);
        auto points = g.slope(s, t, (ll)kmax);
        if (!points.empty()) values[0] = 0;
        for (int i = 0; i + 1 < (int)points.size(); i++) {
            ll f0 = points[i].first;
            ll c0 = points[i].second;
            ll f1 = points[i + 1].first;
            ll c1 = points[i + 1].second;
            if (f0 <= kmax) values[(int)f0] = f0 * base - c0;
            if (f0 == f1) continue;
            ll unit_cost = (c1 - c0) / (f1 - f0);
            for (ll f = f0 + 1; f <= f1 && f <= kmax; f++) {
                ll cost = c0 + unit_cost * (f - f0);
                values[(int)f] = f * base - cost;
            }
        }
        return values;
    }

private:
    struct Edge {
        int l, r;
        ll cap, profit;
    };

    int left_size, right_size;
    std::vector<ll> left_cap, right_cap;
    std::vector<Edge> edges;

    ll calc_base() const {
        ll base = 0;
        for (const auto &e : edges) base = std::max(base, e.profit);
        return base;
    }
};


/*
 * McfMaxProfitMatrixPick
 *
 * H 行 W 列の候補マスから、行ごとの選択上限・列ごとの選択上限を守ってマスを選び、
 * 選んだマス利益の合計を最大化するソルバー
 *
 * 代表例
 * - 各行・各列から高々 K 個選ぶ最大和問題
 * - 重み付き非攻撃ルーク配置
 * - 行と列の容量制約を持つ 0/1 マス選択
 * - AtCoder Library Practice Contest E 型のマス選択
 *
 * 使い方
 * 1. McfMaxProfitMatrixPick solver(H, W, row_default, col_default) を作る
 *    - デフォルトでは各行・各列の容量は 1
 * 2. 必要なら set_row_cap(y, cap), set_col_cap(x, cap) で容量を変更する
 * 3. 選べるマスだけ add_cell(y, x, profit) で追加する
 *    - profit は負・0・正のいずれも可
 *    - 選べないマスは add_cell しない
 * 4. solve_exact(k) または solve_at_most(k) を呼ぶ
 * 5. Result を見る
 *    - profit が最大利益
 *    - cells が選ばれた (y, x) の一覧
 *    - selected_ids は add_cell の返り値 ID
 *
 * 注意
 * - 各 add_cell 候補は高々 1 回だけ選ばれる
 * - マスごとに複数単位選べる問題は McfMaxProfitBipartiteFlow を使う
 */
struct McfMaxProfitMatrixPick {
    using ll = long long;
    using Graph = atcoder::mcf_graph<ll, ll>;

    struct Result {
        bool feasible = false;
        ll picked = 0;
        ll profit = 0;
        std::vector<int> selected_ids;
        std::vector<std::pair<int, int>> cells;
    };

    // h 行 w 列のマス選択ソルバーを初期化する / O(H+W)
    McfMaxProfitMatrixPick(int height, int width, ll row_cap_default = 1, ll col_cap_default = 1)
        : h(height), w(width), row_cap(height, row_cap_default), col_cap(width, col_cap_default) {
        assert(0 <= height);
        assert(0 <= width);
        assert(0 <= row_cap_default);
        assert(0 <= col_cap_default);
    }

    // 行 y の選択上限を設定する / O(1)
    void set_row_cap(int y, ll cap) {
        assert(0 <= y && y < h);
        assert(0 <= cap);
        row_cap[y] = cap;
    }

    // 列 x の選択上限を設定する / O(1)
    void set_col_cap(int x, ll cap) {
        assert(0 <= x && x < w);
        assert(0 <= cap);
        col_cap[x] = cap;
    }

    // マス (y,x) を選べる候補として追加し、その候補 ID を返す / O(1)
    int add_cell(int y, int x, ll profit) {
        assert(0 <= y && y < h);
        assert(0 <= x && x < w);
        int id = (int)cells.size();
        cells.push_back({y, x, profit});
        return id;
    }

    // ちょうど k マス選ぶ最大利益を求める / O(F(V+E)log(V+E))
    Result solve_exact(ll k) const {
        assert(0 <= k);
        ll base = calc_base();

        // 行を左側、列を右側にした容量付き二部マッチングとして解く
        int s = h + w;
        int t = s + 1;
        Graph g(t + 1);
        for (int y = 0; y < h; y++) g.add_edge(s, y, row_cap[y], 0);
        for (int x = 0; x < w; x++) g.add_edge(h + x, t, col_cap[x], 0);

        std::vector<int> edge_ids(cells.size());
        for (int i = 0; i < (int)cells.size(); i++) {
            const auto &c = cells[i];
            edge_ids[i] = g.add_edge(c.y, h + c.x, 1, base - c.profit);
        }

        auto [flow, cost] = g.flow(s, t, k);
        Result res;
        res.feasible = (flow == k);
        res.picked = flow;
        res.profit = flow * base - cost;

        // 選ばれたマスを候補 ID と座標で復元する
        for (int i = 0; i < (int)cells.size(); i++) {
            auto e = g.get_edge(edge_ids[i]);
            if (e.flow > 0) {
                res.selected_ids.push_back(i);
                res.cells.push_back({cells[i].y, cells[i].x});
            }
        }
        return res;
    }

    // 高々 k マス選ぶ最大利益を求める / O(F(V+E)log(V+E))
    Result solve_at_most(ll k) const {
        assert(0 <= k);
        ll base = calc_base();

        // 選ばない分を S->T のダミー辺へ流すことで、高々 k 個選択を表現する
        int s = h + w;
        int t = s + 1;
        Graph g(t + 1);
        for (int y = 0; y < h; y++) g.add_edge(s, y, row_cap[y], 0);
        for (int x = 0; x < w; x++) g.add_edge(h + x, t, col_cap[x], 0);

        std::vector<int> edge_ids(cells.size());
        for (int i = 0; i < (int)cells.size(); i++) {
            const auto &c = cells[i];
            edge_ids[i] = g.add_edge(c.y, h + c.x, 1, base - c.profit);
        }
        g.add_edge(s, t, k, base);

        auto [flow, cost] = g.flow(s, t, k);
        Result res;
        res.feasible = (flow == k);
        res.profit = k * base - cost;

        // ダミー辺以外に流れたマスだけを、実際に選んだマスとして返す
        for (int i = 0; i < (int)cells.size(); i++) {
            auto e = g.get_edge(edge_ids[i]);
            if (e.flow > 0) {
                res.picked += e.flow;
                res.selected_ids.push_back(i);
                res.cells.push_back({cells[i].y, cells[i].x});
            }
        }
        return res;
    }

private:
    struct CellEdge {
        int y, x;
        ll profit;
    };

    int h, w;
    std::vector<ll> row_cap, col_cap;
    std::vector<CellEdge> cells;

    ll calc_base() const {
        ll base = 0;
        for (const auto &c : cells) base = std::max(base, c.profit);
        return base;
    }
};


/*
 * McfMaxProfitGridPairing
 *
 * H 行 W 列のグリッド上で、隣接する 2 マスを 1 ペアとして候補追加し、
 * 各マスが高々 1 回だけ使われるようにペアを選んで利益合計を最大化するソルバー
 *
 * 代表例
 * - 最大重みドミノ配置
 * - グリッド上の隣接セルマッチング
 * - 1 マスを複数ペアで使えない隣接ペア選択
 *
 * 使い方
 * 1. McfMaxProfitGridPairing solver(H, W) を作る
 * 2. 選べる隣接ペアを add_pair(y1, x1, y2, x2, profit) で追加する
 *    - 2 マスはグリッド内かつ上下左右に隣接している必要がある
 *    - 内部では市松模様で黒白に向きを揃えるため、渡す順番はどちらでもよい
 *    - profit は負・0・正のいずれも可
 * 3. solve_exact(k) または solve_at_most(k) を呼ぶ
 * 4. Result を見る
 *    - profit が最大利益
 *    - pairs は選ばれたペアの一覧。add_pair に渡した元の向きで返る
 *    - selected_ids は add_pair の返り値 ID
 *
 * 注意
 * - 障害物マスがある場合は、そのマスを含む add_pair を呼ばなければよい
 * - 全隣接ペアを自動追加する機能はないので、必要な候補だけ利用側でループして追加する
 */
struct McfMaxProfitGridPairing {
    using ll = long long;
    using Graph = atcoder::mcf_graph<ll, ll>;

    struct Cell {
        int y = 0;
        int x = 0;
    };

    struct Pair {
        Cell a;
        Cell b;
    };

    struct Result {
        bool feasible = false;
        ll picked = 0;
        ll profit = 0;
        std::vector<int> selected_ids;
        std::vector<Pair> pairs;
    };

    // h 行 w 列のグリッド隣接ペア選択ソルバーを初期化する / O(1)
    McfMaxProfitGridPairing(int height, int width) : h(height), w(width) {
        assert(0 <= height);
        assert(0 <= width);
    }

    // 隣接する 2 マスのペア候補を追加し、その候補 ID を返す / O(1)
    int add_pair(int y1, int x1, int y2, int x2, ll profit) {
        assert(inside(y1, x1));
        assert(inside(y2, x2));
        assert(std::abs(y1 - y2) + std::abs(x1 - x2) == 1);
        assert(((y1 + x1) & 1) != ((y2 + x2) & 1));
        int id = (int)pairs.size();
        pairs.push_back({{y1, x1}, {y2, x2}, profit});
        return id;
    }

    // ちょうど k ペア選ぶ最大利益を求める / O(F(V+E)log(V+E))
    Result solve_exact(ll k) const {
        assert(0 <= k);
        ll base = calc_base();

        // 市松模様の黒マスから白マスへ辺を張り、二部マッチングとして解く
        int n = h * w;
        int s = n;
        int t = s + 1;
        Graph g(t + 1);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int v = cell_id(y, x);
                if (((y + x) & 1) == 0) g.add_edge(s, v, 1, 0);
                else g.add_edge(v, t, 1, 0);
            }
        }

        std::vector<int> edge_ids(pairs.size());
        for (int i = 0; i < (int)pairs.size(); i++) {
            auto [black, white] = directed_cells(pairs[i]);
            edge_ids[i] = g.add_edge(cell_id(black.y, black.x), cell_id(white.y, white.x), 1, base - pairs[i].profit);
        }

        auto [flow, cost] = g.flow(s, t, k);
        Result res;
        res.feasible = (flow == k);
        res.picked = flow;
        res.profit = flow * base - cost;

        // 選ばれたペアは、add_pair に渡した元の向きのまま返す
        for (int i = 0; i < (int)pairs.size(); i++) {
            auto e = g.get_edge(edge_ids[i]);
            if (e.flow > 0) {
                res.selected_ids.push_back(i);
                res.pairs.push_back({pairs[i].a, pairs[i].b});
            }
        }
        return res;
    }

    // 高々 k ペア選ぶ最大利益を求める / O(F(V+E)log(V+E))
    Result solve_at_most(ll k) const {
        assert(0 <= k);
        ll base = calc_base();

        // 選ばない分を S->T のダミー辺へ流し、最大利益のペアだけが実辺を使うようにする
        int n = h * w;
        int s = n;
        int t = s + 1;
        Graph g(t + 1);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int v = cell_id(y, x);
                if (((y + x) & 1) == 0) g.add_edge(s, v, 1, 0);
                else g.add_edge(v, t, 1, 0);
            }
        }

        std::vector<int> edge_ids(pairs.size());
        for (int i = 0; i < (int)pairs.size(); i++) {
            auto [black, white] = directed_cells(pairs[i]);
            edge_ids[i] = g.add_edge(cell_id(black.y, black.x), cell_id(white.y, white.x), 1, base - pairs[i].profit);
        }
        g.add_edge(s, t, k, base);

        auto [flow, cost] = g.flow(s, t, k);
        Result res;
        res.feasible = (flow == k);
        res.profit = k * base - cost;

        // ダミー辺以外に流れたペアだけを、実際に選ばれたペアとして返す
        for (int i = 0; i < (int)pairs.size(); i++) {
            auto e = g.get_edge(edge_ids[i]);
            if (e.flow > 0) {
                res.picked += e.flow;
                res.selected_ids.push_back(i);
                res.pairs.push_back({pairs[i].a, pairs[i].b});
            }
        }
        return res;
    }

private:
    struct PairEdge {
        Cell a, b;
        ll profit;
    };

    int h, w;
    std::vector<PairEdge> pairs;

    bool inside(int y, int x) const {
        return 0 <= y && y < h && 0 <= x && x < w;
    }

    int cell_id(int y, int x) const {
        return y * w + x;
    }

    std::pair<Cell, Cell> directed_cells(const PairEdge &e) const {
        if (((e.a.y + e.a.x) & 1) == 0) return {e.a, e.b};
        return {e.b, e.a};
    }

    ll calc_base() const {
        ll base = 0;
        for (const auto &e : pairs) base = std::max(base, e.profit);
        return base;
    }
};


/*
 * McfMaxProfitIntervalKSelection
 *
 * 半開区間 [l,r) の候補から、任意の時点で重なる区間数が高々 k になるように区間を選び、
 * 選んだ区間利益の合計を最大化するソルバー
 *
 * 代表例
 * - 会議室や機械が k 台あるときの予約・仕事選択
 * - 同時掲載数に上限がある広告区間選択
 * - レンタル品の予約受理最大利益
 * - 重なり高々 k の重み付き区間スケジューリング
 *
 * 使い方
 * 1. McfMaxProfitIntervalKSelection solver を作る
 * 2. add_interval(l, r, profit) で候補区間を追加する
 *    - 区間は半開区間 [l,r) として扱う
 *    - l < r が必要
 *    - profit は負・0・正のいずれも可
 * 3. solve(k) を呼ぶ
 *    - k は同時に選べる区間数の上限
 *    - 選ぶ区間数そのものは固定されない
 * 4. Result を見る
 *    - profit が最大利益
 *    - selected_ids は add_interval の返り値 ID
 *
 * 注意
 * - [1,3) と [3,5) は重ならない
 * - k=0 または候補が空の場合は、利益 0 の実行可能解を返す
 */
struct McfMaxProfitIntervalKSelection {
    using ll = long long;
    using Graph = atcoder::mcf_graph<ll, ll>;

    struct Result {
        bool feasible = false;
        ll profit = 0;
        std::vector<int> selected_ids;
    };

    // 区間選択ソルバーを空で初期化する / O(1)
    McfMaxProfitIntervalKSelection() = default;

    // 半開区間 [l,r) を選べる候補として追加し、その候補 ID を返す / O(1)
    int add_interval(ll l, ll r, ll profit) {
        assert(l < r);
        int id = (int)intervals.size();
        intervals.push_back({l, r, profit});
        return id;
    }

    // 任意の時点で重なり高々 k となる区間集合の最大利益を求める / O(F(V+E)log(V+E)+NlogN)
    Result solve(ll k) const {
        assert(0 <= k);
        Result res;
        if (k == 0 || intervals.empty()) {
            res.feasible = true;
            return res;
        }

        // 座標圧縮した時刻上に k 本の経路を流し、区間辺を使うと profit だけ得をする形にする
        std::vector<ll> xs;
        xs.reserve(intervals.size() * 2);
        ll base = 0;
        for (const auto &e : intervals) {
            xs.push_back(e.l);
            xs.push_back(e.r);
            base = std::max(base, e.profit);
        }
        std::sort(xs.begin(), xs.end());
        xs.erase(std::unique(xs.begin(), xs.end()), xs.end());

        int m = (int)xs.size();
        int s = 0;
        int t = m - 1;
        Graph g(m);
        for (int i = 0; i + 1 < m; i++) g.add_edge(i, i + 1, k, base);

        std::vector<int> edge_ids(intervals.size());
        for (int i = 0; i < (int)intervals.size(); i++) {
            const auto &e = intervals[i];
            int l = (int)(std::lower_bound(xs.begin(), xs.end(), e.l) - xs.begin());
            int r = (int)(std::lower_bound(xs.begin(), xs.end(), e.r) - xs.begin());
            edge_ids[i] = g.add_edge(l, r, 1, base * (r - l) - e.profit);
        }

        auto [flow, cost] = g.flow(s, t, k);
        res.feasible = (flow == k);
        res.profit = k * base * (m - 1) - cost;

        // flow が流れた区間辺が、選ばれた区間に対応する
        for (int i = 0; i < (int)intervals.size(); i++) {
            auto e = g.get_edge(edge_ids[i]);
            if (e.flow > 0) res.selected_ids.push_back(i);
        }
        return res;
    }

private:
    struct Interval {
        ll l, r, profit;
    };

    std::vector<Interval> intervals;
};


/*
 * McfMinCostSupplyDemandFlow
 *
 * 一般の有向グラフ上で、各頂点の供給量・需要量をすべて満たすように流し、
 * 辺コストの合計を最小化するソルバー
 *
 * 代表例
 * - 複数地点の余剰から不足への輸送
 * - 工場、倉庫、店舗のような多段階物流
 * - 日付や時刻を頂点にした在庫管理・生産計画
 * - 時間展開ネットワーク上の需要供給付きフロー
 *
 * 使い方
 * 1. McfMinCostSupplyDemandFlow solver(N) を作る
 * 2. add_supply(v, amount) で頂点 v に供給量を追加する
 * 3. add_demand(v, amount) で頂点 v に需要量を追加する
 * 4. add_edge(from, to, cap, cost) で使える有向辺を追加する
 *    - cost は 1 単位あたりの非負コスト
 *    - cap はその辺の容量
 * 5. solve() を呼ぶ
 * 6. Result を見る
 *    - feasible=false のとき、供給需要の不一致、または容量不足で実行不能
 *    - cost が最小コスト
 *    - edge_flow[id] が各元辺に流れた量
 *
 * 注意
 * - 供給合計と需要合計が一致しない場合は feasible=false
 * - 下限付き辺や循環流の専用処理は持たない。必要なら通常のフロー変換を利用側で行う
 */
struct McfMinCostSupplyDemandFlow {
    using ll = long long;
    using Graph = atcoder::mcf_graph<ll, ll>;

    struct Result {
        bool feasible = false;
        ll flow = 0;
        ll cost = 0;
        std::vector<ll> edge_flow;
    };

    // n 頂点の供給需要付き有向グラフを初期化する / O(N)
    McfMinCostSupplyDemandFlow(int vertex_count) : n(vertex_count), supply(vertex_count, 0), demand(vertex_count, 0) {
        assert(0 <= vertex_count);
    }

    // 頂点 v の供給量を amount だけ追加する / O(1)
    void add_supply(int v, ll amount) {
        assert(0 <= v && v < n);
        assert(0 <= amount);
        supply[v] += amount;
    }

    // 頂点 v の需要量を amount だけ追加する / O(1)
    void add_demand(int v, ll amount) {
        assert(0 <= v && v < n);
        assert(0 <= amount);
        demand[v] += amount;
    }

    // from から to へ流せる有向辺を追加し、その候補 ID を返す / O(1)
    int add_edge(int from, int to, ll cap, ll cost) {
        assert(0 <= from && from < n);
        assert(0 <= to && to < n);
        assert(0 <= cap);
        assert(0 <= cost);
        int id = (int)edges.size();
        edges.push_back({from, to, cap, cost});
        return id;
    }

    // すべての供給と需要を満たす最小コストを求める / O(F(V+E)log(V+E))
    Result solve() const {
        Result res;
        res.edge_flow.assign(edges.size(), 0);

        // 供給合計と需要合計が一致しない場合は、そもそも実行可能解がない
        ll total_supply = 0, total_demand = 0;
        for (ll x : supply) total_supply += x;
        for (ll x : demand) total_demand += x;
        if (total_supply != total_demand) return res;

        int ss = n;
        int tt = n + 1;
        Graph g(n + 2);
        for (int v = 0; v < n; v++) {
            if (supply[v] > 0) g.add_edge(ss, v, supply[v], 0);
            if (demand[v] > 0) g.add_edge(v, tt, demand[v], 0);
        }

        std::vector<int> edge_ids(edges.size());
        for (int i = 0; i < (int)edges.size(); i++) {
            const auto &e = edges[i];
            edge_ids[i] = g.add_edge(e.from, e.to, e.cap, e.cost);
        }

        auto [flow, cost] = g.flow(ss, tt, total_supply);
        res.feasible = (flow == total_supply);
        res.flow = flow;
        res.cost = cost;

        // 元の辺に流れた量だけを、追加順の ID で復元する
        for (int i = 0; i < (int)edges.size(); i++) {
            auto e = g.get_edge(edge_ids[i]);
            res.edge_flow[i] = e.flow;
        }
        return res;
    }

private:
    struct Edge {
        int from, to;
        ll cap, cost;
    };

    int n;
    std::vector<ll> supply, demand;
    std::vector<Edge> edges;
};


/*
 * McfMinCostKPaths
 *
 * 一般の有向グラフ上で、始点 s から終点 t へちょうど k 単位流す最小コスト問題を解く薄いラッパ
 * 1 単位のフローを 1 本の経路と見れば、容量付きの k 本経路選択として使える
 *
 * 代表例
 * - k 本の辺素または容量付き最短路
 * - 複数荷物・複数パケットを同じネットワークで送る最小コスト
 * - グリッドや DAG 上の複数経路選択の土台
 *
 * 使い方
 * 1. McfMinCostKPaths solver(N) を作る
 * 2. add_edge(from, to, cap, cost) で有向辺を追加する
 *    - 辺素にしたい辺は cap=1
 *    - cost は 1 単位あたりの非負コスト
 * 3. solve(s, t, k) を呼ぶ
 * 4. Result を見る
 *    - feasible=false のとき、s から t へ k 単位流せない
 *    - cost が最小コスト
 *    - edge_flow[id] が各辺に流れた量
 *
 * 注意
 * - 頂点素にしたい場合は、利用側で v_in -> v_out の容量 1 辺を作る頂点分割を行う
 * - このソルバーはパス分解までは行わず、辺ごとの flow だけを返す
 */
struct McfMinCostKPaths {
    using ll = long long;
    using Graph = atcoder::mcf_graph<ll, ll>;

    struct Result {
        bool feasible = false;
        ll flow = 0;
        ll cost = 0;
        std::vector<ll> edge_flow;
    };

    // n 頂点の容量付き有向グラフを初期化する / O(1)
    McfMinCostKPaths(int vertex_count) : n(vertex_count) {
        assert(0 <= vertex_count);
    }

    // from から to へ流せる有向辺を追加し、その候補 ID を返す / O(1)
    int add_edge(int from, int to, ll cap, ll cost) {
        assert(0 <= from && from < n);
        assert(0 <= to && to < n);
        assert(0 <= cap);
        assert(0 <= cost);
        int id = (int)edges.size();
        edges.push_back({from, to, cap, cost});
        return id;
    }

    // s から t へちょうど k 単位流す最小コストを求める / O(F(V+E)log(V+E))
    Result solve(int s, int t, ll k) const {
        assert(0 <= s && s < n);
        assert(0 <= t && t < n);
        assert(s != t);
        assert(0 <= k);

        // 入力された有向辺をそのまま ACL の最小費用流グラフへ追加する
        Graph g(n);
        std::vector<int> edge_ids(edges.size());
        for (int i = 0; i < (int)edges.size(); i++) {
            const auto &e = edges[i];
            edge_ids[i] = g.add_edge(e.from, e.to, e.cap, e.cost);
        }

        auto [flow, cost] = g.flow(s, t, k);
        Result res;
        res.feasible = (flow == k);
        res.flow = flow;
        res.cost = cost;
        res.edge_flow.assign(edges.size(), 0);

        // 各元辺に実際に流れた量を、追加順の ID で復元する
        for (int i = 0; i < (int)edges.size(); i++) {
            auto e = g.get_edge(edge_ids[i]);
            res.edge_flow[i] = e.flow;
        }
        return res;
    }

private:
    struct Edge {
        int from, to;
        ll cap, cost;
    };

    int n;
    std::vector<Edge> edges;
};

#if __INCLUDE_LEVEL__ == 0
int main() {
    using ll = long long;
    const ll INF = (1LL << 60);
    const ll NEG_INF = -(1LL << 60);
    std::mt19937 rng(1);
    auto rnd_mod = [&](int mod) -> int {
        assert(0 < mod);
        return static_cast<int>(rng() % static_cast<std::mt19937::result_type>(mod));
    };

    auto brute_matching_min = [&](int L, int R, const std::vector<std::tuple<int, int, ll>> &es, int k) {
        ll best = INF;
        int m = (int)es.size();
        for (int mask = 0; mask < (1 << m); mask++) {
            if (__builtin_popcount((unsigned)mask) != k) continue;
            std::vector<int> used_l(L, 0), used_r(R, 0);
            ll sum = 0;
            bool ok = true;
            for (int i = 0; i < m; i++) if ((mask >> i) & 1) {
                auto [l, r, c] = es[i];
                if (used_l[l] || used_r[r]) ok = false;
                used_l[l] = used_r[r] = 1;
                sum += c;
            }
            if (ok) best = std::min(best, sum);
        }
        return best;
    };

    auto brute_matching_max_exact = [&](int L, int R, const std::vector<std::tuple<int, int, ll>> &es, int k) {
        ll best = NEG_INF;
        int m = (int)es.size();
        for (int mask = 0; mask < (1 << m); mask++) {
            if (__builtin_popcount((unsigned)mask) != k) continue;
            std::vector<int> used_l(L, 0), used_r(R, 0);
            ll sum = 0;
            bool ok = true;
            for (int i = 0; i < m; i++) if ((mask >> i) & 1) {
                auto [l, r, p] = es[i];
                if (used_l[l] || used_r[r]) ok = false;
                used_l[l] = used_r[r] = 1;
                sum += p;
            }
            if (ok) best = std::max(best, sum);
        }
        return best;
    };

    auto brute_matching_max_at_most = [&](int L, int R, const std::vector<std::tuple<int, int, ll>> &es, int k) {
        ll best = 0;
        int m = (int)es.size();
        for (int mask = 0; mask < (1 << m); mask++) {
            if (__builtin_popcount((unsigned)mask) > k) continue;
            std::vector<int> used_l(L, 0), used_r(R, 0);
            ll sum = 0;
            bool ok = true;
            for (int i = 0; i < m; i++) if ((mask >> i) & 1) {
                auto [l, r, p] = es[i];
                if (used_l[l] || used_r[r]) ok = false;
                used_l[l] = used_r[r] = 1;
                sum += p;
            }
            if (ok) best = std::max(best, sum);
        }
        return best;
    };

    auto brute_bipartite_flow = [&](int L, int R, const std::vector<ll> &lc, const std::vector<ll> &rc,
                                    const std::vector<std::tuple<int, int, ll, ll>> &es, int need, bool maximize, bool at_most) {
        ll best = maximize ? NEG_INF : INF;
        int m = (int)es.size();
        std::vector<ll> lu(L, 0), ru(R, 0);
        auto dfs = [&](auto &&self, int idx, ll total, ll val) -> void {
            if (idx == m) {
                if (at_most) {
                    if (total > need) return;
                } else {
                    if (total != need) return;
                }
                if (maximize) best = std::max(best, val);
                else best = std::min(best, val);
                return;
            }
            auto [l, r, cap, score] = es[idx];
            for (ll f = 0; f <= cap; f++) {
                if (lu[l] + f > lc[l] || ru[r] + f > rc[r] || total + f > need) break;
                lu[l] += f;
                ru[r] += f;
                self(self, idx + 1, total + f, val + f * score);
                lu[l] -= f;
                ru[r] -= f;
            }
        };
        dfs(dfs, 0, 0, 0);
        if (maximize && at_most) best = std::max(best, 0LL);
        return best;
    };

    auto brute_matrix_exact = [&](int H, int W, const std::vector<ll> &row_cap, const std::vector<ll> &col_cap,
                                  const std::vector<std::tuple<int, int, ll>> &cells, int k) {
        ll best = NEG_INF;
        int m = (int)cells.size();
        for (int mask = 0; mask < (1 << m); mask++) {
            if (__builtin_popcount((unsigned)mask) != k) continue;
            std::vector<ll> ru(H, 0), cu(W, 0);
            ll sum = 0;
            bool ok = true;
            for (int i = 0; i < m; i++) if ((mask >> i) & 1) {
                auto [y, x, p] = cells[i];
                ru[y]++;
                cu[x]++;
                sum += p;
            }
            for (int y = 0; y < H; y++) if (ru[y] > row_cap[y]) ok = false;
            for (int x = 0; x < W; x++) if (cu[x] > col_cap[x]) ok = false;
            if (ok) best = std::max(best, sum);
        }
        return best;
    };

    auto brute_matrix_at_most = [&](int H, int W, const std::vector<ll> &row_cap, const std::vector<ll> &col_cap,
                                    const std::vector<std::tuple<int, int, ll>> &cells, int k) {
        ll best = 0;
        int m = (int)cells.size();
        for (int mask = 0; mask < (1 << m); mask++) {
            if (__builtin_popcount((unsigned)mask) > k) continue;
            std::vector<ll> ru(H, 0), cu(W, 0);
            ll sum = 0;
            bool ok = true;
            for (int i = 0; i < m; i++) if ((mask >> i) & 1) {
                auto [y, x, p] = cells[i];
                ru[y]++;
                cu[x]++;
                sum += p;
            }
            for (int y = 0; y < H; y++) if (ru[y] > row_cap[y]) ok = false;
            for (int x = 0; x < W; x++) if (cu[x] > col_cap[x]) ok = false;
            if (ok) best = std::max(best, sum);
        }
        return best;
    };

    auto brute_grid = [&](int H, int W, const std::vector<std::tuple<int, int, int, int, ll>> &es, int k, bool exact) {
        ll best = exact ? NEG_INF : 0;
        int m = (int)es.size();
        for (int mask = 0; mask < (1 << m); mask++) {
            int cnt = __builtin_popcount((unsigned)mask);
            if (exact && cnt != k) continue;
            if (!exact && cnt > k) continue;
            std::vector<int> used(H * W, 0);
            ll sum = 0;
            bool ok = true;
            for (int i = 0; i < m; i++) if ((mask >> i) & 1) {
                auto [y1, x1, y2, x2, p] = es[i];
                int a = y1 * W + x1;
                int b = y2 * W + x2;
                if (used[a] || used[b]) ok = false;
                used[a] = used[b] = 1;
                sum += p;
            }
            if (ok) best = std::max(best, sum);
        }
        return best;
    };

    auto brute_interval = [&](const std::vector<std::tuple<ll, ll, ll>> &iv, int k) {
        ll best = 0;
        int m = (int)iv.size();
        for (int mask = 0; mask < (1 << m); mask++) {
            std::vector<ll> xs;
            ll sum = 0;
            for (int i = 0; i < m; i++) if ((mask >> i) & 1) {
                auto [l, r, p] = iv[i];
                xs.push_back(l);
                xs.push_back(r);
                sum += p;
            }
            std::sort(xs.begin(), xs.end());
            xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
            bool ok = true;
            for (int j = 0; j + 1 < (int)xs.size(); j++) {
                int cnt = 0;
                for (int i = 0; i < m; i++) if ((mask >> i) & 1) {
                    auto [l, r, p] = iv[i];
                    if (l < xs[j + 1] && xs[j] < r) cnt++;
                }
                if (cnt > k) ok = false;
            }
            if (ok) best = std::max(best, sum);
        }
        return best;
    };

    auto brute_supply_demand = [&](int n, const std::vector<ll> &supply, const std::vector<ll> &demand,
                                   const std::vector<std::tuple<int, int, ll, ll>> &es) {
        ll ts = 0, td = 0;
        for (ll x : supply) ts += x;
        for (ll x : demand) td += x;
        if (ts != td) return INF;
        ll best = INF;
        int m = (int)es.size();
        std::vector<ll> out(n, 0), in(n, 0);
        auto dfs = [&](auto &&self, int idx, ll cost) -> void {
            if (idx == m) {
                for (int v = 0; v < n; v++) if (out[v] - in[v] != supply[v] - demand[v]) return;
                best = std::min(best, cost);
                return;
            }
            auto [a, b, cap, c] = es[idx];
            for (ll f = 0; f <= cap; f++) {
                out[a] += f;
                in[b] += f;
                self(self, idx + 1, cost + f * c);
                out[a] -= f;
                in[b] -= f;
            }
        };
        dfs(dfs, 0, 0);
        return best;
    };

    auto brute_k_paths = [&](int n, int s, int t, int k, const std::vector<std::tuple<int, int, ll, ll>> &es) {
        ll best = INF;
        int m = (int)es.size();
        std::vector<ll> out(n, 0), in(n, 0);
        auto dfs = [&](auto &&self, int idx, ll cost) -> void {
            if (idx == m) {
                for (int v = 0; v < n; v++) {
                    ll need = 0;
                    if (v == s) need = k;
                    if (v == t) need = -k;
                    if (out[v] - in[v] != need) return;
                }
                best = std::min(best, cost);
                return;
            }
            auto [a, b, cap, c] = es[idx];
            for (ll f = 0; f <= cap; f++) {
                out[a] += f;
                in[b] += f;
                self(self, idx + 1, cost + f * c);
                out[a] -= f;
                in[b] -= f;
            }
        };
        dfs(dfs, 0, 0);
        return best;
    };

    {
        McfMinCostBipartiteMatching s(2, 2);
        s.add_edge(0, 0, 5);
        s.add_edge(0, 1, 1);
        s.add_edge(1, 0, 2);
        s.add_edge(1, 1, 4);
        auto r = s.solve_exact(2);
        assert(r.feasible && r.flow == 2 && r.cost == 3 && (int)r.selected_ids.size() == 2);
        auto bad = s.solve_exact(3);
        assert(!bad.feasible && bad.flow == 2);
    }
    {
        McfMaxProfitBipartiteMatching s(2, 2);
        s.add_edge(0, 0, 5);
        s.add_edge(0, 1, 1);
        s.add_edge(1, 0, 2);
        s.add_edge(1, 1, 4);
        auto r = s.solve_exact(2);
        assert(r.feasible && r.picked == 2 && r.profit == 9);
        auto a = s.solve_at_most(1);
        assert(a.feasible && a.profit == 5);
        auto vals = s.solve_values(2);
        assert(vals[0] == 0 && vals[1] == 5 && vals[2] == 9);
        McfMaxProfitBipartiteMatching neg(1, 1);
        neg.add_edge(0, 0, -5);
        assert(neg.solve_exact(1).profit == -5);
        assert(neg.solve_at_most(1).profit == 0);
    }
    {
        McfMinCostBipartiteFlow s(2, 2);
        s.set_left_cap(0, 2);
        s.set_left_cap(1, 1);
        s.set_right_cap(0, 1);
        s.set_right_cap(1, 2);
        s.add_edge(0, 0, 1, 4);
        s.add_edge(0, 1, 2, 1);
        s.add_edge(1, 0, 1, 2);
        s.add_edge(1, 1, 1, 5);
        auto r = s.solve_exact(3);
        assert(r.feasible && r.flow == 3 && r.cost == 4);
    }
    {
        McfMaxProfitBipartiteFlow s(2, 2);
        s.set_left_cap(0, 2);
        s.set_left_cap(1, 1);
        s.set_right_cap(0, 1);
        s.set_right_cap(1, 2);
        s.add_edge(0, 0, 1, 4);
        s.add_edge(0, 1, 2, 1);
        s.add_edge(1, 0, 1, 5);
        s.add_edge(1, 1, 1, 10);
        auto r = s.solve_exact(3);
        assert(r.feasible && r.picked == 3 && r.profit == 15);
        assert(s.solve_at_most(2).profit == 14);
        auto vals = s.solve_values(3);
        assert(vals[0] == 0 && vals[1] == 10 && vals[2] == 14 && vals[3] == 15);
    }
    {
        McfMaxProfitMatrixPick s(2, 2);
        s.add_cell(0, 0, 5);
        s.add_cell(0, 1, 1);
        s.add_cell(1, 0, 2);
        s.add_cell(1, 1, 4);
        assert(s.solve_exact(2).profit == 9);
        assert(s.solve_at_most(1).profit == 5);
    }
    {
        McfMaxProfitGridPairing s(2, 2);
        s.add_pair(0, 0, 0, 1, 5);
        s.add_pair(0, 0, 1, 0, 3);
        s.add_pair(1, 1, 0, 1, 4);
        s.add_pair(1, 1, 1, 0, 6);
        assert(s.solve_exact(2).profit == 11);
        assert(s.solve_at_most(1).profit == 6);
    }
    {
        McfMaxProfitIntervalKSelection s;
        s.add_interval(0, 2, 5);
        s.add_interval(1, 3, 6);
        s.add_interval(2, 4, 4);
        assert(s.solve(1).profit == 9);
        assert(s.solve(2).profit == 15);
    }
    {
        McfMinCostSupplyDemandFlow s(4);
        s.add_supply(0, 2);
        s.add_supply(1, 1);
        s.add_demand(2, 1);
        s.add_demand(3, 2);
        s.add_edge(0, 2, 1, 4);
        s.add_edge(0, 3, 2, 1);
        s.add_edge(1, 2, 1, 2);
        s.add_edge(1, 3, 1, 5);
        auto r = s.solve();
        assert(r.feasible && r.flow == 3 && r.cost == 4);
        McfMinCostSupplyDemandFlow bad(2);
        bad.add_supply(0, 1);
        assert(!bad.solve().feasible);
    }
    {
        McfMinCostKPaths s(3);
        s.add_edge(0, 1, 1, 1);
        s.add_edge(1, 2, 1, 1);
        s.add_edge(0, 2, 1, 5);
        auto r = s.solve(0, 2, 2);
        assert(r.feasible && r.flow == 2 && r.cost == 7);
        assert(!s.solve(0, 2, 3).feasible);
    }
    std::cout << "fixed tests passed\n";

    for (int tc = 0; tc < 300; tc++) {
        int L = 1 + rnd_mod(4), R = 1 + rnd_mod(4);
        std::vector<std::tuple<int, int, ll>> es;
        for (int l = 0; l < L; l++) for (int r = 0; r < R; r++) if (rnd_mod(2)) {
            es.push_back({l, r, static_cast<ll>(rnd_mod(11)) - 3});
        }
        if ((int)es.size() > 10) es.resize(10);
        int kmax = std::min({L, R, (int)es.size()});
        int k = rnd_mod(kmax + 2);

        McfMinCostBipartiteMatching mn(L, R);
        for (auto [l, r, p] : es) mn.add_edge(l, r, p + 3);
        std::vector<std::tuple<int, int, ll>> ces;
        for (auto [l, r, p] : es) ces.push_back({l, r, p + 3});
        auto br_min = brute_matching_min(L, R, ces, k);
        auto rr_min = mn.solve_exact(k);
        assert((br_min < INF) == rr_min.feasible);
        if (br_min < INF) assert(rr_min.cost == br_min);

        McfMaxProfitBipartiteMatching mx(L, R);
        for (auto [l, r, p] : es) mx.add_edge(l, r, p);
        auto br_exact = brute_matching_max_exact(L, R, es, k);
        auto rr_exact = mx.solve_exact(k);
        assert((br_exact > NEG_INF) == rr_exact.feasible);
        if (br_exact > NEG_INF) assert(rr_exact.profit == br_exact);
        assert(mx.solve_at_most(k).profit == brute_matching_max_at_most(L, R, es, k));
        auto vals = mx.solve_values(kmax + 1);
        for (int f = 0; f <= kmax + 1; f++) {
            auto b = brute_matching_max_exact(L, R, es, f);
            if (b == NEG_INF) assert(vals[f] == McfMaxProfitBipartiteMatching::NEG_INF);
            else assert(vals[f] == b);
        }
    }
    std::cout << "random matching tests passed\n";

    for (int tc = 0; tc < 250; tc++) {
        int L = 1 + rnd_mod(3), R = 1 + rnd_mod(3);
        std::vector<ll> lc(L), rc(R);
        for (int i = 0; i < L; i++) lc[i] = rnd_mod(3);
        for (int i = 0; i < R; i++) rc[i] = rnd_mod(3);
        std::vector<std::tuple<int, int, ll, ll>> es;
        for (int l = 0; l < L; l++) for (int r = 0; r < R; r++) if (rnd_mod(2)) {
            es.push_back({l, r, static_cast<ll>(rnd_mod(3)), static_cast<ll>(rnd_mod(9)) - 3});
        }
        int k = rnd_mod(5);

        McfMinCostBipartiteFlow mn(L, R);
        McfMaxProfitBipartiteFlow mx(L, R);
        for (int i = 0; i < L; i++) mn.set_left_cap(i, lc[i]), mx.set_left_cap(i, lc[i]);
        for (int i = 0; i < R; i++) mn.set_right_cap(i, rc[i]), mx.set_right_cap(i, rc[i]);
        std::vector<std::tuple<int, int, ll, ll>> cost_edges;
        for (auto [l, r, cap, p] : es) {
            mn.add_edge(l, r, cap, p + 3);
            mx.add_edge(l, r, cap, p);
            cost_edges.push_back({l, r, cap, p + 3});
        }
        auto bmin = brute_bipartite_flow(L, R, lc, rc, cost_edges, k, false, false);
        auto rmin = mn.solve_exact(k);
        assert((bmin < INF) == rmin.feasible);
        if (bmin < INF) assert(rmin.cost == bmin);

        auto bmax = brute_bipartite_flow(L, R, lc, rc, es, k, true, false);
        auto rmax = mx.solve_exact(k);
        assert((bmax > NEG_INF) == rmax.feasible);
        if (bmax > NEG_INF) assert(rmax.profit == bmax);
        auto bat = brute_bipartite_flow(L, R, lc, rc, es, k, true, true);
        assert(mx.solve_at_most(k).profit == bat);
        auto vals = mx.solve_values(k);
        for (int f = 0; f <= k; f++) {
            auto b = brute_bipartite_flow(L, R, lc, rc, es, f, true, false);
            if (b == NEG_INF) assert(vals[f] == McfMaxProfitBipartiteFlow::NEG_INF);
            else assert(vals[f] == b);
        }
    }
    std::cout << "random bipartite flow tests passed\n";

    for (int tc = 0; tc < 250; tc++) {
        int H = 1 + rnd_mod(3), W = 1 + rnd_mod(3);
        std::vector<ll> row_cap(H), col_cap(W);
        for (int y = 0; y < H; y++) row_cap[y] = rnd_mod(3);
        for (int x = 0; x < W; x++) col_cap[x] = rnd_mod(3);
        std::vector<std::tuple<int, int, ll>> cells;
        McfMaxProfitMatrixPick s(H, W, 0, 0);
        for (int y = 0; y < H; y++) s.set_row_cap(y, row_cap[y]);
        for (int x = 0; x < W; x++) s.set_col_cap(x, col_cap[x]);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (rnd_mod(2)) {
            ll p = static_cast<ll>(rnd_mod(10)) - 3;
            cells.push_back({y, x, p});
            s.add_cell(y, x, p);
        }
        int k = rnd_mod(H * W + 2);
        auto be = brute_matrix_exact(H, W, row_cap, col_cap, cells, k);
        auto re = s.solve_exact(k);
        assert((be > NEG_INF) == re.feasible);
        if (be > NEG_INF) assert(re.profit == be);
        assert(s.solve_at_most(k).profit == brute_matrix_at_most(H, W, row_cap, col_cap, cells, k));
    }
    std::cout << "random matrix tests passed\n";

    for (int tc = 0; tc < 200; tc++) {
        int H = 1 + rnd_mod(3), W = 1 + rnd_mod(3);
        McfMaxProfitGridPairing s(H, W);
        std::vector<std::tuple<int, int, int, int, ll>> es;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            const int dy[2] = {1, 0};
            const int dx[2] = {0, 1};
            for (int d = 0; d < 2; d++) {
                int ny = y + dy[d], nx = x + dx[d];
                if (ny >= H || nx >= W || !(rnd_mod(2))) continue;
                ll p = static_cast<ll>(rnd_mod(10)) - 3;
                es.push_back({y, x, ny, nx, p});
                s.add_pair(y, x, ny, nx, p);
            }
        }
        int k = rnd_mod(H * W / 2 + 2);
        auto be = brute_grid(H, W, es, k, true);
        auto re = s.solve_exact(k);
        assert((be > NEG_INF) == re.feasible);
        if (be > NEG_INF) assert(re.profit == be);
        assert(s.solve_at_most(k).profit == brute_grid(H, W, es, k, false));
    }
    std::cout << "random grid pairing tests passed\n";

    for (int tc = 0; tc < 300; tc++) {
        McfMaxProfitIntervalKSelection s;
        std::vector<std::tuple<ll, ll, ll>> iv;
        int m = rnd_mod(9);
        for (int i = 0; i < m; i++) {
            ll l = rnd_mod(6);
            ll r = rnd_mod(6);
            if (l == r) r = (r + 1) % 6;
            if (l > r) std::swap(l, r);
            ll p = static_cast<ll>(rnd_mod(12)) - 3;
            iv.push_back({l, r, p});
            s.add_interval(l, r, p);
        }
        int k = rnd_mod(4);
        assert(s.solve(k).profit == brute_interval(iv, k));
    }
    std::cout << "random interval tests passed\n";

    for (int tc = 0; tc < 120; tc++) {
        int n = 2 + (rnd_mod(3));
        std::vector<ll> supply(n, 0), demand(n, 0);
        int total = rnd_mod(4);
        for (int i = 0; i < total; i++) supply[rnd_mod(n)]++;
        for (int i = 0; i < total; i++) demand[rnd_mod(n)]++;
        McfMinCostSupplyDemandFlow s(n);
        for (int v = 0; v < n; v++) {
            s.add_supply(v, supply[v]);
            s.add_demand(v, demand[v]);
        }
        std::vector<std::tuple<int, int, ll, ll>> es;
        for (int i = 0; i < 5; i++) {
            int a = rnd_mod(n), b = rnd_mod(n);
            ll cap = rnd_mod(3), cost = rnd_mod(6);
            es.push_back({a, b, cap, cost});
            s.add_edge(a, b, cap, cost);
        }
        auto b = brute_supply_demand(n, supply, demand, es);
        auto r = s.solve();
        assert((b < INF) == r.feasible);
        if (b < INF) assert(r.cost == b);
    }
    std::cout << "random supply-demand tests passed\n";

    for (int tc = 0; tc < 120; tc++) {
        int n = 2 + (rnd_mod(3));
        int s_node = 0, t_node = n - 1;
        McfMinCostKPaths s(n);
        std::vector<std::tuple<int, int, ll, ll>> es;
        for (int i = 0; i < 6; i++) {
            int a = rnd_mod(n), b = rnd_mod(n);
            if (a == b) b = (b + 1) % n;
            ll cap = rnd_mod(3), cost = rnd_mod(6);
            es.push_back({a, b, cap, cost});
            s.add_edge(a, b, cap, cost);
        }
        int k = rnd_mod(4);
        auto b = brute_k_paths(n, s_node, t_node, k, es);
        auto r = s.solve(s_node, t_node, k);
        assert((b < INF) == r.feasible);
        if (b < INF) assert(r.cost == b);
    }
    std::cout << "random k-path tests passed\n";
    std::cout << "all tests passed\n";
    return 0;
}
#endif
