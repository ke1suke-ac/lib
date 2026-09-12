#pragma once

/*
最大被覆・予算付き最大被覆・最小費用集合被覆をヒューリスティックに解くライブラリ

重要対象を 0..target_count-1、候補を 0..candidate_count-1 として、各候補が
被覆する対象の一覧を与える。グラフ距離や幾何距離から被覆関係を作る処理は本ライブラリの
外で行い、本ライブラリは圧縮済みの集合系だけを扱う。

主な用途:
- 最大 K 個の施設・センサー・操作を選び、重要対象の重み和を最大化する
- 候補ごとの費用と総予算の下で、被覆する重要対象の重み和を最大化する
- 全重要対象を被覆する候補集合の費用を小さくする

基本的な使い方:
coverage_problem problem{weights, costs, covered_targets};
coverage_solver solver(problem);
auto a = solver.solve_maximum_coverage(選択数上限);
auto b = solver.solve_budgeted_maximum_coverage(予算);
auto c = solver.solve_minimum_set_cover();

探索の流れ:
1. 重複候補と、安価な上位集合に包含支配される候補を安全に除去する
2. 空状態の限界利得を前計算し、線形時間で構築したheapによるlazy貪欲法で初期解を作る
3. 選択数制約では同点遷移を許すrandom/related ruin LNSを使う
4. 予算制約ではランダム再始動と1-swapを使う
5. 集合被覆では希少対象を優先する疎repairと3種類のruin LNSを使う
6. 常に最良の実行可能解を保持する

制約と注意:
- 対象重みと候補費用は非負の long long とする
- covered_targets 内の重複は構築時に除去する
- 複数予算、候補間の競合・依存、複数回被覆による追加利得は扱わない
- 重み和、費用和、CSR要素数がそれぞれの整数型の範囲に収まることは呼び出し側で保証する
- 同じ被覆関係のまま、coverage_subproblemで固定・禁止・候補pool・重み・費用を差し替えられる
- coverage_cardinality_bounds{K, K}でexact Kを指定し、evaluateで外部解を再評価できる
- std::spanが参照する入力配列はメソッドが返るまで保持する
- 時間上限はsoft limit。相対時間だけなら決定的初期解を完成させる。絶対deadlineは
  greedy開始時とheap取り出し256回ごとに確認するが、前処理・冗長除去・補充は途中停止しない
- constメソッドでも作業配列を再利用するため、同じsolverへの同時呼び出しは行わない
- 計算量表記ではTを対象数、Cを候補数、Lを被覆関係総数、Sを候補1個の最大被覆数とする
*/

#include <bits/stdc++.h>

struct coverage_problem {
    std::vector<long long> target_weights;
    std::vector<long long> candidate_costs;
    std::vector<std::vector<int>> covered_targets;
    std::vector<int> forced_candidates;
    std::vector<int> forbidden_candidates;
};

struct coverage_solver_options {
    // 改善探索の時間上限。-1なら時間で停止しない
    int time_limit_ms = 100;
    // ランダム再始動、swap、LNSの試行回数上限
    long long iteration_limit = std::numeric_limits<long long>::max();
    // 同じ入力・同じ反復回数で探索を再現する乱数seed
    std::uint64_t seed = 1;
    // 外側solverと共有する絶対締切。初期解構築もこの時刻を確認する
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::time_point::max();
};

struct coverage_solution {
    std::vector<int> selected_candidates;
    long long covered_weight = 0;
    long long total_cost = 0;
    int covered_target_count = 0;
    int uncovered_target_count = 0;
    long long iterations = 0;
    bool feasible = false;
};


// 呼び出し中だけ参照する部分問題条件。空spanは差し替えなしを表す
struct coverage_subproblem {
    std::span<const int> fixed_candidates{};
    std::span<const int> forbidden_candidates{};
    bool restrict_to_candidate_pool = false;
    std::span<const int> candidate_pool{};
    std::span<const long long> target_weights_override{};
    std::span<const long long> candidate_costs_override{};
};

struct coverage_cardinality_bounds {
    int min_selected = 0;
    int max_selected = std::numeric_limits<int>::max();
};

struct coverage_evaluation {
    long long covered_weight = 0;
    long long total_cost = 0;
    int covered_target_count = 0;
    int uncovered_target_count = 0;
    int selected_count = 0;
};

class coverage_solver {
    struct fast_random {
        std::uint64_t state;

        explicit fast_random(std::uint64_t seed) : state(seed + 0x9e3779b97f4a7c15ULL) {}

        std::uint64_t next_u64() {
            std::uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return z ^ (z >> 31);
        }

        int next_int(int upper_bound) {
            assert(upper_bound > 0);
            return static_cast<int>(next_u64() % static_cast<std::uint64_t>(upper_bound));
        }

    };


    // CSRをコピーせず、通常問題と部分問題を同じ探索で扱う読み取り専用view
    struct problem_view {
        int target_count = 0;
        int candidate_count = 0;
        std::span<const long long> target_weights, candidate_costs, candidate_weight_sums;
        std::span<const int> candidate_offsets, candidate_targets, target_offsets, target_candidates;
        std::span<const long long> target_rarity;
        // activeは前処理・当該部分問題の禁止条件も合成した最終的な探索可否
        std::span<const unsigned char> forced, forbidden, active;

        void check_candidate(int candidate) const {
            if (candidate < 0 || candidate >= candidate_count) {
                throw std::invalid_argument("候補番号が範囲外");
            }
        }
    };

    struct prepared_problem {
        int target_count = 0;
        int candidate_count = 0;
        std::vector<long long> target_weights;
        std::vector<long long> candidate_costs;
        std::vector<long long> candidate_weight_sums;
        std::vector<int> candidate_offsets;
        std::vector<int> candidate_targets;
        std::vector<int> target_offsets;
        std::vector<int> target_candidates;
        std::vector<long long> target_rarity;
        std::vector<unsigned char> forced;
        std::vector<unsigned char> forbidden;
        std::vector<unsigned char> active;

        explicit prepared_problem(const coverage_problem& problem) {
            if (problem.candidate_costs.size() != problem.covered_targets.size()) {
                throw std::invalid_argument("candidate_costs と covered_targets の要素数が異なる");
            }
            if (problem.target_weights.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
                problem.candidate_costs.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                throw std::invalid_argument("対象数または候補数が int の範囲を超えている");
            }

            target_count = static_cast<int>(problem.target_weights.size());
            candidate_count = static_cast<int>(problem.candidate_costs.size());
            target_weights = problem.target_weights;
            candidate_costs = problem.candidate_costs;
            candidate_weight_sums.assign(candidate_count, 0);
            forced.assign(candidate_count, 0);
            forbidden.assign(candidate_count, 0);
            active.assign(candidate_count, 1);

            for (long long weight : target_weights) {
                if (weight < 0) throw std::invalid_argument("対象重みは非負でなければならない");
            }
            for (long long cost : candidate_costs) {
                if (cost < 0) throw std::invalid_argument("候補費用は非負でなければならない");
            }
            const auto check_candidate = [&](int candidate) {
                if (candidate < 0 || candidate >= candidate_count) {
                    throw std::invalid_argument("候補番号が範囲外");
                }
            };
            for (int candidate : problem.forced_candidates) {
                check_candidate(candidate);
                forced[candidate] = 1;
            }
            for (int candidate : problem.forbidden_candidates) {
                check_candidate(candidate);
                forbidden[candidate] = 1;
            }
            for (int candidate = 0; candidate < candidate_count; ++candidate) {
                if (forced[candidate] && forbidden[candidate]) {
                    throw std::invalid_argument("同じ候補を選択固定と選択禁止にできない");
                }
                if (forbidden[candidate]) active[candidate] = 0;
            }

            // 各候補の対象一覧を正規化し、候補方向のCSRを構築する
            std::vector<std::vector<int>> normalized = problem.covered_targets;
            std::vector<std::uint64_t> coverage_hash(candidate_count, 0);
            candidate_offsets.resize(problem.candidate_costs.size() + 1);
            for (int candidate = 0; candidate < candidate_count; ++candidate) {
                auto& targets = normalized[candidate];
                std::sort(targets.begin(), targets.end());
                targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
                for (int target : targets) {
                    if (target < 0 || target >= target_count) {
                        throw std::invalid_argument("covered_targets に範囲外の対象番号がある");
                    }
                    candidate_weight_sums[candidate] += target_weights[target];
                }
                if (targets.size() > static_cast<std::size_t>(
                                         std::numeric_limits<int>::max() -
                                         candidate_offsets[candidate])) {
                    throw std::invalid_argument("被覆関係数が int の範囲を超えている");
                }
                candidate_offsets[candidate + 1] =
                    candidate_offsets[candidate] + static_cast<int>(targets.size());
                candidate_targets.insert(candidate_targets.end(), targets.begin(), targets.end());
                if (targets.empty() && !forced[candidate]) active[candidate] = 0;
                std::uint64_t hash = 0x9e3779b97f4a7c15ULL;
                for (int target : targets) {
                    hash ^= static_cast<std::uint64_t>(target + 1) + 0x9e3779b97f4a7c15ULL +
                            (hash << 6) + (hash >> 2);
                }
                coverage_hash[candidate] = hash;
            }

            // 被覆集合が完全一致する候補は、固定候補がなければ最安候補だけを有効にする
            std::vector<int> duplicate_order;
            duplicate_order.reserve(candidate_count);
            for (int candidate = 0; candidate < candidate_count; ++candidate) {
                if (active[candidate]) duplicate_order.push_back(candidate);
            }
            std::sort(duplicate_order.begin(), duplicate_order.end(), [&](int lhs, int rhs) {
                if (coverage_hash[lhs] != coverage_hash[rhs]) return coverage_hash[lhs] < coverage_hash[rhs];
                const int lhs_size = candidate_offsets[lhs + 1] - candidate_offsets[lhs];
                const int rhs_size = candidate_offsets[rhs + 1] - candidate_offsets[rhs];
                if (lhs_size != rhs_size) return lhs_size < rhs_size;
                return lhs < rhs;
            });
            for (int begin = 0; begin < static_cast<int>(duplicate_order.size());) {
                int end = begin + 1;
                const int first = duplicate_order[begin];
                const int target_size = candidate_offsets[first + 1] - candidate_offsets[first];
                while (end < static_cast<int>(duplicate_order.size()) &&
                       coverage_hash[duplicate_order[end]] == coverage_hash[first] &&
                       candidate_offsets[duplicate_order[end] + 1] -
                               candidate_offsets[duplicate_order[end]] == target_size) {
                    ++end;
                }

                // 同一hash・同一長の範囲を完全一致する被覆列ごとに分割する
                std::vector<std::vector<int>> equal_groups;
                for (int index = begin; index < end; ++index) {
                    const int candidate = duplicate_order[index];
                    bool found = false;
                    for (auto& group : equal_groups) {
                        const int representative = group.front();
                        const bool equal = std::equal(
                            candidate_targets.begin() + candidate_offsets[candidate],
                            candidate_targets.begin() + candidate_offsets[candidate + 1],
                            candidate_targets.begin() + candidate_offsets[representative]);
                        if (equal) {
                            group.push_back(candidate);
                            found = true;
                            break;
                        }
                    }
                    if (!found) equal_groups.push_back({candidate});
                }
                for (const auto& group : equal_groups) {
                    int forced_count = 0;
                    for (int candidate : group) forced_count += forced[candidate];
                    if (forced_count > 0) {
                        for (int candidate : group) {
                            if (!forced[candidate]) active[candidate] = 0;
                        }
                    } else {
                        int keep = group.front();
                        for (int candidate : group) {
                            if (candidate_costs[candidate] < candidate_costs[keep] ||
                                (candidate_costs[candidate] == candidate_costs[keep] && candidate < keep)) {
                                keep = candidate;
                            }
                        }
                        for (int candidate : group) {
                            if (candidate != keep) active[candidate] = 0;
                        }
                    }
                }
                begin = end;
            }

            // 対象方向のCSRを構築し、related ruinと集合被覆repairに利用する
            target_offsets.assign(problem.target_weights.size() + 1, 0);
            for (int target : candidate_targets) ++target_offsets[target + 1];
            for (int target = 0; target < target_count; ++target) {
                target_offsets[target + 1] += target_offsets[target];
            }
            target_candidates.resize(candidate_targets.size());
            std::vector<int> cursor = target_offsets;
            for (int candidate = 0; candidate < candidate_count; ++candidate) {
                for (int index = candidate_offsets[candidate]; index < candidate_offsets[candidate + 1]; ++index) {
                    const int target = candidate_targets[index];
                    target_candidates[cursor[target]++] = candidate;
                }
            }

            // 安価な上位集合候補が存在する候補を、希少対象を使った限定探索で安全に除去する
            std::vector<int> dominance_candidates;
            for (int candidate = 0; candidate < candidate_count; ++candidate) {
                if (!active[candidate] || forced[candidate]) continue;
                const int begin = candidate_offsets[candidate];
                const int end = candidate_offsets[candidate + 1];
                if (begin == end) continue;

                int rare_target = candidate_targets[begin];
                for (int index = begin + 1; index < end; ++index) {
                    const int target = candidate_targets[index];
                    if (target_offsets[target + 1] - target_offsets[target] <
                        target_offsets[rare_target + 1] - target_offsets[rare_target]) {
                        rare_target = target;
                    }
                }
                dominance_candidates.clear();
                for (int index = target_offsets[rare_target];
                     index < target_offsets[rare_target + 1]; ++index) {
                    const int other = target_candidates[index];
                    if (other == candidate || !active[other] ||
                        candidate_costs[other] > candidate_costs[candidate] ||
                        candidate_offsets[other + 1] - candidate_offsets[other] < end - begin) {
                        continue;
                    }
                    dominance_candidates.push_back(other);
                }
                constexpr int dominance_check_limit = 96;
                if (static_cast<int>(dominance_candidates.size()) > dominance_check_limit) {
                    std::nth_element(
                        dominance_candidates.begin(),
                        dominance_candidates.begin() + dominance_check_limit,
                        dominance_candidates.end(), [&](int lhs, int rhs) {
                            if (candidate_costs[lhs] != candidate_costs[rhs]) {
                                return candidate_costs[lhs] < candidate_costs[rhs];
                            }
                            const int lhs_size = candidate_offsets[lhs + 1] - candidate_offsets[lhs];
                            const int rhs_size = candidate_offsets[rhs + 1] - candidate_offsets[rhs];
                            if (lhs_size != rhs_size) return lhs_size > rhs_size;
                            return lhs < rhs;
                        });
                    dominance_candidates.resize(dominance_check_limit);
                }
                for (int other : dominance_candidates) {
                    if (std::includes(candidate_targets.begin() + candidate_offsets[other],
                                      candidate_targets.begin() + candidate_offsets[other + 1],
                                      candidate_targets.begin() + begin,
                                      candidate_targets.begin() + end)) {
                        active[candidate] = 0;
                        break;
                    }
                }
            }

            // 集合被覆用に、被覆可能候補が少ない対象ほど大きい希少度を前計算する
            target_rarity.resize(target_count);
            for (int target = 0; target < target_count; ++target) {
                int available = 0;
                for (int index = target_offsets[target]; index < target_offsets[target + 1]; ++index) {
                    const int candidate = target_candidates[index];
                    available += active[candidate];
                }
                target_rarity[target] = available == 0 ? 0 : std::max(1LL, 1000000LL / available);
            }
        }

    };

