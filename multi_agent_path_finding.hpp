#pragma once

#include <bits/stdc++.h>

#include <atcoder/mincostflow>

/*
 * multi_agent_path_finding_v05: AHC向けの複数エージェント経路計画・搬送solver。
 *
 * 同期・無向・重みなしグラフ上で、各エージェントが1頂点を占有し、1ターンに
 * 隣接頂点への移動または待機を行うモデルを扱う。同時刻の頂点衝突と、辺上の
 * 正面衝突（u->v と v->u の同時移動）を禁止する。
 *
 * 静的MAPFには優先度付き時空間A*、goal干渉順、PIBT、LNSを用いる。
 * 継続搬送には搬送中優先PIBTと、greedy・端点分散・最小費用流による割当を用いる。
 * 既存解の一部だけを直す場合は、外部時空間予約を重ねたagent部分修復と
 * 時間窓修復を提供する。同一グラフの反復呼出しでは距離表と探索領域を再利用する。
 * 最適性・完全性より、AHCで短時間に良い実行可能解を得ることを優先する。
 */

struct mapf_graph {
    int vertex_count = 0;
    std::vector<int> offsets;
    std::vector<int> edges;

    mapf_graph() = default;

    // 無向辺集合から重複辺を除いたCSRグラフを構築する。O(V + E log E)
    mapf_graph(int n, const std::vector<std::pair<int, int>>& undirected_edges)
        : vertex_count(n) {
        assert(n >= 0);
        std::vector<std::vector<int>> adjacency(n);
        for (const auto& [u, v] : undirected_edges) {
            assert(0 <= u && u < n);
            assert(0 <= v && v < n);
            if (u == v) continue;
            adjacency[u].push_back(v);
            adjacency[v].push_back(u);
        }
        offsets.resize(n + 1);
        for (int v = 0; v < n; ++v) {
            auto& next = adjacency[v];
            std::sort(next.begin(), next.end());
            next.erase(std::unique(next.begin(), next.end()), next.end());
            offsets[v + 1] = offsets[v] + int(next.size());
        }
        edges.resize(offsets.back());
        for (int v = 0; v < n; ++v) {
            std::copy(adjacency[v].begin(), adjacency[v].end(), edges.begin() + offsets[v]);
        }
    }

    // 頂点数を返す。O(1)
    int size() const { return vertex_count; }

    // 頂点vの隣接頂点列を返す。O(1)
    std::span<const int> neighbors(int v) const {
        assert(0 <= v && v < vertex_count);
        return std::span<const int>(edges.data() + offsets[v], offsets[v + 1] - offsets[v]);
    }

    // 頂点vの次数を返す。O(1)
    int degree(int v) const {
        assert(0 <= v && v < vertex_count);
        return offsets[v + 1] - offsets[v];
    }
};

enum class mapf_strategy {
    prioritized,
    pibt,
    automatic,
};

enum class mapf_objective {
    sum_of_costs,
    makespan,
};

enum class mapf_status {
    success,
    invalid_input,
    unreachable,
    horizon_too_short,
    timeout,
    search_failed,
    memory_limit,
};

// 複数の部分solverで共有できる絶対deadline。
struct mapf_deadline {
    using clock = std::chrono::steady_clock;
    clock::time_point end = clock::time_point::max();

    mapf_deadline() = default;
    explicit mapf_deadline(clock::time_point end_time) : end(end_time) {}

    static mapf_deadline after_ms(double milliseconds) {
        if (milliseconds < 0.0) return {};
        return mapf_deadline(clock::now() +
                             std::chrono::duration_cast<clock::duration>(
                                 std::chrono::duration<double, std::milli>(milliseconds)));
    }

    bool expired() const { return clock::now() >= end; }
};

// 外部で確定済みの経路・障害物を時空間予約として渡す。
// ownerは-1が空き、-2が障害物、-3以下がreserve_pathで追加した外部経路を表す。
struct mapf_reservations {
    int vertex_count = 0;
    int horizon = -1;
    std::vector<int> owner;

private:
    mutable std::vector<std::uint64_t> blocked_moves;
    int next_path_owner = -3;
    mutable bool moves_sorted = true;

public:
    mapf_reservations() = default;
    mapf_reservations(int n, int max_time)
        : vertex_count(n),
          horizon(max_time) {
        assert(n >= 0 && max_time >= 0);
        owner.assign(std::size_t(n) * (std::size_t(max_time) + 1), -1);
    }

    bool valid() const {
        if (vertex_count < 0 || horizon < 0) return false;
        const std::size_t width = std::size_t(vertex_count);
        const std::size_t height = std::size_t(horizon) + 1;
        if (width != 0 && height > std::numeric_limits<std::size_t>::max() / width) {
            return false;
        }
        return owner.size() == width * height;
    }

    bool valid_time_vertex(int time, int vertex) const {
        return 0 <= time && time <= horizon && 0 <= vertex && vertex < vertex_count;
    }

    int at(int time, int vertex) const {
        assert(valid_time_vertex(time, vertex));
        return owner[std::size_t(time) * std::size_t(vertex_count) + std::size_t(vertex)];
    }

    bool block_vertex(int time, int vertex) {
        if (!valid_time_vertex(time, vertex)) return false;
        int& value = owner[std::size_t(time) * std::size_t(vertex_count) + std::size_t(vertex)];
        if (value != -1 && value != -2) return false;
        value = -2;
        return true;
    }

private:
    std::uint64_t move_key(int time, int from, int to) const {
        return (std::uint64_t(time) * std::uint64_t(vertex_count) + std::uint64_t(from)) *
                   std::uint64_t(vertex_count) +
               std::uint64_t(to);
    }

public:
    bool block_move(int time, int from, int to) {
        if (time < 0 || time >= horizon || from < 0 || from >= vertex_count || to < 0 ||
            to >= vertex_count) {
            return false;
        }
        blocked_moves.push_back(move_key(time, from, to));
        moves_sorted = false;
        return true;
    }

    bool block_edge(int time, int u, int v) {
        const bool forward = block_move(time, u, v);
        const bool backward = block_move(time, v, u);
        return forward && backward;
    }

    // path[0]をstart_timeに置く。hold_untilを省略するとpath末尾までだけ予約する。
    bool reserve_path(int start_time,
                      std::span<const int> path,
                      int hold_until = -1) {
        if (path.empty() || start_time < 0 || start_time > horizon) return false;
        const int arrival_time = start_time + int(path.size()) - 1;
        if (arrival_time > horizon) return false;
        if (hold_until < 0) hold_until = arrival_time;
        if (hold_until < arrival_time || hold_until > horizon) return false;
        for (int time = start_time; time <= hold_until; ++time) {
            const int index = std::min(time - start_time, int(path.size()) - 1);
            const int vertex = path[index];
            if (!valid_time_vertex(time, vertex) || at(time, vertex) != -1) return false;
            if (time == start_time) continue;
            const int previous = path[std::min(time - start_time - 1, int(path.size()) - 1)];
            if (move_is_blocked(time - 1, previous, vertex)) return false;
            const int opposite_owner = at(time - 1, vertex);
            if (opposite_owner <= -3 && at(time, previous) == opposite_owner) return false;
        }
        const int path_owner = next_path_owner--;
        for (int time = start_time; time <= hold_until; ++time) {
            const int index = std::min(time - start_time, int(path.size()) - 1);
            const int vertex = path[index];
            owner[std::size_t(time) * std::size_t(vertex_count) + std::size_t(vertex)] =
                path_owner;
        }
        return true;
    }

    bool move_is_blocked(int time, int from, int to) const {
        if (time < 0 || time >= horizon || from < 0 || from >= vertex_count || to < 0 ||
            to >= vertex_count) {
            return false;
        }
        if (blocked_moves.empty()) return false;
        if (!moves_sorted) {
            std::sort(blocked_moves.begin(), blocked_moves.end());
            blocked_moves.erase(
                std::unique(blocked_moves.begin(), blocked_moves.end()), blocked_moves.end());
            moves_sorted = true;
        }
        return std::binary_search(blocked_moves.begin(),
                                  blocked_moves.end(),
                                  move_key(time, from, to));
    }
};

struct mapf_options {
    mapf_strategy strategy = mapf_strategy::automatic;
    mapf_objective objective = mapf_objective::sum_of_costs;
    // 負値なら時計を使わず、反復数だけで停止する
    double time_limit_ms = 1000.0;
    std::uint64_t seed = 1;
    // 負値なら最短距離・頂点数・エージェント数から自動設定する
    int max_time = -1;
    int prioritized_restarts = 8;
    int lns_iterations = 64;
    // 0以下でLNS無効
    int lns_size = 8;
    // prioritized planningで既存経路が多く通る頂点を避ける強さ
    int congestion_weight = 0;
    // 非nullならtime_limit_msと共に、先に到達した方で停止する
    const mapf_deadline* deadline = nullptr;
};

struct mapf_result {
    bool success = false;
    mapf_status status = mapf_status::invalid_input;
    std::vector<std::vector<int>> paths;
    long long sum_of_costs = 0;
    int makespan = 0;
    long long expanded_states = 0;
    int attempts = 0;
    int lns_improvements = 0;
    mapf_strategy strategy_used = mapf_strategy::prioritized;
};

struct mapd_agent {
    int start = -1;
};

struct mapd_task {
    int pickup = -1;
    int delivery = -1;
    int release_time = 0;
};

enum class mapd_assignment_strategy {
    automatic,
    greedy,
    greedy_unique_endpoints,
    min_cost_flow,
};

struct mapd_options {
    int max_time = 1000;
    // 負値なら時計を使わずmax_timeまで全候補を実行する
    double time_limit_ms = 1000.0;
    std::uint64_t seed = 1;
    mapd_assignment_strategy assignment = mapd_assignment_strategy::automatic;
    // automaticが比較する候補数。1以上8以下に丸める
    int portfolio_runs = 8;
    const mapf_deadline* deadline = nullptr;
};

struct mapd_result {
    bool valid = false;
    int completed_tasks = 0;
    long long sum_flow_time = 0;
    long long total_moves = 0;
    std::vector<std::vector<int>> paths;
    std::vector<int> task_agent;
    std::vector<int> task_pickup_time;
    std::vector<int> task_completion_time;
};

struct mapf_repair_request {
    std::span<const int> agents;
    // end_time < 0なら始点から新しいgoalまで経路全体を再計画する。
    // end_time >= 0ならbaseの[begin_time, end_time]だけを再計画する。
    int begin_time = 0;
    int end_time = -1;
    const mapf_reservations* external_reservations = nullptr;
};

// 同一グラフで再利用する探索領域。グラフ変更前にclear()を呼ぶ。
// 型・状態・共有補助処理はprivate。利用者向け操作は末尾のpublic節だけ。
class mapf_workspace {
    friend bool validate_mapf_result(const mapf_graph&, std::span<const int>,
                                     std::span<const int>, const mapf_result&);
    friend bool validate_mapf_result(const mapf_graph&, std::span<const int>,
                                     std::span<const int>, const mapf_result&,
                                     const mapf_reservations&);
    friend mapf_result solve_mapf(const mapf_graph&, std::span<const int>,
                                  std::span<const int>, mapf_workspace&, const mapf_options&);
    friend mapf_result improve_mapf(const mapf_graph&, std::span<const int>,
                                    std::span<const int>, const mapf_result&,
                                    mapf_workspace&, const mapf_options&);
    friend mapf_result repair_mapf(const mapf_graph&, std::span<const int>,
                                   std::span<const int>, const mapf_result&,
                                   const mapf_repair_request&, mapf_workspace&, const mapf_options&);
    friend mapd_result solve_mapd(const mapf_graph&, std::span<const mapd_agent>,
                                  std::span<const mapd_task>, const mapd_options&);
#ifdef MAPF_TESTING
    friend struct mapf_test_access;
#endif
    static constexpr int inf_distance = std::numeric_limits<int>::max() / 4;

    struct timer {
        using clock = std::chrono::steady_clock;
        clock::time_point start = clock::now();
        double limit_ms = 0.0;
        const mapf_deadline* shared_deadline = nullptr;

        explicit timer(double milliseconds, const mapf_deadline* deadline = nullptr)
            : limit_ms(milliseconds), shared_deadline(deadline) {}

        bool expired() const {
            if (shared_deadline != nullptr && shared_deadline->expired()) return true;
            if (limit_ms < 0.0) return false;
            const double elapsed = std::chrono::duration<double, std::milli>(clock::now() - start).count();
            return elapsed >= limit_ms;
        }
    };

    struct distance_table {
        const mapf_graph* graph_identity = nullptr;
        int vertex_count = 0;
        std::vector<int> target_to_row;
        std::vector<int> row_targets;
        std::vector<int> distances;
        std::vector<int> queue;
        std::vector<int> missing_targets;

        distance_table() = default;

        distance_table(const mapf_graph& g, std::span<const int> targets)
            { reset(g); add_targets(g, targets); }

        void reset(const mapf_graph& g) {
            graph_identity = &g;
            vertex_count = g.size();
            target_to_row.assign(g.size(), -1);
            row_targets.clear();
            distances.clear();
            queue.resize(g.size());
            missing_targets.clear();
        }

        void add_targets(const mapf_graph& g, std::span<const int> targets) {
            assert(graph_identity == &g && vertex_count == g.size());
            missing_targets.clear();
            missing_targets.reserve(targets.size());
            for (const int target : targets) {
                assert(0 <= target && target < g.size());
                if (target_to_row[target] != -1) continue;
                target_to_row[target] = -2;
                missing_targets.push_back(target);
            }
            const int old_rows = int(row_targets.size());
            distances.resize(std::size_t(old_rows + int(missing_targets.size())) *
                             std::size_t(vertex_count));
            for (int index = 0; index < int(missing_targets.size()); ++index) {
                const int target = missing_targets[index];
                const int row = old_rows + index;
                target_to_row[target] = row;
                row_targets.push_back(target);
                int* dist = distances.data() + std::size_t(row) * std::size_t(vertex_count);
                std::fill(dist, dist + vertex_count, inf_distance);
                int head = 0;
                int tail = 0;
                queue[tail++] = target;
                dist[target] = 0;
                while (head < tail) {
                    const int v = queue[head++];
                    const int next_distance = dist[v] + 1;
                    for (const int to : g.neighbors(v)) {
                        if (dist[to] != inf_distance) continue;
                        dist[to] = next_distance;
                        queue[tail++] = to;
                    }
                }

            }
        }

