#pragma once
#include <bits/stdc++.h>

// general_weighted_matching
// 一般グラフの重み付きマッチングを Edmonds blossom + primal-dual で求めるヘッダオンリーライブラリ
// public API は 0-index、内部 blossom は 1-index で管理する
// 時間計算量は O(n^3)、空間計算量は O(n^2)
// InternalWeight を int にすると、重み範囲が十分小さい問題で定数倍を小さくできる
//
// 典型的なユースケース
//
// 1. 単純な最大重みマッチング
//    重み総和が最大になる任意サイズのマッチングを求める。正重み辺だけを選べばよい問題ではこれを使う。
//
//    int n, m;
//    cin >> n >> m;
//    general_weighted_matching<long long> g(n);
//    for (int i = 0; i < m; i++) {
//        int u, v;
//        long long w;
//        cin >> u >> v >> w;
//        g.add_edge(u, v, w);
//    }
//    auto res = g.max_weight_matching();
//    cout << res.size << ' ' << res.weight << '\n';
//    for (auto [u, v] : res.pairs) cout << u << ' ' << v << '\n';
//
// 2. 添付問題のように n <= 500, w <= 1,000,000 程度で最大重みマッチングを出力する
//    内部重みを int にするとメモリ使用量と定数倍が少し小さくなる。答えの総和は result.weight に long long で入る。
//
//    general_weighted_matching<int, int> g(n);
//    for (int i = 0; i < m; i++) {
//        int u, v, w;
//        cin >> u >> v >> w;
//        g.add_edge(u, v, w);
//    }
//    auto res = g.max_weight_matching();
//    cout << res.size << ' ' << res.weight << '\n';
//    for (auto [u, v] : res.pairs) cout << u << ' ' << v << '\n';
//
// 3. 重み最大を優先し、同じ重みならペア数も最大にしたい
//    通常の max_weight_matching は、総重みが変わらない 0 重み辺を選ばないことがある。
//
//    auto res = g.max_weight_matching_prefer_max_cardinality();
//
// 4. できるだけ多くペアを作り、その中で利得最大またはコスト最小にしたい
//    最大濃度マッチングを主目的にする割当問題で使う。
//
//    auto best_profit = g.max_cardinality_max_weight_matching();
//    auto min_cost = g.max_cardinality_min_weight_matching();
//
// 5. 完全マッチングが必要な割当・ペアリング問題
//    存在しない場合は std::nullopt を返す。n が奇数なら必ず存在しない。
//
//    if (auto res = g.min_weight_perfect_matching()) {
//        cout << res->weight << '\n';
//        for (auto [u, v] : res->pairs) cout << u << ' ' << v << '\n';
//    } else {
//        cout << "no perfect matching\n";
//    }
//
// 結果の見方
// res.weight は元の重みの総和、res.size は選んだ辺数、res.mate[v] は v の相手で未マッチなら -1
// res.pairs は選ばれた頂点ペア一覧、res.edge_ids は対応する add_edge の戻り値一覧
// 多重辺は許可され、目的関数に対して最も有利な辺が内部的に採用される

template <class Weight = long long, class InternalWeight = long long>
struct general_weighted_matching {
    static_assert(std::is_integral_v<Weight>, "Weight must be an integral type");
    static_assert(std::is_integral_v<InternalWeight>, "InternalWeight must be an integral type");
    static_assert(std::is_signed_v<InternalWeight>, "InternalWeight must be signed");

    struct edge {
        int from;
        int to;
        Weight weight;
    };

    struct result {
        long long weight = 0;
        int size = 0;
        std::vector<int> mate;
        std::vector<int> matched_edge_id;
        std::vector<std::pair<int, int>> pairs;
        std::vector<int> edge_ids;
    };

    // 空のグラフを構築する / O(1)
    general_weighted_matching() = default;

    // n 頂点のグラフを構築する / O(n^2)
    explicit general_weighted_matching(int n) { init(n); }

    // n 頂点の空グラフに初期化する / O(n^2)
    void init(int n) {
        assert(n >= 0);
        n_ = n;
        edges_.clear();
        allocate_work();
    }