    enum class resource_mode {
        cardinality,
        candidate_cost,
    };

    struct state {
        const problem_view* problem = nullptr;
        std::vector<unsigned char> selected;
        std::vector<int> selected_candidates;
        std::vector<int> selected_positions;
        std::vector<int> cover_count;
        long long covered_weight = 0;
        long long total_cost = 0;
        int covered_target_count = 0;

        explicit state(const problem_view& prepared)
            : problem(&prepared),
              selected(prepared.candidate_count, 0),
              selected_positions(prepared.candidate_count, -1),
              cover_count(prepared.target_count, 0) {}

        long long used_resource(resource_mode mode) const {
            return mode == resource_mode::cardinality
                       ? static_cast<long long>(selected_candidates.size())
                       : total_cost;
        }

        long long gain(int candidate) const {
            if (covered_target_count == 0) {
                return problem->candidate_weight_sums[candidate];
            }
            long long result = 0;
            for (int index = problem->candidate_offsets[candidate];
                 index < problem->candidate_offsets[candidate + 1]; ++index) {
                const int target = problem->candidate_targets[index];
                result += (cover_count[target] == 0) * problem->target_weights[target];
            }
            return result;
        }

        long long loss(int candidate) const {
            long long result = 0;
            for (int index = problem->candidate_offsets[candidate];
                 index < problem->candidate_offsets[candidate + 1]; ++index) {
                const int target = problem->candidate_targets[index];
                result += (cover_count[target] == 1) * problem->target_weights[target];
            }
            return result;
        }

        int unique_target_count(int candidate) const {
            int result = 0;
            for (int index = problem->candidate_offsets[candidate];
                 index < problem->candidate_offsets[candidate + 1]; ++index) {
                result += cover_count[problem->candidate_targets[index]] == 1;
            }
            return result;
        }

        void add(int candidate) {
            assert(!selected[candidate]);
            selected[candidate] = 1;
            selected_positions[candidate] = static_cast<int>(selected_candidates.size());
            selected_candidates.push_back(candidate);
            total_cost += problem->candidate_costs[candidate];

            for (int index = problem->candidate_offsets[candidate];
                 index < problem->candidate_offsets[candidate + 1]; ++index) {
                const int target = problem->candidate_targets[index];
                if (cover_count[target]++ == 0) {
                    covered_weight += problem->target_weights[target];
                    ++covered_target_count;
                }
            }
        }

        void remove(int candidate) {
            assert(selected[candidate]);

            // 選択候補一覧からswap-removeし、候補番号から位置への逆引きも更新する
            const int position = selected_positions[candidate];
            const int last = selected_candidates.back();
            selected_candidates[position] = last;
            selected_positions[last] = position;
            selected_candidates.pop_back();
            selected_positions[candidate] = -1;
            selected[candidate] = 0;
            total_cost -= problem->candidate_costs[candidate];

            for (int index = problem->candidate_offsets[candidate];
                 index < problem->candidate_offsets[candidate + 1]; ++index) {
                const int target = problem->candidate_targets[index];
                if (--cover_count[target] == 0) {
                    covered_weight -= problem->target_weights[target];
                    --covered_target_count;
                }
            }
        }
    };

    struct time_keeper {
        using clock = std::chrono::steady_clock;

        clock::time_point deadline;
        clock::time_point absolute_deadline;
        long long iteration_limit;
        long long iterations = 0;

        explicit time_keeper(const coverage_solver_options& options)
            : deadline(options.time_limit_ms < 0
                           ? clock::time_point::max()
                           : clock::now() + std::chrono::milliseconds(options.time_limit_ms)),
              absolute_deadline(options.deadline),
              iteration_limit(options.iteration_limit) {
            deadline = std::min(deadline, absolute_deadline);
            if (options.time_limit_ms < -1) {
                throw std::invalid_argument("time_limit_ms は -1 以上でなければならない");
            }
            if (options.iteration_limit < 0) {
                throw std::invalid_argument("iteration_limit は非負でなければならない");
            }
        }

        bool expired(bool force_clock_check = false) const {
            if (iterations >= iteration_limit) return true;
            if (!force_clock_check && (iterations & 63LL) != 0) return false;
            return clock::now() >= deadline;
        }

        bool absolute_expired() const {
            return absolute_deadline != clock::time_point::max() && clock::now() >= absolute_deadline;
        }

    };

    enum class greedy_metric {
        gain,
        gain_per_resource,
    };

    struct greedy_node {
        double priority = 0;
        long long gain = 0;
        int candidate = -1;
    };

    struct greedy_node_less {
        bool operator()(const greedy_node& lhs, const greedy_node& rhs) const {
            if (lhs.priority != rhs.priority) return lhs.priority < rhs.priority;
            if (lhs.gain != rhs.gain) return lhs.gain < rhs.gain;
            return lhs.candidate > rhs.candidate;
        }
    };

    struct greedy_workspace {
        std::vector<long long> initial_gains;
        std::vector<int> touched_candidates;
        std::vector<unsigned char> blocked_candidates;
        std::vector<int> original_selected_stamp;
        std::vector<int> uncovered_targets;
        std::vector<greedy_node> heap_nodes;
        std::vector<int> removable_candidates;
        std::vector<int> related_overlap;
        std::vector<int> related_touched;
        std::vector<int> ruin_result;
        std::vector<int> original_selected;
        std::vector<int> added_candidates;
        int stamp = 0;
        const time_keeper* timer;
        unsigned int deadline_checks = 0;

        greedy_workspace(int candidate_count, int target_count, const time_keeper& clock_state)
            : initial_gains(candidate_count, 0),
              blocked_candidates(candidate_count, 0),
              original_selected_stamp(candidate_count, 0), timer(&clock_state) {
            uncovered_targets.reserve(target_count);
            heap_nodes.reserve(candidate_count);
            removable_candidates.reserve(candidate_count);
            related_overlap.reserve(candidate_count);
            related_touched.reserve(candidate_count);
            ruin_result.reserve(candidate_count);
            original_selected.reserve(candidate_count);
            added_candidates.reserve(candidate_count);
        }
        bool interrupted() {
            return (deadline_checks++ & 255U) == 0 && timer->absolute_expired();
        }
    };

    static double greedy_priority(long long gain, long long resource,
                                         greedy_metric metric, int factor_permille) {
        if (metric == greedy_metric::gain || resource == 0) {
            if (resource == 0 && gain > 0) return std::numeric_limits<double>::infinity();
            return static_cast<double>(gain) * static_cast<double>(factor_permille);
        }
        return static_cast<double>(gain) * static_cast<double>(factor_permille) /
               static_cast<double>(resource);
    }

    static int noise_factor(std::uint64_t construction_seed, int candidate, int noise_permille) {
        if (noise_permille == 0) return 1000;
        std::uint64_t z = construction_seed +
                          static_cast<std::uint64_t>(candidate + 1) * 0x9e3779b97f4a7c15ULL;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        z ^= z >> 31;
        const int width = noise_permille * 2 + 1;
        return 1000 - noise_permille + static_cast<int>(z % static_cast<std::uint64_t>(width));
    }

    static void add_forced_candidates(state& current) {
        for (int candidate = 0; candidate < current.problem->candidate_count; ++candidate) {
            if (current.problem->forced[candidate]) current.add(candidate);
        }
    }

    static bool better_maximum(const state& lhs, const state& rhs, resource_mode mode) {
        if (lhs.covered_weight != rhs.covered_weight) return lhs.covered_weight > rhs.covered_weight;
        if (lhs.used_resource(mode) != rhs.used_resource(mode)) {
            return lhs.used_resource(mode) < rhs.used_resource(mode);
        }
        return lhs.selected_candidates.size() < rhs.selected_candidates.size();
    }

    static void greedy_fill_maximum(state& current, long long limit, resource_mode mode,
                                    greedy_metric metric, std::uint64_t construction_seed,
                                    int noise_permille,
                                    std::vector<int>* added_candidates,
                                    greedy_workspace& workspace,
                                    bool use_sparse_initialization) {
        if (workspace.timer->absolute_expired()) return;
        const problem_view& problem = *current.problem;
        std::vector<greedy_node>& heap = workspace.heap_nodes;
        heap.clear();
        const greedy_node_less heap_less;

        const auto resource = [&](int candidate) {
            return mode == resource_mode::cardinality ? 1 : problem.candidate_costs[candidate];
        };

        // 利用可否は列挙側で確定済み。選択済み候補の未被覆利得は0なのでheapへ入らない
        auto push_candidate = [&](int candidate, long long gain) {
            const long long candidate_resource = resource(candidate);
            if (candidate_resource > limit - current.used_resource(mode) || gain <= 0) return;
            const int factor = noise_factor(construction_seed, candidate, noise_permille);
            heap.push_back({greedy_priority(gain, candidate_resource, metric, factor), gain, candidate});
        };

        // 未被覆対象が少ないrepairでは逆向きCSRから正利得候補だけを列挙する
        if (use_sparse_initialization) {
            workspace.touched_candidates.clear();
            for (int target = 0; target < problem.target_count; ++target) {
                if (current.cover_count[target] != 0 || problem.target_weights[target] == 0) continue;
                for (int index = problem.target_offsets[target];
                    index < problem.target_offsets[target + 1]; ++index) {
                    const int candidate = problem.target_candidates[index];
                    if (!problem.active[candidate]) continue;
                    if (workspace.initial_gains[candidate] == 0) {
                        workspace.touched_candidates.push_back(candidate);
                    }
                    workspace.initial_gains[candidate] += problem.target_weights[target];
                }
            }
            for (int candidate : workspace.touched_candidates) {
                const long long gain = workspace.initial_gains[candidate];
                workspace.initial_gains[candidate] = 0;
                push_candidate(candidate, gain);
            }
        } else {
            // 現状態での利得を上界としてheapへ入れる。追加だけなら利得は単調非増加となる
            for (int candidate = 0; candidate < problem.candidate_count; ++candidate) {
                if (!problem.active[candidate] || current.selected[candidate]) continue;
                push_candidate(candidate, current.gain(candidate));
            }
        }
        std::make_heap(heap.begin(), heap.end(), heap_less);

        // 取り出した候補だけ利得を再計算し、次点の古い上界以上なら選択を確定する
        while (!heap.empty()) {
            if (workspace.interrupted()) break;
            std::pop_heap(heap.begin(), heap.end(), heap_less);
            const greedy_node node = heap.back();
            heap.pop_back();
            const int candidate = node.candidate;
            const long long candidate_resource = resource(candidate);
            if (candidate_resource > limit - current.used_resource(mode)) continue;

            const long long gain = current.gain(candidate);
            if (gain <= 0) continue;
            const int factor = noise_factor(construction_seed, candidate, noise_permille);
            const double priority = greedy_priority(gain, candidate_resource, metric, factor);
            if (!heap.empty() && priority < heap.front().priority) {
                heap.push_back({priority, gain, candidate});
                std::push_heap(heap.begin(), heap.end(), heap_less);
                continue;
            }
            current.add(candidate);
            if (added_candidates != nullptr) added_candidates->push_back(candidate);
        }
    }

