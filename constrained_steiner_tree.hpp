#pragma once
#include <bits/stdc++.h>

/*
非負重み無向グラフ上で、頂点選択の排他制約を満たす Steiner tree をヒューリスティックに求めるライブラリ。
terminal と中継頂点を含む「最終的に使用する頂点」を選択とみなし、排他ペア・高々1頂点の排他グループ、
呼び出しごとの使用禁止頂点・辺、外側で既に選択済みの頂点を扱う。
通常の Steiner tree 構築に加え、候補辺の正規化、既存解の改善、非連結・制約違反部分解の修復、
撤去不能な既設辺への追加接続を提供する。制約付き探索は衝突頂点を片側ずつ禁止する制限付き分岐を使う。
improve は合法な連結初期解を保持し、探索上限や期限に達しても既知の解を返す。
厳密解・実行可能解の発見は保証しない。制約なし呼び出しは内部の通常 Steiner engine へ直接委譲する。
頂点番号・返却辺番号は add_edge 順の 0-indexed。
計算量表記では N=頂点数、M=辺数、K=terminal数、L=候補辺数、A=内部局所探索回数、
B=制約分岐で評価するノード数、C=最大距離を表す。
status::proven_infeasible は固定頂点同士の衝突など証明できた場合だけ返し、
ヒューリスティック探索で合法解を発見できなかった場合は status::not_found を返す。
*/

class constrained_steiner_tree {
public:
    enum class preset { fast, balanced, quality };
    enum class path_mode { auto_select, terminal_cache, on_demand };
    enum class status { feasible, proven_infeasible, not_found, invalid_candidate, invalid_input };

    struct options {
        preset preset_mode = preset::balanced;
        path_mode path_mode_value = path_mode::auto_select;
        std::uint64_t seed = 0;
        int time_limit_ms = 0;
        int restart_limit = -1;
        size_t memory_limit_bytes = 256ULL << 20;
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max();
        int constraint_node_limit = -1;
        int constraint_depth_limit = -1;
    };

private:
    struct engine_statistics {
        path_mode path_mode_used = path_mode::on_demand;
        size_t path_memory_bytes = 0;
        int initial_solution_count = 0;
        int restart_count = 0;
        int local_move_count = 0;
        int improvement_count = 0;
        long long dijkstra_count = 0;
        long long edge_relaxation_count = 0;
    };
public:
    struct statistics : engine_statistics {
        int constraint_nodes = 0;
        int conflict_candidates = 0;
        int feasible_candidates = 0;
    };

    struct result {
        status state = status::not_found;
        long long cost = 0;
        std::vector<int> edges;
        statistics stats;
        // 実行可能解を保持しているかを返す O(1)
        bool ok() const { return state == status::feasible; }
    };

    struct augmentation_result {
        status state = status::not_found;
        long long added_cost = 0;
        std::vector<int> added_edges;
        statistics stats;
        // 実行可能解を保持しているかを返す O(1)
        bool ok() const { return state == status::feasible; }
    };

    struct edge_type { int from; int to; long long cost; };

    class selection_rules {
        friend class constrained_steiner_tree;
        int vertex_count_ = 0;
        std::vector<std::vector<int>> groups_;
        std::unordered_map<int, std::vector<int>> memberships_;

        void add_group(std::vector<int> group) {
            for (int vertex : group) memberships_[vertex].push_back(static_cast<int>(groups_.size()));
            groups_.push_back(std::move(group));
        }

        std::span<const int> groups_of(int vertex) const {
            auto found = memberships_.find(vertex);
            return found == memberships_.end() ? std::span<const int>{} : found->second;
        }

    public:
        // 指定頂点数の空の排他制約集合を構築する O(1)
        explicit selection_rules(int vertex_count = 0) : vertex_count_(vertex_count) { assert(vertex_count >= 0); }

        // 2頂点を同時使用不可にする 平均償却 O(1)
        void add_conflict(int u, int v) {
            assert(0 <= u && u < vertex_count_ && 0 <= v && v < vertex_count_);
            assert(u != v);
            if (u == v) return;
            add_group({u, v});
        }

        // グループ内から高々1頂点だけ使用可能にする O(S log S)、Sは入力頂点数
        void add_exclusive_group(std::span<const int> vertices) {
            std::vector<int> group(vertices.begin(), vertices.end());
            for (int vertex : group) { assert(0 <= vertex && vertex < vertex_count_); (void)vertex; }
            group = normalize_ids(std::move(group));
            if (group.size() < 2) return;
            add_group(std::move(group));
        }

        // 全排他制約を削除する O(G+V)、Gは登録グループ数、Vは登録頂点数
        void clear() {
            groups_.clear(); memberships_.clear();
        }

        // 登録グループ数を返す O(1)
        int group_count() const { return static_cast<int>(groups_.size()); }

        // 制約対象の頂点数を返す O(1)
        int vertex_count() const { return vertex_count_; }
    };

    struct query_context {
        const selection_rules *rules = nullptr;
        std::span<const int> forbidden_vertices{};
        std::span<const int> forbidden_edges{};
        std::span<const int> selected_outside{};
    };

private:


    struct tree_solution {
        bool ok = false;
        long long cost = 0;
        std::vector<int> edges;
    };
    struct engine_result : tree_solution { engine_statistics statistics; };


    template <bool Filtered>
    class base_engine {
    public:
        using edge_type = constrained_steiner_tree::edge_type;

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

            static int bucket_index(key_type key, key_type last) {
                const key_type diff = key ^ last;
                if (diff == 0) return 0;
                return 64 - __builtin_clzll(diff);
            }

        public:
            // 保持しているキーを全て取り除く O(1)、バケット数は 65
            void clear() {
                for (auto &bucket : buckets_) bucket.clear();
                last_ = 0;
                size_ = 0;
            }

            // キューが空かを返す O(1)
            bool empty() const {
                return size_ == 0;
            }

            // 単調な非負距離の候補を追加する 償却 O(1)
            void push(key_type key, value_type value) {
                buckets_[bucket_index(key, last_)].push_back({key, value});
                ++size_;
            }

