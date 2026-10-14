/*
 * facility_location v13: AHCの全体問題・部分問題で使う重み付き離散k-median solver
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
 *   - Calcは符号付き整数または浮動小数点型とし、中間演算を含むオーバーフローは検査しない
 *   - facility_costは負でもよい。小数型のswapは再計算した目的値でも改善する場合だけ受理する
 *   - 容量、施設間相互作用、連結性、k-center、可変kには対応しない
 *   - costはcost[facility * demand_count + demand]の候補施設優先配列
 * 計算量ではQ=需要点数、M=候補数、K=選択数、T=初期試行数、P=摂動数、
 * E=候補評価数、S=swap適用・復元回数、F=固定施設数を表す
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

    // 全探索統計を改行なしで出力する。O(1)
    friend std::ostream& operator<<(std::ostream& os,
                                    const facility_location_statistics& value) {
        return os << "{candidate_evaluations=" << value.candidate_evaluations
                  << ", accepted_swaps=" << value.accepted_swaps
                  << ", completed_initial_trials=" << value.completed_initial_trials
                  << ", completed_perturbations=" << value.completed_perturbations
                  << ", deadline_reached=" << (value.deadline_reached ? "true" : "false")
                  << ", evaluation_limit_reached="
                  << (value.evaluation_limit_reached ? "true" : "false") << '}';
    }
};

template <class Calc>
struct facility_location_result {
    Calc cost{};
    std::vector<int> facilities;
    // fallbackが選ばれた需要点は-1
    std::vector<int> assignment;
    facility_location_statistics statistics;

    // 結果を各配列の先頭20要素まで改行なしで出力する。O(min(K,20)+min(Q,20)+1)
    friend std::ostream& operator<<(std::ostream& os,
                                    const facility_location_result& value) {
        os << "facility_location_result{cost=";
        // 標準ostreamにない128ビット整数も、最小値を含め十進数で表示する
        if constexpr (std::is_same_v<Calc, __int128_t>) {
            __uint128_t magnitude = static_cast<__uint128_t>(value.cost);
            if (value.cost < 0) magnitude = __uint128_t{} - magnitude;
            char digits[40];
            char* const end = digits + sizeof(digits);
            char* begin = end;
            do {
                *--begin = static_cast<char>('0' + magnitude % 10);
                magnitude /= 10;
            } while (magnitude != 0);
            if (value.cost < 0) *--begin = '-';
            os.write(begin, static_cast<std::streamsize>(end - begin));
        } else {
            os << +value.cost;
        }

        // 配列は元の順序を保ち、総要素数と省略した要素数を添える
        const auto write_array = [&](const char* name, const std::vector<int>& values) {
            os << ", " << name << "(size=" << values.size() << ")=[";
            const std::size_t count = std::min<std::size_t>(values.size(), 20);
            for (std::size_t i = 0; i < count; ++i) {
                if (i != 0) os << ", ";
                os << values[i];
            }
            if (count < values.size()) os << ", ... (+" << values.size() - count << ")";
            os << ']';
        };
        write_array("facilities", value.facilities);
        write_array("assignment", value.assignment);
        return os << ", statistics=" << value.statistics << '}';
    }
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
// problemはworkspaceより長く生存し、workspaceの存続中は内容と所在を変えてはならない。
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

        // 施設費用を加算し、小数型では施設ID順にそろえて同じ集合の丸め結果を固定する
        void add_selection_costs() {
            if constexpr (Generalized) {
                if (problem.facility_cost.empty()) return;
                if constexpr (std::is_floating_point_v<Calc>) {
                    // 加算順を入力順・固定基線・swap位置に依存させない
                    auto ordered = facilities;
                    std::sort(ordered.begin(), ordered.end());
                    for (int facility : ordered) current_cost += problem.facility_cost[facility];
                } else if (fixed_baseline) {
                    // 整数型では固定施設の合計を再利用する
                    current_cost += fixed_baseline->selection_cost;
                    for (int facility : facilities) {
                        if (!fixed_mask[facility]) current_cost += problem.facility_cost[facility];
                    }
                } else {
                    for (int facility : facilities) current_cost += problem.facility_cost[facility];
                }
            }
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
                add_selection_costs();
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
                    const Calc previous_cost = current_cost;
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
                    add_selection_costs();
                    if constexpr (std::is_floating_point_v<Calc>) {
                        // 差分の丸め誤差だけで同じ施設集合を循環しないよう、実際の改善を確認する
                        if (!(current_cost < previous_cost)) {
                            facilities[move.out_position] = outgoing;
                            rebuild_state();
                            return false;
                        }
                    }
                    ++statistics.accepted_swaps;
                    remember_best();
                    return true;
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
                        if (move.delta < Calc{} && apply_swap(move)) improved = true;
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

        // 複数初期解を局所最適化してILSを行う。O(M log(M+1)+T(MQ+K(M+Q))+(P+S+T+1)(M+QK+K log(K+1))+E(Q+K))
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

        // 施設集合を初期最良解として1-swapとILSで改善する。O(M log(M+1)+(P+S+1)(M+QK+K log(K+1))+E(Q+K))
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

    // 保存した問題を新規構築から解く。O(M log(M+1)+T(MQ+K(M+Q))+(P+S+T+1)(M+QK+K log(K+1))+E(Q+K))
    result_type solve(facility_location_options options = {}) {
        state.restart(options);
        return state.solve();
    }

    // 初期施設集合を改善する。O(M log(M+1)+(P+S+1)(M+QK+K log(K+1))+E(Q+K))
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

// 制約内でK施設を選ぶ。O(M log(M+1)+T(MQ+K(M+Q))+(P+S+T+1)(M+QK+K log(K+1))+E(Q+K))
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

// 全候補からK施設を選ぶ。O(M log(M+1)+T(MQ+K(M+Q))+(P+S+T+1)(M+QK+K log(K+1))+E(Q+K))
template <class Cost, class Calc = long long>
facility_location_result<Calc> solve_k_median(
    const facility_location_problem<Cost, Calc>& problem,
    int facility_count,
    facility_location_options options = {}) {
    return solve_k_median(problem, facility_count,
                          facility_location_restrictions{}, options);
}

// 固定施設を保って許可候補内で改善する。O(M log(M+1)+(P+S+1)(M+QK+K log(K+1))+E(Q+K))
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

// 全候補内で指定施設集合を改善する。O(M log(M+1)+(P+S+1)(M+QK+K log(K+1))+E(Q+K))
template <class Cost, class Calc = long long>
facility_location_result<Calc> improve_k_median(
    const facility_location_problem<Cost, Calc>& problem,
    std::vector<int> initial_facilities,
    facility_location_options options = {}) {
    return improve_k_median(problem, std::move(initial_facilities),
                            facility_location_restrictions{}, options);
}

#if __INCLUDE_LEVEL__ == 0

namespace {

using Problem = facility_location_problem<int, long long>;
using Result = facility_location_result<long long>;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void check_result(const Problem& problem, int facility_count, const Result& result) {
    require(static_cast<int>(result.facilities.size()) == facility_count,
            "facility count mismatch");
    require(static_cast<int>(result.assignment.size()) == problem.demand_count,
            "assignment count mismatch");
    require(std::is_sorted(result.facilities.begin(), result.facilities.end()),
            "facilities are not sorted");
    require(std::adjacent_find(result.facilities.begin(), result.facilities.end()) ==
                result.facilities.end(),
            "duplicate facility");
    require(result.cost == evaluate_k_median(problem, result.facilities),
            "result cost mismatch");
    for (int demand = 0; demand < problem.demand_count; ++demand) {
        const int assigned = result.assignment[demand];
        if (assigned == -1) {
            require(!problem.fallback_cost.empty(),
                    "fallback assignment without fallback cost");
            for (int facility : result.facilities) {
                require(problem.fallback_cost[demand] <
                            problem.cost[static_cast<std::size_t>(facility) *
                                             problem.demand_count + demand],
                        "fallback does not strictly beat open facilities");
            }
        } else {
            require(std::binary_search(result.facilities.begin(),
                                       result.facilities.end(), assigned),
                    "assignment is not open");
            const int assigned_cost =
                problem.cost[static_cast<std::size_t>(assigned) *
                                 problem.demand_count + demand];
            if (!problem.fallback_cost.empty()) {
                require(assigned_cost <= problem.fallback_cost[demand],
                        "open assignment is worse than fallback");
            }
            for (int facility : result.facilities) {
                const int value =
                    problem.cost[static_cast<std::size_t>(facility) *
                                     problem.demand_count + demand];
                require(std::pair{assigned_cost, assigned} <=
                            std::pair{value, facility},
                        "assignment is not the tie-broken nearest facility");
            }
        }
    }
}

Problem random_problem(std::mt19937_64& rng, int demands, int candidates) {
    Problem problem;
    problem.demand_count = demands;
    problem.candidate_count = candidates;
    problem.cost.resize(static_cast<std::size_t>(demands) * candidates);
    problem.demand_weight.resize(demands);
    for (int facility = 0; facility < candidates; ++facility) {
        for (int demand = 0; demand < demands; ++demand) {
            problem.cost[static_cast<std::size_t>(facility) * demands + demand] =
                1 + static_cast<int>(rng() % 1000000);
        }
    }
    for (long long& weight : problem.demand_weight) {
        weight = 1 + static_cast<long long>(rng() % 20);
    }
    return problem;
}

long long exact_optimum(const Problem& problem, int facility_count) {
    long long best = std::numeric_limits<long long>::max();
    std::vector<int> selected;
    const auto dfs = [&](const auto& self, int next) -> void {
        if (static_cast<int>(selected.size()) == facility_count) {
            best = std::min(best, evaluate_k_median(problem, selected));
            return;
        }
        const int needed = facility_count - static_cast<int>(selected.size());
        for (int facility = next;
             facility <= problem.candidate_count - needed; ++facility) {
            selected.push_back(facility);
            self(self, facility + 1);
            selected.pop_back();
        }
    };
    dfs(dfs, 0);
    return best;
}

long long exact_restricted_optimum(
    const Problem& problem, int facility_count,
    const facility_location_restrictions& restrictions) {
    std::vector<unsigned char> fixed(problem.candidate_count, 0);
    std::vector<unsigned char> allowed(problem.candidate_count, 0);
    std::vector<int> selected = restrictions.fixed_facilities;
    for (int facility : restrictions.fixed_facilities) {
        fixed[facility] = 1;
        allowed[facility] = 1;
    }
    if (restrictions.candidate_facilities.empty()) {
        std::fill(allowed.begin(), allowed.end(), 1);
    } else {
        for (int facility : restrictions.candidate_facilities) {
            allowed[facility] = 1;
        }
    }
    std::vector<int> movable;
    for (int facility = 0; facility < problem.candidate_count; ++facility) {
        if (allowed[facility] && !fixed[facility]) movable.push_back(facility);
    }

    long long best = std::numeric_limits<long long>::max();
    const auto dfs = [&](const auto& self, int next) -> void {
        if (static_cast<int>(selected.size()) == facility_count) {
            best = std::min(best, evaluate_k_median(problem, selected));
            return;
        }
        const int needed = facility_count - static_cast<int>(selected.size());
        for (int index = next;
             index <= static_cast<int>(movable.size()) - needed; ++index) {
            selected.push_back(movable[index]);
            self(self, index + 1);
            selected.pop_back();
        }
    };
    dfs(dfs, 0);
    return best;
}

void check_restrictions(const Result& result,
                        const facility_location_restrictions& restrictions) {
    for (int facility : restrictions.fixed_facilities) {
        require(std::binary_search(result.facilities.begin(),
                                   result.facilities.end(), facility),
                "fixed facility was removed");
    }
    if (!restrictions.candidate_facilities.empty()) {
        for (int facility : result.facilities) {
            require(std::find(restrictions.fixed_facilities.begin(),
                              restrictions.fixed_facilities.end(), facility) !=
                        restrictions.fixed_facilities.end() ||
                        std::find(restrictions.candidate_facilities.begin(),
                                  restrictions.candidate_facilities.end(), facility) !=
                            restrictions.candidate_facilities.end(),
                    "selected facility is outside the candidate pool");
        }
    }
}

void test_edge_cases() {
    Problem problem;
    problem.demand_count = 3;
    problem.candidate_count = 4;
    problem.cost = {0, 5, 9, 1, 2, 8, 7, 1, 1, 3, 3, 3};
    problem.demand_weight = {2, 3, 5};

    for (int facility_count : {1, 2, 4}) {
        facility_location_options options;
        options.max_perturbations = 8;
        const Result result = solve_k_median(problem, facility_count, options);
        check_result(problem, facility_count, result);
        if (facility_count == 1 || facility_count == 4) {
            require(result.cost == exact_optimum(problem, facility_count),
                    "edge optimum mismatch");
        }
    }

    problem.demand_weight.clear();
    check_result(problem, 2, solve_k_median(problem, 2));

    problem.demand_weight = {0, 0, 0};
    const Result zero_weight = solve_k_median(problem, 1);
    check_result(problem, 1, zero_weight);
    require(zero_weight.cost == 0, "zero-weight objective mismatch");
}

void test_local_optimum_and_improve() {
    std::mt19937_64 rng(123456789);
    for (int test = 0; test < 300; ++test) {
        const int demands = 2 + static_cast<int>(rng() % 20);
        const int candidates = 2 + static_cast<int>(rng() % 14);
        const int facility_count = 1 + static_cast<int>(rng() % candidates);
        Problem problem = random_problem(rng, demands, candidates);

        std::vector<int> initial(candidates);
        std::iota(initial.begin(), initial.end(), 0);
        std::shuffle(initial.begin(), initial.end(), rng);
        initial.resize(facility_count);
        const long long initial_cost = evaluate_k_median(problem, initial);

        facility_location_options options;
        options.seed = rng();
        options.max_perturbations = 0;
        const Result result = improve_k_median(problem, initial, options);
        check_result(problem, facility_count, result);
        require(result.cost <= initial_cost, "improve worsened the solution");

        // deadlineなしのeager探索後は、どの1-swapでも目的値が改善しない
        std::vector<unsigned char> open(candidates, 0);
        for (int facility : result.facilities) open[facility] = 1;
        std::vector<int> moved = result.facilities;
        for (int incoming = 0; incoming < candidates; ++incoming) {
            if (open[incoming]) continue;
            for (int position = 0; position < facility_count; ++position) {
                const int outgoing = moved[position];
                moved[position] = incoming;
                require(evaluate_k_median(problem, moved) >= result.cost,
                        "result is not 1-swap local optimum");
                moved[position] = outgoing;
            }
        }
    }
}

void test_random_solve_and_exact_bounds() {
    std::mt19937_64 rng(20260717);
    for (int test = 0; test < 120; ++test) {
        const int demands = 4 + static_cast<int>(rng() % 28);
        const int candidates = 4 + static_cast<int>(rng() % 16);
        const int facility_count = 1 + static_cast<int>(rng() % candidates);
        Problem problem = random_problem(rng, demands, candidates);
        facility_location_options options;
        options.seed = rng();
        options.max_perturbations = 12;
        const Result result = solve_k_median(problem, facility_count, options);
        check_result(problem, facility_count, result);
        if (candidates <= 10) {
            require(result.cost >= exact_optimum(problem, facility_count),
                    "heuristic beats exact optimum");
        }
    }
}

void test_restricted_generalized_objective() {
    std::mt19937_64 rng(0x4f31a9c2ULL);
    for (int test = 0; test < 240; ++test) {
        const int demands = 2 + static_cast<int>(rng() % 11);
        const int candidates = 3 + static_cast<int>(rng() % 8);
        const int allowed_count = 2 + static_cast<int>(rng() % (candidates - 1));
        const int fixed_count = static_cast<int>(
            rng() % (std::min(2, allowed_count - 1) + 1));
        const int facility_count =
            fixed_count + 1 +
            static_cast<int>(rng() % (allowed_count - fixed_count));

        Problem problem = random_problem(rng, demands, candidates);
        for (int& value : problem.cost) value = static_cast<int>(value % 101);
        problem.facility_cost.resize(candidates);
        for (long long& value : problem.facility_cost) {
            value = static_cast<long long>(rng() % 601) - 300;
        }
        if ((test & 1) != 0) {
            problem.fallback_cost.resize(demands);
            for (int& value : problem.fallback_cost) {
                value = 10 + static_cast<int>(rng() % 111);
            }
        }

        facility_location_restrictions restrictions;
        for (int facility = 0; facility < fixed_count; ++facility) {
            restrictions.fixed_facilities.push_back(facility);
        }
        for (int facility = 0; facility < allowed_count; ++facility) {
            restrictions.candidate_facilities.push_back(facility);
        }

        std::vector<int> initial;
        for (int facility = 0; facility < facility_count; ++facility) {
            initial.push_back(facility);
        }
        const long long initial_cost = evaluate_k_median(problem, initial);
        facility_location_options options;
        options.seed = rng();
        options.max_perturbations = 0;
        const Result improved = improve_k_median(
            problem, initial, restrictions, options);
        check_result(problem, facility_count, improved);
        check_restrictions(improved, restrictions);
        require(improved.cost <= initial_cost,
                "restricted improve worsened the solution");

        // 固定施設を除く全削除候補と、許可された全追加候補を総当たりする
        std::vector<unsigned char> open(candidates, 0);
        std::vector<unsigned char> fixed(candidates, 0);
        for (int facility : improved.facilities) open[facility] = 1;
        for (int facility : restrictions.fixed_facilities) fixed[facility] = 1;
        std::vector<int> moved = improved.facilities;
        for (int incoming = 0; incoming < allowed_count; ++incoming) {
            if (open[incoming]) continue;
            for (int position = 0; position < facility_count; ++position) {
                if (fixed[moved[position]]) continue;
                const int outgoing = moved[position];
                moved[position] = incoming;
                require(evaluate_k_median(problem, moved) >= improved.cost,
                        "restricted result is not a 1-swap local optimum");
                moved[position] = outgoing;
            }
        }

        const long long optimum = exact_restricted_optimum(
            problem, facility_count, restrictions);
        require(improved.cost >= optimum,
                "restricted heuristic beats exact optimum");

        options.max_perturbations = 8;
        const Result solved = solve_k_median(
            problem, facility_count, restrictions, options);
        check_result(problem, facility_count, solved);
        check_restrictions(solved, restrictions);
        require(solved.cost >= optimum,
                "restricted solve beats exact optimum");
    }
}

void test_fallback_assignment_and_fixed_only() {
    Problem problem;
    problem.demand_count = 2;
    problem.candidate_count = 4;
    problem.cost = {5, 10, 5, 3, 9, 9, 1, 20};
    problem.fallback_cost = {5, 2};
    problem.facility_cost = {0, 0, 100, 100};

    facility_location_options options;
    options.max_perturbations = 0;
    const Result fallback_result = solve_k_median(problem, 1, options);
    check_result(problem, 1, fallback_result);
    require(fallback_result.assignment[0] != -1,
            "real facility must win a fallback tie");
    require(fallback_result.assignment[1] == -1,
            "strictly cheaper fallback was not selected");

    facility_location_restrictions restrictions;
    restrictions.fixed_facilities = {1, 3};
    restrictions.candidate_facilities = {0, 1, 2, 3};
    const Result fixed_result = solve_k_median(problem, 2, restrictions, options);
    check_result(problem, 2, fixed_result);
    require(fixed_result.facilities == restrictions.fixed_facilities,
            "fixed-only solve changed facilities");
    require(fixed_result.statistics.candidate_evaluations == 0,
            "fixed-only solve evaluated a swap");
}

void test_candidate_evaluation_limit_and_statistics() {
    std::mt19937_64 rng(919191);
    Problem problem = random_problem(rng, 31, 17);
    facility_location_options options;
    options.seed = 1234;
    options.initial_trials = 1;
    options.max_perturbations = 100;
    options.max_candidate_evaluations = 7;
    const Result result = solve_k_median(problem, 4, options);
    check_result(problem, 4, result);
    require(result.statistics.candidate_evaluations == 7,
            "candidate evaluation limit is not exact");
    require(result.statistics.evaluation_limit_reached,
            "evaluation limit flag was not set");
    require(result.statistics.accepted_swaps <=
                result.statistics.candidate_evaluations,
            "accepted swap count is invalid");
    require(result.statistics.completed_initial_trials == 1,
            "initial trial statistic mismatch");

    options.max_candidate_evaluations = 0;
    const Result zero = solve_k_median(problem, 4, options);
    require(zero.statistics.candidate_evaluations == 0,
            "zero evaluation budget was exceeded");
    require(zero.statistics.evaluation_limit_reached,
            "zero evaluation limit flag was not set");
}

void test_workspace_matches_one_shot_calls() {
    std::mt19937_64 rng(707070);
    Problem problem = random_problem(rng, 19, 13);
    problem.fallback_cost.resize(problem.demand_count);
    for (int& value : problem.fallback_cost) {
        value = 10000 + static_cast<int>(rng() % 500000);
    }
    problem.facility_cost.resize(problem.candidate_count);
    for (long long& value : problem.facility_cost) {
        value = static_cast<long long>(rng() % 10001) - 5000;
    }
    facility_location_restrictions restrictions;
    restrictions.fixed_facilities = {1, 4};
    restrictions.candidate_facilities = {0, 1, 2, 3, 4, 5, 7, 8, 10, 12};
    std::vector<int> initial = {1, 2, 4, 8};
    facility_location_workspace<int, long long> workspace(
        problem, static_cast<int>(initial.size()), restrictions);

    for (std::uint64_t seed : {1ULL, 9ULL, 123456789ULL}) {
        facility_location_options options;
        options.seed = seed;
        options.max_perturbations = 7;
        options.max_candidate_evaluations = 150;
        const Result one_shot = improve_k_median(
            problem, initial, restrictions, options);
        const Result reused = workspace.improve(initial, options);
        require(one_shot.cost == reused.cost &&
                    one_shot.facilities == reused.facilities &&
                    one_shot.assignment == reused.assignment &&
                    one_shot.statistics.candidate_evaluations ==
                        reused.statistics.candidate_evaluations &&
                    one_shot.statistics.accepted_swaps ==
                        reused.statistics.accepted_swaps,
                "workspace improve differs from one-shot call");
    }

    facility_location_options options;
    options.seed = 55;
    options.initial_trials = 3;
    options.max_perturbations = 5;
    const Result one_shot = solve_k_median(problem, 4, restrictions, options);
    const Result reused = workspace.solve(options);
    require(one_shot.cost == reused.cost &&
                one_shot.facilities == reused.facilities,
            "workspace solve differs from one-shot call");
}

void test_floating_point() {
    facility_location_problem<double, double> problem;
    problem.demand_count = 2;
    problem.candidate_count = 3;
    problem.cost = {0.0, 2.5, 1.0, 1.0, 3.0, 0.0};
    problem.demand_weight = {0.5, 2.0};
    facility_location_options options;
    options.max_perturbations = 3;
    const auto result = solve_k_median(problem, 1, options);
    require(std::abs(result.cost - 1.5) < 1e-12,
            "floating-point result mismatch");
}

// 有効な数値境界、制約の組合せ、停止条件、反復呼出しを独立な全探索で検証する
void test_extended_contracts() {
    std::mt19937_64 rng(202609061234ULL);
    int exact_matches = 0;
    for (int test = 0; test < 1600; ++test) {
        Problem p;
        p.demand_count = 1 + static_cast<int>(rng() % 10);
        p.candidate_count = 1 + static_cast<int>(rng() % 8);
        const int k = 1 + static_cast<int>(rng() % p.candidate_count);
        p.cost.resize(static_cast<std::size_t>(p.demand_count) * p.candidate_count);
        for (int& v : p.cost) v = static_cast<int>(rng() % (test % 5 == 0 ? 2 : 100));
        if (test % 7 == 0) std::fill(p.cost.begin(), p.cost.end(), 0);
        if (test % 11 == 0) std::fill(p.cost.begin(), p.cost.end(), std::numeric_limits<int>::max());
        if (test & 1) {
            p.demand_weight.resize(p.demand_count);
            for (auto& w : p.demand_weight) w = static_cast<long long>(rng() % 6);
        }
        if (test & 2) {
            p.fallback_cost.resize(p.demand_count);
            for (auto& v : p.fallback_cost) v = static_cast<int>(rng() % 100);
        }
        if (test & 4) {
            p.facility_cost.resize(p.candidate_count);
            for (auto& v : p.facility_cost) v = static_cast<long long>(rng() % 201) - 100;
        }
        std::vector<int> ids(p.candidate_count);
        std::iota(ids.begin(), ids.end(), 0);
        std::shuffle(ids.begin(), ids.end(), rng);
        std::vector<int> initial(ids.begin(), ids.begin() + k);
        facility_location_restrictions restrictions;
        const int fixed_count = static_cast<int>(rng() % (k + 1));
        restrictions.fixed_facilities.assign(ids.begin(), ids.begin() + fixed_count);
        std::vector<int> allowed = ids;
        if (test & 8) {
            const int pool_size = k + static_cast<int>(rng() % (p.candidate_count - k + 1));
            allowed.resize(pool_size);
            restrictions.candidate_facilities = allowed;
            if (test & 16) restrictions.candidate_facilities.erase(
                restrictions.candidate_facilities.begin(),
                restrictions.candidate_facilities.begin() + fixed_count);
        }
        // 空の候補vectorは全候補許可なので、空集合との混同を避ける
        if (restrictions.candidate_facilities.empty()) allowed = ids;
        facility_location_options options;
        options.seed = rng(); options.max_perturbations = 0;
        const auto local = improve_k_median(p, initial, restrictions, options);
        check_result(p, k, local); check_restrictions(local, restrictions);
        require(local.cost <= evaluate_k_median(p, initial), "extended improve monotonicity");
        auto moved = local.facilities;
        for (int incoming : allowed) {
            if (std::find(moved.begin(), moved.end(), incoming) != moved.end()) continue;
            for (int position = 0; position < k; ++position) {
                const int outgoing = moved[position];
                if (std::find(restrictions.fixed_facilities.begin(), restrictions.fixed_facilities.end(), outgoing) != restrictions.fixed_facilities.end()) continue;
                moved[position] = incoming;
                require(evaluate_k_median(p, moved) >= local.cost, "extended local optimum");
                moved[position] = outgoing;
            }
        }
        const long long optimum = exact_restricted_optimum(p, k, restrictions);
        require(local.cost >= optimum, "extended exact lower bound");
        exact_matches += local.cost == optimum;

        // 同一workspaceをsolve/improve交互に使い、全ての組合せでone-shotに照合する
        facility_location_workspace<int, long long> ws(p, k, restrictions);
        for (int repeat = 0; repeat < 3; ++repeat) {
            options.seed = rng(); options.initial_trials = repeat + 1;
            options.max_perturbations = repeat;
            options.max_candidate_evaluations = static_cast<std::uint64_t>(repeat * 7);
            const auto one = solve_k_median(p, k, restrictions, options);
            const auto reused = ws.solve(options);
            check_result(p, k, one); check_restrictions(one, restrictions);
            require(one.cost >= optimum, "extended solve exact lower bound");
            require(one.facilities == reused.facilities && one.cost == reused.cost &&
                        one.assignment == reused.assignment &&
                        one.statistics.candidate_evaluations == reused.statistics.candidate_evaluations,
                    "extended workspace solve equality");
            const auto io = improve_k_median(p, initial, restrictions, options);
            const auto iw = ws.improve(initial, options);
            require(io.cost == iw.cost && io.facilities == iw.facilities &&
                        io.assignment == iw.assignment &&
                        io.statistics.candidate_evaluations == iw.statistics.candidate_evaluations,
                    "extended workspace improve equality");
            require(io.cost <= evaluate_k_median(p, initial), "bounded improve monotonicity");
            require(one.statistics.candidate_evaluations <= options.max_candidate_evaluations &&
                        io.statistics.candidate_evaluations <= options.max_candidate_evaluations,
                    "extended evaluation budget");
        }
        options.deadline = facility_location_options::clock::now() - std::chrono::milliseconds(1);
        const auto stopped = solve_k_median(p, k, restrictions, options);
        check_result(p, k, stopped); check_restrictions(stopped, restrictions);
        require(stopped.statistics.deadline_reached, "expired deadline status");
    }
    std::cout << "extended integer local-optimum checks: 1600; exact matches: " << exact_matches << '\n';

    // 小数重み・fallback・正負の単項費用を含め、doubleの局所解も全1-swapに照合する
    for (int test = 0; test < 300; ++test) {
        facility_location_problem<double, double> p;
        p.demand_count = 7; p.candidate_count = 8;
        p.cost.resize(56); p.demand_weight.resize(7);
        p.fallback_cost.resize(7); p.facility_cost.resize(8);
        for (double& v : p.cost) v = static_cast<double>(rng() % 1000) / 137.0;
        for (double& w : p.demand_weight) w = static_cast<double>(rng() % 15) / 7.0;
        for (double& v : p.fallback_cost) v = static_cast<double>(rng() % 1000) / 211.0;
        for (double& v : p.facility_cost) v = static_cast<double>(static_cast<int>(rng() % 100) - 50) / 29.0;
        facility_location_restrictions restrictions;
        restrictions.fixed_facilities = {1};
        restrictions.candidate_facilities = {0, 2, 3, 4, 5, 7};
        facility_location_options options;
        options.max_perturbations = 0; options.max_candidate_evaluations = 10000;
        options.seed = rng();
        const auto result = improve_k_median(p, {1, 2, 4}, restrictions, options);
        const double evaluated = evaluate_k_median(p, result.facilities);
        require(std::abs(evaluated - result.cost) <= 1e-9, "extended floating objective");
        require(result.cost <= evaluate_k_median(p, std::vector<int>{1, 2, 4}) + 1e-9, "extended floating monotonicity");
        auto moved = result.facilities;
        for (int incoming : restrictions.candidate_facilities) {
            if (std::find(moved.begin(), moved.end(), incoming) != moved.end()) continue;
            for (int position = 0; position < 3; ++position) {
                const int outgoing = moved[position]; if (outgoing == 1) continue;
                moved[position] = incoming;
                require(evaluate_k_median(p, moved) + 1e-9 >= result.cost, "extended floating local optimum");
                moved[position] = outgoing;
            }
        }
    }
    std::cout << "extended fractional floating checks: 300\n";
}

// 3需要点×4施設の全4096通りの0/1費用を調べる。
// 全ての2施設初期解で、同値・fallback・固定施設・正負単項費用を組み合わせる。
void test_exhaustive_binary_states() {
    int cases = 0;
    for (unsigned pattern = 0; pattern < 4096; ++pattern) {
        Problem p;
        p.demand_count = 3;
        p.candidate_count = 4;
        p.cost.resize(12);
        for (int index = 0; index < 12; ++index) {
            p.cost[index] = static_cast<int>((pattern >> index) & 1U);
        }
        if (pattern & 1U) p.demand_weight = {1, 0, 2};
        if (pattern & 2U) p.fallback_cost = {1, 0, 1};
        if (pattern & 4U) p.facility_cost = {-1, 0, 1, -1};
        for (int a = 0; a < 4; ++a) {
            for (int b = a + 1; b < 4; ++b) {
                facility_location_restrictions restrictions;
                if (pattern & 8U) restrictions.fixed_facilities = {a};
                if (pattern & 16U) restrictions.candidate_facilities = {a, b, (b + 1) % 4};
                std::sort(restrictions.candidate_facilities.begin(), restrictions.candidate_facilities.end());
                restrictions.candidate_facilities.erase(
                    std::unique(restrictions.candidate_facilities.begin(), restrictions.candidate_facilities.end()),
                    restrictions.candidate_facilities.end());
                facility_location_options options;
                options.seed = 700001U + pattern;
                options.max_perturbations = 3;
                const std::vector<int> initial{a, b};
                const auto result = improve_k_median(p, initial, restrictions, options);
                check_result(p, 2, result);
                check_restrictions(result, restrictions);
                require(result.cost <= evaluate_k_median(p, initial), "binary monotonicity");
                for (int incoming = 0; incoming < 4; ++incoming) {
                    if (!restrictions.candidate_facilities.empty() &&
                        !std::binary_search(restrictions.candidate_facilities.begin(), restrictions.candidate_facilities.end(), incoming)) continue;
                    if (std::find(result.facilities.begin(), result.facilities.end(), incoming) != result.facilities.end()) continue;
                    auto moved = result.facilities;
                    for (int position = 0; position < 2; ++position) {
                        const int old = moved[position];
                        if (std::find(restrictions.fixed_facilities.begin(), restrictions.fixed_facilities.end(), old) != restrictions.fixed_facilities.end()) continue;
                        moved[position] = incoming;
                        require(evaluate_k_median(p, moved) >= result.cost, "binary local optimum");
                        moved[position] = old;
                    }
                }
                ++cases;
            }
        }
    }
    require(cases == 24576, "binary test count");
    std::cout << "exhaustive binary initial states: " << cases << '\n';
}

void test_canonical_return_cost() {
    facility_location_problem<int, double> problem;
    problem.demand_count = 1;
    problem.candidate_count = 3;
    problem.cost = {0, 0, 0};
    problem.facility_cost = {1e16, -1e16, 1.0};
    facility_location_restrictions restrictions;
    restrictions.fixed_facilities = {2};
    facility_location_options options;
    options.max_perturbations = 0;
    for (const auto& result : {
             improve_k_median(problem, {0, 1, 2}, restrictions, options),
             solve_k_median(problem, 3, restrictions, options)}) {
        require(result.cost == evaluate_k_median(problem, result.facilities),
                "returned cost differs from canonical evaluation");
        require(result.cost == 1.0, "canonical cancellation regression");
    }
}

void test_additional_numeric_types() {
    {
        using Wide = __int128_t;
        facility_location_problem<long long, Wide> p;
        p.demand_count = 2;
        p.candidate_count = 3;
        constexpr long long unit = 1000000000000000000LL;
        p.cost = {4 * unit, 4 * unit, unit, 2 * unit, 2 * unit, unit};
        p.demand_weight = {20, 10};
        facility_location_options options;
        options.max_perturbations = 2;
        auto result = solve_k_median(p, 1, options);
        require(result.cost == static_cast<Wide>(40) * unit, "128-bit accumulation");
        p.fallback_cost = {unit / 2, unit / 2};
        p.facility_cost = {-static_cast<Wide>(10) * unit,
                           static_cast<Wide>(10) * unit,
                          -static_cast<Wide>(10) * unit};
        facility_location_restrictions restrictions;
        restrictions.fixed_facilities = {0};
        restrictions.candidate_facilities = {1, 2};
        result = solve_k_median(p, 2, restrictions, options);
        require(result.facilities == std::vector<int>({0, 2}) &&
                    result.cost == -static_cast<Wide>(5) * unit,
                "128-bit signed constrained objective");
        require(result.cost == evaluate_k_median(p, result.facilities),
                "128-bit canonical evaluation");
    }
    {
        facility_location_problem<float, long double> p;
        p.demand_count = 2;
        p.candidate_count = 3;
        p.cost = {1.5F, 2.5F, 1.0F, 1.0F, 3.0F, 0.0F};
        p.demand_weight = {0.5L, 2.0L};
        p.fallback_cost = {2.0F, 2.0F};
        p.facility_cost = {-0.5L, 0.25L, 1.0L};
        facility_location_options options;
        options.max_perturbations = 2;
        facility_location_workspace<float, long double> workspace(p, 1);
        for (const auto& result : {solve_k_median(p, 1, options),
                                  workspace.solve(options),
                                  workspace.improve({0}, options)}) {
            require(result.facilities == std::vector<int>({2}) && result.cost == 2.0L,
                    "float / long double objective");
            require(result.cost == evaluate_k_median(p, result.facilities),
                    "long double canonical evaluation");
        }
    }
}


// 小数の丸め誤差による循環を、3需要点・3施設の最小構成で再現する
void test_floating_cycle_regression() {
    facility_location_problem<double, double> p;
    p.demand_count = 3;
    p.candidate_count = 3;
    p.cost = {0.5, 0.3, 0.3, 0.3, 0.1, 0.4, 0.4, 0.5, 0.2};
    p.facility_cost = {-1.1, -0.8, -1.1};
    facility_location_options options;
    options.max_perturbations = 0;
    options.max_candidate_evaluations = 1000;
    facility_location_workspace<double, double> workspace(p, 1);
    for (int initial = 0; initial < 3; ++initial) {
        for (const auto& result : {improve_k_median(p, {initial}, options),
                                  workspace.improve({initial}, options)}) {
            require(result.cost == 0.0, "cancelled objective changed");
            require(result.statistics.accepted_swaps == 0, "equal-cost swap accepted");
            require(result.statistics.candidate_evaluations == 2,
                    "equal-cost candidates did not finish in one pass");
            require(!result.statistics.evaluation_limit_reached,
                    "floating-point search cycles until the evaluation limit");
        }
    }
    options.max_candidate_evaluations = std::numeric_limits<std::uint64_t>::max();
    require(improve_k_median(p, {0}, options).statistics.accepted_swaps == 0,
            "unbounded floating-point search did not reject roundoff swaps");
}

template <class Cost, class Calc>
void check_floating_result(const facility_location_problem<Cost, Calc>& p,
                           const std::vector<int>& initial,
                           const facility_location_result<Calc>& result,
                           const facility_location_restrictions& restrictions) {
    require(std::is_sorted(result.facilities.begin(), result.facilities.end()),
            "floating facilities not sorted");
    require(std::adjacent_find(result.facilities.begin(), result.facilities.end()) ==
                result.facilities.end(), "floating duplicate facility");
    require(result.facilities.size() == initial.size(), "floating facility count");
    auto canonical_initial = initial;
    std::sort(canonical_initial.begin(), canonical_initial.end());
    require(result.cost <= evaluate_k_median(p, canonical_initial),
            "floating canonical objective worsened");
    require(result.cost == evaluate_k_median(p, result.facilities),
            "floating returned objective differs from evaluation");
    require(!result.statistics.evaluation_limit_reached,
            "floating search exhausted a sufficient evaluation budget");
    for (int facility : restrictions.fixed_facilities) {
        require(std::binary_search(result.facilities.begin(), result.facilities.end(), facility),
                "floating fixed facility removed");
    }
    for (int facility : result.facilities) {
        require(0 <= facility && facility < p.candidate_count, "floating invalid ID");
        require(restrictions.candidate_facilities.empty() ||
                    std::find(restrictions.fixed_facilities.begin(), restrictions.fixed_facilities.end(), facility) != restrictions.fixed_facilities.end() ||
                    std::find(restrictions.candidate_facilities.begin(), restrictions.candidate_facilities.end(), facility) != restrictions.candidate_facilities.end(),
                "floating forbidden facility");
    }
    // 公開評価関数とは独立に、全施設走査で割当てと目的値を照合する
    Calc expected{};
    for (int demand = 0; demand < p.demand_count; ++demand) {
        int owner = -1;
        Cost best = p.fallback_cost.empty() ? std::numeric_limits<Cost>::max() : p.fallback_cost[demand];
        for (int facility : result.facilities) {
            const Cost cost = p.cost[static_cast<std::size_t>(facility) * p.demand_count + demand];
            if (cost < best || (cost == best && owner == -1)) {
                best = cost;
                owner = facility;
            }
        }
        require(result.assignment[demand] == owner, "floating nearest assignment");
        const Calc weight = p.demand_weight.empty() ? Calc{1} : p.demand_weight[demand];
        expected += weight * static_cast<Calc>(best);
    }
    if (!p.facility_cost.empty()) {
        for (int facility : result.facilities) expected += p.facility_cost[facility];
    }
    require(result.cost == expected, "floating naive objective mismatch");
}

// 相殺・固定基線・並べ替えを含む小数入力で、停止とworkspace再利用を検証する
void test_cancelled_floating_objectives() {
    std::mt19937_64 rng(2026091201);
    for (int test = 0; test < 2400; ++test) {
        facility_location_problem<double, double> p;
        p.demand_count = 2 + static_cast<int>(rng() % 8);
        const int movable_count = 3 + static_cast<int>(rng() % 6);
        const bool fixed = (test & 1) != 0;
        p.candidate_count = movable_count + static_cast<int>(fixed);
        p.cost.resize(static_cast<std::size_t>(p.demand_count) * p.candidate_count);
        p.facility_cost.resize(p.candidate_count);
        for (int facility = 0; facility < movable_count; ++facility) {
            double sum = 0;
            for (int demand = 0; demand < p.demand_count; ++demand) {
                const double cost = static_cast<double>(rng() % 6) / 10.0;
                p.cost[static_cast<std::size_t>(facility) * p.demand_count + demand] = cost;
                sum += cost;
            }
            p.facility_cost[facility] = -sum;
            if (test % 3 == 0) p.facility_cost[facility] = std::nextafter(-sum, -INFINITY);
        }
        facility_location_restrictions restrictions;
        std::vector<int> initial{static_cast<int>(rng() % movable_count)};
        if (fixed) {
            for (int demand = 0; demand < p.demand_count; ++demand) {
                p.cost[static_cast<std::size_t>(movable_count) * p.demand_count + demand] = 1;
            }
            p.facility_cost[movable_count] = 0.1;
            restrictions.fixed_facilities = {movable_count};
            initial.insert(initial.begin(), movable_count);
        }
        facility_location_options options;
        options.seed = rng();
        options.max_perturbations = 0;
        options.max_candidate_evaluations = 1000;
        facility_location_workspace<double, double> workspace(p, static_cast<int>(initial.size()), restrictions);
        const auto result = improve_k_median(p, initial, restrictions, options);
        check_floating_result(p, initial, result, restrictions);
        // 可動1施設なので訪問可能な集合数はmovable_count以下となる
        require(result.statistics.accepted_swaps < static_cast<std::uint64_t>(movable_count),
                "floating search revisited a facility set");
        for (int repeat = 0; repeat < 2; ++repeat) {
            const auto reused = workspace.improve(initial, options);
            check_floating_result(p, initial, reused, restrictions);
            require(reused.facilities == result.facilities && reused.cost == result.cost &&
                        reused.statistics.candidate_evaluations == result.statistics.candidate_evaluations &&
                        reused.statistics.accepted_swaps == result.statistics.accepted_swaps,
                    "floating rollback corrupted reusable state");
        }
        const auto solved = workspace.solve(options);
        check_floating_result(p, solved.facilities, solved, restrictions);
    }
    std::cout << "cancelled floating objective inputs: 2400\n";
}

template <class Cost, class Calc>
void test_floating_rollback_and_types() {
    std::mt19937_64 rng(2026091202);
    for (int test = 0; test < 1200; ++test) {
        facility_location_problem<Cost, Calc> p;
        p.demand_count = 1 + static_cast<int>(rng() % 9);
        p.candidate_count = 2 + static_cast<int>(rng() % 7);
        const int k = 1 + static_cast<int>(rng() % p.candidate_count);
        p.cost.resize(static_cast<std::size_t>(p.demand_count) * p.candidate_count);
        for (auto& cost : p.cost) cost = static_cast<Cost>(rng() % 20) / Cost{10};
        if (test & 1) {
            p.demand_weight.resize(p.demand_count);
            for (auto& weight : p.demand_weight) weight = static_cast<Calc>(rng() % 4) / Calc{3};
        }
        if (test & 2) {
            p.fallback_cost.resize(p.demand_count);
            for (auto& cost : p.fallback_cost) cost = static_cast<Cost>(rng() % 20) / Cost{10};
        }
        if (test & 4) {
            p.facility_cost.resize(p.candidate_count);
            for (auto& cost : p.facility_cost) cost = static_cast<Calc>(static_cast<int>(rng() % 41) - 20) / Calc{10};
        }
        std::vector<int> ids(p.candidate_count);
        std::iota(ids.begin(), ids.end(), 0);
        std::shuffle(ids.begin(), ids.end(), rng);
        std::vector<int> initial(ids.begin(), ids.begin() + k);
        facility_location_restrictions restrictions;
        if (test & 8) restrictions.fixed_facilities = {initial.back()};
        if (test & 16) restrictions.candidate_facilities.assign(ids.begin(), ids.begin() + k + static_cast<int>(rng() % (p.candidate_count - k + 1)));
        facility_location_options options;
        options.seed = rng();
        options.max_perturbations = test % 4;
        options.max_candidate_evaluations = 10000;
        facility_location_workspace<Cost, Calc> workspace(p, k, restrictions);
        const auto result = improve_k_median(p, initial, restrictions, options);
        check_floating_result(p, initial, result, restrictions);
        const auto reused = workspace.improve(initial, options);
        check_floating_result(p, initial, reused, restrictions);
        require(reused.cost == result.cost && reused.facilities == result.facilities &&
                    reused.statistics.accepted_swaps == result.statistics.accepted_swaps,
                "typed floating workspace mismatch");
    }
}

// 表示上限・数値型・ストリーム連結と、出力が結果を変更しないことを検証する
void test_debug_output() {
    const auto format = [](const auto& value) {
        std::ostringstream os;
        require(&(os << value) == &os, "debug output returned a different stream");
        return os.str();
    };
    const std::string zero_statistics =
        "{candidate_evaluations=0, accepted_swaps=0, completed_initial_trials=0, "
        "completed_perturbations=0, deadline_reached=false, evaluation_limit_reached=false}";
    require(format(facility_location_statistics{}) == zero_statistics,
            "default statistics output");
    require(format(Result{}) ==
                "facility_location_result{cost=0, facilities(size=0)=[], assignment(size=0)=[], statistics=" +
                zero_statistics + '}', "empty result output");

    // 全フィールドと負の費用・fallbackを表示し、boolalphaに依存させない
    const Result example{-12, {1, 3}, {1, -1, 3}, {8, 2, 1, 0, false, true}};
    const std::string expected =
        "facility_location_result{cost=-12, facilities(size=2)=[1, 3], "
        "assignment(size=3)=[1, -1, 3], statistics={candidate_evaluations=8, "
        "accepted_swaps=2, completed_initial_trials=1, completed_perturbations=0, "
        "deadline_reached=false, evaluation_limit_reached=true}}";
    require(format(example) == expected, "nonempty result output");
    std::ostringstream chained;
    chained << example << '\n' << example.statistics;
    require(chained.str() == expected + '\n' + format(example.statistics), "chained output");
    for (int flags = 0; flags < 4; ++flags) {
        const facility_location_statistics stats{7, 3, 2, 1, (flags & 1) != 0, (flags & 2) != 0};
        std::ostringstream os;
        os << std::boolalpha;
        const auto saved_flags = os.flags();
        os << stats;
        require(os.str() == format(stats) && os.flags() == saved_flags,
                "boolean output changed with stream flags");
        require(os.str().find(std::string("deadline_reached=") +
                    (stats.deadline_reached ? "true" : "false")) != std::string::npos &&
                os.str().find(std::string("evaluation_limit_reached=") +
                    (stats.evaluation_limit_reached ? "true" : "false")) != std::string::npos,
                "boolean output mismatch");
    }

    // 20要素境界の両側と大きい配列を確認し、末尾の番兵が表示されないことを検証する
    for (int size : {0, 1, 19, 20, 21, 10000}) {
        Result value;
        value.facilities.resize(static_cast<std::size_t>(size));
        std::iota(value.facilities.begin(), value.facilities.end(), 0);
        value.assignment.assign(static_cast<std::size_t>(size), -1);
        if (size > 20) {
            value.facilities[20] = 987654321;
            value.assignment[20] = 123456789;
        }
        const Result before = value;
        const std::string output = format(value);
        for (const std::string& name : {std::string("facilities"), std::string("assignment")}) {
            const std::string marker = name + "(size=" + std::to_string(size) + ")=[";
            const auto start = output.find(marker);
            require(start != std::string::npos, "debug array size missing");
            const auto end = output.find(']', start);
            const std::string body = output.substr(start + marker.size(), end - start - marker.size());
            const int elements = std::min(size, 20);
            require(std::count(body.begin(), body.end(), ',') ==
                        std::max(0, elements - 1) + static_cast<int>(size > 20),
                    "debug array display bound");
            require((body.find("...") != std::string::npos) == (size > 20),
                    "unexpected debug truncation");
            if (size > 20) require(body.ends_with("... (+" + std::to_string(size - 20) + ')'),
                                   "debug omitted count");
        }
        require(output.find("987654321") == std::string::npos &&
                    output.find("123456789") == std::string::npos && output.size() < 1000,
                "debug output printed hidden elements");
        require(value.cost == before.cost && value.facilities == before.facilities &&
                    value.assignment == before.assignment &&
                    format(value.statistics) == format(before.statistics), "debug output mutated result");
    }

    // 浮動小数点は利用側の書式を使い、標準整数と128ビット整数の端点も扱う
    for (auto cost : {std::numeric_limits<long long>::min(), std::numeric_limits<long long>::max()}) {
        Result value;
        value.cost = cost;
        require(format(value).starts_with("facility_location_result{cost=" + std::to_string(cost) + ','),
                "64-bit debug cost");
    }
    facility_location_result<signed char> narrow;
    narrow.cost = -7;
    require(format(narrow).starts_with("facility_location_result{cost=-7,"), "narrow numeric cost");
    for (const auto cost : {1.25L, -2.5L}) {
        facility_location_result<long double> value;
        value.cost = cost;
        std::ostringstream os;
        os << std::fixed << std::setprecision(3);
        os.fill('#');
        const auto flags = os.flags();
        os << value;
        require(os.str().starts_with(cost > 0 ? "facility_location_result{cost=1.250,"
                                              : "facility_location_result{cost=-2.500,"),
                "floating debug formatting");
        require(os.flags() == flags && os.precision() == 3 && os.fill() == '#',
                "debug output changed numeric format");
    }
    using Wide = __int128_t;
    const std::array<Wide, 5> wide_values{0, 1, -1, std::numeric_limits<Wide>::min(),
                                       std::numeric_limits<Wide>::max()};
    const std::array<std::string, 5> wide_text{
        "0", "1", "-1", "-170141183460469231731687303715884105728",
        "170141183460469231731687303715884105727"};
    for (std::size_t i = 0; i < wide_values.size(); ++i) {
        facility_location_result<Wide> value;
        value.cost = wide_values[i];
        require(format(value).starts_with("facility_location_result{cost=" + wide_text[i] + ','),
                "128-bit debug cost");
    }
    facility_location_statistics large;
    large.candidate_evaluations = std::numeric_limits<std::uint64_t>::max();
    large.accepted_swaps = large.candidate_evaluations;
    require(format(large).starts_with("{candidate_evaluations=18446744073709551615, "
                                      "accepted_swaps=18446744073709551615,"),
            "64-bit debug statistics");

    // 新規構築・既存解改善・workspaceの全返却経路をconstのまま直接出力する
    Problem p{3, 3, {0, 8, 9, 8, 0, 9, 3, 4, 0}, {}, {5, 5, 5}, {-1, 2, 0}};
    facility_location_restrictions restrictions{{0}, {1, 2}};
    facility_location_options options;
    options.max_perturbations = 0;
    facility_location_workspace<int, long long> workspace(p, 2, restrictions);
    for (const auto& result : {solve_k_median(p, 2, options), improve_k_median(p, {0, 1}, options),
                              solve_k_median(p, 2, restrictions, options),
                              improve_k_median(p, {0, 1}, restrictions, options),
                              workspace.solve(options), workspace.improve({0, 1}, options)}) {
        check_result(p, 2, result);
        require(format(result).starts_with("facility_location_result{cost=" +
                    std::to_string(result.cost) + ','), "returned result debug output");
    }
}

}  // namespace

int main() {
    try {
        test_debug_output();
        test_floating_cycle_regression();
        test_cancelled_floating_objectives();
        test_floating_rollback_and_types<double, double>();
        test_floating_rollback_and_types<float, float>();
        test_floating_rollback_and_types<float, long double>();
        std::cout << "floating rollback/type inputs: 3600\n";
        test_canonical_return_cost();
        test_additional_numeric_types();
        test_exhaustive_binary_states();
        test_extended_contracts();
        test_edge_cases();
        test_local_optimum_and_improve();
        test_random_solve_and_exact_bounds();
        test_restricted_generalized_objective();
        test_fallback_assignment_and_fixed_only();
        test_candidate_evaluation_limit_and_statistics();
        test_workspace_matches_one_shot_calls();
        test_floating_point();
        std::cout << "all tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "test failed: " << error.what() << '\n';
        return 1;
    }
}

#endif
