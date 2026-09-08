/*
 * facility_location v11: AHCの全体問題・部分問題で使う重み付き離散k-median solver
 *
 * 選択施設集合 S (|S|=k) に対し、次の分離可能な目的関数を最小化する。
 *
 *   sum_i w[i] * min(fallback[i], min_{f in S} cost[f][i])
 *       + sum_{f in S} facility_cost[f]
 *
 * fallback_cost と facility_cost は省略可能で、省略時は通常の重み付きk-medianと
 * 完全に同じ意味になる。fixed_facilities と candidate_facilities を指定すると、
 * 全体解の一部だけを固定し、狭い候補集合内で高速にrepairできる。
 *
 * アルゴリズム:
 *   1. 重み付きD^2-samplingで複数の初期解を作る
 *   2. 最近施設と第2近傍施設を使うFastPAM型1-swapをeagerに適用する
 *   3. 最良解から最大2施設を交換し、再び局所探索するILSを繰り返す
 *
 * 制約と注意:
 *   - demand_count > 0、1 <= k <= candidate_countを前提とする
 *   - costとfallback_costは有限かつ非負、demand_weightは非負を前提とする
 *   - facility_costは符号付きでもよいが、Calcのオーバーフローは検査しない
 *   - 容量、施設間相互作用、連結性、k-center、可変kには対応しない
 *   - costはcost[facility * demand_count + demand]の候補施設優先配列
 * 計算量ではQ=需要点数、M=候補数、K=選択数、T=初期試行数、P=摂動数、
 * E=候補評価数、S=受理swap数、F=固定施設数を表す
 */
#pragma once

#include <bits/stdc++.h>

struct facility_location_options {
    using clock = std::chrono::steady_clock;

    // 探索を停止する絶対時刻。デフォルトでは時刻で停止しない
    clock::time_point deadline = clock::time_point::max();

    // 乱数seed。同じ入力・設定・実行環境では同じ探索順になる
    std::uint64_t seed = 1;

    // D^2-sampling初期解の試行数。1以上を指定する
    int initial_trials = 4;

    // ILS回数。-1ならdeadline指定時は時間まで、未指定時は32回。0ならILSを行わない
    int max_perturbations = -1;

    // 1-swapで評価する追加候補数の上限。初期解構築の仕事量は含まない
    std::uint64_t max_candidate_evaluations =
        std::numeric_limits<std::uint64_t>::max();
};

template <class Cost, class Calc = long long>
struct facility_location_problem {
    int demand_count = 0;
    int candidate_count = 0;

    // cost[facility * demand_count + demand] の候補施設優先配列
    std::vector<Cost> cost;

    // 空なら全需要点の重みを1とする
    std::vector<Calc> demand_weight;

    // 空ならfallbackなし。要素数demand_countなら、需要点ごとの外部供給コスト
    std::vector<Cost> fallback_cost;

    // 空なら全施設0。要素数candidate_countの施設選択に伴う単項コスト
    std::vector<Calc> facility_cost;
};

struct facility_location_restrictions {
    // 必ず選択し、swapで削除しない施設。重複は不可
    std::vector<int> fixed_facilities;

    // 選択を許す施設。空なら全候補を許す。fixedとの重複は許すが、内部で除外する
    std::vector<int> candidate_facilities;
};

struct facility_location_statistics {
    std::uint64_t candidate_evaluations = 0;
    std::uint64_t accepted_swaps = 0;
    int completed_initial_trials = 0;
    int completed_perturbations = 0;
    bool deadline_reached = false;
    bool evaluation_limit_reached = false;
};

template <class Calc>
struct facility_location_result {
    Calc cost{};
    std::vector<int> facilities;
    // fallbackが選ばれた需要点は-1
    std::vector<int> assignment;
    facility_location_statistics statistics;
};

template <class Cost, class Calc = long long>
facility_location_result<Calc> solve_k_median(
    const facility_location_problem<Cost, Calc>& problem,
    int facility_count,
    const facility_location_restrictions& restrictions,
    facility_location_options options = {});

template <class Cost, class Calc = long long>
facility_location_result<Calc> improve_k_median(
    const facility_location_problem<Cost, Calc>& problem,
    std::vector<int> initial_facilities,
    const facility_location_restrictions& restrictions,
    facility_location_options options = {});

// 同じproblem・k・restrictionsを反復して解く際に内部配列を再利用するworkspace。
// problemはworkspaceより長く生存しなければならない。
template <class Cost, class Calc = long long>
class facility_location_workspace {
    using problem_type = facility_location_problem<Cost, Calc>;
    using result_type = facility_location_result<Calc>;
    template <bool Generalized>
    class search_state {
        using clock = std::chrono::steady_clock;

        struct fixed_baseline_state {
            std::vector<int> nearest_facility;
            std::vector<Cost> nearest_cost;
            std::vector<Cost> second_cost;
            Calc selection_cost{};
        };

        const problem_type& problem;
        facility_location_options options;
        std::mt19937_64 rng;
        int demand_count;
        int candidate_count;
        int facility_count;