            // 最小キーと頂点を取り出す 償却 O(log C)
            std::pair<key_type, value_type> pop() {
                // 最初の非空バケットの最小値を新しい基準とし、候補を振り分け直す
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

                // 基準値に等しい候補を 1 個取り出す
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
            // n 個の独立な成分を構築する O(n)
            explicit dsu(int n)
                : parent_(n, -1), has_terminal_(n, false) {}

            // 併合前の頂点を terminal として登録する O(1)
            void set_terminal(int vertex) {
                has_terminal_[vertex] = true;
                ++terminal_components_;
            }

            // 2 成分を併合し、terminal を含む成分数を更新する 償却 O(alpha(N))
            bool merge(int a, int b) {
                // 根を求め、辿った経路を根へ直接つなぎ直す
                const auto leader = [&](int vertex) -> int {
                    int root = vertex;
                    while (parent_[root] >= 0) root = parent_[root];
                    while (vertex != root) vertex = std::exchange(parent_[vertex], root);
                    return root;

                };

                // 小さい成分を大きい成分へ併合する
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

            // 全 terminal が同じ成分に含まれるかを返す O(1)
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

            // 相対予算と絶対 deadline の早い方を停止目標にする O(1)
            explicit time_controller(const options &options)
                : begin(std::chrono::steady_clock::now()), stop(options.deadline) {
                if (options.time_limit_ms > 0) {
                    const auto relative_stop = begin + std::chrono::milliseconds(options.time_limit_ms);
                    if (relative_stop < stop) stop = relative_stop;
                }
            }

            // 停止目標に到達したかを返す O(1)
            bool over() const {
                return stop != std::chrono::steady_clock::time_point::max() &&
                    std::chrono::steady_clock::now() >= stop;
            }

            // 指定割合の予算を使い切ったかを返す O(1)
            bool over_fraction(int numerator, int denominator) const {
                assert(0 <= numerator && numerator <= denominator && 0 < denominator);
                if (stop == std::chrono::steady_clock::time_point::max()) return false;
                const auto budget = stop > begin ? stop - begin : std::chrono::steady_clock::duration::zero();
                // 商と余りに分け、遠い deadline でも倍率の乗算をオーバーフローさせない
                const auto threshold = begin + budget / denominator * numerator +
                    budget % denominator * numerator / denominator;
                return std::chrono::steady_clock::now() >= threshold;
            }

        };

        struct tree_view {
            std::vector<int> head;
            std::vector<int> to;
            std::vector<int> next;
            std::vector<int> degree;
        };

        struct key_path {
            int endpoint_a = -1;
            int endpoint_b = -1;
            long long cost = 0;
            // key-pathは木内ID。terminal再接続ではグラフ辺IDへ変換して使用する。
            std::vector<int> edge_ids;
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
        const std::vector<unsigned char> *forbidden_vertex_ = nullptr;
        const std::vector<unsigned char> *forbidden_edge_ = nullptr;

        mutable std::vector<unsigned char> enabled_edge_;
        mutable bool edge_filter_ready_ = false;

        bool edge_enabled(int edge_id) const {
            if constexpr (!Filtered) return true;
            if (forbidden_vertex_ == nullptr) return true;
            if (edge_filter_ready_) return enabled_edge_[edge_id] != 0;
            if ((*forbidden_edge_)[edge_id]) return false;
            const auto &edge = edges_[edge_id];
            return !(*forbidden_vertex_)[edge.from] && !(*forbidden_vertex_)[edge.to];
        }

        void prepare_edge_filter() const {
            if constexpr (Filtered) {
                if (edge_filter_ready_) return;
                // 最短路探索を行う分岐だけ、反復参照する禁止判定を辺ごとにまとめる
                enabled_edge_.resize(edges_.size());
                for (int id = 0; id < static_cast<int>(edges_.size()); ++id)
                    enabled_edge_[id] = edge_enabled(id);
                edge_filter_ready_ = true;
            }
        }

        // 入力順を保ち、費用の異なるbyteだけを安定radix sortする。
        template <class T, class Cost>
        static void radix_sort_cost(std::vector<T> &values, unsigned long long varying_bits, Cost cost) {
            if (varying_bits == 0) return;
            std::vector<T> buffer(values.size());
            std::array<unsigned int, 256> count{};
            for (int shift = 0; shift < 64; shift += 8) {
                if (((varying_bits >> shift) & 255ULL) == 0) continue;
                const auto key = [&](const T &value) {
                    return static_cast<size_t>((static_cast<unsigned long long>(cost(value)) >> shift) & 255ULL);
                };
                count.fill(0);
                for (const auto &value : values) ++count[key(value)];
                unsigned int position = 0;
                for (auto &value : count) {
                    const unsigned int frequency = value;
                    value = position;
                    position += frequency;
                }
                for (const auto &value : values) buffer[count[key(value)]++] = value;
                values.swap(buffer);
            }
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
            radix_sort_cost(workspace.edge_order, varying_bits,
                [&](int id) { return edges_[id].cost; });
        }

        // 構築を必要時まで遅延し、構築時間を相対予算から除く
        void ensure_workspace(solve_workspace &workspace, time_controller &timer,
                              const options &options) const {
            if (workspace_ready_) return;
            const auto preparation_begin = std::chrono::steady_clock::now();
            build_workspace(workspace);
            workspace_ready_ = true;
            timer.begin += std::chrono::steady_clock::now() - preparation_begin;
            if (options.time_limit_ms > 0)
                timer.stop = std::min(options.deadline,
                    timer.begin + std::chrono::milliseconds(options.time_limit_ms));
        }

        template <bool Free = false>
        void dijkstra_full(const std::vector<int> &sources, bool need_owner,
                           solve_workspace &workspace, engine_statistics &statistics, const std::vector<char> *is_free = nullptr) const {
            prepare_edge_filter();
            workspace.dist.assign(n_, INF_VALUE);
            workspace.parent_edge.assign(n_, -1);
            if (need_owner) workspace.owner.assign(n_, -1);
            workspace.heap.clear();

            // source の添字を owner とする
            for (int source_index = 0; source_index < static_cast<int>(sources.size()); ++source_index) {
                const int source = sources[source_index];
                if (workspace.dist[source] == 0) continue;
                workspace.dist[source] = 0;
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
                    if constexpr (Filtered) { if (!enabled_edge_[arc.edge_id]) continue; }
                    const long long cost = Free && (*is_free)[arc.edge_id] ? 0 : arc.cost;
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

        template <bool Free = false>
        int dijkstra_to_target(const std::vector<int> &sources, const std::vector<char> &target,
                               long long distance_limit, solve_workspace &workspace,
                               engine_statistics &statistics, const std::vector<char> *is_free = nullptr) const {
            prepare_edge_filter();
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
                if (distance != workspace.dist[vertex]) continue;
                if (distance >= distance_limit) return -1;
                if (target[vertex]) return vertex;

                for (int arc_index = workspace.start[vertex]; arc_index < workspace.start[vertex + 1]; ++arc_index) {
                    ++statistics.edge_relaxation_count;
                    const auto &arc = workspace.arcs[arc_index];
                    if constexpr (Filtered) { if (!enabled_edge_[arc.edge_id]) continue; }
                    const long long cost = Free && (*is_free)[arc.edge_id] ? 0 : arc.cost;
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

        template <bool Free = false>
        void incremental_dijkstra(
            const std::vector<int> &new_sources,
            std::vector<long long> &distance,
            std::vector<int> &parent_edge,
            solve_workspace &local_workspace,
            engine_statistics &local_statistics, const std::vector<char> *is_free = nullptr) const {
            prepare_edge_filter();
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
                    if constexpr (Filtered) { if (!enabled_edge_[arc.edge_id]) continue; }
                    const long long cost = Free && (*is_free)[arc.edge_id] ? 0 : arc.cost;
                    if (current_distance > INF_VALUE - cost) continue;
                    const long long next_distance = current_distance + cost;
                    if (next_distance >= distance[arc.to]) continue;
                    distance[arc.to] = next_distance;
                    parent_edge[arc.to] = arc.edge_id;
                    local_workspace.heap.push(static_cast<unsigned long long>(next_distance), arc.to);
                }
            }

        }

        std::vector<int> restore_parent_path(int source, int target,
                                             std::span<const int> parent_edge) const {
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
                                               std::span<const int> parent_edge) const {
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
            return restore_parent_path(terminals[terminal_index], target,
                std::span<const int>(store.trace).subspan(static_cast<size_t>(terminal_index) * n_, n_));
        }

        // 1 terminalの距離・親辺を指定行へ保存する。
        template <bool Free = false>
        void store_terminal_row(int terminal, size_t row, path_store &store,
            solve_workspace &workspace, engine_statistics &statistics,
            const std::vector<char> *is_free = nullptr) const {
            dijkstra_full<Free>({terminal}, false, workspace, statistics, is_free);
            const auto offset = static_cast<std::ptrdiff_t>(row * n_);
            std::copy(workspace.dist.begin(), workspace.dist.end(), store.dist.begin() + offset);
            std::copy(workspace.parent_edge.begin(), workspace.parent_edge.end(), store.trace.begin() + offset);
        }

        template <bool Free = false>
        void build_terminal_store(const std::vector<int> &terminals, path_store &store,
            solve_workspace &workspace, engine_statistics &statistics,
            const std::vector<char> *is_free = nullptr) const {
            store.dist.resize(terminals.size() * n_);
            store.trace.resize(terminals.size() * n_);
            for (size_t index = 0; index < terminals.size(); ++index) {
                store_terminal_row<Free>(terminals[index], index, store, workspace, statistics, is_free);
            }
        }

        path_store *cached_terminal_store(
            const std::vector<int> &terminals, solve_workspace &workspace,
            engine_statistics &statistics) const {
            if (terminal_store_ready_ && cached_terminals_ == terminals) return &reusable_terminal_store_;

            std::vector<int> old_row(terminals.size(), -1);
            size_t shared_count = 0;
            for (size_t index = 0; terminal_store_ready_ && index < terminals.size(); ++index) {
                auto row = std::ranges::lower_bound(cached_terminals_, terminals[index]);
                if (row != cached_terminals_.end() && *row == terminals[index]) {
                    old_row[index] = static_cast<int>(row - cached_terminals_.begin());
                    ++shared_count;
                }
            }

            if (shared_count == 0) {
                build_terminal_store(terminals, reusable_terminal_store_, workspace, statistics);
                cached_terminals_ = terminals;
                terminal_store_ready_ = true;
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
                store_terminal_row(terminals[index], index, reusable_terminal_store_, workspace, statistics);
            }
            cached_terminals_ = terminals;
            return &reusable_terminal_store_;
        }

        path_mode choose_path_mode(
            const options &options, int terminal_count,
            const time_controller &timer) const {
            const auto budget_milliseconds = [&]() -> long long {
                if (timer.stop == std::chrono::steady_clock::time_point::max()) return 0;
                if (timer.stop <= timer.begin) return 1;
                const long long milliseconds = static_cast<long long>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(timer.stop - timer.begin).count());
                return std::max(1LL, milliseconds);

            };

            if (options.path_mode_value != path_mode::auto_select) return options.path_mode_value;
            if (options.preset_mode == preset::fast) {
                return path_mode::on_demand;
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
                return path_mode::terminal_cache;
            }
            return path_mode::on_demand;
        }

        // solve・repair・improveで同じcache選択とメモリ上限を用いる。
        template <bool Free = false>
        path_store *prepare_terminal_cache(const std::vector<int> &terminals,
            const options &options, const time_controller &timer,
            solve_workspace &workspace, engine_statistics &statistics,
            path_store *free_store = nullptr, const std::vector<char> *is_free = nullptr) const {
            const int terminal_count = static_cast<int>(terminals.size());
            path_mode mode = choose_path_mode(options, terminal_count, timer);
            const size_t terminal_bytes = static_cast<size_t>(terminal_count) * n_ *
                (sizeof(long long) + sizeof(int));
            if (mode == path_mode::terminal_cache &&
                terminal_bytes > options.memory_limit_bytes) {
                mode = path_mode::on_demand;
            }

            path_store *store_pointer = nullptr;
            if (!timer.over() && mode == path_mode::terminal_cache) {
                if constexpr (Free) {
                    build_terminal_store<true>(terminals, *free_store, workspace, statistics, is_free);
                    store_pointer = free_store;
                } else {
                    store_pointer = cached_terminal_store(terminals, workspace, statistics);
                }
                statistics.path_memory_bytes = terminal_bytes;
            } else {
                mode = path_mode::on_demand;
            }
            statistics.path_mode_used = mode;
            return store_pointer;
        }

        // 非terminalの葉を除き、無料辺を除いた費用をオーバーフローせず集計する
        template <bool Free = false>
        tree_solution prune_tree(std::vector<int> tree_edges,
                                 const std::vector<int> &terminals,
                                 const std::vector<char> *is_free = nullptr) const {
            tree_solution result;
            if (terminals.size() <= 1) {
                result.ok = true;
                return result;
            }

            // 木では次数1の頂点に接続する辺をXORだけで特定できる
            std::vector<int> incident_xor(n_, 0), degree(n_, 0);
            for (int id : tree_edges) {
                const auto &edge = edges_[id];
                incident_xor[edge.from] ^= id;
                incident_xor[edge.to] ^= id;
                ++degree[edge.from]; ++degree[edge.to];
            }
            // 非terminalの葉から順に接続辺を削除する
            std::vector<char> is_terminal(n_, false);
            for (int terminal : terminals) is_terminal[terminal] = true;
            std::vector<int> queue;
            queue.reserve(tree_edges.size());
            for (int vertex = 0; vertex < n_; ++vertex)
                if (!is_terminal[vertex] && degree[vertex] == 1) queue.push_back(vertex);
            for (size_t qi = 0; qi < queue.size(); ++qi) {
                const int vertex = queue[qi];
                if (degree[vertex] != 1) continue;
                const int id = incident_xor[vertex];
                const auto &edge = edges_[id];
                const int adjacent = edge.from == vertex ? edge.to : edge.from;
                degree[vertex] = 0;
                --degree[adjacent];
                incident_xor[adjacent] ^= id;
                if (!is_terminal[adjacent] && degree[adjacent] == 1) queue.push_back(adjacent);
            }

            result.ok = true;
            for (int edge_id : tree_edges) {
                const auto &edge = edges_[edge_id];
                if (degree[edge.from] == 0 || degree[edge.to] == 0) continue;
                result.edges.push_back(edge_id);
                if (!Free || !(*is_free)[edge_id]) {
                    if (result.cost > std::numeric_limits<long long>::max() - edge.cost) return {};
                    result.cost += edge.cost;
                }
            }
            std::sort(result.edges.begin(), result.edges.end());
            return result;
        }

        template <bool Free = false>
        tree_solution normalize_sorted_candidate(std::span<const int> candidate,
            const std::vector<int> &terminals, const std::vector<char> *is_free = nullptr) const {
            if (terminals.size() <= 1) return tree_solution{true, 0, {}};
            dsu union_find(n_);
            for (int terminal : terminals) union_find.set_terminal(terminal);
            std::vector<int> tree_edges;
            tree_edges.reserve(std::min(candidate.size(), static_cast<size_t>(n_ - 1)));
            for (int edge_id : candidate) {
                if (!edge_enabled(edge_id)) continue;
                const auto &edge = edges_[edge_id];
                if (!union_find.merge(edge.from, edge.to)) continue;
                tree_edges.push_back(edge_id);
                if (union_find.terminals_connected())
                    return prune_tree<Free>(std::move(tree_edges), terminals, is_free);
            }
            return {};
        }

        template <bool Free = false>
        tree_solution normalize_candidate(std::vector<int> candidate,
            const std::vector<int> &terminals, const std::vector<int> *free_edges = nullptr,
            const std::vector<char> *is_free = nullptr) const {
            if (terminals.size() <= 1) return tree_solution{true, 0, {}};
            if constexpr (Free) candidate.insert(candidate.end(), free_edges->begin(), free_edges->end());
            std::sort(candidate.begin(), candidate.end(), [&](int a, int b) {
                const long long left = Free && (*is_free)[a] ? 0 : edges_[a].cost;
                const long long right = Free && (*is_free)[b] ? 0 : edges_[b].cost;
                return left != right ? left < right : a < b;
            });
            candidate.erase(std::unique(candidate.begin(), candidate.end()), candidate.end());
            return normalize_sorted_candidate<Free>(candidate, terminals, is_free);
        }

        std::vector<char> make_edge_mark(const std::vector<int> &edge_ids) const {
            std::vector<char> marked(edges_.size(), false);
            for (int edge_id : edge_ids) marked[edge_id] = true;
            return marked;
        }

        tree_view make_tree_view(const tree_solution &solution) const {
            tree_view view;
            const int edge_count = static_cast<int>(solution.edges.size());
            view.head.assign(n_, -1);
            view.to.resize(2 * edge_count);
            view.next.resize(2 * edge_count);
            view.degree.assign(n_, 0);
            for (int local_id = 0; local_id < edge_count; ++local_id) {
                const int global_id = solution.edges[local_id];
                const auto &edge = edges_[global_id];
                for (int direction = 0; direction < 2; ++direction) {
                    const int from = direction ? edge.to : edge.from;
                    const int to = direction ? edge.from : edge.to;
                    const int arc = 2 * local_id + direction;
                    view.to[arc] = to;
                    view.next[arc] = view.head[from];
                    view.head[from] = arc;
                    ++view.degree[from];
                }
            }
            return view;
        }

        tree_solution voronoi_mst(const std::vector<int> &terminals, solve_workspace &workspace,
                                  engine_statistics &statistics) const {
            return normalize_candidate(seeded_voronoi_candidate<false>(terminals, {}, {}, workspace, statistics), terminals);
        }

        template <bool Free = true>
        std::vector<int> seeded_voronoi_candidate(
            const std::vector<int> &terminals, const std::vector<int> &seed_edges,
            const std::vector<char> &is_free, solve_workspace &workspace,
            engine_statistics &statistics) const {
            dijkstra_full<Free>(terminals, true, workspace, statistics, &is_free);
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
                if (!edge_enabled(edge_id)) continue;
                const auto &edge = edges_[edge_id];
                const long long edge_cost = Free && is_free[edge_id] ? 0 : edge.cost;
                const int owner_a = workspace.owner[edge.from];
                const int owner_b = workspace.owner[edge.to];
                if (owner_a < 0 || owner_b < 0 || owner_a == owner_b) continue;
                const long long left = workspace.dist[edge.from];
                const long long right = workspace.dist[edge.to];
                if (left > INF_VALUE - edge_cost || left + edge_cost > INF_VALUE - right) continue;
                const long long link_cost = left + edge_cost + right;
                if (links.empty()) reference_cost = static_cast<unsigned long long>(link_cost);
                else varying_bits |= reference_cost ^ static_cast<unsigned long long>(link_cost);
                links.push_back(terminal_link{link_cost, owner_a, owner_b, edge_id});
            }
            // cost の異なる byte だけを下位から安定ソートし、同コスト時の edge_id 順も保つ
            radix_sort_cost(links, varying_bits, [](const auto &link) { return link.cost; });

            // terminal 間候補グラフの MST 辺を元グラフ上の Voronoi 親経路へ展開する
            dsu union_find(static_cast<int>(terminals.size()));
            std::vector<int> candidate = seed_edges;
            int selected = 0;
            for (const auto &link : links) {
                if (!union_find.merge(link.owner_a, link.owner_b)) continue;
                const auto &bridge = edges_[link.edge_id];
                for (int vertex : {bridge.from, bridge.to}) {
                    auto path = restore_to_any_source(vertex, workspace.parent_edge);
                    candidate.insert(candidate.end(), path.begin(), path.end());
                }
                candidate.push_back(link.edge_id);
                ++selected;
                if (selected + 1 == static_cast<int>(terminals.size())) break;
            }
            ++statistics.initial_solution_count;
            if (selected + 1 < static_cast<int>(terminals.size())) return {};
            return candidate;
        }

        std::vector<int> sequential_cached_candidate(
            const std::vector<int> &terminals, const path_store &store,
            int root_index, int rcl, std::mt19937_64 &random,
            engine_statistics &statistics) const {
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
                    for (int vertex : {edge.from, edge.to}) if (!in_tree[vertex]) {
                        in_tree[vertex] = true;
                        new_vertices.push_back(vertex);
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

        template <bool Free = false>
        bool induced_mst_improve(tree_solution &best, const std::vector<int> &terminals,
            const solve_workspace &workspace, engine_statistics &statistics,
            const std::vector<int> *free_edges = nullptr, const std::vector<char> *is_free = nullptr) const {
            if (!best.ok) return false;
            std::vector<char> allowed(n_, false);
            for (int terminal : terminals) allowed[terminal] = true;
            const auto allow_edges = [&](const std::vector<int> &ids) {
                for (int id : ids) allowed[edges_[id].from] = allowed[edges_[id].to] = true;
            };
            allow_edges(best.edges);
            if constexpr (Free) allow_edges(*free_edges);
            ++statistics.local_move_count;
            dsu union_find(n_);
            for (int terminal : terminals) union_find.set_terminal(terminal);
            std::vector<int> tree_edges;
            tree_edges.reserve(std::max(0, n_ - 1));
            // 既設辺を先に併合し、追加辺は事前に整列した費用順で選ぶ。
            if constexpr (Free) {
                for (int id : *free_edges) {
                    if (!edge_enabled(id)) continue;
                    const auto &edge = edges_[id];
                    if (union_find.merge(edge.from, edge.to)) tree_edges.push_back(id);
                }
            }
            if (!union_find.terminals_connected()) {
                for (int id : workspace.edge_order) {
                    if ((Free && (*is_free)[id]) || !edge_enabled(id)) continue;
                    const auto &edge = edges_[id];
                    if (!allowed[edge.from] || !allowed[edge.to] || !union_find.merge(edge.from, edge.to)) continue;
                    tree_edges.push_back(id);
                    if (union_find.terminals_connected()) break;
                }
            }
            if (!union_find.terminals_connected()) return false;
            auto candidate = prune_tree<Free>(std::move(tree_edges), terminals, is_free);
            if (!candidate.ok || candidate.cost >= best.cost) return false;
            best = std::move(candidate);
            ++statistics.improvement_count;
            return true;
        }

        // 木の指定arcから、terminalまたは次数2以外の頂点まで辿る。
        key_path trace_key_path(int start, int arc, const tree_solution &best,
            const tree_view &view, const std::vector<char> &is_terminal,
            const std::vector<char> *is_free) const {
            key_path path;
            path.endpoint_a = start;
            int previous = start;
            while (arc >= 0) {
                const int local_id = (arc / 2);
                path.edge_ids.push_back(local_id);
                const int global_id = best.edges[local_id];
                if (is_free == nullptr || !(*is_free)[global_id]) path.cost += edges_[global_id].cost;
                const int vertex = view.to[arc];
                if (is_terminal[vertex] || view.degree[vertex] != 2) {
                    path.endpoint_b = vertex;
                    break;
                }
                arc = view.head[vertex];
                while (arc >= 0 && view.to[arc] == previous) arc = view.next[arc];
                previous = vertex;
            }
            return path;
        }

        template <bool Free = false>
        bool terminal_branch_reconnect(tree_solution &best, const std::vector<int> &terminals,
                                       const path_store *store, int attempt_limit,
                                       const time_controller &timer, solve_workspace &workspace,
                                       engine_statistics &statistics, const std::vector<int> *free_edges = nullptr,
                                       const std::vector<char> *is_free = nullptr) const {
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
                    if (!Free || !(*is_free)[edge_id]) cost += edge.cost;
                    vertex = edge.from == vertex ? edge.to : edge.from;
                    if (target[vertex]) return {std::move(path), cost};
                }
                return {{}, INF_VALUE};

            };

            if (!best.ok || best.edges.empty()) return false;
            const tree_view view = make_tree_view(best);
            std::vector<char> is_terminal(n_, false);
            for (int terminal : terminals) is_terminal[terminal] = true;
            std::vector<key_path> branches;

            // 葉 terminal から最初の key vertex までを 1 本の branch として抽出する
            for (int terminal : terminals) {
                if (view.degree[terminal] != 1) continue;
                auto current = trace_key_path(terminal, view.head[terminal], best, view,
                                           is_terminal, Free ? is_free : nullptr);
                for (int &edge_id : current.edge_ids) edge_id = best.edges[edge_id];
                if ((!Free || current.cost > 0) && current.edge_ids.size() < best.edges.size())
                    branches.push_back(std::move(current));
            }
            std::sort(branches.begin(), branches.end(), [](const key_path &lhs, const key_path &rhs) {
                return lhs.cost > rhs.cost;
            });

            const int tries = std::min(attempt_limit, static_cast<int>(branches.size()));
            for (int branch_index = 0; branch_index < tries; ++branch_index) {
                if (timer.over()) break;
                const auto &current = branches[branch_index];
                const auto removed_mark = make_edge_mark(current.edge_ids);
                std::vector<char> target(n_, false);
                for (int edge_id : best.edges) {
                    if (removed_mark[edge_id]) continue;
                    target[edges_[edge_id].from] = true;
                    target[edges_[edge_id].to] = true;
                }

                // cache があれば距離表を走査し、なければ nearest-target Dijkstra を行う
                int goal = -1;
                long long replacement_cost = INF_VALUE;
                std::vector<int> path;
                if (store != nullptr) {
                    const int source_index = static_cast<int>(std::lower_bound(terminals.begin(), terminals.end(), current.endpoint_a) - terminals.begin());
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
                    goal = dijkstra_to_target<Free>({current.endpoint_a}, target, current.cost, workspace, statistics, is_free);
                    if (goal >= 0) {
                        replacement_cost = workspace.dist[goal];
                        path = restore_parent_path(current.endpoint_a, goal, workspace.parent_edge);
                    }
                }
                ++statistics.local_move_count;
                if (goal < 0 || replacement_cost >= current.cost || path.empty()) continue;
                auto [short_path, short_cost] = truncate_path_to_target(current.endpoint_a, path, target);
                if (short_path.empty() || short_cost >= current.cost) continue;
                std::vector<int> candidate;
                candidate.reserve(best.edges.size() - current.edge_ids.size() + short_path.size());
                for (int edge_id : best.edges) {
                    if (!removed_mark[edge_id]) candidate.push_back(edge_id);
                }
                candidate.insert(candidate.end(), short_path.begin(), short_path.end());
                if constexpr (Free) {
                    auto improved = normalize_candidate<true>(std::move(candidate), terminals, free_edges, is_free);
                    if (!improved.ok || improved.cost >= best.cost) continue;
                    best = std::move(improved);
                } else {
                    std::sort(candidate.begin(), candidate.end());
                    best.edges = std::move(candidate);
                    best.cost = best.cost - current.cost + short_cost;
                }
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
                    const int first_local = (first_arc / 2);
                    if (visited[first_local]) continue;
                    auto path = trace_key_path(start_vertex, first_arc, best, view, is_terminal, is_free);
                    for (int local_id : path.edge_ids) visited[local_id] = true;
                    if (path.endpoint_b >= 0) paths.push_back(std::move(path));
                }
            }
            std::sort(paths.begin(), paths.end(), [](const key_path &lhs, const key_path &rhs) {
                if (lhs.cost != rhs.cost) return lhs.cost > rhs.cost;
                return lhs.edge_ids.size() > rhs.edge_ids.size();
            });
            return paths;
        }

        template <bool Free = false>
        bool key_path_exchange(tree_solution &best, const std::vector<int> &terminals,
                               int attempt_limit, const time_controller &timer,
                               solve_workspace &workspace,
                               engine_statistics &statistics, const std::vector<int> *free_edges = nullptr,
                               const std::vector<char> *is_free = nullptr) const {
            if (!best.ok || best.edges.empty()) return false;
            const tree_view view = make_tree_view(best);
            const auto paths = enumerate_key_paths(best, terminals, view, is_free);
            const int tries = std::min(attempt_limit, static_cast<int>(paths.size()));
            std::vector<char> blocked(best.edges.size(), false);

            for (int path_index = 0; path_index < tries; ++path_index) {
                if (timer.over()) break;
                const auto &path = paths[path_index];
                if (Free && path.cost == 0) break;
                std::fill(blocked.begin(), blocked.end(), false);
                for (int local_id : path.edge_ids) blocked[local_id] = true;
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
                            if (blocked[(arc_index / 2)]) continue;
                            const int adjacent = view.to[arc_index];
                            if (mark[adjacent]) continue;
                            mark[adjacent] = true;
                            stack.push_back(adjacent);
                        }
                    }
                };
                collect_component(path.endpoint_a, component_a, vertices_a);
                collect_component(path.endpoint_b, component_b, vertices_b);

