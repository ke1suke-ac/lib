#pragma once
#include <bits/stdc++.h>

// 無向一般グラフの最大基数マッチングを求めるライブラリ
// Edmondsのblossom algorithmにより、0-indexedの無向グラフに対して最大マッチングを O(NM) で求める
// 多重辺は許可し、自己ループは入力辺として保持するがマッチングや最小辺被覆には使用しない
// 取得系APIは通常solve()後に呼び、solve()前は現在保持している空マッチングに対する結果を返す
// 二部グラフにも使えるが、二部グラフと分かっている場合はHopcroft-Karpなどの専用ライブラリの方が高速
//
// 典型的なユースケース1: 最大マッチングサイズとマッチング辺を出力する
//   int n, m;
//   std::cin >> n >> m;
//   general_matching gm(n);
//   gm.reserve_edges(m);
//   for (int i = 0; i < m; ++i) {
//       int u, v;
//       std::cin >> u >> v;
//       gm.add_edge(u, v);
//   }
//   int ans = gm.solve();
//   std::cout << ans << '\n';
//   for (auto [u, v] : gm.matching()) {
//       std::cout << u << ' ' << v << '\n';
//   }
//
// 典型的なユースケース2: 各頂点のマッチ相手や完全マッチングの有無を使う
//   gm.solve();
//   if (gm.is_perfect()) {
//       // 全頂点がマッチしている
//   }
//   const std::vector<int>& mate = gm.mate();
//   for (int v = 0; v < n; ++v) {
//       if (mate[v] == -1) {
//           // vは未マッチ
//       } else if (v < mate[v]) {
//           // 辺 v-mate[v] がマッチングに含まれる
//       }
//   }
//
// 典型的なユースケース3: 選ばれた入力辺IDが必要な問題で使う
//   std::vector<int> edge_id;
//   edge_id.reserve(m);
//   for (int i = 0; i < m; ++i) {
//       int u, v;
//       std::cin >> u >> v;
//       edge_id.push_back(gm.add_edge(u, v));
//   }
//   gm.solve();
//   for (int id : gm.matching_edge_ids()) {
//       // id番目の入力辺がマッチングに使われている
//   }
//
// 典型的なユースケース4: 最大マッチング後の未マッチ頂点を処理する
//   gm.solve();
//   std::cout << gm.unmatched_count() << '\n';
//   for (int v : gm.unmatched_vertices()) {
//       // vは最大マッチングで未マッチの頂点
//   }
//
// 典型的なユースケース5: 孤立点のない一般グラフで最小辺被覆を構成する
//   gm.solve();
//   auto cover = gm.minimum_edge_cover_edge_ids();
//   if (!cover.possible) {
//       std::cout << -1 << '\n';
//   } else {
//       std::cout << cover.edge_ids.size() << '\n';
//       for (int id : cover.edge_ids) {
//           // id番目の入力辺を選ぶと全頂点が少なくとも1本の選択辺に接する
//       }
//   }
//
// 典型的なユースケース6: 同じオブジェクトを別グラフに使い回す
//   general_matching gm;
//   for (int tc = 0; tc < t; ++tc) {
//       int n, m;
//       std::cin >> n >> m;
//       gm.reset(n);
//       gm.reserve_edges(m);
//       for (int i = 0; i < m; ++i) {
//           int u, v;
//           std::cin >> u >> v;
//           gm.add_edge(u, v);
//       }
//       std::cout << gm.solve() << '\n';
//   }
class general_matching {
public:
    // 最小辺被覆の構成結果
    struct edge_cover_result {
        bool possible;
        std::vector<int> edge_ids;
    };

    general_matching() = default;

    // 頂点数nの空グラフを構築する O(N)
    explicit general_matching(int n) {
        reset(n);
    }

    // 頂点数nの空グラフに初期化する O(N)
    void reset(int n) {
        assert(0 <= n);
        n_ = n;
        edges_.clear();
        offset_.clear();
        adj_.clear();
        mate_.assign(static_cast<std::size_t>(n_), -1);
        matching_size_ = 0;
    }