        std::vector<int> fixed_facilities;
        std::vector<unsigned char> fixed_mask;
        std::vector<int> movable_candidates;
        bool full_candidate_domain = false;
        std::unique_ptr<fixed_baseline_state> fixed_baseline;
        std::vector<int> facilities;
        std::vector<int> open_position;
        std::vector<int> nearest_facility;
        std::vector<int> owner_position;
        std::vector<Cost> nearest_cost;
        std::vector<Cost> second_cost;
        std::vector<Calc> weighted_nearest_cost;
        std::vector<Calc> weighted_second_cost;
        Calc current_cost{};

        std::vector<int> best_facilities;
        Calc best_cost{};

        std::vector<Calc> removal_adjustment;
        std::vector<int> candidate_order;
        std::vector<int> movable_positions;
        facility_location_statistics statistics;

        const Cost& distance(int facility, int demand) const {
            return problem.cost[
                static_cast<std::size_t>(facility) * demand_count + demand];
        }

        Calc weight(int demand) const {
            return problem.demand_weight.empty() ? Calc{1} : problem.demand_weight[demand];
        }

        Calc selection_cost(int facility) const {
            if constexpr (Generalized) {
                return problem.facility_cost.empty() ? Calc{} : problem.facility_cost[facility];
            } else {
                static_cast<void>(facility);
                return Calc{};
            }
        }

        bool expired() {
            if (options.deadline != clock::time_point::max() &&
                clock::now() >= options.deadline) {
                statistics.deadline_reached = true;
                return true;
            }
            return false;
        }

        std::uint64_t random_below(std::uint64_t upper) {
            assert(upper > 0);
            return rng() % upper;
        }

        bool evaluation_budget_exhausted() const {
            return statistics.candidate_evaluations >=
                   options.max_candidate_evaluations;
        }

        // 現在の施設配列のうち、swapで削除してよい位置を昇順で作る。
        void initialize_movable_positions() {
            if constexpr (!Generalized) {
                return;
            } else if (fixed_facilities.empty()) {
                return;
            }
            movable_positions.clear();
            for (int position = 0; position < facility_count; ++position) {
                if (!fixed_mask[facilities[position]]) {
                    movable_positions.push_back(position);
                }
            }
            assert(static_cast<int>(movable_positions.size()) ==
                   facility_count - static_cast<int>(fixed_facilities.size()));
        }

        void rebuild_state() {
            const auto rebuild = [&]<bool HasFallback>() {
                std::fill(open_position.begin(), open_position.end(), -1);
                for (int position = 0; position < facility_count; ++position) {
                    const int facility = facilities[position];
                    assert(0 <= facility && facility < candidate_count);
                    assert(open_position[facility] == -1);
                    open_position[facility] = position;
                }

                const Cost inf = std::numeric_limits<Cost>::max();
                if (Generalized && fixed_baseline) {
                    nearest_facility = fixed_baseline->nearest_facility;
                    nearest_cost = fixed_baseline->nearest_cost;
                    second_cost = fixed_baseline->second_cost;
                } else {
                    std::fill(nearest_facility.begin(), nearest_facility.end(), HasFallback ? -1 : candidate_count);
                    if constexpr (HasFallback) {
                        nearest_cost = problem.fallback_cost;
                    } else {
                        std::fill(nearest_cost.begin(), nearest_cost.end(), inf);
                    }
                    std::fill(second_cost.begin(), second_cost.end(), inf);
                }

                // 施設候補を外側にして、候補施設優先のコスト行列を連続走査する
                for (int position = 0; position < facility_count; ++position) {
                    const int facility = facilities[position];
                    if constexpr (Generalized) {
                        if (fixed_baseline && fixed_mask[facility]) continue;
                    }
                    const Cost* facility_cost = problem.cost.data() +
                                                static_cast<std::size_t>(facility) *
                                                    demand_count;
                    for (int demand = 0; demand < demand_count; ++demand) {
                        const Cost value = facility_cost[demand];
                        const bool wins = [&] {
                            if constexpr (HasFallback) {
                                return value < nearest_cost[demand] ||
                                       (value == nearest_cost[demand] &&
                                        (nearest_facility[demand] == -1 ||
                                         facility < nearest_facility[demand]));
                            } else {
                                return value < nearest_cost[demand] ||
                                       (value == nearest_cost[demand] &&
                                        facility < nearest_facility[demand]);
                            }
                        }();
                        if (wins) {
                            second_cost[demand] = nearest_cost[demand];
                            nearest_cost[demand] = value;
                            nearest_facility[demand] = facility;
                        } else if (value < second_cost[demand]) {
                            second_cost[demand] = value;
                        }
                    }
                }

                current_cost = Calc{};
                for (int demand = 0; demand < demand_count; ++demand) {
                    if constexpr (HasFallback) {
                        owner_position[demand] = nearest_facility[demand] == -1
                                                     ? -1
                                                     : open_position[nearest_facility[demand]];
                    } else {
                        owner_position[demand] = open_position[nearest_facility[demand]];
                    }
                    const Calc demand_weight = weight(demand);
                    weighted_nearest_cost[demand] =
                        demand_weight * static_cast<Calc>(nearest_cost[demand]);
                    weighted_second_cost[demand] =
                        second_cost[demand] == inf
                            ? std::numeric_limits<Calc>::max()
                            : demand_weight * static_cast<Calc>(second_cost[demand]);
                    current_cost += weighted_nearest_cost[demand];
                }
                if constexpr (Generalized) {
                    if (!problem.facility_cost.empty()) {
                        if (fixed_baseline) {
                            current_cost += fixed_baseline->selection_cost;
                            for (int facility : facilities) {
                                if (!fixed_mask[facility]) {
                                    current_cost += problem.facility_cost[facility];
                                }
                            }
                        } else {
                            for (int facility : facilities) {
                                current_cost += problem.facility_cost[facility];
                            }
                        }
                    }
                }
            };

            if constexpr (!Generalized) {
                rebuild.template operator()<false>();
            } else if (problem.fallback_cost.empty()) {
                rebuild.template operator()<false>();
            } else {
                rebuild.template operator()<true>();
            }
        }