        int row_of(int target) const {
            assert(0 <= target && target < vertex_count);
            return target_to_row[target];
        }

        int get_by_row(int row, int vertex) const {
            assert(0 <= row && row < int(row_targets.size()));
            assert(0 <= vertex && vertex < vertex_count);
            return distances[std::size_t(row) * std::size_t(vertex_count) + std::size_t(vertex)];
        }

    };

    struct reservation_table {
        int vertex_count = 0;
        int max_time = 0;
        int capacity_time = -1;
        std::vector<int> owner;
        std::vector<int> visit_count;
        const mapf_reservations* external = nullptr;
        int external_time_offset = 0;

        void prepare(int n, int horizon) {
            assert(n >= 0 && horizon >= 0);
            if (n != vertex_count || horizon > capacity_time) {
                vertex_count = n;
                capacity_time = horizon;
                owner.resize(std::size_t(n) * (std::size_t(horizon) + 1));
                visit_count.resize(n);
            }
            max_time = horizon;
        }

        int at(int time, int vertex) const {
            return owner[std::size_t(time) * std::size_t(vertex_count) + std::size_t(vertex)];
        }

        void set(int time, int vertex, int agent) {
            owner[std::size_t(time) * std::size_t(vertex_count) + std::size_t(vertex)] = agent;
        }

        bool reset(std::span<const int> starts,
                   const mapf_reservations* external_reservations = nullptr,
                   int time_offset = 0) {
            external = external_reservations;
            external_time_offset = time_offset;
            const std::size_t active_size =
                std::size_t(vertex_count) * (std::size_t(max_time) + 1);
            std::fill(owner.begin(), owner.begin() + active_size, -1);
            std::fill(visit_count.begin(), visit_count.end(), 0);

            if (external != nullptr) {
                if (!external->valid() || external->vertex_count != vertex_count ||
                    time_offset < 0) {
                    return false;
                }
                const int copied_horizon =
                    time_offset > external->horizon
                        ? -1
                        : std::min(max_time, external->horizon - time_offset);
                for (int time = 0; time <= copied_horizon; ++time) {
                    for (int vertex = 0; vertex < vertex_count; ++vertex) {
                        const int value = external->at(time_offset + time, vertex);
                        if (value == -1) continue;
                        owner[std::size_t(time) * std::size_t(vertex_count) +
                              std::size_t(vertex)] = value;
                    }
                }
            }
            for (int agent = 0; agent < int(starts.size()); ++agent) {
                if (starts[agent] < 0 || starts[agent] >= vertex_count) return false;
                const int old_owner = at(0, starts[agent]);
                if (old_owner != -1 && old_owner != agent) return false;
                set(0, starts[agent], agent);
            }
            return true;
        }

        bool reserve_path(int agent, std::span<const int> path) {
            if (path.empty()) return false;
            const int arrival = int(path.size()) - 1;
            for (int time = 0; time <= max_time; ++time) {
                const int vertex = time <= arrival ? path[time] : path.back();
                if (vertex < 0 || vertex >= vertex_count) return false;
                const int old_owner = at(time, vertex);
                if (old_owner != -1 && old_owner != agent) return false;
                if (time > 0) {
                    const int previous = time <= arrival ? path[time - 1] : path.back();
                    if (external != nullptr &&
                        external->move_is_blocked(
                            external_time_offset + time - 1, previous, vertex)) {
                        return false;
                    }
                    const int opposite_owner = at(time - 1, vertex);
                    const bool moving_owner = opposite_owner >= 0 || opposite_owner <= -3;
                    if (moving_owner && opposite_owner != agent &&
                        at(time, previous) == opposite_owner) {
                        return false;
                    }
                }
            }
            for (int time = 0; time <= max_time; ++time) {
                const int vertex = time <= arrival ? path[time] : path.back();
                set(time, vertex, agent);
                if (time <= arrival) ++visit_count[vertex];
            }
            return true;
        }

    };

    struct astar_workspace {
        struct astar_outcome {
            bool success = false;
            std::vector<int> path;
            long long expanded_states = 0;
        };
        struct open_node {
            std::uint64_t key;
            int time;
            int vertex;

            bool operator<(const open_node& rhs) const {
                if (key != rhs.key) return key > rhs.key;
                return time < rhs.time;
            }
        };

        int vertex_count = 0;
        int max_time = 0;
        int capacity_time = -1;
        std::vector<int> seen;
        std::unique_ptr<int[]> best_secondary;
        std::unique_ptr<int[]> parent_vertex;
        std::vector<open_node> open_storage;
        int stamp = 0;

        void prepare(int n, int horizon) {
            assert(n >= 0 && horizon >= 0);
            if (n != vertex_count || horizon > capacity_time) {
                vertex_count = n;
                capacity_time = horizon;
                const std::size_t size = std::size_t(n) * (std::size_t(horizon) + 1);
                seen.assign(size, 0);
                // seenが現在のstampに一致する状態だけを読む。値は訪問時に必ず書く
                best_secondary = std::make_unique_for_overwrite<int[]>(size);
                parent_vertex = std::make_unique_for_overwrite<int[]>(size);
                stamp = 0;
            }
            max_time = horizon;
        }

        astar_outcome find_path(const mapf_graph& graph,
                                int agent,
                                int start,
                                int goal,
                                int distance_row,
                                const distance_table& distance,
                                const reservation_table& reservations,
                                int congestion_weight,
                                const timer& deadline) {
            astar_outcome result;
            const int start_distance = distance.get_by_row(distance_row, start);
            if (start_distance == inf_distance) return result;
            int goal_hold_time = 0;
            for (int time = max_time; time >= 0; --time) {
                const int goal_owner = reservations.at(time, goal);
                if (goal_owner != -1 && goal_owner != agent) {
                    goal_hold_time = time + 1;
                    break;
                }
            }
            if (reservations.external != nullptr) {
                for (int time = goal_hold_time; time < max_time; ++time) {
                    const int global_time = reservations.external_time_offset + time;
                    if (global_time >= reservations.external->horizon) break;
                    if (reservations.external->move_is_blocked(global_time, goal, goal)) {
                        goal_hold_time = time + 1;
                    }
                }
            }

            if (goal_hold_time > max_time) return result;

            ++stamp;
            if (stamp == std::numeric_limits<int>::max()) {
                std::fill(seen.begin(), seen.end(), 0);
                stamp = 1;
            }

            const auto open_key = [](int estimated_total, int secondary_cost) {
                const std::uint32_t primary =
                    std::bit_cast<std::uint32_t>(estimated_total) ^ 0x80000000U;
                const std::uint32_t secondary =
                    std::bit_cast<std::uint32_t>(secondary_cost) ^ 0x80000000U;
                return (std::uint64_t(primary) << 32) | std::uint64_t(secondary);

            };
            open_storage.clear();
            auto push_open = [&](open_node node) {
                open_storage.push_back(node);
                std::push_heap(open_storage.begin(), open_storage.end());
            };
            auto pop_open = [&] {
                std::pop_heap(open_storage.begin(), open_storage.end());
                open_node node = open_storage.back();
                open_storage.pop_back();
                return node;
            };
            const int start_state = start;
            seen[start_state] = stamp;
            best_secondary[start_state] = 0;
            parent_vertex[start_state] = -1;
            push_open({open_key(std::max(start_distance, goal_hold_time), 0), 0, start});

            int goal_state = -1;
            while (!open_storage.empty()) {
                const open_node current = pop_open();
                const int state = current.time * vertex_count + current.vertex;
                const int current_secondary =
                    std::bit_cast<int>(std::uint32_t(current.key) ^ 0x80000000U);
                if (seen[state] != stamp || best_secondary[state] != current_secondary) continue;

                ++result.expanded_states;
                if ((result.expanded_states & 4095LL) == 0 && deadline.expired()) return result;

                if (current.vertex == goal && current.time >= goal_hold_time) {
                    goal_state = state;
                    break;
                }
                if (current.time == max_time) continue;

                const int next_time = current.time + 1;
                auto relax = [&](int to) {
                    if (reservations.external != nullptr && reservations.external_time_offset + current.time < reservations.external->horizon &&
                        reservations.external->move_is_blocked(reservations.external_time_offset + current.time, current.vertex, to)) {
                        return;
                    }
                    const int next_owner = reservations.at(current.time + 1, to);
                    if (next_owner != -1 && next_owner != agent) return;

                    const int opposite_agent = reservations.at(current.time, to);
                    const bool moving_owner = opposite_agent >= 0 || opposite_agent <= -3;
                    if (moving_owner && opposite_agent != agent &&
                        reservations.at(current.time + 1, current.vertex) == opposite_agent) {
                        return;
                    }

                    const int heuristic = distance.get_by_row(distance_row, to);
                    if (heuristic == inf_distance || next_time + heuristic > max_time) return;

                    const int state_to = next_time * vertex_count + to;
                    int secondary = current_secondary;
                    secondary += congestion_weight * reservations.visit_count[to];
                    if (to == current.vertex) ++secondary;
                    if (seen[state_to] == stamp && best_secondary[state_to] <= secondary) return;

                    seen[state_to] = stamp;
                    best_secondary[state_to] = secondary;
                    parent_vertex[state_to] = current.vertex;
                    push_open({open_key(std::max(next_time + heuristic, goal_hold_time),
                                        secondary),
                               next_time,
                               to});
                };

                for (const int to : graph.neighbors(current.vertex)) relax(to);
                relax(current.vertex);
            }

            if (goal_state == -1) return result;
            const int arrival = goal_state / vertex_count;
            result.path.resize(arrival + 1);
            int vertex = goal;
            for (int time = arrival; time >= 0; --time) {
                result.path[time] = vertex;
                if (time > 0) {
                    const int state = time * vertex_count + vertex;
                    vertex = parent_vertex[state];
                }
            }
            result.success = true;
            return result;
        }
    };

    struct pibt_workspace {
        struct pibt_candidate {
            int distance;
            int penalty;
            std::uint64_t random;
            int vertex;
        };
        std::vector<int> occupant;
        std::vector<int> order;
        std::vector<std::uint64_t> random_key;
        std::vector<int> next_positions;
        std::vector<unsigned char> next_occupied;
        std::vector<unsigned char> visiting;
        std::vector<int> assigned;
        std::vector<std::vector<pibt_candidate>> candidates;

        pibt_workspace(int vertex_count, int agent_count)
            : occupant(vertex_count),
              order(agent_count),
              random_key(agent_count),
              next_positions(agent_count),
              next_occupied(vertex_count),
              visiting(agent_count),
              candidates(agent_count) {
            assigned.reserve(agent_count);
        }

        bool step(const mapf_graph& graph,
                              const distance_table& distance,
                              std::span<const int> goal_rows,
                              std::span<const int> goals,
                              std::vector<int>& positions,
                              std::vector<int>& previous_positions,
                              std::vector<int>& ages,
                              std::span<const unsigned char> priority_class,
                              std::mt19937_64& rng) {
            const int agent_count = int(positions.size());
            assert(priority_class.empty() || int(priority_class.size()) == agent_count);
            assert(int(occupant.size()) == graph.size());
            assert(int(order.size()) == agent_count);
            std::fill(occupant.begin(), occupant.end(), -1);
            for (int agent = 0; agent < agent_count; ++agent) {
                occupant[positions[agent]] = agent;
            }

            std::iota(order.begin(), order.end(), 0);
            for (int agent = 0; agent < agent_count; ++agent) {
                if (positions[agent] == goals[agent]) {
                    ages[agent] = 0;
                } else if (ages[agent] < std::numeric_limits<int>::max() / 2) {
                    ++ages[agent];
                }
                random_key[agent] = rng();
            }
            std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
                if (!priority_class.empty() && priority_class[lhs] != priority_class[rhs]) {
                    return priority_class[lhs] > priority_class[rhs];
                }
                const int lhs_distance = distance.get_by_row(goal_rows[lhs], positions[lhs]);
                const int rhs_distance = distance.get_by_row(goal_rows[rhs], positions[rhs]);
                const bool both_carrying = !priority_class.empty() && priority_class[lhs] != 0;
                if (ages[lhs] != ages[rhs]) return ages[lhs] > ages[rhs];
                if (both_carrying && lhs_distance != rhs_distance) {
                    return lhs_distance < rhs_distance;
                }
                if (lhs_distance != rhs_distance) return lhs_distance > rhs_distance;
                return random_key[lhs] < random_key[rhs];
            });

            std::fill(next_positions.begin(), next_positions.end(), -1);
            std::fill(next_occupied.begin(), next_occupied.end(), 0);
            std::fill(visiting.begin(), visiting.end(), 0);
            assigned.clear();

            auto rollback = [&](std::size_t marker) {
                while (assigned.size() > marker) {
                    const int agent = assigned.back();
                    assigned.pop_back();
                    next_occupied[next_positions[agent]] = 0;
                    next_positions[agent] = -1;
                }
            };

