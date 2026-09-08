#pragma once
#include <bits/stdc++.h>
#include <atcoder/scc>
using namespace std;

// SCC分解と縮約DAGを扱う競技プログラミング用ライブラリ
// 元グラフは vector<vector<int>> と同じ隣接リスト形式で扱い、辺の行き先型は bool 以外の整数型を受け付ける
// atcoder::scc_graph で強連結成分に縮約し、元頂点からSCC頂点、SCC頂点から元頂点集合、SCC後のDAGを提供する
// 後半のユースケースソルバー関数は、SccDagの典型的な使い方をそのままコピペしやすいサンプルとして実装している

// ============================================================================
// SccDag定義
// ============================================================================

struct SccDag {
    int n = 0;
    int scc_n = 0;
    vector<int> comp;
    vector<vector<int>> groups;
    vector<vector<int>> dag;
    vector<int> indeg;
    vector<int> outdeg;

    SccDag() = default;

    // 隣接リストgをSCC分解し、縮約DAGと変換表を構築する O(N+M)
    template <class T>
    explicit SccDag(const vector<vector<T>>& g) {
        build(g);
    }

    // 隣接リストgをSCC分解し、縮約DAGと変換表を再構築する O(N+M)
    template <class T>
    void build(const vector<vector<T>>& g) {
        using U = remove_cv_t<T>;
        static_assert(is_integral_v<U> && !is_same_v<U, bool>, "SccDag requires vector<vector<integer>>, excluding bool");

        n = (int)g.size();

        // ACLのscc_graphへ元グラフの辺をそのまま追加する
        // 行き先がint範囲に収まることは利用側の前提とし、ここでは検査しない
        atcoder::scc_graph scc(n);
        for (int u = 0; u < n; u++) {
            for (T x : g[u]) {
                scc.add_edge(u, (int)x);
            }
        }

        // ACLのSCC順はトポロジカル順なので、縮約DAG上のDPでそのまま使える
        groups = scc.scc();
        scc_n = (int)groups.size();

        // 元頂点からSCC頂点への変換表を作る
        comp.assign(n, -1);
        for (int c = 0; c < scc_n; c++) {
            for (int v : groups[c]) {
                comp[v] = c;
            }
        }

        // SCCごとに元頂点の外向き辺を走査し、SCC DAGを重複なしで作る
        // seen[d] == c なら、今のSCC c から d への辺はすでに追加済み
        dag.assign(scc_n, {});
        indeg.assign(scc_n, 0);
        outdeg.assign(scc_n, 0);
        vector<int> seen(scc_n, -1);
        for (int c = 0; c < scc_n; c++) {
            for (int u : groups[c]) {
                for (T x : g[u]) {
                    int d = comp[(int)x];
                    if (c == d) continue;
                    if (seen[d] == c) continue;
                    seen[d] = c;
                    dag[c].push_back(d);
                    outdeg[c]++;
                    indeg[d]++;
                }
            }
        }
    }

    // 元頂点vが属するSCC頂点番号を返す O(1)
    int id(int v) const {
        return comp[v];
    }

    // 元頂点uとvが同じSCCに属するかを返す O(1)
    bool same(int u, int v) const {
        return comp[u] == comp[v];
    }

    // SCC頂点cに含まれる元頂点数を返す O(1)
    int size(int c) const {
        return (int)groups[c].size();
    }

    // SCC頂点cに含まれる元頂点一覧を返す O(1)
    const vector<int>& vertices(int c) const {
        return groups[c];
    }

    // SCC DAGの逆向き隣接リストを生成して返す O(C+E)
    vector<vector<int>> rev_dag() const {
        vector<vector<int>> rev(scc_n);
        for (int c = 0; c < scc_n; c++) {
            for (int to : dag[c]) {
                rev[to].push_back(c);
            }
        }
        return rev;
    }
};

// ============================================================================
// ユースケースソルバー関数
// ============================================================================

// グラフ全体が強連結かを判定する O(N+M)
template <class T>
bool scc_is_strongly_connected(const vector<vector<T>>& g) {
    SccDag sg(g);
    return sg.scc_n <= 1;
}

// 有向閉路が存在するかを判定する O(N+M)
template <class T>
bool scc_has_cycle(const vector<vector<T>>& g) {
    SccDag sg(g);

    // サイズ2以上のSCCは必ず有向閉路を含む
    for (const auto& vs : sg.groups) {
        if ((int)vs.size() >= 2) return true;
    }

    // サイズ1のSCCでも自己ループがあれば有向閉路になる
    int n = (int)g.size();
    for (int u = 0; u < n; u++) {
        for (T x : g[u]) {
            if (u == (int)x) return true;
        }
    }
    return false;
}

// 各元頂点が何らかの有向閉路上にあるかを0/1で返す O(N+M)
template <class T>
vector<int> scc_cycle_vertex_flags(const vector<vector<T>>& g) {
    SccDag sg(g);
    vector<int> cyclic(sg.scc_n, 0);

    // サイズ2以上のSCCは、含まれる全頂点が閉路上にある
    for (int c = 0; c < sg.scc_n; c++) {
        if ((int)sg.groups[c].size() >= 2) cyclic[c] = 1;
    }

    // サイズ1のSCCは自己ループがあるときだけ閉路上にある
    int n = (int)g.size();
    for (int u = 0; u < n; u++) {
        for (T x : g[u]) {
            if (u == (int)x) cyclic[sg.comp[u]] = 1;
        }
    }

    vector<int> res(n, 0);
    for (int v = 0; v < n; v++) {
        res[v] = cyclic[sg.comp[v]];
    }
    return res;
}

// 相互到達可能なunordered pairの個数を数える O(N+M)
template <class T>
long long scc_count_mutual_unordered_pairs(const vector<vector<T>>& g) {
    SccDag sg(g);
    long long ans = 0;
    for (const auto& vs : sg.groups) {
        long long s = (long long)vs.size();
        ans += s * (s - 1) / 2;
    }
    return ans;
}