        // 現在解が最良なら、施設集合と目的値を保存する
        void remember_best() {
            if (best_facilities.empty() || current_cost < best_cost) {
                best_cost = current_cost;
                best_facilities = facilities;
            }
        }

        // 未選択施設をランダムな位置から探して返す
        int random_closed_facility() {
            const int count = [&] {
                if constexpr (!Generalized) {
                    return candidate_count;
                } else {
                    return full_candidate_domain
                               ? candidate_count
                               : static_cast<int>(movable_candidates.size());
                }
            }();
            if (count == 0) return -1;
            const int start = static_cast<int>(random_below(count));
            for (int offset = 0; offset < count; ++offset) {
                const int index = (start + offset) % count;
                const int facility = [&] {
                    if constexpr (!Generalized) {
                        return index;
                    } else {
                        return full_candidate_domain ? index
                                                     : movable_candidates[index];
                    }
                }();
                if (open_position[facility] == -1) return facility;
            }
            return -1;
        }

        void descend_eager_swap() {
            struct swap_move {
                Calc delta{};
                int out_position = -1;
                int in_facility = -1;
            };

            const auto descend = [&]<bool BoundedEvaluations>() {
                const auto evaluate_candidate = [&]<bool HasFallback>(int in_facility) -> swap_move {
                    const bool has_facility_cost = Generalized && !problem.facility_cost.empty();
                    if (has_facility_cost) {
                        for (int position = 0; position < facility_count; ++position) {
                            removal_adjustment[position] =
                                -problem.facility_cost[facilities[position]];
                        }
                    } else {
                        std::fill(removal_adjustment.begin(), removal_adjustment.end(), Calc{});
                    }
                    Calc add_delta{};
                    if (has_facility_cost) {
                        add_delta = problem.facility_cost[in_facility];
                    }
                    const Cost* incoming_cost = problem.cost.data() +
                                                static_cast<std::size_t>(in_facility) *
                                                    demand_count;

                    // 施設追加だけの差分と、担当施設を削除する場合に必要な補正を分離する
                    // 加算順と補正先を保ち、候補評価ループの制御命令を減らす
                    #pragma GCC unroll 4
                    for (int demand = 0; demand < demand_count; ++demand) {
                        const Cost incoming = incoming_cost[demand];
                        const Calc weighted_incoming =
                            weight(demand) * static_cast<Calc>(incoming);
                        const Calc add_cost =
                            std::min(weighted_incoming, weighted_nearest_cost[demand]);
                        add_delta += add_cost - weighted_nearest_cost[demand];

                        const int owner = owner_position[demand];
                        const Calc swap_cost =
                            std::min(weighted_incoming, weighted_second_cost[demand]);
                        if constexpr (HasFallback) {
                            if (owner != -1) {
                                removal_adjustment[owner] += swap_cost - add_cost;
                            }
                        } else {
                            removal_adjustment[owner] += swap_cost - add_cost;
                        }
                    }

                    // 施設コストがなければ、削除補正は常に0以上。追加単体で
                    // 改善しない候補は、削除位置の全走査をせずに棄却できる。
                    if (!has_facility_cost) {
                        if (!(add_delta < Calc{})) {
                            return {Calc{}, -1, in_facility};
                        }
                    }

                    // 追加差分へ施設ごとの削除補正を足し、最良の削除施設を選ぶ
                    if (!Generalized || fixed_facilities.empty()) {
                        swap_move best{add_delta + removal_adjustment[0], 0, in_facility};
                        for (int position = 1; position < facility_count; ++position) {
                            const Calc delta = add_delta + removal_adjustment[position];
                            if (delta < best.delta) {
                                best.delta = delta;
                                best.out_position = position;
                            }
                        }
                        return best;
                    } else {
                        assert(!movable_positions.empty());
                        const int first_position = movable_positions[0];
                        swap_move best{add_delta + removal_adjustment[first_position],
                                       first_position, in_facility};
                        for (int index = 1;
                             index < static_cast<int>(movable_positions.size()); ++index) {
                            const int position = movable_positions[index];
                            const Calc delta = add_delta + removal_adjustment[position];
                            if (delta < best.delta) {
                                best.delta = delta;
                                best.out_position = position;
                            }
                        }
                        return best;
                    }
                };

                const auto apply_swap = [&](const swap_move& move) {
                    // 開閉表を差分更新する。固定施設の削除は候補選択時に除外済み
                    const int outgoing = facilities[move.out_position];
                    facilities[move.out_position] = move.in_facility;
                    open_position[outgoing] = -1;
                    open_position[move.in_facility] = move.out_position;
                    const Cost inf = std::numeric_limits<Cost>::max();
                    current_cost = Calc{};
                    for (int demand = 0; demand < demand_count; ++demand) {
                        // 削除施設が第2近傍以下なら、同値を含め最近傍2個を再計算する
                        if (distance(outgoing, demand) <= second_cost[demand]) {
                            Cost first = problem.fallback_cost.empty()
                                             ? inf : problem.fallback_cost[demand];
                            Cost second = inf;
                            int owner = -1;
                            for (int facility : facilities) {
                                const Cost value = distance(facility, demand);
                                if (value < first ||
                                    (value == first && (owner == -1 || facility < owner))) {
                                    second = first;
                                    first = value;
                                    owner = facility;
                                } else {
                                    second = std::min(second, value);
                                }
                            }
                            nearest_cost[demand] = first;
                            second_cost[demand] = second;
                            nearest_facility[demand] = owner;
                        } else {
                            // 削除が最近傍2個に影響しない需要点では、追加施設だけを挿入する
                            const Cost incoming = distance(move.in_facility, demand);
                            if (incoming < nearest_cost[demand] ||
                                (incoming == nearest_cost[demand] &&
                                 (nearest_facility[demand] == -1 || move.in_facility < nearest_facility[demand]))) {
                                second_cost[demand] = nearest_cost[demand];
                                nearest_cost[demand] = incoming;
                                nearest_facility[demand] = move.in_facility;
                            } else {
                                second_cost[demand] = std::min(second_cost[demand], incoming);
                            }
                        }
                        // 加算順を固定して重み付き費用と担当位置を更新する
                        owner_position[demand] = nearest_facility[demand] == -1
                                                     ? -1 : open_position[nearest_facility[demand]];
                        const Calc w = weight(demand);
                        weighted_nearest_cost[demand] =
                            w * static_cast<Calc>(nearest_cost[demand]);
                        weighted_second_cost[demand] = second_cost[demand] == inf
                            ? std::numeric_limits<Calc>::max()
                            : w * static_cast<Calc>(second_cost[demand]);
                        current_cost += weighted_nearest_cost[demand];
                    }
                    if constexpr (Generalized) {
                        if (!problem.facility_cost.empty()) {
                            if (fixed_baseline) {
                                current_cost += fixed_baseline->selection_cost;
                                for (int facility : facilities) {
                                    if (!fixed_mask[facility]) current_cost += problem.facility_cost[facility];
                                }
                            } else {
                                for (int facility : facilities) current_cost += problem.facility_cost[facility];
                            }
                        }
                    }
                    ++statistics.accepted_swaps;
                    remember_best();
                };

                if (movable_positions.empty()) return;
                bool improved = true;
                while (improved && !expired()) {
                    improved = false;
                    std::shuffle(candidate_order.begin(), candidate_order.end(), rng);

                    // 一つ改善するたび状態を更新し、残り候補は新しい状態に対して評価する
                    for (int facility : candidate_order) {
                        if (open_position[facility] != -1) continue;
                        if constexpr (BoundedEvaluations) {
                            if (evaluation_budget_exhausted()) return;
                        }
                        const swap_move move = [&] {
                            if constexpr (!Generalized) {
                                return evaluate_candidate.template operator()<false>(facility);
                            } else {
                                return problem.fallback_cost.empty()
                                    ? evaluate_candidate.template operator()<false>(facility)
                                    : evaluate_candidate.template operator()<true>(facility);
                            }
                        }();
                        ++statistics.candidate_evaluations;
                        if (move.delta < Calc{}) {
                            apply_swap(move);
                            improved = true;
                        }
                        if ((statistics.candidate_evaluations & 15U) == 0 && expired()) break;
                    }
                }
            };

            if (options.max_candidate_evaluations ==
                std::numeric_limits<std::uint64_t>::max()) {
                descend.template operator()<false>();
            } else {
                descend.template operator()<true>();
            }
        }

