/*
 * routing_solver: AHC 向けの複数経路・任意訪問・pickup-delivery solver
 *
 * 静的な頂点間コストを持つ routing 問題を、次の直交した要素の組合せとして扱う。
 * - 全項目必須、報酬最大化、または訪問項目数固定
 * - 車両容量、経路コスト上限、または両方
 * - 単一訪問頂点、または pickup-delivery pair
 *
 * 公開関数は CVRP、Orienteering、prize-collecting VRP、PDP、selective PDP を
 * 型安全な入口として提供する。内部では dense ID、候補近傍、局所探索、
 * ruin-and-recreate を共有し、実行時の virtual dispatch は使用しない。
 *
 * 公開 API は用途別に分け、内部の探索構成はベンチマークで選定した一種類に固定する。
 */
#pragma once

#include <bits/stdc++.h>

namespace routing {

struct RoutingParam {
    int time_limit_ms = 100;
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::time_point::max();
    std::uint64_t seed = 1;
    bool symmetric = true;
    std::uint64_t max_move_evaluations = 0;
};

struct RoutingStats {
    std::uint64_t move_evaluations = 0;
    std::uint64_t accepted_moves = 0;
    std::uint64_t local_search_runs = 0;
    std::uint64_t lns_rounds = 0;
    std::uint64_t best_updates = 0;
    std::uint64_t destroyed_items = 0;
    std::uint64_t repaired_items = 0;
    double distance_build_ms = 0.0;
    double candidate_build_ms = 0.0;
    double construction_ms = 0.0;
    double local_search_ms = 0.0;
    double lns_ms = 0.0;
    double total_ms = 0.0;
};

template <class Vertex, class Load = long long, class Reward = long long,
          class ServiceCost = long long>
struct RouteTask {
    Vertex vertex{};
    Load demand{};
    Reward reward{};
    ServiceCost service_cost{};
    bool mandatory = true;
};

template <class Vertex, class Load = long long, class Reward = long long,
          class ServiceCost = long long>
struct PickupDeliveryTask {
    Vertex pickup{};
    Vertex delivery{};
    Load quantity{};
    Reward reward{};
    ServiceCost pickup_service_cost{};
    ServiceCost delivery_service_cost{};
    bool mandatory = true;
};

template <class Vertex, class Load = long long, class CostLimit = long long>
struct RouteVehicle {
    Vertex start{};
    Vertex finish{};
    Load capacity = std::numeric_limits<Load>::max() / 4;
    CostLimit max_cost = std::numeric_limits<CostLimit>::max() / 4;
};

template <class Vertex, class Calc, class Reward = long long>
struct RoutingSolution {
    std::vector<std::vector<Vertex>> routes;
    std::vector<int> unserved_items;
    Calc travel_cost{};
    Reward collected_reward{};
    bool feasible = false;
};

template <class Vertex, class Calc> class RoutingContext;

template <class Calc = void, class Vertex, class F>
auto make_routing_context(std::span<const Vertex> vertices, const F& dist,
                          bool symmetric = true);

template <class Calc = void, class Vertex, class Load, class Reward,
          class ServiceCost, class CostLimit, class F>
auto solve_cvrp(
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const F& dist, const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Calc = void, class Vertex, class Load, class Reward,
          class ServiceCost, class CostLimit, class F>
auto solve_orienteering(
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const F& dist, const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Calc = void, class Vertex, class Load, class Reward,
          class ServiceCost, class CostLimit, class F>
auto solve_prize_collecting_vrp(
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const F& dist, const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Calc = void, class Vertex, class Load, class Reward,
          class ServiceCost, class CostLimit, class F>
auto solve_selective_vrp(
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const F& dist, int selected_count, const RoutingParam& param = {},
    RoutingStats* stats = nullptr);

template <class Calc = void, class Vertex, class Load, class Reward,
          class ServiceCost, class CostLimit, class F>
auto solve_pdvrp(
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const F& dist, const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Calc = void, class Vertex, class Load, class Reward,
          class ServiceCost, class CostLimit, class F>
auto solve_prize_collecting_pdvrp(
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const F& dist, const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Calc = void, class Vertex, class Load, class Reward,
          class ServiceCost, class CostLimit, class F>
auto solve_selective_pdvrp(
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const F& dist, int selected_count, const RoutingParam& param = {},
    RoutingStats* stats = nullptr);

template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto solve_cvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto solve_orienteering(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto solve_prize_collecting_vrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto solve_selective_vrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    int selected_count, const RoutingParam& param = {},
    RoutingStats* stats = nullptr);

template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto improve_cvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    std::span<const std::vector<Vertex>> initial_routes,
    const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto improve_orienteering(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    std::span<const std::vector<Vertex>> initial_routes,
    const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto improve_prize_collecting_vrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    std::span<const std::vector<Vertex>> initial_routes,
    const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto improve_selective_vrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    std::span<const std::vector<Vertex>> initial_routes, int selected_count,
    const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto improve_pdvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    std::span<const std::vector<Vertex>> initial_routes,
    const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto improve_prize_collecting_pdvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    std::span<const std::vector<Vertex>> initial_routes,
    const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto improve_selective_pdvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    std::span<const std::vector<Vertex>> initial_routes, int selected_count,
    const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto solve_pdvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto solve_prize_collecting_pdvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const RoutingParam& param = {}, RoutingStats* stats = nullptr);

template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto solve_selective_pdvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    int selected_count, const RoutingParam& param = {},
    RoutingStats* stats = nullptr);

template <class Vertex, class Calc>
class RoutingContext {
    enum class SelectionKind : unsigned char {
        AllMandatory,
        MaximizeReward,
        ExactCount,
    };
    static_assert(std::integral<Vertex> && !std::same_as<std::remove_cv_t<Vertex>, bool>);

    std::vector<Vertex> vertices_;
    std::vector<std::vector<Calc>> distance_;
    std::vector<int> candidates_;
    int candidate_width_ = 0;
    bool symmetric_ = true;

public:
    RoutingContext() = default;

    // 登録頂点数を返す。O(1)
    int size() const { return static_cast<int>(vertices_.size()); }

    // 元の頂点 ID を内部 ID へ変換し、未登録なら -1 を返す。O(log n)
    int index_of(Vertex vertex) const {
        const auto it = std::lower_bound(vertices_.begin(), vertices_.end(), vertex);
        if (it == vertices_.end() || *it != vertex) return -1;
        return static_cast<int>(it - vertices_.begin());
    }

    // 内部 ID に対応する元の頂点 ID を返す。O(1)
    Vertex vertex(int index) const { return vertices_[index]; }

    // 内部 ID 間の距離を返す。O(1)
    Calc distance(int from, int to) const {
        return distance_[from][to];
    }

    // 頂点の候補近傍を返す。O(1)
    std::span<const int> candidates(int vertex_index) const {
        if (candidate_width_ == 0) return {};
        return std::span<const int>(
            candidates_.data() + vertex_index * candidate_width_,
            static_cast<std::size_t>(candidate_width_));
    }

    // 距離が対称として構築されたなら true を返す。O(1)
    bool symmetric() const { return symmetric_; }

private:
    static constexpr int candidate_width = 16;
    static constexpr int selection_scan_limit = 32;

    struct FastRng {
        std::uint64_t state;

        explicit FastRng(std::uint64_t seed) : state(seed) {}

        std::uint64_t operator()() {
            std::uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return z ^ (z >> 31);
        }

        int index(int n) {
            assert(n > 0);
            return static_cast<int>((*this)() % static_cast<std::uint64_t>(n));
        }
    };

    struct SearchControl {
        static constexpr std::uint64_t time_check_interval = 64;

        std::chrono::steady_clock::time_point deadline;
        std::uint64_t max_evaluations;
        RoutingStats* stats;
        bool timed_out = false;

        SearchControl(const RoutingParam& param, RoutingStats* stats_)
            : deadline(std::min(
                  std::chrono::steady_clock::now() +
                      std::chrono::milliseconds(param.time_limit_ms),
                  param.deadline)),
              max_evaluations(param.max_move_evaluations),
              stats(stats_) {}

        bool expired_now() {
            if (max_evaluations != 0 && stats->move_evaluations >= max_evaluations) {
                timed_out = true;
                return true;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                timed_out = true;
                return true;
            }
            return false;
        }

        bool after_evaluation() {
            ++stats->move_evaluations;
            if (max_evaluations != 0 && stats->move_evaluations >= max_evaluations) {
                timed_out = true;
                return true;
            }
            if ((stats->move_evaluations & (time_check_interval - 1)) != 0) return false;
            if (std::chrono::steady_clock::now() >= deadline) {
                timed_out = true;
                return true;
            }
            return false;
        }
    };

    template <class T>
    static T infinity() {
        if constexpr (std::floating_point<T>) {
            return std::numeric_limits<T>::infinity();
        } else {
            return std::numeric_limits<T>::max() / 4;
        }
    }

    template <class T>
    static double elapsed_ms(const T& begin) {
        return std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - begin)
            .count();
    }

    template <SelectionKind Selection, class State>
    static bool better_state(const State& lhs, const State& rhs, int exact_count) {
        if (lhs.feasible != rhs.feasible) return lhs.feasible;
        if (!lhs.feasible) {
            if (lhs.resource_feasible != rhs.resource_feasible) {
                return lhs.resource_feasible;
            }
            if (lhs.missing_mandatory != rhs.missing_mandatory) {
                return lhs.missing_mandatory < rhs.missing_mandatory;
            }
            if constexpr (Selection == SelectionKind::AllMandatory) {
                if (lhs.selected_count != rhs.selected_count) {
                    return lhs.selected_count > rhs.selected_count;
                }
            } else if constexpr (Selection == SelectionKind::ExactCount) {
                const int lhs_gap = std::abs(lhs.selected_count - exact_count);
                const int rhs_gap = std::abs(rhs.selected_count - exact_count);
                if (lhs_gap != rhs_gap) return lhs_gap < rhs_gap;
            }
        }
        if constexpr (Selection == SelectionKind::MaximizeReward) {
            if (lhs.collected_reward != rhs.collected_reward) {
                return lhs.collected_reward > rhs.collected_reward;
            }
        }
        return lhs.total_cost < rhs.total_cost;
    }

    template <SelectionKind Selection, class State>
    static bool within_record_tolerance(const State& candidate, const State& best) {
        if (!candidate.feasible || !best.feasible) {
            return false;
        }
        constexpr long double tolerance = 0.005L;
        if constexpr (Selection == SelectionKind::MaximizeReward) {
            const long double best_reward =
                static_cast<long double>(best.collected_reward);
            const long double allowed_loss =
                std::max(1.0L, std::abs(best_reward) * tolerance);
            if (static_cast<long double>(candidate.collected_reward) <
                best_reward - allowed_loss) {
                return false;
            }
        }
        const long double best_cost = static_cast<long double>(best.total_cost);
        const long double allowed_cost =
            best_cost + std::max(1.0L, std::abs(best_cost) * tolerance);
        return static_cast<long double>(candidate.total_cost) <= allowed_cost;
    }

    template <class F>
    RoutingContext(std::span<const Vertex> input_vertices, const F& dist,
                   bool symmetric, RoutingStats* stats) {
        vertices_.assign(input_vertices.begin(), input_vertices.end());
        std::sort(vertices_.begin(), vertices_.end());
        vertices_.erase(
            std::unique(vertices_.begin(), vertices_.end()),
            vertices_.end());
        symmetric_ = symmetric;

        // 全距離を二次元配列へ格納し、以後の近傍評価を O(1) の配列参照にする
        const auto distance_begin = std::chrono::steady_clock::now();
        const int n = static_cast<int>(vertices_.size());
        distance_.assign(n, std::vector<Calc>(n));
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                distance_[i][j] =
                    static_cast<Calc>(dist(vertices_[i], vertices_[j]));
            }
        }
        if (stats) stats->distance_build_ms += elapsed_ms(distance_begin);

        // 各頂点について近い出辺と、非対称時は近い入辺を候補集合にする
        const auto candidate_begin = std::chrono::steady_clock::now();
        candidate_width_ = std::min(candidate_width, std::max(0, n - 1));
        candidates_.assign(
            static_cast<std::size_t>(n) * static_cast<std::size_t>(candidate_width_),
            -1);
        std::vector<std::pair<Calc, int>> values;
        values.reserve(std::max(0, n - 1));
        for (int i = 0; i < n; ++i) {
            values.clear();
            for (int j = 0; j < n; ++j) {
                if (i == j) continue;
                const Calc outgoing = distance(i, j);
                const Calc key = symmetric
                                     ? outgoing
                                     : std::min(outgoing, distance(j, i));
                values.emplace_back(key, j);
            }
            const int width = candidate_width_;
            if (width < static_cast<int>(values.size())) {
                std::nth_element(values.begin(), values.begin() + width, values.end());
                values.resize(width);
            }
            std::sort(values.begin(), values.end());
            for (int j = 0; j < width; ++j) {
                candidates_[i * width + j] = values[j].second;
            }
        }
        if (stats) stats->candidate_build_ms += elapsed_ms(candidate_begin);

    }

    template <class Load, class Reward>
    struct SingleSolver {
        struct Problem {
            int n = 0;
            int vehicle_count = 0;
            std::vector<int> context_vertex;
            std::vector<Load> demand;
            std::vector<Reward> reward;
            std::vector<Calc> service_cost;
            std::vector<unsigned char> mandatory;
            std::vector<int> start;
            std::vector<int> finish;
            std::vector<Load> capacity;
            std::vector<Calc> max_cost;
            std::vector<int> node_by_context;
            int exact_count = 0;
        };

        struct State {
            std::vector<std::vector<int>> routes;
            std::vector<int> route_of;
            std::vector<int> position;
            std::vector<Load> route_load;
            std::vector<Calc> route_cost;
            std::vector<std::vector<Calc>> reverse_prefix;
            std::vector<std::vector<Calc>> cost_prefix;
            std::vector<std::vector<Load>> load_prefix;
            std::vector<unsigned char> selected;
            int selected_count = 0;
            int missing_mandatory = 0;
            Reward collected_reward{};
            Calc total_cost{};
            bool resource_feasible = true;
            bool feasible = false;
        };

        struct InsertionChoice {
            int route = -1;
            int position = -1;
            Calc delta = infinity<Calc>();
            Calc second_delta = infinity<Calc>();
        };

        const RoutingContext& context;
        Problem problem;

        template <SelectionKind Selection, bool UseCapacity>
        void rebuild_single_state(State& state) const {

            // 所属・位置・選択状態を全経路から再構築する
            state.route_of.assign(problem.n, -1);
            state.position.assign(problem.n, -1);
            state.selected.assign(problem.n, 0);
            state.route_load.assign(problem.vehicle_count, Load{});
            state.route_cost.assign(problem.vehicle_count, Calc{});
            state.reverse_prefix.resize(problem.vehicle_count);
            state.selected_count = 0;
            state.collected_reward = Reward{};
            state.total_cost = Calc{};
            state.resource_feasible = true;

            // 2-opt* の棄却候補を O(1) で評価するため、接頭部分の費用と需要を保持する。
            state.cost_prefix.resize(problem.vehicle_count);
            state.load_prefix.resize(problem.vehicle_count);
            for (int r = 0; r < problem.vehicle_count; ++r) {
                const auto& nodes = state.routes[r];
                auto& cost = state.cost_prefix[r];
                auto& load = state.load_prefix[r];
                cost.assign(nodes.size() + 1, Calc{});
                load.assign(nodes.size() + 1, Load{});
                int prev = problem.start[r];
                for (int p = 0; p < static_cast<int>(nodes.size()); ++p) {
                    const int v = nodes[p];
                    cost[p + 1] = cost[p] +
                                  context.distance(prev, problem.context_vertex[v]) +
                                  problem.service_cost[v];
                    load[p + 1] = load[p] + problem.demand[v];
                    prev = problem.context_vertex[v];
                }
            }

            for (int r = 0; r < problem.vehicle_count; ++r) {
                auto& route = state.routes[r];
                for (int p = 0; p < static_cast<int>(route.size()); ++p) {
                    const int v = route[p];
                    if (v < 0 || v >= problem.n || state.selected[v]) {
                        state.resource_feasible = false;
                        continue;
                    }
                    state.selected[v] = 1;
                    state.route_of[v] = r;
                    state.position[v] = p;
                    ++state.selected_count;
                    state.collected_reward += problem.reward[v];
                    state.route_load[r] += problem.demand[v];
                }
                state.route_cost[r] = route.empty() ? Calc{} :
                    state.cost_prefix[r].back() + context.distance(
                        problem.context_vertex[route.back()], problem.finish[r]);
                state.total_cost += state.route_cost[r];
                if constexpr (UseCapacity) {
                    if (state.route_load[r] > problem.capacity[r]) {
                        state.resource_feasible = false;
                    }
                }
                if (state.route_cost[r] > problem.max_cost[r]) {
                    state.resource_feasible = false;
                }

                // 非対称 2-opt の区間内部反転差を prefix sum にする
                auto& prefix = state.reverse_prefix[r];
                prefix.assign(route.size(), Calc{});
                if (!context.symmetric()) {
                    for (int i = 0; i + 1 < static_cast<int>(route.size()); ++i) {
                        prefix[i + 1] = prefix[i] +
                                        context.distance(problem.context_vertex[route[i + 1]],
                                                         problem.context_vertex[route[i]]) -
                                        context.distance(problem.context_vertex[route[i]],
                                                         problem.context_vertex[route[i + 1]]);
                    }
                }
            }

            // selection policy ごとの必須条件を評価する
            state.missing_mandatory = 0;
            for (int v = 0; v < problem.n; ++v) {
                if (problem.mandatory[v] && !state.selected[v]) ++state.missing_mandatory;
            }
            if constexpr (Selection == SelectionKind::AllMandatory) {
                state.feasible = state.resource_feasible && state.selected_count == problem.n;
            } else if constexpr (Selection == SelectionKind::ExactCount) {
                state.feasible = state.resource_feasible && state.missing_mandatory == 0 &&
                                 state.selected_count == problem.exact_count;
            } else {
                state.feasible = state.resource_feasible && state.missing_mandatory == 0;
            }
        }

        Calc insertion_delta(const State& state, int node, int route, int position) const {
            const auto& nodes = state.routes[route];
            const int cv = problem.context_vertex[node];
            if (nodes.empty()) {
                return context.distance(problem.start[route], cv) +
                       problem.service_cost[node] +
                       context.distance(cv, problem.finish[route]);
            }
            const int previous = position == 0
                                     ? problem.start[route]
                                     : problem.context_vertex[nodes[position - 1]];
            const int next = position == static_cast<int>(nodes.size())
                                 ? problem.finish[route]
                                 : problem.context_vertex[nodes[position]];
            return context.distance(previous, cv) + problem.service_cost[node] +
                   context.distance(cv, next) - context.distance(previous, next);
        }

        template <bool UseCapacity>
        InsertionChoice best_single_insertion(const State& state, int node, SearchControl& control) const {
            InsertionChoice result;
            Calc best_route_delta = infinity<Calc>();
            Calc second_route_delta = infinity<Calc>();
            auto record_route = [&](Calc value) {
                if (value < best_route_delta) {
                    second_route_delta = best_route_delta;
                    best_route_delta = value;
                } else if (value > best_route_delta && value < second_route_delta) {
                    second_route_delta = value;
                }
            };

            auto evaluate = [&](int route, int position, Calc& route_best) {
                Calc delta{};
                if constexpr (UseCapacity) {
                    if (state.route_load[route] + problem.demand[node] > problem.capacity[route]) {
                        return !control.after_evaluation();
                    }
                }
                delta = insertion_delta(state, node, route, position);
                const bool feasible = state.route_cost[route] + delta <= problem.max_cost[route];
                if (control.after_evaluation()) return false;
                if (!feasible) return true;
                route_best = std::min(route_best, delta);
                if (delta < result.delta) {
                    result.delta = delta;
                    result.route = route;
                    result.position = position;
                }
                return true;
            };

            // 全位置を調べる場合は候補vectorを作らずrouteごとに直接走査する
            if (problem.n <= 100 || state.selected_count < 32) {
                for (int route = 0; route < problem.vehicle_count; ++route) {
                    Calc route_best = infinity<Calc>();
                    for (int position = 0;
                         position <= static_cast<int>(state.routes[route].size());
                         ++position) {
                        if (!evaluate(route, position, route_best)) return result;
                    }
                    record_route(route_best);
                }
            } else {
                std::vector<std::pair<int, int>> positions;
                positions.reserve(static_cast<std::size_t>(
                    problem.vehicle_count * 2 + 2 * candidate_width));
                for (int route = 0; route < problem.vehicle_count; ++route) {
                    positions.emplace_back(route, 0);
                    positions.emplace_back(
                        route, static_cast<int>(state.routes[route].size()));
                }
                for (int candidate_context :
                     context.candidates(problem.context_vertex[node])) {
                    const int candidate = problem.node_by_context[candidate_context];
                    if (candidate < 0 || !state.selected[candidate]) continue;
                    const int route = state.route_of[candidate];
                    const int position = state.position[candidate];
                    positions.emplace_back(route, position);
                    positions.emplace_back(route, position + 1);
                }
                std::sort(positions.begin(), positions.end());
                positions.erase(std::unique(positions.begin(), positions.end()),
                                positions.end());
                int current_route = -1;
                Calc route_best = infinity<Calc>();
                for (const auto& [route, position] : positions) {
                    if (route != current_route) {
                        if (current_route >= 0) record_route(route_best);
                        current_route = route;
                        route_best = infinity<Calc>();
                    }
                    if (!evaluate(route, position, route_best)) return result;
                }
                if (current_route >= 0) record_route(route_best);
            }
            result.second_delta = second_route_delta;
            if (result.second_delta == infinity<Calc>()) {
                result.second_delta = result.delta;
            }
            return result;
        }

        void insert_single_node(State& state, int node, const InsertionChoice& choice) const {
            state.routes[choice.route].insert(
                state.routes[choice.route].begin() + choice.position, node);
        }

        template <SelectionKind Selection, bool UseCapacity>
        void insert_required_nodes(State& state, std::vector<int> nodes, SearchControl& control,
            bool count_as_repair) const {
            while (!nodes.empty() && !control.timed_out) {
                int chosen_index = -1;
                InsertionChoice chosen;
                auto best_regret = -infinity<Calc>();

                // destroy 後は regret、初期構築では順序を固定して O(n^2) に抑える
                if (nodes.size() <= 64) {
                    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
                        const auto candidate = best_single_insertion<UseCapacity>(state, nodes[i], control);
                        if (control.timed_out) break;
                        if (candidate.route < 0) continue;
                        const auto regret = candidate.second_delta - candidate.delta;
                        if (chosen_index < 0 || regret > best_regret ||
                            (regret == best_regret && candidate.delta < chosen.delta)) {
                            chosen_index = i;
                            chosen = candidate;
                            best_regret = regret;
                        }
                    }
                } else {
                    chosen_index = 0;
                    chosen = best_single_insertion<UseCapacity>(state, nodes[0], control);
                }
                if (chosen_index < 0 || chosen.route < 0) return;
                insert_single_node(state, nodes[chosen_index], chosen);
                if (count_as_repair) ++control.stats->repaired_items;
                nodes.erase(nodes.begin() + chosen_index);
                rebuild_single_state<Selection, UseCapacity>(state);
            }

        }

        template <SelectionKind Selection, bool UseCapacity>
        void fill_single_selection(State& state, SearchControl& control) const {
            const int scan_limit = std::min(problem.n, selection_scan_limit);
            int step = problem.n / 2 + 1;
            while (std::gcd(step, problem.n) != 1) ++step;
            while (!control.timed_out) {
                if constexpr (Selection == SelectionKind::ExactCount) {
                    if (state.selected_count >= problem.exact_count) return;
                }

                int chosen = -1;
                InsertionChoice choice;
                long double best_key = -std::numeric_limits<long double>::infinity();
                const int start = problem.n == 0
                                      ? 0
                                      : static_cast<int>(
                                            (static_cast<long long>(state.selected_count) *
                                                 11939 +
                                             17) %
                                            problem.n);
                int evaluated = 0;
                for (int offset = 0;
                     offset < problem.n && evaluated < scan_limit; ++offset) {
                    const int node = (start + offset * step) % problem.n;
                    if (state.selected[node]) continue;
                    ++evaluated;
                    const auto candidate = best_single_insertion<UseCapacity>(state, node, control);
                    if (control.timed_out) return;
                    if (candidate.route < 0) continue;

                    long double key = -static_cast<long double>(candidate.delta);
                    if constexpr (Selection == SelectionKind::MaximizeReward) {
                        const long double denominator =
                            std::max<long double>(1.0L, static_cast<long double>(candidate.delta));
                        key = static_cast<long double>(problem.reward[node]) / denominator;
                    }
                    if (chosen < 0 || key > best_key ||
                        (key == best_key && candidate.delta < choice.delta)) {
                        chosen = node;
                        choice = candidate;
                        best_key = key;
                    }
                }
                if (chosen < 0) return;
                insert_single_node(state, chosen, choice);
                rebuild_single_state<Selection, UseCapacity>(state);
            }
        }

        Calc remove_delta(const State& state, int route, int position) const {
            const auto& nodes = state.routes[route];
            const int node = nodes[position];
            if (nodes.size() == 1) return -state.route_cost[route];
            const int previous = position == 0
                                     ? problem.start[route]
                                     : problem.context_vertex[nodes[position - 1]];
            const int next = position + 1 == static_cast<int>(nodes.size())
                                 ? problem.finish[route]
                                 : problem.context_vertex[nodes[position + 1]];
            const int current = problem.context_vertex[node];
            return context.distance(previous, next) -
                   context.distance(previous, current) - problem.service_cost[node] -
                   context.distance(current, next);
        }

        template <SelectionKind Selection, bool UseCapacity>
        void single_local_search(State& state, FastRng& rng, SearchControl& control) const {
            auto try_single_relocate = [&](int node, int target_route, int target_position) {
                const int source_route = state.route_of[node];
                const int source_position = state.position[node];
                if (source_route < 0) return false;
                if (source_route == target_route &&
                    (target_position == source_position ||
                     target_position == source_position + 1)) {
                    return false;
                }

                // 経路間 relocate は削除・挿入差分だけで判定する
                if (source_route != target_route) {
                    const auto remove = remove_delta(state, source_route,
                                                     source_position);
                    const auto insert = insertion_delta(state, node,
                                                        target_route, target_position);
                    bool feasible = remove + insert < decltype(remove){};
                    if constexpr (UseCapacity) {
                        feasible = feasible &&
                                   state.route_load[target_route] + problem.demand[node] <=
                                       problem.capacity[target_route];
                    }
                    feasible = feasible &&
                               state.route_cost[source_route] + remove <=
                                   problem.max_cost[source_route] &&
                               state.route_cost[target_route] + insert <=
                                   problem.max_cost[target_route];
                    if (control.after_evaluation()) return false;
                    if (!feasible) return false;
                    state.routes[source_route].erase(
                        state.routes[source_route].begin() + source_position);
                    state.routes[target_route].insert(
                        state.routes[target_route].begin() + target_position, node);
                    rebuild_single_state<Selection, UseCapacity>(state);
                    ++control.stats->accepted_moves;
                    return true;
                }

                // 重なる境界は入口で除外済み。棄却時には経路を編集しない。
                auto& route = state.routes[source_route];
                const auto remove = remove_delta(state, source_route,
                                                 source_position);
                const auto insert = insertion_delta(state, node,
                                                    source_route, target_position);
                const auto new_cost = state.route_cost[source_route] + remove + insert;
                bool feasible = new_cost < state.route_cost[source_route];
                feasible = feasible && new_cost <= problem.max_cost[source_route];
                if (control.after_evaluation() || !feasible) {
                    return false;
                }
                route.erase(route.begin() + source_position);
                if (target_position > source_position) --target_position;
                route.insert(route.begin() + target_position, node);
                rebuild_single_state<Selection, UseCapacity>(state);
                ++control.stats->accepted_moves;
                return true;
            };

            auto try_single_swap = [&](int lhs, int rhs) {
                if (lhs == rhs || !state.selected[lhs] || !state.selected[rhs]) return false;
                const int lhs_route = state.route_of[lhs];
                const int rhs_route = state.route_of[rhs];
                const int lhs_position = state.position[lhs];
                const int rhs_position = state.position[rhs];
                bool feasible = true;
                if constexpr (UseCapacity) {
                    if (lhs_route != rhs_route) {
                        const auto lhs_load = state.route_load[lhs_route] -
                                              problem.demand[lhs] + problem.demand[rhs];
                        const auto rhs_load = state.route_load[rhs_route] -
                                              problem.demand[rhs] + problem.demand[lhs];
                        feasible = lhs_load <= problem.capacity[lhs_route] &&
                                   rhs_load <= problem.capacity[rhs_route];
                    }
                }
                auto fragment_cost = [&](int route_index, int first_position,
                                         int second_position) {
                    const auto& nodes = state.routes[route_index];
                    const int length = static_cast<int>(nodes.size());
                    std::array<int, 4> edges{};
                    int edge_count = 0;
                    edges[edge_count++] = first_position;
                    edges[edge_count++] = first_position + 1;
                    if (second_position >= 0 && second_position != first_position) {
                        edges[edge_count++] = second_position;
                        edges[edge_count++] = second_position + 1;
                    }
                    std::sort(edges.begin(), edges.begin() + edge_count);
                    Calc result{};
                    int previous_edge = -1;
                    for (int i = 0; i < edge_count; ++i) {
                        const int edge = edges[i];
                        if (edge == previous_edge) continue;
                        previous_edge = edge;
                        const int from = edge == 0
                                             ? problem.start[route_index]
                                             : problem.context_vertex[nodes[edge - 1]];
                        const int to = edge == length
                                           ? problem.finish[route_index]
                                           : problem.context_vertex[nodes[edge]];
                        result += context.distance(from, to);
                    }
                    result += problem.service_cost[nodes[first_position]];
                    if (second_position >= 0 && second_position != first_position) {
                        result += problem.service_cost[nodes[second_position]];
                    }
                    return result;
                };
                const Calc lhs_old_fragment = fragment_cost(
                    lhs_route, lhs_position,
                    lhs_route == rhs_route ? rhs_position : -1);
                const Calc rhs_old_fragment =
                    lhs_route == rhs_route
                        ? Calc{}
                        : fragment_cost(rhs_route, rhs_position, -1);
                std::swap(state.routes[lhs_route][lhs_position],
                          state.routes[rhs_route][rhs_position]);
                const Calc lhs_cost = state.route_cost[lhs_route] +
                                      fragment_cost(lhs_route, lhs_position,
                                                    lhs_route == rhs_route ? rhs_position
                                                                           : -1) -
                                      lhs_old_fragment;
                const Calc rhs_cost =
                    lhs_route == rhs_route
                        ? lhs_cost
                        : state.route_cost[rhs_route] +
                              fragment_cost(rhs_route, rhs_position, -1) -
                              rhs_old_fragment;
                const auto old_cost = state.route_cost[lhs_route] +
                                      (lhs_route == rhs_route ? decltype(lhs_cost){}
                                                              : state.route_cost[rhs_route]);
                const auto new_cost = lhs_cost +
                                      (lhs_route == rhs_route ? decltype(lhs_cost){}
                                                              : rhs_cost);
                feasible = feasible && new_cost < old_cost;
                feasible = feasible && lhs_cost <= problem.max_cost[lhs_route] &&
                           (lhs_route == rhs_route ||
                            rhs_cost <= problem.max_cost[rhs_route]);
                if (control.after_evaluation() || !feasible) {
                    std::swap(state.routes[lhs_route][lhs_position],
                              state.routes[rhs_route][rhs_position]);
                    return false;
                }
                rebuild_single_state<Selection, UseCapacity>(state);
                ++control.stats->accepted_moves;
                return true;
            };

            auto try_single_two_opt = [&](int route, int lhs, int rhs) {
                if (lhs > rhs) std::swap(lhs, rhs);
                if (lhs == rhs) return false;
                const auto& nodes = state.routes[route];
                const int previous = lhs == 0
                                         ? problem.start[route]
                                         : problem.context_vertex[nodes[lhs - 1]];
                const int first = problem.context_vertex[nodes[lhs]];
                const int last = problem.context_vertex[nodes[rhs]];
                const int next = rhs + 1 == static_cast<int>(nodes.size())
                                     ? problem.finish[route]
                                     : problem.context_vertex[nodes[rhs + 1]];
                const auto delta = context.distance(previous, last) +
                                   context.distance(first, next) -
                                   context.distance(previous, first) -
                                   context.distance(last, next) +
                                   state.reverse_prefix[route][rhs] -
                                   state.reverse_prefix[route][lhs];
                bool feasible = delta < decltype(delta){};
                feasible = feasible &&
                           state.route_cost[route] + delta <= problem.max_cost[route];
                if (control.after_evaluation()) return false;
                if (!feasible) return false;
                std::reverse(state.routes[route].begin() + lhs,
                             state.routes[route].begin() + rhs + 1);
                rebuild_single_state<Selection, UseCapacity>(state);
                ++control.stats->accepted_moves;
                return true;
            };

            auto try_single_two_opt_star = [&](int lhs_route, int lhs_cut, int rhs_route, int rhs_cut) {
                if (lhs_route == rhs_route) return false;
                auto& lhs_nodes = state.routes[lhs_route];
                auto& rhs_nodes = state.routes[rhs_route];
                // tail の向きを保ち、元と先の終点が異なる場合も境界辺だけ付け替える。
                auto exchanged_cost = [&](int left_route, int left_cut,
                                          int right_route, int right_cut) {
                    const auto& a = state.routes[left_route];
                    const auto& b = state.routes[right_route];
                        if (left_cut == 0 && right_cut == static_cast<int>(b.size())) return Calc{};
                    const int prev = left_cut == 0
                                         ? problem.start[left_route]
                                         : problem.context_vertex[a[left_cut - 1]];
                    Calc value = state.cost_prefix[left_route][left_cut];
                    if (right_cut == static_cast<int>(b.size())) {
                        return value + context.distance(prev, problem.finish[left_route]);
                    }
                    const int next = problem.context_vertex[b[right_cut]];
                    const int old_prev = right_cut == 0
                                             ? problem.start[right_route]
                                             : problem.context_vertex[b[right_cut - 1]];
                    const int last = problem.context_vertex[b.back()];
                    value += context.distance(prev, next) + state.route_cost[right_route] -
                             state.cost_prefix[right_route][right_cut] -
                             context.distance(old_prev, next) -
                             context.distance(last, problem.finish[right_route]) +
                             context.distance(last, problem.finish[left_route]);
                    return value;
                };
                const auto lhs_cost = exchanged_cost(lhs_route, lhs_cut, rhs_route, rhs_cut);
                const auto rhs_cost = exchanged_cost(rhs_route, rhs_cut, lhs_route, lhs_cut);
                bool feasible = lhs_cost + rhs_cost <
                                state.route_cost[lhs_route] + state.route_cost[rhs_route];
                if constexpr (UseCapacity) {
                    feasible = feasible &&
                               state.load_prefix[lhs_route][lhs_cut] +
                                   state.route_load[rhs_route] -
                                   state.load_prefix[rhs_route][rhs_cut] <=
                                   problem.capacity[lhs_route] &&
                               state.load_prefix[rhs_route][rhs_cut] +
                                   state.route_load[lhs_route] -
                                   state.load_prefix[lhs_route][lhs_cut] <=
                                   problem.capacity[rhs_route];
                }
                feasible = feasible && lhs_cost <= problem.max_cost[lhs_route] &&
                           rhs_cost <= problem.max_cost[rhs_route];
                if (control.after_evaluation() || !feasible) return false;
                std::vector<int> tail(lhs_nodes.begin() + lhs_cut, lhs_nodes.end());
                lhs_nodes.erase(lhs_nodes.begin() + lhs_cut, lhs_nodes.end());
                lhs_nodes.insert(lhs_nodes.end(), rhs_nodes.begin() + rhs_cut, rhs_nodes.end());
                rhs_nodes.erase(rhs_nodes.begin() + rhs_cut, rhs_nodes.end());
                rhs_nodes.insert(rhs_nodes.end(), tail.begin(), tail.end());
                rebuild_single_state<Selection, UseCapacity>(state);
                ++control.stats->accepted_moves;
                return true;
            };

            ++control.stats->local_search_runs;
            int failures = 0;
            const int failure_limit = std::max(64, problem.n * 3);
            while (failures < failure_limit && !control.timed_out) {
                if (state.selected_count == 0) return;
                const int node = rng.index(problem.n);
                if (!state.selected[node]) {
                    ++failures;
                    continue;
                }
                bool improved = false;
                for (int candidate_context :
                     context.candidates(problem.context_vertex[node])) {
                    const int other = problem.node_by_context[candidate_context];
                    if (other < 0 || !state.selected[other] || other == node) continue;
                    const int other_route = state.route_of[other];
                    const int other_position = state.position[other];

                    if (try_single_relocate(node, other_route,
                            other_position) ||
                        try_single_relocate(node, other_route,
                            other_position + 1)) {
                        improved = true;
                        break;
                    }
                    if (control.timed_out) break;

                    if (try_single_swap(node, other)) {
                        improved = true;
                        break;
                    }
                    if (control.timed_out) break;

                    if (state.route_of[node] == state.route_of[other] &&
                        try_single_two_opt(state.route_of[node],
                            state.position[node], state.position[other])) {
                        improved = true;
                        break;
                    }
                    if (control.timed_out) break;

                    if (state.route_of[node] != state.route_of[other] &&
                        try_single_two_opt_star(state.route_of[node],
                            state.position[node] + 1, state.route_of[other],
                            state.position[other] + 1)) {
                        improved = true;
                        break;
                    }
                    if (control.timed_out) break;
                }
                failures = improved ? 0 : failures + 1;
            }
        }

        template <SelectionKind Selection, bool UseCapacity>
        void repair_single_state(State& state, SearchControl& control) const {
            std::vector<int> required;
            for (int v = 0; v < problem.n; ++v) {
                if (state.selected[v]) continue;
                if constexpr (Selection == SelectionKind::AllMandatory) {
                    required.push_back(v);
                } else if (problem.mandatory[v]) {
                    required.push_back(v);
                }
            }
            insert_required_nodes<Selection, UseCapacity>(state, std::move(required), control, true);
            if constexpr (Selection != SelectionKind::AllMandatory) {
                fill_single_selection<Selection, UseCapacity>(state, control);
            }
        }

        template <SelectionKind Selection, bool UseCapacity, class ServiceCost, class CostLimit>
        RoutingSolution<Vertex, Calc, Reward> solve(
            std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
            std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
            int exact_count, const RoutingParam& param, RoutingStats* output_stats,
            std::optional<std::span<const std::vector<Vertex>>> initial_routes = std::nullopt) {
            RoutingStats local_stats;
            RoutingStats* stats = output_stats ? output_stats : &local_stats;
            *stats = RoutingStats{};
            const auto total_begin = std::chrono::steady_clock::now();
            problem.n = static_cast<int>(tasks.size());
            problem.vehicle_count = static_cast<int>(vehicles.size());
            problem.context_vertex.resize(problem.n);
            problem.demand.resize(problem.n);
            problem.reward.resize(problem.n);
            problem.service_cost.resize(problem.n);
            problem.mandatory.resize(problem.n);
            problem.node_by_context.assign(context.size(), -1);
            for (int i = 0; i < problem.n; ++i) {
                const int index = context.index_of(tasks[i].vertex);
                assert(index >= 0);
                assert(problem.node_by_context[index] < 0);
                problem.context_vertex[i] = index;
                problem.node_by_context[index] = i;
                problem.demand[i] = tasks[i].demand;
                problem.reward[i] = tasks[i].reward;
                problem.service_cost[i] = static_cast<Calc>(tasks[i].service_cost);
                problem.mandatory[i] = tasks[i].mandatory ? 1 : 0;
            }
            problem.start.resize(problem.vehicle_count);
            problem.finish.resize(problem.vehicle_count);
            problem.capacity.resize(problem.vehicle_count);
            problem.max_cost.resize(problem.vehicle_count);
            for (int i = 0; i < problem.vehicle_count; ++i) {
                problem.start[i] = context.index_of(vehicles[i].start);
                problem.finish[i] = context.index_of(vehicles[i].finish);
                assert(problem.start[i] >= 0 && problem.finish[i] >= 0);
                problem.capacity[i] = vehicles[i].capacity;
                problem.max_cost[i] = static_cast<Calc>(vehicles[i].max_cost);
            }
            problem.exact_count = exact_count;

            State initial;
            if (initial_routes) {
                initial.routes.assign(problem.vehicle_count, {});
                assert(initial_routes->size() == vehicles.size());
                const int route_count = std::min(static_cast<int>(initial_routes->size()),
                                                 problem.vehicle_count);
                for (int r = 0; r < route_count; ++r) {
                    initial.routes[r].reserve((*initial_routes)[r].size());
                    for (Vertex vertex : (*initial_routes)[r]) {
                        const int context_vertex = context.index_of(vertex);
                        assert(context_vertex >= 0);
                        const int node = context_vertex < 0
                                             ? -1
                                             : problem.node_by_context[context_vertex];
                        assert(node >= 0);
                        if (node >= 0) initial.routes[r].push_back(node);
                    }
                }
                rebuild_single_state<Selection, UseCapacity>(initial);
                assert(initial.resource_feasible);
                if constexpr (Selection == SelectionKind::ExactCount) {
                    assert(initial.selected_count <= exact_count);
                }

            }

            auto run_search = [&]() -> State {
                SearchControl control(param, stats);
                FastRng rng(param.seed);
                const auto construction_begin = std::chrono::steady_clock::now();
                State best;
                if (initial_routes) {
                    best = std::move(initial);
                    repair_single_state<Selection, UseCapacity>(best, control);
                } else {
                    best.routes.assign(problem.vehicle_count, {});
                    rebuild_single_state<Selection, UseCapacity>(best);

                    // 必須頂点は大需要・depot から遠いものを先にして挿入不能を減らす
                    std::vector<int> required;
                    for (int v = 0; v < problem.n; ++v) {
                        if constexpr (Selection == SelectionKind::AllMandatory) {
                            required.push_back(v);
                        } else if (problem.mandatory[v]) {
                            required.push_back(v);
                        }
                    }
                    std::sort(required.begin(), required.end(), [&](int lhs, int rhs) {
                        if (problem.demand[lhs] != problem.demand[rhs]) {
                            return problem.demand[lhs] > problem.demand[rhs];
                        }
                        auto nearest_depot = [&](int node) {
                            auto value = infinity<Calc>();
                            for (int r = 0; r < problem.vehicle_count; ++r) {
                                value = std::min(value, context.distance(
                                    problem.start[r], problem.context_vertex[node]));
                            }
                            return value;
                        };
                        return nearest_depot(lhs) > nearest_depot(rhs);
                    });
                    insert_required_nodes<Selection, UseCapacity>(best, std::move(required), control, false);
                    if constexpr (Selection != SelectionKind::AllMandatory) {
                        fill_single_selection<Selection, UseCapacity>(best, control);
                    }

                }
                stats->construction_ms += elapsed_ms(construction_begin);
                // K=0 の実行可能解は空経路だけで、探索しても改善しない。
                if constexpr (Selection == SelectionKind::ExactCount) {
                    if (problem.exact_count == 0 && best.feasible) return best;
                }
                if (problem.n == 0 || problem.vehicle_count == 0 || control.timed_out) {
                    return best;
                }

                // 初期解を局所最適化して incumbent とする
                const auto local_begin = std::chrono::steady_clock::now();
                single_local_search<Selection, UseCapacity>(best, rng, control);
                stats->local_search_ms += elapsed_ms(local_begin);
                State current = best;

                // best 解から複数 destroy operator で摂動し、repair と局所探索を繰り返す
                const auto lns_begin = std::chrono::steady_clock::now();
                auto destroy_single_items = [&](State& state, int round) -> std::size_t {
                    std::vector<int> selected;
                    for (int v = 0; v < problem.n; ++v) {
                        if (state.selected[v]) selected.push_back(v);
                    }
                    if (selected.empty()) return 0;
                    const int percent = 15 + rng.index(16);
                    const int remove_count = std::clamp(
                        static_cast<int>(selected.size()) * percent / 100, 1,
                        std::min<int>(64, static_cast<int>(selected.size())));

                    // round ごとに random / worst / related destroy を循環する
                    auto sort_by_key = [&](auto key, bool descending) {
                        std::vector<std::pair<Calc, int>> ranking;
                        ranking.reserve(selected.size());
                        for (int item : selected) ranking.emplace_back(key(item), item);
                        std::sort(ranking.begin(), ranking.end(), [&](const auto& lhs, const auto& rhs) {
                            if (lhs.first != rhs.first) return descending ? lhs.first > rhs.first : lhs.first < rhs.first;
                            return lhs.second < rhs.second;
                        });
                        for (std::size_t i = 0; i < selected.size(); ++i) selected[i] = ranking[i].second;
                    };
                    const int mode = round % 3;
                    if (mode == 0) {
                        std::shuffle(selected.begin(), selected.end(),
                                     std::mt19937_64(rng()));
                    } else if (mode == 1) {
                        sort_by_key([&](int item) { return -remove_delta(state, state.route_of[item], state.position[item]); }, true);
                    } else {
                        const int seed = selected[rng.index(static_cast<int>(selected.size()))];
                        auto relatedness = [&](int node) {
                            return std::min(
                                context.distance(problem.context_vertex[seed],
                                                 problem.context_vertex[node]),
                                context.distance(problem.context_vertex[node],
                                                 problem.context_vertex[seed]));
                        };
                        sort_by_key(relatedness, false);
                    }
                    selected.resize(remove_count);

                    std::vector<unsigned char> removed(problem.n, 0);
                    for (int item : selected) removed[item] = 1;
                    for (auto& route : state.routes) {
                        std::erase_if(route, [&](int node) { return removed[node] != 0; });
                    }
                    return selected.size();
                };
                int round = 0;
                while (!control.expired_now()) {
                    State candidate = current;
                    stats->destroyed_items += destroy_single_items(candidate, round);
                    rebuild_single_state<Selection, UseCapacity>(candidate);
                    repair_single_state<Selection, UseCapacity>(candidate, control);
                    if (control.timed_out) break;
                    single_local_search<Selection, UseCapacity>(candidate, rng, control);
                    ++stats->lns_rounds;
                    if (better_state<Selection>(candidate, best,
                                                       problem.exact_count)) {
                        best = candidate;
                        current = std::move(candidate);
                        ++stats->best_updates;
                    } else if (better_state<Selection>(candidate, current,
                                                              problem.exact_count) ||
                               within_record_tolerance<Selection>(
                                   candidate, best)) {
                        current = std::move(candidate);
                    }
                    ++round;
                    if ((round & 31) == 0) current = best;
                }
                stats->lns_ms += elapsed_ms(lns_begin);
                return best;
            };
            State state = run_search();
            RoutingSolution<Vertex, Calc, Reward> result;
            result.routes.resize(state.routes.size());
            for (int r = 0; r < static_cast<int>(state.routes.size()); ++r) {
                result.routes[r].reserve(state.routes[r].size());
                for (int node : state.routes[r]) result.routes[r].push_back(tasks[node].vertex);
            }
            for (int v = 0; v < problem.n; ++v) {
                if (!state.selected[v]) result.unserved_items.push_back(v);
            }
            result.travel_cost = state.total_cost;
            result.collected_reward = state.collected_reward;
            result.feasible = state.feasible;
            stats->total_ms = elapsed_ms(total_begin);
            return result;
        }
    };

    template <class Load, class Reward>
    struct PairSolver {
        struct Problem {
            int request_count = 0;
            int node_count = 0;
            int vehicle_count = 0;
            std::vector<int> pickup_context;
            std::vector<int> delivery_context;
            std::vector<Load> quantity;
            std::vector<Reward> reward;
            std::vector<Calc> pickup_service;
            std::vector<Calc> delivery_service;
            std::vector<unsigned char> mandatory;
            std::vector<int> start;
            std::vector<int> finish;
            std::vector<Load> capacity;
            std::vector<Calc> max_cost;
            std::vector<int> node_by_context;
            int exact_count = 0;
        };

        struct State {
            std::vector<std::vector<int>> routes;
            std::vector<int> node_route;
            std::vector<int> node_position;
            std::vector<int> request_route;
            std::vector<int> pickup_position;
            std::vector<int> delivery_position;
            std::vector<unsigned char> selected;
            std::vector<Calc> route_cost;
            std::vector<std::vector<Load>> load_before;
            int selected_count = 0;
            int missing_mandatory = 0;
            Reward collected_reward{};
            Calc total_cost{};
            bool resource_feasible = true;
            bool feasible = false;
        };

        struct InsertionChoice {
            int route = -1;
            int pickup_position = -1;
            int delivery_base_position = -1;
            Calc delta = infinity<Calc>();
            Calc second_delta = infinity<Calc>();
        };

        const RoutingContext& context;
        Problem problem;

        int pair_context_vertex(int node) const {
            const int request = node / 2;
            return ((node & 1) == 0) ? problem.pickup_context[request]
                                        : problem.delivery_context[request];
        }

        template <SelectionKind Selection>
        void rebuild_pair_state(State& state) const {
            auto pair_route_cost = [&](std::span<const int> route, int route_index) -> Calc {
                auto pair_service_cost = [&](int node) -> Calc {
                    const int request = node / 2;
                    return ((node & 1) == 0) ? problem.pickup_service[request]
                                                : problem.delivery_service[request];
                };
                if (route.empty()) return Calc{};
                Calc result = context.distance(problem.start[route_index],
                                               pair_context_vertex(route.front()));
                for (int i = 0; i < static_cast<int>(route.size()); ++i) {
                    result += pair_service_cost(route[i]);
                    if (i + 1 < static_cast<int>(route.size())) {
                        result += context.distance(pair_context_vertex(route[i]),
                                                   pair_context_vertex(route[i + 1]));
                    }
                }
                result += context.distance(pair_context_vertex(route.back()),
                                           problem.finish[route_index]);
                return result;
            };

            state.node_route.assign(problem.node_count, -1);
            state.node_position.assign(problem.node_count, -1);
            state.request_route.assign(problem.request_count, -1);
            state.pickup_position.assign(problem.request_count, -1);
            state.delivery_position.assign(problem.request_count, -1);
            state.selected.assign(problem.request_count, 0);
            state.route_cost.assign(problem.vehicle_count, Calc{});
            state.load_before.resize(problem.vehicle_count);
            state.selected_count = 0;
            state.missing_mandatory = 0;
            state.collected_reward = Reward{};
            state.total_cost = Calc{};
            state.resource_feasible = true;

            // 各 route の重複、途中積載量、経路コストを全再計算する
            for (int r = 0; r < problem.vehicle_count; ++r) {
                const auto& route = state.routes[r];
                auto& prefix = state.load_before[r];
                prefix.assign(route.size() + 1, Load{});
                for (int p = 0; p < static_cast<int>(route.size()); ++p) {
                    const int node = route[p];
                    if (node < 0 || node >= problem.node_count || state.node_route[node] >= 0) {
                        state.resource_feasible = false;
                        continue;
                    }
                    state.node_route[node] = r;
                    state.node_position[node] = p;
                    const int request = node / 2;
                    Load delta = problem.quantity[request];
                    if (!((node & 1) == 0)) delta = -delta;
                    prefix[p + 1] = prefix[p] + delta;
                    if (prefix[p + 1] < Load{} ||
                        prefix[p + 1] > problem.capacity[r]) {
                        state.resource_feasible = false;
                    }
                }
                if (prefix.back() != Load{}) {
                    state.resource_feasible = false;
                }
                state.route_cost[r] = pair_route_cost(std::span<const int>(route), r);
                state.total_cost += state.route_cost[r];
                if (state.route_cost[r] > problem.max_cost[r]) {
                    state.resource_feasible = false;
                }
            }

            // request の両端が同一 route にあり pickup が先なら選択済みとする
            for (int request = 0; request < problem.request_count; ++request) {
                const int pickup = request * 2;
                const int delivery = pickup + 1;
                const int pickup_route = state.node_route[pickup];
                const int delivery_route = state.node_route[delivery];
                if ((pickup_route < 0) != (delivery_route < 0)) {
                    state.resource_feasible = false;
                    continue;
                }
                if (pickup_route < 0) continue;
                if (pickup_route != delivery_route ||
                    state.node_position[pickup] >= state.node_position[delivery]) {
                    state.resource_feasible = false;
                    continue;
                }
                state.selected[request] = 1;
                state.request_route[request] = pickup_route;
                state.pickup_position[request] = state.node_position[pickup];
                state.delivery_position[request] = state.node_position[delivery];
                ++state.selected_count;
                state.collected_reward += problem.reward[request];
            }
            for (int request = 0; request < problem.request_count; ++request) {
                if (problem.mandatory[request] && !state.selected[request]) {
                    ++state.missing_mandatory;
                }
            }

            if constexpr (Selection == SelectionKind::AllMandatory) {
                state.feasible = state.resource_feasible &&
                                 state.selected_count == problem.request_count;
            } else if constexpr (Selection == SelectionKind::ExactCount) {
                state.feasible = state.resource_feasible && state.missing_mandatory == 0 &&
                                 state.selected_count == problem.exact_count;
            } else {
                state.feasible = state.resource_feasible && state.missing_mandatory == 0;
            }
        }

        InsertionChoice best_pair_insertion(const State& state, int request, bool allow_separated_positions,
            SearchControl& control) const {
            auto pair_insertion_delta = [&](int route, int pickup_position, int delivery_base_position) -> Calc {
                const auto& nodes = state.routes[route];
                const int length = static_cast<int>(nodes.size());
                const int pickup = problem.pickup_context[request];
                const int delivery = problem.delivery_context[request];
                const Calc pickup_service = problem.pickup_service[request];
                const Calc delivery_service = problem.delivery_service[request];
                if (length == 0) {
                    return context.distance(problem.start[route], pickup) + pickup_service +
                           context.distance(pickup, delivery) + delivery_service +
                           context.distance(delivery, problem.finish[route]);
                }
                const int pickup_previous = pickup_position == 0
                                                ? problem.start[route]
                                                : pair_context_vertex(nodes[pickup_position - 1]);
                const int pickup_next = pickup_position == length
                                            ? problem.finish[route]
                                            : pair_context_vertex(nodes[pickup_position]);
                if (delivery_base_position == pickup_position) {
                    return context.distance(pickup_previous, pickup) + pickup_service +
                           context.distance(pickup, delivery) + delivery_service +
                           context.distance(delivery, pickup_next) -
                           context.distance(pickup_previous, pickup_next);
                }
                const int delivery_previous = pair_context_vertex(nodes[delivery_base_position - 1]);
                const int delivery_next = delivery_base_position == length
                                              ? problem.finish[route]
                                              : pair_context_vertex(nodes[delivery_base_position]);
                return context.distance(pickup_previous, pickup) + pickup_service +
                       context.distance(pickup, pickup_next) -
                       context.distance(pickup_previous, pickup_next) +
                       context.distance(delivery_previous, delivery) + delivery_service +
                       context.distance(delivery, delivery_next) -
                       context.distance(delivery_previous, delivery_next);
            };

            InsertionChoice result;
            Calc best_route_delta = infinity<Calc>();
            Calc second_route_delta = infinity<Calc>();
            auto record_route = [&](Calc value) {
                if (value < best_route_delta) {
                    second_route_delta = best_route_delta;
                    best_route_delta = value;
                } else if (value > best_route_delta && value < second_route_delta) {
                    second_route_delta = value;
                }
            };

            for (int route = 0;
                 route < problem.vehicle_count && !control.timed_out; ++route) {
                const int length = static_cast<int>(state.routes[route].size());
                Calc route_best = infinity<Calc>();
                auto evaluate = [&](int pickup_position, int delivery_position,
                                    bool capacity_feasible) {
                    // 容量だけで棄却できる候補も評価数は数え、距離参照だけを省く。
                    if (!capacity_feasible) return !control.after_evaluation();
                    const Calc delta = pair_insertion_delta(route,
                        pickup_position, delivery_position);
                    const bool feasible = state.route_cost[route] + delta <=
                                   problem.max_cost[route];
                    if (control.after_evaluation()) return false;
                    if (!feasible) return true;
                    route_best = std::min(route_best, delta);
                    if (delta < result.delta) {
                        result = {route, pickup_position, delivery_position, delta,
                                  result.second_delta};
                    }
                    return true;
                };

                auto scan_deliveries = [&](int pickup_position,
                                           const auto& delivery_positions) {
                    Load peak_load = state.load_before[route][pickup_position];
                    int next_load_position = pickup_position + 1;
                    for (int delivery_position : delivery_positions) {
                        if (delivery_position < pickup_position) continue;
                        bool capacity_feasible = true;
                        while (next_load_position <= delivery_position) {
                            peak_load = std::max(
                                peak_load,
                                state.load_before[route][next_load_position]);
                            ++next_load_position;
                        }
                        capacity_feasible =
                            peak_load + problem.quantity[request] <=
                            problem.capacity[route];
                        // 区間最大荷重は配送位置を右へ動かしても減らない。
                        // 一度容量を超えた後の全候補を枝刈りする。
                        if (!capacity_feasible) break;
                        if (!evaluate(pickup_position, delivery_position,
                                      capacity_feasible)) {
                            return false;
                        }
                    }
                    return true;
                };

                if (problem.request_count <= 60 || length <= 32) {
                    if (allow_separated_positions) {
                        for (int pickup_position = 0;
                             pickup_position <= length; ++pickup_position) {
                            std::ranges::iota_view<int, int> delivery_positions(
                                pickup_position, length + 1);
                            if (!scan_deliveries(pickup_position,
                                                 delivery_positions)) {
                                return result;
                            }
                        }
                    } else {
                        for (int pickup_position = 0;
                             pickup_position <= length; ++pickup_position) {
                            const bool capacity_feasible =
                                state.load_before[route][pickup_position] +
                                    problem.quantity[request] <=
                                problem.capacity[route];
                            if (!evaluate(pickup_position, pickup_position,
                                          capacity_feasible)) {
                                return result;
                            }
                        }
                    }
                } else {
                    std::vector<int> pickup_positions = {0, length};
                    std::vector<int> delivery_positions = {0, length};
                    auto add_candidate_positions = [&](int context_vertex,
                                                       std::vector<int>& positions) {
                        for (int candidate_context :
                             context.candidates(context_vertex)) {
                            const int node = problem.node_by_context[candidate_context];
                            if (node < 0 || state.node_route[node] != route) continue;
                            const int position = state.node_position[node];
                            positions.push_back(position);
                            positions.push_back(position + 1);
                        }
                        std::sort(positions.begin(), positions.end());
                        positions.erase(
                            std::unique(positions.begin(), positions.end()),
                            positions.end());
                    };
                    add_candidate_positions(problem.pickup_context[request],
                                            pickup_positions);
                    if (allow_separated_positions) {
                        add_candidate_positions(problem.delivery_context[request], delivery_positions);
                    }
                    for (int pickup_position : pickup_positions) {
                        if (allow_separated_positions) {
                            if (!scan_deliveries(pickup_position,
                                                 delivery_positions)) {
                                return result;
                            }
                        } else {
                            const bool capacity_feasible =
                                state.load_before[route][pickup_position] +
                                    problem.quantity[request] <=
                                problem.capacity[route];
                            if (!evaluate(pickup_position, pickup_position,
                                          capacity_feasible)) {
                                return result;
                            }
                        }
                    }
                }
                record_route(route_best);
            }
            result.second_delta = second_route_delta;
            if (result.second_delta == infinity<Calc>()) {
                result.second_delta = result.delta;
            }
            return result;
        }

        void insert_pair(State& state, int request, const InsertionChoice& choice) const {
            auto& route = state.routes[choice.route];
            route.insert(route.begin() + choice.pickup_position, request * 2);
            route.insert(route.begin() + choice.delivery_base_position + 1,
                         request * 2 + 1);
        }

        template <SelectionKind Selection>
        void insert_required_pairs(State& state, std::vector<int> requests, SearchControl& control,
            bool repair_phase) const {
            while (!requests.empty() && !control.timed_out) {
                int chosen_index = -1;
                InsertionChoice choice;
                auto best_regret = -infinity<Calc>();
                const bool use_regret = requests.size() <= 48;
                if (use_regret) {
                    for (int i = 0; i < static_cast<int>(requests.size()); ++i) {
                        const auto candidate = best_pair_insertion(state, requests[i],
                            true, control);
                        if (control.timed_out) break;
                        if (candidate.route < 0) continue;
                        const auto regret = candidate.second_delta - candidate.delta;
                        if (chosen_index < 0 || regret > best_regret ||
                            (regret == best_regret && candidate.delta < choice.delta)) {
                            chosen_index = i;
                            choice = candidate;
                            best_regret = regret;
                        }
                    }
                } else {
                    // 大規模初期構築だけは pickup-delivery を連続挿入して
                    // 三乗時間を避ける。LNS repair では離れた挿入位置も調べる。
                    chosen_index = 0;
                    choice = best_pair_insertion(state, requests[0], repair_phase,
                        control);
                }
                if (chosen_index < 0 || choice.route < 0) return;
                const int inserted_request = requests[chosen_index];
                insert_pair(state, inserted_request, choice);
                if (repair_phase) ++control.stats->repaired_items;
                requests.erase(requests.begin() + chosen_index);
                rebuild_pair_state<Selection>(state);
            }

        }

        template <SelectionKind Selection>
        void fill_pair_selection(State& state, SearchControl& control) const {
            const int scan_limit =
                std::min(problem.request_count, selection_scan_limit);
            int step = problem.request_count / 2 + 1;
            while (std::gcd(step, problem.request_count) != 1) ++step;
            while (!control.timed_out) {
                if constexpr (Selection == SelectionKind::ExactCount) {
                    if (state.selected_count >= problem.exact_count) return;
                }
                int chosen = -1;
                InsertionChoice choice;
                long double best_key = -std::numeric_limits<long double>::infinity();
                const int start = problem.request_count == 0
                                      ? 0
                                      : static_cast<int>(
                                            (static_cast<long long>(state.selected_count) *
                                                 11939 +
                                             17) %
                                            problem.request_count);
                int evaluated = 0;
                for (int offset = 0;
                     offset < problem.request_count && evaluated < scan_limit;
                     ++offset) {
                    const int request =
                        (start + offset * step) % problem.request_count;
                    if (state.selected[request]) continue;
                    ++evaluated;
                    const auto candidate = best_pair_insertion(state, request, true,
                        control);
                    if (control.timed_out) return;
                    if (candidate.route < 0) continue;
                    long double key = -static_cast<long double>(candidate.delta);
                    if constexpr (Selection == SelectionKind::MaximizeReward) {
                        const long double denominator =
                            std::max<long double>(1.0L,
                                                  static_cast<long double>(candidate.delta));
                        key = static_cast<long double>(problem.reward[request]) /
                              denominator;
                    }
                    if (chosen < 0 || key > best_key ||
                        (key == best_key && candidate.delta < choice.delta)) {
                        chosen = request;
                        choice = candidate;
                        best_key = key;
                    }
                }
                if (chosen < 0) return;
                insert_pair(state, chosen, choice);
                rebuild_pair_state<Selection>(state);
            }
        }

        template <SelectionKind Selection>
        void repair_pair_state(State& state, SearchControl& control) const {
            std::vector<int> required;
            for (int request = 0; request < problem.request_count; ++request) {
                if (state.selected[request]) continue;
                if constexpr (Selection == SelectionKind::AllMandatory) {
                    required.push_back(request);
                } else if (problem.mandatory[request]) {
                    required.push_back(request);
                }
            }
            insert_required_pairs<Selection>(state, std::move(required), control, true);
            if constexpr (Selection != SelectionKind::AllMandatory) {
                fill_pair_selection<Selection>(state, control);
            }
        }

        template <SelectionKind Selection, class ServiceCost, class CostLimit>
        RoutingSolution<Vertex, Calc, Reward> solve(
            std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
            std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
            int exact_count, const RoutingParam& param, RoutingStats* output_stats,
            std::optional<std::span<const std::vector<Vertex>>> initial_routes = std::nullopt) {
            RoutingStats local_stats;
            RoutingStats* stats = output_stats ? output_stats : &local_stats;
            *stats = RoutingStats{};
            const auto total_begin = std::chrono::steady_clock::now();
            problem.request_count = static_cast<int>(tasks.size());
            problem.node_count = problem.request_count * 2;
            problem.vehicle_count = static_cast<int>(vehicles.size());
            problem.pickup_context.resize(problem.request_count);
            problem.delivery_context.resize(problem.request_count);
            problem.quantity.resize(problem.request_count);
            problem.reward.resize(problem.request_count);
            problem.pickup_service.resize(problem.request_count);
            problem.delivery_service.resize(problem.request_count);
            problem.mandatory.resize(problem.request_count);
            problem.node_by_context.assign(context.size(), -1);
            for (int i = 0; i < problem.request_count; ++i) {
                const int pickup = context.index_of(tasks[i].pickup);
                const int delivery = context.index_of(tasks[i].delivery);
                assert(pickup >= 0 && delivery >= 0 && pickup != delivery);
                assert(problem.node_by_context[pickup] < 0 &&
                       problem.node_by_context[delivery] < 0);
                problem.pickup_context[i] = pickup;
                problem.delivery_context[i] = delivery;
                problem.node_by_context[pickup] = i * 2;
                problem.node_by_context[delivery] = i * 2 + 1;
                problem.quantity[i] = tasks[i].quantity;
                problem.reward[i] = tasks[i].reward;
                problem.pickup_service[i] =
                    static_cast<Calc>(tasks[i].pickup_service_cost);
                problem.delivery_service[i] =
                    static_cast<Calc>(tasks[i].delivery_service_cost);
                problem.mandatory[i] = tasks[i].mandatory ? 1 : 0;
            }
            problem.start.resize(problem.vehicle_count);
            problem.finish.resize(problem.vehicle_count);
            problem.capacity.resize(problem.vehicle_count);
            problem.max_cost.resize(problem.vehicle_count);
            for (int i = 0; i < problem.vehicle_count; ++i) {
                problem.start[i] = context.index_of(vehicles[i].start);
                problem.finish[i] = context.index_of(vehicles[i].finish);
                assert(problem.start[i] >= 0 && problem.finish[i] >= 0);
                problem.capacity[i] = vehicles[i].capacity;
                problem.max_cost[i] = static_cast<Calc>(vehicles[i].max_cost);
            }
            problem.exact_count = exact_count;

            State initial;
            if (initial_routes) {
                initial.routes.assign(problem.vehicle_count, {});
                assert(initial_routes->size() == vehicles.size());
                const int route_count = std::min(static_cast<int>(initial_routes->size()),
                                                 problem.vehicle_count);
                for (int r = 0; r < route_count; ++r) {
                    initial.routes[r].reserve((*initial_routes)[r].size());
                    for (Vertex vertex : (*initial_routes)[r]) {
                        const int context_vertex = context.index_of(vertex);
                        assert(context_vertex >= 0);
                        const int node = context_vertex < 0
                                             ? -1
                                             : problem.node_by_context[context_vertex];
                        assert(node >= 0);
                        if (node >= 0) initial.routes[r].push_back(node);
                    }
                }
                rebuild_pair_state<Selection>(initial);
                assert(initial.resource_feasible);
                if constexpr (Selection == SelectionKind::ExactCount) {
                    assert(initial.selected_count <= exact_count);
                }

            }

            auto run_search = [&]() -> State {
                SearchControl control(param, stats);
                FastRng rng(param.seed);
                const auto construction_begin = std::chrono::steady_clock::now();
                State best;
                if (initial_routes) {
                    best = std::move(initial);
                    repair_pair_state<Selection>(best, control);
                } else {
                    best.routes.assign(problem.vehicle_count, {});
                    rebuild_pair_state<Selection>(best);
                    std::vector<int> required;
                    for (int request = 0; request < problem.request_count; ++request) {
                        if constexpr (Selection == SelectionKind::AllMandatory) {
                            required.push_back(request);
                        } else if (problem.mandatory[request]) {
                            required.push_back(request);
                        }
                    }
                    std::sort(required.begin(), required.end(), [&](int lhs, int rhs) {
                        if (problem.quantity[lhs] != problem.quantity[rhs]) {
                            return problem.quantity[lhs] > problem.quantity[rhs];
                        }
                        return lhs < rhs;
                    });
                    insert_required_pairs<Selection>(best, std::move(required), control, false);
                    if constexpr (Selection != SelectionKind::AllMandatory) {
                        fill_pair_selection<Selection>(best, control);
                    }

                }
                stats->construction_ms += elapsed_ms(construction_begin);
                // K=0 の実行可能解は空経路だけで、探索しても改善しない。
                if constexpr (Selection == SelectionKind::ExactCount) {
                    if (problem.exact_count == 0 && best.feasible) return best;
                }
                if (problem.request_count == 0 || problem.vehicle_count == 0 ||
                    control.timed_out) {
                    return best;
                }
                State current = best;

                const auto lns_begin = std::chrono::steady_clock::now();
                auto destroy_pair_items = [&](State& state, int round) -> std::size_t {
                    auto pair_removal_saving = [&](int request) -> Calc {
                        const int route_index = state.request_route[request];
                        const auto& route = state.routes[route_index];
                        // 最後の依頼を外すと空経路の費用は0になる。
                        if (route.size() == 2) return state.route_cost[route_index];
                        const int pickup_position = state.pickup_position[request];
                        const int delivery_position = state.delivery_position[request];
                        const int pickup = problem.pickup_context[request];
                        const int delivery = problem.delivery_context[request];
                        const int before_pickup =
                            pickup_position == 0
                                ? problem.start[route_index]
                                : pair_context_vertex(route[pickup_position - 1]);
                        const int after_delivery =
                            delivery_position + 1 == static_cast<int>(route.size())
                                ? problem.finish[route_index]
                                : pair_context_vertex(route[delivery_position + 1]);

                        // 隣接するpairは3辺を1辺へ置き換える
                        if (delivery_position == pickup_position + 1) {
                            return context.distance(before_pickup, pickup) +
                                   problem.pickup_service[request] +
                                   context.distance(pickup, delivery) +
                                   problem.delivery_service[request] +
                                   context.distance(delivery, after_delivery) -
                                   context.distance(before_pickup, after_delivery);
                        }

                        // 離れたpairは両端点の除去差分を独立に加算できる
                        const int after_pickup =
                            pair_context_vertex(route[pickup_position + 1]);
                        const int before_delivery =
                            pair_context_vertex(route[delivery_position - 1]);
                        const auto pickup_saving =
                            context.distance(before_pickup, pickup) +
                            problem.pickup_service[request] +
                            context.distance(pickup, after_pickup) -
                            context.distance(before_pickup, after_pickup);
                        const auto delivery_saving =
                            context.distance(before_delivery, delivery) +
                            problem.delivery_service[request] +
                            context.distance(delivery, after_delivery) -
                            context.distance(before_delivery, after_delivery);
                        return pickup_saving + delivery_saving;
                    };
                    std::vector<int> selected;
                    for (int request = 0; request < problem.request_count; ++request) {
                        if (state.selected[request]) selected.push_back(request);
                    }
                    if (selected.empty()) return 0;
                    const int percent = 10 + rng.index(16);
                    const int remove_count = std::clamp(
                        static_cast<int>(selected.size()) * percent / 100, 1,
                        std::min<int>(48, static_cast<int>(selected.size())));
                    auto sort_by_key = [&](auto key, bool descending) {
                        std::vector<std::pair<Calc, int>> ranking;
                        ranking.reserve(selected.size());
                        for (int item : selected) ranking.emplace_back(key(item), item);
                        std::sort(ranking.begin(), ranking.end(), [&](const auto& lhs, const auto& rhs) {
                            if (lhs.first != rhs.first) return descending ? lhs.first > rhs.first : lhs.first < rhs.first;
                            return lhs.second < rhs.second;
                        });
                        for (std::size_t i = 0; i < selected.size(); ++i) selected[i] = ranking[i].second;
                    };
                    const int mode = round % 3;
                    if (mode == 0) {
                        for (int i = static_cast<int>(selected.size()) - 1; i > 0; --i) {
                            std::swap(selected[i], selected[rng.index(i + 1)]);
                        }
                    } else if (mode == 1) {
                        sort_by_key(pair_removal_saving, true);
                    } else {
                        const int seed = selected[rng.index(static_cast<int>(selected.size()))];
                        auto relatedness = [&](int request) {
                            return std::min(
                                       context.distance(problem.pickup_context[seed],
                                                        problem.pickup_context[request]),
                                       context.distance(problem.pickup_context[request],
                                                        problem.pickup_context[seed])) +
                                   std::min(
                                       context.distance(problem.delivery_context[seed],
                                                        problem.delivery_context[request]),
                                       context.distance(problem.delivery_context[request],
                                                        problem.delivery_context[seed]));
                        };
                        sort_by_key(relatedness, false);
                    }
                    selected.resize(remove_count);

                    std::vector<unsigned char> removed(problem.request_count, 0);
                    for (int item : selected) removed[item] = 1;
                    for (auto& route : state.routes) {
                        std::erase_if(route, [&](int node) { return removed[node / 2] != 0; });
                    }
                    return selected.size();
                };
                int round = 0;
                while (!control.expired_now()) {
                    State candidate = current;
                    stats->destroyed_items += destroy_pair_items(candidate, round);
                    rebuild_pair_state<Selection>(candidate);
                    repair_pair_state<Selection>(candidate, control);
                    if (control.timed_out) break;
                    ++stats->lns_rounds;
                    if (better_state<Selection>(candidate, best,
                                                     problem.exact_count)) {
                        best = candidate;
                        current = std::move(candidate);
                        ++stats->best_updates;
                    } else if (better_state<Selection>(candidate, current,
                                                            problem.exact_count) ||
                               within_record_tolerance<Selection>(
                                   candidate, best)) {
                        current = std::move(candidate);
                    }
                    ++round;
                    if ((round & 31) == 0) current = best;
                }
                stats->lns_ms += elapsed_ms(lns_begin);
                return best;
            };
            State state = run_search();
            RoutingSolution<Vertex, Calc, Reward> result;
            result.routes.resize(state.routes.size());
            for (int r = 0; r < static_cast<int>(state.routes.size()); ++r) {
                result.routes[r].reserve(state.routes[r].size());
                for (int node : state.routes[r]) {
                    const int request = node / 2;
                    result.routes[r].push_back(((node & 1) == 0)
                                                   ? tasks[request].pickup
                                                   : tasks[request].delivery);
                }
            }
            for (int request = 0; request < problem.request_count; ++request) {
                if (!state.selected[request]) result.unserved_items.push_back(request);
            }
            result.travel_cost = state.total_cost;
            result.collected_reward = state.collected_reward;
            result.feasible = state.feasible;
            stats->total_ms = elapsed_ms(total_begin);
            return result;
        }
    };

    template <SelectionKind Selection, bool UseCapacity, class Load, class Reward, class ServiceCost,
        class CostLimit, class F>
    static auto solve_single(
        std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
        const F& dist, int exact_count, const RoutingParam& param, RoutingStats* stats) {
        const auto total_begin = std::chrono::steady_clock::now();
        RoutingStats context_stats;
        std::vector<Vertex> vertices;
        vertices.reserve(tasks.size() + vehicles.size() * 2);
        for (const auto& task : tasks) vertices.push_back(task.vertex);
        for (const auto& vehicle : vehicles) {
            vertices.push_back(vehicle.start);
            vertices.push_back(vehicle.finish);
        }

        RoutingContext context(vertices, dist, param.symmetric, &context_stats);
        RoutingParam search_param = param;
        search_param.deadline = std::min(
            search_param.deadline,
            total_begin + std::chrono::milliseconds(param.time_limit_ms));
        auto result = SingleSolver<Load, Reward>{context, {}}.template solve<Selection, UseCapacity>(tasks,
            vehicles, exact_count, search_param, stats);
        if (stats) {
            stats->distance_build_ms = context_stats.distance_build_ms;
            stats->candidate_build_ms = context_stats.candidate_build_ms;
            stats->total_ms = elapsed_ms(total_begin);
        }
        return result;
    }

    template <SelectionKind Selection, class Load, class Reward, class ServiceCost, class CostLimit, class F>
    static auto solve_pair(
        std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
        const F& dist, int exact_count, const RoutingParam& param, RoutingStats* stats) {
        const auto total_begin = std::chrono::steady_clock::now();
        RoutingStats context_stats;
        std::vector<Vertex> vertices;
        vertices.reserve(tasks.size() * 2 + vehicles.size() * 2);
        for (const auto& task : tasks) {
            vertices.push_back(task.pickup);
            vertices.push_back(task.delivery);
        }
        for (const auto& vehicle : vehicles) {
            vertices.push_back(vehicle.start);
            vertices.push_back(vehicle.finish);
        }

        RoutingContext context(vertices, dist, param.symmetric, &context_stats);
        RoutingParam search_param = param;
        search_param.deadline = std::min(
            search_param.deadline,
            total_begin + std::chrono::milliseconds(param.time_limit_ms));
        auto result = PairSolver<Load, Reward>{context, {}}.template solve<Selection>(tasks, vehicles,
            exact_count, search_param, stats);
        if (stats) {
            stats->distance_build_ms = context_stats.distance_build_ms;
            stats->candidate_build_ms = context_stats.candidate_build_ms;
            stats->total_ms = elapsed_ms(total_begin);
        }
        return result;
    }

    template <class C, class V, class F>
    friend auto make_routing_context(std::span<const V> vertices, const F& dist,
                              bool symmetric);

    template <class C, class V, class Load, class Reward,
              class ServiceCost, class CostLimit, class F>
    friend auto solve_cvrp(
        std::span<const RouteTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        const F& dist, const RoutingParam& param, RoutingStats* stats);

    template <class C, class V, class Load, class Reward,
              class ServiceCost, class CostLimit, class F>
    friend auto solve_orienteering(
        std::span<const RouteTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        const F& dist, const RoutingParam& param, RoutingStats* stats);

    template <class C, class V, class Load, class Reward,
              class ServiceCost, class CostLimit, class F>
    friend auto solve_prize_collecting_vrp(
        std::span<const RouteTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        const F& dist, const RoutingParam& param, RoutingStats* stats);

    template <class C, class V, class Load, class Reward,
              class ServiceCost, class CostLimit, class F>
    friend auto solve_selective_vrp(
        std::span<const RouteTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        const F& dist, int selected_count, const RoutingParam& param,
        RoutingStats* stats);

    template <class C, class V, class Load, class Reward,
              class ServiceCost, class CostLimit, class F>
    friend auto solve_pdvrp(
        std::span<const PickupDeliveryTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        const F& dist, const RoutingParam& param, RoutingStats* stats);

    template <class C, class V, class Load, class Reward,
              class ServiceCost, class CostLimit, class F>
    friend auto solve_prize_collecting_pdvrp(
        std::span<const PickupDeliveryTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        const F& dist, const RoutingParam& param, RoutingStats* stats);

    template <class C, class V, class Load, class Reward,
              class ServiceCost, class CostLimit, class F>
    friend auto solve_selective_pdvrp(
        std::span<const PickupDeliveryTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        const F& dist, int selected_count, const RoutingParam& param,
        RoutingStats* stats);

    template <class V, class C, class Load, class Reward,
              class ServiceCost, class CostLimit>
    friend auto solve_cvrp(
        const RoutingContext<V, C>& context,
        std::span<const RouteTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        const RoutingParam& param, RoutingStats* stats);

    template <class V, class C, class Load, class Reward,
              class ServiceCost, class CostLimit>
    friend auto solve_orienteering(
        const RoutingContext<V, C>& context,
        std::span<const RouteTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        const RoutingParam& param, RoutingStats* stats);

    template <class V, class C, class Load, class Reward,
              class ServiceCost, class CostLimit>
    friend auto solve_prize_collecting_vrp(
        const RoutingContext<V, C>& context,
        std::span<const RouteTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        const RoutingParam& param, RoutingStats* stats);

    template <class V, class C, class Load, class Reward,
              class ServiceCost, class CostLimit>
    friend auto solve_selective_vrp(
        const RoutingContext<V, C>& context,
        std::span<const RouteTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        int selected_count, const RoutingParam& param,
        RoutingStats* stats);

    template <class V, class C, class Load, class Reward,
              class ServiceCost, class CostLimit>
    friend auto improve_cvrp(
        const RoutingContext<V, C>& context,
        std::span<const RouteTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        std::span<const std::vector<V>> initial_routes,
        const RoutingParam& param, RoutingStats* stats);

    template <class V, class C, class Load, class Reward,
              class ServiceCost, class CostLimit>
    friend auto improve_orienteering(
        const RoutingContext<V, C>& context,
        std::span<const RouteTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        std::span<const std::vector<V>> initial_routes,
        const RoutingParam& param, RoutingStats* stats);

    template <class V, class C, class Load, class Reward,
              class ServiceCost, class CostLimit>
    friend auto improve_prize_collecting_vrp(
        const RoutingContext<V, C>& context,
        std::span<const RouteTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        std::span<const std::vector<V>> initial_routes,
        const RoutingParam& param, RoutingStats* stats);

    template <class V, class C, class Load, class Reward,
              class ServiceCost, class CostLimit>
    friend auto improve_selective_vrp(
        const RoutingContext<V, C>& context,
        std::span<const RouteTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        std::span<const std::vector<V>> initial_routes, int selected_count,
        const RoutingParam& param, RoutingStats* stats);

    template <class V, class C, class Load, class Reward,
              class ServiceCost, class CostLimit>
    friend auto improve_pdvrp(
        const RoutingContext<V, C>& context,
        std::span<const PickupDeliveryTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        std::span<const std::vector<V>> initial_routes,
        const RoutingParam& param, RoutingStats* stats);

    template <class V, class C, class Load, class Reward,
              class ServiceCost, class CostLimit>
    friend auto improve_prize_collecting_pdvrp(
        const RoutingContext<V, C>& context,
        std::span<const PickupDeliveryTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        std::span<const std::vector<V>> initial_routes,
        const RoutingParam& param, RoutingStats* stats);

    template <class V, class C, class Load, class Reward,
              class ServiceCost, class CostLimit>
    friend auto improve_selective_pdvrp(
        const RoutingContext<V, C>& context,
        std::span<const PickupDeliveryTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        std::span<const std::vector<V>> initial_routes, int selected_count,
        const RoutingParam& param, RoutingStats* stats);

    template <class V, class C, class Load, class Reward,
              class ServiceCost, class CostLimit>
    friend auto solve_pdvrp(
        const RoutingContext<V, C>& context,
        std::span<const PickupDeliveryTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        const RoutingParam& param, RoutingStats* stats);

    template <class V, class C, class Load, class Reward,
              class ServiceCost, class CostLimit>
    friend auto solve_prize_collecting_pdvrp(
        const RoutingContext<V, C>& context,
        std::span<const PickupDeliveryTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        const RoutingParam& param, RoutingStats* stats);

    template <class V, class C, class Load, class Reward,
              class ServiceCost, class CostLimit>
    friend auto solve_selective_pdvrp(
        const RoutingContext<V, C>& context,
        std::span<const PickupDeliveryTask<V, Load, Reward, ServiceCost>> tasks,
        std::span<const RouteVehicle<V, Load, CostLimit>> vehicles,
        int selected_count, const RoutingParam& param,
        RoutingStats* stats);
};

// 頂点集合の距離行列と候補近傍を構築する。O(n^2)
template <class Calc, class Vertex, class F>
auto make_routing_context(std::span<const Vertex> vertices, const F& dist,
                          bool symmetric) {
    using Dist = std::invoke_result_t<F, Vertex, Vertex>;
    using ResultCalc = std::conditional_t<std::is_void_v<Calc>,
                                          std::conditional_t<std::floating_point<std::remove_cvref_t<Dist>>,
                                              double, long long>, Calc>;
    return RoutingContext<Vertex, ResultCalc>(vertices, dist, symmetric, nullptr);
}

// vector から距離行列と候補近傍を構築する。O(n^2)
template <class Calc = void, class Vertex, class Allocator, class F>
auto make_routing_context(const std::vector<Vertex, Allocator>& vertices,
                          const F& dist, bool symmetric = true) {
    return make_routing_context<Calc>(std::span<const Vertex>(vertices), dist,
                                      symmetric);
}

// 全顧客必須の容量付き複数経路を構築・改善する。探索量は時間制限に依存、初期化 O(n^2)
template <class Calc, class Vertex, class Load, class Reward,
          class ServiceCost, class CostLimit, class F>
auto solve_cvrp(
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const F& dist, const RoutingParam& param, RoutingStats* stats) {
    using Dist = std::invoke_result_t<F, Vertex, Vertex>;
    using ResultCalc = std::conditional_t<std::is_void_v<Calc>,
        std::conditional_t<std::floating_point<std::remove_cvref_t<Dist>>, double, long long>, Calc>;
    return RoutingContext<Vertex, ResultCalc>::template solve_single<RoutingContext<Vertex,
        ResultCalc>::SelectionKind::AllMandatory, true>(
        tasks, vehicles, dist, static_cast<int>(tasks.size()), param, stats);
}

// 報酬付き任意頂点を経路上限内で選択・順序最適化する。探索量は時間制限に依存、初期化 O(n^2)
template <class Calc, class Vertex, class Load, class Reward,
          class ServiceCost, class CostLimit, class F>
auto solve_orienteering(
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const F& dist, const RoutingParam& param, RoutingStats* stats) {
    using Dist = std::invoke_result_t<F, Vertex, Vertex>;
    using ResultCalc = std::conditional_t<std::is_void_v<Calc>,
        std::conditional_t<std::floating_point<std::remove_cvref_t<Dist>>, double, long long>, Calc>;
    return RoutingContext<Vertex, ResultCalc>::template solve_single<RoutingContext<Vertex,
        ResultCalc>::SelectionKind::MaximizeReward, false>(
        tasks, vehicles, dist, 0, param, stats);
}

// 容量と経路上限を守りながら報酬付き顧客を選択する。探索量は時間制限に依存、初期化 O(n^2)
template <class Calc, class Vertex, class Load, class Reward,
          class ServiceCost, class CostLimit, class F>
auto solve_prize_collecting_vrp(
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const F& dist, const RoutingParam& param, RoutingStats* stats) {
    using Dist = std::invoke_result_t<F, Vertex, Vertex>;
    using ResultCalc = std::conditional_t<std::is_void_v<Calc>,
        std::conditional_t<std::floating_point<std::remove_cvref_t<Dist>>, double, long long>, Calc>;
    return RoutingContext<Vertex, ResultCalc>::template solve_single<RoutingContext<Vertex,
        ResultCalc>::SelectionKind::MaximizeReward, true>(
        tasks, vehicles, dist, 0, param, stats);
}

// 訪問顧客数を固定して容量・経路上限付き経路を求める。探索量は時間制限に依存、初期化 O(n^2)
template <class Calc, class Vertex, class Load, class Reward,
          class ServiceCost, class CostLimit, class F>
auto solve_selective_vrp(
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const F& dist, int selected_count, const RoutingParam& param,
    RoutingStats* stats) {
    using Dist = std::invoke_result_t<F, Vertex, Vertex>;
    using ResultCalc = std::conditional_t<std::is_void_v<Calc>,
        std::conditional_t<std::floating_point<std::remove_cvref_t<Dist>>, double, long long>, Calc>;
    return RoutingContext<Vertex, ResultCalc>::template solve_single<RoutingContext<Vertex,
        ResultCalc>::SelectionKind::ExactCount, true>(
        tasks, vehicles, dist, selected_count, param, stats);
}

// 全依頼必須の pickup-delivery VRP を解く。探索量は時間制限に依存、初期化 O(n^2)
template <class Calc, class Vertex, class Load, class Reward,
          class ServiceCost, class CostLimit, class F>
auto solve_pdvrp(
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const F& dist, const RoutingParam& param, RoutingStats* stats) {
    using Dist = std::invoke_result_t<F, Vertex, Vertex>;
    using ResultCalc = std::conditional_t<std::is_void_v<Calc>,
        std::conditional_t<std::floating_point<std::remove_cvref_t<Dist>>, double, long long>, Calc>;
    return RoutingContext<Vertex, ResultCalc>::template solve_pair<RoutingContext<Vertex,
        ResultCalc>::SelectionKind::AllMandatory>(
        tasks, vehicles, dist, static_cast<int>(tasks.size()), param, stats);
}

// 報酬付き pickup-delivery 依頼を容量・経路上限内で選ぶ。探索量は時間制限に依存、初期化 O(n^2)
template <class Calc, class Vertex, class Load, class Reward,
          class ServiceCost, class CostLimit, class F>
auto solve_prize_collecting_pdvrp(
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const F& dist, const RoutingParam& param, RoutingStats* stats) {
    using Dist = std::invoke_result_t<F, Vertex, Vertex>;
    using ResultCalc = std::conditional_t<std::is_void_v<Calc>,
        std::conditional_t<std::floating_point<std::remove_cvref_t<Dist>>, double, long long>, Calc>;
    return RoutingContext<Vertex, ResultCalc>::template solve_pair<RoutingContext<Vertex,
        ResultCalc>::SelectionKind::MaximizeReward>(
        tasks, vehicles, dist, 0, param, stats);
}

// 依頼数固定の pickup-delivery VRP を解く。探索量は時間制限に依存、初期化 O(n^2)
template <class Calc, class Vertex, class Load, class Reward,
          class ServiceCost, class CostLimit, class F>
auto solve_selective_pdvrp(
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const F& dist, int selected_count, const RoutingParam& param,
    RoutingStats* stats) {
    using Dist = std::invoke_result_t<F, Vertex, Vertex>;
    using ResultCalc = std::conditional_t<std::is_void_v<Calc>,
        std::conditional_t<std::floating_point<std::remove_cvref_t<Dist>>, double, long long>, Calc>;
    return RoutingContext<Vertex, ResultCalc>::template solve_pair<RoutingContext<Vertex,
        ResultCalc>::SelectionKind::ExactCount>(
        tasks, vehicles, dist, selected_count, param, stats);
}

// 構築済み context を再利用して全顧客必須の容量付き複数経路を解く。探索量は時間制限に依存
template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto solve_cvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const RoutingParam& param, RoutingStats* stats) {
    return typename RoutingContext<Vertex, Calc>::template SingleSolver<Load, Reward>{context,
        {}}.template solve<RoutingContext<Vertex, Calc>::SelectionKind::AllMandatory, true>(
        tasks, vehicles, static_cast<int>(tasks.size()), param, stats);
}