    // 無向辺 u-v を重み w で追加し、辺 ID を返す / O(1)
    int add_edge(int u, int v, Weight w) {
        assert(0 <= u && u < n_);
        assert(0 <= v && v < n_);
        assert(u != v);
        int id = (int)edges_.size();
        edges_.push_back({u, v, w});
        return id;
    }

    // 重み最大のマッチングを求める / O(n^3)
    result max_weight_matching() {
        return solve_impl([&](Weight w) -> internal_weight {
            return checked_cast(w);
        });
    }

    // 重み最大、同重みならサイズ最大のマッチングを求める / O(n^3)
    result max_weight_matching_prefer_max_cardinality() {
        const internal_weight scale = (internal_weight)n_ + 1;
        return solve_impl([&](Weight w) -> internal_weight {
            return checked_cast((i128)w * scale + 1);
        });
    }

    // サイズ最大、同サイズなら重み最大のマッチングを求める / O(n^3)
    result max_cardinality_max_weight_matching() {
        const internal_weight bonus = cardinality_bonus();
        return solve_impl([&](Weight w) -> internal_weight {
            return checked_cast((i128)bonus + (i128)w);
        });
    }

    // サイズ最大、同サイズなら重み最小のマッチングを求める / O(n^3)
    result max_cardinality_min_weight_matching() {
        const internal_weight bonus = cardinality_bonus();
        return solve_impl([&](Weight w) -> internal_weight {
            return checked_cast((i128)bonus - (i128)w);
        });
    }

    // 完全マッチングの中で重み最大のものを求め、存在しなければ nullopt を返す / O(n^3)
    std::optional<result> max_weight_perfect_matching() {
        if (n_ % 2) return std::nullopt;
        result res = max_cardinality_max_weight_matching();
        if (res.size != n_ / 2) return std::nullopt;
        return res;
    }

    // 完全マッチングの中で重み最小のものを求め、存在しなければ nullopt を返す / O(n^3)
    std::optional<result> min_weight_perfect_matching() {
        if (n_ % 2) return std::nullopt;
        result res = max_cardinality_min_weight_matching();
        if (res.size != n_ / 2) return std::nullopt;
        return res;
    }

    // 頂点数を返す / O(1)
    int size() const { return n_; }

    // 追加された辺の一覧を返す / O(1)
    const std::vector<edge>& edges() const { return edges_; }

private:
    using internal_weight = InternalWeight;
    using i128 = __int128_t;

    struct internal_edge {
        int u = 0;
        int v = 0;
        internal_weight w = 0;
    };

    int n_ = 0;
    int nx_ = 0;
    int lim_ = 0;
    int visit_stamp_ = 0;

    std::vector<edge> edges_;
    std::vector<internal_edge> g_;
    std::vector<internal_weight> label_;
    std::vector<int> match_, slack_, st_, parent_, state_, visited_;
    std::vector<int> blossom_from_, blossom_pos_;
    std::vector<int> blossom_start_, blossom_dir_;
    std::vector<std::vector<int>> blossom_;

    std::vector<int> que_;
    int que_head_ = 0;

    std::vector<int> tree_vertices_, tree_all_vertices_, tree_roots_, slack_roots_;
    std::vector<unsigned char> in_tree_vertex_, in_tree_all_vertex_, in_tree_root_, in_slack_root_;
    std::vector<int> best_edge_id_;

    internal_edge& ge(int u, int v) { return g_[(long long)u * lim_ + v]; }
    const internal_edge& ge(int u, int v) const { return g_[(long long)u * lim_ + v]; }
    int& from_at(int b, int x) { return blossom_from_[(long long)b * (n_ + 1) + x]; }
    int from_at(int b, int x) const { return blossom_from_[(long long)b * (n_ + 1) + x]; }
    int& pos_at(int b, int x) { return blossom_pos_[(long long)b * lim_ + x]; }
    int pos_at(int b, int x) const { return blossom_pos_[(long long)b * lim_ + x]; }