        // 指定回数またはdeadlineまで、2施設ruinとeager 1-swapを繰り返す
        void run_ils() {
            // 保存した最良解から異なる2施設をランダム交換し、局所最適の谷を移る
            const auto perturb_from_best = [&]() {
                const int movable_count = static_cast<int>(movable_positions.size());
                const int candidate_pool_size = [&] {
                    if constexpr (!Generalized) {
                        return candidate_count;
                    } else {
                        return full_candidate_domain
                                   ? candidate_count
                                   : static_cast<int>(movable_candidates.size());
                    }
                }();
                const int closed_count = candidate_pool_size - movable_count;
                if (movable_count == 0 || closed_count == 0) return false;
                std::fill(open_position.begin(), open_position.end(), -1);
                facilities = best_facilities;
                for (int position = 0; position < facility_count; ++position) open_position[facilities[position]] = position;

                const int strength = std::min({2, movable_count, closed_count});
                std::shuffle(movable_positions.begin(), movable_positions.end(), rng);

                // 削除施設を再び直後に選ばないことで、実際に異なる谷へ移りやすくする
                for (int index = 0; index < strength; ++index) {
                    const int position = movable_positions[index];
                    open_position[facilities[position]] = -2;
                }

                // 閉じている施設をランダム順に探し、削除した各位置へ入れる
                for (int index = 0; index < strength; ++index) {
                    const int incoming = random_closed_facility();
                    if (incoming == -1) return false;
                    const int position = movable_positions[index];
                    facilities[position] = incoming;
                    open_position[incoming] = position;
                }
                rebuild_state();
                return true;
            };

            const int limit = options.max_perturbations >= 0
                                  ? options.max_perturbations
                                  : options.deadline == clock::time_point::max()
                                        ? 32
                                        : std::numeric_limits<int>::max();

            for (int iteration = 0;
                 iteration < limit && !expired() && !evaluation_budget_exhausted();
                 ++iteration) {
                if (!perturb_from_best()) break;
                descend_eager_swap();

                // 摂動だけで最良を更新し、改善swapがなかった場合も保存する
                remember_best();
                ++statistics.completed_perturbations;
            }
        }