    // 辺配列の容量を予約する O(M)
    void reserve_edges(int m) {
        assert(0 <= m);
        edges_.reserve(static_cast<std::size_t>(m));
    }

    // 辺u-vを追加し、その辺IDを返す O(1) amortized
    int add_edge(int u, int v) {
        assert(0 <= u && u < n_);
        assert(0 <= v && v < n_);
        int id = static_cast<int>(edges_.size());
        edges_.push_back({u, v});
        return id;
    }

    // 最大マッチングを求め、マッチングサイズを返す O(NM)
    int solve() {
        // 空マッチングから作り直す
        mate_.assign(static_cast<std::size_t>(n_), -1);
        matching_size_ = 0;

        // 簡単な増加路を先に処理し、最大サイズなら重い探索を省略する
        greedy_initial_matching();
        if (matching_size_ == n_ / 2) return matching_size_;

        int shortage = n_ / 2 - matching_size_;
        if (shortage <= std::max(8, n_ / 16)) {
            augment_short_paths();
            if (matching_size_ == n_ / 2) return matching_size_;
        }

        // 残りは完全なblossom探索で増加路を探す
        build_graph();
        prepare_work_buffers();
        for (int root = 0; root < n_; ++root) {
            if (mate_[root] != -1) continue;
            int finish = find_augmenting_path(root);
            if (finish == -1) continue;
            augment_path(finish);
            ++matching_size_;
            if (matching_size_ == n_ / 2) break;
        }
        return matching_size_;
    }

    // 現在保持しているマッチングサイズを返す O(1)
    int size() const {
        return matching_size_;
    }

    // 完全マッチングならtrueを返す O(1)
    bool is_perfect() const {
        return matching_size_ * 2 == n_;
    }

    // 未マッチ頂点数を返す O(1)
    int unmatched_count() const {
        return n_ - 2 * matching_size_;
    }

    // 未マッチ頂点の配列を返す O(N)
    std::vector<int> unmatched_vertices() const {
        std::vector<int> res;
        res.reserve(static_cast<std::size_t>(unmatched_count()));
        for (int v = 0; v < n_; ++v) {
            if (mate_[v] == -1) res.push_back(v);
        }
        return res;
    }

    // mate[v]がvのマッチ相手、未マッチなら-1である配列を返す O(1)
    const std::vector<int>& mate() const {
        return mate_;
    }

    // マッチングを頂点対の配列として返す O(N)
    std::vector<std::pair<int, int>> matching() const {
        std::vector<std::pair<int, int>> res;
        res.reserve(static_cast<std::size_t>(matching_size_));
        for (int v = 0; v < n_; ++v) {
            int u = mate_[v];
            if (u != -1 && v < u) res.push_back({v, u});
        }
        return res;
    }

    // マッチングに対応する入力辺IDを返す O(N+M)
    std::vector<int> matching_edge_ids() const {
        std::vector<int> res;
        res.reserve(static_cast<std::size_t>(matching_size_));
        std::vector<char> used(static_cast<std::size_t>(n_), 0);

        // 多重辺がある場合は、入力で最初に現れる該当辺を採用する
        for (int id = 0; id < static_cast<int>(edges_.size()); ++id) {
            int u = edges_[id].from;
            int v = edges_[id].to;
            if (u == v) continue;
            if (!used[u] && !used[v] && mate_[u] == v) {
                used[u] = 1;
                used[v] = 1;
                res.push_back(id);
            }
        }
        return res;
    }