// 構築済み context を再利用して Team Orienteering を解く。探索量は時間制限に依存
template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto solve_orienteering(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const RoutingParam& param, RoutingStats* stats) {
    return typename RoutingContext<Vertex, Calc>::template SingleSolver<Load, Reward>{context,
        {}}.template solve<RoutingContext<Vertex, Calc>::SelectionKind::MaximizeReward, false>(
        tasks, vehicles, 0, param, stats);
}

// 構築済み context を再利用して prize-collecting VRP を解く。探索量は時間制限に依存
template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto solve_prize_collecting_vrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const RoutingParam& param, RoutingStats* stats) {
    return typename RoutingContext<Vertex, Calc>::template SingleSolver<Load, Reward>{context,
        {}}.template solve<RoutingContext<Vertex, Calc>::SelectionKind::MaximizeReward, true>(
        tasks, vehicles, 0, param, stats);
}

// 構築済み context を再利用して訪問数固定の VRP を解く。探索量は時間制限に依存
template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto solve_selective_vrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    int selected_count, const RoutingParam& param,
    RoutingStats* stats) {
    return typename RoutingContext<Vertex, Calc>::template SingleSolver<Load, Reward>{context,
        {}}.template solve<RoutingContext<Vertex, Calc>::SelectionKind::ExactCount, true>(
        tasks, vehicles, selected_count, param, stats);
}

