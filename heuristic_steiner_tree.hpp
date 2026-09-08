#pragma once
// v17: scoped implementation helpers; public calls and search algorithms preserved.
#include <bits/stdc++.h>

/*
非負重み無向グラフ上の Steiner tree をヒューリスティックに求めるライブラリ。
厳密解は保証せず、AHC の全体解構築と部分問題 repair で短時間に良い実行可能解を返すことを重視する。
Voronoi-MST、逐次最短路の multi-start、誘導部分グラフ MST、terminal 枝再接続、
key-path 交換、elite 解合成を組み合わせる。同じグラフで繰り返し呼ぶ場合は CSR、辺順序、作業配列を再利用する。
頂点番号は 0-indexed、返す辺番号は add_edge した順の 0-indexed とする。

典型的な使用例:
    heuristic_steiner_tree st(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        cin >> u >> v >> w;
        st.add_edge(u, v, w);
    }

    auto res = st.solve(terminals);
    if (!res.ok) {
        cout << -1 << '\n';
    } else {
        cout << res.cost << ' ' << res.edges.size() << '\n';
        for (int edge_id : res.edges) cout << edge_id << '\n';
    }

時間制限付きで品質を優先する例:
    heuristic_steiner_tree_options options;
    options.preset = heuristic_steiner_tree_preset::quality;
    options.time_limit_ms = 950;
    options.seed = 123456789;
    auto res = st.solve(terminals, options);

既存解を局所改善する例:
    // initial_edges は元グラフの辺番号。木でなくても、terminal を接続する部分グラフならよい。
    auto improved = st.improve(terminals, initial_edges, options);

非連結な部分解を修復する例:
    // partial_edges は warm start であり、不要なら削除される。
    auto repaired = st.repair(terminals, partial_edges, options);

既設ネットワークへ追加する例:
    // base_edges は撤去不能かつ追加費用 0。返るのは追加辺と追加費用だけ。
    auto augmented = st.augment(terminals, base_edges, options);

注意:
    計算量の N は頂点数、M は辺数、K は terminal 数、A は試行数、C は最大探索距離を表す。
    L は正規化する候補辺集合の最大長で、入力や経路展開に含まれる重複も数える。
    無向グラフ、非負 long long 重みを前提とする。
    多重辺に対応し、自己ループは入力可能だが解の改善には使われない。
    terminals の重複は各公開 solve/repair 系メソッド内で除去する。
    time_limit_ms が 0 かつ deadline 未指定なら固定反復数で動作し、同一入力・seed なら決定的になる。
    time_limit_ms と deadline の早い方を停止目標にする。どちらも soft limit であり、
    実行可能解を確保する最初の構築や進行中の Dijkstra により少し超過する場合がある。
    通常グラフ用 terminal cache は直近1組を保持する。不要になったら clear_terminal_cache() で解放できる。
    内部 workspace と cache を更新するため、同じsolverインスタンスへの並行呼び出しは非対応である。

既定 preset:
    fast:
        Voronoi-MST、逐次最短路、誘導部分グラフ MST を各 1 回程度実行する
    balanced:
        複数の初期解と 4 restart、上位 4 候補への terminal 枝再接続と key-path 交換を組み合わせる
    quality:
        20 restart、elite 合成、探索途中の候補を含む複数解の局所改善を行う

部分問題 API:
    normalize: 候補辺だけで MST 化・葉刈りする。辺は追加しない
    improve:   連結済み候補を正規化し、局所改善する
    repair:    非連結でもよい warm start に辺を追加し、不要辺の削除も許す
    augment:   base 辺を撤去不能・費用 0 として、必要な追加辺だけを求める

実装上の高速化:
    逐次最短路法は新規追加頂点だけで terminal ごとの最良接続先を増分更新する
    全辺と Voronoi 境界辺は安定 8-bit radix sort で並べ、全要素で同じ byte は自動的に省略する
    Kruskal の DSU は terminal 個数ではなく terminal の有無と残り成分数だけを保持する
    局所探索用 Dijkstra は訪問頂点だけを世代番号で初期化し、改善不能な距離で打ち切る
    terminal 枝の置換は最初の木との交点で経路を打ち切り、木であることを保ったまま直接更新する
    repair / augment は seed 辺を 0-cost overlay とする Dijkstra と Voronoi 構築を使う
    augment の terminal cache も 0-cost overlay に対応し、グラフの再構築を避ける
    通常グラフの terminal cache は呼び出し間で再利用し、terminal 変更時も共通行を引き継ぐ
    共通行は同じ表の中で前へ圧縮してから後ろへ展開し、新旧2表を別々には構築しない
    augmentでbaseだけで接続済みの場合と、improveの入力が非連結の場合はCSR・辺順序を構築しない

自動最短路モード:
    K<=32 かつメモリ・時間予算に収まる場合は terminal cache を使う
    上記以外は CSR と radix heap による on-demand 最短路を使う
*/

enum class heuristic_steiner_tree_preset {
    fast,
    balanced,
    quality
};

enum class heuristic_steiner_tree_path_mode {
    auto_select,
    terminal_cache,
    on_demand
};

struct heuristic_steiner_tree_options {
    heuristic_steiner_tree_preset preset = heuristic_steiner_tree_preset::balanced;
    heuristic_steiner_tree_path_mode path_mode = heuristic_steiner_tree_path_mode::auto_select;
    std::uint64_t seed = 0;
    int time_limit_ms = 0;
    int restart_limit = -1;
    size_t memory_limit_bytes = 256ULL << 20;
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::time_point::max();
};

struct heuristic_steiner_tree_statistics {
    heuristic_steiner_tree_path_mode path_mode_used = heuristic_steiner_tree_path_mode::on_demand;
    size_t path_memory_bytes = 0;
    int initial_solution_count = 0;
    int restart_count = 0;
    int local_move_count = 0;
    int improvement_count = 0;
    long long dijkstra_count = 0;
    long long edge_relaxation_count = 0;
};

struct heuristic_steiner_tree_result {
    bool ok = false;
    long long cost = 0;
    std::vector<int> edges;
    heuristic_steiner_tree_statistics statistics;
};

struct heuristic_steiner_tree_augmentation_result {
    bool ok = false;
    long long added_cost = 0;
    std::vector<int> added_edges;
    heuristic_steiner_tree_statistics statistics;
};

class heuristic_steiner_tree {
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

    struct tree_solution {
        bool ok = false;
        long long cost = 0;
        std::vector<int> edges;
    };

    class radix_heap {
    private:
        using key_type = unsigned long long;
        using value_type = int;

        std::array<std::vector<std::pair<key_type, value_type>>, 65> buckets_;
        key_type last_ = 0;
        size_t size_ = 0;

        static int bucket_index(key_type key, key_type last) {
            const key_type diff = key ^ last;
            if (diff == 0) return 0;
            return 64 - __builtin_clzll(diff);
        }

    public:
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
            if (buckets_[0].empty()) {
                int bucket_id = 1;
                while (bucket_id < 65 && buckets_[bucket_id].empty()) ++bucket_id;
                assert(bucket_id < 65);

                key_type next_last = buckets_[bucket_id][0].first;
                for (const auto &item : buckets_[bucket_id]) {
                    if (item.first < next_last) next_last = item.first;
                }
                last_ = next_last;
                for (const auto &item : buckets_[bucket_id]) {
                    buckets_[bucket_index(item.first, last_)].push_back(item);
                }
                buckets_[bucket_id].clear();
            }

