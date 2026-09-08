#pragma once
#include <bits/stdc++.h>

/*
非負重み無向グラフ上の exact Steiner tree を求めるライブラリ。
terminal 数が小さい場合に、部分集合 DP と最短路伝播により最小コストと使用辺集合を求める。
N が小さい場合は全点対最短路による dense モード、そうでない場合は CSR と radix heap による sparse モードを自動選択する。
頂点番号は 0-indexed、辺番号は add_edge で追加した順の 0-indexed とする。

典型的なユースケース 1: 最小コストと使用辺番号を出力する問題
    int n, m;
    cin >> n >> m;
    steiner_tree st(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        cin >> u >> v >> w;
        st.add_edge(u, v, w);  // 返り値は i と一致し、res.edges の辺番号になる
    }
    int k;
    cin >> k;
    vector<int> terminals(k);
    for (int &v : terminals) cin >> v;

    steiner_tree_result res = st.solve(terminals);
    if (!res.ok) {
        cout << -1 << '\n';
    } else {
        cout << res.cost << ' ' << res.edges.size() << '\n';
        for (int i = 0; i < (int)res.edges.size(); ++i) {
            if (i) cout << ' ';
            cout << res.edges[i];
        }
        cout << '\n';
    }

典型的なユースケース 2: 最小コストだけが必要な問題
    long long ans = st.solve_cost(terminals);
    if (ans == steiner_tree::inf()) {
        cout << -1 << '\n';
    } else {
        cout << ans << '\n';
    }
    solve_cost は使用辺集合の復元情報を持たないため、solve よりメモリ使用量が小さい。

典型的なユースケース 3: 入力が 1-indexed の問題
    --u;
    --v;
    st.add_edge(u, v, w);
    for (int &x : terminals) --x;
    このライブラリは常に 0-indexed を受け取り、0-indexed の辺番号を返す。

注意:
    無向グラフ、非負 long long 重みを前提とする。
    多重辺に対応し、自己ループは入力可能だが最短路改善には実質使われない。
    terminals の重複は solve 内で除去され、terminal が 0 個または 1 個ならコスト 0 を返す。
*/

struct steiner_tree_result {
    bool ok = false;
    long long cost = 0;
    std::vector<int> edges;
};

class steiner_tree {
public:
    struct edge_type {
        int from;
        int to;
        long long cost;
    };

private:
    struct arc_type {
        int to;
        int edge_id;
        long long cost;
    };

    class radix_heap {
    private:
        using key_type = unsigned long long;
        using value_type = int;

        std::array<std::vector<std::pair<key_type, value_type>>, 65> buckets_;
        key_type last_ = 0;
        size_t size_ = 0;

        static int bucket_index(key_type x, key_type last) {
            const key_type diff = x ^ last;
            if (diff == 0) return 0;
            return 64 - __builtin_clzll(diff);
        }

    public:
        radix_heap() = default;

        void clear() {
            for (auto &bucket : buckets_) bucket.clear();
            last_ = 0;
            size_ = 0;
        }

        bool empty() const {
            return size_ == 0;
        }

        void push(key_type key, value_type value) {
            buckets_[bucket_index(key, last_)].push_back({key, value});
            ++size_;
        }

        std::pair<key_type, value_type> pop() {
            // 最小 bucket が空なら、次の非空 bucket を最小キー基準で再分配する
            if (buckets_[0].empty()) {
                int id = 1;
                while (id < 65 && buckets_[id].empty()) ++id;
                assert(id < 65);

                key_type next_last = buckets_[id][0].first;
                for (const auto &[key, value] : buckets_[id]) {
                    (void)value;
                    if (key < next_last) next_last = key;
                }
                last_ = next_last;

                std::vector<std::pair<key_type, value_type>> moved;
                moved.swap(buckets_[id]);
                for (const auto &[key, value] : moved) {
                    buckets_[bucket_index(key, last_)].push_back({key, value});
                }
            }

            // bucket 0 はすべて last_ と同じキーなので、そのまま 1 要素を取り出す
            auto res = buckets_[0].back();
            buckets_[0].pop_back();
            --size_;
            return res;
        }
    };

    static constexpr long long INF_VALUE = std::numeric_limits<long long>::max() / 4;
    static constexpr int DENSE_MODE_MAX_N = 128;