    static bool greedy_fill_set_cover(state& current, std::uint64_t construction_seed,
                                      int noise_permille,
                                      const std::vector<unsigned char>* blocked,
                                      std::vector<int>* added_candidates,
                                      greedy_workspace& workspace,
                                      bool use_sparse_initialization,
                                      const std::vector<int>* uncovered_targets_hint) {
        if (workspace.timer->absolute_expired()) {
            return current.covered_target_count == current.problem->target_count;
        }
        const problem_view& problem = *current.problem;
        std::vector<greedy_node>& heap = workspace.heap_nodes;
        heap.clear();
        const greedy_node_less heap_less;

        const auto uncovered_rarity_gain = [&](int candidate) {
            long long result = 0;
            for (int index = problem.candidate_offsets[candidate];
                 index < problem.candidate_offsets[candidate + 1]; ++index) {
                const int target = problem.candidate_targets[index];
                result += (current.cover_count[target] == 0) * problem.target_rarity[target];
            }
            return result;
        };

        // 利用可否は列挙側で確定済み。選択済み候補の未被覆利得は0なのでheapへ入らない
        auto push_candidate = [&](int candidate, long long gain) {
            if (gain <= 0) return;
            if (blocked != nullptr && (*blocked)[candidate]) return;
            const int factor = noise_factor(construction_seed, candidate, noise_permille);
            heap.push_back({greedy_priority(gain, problem.candidate_costs[candidate],
                                       greedy_metric::gain_per_resource, factor),
                       gain, candidate});
        };

        // 新たに被覆する対象の希少度和/費用を用いる。費用0の候補は正の利得があれば最優先する
        if (use_sparse_initialization) {
            workspace.touched_candidates.clear();
            auto add_target = [&](int target) {
                if (current.cover_count[target] != 0) return;
                const long long contribution = problem.target_rarity[target];
                if (contribution == 0) return;
                for (int index = problem.target_offsets[target];
                    index < problem.target_offsets[target + 1]; ++index) {
                    const int candidate = problem.target_candidates[index];
                    if (!problem.active[candidate]) continue;
                    if (workspace.initial_gains[candidate] == 0) {
                        workspace.touched_candidates.push_back(candidate);
                    }
                    workspace.initial_gains[candidate] += contribution;
                }
            };
            if (uncovered_targets_hint != nullptr) {
                for (int target : *uncovered_targets_hint) add_target(target);
            } else {
                for (int target = 0; target < problem.target_count; ++target) add_target(target);
            }
            for (int candidate : workspace.touched_candidates) {
                const long long gain = workspace.initial_gains[candidate];
                workspace.initial_gains[candidate] = 0;
                push_candidate(candidate, gain);
            }
        } else {
            for (int candidate = 0; candidate < problem.candidate_count; ++candidate) {
                if (!problem.active[candidate] || current.selected[candidate]) continue;
                const long long gain = uncovered_rarity_gain(candidate);
                push_candidate(candidate, gain);
            }
        }
        std::make_heap(heap.begin(), heap.end(), heap_less);

        // lazy greedyで全対象が被覆されるまで候補を追加する
        while (current.covered_target_count < problem.target_count && !heap.empty()) {
            if (workspace.interrupted()) break;
            std::pop_heap(heap.begin(), heap.end(), heap_less);
            const greedy_node node = heap.back();
            heap.pop_back();
            const int candidate = node.candidate;
            const long long gain = uncovered_rarity_gain(candidate);
            if (gain <= 0) continue;
            const int factor = noise_factor(construction_seed, candidate, noise_permille);
            const double priority =
                greedy_priority(gain, problem.candidate_costs[candidate],
                                greedy_metric::gain_per_resource, factor);
            if (!heap.empty() && priority < heap.front().priority) {
                heap.push_back({priority, gain, candidate});
                std::push_heap(heap.begin(), heap.end(), heap_less);
                continue;
            }
            current.add(candidate);
            if (added_candidates != nullptr) added_candidates->push_back(candidate);
        }
        return current.covered_target_count == problem.target_count;
    }

    static void remove_redundant_maximum(state& current) {
        // 削除すると残存候補のlossは減らない。一度残した候補の再検査は不要。
        for (int position = static_cast<int>(current.selected_candidates.size()) - 1;
             position >= 0; --position) {
            const int candidate = current.selected_candidates[position];
            if (!current.problem->forced[candidate] && current.loss(candidate) == 0) {
                current.remove(candidate);
            }
        }
    }