// 各クエリ(u,v)についてuとvが相互到達可能かを0/1で返す O(N+M+Q)
template <class T>
vector<int> scc_same_component_queries(const vector<vector<T>>& g, const vector<pair<int, int>>& queries) {
    SccDag sg(g);
    vector<int> res;
    res.reserve(queries.size());
    for (auto [u, v] : queries) {
        res.push_back(sg.same(u, v) ? 1 : 0);
    }
    return res;
}

// 入次数0のSCCに属する元頂点を0/1で返す O(N+M)
template <class T>
vector<int> scc_source_vertex_flags(const vector<vector<T>>& g) {
    SccDag sg(g);
    vector<int> res(sg.n, 0);

    // SCC DAGのsourceは、外部から到達されない開始候補になる
    for (int c = 0; c < sg.scc_n; c++) {
        if (sg.indeg[c] != 0) continue;
        for (int v : sg.groups[c]) res[v] = 1;
    }
    return res;
}

// 出次数0のSCCに属する元頂点を0/1で返す O(N+M)
template <class T>
vector<int> scc_sink_vertex_flags(const vector<vector<T>>& g) {
    SccDag sg(g);
    vector<int> res(sg.n, 0);

    // SCC DAGのsinkは、外へ出られない終端・吸収候補になる
    for (int c = 0; c < sg.scc_n; c++) {
        if (sg.outdeg[c] != 0) continue;
        for (int v : sg.groups[c]) res[v] = 1;
    }
    return res;
}

// いくつの始点を選べば全頂点へ到達できるかの最小値を返す O(N+M)
template <class T>
int scc_min_start_vertices_to_reach_all(const vector<vector<T>>& g) {
    SccDag sg(g);
    int ans = 0;
    for (int c = 0; c < sg.scc_n; c++) {
        if (sg.indeg[c] == 0) ans++;
    }
    return ans;
}

// 全頂点から到達可能な元頂点集合を返し、存在しなければ空配列を返す O(N+M+KlogK)
template <class T>
vector<int> scc_vertices_reachable_from_all(const vector<vector<T>>& g) {
    SccDag sg(g);
    int sink = -1;
    int cnt = 0;

    // 全頂点から到達できる候補は、sink SCCが一意な場合のそのSCCだけ
    for (int c = 0; c < sg.scc_n; c++) {
        if (sg.outdeg[c] == 0) {
            sink = c;
            cnt++;
        }
    }
    if (cnt != 1) return {};

    vector<int> ans = sg.groups[sink];
    sort(ans.begin(), ans.end());
    return ans;
}

// 全頂点へ到達可能な元頂点集合を返し、存在しなければ空配列を返す O(N+M+KlogK)
template <class T>
vector<int> scc_vertices_can_reach_all(const vector<vector<T>>& g) {
    SccDag sg(g);
    int source = -1;
    int cnt = 0;

    // 全頂点へ到達できる候補は、source SCCが一意な場合のそのSCCだけ
    for (int c = 0; c < sg.scc_n; c++) {
        if (sg.indeg[c] == 0) {
            source = c;
            cnt++;
        }
    }
    if (cnt != 1) return {};

    vector<int> ans = sg.groups[source];
    sort(ans.begin(), ans.end());
    return ans;
}

// グラフ全体を強連結にするために追加すべき辺数の最小値を返す O(N+M)
template <class T>
int scc_min_edges_to_strongly_connect(const vector<vector<T>>& g) {
    SccDag sg(g);
    if (sg.scc_n <= 1) return 0;

    // 縮約DAGのsource数とsink数の大きい方が、強連結化に必要な最小追加辺数
    int sources = 0;
    int sinks = 0;
    for (int c = 0; c < sg.scc_n; c++) {
        if (sg.indeg[c] == 0) sources++;
        if (sg.outdeg[c] == 0) sinks++;
    }
    return max(sources, sinks);
}

// 任意始点でSCC内の重みを全回収できるときの最大重みパス値を返す O(N+M)
template <class T>
long long scc_max_weight_path_any_start(const vector<vector<T>>& g, const vector<long long>& w) {
    SccDag sg(g);
    if (sg.n == 0) return 0;

    // SCC内は互いに行き来できるため、同じSCCの重みはまとめて1回だけ足す
    vector<long long> sw(sg.scc_n, 0);
    for (int v = 0; v < sg.n; v++) {
        sw[sg.comp[v]] += w[v];
    }

    // ACLのSCC順はトポロジカル順なので、そのまま前からDAG DPできる
    vector<long long> dp = sw;
    long long ans = *max_element(dp.begin(), dp.end());
    for (int c = 0; c < sg.scc_n; c++) {
        ans = max(ans, dp[c]);
        for (int to : sg.dag[c]) {
            dp[to] = max(dp[to], dp[c] + sw[to]);
        }
    }
    return ans;
}

// 指定始点sからSCC内の重みを全回収できるときの最大重みパス値を返す O(N+M)
template <class T>
long long scc_max_weight_path_from_start(const vector<vector<T>>& g, const vector<long long>& w, int s) {
    SccDag sg(g);
    const long long neg = -(1LL << 60);

    // SCCごとの重み和を作る
    vector<long long> sw(sg.scc_n, 0);
    for (int v = 0; v < sg.n; v++) {
        sw[sg.comp[v]] += w[v];
    }

    // 始点のSCCだけを初期化し、到達できるSCCにだけ値を流す
    vector<long long> dp(sg.scc_n, neg);
    int cs = sg.comp[s];
    dp[cs] = sw[cs];
    long long ans = sw[cs];
    for (int c = 0; c < sg.scc_n; c++) {
        if (dp[c] == neg) continue;
        ans = max(ans, dp[c]);
        for (int to : sg.dag[c]) {
            dp[to] = max(dp[to], dp[c] + sw[to]);
        }
    }
    return ans;
}