            const auto result = buckets_[0].back();
            buckets_[0].pop_back();
            --size_;
            return result;
        }
    };

    class dsu {
    private:
        std::vector<int> parent_;
        std::vector<char> has_terminal_;
        int terminal_components_ = 0;

    public:
        explicit dsu(int n)
            : parent_(n, -1), has_terminal_(n, false) {}

        void set_terminal(int vertex) {
            if (has_terminal_[vertex]) return;
            has_terminal_[vertex] = true;
            ++terminal_components_;
        }

        bool merge(int a, int b) {
            const auto leader = [&](int vertex) -> int {
                int root = vertex;
                while (parent_[root] >= 0) root = parent_[root];
                while (vertex != root) vertex = std::exchange(parent_[vertex], root);
                return root;

            };

            a = leader(a);
            b = leader(b);
            if (a == b) return false;
            if (parent_[a] > parent_[b]) std::swap(a, b);
            parent_[a] += parent_[b];
            parent_[b] = a;
            if (has_terminal_[a] && has_terminal_[b]) --terminal_components_;
            has_terminal_[a] = has_terminal_[a] || has_terminal_[b];
            return true;
        }

        bool terminals_connected() const {
            return terminal_components_ <= 1;
        }
    };

    struct solve_workspace {
        std::vector<int> start;
        std::vector<arc_type> arcs;
        std::vector<int> edge_order;
        radix_heap heap;
        std::vector<long long> dist;
        std::vector<int> parent_edge;
        std::vector<int> owner;
        std::vector<int> visit_stamp;
        int current_stamp = 0;
    };

    struct path_store {
        std::vector<long long> dist;
        std::vector<int> trace;
    };

    struct time_controller {
        std::chrono::steady_clock::time_point begin;
        std::chrono::steady_clock::time_point stop;

        explicit time_controller(const heuristic_steiner_tree_options &options)
            : begin(std::chrono::steady_clock::now()), stop(options.deadline) {
            if (options.time_limit_ms > 0) {
                const auto relative_stop = begin + std::chrono::milliseconds(options.time_limit_ms);
                if (relative_stop < stop) stop = relative_stop;
            }
        }

        bool over() const {
            return stop != std::chrono::steady_clock::time_point::max() &&
                std::chrono::steady_clock::now() >= stop;
        }

        bool over_fraction(int numerator, int denominator) const {
            assert(0 <= numerator && numerator <= denominator && 0 < denominator);
            if (stop == std::chrono::steady_clock::time_point::max()) return false;
            const auto budget = stop > begin ? stop - begin : std::chrono::steady_clock::duration::zero();
            const auto threshold = begin + budget * numerator / denominator;
            return std::chrono::steady_clock::now() >= threshold;
        }

    };

    struct tree_view {
        std::vector<int> head;
        std::vector<int> to;
        std::vector<int> next;
        std::vector<int> local_edge;
        std::vector<int> degree;
    };

    struct key_path {
        int endpoint_a = -1;
        int endpoint_b = -1;
        long long cost = 0;
        std::vector<int> local_edges;
    };

    static constexpr long long INF_VALUE = std::numeric_limits<long long>::max() / 4;
    static constexpr int TERMINAL_CACHE_AUTO_MAX_K = 32;
    static constexpr int CACHED_SEQUENTIAL_MAX_K = 192;
    static constexpr int ON_DEMAND_SEQUENTIAL_MAX_K = 512;
    static constexpr long long CACHE_RELAXATIONS_PER_MILLISECOND = 20000;

    int n_ = 0;
    std::vector<edge_type> edges_;
    mutable solve_workspace reusable_workspace_;
    mutable bool workspace_ready_ = false;
    mutable path_store reusable_terminal_store_;
    mutable std::vector<int> cached_terminals_;
    mutable bool terminal_store_ready_ = false;

    static std::vector<int> normalize_terminals(std::vector<int> terminals) {
        std::sort(terminals.begin(), terminals.end());
        terminals.erase(std::unique(terminals.begin(), terminals.end()), terminals.end());
        return terminals;
    }

    void build_workspace(solve_workspace &workspace) const {
        // 各頂点の次数を数えて CSR の開始位置を構築する
        workspace.start.assign(static_cast<size_t>(n_) + 1, 0);
        for (const auto &edge : edges_) {
            if (edge.from == edge.to) continue;
            ++workspace.start[edge.from + 1];
            ++workspace.start[edge.to + 1];
        }
        for (int vertex = 0; vertex < n_; ++vertex) {
            workspace.start[vertex + 1] += workspace.start[vertex];
        }

        // 無向辺を両方向の arc として連続領域へ格納する
        workspace.arcs.resize(workspace.start[n_]);
        std::vector<int> position = workspace.start;
        for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); ++edge_id) {
            const auto &edge = edges_[edge_id];
            if (edge.from == edge.to) continue;
            workspace.arcs[position[edge.from]++] = arc_type{edge.to, edge_id, edge.cost};
            workspace.arcs[position[edge.to]++] = arc_type{edge.from, edge_id, edge.cost};
        }

        // 入力順を保つ安定 radix sort により、辺番号を (cost, edge_id) 順へ一度だけ並べる
        workspace.edge_order.resize(edges_.size());
        std::iota(workspace.edge_order.begin(), workspace.edge_order.end(), 0);
        unsigned long long varying_bits = 0;
        const unsigned long long reference_cost = edges_.empty()
            ? 0 : static_cast<unsigned long long>(edges_.front().cost);
        for (const auto &edge : edges_) {
            varying_bits |= reference_cost ^ static_cast<unsigned long long>(edge.cost);
        }
        std::vector<int> buffer(workspace.edge_order.size());
        std::array<unsigned int, 256> count{};
        for (int shift = 0; shift < 64; shift += 8) {
            if (((varying_bits >> shift) & 255ULL) == 0) continue;
            count.fill(0);
            for (int edge_id : workspace.edge_order) {
                const auto key = static_cast<size_t>(
                    (static_cast<unsigned long long>(edges_[edge_id].cost) >> shift) & 255ULL);
                ++count[key];
            }
            unsigned int radix_position = 0;
            for (unsigned int &value : count) {
                const unsigned int frequency = value;
                value = radix_position;
                radix_position += frequency;
            }
            for (int edge_id : workspace.edge_order) {
                const auto key = static_cast<size_t>(
                    (static_cast<unsigned long long>(edges_[edge_id].cost) >> shift) & 255ULL);
                buffer[count[key]++] = edge_id;
            }
            workspace.edge_order.swap(buffer);
        }
    }

    // 不要な構築を遅延する。構築時間を相対予算から除く従来の扱いは保つ
    void ensure_workspace(solve_workspace &workspace, time_controller &timer,
                          const heuristic_steiner_tree_options &options) const {
        if (workspace_ready_) return;
        const auto preparation_begin = std::chrono::steady_clock::now();
        build_workspace(workspace);
        workspace_ready_ = true;
        timer.begin += std::chrono::steady_clock::now() - preparation_begin;
        if (options.time_limit_ms > 0)
            timer.stop = std::min(options.deadline,
                timer.begin + std::chrono::milliseconds(options.time_limit_ms));
    }

    void dijkstra_full(const std::vector<int> &sources, bool need_owner,
                       solve_workspace &workspace, heuristic_steiner_tree_statistics &statistics) const {
        workspace.dist.assign(n_, INF_VALUE);
        workspace.parent_edge.assign(n_, -1);
        if (need_owner) workspace.owner.assign(n_, -1);
        workspace.heap.clear();

        // source の添字を owner とする
        for (int source_index = 0; source_index < static_cast<int>(sources.size()); ++source_index) {
            const int source = sources[source_index];
            if (workspace.dist[source] == 0) continue;
            workspace.dist[source] = 0;
            workspace.parent_edge[source] = -1;
            if (need_owner) workspace.owner[source] = source_index;
            workspace.heap.push(0, source);
        }

        ++statistics.dijkstra_count;
        while (!workspace.heap.empty()) {
            const auto [unsigned_distance, vertex] = workspace.heap.pop();
            const long long distance = static_cast<long long>(unsigned_distance);
            if (distance != workspace.dist[vertex]) continue;

            for (int arc_index = workspace.start[vertex]; arc_index < workspace.start[vertex + 1]; ++arc_index) {
                ++statistics.edge_relaxation_count;
                const auto &arc = workspace.arcs[arc_index];
                if (distance > INF_VALUE - arc.cost) continue;
                const long long next_distance = distance + arc.cost;
                if (next_distance >= workspace.dist[arc.to]) continue;
                workspace.dist[arc.to] = next_distance;
                workspace.parent_edge[arc.to] = arc.edge_id;
                if (need_owner) workspace.owner[arc.to] = workspace.owner[vertex];
                workspace.heap.push(static_cast<unsigned long long>(next_distance), arc.to);
            }
        }
    }

    int dijkstra_to_target(const std::vector<int> &sources, const std::vector<char> &target,
                           long long distance_limit, solve_workspace &workspace,
                           heuristic_steiner_tree_statistics &statistics) const {
        if (workspace.visit_stamp.empty()) {
            workspace.visit_stamp.assign(n_, 0);
            // improve の初回 on-demand 呼び出しでは full Dijkstra を経由していない
            workspace.dist.resize(n_);
            workspace.parent_edge.resize(n_);
        }
        if (++workspace.current_stamp == std::numeric_limits<int>::max()) {
            std::fill(workspace.visit_stamp.begin(), workspace.visit_stamp.end(), 0);
            workspace.current_stamp = 1;
        }
        const int stamp = workspace.current_stamp;
        workspace.heap.clear();
        for (int source : sources) {
            if (workspace.visit_stamp[source] == stamp) continue;
            workspace.visit_stamp[source] = stamp;
            workspace.dist[source] = 0;
            workspace.parent_edge[source] = -1;
            workspace.heap.push(0, source);
        }

        // 訪れた頂点だけを初期化し、改善不能な距離に達した時点で打ち切る
        ++statistics.dijkstra_count;
        while (!workspace.heap.empty()) {
            const auto [unsigned_distance, vertex] = workspace.heap.pop();
            const long long distance = static_cast<long long>(unsigned_distance);
            if (workspace.visit_stamp[vertex] != stamp || distance != workspace.dist[vertex]) continue;
            if (distance >= distance_limit) return -1;
            if (target[vertex]) return vertex;

            for (int arc_index = workspace.start[vertex]; arc_index < workspace.start[vertex + 1]; ++arc_index) {
                ++statistics.edge_relaxation_count;
                const auto &arc = workspace.arcs[arc_index];
                if (distance > INF_VALUE - arc.cost) continue;
                const long long next_distance = distance + arc.cost;
                if (next_distance >= distance_limit) continue;
                if (workspace.visit_stamp[arc.to] == stamp && next_distance >= workspace.dist[arc.to]) continue;
                workspace.visit_stamp[arc.to] = stamp;
                workspace.dist[arc.to] = next_distance;
                workspace.parent_edge[arc.to] = arc.edge_id;
                workspace.heap.push(static_cast<unsigned long long>(next_distance), arc.to);
            }
        }
        return -1;
    }

    int dijkstra_to_target_with_free_edges(
        const std::vector<int> &sources, const std::vector<char> &target,
        long long distance_limit, const std::vector<char> &is_free,
        solve_workspace &workspace, heuristic_steiner_tree_statistics &statistics) const {
        if (workspace.visit_stamp.empty()) {
            workspace.visit_stamp.assign(n_, 0);
            workspace.dist.resize(n_);
            workspace.parent_edge.resize(n_);
        }
        if (++workspace.current_stamp == std::numeric_limits<int>::max()) {
            std::fill(workspace.visit_stamp.begin(), workspace.visit_stamp.end(), 0);
            workspace.current_stamp = 1;
        }
        const int stamp = workspace.current_stamp;
        workspace.heap.clear();
        for (int source : sources) {
            if (workspace.visit_stamp[source] == stamp) continue;
            workspace.visit_stamp[source] = stamp;
            workspace.dist[source] = 0;
            workspace.parent_edge[source] = -1;
            workspace.heap.push(0, source);
        }

        ++statistics.dijkstra_count;
        while (!workspace.heap.empty()) {
            const auto [unsigned_distance, vertex] = workspace.heap.pop();
            const long long distance = static_cast<long long>(unsigned_distance);
            if (workspace.visit_stamp[vertex] != stamp || distance != workspace.dist[vertex]) continue;
            if (distance >= distance_limit) return -1;
            if (target[vertex]) return vertex;
            for (int arc_index = workspace.start[vertex]; arc_index < workspace.start[vertex + 1]; ++arc_index) {
                ++statistics.edge_relaxation_count;
                const auto &arc = workspace.arcs[arc_index];
                const long long cost = is_free[arc.edge_id] ? 0 : arc.cost;
                if (distance > INF_VALUE - cost) continue;
                const long long next_distance = distance + cost;
                if (next_distance >= distance_limit) continue;
                if (workspace.visit_stamp[arc.to] == stamp && next_distance >= workspace.dist[arc.to]) continue;
                workspace.visit_stamp[arc.to] = stamp;
                workspace.dist[arc.to] = next_distance;
                workspace.parent_edge[arc.to] = arc.edge_id;
                workspace.heap.push(static_cast<unsigned long long>(next_distance), arc.to);
            }
        }
        return -1;
    }

    void dijkstra_full_with_free_edges(
        const std::vector<int> &sources, bool need_owner,
        const std::vector<char> &free_edge, solve_workspace &workspace,
        heuristic_steiner_tree_statistics &statistics) const {
        workspace.dist.assign(n_, INF_VALUE);
        workspace.parent_edge.assign(n_, -1);
        if (need_owner) workspace.owner.assign(n_, -1);
        workspace.heap.clear();
        for (int source_index = 0; source_index < static_cast<int>(sources.size()); ++source_index) {
            const int source = sources[source_index];
            if (workspace.dist[source] == 0) continue;
            workspace.dist[source] = 0;
            workspace.parent_edge[source] = -1;
            if (need_owner) workspace.owner[source] = source_index;
            workspace.heap.push(0, source);
        }

        ++statistics.dijkstra_count;
        while (!workspace.heap.empty()) {
            const auto [unsigned_distance, vertex] = workspace.heap.pop();
            const long long distance = static_cast<long long>(unsigned_distance);
            if (distance != workspace.dist[vertex]) continue;
            for (int arc_index = workspace.start[vertex]; arc_index < workspace.start[vertex + 1]; ++arc_index) {
                ++statistics.edge_relaxation_count;
                const auto &arc = workspace.arcs[arc_index];
                const long long cost = free_edge[arc.edge_id] ? 0 : arc.cost;
                if (distance > INF_VALUE - cost) continue;
                const long long next_distance = distance + cost;
                if (next_distance >= workspace.dist[arc.to]) continue;
                workspace.dist[arc.to] = next_distance;
                workspace.parent_edge[arc.to] = arc.edge_id;
                if (need_owner) workspace.owner[arc.to] = workspace.owner[vertex];
                workspace.heap.push(static_cast<unsigned long long>(next_distance), arc.to);
            }
        }
    }

    std::vector<int> restore_parent_path(int source, int target,
                                         const std::vector<int> &parent_edge) const {
        std::vector<int> path;
        int vertex = target;
        for (int step = 0; step <= n_ && vertex != source; ++step) {
            const int edge_id = parent_edge[vertex];
            if (edge_id < 0) return {};
            path.push_back(edge_id);
            const auto &edge = edges_[edge_id];
            vertex = edge.from == vertex ? edge.to : edge.from;
        }
        if (vertex != source) return {};
        return path;
    }

    std::vector<int> restore_to_any_source(int target,
                                           const std::vector<int> &parent_edge) const {
        std::vector<int> path;
        int vertex = target;
        for (int step = 0; step <= n_; ++step) {
            const int edge_id = parent_edge[vertex];
            if (edge_id < 0) return path;
            path.push_back(edge_id);
            const auto &edge = edges_[edge_id];
            vertex = edge.from == vertex ? edge.to : edge.from;
        }
        return {};
    }

    long long cached_terminal_distance(const path_store &store, int terminal_index,
                                       int vertex) const {
        return store.dist[static_cast<size_t>(terminal_index) * n_ + vertex];
    }

    std::vector<int> restore_cached_terminal_path(const path_store &store, int terminal_index,
                                                  int target,
                                                  const std::vector<int> &terminals) const {
        const size_t base = static_cast<size_t>(terminal_index) * n_;
        std::vector<int> path;
        int vertex = target;
        for (int step = 0; step <= n_ && vertex != terminals[terminal_index]; ++step) {
            const int edge_id = store.trace[base + vertex];
            if (edge_id < 0) return {};
            path.push_back(edge_id);
            const auto &edge = edges_[edge_id];
            vertex = edge.from == vertex ? edge.to : edge.from;
        }
        if (vertex != terminals[terminal_index]) return {};
        return path;
    }

    path_store *cached_terminal_store(
        const std::vector<int> &terminals, solve_workspace &workspace,
        heuristic_steiner_tree_statistics &statistics) const {
        const auto build_terminal_store = [&](
            const std::vector<int> &local_terminals,
            path_store &store,
            solve_workspace &local_workspace,
            heuristic_steiner_tree_statistics &local_statistics) {
            const int terminal_count = static_cast<int>(local_terminals.size());
            store.dist.resize(static_cast<size_t>(terminal_count) * n_);
            store.trace.resize(static_cast<size_t>(terminal_count) * n_);

            // 各 terminal から 1 回ずつ Dijkstra し、距離と親辺を連続領域へ保存する
            for (int terminal_index = 0; terminal_index < terminal_count; ++terminal_index) {
                dijkstra_full(std::vector<int>{local_terminals[terminal_index]}, false, local_workspace, local_statistics);
                const size_t base = static_cast<size_t>(terminal_index) * n_;
                std::copy(local_workspace.dist.begin(), local_workspace.dist.end(), store.dist.begin() + static_cast<std::ptrdiff_t>(base));
                std::copy(local_workspace.parent_edge.begin(), local_workspace.parent_edge.end(),
                          store.trace.begin() + static_cast<std::ptrdiff_t>(base));
            }

        };

        if (!terminal_store_ready_) {
            build_terminal_store(terminals, reusable_terminal_store_, workspace, statistics);
            cached_terminals_ = terminals;
            terminal_store_ready_ = true;
            return &reusable_terminal_store_;
        }
        if (cached_terminals_ == terminals) return &reusable_terminal_store_;

        std::vector<int> old_row(terminals.size(), -1);
        size_t shared_count = 0;
        size_t old_index = 0;
        size_t new_index = 0;
        while (old_index < cached_terminals_.size() && new_index < terminals.size()) {
            if (cached_terminals_[old_index] < terminals[new_index]) {
                ++old_index;
            } else if (terminals[new_index] < cached_terminals_[old_index]) {
                ++new_index;
            } else {
                old_row[new_index] = static_cast<int>(old_index);
                ++shared_count;
                ++old_index;
                ++new_index;
            }
        }

        if (shared_count == 0) {
            build_terminal_store(terminals, reusable_terminal_store_, workspace, statistics);
            cached_terminals_ = terminals;
            return &reusable_terminal_store_;
        }

        // 共通行を前から詰める。昇順集合なのでコピー先は常にコピー元以下となる
        const size_t width = n_;
        const auto move_row = [&](size_t source_row, size_t destination_row) {
            if (source_row == destination_row) return;
            const size_t source = source_row * width;
            const size_t destination = destination_row * width;
            std::memmove(reusable_terminal_store_.dist.data() + destination,
                         reusable_terminal_store_.dist.data() + source, width * sizeof(long long));
            std::memmove(reusable_terminal_store_.trace.data() + destination,
                         reusable_terminal_store_.trace.data() + source, width * sizeof(int));
        };
        size_t compact_row = 0;
        for (size_t index = 0; index < terminals.size(); ++index) {
            if (old_row[index] < 0) continue;
            move_row(old_row[index], compact_row);
            old_row[index] = static_cast<int>(compact_row++);
        }
        reusable_terminal_store_.dist.resize(terminals.size() * width);
        reusable_terminal_store_.trace.resize(terminals.size() * width);

        // 後ろから目的行へ広げれば、まだ移動していない共通行を上書きしない
        for (size_t index = terminals.size(); index-- > 0;) {
            if (old_row[index] < 0) continue;
            move_row(old_row[index], index);
        }
        for (size_t index = 0; index < terminals.size(); ++index) {
            if (old_row[index] >= 0) continue;
            const size_t destination = index * width;
            dijkstra_full(
                std::vector<int>{terminals[index]}, false, workspace, statistics);
            std::copy(workspace.dist.begin(), workspace.dist.end(),
                      reusable_terminal_store_.dist.begin() + static_cast<std::ptrdiff_t>(destination));
            std::copy(workspace.parent_edge.begin(), workspace.parent_edge.end(),
                      reusable_terminal_store_.trace.begin() + static_cast<std::ptrdiff_t>(destination));
        }
        cached_terminals_ = terminals;
        return &reusable_terminal_store_;
    }

    heuristic_steiner_tree_path_mode choose_path_mode(
        const heuristic_steiner_tree_options &options, int terminal_count,
        const time_controller &timer) const {
        const auto budget_milliseconds = [&]() -> long long {
            if (timer.stop == std::chrono::steady_clock::time_point::max()) return 0;
            if (timer.stop <= timer.begin) return 1;
            const long long milliseconds = static_cast<long long>(
                std::chrono::duration_cast<std::chrono::milliseconds>(timer.stop - timer.begin).count());
            return std::max(1LL, milliseconds);

        };

        if (options.path_mode != heuristic_steiner_tree_path_mode::auto_select) return options.path_mode;
        if (options.preset == heuristic_steiner_tree_preset::fast) {
            return heuristic_steiner_tree_path_mode::on_demand;
        }

        // terminal cache は構築コストが時間予算を圧迫しない場合に限定する
        const size_t terminal_bytes = static_cast<size_t>(terminal_count) * n_ *
            (sizeof(long long) + sizeof(int));
        const long long estimated_relaxations = 2LL * static_cast<long long>(edges_.size()) * terminal_count;
        const long long budget_ms = budget_milliseconds();
        const bool fits_time = budget_ms <= 0 ||
            estimated_relaxations <= budget_ms *
                CACHE_RELAXATIONS_PER_MILLISECOND;
        if (terminal_count <= TERMINAL_CACHE_AUTO_MAX_K && fits_time &&
            terminal_bytes <= options.memory_limit_bytes / 2) {
            return heuristic_steiner_tree_path_mode::terminal_cache;
        }
        return heuristic_steiner_tree_path_mode::on_demand;
    }

    tree_solution prune_tree(std::vector<int> tree_edges,
                             const std::vector<int> &terminals) const {
        tree_solution result;
        if (terminals.size() <= 1) {
            result.ok = true;
            return result;
        }

        // 木を flat adjacency に展開し、非 terminal の葉を queue で反復削除する
        const int tree_size = static_cast<int>(tree_edges.size());
        std::vector<int> head(n_, -1);
        std::vector<int> to(static_cast<size_t>(2 * tree_size));
        std::vector<int> next(static_cast<size_t>(2 * tree_size));
        std::vector<int> local_edge(static_cast<size_t>(2 * tree_size));
        std::vector<int> degree(n_, 0);
        for (int local_id = 0; local_id < tree_size; ++local_id) {
            const auto &edge = edges_[tree_edges[local_id]];
            const int first_arc = 2 * local_id;
            const int second_arc = first_arc + 1;
            to[first_arc] = edge.to;
            next[first_arc] = head[edge.from];
            local_edge[first_arc] = local_id;
            head[edge.from] = first_arc;
            to[second_arc] = edge.from;
            next[second_arc] = head[edge.to];
            local_edge[second_arc] = local_id;
            head[edge.to] = second_arc;
            ++degree[edge.from];
            ++degree[edge.to];
        }

        std::vector<char> is_terminal(n_, false);
        for (int terminal : terminals) is_terminal[terminal] = true;
        std::vector<char> alive(tree_size, true);
        std::queue<int> queue;
        for (int vertex = 0; vertex < n_; ++vertex) {
            if (!is_terminal[vertex] && degree[vertex] == 1) queue.push(vertex);
        }
        while (!queue.empty()) {
            const int vertex = queue.front();
            queue.pop();
            if (is_terminal[vertex] || degree[vertex] != 1) continue;
            for (int arc_index = head[vertex]; arc_index >= 0; arc_index = next[arc_index]) {
                const int edge_index = local_edge[arc_index];
                if (!alive[edge_index]) continue;
                alive[edge_index] = false;
                const int adjacent = to[arc_index];
                --degree[vertex];
                --degree[adjacent];
                if (!is_terminal[adjacent] && degree[adjacent] == 1) queue.push(adjacent);
                break;
            }
        }

        result.ok = true;
        for (int local_id = 0; local_id < tree_size; ++local_id) {
            if (!alive[local_id]) continue;
            result.edges.push_back(tree_edges[local_id]);
            result.cost += edges_[tree_edges[local_id]].cost;
        }
        std::sort(result.edges.begin(), result.edges.end());
        return result;
    }

    tree_solution normalize_candidate(std::vector<int> candidate,
                                      const std::vector<int> &terminals) const {
        if (terminals.size() <= 1) return tree_solution{true, 0, {}};
        std::sort(candidate.begin(), candidate.end(), [&](int lhs, int rhs) {
            if (edges_[lhs].cost != edges_[rhs].cost) return edges_[lhs].cost < edges_[rhs].cost;
            return lhs < rhs;
        });
        candidate.erase(std::unique(candidate.begin(), candidate.end()), candidate.end());

        // 候補辺上で Kruskal を行い、全 terminal が初めて連結した時点で打ち切る
        dsu union_find(n_);
        for (int terminal : terminals) union_find.set_terminal(terminal);
        std::vector<int> tree_edges;
        tree_edges.reserve(candidate.size());
        bool connected = false;
        for (int edge_id : candidate) {
            const auto &edge = edges_[edge_id];
            if (edge.from == edge.to || !union_find.merge(edge.from, edge.to)) continue;
            tree_edges.push_back(edge_id);
            if (union_find.terminals_connected()) {
                connected = true;
                break;
            }
        }
        if (!connected) return {};
        return prune_tree(std::move(tree_edges), terminals);
    }

    std::vector<char> make_edge_mark(const std::vector<int> &edge_ids) const {
        std::vector<char> marked(edges_.size(), false);
        for (int edge_id : edge_ids) marked[edge_id] = true;
        return marked;
    }

    tree_solution normalize_candidate_with_free_edges(
        std::vector<int> candidate, const std::vector<int> &free_edges,
        const std::vector<char> &is_free, const std::vector<int> &terminals) const {
        if (terminals.size() <= 1) return tree_solution{true, 0, {}};
        candidate.insert(candidate.end(), free_edges.begin(), free_edges.end());
        std::sort(candidate.begin(), candidate.end(), [&](int lhs, int rhs) {
            const long long left_cost = is_free[lhs] ? 0 : edges_[lhs].cost;
            const long long right_cost = is_free[rhs] ? 0 : edges_[rhs].cost;
            if (left_cost != right_cost) return left_cost < right_cost;
            return lhs < rhs;
        });
        candidate.erase(std::unique(candidate.begin(), candidate.end()), candidate.end());

        dsu union_find(n_);
        for (int terminal : terminals) union_find.set_terminal(terminal);
        std::vector<int> tree_edges;
        tree_edges.reserve(candidate.size());
        bool connected = false;
        for (int edge_id : candidate) {
            const auto &edge = edges_[edge_id];
            if (edge.from == edge.to || !union_find.merge(edge.from, edge.to)) continue;
            tree_edges.push_back(edge_id);
            if (union_find.terminals_connected()) {
                connected = true;
                break;
            }
        }
        if (!connected) return {};
        tree_solution result = prune_tree(std::move(tree_edges), terminals);
        result.cost = 0;
        for (int edge_id : result.edges) {
            if (!is_free[edge_id]) result.cost += edges_[edge_id].cost;
        }
        return result;
    }

    tree_view make_tree_view(const tree_solution &solution) const {
        tree_view view;
        const int edge_count = static_cast<int>(solution.edges.size());
        view.head.assign(n_, -1);
        view.to.resize(2 * edge_count);
        view.next.resize(2 * edge_count);
        view.local_edge.resize(2 * edge_count);
        view.degree.assign(n_, 0);
        for (int local_id = 0; local_id < edge_count; ++local_id) {
            const int global_id = solution.edges[local_id];
            const auto &edge = edges_[global_id];
            const int first_arc = 2 * local_id;
            const int second_arc = first_arc + 1;
            view.to[first_arc] = edge.to;
            view.next[first_arc] = view.head[edge.from];
            view.local_edge[first_arc] = local_id;
            view.head[edge.from] = first_arc;
            view.to[second_arc] = edge.from;
            view.next[second_arc] = view.head[edge.to];
            view.local_edge[second_arc] = local_id;
            view.head[edge.to] = second_arc;
            ++view.degree[edge.from];
            ++view.degree[edge.to];
        }
        return view;
    }

    tree_solution voronoi_mst(const std::vector<int> &terminals, solve_workspace &workspace,
                              heuristic_steiner_tree_statistics &statistics) const {
        dijkstra_full(terminals, true, workspace, statistics);
        struct terminal_link {
            long long cost;
            int owner_a;
            int owner_b;
            int edge_id;
        };
        std::vector<terminal_link> links;
        links.reserve(edges_.size());
        unsigned long long reference_cost = 0;
        unsigned long long varying_bits = 0;
        for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); ++edge_id) {
            const auto &edge = edges_[edge_id];
            const int owner_a = workspace.owner[edge.from];
            const int owner_b = workspace.owner[edge.to];
            if (owner_a < 0 || owner_b < 0 || owner_a == owner_b) continue;
            const long long left = workspace.dist[edge.from];
            const long long right = workspace.dist[edge.to];
            if (left > INF_VALUE - edge.cost || left + edge.cost > INF_VALUE - right) continue;
            const long long link_cost = left + edge.cost + right;
            if (links.empty()) reference_cost = static_cast<unsigned long long>(link_cost);
            else varying_bits |= reference_cost ^ static_cast<unsigned long long>(link_cost);
            links.push_back(terminal_link{link_cost, owner_a, owner_b, edge_id});
        }
        // cost の異なる byte だけを下位から安定ソートし、同コスト時の edge_id 順も保つ
        std::vector<terminal_link> buffer(links.size());
        std::array<unsigned int, 256> count{};
        for (int shift = 0; shift < 64; shift += 8) {
            if (((varying_bits >> shift) & 255ULL) == 0) continue;
            count.fill(0);
            for (const auto &link : links) {
                const auto key = static_cast<size_t>(
                    (static_cast<unsigned long long>(link.cost) >> shift) & 255ULL);
                ++count[key];
            }
            unsigned int position = 0;
            for (unsigned int &value : count) {
                const unsigned int frequency = value;
                value = position;
                position += frequency;
            }
            for (const auto &link : links) {
                const auto key = static_cast<size_t>(
                    (static_cast<unsigned long long>(link.cost) >> shift) & 255ULL);
                buffer[count[key]++] = link;
            }
            links.swap(buffer);
        }

        // terminal 間候補グラフの MST 辺を元グラフ上の Voronoi 親経路へ展開する
        dsu union_find(static_cast<int>(terminals.size()));
        std::vector<int> candidate;
        int selected = 0;
        for (const auto &link : links) {
            if (!union_find.merge(link.owner_a, link.owner_b)) continue;
            const auto &bridge = edges_[link.edge_id];
            auto left_path = restore_to_any_source(bridge.from, workspace.parent_edge);
            auto right_path = restore_to_any_source(bridge.to, workspace.parent_edge);
            candidate.insert(candidate.end(), left_path.begin(), left_path.end());
            candidate.push_back(link.edge_id);
            candidate.insert(candidate.end(), right_path.begin(), right_path.end());
            ++selected;
            if (selected + 1 == static_cast<int>(terminals.size())) break;
        }
        ++statistics.initial_solution_count;
        return normalize_candidate(std::move(candidate), terminals);
    }

    std::vector<int> seeded_voronoi_candidate(
        const std::vector<int> &terminals, const std::vector<int> &seed_edges,
        const std::vector<char> &is_free, solve_workspace &workspace,
        heuristic_steiner_tree_statistics &statistics) const {
        struct seeded_link {
            long long cost;
            int owner_a;
            int owner_b;
            int edge_id;
        };

        const auto sort_seeded_links = [&](std::vector<seeded_link> &links) {
            if (links.empty()) return;
            const unsigned long long reference_cost = static_cast<unsigned long long>(links.front().cost);
            unsigned long long varying_bits = 0;
            for (const auto &link : links) {
                varying_bits |= reference_cost ^ static_cast<unsigned long long>(link.cost);
            }
            std::vector<seeded_link> buffer(links.size());
            std::array<unsigned int, 256> count{};
            for (int shift = 0; shift < 64; shift += 8) {
                if (((varying_bits >> shift) & 255ULL) == 0) continue;
                count.fill(0);
                for (const auto &link : links) {
                    const auto key = static_cast<size_t>(
                        (static_cast<unsigned long long>(link.cost) >> shift) & 255ULL);
                    ++count[key];
                }
                unsigned int position = 0;
                for (unsigned int &value : count) {
                    const unsigned int frequency = value;
                    value = position;
                    position += frequency;
                }
                for (const auto &link : links) {
                    const auto key = static_cast<size_t>(
                        (static_cast<unsigned long long>(link.cost) >> shift) & 255ULL);
                    buffer[count[key]++] = link;
                }
                links.swap(buffer);
            }

        };

        dijkstra_full_with_free_edges(terminals, true, is_free, workspace, statistics);
        std::vector<seeded_link> links;
        links.reserve(edges_.size());
        for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); ++edge_id) {
            const auto &edge = edges_[edge_id];
            const int owner_a = workspace.owner[edge.from];
            const int owner_b = workspace.owner[edge.to];
            if (owner_a < 0 || owner_b < 0 || owner_a == owner_b) continue;
            const long long left = workspace.dist[edge.from];
            const long long right = workspace.dist[edge.to];
            const long long edge_cost = is_free[edge_id] ? 0 : edge.cost;
            if (left > INF_VALUE - edge_cost || left + edge_cost > INF_VALUE - right) continue;
            links.push_back(seeded_link{left + edge_cost + right, owner_a, owner_b, edge_id});
        }
        sort_seeded_links(links);

        dsu union_find(static_cast<int>(terminals.size()));
        for (int index = 0; index < static_cast<int>(terminals.size()); ++index) {
            union_find.set_terminal(index);
        }
        std::vector<int> candidate = seed_edges;
        for (const auto &link : links) {
            if (!union_find.merge(link.owner_a, link.owner_b)) continue;
            const auto &bridge = edges_[link.edge_id];
            auto left_path = restore_to_any_source(bridge.from, workspace.parent_edge);
            auto right_path = restore_to_any_source(bridge.to, workspace.parent_edge);
            candidate.insert(candidate.end(), left_path.begin(), left_path.end());
            candidate.push_back(link.edge_id);
            candidate.insert(candidate.end(), right_path.begin(), right_path.end());
            if (union_find.terminals_connected()) break;
        }
        ++statistics.initial_solution_count;
        if (!union_find.terminals_connected()) return {};
        return candidate;
    }

    std::vector<int> sequential_cached_candidate(
        const std::vector<int> &terminals, const path_store &store,
        int root_index, int rcl, std::mt19937_64 &random,
        heuristic_steiner_tree_statistics &statistics) const {
        struct choice {
            long long distance;
            int terminal_index;
            int target;
        };
        const int terminal_count = static_cast<int>(terminals.size());
        std::vector<char> connected(terminal_count, false);
        std::vector<char> in_tree(n_, false);
        std::vector<long long> best_distance(terminal_count, INF_VALUE);
        std::vector<int> best_target(terminal_count, -1);
        connected[root_index] = true;
        in_tree[terminals[root_index]] = true;
        for (int terminal_index = 0; terminal_index < terminal_count; ++terminal_index) {
            if (terminal_index == root_index) continue;
            best_distance[terminal_index] = cached_terminal_distance(
                store, terminal_index, terminals[root_index]);
            best_target[terminal_index] = terminals[root_index];
        }
        std::vector<int> candidate;

        // 新しく木へ入った頂点だけで各 terminal の最良接続先を更新する
        for (int added = 1; added < terminal_count; ++added) {
            std::vector<choice> top;
            top.reserve(rcl);
            for (int terminal_index = 0; terminal_index < terminal_count; ++terminal_index) {
                if (connected[terminal_index] || best_distance[terminal_index] == INF_VALUE) continue;
                const choice current{best_distance[terminal_index], terminal_index,
                                     best_target[terminal_index]};
                auto position = std::lower_bound(top.begin(), top.end(), current,
                    [](const choice &lhs, const choice &rhs) {
                        if (lhs.distance != rhs.distance) return lhs.distance < rhs.distance;
                        return lhs.terminal_index < rhs.terminal_index;
                    });
                if (position == top.end() && static_cast<int>(top.size()) >= rcl) continue;
                top.insert(position, current);
                if (static_cast<int>(top.size()) > rcl) top.pop_back();
            }
            if (top.empty()) return {};
            const int selected = static_cast<int>(random() % static_cast<std::uint64_t>(top.size()));
            const choice picked = top[selected];
            auto path = restore_cached_terminal_path(store, picked.terminal_index,
                                                     picked.target, terminals);
            if (path.empty() && terminals[picked.terminal_index] != picked.target) return {};
            candidate.insert(candidate.end(), path.begin(), path.end());
            connected[picked.terminal_index] = true;

            std::vector<int> new_vertices;
            new_vertices.reserve(path.size() + 1);
            const int selected_terminal = terminals[picked.terminal_index];
            if (!in_tree[selected_terminal]) {
                in_tree[selected_terminal] = true;
                new_vertices.push_back(selected_terminal);
            }
            for (int edge_id : path) {
                const auto &edge = edges_[edge_id];
                if (!in_tree[edge.from]) {
                    in_tree[edge.from] = true;
                    new_vertices.push_back(edge.from);
                }
                if (!in_tree[edge.to]) {
                    in_tree[edge.to] = true;
                    new_vertices.push_back(edge.to);
                }
            }
            for (int terminal_index = 0; terminal_index < terminal_count; ++terminal_index) {
                if (connected[terminal_index]) continue;
                for (int target : new_vertices) {
                    const long long distance = cached_terminal_distance(
                        store, terminal_index, target);
                    if (distance < best_distance[terminal_index]) {
                        best_distance[terminal_index] = distance;
                        best_target[terminal_index] = target;
                    }
                }
            }
        }
        ++statistics.initial_solution_count;
        return candidate;
    }

    bool induced_mst_improve(tree_solution &best, const std::vector<int> &terminals,
                             const solve_workspace &workspace,
                             heuristic_steiner_tree_statistics &statistics) const {
        const auto kruskal_filtered = [&](
            const std::vector<char> &allowed,
            const std::vector<int> &local_terminals,
            const solve_workspace &local_workspace) -> tree_solution {
            dsu union_find(n_);
            for (int terminal : local_terminals) union_find.set_terminal(terminal);
            std::vector<int> tree_edges;
            tree_edges.reserve(std::max(0, n_ - 1));
            bool connected = false;

            // 重み順に事前整列した全辺を走査し、許可頂点内の辺だけを採用する
            for (int edge_id : local_workspace.edge_order) {
                const auto &edge = edges_[edge_id];
                if (edge.from == edge.to) continue;
                if (!allowed[edge.from] || !allowed[edge.to]) continue;
                if (!union_find.merge(edge.from, edge.to)) continue;
                tree_edges.push_back(edge_id);
                if (union_find.terminals_connected()) {
                    connected = true;
                    break;
                }
            }
            if (!connected) return {};
            return prune_tree(std::move(tree_edges), local_terminals);

        };

        if (!best.ok) return false;
        std::vector<char> allowed(n_, false);
        for (int terminal : terminals) allowed[terminal] = true;
        for (int edge_id : best.edges) {
            allowed[edges_[edge_id].from] = true;
            allowed[edges_[edge_id].to] = true;
        }

        // 現在木に含まれる頂点が誘導する部分グラフ上で MST を取り直す
        ++statistics.local_move_count;
        auto candidate = kruskal_filtered(allowed, terminals, workspace);
        if (!candidate.ok || candidate.cost >= best.cost) return false;
        best = std::move(candidate);
        ++statistics.improvement_count;
        return true;
    }

    bool induced_mst_improve_with_free_edges(
        tree_solution &best, const std::vector<int> &terminals,
        const std::vector<int> &free_edges, const std::vector<char> &is_free,
        const solve_workspace &workspace,
        heuristic_steiner_tree_statistics &statistics) const {
        const auto kruskal_filtered_with_free_edges = [&](
            const std::vector<char> &allowed,
            const std::vector<int> &local_free_edges,
            const std::vector<char> &local_is_free,
            const std::vector<int> &local_terminals,
            const solve_workspace &local_workspace) -> tree_solution {
            dsu union_find(n_);
            for (int terminal : local_terminals) union_find.set_terminal(terminal);
            std::vector<int> tree_edges;
            tree_edges.reserve(std::max(0, n_ - 1));
            for (int edge_id : local_free_edges) {
                const auto &edge = edges_[edge_id];
                if (edge.from == edge.to || !allowed[edge.from] || !allowed[edge.to]) continue;
                if (union_find.merge(edge.from, edge.to)) tree_edges.push_back(edge_id);
            }
            bool connected = union_find.terminals_connected();
            if (!connected) {
                for (int edge_id : local_workspace.edge_order) {
                    if (local_is_free[edge_id]) continue;
                    const auto &edge = edges_[edge_id];
                    if (edge.from == edge.to || !allowed[edge.from] || !allowed[edge.to]) continue;
                    if (!union_find.merge(edge.from, edge.to)) continue;
                    tree_edges.push_back(edge_id);
                    if (union_find.terminals_connected()) {
                        connected = true;
                        break;
                    }
                }
            }
            if (!connected) return {};
            tree_solution result = prune_tree(std::move(tree_edges), local_terminals);
            result.cost = 0;
            for (int edge_id : result.edges) {
                if (!local_is_free[edge_id]) result.cost += edges_[edge_id].cost;
            }
            return result;

        };

        if (!best.ok) return false;
        std::vector<char> allowed(n_, false);
        for (int edge_id : free_edges) {
            allowed[edges_[edge_id].from] = true;
            allowed[edges_[edge_id].to] = true;
        }
        for (int edge_id : best.edges) {
            allowed[edges_[edge_id].from] = true;
            allowed[edges_[edge_id].to] = true;
        }
        ++statistics.local_move_count;
        tree_solution candidate = kruskal_filtered_with_free_edges(allowed, free_edges, is_free, terminals, workspace);
        if (!candidate.ok || candidate.cost >= best.cost) return false;
        best = std::move(candidate);
        ++statistics.improvement_count;
        return true;
    }

    bool terminal_branch_reconnect(tree_solution &best, const std::vector<int> &terminals,
                                   const std::vector<int> &terminal_index,
                                   const path_store *store, int attempt_limit,
                                   const time_controller &timer, solve_workspace &workspace,
                                   heuristic_steiner_tree_statistics &statistics) const {
        const auto truncate_path_to_target = [&](
            int source,
            const std::vector<int> &input_path,
            const std::vector<char> &target) -> std::pair<std::vector<int>, long long> {
            if (input_path.empty()) return {{}, INF_VALUE};
            std::vector<int> path;
            path.reserve(input_path.size());
            long long cost = 0;
            int vertex = source;
            for (int index = static_cast<int>(input_path.size()) - 1; index >= 0; --index) {
                const int edge_id = input_path[index];
                const auto &edge = edges_[edge_id];
                if (edge.from != vertex && edge.to != vertex) return {{}, INF_VALUE};
                path.push_back(edge_id);
                cost += edge.cost;
                vertex = edge.from == vertex ? edge.to : edge.from;
                if (target[vertex]) return {std::move(path), cost};
            }
            return {{}, INF_VALUE};

        };

        if (!best.ok || best.edges.empty()) return false;
        const tree_view view = make_tree_view(best);
        std::vector<char> is_terminal(n_, false);
        for (int terminal : terminals) is_terminal[terminal] = true;
        struct branch {
            int terminal = -1;
            long long cost = 0;
            std::vector<int> edges;
        };
        std::vector<branch> branches;

        // 葉 terminal から最初の key vertex までを 1 本の branch として抽出する
        for (int terminal : terminals) {
            if (view.degree[terminal] != 1) continue;
            branch current;
            current.terminal = terminal;
            int previous = -1;
            int vertex = terminal;
            for (int step = 0; step <= static_cast<int>(best.edges.size()); ++step) {
                int next_vertex = -1;
                int next_edge = -1;
                for (int arc_index = view.head[vertex]; arc_index >= 0; arc_index = view.next[arc_index]) {
                    if (view.to[arc_index] == previous) continue;
                    next_vertex = view.to[arc_index];
                    next_edge = best.edges[view.local_edge[arc_index]];
                    break;
                }
                if (next_vertex < 0) break;
                current.edges.push_back(next_edge);
                current.cost += edges_[next_edge].cost;
                previous = vertex;
                vertex = next_vertex;
                if (is_terminal[vertex] || view.degree[vertex] != 2) break;
            }
            if (current.edges.size() < best.edges.size()) branches.push_back(std::move(current));
        }
        std::sort(branches.begin(), branches.end(), [](const branch &lhs, const branch &rhs) {
            return lhs.cost > rhs.cost;
        });

        std::vector<int> removed_mark(edges_.size(), 0);
        int stamp = 0;
        const int tries = std::min(attempt_limit, static_cast<int>(branches.size()));
        for (int branch_index = 0; branch_index < tries; ++branch_index) {
            if (timer.over()) break;
            const auto &current = branches[branch_index];
            ++stamp;
            for (int edge_id : current.edges) removed_mark[edge_id] = stamp;
            std::vector<char> target(n_, false);
            for (int edge_id : best.edges) {
                if (removed_mark[edge_id] == stamp) continue;
                target[edges_[edge_id].from] = true;
                target[edges_[edge_id].to] = true;
            }

            // cache があれば距離表を走査し、なければ nearest-target Dijkstra を行う
            int goal = -1;
            long long replacement_cost = INF_VALUE;
            std::vector<int> path;
            if (store != nullptr) {
                const int source_index = terminal_index[current.terminal];
                for (int vertex = 0; vertex < n_; ++vertex) {
                    if (!target[vertex]) continue;
                    const long long distance = cached_terminal_distance(*store, source_index, vertex);
                    if (distance < replacement_cost) {
                        replacement_cost = distance;
                        goal = vertex;
                    }
                }
                if (goal >= 0 && replacement_cost < current.cost) {
                    path = restore_cached_terminal_path(*store, source_index, goal, terminals);
                }
            } else {
                goal = dijkstra_to_target(std::vector<int>{current.terminal}, target, current.cost,
                                          workspace, statistics);
                if (goal >= 0) {
                    replacement_cost = workspace.dist[goal];
                    path = restore_parent_path(current.terminal, goal, workspace.parent_edge);
                }
            }
            ++statistics.local_move_count;
            if (goal < 0 || replacement_cost >= current.cost || path.empty()) continue;
            auto [short_path, short_cost] = truncate_path_to_target(current.terminal, path, target);
            if (short_path.empty() || short_cost >= current.cost) continue;
            std::vector<int> candidate;
            candidate.reserve(best.edges.size() - current.edges.size() + short_path.size());
            for (int edge_id : best.edges) {
                if (removed_mark[edge_id] != stamp) candidate.push_back(edge_id);
            }
            candidate.insert(candidate.end(), short_path.begin(), short_path.end());
            std::sort(candidate.begin(), candidate.end());
            best.edges = std::move(candidate);
            best.cost = best.cost - current.cost + short_cost;
            ++statistics.improvement_count;
            return true;
        }
        return false;
    }

    std::vector<key_path> enumerate_key_paths(
        const tree_solution &best, const std::vector<int> &terminals,
        const tree_view &view, const std::vector<char> *is_free = nullptr) const {
        std::vector<char> is_terminal(n_, false);
        for (int terminal : terminals) is_terminal[terminal] = true;
        std::vector<char> is_key(n_, false);
        for (int vertex = 0; vertex < n_; ++vertex) {
            is_key[vertex] = is_terminal[vertex] || view.degree[vertex] != 2;
        }
        std::vector<char> visited(best.edges.size(), false);
        std::vector<key_path> paths;

        // key vertex から未訪問辺を辿り、内部が次数 2 の最大経路を列挙する
        for (int start_vertex = 0; start_vertex < n_; ++start_vertex) {
            if (!is_key[start_vertex] || view.degree[start_vertex] == 0) continue;
            for (int first_arc = view.head[start_vertex]; first_arc >= 0; first_arc = view.next[first_arc]) {
                const int first_local = view.local_edge[first_arc];
                if (visited[first_local]) continue;
                key_path path;
                path.endpoint_a = start_vertex;
                int previous_vertex = start_vertex;
                int vertex = view.to[first_arc];
                int local_id = first_local;
                while (true) {
                    visited[local_id] = true;
                    path.local_edges.push_back(local_id);
                    const int global_id = best.edges[local_id];
                    if (is_free == nullptr || !(*is_free)[global_id]) {
                        path.cost += edges_[global_id].cost;
                    }
                    if (is_key[vertex]) {
                        path.endpoint_b = vertex;
                        break;
                    }
                    int next_arc = -1;
                    for (int arc_index = view.head[vertex]; arc_index >= 0; arc_index = view.next[arc_index]) {
                        if (view.to[arc_index] != previous_vertex) {
                            next_arc = arc_index;
                            break;
                        }
                    }
                    if (next_arc < 0) break;
                    previous_vertex = vertex;
                    vertex = view.to[next_arc];
                    local_id = view.local_edge[next_arc];
                }
                if (path.endpoint_b >= 0) paths.push_back(std::move(path));
            }
        }
        std::sort(paths.begin(), paths.end(), [](const key_path &lhs, const key_path &rhs) {
            if (lhs.cost != rhs.cost) return lhs.cost > rhs.cost;
            return lhs.local_edges.size() > rhs.local_edges.size();
        });
        return paths;
    }

    bool key_path_exchange(tree_solution &best, const std::vector<int> &terminals,
                           int attempt_limit, const time_controller &timer,
                           solve_workspace &workspace,
                           heuristic_steiner_tree_statistics &statistics) const {
        if (!best.ok || best.edges.empty()) return false;
        const tree_view view = make_tree_view(best);
        const auto paths = enumerate_key_paths(best, terminals, view);
        const int tries = std::min(attempt_limit, static_cast<int>(paths.size()));
        std::vector<char> blocked(best.edges.size(), false);

        for (int path_index = 0; path_index < tries; ++path_index) {
            if (timer.over()) break;
            const auto &path = paths[path_index];
            std::fill(blocked.begin(), blocked.end(), false);
            for (int local_id : path.local_edges) blocked[local_id] = true;
            std::vector<char> component_a(n_, false);
            std::vector<char> component_b(n_, false);
            std::vector<int> vertices_a;
            std::vector<int> vertices_b;

            auto collect_component = [&](int source, std::vector<char> &mark,
                                         std::vector<int> &vertices) {
                std::vector<int> stack{source};
                mark[source] = true;
                while (!stack.empty()) {
                    const int vertex = stack.back();
                    stack.pop_back();
                    vertices.push_back(vertex);
                    for (int arc_index = view.head[vertex]; arc_index >= 0;
                         arc_index = view.next[arc_index]) {
                        if (blocked[view.local_edge[arc_index]]) continue;
                        const int adjacent = view.to[arc_index];
                        if (mark[adjacent]) continue;
                        mark[adjacent] = true;
                        stack.push_back(adjacent);
                    }
                }
            };
            collect_component(path.endpoint_a, component_a, vertices_a);
            collect_component(path.endpoint_b, component_b, vertices_b);
            if (vertices_a.empty() || vertices_b.empty()) continue;

            // 小さい成分を multi-source とし、もう一方へ最短路で再接続する
            const bool use_a_as_source = vertices_a.size() <= vertices_b.size();
            const std::vector<int> &sources = use_a_as_source ? vertices_a : vertices_b;
            const std::vector<char> &target_mark = use_a_as_source ? component_b : component_a;
            const int goal = dijkstra_to_target(sources, target_mark, path.cost,
                                                workspace, statistics);
            ++statistics.local_move_count;
            if (goal < 0) continue;
            const long long replacement_cost = workspace.dist[goal];
            auto replacement = restore_to_any_source(goal, workspace.parent_edge);
            if (replacement_cost >= path.cost || replacement.empty()) continue;

            std::vector<int> candidate;
            candidate.reserve(best.edges.size() - path.local_edges.size() + replacement.size());
            for (int local_id = 0; local_id < static_cast<int>(best.edges.size()); ++local_id) {
                if (!blocked[local_id]) candidate.push_back(best.edges[local_id]);
            }
            candidate.insert(candidate.end(), replacement.begin(), replacement.end());
            auto improved = normalize_candidate(std::move(candidate), terminals);
            if (improved.ok && improved.cost < best.cost) {
                best = std::move(improved);
                ++statistics.improvement_count;
                return true;
            }
        }
        return false;
    }

    bool key_path_exchange_with_free_edges(
        tree_solution &best, const std::vector<int> &terminals,
        const std::vector<int> &free_edges, const std::vector<char> &is_free,
        int attempt_limit, const time_controller &timer, solve_workspace &workspace,
        heuristic_steiner_tree_statistics &statistics) const {
        if (!best.ok || best.edges.empty()) return false;
        const tree_view view = make_tree_view(best);
        const auto paths = enumerate_key_paths(best, terminals, view, &is_free);
        const int tries = std::min(attempt_limit, static_cast<int>(paths.size()));
        std::vector<char> blocked(best.edges.size(), false);

        for (int path_index = 0; path_index < tries; ++path_index) {
            if (timer.over()) break;
            const auto &path = paths[path_index];
            if (path.cost == 0) break;
            std::fill(blocked.begin(), blocked.end(), false);
            for (int local_id : path.local_edges) blocked[local_id] = true;
            std::vector<char> component_a(n_, false);
            std::vector<char> component_b(n_, false);
            std::vector<int> vertices_a;
            std::vector<int> vertices_b;

            auto collect_component = [&](int source, std::vector<char> &mark,
                                         std::vector<int> &vertices) {
                std::vector<int> stack{source};
                mark[source] = true;
                while (!stack.empty()) {
                    const int vertex = stack.back();
                    stack.pop_back();
                    vertices.push_back(vertex);
                    for (int arc_index = view.head[vertex]; arc_index >= 0;
                         arc_index = view.next[arc_index]) {
                        if (blocked[view.local_edge[arc_index]]) continue;
                        const int adjacent = view.to[arc_index];
                        if (mark[adjacent]) continue;
                        mark[adjacent] = true;
                        stack.push_back(adjacent);
                    }
                }
            };
            collect_component(path.endpoint_a, component_a, vertices_a);
            collect_component(path.endpoint_b, component_b, vertices_b);
            if (vertices_a.empty() || vertices_b.empty()) continue;

            const bool use_a_as_source = vertices_a.size() <= vertices_b.size();
            const std::vector<int> &sources = use_a_as_source ? vertices_a : vertices_b;
            const std::vector<char> &target_mark = use_a_as_source ? component_b : component_a;
            const int goal = dijkstra_to_target_with_free_edges(
                sources, target_mark, path.cost, is_free, workspace, statistics);
            ++statistics.local_move_count;
            if (goal < 0) continue;
            const long long replacement_cost = workspace.dist[goal];
            auto replacement = restore_to_any_source(goal, workspace.parent_edge);
            if (replacement_cost >= path.cost || replacement.empty()) continue;

            std::vector<int> candidate;
            candidate.reserve(best.edges.size() - path.local_edges.size() + replacement.size());
            for (int local_id = 0; local_id < static_cast<int>(best.edges.size()); ++local_id) {
                if (!blocked[local_id]) candidate.push_back(best.edges[local_id]);
            }
            candidate.insert(candidate.end(), replacement.begin(), replacement.end());
            tree_solution improved = normalize_candidate_with_free_edges(
                std::move(candidate), free_edges, is_free, terminals);
            if (improved.ok && improved.cost < best.cost) {
                best = std::move(improved);
                ++statistics.improvement_count;
                return true;
            }
        }
        return false;
    }

    static bool same_solution(const tree_solution &lhs, const tree_solution &rhs) {
        return lhs.cost == rhs.cost && lhs.edges == rhs.edges;
    }

    static void add_elite(std::vector<tree_solution> &elite, tree_solution candidate, int limit) {
        if (!candidate.ok) return;
        for (const auto &stored : elite) {
            if (same_solution(stored, candidate)) return;
        }
        elite.push_back(std::move(candidate));
        std::sort(elite.begin(), elite.end(), [](const tree_solution &lhs, const tree_solution &rhs) {
            if (lhs.cost != rhs.cost) return lhs.cost < rhs.cost;
            return lhs.edges.size() < rhs.edges.size();
        });
        if (static_cast<int>(elite.size()) > limit) elite.resize(limit);
    }

    static void remember_elite_seeds(
        const std::vector<tree_solution> &elite,
        std::vector<tree_solution> &local_seed_pool) {
        const int add_count = std::min(2, static_cast<int>(elite.size()));
        for (int index = 0; index < add_count; ++index) {
            bool duplicate = false;
            for (const auto &stored : local_seed_pool) {
                if (same_solution(stored, elite[index])) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate && local_seed_pool.size() < 12) {
                local_seed_pool.push_back(elite[index]);
            }
        }
    }

    // 従来のelite局所探索を完了してから、残り予算で最良解だけを改善する。
    void final_refinement(tree_solution &best, const std::vector<int> &terminals,
        const std::vector<int> &free_edges, const std::vector<char> &is_free,
        bool quality, const time_controller &timer, solve_workspace &workspace,
        heuristic_steiner_tree_statistics &statistics) const {
        struct branch_neighborhood {
            long long cost = 0;
            int vertex = -1;
            size_t begin = 0, end = 0;
        };

        // 頂点ごとに全key-pathを再走査せず、端点の索引を1回だけ構築する。
        // 各頂点内のpath順と、候補の費用降順・頂点番号昇順は従来の列挙と同じ。
        const auto enumerate_branch_neighborhoods = [&](
            const std::vector<key_path> &paths,
            const std::vector<int> &local_terminals) -> std::pair<std::vector<branch_neighborhood>, std::vector<int>> {
            std::vector<std::pair<int, int>> incidence;
            incidence.reserve(paths.size() * 2);
            for (int index = 0; index < static_cast<int>(paths.size()); ++index) {
                incidence.emplace_back(paths[index].endpoint_a, index);
                incidence.emplace_back(paths[index].endpoint_b, index);
            }
            std::sort(incidence.begin(), incidence.end());
            std::vector<branch_neighborhood> branches;
            branches.reserve(paths.size());
            std::vector<int> path_indices;
            path_indices.reserve(paths.size() * 2);
            for (size_t begin = 0; begin < incidence.size();) {
                size_t end = begin + 1;
                const int vertex = incidence[begin].first;
                while (end < incidence.size() && incidence[end].first == vertex) ++end;
                if (end - begin >= 3 && !std::binary_search(local_terminals.begin(), local_terminals.end(), vertex)) {
                    branch_neighborhood branch;
                    branch.vertex = vertex;
                    branch.begin = path_indices.size();
                    for (size_t i = begin; i < end; ++i) {
                        const int index = incidence[i].second;
                        branch.cost += paths[index].cost;
                        path_indices.push_back(index);
                    }
                    branch.end = path_indices.size();
                    branches.push_back(std::move(branch));
                }
                begin = end;
            }
            std::sort(branches.begin(), branches.end(), [](const auto &a, const auto &b) {
                return a.cost != b.cost ? a.cost > b.cost : a.vertex < b.vertex;
            });
            return {std::move(branches), std::move(path_indices)};

        };

        // 隣接する次数3の分岐2点を除去し、残る4成分の最小追加費用接続を探索する。
        const auto reconnect_four_components = [&](
            tree_solution &local_best,
            const std::vector<int> &local_terminals,
            const std::vector<int> &local_free_edges,
            const std::vector<char> &local_is_free,
            bool local_quality,
            const time_controller &local_timer,
            solve_workspace &local_workspace,
            heuristic_steiner_tree_statistics &local_statistics) {
            if (!local_best.ok || local_best.cost == 0 || local_timer.over()) return;
            const auto original = local_best;
            const auto view = make_tree_view(original);
            const auto paths = enumerate_key_paths(original, local_terminals, view,
                local_free_edges.empty() ? nullptr : &local_is_free);
            const auto indexed = enumerate_branch_neighborhoods(paths, local_terminals);
            std::vector<int> branch_index(n_, -1);
            for (int i = 0; i < static_cast<int>(indexed.first.size()); ++i) {
                const auto &branch = indexed.first[i];
                if (branch.end - branch.begin == 3) branch_index[branch.vertex] = i;
            }
            std::vector<std::tuple<long long, int, int>> regions;
            for (const auto &path : paths) {
                const int a = branch_index[path.endpoint_a], b = branch_index[path.endpoint_b];
                if (a < 0 || b < 0) continue;
                regions.emplace_back(indexed.first[a].cost + indexed.first[b].cost - path.cost,
                                     path.endpoint_a, path.endpoint_b);
            }
            std::sort(regions.begin(), regions.end(), [](const auto &a, const auto &b) {
                if (std::get<0>(a) != std::get<0>(b)) return std::get<0>(a) > std::get<0>(b);
                return a < b;
            });
            const int tries = std::min(static_cast<int>(regions.size()), local_quality ? 2 : 1);
            const std::array<std::array<int, 2>, 6> pairs{{{{0,1}},{{0,2}},{{0,3}},{{1,2}},{{1,3}},{{2,3}}}};
            const std::vector<char> no_target(n_, false);
            for (int attempt = 0; attempt < tries && !local_timer.over(); ++attempt) {
                const auto [limit, left_center, right_center] = regions[attempt];
                const auto center = [&](int v) { return v == left_center || v == right_center; };
                std::vector<char> blocked(original.edges.size());
                std::vector<int> endpoints;
                for (const auto &path : paths) if (center(path.endpoint_a) || center(path.endpoint_b)) {
                    for (int local : path.local_edges) blocked[local] = true;
                    if (!center(path.endpoint_a)) endpoints.push_back(path.endpoint_a);
                    if (!center(path.endpoint_b)) endpoints.push_back(path.endpoint_b);
                }
                std::sort(endpoints.begin(), endpoints.end());
                endpoints.erase(std::unique(endpoints.begin(), endpoints.end()), endpoints.end());
                if (endpoints.size() != 4) continue;
                auto forest = local_free_edges;
                for (size_t i = 0; i < original.edges.size(); ++i)
                    if (!blocked[i]) forest.push_back(original.edges[i]);
                const auto zero = make_edge_mark(forest);
                std::array<std::vector<long long>, 4> single;
                std::array<std::vector<int>, 4> single_parent;
                for (int side = 0; side < 4; ++side) {
                    if (local_timer.over()) return;
                    std::vector<int> sources{endpoints[side]};
                    std::vector<char> seen(n_); seen[sources[0]] = true;
                    for (size_t i = 0; i < sources.size(); ++i)
                        for (int a = view.head[sources[i]]; a >= 0; a = view.next[a]) {
                            const int v = view.to[a];
                            if (!blocked[view.local_edge[a]] && !seen[v]) { seen[v] = true; sources.push_back(v); }
                        }
                    dijkstra_to_target_with_free_edges(sources, no_target, limit + 1, zero, local_workspace, local_statistics);
                    single[side].assign(n_, INF_VALUE);
                    single_parent[side].assign(n_, -1);
                    for (int v = 0; v < n_; ++v) if (local_workspace.visit_stamp[v] == local_workspace.current_stamp) {
                        single[side][v] = local_workspace.dist[v]; single_parent[side][v] = local_workspace.parent_edge[v];
                    }
                }
                std::array<std::vector<long long>, 6> joined;
                std::array<std::vector<int>, 6> joined_parent;
                for (int pair = 0; pair < 6; ++pair) {
                    if (local_timer.over()) return;
                    // 相補な2成分を結ぶ費用が最低限必要なので、その下界を差し引く。
                    const auto &other = pairs[5 - pair];
                    const long long pair_limit = limit - single[other[0]][endpoints[other[1]]];
                    local_workspace.dist.assign(n_, INF_VALUE);
                    local_workspace.parent_edge.assign(n_, -1);
                    local_workspace.heap.clear();
                    for (int v = 0; v < n_; ++v) {
                        const long long cost = single[pairs[pair][0]][v] + single[pairs[pair][1]][v];
                        if (cost > pair_limit) continue;
                        local_workspace.dist[v] = cost;
                        local_workspace.heap.push(static_cast<unsigned long long>(cost), v);
                    }
                    ++local_statistics.dijkstra_count;
                    while (!local_workspace.heap.empty()) {
                        const auto [unsigned_distance, vertex] = local_workspace.heap.pop();
                        const long long distance = static_cast<long long>(unsigned_distance);
                        if (distance != local_workspace.dist[vertex]) continue;
                        for (int a = local_workspace.start[vertex]; a < local_workspace.start[vertex + 1]; ++a) {
                            ++local_statistics.edge_relaxation_count;
                            const auto &arc = local_workspace.arcs[a];
                            const long long cost = zero[arc.edge_id] ? 0 : arc.cost;
                            if (cost > pair_limit - distance) continue;
                            const long long next = distance + cost;
                            if (next >= local_workspace.dist[arc.to]) continue;
                            local_workspace.dist[arc.to] = next;
                            local_workspace.parent_edge[arc.to] = arc.edge_id;
                            local_workspace.heap.push(static_cast<unsigned long long>(next), arc.to);
                        }
                    }
                    joined[pair] = local_workspace.dist;
                    joined_parent[pair] = local_workspace.parent_edge;
                }
                long long min_sum = INF_VALUE;
                int partition = -1, junction = -1;
                for (int pair = 0; pair < 3; ++pair) for (int v = 0; v < n_; ++v) {
                    const long long sum = joined[pair][v] + joined[5 - pair][v];
                    if (sum < min_sum) { min_sum = sum; partition = pair; junction = v; }
                }
                if (junction < 0 || local_timer.over()) continue;
                for (int pair : {partition, 5 - partition}) {
                    int v = junction;
                    for (int step = 0; step <= n_; ++step) {
                        const int id = joined_parent[pair][v];
                        if (id < 0) break;
                        forest.push_back(id);
                        const auto &edge = edges_[id];
                        v = edge.from == v ? edge.to : edge.from;
                    }
                    for (int side : pairs[pair]) {
                        auto path = restore_to_any_source(v, single_parent[side]);
                        forest.insert(forest.end(), path.begin(), path.end());
                    }
                }
                auto candidate = local_free_edges.empty() ? normalize_candidate(std::move(forest), local_terminals) :
                    normalize_candidate_with_free_edges(std::move(forest), local_free_edges, local_is_free, local_terminals);
                ++local_statistics.local_move_count;
                if (candidate.ok && candidate.cost < local_best.cost) {
                    local_best = std::move(candidate); ++local_statistics.improvement_count;
                }
            }

        };

        // 分岐に接する最大経路をまとめて外し、残存辺をseedとするVoronoiで再構築する。
        const auto destroy_branch = [&](
            tree_solution &local_best,
            const std::vector<int> &local_terminals,
            const std::vector<int> &local_free_edges,
            const std::vector<char> &local_is_free,
            bool local_quality,
            const time_controller &local_timer,
            solve_workspace &local_workspace,
            heuristic_steiner_tree_statistics &local_statistics) {
            if (!local_best.ok || local_best.cost == 0 || local_timer.over()) return;
            const auto original = local_best;
            const auto view = make_tree_view(original);
            const auto paths = enumerate_key_paths(original, local_terminals, view,
                local_free_edges.empty() ? nullptr : &local_is_free);
            const auto [order, path_indices] = enumerate_branch_neighborhoods(paths, local_terminals);
            const int tries = std::min(static_cast<int>(order.size()), local_quality ? 12 : 4);
            for (int i = 0; i < tries && !local_timer.over(); ++i) {
                std::vector<char> removed(original.edges.size());
                for (size_t slot = order[i].begin; slot < order[i].end; ++slot)
                    for (int local : paths[path_indices[slot]].local_edges) removed[local] = true;
                auto seed = local_free_edges;
                for (size_t j = 0; j < original.edges.size(); ++j)
                    if (!removed[j]) seed.push_back(original.edges[j]);
                const auto mark = make_edge_mark(seed);
                auto candidate = seeded_voronoi_candidate(local_terminals, seed, mark, local_workspace, local_statistics);
                auto improved = local_free_edges.empty() ? normalize_candidate(std::move(candidate), local_terminals) :
                    normalize_candidate_with_free_edges(std::move(candidate), local_free_edges, local_is_free, local_terminals);
                ++local_statistics.local_move_count;
                if (improved.ok && improved.cost < local_best.cost) {
                    local_best = std::move(improved);
                    ++local_statistics.improvement_count;
                }
            }

        };

        // 最良解の次数3の非terminal分岐を外し、3成分の距離和が最小の接合点へ移す。
        const auto relocate_branch = [&](
            tree_solution &local_best,
            const std::vector<int> &local_terminals,
            const std::vector<int> &local_free_edges,
            const std::vector<char> &local_is_free,
            bool local_quality,
            const time_controller &local_timer,
            solve_workspace &local_workspace,
            heuristic_steiner_tree_statistics &local_statistics) {
            if (!local_best.ok || local_best.cost == 0 || local_timer.over()) return;
            const auto original = local_best;
            const auto view = make_tree_view(original);
            const auto paths = enumerate_key_paths(original, local_terminals, view,
                local_free_edges.empty() ? nullptr : &local_is_free);
            auto [order, path_indices] = enumerate_branch_neighborhoods(paths, local_terminals);
            order.erase(std::remove_if(order.begin(), order.end(), [](const auto &branch) {
                return branch.end - branch.begin != 3;
            }), order.end());
            const int tries = std::min(static_cast<int>(order.size()), local_quality ? 8 : 2);
            for (int i = 0; i < tries && !local_timer.over(); ++i) {
                const int center = order[i].vertex;
                std::vector<char> blocked(original.edges.size());
                std::vector<int> endpoints;
                for (size_t slot = order[i].begin; slot < order[i].end; ++slot) {
                    const auto &p = paths[path_indices[slot]];
                    endpoints.push_back(p.endpoint_a == center ? p.endpoint_b : p.endpoint_a);
                    for (int local : p.local_edges) blocked[local] = true;
                }
                if (endpoints.size() != 3) continue;
                auto forest = local_free_edges;
                for (size_t j = 0; j < original.edges.size(); ++j)
                    if (!blocked[j]) forest.push_back(original.edges[j]);
                const auto zero = make_edge_mark(forest);
                std::array<std::vector<long long>, 3> distance;
                std::array<std::vector<int>, 3> trace;
                for (int side = 0; side < 3; ++side) {
                    if (local_timer.over()) return;
                    std::vector<int> sources{endpoints[side]};
                    std::vector<char> seen(n_); seen[sources[0]] = true;
                    for (size_t j = 0; j < sources.size(); ++j)
                        for (int a = view.head[sources[j]]; a >= 0; a = view.next[a]) {
                            const int v = view.to[a];
                            if (!blocked[view.local_edge[a]] && !seen[v]) { seen[v] = true; sources.push_back(v); }
                        }

                    // 旧3経路がこの費用の接続を与えるため、最小距離和は旧費用以下。
                    // 非負距離なので各距離も同じ上限以下。整数の同値境界を含める。
                    // d(A,v)+d(C,v)>=d(A,C) より、2本目は C-d(A,C) 以下だけで十分。
                    // 3本目も同様。残存森の各成分は0-costで連結なので端点間距離で下界が取れる。
                    const long long pair_lower = side == 0 ? 0 : distance[0][endpoints[side == 1 ? 2 : 1]];
                    const long long limit = order[i].cost - pair_lower + 1;
                    const std::vector<char> no_target(n_, false);
                    dijkstra_to_target_with_free_edges(sources, no_target, limit, zero, local_workspace, local_statistics);
                    distance[side].assign(n_, INF_VALUE);
                    trace[side].assign(n_, -1);
                    for (int v = 0; v < n_; ++v) if (local_workspace.visit_stamp[v] == local_workspace.current_stamp) {
                        distance[side][v] = local_workspace.dist[v]; trace[side][v] = local_workspace.parent_edge[v];
                    }
                }
                long long min_sum = INF_VALUE;
                int junction = -1;
                for (int v = 0; v < n_; ++v) {
                    const long long sum = distance[0][v] + distance[1][v] + distance[2][v];
                    if (sum < min_sum) { min_sum = sum; junction = v; }
                }
                if (junction < 0) continue;
                for (int side = 0; side < 3; ++side) {
                    auto path = restore_to_any_source(junction, trace[side]);
                    forest.insert(forest.end(), path.begin(), path.end());
                }
                auto improved = local_free_edges.empty() ? normalize_candidate(std::move(forest), local_terminals) :
                    normalize_candidate_with_free_edges(std::move(forest), local_free_edges, local_is_free, local_terminals);
                ++local_statistics.local_move_count;
                if (improved.ok && improved.cost < local_best.cost) {
                    local_best = std::move(improved);
                    ++local_statistics.improvement_count;
                }
            }

        };

        const long long before = best.cost;
        relocate_branch(best, terminals, free_edges, is_free, quality, timer, workspace, statistics);
        destroy_branch(best, terminals, free_edges, is_free, quality, timer, workspace, statistics);
        reconnect_four_components(best, terminals, free_edges, is_free, quality, timer, workspace, statistics);
        if (best.cost < before && !timer.over()) {
            if (free_edges.empty()) key_path_exchange(best, terminals, quality ? 16 : 6, timer, workspace, statistics);
            else key_path_exchange_with_free_edges(best, terminals, free_edges, is_free, quality ? 16 : 6, timer, workspace, statistics);
            if (free_edges.empty()) induced_mst_improve(best, terminals, workspace, statistics);
            else induced_mst_improve_with_free_edges(best, terminals, free_edges, is_free, workspace, statistics);
        }
    }