        // 保存した最良施設集合を昇順化し、各需要点の最寄り施設と目的値を返す
        result_type make_result() const {
            result_type result;
            result.facilities = best_facilities;
            auto& selected = result.facilities;
            std::sort(selected.begin(), selected.end());
            result.assignment.resize(demand_count);
            for (int demand = 0; demand < demand_count; ++demand) {
                int best_facility = selected[0];
                Cost best = distance(best_facility, demand);
                for (int position = 1; position < facility_count; ++position) {
                    const int facility = selected[position];
                    const Cost value = distance(facility, demand);
                    if (value < best) {
                        best = value;
                        best_facility = facility;
                    }
                }
                if (!problem.fallback_cost.empty()) {
                    if (problem.fallback_cost[demand] < best) {
                        best = problem.fallback_cost[demand];
                        best_facility = -1;
                    }
                }
                result.assignment[demand] = best_facility;
                result.cost += weight(demand) * static_cast<Calc>(best);
            }
            if (!problem.facility_cost.empty()) {
                for (int facility : selected) {
                    result.cost += problem.facility_cost[facility];
                }
            }
            result.statistics = statistics;
            result.statistics.evaluation_limit_reached =
                options.max_candidate_evaluations !=
                    std::numeric_limits<std::uint64_t>::max() &&
                evaluation_budget_exhausted();
            return result;
        }

    public:
        // 問題と制約を保持して固定基線・作業配列を構築する。O(M log(M+1) + Q(K+1))
        search_state(const problem_type& problem_, int facility_count_,
                     const facility_location_restrictions& restrictions_,
                     facility_location_options options_)
            : problem(problem_),
              options(options_),
              rng(options.seed),
              demand_count(problem.demand_count),
              candidate_count(problem.candidate_count),
              facility_count(facility_count_),
              open_position(candidate_count, -1),
              nearest_facility(demand_count),
              owner_position(demand_count),
              nearest_cost(demand_count),
              second_cost(demand_count),
              weighted_nearest_cost(demand_count),
              weighted_second_cost(demand_count),
              removal_adjustment(facility_count) {
            assert(demand_count > 0);
            assert(candidate_count > 0);
            assert(1 <= facility_count && facility_count <= candidate_count);
            assert(problem.cost.size() ==
                   static_cast<std::size_t>(demand_count) * candidate_count);
            assert(problem.demand_weight.empty() ||
                   problem.demand_weight.size() == static_cast<std::size_t>(demand_count));
            assert(problem.fallback_cost.empty() ||
                   problem.fallback_cost.size() == static_cast<std::size_t>(demand_count));
            assert(problem.facility_cost.empty() ||
                   problem.facility_cost.size() == static_cast<std::size_t>(candidate_count));
            assert(options.initial_trials >= 1);
            assert(options.max_perturbations >= -1);

            if constexpr (!Generalized) {
                assert(restrictions_.fixed_facilities.empty());
                assert(restrictions_.candidate_facilities.empty());
                assert(problem.fallback_cost.empty());
                assert(problem.facility_cost.empty());
                full_candidate_domain = true;
                candidate_order.resize(candidate_count);
                std::iota(candidate_order.begin(), candidate_order.end(), 0);
                movable_positions.resize(facility_count);
                std::iota(movable_positions.begin(), movable_positions.end(), 0);
                return;
            }

            fixed_facilities = restrictions_.fixed_facilities;
            std::sort(fixed_facilities.begin(), fixed_facilities.end());
            assert(std::adjacent_find(fixed_facilities.begin(), fixed_facilities.end()) ==
                   fixed_facilities.end());
            if (!fixed_facilities.empty()) fixed_mask.assign(candidate_count, 0);
            for (int facility : fixed_facilities) {
                assert(0 <= facility && facility < candidate_count);
                fixed_mask[facility] = 1;
            }

            full_candidate_domain = fixed_facilities.empty() &&
                                    restrictions_.candidate_facilities.empty();
            if (restrictions_.candidate_facilities.empty()) {
                if (full_candidate_domain) {
                    candidate_order.resize(candidate_count);
                    std::iota(candidate_order.begin(), candidate_order.end(), 0);
                } else {
                    movable_candidates.reserve(candidate_count -
                                                 fixed_facilities.size());
                    for (int facility = 0; facility < candidate_count; ++facility) {
                        if (!fixed_mask[facility]) movable_candidates.push_back(facility);
                    }
                }
            } else {
                std::vector<int> listed = restrictions_.candidate_facilities;
                std::sort(listed.begin(), listed.end());
                assert(std::adjacent_find(listed.begin(), listed.end()) == listed.end());
                movable_candidates.reserve(listed.size());
                for (int facility : listed) {
                    assert(0 <= facility && facility < candidate_count);
                    if (fixed_facilities.empty() || !fixed_mask[facility]) {
                        movable_candidates.push_back(facility);
                    }
                }
            }
            assert(static_cast<int>(fixed_facilities.size()) <= facility_count);
            const int movable_candidate_count = full_candidate_domain
                                                    ? candidate_count
                                                    : static_cast<int>(
                                                          movable_candidates.size());
            assert(static_cast<int>(fixed_facilities.size()) +
                       movable_candidate_count >=
                   facility_count);
            static_cast<void>(movable_candidate_count);
            if (!full_candidate_domain) candidate_order = movable_candidates;
            movable_positions.resize(facility_count - fixed_facilities.size());
            if (fixed_facilities.empty()) {
                std::iota(movable_positions.begin(), movable_positions.end(), 0);
            }
            // 固定施設とfallbackだけからなる不変な最近傍状態を一度だけ構築する。
            if (fixed_facilities.empty()) return;

            fixed_baseline = std::make_unique<fixed_baseline_state>();
            const Cost inf = std::numeric_limits<Cost>::max();
            fixed_baseline->nearest_facility.assign(demand_count, -1);
            fixed_baseline->nearest_cost.assign(demand_count, inf);
            fixed_baseline->second_cost.assign(demand_count, inf);
            if (!problem.fallback_cost.empty()) {
                fixed_baseline->nearest_cost = problem.fallback_cost;
            }

            for (int facility : fixed_facilities) {
                const Cost* facility_cost = problem.cost.data() +
                                            static_cast<std::size_t>(facility) *
                                                demand_count;
                for (int demand = 0; demand < demand_count; ++demand) {
                    const Cost value = facility_cost[demand];
                    if (value < fixed_baseline->nearest_cost[demand] ||
                        (value == fixed_baseline->nearest_cost[demand] &&
                         (fixed_baseline->nearest_facility[demand] == -1 ||
                          facility < fixed_baseline->nearest_facility[demand]))) {
                        fixed_baseline->second_cost[demand] =
                            fixed_baseline->nearest_cost[demand];
                        fixed_baseline->nearest_cost[demand] = value;
                        fixed_baseline->nearest_facility[demand] = facility;
                    } else if (value < fixed_baseline->second_cost[demand]) {
                        fixed_baseline->second_cost[demand] = value;
                    }
                }
                fixed_baseline->selection_cost += selection_cost(facility);
            }
        }