            auto decide = [&](auto&& self, int agent, int forbidden_vertex) -> bool {
                if (next_positions[agent] != -1) return true;
                if (visiting[agent]) return false;
                visiting[agent] = 1;

                auto& choices = candidates[agent];
                choices.clear();
                choices.reserve(std::size_t(graph.degree(positions[agent])) + 1);
                auto add_candidate = [&](int vertex) {
                    const int goal_distance = distance.get_by_row(goal_rows[agent], vertex);
                    if (goal_distance == inf_distance) return;
                    int penalty = 0;
                    if (vertex == previous_positions[agent] && vertex != positions[agent]) penalty += 2;
                    if (vertex == positions[agent] && positions[agent] != goals[agent]) ++penalty;
                    choices.push_back({goal_distance, penalty, rng(), vertex});
                };
                for (const int to : graph.neighbors(positions[agent])) add_candidate(to);
                add_candidate(positions[agent]);
                const auto less = [](const pibt_candidate& lhs, const pibt_candidate& rhs) {
                    if (lhs.distance != rhs.distance) return lhs.distance < rhs.distance;
                    if (lhs.penalty != rhs.penalty) return lhs.penalty < rhs.penalty;
                    return lhs.random < rhs.random;
                };
                // 必要になった候補だけ選ぶ。乱数キーは候補列の構築時に全て生成する
                for (auto next = choices.begin(); next != choices.end(); ++next) {
                    std::iter_swap(next, std::min_element(next, choices.end(), less));
                    const pibt_candidate& choice = *next;
                    const int to = choice.vertex;
                    if (to == forbidden_vertex || next_occupied[to]) continue;
                    const std::size_t marker = assigned.size();
                    next_positions[agent] = to;
                    next_occupied[to] = 1;
                    assigned.push_back(agent);

                    const int blocking_agent = occupant[to];
                    bool accepted = blocking_agent == -1 || blocking_agent == agent;
                    if (!accepted && next_positions[blocking_agent] != -1) {
                        accepted = next_positions[blocking_agent] != positions[agent];
                    } else if (!accepted) {
                        accepted = self(self, blocking_agent, positions[agent]);
                    }
                    if (accepted) {
                        visiting[agent] = 0;
                        return true;
                    }
                    rollback(marker);
                }

                visiting[agent] = 0;
                return false;
            };

            for (const int agent : order) {
                if (next_positions[agent] != -1) continue;
                if (!decide(decide, agent, -1)) return false;
            }

            previous_positions = positions;
            positions.swap(next_positions);
            return true;
        }
    };

    static void compute_costs(mapf_result& result) {
        result.sum_of_costs = 0;
        result.makespan = 0;
        for (const auto& path : result.paths) {
            const int cost = int(path.size()) - 1;
            result.sum_of_costs += cost;
            result.makespan = std::max(result.makespan, cost);
        }
    }

    static bool better_result(const mapf_result& lhs,
                              const mapf_result& rhs,
                              mapf_objective objective) {
        if (lhs.success != rhs.success) return lhs.success;
        if (!lhs.success) return false;
        if (objective == mapf_objective::sum_of_costs) {
            if (lhs.sum_of_costs != rhs.sum_of_costs) return lhs.sum_of_costs < rhs.sum_of_costs;
            return lhs.makespan < rhs.makespan;
        }
        if (lhs.makespan != rhs.makespan) return lhs.makespan < rhs.makespan;
        return lhs.sum_of_costs < rhs.sum_of_costs;
    }

    static int path_position(std::span<const int> path, int time) {
        return path[std::min(time, int(path.size()) - 1)];
    }

    static bool validate_paths(const mapf_graph& graph,
                               std::span<const int> starts,
                               const std::vector<std::vector<int>>& paths,
                               const mapf_reservations* external = nullptr) {
        const int agent_count = int(starts.size());
        if (agent_count == 0 || graph.size() <= 0 || int(paths.size()) != agent_count) return false;
        if (external != nullptr &&
            (!external->valid() || external->vertex_count != graph.size())) {
            return false;
        }

        int horizon = external == nullptr ? 0 : external->horizon;
        for (int agent = 0; agent < agent_count; ++agent) {
            const auto& path = paths[agent];
            if (path.empty() || path.front() != starts[agent]) return false;
            horizon = std::max(horizon, int(path.size()) - 1);
            for (int time = 0; time < int(path.size()); ++time) {
                const int vertex = path[time];
                if (vertex < 0 || vertex >= graph.size()) return false;
                if (time == 0 || path[time - 1] == vertex) continue;
                const auto adjacent = graph.neighbors(path[time - 1]);
                if (!std::binary_search(adjacent.begin(), adjacent.end(), vertex)) return false;
            }
        }
        std::vector<int> previous(agent_count);
        std::vector<int> current(agent_count);
        std::vector<int> current_owner(graph.size(), -1);
        for (int time = 0; time <= horizon; ++time) {
            std::fill(current_owner.begin(), current_owner.end(), -1);
            for (int agent = 0; agent < agent_count; ++agent) {
                const int vertex = path_position(paths[agent], time);
                current[agent] = vertex;
                if (current_owner[vertex] != -1) return false;
                current_owner[vertex] = agent;
                if (external != nullptr && time <= external->horizon &&
                    external->at(time, vertex) != -1) {
                    return false;
                }
                if (time > 0 && external != nullptr && time <= external->horizon) {
                    if (external->move_is_blocked(time - 1, previous[agent], vertex)) return false;
                    const int opposite_owner = external->at(time - 1, vertex);
                    if (opposite_owner <= -3 &&
                        external->at(time, previous[agent]) == opposite_owner) {
                        return false;
                    }
                }
            }
            if (time > 0) {
                for (int agent = 0; agent < agent_count; ++agent) {
                    const int other = current_owner[previous[agent]];
                    if (other != -1 && other != agent && previous[other] == current[agent]) {
                        return false;
                    }
                }
            }
            previous.swap(current);
        }
        return true;
    }

    static bool validate_result(const mapf_graph& graph,
                                std::span<const int> starts,
                                std::span<const int> goals,
                                const mapf_result& result,
                                const mapf_reservations* external = nullptr) {
        const int agent_count = int(starts.size());
        if (!result.success || int(goals.size()) != agent_count ||
            int(result.paths.size()) != agent_count) {
            return false;
        }
        for (int agent = 0; agent < agent_count; ++agent) {
            if (result.paths[agent].empty() || result.paths[agent].back() != goals[agent]) {
                return false;
            }
        }
        return validate_paths(graph, starts, result.paths, external);
    }

    distance_table distance;
    astar_workspace astar;
    reservation_table reservations;
    std::vector<int> goal_rows;
    std::vector<unsigned char> selection_mask;

    void improve_with_lns(const mapf_graph& graph,
                          std::span<const int> starts,
                          std::span<const int> goals,
                          int max_time,
                          const mapf_options& options,
                          const timer& deadline,
                          std::mt19937_64& rng,
                          mapf_result& best) {
        if (!best.success || options.lns_iterations <= 0 || options.lns_size <= 0) return;

        const int agent_count = int(starts.size());
        astar.prepare(graph.size(), max_time);
        reservations.prepare(graph.size(), max_time);

        auto choose_agents = [&](const mapf_result& current, int iteration) {
            const int neighborhood_size = std::clamp(options.lns_size, 1, agent_count);
            std::vector<int> agents(agent_count);
            std::iota(agents.begin(), agents.end(), 0);

            if ((iteration & 3) == 3) {
                std::shuffle(agents.begin(), agents.end(), rng);
                agents.resize(neighborhood_size);
                return agents;
            }

            std::vector<int> delay(agent_count);
            for (int agent = 0; agent < agent_count; ++agent) {
                delay[agent] = int(current.paths[agent].size()) - 1 -
                               distance.get_by_row(goal_rows[agent], starts[agent]);
            }
            const int seed = iteration % 3 == 0
                                 ? int(std::max_element(delay.begin(), delay.end()) - delay.begin())
                                 : int(rng() % std::uint64_t(agent_count));

            std::vector<int> marked(graph.size(), 0);
            for (const int vertex : current.paths[seed]) {
                marked[vertex] = 2;
                for (const int to : graph.neighbors(vertex)) {
                    if (marked[to] == 0) marked[to] = 1;
                }
            }

            std::vector<int> relatedness(agent_count, 0);
            for (int agent = 0; agent < agent_count; ++agent) {
                for (const int vertex : current.paths[agent]) {
                    relatedness[agent] += marked[vertex];
                }
            }

            std::stable_sort(agents.begin(), agents.end(), [&](int lhs, int rhs) {
                if (relatedness[lhs] != relatedness[rhs]) return relatedness[lhs] > relatedness[rhs];
                return delay[lhs] > delay[rhs];
            });
            agents.resize(neighborhood_size);
            return agents;
        };

        for (int iteration = 0; iteration < options.lns_iterations; ++iteration) {
            if (deadline.expired()) break;
            const std::vector<int> selected = choose_agents(best, iteration);
            std::vector<unsigned char> is_selected(agent_count, 0);
            for (const int agent : selected) is_selected[agent] = 1;

            if (!reservations.reset(starts)) return;
            bool fixed_paths_valid = true;
            for (int agent = 0; agent < agent_count; ++agent) {
                if (!is_selected[agent] &&
                    !reservations.reserve_path(agent, best.paths[agent])) {
                    fixed_paths_valid = false;
                    break;
                }
            }
            if (!fixed_paths_valid) continue;

            std::vector<int> order = selected;
            std::stable_sort(order.begin(), order.end(), [&](int lhs, int rhs) {
                const int lhs_delay = int(best.paths[lhs].size()) - 1 -
                               distance.get_by_row(goal_rows[lhs], starts[lhs]);
                const int rhs_delay = int(best.paths[rhs].size()) - 1 -
                               distance.get_by_row(goal_rows[rhs], starts[rhs]);
                return lhs_delay > rhs_delay;
            });
            if ((iteration & 1) != 0) std::shuffle(order.begin(), order.end(), rng);

            mapf_result candidate = best;
            bool success = true;
            for (const int agent : order) {
                auto path = astar.find_path(graph,
                                                  agent,
                                                  starts[agent],
                                                  goals[agent],
                                                  goal_rows[agent],
                                                  distance,
                                                  reservations,
                                                  options.congestion_weight,
                                                  deadline);
                best.expanded_states += path.expanded_states;
                if (!path.success) {
                    success = false;
                    break;
                }
                candidate.paths[agent] = std::move(path.path);
                if (!reservations.reserve_path(agent, candidate.paths[agent])) {
                    success = false;
                    break;
                }
            }
            if (!success) continue;
            compute_costs(candidate);
            if (better_result(candidate, best, options.objective)) {
                const long long expanded = best.expanded_states;
                const int attempts = best.attempts;
                const int improvements = best.lns_improvements + 1;
                best = std::move(candidate);
                best.expanded_states = expanded;
                best.attempts = attempts;
                best.lns_improvements = improvements;
            }
        }
    }

public:
    mapf_workspace() = default;
    mapf_workspace(const mapf_workspace&) = delete;
    mapf_workspace& operator=(const mapf_workspace&) = delete;
    // 探索領域を移し、移動元を再利用可能な空の状態に戻す。O(1)
    mapf_workspace(mapf_workspace&& other) noexcept { *this = std::move(other); }

    // 探索領域を移し、移動元を再利用可能な空の状態に戻す。O(1)
    mapf_workspace& operator=(mapf_workspace&& other) noexcept {
        if (this != &other) {
            // 配列と対応する管理情報をまとめて移す
            distance = std::move(other.distance);
            astar = std::move(other.astar);
            reservations = std::move(other.reservations);
            goal_rows = std::move(other.goal_rows);
            selection_mask = std::move(other.selection_mask);
            // 移動元に残るグラフ識別子・確保済みサイズも初期化する
            other.clear();
        }
        return *this;
    }

    // 距離表と探索領域を破棄して初期状態へ戻す。O(1)
    void clear() {
        distance = {};
        astar = {};
        reservations = {};
        goal_rows.clear();
        selection_mask.clear();
    }

    // 未登録の終点への距離表を追加する。O(V + |targets| + U(V+E))、Uは新規終点数
    bool prepare(const mapf_graph& graph, std::span<const int> targets) {
        if (graph.size() <= 0) return false;
        if (distance.graph_identity != &graph || distance.vertex_count != graph.size()) {
            distance.reset(graph);
        }
        for (const int target : targets) {
            if (target < 0 || target >= graph.size()) return false;
        }
        distance.add_targets(graph, targets);
        return true;
    }

    // 計算済みの終点数を返す。O(1)
    int cached_target_count() const { return int(distance.row_targets.size()); }

};

// 経路と報告された完了タスクを検証する。O((V+K)H + KH log V + Q log Q)、Kはagent数、Qはtask数、Hは経路長
inline bool validate_mapd_result(const mapf_graph& graph,
                                 std::span<const mapd_agent> agents,
                                 std::span<const mapd_task> tasks,
                                 const mapd_result& result) {
    const int agent_count = int(agents.size());
    const int task_count = int(tasks.size());
    if (agent_count == 0 || graph.size() <= 0 ||
        int(result.paths.size()) != agent_count ||
        int(result.task_agent.size()) != task_count ||
        int(result.task_pickup_time.size()) != task_count ||
        int(result.task_completion_time.size()) != task_count) {
        return false;
    }
    int horizon = -1;
    for (int agent = 0; agent < agent_count; ++agent) {
        const auto& path = result.paths[agent];
        if (path.empty() || path.front() != agents[agent].start) return false;
        if (horizon == -1) horizon = int(path.size()) - 1;
        if (int(path.size()) != horizon + 1) return false;
        for (int time = 0; time <= horizon; ++time) {
            const int vertex = path[time];
            if (vertex < 0 || vertex >= graph.size()) return false;
            if (time == 0 || path[time - 1] == vertex) continue;
            const auto adjacent = graph.neighbors(path[time - 1]);
            if (!std::binary_search(adjacent.begin(), adjacent.end(), vertex)) return false;
        }
    }

    std::vector<int> previous(agent_count);
    std::vector<int> current(agent_count);
    std::vector<int> owner(graph.size(), -1);
    long long counted_moves = 0;
    for (int time = 0; time <= horizon; ++time) {
        std::fill(owner.begin(), owner.end(), -1);
        for (int agent = 0; agent < agent_count; ++agent) {
            current[agent] = result.paths[agent][time];
            if (owner[current[agent]] != -1) return false;
            owner[current[agent]] = agent;
            if (time > 0 && current[agent] != previous[agent]) ++counted_moves;
        }
        if (time > 0) {
            for (int agent = 0; agent < agent_count; ++agent) {
                const int other = owner[previous[agent]];
                if (other != -1 && other != agent && previous[other] == current[agent]) return false;
            }
        }
        previous.swap(current);
    }
    if (counted_moves != result.total_moves) return false;

    int counted_completed = 0;
    long long counted_flow_time = 0;
    std::vector<std::vector<std::pair<int, int>>> intervals(agent_count);
    for (int task = 0; task < task_count; ++task) {
        const int completion = result.task_completion_time[task];
        if (completion == -1) continue;
        const int pickup = result.task_pickup_time[task];
        const int agent = result.task_agent[task];
        if (agent < 0 || agent >= agent_count || pickup < tasks[task].release_time ||
            pickup > completion || completion > horizon) {
            return false;
        }
        if (result.paths[agent][pickup] != tasks[task].pickup ||
            result.paths[agent][completion] != tasks[task].delivery) {
            return false;
        }
        intervals[agent].push_back({pickup, completion});
        ++counted_completed;
        counted_flow_time += completion - tasks[task].release_time;
    }
    for (auto& xs : intervals) {
        std::sort(xs.begin(), xs.end());
        for (int index = 1; index < int(xs.size()); ++index) {
            if (xs[index - 1].second > xs[index].first) return false;
        }
    }
    return counted_completed == result.completed_tasks &&
           counted_flow_time == result.sum_flow_time;
}