// 既存の全顧客必須 CVRP 解を context 再利用で改善する。探索量は時間制限に依存
template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto improve_cvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    std::span<const std::vector<Vertex>> initial_routes,
    const RoutingParam& param, RoutingStats* stats) {
    return typename RoutingContext<Vertex, Calc>::template SingleSolver<Load, Reward>{context,
        {}}.template solve<RoutingContext<Vertex, Calc>::SelectionKind::AllMandatory, true>(
        tasks, vehicles, static_cast<int>(tasks.size()), param, stats, initial_routes);
}

// 既存の Team Orienteering 解を context 再利用で改善する。探索量は時間制限に依存
template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto improve_orienteering(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    std::span<const std::vector<Vertex>> initial_routes,
    const RoutingParam& param, RoutingStats* stats) {
    return typename RoutingContext<Vertex, Calc>::template SingleSolver<Load, Reward>{context,
        {}}.template solve<RoutingContext<Vertex, Calc>::SelectionKind::MaximizeReward, false>(
        tasks, vehicles, 0, param, stats, initial_routes);
}

// 既存の prize-collecting VRP 解を context 再利用で改善する。探索量は時間制限に依存
template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto improve_prize_collecting_vrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    std::span<const std::vector<Vertex>> initial_routes,
    const RoutingParam& param, RoutingStats* stats) {
    return typename RoutingContext<Vertex, Calc>::template SingleSolver<Load, Reward>{context,
        {}}.template solve<RoutingContext<Vertex, Calc>::SelectionKind::MaximizeReward, true>(
        tasks, vehicles, 0, param, stats, initial_routes);
}