                // 小さい成分を multi-source とし、もう一方へ最短路で再接続する
                const bool use_a_as_source = vertices_a.size() <= vertices_b.size();
                const std::vector<int> &sources = use_a_as_source ? vertices_a : vertices_b;
                const std::vector<char> &target_mark = use_a_as_source ? component_b : component_a;
                const int goal = dijkstra_to_target<Free>(sources, target_mark, path.cost, workspace, statistics, is_free);
                ++statistics.local_move_count;
                if (goal < 0) continue;
                const long long replacement_cost = workspace.dist[goal];
                auto replacement = restore_to_any_source(goal, workspace.parent_edge);
                if (replacement_cost >= path.cost || replacement.empty()) continue;

                std::vector<int> candidate;
                candidate.reserve(best.edges.size() - path.edge_ids.size() + replacement.size());
                for (int local_id = 0; local_id < static_cast<int>(best.edges.size()); ++local_id) {
                    if (!blocked[local_id]) candidate.push_back(best.edges[local_id]);
                }
                candidate.insert(candidate.end(), replacement.begin(), replacement.end());
                auto improved = normalize_candidate<Free>(std::move(candidate), terminals, free_edges, is_free);
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

        path_store capture_paths(const solve_workspace &workspace) const {
            path_store row{std::vector<long long>(n_, INF_VALUE), std::vector<int>(n_, -1)};
            for (int vertex = 0; vertex < n_; ++vertex) if (workspace.visit_stamp[vertex] == workspace.current_stamp) {
                row.dist[vertex] = workspace.dist[vertex];
                row.trace[vertex] = workspace.parent_edge[vertex];
            }
            return row;
        }