    int n_ = 0;
    std::vector<edge_type> edges_;

    static bool use_dense_mode(int n) {
        return n <= DENSE_MODE_MAX_N;
    }

    void build_csr(std::vector<int> &start, std::vector<arc_type> &arcs) const {
        // 各頂点の次数を数え、CSR の開始位置を作る
        start.assign(n_ + 1, 0);
        for (const auto &e : edges_) {
            if (e.from == e.to) continue;
            ++start[e.from + 1];
            ++start[e.to + 1];
        }
        for (int i = 0; i < n_; ++i) start[i + 1] += start[i];

        // 無向辺を両向き arc として連続領域に詰める
        arcs.resize(start[n_]);
        std::vector<int> pos = start;
        // 追加順の edge_id を保ったまま arc に展開する
        for (int id = 0; id < static_cast<int>(edges_.size()); ++id) {
            const auto &e = edges_[id];
            if (e.from == e.to) continue;
            arcs[pos[e.from]++] = arc_type{e.to, id, e.cost};
            arcs[pos[e.to]++] = arc_type{e.from, id, e.cost};
        }
    }

    static std::vector<int> normalize_terminals(std::vector<int> terminals) {
        std::sort(terminals.begin(), terminals.end());
        terminals.erase(std::unique(terminals.begin(), terminals.end()), terminals.end());
        return terminals;
    }

    static void merge_subsets(int n, int mask, std::vector<long long> &dp, std::vector<int> *trace) {
        // 最下位 bit を必ず sub 側に含め、同じ分割の二重列挙を避ける
        const int low_bit = mask & -mask;
        const int rest = mask ^ low_bit;
        long long *dst = dp.data() + static_cast<size_t>(mask) * n;

        for (int s = rest;; s = (s - 1) & rest) {
            const int sub = s | low_bit;
            const int other = mask ^ sub;
            if (other != 0) {
                // 同じ終端頂点 v で 2 つの部分木を合体する
                const long long *a = dp.data() + static_cast<size_t>(sub) * n;
                const long long *b = dp.data() + static_cast<size_t>(other) * n;

                for (int v = 0; v < n; ++v) {
                    const long long cand = a[v] + b[v];
                    if (cand < dst[v]) {
                        dst[v] = cand;
                        if (trace != nullptr) {
                            (*trace)[static_cast<size_t>(mask) * n + v] = -sub;
                        }
                    }
                }
            }
            if (s == 0) break;
        }
    }