public:
    // n 頂点の空グラフを構築する O(1)
    explicit heuristic_steiner_tree(int n) : n_(n) {
        assert(n >= 0);
    }

    // 無向辺を追加し、その 0-indexed 辺番号を返す O(1)
    int add_edge(int u, int v, long long w) {
        assert(0 <= u && u < n_ && 0 <= v && v < n_ && 0 <= w);
        const int edge_id = static_cast<int>(edges_.size());
        edges_.push_back(edge_type{u, v, w});
        workspace_ready_ = false;
        terminal_store_ready_ = false;
        return edge_id;
    }

    // 入力辺 L 本だけを MST 化し、非 terminal の葉を削除する O(K log K + N + L log L)
    heuristic_steiner_tree_result normalize(
        const std::vector<int> &input_terminals,
        const std::vector<int> &candidate_edge_ids) const {
        const std::vector<int> terminals = normalize_terminals(input_terminals);
        heuristic_steiner_tree_result result;
        for (int terminal : terminals) {
            assert(0 <= terminal && terminal < n_);
            (void)terminal;
        }
        for (int edge_id : candidate_edge_ids) {
            assert(0 <= edge_id && edge_id < static_cast<int>(edges_.size()));
            (void)edge_id;
        }
        tree_solution normalized = normalize_candidate(candidate_edge_ids, terminals);
        result.ok = normalized.ok;
        result.cost = normalized.ok ? normalized.cost : INF_VALUE;
        result.edges = std::move(normalized.edges);
        return result;
    }

    // 非連結な部分辺集合を追加・削除で修復する O(K(N + M log C) + A(N + M log C + L log L))
    heuristic_steiner_tree_result repair(
        const std::vector<int> &input_terminals,
        const std::vector<int> &partial_edge_ids,
        const heuristic_steiner_tree_options &options = {}) const {
        const std::vector<int> terminals = normalize_terminals(input_terminals);
        heuristic_steiner_tree_result result;
        for (int terminal : terminals) {
            assert(0 <= terminal && terminal < n_);
            (void)terminal;
        }
        for (int edge_id : partial_edge_ids) {
            assert(0 <= edge_id && edge_id < static_cast<int>(edges_.size()));
            (void)edge_id;
        }
        if (terminals.size() <= 1) {
            result.ok = true;
            return result;
        }
        if (partial_edge_ids.empty()) return solve(terminals, options);

        solve_workspace &workspace = reusable_workspace_;
        if (!workspace_ready_) {
            build_workspace(workspace);
            workspace_ready_ = true;
        }
        heuristic_steiner_tree_statistics statistics;
        const time_controller timer(options);
        const std::vector<char> is_seed = make_edge_mark(partial_edge_ids);
        const bool fast = options.preset == heuristic_steiner_tree_preset::fast;
        const bool quality = options.preset == heuristic_steiner_tree_preset::quality;
        tree_solution best;
        auto consider = [&](tree_solution candidate) {
            if (!candidate.ok) return;
            if (!best.ok || candidate.cost < best.cost) best = std::move(candidate);
        };

        consider(normalize_candidate(partial_edge_ids, terminals));
        if (!best.ok || !timer.over()) {
            auto candidate = seeded_voronoi_candidate(
                terminals, partial_edge_ids, is_seed, workspace, statistics);
            consider(normalize_candidate(std::move(candidate), terminals));
        }
        if (!timer.over()) {
            consider(voronoi_mst(terminals, workspace, statistics));
        }

        if (best.ok && !timer.over()) {
            induced_mst_improve(best, terminals, workspace, statistics);
        }
        if (best.ok && !fast && !timer.over()) {
            const int terminal_count = static_cast<int>(terminals.size());
            heuristic_steiner_tree_path_mode mode = choose_path_mode(options, terminal_count, timer);
            const size_t terminal_bytes = static_cast<size_t>(terminal_count) * n_ *
                (sizeof(long long) + sizeof(int));
            if (mode == heuristic_steiner_tree_path_mode::terminal_cache &&
                terminal_bytes > options.memory_limit_bytes) {
                mode = heuristic_steiner_tree_path_mode::on_demand;
            }
            path_store *store_pointer = nullptr;
            if (mode == heuristic_steiner_tree_path_mode::terminal_cache && !timer.over()) {
                store_pointer = cached_terminal_store(
                    terminals, workspace, statistics);
                statistics.path_memory_bytes = terminal_bytes;
            } else {
                mode = heuristic_steiner_tree_path_mode::on_demand;
            }
            statistics.path_mode_used = mode;

            std::vector<int> terminal_index(n_, -1);
            for (int index = 0; index < terminal_count; ++index) {
                terminal_index[terminals[index]] = index;
            }
            const int branch_rounds = quality ? 8 : 2;
            const int branch_attempts = quality ? 24 : 8;
            for (int round = 0; round < branch_rounds && !timer.over(); ++round) {
                if (!terminal_branch_reconnect(best, terminals, terminal_index, store_pointer,
                                               branch_attempts, timer, workspace, statistics)) break;
                induced_mst_improve(best, terminals, workspace, statistics);
            }
            const int key_rounds = quality ? 4 : 1;
            const int key_attempts = quality ? 16 : 6;
            for (int round = 0; round < key_rounds && !timer.over(); ++round) {
                if (!key_path_exchange(best, terminals, key_attempts,
                                       timer, workspace, statistics)) break;
                induced_mst_improve(best, terminals, workspace, statistics);
            }
        }

        if (options.preset != heuristic_steiner_tree_preset::fast)
            final_refinement(best, terminals, {}, {}, quality, timer, workspace, statistics);
        result.ok = best.ok;
        result.cost = best.ok ? best.cost : INF_VALUE;
        result.edges = std::move(best.edges);
        result.statistics = statistics;
        return result;
    }

    // 撤去不能な既設辺を費用 0 として追加辺を求める O(A(K(N + M log C) + L log L + K^2))
    heuristic_steiner_tree_augmentation_result augment(
        const std::vector<int> &input_terminals,
        const std::vector<int> &base_edge_ids,
        const heuristic_steiner_tree_options &options = {}) const {
        const auto terminal_branch_reconnect_with_free_edges = [&](
            tree_solution &best,
            const std::vector<int> &terminals,
            const std::vector<int> &free_edges,
            const std::vector<char> &is_free,
            const std::vector<int> &terminal_index,
            const path_store *store,
            int attempt_limit,
            const time_controller &timer,
            solve_workspace &workspace,
            heuristic_steiner_tree_statistics &statistics) -> bool {
            const auto truncate_path_to_target_with_free_edges = [&](
                int source,
                const std::vector<int> &input_path,
                const std::vector<char> &target,
                const std::vector<char> &local_is_free) -> std::pair<std::vector<int>, long long> {
                if (input_path.empty()) return {{}, INF_VALUE};
                std::vector<int> path;
                path.reserve(input_path.size());
                long long cost = 0;
                int vertex = source;
                for (int index = static_cast<int>(input_path.size()) - 1; index >= 0; --index) {
                    const int edge_id = input_path[index];
                    const auto &edge = edges_[edge_id];
                    if (edge.from != vertex && edge.to != vertex) return {{}, INF_VALUE};
                    path.push_back(edge_id);
                    if (!local_is_free[edge_id]) cost += edge.cost;
                    vertex = edge.from == vertex ? edge.to : edge.from;
                    if (target[vertex]) return {std::move(path), cost};
                }
                return {{}, INF_VALUE};

            };

            if (!best.ok || best.edges.empty()) return false;
            const tree_view view = make_tree_view(best);
            std::vector<char> is_terminal(n_, false);
            for (int terminal : terminals) is_terminal[terminal] = true;
            struct branch {
                int terminal = -1;
                long long cost = 0;
                std::vector<int> edges;
            };
            std::vector<branch> branches;
            for (int terminal : terminals) {
                if (view.degree[terminal] != 1) continue;
                branch current;
                current.terminal = terminal;
                int previous = -1;
                int vertex = terminal;
                for (int step = 0; step <= static_cast<int>(best.edges.size()); ++step) {
                    int next_vertex = -1;
                    int next_edge = -1;
                    for (int arc_index = view.head[vertex]; arc_index >= 0; arc_index = view.next[arc_index]) {
                        if (view.to[arc_index] == previous) continue;
                        next_vertex = view.to[arc_index];
                        next_edge = best.edges[view.local_edge[arc_index]];
                        break;
                    }
                    if (next_vertex < 0) break;
                    current.edges.push_back(next_edge);
                    if (!is_free[next_edge]) current.cost += edges_[next_edge].cost;
                    previous = vertex;
                    vertex = next_vertex;
                    if (is_terminal[vertex] || view.degree[vertex] != 2) break;
                }
                if (current.cost > 0 && current.edges.size() < best.edges.size()) {
                    branches.push_back(std::move(current));
                }
            }
            std::sort(branches.begin(), branches.end(), [](const branch &lhs, const branch &rhs) {
                return lhs.cost > rhs.cost;
            });

            std::vector<int> removed_mark(edges_.size(), 0);
            int stamp = 0;
            const int tries = std::min(attempt_limit, static_cast<int>(branches.size()));
            for (int branch_index = 0; branch_index < tries; ++branch_index) {
                if (timer.over()) break;
                const auto &current = branches[branch_index];
                ++stamp;
                for (int edge_id : current.edges) removed_mark[edge_id] = stamp;
                std::vector<char> target(n_, false);
                for (int edge_id : best.edges) {
                    if (removed_mark[edge_id] == stamp) continue;
                    target[edges_[edge_id].from] = true;
                    target[edges_[edge_id].to] = true;
                }
                int goal = -1;
                long long replacement_cost = INF_VALUE;
                std::vector<int> path;
                if (store != nullptr) {
                    const int source_index = terminal_index[current.terminal];
                    for (int vertex = 0; vertex < n_; ++vertex) {
                        if (!target[vertex]) continue;
                        const long long distance = cached_terminal_distance(
                            *store, source_index, vertex);
                        if (distance < replacement_cost) {
                            replacement_cost = distance;
                            goal = vertex;
                        }
                    }
                    if (goal >= 0 && replacement_cost < current.cost) {
                        path = restore_cached_terminal_path(
                            *store, source_index, goal, terminals);
                    }
                } else {
                    goal = dijkstra_to_target_with_free_edges(
                        std::vector<int>{current.terminal}, target, current.cost,
                        is_free, workspace, statistics);
                    if (goal >= 0) {
                        replacement_cost = workspace.dist[goal];
                        path = restore_parent_path(
                            current.terminal, goal, workspace.parent_edge);
                    }
                }
                ++statistics.local_move_count;
                if (goal < 0 || replacement_cost >= current.cost || path.empty()) continue;
                auto [short_path, short_cost] = truncate_path_to_target_with_free_edges(current.terminal, path, target, is_free);
                if (short_path.empty() || short_cost >= current.cost) continue;
                std::vector<int> candidate;
                candidate.reserve(best.edges.size() - current.edges.size() + short_path.size());
                for (int edge_id : best.edges) {
                    if (removed_mark[edge_id] != stamp) candidate.push_back(edge_id);
                }
                candidate.insert(candidate.end(), short_path.begin(), short_path.end());
                tree_solution improved = normalize_candidate_with_free_edges(
                    std::move(candidate), free_edges, is_free, terminals);
                if (improved.ok && improved.cost < best.cost) {
                    best = std::move(improved);
                    ++statistics.improvement_count;
                    return true;
                }
            }
            return false;

        };

        const auto seeded_sequential_candidate = [&](
            const std::vector<int> &terminals,
            const std::vector<int> &seed_edges,
            const std::vector<char> &is_free,
            int root_index,
            int rcl,
            std::mt19937_64 &random,
            solve_workspace &workspace,
            heuristic_steiner_tree_statistics &statistics) -> std::vector<int> {
            const auto incremental_dijkstra_with_free_edges = [&](
                const std::vector<int> &new_sources,
                const std::vector<char> &free_edge,
                std::vector<long long> &distance,
                std::vector<int> &parent_edge,
                solve_workspace &local_workspace,
                heuristic_steiner_tree_statistics &local_statistics) {
                local_workspace.heap.clear();
                for (int source : new_sources) {
                    if (distance[source] == 0) continue;
                    distance[source] = 0;
                    parent_edge[source] = -1;
                    local_workspace.heap.push(0, source);
                }

                ++local_statistics.dijkstra_count;
                while (!local_workspace.heap.empty()) {
                    const auto [unsigned_distance, vertex] = local_workspace.heap.pop();
                    const long long current_distance = static_cast<long long>(unsigned_distance);
                    if (current_distance != distance[vertex]) continue;
                    for (int arc_index = local_workspace.start[vertex]; arc_index < local_workspace.start[vertex + 1]; ++arc_index) {
                        ++local_statistics.edge_relaxation_count;
                        const auto &arc = local_workspace.arcs[arc_index];
                        const long long cost = free_edge[arc.edge_id] ? 0 : arc.cost;
                        if (current_distance > INF_VALUE - cost) continue;
                        const long long next_distance = current_distance + cost;
                        if (next_distance >= distance[arc.to]) continue;
                        distance[arc.to] = next_distance;
                        parent_edge[arc.to] = arc.edge_id;
                        local_workspace.heap.push(static_cast<unsigned long long>(next_distance), arc.to);
                    }
                }

            };

            const int terminal_count = static_cast<int>(terminals.size());
            std::vector<char> connected(terminal_count, false);
            std::vector<char> in_tree(n_, false);
            std::vector<long long> distance(n_, INF_VALUE);
            std::vector<int> parent_edge(n_, -1);
            connected[root_index] = true;
            in_tree[terminals[root_index]] = true;
            incremental_dijkstra_with_free_edges(std::vector<int>{terminals[root_index]}, is_free, distance, parent_edge, workspace, statistics);
            std::vector<int> candidate = seed_edges;

            for (int added = 1; added < terminal_count; ++added) {
                std::vector<std::pair<long long, int>> top;
                top.reserve(rcl);
                for (int terminal_index = 0; terminal_index < terminal_count; ++terminal_index) {
                    if (connected[terminal_index] || distance[terminals[terminal_index]] == INF_VALUE) continue;
                    const std::pair<long long, int> current{
                        distance[terminals[terminal_index]], terminal_index};
                    auto position = std::lower_bound(top.begin(), top.end(), current);
                    if (position == top.end() && static_cast<int>(top.size()) >= rcl) continue;
                    top.insert(position, current);
                    if (static_cast<int>(top.size()) > rcl) top.pop_back();
                }
                if (top.empty()) return {};
                const auto picked = top[static_cast<size_t>(
                    random() % static_cast<std::uint64_t>(top.size()))];
                const int terminal_index = picked.second;
                const int terminal = terminals[terminal_index];
                auto path = restore_to_any_source(terminal, parent_edge);
                if (path.empty() && distance[terminal] != 0) return {};
                candidate.insert(candidate.end(), path.begin(), path.end());
                connected[terminal_index] = true;

                std::vector<int> new_sources;
                new_sources.reserve(path.size() + 1);
                if (!in_tree[terminal]) {
                    in_tree[terminal] = true;
                    new_sources.push_back(terminal);
                }
                for (int edge_id : path) {
                    const auto &edge = edges_[edge_id];
                    if (!in_tree[edge.from]) {
                        in_tree[edge.from] = true;
                        new_sources.push_back(edge.from);
                    }
                    if (!in_tree[edge.to]) {
                        in_tree[edge.to] = true;
                        new_sources.push_back(edge.to);
                    }
                }
                incremental_dijkstra_with_free_edges(new_sources, is_free, distance, parent_edge, workspace, statistics);
            }
            ++statistics.initial_solution_count;
            return candidate;

        };

        const auto build_terminal_store_with_free_edges = [&](
            const std::vector<int> &terminals,
            const std::vector<char> &is_free,
            path_store &store,
            solve_workspace &workspace,
            heuristic_steiner_tree_statistics &statistics) {
            const int terminal_count = static_cast<int>(terminals.size());
            store.dist.resize(static_cast<size_t>(terminal_count) * n_);
            store.trace.resize(static_cast<size_t>(terminal_count) * n_);
            for (int terminal_index = 0; terminal_index < terminal_count; ++terminal_index) {
                dijkstra_full_with_free_edges(
                    std::vector<int>{terminals[terminal_index]}, false,
                    is_free, workspace, statistics);
                const size_t base = static_cast<size_t>(terminal_index) * n_;
                std::copy(workspace.dist.begin(), workspace.dist.end(),
                          store.dist.begin() + static_cast<std::ptrdiff_t>(base));
                std::copy(workspace.parent_edge.begin(), workspace.parent_edge.end(),
                          store.trace.begin() + static_cast<std::ptrdiff_t>(base));
            }

        };

        const std::vector<int> terminals = normalize_terminals(input_terminals);
        heuristic_steiner_tree_augmentation_result result;
        for (int terminal : terminals) {
            assert(0 <= terminal && terminal < n_);
            (void)terminal;
        }
        for (int edge_id : base_edge_ids) {
            assert(0 <= edge_id && edge_id < static_cast<int>(edges_.size()));
            (void)edge_id;
        }
        if (terminals.size() <= 1) {
            result.ok = true;
            return result;
        }

        solve_workspace &workspace = reusable_workspace_;
        heuristic_steiner_tree_statistics statistics;
        time_controller timer(options);
        std::mt19937_64 random(options.seed);
        const std::vector<char> is_base = make_edge_mark(base_edge_ids);
        tree_solution base_only = normalize_candidate_with_free_edges(
            {}, base_edge_ids, is_base, terminals);
        if (base_only.ok && base_only.cost == 0) {
            result.ok = true;
            result.statistics = statistics;
            return result;
        }

        ensure_workspace(workspace, timer, options);
        const int terminal_count = static_cast<int>(terminals.size());
        const bool fast = options.preset == heuristic_steiner_tree_preset::fast;
        const bool quality = options.preset == heuristic_steiner_tree_preset::quality;
        const int elite_limit = fast ? 0 : (quality ? 8 : 4);
        std::vector<tree_solution> elite;
        std::vector<tree_solution> local_seed_pool;
        tree_solution best;
        auto consider = [&](tree_solution candidate) {
            if (!candidate.ok) return;
            if (elite_limit > 0) add_elite(elite, candidate, elite_limit);
            if (!best.ok || candidate.cost < best.cost) best = std::move(candidate);
        };
        auto voronoi_edges = seeded_voronoi_candidate(
            terminals, base_edge_ids, is_base, workspace, statistics);
        consider(normalize_candidate_with_free_edges(
            std::move(voronoi_edges), base_edge_ids, is_base, terminals));

        heuristic_steiner_tree_path_mode mode = choose_path_mode(
            options, terminal_count, timer);
        const size_t terminal_bytes = static_cast<size_t>(terminal_count) *
            n_ * (sizeof(long long) + sizeof(int));
        if (mode == heuristic_steiner_tree_path_mode::terminal_cache &&
            terminal_bytes > options.memory_limit_bytes) {
            mode = heuristic_steiner_tree_path_mode::on_demand;
        }
        path_store store;
        path_store *store_pointer = nullptr;
        if (!timer.over() && mode == heuristic_steiner_tree_path_mode::terminal_cache) {
            build_terminal_store_with_free_edges(terminals, is_base, store, workspace, statistics);
            store_pointer = &store;
            statistics.path_memory_bytes = terminal_bytes;
        } else {
            mode = heuristic_steiner_tree_path_mode::on_demand;
        }
        statistics.path_mode_used = mode;

        auto sequential_candidate = [&](int root_index, int rcl) {
            std::vector<int> candidate;
            if (store_pointer != nullptr) {
                candidate = sequential_cached_candidate(
                    terminals, *store_pointer, root_index, rcl, random, statistics);
            } else {
                candidate = seeded_sequential_candidate(terminals, base_edge_ids, is_base, root_index, rcl, random, workspace, statistics);
            }
            return normalize_candidate_with_free_edges(
                std::move(candidate), base_edge_ids, is_base, terminals);
        };
        const int sequential_limit = store_pointer == nullptr
            ? ON_DEMAND_SEQUENTIAL_MAX_K : CACHED_SEQUENTIAL_MAX_K;
        if (terminal_count <= sequential_limit && !timer.over_fraction(3, 4)) {
            const int root_index = store_pointer == nullptr
                ? static_cast<int>(random() % terminals.size())
                : 0;
            consider(sequential_candidate(root_index, 1));
        }
        if (quality) remember_elite_seeds(elite, local_seed_pool);

        int restart_limit = options.restart_limit;
        if (restart_limit < 0) {
            if (fast) restart_limit = 0;
            else if (options.time_limit_ms > 0 ||
                     options.deadline != std::chrono::steady_clock::time_point::max()) {
                restart_limit = std::numeric_limits<int>::max();
            } else {
                restart_limit = quality ? 20 : 4;
            }
        }
        for (int restart = 0;
             restart < restart_limit && !timer.over_fraction(3, 4); ++restart) {
            if (terminal_count > sequential_limit) break;
            ++statistics.restart_count;
            const int root_index = static_cast<int>(random() % terminals.size());
            const int rcl = 1 + restart % 4;
            consider(sequential_candidate(root_index, rcl));
            if (quality && (restart + 1) % 4 == 0) {
                remember_elite_seeds(elite, local_seed_pool);
            }
        }

        if (!fast && elite.size() >= 2 && !timer.over_fraction(3, 4)) {
            const size_t original_size = elite.size();
            for (size_t left = 0; left < original_size && !timer.over_fraction(3, 4); ++left) {
                for (size_t right = left + 1;
                     right < original_size && !timer.over_fraction(3, 4); ++right) {
                    std::vector<int> candidate = elite[left].edges;
                    candidate.insert(candidate.end(), elite[right].edges.begin(), elite[right].edges.end());
                    consider(normalize_candidate_with_free_edges(
                        std::move(candidate), base_edge_ids, is_base, terminals));
                }
            }
        }

        std::vector<int> terminal_index;
        if (!fast) {
            terminal_index.assign(n_, -1);
            for (int index = 0; index < terminal_count; ++index) {
                terminal_index[terminals[index]] = index;
            }
        }
        auto improve_candidate = [&](tree_solution candidate) {
            if (!candidate.ok || timer.over()) return candidate;
            induced_mst_improve_with_free_edges(
                candidate, terminals, base_edge_ids, is_base, workspace, statistics);
            if (fast) return candidate;

            const int branch_rounds = quality ? 8 : 2;
            const int branch_attempts = quality ? 24 : 8;
            for (int round = 0; round < branch_rounds && !timer.over(); ++round) {
                if (!terminal_branch_reconnect_with_free_edges(candidate, terminals, base_edge_ids, is_base, terminal_index, store_pointer, branch_attempts, timer, workspace, statistics)) {
                    break;
                }
                induced_mst_improve_with_free_edges(
                    candidate, terminals, base_edge_ids, is_base, workspace, statistics);
            }
            const int key_rounds = quality ? 4 : 1;
            const int key_attempts = quality ? 16 : 6;
            for (int round = 0; round < key_rounds && !timer.over(); ++round) {
                if (!key_path_exchange_with_free_edges(
                        candidate, terminals, base_edge_ids, is_base,
                        key_attempts, timer, workspace, statistics)) {
                    break;
                }
                induced_mst_improve_with_free_edges(
                    candidate, terminals, base_edge_ids, is_base, workspace, statistics);
            }
            return candidate;
        };

        if (best.ok && !timer.over()) {
            std::vector<tree_solution> local_candidates{best};
            const auto append_unique = [&](const tree_solution &candidate) {
                for (const auto &stored : local_candidates) {
                    if (same_solution(stored, candidate)) return;
                }
                local_candidates.push_back(candidate);
            };
            if (quality) {
                for (const auto &candidate : local_seed_pool) append_unique(candidate);
            }
            const int local_elite_limit = std::min(4, static_cast<int>(elite.size()));
            for (int index = quality ? 0 : 1; index < local_elite_limit; ++index) {
                append_unique(elite[index]);
            }
            for (auto &candidate : local_candidates) {
                if (timer.over()) break;
                tree_solution improved = improve_candidate(std::move(candidate));
                if (improved.ok && improved.cost < best.cost) best = std::move(improved);
            }
        }

        if (!fast) final_refinement(best, terminals, base_edge_ids, is_base, quality, timer, workspace, statistics);
        result.ok = best.ok;
        result.added_cost = best.ok ? best.cost : INF_VALUE;
        if (best.ok) {
            for (int edge_id : best.edges) {
                if (!is_base[edge_id]) result.added_edges.push_back(edge_id);
            }
        }
        result.statistics = statistics;
        return result;
    }

    // N=頂点数、M=辺数、K=terminal 数、A=preset/restart 由来の試行数とする。
    // オプションは省略可能。ヒューリスティックな Steiner tree を求める
    // O(A * (K * (N + M log C) + M log M + K^2))、C は最大探索距離
    heuristic_steiner_tree_result solve(const std::vector<int> &input_terminals,
                                        const heuristic_steiner_tree_options &options = {}) const {
        const auto sequential_shortest_path = [&](
            const std::vector<int> &terminals,
            const path_store *store,
            int root_index,
            int rcl,
            std::mt19937_64 &random,
            solve_workspace &workspace,
            heuristic_steiner_tree_statistics &statistics) -> tree_solution {
            const auto sequential_on_demand = [&](
                const std::vector<int> &local_terminals,
                int local_root_index,
                int local_rcl,
                std::mt19937_64 &local_random,
                solve_workspace &caller_workspace,
                heuristic_steiner_tree_statistics &caller_statistics) -> tree_solution {
                const auto incremental_dijkstra = [&](
                    const std::vector<int> &new_sources,
                    std::vector<long long> &distance,
                    std::vector<int> &parent_edge,
                    solve_workspace &local_workspace,
                    heuristic_steiner_tree_statistics &local_statistics) {
                    local_workspace.heap.clear();
                    for (int source : new_sources) {
                        if (distance[source] == 0) continue;
                        distance[source] = 0;
                        parent_edge[source] = -1;
                        local_workspace.heap.push(0, source);
                    }

                    // 新しく木へ加わった頂点だけを距離 0 として、既存距離の改善分を伝播する
                    ++local_statistics.dijkstra_count;
                    while (!local_workspace.heap.empty()) {
                        const auto [unsigned_distance, vertex] = local_workspace.heap.pop();
                        const long long current_distance = static_cast<long long>(unsigned_distance);
                        if (current_distance != distance[vertex]) continue;
                        for (int arc_index = local_workspace.start[vertex]; arc_index < local_workspace.start[vertex + 1]; ++arc_index) {
                            ++local_statistics.edge_relaxation_count;
                            const auto &arc = local_workspace.arcs[arc_index];
                            if (current_distance > INF_VALUE - arc.cost) continue;
                            const long long next_distance = current_distance + arc.cost;
                            if (next_distance >= distance[arc.to]) continue;
                            distance[arc.to] = next_distance;
                            parent_edge[arc.to] = arc.edge_id;
                            local_workspace.heap.push(static_cast<unsigned long long>(next_distance), arc.to);
                        }
                    }

                };

                const int terminal_count = static_cast<int>(local_terminals.size());
                std::vector<char> connected(terminal_count, false);
                std::vector<char> in_tree(n_, false);
                std::vector<long long> distance(n_, INF_VALUE);
                std::vector<int> parent_edge(n_, -1);
                connected[local_root_index] = true;
                in_tree[local_terminals[local_root_index]] = true;
                incremental_dijkstra(std::vector<int>{local_terminals[local_root_index]}, distance, parent_edge, caller_workspace, caller_statistics);
                std::vector<int> candidate;

                // 木へ追加した頂点だけを新しい距離 0 source とし、最短距離を増分更新する
                for (int added = 1; added < terminal_count; ++added) {
                    std::vector<std::pair<long long, int>> top;
                    top.reserve(local_rcl);
                    for (int terminal_index = 0; terminal_index < terminal_count; ++terminal_index) {
                        if (connected[terminal_index]) continue;
                        const std::pair<long long, int> current{
                            distance[local_terminals[terminal_index]], terminal_index};
                        if (current.first == INF_VALUE) continue;
                        const auto position = std::lower_bound(top.begin(), top.end(), current);
                        if (position == top.end() && static_cast<int>(top.size()) >= local_rcl) continue;
                        top.insert(position, current);
                        if (static_cast<int>(top.size()) > local_rcl) top.pop_back();
                    }
                    if (top.empty()) return {};
                    const int selected = static_cast<int>(local_random() % static_cast<std::uint64_t>(top.size()));
                    const int terminal_index = top[selected].second;
                    int vertex = local_terminals[terminal_index];
                    std::vector<int> new_sources;
                    for (int step = 0; step <= n_ && !in_tree[vertex]; ++step) {
                        const int edge_id = parent_edge[vertex];
                        if (edge_id < 0) return {};
                        candidate.push_back(edge_id);
                        const auto &edge = edges_[edge_id];
                        if (!in_tree[vertex]) {
                            in_tree[vertex] = true;
                            new_sources.push_back(vertex);
                        }
                        vertex = edge.from == vertex ? edge.to : edge.from;
                    }
                    if (!in_tree[vertex]) return {};
                    connected[terminal_index] = true;
                    incremental_dijkstra(new_sources, distance, parent_edge, caller_workspace, caller_statistics);
                }
                ++caller_statistics.initial_solution_count;
                return normalize_candidate(std::move(candidate), local_terminals);

            };

            const auto sequential_cached = [&](
                const std::vector<int> &local_terminals,
                const path_store &local_store,
                int local_root_index,
                int local_rcl,
                std::mt19937_64 &local_random,
                heuristic_steiner_tree_statistics &local_statistics) -> tree_solution {
                return normalize_candidate(
                    sequential_cached_candidate(
                        local_terminals, local_store, local_root_index, local_rcl, local_random, local_statistics),
                    local_terminals);

            };

            if (store != nullptr) {
                return sequential_cached(terminals, *store, root_index, rcl, random, statistics);
            }
            return sequential_on_demand(terminals, root_index, rcl, random, workspace, statistics);

        };

        const std::vector<int> terminals = normalize_terminals(input_terminals);
        heuristic_steiner_tree_result result;
        if (terminals.size() <= 1) {
            result.ok = true;
            return result;
        }
        for (int terminal : terminals) {
            assert(0 <= terminal && terminal < n_);
            (void)terminal;
        }

        // 共通ワーク領域と探索状態を初期化する
        solve_workspace &workspace = reusable_workspace_;
        if (!workspace_ready_) {
            build_workspace(workspace);
            workspace_ready_ = true;
        }
        heuristic_steiner_tree_statistics statistics;
        const time_controller timer(options);
        std::mt19937_64 random(options.seed);
        const int terminal_count = static_cast<int>(terminals.size());
        const bool fast = options.preset == heuristic_steiner_tree_preset::fast;
        const bool quality = options.preset == heuristic_steiner_tree_preset::quality;
        const int elite_limit = fast ? 0 : (quality ? 8 : 4);
        std::vector<tree_solution> elite;
        std::vector<tree_solution> local_seed_pool;
        tree_solution best;
        auto consider = [&](tree_solution candidate) {
            if (!candidate.ok) return;
            if (elite_limit > 0) add_elite(elite, candidate, elite_limit);
            if (!best.ok || candidate.cost < best.cost) best = std::move(candidate);
        };

        // 最短路表の構築前に軽量な候補を作り、短い時間制限でも実行可能解を確保する
        consider(voronoi_mst(terminals, workspace, statistics));

        // terminal cache が有利と判断した場合だけ最短路表を構築する
        heuristic_steiner_tree_path_mode mode = choose_path_mode(options, terminal_count, timer);
        const size_t terminal_bytes = static_cast<size_t>(terminal_count) * n_ *
            (sizeof(long long) + sizeof(int));
        if (mode == heuristic_steiner_tree_path_mode::terminal_cache &&
            terminal_bytes > options.memory_limit_bytes) {
            mode = heuristic_steiner_tree_path_mode::on_demand;
        }

        path_store *store_pointer = nullptr;
        if (!timer.over() && mode == heuristic_steiner_tree_path_mode::terminal_cache) {
            store_pointer = cached_terminal_store(
                terminals, workspace, statistics);
            statistics.path_memory_bytes = terminal_bytes;
        } else {
            mode = heuristic_steiner_tree_path_mode::on_demand;
        }
        statistics.path_mode_used = mode;

        const int sequential_limit = store_pointer == nullptr
            ? ON_DEMAND_SEQUENTIAL_MAX_K : CACHED_SEQUENTIAL_MAX_K;
        if (terminal_count <= sequential_limit && !timer.over_fraction(3, 4)) {
            const int root_index = store_pointer == nullptr
                ? static_cast<int>(random() % terminals.size())
                : 0;
            consider(sequential_shortest_path(terminals, store_pointer, root_index, 1, random, workspace, statistics));
        }
        if (quality) remember_elite_seeds(elite, local_seed_pool);

        int restart_limit = options.restart_limit;
        if (restart_limit < 0) {
            if (fast) restart_limit = 0;
            else if (options.time_limit_ms > 0 ||
                     options.deadline != std::chrono::steady_clock::time_point::max()) {
                restart_limit = std::numeric_limits<int>::max();
            }
            else restart_limit = quality ? 20 : 4;
        }

        // 予算の 3/4 まで多様な初期解を生成し、残りを局所改善用に確保する
        for (int restart = 0;
             restart < restart_limit && !timer.over_fraction(3, 4); ++restart) {
            if (terminal_count > sequential_limit) break;
            ++statistics.restart_count;
            const int root_index = static_cast<int>(random() % terminals.size());
            const int rcl = 1 + restart % 4;
            consider(sequential_shortest_path(terminals, store_pointer, root_index, rcl, random, workspace, statistics));
            if (timer.over_fraction(3, 4)) break;
            if (quality && (restart + 1) % 4 == 0) {
                remember_elite_seeds(elite, local_seed_pool);
            }
        }

        // balanced / quality では elite 解を組み合わせて良い部分構造を集める
        if (!fast && elite.size() >= 2 && !timer.over_fraction(3, 4)) {
            const size_t original_size = elite.size();
            for (size_t left = 0; left < original_size && !timer.over_fraction(3, 4); ++left) {
                for (size_t right = left + 1;
                     right < original_size && !timer.over_fraction(3, 4); ++right) {
                    std::vector<int> candidate = elite[left].edges;
                    candidate.insert(candidate.end(), elite[right].edges.begin(), elite[right].edges.end());
                    consider(normalize_candidate(std::move(candidate), terminals));
                }
            }
        }

        std::vector<int> terminal_index;
        if (!fast) {
            terminal_index.assign(n_, -1);
            for (int index = 0; index < terminal_count; ++index) {
                terminal_index[terminals[index]] = index;
            }
        }

        // 安価な改善から順に適用し、quality では複数の elite 解を別々に局所最適化する
        auto improve_candidate = [&](tree_solution candidate) {
            if (!candidate.ok || timer.over()) return candidate;
            induced_mst_improve(candidate, terminals, workspace, statistics);
            if (fast) return candidate;

            const int branch_rounds = quality ? 8 : 2;
            const int branch_attempts = quality ? 24 : 8;
            for (int round = 0; round < branch_rounds && !timer.over(); ++round) {
                if (!terminal_branch_reconnect(candidate, terminals, terminal_index, store_pointer,
                                               branch_attempts, timer, workspace, statistics)) break;
                induced_mst_improve(candidate, terminals, workspace, statistics);
            }

            const int key_rounds = quality ? 4 : 1;
            const int key_attempts = quality ? 16 : 6;
            for (int round = 0; round < key_rounds && !timer.over(); ++round) {
                if (!key_path_exchange(candidate, terminals, key_attempts,
                                       timer, workspace, statistics)) break;
                induced_mst_improve(candidate, terminals, workspace, statistics);
            }
            return candidate;
        };

        if (best.ok && !timer.over()) {
            std::vector<tree_solution> local_candidates{best};
            const auto append_unique = [&](const tree_solution &candidate) {
                for (const auto &stored : local_candidates) {
                    if (same_solution(stored, candidate)) return;
                }
                local_candidates.push_back(candidate);
            };
            if (quality) {
                for (const auto &candidate : local_seed_pool) append_unique(candidate);
                const int local_elite_limit = std::min(4, static_cast<int>(elite.size()));
                for (int index = 0; index < local_elite_limit; ++index) append_unique(elite[index]);
            } else if (!fast) {
                const int local_elite_limit = std::min(4, static_cast<int>(elite.size()));
                for (int index = 1; index < local_elite_limit; ++index) append_unique(elite[index]);
            }
            for (auto &candidate : local_candidates) {
                if (timer.over()) break;
                auto improved = improve_candidate(std::move(candidate));
                if (improved.ok && improved.cost < best.cost) best = std::move(improved);
            }
        }

        if (options.preset != heuristic_steiner_tree_preset::fast)
            final_refinement(best, terminals, {}, {}, quality, timer, workspace, statistics);
        result.ok = best.ok;
        result.cost = best.cost;
        result.edges = std::move(best.edges);
        result.statistics = statistics;
        return result;
    }

    // 既存の辺集合を正規化し、preset に応じた局所探索で改善する
    // O(K * (N + M log C) + A * (N + M log C + M log M))、C は最大探索距離
    heuristic_steiner_tree_result improve(const std::vector<int> &input_terminals,
                                          const std::vector<int> &initial_edge_ids,
                                          const heuristic_steiner_tree_options &options = {}) const {
        const std::vector<int> terminals = normalize_terminals(input_terminals);
        heuristic_steiner_tree_result result;
        for (int terminal : terminals) {
            assert(0 <= terminal && terminal < n_);
            (void)terminal;
        }
        for (int edge_id : initial_edge_ids) {
            assert(0 <= edge_id && edge_id < static_cast<int>(edges_.size()));
            (void)edge_id;
        }
        if (terminals.size() <= 1) {
            result.ok = true;
            return result;
        }

        // 固定グラフ用 workspace を再利用し、入力部分グラフを最小全域木化・葉刈りする
        solve_workspace &workspace = reusable_workspace_;
        heuristic_steiner_tree_statistics statistics;
        time_controller timer(options);
        tree_solution best = normalize_candidate(initial_edge_ids, terminals);
        if (!best.ok) {
            result.cost = INF_VALUE;
            result.statistics = statistics;
            return result;
        }
        ensure_workspace(workspace, timer, options);
        induced_mst_improve(best, terminals, workspace, statistics);
        if (options.preset == heuristic_steiner_tree_preset::fast || timer.over()) {
            result.ok = true;
            result.cost = best.cost;
            result.edges = std::move(best.edges);
            result.statistics = statistics;
            return result;
        }

        // terminal cache が予算内なら枝再接続で利用し、そうでなければ打切り Dijkstra を使う
        const int terminal_count = static_cast<int>(terminals.size());
        heuristic_steiner_tree_path_mode mode = choose_path_mode(options, terminal_count, timer);
        const size_t terminal_bytes = static_cast<size_t>(terminal_count) * n_ *
            (sizeof(long long) + sizeof(int));
        if (mode == heuristic_steiner_tree_path_mode::terminal_cache &&
            terminal_bytes > options.memory_limit_bytes) {
            mode = heuristic_steiner_tree_path_mode::on_demand;
        }
        path_store *store_pointer = nullptr;
        if (mode == heuristic_steiner_tree_path_mode::terminal_cache && !timer.over()) {
            store_pointer = cached_terminal_store(
                terminals, workspace, statistics);
            statistics.path_memory_bytes = terminal_bytes;
        } else {
            mode = heuristic_steiner_tree_path_mode::on_demand;
        }
        statistics.path_mode_used = mode;

        std::vector<int> terminal_index(n_, -1);
        for (int index = 0; index < terminal_count; ++index) {
            terminal_index[terminals[index]] = index;
        }
        const bool quality = options.preset == heuristic_steiner_tree_preset::quality;
        const int branch_rounds = quality ? 8 : 2;
        const int branch_attempts = quality ? 24 : 8;
        for (int round = 0; round < branch_rounds && !timer.over(); ++round) {
            if (!terminal_branch_reconnect(best, terminals, terminal_index, store_pointer,
                                           branch_attempts, timer, workspace, statistics)) break;
            induced_mst_improve(best, terminals, workspace, statistics);
        }
        const int key_rounds = quality ? 4 : 1;
        const int key_attempts = quality ? 16 : 6;
        for (int round = 0; round < key_rounds && !timer.over(); ++round) {
            if (!key_path_exchange(best, terminals, key_attempts,
                                   timer, workspace, statistics)) break;
            induced_mst_improve(best, terminals, workspace, statistics);
        }

        final_refinement(best, terminals, {}, {}, quality, timer, workspace, statistics);
        result.ok = true;
        result.cost = best.cost;
        result.edges = std::move(best.edges);
        result.statistics = statistics;
        return result;
    }

    // ヒューリスティック解のコストだけを返す
    // O(A * (K * (N + M log C) + M log M + K^2))、C は最大探索距離
    long long solve_cost(const std::vector<int> &terminals,
                         const heuristic_steiner_tree_options &options = {}) const {
        const auto result = solve(terminals, options);
        return result.ok ? result.cost : inf();
    }

    // 到達不能を表す十分大きな値を返す O(1)
    static constexpr long long inf() {
        return INF_VALUE;
    }

    // 自動 terminal cache モードで使用する terminal 数上限を返す O(1)
    static constexpr int terminal_cache_auto_max_k() {
        return TERMINAL_CACHE_AUTO_MAX_K;
    }

    // 保持している通常グラフ用 terminal cache を解放する O(1)、解放領域は O(KN)
    void clear_terminal_cache() const {
        reusable_terminal_store_ = path_store{};
        std::vector<int>().swap(cached_terminals_);
        terminal_store_ready_ = false;
    }

    // 追加済み辺数を返す O(1)
    int edge_count() const {
        return static_cast<int>(edges_.size());
    }

    // 指定辺番号の辺情報を返す O(1)
    const edge_type &edge(int edge_id) const {
        return edges_[edge_id];
    }
};