// 既存の訪問数固定 VRP 解を context 再利用で改善する。探索量は時間制限に依存
template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto improve_selective_vrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const RouteTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    std::span<const std::vector<Vertex>> initial_routes, int selected_count,
    const RoutingParam& param, RoutingStats* stats) {
    return typename RoutingContext<Vertex, Calc>::template SingleSolver<Load, Reward>{context,
        {}}.template solve<RoutingContext<Vertex, Calc>::SelectionKind::ExactCount, true>(
        tasks, vehicles, selected_count, param, stats, initial_routes);
}

// 既存の全依頼必須 PDVRP 解を context 再利用で改善する。探索量は時間制限に依存
template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto improve_pdvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    std::span<const std::vector<Vertex>> initial_routes,
    const RoutingParam& param, RoutingStats* stats) {
    return typename RoutingContext<Vertex, Calc>::template PairSolver<Load, Reward>{context,
        {}}.template solve<RoutingContext<Vertex, Calc>::SelectionKind::AllMandatory>(
        tasks, vehicles, static_cast<int>(tasks.size()), param, stats, initial_routes);
}

// 既存の報酬付き PDVRP 解を context 再利用で改善する。探索量は時間制限に依存
template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto improve_prize_collecting_pdvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    std::span<const std::vector<Vertex>> initial_routes,
    const RoutingParam& param, RoutingStats* stats) {
    return typename RoutingContext<Vertex, Calc>::template PairSolver<Load, Reward>{context,
        {}}.template solve<RoutingContext<Vertex, Calc>::SelectionKind::MaximizeReward>(
        tasks, vehicles, 0, param, stats, initial_routes);
}