    // 作業領域を確保する / O(n^2)
    void allocate_work() {
        lim_ = 2 * n_ + 5;
        g_.assign((long long)lim_ * lim_, internal_edge{});
        label_.assign(lim_, 0);
        match_.assign(lim_, 0);
        slack_.assign(lim_, 0);
        st_.assign(lim_, 0);
        parent_.assign(lim_, 0);
        state_.assign(lim_, -1);
        visited_.assign(lim_, 0);
        blossom_from_.assign((long long)lim_ * (n_ + 1), 0);
        blossom_pos_.assign((long long)lim_ * lim_, 0);
        blossom_start_.assign(lim_, 0);
        blossom_dir_.assign(lim_, 1);
        blossom_.assign(lim_, {});
        in_tree_vertex_.assign(lim_, 0);
        in_tree_all_vertex_.assign(lim_, 0);
        in_tree_root_.assign(lim_, 0);
        in_slack_root_.assign(lim_, 0);
        best_edge_id_.assign((long long)n_ * n_, -1);
        que_.reserve(n_ + 1);
        tree_vertices_.reserve(n_ + 1);
        tree_all_vertices_.reserve(n_ + 1);
        tree_roots_.reserve(lim_);
        slack_roots_.reserve(lim_);
    }

    // __int128 から内部重みに変換する / O(1)
    static internal_weight checked_cast(i128 x) {
        assert((i128)std::numeric_limits<internal_weight>::min() <= x);
        assert(x <= (i128)std::numeric_limits<internal_weight>::max());
        return (internal_weight)x;
    }

    // サイズ優先用の十分大きいボーナスを返す / O(m)
    internal_weight cardinality_bonus() const {
        i128 max_abs = 0;
        for (const auto& e : edges_) {
            i128 w = (i128)e.weight;
            if (w < 0) w = -w;
            if (max_abs < w) max_abs = w;
        }
        return checked_cast(max_abs * std::max(1, n_) + 1);
    }

    // 辺の reduced cost の 2 倍を返す / O(1)
    internal_weight edge_delta(const internal_edge& e) const {
        return label_[e.u] + label_[e.v] - e.w * 2;
    }

    // 探索木で触った root を記録する / O(1)
    void touch_root(int x) {
        if (!in_tree_root_[x]) {
            in_tree_root_[x] = 1;
            tree_roots_.push_back(x);
        }
    }

    // slack が設定された root を記録する / O(1)
    void touch_slack_root(int x) {
        if (!in_slack_root_[x]) {
            in_slack_root_[x] = 1;
            slack_roots_.push_back(x);
        }
    }

    // x への最良 slack 候補を u で更新する / O(1)
    void update_slack(int u, int x) {
        if (!slack_[x] || edge_delta(ge(u, x)) < edge_delta(ge(slack_[x], x))) {
            if (!slack_[x]) touch_slack_root(x);
            slack_[x] = u;
        }
    }

    // x への slack 候補を探索木全体から再計算する / O(n)
    void set_slack(int x) {
        slack_[x] = 0;
        for (int u : tree_vertices_) {
            if (ge(u, x).w > 0 && st_[u] != x && state_[st_[u]] == 0) update_slack(u, x);
        }
    }

    // outer 側の blossom/base をキューへ追加する / O(blossom size)
    void q_push(int x) {
        if (x == 0) return;
        if (x <= n_) {
            que_.push_back(x);
            if (!in_tree_vertex_[x]) {
                in_tree_vertex_[x] = 1;
                tree_vertices_.push_back(x);
            }
            if (!in_tree_all_vertex_[x]) {
                in_tree_all_vertex_[x] = 1;
                tree_all_vertices_.push_back(x);
            }
        } else {
            for (int y : blossom_[x]) q_push(y);
        }
    }

    // 探索木に含まれる base 頂点を記録する / O(blossom size)
    void mark_tree(int x) {
        if (x == 0) return;
        if (x <= n_) {
            if (!in_tree_all_vertex_[x]) {
                in_tree_all_vertex_[x] = 1;
                tree_all_vertices_.push_back(x);
            }
        } else {
            for (int y : blossom_[x]) mark_tree(y);
        }
    }