// 指定始点sから指定終点tへ到達する最大重みパス値を返し、到達不能なら-(1LL<<60)を返す O(N+M)
template <class T>
long long scc_max_weight_path_start_to_goal(const vector<vector<T>>& g, const vector<long long>& w, int s, int t) {
    SccDag sg(g);
    const long long neg = -(1LL << 60);

    // SCCごとの重み和を作る
    vector<long long> sw(sg.scc_n, 0);
    for (int v = 0; v < sg.n; v++) {
        sw[sg.comp[v]] += w[v];
    }

    // 始点SCCからDAG DPし、終点SCCの値だけを答えにする
    vector<long long> dp(sg.scc_n, neg);
    int cs = sg.comp[s];
    int ct = sg.comp[t];
    dp[cs] = sw[cs];
    for (int c = 0; c < sg.scc_n; c++) {
        if (dp[c] == neg) continue;
        for (int to : sg.dag[c]) {
            dp[to] = max(dp[to], dp[c] + sw[to]);
        }
    }
    return dp[ct];
}

// 各クエリ(u,v)についてuからvへ到達可能かを0/1で返す O((C+E)C/64+Q)
template <class T>
vector<int> scc_reachability_queries(const vector<vector<T>>& g, const vector<pair<int, int>>& queries) {
    SccDag sg(g);
    int C = sg.scc_n;
    int B = (C + 63) >> 6;
    vector<vector<unsigned long long>> reach(C, vector<unsigned long long>(B, 0));

    // DAGを後ろから処理し、各SCCから到達できるSCC集合をbitsetで持つ
    for (int c = C - 1; c >= 0; c--) {
        reach[c][c >> 6] |= 1ULL << (c & 63);
        for (int to : sg.dag[c]) {
            for (int b = 0; b < B; b++) {
                reach[c][b] |= reach[to][b];
            }
        }
    }

    vector<int> res;
    res.reserve(queries.size());
    for (auto [u, v] : queries) {
        int a = sg.comp[u];
        int b = sg.comp[v];
        res.push_back(((reach[a][b >> 6] >> (b & 63)) & 1ULL) ? 1 : 0);
    }
    return res;
}

// 各元頂点から到達可能な元頂点数を返す O((C+E)C/64+R)
template <class T>
vector<int> scc_reachable_vertex_count(const vector<vector<T>>& g) {
    SccDag sg(g);
    int C = sg.scc_n;
    int B = (C + 63) >> 6;
    vector<vector<unsigned long long>> reach(C, vector<unsigned long long>(B, 0));

    // SCC DAG上の到達可能成分集合を作る
    for (int c = C - 1; c >= 0; c--) {
        reach[c][c >> 6] |= 1ULL << (c & 63);
        for (int to : sg.dag[c]) {
            for (int b = 0; b < B; b++) {
                reach[c][b] |= reach[to][b];
            }
        }
    }

    // 到達可能成分集合を、元頂点数の和に変換する
    vector<int> comp_size(C, 0);
    for (int c = 0; c < C; c++) comp_size[c] = (int)sg.groups[c].size();

    vector<int> cnt_comp(C, 0);
    for (int c = 0; c < C; c++) {
        int cnt = 0;
        for (int b = 0; b < B; b++) {
            unsigned long long x = reach[c][b];
            while (x) {
                int k = __builtin_ctzll(x);
                int d = (b << 6) + k;
                if (d < C) cnt += comp_size[d];
                x &= x - 1;
            }
        }
        cnt_comp[c] = cnt;
    }

    vector<int> res(sg.n, 0);
    for (int v = 0; v < sg.n; v++) {
        res[v] = cnt_comp[sg.comp[v]];
    }
    return res;
}

// 到達可能なordered pair(u,v)の個数をu==vも含めて数える O((C+E)C/64+R)
template <class T>
long long scc_count_reachable_ordered_pairs(const vector<vector<T>>& g) {
    SccDag sg(g);
    int C = sg.scc_n;
    int B = (C + 63) >> 6;
    vector<vector<unsigned long long>> reach(C, vector<unsigned long long>(B, 0));

    // SCC DAG上で到達可能成分集合を作る
    for (int c = C - 1; c >= 0; c--) {
        reach[c][c >> 6] |= 1ULL << (c & 63);
        for (int to : sg.dag[c]) {
            for (int b = 0; b < B; b++) {
                reach[c][b] |= reach[to][b];
            }
        }
    }

    // 各SCC cについて、c内の各元頂点から到達できる元頂点数を足す
    vector<int> comp_size(C, 0);
    for (int c = 0; c < C; c++) comp_size[c] = (int)sg.groups[c].size();

    long long ans = 0;
    for (int c = 0; c < C; c++) {
        long long reach_vertices = 0;
        for (int b = 0; b < B; b++) {
            unsigned long long x = reach[c][b];
            while (x) {
                int k = __builtin_ctzll(x);
                int d = (b << 6) + k;
                if (d < C) reach_vertices += comp_size[d];
                x &= x - 1;
            }
        }
        ans += (long long)comp_size[c] * reach_vertices;
    }
    return ans;
}