        // 同じproblem・k・restrictionsで作業領域を再利用して次の探索を始める。O(M+K)
        void restart(facility_location_options options_) {
            assert(options_.initial_trials >= 1);
            assert(options_.max_perturbations >= -1);
            options = options_;
            rng.seed(options.seed);
            current_cost = Calc{};
            best_facilities.clear();
            best_cost = Calc{};
            statistics = {};
            if (full_candidate_domain) {
                candidate_order.resize(candidate_count);
                std::iota(candidate_order.begin(), candidate_order.end(), 0);
            } else {
                candidate_order = movable_candidates;
            }
            if (fixed_facilities.empty()) {
                movable_positions.resize(facility_count);
                std::iota(movable_positions.begin(), movable_positions.end(), 0);
            }
        }

        // 複数初期解を局所最適化してILSを行う。O(M log(M+1)+T(MQ+K(M+Q))+(P+S+T+1)(M+QK)+E(Q+K))
        result_type solve() {
            for (int trial = 0;
                 trial < options.initial_trials && (!expired() || trial == 0); ++trial) {
                {
                    // 重み付きD^2-samplingで初期施設集合を構築する
                    // 全候補を走査し、需要点1個だけに対して最も近い未選択施設を返す
                    const auto closest_closed_facility = [&](int demand) {
                        int best_facility = -1;
                        Cost best{};
                        if (!Generalized || full_candidate_domain) {
                            for (int facility = 0; facility < candidate_count; ++facility) {
                                if (open_position[facility] != -1) continue;
                                const Cost value = distance(facility, demand);
                                if (best_facility == -1 || value < best) {
                                    best = value;
                                    best_facility = facility;
                                }
                            }
                        } else {
                            for (int facility : movable_candidates) {
                                if (open_position[facility] != -1) continue;
                                const Cost value = distance(facility, demand);
                                if (best_facility == -1 || value < best) {
                                    best = value;
                                    best_facility = facility;
                                }
                            }
                        }
                        return best_facility;
                    };

                    // w[i] * served[i]^2 に比例する確率で、次の中心となる需要点を選ぶ
                    const auto choose_demand_by_squared_distance = [&](const std::vector<Cost>& served) {
                        long double total = 0;
                        for (int demand = 0; demand < demand_count; ++demand) {
                            const long double d = static_cast<long double>(served[demand]);
                            total += static_cast<long double>(weight(demand)) * d * d;
                        }
                        if (!(total > 0)) return static_cast<int>(random_below(demand_count));

                        long double target =
                            std::generate_canonical<long double, 64>(rng) * total;
                        for (int demand = 0; demand < demand_count; ++demand) {
                            const long double d = static_cast<long double>(served[demand]);
                            target -= static_cast<long double>(weight(demand)) * d * d;
                            if (target <= 0) return demand;
                        }
                        return demand_count - 1;
                    };

                    std::vector<int> selected;
                    if constexpr (Generalized) selected = fixed_facilities;
                    selected.reserve(facility_count);
                    std::fill(open_position.begin(), open_position.end(), -1);
                    for (int position = 0; position < static_cast<int>(selected.size()); ++position) {
                        open_position[selected[position]] = position;
                    }

                    const Cost inf = std::numeric_limits<Cost>::max();
                    std::vector<Cost> served(demand_count, inf);
                    if constexpr (Generalized) {
                        if (fixed_baseline) {
                            served = fixed_baseline->nearest_cost;
                        } else {
                            if (!problem.fallback_cost.empty()) served = problem.fallback_cost;
                        }
                    }

                    // 最初の可動施設は、最初の試行だけ厳密な最良追加、それ以外は乱す
                    if (static_cast<int>(selected.size()) < facility_count) {
                        int first = -1;
                        if (trial != 0) {
                            first = random_closed_facility();
                        } else {
                            // 固定施設・fallbackへ1施設を足した目的値を全候補で比較する。
                            Calc best{};
                            if constexpr (!Generalized) {
                                for (int facility = 0; facility < candidate_count; ++facility) {
                                    Calc value{};
                                    for (int demand = 0; demand < demand_count; ++demand) {
                                        value += weight(demand) *
                                                 static_cast<Calc>(distance(facility, demand));
                                    }
                                    if (first == -1 || value < best) {
                                        best = value;
                                        first = facility;
                                    }
                                }
                            } else {
                                const auto consider = [&](int facility) {
                                    if (open_position[facility] != -1) return;
                                    Calc value = selection_cost(facility);
                                    for (int demand = 0; demand < demand_count; ++demand) {
                                        value += weight(demand) * static_cast<Calc>(
                                                                      std::min(served[demand],
                                                                               distance(facility, demand)));
                                    }
                                    if (first == -1 || value < best) {
                                        best = value;
                                        first = facility;
                                    }
                                };
                                if (full_candidate_domain) {
                                    for (int facility = 0; facility < candidate_count; ++facility) {
                                        consider(facility);
                                    }
                                } else {
                                    for (int facility : movable_candidates) consider(facility);
                                }
                            }
                        }
                        assert(first != -1);
                        open_position[first] = static_cast<int>(selected.size());
                        selected.push_back(first);
                        for (int demand = 0; demand < demand_count; ++demand) {
                            served[demand] = std::min(served[demand], distance(first, demand));
                        }
                    }

                    // 遠くて重い需要点を選び、その需要点へ最も近い未選択施設を追加する
                    while (static_cast<int>(selected.size()) < facility_count && !expired()) {
                        const int demand = choose_demand_by_squared_distance(served);
                        const int facility = closest_closed_facility(demand);
                        assert(facility != -1);
                        open_position[facility] = static_cast<int>(selected.size());
                        selected.push_back(facility);
                        for (int i = 0; i < demand_count; ++i) {
                            served[i] = std::min(served[i], distance(facility, i));
                        }
                    }

                    // 制限時刻に達しても、返す施設集合だけは必ず実行可能にする
                    while (static_cast<int>(selected.size()) < facility_count) {
                        const int facility = random_closed_facility();
                        assert(facility != -1);
                        open_position[facility] = static_cast<int>(selected.size());
                        selected.push_back(facility);
                    }
                    facilities = std::move(selected);
                }
                initialize_movable_positions();
                rebuild_state();
                remember_best();
                descend_eager_swap();
                ++statistics.completed_initial_trials;
            }
            run_ils();
            return make_result();
        }