        // elite 局所探索を完了してから、残り予算で最良解だけを改善する
        void final_refinement(tree_solution &best, const std::vector<int> &terminals,
            const std::vector<int> &free_edges, const std::vector<char> &is_free,
            bool quality, const time_controller &timer, solve_workspace &workspace,
            engine_statistics &statistics) const {
            struct branch_neighborhood {
                long long cost = 0;
                int vertex = -1;
                size_t begin = 0, end = 0;
            };

            // 頂点ごとに全key-pathを再走査せず、端点の索引を1回だけ構築する。
            // 各頂点内は path 順とし、候補は費用降順・頂点番号昇順で列挙する。
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
            const auto reconnect_four_components = [&]() {
                if (!best.ok || best.cost == 0 || timer.over()) return;
                const auto original = best;
                const auto view = make_tree_view(original);
                const auto paths = enumerate_key_paths(original, terminals, view,
                    free_edges.empty() ? nullptr : &is_free);
                const auto indexed = enumerate_branch_neighborhoods(paths, terminals);
                std::vector<int> branch_index(n_, -1);
                for (int i = 0; i < static_cast<int>(indexed.first.size()); ++i) {
                    const auto &branch = indexed.first[i];
                    if (branch.end - branch.begin == 3) branch_index[branch.vertex] = i;
                }
                std::vector<std::tuple<long long, int, int>> regions;
                for (const auto &path : paths) {
                    const int a = branch_index[path.endpoint_a], b = branch_index[path.endpoint_b];
                    if (a < 0 || b < 0) continue;
                    regions.emplace_back(indexed.first[a].cost - path.cost + indexed.first[b].cost,
                                         path.endpoint_a, path.endpoint_b);
                }
                std::sort(regions.begin(), regions.end(), [](const auto &a, const auto &b) {
                    if (std::get<0>(a) != std::get<0>(b)) return std::get<0>(a) > std::get<0>(b);
                    return a < b;
                });
                const int tries = std::min(static_cast<int>(regions.size()), quality ? 2 : 1);
                const std::array<std::array<int, 2>, 6> pairs{{{{0,1}},{{0,2}},{{0,3}},{{1,2}},{{1,3}},{{2,3}}}};
                const std::vector<char> no_target(n_, false);
                for (int attempt = 0; attempt < tries && !timer.over(); ++attempt) {
                    const auto [limit, left_center, right_center] = regions[attempt];
                    const auto center = [&](int v) { return v == left_center || v == right_center; };
                    std::vector<char> blocked(original.edges.size());
                    std::vector<int> endpoints;
                    for (const auto &path : paths) if (center(path.endpoint_a) || center(path.endpoint_b)) {
                        for (int local : path.edge_ids) blocked[local] = true;
                        if (!center(path.endpoint_a)) endpoints.push_back(path.endpoint_a);
                        if (!center(path.endpoint_b)) endpoints.push_back(path.endpoint_b);
                    }
                    std::sort(endpoints.begin(), endpoints.end());
                    endpoints.erase(std::unique(endpoints.begin(), endpoints.end()), endpoints.end());
                    if (endpoints.size() != 4) continue;
                    auto forest = free_edges;
                    for (size_t i = 0; i < original.edges.size(); ++i)
                        if (!blocked[i]) forest.push_back(original.edges[i]);
                    const auto zero = make_edge_mark(forest);
                    std::array<path_store, 4> single;
                    for (int side = 0; side < 4; ++side) {
                        if (timer.over()) return;
                        std::vector<int> sources{endpoints[side]};
                        std::vector<char> seen(n_); seen[sources[0]] = true;
                        for (size_t i = 0; i < sources.size(); ++i)
                            for (int a = view.head[sources[i]]; a >= 0; a = view.next[a]) {
                                const int v = view.to[a];
                                if (!blocked[(a / 2)] && !seen[v]) { seen[v] = true; sources.push_back(v); }
                            }
                        dijkstra_to_target<true>(sources, no_target, std::min(limit, INF_VALUE - 1) + 1, workspace, statistics, &zero);
                        single[side] = capture_paths(workspace);
                    }
                    std::array<path_store, 6> joined;
                    for (int pair = 0; pair < 6; ++pair) {
                        if (timer.over()) return;
                        // 相補な2成分を結ぶ費用が最低限必要なので、その下界を差し引く。
                        const auto &other = pairs[5 - pair];
                        const long long pair_limit = limit - single[other[0]].dist[endpoints[other[1]]];
                        workspace.dist.assign(n_, INF_VALUE);
                        workspace.parent_edge.assign(n_, -1);
                        workspace.heap.clear();
                        for (int v = 0; v < n_; ++v) {
                            const long long cost = single[pairs[pair][0]].dist[v] + single[pairs[pair][1]].dist[v];
                            if (cost > pair_limit) continue;
                            workspace.dist[v] = cost;
                            workspace.heap.push(static_cast<unsigned long long>(cost), v);
                        }
                        ++statistics.dijkstra_count;
                        while (!workspace.heap.empty()) {
                            const auto [unsigned_distance, vertex] = workspace.heap.pop();
                            const long long distance = static_cast<long long>(unsigned_distance);
                            if (distance != workspace.dist[vertex]) continue;
                            for (int a = workspace.start[vertex]; a < workspace.start[vertex + 1]; ++a) {
                                ++statistics.edge_relaxation_count;
                                const auto &arc = workspace.arcs[a];
                                if constexpr (Filtered) { if (!enabled_edge_[arc.edge_id]) continue; }
                                const long long cost = zero[arc.edge_id] ? 0 : arc.cost;
                                if (cost > pair_limit - distance) continue;
                                const long long next = distance + cost;
                                if (next >= workspace.dist[arc.to]) continue;
                                workspace.dist[arc.to] = next;
                                workspace.parent_edge[arc.to] = arc.edge_id;
                                workspace.heap.push(static_cast<unsigned long long>(next), arc.to);
                            }
                        }
                        joined[pair] = {workspace.dist, workspace.parent_edge};
                    }
                    long long min_sum = INF_VALUE;
                    int partition = -1, junction = -1;
                    for (int pair = 0; pair < 3; ++pair) for (int v = 0; v < n_; ++v) {
                        const long long sum = joined[pair].dist[v] + joined[5 - pair].dist[v];
                        if (sum < min_sum) { min_sum = sum; partition = pair; junction = v; }
                    }
                    if (junction < 0 || timer.over()) continue;
                    for (int pair : {partition, 5 - partition}) {
                        int v = junction;
                        for (int step = 0; step <= n_; ++step) {
                            const int id = joined[pair].trace[v];
                            if (id < 0) break;
                            forest.push_back(id);
                            const auto &edge = edges_[id];
                            v = edge.from == v ? edge.to : edge.from;
                        }
                        for (int side : pairs[pair]) {
                            auto path = restore_to_any_source(v, single[side].trace);
                            forest.insert(forest.end(), path.begin(), path.end());
                        }
                    }
                    auto candidate = free_edges.empty() ? normalize_candidate(std::move(forest), terminals) :
                        normalize_candidate<true>(std::move(forest), terminals, &free_edges, &is_free);
                    ++statistics.local_move_count;
                    if (candidate.ok && candidate.cost < best.cost) {
                        best = std::move(candidate); ++statistics.improvement_count;
                    }
                }

            };

            // 分岐に接する最大経路をまとめて外し、残存辺をseedとするVoronoiで再構築する。
            const auto destroy_branch = [&]() {
                if (!best.ok || best.cost == 0 || timer.over()) return;
                const auto original = best;
                const auto view = make_tree_view(original);
                const auto paths = enumerate_key_paths(original, terminals, view,
                    free_edges.empty() ? nullptr : &is_free);
                const auto [order, path_indices] = enumerate_branch_neighborhoods(paths, terminals);
                const int tries = std::min(static_cast<int>(order.size()), quality ? 12 : 4);
                for (int i = 0; i < tries && !timer.over(); ++i) {
                    std::vector<char> removed(original.edges.size());
                    for (size_t slot = order[i].begin; slot < order[i].end; ++slot)
                        for (int local : paths[path_indices[slot]].edge_ids) removed[local] = true;
                    auto seed = free_edges;
                    for (size_t j = 0; j < original.edges.size(); ++j)
                        if (!removed[j]) seed.push_back(original.edges[j]);
                    const auto mark = make_edge_mark(seed);
                    auto candidate = seeded_voronoi_candidate(terminals, seed, mark, workspace, statistics);
                    auto improved = free_edges.empty() ? normalize_candidate(std::move(candidate), terminals) :
                        normalize_candidate<true>(std::move(candidate), terminals, &free_edges, &is_free);
                    ++statistics.local_move_count;
                    if (improved.ok && improved.cost < best.cost) {
                        best = std::move(improved);
                        ++statistics.improvement_count;
                    }
                }

            };

            // 最良解の次数3の非terminal分岐を外し、3成分の距離和が最小の接合点へ移す。
            const auto relocate_branch = [&]() {
                if (!best.ok || best.cost == 0 || timer.over()) return;
                const auto original = best;
                const auto view = make_tree_view(original);
                const auto paths = enumerate_key_paths(original, terminals, view,
                    free_edges.empty() ? nullptr : &is_free);
                auto [order, path_indices] = enumerate_branch_neighborhoods(paths, terminals);
                std::erase_if(order, [](const auto &branch) {
                    return branch.end - branch.begin != 3;
                });
                const int tries = std::min(static_cast<int>(order.size()), quality ? 8 : 2);
                for (int i = 0; i < tries && !timer.over(); ++i) {
                    const int center = order[i].vertex;
                    std::vector<char> blocked(original.edges.size());
                    std::vector<int> endpoints;
                    for (size_t slot = order[i].begin; slot < order[i].end; ++slot) {
                        const auto &p = paths[path_indices[slot]];
                        endpoints.push_back(p.endpoint_a == center ? p.endpoint_b : p.endpoint_a);
                        for (int local : p.edge_ids) blocked[local] = true;
                    }
                    if (endpoints.size() != 3) continue;
                    auto forest = free_edges;
                    for (size_t j = 0; j < original.edges.size(); ++j)
                        if (!blocked[j]) forest.push_back(original.edges[j]);
                    const auto zero = make_edge_mark(forest);
                    std::array<path_store, 3> distance;
                    for (int side = 0; side < 3; ++side) {
                        if (timer.over()) return;
                        std::vector<int> sources{endpoints[side]};
                        std::vector<char> seen(n_); seen[sources[0]] = true;
                        for (size_t j = 0; j < sources.size(); ++j)
                            for (int a = view.head[sources[j]]; a >= 0; a = view.next[a]) {
                                const int v = view.to[a];
                                if (!blocked[(a / 2)] && !seen[v]) { seen[v] = true; sources.push_back(v); }
                            }

                        // 旧3経路がこの費用の接続を与えるため、最小距離和は旧費用以下。
                        // 非負距離なので各距離も同じ上限以下。整数の同値境界を含める。
                        // d(A,v)+d(C,v)>=d(A,C) より、2本目は C-d(A,C) 以下だけで十分。
                        // 3本目も同様。残存森の各成分は0-costで連結なので端点間距離で下界が取れる。
                        const long long pair_lower = side == 0 ? 0 : distance[0].dist[endpoints[side == 1 ? 2 : 1]];
                        const long long limit = std::min(order[i].cost - pair_lower, INF_VALUE - 1) + 1;
                        const std::vector<char> no_target(n_, false);
                        dijkstra_to_target<true>(sources, no_target, limit, workspace, statistics, &zero);
                        distance[side] = capture_paths(workspace);
                    }
                    long long min_sum = INF_VALUE;
                    int junction = -1;
                    for (int v = 0; v < n_; ++v) {
                        const long long sum = distance[0].dist[v] + distance[1].dist[v] + distance[2].dist[v];
                        if (sum < min_sum) { min_sum = sum; junction = v; }
                    }
                    if (junction < 0) continue;
                    for (int side = 0; side < 3; ++side) {
                        auto path = restore_to_any_source(junction, distance[side].trace);
                        forest.insert(forest.end(), path.begin(), path.end());
                    }
                    auto improved = free_edges.empty() ? normalize_candidate(std::move(forest), terminals) :
                        normalize_candidate<true>(std::move(forest), terminals, &free_edges, &is_free);
                    ++statistics.local_move_count;
                    if (improved.ok && improved.cost < best.cost) {
                        best = std::move(improved);
                        ++statistics.improvement_count;
                    }
                }

            };

            const long long before = best.cost;
            relocate_branch();
            destroy_branch();
            reconnect_four_components();
            if (best.cost < before && !timer.over()) {
                if (free_edges.empty()) key_path_exchange(best, terminals, quality ? 16 : 6, timer, workspace, statistics);
                else key_path_exchange<true>(best, terminals, quality ? 16 : 6, timer, workspace, statistics, &free_edges, &is_free);
                if (free_edges.empty()) induced_mst_improve(best, terminals, workspace, statistics);
                else induced_mst_improve<true>(best, terminals, workspace, statistics, &free_edges, &is_free);
            }
        }