inline mapd_result solve_mapd(const mapf_graph& graph,
                              std::span<const mapd_agent> agents,
                              std::span<const mapd_task> tasks,
                              const mapd_options& options = {}) {
    if (options.assignment == mapd_assignment_strategy::automatic) {
        const std::array<mapd_assignment_strategy, 8> strategies = {
            mapd_assignment_strategy::greedy,
            mapd_assignment_strategy::greedy_unique_endpoints,
            mapd_assignment_strategy::min_cost_flow,
            mapd_assignment_strategy::greedy,
            mapd_assignment_strategy::greedy_unique_endpoints,
            mapd_assignment_strategy::greedy,
            mapd_assignment_strategy::min_cost_flow,
            mapd_assignment_strategy::greedy,
        };
        const auto begin = std::chrono::steady_clock::now();
        mapd_result best;
        const int runs = std::clamp(options.portfolio_runs, 1, int(strategies.size()));
        for (int index = 0; index < runs; ++index) {
            if (options.deadline != nullptr && options.deadline->expired()) break;
            mapd_options candidate_options = options;
            candidate_options.assignment = strategies[index];
            candidate_options.seed = options.seed +
                                     std::uint64_t(index) * 0x9e3779b97f4a7c15ULL;
            if (options.time_limit_ms >= 0.0) {
                const double elapsed = std::chrono::duration<double, std::milli>(
                                           std::chrono::steady_clock::now() - begin)
                                           .count();
                const double remaining = options.time_limit_ms - elapsed;
                if (remaining <= 0.0) break;
                candidate_options.time_limit_ms = remaining;
            }
            mapd_result candidate = solve_mapd(graph, agents, tasks, candidate_options);
            const bool better = candidate.valid &&
                                (!best.valid ||
                                 candidate.completed_tasks > best.completed_tasks ||
                                 (candidate.completed_tasks == best.completed_tasks &&
                                  candidate.sum_flow_time < best.sum_flow_time) ||
                                 (candidate.completed_tasks == best.completed_tasks &&
                                  candidate.sum_flow_time == best.sum_flow_time &&
                                  candidate.total_moves < best.total_moves));
            if (better) best = std::move(candidate);
        }
        return best;
    }

    mapd_result result;
    const int agent_count = int(agents.size());
    const int task_count = int(tasks.size());
    if (agent_count == 0 || graph.size() == 0 || options.max_time < 0) return result;

    std::vector<int> starts(agent_count);
    std::vector<int> start_owner(graph.size(), -1);
    std::vector<int> targets;
    targets.reserve(std::size_t(agent_count) + std::size_t(task_count) * 2);
    for (int agent = 0; agent < agent_count; ++agent) {
        starts[agent] = agents[agent].start;
        if (starts[agent] < 0 || starts[agent] >= graph.size() ||
            start_owner[starts[agent]] != -1) {
            return result;
        }
        start_owner[starts[agent]] = agent;
        targets.push_back(starts[agent]);
    }
    for (const mapd_task& task : tasks) {
        if (task.pickup < 0 || task.pickup >= graph.size() ||
            task.delivery < 0 || task.delivery >= graph.size() || task.release_time < 0) {
            return result;
        }
        targets.push_back(task.pickup);
        targets.push_back(task.delivery);
    }

    mapf_workspace::timer deadline(options.time_limit_ms, options.deadline);
    mapf_workspace::distance_table distance(graph, targets);
    const mapd_assignment_strategy assignment_strategy = options.assignment;
    std::vector<int> positions = starts;
    std::vector<int> previous_positions = starts;
    std::vector<int> parking_goal = starts;
    std::vector<int> assigned_task(agent_count, -1);
    std::vector<unsigned char> carrying(agent_count, 0);
    std::vector<unsigned char> task_status(task_count, 0);
    std::vector<int> ages(agent_count, 0);
    mapf_workspace::pibt_workspace pibt_state(graph.size(), agent_count);
    std::mt19937_64 rng(options.seed);

    result.paths.resize(agent_count);
    for (int agent = 0; agent < agent_count; ++agent) result.paths[agent].push_back(starts[agent]);
    result.task_agent.assign(task_count, -1);
    result.task_pickup_time.assign(task_count, -1);
    result.task_completion_time.assign(task_count, -1);

    auto process_arrivals = [&](int time) {
        for (int agent = 0; agent < agent_count; ++agent) {
            const int task = assigned_task[agent];
            if (task == -1) continue;
            if (!carrying[agent] && positions[agent] == tasks[task].pickup) {
                carrying[agent] = 1;
                if (result.task_pickup_time[task] == -1) result.task_pickup_time[task] = time;
            }
            if (carrying[agent] && positions[agent] == tasks[task].delivery) {
                result.task_completion_time[task] = time;
                task_status[task] = 2;
                assigned_task[agent] = -1;
                carrying[agent] = 0;
                parking_goal[agent] = positions[agent];
                ++result.completed_tasks;
                result.sum_flow_time += time - tasks[task].release_time;
            }
        }
    };

    struct pair_cost {
        int cost;
        int agent;
        int task;
    };
    auto task_cost = [&](int agent_vertex, const mapd_task& task, int remaining_time) {
        const int to_pickup = distance.get_by_row(distance.row_of(task.pickup), agent_vertex);
        const int to_delivery = distance.get_by_row(distance.row_of(task.delivery), task.pickup);
        if (to_pickup == mapf_workspace::inf_distance || to_delivery == mapf_workspace::inf_distance) return mapf_workspace::inf_distance;
        // 衝突を無視した最短時間でも間に合わない組は割り当てない。
        if (to_pickup + to_delivery > remaining_time) return mapf_workspace::inf_distance;
        // 空荷の移動を重めに評価する。完了可能性の下界とは分けて扱う。
        return 2 * to_pickup + to_delivery;
    };

    for (int time = 0; time < options.max_time; ++time) {
        process_arrivals(time);
        // 期限切れ、または全タスク完了後は現在位置で残り時刻を埋める。
        if (deadline.expired() ||
            (result.completed_tasks == task_count && positions == parking_goal)) {
            for (int agent = 0; agent < agent_count; ++agent) {
                result.paths[agent].resize(std::size_t(options.max_time) + 1, positions[agent]);
            }
            break;
        }

        std::vector<int> idle_agents;
        std::vector<int> available_tasks;
        for (int agent = 0; agent < agent_count; ++agent) {
            if (assigned_task[agent] == -1) idle_agents.push_back(agent);
        }
        for (int task = 0; task < task_count; ++task) {
            if (task_status[task] == 0 && tasks[task].release_time <= time) {
                available_tasks.push_back(task);
            }
        }
        if (!idle_agents.empty() && !available_tasks.empty()) {
            if (assignment_strategy == mapd_assignment_strategy::greedy) {
                const int remaining_time = options.max_time - time;
                auto assign = [&] {
                    std::vector<pair_cost> pairs;
                    pairs.reserve(std::size_t(idle_agents.size()) * std::size_t(available_tasks.size()));
                    const auto less = [](const pair_cost& lhs, const pair_cost& rhs) {
                        return std::tie(lhs.cost, lhs.task, lhs.agent) < std::tie(rhs.cost, rhs.task, rhs.agent);
                    };
                    for (const int agent : idle_agents) {
                        const std::size_t begin = pairs.size();
                        for (const int task : available_tasks) {
                            const int cost = task_cost(positions[agent], tasks[task], remaining_time);
                            if (cost != mapf_workspace::inf_distance) pairs.push_back({cost, agent, task});
                        }
                        // 他のidle agentが先に使えるtaskは高々A-1個なので、各agentの上位A個で十分
                        const std::size_t keep = std::min(idle_agents.size(), pairs.size() - begin);
                        if (begin + keep < pairs.size()) {
                            std::nth_element(pairs.begin() + std::ptrdiff_t(begin),
                                             pairs.begin() + std::ptrdiff_t(begin + keep), pairs.end(), less);
                            pairs.resize(begin + keep);
                        }
                    }
                    std::sort(pairs.begin(), pairs.end(), [](const pair_cost& lhs, const pair_cost& rhs) {
                        if (lhs.cost != rhs.cost) return lhs.cost < rhs.cost;
                        if (lhs.task != rhs.task) return lhs.task < rhs.task;
                        return lhs.agent < rhs.agent;
                    });
                    for (const pair_cost& pair : pairs) {
                        if (assigned_task[pair.agent] != -1 || task_status[pair.task] != 0) continue;
                        assigned_task[pair.agent] = pair.task;
                        task_status[pair.task] = 1;
                    }
                };
                assign();
            } else if (assignment_strategy ==
                       mapd_assignment_strategy::greedy_unique_endpoints) {
                std::vector<unsigned char> endpoint_used(graph.size(), 0);
                for (int agent = 0; agent < agent_count; ++agent) {
                    const int task = assigned_task[agent];
                    if (task == -1) continue;
                    endpoint_used[tasks[task].pickup] = 1;
                    endpoint_used[tasks[task].delivery] = 1;
                }
                const int remaining_time = options.max_time - time;
                auto assign = [&] {
                    std::vector<pair_cost> pairs;
                    pairs.reserve(std::size_t(idle_agents.size()) * std::size_t(available_tasks.size()));
                    for (const int agent : idle_agents) {
                        for (const int task : available_tasks) {
                            const int cost = task_cost(positions[agent], tasks[task], remaining_time);
                            if (cost != mapf_workspace::inf_distance) pairs.push_back({cost, agent, task});
                        }
                    }
                    std::sort(pairs.begin(), pairs.end(), [](const pair_cost& lhs, const pair_cost& rhs) {
                        if (lhs.cost != rhs.cost) return lhs.cost < rhs.cost;
                        if (lhs.task != rhs.task) return lhs.task < rhs.task;
                        return lhs.agent < rhs.agent;
                    });

                    for (int pass = 0; pass < 2; ++pass) {
                        for (const pair_cost& pair : pairs) {
                            if (assigned_task[pair.agent] != -1 || task_status[pair.task] != 0) continue;
                            const mapd_task& task = tasks[pair.task];
                            const bool conflicts = endpoint_used[task.pickup] || endpoint_used[task.delivery];
                            if (pass == 0 && conflicts) continue;
                            assigned_task[pair.agent] = pair.task;
                            task_status[pair.task] = 1;
                            endpoint_used[task.pickup] = 1;
                            endpoint_used[task.delivery] = 1;
                        }
                    }
                };
                assign();
            } else {
                const int remaining_time = options.max_time - time;
                auto assign = [&] {
                    const int idle_count = int(idle_agents.size());
                    const int available_count = int(available_tasks.size());
                    const int source = 0;
                    const int agent_base = 1;
                    const int task_base = agent_base + idle_count;
                    const int sink = task_base + available_count;
                    atcoder::mcf_graph<int, int> flow_graph(sink + 1);
                    for (int index = 0; index < idle_count; ++index) {
                        flow_graph.add_edge(source, agent_base + index, 1, 0);
                    }
                    for (int index = 0; index < available_count; ++index) {
                        flow_graph.add_edge(task_base + index, sink, 1, 0);
                    }

                    struct assignment_edge {
                        int edge_id;
                        int agent;
                        int task;
                    };
                    std::vector<assignment_edge> assignment_edges;
                    assignment_edges.reserve(std::size_t(idle_count) * std::size_t(available_count));
                    for (int agent_index = 0; agent_index < idle_count; ++agent_index) {
                        const int agent = idle_agents[agent_index];
                        for (int task_index = 0; task_index < available_count; ++task_index) {
                            const int task = available_tasks[task_index];
                            const int cost = task_cost(positions[agent], tasks[task], remaining_time);
                            if (cost == mapf_workspace::inf_distance) continue;
                            const int edge_id = flow_graph.add_edge(
                                agent_base + agent_index, task_base + task_index, 1, cost);
                            assignment_edges.push_back({edge_id, agent, task});
                        }
                    }

                    flow_graph.flow(source, sink, std::min(idle_count, available_count));
                    for (const assignment_edge& assignment : assignment_edges) {
                        if (flow_graph.get_edge(assignment.edge_id).flow == 0) continue;
                        assigned_task[assignment.agent] = assignment.task;
                        task_status[assignment.task] = 1;
                    }
                };
                assign();
            }
            for (const int agent : idle_agents) {
                const int task = assigned_task[agent];
                if (task != -1 && result.task_agent[task] == -1) {
                    result.task_agent[task] = agent;
                }
            }
            process_arrivals(time);
        }

        std::vector<int> goals(agent_count);
        std::vector<int> goal_rows(agent_count);
        for (int agent = 0; agent < agent_count; ++agent) {
            const int task = assigned_task[agent];
            if (task == -1) {
                goals[agent] = parking_goal[agent];
            } else {
                goals[agent] = carrying[agent] ? tasks[task].delivery : tasks[task].pickup;
            }
            goal_rows[agent] = distance.row_of(goals[agent]);
        }

        const std::vector<int> before = positions;
        const bool moved = pibt_state.step(graph,
            distance,
            goal_rows,
            goals,
            positions,
            previous_positions,
            ages,
            std::span<const unsigned char>(carrying),
            rng);
        if (!moved) {
            positions = before;
            previous_positions = before;
        }
        for (int agent = 0; agent < agent_count; ++agent) {
            result.paths[agent].push_back(positions[agent]);
            if (positions[agent] != before[agent]) ++result.total_moves;
        }
    }
    process_arrivals(options.max_time);
    result.valid = validate_mapd_result(graph, agents, tasks, result);
    return result;
}