// 何らかの有向閉路へ到達可能な元頂点を0/1で返す O(N+M)
template <class T>
vector<int> scc_can_reach_cycle_flags(const vector<vector<T>>& g) {
    SccDag sg(g);
    vector<int> cyclic(sg.scc_n, 0);

    // 閉路を持つSCCを判定する
    for (int c = 0; c < sg.scc_n; c++) {
        if ((int)sg.groups[c].size() >= 2) cyclic[c] = 1;
    }
    for (int u = 0; u < sg.n; u++) {
        for (T x : g[u]) {
            if (u == (int)x) cyclic[sg.comp[u]] = 1;
        }
    }

    // 閉路SCCから逆向きにたどると、その閉路へ到達できるSCCが分かる
    vector<vector<int>> rev = sg.rev_dag();
    vector<int> ok(sg.scc_n, 0);
    vector<int> st;
    for (int c = 0; c < sg.scc_n; c++) {
        if (cyclic[c]) {
            ok[c] = 1;
            st.push_back(c);
        }
    }
    while (!st.empty()) {
        int c = st.back();
        st.pop_back();
        for (int pre : rev[c]) {
            if (ok[pre]) continue;
            ok[pre] = 1;
            st.push_back(pre);
        }
    }

    vector<int> res(sg.n, 0);
    for (int v = 0; v < sg.n; v++) {
        res[v] = ok[sg.comp[v]];
    }
    return res;
}

// 始点sから到達可能な範囲に有向閉路が存在するかを判定する O(N+M)
template <class T>
bool scc_has_cycle_reachable_from_start(const vector<vector<T>>& g, int s) {
    SccDag sg(g);
    vector<int> cyclic(sg.scc_n, 0);

    // 閉路を持つSCCを判定する
    for (int c = 0; c < sg.scc_n; c++) {
        if ((int)sg.groups[c].size() >= 2) cyclic[c] = 1;
    }
    for (int u = 0; u < sg.n; u++) {
        for (T x : g[u]) {
            if (u == (int)x) cyclic[sg.comp[u]] = 1;
        }
    }

    // 始点SCCからDAG上を前向きに探索する
    vector<int> seen(sg.scc_n, 0);
    vector<int> st;
    int cs = sg.comp[s];
    seen[cs] = 1;
    st.push_back(cs);
    while (!st.empty()) {
        int c = st.back();
        st.pop_back();
        if (cyclic[c]) return true;
        for (int to : sg.dag[c]) {
            if (seen[to]) continue;
            seen[to] = 1;
            st.push_back(to);
        }
    }
    return false;
}

// sからtへの経路上に有向閉路が存在するかを判定する O(N+M)
template <class T>
bool scc_has_cycle_on_path(const vector<vector<T>>& g, int s, int t) {
    SccDag sg(g);
    vector<int> cyclic(sg.scc_n, 0);

    // 閉路を持つSCCを判定する
    for (int c = 0; c < sg.scc_n; c++) {
        if ((int)sg.groups[c].size() >= 2) cyclic[c] = 1;
    }
    for (int u = 0; u < sg.n; u++) {
        for (T x : g[u]) {
            if (u == (int)x) cyclic[sg.comp[u]] = 1;
        }
    }

    // sから到達可能なSCCを前向きにマークする
    vector<int> from_s(sg.scc_n, 0);
    vector<int> st;
    int cs = sg.comp[s];
    from_s[cs] = 1;
    st.push_back(cs);
    while (!st.empty()) {
        int c = st.back();
        st.pop_back();
        for (int to : sg.dag[c]) {
            if (from_s[to]) continue;
            from_s[to] = 1;
            st.push_back(to);
        }
    }

    // tへ到達可能なSCCを逆向きにマークする
    vector<vector<int>> rev = sg.rev_dag();
    vector<int> to_t(sg.scc_n, 0);
    int ct = sg.comp[t];
    to_t[ct] = 1;
    st.push_back(ct);
    while (!st.empty()) {
        int c = st.back();
        st.pop_back();
        for (int pre : rev[c]) {
            if (to_t[pre]) continue;
            to_t[pre] = 1;
            st.push_back(pre);
        }
    }

    // sから到達可能、かつtへ到達可能、かつ閉路を持つSCCがあれば条件を満たす
    for (int c = 0; c < sg.scc_n; c++) {
        if (from_s[c] && to_t[c] && cyclic[c]) return true;
    }
    return false;
}

// functional graphのサイクル上にある頂点を0/1で返す O(N)
vector<int> scc_functional_cycle_vertex_flags(const vector<int>& to) {
    int n = (int)to.size();
    vector<vector<int>> g(n);
    for (int v = 0; v < n; v++) {
        g[v].push_back(to[v]);
    }

    SccDag sg(g);
    vector<int> res(n, 0);

    // functional graphでは、サイズ2以上のSCCと自己ループがサイクルそのものになる
    for (int c = 0; c < sg.scc_n; c++) {
        if ((int)sg.groups[c].size() >= 2) {
            for (int v : sg.groups[c]) res[v] = 1;
        }
    }
    for (int v = 0; v < n; v++) {
        if (to[v] == v) res[v] = 1;
    }
    return res;
}

// 任意の2頂点が少なくとも片方向に到達可能なsemi-connectedグラフかを判定する O(N+M)
template <class T>
bool scc_is_semiconnected(const vector<vector<T>>& g) {
    SccDag sg(g);
    if (sg.scc_n <= 1) return true;

    // SCC DAGのトポロジカル順で隣り合うSCC間に辺があれば、全SCCを1本の有向パスで辿れる
    vector<int> ok(sg.scc_n - 1, 0);
    for (int c = 0; c < sg.scc_n; c++) {
        for (int to : sg.dag[c]) {
            if (to == c + 1) ok[c] = 1;
        }
    }
    for (int x : ok) {
        if (!x) return false;
    }
    return true;
}