    // x 以下の blossom/base の所属 root を b に変更する / O(blossom size)
    void set_st(int x, int b) {
        st_[x] = b;
        if (x > n_) {
            for (int y : blossom_[x]) set_st(y, b);
        }
    }

    // blossom b を現在の向きで i 番目から見る / O(1)
    int blossom_at(int b, int i) const {
        int k = (int)blossom_[b].size();
        int idx = blossom_start_[b] + blossom_dir_[b] * i;
        idx %= k;
        if (idx < 0) idx += k;
        return blossom_[b][idx];
    }

    // blossom b の中で x が現在の向きの何番目か返す / O(1)
    int blossom_pos_view(int b, int x) const {
        int k = (int)blossom_[b].size();
        int p = pos_at(b, x);
        int res = (blossom_dir_[b] == 1 ? p - blossom_start_[b] : blossom_start_[b] - p);
        res %= k;
        if (res < 0) res += k;
        return res;
    }

    // blossom b の開始位置を shift だけ回す / O(1)
    void blossom_rotate(int b, int shift) {
        int k = (int)blossom_[b].size();
        int idx = blossom_start_[b] + blossom_dir_[b] * shift;
        idx %= k;
        if (idx < 0) idx += k;
        blossom_start_[b] = idx;
    }

    // xr が偶数位置になるように blossom b の向きを調整し、その位置を返す / O(1)
    int get_pr(int b, int xr) {
        int k = (int)blossom_[b].size();
        int pr = blossom_pos_view(b, xr);
        if (pr & 1) {
            blossom_dir_[b] = -blossom_dir_[b];
            pr = k - pr;
        }
        return pr;
    }

    // u を v 側の代表辺でマッチさせ、blossom 内部のマッチングも更新する / O(blossom size)
    void set_match(int u, int v) {
        internal_edge e = ge(u, v);
        match_[u] = e.v;
        if (u <= n_) return;

        int xr = from_at(u, e.u);
        int pr = get_pr(u, xr);
        for (int i = 0; i < pr; i++) set_match(blossom_at(u, i), blossom_at(u, i ^ 1));
        set_match(xr, v);
        blossom_rotate(u, pr);
    }

    // u-v から交互路をたどってマッチングを増加する / O(n)
    void augment(int u, int v) {
        while (true) {
            int next_v = st_[match_[u]];
            set_match(u, v);
            if (!next_v) return;
            set_match(next_v, st_[parent_[next_v]]);
            u = st_[parent_[next_v]];
            v = next_v;
        }
    }

    // 交互木上で u と v の LCA blossom を返す。なければ 0 / O(n)
    int get_lca(int u, int v) {
        ++visit_stamp_;
        for (; u || v; std::swap(u, v)) {
            if (u == 0) continue;
            if (visited_[u] == visit_stamp_) return u;
            visited_[u] = visit_stamp_;
            u = st_[match_[u]];
            if (u) u = st_[parent_[u]];
        }
        return 0;
    }