    template <bool Restore>
    steiner_tree_result solve_dense_impl(const std::vector<int> &terminals) const {
        const int terminal_count = static_cast<int>(terminals.size());
        const int dp_terminal_count = terminal_count - 1;
        const int states = 1 << dp_terminal_count;
        const int root = terminals.back();
        const size_t total_size = static_cast<size_t>(states) * static_cast<size_t>(n_);

        // dense モードでは全点対距離と、復元用の最短路末尾辺を持つ
        std::vector<long long> dist(static_cast<size_t>(n_) * n_, INF_VALUE);
        std::vector<int> prev_edge;
        if constexpr (Restore) prev_edge.assign(static_cast<size_t>(n_) * n_, -1);

        for (int i = 0; i < n_; ++i) dist[static_cast<size_t>(i) * n_ + i] = 0;
        // 多重辺は最短路に使える最小重みの辺だけを距離行列に反映する
        for (int id = 0; id < static_cast<int>(edges_.size()); ++id) {
            const auto &e = edges_[id];
            if (e.cost < dist[static_cast<size_t>(e.from) * n_ + e.to]) {
                dist[static_cast<size_t>(e.from) * n_ + e.to] = e.cost;
                dist[static_cast<size_t>(e.to) * n_ + e.from] = e.cost;
                if constexpr (Restore) {
                    prev_edge[static_cast<size_t>(e.from) * n_ + e.to] = id;
                    prev_edge[static_cast<size_t>(e.to) * n_ + e.from] = id;
                }
            }
        }

        // Floyd-Warshall で全点対最短路を求める
        for (int mid = 0; mid < n_; ++mid) {
            const long long *dist_mid = dist.data() + static_cast<size_t>(mid) * n_;
            for (int from = 0; from < n_; ++from) {
                long long *dist_from = dist.data() + static_cast<size_t>(from) * n_;
                const long long through_mid = dist_from[mid];
                if (through_mid >= INF_VALUE) continue;

                for (int to = 0; to < n_; ++to) {
                    const long long cand = through_mid + dist_mid[to];
                    if (cand < dist_from[to]) {
                        dist_from[to] = cand;
                        if constexpr (Restore) {
                            prev_edge[static_cast<size_t>(from) * n_ + to] = prev_edge[static_cast<size_t>(mid) * n_ + to];
                        }
                    }
                }
            }
        }

        // dp[mask][v] を flat 配列で持つ
        std::vector<long long> dp(total_size, INF_VALUE);
        std::vector<int> trace;
        if constexpr (Restore) trace.assign(total_size, 0);
        for (int i = 0; i < dp_terminal_count; ++i) {
            dp[static_cast<size_t>(1 << i) * n_ + terminals[i]] = 0;
        }

        std::vector<long long> src(n_);
        for (int mask = 1; mask < states; ++mask) {
            // 部分集合同士を同じ頂点でマージしてから、最短路で終端頂点を動かす
            merge_subsets(n_, mask, dp, Restore ? &trace : nullptr);

            long long *dst = dp.data() + static_cast<size_t>(mask) * n_;
            std::copy(dst, dst + n_, src.begin());
            for (int from = 0; from < n_; ++from) {
                // from を始点に全点対距離で一括緩和する
                const long long base = src[from];
                if (base >= INF_VALUE) continue;
                const long long *dist_from = dist.data() + static_cast<size_t>(from) * n_;

                for (int to = 0; to < n_; ++to) {
                    const long long cand = base + dist_from[to];
                    if (cand < dst[to]) {
                        dst[to] = cand;
                        if constexpr (Restore) {
                            trace[static_cast<size_t>(mask) * n_ + to] = from + 1;
                        }
                    }
                }
            }
        }

        const int full_mask = states - 1;
        const long long answer = dp[static_cast<size_t>(full_mask) * n_ + root];
        if (answer >= INF_VALUE) {
            return steiner_tree_result{false, INF_VALUE, {}};
        }
        if constexpr (!Restore) {
            return steiner_tree_result{true, answer, {}};
        } else {
            std::vector<int> used_edges;
            used_edges.reserve(n_ > 0 ? static_cast<size_t>(n_ - 1) : 0);

            auto add_path = [&](int from, int to) {
                // dense 伝播は中間頂点を trace に持たないため、最短路行列から辺列を展開する
                while (to != from) {
                    const int edge_id = prev_edge[static_cast<size_t>(from) * n_ + to];
                    assert(edge_id >= 0);
                    used_edges.push_back(edge_id);
                    const auto &e = edges_[edge_id];
                    to = (e.from == to ? e.to : e.from);
                }
            };

            auto restore = [&](auto &&self, int mask, int v) -> void {
                // 正なら最短路移動元、負なら部分集合分割、0 なら初期 terminal
                const int tr = trace[static_cast<size_t>(mask) * n_ + v];
                if (tr > 0) {
                    const int from = tr - 1;
                    add_path(from, v);
                    self(self, mask, from);
                    return;
                }
                if (tr < 0) {
                    const int sub = -tr;
                    self(self, sub, v);
                    self(self, mask ^ sub, v);
                    return;
                }
            };

            restore(restore, full_mask, root);
            std::sort(used_edges.begin(), used_edges.end());
            used_edges.erase(std::unique(used_edges.begin(), used_edges.end()), used_edges.end());
            return steiner_tree_result{true, answer, used_edges};
        }
    }