// 有向オイラー閉路が存在するかを判定する O(N+M)
template <class T>
bool scc_has_euler_circuit(const vector<vector<T>>& g) {
    int n = (int)g.size();
    vector<int> indeg(n, 0), outdeg(n, 0);
    int edges = 0;

    // まず各頂点の入次数と出次数を数える
    for (int u = 0; u < n; u++) {
        for (T x : g[u]) {
            int v = (int)x;
            outdeg[u]++;
            indeg[v]++;
            edges++;
        }
    }
    if (edges == 0) return true;

    // オイラー閉路では全頂点で入次数と出次数が一致する必要がある
    for (int v = 0; v < n; v++) {
        if (indeg[v] != outdeg[v]) return false;
    }

    // 辺を持つ頂点だけが同じSCCに属していれば、孤立頂点は無視できる
    SccDag sg(g);
    int root = -1;
    for (int v = 0; v < n; v++) {
        if (indeg[v] + outdeg[v] == 0) continue;
        if (root == -1) root = sg.comp[v];
        else if (root != sg.comp[v]) return false;
    }
    return true;
}

// 有向オイラー路が存在するかを判定する O(N+M)
template <class T>
bool scc_has_euler_path(const vector<vector<T>>& g) {
    int n = (int)g.size();
    vector<int> indeg(n, 0), outdeg(n, 0);
    int edges = 0;

    // 次数条件を調べる
    for (int u = 0; u < n; u++) {
        for (T x : g[u]) {
            int v = (int)x;
            outdeg[u]++;
            indeg[v]++;
            edges++;
        }
    }
    if (edges == 0) return true;

    int start = -1;
    int goal = -1;
    for (int v = 0; v < n; v++) {
        int diff = outdeg[v] - indeg[v];
        if (diff == 1) {
            if (start != -1) return false;
            start = v;
        } else if (diff == -1) {
            if (goal != -1) return false;
            goal = v;
        } else if (diff != 0) {
            return false;
        }
    }
    if ((start == -1) != (goal == -1)) return false;

    // 閉路型なら、そのままSCCで連結性を確認する
    if (start == -1) {
        SccDag sg(g);
        int root = -1;
        for (int v = 0; v < n; v++) {
            if (indeg[v] + outdeg[v] == 0) continue;
            if (root == -1) root = sg.comp[v];
            else if (root != sg.comp[v]) return false;
        }
        return true;
    }

    // 路型なら、終点から始点へ仮辺を足すとオイラー閉路条件に帰着できる
    vector<vector<int>> h(n);
    for (int u = 0; u < n; u++) {
        h[u].reserve(g[u].size() + 1);
        for (T x : g[u]) h[u].push_back((int)x);
    }
    h[goal].push_back(start);

    SccDag sg(h);
    int root = -1;
    for (int v = 0; v < n; v++) {
        if (indeg[v] + outdeg[v] == 0) continue;
        if (root == -1) root = sg.comp[v];
        else if (root != sg.comp[v]) return false;
    }
    return true;
}

// ============================================================================
// テスト
// ============================================================================

#if __INCLUDE_LEVEL__ == 0
#include <cassert>

static vector<vector<int>> test_reachability(const vector<vector<int>>& g) {
    int n = (int)g.size();
    vector<vector<int>> reach(n, vector<int>(n, 0));
    for (int s = 0; s < n; s++) {
        vector<int> st = {s};
        reach[s][s] = 1;
        while (!st.empty()) {
            int u = st.back();
            st.pop_back();
            for (int v : g[u]) {
                if (reach[s][v]) continue;
                reach[s][v] = 1;
                st.push_back(v);
            }
        }
    }
    return reach;
}

struct TestSccInfo {
    int C = 0;
    vector<int> comp;
    vector<vector<int>> groups;
    vector<vector<int>> dag;
    vector<int> indeg;
    vector<int> outdeg;
};

static TestSccInfo test_naive_scc_info(const vector<vector<int>>& g) {
    int n = (int)g.size();
    auto reach = test_reachability(g);
    TestSccInfo info;
    info.comp.assign(n, -1);

    // 到達可能性行列から、相互到達可能な頂点を同じ成分にまとめる
    for (int v = 0; v < n; v++) {
        if (info.comp[v] != -1) continue;
        int c = info.C++;
        info.groups.push_back({});
        for (int u = 0; u < n; u++) {
            if (info.comp[u] == -1 && reach[v][u] && reach[u][v]) {
                info.comp[u] = c;
                info.groups[c].push_back(u);
            }
        }
    }

    // 成分間の辺を重複なしで作る
    info.dag.assign(info.C, {});
    info.indeg.assign(info.C, 0);
    info.outdeg.assign(info.C, 0);
    vector<vector<int>> has(info.C, vector<int>(info.C, 0));
    for (int u = 0; u < n; u++) {
        for (int v : g[u]) {
            int a = info.comp[u];
            int b = info.comp[v];
            if (a == b || has[a][b]) continue;
            has[a][b] = 1;
            info.dag[a].push_back(b);
            info.outdeg[a]++;
            info.indeg[b]++;
        }
    }
    return info;
}

static vector<int> test_naive_cycle_vertex_flags(const vector<vector<int>>& g) {
    int n = (int)g.size();
    auto reach = test_reachability(g);
    vector<int> res(n, 0);

    // 自己ループ、または自分以外の頂点との相互到達があれば閉路上にある
    for (int u = 0; u < n; u++) {
        for (int v : g[u]) {
            if (u == v) res[u] = 1;
        }
    }
    for (int v = 0; v < n; v++) {
        for (int u = 0; u < n; u++) {
            if (u != v && reach[v][u] && reach[u][v]) res[v] = 1;
        }
    }
    return res;
}

static bool test_naive_is_strongly_connected(const vector<vector<int>>& g) {
    int n = (int)g.size();
    auto reach = test_reachability(g);
    for (int u = 0; u < n; u++) {
        for (int v = 0; v < n; v++) {
            if (!reach[u][v]) return false;
        }
    }
    return true;
}

static long long test_naive_mutual_unordered_pairs(const vector<vector<int>>& g) {
    int n = (int)g.size();
    auto reach = test_reachability(g);
    long long ans = 0;
    for (int u = 0; u < n; u++) {
        for (int v = u + 1; v < n; v++) {
            if (reach[u][v] && reach[v][u]) ans++;
        }
    }
    return ans;
}