    // outer 同士を結ぶ辺から新しい blossom を作る / O(n^2)
    void add_blossom(int u, int lca, int v) {
        int b = n_ + 1;
        while (b <= nx_ && st_[b]) ++b;
        if (b > nx_) ++nx_;

        label_[b] = 0;
        state_[b] = 0;
        match_[b] = match_[lca];
        blossom_[b].clear();
        blossom_[b].push_back(lca);

        // u 側から LCA までの交互路を blossom の片側として集める
        for (int x = u; x != lca;) {
            blossom_[b].push_back(x);
            int y = st_[match_[x]];
            blossom_[b].push_back(y);
            q_push(y);
            x = st_[parent_[y]];
        }
        std::reverse(blossom_[b].begin() + 1, blossom_[b].end());

        // v 側から LCA までの交互路をもう片側として集める
        for (int x = v; x != lca;) {
            blossom_[b].push_back(x);
            int y = st_[match_[x]];
            blossom_[b].push_back(y);
            q_push(y);
            x = st_[parent_[y]];
        }

        blossom_start_[b] = 0;
        blossom_dir_[b] = 1;
        for (int i = 0; i < (int)blossom_[b].size(); i++) pos_at(b, blossom_[b][i]) = i;

        touch_root(b);
        mark_tree(b);
        set_st(b, b);

        // 新 blossom と他 root を結ぶ代表辺を、構成要素から最小 slack の辺として作る
        for (int x = 1; x <= nx_; x++) {
            ge(b, x) = {b, x, 0};
            ge(x, b) = {x, b, 0};
        }
        for (int x = 1; x <= n_; x++) from_at(b, x) = 0;
        for (int xs : blossom_[b]) {
            for (int x = 1; x <= nx_; x++) {
                if (ge(b, x).w == 0 || edge_delta(ge(xs, x)) < edge_delta(ge(b, x))) {
                    ge(b, x) = ge(xs, x);
                    ge(x, b) = ge(x, xs);
                }
            }
            for (int x = 1; x <= n_; x++) {
                if (from_at(xs, x)) from_at(b, x) = xs;
            }
        }
        set_slack(b);
    }

    // dual label が 0 になった inner blossom を展開する / O(n^2)
    void expand_blossom(int b) {
        for (int x : blossom_[b]) set_st(x, x);

        int xr = from_at(b, ge(b, parent_[b]).u);
        int pr = get_pr(b, xr);

        // root から入口までを交互木として復元する
        for (int i = 0; i < pr; i += 2) {
            int xs = blossom_at(b, i);
            int xns = blossom_at(b, i + 1);
            parent_[xs] = ge(xns, xs).u;
            state_[xs] = 1;
            state_[xns] = 0;
            touch_root(xs);
            mark_tree(xs);
            touch_root(xns);
            slack_[xs] = 0;
            set_slack(xns);
            q_push(xns);
        }

        // 入口を元の inner 側としてつなぎ直し、残りは未訪問に戻す
        state_[xr] = 1;
        parent_[xr] = parent_[b];
        touch_root(xr);
        mark_tree(xr);
        for (int i = pr + 1; i < (int)blossom_[b].size(); i++) {
            int x = blossom_at(b, i);
            state_[x] = -1;
            set_slack(x);
        }
        st_[b] = 0;
    }

    // タイト辺 e を見つけたときの交互木更新を行い、増加できたら true / O(n^2)
    bool on_found_edge(const internal_edge& e) {
        int u = st_[e.u];
        int v = st_[e.v];
        if (state_[v] == -1) {
            parent_[v] = e.u;
            state_[v] = 1;
            touch_root(v);
            mark_tree(v);

            int nu = st_[match_[v]];
            slack_[v] = 0;
            if (nu) {
                slack_[nu] = 0;
                state_[nu] = 0;
                touch_root(nu);
                q_push(nu);
            }
        } else if (state_[v] == 0) {
            int lca = get_lca(u, v);
            if (!lca) {
                augment(u, v);
                augment(v, u);
                return true;
            }
            add_blossom(u, lca, v);
        }
        return false;
    }