        // 施設集合を初期最良解として1-swapとILSで改善する。O(M log(M+1)+(P+S+1)(M+QK)+E(Q+K))
        // 一時vectorはmoveし、const参照なら確保済み領域へcopyして改善する。
        template <class InitialFacilities>
        result_type improve(InitialFacilities&& initial_facilities) {
            assert(static_cast<int>(initial_facilities.size()) == facility_count);
            facilities = std::forward<InitialFacilities>(initial_facilities);
            rebuild_state();
            if constexpr (Generalized) {
                if (!full_candidate_domain) {
                    for (int facility : facilities) {
                        const bool fixed =
                            !fixed_mask.empty() && fixed_mask[facility];
                        assert(fixed ||
                               std::binary_search(movable_candidates.begin(),
                                                  movable_candidates.end(), facility));
                        static_cast<void>(fixed);
                    }
                }
            }
            for (int facility : fixed_facilities) {
                assert(open_position[facility] != -1);
                static_cast<void>(facility);
            }
            initialize_movable_positions();
            remember_best();
            descend_eager_swap();
            run_ils();
            return make_result();
        }

    };

    search_state<true> state;

    template <class C, class A>
    friend facility_location_result<A> solve_k_median(
        const facility_location_problem<C, A>&, int,
        const facility_location_restrictions&, facility_location_options);
    template <class C, class A>
    friend facility_location_result<A> improve_k_median(
        const facility_location_problem<C, A>&, std::vector<int>,
        const facility_location_restrictions&, facility_location_options);

public:
    // 問題・制約を保持して作業領域を確保する。Q需要、M候補、F固定としてO(M log M + Q(F+1))
    facility_location_workspace(
        const problem_type& problem, int facility_count,
        const facility_location_restrictions& restrictions = {})
        : state(problem, facility_count, restrictions,
                facility_location_options{}) {}