inline bool validate_mapf_result(const mapf_graph& graph,
                                 std::span<const int> starts,
                                 std::span<const int> goals,
                                 const mapf_result& result) {
    return mapf_workspace::validate_result(graph, starts, goals, result);
}

inline bool validate_mapf_result(const mapf_graph& graph,
                                 std::span<const int> starts,
                                 std::span<const int> goals,
                                 const mapf_result& result,
                                 const mapf_reservations& external_reservations) {
    return mapf_workspace::validate_result(graph, starts, goals, result, &external_reservations);
}

inline mapf_result solve_mapf(const mapf_graph& graph,
                              std::span<const int> starts,
                              std::span<const int> goals,
                              mapf_workspace& workspace,
                              const mapf_options& options = {}) {
    mapf_result failure;
    const int agent_count = int(starts.size());
    if (agent_count == 0 || int(goals.size()) != agent_count || graph.size() == 0) {
        return failure;
    }

    std::vector<int> start_owner(graph.size(), -1);
    std::vector<int> goal_owner(graph.size(), -1);
    for (int agent = 0; agent < agent_count; ++agent) {
        if (starts[agent] < 0 || starts[agent] >= graph.size() ||
            goals[agent] < 0 || goals[agent] >= graph.size()) {
            return failure;
        }
        if (start_owner[starts[agent]] != -1 || goal_owner[goals[agent]] != -1) {
            return failure;
        }
        start_owner[starts[agent]] = agent;
        goal_owner[goals[agent]] = agent;
    }

    mapf_workspace::timer deadline(options.time_limit_ms, options.deadline);
    if (!workspace.prepare(graph, goals)) {
        failure.status = mapf_status::memory_limit;
        return failure;
    }
    const mapf_workspace::distance_table& distance = workspace.distance;
    workspace.goal_rows.resize(agent_count);
    int max_shortest_distance = 0;
    for (int agent = 0; agent < agent_count; ++agent) {
        workspace.goal_rows[agent] = distance.row_of(goals[agent]);
        const int shortest =
            distance.get_by_row(workspace.goal_rows[agent], starts[agent]);
        if (shortest == mapf_workspace::inf_distance) {
            failure.status = mapf_status::unreachable;
            return failure;
        }
        max_shortest_distance = std::max(max_shortest_distance, shortest);
    }

    int max_time = options.max_time;
    if (max_time < 0) {
        max_time = max_shortest_distance +
                   std::max({16, max_shortest_distance, agent_count});
    }
    if (max_time < max_shortest_distance) {
        failure.status = mapf_status::horizon_too_short;
        return failure;
    }
    if ((std::size_t(max_time) + 1) * std::size_t(graph.size()) >
        std::size_t(std::numeric_limits<int>::max())) {
        failure.status = mapf_status::memory_limit;
        return failure;
    }
    if (deadline.expired()) {
        failure.status = mapf_status::timeout;
        return failure;
    }

    std::mt19937_64 rng(options.seed);
    const auto& goal_rows = workspace.goal_rows;
    auto solve_prioritized = [&](const mapf_options& planning_options) {
        auto& reservations = workspace.reservations;
        workspace.astar.prepare(graph.size(), max_time);
        reservations.prepare(graph.size(), max_time);
        mapf_result best;
        long long total_expanded = 0;
        const int restarts = std::max(1, planning_options.prioritized_restarts);
        std::vector<int> goal_blocking(agent_count, 0);
        for (int goal_agent = 0; goal_agent < agent_count; ++goal_agent) {
            for (int path_agent = 0; path_agent < agent_count; ++path_agent) {
                if (goal_agent == path_agent) continue;
                const int via_goal =
                    distance.get_by_row(goal_rows[goal_agent], starts[path_agent]);
                const int goal_to_goal =
                    distance.get_by_row(goal_rows[path_agent], goals[goal_agent]);
                const int shortest =
                    distance.get_by_row(goal_rows[path_agent], starts[path_agent]);
                if (via_goal != mapf_workspace::inf_distance && goal_to_goal != mapf_workspace::inf_distance &&
                    via_goal + goal_to_goal == shortest) {
                    ++goal_blocking[goal_agent];
                }
            }
        }

        auto plan_order = [&](std::span<const int> order, int& failed_agent) -> mapf_result {
            failed_agent = -1;
            mapf_result result;
            result.paths.resize(agent_count);
            if (!reservations.reset(starts)) return result;

            for (const int agent : order) {
                auto path = workspace.astar.find_path(graph,
                                                         agent,
                                                         starts[agent],
                                                         goals[agent],
                                                         goal_rows[agent],
                                                         distance,
                                                         reservations,
                                                         planning_options.congestion_weight,
                                                         deadline);
                result.expanded_states += path.expanded_states;
                if (!path.success) {
                    failed_agent = agent;
                    return result;
                }
                result.paths[agent] = std::move(path.path);
                if (!reservations.reserve_path(agent, result.paths[agent])) {
                    failed_agent = agent;
                    return {};
                }
            }

            result.success = true;
            result.status = mapf_status::success;
            result.strategy_used = mapf_strategy::prioritized;
            mapf_workspace::compute_costs(result);
            return result;
        };

        int previous_failed_agent = -1;
        for (int attempt_index = 0; attempt_index < restarts; ++attempt_index) {
            if (attempt_index > 0 && deadline.expired()) break;
            std::vector<int> order(agent_count);
            std::iota(order.begin(), order.end(), 0);

            if (attempt_index == 0) {
                std::stable_sort(order.begin(), order.end(), [&](int lhs, int rhs) {
                    if (goal_blocking[lhs] != goal_blocking[rhs]) {
                        return goal_blocking[lhs] < goal_blocking[rhs];
                    }
                    return distance.get_by_row(goal_rows[lhs], starts[lhs]) >
                           distance.get_by_row(goal_rows[rhs], starts[rhs]);
                });
            } else if (attempt_index == 1) {
                std::stable_sort(order.begin(), order.end(), [&](int lhs, int rhs) {
                    const int lhs_distance = distance.get_by_row(goal_rows[lhs], starts[lhs]);
                    const int rhs_distance = distance.get_by_row(goal_rows[rhs], starts[rhs]);
                    if (lhs_distance != rhs_distance) return lhs_distance > rhs_distance;
                    return graph.degree(goals[lhs]) < graph.degree(goals[rhs]);
                });
            } else if (attempt_index == 2) {
                std::stable_sort(order.begin(), order.end(), [&](int lhs, int rhs) {
                    if (graph.degree(goals[lhs]) != graph.degree(goals[rhs])) {
                        return graph.degree(goals[lhs]) < graph.degree(goals[rhs]);
                    }
                    return distance.get_by_row(goal_rows[lhs], starts[lhs]) >
                           distance.get_by_row(goal_rows[rhs], starts[rhs]);
                });
            } else {
                std::shuffle(order.begin(), order.end(), rng);
            }
            if (previous_failed_agent != -1) {
                const auto failed_position =
                    std::find(order.begin(), order.end(), previous_failed_agent);
                std::rotate(order.begin(), failed_position, failed_position + 1);
            }
            int failed_agent = -1;
            mapf_result attempt = plan_order(order, failed_agent);
            previous_failed_agent = failed_agent;
            total_expanded += attempt.expanded_states;
            ++best.attempts;
            if (mapf_workspace::better_result(attempt, best, planning_options.objective)) {
                const int attempts = best.attempts;
                best = std::move(attempt);
                best.attempts = attempts;
            }
        }
        best.expanded_states = total_expanded;
        return best;
    };
    auto solve_pibt = [&](const mapf_options& planning_options) {
        mapf_result best;
        const int attempts = std::max(1, std::min(planning_options.prioritized_restarts, 8));
        mapf_workspace::pibt_workspace pibt_state(graph.size(), agent_count);

        for (int attempt = 0; attempt < attempts; ++attempt) {
            if (attempt > 0 && deadline.expired()) break;
            std::vector<int> positions(starts.begin(), starts.end());
            std::vector<int> previous_positions = positions;
            std::vector<int> ages(agent_count);
            mapf_result candidate;
            candidate.paths.resize(agent_count);
            for (int agent = 0; agent < agent_count; ++agent) {
                candidate.paths[agent].push_back(positions[agent]);
                ages[agent] = distance.get_by_row(goal_rows[agent], positions[agent]);
            }

            bool reached = std::equal(positions.begin(), positions.end(), goals.begin(), goals.end());
            for (int time = 0; time < max_time && !reached; ++time) {
                if ((time & 31) == 0 && deadline.expired()) break;
                if (!pibt_state.step(graph, distance, goal_rows, goals, positions, previous_positions, ages, std::span<const unsigned char>{}, rng)) {
                    break;
                }
                reached = true;
                for (int agent = 0; agent < agent_count; ++agent) {
                    candidate.paths[agent].push_back(positions[agent]);
                    reached = reached && positions[agent] == goals[agent];
                }
            }
            if (!reached) {
                ++best.attempts;
                continue;
            }

            for (int agent = 0; agent < agent_count; ++agent) {
                int last_non_goal = -1;
                for (int time = 0; time < int(candidate.paths[agent].size()); ++time) {
                    if (candidate.paths[agent][time] != goals[agent]) last_non_goal = time;
                }
                candidate.paths[agent].resize(last_non_goal + 2);
            }
            candidate.success = true;
            candidate.status = mapf_status::success;
            candidate.strategy_used = mapf_strategy::pibt;
            mapf_workspace::compute_costs(candidate);
            ++best.attempts;
            if (mapf_workspace::better_result(candidate, best, planning_options.objective)) {
                const int total_attempts = best.attempts;
                best = std::move(candidate);
                best.attempts = total_attempts;
            }
        }
        return best;
    };
    mapf_result result;
    if (options.strategy == mapf_strategy::prioritized) {
        result = solve_prioritized(options);
        workspace.improve_with_lns(graph, starts, goals, max_time, options, deadline, rng, result);
    } else if (options.strategy == mapf_strategy::pibt) {
        result = solve_pibt(options);
    } else {
        mapf_options construction_options = options;
        construction_options.congestion_weight = 0;
        construction_options.lns_iterations = 0;

        mapf_result construction_best;
        long long construction_expanded = 0;
        int construction_attempts = 0;
        auto consider_construction = [&](mapf_result candidate) {
            construction_expanded += candidate.expanded_states;
            construction_attempts += candidate.attempts;
            if (mapf_workspace::better_result(
                    candidate, construction_best, options.objective)) {
                construction_best = std::move(candidate);
            }
        };
        if (!deadline.expired()) {
            mapf_options pibt_options = construction_options;
            pibt_options.prioritized_restarts = 1;
            consider_construction(solve_pibt(pibt_options));
        }

        construction_options.prioritized_restarts = 2;
        mapf_result primary = solve_prioritized(construction_options);
        const bool primary_succeeded = primary.success;
        consider_construction(std::move(primary));

        if (!primary_succeeded && !deadline.expired()) {
            construction_options.congestion_weight = 1;
            construction_options.prioritized_restarts =
                std::max(1, options.prioritized_restarts);
            consider_construction(solve_prioritized(construction_options));
        }

        result = std::move(construction_best);
        result.expanded_states = construction_expanded;
        result.attempts = construction_attempts;
        mapf_options lns_options = options;
        lns_options.congestion_weight = 0;
        workspace.improve_with_lns(graph, starts, goals, max_time, lns_options, deadline, rng, result);
    }

    if (!result.success) {
        result.status =
            deadline.expired() ? mapf_status::timeout : mapf_status::search_failed;
        return result;
    }
    result.status = mapf_status::success;
    if (!validate_mapf_result(graph, starts, goals, result)) {
        failure.status = mapf_status::search_failed;
        failure.expanded_states = result.expanded_states;
        failure.attempts = result.attempts;
        return failure;
    }
    return result;
}

inline mapf_result solve_mapf(const mapf_graph& graph,
                              std::span<const int> starts,
                              std::span<const int> goals,
                              const mapf_options& options = {}) {
    mapf_workspace workspace;
    return solve_mapf(graph, starts, goals, workspace, options);
}

inline mapf_result improve_mapf(const mapf_graph& graph,
                                std::span<const int> starts,
                                std::span<const int> goals,
                                const mapf_result& initial_solution,
                                mapf_workspace& workspace,
                                const mapf_options& options = {}) {
    if (!validate_mapf_result(graph, starts, goals, initial_solution)) return {};
    mapf_result unchanged = initial_solution;
    unchanged.status = mapf_status::success;
    mapf_workspace::compute_costs(unchanged);

    mapf_workspace::timer deadline(options.time_limit_ms, options.deadline);
    if (!workspace.prepare(graph, goals)) return unchanged;
    const int agent_count = int(starts.size());
    workspace.goal_rows.resize(agent_count);
    int max_shortest_distance = 0;
    for (int agent = 0; agent < agent_count; ++agent) {
        workspace.goal_rows[agent] = workspace.distance.row_of(goals[agent]);
        max_shortest_distance = std::max(
            max_shortest_distance,
            workspace.distance.get_by_row(workspace.goal_rows[agent], starts[agent]));
    }
    int max_time = options.max_time;
    if (max_time < 0) {
        max_time = std::max(
            unchanged.makespan,
            max_shortest_distance +
                std::max({16, max_shortest_distance, agent_count}));
    }
    if (max_time < unchanged.makespan || max_time < max_shortest_distance ||
        (std::size_t(max_time) + 1) * std::size_t(graph.size()) >
            std::size_t(std::numeric_limits<int>::max()) ||
        deadline.expired()) {
        return unchanged;
    }

    std::mt19937_64 rng(options.seed);
    mapf_result result = unchanged;
    workspace.improve_with_lns(graph, starts, goals, max_time, options, deadline, rng, result);
    result.status = mapf_status::success;
    return validate_mapf_result(graph, starts, goals, result) ? result : unchanged;
}