#if __INCLUDE_LEVEL__ == 0

int main() {
    struct brute_dsu {
        std::vector<int> parent;
        explicit brute_dsu(int n) : parent(n, -1) {}
        int leader(int v) {
            if (parent[v] < 0) return v;
            return parent[v] = leader(parent[v]);
        }
        void merge(int a, int b) {
            a = leader(a);
            b = leader(b);
            if (a == b) return;
            if (parent[a] > parent[b]) std::swap(a, b);
            parent[a] += parent[b];
            parent[b] = a;
        }
    };

    const auto brute_force = [&](int n, const std::vector<heuristic_steiner_tree::edge_type> &edges,
                          const std::vector<int> &terminals) -> long long {
        long long best = heuristic_steiner_tree::inf();
        const int edge_count = static_cast<int>(edges.size());
        for (int mask = 0; mask < (1 << edge_count); ++mask) {
            brute_dsu union_find(n);
            long long cost = 0;
            for (int edge_id = 0; edge_id < edge_count; ++edge_id) {
                if ((mask & (1 << edge_id)) == 0) continue;
                union_find.merge(edges[edge_id].from, edges[edge_id].to);
                cost += edges[edge_id].cost;
            }
            bool connected = true;
            for (int terminal : terminals) {
                if (union_find.leader(terminal) != union_find.leader(terminals[0])) connected = false;
            }
            if (connected) best = std::min(best, cost);
        }
        return best;
    };

    const auto verify_result = [&](const heuristic_steiner_tree &solver, const std::vector<int> &terminals,
                       const heuristic_steiner_tree_result &result) -> bool {
        if (!result.ok) return false;
        brute_dsu union_find(1000);
        long long cost = 0;
        for (int edge_id : result.edges) {
            const auto &edge = solver.edge(edge_id);
            union_find.merge(edge.from, edge.to);
            cost += edge.cost;
        }
        if (cost != result.cost) return false;
        for (int terminal : terminals) {
            if (union_find.leader(terminal) != union_find.leader(terminals[0])) return false;
        }
        return true;
    };

    const auto verify_augmentation = [&](
        const heuristic_steiner_tree &solver, const std::vector<int> &terminals,
        const std::vector<int> &base_edges,
        const heuristic_steiner_tree_augmentation_result &result) -> bool {
        if (!result.ok) return false;
        brute_dsu union_find(1000);
        std::vector<char> used(solver.edge_count(), false);
        for (int edge_id : base_edges) {
            if (edge_id < 0 || edge_id >= solver.edge_count()) return false;
            const auto &edge = solver.edge(edge_id);
            union_find.merge(edge.from, edge.to);
            used[edge_id] = true;
        }
        long long added_cost = 0;
        for (int edge_id : result.added_edges) {
            if (edge_id < 0 || edge_id >= solver.edge_count() || used[edge_id]) return false;
            used[edge_id] = true;
            const auto &edge = solver.edge(edge_id);
            union_find.merge(edge.from, edge.to);
            added_cost += edge.cost;
        }
        if (added_cost != result.added_cost) return false;
        for (int terminal : terminals) {
            if (union_find.leader(terminal) != union_find.leader(terminals[0])) return false;
        }
        return true;
    };

    {
        // solve を先行させない improve の初回 on-demand 呼び出しも動作する
        heuristic_steiner_tree solver(5);
        solver.add_edge(0, 1, 5);
        solver.add_edge(1, 2, 5);
        solver.add_edge(1, 3, 5);
        solver.add_edge(3, 4, 5);
        solver.add_edge(0, 4, 1);
        heuristic_steiner_tree_options options;
        options.path_mode = heuristic_steiner_tree_path_mode::on_demand;
        const auto result = solver.improve({0, 2, 4}, {0, 1, 2, 3}, options);
        assert(result.ok && verify_result(solver, {0, 2, 4}, result));
        assert(result.cost <= 20);
    }

    {
        heuristic_steiner_tree solver(6);
        solver.add_edge(0, 1, 1);
        solver.add_edge(1, 2, 1);
        solver.add_edge(2, 3, 1);
        solver.add_edge(3, 4, 1);
        solver.add_edge(4, 5, 1);
        solver.add_edge(0, 5, 10);
        const auto result = solver.solve({0, 3, 5});
        assert(result.ok && result.cost == 5);
        assert(verify_result(solver, {0, 3, 5}, result));
        const auto reused = solver.solve({1, 4});
        assert(reused.ok && reused.cost == 3);
        assert(verify_result(solver, {1, 4}, reused));

        heuristic_steiner_tree_options options;
        options.preset = heuristic_steiner_tree_preset::quality;
        options.path_mode = heuristic_steiner_tree_path_mode::on_demand;
        const auto improved = solver.improve({0, 3, 5}, {0, 1, 2, 5}, options);
        assert(improved.ok && improved.cost == 5);
        assert(verify_result(solver, {0, 3, 5}, improved));
        assert(!solver.improve({0, 3, 5}, {0, 1}).ok);
    }
    {
        heuristic_steiner_tree solver(4);
        solver.add_edge(0, 1, 1);
        solver.add_edge(2, 3, 1);
        const auto result = solver.solve({0, 3});
        assert(!result.ok);
    }
    {
        heuristic_steiner_tree solver(3);
        assert(solver.solve({}).ok);
        assert(solver.solve({1, 1}).cost == 0);
    }
    {
        heuristic_steiner_tree solver(4);
        solver.add_edge(0, 0, 0);
        solver.add_edge(0, 1, 10);
        solver.add_edge(0, 1, 0);
        solver.add_edge(1, 2, 0);
        solver.add_edge(2, 3, 1);
        const auto result = solver.solve({0, 2, 3});
        assert(result.ok && result.cost == 1);
        assert(verify_result(solver, {0, 2, 3}, result));
        assert(solver.solve_cost({0, 2, 3}) == 1);
    }
    {
        heuristic_steiner_tree solver(3);
        solver.add_edge(0, 1, 10);
        solver.add_edge(1, 2, 10);
        assert(solver.solve({0, 2}).cost == 20);
        solver.add_edge(0, 2, 1);
        const auto result = solver.solve({0, 2});
        assert(result.ok && result.cost == 1);
        assert(verify_result(solver, {0, 2}, result));
    }
    {
        // radix sort の上位 byte と、大きな非負コストの加算を確認する
        const long long high = 1LL << 55;
        heuristic_steiner_tree solver(4);
        solver.add_edge(0, 1, high + 5);
        solver.add_edge(1, 2, high + 7);
        solver.add_edge(2, 3, high + 9);
        solver.add_edge(0, 3, 3 * high + 30);
        const auto result = solver.solve({0, 2, 3});
        assert(result.ok && result.cost == 3 * high + 21);
        assert(verify_result(solver, {0, 2, 3}, result));
    }
    {
        // 全辺で共通する下位 byte を省略しても (cost, edge_id) 順が保たれることを確認する
        const long long unit = 1LL << 32;
        heuristic_steiner_tree solver(5);
        solver.add_edge(0, 1, 3 * unit);
        solver.add_edge(1, 4, 3 * unit);
        solver.add_edge(0, 2, unit);
        solver.add_edge(2, 3, unit);
        solver.add_edge(3, 4, unit);
        solver.add_edge(1, 2, 10 * unit);
        const auto result = solver.solve({0, 4});
        assert(result.ok && result.cost == 3 * unit);
        assert(verify_result(solver, {0, 4}, result));
    }
    {
        heuristic_steiner_tree solver(5);
        const int e01 = solver.add_edge(0, 1, 1);
        const int e12 = solver.add_edge(1, 2, 1);
        const int e23 = solver.add_edge(2, 3, 1);
        const int e34 = solver.add_edge(3, 4, 1);
        const int e14 = solver.add_edge(1, 4, 10);

        const auto normalized = solver.normalize({0, 4}, {e01, e12, e23, e34, e14});
        assert(normalized.ok && normalized.cost == 4);
        assert(verify_result(solver, {0, 4}, normalized));
        assert(!solver.normalize({0, 4}, {e01, e34}).ok);

        heuristic_steiner_tree_options options;
        options.preset = heuristic_steiner_tree_preset::balanced;
        options.restart_limit = 4;
        const auto repaired = solver.repair({0, 4}, {e01, e34}, options);
        assert(repaired.ok && repaired.cost == 4);
        assert(verify_result(solver, {0, 4}, repaired));
        const auto repaired_biased = solver.repair({0, 4}, {e14}, options);
        assert(repaired_biased.ok && repaired_biased.cost == 4);
        assert(verify_result(solver, {0, 4}, repaired_biased));

        const auto augmented = solver.augment({0, 4}, {e01, e34}, options);
        assert(augmented.ok && augmented.added_cost == 2);
        assert(verify_augmentation(solver, {0, 4}, {e01, e34}, augmented));
        const auto already_connected = solver.augment({0, 4}, {e01, e12, e23, e34}, options);
        assert(already_connected.ok && already_connected.added_cost == 0 &&
               already_connected.added_edges.empty());
        assert(verify_augmentation(
            solver, {0, 4}, {e01, e12, e23, e34}, already_connected));
    }
    {
        heuristic_steiner_tree solver(4);
        const int e01 = solver.add_edge(0, 1, 100);
        const int e23 = solver.add_edge(2, 3, 100);
        const int e12 = solver.add_edge(1, 2, 7);
        const int e03 = solver.add_edge(0, 3, 20);
        heuristic_steiner_tree_options options;
        options.preset = heuristic_steiner_tree_preset::fast;
        const auto augmented = solver.augment({0, 3}, {e01, e23}, options);
        assert(augmented.ok && augmented.added_cost == 7);
        assert(augmented.added_edges == std::vector<int>{e12});
        assert(verify_augmentation(solver, {0, 3}, {e01, e23}, augmented));
        (void)e03;
    }
    {
        heuristic_steiner_tree solver(3);
        const int e01 = solver.add_edge(0, 1, 1);
        const int e12 = solver.add_edge(1, 2, 1);
        heuristic_steiner_tree_options options;
        options.deadline = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
        const auto result = solver.solve({0, 2}, options);
        assert(result.ok && result.cost == 2);
        const auto repaired = solver.repair({0, 2}, {e01}, options);
        assert(repaired.ok && repaired.cost == 2);
        assert(verify_result(solver, {0, 2}, repaired));
        const auto augmented = solver.augment({0, 2}, {e01}, options);
        assert(augmented.ok && augmented.added_cost == 1 &&
               augmented.added_edges == std::vector<int>{e12});
        assert(verify_augmentation(solver, {0, 2}, {e01}, augmented));
    }
    {
        // 同じグラフの terminal cache を再利用し、terminal 交換時は共通行だけ引き継ぐ
        const std::vector<heuristic_steiner_tree::edge_type> edges{
            {0, 1, 4}, {1, 2, 3}, {2, 3, 2}, {3, 4, 5},
            {4, 5, 1}, {5, 6, 6}, {6, 7, 2}, {0, 4, 8},
            {2, 6, 7}, {1, 7, 12}};
        heuristic_steiner_tree solver(8);
        heuristic_steiner_tree fresh_solver(8);
        for (const auto &edge : edges) {
            solver.add_edge(edge.from, edge.to, edge.cost);
            fresh_solver.add_edge(edge.from, edge.to, edge.cost);
        }
        heuristic_steiner_tree_options options;
        options.preset = heuristic_steiner_tree_preset::quality;
        options.path_mode = heuristic_steiner_tree_path_mode::terminal_cache;
        options.restart_limit = 4;
        options.seed = 987654321;
        const auto first = solver.solve({0, 2, 4, 6}, options);
        const auto changed = solver.solve({0, 2, 4, 7}, options);
        const auto fresh = fresh_solver.solve({0, 2, 4, 7}, options);
        const auto repeated = solver.solve({0, 2, 4, 7}, options);
        assert(first.ok && changed.ok && fresh.ok && repeated.ok);
        assert(changed.cost == fresh.cost && changed.edges == fresh.edges);
        assert(repeated.cost == changed.cost && repeated.edges == changed.edges);
        assert(changed.statistics.dijkstra_count < fresh.statistics.dijkstra_count);
        assert(repeated.statistics.dijkstra_count < changed.statistics.dijkstra_count);

        heuristic_steiner_tree limited_solver(8);
        for (const auto &edge : edges) {
            limited_solver.add_edge(edge.from, edge.to, edge.cost);
        }
        options.memory_limit_bytes = 4ULL * 8 *
            (sizeof(long long) + sizeof(int));
        assert(limited_solver.solve({0, 2, 4, 6}, options).ok);
        const auto limited = limited_solver.solve({0, 2, 4, 7}, options);
        assert(limited.cost == fresh.cost && limited.edges == fresh.edges);
        assert(limited.statistics.dijkstra_count < fresh.statistics.dijkstra_count);

        solver.clear_terminal_cache();
        const auto cleared = solver.solve({0, 2, 4, 7}, options);
        assert(cleared.cost == fresh.cost && cleared.edges == fresh.edges);
        assert(cleared.statistics.dijkstra_count == fresh.statistics.dijkstra_count);
    }

    std::mt19937_64 random(123456789);
    for (int test = 0; test < 200; ++test) {
        const int n = 2 + static_cast<int>(random() % 6);
        const int edge_count = n - 1 + static_cast<int>(random() % 5);
        heuristic_steiner_tree solver(n);
        std::vector<heuristic_steiner_tree::edge_type> edges;
        for (int vertex = 1; vertex < n; ++vertex) {
            const int parent = static_cast<int>(random() % static_cast<std::uint64_t>(vertex));
            const long long cost = 1 + static_cast<long long>(random() % 20);
            solver.add_edge(parent, vertex, cost);
            edges.push_back({parent, vertex, cost});
        }
        while (static_cast<int>(edges.size()) < edge_count) {
            const int u = static_cast<int>(random() % static_cast<std::uint64_t>(n));
            const int v = static_cast<int>(random() % static_cast<std::uint64_t>(n));
            if (u == v) continue;
            const long long cost = 1 + static_cast<long long>(random() % 20);
            solver.add_edge(u, v, cost);
            edges.push_back({u, v, cost});
        }
        std::vector<int> terminals;
        for (int vertex = 0; vertex < n; ++vertex) {
            if ((random() & 1ULL) != 0) terminals.push_back(vertex);
        }
        if (terminals.empty()) terminals.push_back(0);
        if (terminals.size() == 1 && n >= 2) terminals.push_back(1);
        const long long optimum = brute_force(n, edges, terminals);
        for (heuristic_steiner_tree_path_mode mode : {
                 heuristic_steiner_tree_path_mode::terminal_cache,
                 heuristic_steiner_tree_path_mode::on_demand}) {
            heuristic_steiner_tree_options options;
            options.preset = heuristic_steiner_tree_preset::quality;
            options.path_mode = mode;
            options.seed = static_cast<std::uint64_t>(test);
            options.restart_limit = 4;
            const auto result = solver.solve(terminals, options);
            assert(verify_result(solver, terminals, result));
            assert(result.cost >= optimum);
        }
    }

    std::cout << "OK\n";
}
#endif