    static void remove_redundant_set_cover(state& current) {
        std::vector<int> order;
        order.reserve(current.selected_candidates.size());
        for (int candidate : current.selected_candidates) {
            if (!current.problem->forced[candidate]) order.push_back(candidate);
        }
        std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
            if (current.problem->candidate_costs[lhs] != current.problem->candidate_costs[rhs]) {
                return current.problem->candidate_costs[lhs] > current.problem->candidate_costs[rhs];
            }
            return lhs < rhs;
        });
        for (int candidate : order) {
            if (current.unique_target_count(candidate) == 0) {
                current.remove(candidate);
            }
        }
    }

    static int choose_ruin_size(const state& current, fast_random& random) {
        int count = 0;
        for (int candidate : current.selected_candidates) {
            count += !current.problem->forced[candidate];
        }
        if (count == 0) return 0;
        const int upper = std::max(1, count / 3);
        int size = 1;
        while (size < upper && random.next_int(100) < 50) size *= 2;
        return std::min(size, upper);
    }

    static void choose_random_ruin(const state& current, int ruin_size,
                                   fast_random& random, greedy_workspace& workspace,
                                   std::vector<int>& result) {
        std::vector<int>& candidates = workspace.removable_candidates;
        candidates.clear();
        for (int candidate : current.selected_candidates) {
            if (!current.problem->forced[candidate]) candidates.push_back(candidate);
        }
        for (int index = 0; index < ruin_size; ++index) {
            const int other = index + random.next_int(static_cast<int>(candidates.size()) - index);
            std::swap(candidates[index], candidates[other]);
        }
        result.assign(candidates.begin(), candidates.begin() + ruin_size);
    }

    static void choose_related_ruin(const state& current, int ruin_size,
                                    fast_random& random, greedy_workspace& workspace,
                                    std::vector<int>& result) {
        const problem_view& problem = *current.problem;
        std::vector<int>& removable = workspace.removable_candidates;
        removable.clear();
        for (int candidate : current.selected_candidates) {
            if (!problem.forced[candidate]) removable.push_back(candidate);
        }
        result.clear();
        if (removable.empty()) return;

        const int seed = removable[random.next_int(static_cast<int>(removable.size()))];
        std::vector<int>& overlap = workspace.related_overlap;
        overlap.assign(current.selected_candidates.size(), 0);
        std::vector<int>& touched = workspace.related_touched;
        touched.clear();
        for (int index = problem.candidate_offsets[seed];
             index < problem.candidate_offsets[seed + 1]; ++index) {
            const int target = problem.candidate_targets[index];
            for (int reverse_index = problem.target_offsets[target];
                 reverse_index < problem.target_offsets[target + 1]; ++reverse_index) {
                const int candidate = problem.target_candidates[reverse_index];
                if (!current.selected[candidate] || problem.forced[candidate]) continue;
                const int position = current.selected_positions[candidate];
                if (overlap[position]++ == 0) touched.push_back(candidate);
            }
        }
        std::sort(touched.begin(), touched.end(), [&](int lhs, int rhs) {
            const int lhs_overlap = overlap[current.selected_positions[lhs]];
            const int rhs_overlap = overlap[current.selected_positions[rhs]];
            if (lhs_overlap != rhs_overlap) return lhs_overlap > rhs_overlap;
            return lhs < rhs;
        });

        for (int candidate : touched) {
            if (static_cast<int>(result.size()) == ruin_size) break;
            result.push_back(candidate);
        }
        if (static_cast<int>(result.size()) < ruin_size) {
            for (int index = 0; index < static_cast<int>(removable.size()); ++index) {
                const int other = index + random.next_int(static_cast<int>(removable.size()) - index);
                std::swap(removable[index], removable[other]);
            }
            for (int candidate : removable) {
                if (static_cast<int>(result.size()) == ruin_size) break;
                if (std::find(result.begin(), result.end(), candidate) == result.end()) {
                    result.push_back(candidate);
                }
            }
        }
    }

    static coverage_solution make_solution(const state& current, long long iterations,
                                           bool feasible) {
        coverage_solution result;
        result.selected_candidates = current.selected_candidates;
        std::sort(result.selected_candidates.begin(), result.selected_candidates.end());
        result.covered_weight = current.covered_weight;
        result.total_cost = current.total_cost;
        result.covered_target_count = current.covered_target_count;
        result.uncovered_target_count = current.problem->target_count - current.covered_target_count;
        result.iterations = iterations;
        result.feasible = feasible;
        return result;
    }

    static state state_from_initial(const problem_view& problem,
                                    std::span<const int> initial_candidates) {
        state result(problem);
        add_forced_candidates(result);
        std::vector<unsigned char> seen(problem.candidate_count, 0);
        for (int candidate : initial_candidates) {
            problem.check_candidate(candidate);
            if (problem.forbidden[candidate]) {
                throw std::invalid_argument("初期解に選択禁止候補が含まれている");
            }
            if (seen[candidate]) throw std::invalid_argument("初期解に同じ候補が複数含まれている");
            seen[candidate] = 1;
            if (!result.selected[candidate]) result.add(candidate);
        }
        return result;
    }

    static coverage_solution solve_budget_impl(
        const problem_view& problem, long long budget,
        const coverage_solver_options& options,
        const std::span<const int>* initial_candidates) {
        if (budget < 0) throw std::invalid_argument("上限は非負でなければならない");
        time_keeper timer(options);
        fast_random random(options.seed);
        greedy_workspace workspace(problem.candidate_count, problem.target_count, timer);

        state base(problem);
        add_forced_candidates(base);
        if (base.total_cost > budget) return make_solution(base, 0, false);

        // 費用対利得を優先する解と、利得だけを優先する解を比較する
        state best = base;
        greedy_fill_maximum(best, budget, resource_mode::candidate_cost,
                            greedy_metric::gain_per_resource, options.seed, 0,
                            nullptr, workspace, false);
        remove_redundant_maximum(best);

        state gain_first = base;
        greedy_fill_maximum(gain_first, budget, resource_mode::candidate_cost,
                            greedy_metric::gain,
                            options.seed ^ 0xa0761d6478bd642fULL, 0,
                            nullptr, workspace, false);
        remove_redundant_maximum(gain_first);
        if (better_maximum(gain_first, best, resource_mode::candidate_cost)) {
            best = std::move(gain_first);
        }

        if (initial_candidates != nullptr) {
            state initial = state_from_initial(problem, *initial_candidates);
            if (initial.total_cost > budget) {
                throw std::invalid_argument("初期解が上限を超えている");
            }
            greedy_fill_maximum(initial, budget, resource_mode::candidate_cost,
                                greedy_metric::gain_per_resource,
                                options.seed ^ 0xe7037ed1a0b428dbULL, 0,
                                nullptr, workspace, false);
            remove_redundant_maximum(initial);
            if (better_maximum(initial, best, resource_mode::candidate_cost)) {
                best = std::move(initial);
            }
        }

        // 固定候補から追加可能な正利得候補がなければ、予算内の改善余地はない
        if (best.selected_candidates.size() == base.selected_candidates.size()) {
            return make_solution(best, 0, true);
        }

        const auto attempt_budget_one_swap = [&](state& current) {
            int add_candidate = -1;
            double best_priority = -1;
            const int candidate_count = current.problem->candidate_count;
            if (candidate_count == 0) return;
            constexpr int sample_count = 24;
            for (int attempt = 0; attempt < sample_count; ++attempt) {
                const int candidate = random.next_int(candidate_count);
                if (!current.problem->active[candidate] ||
                    current.selected[candidate]) {
                    continue;
                }
                const long long gain = current.gain(candidate);
                if (gain <= 0) continue;
                const long long cost = current.problem->candidate_costs[candidate];
                if (cost > budget) continue;
                const double priority = greedy_priority(
                    gain, cost, greedy_metric::gain_per_resource, 1000);
                if (priority > best_priority) {
                    best_priority = priority;
                    add_candidate = candidate;
                }
            }
            if (add_candidate < 0) return;

            // 空き資源へ正利得候補をそのまま追加できる場合はswapせず追加する
            if (current.problem->candidate_costs[add_candidate] <= budget - current.total_cost) {
                current.add(add_candidate);
                return;
            }

            const int selected_count = static_cast<int>(current.selected_candidates.size());
            constexpr int selected_sample_count = 128;
            const int attempts = std::min(selected_count, selected_sample_count);
            int best_remove = -1;
            const long long add_gain = current.gain(add_candidate);
            long long best_delta = std::numeric_limits<long long>::min();
            long long best_resource_change = std::numeric_limits<long long>::max();

            // 追加候補を固定し、全選択候補またはsamplingした候補との正確なswap差分を比較する
            for (int attempt = 0; attempt < attempts; ++attempt) {
                const int remove_candidate =
                    selected_count <= selected_sample_count
                        ? current.selected_candidates[attempt]
                        : current.selected_candidates[random.next_int(selected_count)];
                if (current.problem->forced[remove_candidate]) continue;
                const long long resource_change =
                    current.problem->candidate_costs[add_candidate] -
                    current.problem->candidate_costs[remove_candidate];
                if (resource_change > budget - current.total_cost) continue;
                const long long delta = [&]() {
                    long long result = add_gain - current.loss(remove_candidate);
                    int lhs = current.problem->candidate_offsets[remove_candidate];
                    int rhs = current.problem->candidate_offsets[add_candidate];
                    const int lhs_end = current.problem->candidate_offsets[remove_candidate + 1];
                    const int rhs_end = current.problem->candidate_offsets[add_candidate + 1];

                    // 削除候補だけが覆う共通対象は、追加候補によって直ちに再被覆されるため損失を戻す
                    while (lhs < lhs_end && rhs < rhs_end) {
                        const int lhs_target = current.problem->candidate_targets[lhs];
                        const int rhs_target = current.problem->candidate_targets[rhs];
                        if (lhs_target < rhs_target) {
                            ++lhs;
                        } else if (rhs_target < lhs_target) {
                            ++rhs;
                        } else {
                            if (current.cover_count[lhs_target] == 1) result += current.problem->target_weights[lhs_target];
                            ++lhs;
                            ++rhs;
                        }
                    }
                    return result;
                }();
                if (delta > best_delta ||
                    (delta == best_delta && resource_change < best_resource_change)) {
                    best_delta = delta;
                    best_resource_change = resource_change;
                    best_remove = remove_candidate;
                }
            }

            if (best_remove < 0 || best_delta < 0 ||
                (best_delta == 0 && best_resource_change >= 0)) {
                return;
            }
            current.remove(best_remove);
            current.add(add_candidate);
        };

        constexpr int random_restarts = 1000000;
        constexpr int swap_attempts = 8;
        for (int restart = 0; restart < random_restarts && !timer.expired(true); ++restart) {
            state candidate = base;
            const greedy_metric metric = (restart & 1) == 0
                                             ? greedy_metric::gain_per_resource
                                             : greedy_metric::gain;
            greedy_fill_maximum(candidate, budget, resource_mode::candidate_cost,
                                metric, random.next_u64(), 180,
                                nullptr, workspace, false);
            remove_redundant_maximum(candidate);

            // 完了した再始動を先に数え、残りの反復枠だけでswapを試す
            ++timer.iterations;
            for (int attempt = 0; attempt < swap_attempts && !timer.expired(); ++attempt) {
                attempt_budget_one_swap(candidate);
                ++timer.iterations;
            }
            if (better_maximum(candidate, best, resource_mode::candidate_cost)) {
                best = std::move(candidate);
            }
        }
        return make_solution(best, timer.iterations, true);
    }

    static coverage_solution solve_set_cover_impl(
        const problem_view& problem, const coverage_solver_options& options,
        const std::span<const int>* initial_candidates) {
        time_keeper timer(options);
        fast_random random(options.seed);
        greedy_workspace workspace(problem.candidate_count, problem.target_count, timer);

        // 各対象が少なくとも一つの利用可能候補から被覆できるか先に確認する
        for (int target = 0; target < problem.target_count; ++target) {
            bool coverable = false;
            for (int index = problem.target_offsets[target];
                 index < problem.target_offsets[target + 1]; ++index) {
                const int candidate = problem.target_candidates[index];
                coverable |= problem.active[candidate];
            }
            if (!coverable) {
                state infeasible(problem);
                add_forced_candidates(infeasible);
                return make_solution(infeasible, 0, false);
            }
        }

        state base(problem);
        add_forced_candidates(base);
        state best = base;
        if (greedy_fill_set_cover(best, options.seed, 0, nullptr,
                                  nullptr, workspace, false, nullptr)) {
            remove_redundant_set_cover(best);
        }

        const auto better_set_cover = [](const state& lhs, const state& rhs) {
            if (lhs.covered_target_count != lhs.problem->target_count) return false;
            if (rhs.covered_target_count != rhs.problem->target_count) return true;
            if (lhs.total_cost != rhs.total_cost) return lhs.total_cost < rhs.total_cost;
            return lhs.selected_candidates.size() < rhs.selected_candidates.size();
        };

        // improveでは不完全な初期解も許し、未被覆対象を貪欲repairして比較する
        if (initial_candidates != nullptr) {
            state initial = state_from_initial(problem, *initial_candidates);
            if (greedy_fill_set_cover(initial, options.seed ^ 0xe7037ed1a0b428dbULL,
                                      0, nullptr, nullptr, workspace,
                                      true, nullptr)) {
                remove_redundant_set_cover(initial);
                if (better_set_cover(initial, best)) best = std::move(initial);
            }
        }

        // 固定候補だけで全被覆できるなら、費用も選択数もこれ以上は減らせない
        if (best.covered_target_count == problem.target_count &&
            best.selected_candidates.size() == base.selected_candidates.size()) {
            return make_solution(best, 0, true);
        }

        constexpr int random_restarts = 4;
        for (int restart = 0; restart < random_restarts && !timer.expired(true); ++restart) {
            state candidate = base;
            if (greedy_fill_set_cover(candidate, random.next_u64(), 250,
                                      nullptr, nullptr, workspace,
                                      false, nullptr)) {
                remove_redundant_set_cover(candidate);
                if (better_set_cover(candidate, best)) best = std::move(candidate);
            }
            ++timer.iterations;
        }

        if (best.covered_target_count != problem.target_count) {
            return make_solution(best, timer.iterations, false);
        }
        const auto apply_set_cover_lns = [&](state& current, int ruin_kind) {
            const int ruin_size = choose_ruin_size(current, random);
            if (ruin_size == 0) return;

            std::vector<int>& removed = workspace.ruin_result;
            removed.clear();
            if (ruin_kind == 0) {
                choose_random_ruin(current, ruin_size, random, workspace, removed);
            } else if (ruin_kind == 1) {
                for (int index = 0; index < ruin_size; ++index) {
                    const int candidate = [&]() {
                        constexpr int sample_count = 48;

                        const int selected_count = static_cast<int>(current.selected_candidates.size());
                        if (selected_count == 0) return -1;
                        const int attempts = selected_count <= sample_count ? selected_count : sample_count;
                        int best_candidate = -1;
                        double best_priority = std::numeric_limits<double>::infinity();
                        for (int attempt = 0; attempt < attempts; ++attempt) {
                            const int sampled = selected_count <= sample_count
                                                      ? current.selected_candidates[attempt]
                                                      : current.selected_candidates[random.next_int(selected_count)];
                            if (current.problem->forced[sampled] ||
                                current.problem->candidate_costs[sampled] == 0) {
                                continue;
                            }
                            const double priority =
                                static_cast<double>(current.unique_target_count(sampled)) /
                                static_cast<double>(current.problem->candidate_costs[sampled]);
                            if (priority < best_priority) {
                                best_priority = priority;
                                best_candidate = sampled;
                            }
                        }
                        return best_candidate;
                    }();
                    if (candidate < 0) break;
                    current.remove(candidate);
                    removed.push_back(candidate);
                }
                for (auto it = removed.rbegin(); it != removed.rend(); ++it) current.add(*it);
            } else {
                choose_related_ruin(current, ruin_size, random, workspace, removed);
            }
            if (removed.empty()) return;

            const long long old_cost = current.total_cost;
            const int old_selected_count = static_cast<int>(current.selected_candidates.size());
            std::vector<int>& original_selected = workspace.original_selected;
            original_selected.assign(current.selected_candidates.begin(),
                                     current.selected_candidates.end());
            if (++workspace.stamp == std::numeric_limits<int>::max()) {
                std::fill(workspace.original_selected_stamp.begin(),
                          workspace.original_selected_stamp.end(), 0);
                workspace.stamp = 1;
            }
            for (int candidate : original_selected) {
                workspace.original_selected_stamp[candidate] = workspace.stamp;
            }
            for (int candidate : removed) current.remove(candidate);

            workspace.uncovered_targets.clear();
            for (int candidate : removed) {
                for (int index = current.problem->candidate_offsets[candidate];
                     index < current.problem->candidate_offsets[candidate + 1]; ++index) {
                    const int target = current.problem->candidate_targets[index];
                    if (current.cover_count[target] == 0) {
                        workspace.uncovered_targets.push_back(target);
                    }
                }
            }
            std::sort(workspace.uncovered_targets.begin(), workspace.uncovered_targets.end());
            workspace.uncovered_targets.erase(
                std::unique(workspace.uncovered_targets.begin(), workspace.uncovered_targets.end()),
                workspace.uncovered_targets.end());

            // 直前に除去した候補を一度禁止し、同じ解への即時復帰を避ける
            for (int candidate : removed) workspace.blocked_candidates[candidate] = 1;

            std::vector<int>& added = workspace.added_candidates;
            added.clear();
            bool feasible = greedy_fill_set_cover(
                current, random.next_u64(), 250, &workspace.blocked_candidates,
                &added, workspace, true, &workspace.uncovered_targets);
            if (!feasible) {
                greedy_fill_set_cover(current, random.next_u64(), 0, nullptr,
                                      &added, workspace, true,
                                      &workspace.uncovered_targets);
                feasible = current.covered_target_count == current.problem->target_count;
            }
            for (int candidate : removed) workspace.blocked_candidates[candidate] = 0;
            if (feasible) remove_redundant_set_cover(current);

            if (feasible &&
                (current.total_cost < old_cost ||
                 (current.total_cost == old_cost &&
                  static_cast<int>(current.selected_candidates.size()) < old_selected_count))) {
                return;
            }

            // repairで新規追加した候補を消し、元解にあった候補だけを差分で戻す
            for (auto it = added.rbegin(); it != added.rend(); ++it) {
                const int candidate = *it;
                if (current.selected[candidate] &&
                    workspace.original_selected_stamp[candidate] != workspace.stamp) {
                    current.remove(candidate);
                }
            }
            for (int candidate : original_selected) {
                if (!current.selected[candidate]) current.add(candidate);
            }
        };

        int round = 0;
        while (!timer.expired(true)) {
            for (int offset = 0; offset < 3 && !timer.expired(); ++offset) {
                const int kind = (round + offset) % 3;
                apply_set_cover_lns(best, kind);
                ++timer.iterations;
            }
            round = (round + 1) % 3;
        }
        return make_solution(best, timer.iterations,
                             best.covered_target_count == problem.target_count);
    }