inline mapf_result improve_mapf(const mapf_graph& graph,
                                std::span<const int> starts,
                                std::span<const int> goals,
                                const mapf_result& initial_solution,
                                const mapf_options& options = {}) {
    mapf_workspace workspace;
    return improve_mapf(graph, starts, goals, initial_solution, workspace, options);
}

inline mapf_result repair_mapf(const mapf_graph& graph,
                               std::span<const int> starts,
                               std::span<const int> goals,
                               const mapf_result& base,
                               const mapf_repair_request& request,
                               mapf_workspace& workspace,
                               const mapf_options& options = {}) {
    mapf_result failure;
    const int agent_count = int(starts.size());
    if (agent_count == 0 || int(goals.size()) != agent_count || graph.size() == 0 ||
        !base.success || int(base.paths.size()) != agent_count ||
        !mapf_workspace::validate_paths(graph, starts, base.paths)) {
        return failure;
    }
    if (request.external_reservations != nullptr &&
        (!request.external_reservations->valid() ||
         request.external_reservations->vertex_count != graph.size())) {
        return failure;
    }

    std::vector<int> goal_owner(graph.size(), -1);
    bool base_matches_goals = true;
    for (int agent = 0; agent < agent_count; ++agent) {
        if (goals[agent] < 0 || goals[agent] >= graph.size() ||
            goal_owner[goals[agent]] != -1) {
            return failure;
        }
        goal_owner[goals[agent]] = agent;
        base_matches_goals &= base.paths[agent].back() == goals[agent];
    }

    workspace.selection_mask.assign(agent_count, 0);
    for (const int agent : request.agents) {
        if (agent < 0 || agent >= agent_count || workspace.selection_mask[agent]) return failure;
        workspace.selection_mask[agent] = 1;
    }

    mapf_result unchanged = base;
    unchanged.status = mapf_status::success;
    mapf_workspace::compute_costs(unchanged);
    mapf_workspace::timer deadline(options.time_limit_ms, options.deadline);
    std::mt19937_64 rng(options.seed);

    auto plan_selected = [&](std::span<const int> plan_starts, std::span<const int> plan_goals,
                             const std::vector<std::vector<int>>& fixed_paths,
                             int max_time, int external_time_offset) {
        const auto& goal_rows = workspace.goal_rows;
        const auto& distance = workspace.distance;
        auto& astar = workspace.astar;
        auto& reservations = workspace.reservations;
        const auto selected = request.agents;
        const auto* external = request.external_reservations;
        mapf_result best;
        best.status = mapf_status::search_failed;
        astar.prepare(graph.size(), max_time);
        reservations.prepare(graph.size(), max_time);

        std::vector<unsigned char> is_selected(agent_count, 0);
        for (const int agent : selected) is_selected[agent] = 1;
        std::vector<int> base_order(selected.begin(), selected.end());
        std::stable_sort(base_order.begin(), base_order.end(), [&](int lhs, int rhs) {
            const int lhs_shortest = distance.get_by_row(goal_rows[lhs], plan_starts[lhs]);
            const int rhs_shortest = distance.get_by_row(goal_rows[rhs], plan_starts[rhs]);
            const int lhs_delay = int(fixed_paths[lhs].size()) - 1 - lhs_shortest;
            const int rhs_delay = int(fixed_paths[rhs].size()) - 1 - rhs_shortest;
            if (lhs_delay != rhs_delay) return lhs_delay > rhs_delay;
            return lhs_shortest > rhs_shortest;
        });

        long long total_expanded = 0;
        int previous_failed_agent = -1;
        const int requested_restarts = std::max(1, options.prioritized_restarts);
        int distinct_orders = 1;
        for (int count = 2; count <= int(selected.size()) &&
                            distinct_orders < requested_restarts;
             ++count) {
            if (distinct_orders > requested_restarts / count) {
                distinct_orders = requested_restarts;
            } else {
                distinct_orders *= count;
            }
        }
        const int restarts = std::min(requested_restarts, distinct_orders);
        std::vector<std::vector<int>> tried_orders;
        tried_orders.reserve(restarts);
        if (!reservations.reset(plan_starts, external, external_time_offset)) return best;
        for (int agent = 0; agent < agent_count; ++agent) {
            if (is_selected[agent]) continue;
            if (!reservations.reserve_path(agent, fixed_paths[agent])) return best;
        }
        std::vector<int> fixed_visit_count;
        if (restarts > 1) {
            fixed_visit_count = reservations.visit_count;
        }
        for (int attempt = 0; attempt < restarts; ++attempt) {
            if (deadline.expired()) break;

            std::vector<int> order = base_order;
            if (attempt > 0) {
                bool found = false;
                for (int retry = 0; retry < 32 && !found; ++retry) {
                    std::shuffle(order.begin(), order.end(), rng);
                    std::vector<int> failed_first = order;
                    if (previous_failed_agent != -1) {
                        const auto position = std::find(
                            failed_first.begin(), failed_first.end(), previous_failed_agent);
                        if (position != failed_first.end()) {
                            std::rotate(failed_first.begin(), position, position + 1);
                        }
                    }
                    if (std::find(tried_orders.begin(), tried_orders.end(), failed_first) ==
                        tried_orders.end()) {
                        order = std::move(failed_first);
                        found = true;
                    } else if (std::find(tried_orders.begin(), tried_orders.end(), order) ==
                               tried_orders.end()) {
                        found = true;
                    }
                }
                if (!found) break;
            }
            tried_orders.push_back(order);
            ++best.attempts;

            mapf_result candidate;
            candidate.paths = fixed_paths;
            bool success = true;
            std::vector<int> planned_agents;
            planned_agents.reserve(order.size());
            previous_failed_agent = -1;
            for (const int agent : order) {
                auto path = astar.find_path(graph,
                                                     agent,
                                                     plan_starts[agent],
                                                     plan_goals[agent],
                                                     goal_rows[agent],
                                                     distance,
                                                     reservations,
                                                     options.congestion_weight,
                                                     deadline);
                total_expanded += path.expanded_states;
                if (!path.success) {
                    success = false;
                    previous_failed_agent = agent;
                    break;
                }
                candidate.paths[agent] = std::move(path.path);
                if (!reservations.reserve_path(agent, candidate.paths[agent])) {
                    success = false;
                    previous_failed_agent = agent;
                    break;
                }
                planned_agents.push_back(agent);
            }
            if (success) {
                candidate.success = true;
                candidate.status = mapf_status::success;
                mapf_workspace::compute_costs(candidate);
            }
            const bool continue_search = attempt + 1 < restarts && !deadline.expired();
            if (continue_search) {
                for (const int agent : planned_agents) {
                    const auto& path = candidate.paths[agent];
                    assert(!path.empty());
                    const int arrival = int(path.size()) - 1;
                    for (int time = 1; time <= reservations.max_time; ++time) {
                        const int vertex = time <= arrival ? path[time] : path.back();
                        int& value = reservations.owner[std::size_t(time) * std::size_t(reservations.vertex_count) +
                                           std::size_t(vertex)];
                        assert(value == agent);
                        value = -1;
                    }

                }
                reservations.visit_count = fixed_visit_count;
            }
            if (success && mapf_workspace::better_result(candidate, best, options.objective)) {
                const int attempts = best.attempts;
                best = std::move(candidate);
                best.attempts = attempts;
            }
        }
        best.expanded_states = total_expanded;
        if (!best.success) {
            best.status = deadline.expired() ? mapf_status::timeout : mapf_status::search_failed;
        }
        return best;
    };

    auto choose_result = [&](mapf_result candidate, bool unchanged_valid) {
        if (candidate.success) {
            const bool candidate_valid = request.external_reservations == nullptr
                ? validate_mapf_result(graph, starts, goals, candidate)
                : validate_mapf_result(graph, starts, goals, candidate, *request.external_reservations);
            if (!candidate_valid) {
                candidate.success = false;
                candidate.status = mapf_status::search_failed;
            }
        }
        if (unchanged_valid &&
            (!candidate.success ||
             mapf_workspace::better_result(unchanged, candidate, options.objective))) {
            unchanged.expanded_states = candidate.expanded_states;
            unchanged.attempts = candidate.attempts;
            unchanged.lns_improvements = 0;
            unchanged.strategy_used = mapf_strategy::prioritized;
            return unchanged;
        }
        return candidate;
    };

    if (request.end_time < 0) {
        if (request.begin_time != 0) return failure;
        for (int agent = 0; agent < agent_count; ++agent) {
            if (!workspace.selection_mask[agent] && base.paths[agent].back() != goals[agent]) {
                return failure;
            }
        }
        bool unchanged_valid = base_matches_goals;
        if (unchanged_valid && request.external_reservations != nullptr) {
            unchanged_valid = mapf_workspace::validate_paths(
                graph, starts, unchanged.paths, request.external_reservations);
        }
        if (request.agents.empty()) {
            if (unchanged_valid) {
                unchanged.expanded_states = 0;
                unchanged.attempts = 0;
                return unchanged;
            }
            failure.status = mapf_status::search_failed;
            return failure;
        }
        std::vector<int> repair_targets;
        repair_targets.reserve(request.agents.size());
        for (const int agent : request.agents) repair_targets.push_back(goals[agent]);
        if (!workspace.prepare(graph, repair_targets)) {
            failure.status = mapf_status::memory_limit;
            return failure;
        }
        workspace.goal_rows.assign(agent_count, -1);
        int max_shortest_distance = 0;
        for (const int agent : request.agents) {
            workspace.goal_rows[agent] = workspace.distance.row_of(goals[agent]);
            const int shortest = workspace.distance.get_by_row(
                workspace.goal_rows[agent], starts[agent]);
            if (shortest == mapf_workspace::inf_distance) {
                failure.status = mapf_status::unreachable;
                return failure;
            }
            max_shortest_distance = std::max(max_shortest_distance, shortest);
        }
        int max_time = options.max_time;
        if (max_time < 0) {
            max_time = std::max(
                unchanged.makespan,
                max_shortest_distance +
                    std::max({16, max_shortest_distance, agent_count}));
        } else if (max_time < unchanged.makespan) {
            failure.status = mapf_status::horizon_too_short;
            return failure;
        }
        if (request.external_reservations != nullptr) {
            max_time = std::max(max_time, request.external_reservations->horizon);
        }
        if (max_time < max_shortest_distance) {
            failure.status = mapf_status::horizon_too_short;
            return failure;
        }
        if ((std::size_t(max_time) + 1) * std::size_t(graph.size()) >
            std::size_t(std::numeric_limits<int>::max())) {
            failure.status = mapf_status::memory_limit;
            return failure;
        }

        mapf_result candidate = plan_selected(starts, goals, base.paths, max_time, 0);
        return choose_result(std::move(candidate), unchanged_valid);
    }

    if (!base_matches_goals ||
        request.begin_time < 0 || request.end_time <= request.begin_time ||
        request.end_time > unchanged.makespan) {
        return failure;
    }
    const int begin_time = request.begin_time;
    const int end_time = request.end_time;
    const int window = end_time - begin_time;
    const bool unchanged_valid =
        request.external_reservations == nullptr ||
        mapf_workspace::validate_paths(
            graph, starts, unchanged.paths, request.external_reservations);
    if (request.agents.empty()) {
        if (unchanged_valid) {
            unchanged.expanded_states = 0;
            unchanged.attempts = 0;
            return unchanged;
        }
        failure.status = mapf_status::search_failed;
        return failure;
    }
    std::vector<int> local_starts(agent_count);
    std::vector<int> local_goals(agent_count);
    std::vector<std::vector<int>> local_paths(
        agent_count, std::vector<int>(std::size_t(window + 1)));
    for (int agent = 0; agent < agent_count; ++agent) {
        for (int time = 0; time <= window; ++time) {
            local_paths[agent][time] =
                mapf_workspace::path_position(base.paths[agent], begin_time + time);
        }
        local_starts[agent] = local_paths[agent].front();
        local_goals[agent] = local_paths[agent].back();
    }
    std::vector<int> repair_targets;
    repair_targets.reserve(request.agents.size());
    for (const int agent : request.agents) repair_targets.push_back(local_goals[agent]);
    if (!workspace.prepare(graph, repair_targets)) {
        failure.status = mapf_status::memory_limit;
        return failure;
    }
    workspace.goal_rows.assign(agent_count, -1);
    for (const int agent : request.agents) {
        workspace.goal_rows[agent] = workspace.distance.row_of(local_goals[agent]);
    }
    mapf_result local_candidate = plan_selected(local_starts, local_goals, local_paths, window, begin_time);
    if (!local_candidate.success) return choose_result(std::move(local_candidate), unchanged_valid);

    mapf_result candidate = unchanged;
    candidate.expanded_states = local_candidate.expanded_states;
    candidate.attempts = local_candidate.attempts;
    candidate.lns_improvements = 0;
    candidate.strategy_used = mapf_strategy::prioritized;
    for (const int agent : request.agents) {
        std::vector<int> path;
        path.reserve(std::size_t(unchanged.makespan + 1));
        for (int time = 0; time < begin_time; ++time) {
            path.push_back(mapf_workspace::path_position(base.paths[agent], time));
        }
        for (int time = 0; time <= window; ++time) {
            path.push_back(mapf_workspace::path_position(local_candidate.paths[agent], time));
        }
        for (int time = end_time + 1; time <= unchanged.makespan; ++time) {
            path.push_back(mapf_workspace::path_position(base.paths[agent], time));
        }
        while (path.size() > 1 && path.back() == path[path.size() - 2]) path.pop_back();
        candidate.paths[agent] = std::move(path);
    }
    candidate.success = true;
    candidate.status = mapf_status::success;
    mapf_workspace::compute_costs(candidate);
    return choose_result(std::move(candidate), unchanged_valid);
}