    template <bool Restore>
    steiner_tree_result solve_sparse_impl(const std::vector<int> &terminals) const {
        const int terminal_count = static_cast<int>(terminals.size());
        const int dp_terminal_count = terminal_count - 1;
        const int states = 1 << dp_terminal_count;
        const int root = terminals.back();
        const size_t total_size = static_cast<size_t>(states) * static_cast<size_t>(n_);

        // sparse モードでは CSR と multi-source Dijkstra で最短路伝播を行う
        std::vector<int> start;
        std::vector<arc_type> arcs;
        build_csr(start, arcs);

        std::vector<long long> dp(total_size, INF_VALUE);
        std::vector<int> trace;
        if constexpr (Restore) trace.assign(total_size, 0);
        for (int i = 0; i < dp_terminal_count; ++i) {
            dp[static_cast<size_t>(1 << i) * n_ + terminals[i]] = 0;
        }

        radix_heap heap;
        auto run_dijkstra = [&](int mask) {
            // 現在の dp[mask][*] 全体を始点集合として Dijkstra を走らせる
            long long *dist = dp.data() + static_cast<size_t>(mask) * n_;
            heap.clear();

            for (int v = 0; v < n_; ++v) {
                if (dist[v] < INF_VALUE) {
                    heap.push(static_cast<unsigned long long>(dist[v]), v);
                }
            }

            while (!heap.empty()) {
                const auto [raw_dist, v] = heap.pop();
                const long long cur_dist = static_cast<long long>(raw_dist);
                if (cur_dist != dist[v]) continue;

                for (int eid = start[v]; eid < start[v + 1]; ++eid) {
                    // 親辺を trace に保存すると、復元時に隣接辺探索が不要になる
                    const arc_type &arc = arcs[eid];
                    const long long nd = cur_dist + arc.cost;
                    if (nd < dist[arc.to]) {
                        dist[arc.to] = nd;
                        if constexpr (Restore) {
                            trace[static_cast<size_t>(mask) * n_ + arc.to] = arc.edge_id + 1;
                        }
                        heap.push(static_cast<unsigned long long>(nd), arc.to);
                    }
                }
            }
        };

        for (int mask = 1; mask < states; ++mask) {
            // 部分集合同士を同じ頂点でマージしてから、Dijkstra で終端頂点を動かす
            merge_subsets(n_, mask, dp, Restore ? &trace : nullptr);
            run_dijkstra(mask);
        }

        const int full_mask = states - 1;
        const long long answer = dp[static_cast<size_t>(full_mask) * n_ + root];
        if (answer >= INF_VALUE) {
            return steiner_tree_result{false, INF_VALUE, {}};
        }
        if constexpr (!Restore) {
            return steiner_tree_result{true, answer, {}};
        } else {
            std::vector<int> used_edges;
            used_edges.reserve(n_ > 0 ? static_cast<size_t>(n_ - 1) : 0);

            auto restore = [&](auto &&self, int mask, int v) -> void {
                // 正なら Dijkstra 親辺、負なら部分集合分割、0 なら初期 terminal
                const int tr = trace[static_cast<size_t>(mask) * n_ + v];
                if (tr > 0) {
                    const int edge_id = tr - 1;
                    used_edges.push_back(edge_id);
                    const auto &e = edges_[edge_id];
                    const int prev = (e.from == v ? e.to : e.from);
                    self(self, mask, prev);
                    return;
                }
                if (tr < 0) {
                    const int sub = -tr;
                    self(self, sub, v);
                    self(self, mask ^ sub, v);
                    return;
                }
            };

            restore(restore, full_mask, root);
            std::sort(used_edges.begin(), used_edges.end());
            used_edges.erase(std::unique(used_edges.begin(), used_edges.end()), used_edges.end());
            return steiner_tree_result{true, answer, used_edges};
        }
    }

    template <bool Restore>
    steiner_tree_result solve_impl(const std::vector<int> &input_terminals) const {
        // terminal の重複を除き、以降の root 選択を安定させる
        std::vector<int> terminals = normalize_terminals(input_terminals);
        for (int v : terminals) {
            assert(0 <= v && v < n_);
        }
        if (terminals.size() <= 1) {
            return steiner_tree_result{true, 0, {}};
        }

        const int dp_terminal_count = static_cast<int>(terminals.size()) - 1;
        assert(dp_terminal_count < 30);
        // 添付問題のように N が小さい場合は dense、それ以外は sparse を使う
        if (use_dense_mode(n_)) {
            return solve_dense_impl<Restore>(terminals);
        }
        return solve_sparse_impl<Restore>(terminals);
    }

public:
    // n 頂点の空グラフを作る: O(n)
    explicit steiner_tree(int n) : n_(n) {
        assert(n >= 0);
    }