public:
    // 入力を正規化して支配除去と両方向CSR構築を行う O(L log(L+1) + C log(C+1) + C L + T)
    explicit coverage_solver(const coverage_problem& problem) : problem_(problem) {}

    // 最大K個の候補で被覆重み和を最大化する O(T + C + L + 探索処理量)
    coverage_solution solve_maximum_coverage(
        int max_selected, const coverage_solver_options& options = {},
        const coverage_subproblem& subproblem = {}) const {
        return cardinality({0, max_selected}, nullptr, options, subproblem);
    }

    // 選択数の下限・上限の下で最大被覆を解く O(T + C log C + L + 探索処理量)
    coverage_solution solve_maximum_coverage(
        coverage_cardinality_bounds bounds, const coverage_solver_options& options = {},
        const coverage_subproblem& subproblem = {}) const {
        return cardinality(bounds, nullptr, options, subproblem);
    }

    // 初期解から最大K個の最大被覆解を改善する O(T + C + L + 探索処理量)
    coverage_solution improve_maximum_coverage(
        int max_selected, std::span<const int> initial_candidates,
        const coverage_solver_options& options = {},
        const coverage_subproblem& subproblem = {}) const {
        return cardinality({0, max_selected}, &initial_candidates, options, subproblem);
    }

    // リスト形式の初期解から最大被覆を改善する O(T + C + L + 探索処理量)
    coverage_solution improve_maximum_coverage(
        int max_selected, std::initializer_list<int> initial_candidates,
        const coverage_solver_options& options = {},
        const coverage_subproblem& subproblem = {}) const {
        return improve_maximum_coverage(max_selected, as_span(initial_candidates), options, subproblem);
    }

    // 初期解から選択数上下限付き最大被覆を改善する O(T + C log C + L + 探索処理量)
    coverage_solution improve_maximum_coverage(
        coverage_cardinality_bounds bounds, std::span<const int> initial_candidates,
        const coverage_solver_options& options = {},
        const coverage_subproblem& subproblem = {}) const {
        return cardinality(bounds, &initial_candidates, options, subproblem);
    }

    // リスト形式の初期解から選択数上下限付き最大被覆を改善する O(T + C log C + L + 探索処理量)
    coverage_solution improve_maximum_coverage(
        coverage_cardinality_bounds bounds, std::initializer_list<int> initial_candidates,
        const coverage_solver_options& options = {},
        const coverage_subproblem& subproblem = {}) const {
        return improve_maximum_coverage(bounds, as_span(initial_candidates), options, subproblem);
    }

    // 総費用budget以下で被覆重み和を最大化する O(T + C + L + 探索処理量)
    coverage_solution solve_budgeted_maximum_coverage(
        long long budget, const coverage_solver_options& options = {},
        const coverage_subproblem& subproblem = {}) const {
        const auto problem = prepare(subproblem);
        return solve_budget_impl(problem, budget, options, nullptr);
    }

    // 初期解から予算付き最大被覆解を改善する O(T + C + L + 探索処理量)
    coverage_solution improve_budgeted_maximum_coverage(
        long long budget, std::span<const int> initial_candidates,
        const coverage_solver_options& options = {},
        const coverage_subproblem& subproblem = {}) const {
        const auto problem = prepare(subproblem);
        return solve_budget_impl(problem, budget, options, &initial_candidates);
    }

    // リスト形式の初期解から予算付き最大被覆を改善する O(T + C + L + 探索処理量)
    coverage_solution improve_budgeted_maximum_coverage(
        long long budget, std::initializer_list<int> initial_candidates,
        const coverage_solver_options& options = {},
        const coverage_subproblem& subproblem = {}) const {
        return improve_budgeted_maximum_coverage(budget, as_span(initial_candidates), options, subproblem);
    }

    // 全対象を被覆する候補集合の費用を小さくする O(T + C + L + 探索処理量)
    coverage_solution solve_minimum_set_cover(
        const coverage_solver_options& options = {},
        const coverage_subproblem& subproblem = {}) const {
        const auto problem = prepare(subproblem);
        return solve_set_cover_impl(problem, options, nullptr);
    }

    // 未完成の初期解も許して最小費用集合被覆を改善する O(T + C + L + 探索処理量)
    coverage_solution improve_minimum_set_cover(
        std::span<const int> initial_candidates,
        const coverage_solver_options& options = {},
        const coverage_subproblem& subproblem = {}) const {
        const auto problem = prepare(subproblem);
        return solve_set_cover_impl(problem, options, &initial_candidates);
    }

    // リスト形式の初期解から最小費用集合被覆を改善する O(T + C + L + 探索処理量)
    coverage_solution improve_minimum_set_cover(
        std::initializer_list<int> initial_candidates,
        const coverage_solver_options& options = {},
        const coverage_subproblem& subproblem = {}) const {
        return improve_minimum_set_cover(as_span(initial_candidates), options, subproblem);
    }

    // 固定候補を補って選択集合を再評価する O(T + C + L)
    coverage_evaluation evaluate(
        std::span<const int> selected_candidates,
        const coverage_subproblem& subproblem = {}) const {
        const auto problem = prepare(subproblem);
        const auto current = state_from_initial(problem, selected_candidates);
        return {current.covered_weight, current.total_cost, current.covered_target_count,
                problem.target_count - current.covered_target_count,
                static_cast<int>(current.selected_candidates.size())};
    }

    // リスト形式の選択集合を再評価する O(T + C + L)
    coverage_evaluation evaluate(
        std::initializer_list<int> selected_candidates,
        const coverage_subproblem& subproblem = {}) const {
        return evaluate(as_span(selected_candidates), subproblem);
    }

    // 入力時の対象数を返す O(1)
    int target_count() const { return problem_.target_count; }

    // 前処理で除外した候補も含めて入力時の候補数を返す O(1)
    int candidate_count() const { return problem_.candidate_count; }

private:
#if __INCLUDE_LEVEL__ == 0
    struct selftest;
    friend int main();
#endif
    prepared_problem problem_;
    // const APIでも作業領域は再利用する。同じインスタンスへの同時呼び出しは行わない
    mutable std::vector<unsigned char> work_forced_, work_forbidden_, work_active_;
    mutable std::vector<long long> work_sums_, work_rarity_;

    static std::span<const int> as_span(std::initializer_list<int> values) {
        return {values.begin(), values.size()};
    }

    problem_view prepare(const coverage_subproblem& sub) const {
        auto view = problem_view{
            problem_.target_count, problem_.candidate_count, problem_.target_weights, problem_.candidate_costs,
            problem_.candidate_weight_sums, problem_.candidate_offsets, problem_.candidate_targets,
            problem_.target_offsets, problem_.target_candidates, problem_.target_rarity,
            problem_.forced, problem_.forbidden, problem_.active};
        const bool changed_weights = !sub.target_weights_override.empty();
        const bool changed_costs = !sub.candidate_costs_override.empty();
        if (sub.fixed_candidates.empty() && sub.forbidden_candidates.empty() &&
            !sub.restrict_to_candidate_pool && !changed_weights && !changed_costs) return view;

        // 差し替え値は所有せず、呼び出しが終わるまで参照する
        const auto validate_values = [](std::span<const long long> values, int expected) {
            if (values.size() != static_cast<std::size_t>(expected)) {
                throw std::invalid_argument("差し替え配列の要素数が異なる");
            }
            for (long long value : values) {
                if (value < 0) throw std::invalid_argument("差し替え値は非負でなければならない");
            }
        };
        if (changed_weights) {
            validate_values(sub.target_weights_override, view.target_count);
            view.target_weights = sub.target_weights_override;
            work_sums_.assign(view.candidate_count, 0);
            for (int c = 0; c < view.candidate_count; ++c) {
                for (int i = view.candidate_offsets[c]; i < view.candidate_offsets[c + 1]; ++i) {
                    work_sums_[c] += view.target_weights[view.candidate_targets[i]];
                }
            }
            view.candidate_weight_sums = work_sums_;
        }
        if (changed_costs) {
            validate_values(sub.candidate_costs_override, view.candidate_count);
            view.candidate_costs = sub.candidate_costs_override;
        }

        // 部分問題条件を検証し、恒久条件と合成する
        work_forced_.assign(view.forced.begin(), view.forced.end());
        work_forbidden_.assign(view.forbidden.begin(), view.forbidden.end());
        const auto mark_unique = [&](std::span<const int> values, std::vector<unsigned char>& mask) {
            std::vector<unsigned char> seen(view.candidate_count, 0);
            for (int c : values) {
                view.check_candidate(c);
                if (seen[c]) throw std::invalid_argument("部分問題の候補リストに重複がある");
                seen[c] = 1;
                mask[c] = 1;
            }
        };
        mark_unique(sub.fixed_candidates, work_forced_);
        mark_unique(sub.forbidden_candidates, work_forbidden_);
        std::vector<unsigned char> pool(view.candidate_count, 0);
        if (sub.restrict_to_candidate_pool) mark_unique(sub.candidate_pool, pool);

        // 禁止・pool・費用が変わると構築時の支配除去は安全でないため再び有効にする
        const bool restore = !sub.forbidden_candidates.empty() || sub.restrict_to_candidate_pool || changed_costs;
        work_active_.resize(view.candidate_count);
        for (int c = 0; c < view.candidate_count; ++c) {
            if (work_forced_[c] && work_forbidden_[c]) {
                throw std::invalid_argument("同じ候補を固定と禁止にできない");
            }
            if (sub.restrict_to_candidate_pool && !pool[c] && !work_forced_[c]) {
                work_forbidden_[c] = 1;
            }
            work_active_[c] = static_cast<unsigned char>(
                !work_forbidden_[c] && (work_forced_[c] || (restore ? view.candidate_offsets[c] < view.candidate_offsets[c + 1]
                                           : view.active[c] != 0)));
        }
        view.forced = work_forced_;
        view.forbidden = work_forbidden_;
        view.active = work_active_;

        // 当該呼び出しで利用可能な候補に合わせて希少度を計算する
        work_rarity_.resize(view.target_count);
        for (int t = 0; t < view.target_count; ++t) {
            int available = 0;
            for (int i = view.target_offsets[t]; i < view.target_offsets[t + 1]; ++i) {
                const int c = view.target_candidates[i];
                available += view.active[c];
            }
            work_rarity_[t] = available == 0 ? 0 : std::max(1LL, 1000000LL / available);
        }
        view.target_rarity = work_rarity_;
        return view;
    }

    coverage_solution cardinality(
        coverage_cardinality_bounds bounds, const std::span<const int>* initial_ids,
        const coverage_solver_options& options, const coverage_subproblem& sub) const {
        if (bounds.min_selected < 0 || bounds.max_selected < bounds.min_selected) {
            throw std::invalid_argument("選択数上下限が不正");
        }
        const auto problem = prepare(sub);
        // 下限が実現不能なら初期解の検証より先に返す
        if (bounds.min_selected > 0 &&
            std::count(problem.forbidden.begin(), problem.forbidden.end(), 0) < bounds.min_selected) {
            const time_keeper validate_options(options);
            state base(problem);
            add_forced_candidates(base);
            return make_solution(base, 0, false);
        }
        auto result = [&]() {
            const int max_selected = bounds.max_selected;

            time_keeper timer(options);
            fast_random random(options.seed);
            greedy_workspace workspace(problem.candidate_count, problem.target_count, timer);

            state base(problem);
            add_forced_candidates(base);
            if (static_cast<int>(base.selected_candidates.size()) > max_selected) {
                return make_solution(base, 0, false);
            }

            // deterministic greedyで初期解を作る
            state best = base;
            greedy_fill_maximum(best, max_selected, resource_mode::cardinality,
                                greedy_metric::gain, options.seed, 0,
                                nullptr, workspace, false);
            remove_redundant_maximum(best);

            // improveでは与えた初期解を実行可能性検査後に比較対象へ加える
            if (initial_ids != nullptr) {
                state initial = state_from_initial(problem, *initial_ids);
                if (static_cast<int>(initial.selected_candidates.size()) > max_selected) {
                    throw std::invalid_argument("初期解が上限を超えている");
                }
                greedy_fill_maximum(initial, max_selected, resource_mode::cardinality,
                                    greedy_metric::gain,
                                    options.seed ^ 0xe7037ed1a0b428dbULL, 0, nullptr,
                                    workspace, true);
                remove_redundant_maximum(initial);
                if (better_maximum(initial, best, resource_mode::cardinality)) {
                    best = std::move(initial);
                }
            }

            // 正利得候補を追加できず固定候補だけなら、これ以上の重み・選択数改善はない
            // improveの入力検証後に判定し、下限不足の補充は公開メソッド側で行う
            if (best.selected_candidates.size() == base.selected_candidates.size()) {
                return make_solution(best, 0, true);
            }

            // 少数のランダム化貪欲初期解を比較し、短時間でも初期解の多様性を確保する
            constexpr int random_restarts = 4;
            for (int restart = 0; restart < random_restarts && !timer.expired(true); ++restart) {
                state candidate = base;
                const std::uint64_t construction_seed = random.next_u64();
                greedy_fill_maximum(candidate, max_selected, resource_mode::cardinality,
                                    greedy_metric::gain, construction_seed, 180,
                                    nullptr, workspace, false);
                remove_redundant_maximum(candidate);
                if (better_maximum(candidate, best, resource_mode::cardinality)) {
                    best = std::move(candidate);
                }
                ++timer.iterations;
            }

            const auto apply_cardinality_lns = [&](state& current, int ruin_kind) {
                const int ruin_size = choose_ruin_size(current, random);
                if (ruin_size == 0) return;

                std::vector<int>& removed = workspace.ruin_result;
                removed.clear();
                if (ruin_kind == 0) {
                    choose_random_ruin(current, ruin_size, random, workspace, removed);
                } else {
                    choose_related_ruin(current, ruin_size, random, workspace, removed);
                }
                if (removed.empty()) return;

                const long long old_weight = current.covered_weight;
                const int old_selected_count = static_cast<int>(current.selected_candidates.size());
                for (int candidate : removed) current.remove(candidate);

                // 未被覆対象から候補を疎に列挙し、ランダム化貪欲法で再構築する
                std::vector<int>& added = workspace.added_candidates;
                added.clear();
                greedy_fill_maximum(current, max_selected, resource_mode::cardinality,
                                    greedy_metric::gain, random.next_u64(), 180,
                                    &added, workspace, true);

                const bool improved = current.covered_weight > old_weight ||
                                      (current.covered_weight == old_weight &&
                                       static_cast<int>(current.selected_candidates.size()) <=
                                           old_selected_count);
                if (improved) return;

                for (auto it = added.rbegin(); it != added.rend(); ++it) current.remove(*it);
                for (auto it = removed.rbegin(); it != removed.rend(); ++it) current.add(*it);
            };

            // random ruinとrelated ruinを交互に試す
            int round = 0;
            while (!timer.expired(true)) {
                for (int offset = 0; offset < 2 && !timer.expired(); ++offset) {
                    const int kind = (round + offset) % 2;
                    apply_cardinality_lns(best, kind);
                    ++timer.iterations;
                }
                round = (round + 1) % 2;
            }
            return make_solution(best, timer.iterations, true);
        }();
        if (!result.feasible || static_cast<int>(result.selected_candidates.size()) >= bounds.min_selected) return result;

        // 被覆重みを保ったまま安い候補から補充する。空集合・支配候補も補充対象になる
        state current = state_from_initial(problem, result.selected_candidates);
        std::vector<int> fillers;
        for (int c = 0; c < problem.candidate_count; ++c) {
            if (!problem.forbidden[c] && !current.selected[c]) fillers.push_back(c);
        }
        std::sort(fillers.begin(), fillers.end(), [&](int a, int b) {
            if (problem.candidate_costs[a] != problem.candidate_costs[b]) {
                return problem.candidate_costs[a] < problem.candidate_costs[b];
            }
            return a < b;
        });
        for (int c : fillers) {
            if (static_cast<int>(current.selected_candidates.size()) >= bounds.min_selected) break;
            current.add(c);
        }
        return make_solution(current, result.iterations, true);
    }
};