    // 現在の dual 状態から増加路を 1 本探してマッチングを増やす / O(n^2) amortized
    bool matching() {
        std::fill(state_.begin() + 1, state_.begin() + nx_ + 1, -1);
        std::fill(slack_.begin() + 1, slack_.begin() + nx_ + 1, 0);
        que_.clear();
        que_head_ = 0;

        tree_vertices_.clear();
        tree_all_vertices_.clear();
        std::fill(in_tree_vertex_.begin(), in_tree_vertex_.end(), 0);
        std::fill(in_tree_all_vertex_.begin(), in_tree_all_vertex_.end(), 0);

        tree_roots_.clear();
        slack_roots_.clear();
        std::fill(in_tree_root_.begin(), in_tree_root_.end(), 0);
        std::fill(in_slack_root_.begin(), in_slack_root_.end(), 0);

        for (int x = 1; x <= nx_; x++) {
            if (st_[x] == x && !match_[x]) {
                parent_[x] = 0;
                state_[x] = 0;
                touch_root(x);
                q_push(x);
            }
        }
        if (que_.empty()) return false;

        const internal_weight inf = std::numeric_limits<internal_weight>::max() / 4;
        while (true) {
            // 現在のタイト辺だけで BFS を進め、非タイト辺は slack 候補として保持する
            while (que_head_ < (int)que_.size()) {
                int u = que_[que_head_++];
                if (state_[st_[u]] == 1) continue;
                for (int v = 1; v <= n_; v++) {
                    if (ge(u, v).w > 0 && st_[u] != st_[v]) {
                        if (edge_delta(ge(u, v)) == 0) {
                            if (on_found_edge(ge(u, v))) return true;
                        } else {
                            update_slack(u, st_[v]);
                        }
                    }
                }
            }

            // 次に何かが起こる最小の dual 移動量を計算する
            internal_weight d = inf;
            for (int b : tree_roots_) {
                if (b > n_ && st_[b] == b && state_[b] == 1) d = std::min(d, label_[b] / 2);
            }
            for (int x : slack_roots_) {
                if (st_[x] == x && slack_[x]) {
                    internal_weight delta = edge_delta(ge(slack_[x], x));
                    if (state_[x] == -1) d = std::min(d, delta);
                    else if (state_[x] == 0) d = std::min(d, delta / 2);
                }
            }

            // base 頂点と blossom の dual label を更新する
            for (int u : tree_all_vertices_) {
                int su = st_[u];
                if (state_[su] == 0) {
                    if (label_[u] <= d) return false;
                    label_[u] -= d;
                } else if (state_[su] == 1) {
                    label_[u] += d;
                }
            }
            for (int b : tree_roots_) {
                if (b > n_ && st_[b] == b) {
                    if (state_[b] == 0) label_[b] += d * 2;
                    else if (state_[b] == 1) label_[b] -= d * 2;
                }
            }

            // 新たにタイトになった slack 辺と、展開可能になった blossom を処理する
            que_.clear();
            que_head_ = 0;
            for (int x : slack_roots_) {
                if (st_[x] == x && slack_[x] && st_[slack_[x]] != x && edge_delta(ge(slack_[x], x)) == 0) {
                    if (on_found_edge(ge(slack_[x], x))) return true;
                }
            }
            for (int b : tree_roots_) {
                if (b > n_ && st_[b] == b && state_[b] == 1 && label_[b] == 0) expand_blossom(b);
            }
        }
    }

    // 内部グラフと初期 dual を構築する / O(n^2 + m)
    template <class Transform>
    void build_internal_graph(Transform transform) {
        for (int u = 1; u <= n_; u++) {
            for (int v = 1; v <= n_; v++) ge(u, v) = {u, v, 0};
        }
        std::fill(best_edge_id_.begin(), best_edge_id_.end(), -1);

        internal_weight max_w = 0;
        for (int id = 0; id < (int)edges_.size(); id++) {
            const auto& e = edges_[id];
            internal_weight w = transform(e.weight);
            if (w <= 0) continue;
            int u = e.from + 1;
            int v = e.to + 1;
            if (w > ge(u, v).w) {
                ge(u, v) = {u, v, w};
                ge(v, u) = {v, u, w};
                best_edge_id_[e.from * n_ + e.to] = id;
                best_edge_id_[e.to * n_ + e.from] = id;
                max_w = std::max(max_w, w);
            }
        }

        nx_ = n_;
        visit_stamp_ = 0;
        std::fill(match_.begin(), match_.end(), 0);
        std::fill(st_.begin(), st_.end(), 0);
        std::fill(parent_.begin(), parent_.end(), 0);
        std::fill(visited_.begin(), visited_.end(), 0);
        std::fill(label_.begin(), label_.end(), 0);
        for (auto& b : blossom_) b.clear();

        for (int u = 0; u <= n_; u++) st_[u] = u;
        for (int u = 1; u <= n_; u++) {
            label_[u] = max_w;
            for (int v = 1; v <= n_; v++) from_at(u, v) = (u == v ? u : 0);
        }
    }