    // 無向辺 u-v を重み w で追加し、追加した辺番号を返す: O(1)
    int add_edge(int u, int v, long long w) {
        assert(0 <= u && u < n_);
        assert(0 <= v && v < n_);
        assert(w >= 0);
        const int id = static_cast<int>(edges_.size());
        edges_.push_back(edge_type{u, v, w});
        return id;
    }

    // terminal 全体を連結する最小コストと使用辺集合を返す: O(3^K N + 2^K min(N^2, M log C) + N^3)
    steiner_tree_result solve(const std::vector<int> &terminals) const {
        return solve_impl<true>(terminals);
    }

    // terminal 全体を連結する最小コストを返し、不能なら inf() を返す: O(3^K N + 2^K min(N^2, M log C) + N^3)
    long long solve_cost(const std::vector<int> &terminals) const {
        steiner_tree_result res = solve_impl<false>(terminals);
        return res.ok ? res.cost : INF_VALUE;
    }

    // 到達不能を表す十分大きい値を返す: O(1)
    static constexpr long long inf() {
        return INF_VALUE;
    }

    // 追加済みの辺数を返す: O(1)
    int edge_count() const {
        return static_cast<int>(edges_.size());
    }

    // 辺番号 id の辺情報を返す: O(1)
    const edge_type &edge(int id) const {
        assert(0 <= id && id < static_cast<int>(edges_.size()));
        return edges_[id];
    }
};

#if __INCLUDE_LEVEL__ == 0

namespace steiner_tree_test {

long long reference_cost(int n, const std::vector<steiner_tree::edge_type> &edges, std::vector<int> terminals) {
    std::sort(terminals.begin(), terminals.end());
    terminals.erase(std::unique(terminals.begin(), terminals.end()), terminals.end());
    if (terminals.size() <= 1) return 0;

    const long long inf = steiner_tree::inf();
    std::vector<std::vector<long long>> dist(n, std::vector<long long>(n, inf));
    for (int i = 0; i < n; ++i) dist[i][i] = 0;
    for (const auto &e : edges) {
        if (e.cost < dist[e.from][e.to]) {
            dist[e.from][e.to] = e.cost;
            dist[e.to][e.from] = e.cost;
        }
    }
    for (int k = 0; k < n; ++k) {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                const long long nd = dist[i][k] + dist[k][j];
                if (nd < dist[i][j]) dist[i][j] = nd;
            }
        }
    }

    const int t = static_cast<int>(terminals.size()) - 1;
    const int states = 1 << t;
    std::vector<long long> dp(static_cast<size_t>(states) * n, inf);
    for (int i = 0; i < t; ++i) dp[static_cast<size_t>(1 << i) * n + terminals[i]] = 0;

    for (int mask = 1; mask < states; ++mask) {
        long long *dst = dp.data() + static_cast<size_t>(mask) * n;
        for (int sub = (mask - 1) & mask; sub > 0; sub = (sub - 1) & mask) {
            const int other = mask ^ sub;
            if (sub > other) continue;
            const long long *a = dp.data() + static_cast<size_t>(sub) * n;
            const long long *b = dp.data() + static_cast<size_t>(other) * n;
            for (int v = 0; v < n; ++v) {
                const long long cand = a[v] + b[v];
                if (cand < dst[v]) dst[v] = cand;
            }
        }

        std::vector<long long> next(dst, dst + n);
        for (int s = 0; s < n; ++s) {
            if (dst[s] >= inf) continue;
            for (int v = 0; v < n; ++v) {
                const long long cand = dst[s] + dist[s][v];
                if (cand < next[v]) next[v] = cand;
            }
        }
        std::copy(next.begin(), next.end(), dst);
    }
    return dp[static_cast<size_t>(states - 1) * n + terminals.back()];
}

bool check_result(int n, const std::vector<steiner_tree::edge_type> &edges, const std::vector<int> &terminals, const steiner_tree_result &res) {
    if (!res.ok) return false;
    long long sum = 0;
    std::vector<std::vector<int>> graph(n);
    for (int id : res.edges) {
        if (id < 0 || id >= static_cast<int>(edges.size())) return false;
        const auto &e = edges[id];
        sum += e.cost;
        graph[e.from].push_back(e.to);
        graph[e.to].push_back(e.from);
    }
    if (sum != res.cost) return false;
    if (terminals.empty()) return res.cost == 0;

    std::vector<int> seen(n, 0);
    std::queue<int> que;
    que.push(terminals[0]);
    seen[terminals[0]] = 1;
    while (!que.empty()) {
        const int v = que.front();
        que.pop();
        for (int to : graph[v]) {
            if (!seen[to]) {
                seen[to] = 1;
                que.push(to);
            }
        }
    }
    for (int v : terminals) {
        if (!seen[v]) return false;
    }
    return true;
}

}  // テスト用 namespace