static vector<int> test_naive_source_flags(const vector<vector<int>>& g) {
    auto info = test_naive_scc_info(g);
    vector<int> res((int)g.size(), 0);
    for (int v = 0; v < (int)g.size(); v++) {
        res[v] = info.indeg[info.comp[v]] == 0;
    }
    return res;
}

static vector<int> test_naive_sink_flags(const vector<vector<int>>& g) {
    auto info = test_naive_scc_info(g);
    vector<int> res((int)g.size(), 0);
    for (int v = 0; v < (int)g.size(); v++) {
        res[v] = info.outdeg[info.comp[v]] == 0;
    }
    return res;
}

static vector<int> test_naive_vertices_reachable_from_all(const vector<vector<int>>& g) {
    int n = (int)g.size();
    auto reach = test_reachability(g);
    vector<int> ans;
    for (int v = 0; v < n; v++) {
        bool ok = true;
        for (int u = 0; u < n; u++) ok &= reach[u][v];
        if (ok) ans.push_back(v);
    }
    return ans;
}

static vector<int> test_naive_vertices_can_reach_all(const vector<vector<int>>& g) {
    int n = (int)g.size();
    auto reach = test_reachability(g);
    vector<int> ans;
    for (int u = 0; u < n; u++) {
        bool ok = true;
        for (int v = 0; v < n; v++) ok &= reach[u][v];
        if (ok) ans.push_back(u);
    }
    return ans;
}

static vector<int> test_topological_order(const vector<vector<int>>& dag) {
    int n = (int)dag.size();
    vector<int> indeg(n, 0);
    for (int u = 0; u < n; u++) {
        for (int v : dag[u]) indeg[v]++;
    }
    queue<int> q;
    for (int v = 0; v < n; v++) {
        if (indeg[v] == 0) q.push(v);
    }
    vector<int> topo;
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        topo.push_back(u);
        for (int v : dag[u]) {
            if (--indeg[v] == 0) q.push(v);
        }
    }
    assert((int)topo.size() == n);
    return topo;
}

static long long test_naive_max_weight_path_any_start(const vector<vector<int>>& g, const vector<long long>& w) {
    int n = (int)g.size();
    if (n == 0) return 0;
    auto info = test_naive_scc_info(g);
    vector<long long> sw(info.C, 0);
    for (int v = 0; v < n; v++) sw[info.comp[v]] += w[v];

    vector<int> topo = test_topological_order(info.dag);
    vector<long long> dp = sw;
    long long ans = *max_element(dp.begin(), dp.end());
    for (int c : topo) {
        ans = max(ans, dp[c]);
        for (int to : info.dag[c]) dp[to] = max(dp[to], dp[c] + sw[to]);
    }
    return ans;
}

static long long test_naive_max_weight_path_from_start(const vector<vector<int>>& g, const vector<long long>& w, int s) {
    auto info = test_naive_scc_info(g);
    vector<long long> sw(info.C, 0);
    for (int v = 0; v < (int)g.size(); v++) sw[info.comp[v]] += w[v];

    const long long neg = -(1LL << 60);
    vector<int> topo = test_topological_order(info.dag);
    vector<long long> dp(info.C, neg);
    int cs = info.comp[s];
    dp[cs] = sw[cs];
    long long ans = sw[cs];
    for (int c : topo) {
        if (dp[c] == neg) continue;
        ans = max(ans, dp[c]);
        for (int to : info.dag[c]) dp[to] = max(dp[to], dp[c] + sw[to]);
    }
    return ans;
}

static long long test_naive_max_weight_path_start_to_goal(const vector<vector<int>>& g, const vector<long long>& w, int s, int t) {
    auto info = test_naive_scc_info(g);
    vector<long long> sw(info.C, 0);
    for (int v = 0; v < (int)g.size(); v++) sw[info.comp[v]] += w[v];

    const long long neg = -(1LL << 60);
    vector<int> topo = test_topological_order(info.dag);
    vector<long long> dp(info.C, neg);
    int cs = info.comp[s];
    int ct = info.comp[t];
    dp[cs] = sw[cs];
    for (int c : topo) {
        if (dp[c] == neg) continue;
        for (int to : info.dag[c]) dp[to] = max(dp[to], dp[c] + sw[to]);
    }
    return dp[ct];
}

static vector<int> test_naive_reachable_vertex_count(const vector<vector<int>>& g) {
    int n = (int)g.size();
    auto reach = test_reachability(g);
    vector<int> res(n, 0);
    for (int u = 0; u < n; u++) {
        for (int v = 0; v < n; v++) res[u] += reach[u][v];
    }
    return res;
}

static long long test_naive_reachable_ordered_pairs(const vector<vector<int>>& g) {
    int n = (int)g.size();
    auto reach = test_reachability(g);
    long long ans = 0;
    for (int u = 0; u < n; u++) {
        for (int v = 0; v < n; v++) ans += reach[u][v];
    }
    return ans;
}

static vector<int> test_naive_can_reach_cycle_flags(const vector<vector<int>>& g) {
    int n = (int)g.size();
    auto reach = test_reachability(g);
    auto cyc = test_naive_cycle_vertex_flags(g);
    vector<int> res(n, 0);
    for (int u = 0; u < n; u++) {
        for (int v = 0; v < n; v++) {
            if (cyc[v] && reach[u][v]) res[u] = 1;
        }
    }
    return res;
}

static bool test_naive_has_cycle_on_path(const vector<vector<int>>& g, int s, int t) {
    int n = (int)g.size();
    auto reach = test_reachability(g);
    auto cyc = test_naive_cycle_vertex_flags(g);
    for (int v = 0; v < n; v++) {
        if (cyc[v] && reach[s][v] && reach[v][t]) return true;
    }
    return false;
}