    // 内部マッチングを public result に変換する / O(n)
    result make_result() const {
        result res;
        res.mate.assign(n_, -1);
        res.matched_edge_id.assign(n_, -1);
        for (int u = 1; u <= n_; u++) {
            int v = match_[u];
            if (v && u < v) {
                int a = u - 1;
                int b = v - 1;
                int id = best_edge_id_[a * n_ + b];
                assert(id >= 0);
                res.size++;
                res.weight += (long long)edges_[id].weight;
                res.mate[a] = b;
                res.mate[b] = a;
                res.matched_edge_id[a] = id;
                res.matched_edge_id[b] = id;
                res.pairs.push_back({a, b});
                res.edge_ids.push_back(id);
            }
        }
        return res;
    }

    // transform 後の正重みによる最大重みマッチングを解く / O(n^3)
    template <class Transform>
    result solve_impl(Transform transform) {
        if (n_ == 0) return result{};
        build_internal_graph(transform);
        while (matching()) {}
        return make_result();
    }
};

#if __INCLUDE_LEVEL__ == 0
#include <random>

namespace general_weighted_matching_test {

struct naive_result {
    long long weight = 0;
    int size = 0;
    bool exists = false;
};

naive_result naive_solve(int n, const std::vector<std::tuple<int, int, long long>>& edges, int mode) {
    std::vector<std::vector<long long>> w(n, std::vector<long long>(n, 0));
    std::vector<std::vector<char>> has(n, std::vector<char>(n, 0));
    bool minimize_pair = (mode == 3 || mode == 5);
    for (auto [u, v, c] : edges) {
        if (!has[u][v]) {
            has[u][v] = has[v][u] = 1;
            w[u][v] = w[v][u] = c;
        } else {
            if (minimize_pair) w[u][v] = w[v][u] = std::min(w[u][v], c);
            else w[u][v] = w[v][u] = std::max(w[u][v], c);
        }
    }

    naive_result best;
    best.exists = false;
    std::vector<char> used(n, 0);

    auto is_better = [&](const naive_result& a, const naive_result& b) -> bool {
        if (!b.exists) return true;
        if (mode == 0) return a.weight > b.weight;
        if (mode == 1) return std::pair<long long, int>(a.weight, a.size) > std::pair<long long, int>(b.weight, b.size);
        if (mode == 2) return std::pair<int, long long>(a.size, a.weight) > std::pair<int, long long>(b.size, b.weight);
        if (mode == 3) return std::pair<int, long long>(a.size, -a.weight) > std::pair<int, long long>(b.size, -b.weight);
        if (mode == 4) return a.size == n / 2 && (!b.exists || a.weight > b.weight);
        return a.size == n / 2 && (!b.exists || a.weight < b.weight);
    };

    auto dfs = [&](auto&& self, int start, int sz, long long sum) -> void {
        int v = start;
        while (v < n && used[v]) ++v;
        if (v == n) {
            naive_result cur{sum, sz, true};
            bool perfect_ok = (n % 2 == 0 && sz == n / 2);
            if ((mode <= 3 || perfect_ok) && is_better(cur, best)) best = cur;
            return;
        }
        used[v] = 1;
        self(self, v + 1, sz, sum);
        for (int u = v + 1; u < n; u++) {
            if (!used[u] && has[v][u]) {
                used[u] = 1;
                self(self, v + 1, sz + 1, sum + w[v][u]);
                used[u] = 0;
            }
        }
        used[v] = 0;
    };
    dfs(dfs, 0, 0, 0);
    if (!best.exists && mode <= 3) best = {0, 0, true};
    return best;
}

void check_result(int n, const std::vector<std::tuple<int, int, long long>>& edges,
                  const general_weighted_matching<long long>::result& res) {
    std::vector<char> seen(n, 0);
    long long sum = 0;
    for (int id : res.edge_ids) {
        assert(0 <= id && id < (int)edges.size());
        auto [u, v, w] = edges[id];
        assert(!seen[u] && !seen[v]);
        seen[u] = seen[v] = 1;
        sum += w;
    }
    assert(sum == res.weight);
    assert((int)res.edge_ids.size() == res.size);
    assert((int)res.pairs.size() == res.size);
}

void run_fixed_tests() {
    {
        general_weighted_matching<long long> g(7);
        std::vector<std::tuple<int, int, long long>> es = {
            {2, 0, 1}, {0, 5, 2}, {5, 6, 3}, {6, 1, 4},
            {1, 0, 5}, {1, 3, 6}, {3, 4, 7}, {1, 4, 8},
        };
        for (auto [u, v, w] : es) g.add_edge(u, v, w);
        auto res = g.max_weight_matching();
        assert(res.size == 3);
        assert(res.weight == 15);
    }
    {
        general_weighted_matching<long long> g(4);
        std::vector<std::tuple<int, int, long long>> es = {{0, 2, 1}, {1, 3, 1}, {1, 2, 3}};
        for (auto [u, v, w] : es) g.add_edge(u, v, w);
        auto res = g.max_weight_matching();
        assert(res.size == 1);
        assert(res.weight == 3);
    }
    {
        general_weighted_matching<long long> g(4);
        g.add_edge(0, 1, 0);
        g.add_edge(2, 3, 0);
        auto res = g.max_weight_matching_prefer_max_cardinality();
        assert(res.size == 2);
        assert(res.weight == 0);
    }
    {
        general_weighted_matching<long long> g(4);
        g.add_edge(0, 1, -5);
        g.add_edge(2, 3, -7);
        auto res = g.max_cardinality_min_weight_matching();
        assert(res.size == 2);
        assert(res.weight == -12);
        auto p = g.min_weight_perfect_matching();
        assert(p.has_value());
        assert(p->weight == -12);
    }
}

void run_random_tests() {
    std::mt19937 rng(1234567);
    for (int tc = 0; tc < 2000; tc++) {
        int n = (int)(rng() % 9);
        general_weighted_matching<long long> g(n);
        std::vector<std::tuple<int, int, long long>> es;
        for (int u = 0; u < n; u++) {
            for (int v = u + 1; v < n; v++) {
                int multi = (int)(rng() % 3);
                for (int k = 0; k < multi; k++) {
                    long long w = (long long)(int)(rng() % 21) - 10;
                    int id = g.add_edge(u, v, w);
                    assert(id == (int)es.size());
                    es.push_back({u, v, w});
                }
            }
        }

        auto r0 = g.max_weight_matching();
        auto n0 = naive_solve(n, es, 0);
        check_result(n, es, r0);
        assert(r0.weight == n0.weight);

        auto r1 = g.max_weight_matching_prefer_max_cardinality();
        auto n1 = naive_solve(n, es, 1);
        check_result(n, es, r1);
        assert(r1.weight == n1.weight && r1.size == n1.size);

        auto r2 = g.max_cardinality_max_weight_matching();
        auto n2 = naive_solve(n, es, 2);
        check_result(n, es, r2);
        assert(r2.weight == n2.weight && r2.size == n2.size);

        auto r3 = g.max_cardinality_min_weight_matching();
        auto n3 = naive_solve(n, es, 3);
        check_result(n, es, r3);
        assert(r3.weight == n3.weight && r3.size == n3.size);

        auto r4 = g.max_weight_perfect_matching();
        auto n4 = naive_solve(n, es, 4);
        assert((bool)r4 == n4.exists);
        if (r4) {
            check_result(n, es, *r4);
            assert(r4->weight == n4.weight && r4->size == n4.size);
        }

        auto r5 = g.min_weight_perfect_matching();
        auto n5 = naive_solve(n, es, 5);
        assert((bool)r5 == n5.exists);
        if (r5) {
            check_result(n, es, *r5);
            assert(r5->weight == n5.weight && r5->size == n5.size);
        }
    }
}

}  // namespace general_weighted_matching_test

int main() {
    general_weighted_matching_test::run_fixed_tests();
    general_weighted_matching_test::run_random_tests();
    std::cerr << "OK\n";
    return 0;
}
#endif