// 既存の依頼数固定 PDVRP 解を context 再利用で改善する。探索量は時間制限に依存
template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto improve_selective_pdvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    std::span<const std::vector<Vertex>> initial_routes, int selected_count,
    const RoutingParam& param, RoutingStats* stats) {
    return typename RoutingContext<Vertex, Calc>::template PairSolver<Load, Reward>{context,
        {}}.template solve<RoutingContext<Vertex, Calc>::SelectionKind::ExactCount>(
        tasks, vehicles, selected_count, param, stats, initial_routes);
}

// 構築済み context を再利用して全依頼必須の PDVRP を解く。探索量は時間制限に依存
template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto solve_pdvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const RoutingParam& param, RoutingStats* stats) {
    return typename RoutingContext<Vertex, Calc>::template PairSolver<Load, Reward>{context,
        {}}.template solve<RoutingContext<Vertex, Calc>::SelectionKind::AllMandatory>(
        tasks, vehicles, static_cast<int>(tasks.size()), param, stats);
}

// 構築済み context を再利用して報酬付き PDVRP を解く。探索量は時間制限に依存
template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto solve_prize_collecting_pdvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    const RoutingParam& param, RoutingStats* stats) {
    return typename RoutingContext<Vertex, Calc>::template PairSolver<Load, Reward>{context,
        {}}.template solve<RoutingContext<Vertex, Calc>::SelectionKind::MaximizeReward>(
        tasks, vehicles, 0, param, stats);
}