static bool test_naive_semiconnected(const vector<vector<int>>& g) {
    int n = (int)g.size();
    auto reach = test_reachability(g);
    for (int u = 0; u < n; u++) {
        for (int v = 0; v < n; v++) {
            if (!reach[u][v] && !reach[v][u]) return false;
        }
    }
    return true;
}

static bool test_naive_euler_circuit(const vector<vector<int>>& g) {
    int n = (int)g.size();
    vector<int> indeg(n, 0), outdeg(n, 0);
    int edges = 0;
    for (int u = 0; u < n; u++) {
        for (int v : g[u]) {
            outdeg[u]++;
            indeg[v]++;
            edges++;
        }
    }
    if (edges == 0) return true;
    for (int v = 0; v < n; v++) {
        if (indeg[v] != outdeg[v]) return false;
    }

    auto reach = test_reachability(g);
    vector<int> active;
    for (int v = 0; v < n; v++) {
        if (indeg[v] + outdeg[v] > 0) active.push_back(v);
    }
    for (int u : active) {
        for (int v : active) {
            if (!reach[u][v]) return false;
        }
    }
    return true;
}

static bool test_naive_euler_path(const vector<vector<int>>& g) {
    int n = (int)g.size();
    vector<int> indeg(n, 0), outdeg(n, 0);
    int edges = 0;
    for (int u = 0; u < n; u++) {
        for (int v : g[u]) {
            outdeg[u]++;
            indeg[v]++;
            edges++;
        }
    }
    if (edges == 0) return true;

    int start = -1;
    int goal = -1;
    for (int v = 0; v < n; v++) {
        int diff = outdeg[v] - indeg[v];
        if (diff == 1) {
            if (start != -1) return false;
            start = v;
        } else if (diff == -1) {
            if (goal != -1) return false;
            goal = v;
        } else if (diff != 0) {
            return false;
        }
    }
    if ((start == -1) != (goal == -1)) return false;
    if (start == -1) return test_naive_euler_circuit(g);

    vector<vector<int>> h = g;
    h[goal].push_back(start);
    return test_naive_euler_circuit(h);
}

static void test_check_sccdag(const vector<vector<int>>& g) {
    int n = (int)g.size();
    auto reach = test_reachability(g);
    SccDag sg(g);
    assert(sg.n == n);
    assert((int)sg.comp.size() == n);
    assert((int)sg.groups.size() == sg.scc_n);
    assert((int)sg.dag.size() == sg.scc_n);
    assert((int)sg.indeg.size() == sg.scc_n);
    assert((int)sg.outdeg.size() == sg.scc_n);

    // groupsとcompが互いに整合しており、全元頂点がちょうど1回現れることを確認する
    vector<int> seen_vertex(n, 0);
    for (int c = 0; c < sg.scc_n; c++) {
        for (int v : sg.groups[c]) {
            assert(0 <= v && v < n);
            assert(sg.comp[v] == c);
            seen_vertex[v]++;
        }
    }
    for (int v = 0; v < n; v++) assert(seen_vertex[v] == 1);

    // 同じSCCであることと、相互到達可能であることが一致することを確認する
    for (int u = 0; u < n; u++) {
        for (int v = 0; v < n; v++) {
            assert(sg.same(u, v) == (bool)(reach[u][v] && reach[v][u]));
        }
    }

    // SCC DAGに自己辺や重複辺がなく、元グラフの成分間辺をすべて含むことを確認する
    vector<vector<int>> has(sg.scc_n, vector<int>(sg.scc_n, 0));
    vector<int> indeg(sg.scc_n, 0), outdeg(sg.scc_n, 0);
    for (int c = 0; c < sg.scc_n; c++) {
        for (int to : sg.dag[c]) {
            assert(0 <= to && to < sg.scc_n);
            assert(c != to);
            assert(c < to);
            assert(!has[c][to]);
            has[c][to] = 1;
            outdeg[c]++;
            indeg[to]++;
        }
    }
    for (int u = 0; u < n; u++) {
        for (int v : g[u]) {
            int a = sg.comp[u];
            int b = sg.comp[v];
            if (a != b) assert(has[a][b]);
        }
    }
    assert(indeg == sg.indeg);
    assert(outdeg == sg.outdeg);

    // rev_dagがdagの全辺を反転したものになっていることを確認する
    vector<vector<int>> rev = sg.rev_dag();
    vector<vector<int>> rev_has(sg.scc_n, vector<int>(sg.scc_n, 0));
    for (int c = 0; c < sg.scc_n; c++) {
        for (int pre : rev[c]) rev_has[c][pre] = 1;
    }
    for (int c = 0; c < sg.scc_n; c++) {
        for (int to : sg.dag[c]) assert(rev_has[to][c]);
    }
}