    // 保存した問題を新規構築から解く。O(M log(M+1)+T(MQ+K(M+Q))+(P+S+T+1)(M+QK)+E(Q+K))
    result_type solve(facility_location_options options = {}) {
        state.restart(options);
        return state.solve();
    }

    // 初期施設集合を改善する。O(M log(M+1)+(P+S+1)(M+QK)+E(Q+K))
    result_type improve(const std::vector<int>& initial_facilities,
                        facility_location_options options = {}) {
        state.restart(options);
        return state.improve(initial_facilities);
    }
};

// 指定施設集合のサービス費用と単項費用の和。O(Q * facilities.size())
template <class Cost, class Calc = long long>
Calc evaluate_k_median(
    const facility_location_problem<Cost, Calc>& problem,
    const std::vector<int>& facilities) {
    assert(problem.demand_count > 0);
    assert(problem.candidate_count > 0);
    assert(!facilities.empty());
    assert(problem.cost.size() ==
           static_cast<std::size_t>(problem.demand_count) * problem.candidate_count);
    assert(problem.demand_weight.empty() ||
           problem.demand_weight.size() == static_cast<std::size_t>(problem.demand_count));
    assert(problem.fallback_cost.empty() ||
           problem.fallback_cost.size() == static_cast<std::size_t>(problem.demand_count));
    assert(problem.facility_cost.empty() ||
           problem.facility_cost.size() == static_cast<std::size_t>(problem.candidate_count));

    Calc total{};
    for (int demand = 0; demand < problem.demand_count; ++demand) {
        Cost best = problem.cost[
            static_cast<std::size_t>(facilities[0]) * problem.demand_count + demand];
        for (int position = 1; position < static_cast<int>(facilities.size()); ++position) {
            const int facility = facilities[position];
            best = std::min(
                best,
                problem.cost[static_cast<std::size_t>(facility) * problem.demand_count + demand]);
        }
        if (!problem.fallback_cost.empty()) {
            best = std::min(best, problem.fallback_cost[demand]);
        }
        const Calc weight = problem.demand_weight.empty()
                                ? Calc{1}
                                : problem.demand_weight[demand];
        total += weight * static_cast<Calc>(best);
    }
    if (!problem.facility_cost.empty()) {
        for (int facility : facilities) total += problem.facility_cost[facility];
    }
    return total;
}

// 制約内でK施設を選ぶ。O(M log(M+1)+T(MQ+K(M+Q))+(P+S+T+1)(M+QK)+E(Q+K))
template <class Cost, class Calc>
facility_location_result<Calc> solve_k_median(
    const facility_location_problem<Cost, Calc>& problem,
    int facility_count,
    const facility_location_restrictions& restrictions,
    facility_location_options options) {
    const bool generalized = !problem.fallback_cost.empty() ||
                             !problem.facility_cost.empty() ||
                             !restrictions.fixed_facilities.empty() ||
                             !restrictions.candidate_facilities.empty();
    if (!generalized) {
        return typename facility_location_workspace<Cost, Calc>::template search_state<false>(
                   problem, facility_count, restrictions, options)
            .solve();
    }
    return typename facility_location_workspace<Cost, Calc>::template search_state<true>(
               problem, facility_count, restrictions, options)
        .solve();
}

// 全候補からK施設を選ぶ。O(M log(M+1)+T(MQ+K(M+Q))+(P+S+T+1)(M+QK)+E(Q+K))
template <class Cost, class Calc = long long>
facility_location_result<Calc> solve_k_median(
    const facility_location_problem<Cost, Calc>& problem,
    int facility_count,
    facility_location_options options = {}) {
    return solve_k_median(problem, facility_count,
                          facility_location_restrictions{}, options);
}

// 固定施設を保って許可候補内で改善する。O(M log(M+1)+(P+S+1)(M+QK)+E(Q+K))
template <class Cost, class Calc>
facility_location_result<Calc> improve_k_median(
    const facility_location_problem<Cost, Calc>& problem,
    std::vector<int> initial_facilities,
    const facility_location_restrictions& restrictions,
    facility_location_options options) {
    const int facility_count = static_cast<int>(initial_facilities.size());
    const bool generalized = !problem.fallback_cost.empty() ||
                             !problem.facility_cost.empty() ||
                             !restrictions.fixed_facilities.empty() ||
                             !restrictions.candidate_facilities.empty();
    if (!generalized) {
        return typename facility_location_workspace<Cost, Calc>::template search_state<false>(
                   problem, facility_count, restrictions, options)
            .improve(std::move(initial_facilities));
    }
    return typename facility_location_workspace<Cost, Calc>::template search_state<true>(
               problem, facility_count, restrictions, options)
        .improve(std::move(initial_facilities));
}

// 全候補内で指定施設集合を改善する。O(M log(M+1)+(P+S+1)(M+QK)+E(Q+K))
template <class Cost, class Calc = long long>
facility_location_result<Calc> improve_k_median(
    const facility_location_problem<Cost, Calc>& problem,
    std::vector<int> initial_facilities,
    facility_location_options options = {}) {
    return improve_k_median(problem, std::move(initial_facilities),
                            facility_location_restrictions{}, options);
}