#if __INCLUDE_LEVEL__ == 0

#include <iostream>

struct coverage_solver::selftest {
    static void require(bool condition, const std::string& label) {
        if (!condition) throw std::runtime_error("test failed: " + label);
    }

    static long long evaluate_weight(const coverage_problem& problem,
                              const std::vector<int>& selected) {
        std::vector<unsigned char> covered(problem.target_weights.size(), 0);
        for (int candidate : selected) {
            for (int target : problem.covered_targets[candidate]) covered[target] = 1;
        }
        long long result = 0;
        for (int target = 0; target < static_cast<int>(problem.target_weights.size()); ++target) {
            if (covered[target]) result += problem.target_weights[target];
        }
        return result;
    }

    static long long evaluate_cost(const coverage_problem& problem,
                            const std::vector<int>& selected) {
        long long result = 0;
        for (int candidate : selected) result += problem.candidate_costs[candidate];
        return result;
    }

    static bool covers_all(const coverage_problem& problem, const std::vector<int>& selected) {
        std::vector<unsigned char> covered(problem.target_weights.size(), 0);
        for (int candidate : selected) {
            for (int target : problem.covered_targets[candidate]) covered[target] = 1;
        }
        return std::all_of(covered.begin(), covered.end(), [](unsigned char value) {
            return value != 0;
        });
    }

    static coverage_solver_options deterministic_options(std::uint64_t seed = 1) {
        coverage_solver_options options;
        options.time_limit_ms = -1;
        options.iteration_limit = 0;
        options.seed = seed;
        return options;
    }

    static void test_basic_cases() {
        coverage_problem problem;
        problem.target_weights = {5, 7, 11, 13};
        problem.candidate_costs = {2, 3, 4, 1};
        problem.covered_targets = {{0, 1}, {1, 2}, {2, 3}, {0}};
        coverage_solver solver(problem);

        const coverage_solution cardinality =
            solver.solve_maximum_coverage(2, deterministic_options());
        require(cardinality.feasible, "basic cardinality feasible");
        require(cardinality.covered_weight == 36, "basic cardinality optimum");
        require(static_cast<int>(cardinality.selected_candidates.size()) <= 2,
                "basic cardinality limit");

        const coverage_solution budgeted =
            solver.solve_budgeted_maximum_coverage(5, deterministic_options());
        require(budgeted.feasible, "basic budget feasible");
        require(budgeted.total_cost <= 5, "basic budget limit");
        require(budgeted.covered_weight == evaluate_weight(problem, budgeted.selected_candidates),
                "basic budget score");

        const coverage_solution set_cover =
            solver.solve_minimum_set_cover(deterministic_options());
        require(set_cover.feasible, "basic set cover feasible");
        require(covers_all(problem, set_cover.selected_candidates), "basic set cover coverage");
        require(set_cover.total_cost == evaluate_cost(problem, set_cover.selected_candidates),
                "basic set cover cost");
    }

    static void test_forced_forbidden_and_edges() {
        coverage_problem problem;
        problem.target_weights = {0, 2, 3};
        problem.candidate_costs = {0, 2, 3, 1};
        problem.covered_targets = {{0, 0}, {1}, {2}, {1, 2}};
        problem.forced_candidates = {0};
        problem.forbidden_candidates = {3};
        coverage_solver solver(problem);

        const coverage_solution maximum =
            solver.solve_budgeted_maximum_coverage(5, deterministic_options());
        require(maximum.feasible, "forced budget feasible");
        require(std::binary_search(maximum.selected_candidates.begin(),
                                   maximum.selected_candidates.end(), 0),
                "forced selected");
        require(!std::binary_search(maximum.selected_candidates.begin(),
                                    maximum.selected_candidates.end(), 3),
                "forbidden absent");

        const coverage_solution infeasible_budget =
            solver.solve_maximum_coverage(0, deterministic_options());
        require(!infeasible_budget.feasible, "forced cardinality infeasible");

        coverage_problem impossible;
        impossible.target_weights = {1, 1};
        impossible.candidate_costs = {1};
        impossible.covered_targets = {{0}};
        coverage_solver impossible_solver(impossible);
        require(!impossible_solver.solve_minimum_set_cover(deterministic_options()).feasible,
                "uncoverable set cover");
    }

    struct exact_answers {
        long long cardinality = -1;
        long long budgeted = -1;
        long long set_cover = std::numeric_limits<long long>::max();
    };

    static exact_answers brute_force(const coverage_problem& problem, int max_selected,
                              long long budget) {
        const int candidate_count = static_cast<int>(problem.candidate_costs.size());
        require(candidate_count <= 24, "brute force candidate limit");
        exact_answers answer;
        const std::uint64_t subset_count = 1ULL << candidate_count;
        for (std::uint64_t mask = 0; mask < subset_count; ++mask) {
            bool allowed = true;
            for (int candidate : problem.forced_candidates) {
                allowed &= ((mask >> candidate) & 1ULL) != 0;
            }
            for (int candidate : problem.forbidden_candidates) {
                allowed &= ((mask >> candidate) & 1ULL) == 0;
            }
            if (!allowed) continue;

            std::vector<int> selected;
            long long cost = 0;
            for (int candidate = 0; candidate < candidate_count; ++candidate) {
                if (((mask >> candidate) & 1ULL) != 0) {
                    selected.push_back(candidate);
                    cost += problem.candidate_costs[candidate];
                }
            }
            const long long weight = evaluate_weight(problem, selected);
            if (static_cast<int>(selected.size()) <= max_selected) {
                answer.cardinality = std::max(answer.cardinality, weight);
            }
            if (cost <= budget) answer.budgeted = std::max(answer.budgeted, weight);
            if (covers_all(problem, selected)) answer.set_cover = std::min(answer.set_cover, cost);
        }
        return answer;
    }

    static void test_random_against_exact() {
        fast_random random(123456789);
        for (int test = 0; test < 180; ++test) {
            const int target_count = 1 + random.next_int(12);
            const int candidate_count = 1 + random.next_int(14);
            coverage_problem problem;
            problem.target_weights.resize(target_count);
            problem.candidate_costs.resize(candidate_count);
            problem.covered_targets.resize(candidate_count);
            for (long long& weight : problem.target_weights) weight = random.next_int(10);
            for (int candidate = 0; candidate < candidate_count; ++candidate) {
                problem.candidate_costs[candidate] = random.next_int(8);
                for (int target = 0; target < target_count; ++target) {
                    if (random.next_int(100) < 30) {
                        problem.covered_targets[candidate].push_back(target);
                    }
                }
            }
            const int max_selected = random.next_int(candidate_count + 1);
            const long long budget = random.next_int(20);
            const exact_answers exact = brute_force(problem, max_selected, budget);

            // 重複・包含支配で無効化した候補を禁止しても3目的の厳密最適値が変わらないことを確認する
            const prepared_problem prepared(problem);
            coverage_problem reduced = problem;
            for (int candidate = 0; candidate < candidate_count; ++candidate) {
                if (!prepared.active[candidate]) reduced.forbidden_candidates.push_back(candidate);
            }
            const exact_answers reduced_exact = brute_force(reduced, max_selected, budget);
            require(reduced_exact.cardinality == exact.cardinality,
                    "dominance preserves cardinality optimum");
            require(reduced_exact.budgeted == exact.budgeted,
                    "dominance preserves budget optimum");
            require(reduced_exact.set_cover == exact.set_cover,
                    "dominance preserves set cover optimum");

            coverage_solver solver(problem);

            const coverage_solution cardinality =
                solver.solve_maximum_coverage(max_selected, deterministic_options(test + 1));
            require(cardinality.feasible, "random cardinality feasible");
            require(cardinality.covered_weight <= exact.cardinality,
                    "random cardinality not above optimum");
            require(cardinality.covered_weight ==
                        evaluate_weight(problem, cardinality.selected_candidates),
                    "random cardinality score consistency");

            const coverage_solution budgeted =
                solver.solve_budgeted_maximum_coverage(budget, deterministic_options(test + 1000));
            require(budgeted.feasible, "random budget feasible");
            require(budgeted.total_cost <= budget, "random budget limit");
            require(budgeted.covered_weight <= exact.budgeted,
                    "random budget not above optimum");
            require(budgeted.covered_weight ==
                        evaluate_weight(problem, budgeted.selected_candidates),
                    "random budget score consistency");

            const coverage_solution set_cover =
                solver.solve_minimum_set_cover(deterministic_options(test + 2000));
            if (exact.set_cover == std::numeric_limits<long long>::max()) {
                require(!set_cover.feasible, "random impossible set cover");
            } else {
                require(set_cover.feasible, "random set cover feasible");
                require(set_cover.total_cost >= exact.set_cover,
                        "random set cover not below optimum");
                require(covers_all(problem, set_cover.selected_candidates),
                        "random set cover coverage");
            }
        }
    }