        std::vector<int> seeded_sequential_candidate(
            const std::vector<int> &seed_terminals,
            const std::vector<int> &seed_edges,
            const std::vector<char> &is_free,
            int root_index,
            int rcl,
            std::mt19937_64 &random,
            solve_workspace &workspace,
            engine_statistics &statistics) const {
            const int terminal_count = static_cast<int>(seed_terminals.size());
            std::vector<char> connected(terminal_count, false);
            std::vector<char> in_tree(n_, false);
            std::vector<long long> distance(n_, INF_VALUE);
            std::vector<int> parent_edge(n_, -1);
            connected[root_index] = true;
            in_tree[seed_terminals[root_index]] = true;
            incremental_dijkstra<true>(std::vector<int>{seed_terminals[root_index]}, distance, parent_edge, workspace, statistics, &is_free);
            std::vector<int> candidate = seed_edges;

            for (int added = 1; added < terminal_count; ++added) {
                std::vector<std::pair<long long, int>> top;
                top.reserve(rcl);
                for (int terminal_index = 0; terminal_index < terminal_count; ++terminal_index) {
                    if (connected[terminal_index] || distance[seed_terminals[terminal_index]] == INF_VALUE) continue;
                    const std::pair<long long, int> current{
                        distance[seed_terminals[terminal_index]], terminal_index};
                    auto position = std::lower_bound(top.begin(), top.end(), current);
                    if (position == top.end() && static_cast<int>(top.size()) >= rcl) continue;
                    top.insert(position, current);
                    if (static_cast<int>(top.size()) > rcl) top.pop_back();
                }
                if (top.empty()) return {};
                const auto picked = top[static_cast<size_t>(
                    random() % static_cast<std::uint64_t>(top.size()))];
                const int terminal_index = picked.second;
                const int terminal = seed_terminals[terminal_index];
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
                    for (int vertex : {edge.from, edge.to}) if (!in_tree[vertex]) {
                        in_tree[vertex] = true;
                        new_sources.push_back(vertex);
                    }
                }
                incremental_dijkstra<true>(new_sources, distance, parent_edge, workspace, statistics, &is_free);
            }
            ++statistics.initial_solution_count;
            return candidate;

        }