    // solve後の最大マッチングから最小辺被覆に対応する入力辺IDを返し、孤立点があればpossible=falseにする O(N+M)
    edge_cover_result minimum_edge_cover_edge_ids() const {
        edge_cover_result res{true, {}};
        res.edge_ids.reserve(static_cast<std::size_t>(n_ - matching_size_));
        std::vector<char> covered(static_cast<std::size_t>(n_), 0);
        std::vector<int> incident(static_cast<std::size_t>(n_), -1);

        // 非自己ループの接続辺を記録しつつ、最大マッチング辺をすべて採用する
        for (int id = 0; id < static_cast<int>(edges_.size()); ++id) {
            int u = edges_[id].from;
            int v = edges_[id].to;
            if (u == v) continue;
            if (incident[u] == -1) incident[u] = id;
            if (incident[v] == -1) incident[v] = id;
            if (!covered[u] && !covered[v] && mate_[u] == v) {
                covered[u] = 1;
                covered[v] = 1;
                res.edge_ids.push_back(id);
            }
        }

        // 未被覆頂点ごとに、接続する非自己ループ辺を1本追加する
        for (int x = 0; x < n_; ++x) {
            if (covered[x]) continue;
            int use_id = incident[x];
            if (use_id == -1) {
                res.possible = false;
                res.edge_ids.clear();
                return res;
            }
            int u = edges_[use_id].from;
            int v = edges_[use_id].to;
            covered[u] = 1;
            covered[v] = 1;
            res.edge_ids.push_back(use_id);
        }
        return res;
    }

    // 頂点数を返す O(1)
    int vertex_count() const {
        return n_;
    }

    // 入力された辺数を返す O(1)
    int edge_count() const {
        return static_cast<int>(edges_.size());
    }

private:
    struct edge {
        int from;
        int to;
    };

    int n_ = 0;
    std::vector<edge> edges_;

    std::vector<int> offset_;
    std::vector<int> adj_;

    std::vector<int> mate_;
    int matching_size_ = 0;

    std::vector<int> base_;
    std::vector<int> parent_;
    std::vector<int> que_;
    std::vector<char> used_;

    std::vector<int> seen_;
    int seen_token_ = 0;

    void build_graph() {
        // 各頂点の次数を数えてCSRの境界配列を作る
        offset_.assign(static_cast<std::size_t>(n_ + 1), 0);
        for (const edge& e : edges_) {
            if (e.from == e.to) continue;
            ++offset_[e.from + 1];
            ++offset_[e.to + 1];
        }
        for (int v = 0; v < n_; ++v) offset_[v + 1] += offset_[v];

        // 隣接頂点を連続領域へ詰める
        adj_.assign(static_cast<std::size_t>(offset_[n_]), 0);
        std::vector<int> cur = offset_;
        for (const edge& e : edges_) {
            int u = e.from;
            int v = e.to;
            if (u == v) continue;
            adj_[cur[u]++] = v;
            adj_[cur[v]++] = u;
        }
    }

    void prepare_work_buffers() {
        base_.resize(static_cast<std::size_t>(n_));
        parent_.assign(static_cast<std::size_t>(n_), -1);
        que_.resize(static_cast<std::size_t>(n_));
        used_.assign(static_cast<std::size_t>(n_), 0);
        seen_.assign(static_cast<std::size_t>(n_), 0);
        seen_token_ = 0;
    }

    void greedy_initial_matching() {
        for (const edge& e : edges_) {
            int u = e.from;
            int v = e.to;
            if (u == v) continue;
            if (mate_[u] == -1 && mate_[v] == -1) {
                mate_[u] = v;
                mate_[v] = u;
                ++matching_size_;
            }
        }
    }

    void set_free_neighbor(int v, int f, std::vector<int>& first, std::vector<int>& second) const {
        if (first[v] == f || second[v] == f) return;
        if (first[v] == -1) {
            first[v] = f;
        } else if (second[v] == -1) {
            second[v] = f;
        }
    }

    bool try_augment_short(int a, int x, const std::vector<int>& first, const std::vector<int>& second) {
        if (mate_[a] != -1 || mate_[x] == -1) return false;
        int y = mate_[x];

        int b = first[y];
        if (b == a || b == -1 || mate_[b] != -1) b = second[y];
        if (b == a || b == -1 || mate_[b] != -1) return false;

        mate_[a] = x;
        mate_[x] = a;
        mate_[y] = b;
        mate_[b] = y;
        ++matching_size_;
        return true;
    }