    static void test_improve_and_validation() {
        coverage_problem problem;
        problem.target_weights = {1, 2, 3, 4, 5};
        problem.candidate_costs = {2, 2, 2, 3};
        problem.covered_targets = {{0, 1}, {2}, {3, 4}, {0, 2, 4}};
        coverage_solver solver(problem);

        const std::vector<int> initial = {0, 1};
        const coverage_solution improved = solver.improve_maximum_coverage(
            2, initial, deterministic_options());
        require(improved.covered_weight >= evaluate_weight(problem, initial),
                "improve maximum monotonic");

        const coverage_solution repaired = solver.improve_minimum_set_cover(
            {0}, deterministic_options());
        require(repaired.feasible && covers_all(problem, repaired.selected_candidates),
                "improve set cover repairs incomplete input");

        bool threw = false;
        try {
            (void)solver.improve_budgeted_maximum_coverage(
                1, {0}, deterministic_options());
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        require(threw, "invalid initial budget throws");
    }

    static void validate_solution_structure(const coverage_problem& problem,
                                     const coverage_solution& solution) {
        require(std::is_sorted(solution.selected_candidates.begin(), solution.selected_candidates.end()),
                "solution sorted candidate order");
        std::vector<unsigned char> seen(problem.candidate_costs.size(), 0);
        for (int candidate : solution.selected_candidates) {
            require(candidate >= 0 &&
                        candidate < static_cast<int>(problem.candidate_costs.size()),
                    "solution candidate range");
            require(!seen[candidate], "solution candidate uniqueness");
            seen[candidate] = 1;
        }
        for (int candidate : problem.forced_candidates) {
            require(seen[candidate], "solution contains forced candidate");
        }
        for (int candidate : problem.forbidden_candidates) {
            require(!seen[candidate], "solution excludes forbidden candidate");
        }
        require(solution.covered_weight ==
                    evaluate_weight(problem, solution.selected_candidates),
                "solution weight consistency");
        require(solution.total_cost ==
                    evaluate_cost(problem, solution.selected_candidates),
                "solution cost consistency");
        std::vector<unsigned char> covered(problem.target_weights.size(), 0);
        for (int c : solution.selected_candidates) {
            for (int t : problem.covered_targets[c]) covered[t] = 1;
        }
        const int count = static_cast<int>(std::count(covered.begin(), covered.end(), 1));
        require(solution.covered_target_count == count &&
                solution.uncovered_target_count == static_cast<int>(covered.size()) - count,
                "independent covered and uncovered counts");
    }

    static void test_selected_search_consistency() {
        fast_random random(987654321);
        for (int test = 0; test < 30; ++test) {
            const int target_count = 12 + random.next_int(12);
            const int candidate_count = 20 + random.next_int(16);
            coverage_problem problem;
            problem.target_weights.resize(target_count);
            problem.candidate_costs.resize(candidate_count);
            problem.covered_targets.resize(candidate_count);
            for (long long& weight : problem.target_weights) weight = random.next_int(20);
            for (int candidate = 0; candidate < candidate_count; ++candidate) {
                problem.candidate_costs[candidate] = random.next_int(20);
                for (int target = 0; target < target_count; ++target) {
                    if (random.next_int(100) < 20) {
                        problem.covered_targets[candidate].push_back(target);
                    }
                }
            }
            for (int target = 0; target < target_count; ++target) {
                problem.covered_targets[target % candidate_count].push_back(target);
            }

            coverage_solver_options options;
            options.time_limit_ms = 2;
            options.seed = random.next_u64();
            coverage_solver solver(problem);

            const int max_selected = 1 + random.next_int(std::min(10, candidate_count));
            const coverage_solution cardinality =
                solver.solve_maximum_coverage(max_selected, options);
            require(cardinality.feasible, "timed cardinality feasible");
            require(static_cast<int>(cardinality.selected_candidates.size()) <= max_selected,
                    "timed cardinality limit");
            validate_solution_structure(problem, cardinality);

            const long long budget = 20 + random.next_int(80);
            const coverage_solution budgeted =
                solver.solve_budgeted_maximum_coverage(budget, options);
            require(budgeted.feasible, "timed budget feasible");
            require(budgeted.total_cost <= budget, "timed budget limit");
            validate_solution_structure(problem, budgeted);

            const coverage_solution set_cover = solver.solve_minimum_set_cover(options);
            require(set_cover.feasible, "timed set cover feasible");
            require(covers_all(problem, set_cover.selected_candidates),
                    "timed set cover coverage");
            validate_solution_structure(problem, set_cover);

            const coverage_solution improved_cardinality =
                solver.improve_maximum_coverage(max_selected,
                                                cardinality.selected_candidates, options);
            require(improved_cardinality.covered_weight >= cardinality.covered_weight,
                    "timed improve cardinality monotonic");
            validate_solution_structure(problem, improved_cardinality);

            const coverage_solution improved_set_cover =
                solver.improve_minimum_set_cover(set_cover.selected_candidates, options);
            require(improved_set_cover.feasible &&
                        improved_set_cover.total_cost <= set_cover.total_cost,
                    "timed improve set cover monotonic");
            validate_solution_structure(problem, improved_set_cover);
        }
    }

    template<class F>
    static void expect_invalid(F&& function, const std::string& label) {
        bool threw = false;
        try { function(); } catch (const std::invalid_argument&) { threw = true; }
        require(threw, label);
    }

    static void test_subproblem_boundaries() {
        coverage_problem problem{{5, 7, 0}, {1, 8, 0, 4}, {{0, 1, 2}, {0, 1, 2}, {}, {1}}, {}, {}};
        coverage_solver solver(problem);
        auto options = deterministic_options();
        options.iteration_limit = 0;
        coverage_subproblem sub;
        const std::vector<int> forbid{0};
        sub.forbidden_candidates = forbid;
        require(solver.solve_minimum_set_cover(options, sub).total_cost == 8,
                "forbidden dominator restores dominated candidate");
        const std::vector<long long> costs{100, 2, 0, 4};
        sub = {};
        sub.candidate_costs_override = costs;
        require(solver.solve_minimum_set_cover(options, sub).total_cost == 2,
                "cost override restores duplicate");
        const std::vector<int> pool{1, 2};
        sub = {};
        sub.restrict_to_candidate_pool = true;
        sub.candidate_pool = pool;
        const coverage_solver_options aggregate_options{0, 0, 13, std::chrono::steady_clock::time_point::max()};
        require(aggregate_options.seed == 13, "options aggregate field order");
        const auto exact = solver.solve_maximum_coverage({2, 2}, options, sub);
        require(exact.feasible && exact.selected_candidates == pool, "exact K includes empty candidate");
        require(!solver.solve_maximum_coverage({3, 3}, options, sub).feasible, "pool too small");
        const std::vector<int> fixed{3};
        sub.fixed_candidates = fixed;
        const auto evaluated = solver.evaluate({1}, sub);
        require(evaluated.selected_count == 2 && evaluated.total_cost == 12 &&
                evaluated.covered_target_count == 3, "fixed outside pool automatically included");
        expect_invalid([&] { solver.evaluate({0}, sub); }, "initial outside pool");
        expect_invalid([&] { solver.evaluate({3, 3}, sub); }, "duplicate fixed in initial");
        expect_invalid([&] { solver.improve_maximum_coverage(1, {1}, options, sub); }, "fixed union exceeds upper");
        sub = {};
        const std::vector<int> duplicate{1, 1};
        sub.fixed_candidates = duplicate;
        expect_invalid([&] { solver.solve_maximum_coverage(2, options, sub); }, "duplicate fixed");
        sub = {};
        sub.forbidden_candidates = duplicate;
        expect_invalid([&] { solver.solve_maximum_coverage(2, options, sub); }, "duplicate forbidden");
        sub = {};
        sub.candidate_pool = duplicate;
        require(solver.solve_maximum_coverage(1, options, sub).feasible, "disabled pool ignored");
        sub.restrict_to_candidate_pool = true;
        expect_invalid([&] { solver.solve_maximum_coverage(2, options, sub); }, "duplicate pool");
        sub = {};
        sub.fixed_candidates = forbid;
        sub.forbidden_candidates = forbid;
        expect_invalid([&] { solver.solve_minimum_set_cover(options, sub); }, "fixed forbidden conflict");
        sub = {};
        const std::vector<long long> negative{-1, 1, 1};
        sub.target_weights_override = negative;
        expect_invalid([&] { solver.evaluate({}, sub); }, "negative override");
        sub.target_weights_override = costs;
        expect_invalid([&] { solver.evaluate({}, sub); }, "override wrong size");
        expect_invalid([&] { solver.solve_maximum_coverage({2, 1}, options); }, "invalid bounds");
        expect_invalid([&] { solver.evaluate({-1}); }, "negative candidate");
        expect_invalid([&] { solver.evaluate({4}); }, "candidate past end");
        require(solver.improve_maximum_coverage({3, 3}, {}, options).selected_candidates.size() == 3,
                "lower bound repaired from empty initial");

        // 失敗した呼び出しや差し替え後でも通常の条件へ戻る
        require(solver.solve_minimum_set_cover(options).total_cost == 1, "scenario does not leak");
        coverage_solver copy = solver;
        require(copy.evaluate({0}).covered_weight == 12, "copied solver owns its data");
        coverage_solver moved = std::move(copy);
        require(moved.evaluate({0}).covered_weight == 12, "moved solver owns its data");
        auto bad_options = options;
        bad_options.time_limit_ms = -2;
        expect_invalid([&] { solver.solve_maximum_coverage(1, bad_options); }, "invalid time option");
        bad_options = options;
        bad_options.iteration_limit = -1;
        expect_invalid([&] { solver.solve_budgeted_maximum_coverage(5, bad_options); }, "invalid iteration option");
        expect_invalid([&] { solver.solve_minimum_set_cover(bad_options); }, "invalid cover option");
        const coverage_solver empty(coverage_problem{});
        require(empty.solve_maximum_coverage(0, options).feasible, "empty maximum");
        require(empty.solve_budgeted_maximum_coverage(0, options).feasible, "empty budget");
        require(empty.solve_minimum_set_cover(options).feasible, "empty cover");
        require(!empty.solve_maximum_coverage({1, 1}, options).feasible, "empty exact impossible");

        options.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        require(!solver.solve_minimum_set_cover(options).feasible, "expired initial cover");
        require(solver.improve_minimum_set_cover({0}, options).feasible, "expired preserves feasible initial");
        require(solver.improve_maximum_coverage(1, {0}, options).covered_weight == 12,
                "expired maximum preserves initial");
        require(solver.improve_budgeted_maximum_coverage(1, {0}, options).covered_weight == 12,
                "expired budget preserves initial");
    }

    static void test_random_subproblems() {
        fast_random random(49185267);
        for (int trial = 0; trial < 400; ++trial) {
            const int n = 1 + random.next_int(10);
            const int m = 1 + random.next_int(8);
            coverage_problem base;
            base.target_weights.resize(m);
            base.candidate_costs.resize(n);
            base.covered_targets.resize(n);
            for (auto& weight : base.target_weights) weight = random.next_int(15);
            for (auto& cost : base.candidate_costs) cost = random.next_int(10);
            for (int c = 0; c < n; ++c) {
                for (int t = 0; t < m; ++t) {
                    if (random.next_int(100) < 35) base.covered_targets[c].push_back(t);
                }
            }
            coverage_subproblem sub;
            std::vector<int> fixed, forbidden, pool;
            for (int c = 0; c < n; ++c) {
                const int action = random.next_int(10);
                if (action == 0) base.forced_candidates.push_back(c);
                else if (action == 1) base.forbidden_candidates.push_back(c);
                else if (action == 2) fixed.push_back(c);
                else if (action == 3) forbidden.push_back(c);
                if (random.next_int(4) != 0) pool.push_back(c);
            }
            sub.fixed_candidates = fixed;
            sub.forbidden_candidates = forbidden;
            sub.candidate_pool = pool;
            sub.restrict_to_candidate_pool = trial % 2 != 0;
            auto weights = base.target_weights;
            auto costs = base.candidate_costs;
            for (auto& weight : weights) weight = random.next_int(12);
            for (auto& cost : costs) cost = random.next_int(10);
            if (trial % 3 != 0) sub.target_weights_override = weights;
            if (trial % 4 != 0) sub.candidate_costs_override = costs;

            // 独立な集合の列挙で条件を合成し、最適値と実行可能性を確かめる
            coverage_problem naive = base;
            if (!sub.target_weights_override.empty()) naive.target_weights = weights;
            if (!sub.candidate_costs_override.empty()) naive.candidate_costs = costs;
            naive.forced_candidates.insert(naive.forced_candidates.end(), fixed.begin(), fixed.end());
            naive.forbidden_candidates.insert(naive.forbidden_candidates.end(), forbidden.begin(), forbidden.end());
            if (sub.restrict_to_candidate_pool) {
                for (int c = 0; c < n; ++c) {
                    if (std::find(pool.begin(), pool.end(), c) == pool.end() &&
                        std::find(naive.forced_candidates.begin(), naive.forced_candidates.end(), c) == naive.forced_candidates.end()) {
                        naive.forbidden_candidates.push_back(c);
                    }
                }
            }
            const int low = random.next_int(n + 2);
            const int high = low + random.next_int(n + 2 - low);
            const long long budget = random.next_int(25);
            long long best_weight = -1, best_budget = -1;
            long long best_cover = std::numeric_limits<long long>::max();
            std::vector<int> initial_card, initial_budget, initial_cover;
            for (int mask = 0; mask < (1 << n); ++mask) {
                bool valid = true;
                for (int c : naive.forced_candidates) valid &= (mask & (1 << c)) != 0;
                for (int c : naive.forbidden_candidates) valid &= (mask & (1 << c)) == 0;
                if (!valid) continue;
                std::vector<int> selected;
                for (int c = 0; c < n; ++c) if (mask & (1 << c)) selected.push_back(c);
                const auto weight = evaluate_weight(naive, selected);
                const auto cost = evaluate_cost(naive, selected);
                const int count = static_cast<int>(selected.size());
                if (low <= count && count <= high && weight > best_weight) {
                    best_weight = weight; initial_card = selected;
                }
                if (cost <= budget && weight > best_budget) { best_budget = weight; initial_budget = selected; }
                if (covers_all(naive, selected) && cost < best_cover) { best_cover = cost; initial_cover = selected; }
            }
            coverage_solver solver(base);
            coverage_solver_options options;
            options.time_limit_ms = -1;
            options.iteration_limit = 64;
            options.seed = random.next_u64();
            const auto card = solver.solve_maximum_coverage({low, high}, options, sub);
            const auto bud = solver.solve_budgeted_maximum_coverage(budget, options, sub);
            const auto cover = solver.solve_minimum_set_cover(options, sub);
            require(card.feasible == (best_weight >= 0), "subproblem cardinality feasibility");
            require(bud.feasible == (best_budget >= 0), "subproblem budget feasibility");
            require(cover.feasible == (best_cover != std::numeric_limits<long long>::max()), "subproblem cover feasibility");
            for (const auto& solution : {card, bud, cover}) {
                if (!solution.feasible) continue;
                validate_solution_structure(naive, solution);
                const auto evaluation = solver.evaluate(solution.selected_candidates, sub);
                require(evaluation.covered_weight == solution.covered_weight &&
                        evaluation.total_cost == solution.total_cost &&
                        evaluation.covered_target_count == solution.covered_target_count &&
                        evaluation.uncovered_target_count == solution.uncovered_target_count,
                        "evaluation diagnostics match independent score");
            }
            if (card.feasible) {
                require(card.covered_weight <= best_weight && static_cast<int>(card.selected_candidates.size()) >= low &&
                        static_cast<int>(card.selected_candidates.size()) <= high, "subproblem cardinality bound");
                require(solver.improve_maximum_coverage({low, high}, initial_card, options, sub).covered_weight == best_weight,
                        "optimal cardinality initial preserved");
            }
            if (bud.feasible) {
                require(bud.covered_weight <= best_budget && bud.total_cost <= budget, "subproblem budget bound");
                require(solver.improve_budgeted_maximum_coverage(budget, initial_budget, options, sub).covered_weight == best_budget,
                        "optimal budget initial preserved");
            }
            if (cover.feasible) {
                require(cover.total_cost >= best_cover && covers_all(naive, cover.selected_candidates), "subproblem cover bound");
                require(solver.improve_minimum_set_cover(initial_cover, options, sub).total_cost == best_cover,
                        "optimal cover initial preserved");
            }
            const coverage_solver fresh(base);
            require(solver.solve_maximum_coverage(high, options).selected_candidates ==
                    fresh.solve_maximum_coverage(high, options).selected_candidates, "normal call after scenario");
            require(solver.solve_maximum_coverage({low, high}, options, sub).selected_candidates == card.selected_candidates,
                    "deterministic repeated scenario");
        }
    }

    static void test_random_state_differences() {
        fast_random random(84015);
        for (int trial = 0; trial < 50; ++trial) {
            coverage_problem problem;
            problem.target_weights.resize(30);
            problem.candidate_costs.resize(40);
            problem.covered_targets.resize(40);
            for (auto& w : problem.target_weights) w = random.next_int(30);
            for (auto& c : problem.candidate_costs) c = random.next_int(30);
            for (auto& targets : problem.covered_targets) {
                for (int t = 0; t < 30; ++t) if (random.next_int(4) == 0) targets.push_back(t);
            }
            const prepared_problem stored(problem);
            const auto view = problem_view{
                stored.target_count, stored.candidate_count, stored.target_weights, stored.candidate_costs,
                stored.candidate_weight_sums, stored.candidate_offsets, stored.candidate_targets,
                stored.target_offsets, stored.target_candidates, stored.target_rarity,
                stored.forced, stored.forbidden, stored.active};
            state current(view);
            for (int step = 0; step < 400; ++step) {
                const int c = random.next_int(40);
                if (current.selected[c]) current.remove(c); else current.add(c);
                require(current.covered_weight == evaluate_weight(problem, current.selected_candidates), "state add/remove weight");
                require(current.total_cost == evaluate_cost(problem, current.selected_candidates), "state add/remove cost");
                const int a = random.next_int(40), b = random.next_int(40);
                if (current.selected[a] && !current.selected[b]) {
                    auto selected = current.selected_candidates;
                    std::replace(selected.begin(), selected.end(), a, b);
                    const long long old_weight = current.covered_weight;
                    const long long loss = current.loss(a);
                    current.remove(a);
                    require(current.covered_weight == old_weight - loss, "loss difference");
                    const long long gain = current.gain(b);
                    require(current.covered_weight + gain == evaluate_weight(problem, selected), "gain difference");
                    current.add(a);
                }
            }
        }
    }


    // 終了可能な部分問題、境界値、期限切れLNSの復元を個別に照合する
    static void test_review_regressions() {
        auto options = deterministic_options();
        options.iteration_limit = 100;
        const coverage_solver empty(coverage_problem{});
        require(empty.solve_maximum_coverage(0, options).iterations == 0, "empty cardinality stops");
        require(empty.solve_budgeted_maximum_coverage(0, options).iterations == 0, "empty budget stops");
        require(empty.solve_minimum_set_cover(options).iterations == 0, "empty cover stops");
        expect_invalid([&] { empty.improve_maximum_coverage(0, {0}, options); }, "terminal initial validated");
        expect_invalid([&] { empty.improve_minimum_set_cover({0}, options); }, "terminal cover initial validated");

        coverage_problem p{{5, 0}, {7, 3, 0}, {{0, 1}, {0}, {}}, {0}, {}};
        coverage_solver forced(p);
        for (const auto& answer : {forced.solve_maximum_coverage(2, options),
                                  forced.solve_budgeted_maximum_coverage(10, options),
                                  forced.solve_minimum_set_cover(options)}) {
            require(answer.feasible && answer.iterations == 0 && answer.selected_candidates == std::vector<int>{0},
                    "forced solution finishes without random work");
            validate_solution_structure(p, answer);
        }
        const auto exact = forced.solve_maximum_coverage({3, 3}, options);
        require(exact.feasible && exact.selected_candidates.size() == 3 && exact.iterations == 0,
                "terminal cardinality still fills lower bound");
        const coverage_problem no_targets{{}, {2, 1}, {{}, {}}, {0}, {}};
        const coverage_solver no_target_solver(no_targets);
        const auto no_target_cover = no_target_solver.solve_minimum_set_cover(options);
        require(no_target_cover.feasible && no_target_cover.total_cost == 2 && no_target_cover.iterations == 0,
                "empty targets preserve fixed empty candidate");
        require(no_target_solver.solve_maximum_coverage({2, 2}, options).selected_candidates.size() == 2,
                "empty targets exact K still filled");
        expect_invalid([&] { forced.improve_budgeted_maximum_coverage(7, {1}, options); },
                       "terminal budget checks invalid initial");
        expect_invalid([&] { forced.improve_minimum_set_cover({0, 0}, options); },
                       "terminal cover checks duplicates");

        p.forced_candidates.clear();
        p.target_weights = {0, 0};
        coverage_solver zero(p);
        require(zero.solve_maximum_coverage(2, options).iterations == 0, "zero weights stop cardinality");
        require(zero.solve_budgeted_maximum_coverage(10, options).iterations == 0, "zero weights stop budget");
        require(zero.solve_minimum_set_cover(options).covered_target_count == 2, "zero weights still need cover");
        p.target_weights = {5, 0};
        coverage_solver tight(p);
        require(tight.solve_maximum_coverage(0, options).iterations == 0, "zero slots stop");
        require(tight.solve_budgeted_maximum_coverage(0, options).iterations == 0, "no affordable gain stops");

        // 合計がlong long内なら、大きな重み・費用でも差分と返却診断値は正確
        const long long large = std::numeric_limits<long long>::max() / 4;
        coverage_problem wide{{large, large, 0}, {large, large, large}, {{0}, {1}, {0, 1, 2}}, {}, {}};
        coverage_solver wide_solver(wide);
        const auto wide_answer = wide_solver.solve_budgeted_maximum_coverage(large, options);
        require(wide_answer.covered_weight == large * 2 && wide_answer.total_cost == large, "large integer values");
        validate_solution_structure(wide, wide_answer);

        // 局所lambdaのswap差分・期限切れLNS復元は test_internal_v07.py で実装から抽出して検証する。
        // コンストラクタ検証も、公開APIとは別に全入力欄を確認する
        expect_invalid([&] { coverage_solver bad(coverage_problem{{-1}, {1}, {{0}}, {}, {}}); }, "negative base weight");
        expect_invalid([&] { coverage_solver bad(coverage_problem{{1}, {-1}, {{0}}, {}, {}}); }, "negative base cost");
        expect_invalid([&] { coverage_solver bad(coverage_problem{{1}, {}, {{0}}, {}, {}}); }, "base size mismatch");
        expect_invalid([&] { coverage_solver bad(coverage_problem{{1}, {1}, {{1}}, {}, {}}); }, "base target range");
        expect_invalid([&] { coverage_solver bad(coverage_problem{{1}, {1}, {{0}}, {1}, {}}); }, "base fixed range");
        expect_invalid([&] { coverage_solver bad(coverage_problem{{1}, {1}, {{0}}, {0}, {0}}); }, "base fixed forbidden conflict");
    }

    static void test_iteration_limits() {
        const coverage_problem base{{5, 7, 11, 0}, {2, 3, 4, 0, 5},
                                    {{0}, {1}, {2}, {3}, {0, 1}}, {}, {}};
        const coverage_solver solver(base);
        const std::array<int, 1> fixed{3}, forbidden{4};
        const std::array<int, 3> pool{0, 1, 2};
        const std::array<long long, 4> weights{11, 5, 7, 0};
        const std::array<long long, 5> costs{2, 1, 3, 0, 4};

        // 通常問題と、固定・禁止・pool・数値差し替えを併用する部分問題を確認する
        for (int scenario = 0; scenario < 2; ++scenario) {
            coverage_subproblem sub;
            coverage_problem effective = base;
            if (scenario != 0) {
                sub = {fixed, forbidden, true, pool, weights, costs};
                effective.forced_candidates.assign(fixed.begin(), fixed.end());
                effective.forbidden_candidates.assign(forbidden.begin(), forbidden.end());
                effective.target_weights.assign(weights.begin(), weights.end());
                effective.candidate_costs.assign(costs.begin(), costs.end());
            }

            // 再始動と8回のswap、時計確認の64反復、それぞれの境界をまたいで試す
            for (std::uint64_t seed = 0; seed < 32; ++seed) {
                auto options = deterministic_options(seed);
                for (int limit = 0; limit <= 129; ++limit) {
                    options.iteration_limit = limit;
                    const std::array<coverage_solution, 6> answers{
                        solver.solve_maximum_coverage(3, options, sub),
                        solver.improve_maximum_coverage(3, {0}, options, sub),
                        solver.solve_budgeted_maximum_coverage(5, options, sub),
                        solver.improve_budgeted_maximum_coverage(5, {0}, options, sub),
                        solver.solve_minimum_set_cover(options, sub),
                        solver.improve_minimum_set_cover({0}, options, sub)};
                    for (int mode = 0; mode < 6; ++mode) {
                        const auto& answer = answers[mode];
                        require(answer.feasible, "iteration boundary feasible");
                        require(answer.iterations == limit, "iteration limit respected exactly");
                        validate_solution_structure(effective, answer);
                        if (mode < 2) require(answer.selected_candidates.size() <= 3, "iteration cardinality bound");
                        else if (mode < 4) require(answer.total_cost <= 5, "iteration budget bound");
                        else require(covers_all(effective, answer.selected_candidates), "iteration complete cover");
                    }
                }
            }
        }
    }

    static void run_all_tests() {
        test_iteration_limits();
        test_review_regressions();
        test_subproblem_boundaries();
        test_random_subproblems();
        test_random_state_differences();
        test_basic_cases();
        test_forced_forbidden_and_edges();
        test_random_against_exact();
        test_improve_and_validation();
        test_selected_search_consistency();
    }
};

int main() {
    try {
        coverage_solver::selftest::run_all_tests();
        std::cout << "maximum_coverage_solver_v07: all tests passed (49920 iteration-boundary calls, 400 subproblems, 20000 state operations, legacy tests)\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

#endif