        tree_solution sequential_shortest_path(
            const std::vector<int> &query_terminals,
            const path_store *store,
            int root_index,
            int rcl,
            std::mt19937_64 &random,
            solve_workspace &workspace,
            engine_statistics &statistics) const {
            const auto sequential_on_demand = [&](
                const std::vector<int> &local_terminals,
                int local_root_index,
                int local_rcl,
                std::mt19937_64 &local_random,
                solve_workspace &caller_workspace,
                engine_statistics &caller_statistics) -> tree_solution {
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
                        in_tree[vertex] = true;
                        new_sources.push_back(vertex);
                        vertex = edge.from == vertex ? edge.to : edge.from;
                    }
                    if (!in_tree[vertex]) return {};
                    connected[terminal_index] = true;
                    incremental_dijkstra(new_sources, distance, parent_edge, caller_workspace, caller_statistics);
                }
                ++caller_statistics.initial_solution_count;
                return normalize_candidate(std::move(candidate), local_terminals);

            };

            if (store != nullptr) {
                return normalize_candidate(
                    sequential_cached_candidate(query_terminals, *store, root_index, rcl, random, statistics), query_terminals);
            }
            return sequential_on_demand(query_terminals, root_index, rcl, random, workspace, statistics);

        }

        template <bool Free>
        engine_result search(const std::vector<int> &terminals,
            const options &options, const std::vector<int> &base_edge_ids) const {
            engine_result result;
            if (terminals.size() <= 1) {
                result.ok = true;
                return result;
            }

            // 通常探索と無料の既設辺を持つ探索で、制御と局所改善を共有する。
            solve_workspace &workspace = reusable_workspace_;
            engine_statistics statistics;
            time_controller timer(options);
            std::vector<char> is_base;
            if constexpr (Free) {
                is_base = make_edge_mark(base_edge_ids);
                auto base_only = normalize_candidate<true>({}, terminals, &base_edge_ids, &is_base);
                if (base_only.ok && base_only.cost == 0) {
                    result.ok = true;
                    return result;
                }
            }
            ensure_workspace(workspace, timer, options);
            std::mt19937_64 random(options.seed);
            const int terminal_count = static_cast<int>(terminals.size());
            const bool fast = options.preset_mode == preset::fast;
            const bool quality = options.preset_mode == preset::quality;
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
            if constexpr (Free) {
                auto candidate = seeded_voronoi_candidate(terminals, base_edge_ids, is_base, workspace, statistics);
                consider(normalize_candidate<true>(std::move(candidate), terminals, &base_edge_ids, &is_base));
            } else {
                consider(voronoi_mst(terminals, workspace, statistics));
            }

            // terminal cache が有利と判断した場合だけ最短路表を構築する
            path_store free_store;
            path_store *store_pointer = prepare_terminal_cache<Free>(
                terminals, options, timer, workspace, statistics, &free_store, &is_base);

            const auto sequential_candidate = [&](int root_index, int rcl) {
                if constexpr (Free) {
                    auto candidate = store_pointer != nullptr
                        ? sequential_cached_candidate(terminals, *store_pointer, root_index, rcl, random, statistics)
                        : seeded_sequential_candidate(terminals, base_edge_ids, is_base, root_index, rcl, random, workspace, statistics);
                    return normalize_candidate<true>(std::move(candidate), terminals, &base_edge_ids, &is_base);
                } else {
                    return sequential_shortest_path(terminals, store_pointer, root_index, rcl, random, workspace, statistics);
                }
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
                consider(sequential_candidate(root_index, rcl));
                if constexpr (!Free) { if (timer.over_fraction(3, 4)) break; }
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
                        consider(normalize_candidate<Free>(std::move(candidate), terminals, Free ? &base_edge_ids : nullptr, Free ? &is_base : nullptr));
                    }
                }
            }


            // 安価な改善から順に適用し、quality では複数の elite 解を別々に局所最適化する
            auto improve_candidate = [&](tree_solution candidate) {
                if (!candidate.ok || timer.over()) return candidate;
                induced_mst_improve<Free>(candidate, terminals, workspace, statistics, Free ? &base_edge_ids : nullptr, Free ? &is_base : nullptr);
                if (fast) return candidate;

                const int branch_rounds = quality ? 8 : 2;
                const int branch_attempts = quality ? 24 : 8;
                for (int round = 0; round < branch_rounds && !timer.over(); ++round) {
                    if (!terminal_branch_reconnect<Free>(candidate, terminals, store_pointer, branch_attempts, timer, workspace, statistics, Free ? &base_edge_ids : nullptr, Free ? &is_base : nullptr)) break;
                    induced_mst_improve<Free>(candidate, terminals, workspace, statistics, Free ? &base_edge_ids : nullptr, Free ? &is_base : nullptr);
                }

                const int key_rounds = quality ? 4 : 1;
                const int key_attempts = quality ? 16 : 6;
                for (int round = 0; round < key_rounds && !timer.over(); ++round) {
                    if (!key_path_exchange<Free>(candidate, terminals, key_attempts, timer, workspace, statistics, Free ? &base_edge_ids : nullptr, Free ? &is_base : nullptr)) break;
                    induced_mst_improve<Free>(candidate, terminals, workspace, statistics, Free ? &base_edge_ids : nullptr, Free ? &is_base : nullptr);
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
                if (quality) for (const auto &candidate : local_seed_pool) append_unique(candidate);
                if (!fast) {
                    const int local_elite_limit = std::min(4, static_cast<int>(elite.size()));
                    for (int index = quality ? 0 : 1; index < local_elite_limit; ++index) append_unique(elite[index]);
                }
                for (auto &candidate : local_candidates) {
                    if (timer.over()) break;
                    auto improved = improve_candidate(std::move(candidate));
                    if (improved.ok && improved.cost < best.cost) best = std::move(improved);
                }
            }

            if (options.preset_mode != preset::fast)
                final_refinement(best, terminals, base_edge_ids, is_base, quality, timer, workspace, statistics);
            result.ok = best.ok;
            result.cost = Free && !best.ok ? INF_VALUE : best.cost;
            if constexpr (Free) {
                if (best.ok) for (int id : best.edges) {
                    if (!is_base[id]) result.edges.push_back(id);
                }
            } else {
                result.edges = std::move(best.edges);
            }
            result.statistics = statistics;
            return result;
        }

    public:
        // n 頂点の空グラフを構築する O(1)
        explicit base_engine(int n) : n_(n) {
            assert(n >= 0);
        }

        // 無向辺を追加し、その 0-indexed 辺番号を返す 償却 O(1)
        int add_edge(int u, int v, long long w) {
            assert(0 <= u && u < n_ && 0 <= v && v < n_ && 0 <= w);
            const int edge_id = static_cast<int>(edges_.size());
            edges_.push_back(edge_type{u, v, w});
            workspace_ready_ = false;
            terminal_store_ready_ = false;
            return edge_id;
        }

        // 入力辺 L 本だけを MST 化し、非 terminal の葉を削除する O(K log K + N + L log L)
        engine_result normalize(
            const std::vector<int> &terminals,
            const std::vector<int> &candidate_edge_ids, bool sorted = false) const {
            engine_result result;
            tree_solution normalized = sorted ? normalize_sorted_candidate(candidate_edge_ids, terminals)
                                              : normalize_candidate(candidate_edge_ids, terminals);
            result.ok = normalized.ok;
            result.cost = normalized.ok ? normalized.cost : INF_VALUE;
            result.edges = std::move(normalized.edges);
            return result;
        }

        // 非連結な部分辺集合を追加・削除で修復する O(K(N + M log C) + A(N + M log C + L log L))
        engine_result repair(
            const std::vector<int> &terminals,
            const std::vector<int> &partial_edge_ids,
            const options &options) const {
            engine_result result;
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
            engine_statistics statistics;
            const time_controller timer(options);
            const std::vector<char> is_seed = make_edge_mark(partial_edge_ids);
            const bool fast = options.preset_mode == preset::fast;
            const bool quality = options.preset_mode == preset::quality;
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
                path_store *store_pointer = prepare_terminal_cache(terminals, options, timer, workspace, statistics);

                const int branch_rounds = quality ? 8 : 2;
                const int branch_attempts = quality ? 24 : 8;
                for (int round = 0; round < branch_rounds && !timer.over(); ++round) {
                    if (!terminal_branch_reconnect(best, terminals, store_pointer,
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

            if (options.preset_mode != preset::fast)
                final_refinement(best, terminals, {}, {}, quality, timer, workspace, statistics);
            result.ok = best.ok;
            result.cost = best.ok ? best.cost : INF_VALUE;
            result.edges = std::move(best.edges);
            result.statistics = statistics;
            return result;
        }

        // 撤去不能な既設辺を費用 0 として追加辺を求める O(A(K(N + M log C) + L log L + K^2))
        engine_result augment(const std::vector<int> &terminals,
            const std::vector<int> &base_edge_ids, const options &options) const {
            return search<true>(terminals, options, base_edge_ids);
        }

        // ヒューリスティックな Steiner tree を求める O(A(K(N + M log C) + M log M + K^2))
        engine_result solve(const std::vector<int> &terminals, const options &options) const {
            return search<false>(terminals, options, {});
        }

        // 既存の辺集合を正規化し、preset に応じて改善する O(K(N + M log C) + A(N + M log C + M log M))
        engine_result improve(const std::vector<int> &terminals,
                                              const std::vector<int> &initial_edge_ids,
                                              const options &options, engine_result *initial = nullptr) const {
            engine_result result;
            if (terminals.size() <= 1) {
                result.ok = true;
                return result;
            }

            // 固定グラフ用 workspace を再利用し、入力部分グラフを最小全域木化・葉刈りする
            solve_workspace &workspace = reusable_workspace_;
            engine_statistics statistics;
            time_controller timer(options);
            tree_solution best = normalize_candidate(initial_edge_ids, terminals);
            // 衝突分岐で改善解を発見できない場合に備え、正規化済みの初期木を渡す
            if (initial != nullptr && best.ok) {
                initial->ok = true;
                initial->cost = best.cost;
                initial->edges = best.edges;
            }
            if (!best.ok) {
                result.cost = INF_VALUE;
                result.statistics = statistics;
                return result;
            }
            ensure_workspace(workspace, timer, options);
            induced_mst_improve(best, terminals, workspace, statistics);
            if (options.preset_mode == preset::fast || timer.over()) {
                result.ok = true;
                result.cost = best.cost;
                result.edges = std::move(best.edges);
                result.statistics = statistics;
                return result;
            }

            // terminal cache が予算内なら枝再接続で利用し、そうでなければ打切り Dijkstra を使う
            path_store *store_pointer = prepare_terminal_cache(terminals, options, timer, workspace, statistics);

            const bool quality = options.preset_mode == preset::quality;
            const int branch_rounds = quality ? 8 : 2;
            const int branch_attempts = quality ? 24 : 8;
            for (int round = 0; round < branch_rounds && !timer.over(); ++round) {
                if (!terminal_branch_reconnect(best, terminals, store_pointer,
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

        // 到達不能を表す十分大きな値を返す O(1)
        static constexpr long long inf() {
            return INF_VALUE;
        }

        // 保持している通常グラフ用 terminal cache を解放する O(1)、解放領域は O(KN)
        void clear_terminal_cache() const {
            reusable_terminal_store_ = path_store{};
            std::vector<int>().swap(cached_terminals_);
            terminal_store_ready_ = false;
        }

        // 内部の禁止マスクを切り替え、経路cacheを無効化する O(1)
        void set_filter(const std::vector<unsigned char> *forbidden_vertex,
                        const std::vector<unsigned char> *forbidden_edge) {
            forbidden_vertex_ = forbidden_vertex;
            forbidden_edge_ = forbidden_edge;
            edge_filter_ready_ = false;
            clear_terminal_cache();
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
    struct prepared_context {
        const selection_rules *rules = nullptr;
        std::vector<unsigned char> forbidden_vertex;
        std::vector<unsigned char> forbidden_edge;
        std::vector<unsigned char> fixed_vertex;
        std::vector<int> fixed_vertices;
        status state = status::not_found;
        bool valid = true;
    };

    struct branch_node {
        std::vector<int> banned_vertices;
        int depth = 0;
    };

    int n_ = 0;
    std::vector<int> degree_;
    mutable base_engine<false> base_;
    mutable std::unique_ptr<base_engine<true>> filtered_;

    static std::vector<int> normalize_ids(std::vector<int> values) {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
        return values;
    }

    static options make_engine_options(const options &source,
        std::chrono::steady_clock::time_point deadline, int time_limit_ms) {
        options result = source;
        result.deadline = deadline;
        result.time_limit_ms = time_limit_ms;
        return result;
    }

    static void add_engine_statistics(statistics &target, const engine_statistics &source) {
        target.path_mode_used = source.path_mode_used;
        target.path_memory_bytes = std::max(target.path_memory_bytes, source.path_memory_bytes);
        target.initial_solution_count += source.initial_solution_count;
        target.restart_count += source.restart_count;
        target.local_move_count += source.local_move_count;
        target.improvement_count += source.improvement_count;
        target.dijkstra_count += source.dijkstra_count;
        target.edge_relaxation_count += source.edge_relaxation_count;
    }

    static bool options_valid(const options &opts) {
        return opts.constraint_node_limit >= -1 && opts.constraint_depth_limit >= -1;
    }

    // 高速な制約なし経路でも、公開クエリの不正IDを受理しない。
    bool query_inputs_valid(std::span<const int> terminals, std::span<const int> edge_ids,
                            const query_context &context) const {
        const auto valid_ids = [](std::span<const int> ids, int count) {
            return std::all_of(ids.begin(), ids.end(), [count](int id) { return 0 <= id && id < count; });
        };
        return valid_ids(terminals, n_) && valid_ids(edge_ids, base_.edge_count()) &&
            valid_ids(context.selected_outside, n_) &&
            (context.rules == nullptr || context.rules->vertex_count_ == n_);
    }

    bool context_is_empty(const query_context &context) const {
        return (context.rules == nullptr || context.rules->groups_.empty()) &&
            context.forbidden_vertices.empty() && context.forbidden_edges.empty();
    }

    prepared_context prepare_context(const std::vector<int> &terminals,
        const std::vector<int> &base_edge_ids, const query_context &context) const {
        prepared_context prepared;
        prepared.rules = context.rules;
        prepared.forbidden_vertex.assign(n_, 0);
        prepared.forbidden_edge.assign(static_cast<size_t>(base_.edge_count()), 0);
        prepared.fixed_vertex.assign(n_, 0);

        if (context.rules != nullptr) {
            if (context.rules->vertex_count_ != n_) {
                prepared.valid = false; prepared.state = status::invalid_input; return prepared;
            }
        }
        for (int vertex : context.forbidden_vertices) {
            if (vertex < 0 || vertex >= n_) {
                prepared.valid = false; prepared.state = status::invalid_input; return prepared;
            }
            prepared.forbidden_vertex[vertex] = 1;
        }
        for (int edge_id : context.forbidden_edges) {
            if (edge_id < 0 || edge_id >= base_.edge_count()) {
                prepared.valid = false; prepared.state = status::invalid_input; return prepared;
            }
            prepared.forbidden_edge[edge_id] = 1;
        }

        const auto add_fixed = [&](int vertex) {
            if (!prepared.fixed_vertex[vertex]) {
                prepared.fixed_vertex[vertex] = 1;
                prepared.fixed_vertices.push_back(vertex);
            }
        };
        for (int terminal : terminals) {
            if (terminal < 0 || terminal >= n_) {
                prepared.valid = false; prepared.state = status::invalid_input; return prepared;
            }
            add_fixed(terminal);
        }
        for (int vertex : context.selected_outside) {
            if (vertex < 0 || vertex >= n_) {
                prepared.valid = false; prepared.state = status::invalid_input; return prepared;
            }
            add_fixed(vertex);
        }
        for (int edge_id : base_edge_ids) {
            if (edge_id < 0 || edge_id >= base_.edge_count()) {
                prepared.valid = false; prepared.state = status::invalid_input; return prepared;
            }
            if (prepared.forbidden_edge[edge_id]) {
                prepared.valid = false; prepared.state = status::proven_infeasible; return prepared;
            }
            add_fixed(base_.edge(edge_id).from);
            add_fixed(base_.edge(edge_id).to);
        }
        for (int vertex : prepared.fixed_vertices) {
            if (prepared.forbidden_vertex[vertex]) {
                prepared.valid = false; prepared.state = status::proven_infeasible; return prepared;
            }
        }

        if (prepared.rules == nullptr) return prepared;
        std::vector<int> owner(prepared.rules->groups_.size(), -1);
        for (int vertex : prepared.fixed_vertices) {
            for (int group : prepared.rules->groups_of(vertex)) {
                if (owner[group] >= 0 && owner[group] != vertex) {
                    prepared.valid = false; prepared.state = status::proven_infeasible; return prepared;
                }
                owner[group] = vertex;
            }
        }
        for (int group = 0; group < static_cast<int>(owner.size()); ++group) {
            if (owner[group] < 0) continue;
            for (int vertex : prepared.rules->groups_[group]) {
                if (vertex != owner[group] && !prepared.fixed_vertex[vertex])
                    prepared.forbidden_vertex[vertex] = 1;
            }
        }
        return prepared;
    }

    std::pair<int, int> first_conflict(const prepared_context &prepared,
                                       const std::vector<int> &edge_ids) const {
        if (prepared.rules == nullptr || prepared.rules->groups_.empty()) return {-1, -1};
        std::vector<int> owner(prepared.rules->groups_.size(), -1);
        const auto inspect = [&](int vertex) -> std::pair<int, int> {
            for (int group : prepared.rules->groups_of(vertex)) {
                if (owner[group] >= 0 && owner[group] != vertex) return {owner[group], vertex};
                owner[group] = vertex;
            }
            return {-1, -1};
        };
        std::vector<unsigned char> seen(n_, 0);
        for (int vertex : prepared.fixed_vertices) {
            seen[vertex] = 1;
            const auto conflict = inspect(vertex);
            if (conflict.first >= 0) return conflict;
        }
        for (int edge_id : edge_ids) {
            const auto &edge = base_.edge(edge_id);
            for (int vertex : {edge.from, edge.to}) {
                if (seen[vertex]) continue;
                seen[vertex] = 1;
                const auto conflict = inspect(vertex);
                if (conflict.first >= 0) return conflict;
            }
        }
        return {-1, -1};
    }

    bool candidate_uses_forbidden(const prepared_context &prepared,
                                  const std::vector<int> &edge_ids) const {
        for (int edge_id : edge_ids) {
            if (edge_id < 0 || edge_id >= base_.edge_count()) return true;
            if (prepared.forbidden_edge[edge_id]) return true;
            const auto &edge = base_.edge(edge_id);
            if (prepared.forbidden_vertex[edge.from] || prepared.forbidden_vertex[edge.to]) return true;
        }
        return false;
    }

    template <class RunEngine>
    result constrained_search(const prepared_context &prepared,
                              const options &opts,
                              RunEngine run_engine) const {
        result answer;
        if (!prepared.valid) {
            answer.state = prepared.state;
            return answer;
        }
        const bool has_time_budget = opts.time_limit_ms > 0 ||
            opts.deadline != std::chrono::steady_clock::time_point::max();
        const int maximum_nodes = opts.constraint_node_limit >= 0 ? opts.constraint_node_limit :
            (opts.preset_mode == preset::fast ? (has_time_budget ? 16 : 4) :
             (opts.preset_mode == preset::quality ? 48 : (has_time_budget ? 32 : 16)));
        const int maximum_depth = opts.constraint_depth_limit >= 0 ? opts.constraint_depth_limit :
            (opts.preset_mode == preset::fast ? (has_time_budget ? 8 : 4) :
             (opts.preset_mode == preset::quality ? 24 : 12));
        if (maximum_nodes == 0) return answer;
        auto deadline = opts.deadline;
        if (opts.time_limit_ms > 0) {
            const auto relative = std::chrono::steady_clock::now() + std::chrono::milliseconds(opts.time_limit_ms);
            if (relative < deadline) deadline = relative;
        }
        const auto deadline_over = [&] {
            return deadline != std::chrono::steady_clock::time_point::max() &&
                std::chrono::steady_clock::now() >= deadline;
        };
        std::vector<branch_node> stack(1);
        std::vector<std::vector<int>> seen_bans(1);

        const auto push_branch = [&](std::vector<int> bans, int depth) {
            bans = normalize_ids(std::move(bans));
            for (const auto &seen : seen_bans) if (seen == bans) return;
            seen_bans.push_back(bans);
            stack.push_back(branch_node{std::move(bans), depth});
        };

        while (!stack.empty() && answer.stats.constraint_nodes < maximum_nodes && !deadline_over()) {
            branch_node node = std::move(stack.back());
            stack.pop_back();
            if (node.depth > maximum_depth) continue;
            bool invalid = false;
            for (int vertex : node.banned_vertices) {
                if (prepared.fixed_vertex[vertex]) { invalid = true; break; }
            }
            if (invalid) continue;

            ++answer.stats.constraint_nodes;
            std::vector<unsigned char> current_forbidden = prepared.forbidden_vertex;
            for (int vertex : node.banned_vertices) current_forbidden[vertex] = 1;
            if (filtered_ == nullptr) {
                filtered_ = std::make_unique<base_engine<true>>(n_);
                for (int edge_id = 0; edge_id < base_.edge_count(); ++edge_id) {
                    const auto &edge = base_.edge(edge_id);
                    const int added = filtered_->add_edge(edge.from, edge.to, edge.cost);
                    assert(added == edge_id);
                    (void)added;
                }
            }
            auto &engine = *filtered_;
            engine.set_filter(&current_forbidden, &prepared.forbidden_edge);
            const int remaining_nodes = maximum_nodes - answer.stats.constraint_nodes + 1;
            int node_time_ms = 0;
            if (deadline != std::chrono::steady_clock::time_point::max()) {
                const auto now = std::chrono::steady_clock::now();
                const auto remaining = now < deadline ?
                    std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count() : 0;
                node_time_ms = static_cast<int>(std::clamp<long long>(
                    remaining / std::clamp(remaining_nodes, 1, 32), 1, std::numeric_limits<int>::max()));
            }
            options engine_opts = make_engine_options(opts, deadline, node_time_ms);
            engine_opts.path_mode_value = path_mode::on_demand;
            std::uint64_t branch_seed = opts.seed + 0x9e3779b97f4a7c15ULL;
            for (int vertex : node.banned_vertices) {
                branch_seed ^= static_cast<std::uint64_t>(vertex + 1) +
                    0x9e3779b97f4a7c15ULL + (branch_seed << 6) + (branch_seed >> 2);
            }
            engine_opts.seed = branch_seed;
            engine_result candidate = run_engine(engine, engine_opts);
            add_engine_statistics(answer.stats, candidate.statistics);
            if (!candidate.ok) continue;
            if (candidate_uses_forbidden(prepared, candidate.edges)) continue;
            // baseの端点はprepared.fixed_verticesへ登録済みで、辺の禁止も検査済み。
            const auto conflict = first_conflict(prepared, candidate.edges);
            if (conflict.first < 0) {
                ++answer.stats.feasible_candidates;
                if (!answer.ok() || candidate.cost < answer.cost) {
                    answer.state = status::feasible;
                    answer.cost = candidate.cost;
                    answer.edges = std::move(candidate.edges);
                }
                if (opts.preset_mode == preset::fast && deadline == std::chrono::steady_clock::time_point::max()) break;
                continue;
            }
            ++answer.stats.conflict_candidates;
            if (node.depth >= maximum_depth) continue;

            const int a = conflict.first;
            const int b = conflict.second;
            const bool fixed_a = prepared.fixed_vertex[a] != 0;
            const bool fixed_b = prepared.fixed_vertex[b] != 0;
            if (fixed_a && fixed_b) continue;
            if (fixed_a) {
                auto bans = node.banned_vertices;
                bans.push_back(b);
                push_branch(std::move(bans), node.depth + 1);
            } else if (fixed_b) {
                auto bans = node.banned_vertices;
                bans.push_back(a);
                push_branch(std::move(bans), node.depth + 1);
            } else {
                int first = a;
                int second = b;
                if (degree_[first] > degree_[second]) std::swap(first, second);
                auto second_bans = node.banned_vertices;
                second_bans.push_back(second);
                push_branch(std::move(second_bans), node.depth + 1);
                auto first_bans = node.banned_vertices;
                first_bans.push_back(first);
                push_branch(std::move(first_bans), node.depth + 1);
            }
        }
        if (filtered_ != nullptr) filtered_->set_filter(nullptr, nullptr);
        return answer;
    }

    result from_engine_result(engine_result source) const {
        result answer;
        answer.state = source.ok ? status::feasible : status::not_found;
        answer.cost = source.cost;
        answer.edges = std::move(source.edges);
        add_engine_statistics(answer.stats, source.statistics);
        if (source.ok) answer.stats.feasible_candidates = 1;
        return answer;
    }

    static options default_options() { return {}; }
    static query_context default_context() { return {}; }

public:
    // n頂点の空グラフを構築する O(N)
    explicit constrained_steiner_tree(int n)
        : n_(n), degree_(n, 0), base_(n) { assert(n >= 0); }

    // 無向辺を追加し、その0-indexed辺番号を返す 償却 O(1)
    int add_edge(int u, int v, long long cost) {
        assert(0 <= u && u < n_ && 0 <= v && v < n_ && 0 <= cost);
        const int edge_id = base_.add_edge(u, v, cost);
        if (filtered_ != nullptr) {
            const int filtered_id = filtered_->add_edge(u, v, cost);
            assert(filtered_id == edge_id);
            (void)filtered_id;
        }
        if (u != v) { ++degree_[u]; ++degree_[v]; }
        return edge_id;
    }

    // 指定辺番号の辺情報を返す O(1)
    const edge_type &edge(int edge_id) const { return base_.edge(edge_id); }

    // 追加済み辺数を返す O(1)
    int edge_count() const { return base_.edge_count(); }

    // 頂点数を返す O(1)
    int vertex_count() const { return n_; }

    // 通常グラフ用terminal cacheを解放する O(1)、解放領域はO(KN)
    void clear_terminal_cache() const { base_.clear_terminal_cache(); if (filtered_ != nullptr) filtered_->clear_terminal_cache(); }

    // 到達不能を表す十分大きな値を返す O(1)
    static constexpr long long inf() { return base_engine<false>::inf(); }

    // 通常または制約付きSteiner treeを解く O(B * A(K(N + M log C) + M log M + K^2))
    result solve(const std::vector<int> &input_terminals,
                 const options &opts = default_options(), const query_context &context = default_context()) const {
        if (!options_valid(opts)) { result answer; answer.state = status::invalid_input; return answer; }
        if (!query_inputs_valid(input_terminals, {}, context)) {
            result answer; answer.state = status::invalid_input; return answer;
        }
        const std::vector<int> terminals = normalize_ids(input_terminals);
        if (context_is_empty(context)) {
            return from_engine_result(base_.solve(
                terminals, make_engine_options(opts, opts.deadline, opts.time_limit_ms)));
        }
        const prepared_context prepared = prepare_context(terminals, {}, context);
        return constrained_search(prepared, opts,
            [&](base_engine<true> &engine, const options &engine_opts) {
                return engine.solve(terminals, engine_opts);
            });
    }

    // 入力辺だけで合法な連結部分グラフを求める O(K log K + L log L + B(L + N + R) + M)、Rは排他グループの延べ所属数
    result normalize(const std::vector<int> &input_terminals,
                     const std::vector<int> &candidate_edge_ids,
                     const query_context &context = default_context()) const {
        if (!query_inputs_valid(input_terminals, candidate_edge_ids, context)) {
            result answer; answer.state = status::invalid_input; return answer;
        }
        const std::vector<int> terminals = normalize_ids(input_terminals);
        if (context_is_empty(context)) {
            return from_engine_result(base_.normalize(terminals, candidate_edge_ids));
        }
        const prepared_context prepared = prepare_context(terminals, {}, context);
        if (!prepared.valid) { result answer; answer.state = prepared.state; return answer; }
        // 分岐で禁止条件が変わっても辺の費用順は変わらないため、一度だけ並べる
        auto sorted_edges = candidate_edge_ids;
        std::sort(sorted_edges.begin(), sorted_edges.end(), [&](int a, int b) {
            const auto ca = base_.edge(a).cost, cb = base_.edge(b).cost;
            return ca != cb ? ca < cb : a < b;
        });
        sorted_edges.erase(std::unique(sorted_edges.begin(), sorted_edges.end()), sorted_edges.end());
        options opts;
        opts.constraint_node_limit = 32;
        opts.constraint_depth_limit = 24;
        return constrained_search(prepared, opts,
            [&](base_engine<true> &engine, const options &) {
                return engine.normalize(terminals, sorted_edges, true);
            });
    }

    // 非連結・制約違反を含めた部分辺集合をwarm startとして修復する O(B * A(K(N + M log C) + L log L))
    result repair(const std::vector<int> &input_terminals,
                  const std::vector<int> &partial_edge_ids,
                  const options &opts = default_options(), const query_context &context = default_context()) const {
        if (!options_valid(opts)) { result answer; answer.state = status::invalid_input; return answer; }
        if (!query_inputs_valid(input_terminals, partial_edge_ids, context)) {
            result answer; answer.state = status::invalid_input; return answer;
        }
        const std::vector<int> terminals = normalize_ids(input_terminals);
        if (context_is_empty(context)) {
            return from_engine_result(base_.repair(
                terminals, partial_edge_ids,
                make_engine_options(opts, opts.deadline, opts.time_limit_ms)));
        }
        const prepared_context prepared = prepare_context(terminals, {}, context);
        return constrained_search(prepared, opts,
            [&](base_engine<true> &engine, const options &engine_opts) {
                return engine.repair(terminals, partial_edge_ids, engine_opts);
            });
    }

    // 合法な連結初期解を改善し、制約違反入力はinvalid_candidateを返す O(B * A(K(N + M log C) + M log M))
    result improve(const std::vector<int> &input_terminals,
                   const std::vector<int> &initial_edge_ids,
                   const options &opts = default_options(), const query_context &context = default_context()) const {
        if (!options_valid(opts)) { result answer; answer.state = status::invalid_input; return answer; }
        if (!query_inputs_valid(input_terminals, initial_edge_ids, context)) {
            result answer; answer.state = status::invalid_input; return answer;
        }
        const std::vector<int> terminals = normalize_ids(input_terminals);
        if (context_is_empty(context)) {
            return from_engine_result(base_.improve(
                terminals, initial_edge_ids,
                make_engine_options(opts, opts.deadline, opts.time_limit_ms)));
        }
        const prepared_context prepared = prepare_context(terminals, {}, context);
        if (!prepared.valid) {
            result answer; answer.state = prepared.state; return answer;
        }
        if (candidate_uses_forbidden(prepared, initial_edge_ids) ||
            first_conflict(prepared, initial_edge_ids).first >= 0) {
            result answer; answer.state = status::invalid_candidate; return answer;
        }
        // engineで計算した初期木を受け取り、二重の正規化を避ける
        engine_result initial;
        auto answer = constrained_search(prepared, opts,
            [&](base_engine<true> &engine, const options &engine_opts) {
                engine_result local = engine.improve(terminals, initial_edge_ids, engine_opts, initial.ok ? nullptr : &initial);
                if (!local.ok && !initial_edge_ids.empty())
                    local = engine.repair(terminals, initial_edge_ids, engine_opts);
                return local;
            });
        // engineを一度も実行できなかった場合も、既知の合法な初期解を返す
        if (!initial.ok && !answer.ok()) initial = base_.normalize(terminals, initial_edge_ids);
        if (initial.ok && (!answer.ok() || initial.cost < answer.cost)) {
            answer.state = status::feasible;
            answer.cost = initial.cost;
            answer.edges = std::move(initial.edges);
        }
        return answer;
    }

    // 撤去不能な既設辺を費用0として追加辺を求める O(B * A(K(N + M log C) + L log L + K^2))
    augmentation_result augment(const std::vector<int> &input_terminals,
                                const std::vector<int> &base_edge_ids,
                                const options &opts = default_options(), const query_context &context = default_context()) const {
        if (!options_valid(opts)) { augmentation_result answer; answer.state = status::invalid_input; return answer; }
        if (!query_inputs_valid(input_terminals, base_edge_ids, context)) {
            augmentation_result answer; answer.state = status::invalid_input; return answer;
        }
        const std::vector<int> terminals = normalize_ids(input_terminals);
        if (context_is_empty(context)) {
            auto local = base_.augment(
                terminals, base_edge_ids,
                make_engine_options(opts, opts.deadline, opts.time_limit_ms));
            auto answer = from_engine_result(std::move(local));
            return {answer.state, answer.cost, std::move(answer.edges), answer.stats};
        }

        const prepared_context prepared = prepare_context(terminals, base_edge_ids, context);
        result searched = constrained_search(prepared, opts,
            [&](base_engine<true> &engine, const options &engine_opts) {
                return engine.augment(terminals, base_edge_ids, engine_opts);
            });
        augmentation_result answer;
        answer.state = searched.state;
        answer.added_cost = searched.cost;
        answer.added_edges = std::move(searched.edges);
        answer.stats = searched.stats;
        return answer;
    }

    // 既定設定で制約付きSteiner treeを解く O(B * A(K(N + M log C) + M log M + K^2))
    result solve(const std::vector<int> &terminals, const query_context &context) const {
        return solve(terminals, options{}, context);
    }

    // 既定設定で制約付き部分解を修復する O(B * A(K(N + M log C) + L log L))
    result repair(const std::vector<int> &terminals,
                  const std::vector<int> &partial_edge_ids,
                  const query_context &context) const {
        return repair(terminals, partial_edge_ids, options{}, context);
    }

    // 既定設定で制約付きの合法な初期解を改善する O(B * A(K(N + M log C) + M log M))
    result improve(const std::vector<int> &terminals,
                   const std::vector<int> &initial_edge_ids,
                   const query_context &context) const {
        return improve(terminals, initial_edge_ids, options{}, context);
    }

    // 既定設定で制約を守りながら既設網へ追加する O(B * A(K(N + M log C) + L log L + K^2))
    augmentation_result augment(const std::vector<int> &terminals,
                                const std::vector<int> &base_edge_ids,
                                const query_context &context) const {
        return augment(terminals, base_edge_ids, options{}, context);
    }

    // ヒューリスティック解の費用だけを返し、失敗時はinf() O(制約付きsolveと同じ)
    long long solve_cost(const std::vector<int> &terminals,
                         const options &opts = default_options(), const query_context &context = default_context()) const {
        const result answer = solve(terminals, opts, context);
        return answer.ok() ? answer.cost : inf();
    }

    // 既定設定で制約付き解の費用だけを返す O(solveと同じ)
    long long solve_cost(const std::vector<int> &terminals, const query_context &context) const {
        return solve_cost(terminals, options{}, context);
    }
};

#if __INCLUDE_LEVEL__ == 0

int main() {
    const auto check = [](bool condition) {
        if (!condition) throw std::runtime_error("constrained_steiner_tree self-test failed");
    };
    using solver_type = constrained_steiner_tree;

    // 安い違反経路を分岐で捨て、合法な代替経路を選ぶ
    {
        solver_type solver(5);
        solver.add_edge(0, 1, 1);
        solver.add_edge(1, 2, 1);
        solver.add_edge(2, 3, 1);
        solver.add_edge(0, 4, 3);
        solver.add_edge(4, 3, 3);
        solver_type::selection_rules rules(5);
        rules.add_conflict(1, 2);
        solver_type::query_context context;
        context.rules = &rules;
        solver_type::options options;
        options.preset_mode = solver_type::preset::quality;
        options.restart_limit = 4;
        options.constraint_node_limit = 16;
        const auto result = solver.solve({0, 3}, options, context);
        check(result.ok());
        check(result.cost == 6);
        check(result.stats.constraint_nodes >= 2);
    }

    // normalize / improve も排他制約を保つ
    {
        solver_type solver(5);
        const int e01 = solver.add_edge(0, 1, 1);
        const int e12 = solver.add_edge(1, 2, 1);
        const int e23 = solver.add_edge(2, 3, 1);
        const int e04 = solver.add_edge(0, 4, 3);
        const int e43 = solver.add_edge(4, 3, 3);
        solver_type::selection_rules rules(5);
        rules.add_conflict(1, 2);
        solver_type::query_context context;
        context.rules = &rules;
        const auto normalized = solver.normalize({0, 3}, {e01, e12, e23, e04, e43}, context);
        check(normalized.ok());
        check(normalized.cost == 6);
        const auto improved = solver.improve({0, 3}, {e04, e43}, context);
        check(improved.ok());
        check(improved.cost <= 6);
        std::array<unsigned char, 5> used{};
        for (int edge_id : improved.edges) {
            const auto &edge = solver.edge(edge_id);
            used[edge.from] = used[edge.to] = 1;
        }
        check(!(used[1] && used[2]));
    }

    // selected_outside と明示禁止の矛盾を検出する
    {
        solver_type solver(4);
        solver.add_edge(0, 1, 1);
        solver.add_edge(1, 3, 1);
        solver.add_edge(0, 2, 2);
        solver.add_edge(2, 3, 2);
        solver_type::selection_rules rules(4);
        rules.add_conflict(1, 2);
        std::vector<int> outside = {1};
        solver_type::query_context context;
        context.rules = &rules;
        context.selected_outside = outside;
        check(solver.solve({0, 3}, context).cost == 2);
        std::vector<int> forbidden = {1};
        context.forbidden_vertices = forbidden;
        check(solver.solve({0, 3}, context).state == solver_type::status::proven_infeasible);
    }

    // 排他グループを破る部分解を repair で合法化する
    {
        solver_type solver(6);
        const int e01 = solver.add_edge(0, 1, 1);
        const int e12 = solver.add_edge(1, 2, 1);
        const int e25 = solver.add_edge(2, 5, 1);
        solver.add_edge(0, 3, 2);
        solver.add_edge(3, 5, 2);
        solver.add_edge(0, 4, 3);
        solver.add_edge(4, 5, 3);
        solver_type::selection_rules rules(6);
        const std::array<int, 3> group = {1, 2, 4};
        rules.add_exclusive_group(group);
        solver_type::query_context context;
        context.rules = &rules;
        const auto result = solver.repair({0, 5}, {e01, e12, e25}, context);
        check(result.ok());
        std::array<unsigned char, 6> used{};
        for (int edge_id : result.edges) {
            const auto &edge = solver.edge(edge_id);
            used[edge.from] = used[edge.to] = 1;
        }
        check(static_cast<int>(used[1]) + static_cast<int>(used[2]) +
              static_cast<int>(used[4]) <= 1);
    }

    // improve は制約違反の初期解を受理しない
    {
        solver_type solver(4);
        const int e01 = solver.add_edge(0, 1, 1);
        const int e12 = solver.add_edge(1, 2, 1);
        const int e23 = solver.add_edge(2, 3, 1);
        solver.add_edge(0, 3, 10);
        solver_type::selection_rules rules(4);
        rules.add_conflict(1, 2);
        solver_type::query_context context;
        context.rules = &rules;
        check(solver.improve({0, 3}, {e01, e12, e23}, context).state ==
              solver_type::status::invalid_candidate);
    }

    // augment は未使用の既設辺端点も選択済みとして排他制約へ含める
    {
        solver_type solver(5);
        const int base = solver.add_edge(0, 1, 100);
        solver.add_edge(1, 4, 5);
        solver.add_edge(0, 2, 1);
        solver.add_edge(2, 4, 1);
        solver_type::selection_rules rules(5);
        rules.add_conflict(1, 2);
        solver_type::query_context context;
        context.rules = &rules;
        const auto result = solver.augment({0, 4}, {base}, context);
        check(result.ok());
        check(result.added_cost == 5);
        for (int edge_id : result.added_edges) {
            const auto &edge = solver.edge(edge_id);
            check(edge.from != 2 && edge.to != 2);
        }
    }

    // ターンごとに禁止条件を変えても前回のfilterを持ち越さない
    {
        solver_type solver(4);
        solver.add_edge(0, 1, 1);
        solver.add_edge(1, 3, 1);
        solver.add_edge(0, 2, 2);
        solver.add_edge(2, 3, 2);
        std::vector<int> first_forbidden = {1};
        solver_type::query_context first;
        first.forbidden_vertices = first_forbidden;
        check(solver.solve({0, 3}, first).cost == 4);
        std::vector<int> second_forbidden = {2};
        solver_type::query_context second;
        second.forbidden_vertices = second_forbidden;
        check(solver.solve({0, 3}, second).cost == 2);
        check(solver.solve({0, 3}).cost == 2);
    }

    // 制約付き呼び出し後に辺を追加しても内部engineを同期する
    {
        solver_type solver(4);
        solver.add_edge(0, 1, 5);
        solver.add_edge(1, 3, 5);
        solver_type::selection_rules rules(4);
        rules.add_conflict(1, 2);
        solver_type::query_context context;
        context.rules = &rules;
        check(solver.solve({0, 3}, context).cost == 10);
        solver.add_edge(0, 2, 1);
        solver.add_edge(2, 3, 1);
        // 頂点2は1と排他だが1を使わなければ新経路を利用できる
        check(solver.solve({0, 3}, context).cost == 2);
    }

    // 小規模ランダム問題を全辺部分集合の厳密解と照合する
    struct brute_edge { int from; int to; long long cost; };
    struct brute_dsu {
        std::vector<int> parent;
        explicit brute_dsu(int n) : parent(n, -1) {}
        int leader(int vertex) {
            if (parent[vertex] < 0) return vertex;
            return parent[vertex] = leader(parent[vertex]);
        }
        void merge(int a, int b) {
            a = leader(a); b = leader(b);
            if (a == b) return;
            if (parent[a] > parent[b]) std::swap(a, b);
            parent[a] += parent[b]; parent[b] = a;
        }
    };
    std::mt19937_64 random(20261003);
    for (int test = 0; test < 80; ++test) {
        constexpr int n = 6;
        solver_type solver(n);
        std::vector<brute_edge> edges;
        const auto add = [&](int u, int v, long long cost) {
            solver.add_edge(u, v, cost);
            edges.push_back({u, v, cost});
        };
        add(0, n - 1, 20);
        for (int vertex = 1; vertex < n; ++vertex) {
            const int parent = static_cast<int>(random() % static_cast<std::uint64_t>(vertex));
            add(parent, vertex, 1 + static_cast<long long>(random() % 9));
        }
        while (edges.size() < 11) {
            const int u = static_cast<int>(random() % n);
            const int v = static_cast<int>(random() % n);
            if (u == v) continue;
            add(u, v, 1 + static_cast<long long>(random() % 9));
        }

        solver_type::selection_rules rules(n);
        std::vector<std::pair<int, int>> conflicts;
        for (int i = 0; i < 3; ++i) {
            const int a = 1 + static_cast<int>(random() % (n - 2));
            const int b = 1 + static_cast<int>(random() % (n - 2));
            if (a == b) continue;
            rules.add_conflict(a, b);
            conflicts.push_back({a, b});
        }
        solver_type::query_context context;
        context.rules = &rules;
        solver_type::options options;
        options.preset_mode = solver_type::preset::quality;
        options.restart_limit = 4;
        options.constraint_node_limit = 64;
        options.constraint_depth_limit = 16;
        const auto result = solver.solve({0, n - 1}, options, context);
        check(result.ok());

        long long optimum = solver_type::inf();
        const int edge_count = static_cast<int>(edges.size());
        for (int mask = 0; mask < (1 << edge_count); ++mask) {
            brute_dsu union_find(n);
            std::array<unsigned char, n> used{};
            used[0] = used[n - 1] = 1;
            long long cost = 0;
            for (int edge_id = 0; edge_id < edge_count; ++edge_id) {
                if ((mask & (1 << edge_id)) == 0) continue;
                const auto &edge = edges[edge_id];
                union_find.merge(edge.from, edge.to);
                used[edge.from] = used[edge.to] = 1;
                cost += edge.cost;
            }
            if (union_find.leader(0) != union_find.leader(n - 1)) continue;
            bool legal = true;
            for (const auto &[a, b] : conflicts) if (used[a] && used[b]) legal = false;
            if (legal) optimum = std::min(optimum, cost);
        }
        check(result.cost >= optimum);
        std::array<unsigned char, n> used{};
        used[0] = used[n - 1] = 1;
        for (int edge_id : result.edges) {
            const auto &edge = solver.edge(edge_id);
            used[edge.from] = used[edge.to] = 1;
        }
        for (const auto &[a, b] : conflicts) check(!(used[a] && used[b]));
    }

    // 分岐数が0、または期限切れでも、合法な初期解は失わない
    {
        solver_type solver(4);
        solver.add_edge(0, 1, 1); solver.add_edge(1, 3, 1);
        solver.add_edge(0, 2, 2); solver.add_edge(2, 3, 2);
        solver_type::selection_rules rules(4); rules.add_conflict(1, 2);
        solver_type::query_context context; context.rules = &rules;
        solver_type::options options; options.constraint_node_limit = 0;
        auto result = solver.improve({0, 3}, {2, 3}, options, context);
        check(result.ok() && result.cost == 4);
        options.constraint_node_limit = 16;
        options.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        result = solver.improve({0, 3}, {2, 3}, options, context);
        check(result.ok() && result.cost == 4);
    }

    std::cout << "OK\n";
}
#endif