    void augment_short_paths() {
        std::vector<int> first(static_cast<std::size_t>(n_), -1);
        std::vector<int> second(static_cast<std::size_t>(n_), -1);

        while (matching_size_ < n_ / 2) {
            std::fill(first.begin(), first.end(), -1);
            std::fill(second.begin(), second.end(), -1);

            // 各頂点について、隣接する未マッチ頂点を最大2個だけ記録する
            for (const edge& e : edges_) {
                int u = e.from;
                int v = e.to;
                if (u == v) continue;
                if (mate_[u] == -1) set_free_neighbor(v, u, first, second);
                if (mate_[v] == -1) set_free_neighbor(u, v, first, second);
            }

            // 見つかった長さ3の増加路を順に反転する
            int augmented = 0;
            for (const edge& e : edges_) {
                int u = e.from;
                int v = e.to;
                if (u == v) continue;
                if (try_augment_short(u, v, first, second) || try_augment_short(v, u, first, second)) {
                    ++augmented;
                    if (matching_size_ == n_ / 2) break;
                }
            }
            if (augmented == 0) break;
        }
    }

    int find_lca(int a, int b) {
        int token = ++seen_token_;

        // 片方の交互木パスに印を付ける
        while (true) {
            a = base_[a];
            seen_[a] = token;
            if (mate_[a] == -1) break;
            a = parent_[mate_[a]];
        }

        // もう片方のパスから最初に印済みのbaseを探す
        while (true) {
            b = base_[b];
            if (seen_[b] == token) return b;
            b = parent_[mate_[b]];
        }
    }

    void mark_path(int v, int b, int child, int token) {
        while (base_[v] != b) {
            seen_[base_[v]] = token;
            seen_[base_[mate_[v]]] = token;
            parent_[v] = child;
            child = mate_[v];
            v = parent_[mate_[v]];
        }
    }

    void contract_blossom(int a, int b, int root_base, int& q_tail) {
        int token = ++seen_token_;
        mark_path(a, root_base, b, token);
        mark_path(b, root_base, a, token);

        // blossomに含まれる頂点を同じbaseへまとめ、必要なら外点としてキューに入れる
        for (int v = 0; v < n_; ++v) {
            if (seen_[base_[v]] != token) continue;
            base_[v] = root_base;
            if (!used_[v]) {
                used_[v] = 1;
                que_[q_tail++] = v;
            }
        }
    }

    int find_augmenting_path(int root) {
        std::fill(used_.begin(), used_.end(), 0);
        std::fill(parent_.begin(), parent_.end(), -1);
        std::iota(base_.begin(), base_.end(), 0);

        int q_head = 0;
        int q_tail = 0;
        que_[q_tail++] = root;
        used_[root] = 1;

        while (q_head < q_tail) {
            int u = que_[q_head++];

            // 外点uから辺を走査し、交互木の伸長またはblossom縮約を行う
            for (int it = offset_[u]; it < offset_[u + 1]; ++it) {
                int v = adj_[it];
                if (base_[u] == base_[v] || mate_[u] == v) continue;

                if (v == root || (mate_[v] != -1 && parent_[mate_[v]] != -1)) {
                    int root_base = find_lca(u, v);
                    contract_blossom(u, v, root_base, q_tail);
                } else if (parent_[v] == -1) {
                    parent_[v] = u;
                    if (mate_[v] == -1) return v;
                    int next = mate_[v];
                    used_[next] = 1;
                    que_[q_tail++] = next;
                }
            }
        }
        return -1;
    }

    void augment_path(int finish) {
        int v = finish;

        // 終端から根へ親ポインタを辿り、交互路上の辺を反転する
        while (v != -1) {
            int pv = parent_[v];
            int nv = (pv == -1 ? -1 : mate_[pv]);
            mate_[v] = pv;
            if (pv != -1) mate_[pv] = v;
            v = nv;
        }
    }
};

#if __INCLUDE_LEVEL__ == 0
#include <cassert>
#include <random>