inline mapf_result repair_mapf(const mapf_graph& graph,
                               std::span<const int> starts,
                               std::span<const int> goals,
                               const mapf_result& base,
                               const mapf_repair_request& request,
                               const mapf_options& options = {}) {
    mapf_workspace workspace;
    return repair_mapf(graph, starts, goals, base, request, workspace, options);
}

#if __INCLUDE_LEVEL__ == 0

namespace mapf_self_test {

mapf_graph make_grid(int height, int width) {
    std::vector<std::pair<int, int>> edges;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int vertex = y * width + x;
            if (x + 1 < width) edges.push_back({vertex, vertex + 1});
            if (y + 1 < height) edges.push_back({vertex, vertex + width});
        }
    }
    return mapf_graph(height * width, edges);
}

int bfs_distance(const mapf_graph& graph, int start, int goal) {
    std::vector<int> distance(graph.size(), -1);
    std::vector<int> queue(graph.size());
    int head = 0;
    int tail = 0;
    queue[tail++] = start;
    distance[start] = 0;
    while (head < tail) {
        const int vertex = queue[head++];
        if (vertex == goal) return distance[vertex];
        for (const int to : graph.neighbors(vertex)) {
            if (distance[to] != -1) continue;
            distance[to] = distance[vertex] + 1;
            queue[tail++] = to;
        }
    }
    return -1;
}

void test_basic_cases() {
    {
        const mapf_graph graph = make_grid(3, 3);
        const std::vector<int> starts = {4};
        const std::vector<int> goals = {4};
        mapf_options options;
        options.time_limit_ms = -1.0;
        const mapf_result result = solve_mapf(graph, starts, goals, options);
        assert(result.success && result.sum_of_costs == 0 && result.makespan == 0);
        assert(validate_mapf_result(graph, starts, goals, result));
    }
    {
        const mapf_graph graph(2, {{0, 1}, {0, 1}, {0, 0}});
        const std::vector<int> starts = {0, 1};
        const std::vector<int> goals = {1, 0};
        mapf_options options;
        options.time_limit_ms = -1.0;
        options.max_time = 16;
        assert(!solve_mapf(graph, starts, goals, options).success);
    }
    {
        const mapf_graph graph(3, {{0, 1}, {1, 2}, {2, 0}});
        const std::vector<int> starts = {0, 1, 2};
        const std::vector<int> goals = {1, 2, 0};
        mapf_options options;
        options.strategy = mapf_strategy::pibt;
        options.time_limit_ms = -1.0;
        options.max_time = 8;
        const mapf_result result = solve_mapf(graph, starts, goals, options);
        assert(result.success && result.makespan == 1);
    }
    {
        const mapf_graph graph = make_grid(2, 2);
        mapf_result invalid;
        invalid.success = true;
        invalid.paths = {{0, 1}, {3, 1}};
        const std::vector<int> starts = {0, 3};
        const std::vector<int> goals = {1, 1};
        assert(!validate_mapf_result(graph, starts, goals, invalid));
    }
    {
        const mapf_graph graph = make_grid(3, 3);
        const std::vector<mapd_agent> agents = {{0}};
        const std::vector<mapd_task> tasks = {{0, 0, 0}};
        mapd_options options;
        options.max_time = 2;
        options.time_limit_ms = -1.0;
        const mapd_result result = solve_mapd(graph, agents, tasks, options);
        assert(result.valid && result.completed_tasks == 1);
        assert(result.task_pickup_time[0] == 0 && result.task_completion_time[0] == 0);
    }
}

void test_single_agent_shortest_paths() {
    std::mt19937_64 rng(123456789ULL);
    for (int iteration = 0; iteration < 200; ++iteration) {
        const int vertex_count = 5 + int(rng() % 30);
        std::vector<std::pair<int, int>> edges;
        for (int vertex = 1; vertex < vertex_count; ++vertex) {
            edges.push_back({vertex, int(rng() % std::uint64_t(vertex))});
        }
        for (int index = 0; index < vertex_count * 2; ++index) {
            const int lhs = int(rng() % std::uint64_t(vertex_count));
            const int rhs = int(rng() % std::uint64_t(vertex_count));
            if (lhs != rhs) edges.push_back({lhs, rhs});
        }
        const mapf_graph graph(vertex_count, edges);
        const int start = int(rng() % std::uint64_t(vertex_count));
        const int goal = int(rng() % std::uint64_t(vertex_count));
        mapf_options options;
        options.strategy = mapf_strategy::prioritized;
        options.time_limit_ms = -1.0;
        options.prioritized_restarts = 1;
        options.lns_iterations = 0;
        const mapf_result result = solve_mapf(
            graph, std::array{start}, std::array{goal}, options);
        assert(result.success);
        assert(result.sum_of_costs == bfs_distance(graph, start, goal));
    }
}

void test_random_mapf_and_improvement() {
    const mapf_graph graph = make_grid(7, 7);
    std::mt19937_64 rng(987654321ULL);
    std::array<int, 3> successes = {0, 0, 0};
    constexpr std::array<mapf_strategy, 3> strategies = {
        mapf_strategy::prioritized, mapf_strategy::pibt, mapf_strategy::automatic};
    for (int iteration = 0; iteration < 120; ++iteration) {
        const int agent_count = 2 + int(rng() % 12);
        std::vector<int> vertices(graph.size());
        std::iota(vertices.begin(), vertices.end(), 0);
        std::shuffle(vertices.begin(), vertices.end(), rng);
        const std::vector<int> starts(vertices.begin(), vertices.begin() + agent_count);
        std::shuffle(vertices.begin(), vertices.end(), rng);
        const std::vector<int> goals(vertices.begin(), vertices.begin() + agent_count);
        for (int index = 0; index < int(strategies.size()); ++index) {
            mapf_options options;
            options.strategy = strategies[index];
            options.time_limit_ms = -1.0;
            options.max_time = 196;
            options.lns_iterations = strategies[index] == mapf_strategy::pibt ? 0 : 8;
            options.lns_size = 4;
            options.seed = rng();
            const mapf_result result = solve_mapf(graph, starts, goals, options);
            if (!result.success) continue;
            assert(validate_mapf_result(graph, starts, goals, result));
            ++successes[index];
        }
    }
    for (const int count : successes) {
        assert(count >= 112);
        (void)count;
    }

    std::vector<int> vertices(graph.size());
    std::iota(vertices.begin(), vertices.end(), 0);
    std::shuffle(vertices.begin(), vertices.end(), rng);
    const std::vector<int> starts(vertices.begin(), vertices.begin() + 12);
    std::shuffle(vertices.begin(), vertices.end(), rng);
    const std::vector<int> goals(vertices.begin(), vertices.begin() + 12);
    mapf_options base_options;
    base_options.strategy = mapf_strategy::prioritized;
    base_options.time_limit_ms = -1.0;
    base_options.lns_iterations = 0;
    const mapf_result base = solve_mapf(graph, starts, goals, base_options);
    assert(base.success);
    mapf_options improve_options = base_options;
    improve_options.lns_iterations = 32;
    improve_options.lns_size = 8;
    const mapf_result improved = improve_mapf(graph, starts, goals, base, improve_options);
    assert(validate_mapf_result(graph, starts, goals, improved));
    assert(improved.sum_of_costs <= base.sum_of_costs);
}

void test_workspace_repair_and_reservations() {
    const mapf_graph graph = make_grid(5, 5);
    const std::vector<int> starts = {0, 4, 24};
    const std::vector<int> goals = {10, 14, 24};
    mapf_options options;
    options.strategy = mapf_strategy::prioritized;
    options.time_limit_ms = -1.0;
    options.max_time = 20;
    options.prioritized_restarts = 4;
    options.lns_iterations = 0;

    mapf_workspace workspace;
    const mapf_result base = solve_mapf(graph, starts, goals, workspace, options);
    assert(validate_mapf_result(graph, starts, goals, base));
    assert(workspace.cached_target_count() == 3);

    const std::vector<int> changed_goals = {15, 14, 24};
    const std::array<int, 1> selected = {0};
    mapf_repair_request request;
    request.agents = selected;
    const mapf_result repaired =
        repair_mapf(graph, starts, changed_goals, base, request, workspace, options);
    assert(validate_mapf_result(graph, starts, changed_goals, repaired));
    assert(repaired.status == mapf_status::success);
    assert(repaired.paths[1] == base.paths[1] && repaired.paths[2] == base.paths[2]);
    assert(workspace.cached_target_count() == 4);

    const mapf_result empty_repair = repair_mapf(
        graph,
        starts,
        changed_goals,
        repaired,
        mapf_repair_request{},
        workspace,
        options);
    assert(validate_mapf_result(graph, starts, changed_goals, empty_repair));
    assert(empty_repair.paths == repaired.paths);

    {
        const mapf_graph small = make_grid(3, 3);
        const std::vector<int> small_starts = {0};
        const std::vector<int> small_goals = {2};
        mapf_result padded;
        padded.success = true;
        padded.status = mapf_status::success;
        padded.paths = {{0, 1, 2, 2, 2}};
        padded.makespan = 4;
        padded.sum_of_costs = 4;

        mapf_reservations external(9, 4);
        assert(external.block_vertex(1, 1));
        mapf_repair_request window_request;
        window_request.agents = selected;
        window_request.begin_time = 0;
        window_request.end_time = 4;
        window_request.external_reservations = &external;
        mapf_options window_options = options;
        window_options.max_time = -1;
        const mapf_result detour = repair_mapf(small,
                                               small_starts,
                                               small_goals,
                                               padded,
                                               window_request,
                                               window_options);
        assert(validate_mapf_result(small, small_starts, small_goals, detour));
        assert(detour.paths[0][1] != 1);
        assert(validate_mapf_result(
            small, small_starts, small_goals, detour, external));
    }

    {
        mapf_reservations external(4, 1);
        const std::array<int, 2> first = {0, 1};
        const std::array<int, 2> opposite = {1, 0};
        assert(external.reserve_path(0, first));
        assert(!external.reserve_path(0, opposite));
        mapf_reservations blocked(4, 1);
        assert(blocked.block_move(0, 0, 1));
        assert(!blocked.reserve_path(0, first));
    }

    {
        const mapf_graph edge(2, {{0, 1}});
        const std::array<int, 1> edge_starts = {0};
        const std::array<int, 1> edge_goals = {1};
        mapf_result base_path;
        base_path.success = true;
        base_path.status = mapf_status::success;
        base_path.paths = {{0, 1}};
        base_path.sum_of_costs = 1;
        base_path.makespan = 1;
        mapf_reservations no_wait(2, 3);
        assert(no_wait.block_move(1, 1, 1));
        mapf_repair_request wait_request;
        wait_request.agents = selected;
        wait_request.external_reservations = &no_wait;
        const mapf_result delayed = repair_mapf(
            edge, edge_starts, edge_goals, base_path, wait_request, options);
        assert(validate_mapf_result(
            edge, edge_starts, edge_goals, delayed, no_wait));
        assert(delayed.paths[0].size() >= 3 && delayed.paths[0][1] == 0);
    }

    {
        const mapf_deadline expired = mapf_deadline::after_ms(0.0);
        mapf_options deadline_options = options;
        deadline_options.deadline = &expired;
        deadline_options.time_limit_ms = -1.0;
        const mapf_result timed_out =
            solve_mapf(graph, starts, goals, deadline_options);
        assert(!timed_out.success && timed_out.status == mapf_status::timeout);

        mapf_options short_options = options;
        short_options.max_time = 1;
        const mapf_result too_short =
            solve_mapf(graph, std::array{0}, std::array{24}, short_options);
        assert(!too_short.success &&
               too_short.status == mapf_status::horizon_too_short);
    }
}