// 構築済み context を再利用して依頼数固定の PDVRP を解く。探索量は時間制限に依存
template <class Vertex, class Calc, class Load, class Reward,
          class ServiceCost, class CostLimit>
auto solve_selective_pdvrp(
    const RoutingContext<Vertex, Calc>& context,
    std::span<const PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>> tasks,
    std::span<const RouteVehicle<Vertex, Load, CostLimit>> vehicles,
    int selected_count, const RoutingParam& param,
    RoutingStats* stats) {
    return typename RoutingContext<Vertex, Calc>::template PairSolver<Load, Reward>{context,
        {}}.template solve<RoutingContext<Vertex, Calc>::SelectionKind::ExactCount>(
        tasks, vehicles, selected_count, param, stats);
}

}  // namespace routing

#if __INCLUDE_LEVEL__ == 0

int main() {
    enum class SelectionKind : unsigned char {
        AllMandatory,
        MaximizeReward,
        ExactCount,
    };

    struct FastRng {
        std::uint64_t state;

        explicit FastRng(std::uint64_t seed) : state(seed) {}

        std::uint64_t operator()() {
            std::uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return z ^ (z >> 31);
        }

        int index(int n) {
            assert(n > 0);
            return static_cast<int>((*this)() % static_cast<std::uint64_t>(n));
        }
    };

    using Task = routing::RouteTask<int, int, long long, int>;
    using PairTask = routing::PickupDeliveryTask<int, int, long long, int>;
    using Vehicle = routing::RouteVehicle<int, int, long long>;
    using Solution = routing::RoutingSolution<int, long long, long long>;

    struct MatrixDistance {
        const std::vector<std::vector<long long>>* matrix = nullptr;
        long long operator()(int from, int to) const { return (*matrix)[from][to]; }
    };

    auto require = [&](bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAILED: " << message << '\n';
            std::abort();
        }
    };

    auto validate_single = [&](const std::vector<Task>& tasks,
                         const std::vector<Vehicle>& vehicles,
                         const MatrixDistance& dist, const Solution& solution,
                         SelectionKind selection, int exact_count) {
        auto single_route_cost = [&](const std::vector<int>& route, const Vehicle& vehicle) -> long long {
            if (route.empty()) return 0;
            std::vector<int> task_by_vertex(dist.matrix->size(), -1);
            for (int i = 0; i < static_cast<int>(tasks.size()); ++i) {
                task_by_vertex[tasks[i].vertex] = i;
            }
            long long result = dist(vehicle.start, route.front());
            for (int i = 0; i < static_cast<int>(route.size()); ++i) {
                result += tasks[task_by_vertex[route[i]]].service_cost;
                if (i + 1 < static_cast<int>(route.size())) {
                    result += dist(route[i], route[i + 1]);
                }
            }
            result += dist(route.back(), vehicle.finish);
            return result;
        };
        require(solution.routes.size() == vehicles.size(), "single route count");
        std::vector<int> task_by_vertex(dist.matrix->size(), -1);
        for (int i = 0; i < static_cast<int>(tasks.size()); ++i) {
            task_by_vertex[tasks[i].vertex] = i;
        }
        std::vector<unsigned char> seen(tasks.size(), 0);
        long long total_cost = 0;
        long long total_reward = 0;
        int selected_count = 0;
        for (int r = 0; r < static_cast<int>(vehicles.size()); ++r) {
            int load = 0;
            for (int vertex : solution.routes[r]) {
                require(vertex >= 0 && vertex < static_cast<int>(task_by_vertex.size()),
                        "single vertex range");
                const int task = task_by_vertex[vertex];
                require(task >= 0 && !seen[task], "single duplicate or foreign vertex");
                seen[task] = 1;
                ++selected_count;
                load += tasks[task].demand;
                total_reward += tasks[task].reward;
            }
            require(load <= vehicles[r].capacity, "single capacity");
            const long long cost = single_route_cost(solution.routes[r], vehicles[r]);
            require(cost <= vehicles[r].max_cost, "single budget");
            total_cost += cost;
        }
        std::vector<int> expected_unserved;
        for (int i = 0; i < static_cast<int>(tasks.size()); ++i) {
            if (!seen[i]) expected_unserved.push_back(i);
            if (tasks[i].mandatory) require(seen[i], "single mandatory");
        }
        require(expected_unserved == solution.unserved_items, "single unserved list");
        require(total_cost == solution.travel_cost, "single reported cost");
        require(total_reward == solution.collected_reward, "single reported reward");
        if (selection == SelectionKind::AllMandatory) {
            require(selected_count == static_cast<int>(tasks.size()), "single all");
        }
        if (selection == SelectionKind::ExactCount) {
            require(selected_count == exact_count, "single exact count");
        }
        require(solution.feasible, "single feasible flag");
    };

    auto validate_pair = [&](const std::vector<PairTask>& tasks,
                       const std::vector<Vehicle>& vehicles,
                       const MatrixDistance& dist, const Solution& solution,
                       SelectionKind selection, int exact_count) {
        auto pair_route_cost = [&](const std::vector<int>& route, const Vehicle& vehicle) -> long long {
            if (route.empty()) return 0;
            std::vector<std::pair<int, bool>> endpoint(dist.matrix->size(), {-1, false});
            for (int i = 0; i < static_cast<int>(tasks.size()); ++i) {
                endpoint[tasks[i].pickup] = {i, true};
                endpoint[tasks[i].delivery] = {i, false};
            }
            long long result = dist(vehicle.start, route.front());
            for (int i = 0; i < static_cast<int>(route.size()); ++i) {
                const auto [request, pickup] = endpoint[route[i]];
                result += pickup ? tasks[request].pickup_service_cost
                                 : tasks[request].delivery_service_cost;
                if (i + 1 < static_cast<int>(route.size())) {
                    result += dist(route[i], route[i + 1]);
                }
            }
            result += dist(route.back(), vehicle.finish);
            return result;
        };
        require(solution.routes.size() == vehicles.size(), "pair route count");
        std::vector<std::pair<int, int>> endpoint(dist.matrix->size(), {-1, 0});
        for (int i = 0; i < static_cast<int>(tasks.size()); ++i) {
            endpoint[tasks[i].pickup] = {i, 1};
            endpoint[tasks[i].delivery] = {i, -1};
        }
        std::vector<int> pickup_route(tasks.size(), -1);
        std::vector<int> delivery_route(tasks.size(), -1);
        long long total_cost = 0;
        long long total_reward = 0;
        for (int r = 0; r < static_cast<int>(vehicles.size()); ++r) {
            int load = 0;
            std::vector<unsigned char> open(tasks.size(), 0);
            for (int vertex : solution.routes[r]) {
                require(vertex >= 0 && vertex < static_cast<int>(endpoint.size()),
                        "pair endpoint range");
                const auto [request, sign] = endpoint[vertex];
                require(request >= 0, "pair foreign endpoint");
                if (sign > 0) {
                    require(pickup_route[request] < 0, "pair duplicate pickup");
                    pickup_route[request] = r;
                    open[request] = 1;
                    load += tasks[request].quantity;
                    require(load <= vehicles[r].capacity, "pair capacity");
                } else {
                    require(open[request] && delivery_route[request] < 0,
                            "pair precedence");
                    delivery_route[request] = r;
                    open[request] = 0;
                    load -= tasks[request].quantity;
                    require(load >= 0, "pair nonnegative load");
                }
            }
            require(load == 0, "pair final load");
            const long long cost = pair_route_cost(solution.routes[r], vehicles[r]);
            require(cost <= vehicles[r].max_cost, "pair budget");
            total_cost += cost;
        }
        std::vector<int> expected_unserved;
        int selected_count = 0;
        for (int i = 0; i < static_cast<int>(tasks.size()); ++i) {
            require((pickup_route[i] < 0) == (delivery_route[i] < 0), "pair both ends");
            if (pickup_route[i] >= 0) {
                require(pickup_route[i] == delivery_route[i], "pair same route");
                ++selected_count;
                total_reward += tasks[i].reward;
            } else {
                expected_unserved.push_back(i);
            }
            if (tasks[i].mandatory) require(pickup_route[i] >= 0, "pair mandatory");
        }
        require(expected_unserved == solution.unserved_items, "pair unserved list");
        require(total_cost == solution.travel_cost, "pair reported cost");
        require(total_reward == solution.collected_reward, "pair reported reward");
        if (selection == SelectionKind::AllMandatory) {
            require(selected_count == static_cast<int>(tasks.size()), "pair all");
        }
        if (selection == SelectionKind::ExactCount) {
            require(selected_count == exact_count, "pair exact count");
        }
        require(solution.feasible, "pair feasible flag");
    };

    auto test_public_solvers = [&]() {
        auto make_matrix = [&](int n, std::uint64_t seed, bool symmetric) {
            FastRng rng(seed);
            std::vector<std::vector<long long>> result(n, std::vector<long long>(n));
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    if (i == j) continue;
                    if (symmetric && j < i) {
                        result[i][j] = result[j][i];
                    } else {
                        result[i][j] = 1 + static_cast<long long>(rng() % 100U);
                    }
                }
            }
            return result;
        };
        for (int iteration = 0; iteration < 80; ++iteration) {
            const bool symmetric = (iteration % 2) == 0;
            const int n = 5 + iteration % 10;
            const int vehicle_count = 1 + iteration % 4;
            auto matrix = make_matrix(2 * n + 1, 1000U + iteration, symmetric);
            MatrixDistance dist{&matrix};
            std::vector<Task> tasks;
            std::vector<PairTask> pairs;
            for (int i = 0; i < n; ++i) {
                tasks.push_back({i + 1, 1 + i % 4, 3 + i % 7, i % 3, false});
                pairs.push_back({i + 1, n + i + 1, 1 + i % 4, 3 + i % 7,
                                 i % 2, (i + 1) % 2, false});
            }
            std::vector<Vehicle> vehicles(vehicle_count);
            for (auto& vehicle : vehicles) vehicle = {0, 0, n * 4, 1000000};
            routing::RoutingParam param;
            param.time_limit_ms = 10000;
            param.max_move_evaluations = 30000;
            param.seed = static_cast<std::uint64_t>(iteration + 1);
            param.symmetric = symmetric;
            const std::span<const Task> task_span(tasks);
            const std::span<const PairTask> pair_span(pairs);
            const std::span<const Vehicle> vehicle_span(vehicles);

            for (auto& task : tasks) task.mandatory = true;
            auto cvrp = routing::solve_cvrp(task_span, vehicle_span, dist, param);
            validate_single(tasks, vehicles, dist, cvrp,
                            SelectionKind::AllMandatory, n);
            for (auto& task : tasks) task.mandatory = false;
            const int chosen = n / 2;
            auto selective = routing::solve_selective_vrp(
                task_span, vehicle_span, dist, chosen, param);
            validate_single(tasks, vehicles, dist, selective,
                            SelectionKind::ExactCount, chosen);
            auto prize = routing::solve_prize_collecting_vrp(
                task_span, vehicle_span, dist, param);
            validate_single(tasks, vehicles, dist, prize,
                            SelectionKind::MaximizeReward, 0);
            auto orienteering = routing::solve_orienteering(
                task_span, vehicle_span, dist, param);
            validate_single(tasks, vehicles, dist, orienteering,
                            SelectionKind::MaximizeReward, 0);

            if (iteration % 8 == 0) {
                std::vector<int> vertices(2 * n + 1);
                std::iota(vertices.begin(), vertices.end(), 0);
                auto context = routing::make_routing_context<long long>(
                    vertices, dist, symmetric);
                auto rebuilt = routing::solve_prize_collecting_vrp(
                    context, task_span, vehicle_span, param);
                validate_single(tasks, vehicles, dist, rebuilt,
                                SelectionKind::MaximizeReward, 0);
                auto improved = routing::improve_prize_collecting_vrp(
                    context, task_span, vehicle_span,
                    std::span<const std::vector<int>>(rebuilt.routes), param);
                validate_single(tasks, vehicles, dist, improved,
                                SelectionKind::MaximizeReward, 0);
                require(improved.collected_reward >= rebuilt.collected_reward,
                        "single warm start reward");
            }

            for (auto& task : pairs) task.mandatory = true;
            auto pdvrp = routing::solve_pdvrp(pair_span, vehicle_span, dist, param);
            validate_pair(pairs, vehicles, dist, pdvrp,
                          SelectionKind::AllMandatory, n);
            for (auto& task : pairs) task.mandatory = false;
            auto selective_pd = routing::solve_selective_pdvrp(
                pair_span, vehicle_span, dist, chosen, param);
            validate_pair(pairs, vehicles, dist, selective_pd,
                          SelectionKind::ExactCount, chosen);
            auto prize_pd = routing::solve_prize_collecting_pdvrp(
                pair_span, vehicle_span, dist, param);
            validate_pair(pairs, vehicles, dist, prize_pd,
                          SelectionKind::MaximizeReward, 0);
            if (iteration % 8 == 0) {
                std::vector<int> vertices(2 * n + 1);
                std::iota(vertices.begin(), vertices.end(), 0);
                auto context = routing::make_routing_context<long long>(
                    vertices, dist, symmetric);
                auto rebuilt = routing::solve_prize_collecting_pdvrp(
                    context, pair_span, vehicle_span, param);
                validate_pair(pairs, vehicles, dist, rebuilt,
                              SelectionKind::MaximizeReward, 0);
                auto improved = routing::improve_prize_collecting_pdvrp(
                    context, pair_span, vehicle_span,
                    std::span<const std::vector<int>>(rebuilt.routes), param);
                validate_pair(pairs, vehicles, dist, improved,
                              SelectionKind::MaximizeReward, 0);
                require(improved.collected_reward >= rebuilt.collected_reward,
                        "pair warm start reward");
            }
        }
    };
    test_public_solvers();
    std::cout << "routing_solver correctness tests: OK\n";
}
#endif