namespace {

int naive_max_matching_size(int n, const std::vector<std::pair<int, int>>& edges) {
    std::vector<char> can(static_cast<std::size_t>(n * n), 0);
    for (auto [u, v] : edges) {
        if (u == v) continue;
        can[u * n + v] = 1;
        can[v * n + u] = 1;
    }

    int states = 1 << n;
    std::vector<int> dp(static_cast<std::size_t>(states), -1);
    dp[0] = 0;
    for (int mask = 1; mask < states; ++mask) {
        int v = __builtin_ctz(static_cast<unsigned int>(mask));
        int without_v = mask ^ (1 << v);
        int best = dp[without_v];
        for (int u = v + 1; u < n; ++u) {
            if (((without_v >> u) & 1) != 0 && can[v * n + u]) {
                best = std::max(best, dp[without_v ^ (1 << u)] + 1);
            }
        }
        dp[mask] = best;
    }
    return dp[states - 1];
}

void check_matching(const general_matching& gm, const std::vector<std::pair<int, int>>& edges) {
    const int n = gm.vertex_count();
    const std::vector<int>& mate = gm.mate();
    std::vector<char> can(static_cast<std::size_t>(n * n), 0);
    for (auto [u, v] : edges) {
        if (u == v) continue;
        can[u * n + v] = 1;
        can[v * n + u] = 1;
    }

    int cnt = 0;
    for (int v = 0; v < n; ++v) {
        int u = mate[v];
        if (u == -1) continue;
        assert(0 <= u && u < n);
        assert(mate[u] == v);
        if (v < u) {
            ++cnt;
            assert(can[v * n + u]);
        }
    }
    assert(cnt == gm.size());
    assert(static_cast<int>(gm.matching().size()) == gm.size());

    const auto ids = gm.matching_edge_ids();
    assert(static_cast<int>(ids.size()) == gm.size());
    std::vector<char> used(static_cast<std::size_t>(n), 0);
    for (int id : ids) {
        assert(0 <= id && id < static_cast<int>(edges.size()));
        auto [u, v] = edges[id];
        assert(u != v);
        assert(!used[u] && !used[v]);
        assert(mate[u] == v && mate[v] == u);
        used[u] = 1;
        used[v] = 1;
    }

    const auto unmatched = gm.unmatched_vertices();
    assert(gm.unmatched_count() == n - 2 * gm.size());
    assert(static_cast<int>(unmatched.size()) == gm.unmatched_count());
    std::vector<char> seen_unmatched(static_cast<std::size_t>(n), 0);
    for (int v : unmatched) {
        assert(0 <= v && v < n);
        assert(!seen_unmatched[v]);
        assert(mate[v] == -1);
        seen_unmatched[v] = 1;
    }
    for (int v = 0; v < n; ++v) assert(seen_unmatched[v] == (mate[v] == -1));

    std::vector<int> degree(static_cast<std::size_t>(n), 0);
    for (auto [u, v] : edges) {
        if (u == v) continue;
        ++degree[u];
        ++degree[v];
    }

    bool edge_cover_possible = true;
    for (int v = 0; v < n; ++v) {
        if (degree[v] == 0) edge_cover_possible = false;
    }

    const auto cover = gm.minimum_edge_cover_edge_ids();
    assert(cover.possible == edge_cover_possible);
    if (!cover.possible) {
        assert(cover.edge_ids.empty());
        return;
    }
    assert(static_cast<int>(cover.edge_ids.size()) == n - gm.size());

    std::vector<char> covered(static_cast<std::size_t>(n), 0);
    std::vector<char> used_edge(static_cast<std::size_t>(edges.size()), 0);
    for (int id : cover.edge_ids) {
        assert(0 <= id && id < static_cast<int>(edges.size()));
        assert(!used_edge[id]);
        used_edge[id] = 1;
        auto [u, v] = edges[id];
        assert(u != v);
        covered[u] = 1;
        covered[v] = 1;
    }
    for (int v = 0; v < n; ++v) assert(covered[v]);
}

void fixed_tests() {
    {
        general_matching gm(0);
        std::vector<std::pair<int, int>> edges;
        assert(gm.solve() == 0);
        check_matching(gm, edges);
    }
    {
        general_matching gm(1);
        std::vector<std::pair<int, int>> edges{{0, 0}};
        gm.add_edge(0, 0);
        assert(gm.solve() == 0);
        check_matching(gm, edges);
    }
    {
        general_matching gm(2);
        std::vector<std::pair<int, int>> edges{{0, 1}};
        gm.add_edge(0, 1);
        assert(gm.solve() == 1);
        assert(gm.is_perfect());
        check_matching(gm, edges);
    }
    {
        general_matching gm(5);
        std::vector<std::pair<int, int>> edges;
        for (int v = 1; v < 5; ++v) {
            gm.add_edge(0, v);
            edges.push_back({0, v});
        }
        assert(gm.solve() == 1);
        check_matching(gm, edges);
    }
    {
        general_matching gm(5);
        std::vector<std::pair<int, int>> edges;
        for (int i = 0; i < 5; ++i) {
            gm.add_edge(i, (i + 1) % 5);
            edges.push_back({i, (i + 1) % 5});
        }
        assert(gm.solve() == 2);
        check_matching(gm, edges);
    }
    {
        general_matching gm(3);
        std::vector<std::pair<int, int>> edges{{0, 1}, {1, 2}};
        for (auto [u, v] : edges) gm.add_edge(u, v);
        assert(gm.solve() == 1);
        assert(gm.unmatched_count() == 1);
        const auto cover = gm.minimum_edge_cover_edge_ids();
        assert(cover.possible);
        assert(static_cast<int>(cover.edge_ids.size()) == 2);
        check_matching(gm, edges);
    }
    {
        general_matching gm(3);
        std::vector<std::pair<int, int>> edges{{0, 1}, {2, 2}};
        for (auto [u, v] : edges) gm.add_edge(u, v);
        assert(gm.solve() == 1);
        const auto cover = gm.minimum_edge_cover_edge_ids();
        assert(!cover.possible);
        check_matching(gm, edges);
    }
    {
        general_matching gm(7);
        std::vector<std::pair<int, int>> edges{{2, 0}, {0, 5}, {5, 6}, {6, 1}, {1, 0}, {1, 3}, {3, 4}, {1, 4}};
        for (auto [u, v] : edges) gm.add_edge(u, v);
        assert(gm.solve() == 3);
        check_matching(gm, edges);
    }
    {
        general_matching gm(8);
        std::vector<std::pair<int, int>> edges;
        for (int i = 0; i < 8; ++i) {
            for (int j = i + 1; j < 8; ++j) {
                gm.add_edge(i, j);
                edges.push_back({i, j});
            }
        }
        assert(gm.solve() == 4);
        check_matching(gm, edges);
    }
}

void random_tests() {
    std::mt19937 rng(123456789);
    for (int n = 0; n <= 16; ++n) {
        for (int tc = 0; tc < 300; ++tc) {
            int den = 1 + static_cast<int>(rng() % 10);
            std::vector<std::pair<int, int>> edges;
            general_matching gm(n);
            for (int u = 0; u < n; ++u) {
                for (int v = u + 1; v < n; ++v) {
                    if (static_cast<int>(rng() % 10) < den) {
                        edges.push_back({u, v});
                        gm.add_edge(u, v);
                        if ((rng() & 15U) == 0U) {
                            edges.push_back({u, v});
                            gm.add_edge(u, v);
                        }
                    }
                }
                if ((rng() & 31U) == 0U) {
                    edges.push_back({u, u});
                    gm.add_edge(u, u);
                }
            }
            int got = gm.solve();
            int want = naive_max_matching_size(n, edges);
            assert(got == want);
            check_matching(gm, edges);
        }
    }

    for (int tc = 0; tc < 200; ++tc) {
        int n = 17 + static_cast<int>(rng() % 184);
        std::vector<std::pair<int, int>> edges;
        general_matching gm(n);
        for (int u = 0; u < n; ++u) {
            for (int v = u + 1; v < n; ++v) {
                if ((rng() % 1000) < 35) {
                    edges.push_back({u, v});
                    gm.add_edge(u, v);
                }
            }
        }
        gm.solve();
        check_matching(gm, edges);
    }
}

}  // namespace

int main() {
    fixed_tests();
    random_tests();
    return 0;
}
#endif