int main() {
    using namespace std;
    using namespace steiner_tree_test;

    {
        steiner_tree st(1);
        st.add_edge(0, 0, 1);
        vector<int> terminals{0};
        auto res = st.solve(terminals);
        assert(res.ok && res.cost == 0 && res.edges.empty());
        assert(st.solve_cost(terminals) == 0);
    }
    {
        steiner_tree st(6);
        vector<steiner_tree::edge_type> edges;
        auto add = [&](int u, int v, long long w) {
            st.add_edge(u, v, w);
            edges.push_back({u, v, w});
        };
        add(0, 1, 2);
        add(0, 2, 1);
        add(1, 2, 2);
        add(1, 3, 2);
        add(1, 4, 2);
        add(3, 4, 2);
        add(2, 4, 2);
        add(2, 5, 1);
        add(4, 5, 2);
        add(0, 3, 1);
        add(0, 5, 3);
        vector<int> terminals{0, 3, 5};
        auto res = st.solve(terminals);
        assert(res.ok && res.cost == 3);
        assert(check_result(6, edges, terminals, res));
    }
    {
        steiner_tree st(5);
        vector<steiner_tree::edge_type> edges;
        auto add = [&](int u, int v, long long w) {
            st.add_edge(u, v, w);
            edges.push_back({u, v, w});
        };
        add(0, 1, 1);
        add(0, 1, 10);
        add(0, 2, 100);
        add(0, 3, 1000);
        add(0, 4, 10000);
        add(1, 2, 10);
        add(1, 3, 1);
        add(1, 4, 100);
        add(2, 3, 10);
        add(2, 4, 1);
        add(3, 4, 1);
        vector<int> terminals{1, 3, 4};
        auto res = st.solve(terminals);
        assert(res.ok && res.cost == 2);
        assert(check_result(5, edges, terminals, res));
    }
    {
        const int n = 130;
        steiner_tree st(n);
        vector<steiner_tree::edge_type> edges;
        long long expected = 0;
        for (int i = 0; i + 1 < n; ++i) {
            const long long w = i + 1;
            st.add_edge(i, i + 1, w);
            edges.push_back({i, i + 1, w});
            expected += w;
        }
        vector<int> terminals{0, n - 1};
        auto res = st.solve(terminals);
        assert(res.ok && res.cost == expected);
        assert(check_result(n, edges, terminals, res));
    }

    mt19937_64 rng(1);
    for (int tc = 0; tc < 300; ++tc) {
        const int n = 1 + static_cast<int>(rng() % 9);
        vector<steiner_tree::edge_type> edges;
        steiner_tree st(n);

        for (int v = 1; v < n; ++v) {
            const int u = static_cast<int>(rng() % v);
            const long long w = 1 + static_cast<long long>(rng() % 20);
            st.add_edge(u, v, w);
            edges.push_back({u, v, w});
        }
        const int extra = static_cast<int>(rng() % 25);
        for (int i = 0; i < extra; ++i) {
            const int u = static_cast<int>(rng() % n);
            const int v = static_cast<int>(rng() % n);
            const long long w = 1 + static_cast<long long>(rng() % 20);
            st.add_edge(u, v, w);
            edges.push_back({u, v, w});
        }

        vector<int> vertices(n);
        iota(vertices.begin(), vertices.end(), 0);
        shuffle(vertices.begin(), vertices.end(), rng);
        const int k = 1 + static_cast<int>(rng() % min(n, 6));
        vector<int> terminals(vertices.begin(), vertices.begin() + k);

        const long long expected = reference_cost(n, edges, terminals);
        const long long actual_cost = st.solve_cost(terminals);
        auto actual = st.solve(terminals);
        assert(actual_cost == expected);
        assert(actual.ok && actual.cost == expected);
        assert(check_result(n, edges, terminals, actual));
    }

    cerr << "OK\n";
    return 0;
}

#endif