static void test_check_solvers(const vector<vector<int>>& g) {
    int n = (int)g.size();
    auto reach = test_reachability(g);
    auto info = test_naive_scc_info(g);
    auto cyc = test_naive_cycle_vertex_flags(g);

    assert(scc_is_strongly_connected(g) == test_naive_is_strongly_connected(g));
    assert(scc_has_cycle(g) == (find(cyc.begin(), cyc.end(), 1) != cyc.end()));
    assert(scc_cycle_vertex_flags(g) == cyc);
    assert(scc_count_mutual_unordered_pairs(g) == test_naive_mutual_unordered_pairs(g));
    assert(scc_source_vertex_flags(g) == test_naive_source_flags(g));
    assert(scc_sink_vertex_flags(g) == test_naive_sink_flags(g));

    int source_count = 0;
    int sink_count = 0;
    for (int c = 0; c < info.C; c++) {
        if (info.indeg[c] == 0) source_count++;
        if (info.outdeg[c] == 0) sink_count++;
    }
    assert(scc_min_start_vertices_to_reach_all(g) == source_count);
    assert(scc_min_edges_to_strongly_connect(g) == (info.C <= 1 ? 0 : max(source_count, sink_count)));

    assert(scc_vertices_reachable_from_all(g) == test_naive_vertices_reachable_from_all(g));
    assert(scc_vertices_can_reach_all(g) == test_naive_vertices_can_reach_all(g));
    assert(scc_reachable_vertex_count(g) == test_naive_reachable_vertex_count(g));
    assert(scc_count_reachable_ordered_pairs(g) == test_naive_reachable_ordered_pairs(g));
    assert(scc_can_reach_cycle_flags(g) == test_naive_can_reach_cycle_flags(g));
    assert(scc_is_semiconnected(g) == test_naive_semiconnected(g));
    assert(scc_has_euler_circuit(g) == test_naive_euler_circuit(g));
    assert(scc_has_euler_path(g) == test_naive_euler_path(g));

    vector<pair<int, int>> queries;
    for (int u = 0; u < n; u++) {
        for (int v = 0; v < n; v++) queries.push_back({u, v});
    }
    vector<int> same_ans = scc_same_component_queries(g, queries);
    vector<int> reach_ans = scc_reachability_queries(g, queries);
    for (int i = 0; i < (int)queries.size(); i++) {
        auto [u, v] = queries[i];
        assert(same_ans[i] == (reach[u][v] && reach[v][u]));
        assert(reach_ans[i] == reach[u][v]);
    }

    vector<long long> w(n);
    for (int v = 0; v < n; v++) w[v] = (long long)((v * 37 + n * 11) % 17) - 8;
    assert(scc_max_weight_path_any_start(g, w) == test_naive_max_weight_path_any_start(g, w));
    if (n > 0) {
        for (int s = 0; s < n; s++) {
            assert(scc_max_weight_path_from_start(g, w, s) == test_naive_max_weight_path_from_start(g, w, s));
            assert(scc_has_cycle_reachable_from_start(g, s) == (bool)test_naive_can_reach_cycle_flags(g)[s]);
            for (int t = 0; t < n; t++) {
                assert(scc_max_weight_path_start_to_goal(g, w, s, t) == test_naive_max_weight_path_start_to_goal(g, w, s, t));
                assert(scc_has_cycle_on_path(g, s, t) == test_naive_has_cycle_on_path(g, s, t));
            }
        }
    }
}

static void test_functional_graph(const vector<int>& to) {
    int n = (int)to.size();
    vector<vector<int>> g(n);
    for (int v = 0; v < n; v++) g[v].push_back(to[v]);
    assert(scc_functional_cycle_vertex_flags(to) == test_naive_cycle_vertex_flags(g));
}

int main() {
    vector<vector<vector<int>>> cases;

    cases.push_back({});
    cases.push_back({{}});
    cases.push_back({{0}});
    cases.push_back({{1}, {2}, {}});
    cases.push_back({{1}, {2}, {0}});
    cases.push_back({{1}, {0, 2}, {3}, {2}});
    cases.push_back({{1, 1, 2}, {2, 2}, {}});
    cases.push_back({{0, 1}, {2}, {1, 3}, {}});
    cases.push_back({{2}, {2}, {}});
    cases.push_back({{1}, {2}, {}, {2}});
    cases.push_back({{1, 2}, {2}, {0}, {4}, {3}});
    cases.push_back({{1}, {}, {3}, {}});
    cases.push_back({{1}, {2}, {3}, {1}, {2}});
    cases.push_back({{1}, {2}, {0}, {4}, {5}, {3}, {}});
    cases.push_back({{1, 1}, {2}, {0, 3}, {4}, {5}, {3}});

    for (const auto& g : cases) {
        test_check_sccdag(g);
        test_check_solvers(g);
    }

    // bool以外の整数型の行き先を受け取れることを確認する
    vector<vector<long long>> g_ll = {{1LL}, {2LL}, {0LL, 3LL}, {}};
    SccDag sg_ll(g_ll);
    assert(sg_ll.n == 4);
    assert(sg_ll.scc_n == 2);
    assert(scc_has_cycle(g_ll));
    assert((scc_reachable_vertex_count(g_ll) == vector<int>{4, 4, 4, 1}));

    // functional graphの典型ケースを確認する
    test_functional_graph({0});
    test_functional_graph({1, 2, 0});
    test_functional_graph({1, 2, 2, 2, 3});
    test_functional_graph({1, 2, 3, 1, 5, 4});

    // ランダムな小規模有向グラフで、到達可能性行列に基づくnaive実装と比較する
    mt19937 rng(123456789);
    int random_graphs = 3000;
    for (int tc = 0; tc < random_graphs; tc++) {
        int n = (int)(rng() % 9);
        vector<vector<int>> g(n);
        int max_edges = max(1, n * n * 2 + 1);
        int m = (n == 0) ? 0 : (int)(rng() % max_edges);
        for (int i = 0; i < m; i++) {
            int u = (int)(rng() % n);
            int v = (int)(rng() % n);
            g[u].push_back(v);
        }
        test_check_sccdag(g);
        test_check_solvers(g);
    }

    // ランダムなfunctional graphも別途確認する
    int random_functional_graphs = 1000;
    for (int tc = 0; tc < random_functional_graphs; tc++) {
        int n = 1 + (int)(rng() % 10);
        vector<int> to(n);
        for (int v = 0; v < n; v++) to[v] = (int)(rng() % n);
        test_functional_graph(to);
    }

    cout << "SccDag tests passed\n";
    cout << "hand graphs: " << cases.size() << '\n';
    cout << "random graphs: " << random_graphs << '\n';
    cout << "random functional graphs: " << random_functional_graphs << '\n';
    return 0;
}
#endif