void test_random_repairs() {
    const mapf_graph graph = make_grid(8, 8);
    std::mt19937_64 rng(0x243f6a8885a308d3ULL);
    mapf_workspace reused_workspace;
    int successful_repairs = 0;
    int successful_external_repairs = 0;
    for (int iteration = 0; iteration < 80; ++iteration) {
        const int agent_count = 4 + int(rng() % 9);
        std::vector<int> vertices(graph.size());
        std::iota(vertices.begin(), vertices.end(), 0);
        std::shuffle(vertices.begin(), vertices.end(), rng);
        const std::vector<int> starts(vertices.begin(), vertices.begin() + agent_count);
        std::shuffle(vertices.begin(), vertices.end(), rng);
        const std::vector<int> goals(vertices.begin(), vertices.begin() + agent_count);

        mapf_options options;
        options.strategy = mapf_strategy::prioritized;
        options.time_limit_ms = -1.0;
        options.max_time = 128;
        options.prioritized_restarts = 8;
        options.lns_iterations = 0;
        options.seed = rng();
        const mapf_result base = solve_mapf(graph, starts, goals, options);
        if (!base.success) continue;

        std::vector<int> agent_order(agent_count);
        std::iota(agent_order.begin(), agent_order.end(), 0);
        std::shuffle(agent_order.begin(), agent_order.end(), rng);
        const int selected_count = 1 + int(rng() % std::uint64_t(std::min(5, agent_count)));
        std::vector<int> selected(
            agent_order.begin(), agent_order.begin() + selected_count);
        std::vector<unsigned char> is_selected(agent_count, 0);
        for (const int agent : selected) is_selected[agent] = 1;

        std::vector<unsigned char> unavailable(graph.size(), 0);
        for (int agent = 0; agent < agent_count; ++agent) {
            if (!is_selected[agent]) unavailable[goals[agent]] = 1;
        }
        std::vector<int> available;
        for (int vertex = 0; vertex < graph.size(); ++vertex) {
            if (!unavailable[vertex]) available.push_back(vertex);
        }
        std::vector<int> changed_goals = goals;
        for (const int agent : selected) {
            int index = int(rng() % std::uint64_t(available.size()));
            if (available[index] == goals[agent] && available.size() > 1) {
                index = (index + 1) % int(available.size());
            }
            changed_goals[agent] = available[index];
            available[index] = available.back();
            available.pop_back();
        }

        mapf_repair_request request;
        request.agents = selected;
        const mapf_result cold =
            repair_mapf(graph, starts, changed_goals, base, request, options);
        const mapf_result reused = repair_mapf(
            graph, starts, changed_goals, base, request, reused_workspace, options);
        assert(cold.success == reused.success);
        if (!cold.success) continue;
        assert(cold.paths == reused.paths);
        assert(validate_mapf_result(graph, starts, changed_goals, cold));
        for (int agent = 0; agent < agent_count; ++agent) {
            if (!is_selected[agent]) assert(cold.paths[agent] == base.paths[agent]);
        }
        ++successful_repairs;

        int blocked_agent = -1;
        for (const int agent : selected) {
            if (base.paths[agent].size() >= 3) {
                blocked_agent = agent;
                break;
            }
        }
        if (blocked_agent == -1 || base.makespan < 2) continue;
        const int blocked_time = int(base.paths[blocked_agent].size()) / 2;
        mapf_reservations external(graph.size(), base.makespan);
        assert(external.block_vertex(
            blocked_time, base.paths[blocked_agent][blocked_time]));
        const std::array<int, 1> external_selected = {blocked_agent};
        mapf_repair_request external_request;
        external_request.agents = external_selected;
        external_request.begin_time = 0;
        external_request.end_time = base.makespan;
        external_request.external_reservations = &external;
        const mapf_result external_repair =
            repair_mapf(graph, starts, goals, base, external_request, options);
        if (external_repair.success) {
            assert(validate_mapf_result(
                graph, starts, goals, external_repair, external));
            ++successful_external_repairs;
        }
    }
    assert(successful_repairs >= 60);
    assert(successful_external_repairs >= 20);
}

void test_random_mapd() {
    const mapf_graph graph = make_grid(8, 8);
    std::mt19937_64 rng(0x123456789abcdef0ULL);
    constexpr std::array<mapd_assignment_strategy, 4> strategies = {
        mapd_assignment_strategy::automatic,
        mapd_assignment_strategy::greedy,
        mapd_assignment_strategy::greedy_unique_endpoints,
        mapd_assignment_strategy::min_cost_flow,
    };
    for (int iteration = 0; iteration < 40; ++iteration) {
        const int agent_count = 2 + int(rng() % 7);
        std::vector<int> vertices(graph.size());
        std::iota(vertices.begin(), vertices.end(), 0);
        std::shuffle(vertices.begin(), vertices.end(), rng);
        std::vector<mapd_agent> agents(agent_count);
        for (int agent = 0; agent < agent_count; ++agent) agents[agent].start = vertices[agent];
        std::vector<mapd_task> tasks(agent_count * 4);
        for (mapd_task& task : tasks) {
            task.pickup = int(rng() % std::uint64_t(graph.size()));
            do {
                task.delivery = int(rng() % std::uint64_t(graph.size()));
            } while (task.delivery == task.pickup);
            task.release_time = int(rng() % 20);
        }
        for (const mapd_assignment_strategy strategy : strategies) {
            mapd_options options;
            options.max_time = 160;
            options.time_limit_ms = -1.0;
            options.assignment = strategy;
            options.seed = rng();
            const mapd_result result = solve_mapd(graph, agents, tasks, options);
            assert(result.valid);
            assert(validate_mapd_result(graph, agents, tasks, result));
        }
    }
}

void test_added_boundaries_and_reuse() {
    // 同じオブジェクトのグラフを変更したら明示的にcacheを破棄する
    mapf_graph graph(3, {{0, 1}, {1, 2}});
    const std::vector<int> starts = {0};
    const std::vector<int> goals = {2};
    mapf_options options;
    options.strategy = mapf_strategy::prioritized;
    options.lns_iterations = 0;
    options.time_limit_ms = -1;
    mapf_workspace workspace;
    auto result = solve_mapf(graph, starts, goals, workspace, options);
    assert(result.success && result.sum_of_costs == 2);
    graph = mapf_graph(3, {{0, 2}});
    workspace.clear();
    result = solve_mapf(graph, starts, goals, workspace, options);
    assert(result.success && result.sum_of_costs == 1);
    const mapf_graph other(3, {{0, 1}, {1, 2}});
    result = solve_mapf(other, starts, goals, workspace, options);
    assert(result.success && result.sum_of_costs == 2);
    for (int horizon : {2, 40, 2, 80, 4}) {
        options.max_time = horizon;
        result = solve_mapf(other, starts, goals, workspace, options);
        assert(result.success && result.sum_of_costs == 2);
    }

    // 到達不能・horizon不足・期限切れを区別する
    options.max_time = 1;
    assert(solve_mapf(other, starts, goals, options).status == mapf_status::horizon_too_short);
    options.max_time = 10;
    const mapf_graph disconnected(3, {{0, 1}});
    assert(solve_mapf(disconnected, starts, goals, options).status == mapf_status::unreachable);
    mapf_deadline expired(mapf_deadline::clock::now() - std::chrono::seconds(1));
    options.deadline = &expired;
    assert(solve_mapf(other, starts, goals, options).status == mapf_status::timeout);
    options.deadline = nullptr;

    // タスクなし・同一点pickup/delivery・horizonより後のreleaseを全割当方式で検証
    const std::vector<mapd_agent> agents = {{0}};
    for (int assignment = 0; assignment < 4; ++assignment) {
        mapd_options d;
        d.time_limit_ms = -1;
        d.max_time = 0;
        d.assignment = static_cast<mapd_assignment_strategy>(assignment);
        const std::vector<mapd_task> empty;
        auto r = solve_mapd(other, agents, empty, d);
        assert(r.valid && r.completed_tasks == 0 && r.paths[0].size() == 1);
        d.max_time = 10;
        const std::vector<mapd_task> tasks = {{0, 0, 0}, {2, 2, 30}};
        r = solve_mapd(other, agents, tasks, d);
        assert(r.valid && r.completed_tasks == 1);
        assert(r.task_pickup_time[0] == 0 && r.task_completion_time[0] == 0);
        assert(r.task_completion_time[1] == -1);
        assert(validate_mapd_result(other, agents, tasks, r));
        auto invalid = r;
        invalid.task_completion_time[0] = -2;
        assert(!validate_mapd_result(other, agents, tasks, invalid));
    }

    // LNSの比較関数が同じ要素を自分より小さいと判定しないことも含めて反復する
    const mapf_graph grid = make_grid(4, 4);
    const std::vector<int> ss = {0, 1, 2, 3, 4, 5};
    const std::vector<int> gg = {15, 14, 13, 12, 11, 10};
    options.max_time = 30;
    options.strategy = mapf_strategy::automatic;
    options.lns_iterations = 100;
    for (int size : {1, 2, 6, 20}) {
        options.lns_size = size;
        auto r = solve_mapf(grid, ss, gg, options);
        assert(!r.success || validate_mapf_result(grid, ss, gg, r));
    }
}

void test_external_single_agent_oracle() {
    // 小さい時空間グラフを全探索し、外部予約付きA*の可解性・最短到着時刻と照合する
    const mapf_graph graph = make_grid(3, 3);
    std::mt19937_64 rng(0xb7e151628aed2a6bULL);
    constexpr int horizon = 12;
    const std::vector<int> selected = {0};
    for (int iteration = 0; iteration < 600; ++iteration) {
        const int start = int(rng() % 9);
        const int goal = int(rng() % 9);
        mapf_reservations external(9, horizon);
        for (int time = 1; time <= horizon; ++time) {
            for (int v = 0; v < 9; ++v) {
                if (rng() % 20 == 0) assert(external.block_vertex(time, v));
                if (time == horizon) continue;
                if (rng() % 30 == 0) assert(external.block_move(time, v, v));
                for (int to : graph.neighbors(v)) {
                    if (rng() % 35 == 0) assert(external.block_move(time, v, to));
                }
            }
        }
        if (iteration % 3 == 0) {
            const int v = int(rng() % 9);
            const auto ns = graph.neighbors(v);
            const std::vector<int> path = {v, ns[int(rng() % ns.size())]};
            const bool reserved = external.reserve_path(2 + int(rng() % 7), path);
            (void)reserved;
        }

        std::vector<std::vector<unsigned char>> reachable(horizon + 1, std::vector<unsigned char>(9));
        reachable[0][start] = 1;
        int earliest = -1;
        for (int time = 0; time <= horizon; ++time) {
            if (reachable[time][goal]) {
                bool hold_free = true;
                for (int later = time; later <= horizon; ++later) {
                    if (external.at(later, goal) != -1) hold_free = false;
                    if (later < horizon && external.move_is_blocked(later, goal, goal)) hold_free = false;
                }
                if (hold_free && earliest == -1) earliest = time;
            }
            if (time == horizon) continue;
            for (int v = 0; v < 9; ++v) {
                if (!reachable[time][v]) continue;
                auto relax = [&](int to) {
                    if (external.at(time + 1, to) != -1 || external.move_is_blocked(time, v, to)) return;
                    const int owner = external.at(time, to);
                    if (owner <= -3 && external.at(time + 1, v) == owner) return;
                    reachable[time + 1][to] = 1;
                };
                relax(v);
                for (int to : graph.neighbors(v)) relax(to);
            }
        }

        const std::vector<int> starts = {start}, goals = {goal};
        mapf_result base;
        base.success = true;
        base.paths = {{start}};
        mapf_repair_request request;
        request.agents = selected;
        request.external_reservations = &external;
        mapf_options options;
        options.time_limit_ms = -1;
        options.max_time = horizon;
        options.prioritized_restarts = 1;
        const auto result = repair_mapf(graph, starts, goals, base, request, options);
        assert(result.success == (earliest >= 0));
        if (result.success) {
            assert(result.sum_of_costs == earliest);
            assert(validate_mapf_result(graph, starts, goals, result, external));
        }
    }
}

void test_empty_agent_rejection() {
    // 新規探索・修復と同じく、空のagent集合を検証・改善でも拒否する
    const mapf_graph graph(1, {});
    mapf_result empty;
    empty.success = true;
    assert(!validate_mapf_result(graph, {}, {}, empty));
    const mapf_reservations external(1, 0);
    assert(!validate_mapf_result(graph, {}, {}, empty, external));
    mapf_options options;
    options.time_limit_ms = -1;
    mapf_workspace workspace;
    assert(improve_mapf(graph, {}, {}, empty, options).status == mapf_status::invalid_input);
    assert(improve_mapf(graph, {}, {}, empty, workspace, options).status == mapf_status::invalid_input);
    assert(!validate_mapd_result(graph, {}, {}, {}));

    // agentが存在する場合は、移動なし・taskなしの正当な結果を引き続き受け入れる
    const auto stationary = solve_mapf(graph, std::array{0}, std::array{0}, options);
    assert(stationary.success && validate_mapf_result(graph, std::array{0}, std::array{0}, stationary));
    mapd_options mapd;
    mapd.time_limit_ms = -1;
    mapd.max_time = 0;
    const auto no_tasks = solve_mapd(graph, std::array{mapd_agent{0}}, {}, mapd);
    assert(no_tasks.valid && validate_mapd_result(graph, std::array{mapd_agent{0}}, {}, no_tasks));
}

void test_moved_workspace_reuse() {
    static_assert(std::is_nothrow_move_constructible_v<mapf_workspace>);
    static_assert(std::is_nothrow_move_assignable_v<mapf_workspace>);
    std::mt19937_64 rng(0x7a12b985cf63ULL);
    for (int iteration = 0; iteration < 200; ++iteration) {
        const int n = 2 + int(rng() % 30);
        std::vector<std::pair<int, int>> edges;
        for (int vertex = 1; vertex < n; ++vertex) edges.push_back({vertex - 1, vertex});
        const mapf_graph graph(n, edges);
        const std::array starts{int(rng() % std::uint64_t(n))};
        const std::array goals{int(rng() % std::uint64_t(n))};
        mapf_options options;
        options.strategy = mapf_strategy::prioritized;
        options.time_limit_ms = -1;
        options.prioritized_restarts = 1;
        options.lns_iterations = 0;
        options.max_time = n + int(rng() % 40);
        const auto expected = solve_mapf(graph, starts, goals, options);
        assert(expected.success && expected.sum_of_costs == std::abs(starts[0] - goals[0]));
        auto check = [&](mapf_workspace& workspace) {
            const auto actual = solve_mapf(graph, starts, goals, workspace, options);
            assert(actual.success && actual.paths == expected.paths);
            assert(actual.sum_of_costs == expected.sum_of_costs);
        };

        // move構築の両側を、同じグラフ・同じ確保サイズで再利用する
        mapf_workspace source;
        check(source);
        mapf_workspace destination(std::move(source));
        assert(source.cached_target_count() == 0);
        assert(destination.cached_target_count() == 1);
        check(destination);
        check(source);

        // 確保済みの宛先へのmove代入後も、両側の管理情報と配列が一致する
        destination = std::move(source);
        assert(source.cached_target_count() == 0);
        check(source);
        check(destination);
        source = std::move(source);
        check(source);
    }
}

}  // namespace mapf_self_test

int main() {
    mapf_self_test::test_basic_cases();
    mapf_self_test::test_single_agent_shortest_paths();
    mapf_self_test::test_random_mapf_and_improvement();
    mapf_self_test::test_workspace_repair_and_reservations();
    mapf_self_test::test_random_repairs();
    mapf_self_test::test_random_mapd();
    mapf_self_test::test_added_boundaries_and_reuse();
    mapf_self_test::test_external_single_agent_oracle();
    mapf_self_test::test_empty_agent_rejection();
    mapf_self_test::test_moved_workspace_reuse();
    std::cout << "all tests passed\n";
}

#endif
