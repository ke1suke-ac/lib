#pragma once

#include <bits/stdc++.h>

/*
 * generalized_assignment_solver
 *
 * 各仕事を割当可能な候補のいずれかへ割り当て、割当先ごとの容量を守りながら
 * コスト総和を小さくする一般化割当問題（GAP）のヒューリスティックsolver。
 * 各候補は割当先、容量消費量、コストを持ち、同じ仕事・割当先に複数候補を登録してもよい。
 *
 * 典型的な使い方:
 *
 *   generalized_assignment_solver solver(job_count, agent_count, capacities);
 *   for (int i = 0; i < job_count; i++) {
 *       for (auto [j, demand, cost] : candidates[i]) {
 *           solver.add_option(i, j, demand, cost);
 *       }
 *   }
 *
 *   generalized_assignment_solver::solve_options options;
 *   options.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1900);
 *   options.iteration_limit = std::numeric_limits<long long>::max();
 *   options.seed = 123456789;
 *   options.algorithm = generalized_assignment_solver::profile::alns;
 *   auto result = solver.solve(options);
 *
 * result.option_ids[i] は add_option の戻り値、result.agent_of_job[i] は割当先を表す。
 * 実行可能解を発見できなかった場合も、未割当仕事数が最も少ない解を返す。
 * コスト、容量、消費量およびそれらの総和が long long に収まることは呼び出し側で保証する。
 *
 * 探索構成:
 *   1. fail-first + regret によるランダム化初期解
 *   2. ラグランジュ容量価格による初期解誘導
 *   3. relocate / exchange による局所探索
 *   4. 順位付きregret repairと連鎖ruinを使うALNS
 *
 * ALNS用の候補評価順をsolveごとに一度だけ構築し、各repairでは
 * 実行可能な上位2候補までで走査を打ち切る。連鎖ruinでは現在より安い候補先から
 * 所属仕事を選び、その仕事の安い候補先へも連鎖的に進む。
 * repair中の容量減少がないことを利用して各仕事の上位2候補とregretを保持し、
 * 直前にloadが増えた割当先を上位候補に持つ仕事だけを差分更新する。
 * 部分問題を繰り返し解く用途には、固定仕事を保護するrepair、
 * 前処理済みsession、コスト・容量の一時差し替えを利用できる。
 * multi-start間では仕事難易度を再利用し、決定的構築では最良候補だけを保持して
 * 未使用乱数を一括skipする。exchangeは4抽出中の最安候補を入口とし、相手を任意先へ移す。
 * 実行可能化では同じ割当先の省容量モードへの変更も許し、通常の手順で失敗した場合は
 * 容量消費を優先する構築を一度試す。各仕事で容量内に収まる候補の最小コスト和に一致した完全解を得たら
 * 整数比較によって最適性を確認し、追加探索を停止する。
 * relocateとexchangeで整数コスト順を共有し、改善可能な最良実行可能候補で走査を止める。
 * ALNSで割当済みの仕事はregretを負の無限大にし、選択時の間接参照を減らす。
 *
 * 計算量の記号: N=仕事数、A=割当先数、E=候補数、D=仕事ごとの最大候補数、
 * P=N+A+E log(D+1)、T=選択した探索の処理量、Es=可変仕事の候補数、Ts=部分探索の処理量
 *
 * profile::greedy          : 初期解を1個生成
 * profile::regret          : regret初期解を複数生成
 * profile::lagrangian      : 容量価格付き初期解を複数生成
 * profile::local_search    : lagrangian + relocate + exchange
 * profile::alns            : local_search + ruin-and-recreate
 */

struct generalized_assignment_solver {
    class session;

    enum class profile {
        greedy,
        regret,
        lagrangian,
        local_search,
        alns,
    };

    struct solve_options {
        profile algorithm = profile::alns;
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max();
        long long iteration_limit = 10000;
        std::uint64_t seed = 123456789;

        // 高度な探索調整。通常は実測済みのデフォルト値のままでよい
        // 1回のruinで外す仕事数の上限
        int maximum_ruin_jobs = 64;
        // regret repairでラグランジュ容量価格を目的コストへ加える倍率
        double repair_price_weight = 0.25;
        // ALNSで悪化解を受理する初期温度の倍率
        double alns_temperature = 0.05;
        // relocate descentを挟むALNS反復間隔。0なら実行しない
        int local_search_interval = 128;
    };

    enum class repair_status {
        // solve / improve / evaluate の結果
        not_requested,
        // repairを通常どおり実行した
        completed,
        // mutable_jobsまたは固定仕事の割当が不正
        invalid_input,
        // 固定仕事だけで容量を超過しており、固定したままでは実行不能
        fixed_part_infeasible,
    };

    struct result {
        std::vector<int> option_ids;
        std::vector<int> agent_of_job;
        std::vector<long long> load_of_agent;
        long long objective = 0;
        long long total_overflow = 0;
        int unassigned_count = 0;
        long long iterations = 0;
        double lower_bound = -std::numeric_limits<double>::infinity();
        bool initial_solution_used = false;
        repair_status repair_state = repair_status::not_requested;

        // 全仕事が割当済みで容量超過もなければ true を返す / O(1)
        bool feasible() const { return unassigned_count == 0 && total_overflow == 0; }
    };

    struct assignment_option {
        int job;
        int agent;
        long long demand;
        long long cost;
    };

    // 空のsolverを構築する / O(1)
    generalized_assignment_solver() = default;

    // jobs個の仕事、agents個の割当先、各割当先の容量を持つsolverを構築する / O(agents)
    generalized_assignment_solver(int jobs, int agents, std::vector<long long> capacities) {
        init(jobs, agents, std::move(capacities));
    }

    // 問題を空の状態に初期化する / O(agents)
    void init(int jobs, int agents, std::vector<long long> capacities) {
        assert(jobs >= 0);
        assert(agents >= 0);
        assert((int)capacities.size() == agents);
        assert(std::all_of(capacities.begin(), capacities.end(), [](long long capacity) {
            return capacity >= 0;
        }));
        job_count_ = jobs;
        agent_count_ = agents;
        capacities_ = std::move(capacities);
        options_.clear();
    }

    // jobをagentへ割り当てる候補を追加し、候補IDを返す / 償却 O(1)
    int add_option(int job, int agent, long long demand, long long cost) {
        assert(0 <= job && job < job_count_);
        assert(0 <= agent && agent < agent_count_);
        assert(demand >= 0);
        int id = (int)options_.size();
        options_.push_back({job, agent, demand, cost});
        return id;
    }

    // 指定した探索条件で初期解を生成して改善する / O(P + T)
    result solve(const solve_options& options) const;

    // option_idsを初期解候補として受け取り、指定した探索条件で改善する / O(P + T)
    result improve(std::span<const int> option_ids, const solve_options& options) const;

    // mutable_jobsだけを変更して割当を修復・改善し、補集合の仕事は固定する / O(P + Ts)
    result repair(std::span<const int> option_ids,
                  std::span<const int> mutable_jobs,
                  const solve_options& options) const;

    // option_idsが表す割当を評価する / O(jobs + agents)
    result evaluate(std::span<const int> option_ids) const;

    // 前処理済みのsnapshotを作り、反復solve / improve / repairで再利用する / O(P)
    session make_session() const;

    // 仕事数を返す / O(1)
    int job_count() const { return job_count_; }

    // 割当先数を返す / O(1)
    int agent_count() const { return agent_count_; }

    // 追加済み候補数を返す / O(1)
    int option_count() const { return (int)options_.size(); }

    // 登録済み候補を返す / O(1)
    const std::vector<assignment_option>& options() const { return options_; }

    // 容量列を返す / O(1)
    const std::vector<long long>& capacities() const { return capacities_; }

private:
    struct edge {
        int agent;
        long long demand;
        long long cost;
        int raw_id;
    };

    struct prepared_problem {
        int jobs = 0;
        int agents = 0;
        std::vector<long long> capacity;
        std::vector<int> offset;
        std::vector<edge> edges;
        std::vector<int> raw_to_edge;
        long long independent_lower_bound = 0;
        bool independent_bound_valid = true;
        double average_cost_range = 1.0;
        double average_demand = 1.0;
    };

    struct state {
        std::vector<int> chosen_edge;
        std::vector<long long> load;
        std::vector<std::vector<int>> jobs_of_agent;
        std::vector<int> position_in_agent;
        long long objective = 0;
        int unassigned = 0;
    };

    struct fast_rng {
        std::uint64_t state;

        std::uint64_t next() {
            std::uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return z ^ (z >> 31);
        }

        void discard(std::uint64_t count) {
            state += 0x9e3779b97f4a7c15ULL * count;
        }

        int index(int n) {
            assert(n > 0);
            return (int)((__uint128_t)next() * (unsigned)n >> 64);
        }

        double unit() {
            return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0);
        }
    };

    int job_count_ = 0;
    int agent_count_ = 0;
    std::vector<long long> capacities_;
    std::vector<assignment_option> options_;

    static bool time_up(const solve_options& options) {
        return std::chrono::steady_clock::now() >= options.deadline;
    }

    static void refresh_statistics(prepared_problem& problem) {
        problem.independent_lower_bound = 0;
        problem.independent_bound_valid = true;
        long double range_sum = 0;
        long double demand_sum = 0;
        for (int job = 0; job < problem.jobs; job++) {
            if (problem.offset[job] == problem.offset[job + 1]) {
                problem.independent_bound_valid = false;
                continue;
            }
            long long min_cost = problem.edges[problem.offset[job]].cost;
            long long max_cost = min_cost;
            long long feasible_min_cost = 0;
            bool has_feasible_option = false;
            for (int id = problem.offset[job]; id < problem.offset[job + 1]; id++) {
                min_cost = std::min(min_cost, problem.edges[id].cost);
                max_cost = std::max(max_cost, problem.edges[id].cost);
                demand_sum += problem.edges[id].demand;
                const edge& candidate = problem.edges[id];
                if (candidate.demand <= problem.capacity[candidate.agent]) {
                    if (!has_feasible_option || candidate.cost < feasible_min_cost) {
                        feasible_min_cost = candidate.cost;
                    }
                    has_feasible_option = true;
                }
            }
            if (has_feasible_option) problem.independent_lower_bound += feasible_min_cost;
            else problem.independent_bound_valid = false;
            range_sum += (long double)max_cost - min_cost;
        }
        problem.average_cost_range = problem.jobs == 0
            ? 1.0
            : std::max(1.0, (double)(range_sum / problem.jobs));
        problem.average_demand = problem.edges.empty()
            ? 1.0
            : std::max(1.0, (double)(demand_sum / problem.edges.size()));
    }

    void prepare(prepared_problem& problem) const {
        problem.jobs = job_count_;
        problem.agents = agent_count_;
        problem.capacity = capacities_;
        problem.offset.assign(job_count_ + 1, 0);
        problem.raw_to_edge.assign(options_.size(), -1);

        // 候補を仕事ごとの連続領域へ並べる
        for (const auto& option : options_) problem.offset[option.job + 1]++;
        for (int i = 0; i < job_count_; i++) problem.offset[i + 1] += problem.offset[i];
        problem.edges.resize(options_.size());
        std::vector<int> cursor = problem.offset;
        for (int id = 0; id < (int)options_.size(); id++) {
            const auto& option = options_[id];
            int pos = cursor[option.job]++;
            problem.edges[pos] = {option.agent, option.demand, option.cost, id};
        }

        // 同一仕事内をagent・コスト・需要順にし、内部候補順を固定する
        for (int job = 0; job < job_count_; job++) {
            auto first = problem.edges.begin() + problem.offset[job];
            auto last = problem.edges.begin() + problem.offset[job + 1];
            std::sort(first, last, [](const edge& lhs, const edge& rhs) {
                if (lhs.agent != rhs.agent) return lhs.agent < rhs.agent;
                if (lhs.cost != rhs.cost) return lhs.cost < rhs.cost;
                return lhs.demand < rhs.demand;
            });
        }
        for (int id = 0; id < (int)problem.edges.size(); id++) {
            problem.raw_to_edge[problem.edges[id].raw_id] = id;
        }

        // 問題規模に依存しない探索パラメータの基準値を計算する
        refresh_statistics(problem);
    }

    static state make_empty_state(const prepared_problem& problem) {
        state assignment;
        assignment.chosen_edge.assign(problem.jobs, -1);
        assignment.load.assign(problem.agents, 0);
        assignment.jobs_of_agent.resize(problem.agents);
        assignment.position_in_agent.assign(problem.jobs, -1);
        assignment.unassigned = problem.jobs;
        return assignment;
    }

    static void remove_job(const prepared_problem& problem, state& assignment, int job) {
        int old_id = assignment.chosen_edge[job];
        if (old_id < 0) return;
        const edge& old_edge = problem.edges[old_id];

        // 所属列からswap-popで除き、移動した仕事の位置を更新する
        auto& jobs = assignment.jobs_of_agent[old_edge.agent];
        int pos = assignment.position_in_agent[job];
        int moved_job = jobs.back();
        jobs[pos] = moved_job;
        assignment.position_in_agent[moved_job] = pos;
        jobs.pop_back();

        assignment.load[old_edge.agent] -= old_edge.demand;
        assignment.objective -= old_edge.cost;
        assignment.chosen_edge[job] = -1;
        assignment.position_in_agent[job] = -1;
        assignment.unassigned++;
    }

    static void assign_job(const prepared_problem& problem, state& assignment, int job, int edge_id) {
        assert(problem.offset[job] <= edge_id && edge_id < problem.offset[job + 1]);
        if (assignment.chosen_edge[job] >= 0) remove_job(problem, assignment, job);
        const edge& new_edge = problem.edges[edge_id];
        assignment.chosen_edge[job] = edge_id;
        assignment.position_in_agent[job] = (int)assignment.jobs_of_agent[new_edge.agent].size();
        assignment.jobs_of_agent[new_edge.agent].push_back(job);
        assignment.load[new_edge.agent] += new_edge.demand;
        assignment.objective += new_edge.cost;
        assignment.unassigned--;
    }

    static bool fits(const prepared_problem& problem, const state& assignment, int job, int edge_id) {
        const edge& candidate = problem.edges[edge_id];
        long long load = assignment.load[candidate.agent] + candidate.demand;
        int old_id = assignment.chosen_edge[job];
        if (old_id >= 0 && problem.edges[old_id].agent == candidate.agent) {
            load -= problem.edges[old_id].demand;
        }
        return load <= problem.capacity[candidate.agent];
    }

    static bool better_partial(const state& lhs, const state& rhs) {
        if (lhs.unassigned != rhs.unassigned) return lhs.unassigned < rhs.unassigned;
        return lhs.objective < rhs.objective;
    }

    static state construct(const prepared_problem& problem,
                           const std::vector<double>& price,
                           std::vector<double>& job_difficulty,
                           fast_rng& rng,
                           double randomization) {
        state assignment = make_empty_state(problem);
        bool initialize_difficulty = static_cast<int>(job_difficulty.size()) != problem.jobs;
        if (initialize_difficulty) job_difficulty.resize(problem.jobs);
        std::vector<std::pair<double, int>> keyed_jobs;
        keyed_jobs.reserve(problem.jobs);
        for (int job = 0; job < problem.jobs; job++) {
            double difficulty = 0;
            if (initialize_difficulty) {
                int degree = problem.offset[job + 1] - problem.offset[job];
                double best = std::numeric_limits<double>::infinity();
                double second = best;
                double largest_ratio = 0;
                for (int id = problem.offset[job]; id < problem.offset[job + 1]; id++) {
                    const edge& candidate = problem.edges[id];
                    double adjusted = static_cast<double>(candidate.cost) +
                                      price[candidate.agent] *
                                          static_cast<double>(candidate.demand);
                    if (adjusted < best) {
                        second = best;
                        best = adjusted;
                    } else if (adjusted < second) {
                        second = adjusted;
                    }
                    double denominator = static_cast<double>(
                        std::max(1LL, problem.capacity[candidate.agent]));
                    largest_ratio = std::max(
                        largest_ratio, static_cast<double>(candidate.demand) / denominator);
                }
                double regret = std::isfinite(second)
                                    ? second - best
                                    : problem.average_cost_range * 4;
                difficulty = 3.0 / std::max(1, degree) + 2.0 * largest_ratio +
                             regret / (problem.average_cost_range + 1.0);
                job_difficulty[job] = difficulty;
            } else {
                difficulty = job_difficulty[job];
            }
            if (randomization != 0) difficulty += randomization * (rng.unit() - 0.5);
            keyed_jobs.push_back({-difficulty, job});
        }
        if (randomization == 0) rng.discard(static_cast<std::uint64_t>(problem.jobs));
        std::sort(keyed_jobs.begin(), keyed_jobs.end());
        auto choose_construct_edge = [&](int job) {
            std::array<std::pair<double, int>, 3> best = {{
                {std::numeric_limits<double>::infinity(), -1},
                {std::numeric_limits<double>::infinity(), -1},
                {std::numeric_limits<double>::infinity(), -1},
            }};
            double pressure_scale = problem.average_cost_range * 0.12;
            int feasible_count = 0;

            // 決定的構築は最良1個、ランダム化構築は上位3個を保持する
            for (int id = problem.offset[job]; id < problem.offset[job + 1]; id++) {
                const edge& candidate = problem.edges[id];
                long long residual_after = problem.capacity[candidate.agent] -
                                           assignment.load[candidate.agent] - candidate.demand;
                if (residual_after < 0) continue;
                feasible_count++;
                double pressure = pressure_scale * static_cast<double>(candidate.demand) /
                                  (static_cast<double>(residual_after) + problem.average_demand);
                double score = static_cast<double>(candidate.cost) +
                               price[candidate.agent] * static_cast<double>(candidate.demand) + pressure;
                if (randomization != 0) {
                    score += randomization * problem.average_cost_range * 0.08 *
                             (rng.unit() - 0.5);
                }
                std::pair<double, int> value{score, id};
                if (randomization == 0) {
                    if (value < best[0]) best[0] = value;
                    continue;
                }
                for (int k = 0; k < 3; k++) {
                    if (value < best[k]) {
                        std::swap(value, best[k]);
                    }
                }
            }
            if (best[0].second < 0) return -1;
            if (randomization == 0) {
                int selection_draws = std::min(2, feasible_count - 1);
                rng.discard(static_cast<std::uint64_t>(feasible_count + selection_draws));
                return best[0].second;
            }
            if (best[1].second >= 0 && rng.unit() < randomization * 0.10) return best[1].second;
            if (best[2].second >= 0 && rng.unit() < randomization * 0.03) return best[2].second;
            return best[0].second;
        };
        for (const auto& entry : keyed_jobs) {
            int job = entry.second;
            int edge_id = choose_construct_edge(job);
            if (edge_id >= 0) assign_job(problem, assignment, job, edge_id);
        }
        auto try_one_ejection = [&](int job) {
            long double best_delta = std::numeric_limits<long double>::infinity();
            int best_job_edge = -1;
            int best_victim = -1;
            int best_victim_edge = -1;

            // 未割当仕事の容量を空けるため、1仕事の割当先または実行モードを変更する
            for (int id = problem.offset[job]; id < problem.offset[job + 1]; id++) {
                const edge& incoming = problem.edges[id];
                const auto& residents = assignment.jobs_of_agent[incoming.agent];
                int checks = std::min(128, (int)residents.size());
                int start = residents.empty() ? 0 : rng.index((int)residents.size());
                for (int step = 0; step < checks; step++) {
                    int victim = residents[(start + step) % residents.size()];
                    int old_id = assignment.chosen_edge[victim];
                    const edge& old_edge = problem.edges[old_id];
                    if (assignment.load[incoming.agent] - old_edge.demand + incoming.demand >
                        problem.capacity[incoming.agent]) continue;

                    for (int next_id = problem.offset[victim]; next_id < problem.offset[victim + 1]; next_id++) {
                        const edge& outgoing = problem.edges[next_id];
                        long long outgoing_load = assignment.load[outgoing.agent] + outgoing.demand;
                        if (outgoing.agent == incoming.agent) outgoing_load += incoming.demand - old_edge.demand;
                        if (outgoing_load > problem.capacity[outgoing.agent]) continue;
                        long double delta = (long double)incoming.cost + outgoing.cost - old_edge.cost;
                        delta += 0.02L * price[outgoing.agent] * outgoing.demand;
                        if (delta < best_delta) {
                            best_delta = delta;
                            best_job_edge = id;
                            best_victim = victim;
                            best_victim_edge = next_id;
                        }
                    }
                }
            }
            if (best_job_edge < 0) return false;
            assign_job(problem, assignment, best_victim, best_victim_edge);
            assign_job(problem, assignment, job, best_job_edge);
            return true;
        };
        bool progress = true;
        while (assignment.unassigned > 0 && progress) {
            progress = false;
            for (int job = 0; job < problem.jobs; job++) {
                if (assignment.chosen_edge[job] >= 0) continue;
                int best_id = -1;
                double best_score = std::numeric_limits<double>::infinity();
                for (int id = problem.offset[job]; id < problem.offset[job + 1]; id++) {
                    if (!fits(problem, assignment, job, id)) continue;
                    const edge& candidate = problem.edges[id];
                    double score = static_cast<double>(candidate.cost) +
                                   price[candidate.agent] * static_cast<double>(candidate.demand);
                    if (score < best_score) {
                        best_score = score;
                        best_id = id;
                    }
                }
                if (best_id >= 0) {
                    assign_job(problem, assignment, job, best_id);
                    progress = true;
                } else if (try_one_ejection(job)) {
                    progress = true;
                }
            }
        }
        return assignment;
    }

    static void relocate_descent(const prepared_problem& problem,
                                 state& assignment,
                                 fast_rng& rng,
                                 int max_passes,
                                 const solve_options& options,
                                 const std::vector<int>& cost_order) {
        std::vector<int> order(problem.jobs);
        std::iota(order.begin(), order.end(), 0);

        // 各passで仕事順を変え、最良の実行可能relocateを即時適用する
        for (int pass = 0; pass < max_passes && !time_up(options); pass++) {
            for (int i = problem.jobs - 1; i > 0; i--) std::swap(order[i], order[rng.index(i + 1)]);
            bool improved = false;
            for (int job : order) {
                int old_id = assignment.chosen_edge[job];
                int best_id = old_id;
                for (int pos = problem.offset[job]; pos < problem.offset[job + 1]; pos++) {
                    int id = cost_order[pos];
                    if (problem.edges[id].cost >= problem.edges[old_id].cost) break;
                    if (fits(problem, assignment, job, id)) {
                        best_id = id;
                        break;
                    }
                }
                if (best_id != old_id) {
                    assign_job(problem, assignment, job, best_id);
                    improved = true;
                }
            }
            if (!improved) break;
        }
    }

    static void add_unique_job(std::vector<int>& selected,
                               std::vector<int>& marks,
                               int stamp,
                               int job) {
        if (marks[job] == stamp) return;
        marks[job] = stamp;
        selected.push_back(job);
    }

    static void restore_jobs(const prepared_problem& problem,
                             state& assignment,
                             const std::vector<int>& jobs,
                             const std::vector<int>& original_edges) {
        for (int job : jobs) remove_job(problem, assignment, job);
        for (int i = 0; i < (int)jobs.size(); i++) {
            if (original_edges[i] >= 0) assign_job(problem, assignment, jobs[i], original_edges[i]);
        }
    }

    static result make_result(const prepared_problem& problem,
                              const state& assignment,
                              long long iterations,
                              double lower_bound) {
        result answer{.option_ids = std::vector<int>(problem.jobs, -1),
                      .agent_of_job = std::vector<int>(problem.jobs, -1),
                      .load_of_agent = assignment.load,
                      .objective = assignment.objective,
                      .unassigned_count = assignment.unassigned,
                      .iterations = iterations, .lower_bound = lower_bound};
        for (int job = 0; job < problem.jobs; job++) {
            int edge_id = assignment.chosen_edge[job];
            if (edge_id < 0) continue;
            answer.option_ids[job] = problem.edges[edge_id].raw_id;
            answer.agent_of_job[job] = problem.edges[edge_id].agent;
        }
        for (int agent = 0; agent < problem.agents; agent++) {
            answer.total_overflow += std::max(0LL, assignment.load[agent] - problem.capacity[agent]);
        }
        return answer;
    }

    static result solve_prepared(const prepared_problem& problem,
                                 const std::vector<int>* initial_edge_ids,
                                 const solve_options& options) {
        fast_rng rng{options.seed + 0x9e3779b97f4a7c15ULL};
        double lower_bound = -std::numeric_limits<double>::infinity();
        std::vector<double> price;

        state best;
        bool has_candidate = false;

        // 呼び出し側から渡された解が有効なら、初期incumbentとして採用する
        bool initial_solution_used = false;
        if (initial_edge_ids) {
            state initial = make_empty_state(problem);
            bool valid = (int)initial_edge_ids->size() == problem.jobs;
            for (int job = 0; job < problem.jobs; job++) {
                if (!valid) break;
                int edge_id = (*initial_edge_ids)[job];
                if (edge_id < problem.offset[job] || edge_id >= problem.offset[job + 1]) {
                    valid = false;
                    break;
                }
                if (!fits(problem, initial, job, edge_id)) {
                    valid = false;
                    break;
                }
                assign_job(problem, initial, job, edge_id);
            }
            if (valid) {
                best = std::move(initial);
                has_candidate = true;
                initial_solution_used = true;
            }
        }

        if (has_candidate && problem.independent_bound_valid &&
            best.objective == problem.independent_lower_bound) {
            result answer = make_result(problem, best, 0, static_cast<double>(best.objective));
            answer.initial_solution_used = true;
            return answer;
        }
        if (options.algorithm >= profile::lagrangian) {
            auto compute_prices = [](const prepared_problem& input,
                                     const solve_options& parameters, double& bound) __attribute__((noinline)) {
                std::vector<double> capacity_price(input.agents, 0.0);
                std::vector<double> best_price = capacity_price;
                std::vector<long long> load(input.agents);
                const long long edge_count = (long long)input.edges.size();
                int rounds = (int)std::clamp(6000000LL / std::max(1LL, edge_count) + 4, 4LL, 24LL);
                double base_step = input.average_cost_range / input.average_demand;
                bound = -std::numeric_limits<double>::infinity();

                // 容量制約を価格化し、独立な最小割当とsubgradient更新を交互に行う
                for (int round = 0; round < rounds && !time_up(parameters); round++) {
                    std::fill(load.begin(), load.end(), 0);
                    double round_lower_bound = 0;
                    bool complete = true;
                    for (int job = 0; job < input.jobs; job++) {
                        double best_score = std::numeric_limits<double>::infinity();
                        int best_id = -1;
                        for (int id = input.offset[job]; id < input.offset[job + 1]; id++) {
                            const edge& candidate = input.edges[id];
                            double adjusted = static_cast<double>(candidate.cost) +
                                              capacity_price[candidate.agent] * static_cast<double>(candidate.demand);
                            if (adjusted < best_score) {
                                best_score = adjusted;
                                best_id = id;
                            }
                        }
                        if (best_id < 0) {
                            complete = false;
                            continue;
                        }
                        round_lower_bound += best_score;
                        load[input.edges[best_id].agent] += input.edges[best_id].demand;
                    }
                    for (int agent = 0; agent < input.agents; agent++) {
                        round_lower_bound -= capacity_price[agent] * static_cast<double>(input.capacity[agent]);
                    }
                    if (complete && round_lower_bound > bound) {
                        bound = round_lower_bound;
                        best_price = capacity_price;
                    }

                    double step = base_step * std::pow(0.86, round);
                    for (int agent = 0; agent < input.agents; agent++) {
                        double denominator = static_cast<double>(std::max(1LL, input.capacity[agent]));
                        double violation_ratio = static_cast<double>(load[agent] - input.capacity[agent]) / denominator;
                        capacity_price[agent] = std::max(0.0, capacity_price[agent] + step * violation_ratio);
                    }
                }
                return best_price;
            };
            price = compute_prices(problem, options, lower_bound);
        } else {
            price.assign(problem.agents, 0.0);
        }

        // profileに応じた回数だけランダム化構築を行い、最良の実行可能または部分解を残す
        int starts = 1;
        if (options.algorithm >= profile::regret) {
            long long edge_count = (long long)problem.edges.size();
            starts = (int)std::clamp(1200000LL / std::max(1LL, edge_count) + 2, 2LL, 9LL);
        }
        std::vector<double> job_difficulty;
        for (int start = 0; start < starts && !time_up(options); start++) {
            double randomization = start == 0 ? 0.0 : 0.35 + 0.12 * start;
            state candidate = construct(problem, price, job_difficulty, rng, randomization);
            if (!has_candidate || better_partial(candidate, best)) {
                best = std::move(candidate);
                has_candidate = true;
            }
            if (best.unassigned == 0 && problem.independent_bound_valid &&
                best.objective == problem.independent_lower_bound) break;
        }

        if (!has_candidate) best = make_empty_state(problem);

        // 通常回数で実行可能化できなかった場合だけ、残り時間を追加multi-startへ使う
        if (best.unassigned > 0 &&
            options.algorithm >= profile::lagrangian) {
            for (int start = 0; start < 100 && !time_up(options) && best.unassigned > 0; start++) {
                state candidate = construct(
                    problem, price, job_difficulty, rng, 1.0 + 0.03 * start);
                if (better_partial(candidate, best)) best = std::move(candidate);
            }
        }

        if (best.unassigned > 0 &&
            options.algorithm >= profile::lagrangian && !time_up(options)) {
            auto repair_partial_lns = [&](state& assignment) {
                auto regret_repair = [&](const std::vector<int>& selected) {
                    int remaining = (int)selected.size();
                    while (remaining > 0) {
                        int chosen_job = -1;
                        int chosen_edge = -1;
                        double largest_regret = -std::numeric_limits<double>::infinity();

                        // 未割当仕事ごとに実行可能な最良・第2候補を求め、後回しにしにくい仕事を選ぶ
                        for (int selected_id = 0; selected_id < (int)selected.size(); selected_id++) {
                            int job = selected[selected_id];
                            if (assignment.chosen_edge[job] >= 0) continue;
                            double best_score = std::numeric_limits<double>::infinity();
                            double second = best_score;
                            int best_id = -1;

                            for (int id = problem.offset[job]; id < problem.offset[job + 1]; id++) {
                                if (!fits(problem, assignment, job, id)) continue;
                                const edge& candidate = problem.edges[id];
                                double score = static_cast<double>(candidate.cost) + 0.25 *
                                               price[candidate.agent] * static_cast<double>(candidate.demand);
                                score += problem.average_cost_range * 0.03 * (rng.unit() - 0.5);
                                if (score < best_score) {
                                    second = best_score;
                                    best_score = score;
                                    best_id = id;
                                } else if (score < second) {
                                    second = score;
                                }
                            }
                            if (best_id < 0) continue;
                            double regret = std::isfinite(second) ? second - best_score : problem.average_cost_range * 8;
                            int degree = problem.offset[job + 1] - problem.offset[job];
                            regret += problem.average_cost_range / std::max(1, degree);
                            if (regret > largest_regret) {
                                largest_regret = regret;
                                chosen_job = job;
                                chosen_edge = best_id;
                            }
                        }
                        if (chosen_job < 0) return false;
                        assign_job(problem, assignment, chosen_job, chosen_edge);
                        remaining--;
                    }
                    return true;
                };
                std::vector<int> marks(problem.jobs, 0);
                int stamp = 0;

                // 未割当仕事と候補割当先の周辺を小さく壊し、複数仕事の同時入替で実行可能化する
                for (int attempt = 0; attempt < 1000 && assignment.unassigned > 0; attempt++) {
                    if ((attempt & 15) == 0 && time_up(options)) break;
                    std::vector<int> unassigned_jobs;
                    for (int job = 0; job < problem.jobs; job++) {
                        if (assignment.chosen_edge[job] < 0) unassigned_jobs.push_back(job);
                    }
                    int seed = unassigned_jobs[rng.index((int)unassigned_jobs.size())];
                    int count = std::min(problem.jobs, 8 + rng.index(25));
                    count = std::min(count, problem.jobs - assignment.unassigned + 1);
                    ++stamp;
                    std::vector<int> selected;
                    selected.reserve(count);
                    add_unique_job(selected, marks, stamp, seed);

                    // seedの候補先に現在いる仕事を優先し、容量の使い方を局所的に組み替える
                    for (int id = problem.offset[seed]; id < problem.offset[seed + 1] && (int)selected.size() < count; id++) {
                        const auto& residents = assignment.jobs_of_agent[problem.edges[id].agent];
                        if (residents.empty()) continue;
                        int start = rng.index((int)residents.size());
                        for (int k = 0; k < (int)residents.size() && (int)selected.size() < count; k++) {
                            add_unique_job(selected, marks, stamp, residents[(start + k) % residents.size()]);
                        }
                    }
                    while ((int)selected.size() < count) {
                        int job = rng.index(problem.jobs);
                        if (assignment.chosen_edge[job] >= 0) add_unique_job(selected, marks, stamp, job);
                    }

                    std::vector<int> original_edges;
                    original_edges.reserve(selected.size());
                    for (int job : selected) original_edges.push_back(assignment.chosen_edge[job]);
                    for (int job : selected) remove_job(problem, assignment, job);
                    if (!regret_repair(selected)) {
                        restore_jobs(problem, assignment, selected, original_edges);
                    }
                }
            };
            repair_partial_lns(best);
        }

        // 小規模で構築に失敗した場合は、完全探索で実行可能解を取り逃がさない
        if (best.unassigned > 0 && problem.jobs <= 20 && !time_up(options)) {
            auto exact_small_solve = [&]() -> std::optional<state> {
                constexpr long long node_limit = 3000000;
                state assignment = make_empty_state(problem);
                std::vector<int> jobs(problem.jobs);
                std::iota(jobs.begin(), jobs.end(), 0);
                std::sort(jobs.begin(), jobs.end(), [&](int lhs, int rhs) {
                    int lhs_degree = problem.offset[lhs + 1] - problem.offset[lhs];
                    int rhs_degree = problem.offset[rhs + 1] - problem.offset[rhs];
                    return lhs_degree < rhs_degree;
                });

                // 各仕事の候補を安い順にし、容量を無視したsuffix下界を作る
                std::vector<std::vector<int>> candidates(problem.jobs);
                std::vector<long long> suffix_lower_bound(problem.jobs + 1, 0);
                for (int depth = 0; depth < problem.jobs; depth++) {
                    int job = jobs[depth];
                    for (int id = problem.offset[job]; id < problem.offset[job + 1]; id++) {
                        candidates[depth].push_back(id);
                    }
                    std::sort(candidates[depth].begin(), candidates[depth].end(), [&](int lhs, int rhs) {
                        return problem.edges[lhs].cost < problem.edges[rhs].cost;
                    });
                }
                for (int depth = problem.jobs - 1; depth >= 0; depth--) {
                    if (candidates[depth].empty()) return std::nullopt;
                    suffix_lower_bound[depth] = suffix_lower_bound[depth + 1] +
                                                problem.edges[candidates[depth][0]].cost;
                }

                long long best_cost = std::numeric_limits<long long>::max();
                std::vector<int> current_edges(problem.jobs, -1);
                std::vector<int> best_edges;
                long long nodes = 0;
                auto dfs = [&](auto&& self, int depth, long long cost) -> void {
                    if (++nodes > node_limit) return;
                    if ((nodes & 1023) == 0 && time_up(options)) return;
                    if (best_cost != std::numeric_limits<long long>::max() &&
                        cost + suffix_lower_bound[depth] >= best_cost) return;
                    if (depth == problem.jobs) {
                        best_cost = cost;
                        best_edges = current_edges;
                        return;
                    }
                    for (int id : candidates[depth]) {
                        const edge& candidate = problem.edges[id];
                        if (assignment.load[candidate.agent] + candidate.demand > problem.capacity[candidate.agent]) continue;
                        assignment.load[candidate.agent] += candidate.demand;
                        current_edges[depth] = id;
                        self(self, depth + 1, cost + candidate.cost);
                        assignment.load[candidate.agent] -= candidate.demand;
                        if (nodes > node_limit || time_up(options)) break;
                    }
                };
                dfs(dfs, 0, 0);
                if (best_edges.empty()) return std::nullopt;

                for (int depth = 0; depth < problem.jobs; depth++) {
                    assign_job(problem, assignment, jobs[depth], best_edges[depth]);
                }
                return assignment;
            };
            if (auto exact = exact_small_solve()) best = std::move(*exact);
        }

        // 通常の実行可能化で未割当が残った場合だけ、容量消費を強く価格化して一度構築する
        if (best.unassigned > 0 &&
            options.algorithm >= profile::lagrangian && !time_up(options)) {
            std::vector<double> feasibility_price(problem.agents);
            double scale = (problem.average_cost_range + 1.0) * (problem.jobs + 1.0);
            for (int agent = 0; agent < problem.agents; agent++) {
                feasibility_price[agent] = scale / static_cast<double>(std::max(1LL, problem.capacity[agent]));
            }
            std::vector<double> feasibility_difficulty;
            state candidate = construct(problem, feasibility_price, feasibility_difficulty, rng, 0.0);
            if (better_partial(candidate, best)) best = std::move(candidate);
        }

        // 浮動小数の緩和下界ではなく、候補最小値の整数和で最適性を確認する
        if (best.unassigned == 0 && problem.independent_bound_valid && best.objective == problem.independent_lower_bound) {
            result answer = make_result(problem, best, 0, static_cast<double>(best.objective));
            answer.initial_solution_used = initial_solution_used;
            return answer;
        }
        long long iterations = 0;
        std::vector<int> cost_order;
        if (best.unassigned == 0 &&
            options.algorithm >= profile::local_search && !time_up(options)) {
            // 整数コスト順・同値は候補ID順の索引をrelocateとexchangeで共有する
            cost_order.resize(problem.edges.size());
            std::iota(cost_order.begin(), cost_order.end(), 0);
            for (int job = 0; job < problem.jobs; job++) {
                std::sort(cost_order.begin() + problem.offset[job], cost_order.begin() + problem.offset[job + 1],
                          [&](int lhs, int rhs) {
                    if (problem.edges[lhs].cost != problem.edges[rhs].cost)
                        return problem.edges[lhs].cost < problem.edges[rhs].cost;
                    return lhs < rhs;
                });
            }
            relocate_descent(problem, best, rng, 5, options, cost_order);
            long long attempts = std::min(
                1000000LL, std::max(1000LL, (long long)problem.edges.size() * 3));
            auto exchange_descent = [&](state& assignment) {
                if (problem.jobs < 2) return;
                for (long long iteration = 0; iteration < attempts; iteration++) {
                    if ((iteration & 255) == 0 && time_up(options)) return;
                    int first = rng.index(problem.jobs);
                    int first_old_id = assignment.chosen_edge[first];
                    int candidate_count = problem.offset[first + 1] - problem.offset[first];
                    int first_new_id = problem.offset[first] + rng.index(candidate_count);
                    // 一様に4候補を引き、その中の最安候補から2仕事の組替を試す。
                    for (int sample = 0; sample < 3; sample++) {
                        int id = problem.offset[first] + rng.index(candidate_count);
                        if (problem.edges[id].cost < problem.edges[first_new_id].cost) first_new_id = id;
                    }
                    const edge& first_old = problem.edges[first_old_id];
                    const edge& first_new = problem.edges[first_new_id];
                    if (first_new.agent == first_old.agent) continue;
                    const auto& residents = assignment.jobs_of_agent[first_new.agent];
                    if (residents.empty()) continue;
                    int second = residents[rng.index((int)residents.size())];
                    int second_old_id = assignment.chosen_edge[second];
                    const edge& second_old = problem.edges[second_old_id];

                    // 2仕事を外し、最初の仕事を移した後の容量で、相手候補を整数コスト順に調べる。
                    int second_new_id = -1;
                    if (assignment.load[first_new.agent] - second_old.demand + first_new.demand >
                        problem.capacity[first_new.agent]) continue;
                    for (int pos = problem.offset[second]; pos < problem.offset[second + 1]; pos++) {
                        int id = cost_order[pos];
                        const edge& candidate = problem.edges[id];
                        // この候補で改善不能なら、以降の高コスト候補も改善不能。
                        if (static_cast<__int128>(first_new.cost) + candidate.cost >=
                            static_cast<__int128>(first_old.cost) + second_old.cost) break;
                        long long load = assignment.load[candidate.agent];
                        if (candidate.agent == first_old.agent) load -= first_old.demand;
                        if (candidate.agent == second_old.agent) load -= second_old.demand;
                        if (candidate.agent == first_new.agent) load += first_new.demand;
                        if (candidate.demand > problem.capacity[candidate.agent] - load) continue;
                        second_new_id = id;
                        break;
                    }
                    if (second_new_id < 0) continue;

                    // 2仕事を一度外してから入れ直し、中間状態の一時的な容量超過を避ける
                    remove_job(problem, assignment, first);
                    remove_job(problem, assignment, second);
                    assign_job(problem, assignment, first, first_new_id);
                    assign_job(problem, assignment, second, second_new_id);
                }
            };
            exchange_descent(best);
            relocate_descent(problem, best, rng, 2, options, cost_order);
        }

        if (best.unassigned == 0 &&
            options.algorithm >= profile::alns && !time_up(options)) {
            auto run_alns = [](const prepared_problem& alns_problem, state& alns_best,
                               const std::vector<double>& alns_price, fast_rng& alns_rng,
                               const solve_options& alns_options, long long& alns_iterations,
                               const std::vector<int>& alns_cost_order) {
                if (alns_problem.jobs == 0 || alns_options.iteration_limit <= 0) return;
                state current = alns_best;
                std::vector<int> marks(alns_problem.jobs, 0);
                int stamp = 0;
                int maximum_ruin = std::min(
                    std::max(4, alns_options.maximum_ruin_jobs), std::max(4, alns_problem.jobs / 20));
                double price_weight = alns_options.repair_price_weight;
                std::vector<int> repair_order(alns_problem.edges.size());
                std::iota(repair_order.begin(), repair_order.end(), 0);

                /*
                 * regret repairの評価値は1回のsolve中で不変なので、仕事ごとに一度だけ整列する。
                 * repair中は実行可能な先頭2候補を見つければ、最良値とregretを確定できる。
                 */
                for (int job = 0; job < alns_problem.jobs; job++) {
                    auto first = repair_order.begin() + alns_problem.offset[job];
                    auto last = repair_order.begin() + alns_problem.offset[job + 1];
                    std::sort(first, last, [&](int lhs, int rhs) {
                        const edge& left = alns_problem.edges[lhs];
                        const edge& right = alns_problem.edges[rhs];
                        double left_score = static_cast<double>(left.cost) + price_weight *
                                            alns_price[left.agent] * static_cast<double>(left.demand);
                        double right_score = static_cast<double>(right.cost) + price_weight *
                                             alns_price[right.agent] * static_cast<double>(right.demand);
                        if (left_score != right_score) return left_score < right_score;
                        return lhs < rhs;
                    });
                }
                struct repair_workspace {
                    struct watcher_entry {
                        int selected_id;
                        int version;
                    };

                    std::vector<double> job_noise;
                    std::vector<std::array<int, 2>> top_edges;
                    std::vector<double> regret;
                    std::vector<int> version;
                    std::vector<std::vector<watcher_entry>> watchers;
                    std::vector<int> touched_agents;
                };
                repair_workspace workspace;
                auto regret_repair_incremental = [](const prepared_problem& input, state& assignment,
                                                    const std::vector<int>& selected,
                                                    const std::vector<double>& prices, fast_rng& random,
                                                    double weight, const std::vector<int>& ranking,
                                                    repair_workspace& scratch) {
                    int selected_count = (int)selected.size();
                    scratch.job_noise.resize(selected_count);
                    scratch.top_edges.resize(selected_count);
                    scratch.regret.resize(selected_count);
                    scratch.version.assign(selected_count, 0);
                    if ((int)scratch.watchers.size() != input.agents) {
                        scratch.watchers.assign(input.agents, {});
                        scratch.touched_agents.clear();
                    } else {
                        for (int agent : scratch.touched_agents) scratch.watchers[agent].clear();
                        scratch.touched_agents.clear();
                    }
                    for (double& value : scratch.job_noise) value = random.unit() - 0.5;

                    /*
                     * repair中はloadが増えるだけなので、ある仕事の上位2候補が変わるのは、
                     * 直前にloadを増やした割当先の候補が実行不能になった場合だけである。
                     * 上位候補を監視する仕事だけ再走査し、それ以外のregretは再利用する。
                     */
                    auto recompute = [&](int selected_id) {
                        int job = selected[selected_id];
                        scratch.top_edges[selected_id].fill(-1);
                        std::array<double, 2> top_scores;
                        int found = 0;
                        for (int pos = input.offset[job]; pos < input.offset[job + 1]; pos++) {
                            int id = ranking[pos];
                            if (!fits(input, assignment, job, id)) continue;
                            const edge& candidate = input.edges[id];
                            double score = static_cast<double>(candidate.cost) + weight *
                                           prices[candidate.agent] * static_cast<double>(candidate.demand);
                            scratch.top_edges[selected_id][found] = id;
                            top_scores[found] = score;
                            if (++found == 2) break;
                        }
                        if (found == 0) return false;

                        double value = found == 2 ? top_scores[1] - top_scores[0] :
                                                    input.average_cost_range * 8;
                        value += input.average_cost_range * 0.03 *
                                 scratch.job_noise[selected_id];
                        scratch.regret[selected_id] = value;

                        int version = ++scratch.version[selected_id];
                        for (int rank = 0; rank < 2; rank++) {
                            int id = scratch.top_edges[selected_id][rank];
                            if (id < 0) continue;
                            int agent = input.edges[id].agent;
                            if (rank == 1) {
                                int first_id = scratch.top_edges[selected_id][0];
                                if (input.edges[first_id].agent == agent) continue;
                            }
                            auto& watchers = scratch.watchers[agent];
                            if (watchers.empty()) scratch.touched_agents.push_back(agent);
                            watchers.push_back({selected_id, version});
                        }
                        return true;
                    };

                    for (int selected_id = 0; selected_id < selected_count; selected_id++) {
                        if (!recompute(selected_id)) return false;
                    }

                    for (int remaining = selected_count; remaining > 0; remaining--) {
                        int chosen_id = -1;
                        double largest_regret = -std::numeric_limits<double>::infinity();
                        for (int selected_id = 0; selected_id < selected_count; selected_id++) {
                            if (scratch.regret[selected_id] > largest_regret) {
                                largest_regret = scratch.regret[selected_id];
                                chosen_id = selected_id;
                            }
                        }
                        if (chosen_id < 0) return false;

                        int chosen_edge = scratch.top_edges[chosen_id][0];
                        // 割当済みの仕事を最大regret選択から除き、状態配列への参照を省く
                        scratch.regret[chosen_id] = -std::numeric_limits<double>::infinity();
                        int changed_agent = input.edges[chosen_edge].agent;
                        assign_job(input, assignment, selected[chosen_id], chosen_edge);

                        auto& watchers = scratch.watchers[changed_agent];
                        int watcher_count = (int)watchers.size();
                        for (int watcher_id = 0; watcher_id < watcher_count; watcher_id++) {
                            auto watcher = watchers[watcher_id];
                            int selected_id = watcher.selected_id;
                            int job = selected[selected_id];
                            if (assignment.chosen_edge[job] >= 0 ||
                                scratch.version[selected_id] != watcher.version) continue;
                            bool invalidated = false;
                            for (int rank = 0; rank < 2; rank++) {
                                int id = scratch.top_edges[selected_id][rank];
                                if (id >= 0 && input.edges[id].agent == changed_agent &&
                                    !fits(input, assignment, job, id)) {
                                    invalidated = true;
                                }
                            }
                            if (invalidated && !recompute(selected_id)) return false;
                        }
                    }
                    return true;
                };
                auto select_ruin_jobs = [](const prepared_problem& input, const state& assignment,
                                           fast_rng& random, int count, int operation,
                                           std::vector<int>& seen, int generation) {
                    std::vector<int> selected;
                    selected.reserve(count);

                    if (operation == 1) {
                        // 安い候補先とその所属仕事をたどり、複数容量の組替を連鎖的に促す。
                        add_unique_job(selected, seen, generation, random.index(input.jobs));
                        for (int cursor = 0; cursor < static_cast<int>(selected.size()) &&
                             static_cast<int>(selected.size()) < count; cursor++) {
                            int job = selected[cursor];
                            int old_id = assignment.chosen_edge[job];
                            int degree = input.offset[job + 1] - input.offset[job];
                            int first = random.index(degree);
                            for (int k = 0; k < degree && static_cast<int>(selected.size()) < count; k++) {
                                const edge& candidate = input.edges[input.offset[job] + (first + k) % degree];
                                if (candidate.cost >= input.edges[old_id].cost) continue;
                                const auto& residents = assignment.jobs_of_agent[candidate.agent];
                                if (!residents.empty()) add_unique_job(selected, seen, generation,
                                    residents[random.index(static_cast<int>(residents.size()))]);
                            }
                        }
                    }

                    // 指定数に足りない分は一様ランダムに補う
                    while ((int)selected.size() < count) {
                        add_unique_job(selected, seen, generation, random.index(input.jobs));
                    }
                    return selected;
                };

                for (; alns_iterations < alns_options.iteration_limit; alns_iterations++) {
                    if (alns_problem.independent_bound_valid && alns_best.objective == alns_problem.independent_lower_bound) break;
                    if ((alns_iterations & 63) == 0 && time_up(alns_options)) break;
                    int count = 4;
                    if (maximum_ruin > 4) count += alns_rng.index(maximum_ruin - 3);
                    count = std::min(count, alns_problem.jobs);
                    int operation = alns_rng.index(2);
                    if (++stamp == std::numeric_limits<int>::max()) {
                        std::fill(marks.begin(), marks.end(), 0);
                        stamp = 1;
                    }
                    std::vector<int> selected = select_ruin_jobs(
                        alns_problem, current, alns_rng, count, operation, marks, stamp);
                    std::vector<int> original_edges;
                    original_edges.reserve(selected.size());
                    for (int job : selected) original_edges.push_back(current.chosen_edge[job]);
                    long long old_cost = current.objective;
                    for (int job : selected) remove_job(alns_problem, current, job);

                    bool repaired = regret_repair_incremental(
                        alns_problem, current, selected, alns_price, alns_rng, price_weight, repair_order, workspace);
                    if (!repaired) {
                        restore_jobs(alns_problem, current, selected, original_edges);
                        continue;
                    }

                    // 改善解を常に受理し、悪化解も温度に応じて少量受理して局所解から脱出する
                    long long delta = current.objective - old_cost;
                    double cooling = 1.0 / (1.0 + static_cast<double>(alns_iterations) * 0.0005);
                    double temperature = alns_problem.average_cost_range * std::sqrt((double)count) *
                                         (alns_options.alns_temperature * cooling + 0.002);
                    bool accept = delta <= 0 || alns_rng.unit() < std::exp(-(double)delta / temperature);
                    if (!accept) {
                        restore_jobs(alns_problem, current, selected, original_edges);
                    } else if (current.objective < alns_best.objective) {
                        alns_best = current;
                    }

                    if (alns_options.local_search_interval > 0 &&
                        alns_iterations % alns_options.local_search_interval == alns_options.local_search_interval - 1) {
                        relocate_descent(alns_problem, current, alns_rng, 1, alns_options, alns_cost_order);
                        if (current.objective < alns_best.objective) alns_best = current;
                    }

                }
            };
            run_alns(problem, best, price, rng, options, iterations, cost_order);
        }
        if (best.unassigned == 0 && problem.independent_bound_valid && best.objective == problem.independent_lower_bound) {
            lower_bound = static_cast<double>(best.objective);
        }
        result answer = make_result(problem, best, iterations, lower_bound);
        answer.initial_solution_used = initial_solution_used;
        return answer;
    }
};

class generalized_assignment_solver::session {
private:
    friend struct generalized_assignment_solver;

    session() = default;

public:
    // 仕事数を返す / O(1)
    int job_count() const { return problem_.jobs; }
    // 割当先数を返す / O(1)
    int agent_count() const { return problem_.agents; }
    // 登録済み候補数を返す / O(1)
    int option_count() const { return (int)problem_.edges.size(); }
    // 現在の容量列への参照を返す / O(1)
    const std::vector<long long>& capacities() const { return problem_.capacity; }

    // 元の候補IDに対応する現在のコストを返す / O(1)
    long long option_cost(int option_id) const {
        assert(0 <= option_id && option_id < (int)problem_.raw_to_edge.size());
        if (option_id < 0 || option_id >= (int)problem_.raw_to_edge.size()) return 0;
        return problem_.edges[problem_.raw_to_edge[option_id]].cost;
    }

    // 現在のコスト・容量で探索し、必要なら統計を再計算する / O(E + T)
    result solve(const solve_options& options) {
        ensure_statistics();
        return solve_prepared(problem_, nullptr, options);
    }

    // 有効な完全解を初期解として改善する / O(E + N + T)
    result improve(std::span<const int> option_ids, const solve_options& options) {
        ensure_statistics();
        std::vector<int> initial_edges(problem_.jobs, -1);
        if ((int)option_ids.size() == problem_.jobs) {
            for (int job = 0; job < problem_.jobs; job++) {
                initial_edges[job] = decode_option(job, option_ids[job]);
            }
        }
        return solve_prepared(problem_, &initial_edges, options);
    }

    // 可変仕事だけを残余問題化して修復・改善し、固定仕事は変更しない / O(N + A + Es + Ts)
    result repair(std::span<const int> option_ids,
                  std::span<const int> mutable_jobs,
                  const solve_options& options) {
        if ((int)option_ids.size() != problem_.jobs) {
            result answer = blank_result();
            answer.repair_state = repair_status::invalid_input;
            return answer;
        }

        std::vector<unsigned char> is_mutable(problem_.jobs, 0);
        for (int job : mutable_jobs) {
            if (job < 0 || job >= problem_.jobs || is_mutable[job]) {
                result answer = blank_result();
                answer.repair_state = repair_status::invalid_input;
                return answer;
            }
            is_mutable[job] = 1;
        }

        // 固定仕事だけを先に評価し、その負荷を残余容量から差し引く。
        result answer = blank_result();
        answer.unassigned_count = (int)mutable_jobs.size();
        bool fixed_input_valid = true;
        for (int job = 0; job < problem_.jobs; job++) {
            if (is_mutable[job]) continue;
            int edge_id = decode_option(job, option_ids[job]);
            if (edge_id < 0) {
                fixed_input_valid = false;
                answer.unassigned_count++;
                continue;
            }
            const edge& candidate = problem_.edges[edge_id];
            answer.option_ids[job] = candidate.raw_id;
            answer.agent_of_job[job] = candidate.agent;
            answer.load_of_agent[candidate.agent] += candidate.demand;
            answer.objective += candidate.cost;
        }
        finish_overflow(answer);
        if (!fixed_input_valid) {
            answer.repair_state = repair_status::invalid_input;
            return answer;
        }

        if (answer.total_overflow != 0) {
            answer.repair_state = repair_status::fixed_part_infeasible;
            return answer;
        }

        if (mutable_jobs.empty()) {
            answer.unassigned_count = 0;
            answer.initial_solution_used = true;
            answer.lower_bound = (double)answer.objective;
            answer.repair_state = repair_status::completed;
            return answer;
        }

        // 可変仕事だけからなる残余問題を、前処理済みedge列から線形時間で構築する。
        prepared_problem subproblem;
        subproblem.jobs = (int)mutable_jobs.size();
        subproblem.agents = problem_.agents;
        subproblem.capacity.resize(problem_.agents);
        for (int agent = 0; agent < problem_.agents; agent++) {
            subproblem.capacity[agent] = problem_.capacity[agent] - answer.load_of_agent[agent];
        }
        subproblem.offset.assign(subproblem.jobs + 1, 0);
        for (int local_job = 0; local_job < subproblem.jobs; local_job++) {
            int global_job = mutable_jobs[local_job];
            int degree = problem_.offset[global_job + 1] - problem_.offset[global_job];
            subproblem.offset[local_job + 1] = subproblem.offset[local_job] + degree;
        }
        subproblem.edges.reserve(subproblem.offset.back());
        for (int global_job : mutable_jobs) {
            subproblem.edges.insert(
                subproblem.edges.end(),
                problem_.edges.begin() + problem_.offset[global_job],
                problem_.edges.begin() + problem_.offset[global_job + 1]);
        }
        refresh_statistics(subproblem);

        std::vector<int> initial_edges(subproblem.jobs, -1);
        for (int local_job = 0; local_job < subproblem.jobs; local_job++) {
            int global_job = mutable_jobs[local_job];
            int global_edge = decode_option(global_job, option_ids[global_job]);
            if (global_edge < 0) continue;
            initial_edges[local_job] = subproblem.offset[local_job] +
                (global_edge - problem_.offset[global_job]);
        }

        result repaired = solve_prepared(subproblem, &initial_edges, options);
        for (int local_job = 0; local_job < subproblem.jobs; local_job++) {
            int global_job = mutable_jobs[local_job];
            answer.option_ids[global_job] = repaired.option_ids[local_job];
            answer.agent_of_job[global_job] = repaired.agent_of_job[local_job];
        }
        for (int agent = 0; agent < problem_.agents; agent++) {
            answer.load_of_agent[agent] += repaired.load_of_agent[agent];
        }
        answer.objective += repaired.objective;
        answer.unassigned_count = repaired.unassigned_count;
        answer.iterations = repaired.iterations;
        answer.lower_bound = std::isfinite(repaired.lower_bound)
            ? repaired.lower_bound + (double)(answer.objective - repaired.objective)
            : repaired.lower_bound;
        answer.initial_solution_used = repaired.initial_solution_used;
        answer.repair_state = repair_status::completed;
        return answer;
    }

    // sessionで差し替えたコスト・容量を使って評価する / O(N + A)
    result evaluate(std::span<const int> option_ids) const {
        result answer = blank_result();
        if ((int)option_ids.size() != problem_.jobs) return answer;
        answer.unassigned_count = 0;
        for (int job = 0; job < problem_.jobs; job++) {
            int edge_id = decode_option(job, option_ids[job]);
            if (edge_id < 0) {
                answer.unassigned_count++;
                continue;
            }
            const edge& candidate = problem_.edges[edge_id];
            answer.option_ids[job] = candidate.raw_id;
            answer.agent_of_job[job] = candidate.agent;
            answer.load_of_agent[candidate.agent] += candidate.demand;
            answer.objective += candidate.cost;
        }
        finish_overflow(answer);
        return answer;
    }

    // 元solverの候補IDでコストを差し替える。コストの符号制限はない / O(1)
    void set_option_cost(int option_id, long long cost) {
        assert(0 <= option_id && option_id < (int)problem_.raw_to_edge.size());
        if (option_id < 0 || option_id >= (int)problem_.raw_to_edge.size()) return;
        problem_.edges[problem_.raw_to_edge[option_id]].cost = cost;
        statistics_dirty_ = true;
    }

    // 元solverの候補ID順で全コストを一括差し替えする / O(E)
    void set_option_costs(std::span<const long long> costs) {
        assert(costs.size() == problem_.raw_to_edge.size());
        if (costs.size() != problem_.raw_to_edge.size()) return;
        for (int option_id = 0; option_id < (int)costs.size(); option_id++) {
            problem_.edges[problem_.raw_to_edge[option_id]].cost = costs[option_id];
        }
        statistics_dirty_ = true;
    }

    // session生成時のコストへ戻す / O(E)
    void reset_costs() {
        set_option_costs(original_cost_);
    }

    // 指定した割当先の容量を差し替える / O(1)
    void set_capacity(int agent, long long capacity) {
        assert(0 <= agent && agent < problem_.agents);
        assert(capacity >= 0);
        if (agent < 0 || agent >= problem_.agents || capacity < 0) return;
        problem_.capacity[agent] = capacity;
        statistics_dirty_ = true;
    }

    // 全割当先の容量を一括差し替えする / O(A)
    void set_capacities(std::span<const long long> capacities) {
        assert((int)capacities.size() == problem_.agents);
        if ((int)capacities.size() != problem_.agents) return;
        bool valid = std::all_of(capacities.begin(), capacities.end(), [](long long value) {
            return value >= 0;
        });
        assert(valid);
        if (!valid) return;
        problem_.capacity.assign(capacities.begin(), capacities.end());
        statistics_dirty_ = true;
    }

    // session生成時の容量へ戻す / O(A)
    void reset_capacities() {
        problem_.capacity = original_capacity_;
        statistics_dirty_ = true;
    }

private:
    prepared_problem problem_;
    std::vector<long long> original_capacity_;
    std::vector<long long> original_cost_;
    bool statistics_dirty_ = false;

    void ensure_statistics() {
        if (!statistics_dirty_) return;
        refresh_statistics(problem_);
        statistics_dirty_ = false;
    }

    int decode_option(int job, int raw_id) const {
        if (raw_id < 0 || raw_id >= (int)problem_.raw_to_edge.size()) return -1;
        int edge_id = problem_.raw_to_edge[raw_id];
        return problem_.offset[job] <= edge_id && edge_id < problem_.offset[job + 1]
            ? edge_id : -1;
    }

    result blank_result() const {
        return {.option_ids = std::vector<int>(problem_.jobs, -1),
                .agent_of_job = std::vector<int>(problem_.jobs, -1),
                .load_of_agent = std::vector<long long>(problem_.agents, 0),
                .unassigned_count = problem_.jobs};
    }

    void finish_overflow(result& answer) const {
        answer.total_overflow = 0;
        for (int agent = 0; agent < problem_.agents; agent++) {
            answer.total_overflow += std::max(
                0LL, answer.load_of_agent[agent] - problem_.capacity[agent]);
        }
    }
};

inline generalized_assignment_solver::session generalized_assignment_solver::make_session() const {
    session prepared;
    prepare(prepared.problem_);
    prepared.original_capacity_ = prepared.problem_.capacity;
    prepared.original_cost_.resize(option_count());
    for (const edge& candidate : prepared.problem_.edges) {
        prepared.original_cost_[candidate.raw_id] = candidate.cost;
    }
    return prepared;
}

inline generalized_assignment_solver::result generalized_assignment_solver::solve(
        const solve_options& options) const {
    prepared_problem problem;
    prepare(problem);
    return solve_prepared(problem, nullptr, options);
}

inline generalized_assignment_solver::result generalized_assignment_solver::improve(
        std::span<const int> option_ids, const solve_options& options) const {
    prepared_problem problem;
    prepare(problem);
    std::vector<int> initial_edges(job_count_, -1);
    if ((int)option_ids.size() == job_count_) {
        for (int job = 0; job < job_count_; job++) {
            int raw_id = option_ids[job];
            if (raw_id < 0 || raw_id >= (int)problem.raw_to_edge.size()) continue;
            int edge_id = problem.raw_to_edge[raw_id];
            if (problem.offset[job] <= edge_id && edge_id < problem.offset[job + 1]) {
                initial_edges[job] = edge_id;
            }
        }
    }
    return solve_prepared(problem, &initial_edges, options);
}

inline generalized_assignment_solver::result generalized_assignment_solver::repair(
        std::span<const int> option_ids,
        std::span<const int> mutable_jobs,
        const solve_options& options) const {
    session prepared = make_session();
    return prepared.repair(option_ids, mutable_jobs, options);
}

inline generalized_assignment_solver::result generalized_assignment_solver::evaluate(
        std::span<const int> option_ids) const {
    result answer{.option_ids = std::vector<int>(job_count_, -1),
                  .agent_of_job = std::vector<int>(job_count_, -1),
                  .load_of_agent = std::vector<long long>(agent_count_, 0),
                  .unassigned_count = job_count_};
    if ((int)option_ids.size() != job_count_) return answer;

    answer.unassigned_count = 0;
    for (int job = 0; job < job_count_; job++) {
        int id = option_ids[job];
        if (id < 0 || id >= (int)options_.size() || options_[id].job != job) {
            answer.unassigned_count++;
            continue;
        }
        const assignment_option& candidate = options_[id];
        answer.option_ids[job] = id;
        answer.agent_of_job[job] = candidate.agent;
        answer.load_of_agent[candidate.agent] += candidate.demand;
        answer.objective += candidate.cost;
    }
    for (int agent = 0; agent < agent_count_; agent++) {
        answer.total_overflow += std::max(
            0LL, answer.load_of_agent[agent] - capacities_[agent]);
    }
    return answer;
}

#if __INCLUDE_LEVEL__ == 0
int main() {
    auto exact_cost = [&](const generalized_assignment_solver& solver) {
        const auto& options = solver.options();
        int jobs = solver.job_count();
        int agents = solver.agent_count();
        std::vector<std::vector<int>> by_job(jobs);
        for (int id = 0; id < (int)options.size(); id++) by_job[options[id].job].push_back(id);
        std::vector<long long> load(agents, 0);
        long long best = std::numeric_limits<long long>::max();
        auto dfs = [&](auto&& self, int job, long long cost) -> void {
            // 負コストも受け付けるため、途中のコストだけでは枝刈りしない
            if (job == jobs) {
                best = std::min(best, cost);
                return;
            }
            for (int id : by_job[job]) {
                const auto& option = options[id];
                if (load[option.agent] + option.demand > solver.capacities()[option.agent]) continue;
                load[option.agent] += option.demand;
                self(self, job + 1, cost + option.cost);
                load[option.agent] -= option.demand;
            }
        };
        dfs(dfs, 0, 0);
        return best;
    };
    auto check_result = [&](const generalized_assignment_solver& solver,
                             const generalized_assignment_solver::result& result) {
        assert((int)result.option_ids.size() == solver.job_count());
        assert((int)result.load_of_agent.size() == solver.agent_count());
        std::vector<long long> load(solver.agent_count(), 0);
        long long cost = 0;
        int unassigned = 0;
        for (int job = 0; job < solver.job_count(); job++) {
            int id = result.option_ids[job];
            if (id < 0) {
                unassigned++;
                continue;
            }
            assert(id < solver.option_count());
            const auto& option = solver.options()[id];
            assert(option.job == job);
            assert(result.agent_of_job[job] == option.agent);
            load[option.agent] += option.demand;
            cost += option.cost;
        }
        long long overflow = 0;
        for (int agent = 0; agent < solver.agent_count(); agent++) {
            overflow += std::max(0LL, load[agent] - solver.capacities()[agent]);
            assert(load[agent] == result.load_of_agent[agent]);
        }
        assert(cost == result.objective);
        assert(unassigned == result.unassigned_count);
        assert(overflow == result.total_overflow);
    };

    // fixed_tests
    {
        generalized_assignment_solver solver(4, 2, {5, 5});
        solver.add_option(0, 0, 3, 1);
        solver.add_option(0, 1, 2, 8);
        solver.add_option(1, 0, 3, 2);
        solver.add_option(1, 1, 2, 7);
        solver.add_option(2, 0, 2, 2);
        solver.add_option(2, 1, 3, 5);
        solver.add_option(3, 0, 1, 3);
        solver.add_option(3, 1, 2, 4);

        for (int mode = 0; mode <= 4; mode++) {
            generalized_assignment_solver::solve_options options;
            options.algorithm = (generalized_assignment_solver::profile)mode;
            options.iteration_limit = 2000;
            options.seed = 100 + (unsigned)mode;
            auto result = solver.solve(options);
            check_result(solver, result);
            assert(result.feasible());
        }

        auto result = solver.solve({});
        auto evaluated = solver.evaluate(result.option_ids);
        check_result(solver, evaluated);
        assert(evaluated.objective == result.objective);

        std::vector<int> mutable_jobs = {0, 1};
        std::vector<int> partial = result.option_ids;
        partial[0] = -1;
        partial[1] = -1;
        generalized_assignment_solver::solve_options repair_options;
        repair_options.algorithm = generalized_assignment_solver::profile::local_search;
        repair_options.iteration_limit = 2000;
        auto repaired = solver.repair(partial, mutable_jobs, repair_options);
        check_result(solver, repaired);
        assert(repaired.feasible());
        assert(repaired.repair_state == generalized_assignment_solver::repair_status::completed);
        assert(repaired.option_ids[2] == result.option_ids[2]);
        assert(repaired.option_ids[3] == result.option_ids[3]);

        auto non_worsening = solver.repair(result.option_ids, mutable_jobs, repair_options);
        check_result(solver, non_worsening);
        assert(non_worsening.feasible());
        assert(non_worsening.initial_solution_used);
        assert(non_worsening.objective <= result.objective);

        std::vector<int> invalid_fixed = result.option_ids;
        invalid_fixed[2] = -1;
        auto invalid_repair = solver.repair(invalid_fixed, mutable_jobs, repair_options);
        assert(invalid_repair.repair_state == generalized_assignment_solver::repair_status::invalid_input);
        assert(!invalid_repair.feasible());

        std::vector<int> duplicate_mutable = {0, 0};
        auto duplicate_repair = solver.repair(result.option_ids, duplicate_mutable, repair_options);
        assert(duplicate_repair.repair_state ==
               generalized_assignment_solver::repair_status::invalid_input);
        assert(duplicate_repair.unassigned_count == solver.job_count());
        for (auto invalid_mutable : {std::vector<int>{-1}, std::vector<int>{solver.job_count()}}) {
            assert(solver.repair(result.option_ids, invalid_mutable, repair_options).repair_state ==
                   generalized_assignment_solver::repair_status::invalid_input);
        }
        auto wrong_size = result.option_ids;
        wrong_size.pop_back();
        assert(solver.repair(wrong_size, mutable_jobs, repair_options).repair_state ==
               generalized_assignment_solver::repair_status::invalid_input);
        auto wrong_owner = result.option_ids;
        wrong_owner[2] = result.option_ids[3];
        assert(solver.repair(wrong_owner, mutable_jobs, repair_options).repair_state ==
               generalized_assignment_solver::repair_status::invalid_input);
        assert(solver.evaluate(wrong_owner).unassigned_count == 1);

        auto prepared = solver.make_session();
        auto session_evaluated = prepared.evaluate(result.option_ids);
        assert(session_evaluated.objective == result.objective);
        long long old_cost = prepared.option_cost(result.option_ids[0]);
        prepared.set_option_cost(result.option_ids[0], old_cost + 1000);
        auto cost_changed = prepared.evaluate(result.option_ids);
        assert(cost_changed.objective == result.objective + 1000);
        prepared.reset_costs();
        assert(prepared.evaluate(result.option_ids).objective == result.objective);

        int fixed_agent = solver.options()[result.option_ids[2]].agent;
        prepared.set_capacity(fixed_agent, 0);
        auto fixed_infeasible = prepared.repair(result.option_ids, mutable_jobs, repair_options);
        assert(fixed_infeasible.repair_state ==
               generalized_assignment_solver::repair_status::fixed_part_infeasible);
        assert(!fixed_infeasible.feasible());
        prepared.reset_capacities();
        assert(prepared.repair(result.option_ids, mutable_jobs, repair_options).feasible());

        generalized_assignment_solver impossible(1, 1, {3});
        impossible.add_option(0, 0, 4, 1);
        auto no_solution = impossible.solve({});
        check_result(impossible, no_solution);
        assert(!no_solution.feasible());

        generalized_assignment_solver no_agents(2, 0, {});
        auto empty_result = no_agents.solve({});
        check_result(no_agents, empty_result);
        assert(empty_result.unassigned_count == 2);
    }

    // random_tests
    {
        std::mt19937 rng(123456789);
        for (int tc = 0; tc < 400; tc++) {
            int jobs = 3 + (int)(rng() % 6);
            int agents = 2 + (int)(rng() % 3);
            std::vector<int> planted(jobs);
            std::vector<long long> capacity(agents, 0);
            generalized_assignment_solver solver(jobs, agents, capacity);

            std::vector<generalized_assignment_solver::assignment_option> pending;
            for (int job = 0; job < jobs; job++) {
                planted[job] = (int)(rng() % (unsigned)agents);
                for (int agent = 0; agent < agents; agent++) {
                    long long demand = 1 + (long long)(rng() % 5);
                    long long cost = 1 + (long long)(rng() % 30);
                    pending.push_back({job, agent, demand, cost});
                    if (agent == planted[job]) capacity[agent] += demand;
                }
            }
            solver.init(jobs, agents, capacity);
            for (const auto& option : pending) {
                solver.add_option(option.job, option.agent, option.demand, option.cost);
            }

            long long optimum = exact_cost(solver);
            assert(optimum != std::numeric_limits<long long>::max());
            for (int mode = 0; mode <= 4; mode++) {
                generalized_assignment_solver::solve_options options;
                options.algorithm = (generalized_assignment_solver::profile)mode;
                options.iteration_limit = 1000;
                options.seed = ((std::uint64_t)tc << 8) + (unsigned)mode;
                auto result = solver.solve(options);
                check_result(solver, result);
                assert(result.feasible());
                assert(result.objective >= optimum);
            }

            generalized_assignment_solver::solve_options repair_options;
            repair_options.algorithm = generalized_assignment_solver::profile::local_search;
            repair_options.iteration_limit = 300;
            repair_options.seed = 0x9e3779b9ULL + (unsigned)tc;
            auto base = solver.solve(repair_options);
            assert(base.feasible());
            std::vector<int> order(jobs);
            std::iota(order.begin(), order.end(), 0);
            std::shuffle(order.begin(), order.end(), rng);
            order.resize(1 + (int)(rng() % (unsigned)jobs));
            std::vector<unsigned char> mutable_mask(jobs, 0);
            std::vector<int> damaged = base.option_ids;
            for (int job : order) {
                mutable_mask[job] = 1;
                if ((rng() & 1U) != 0) damaged[job] = -1;
            }
            auto repaired = solver.make_session().repair(damaged, order, repair_options);
            check_result(solver, repaired);
            assert(repaired.feasible());
            for (int job = 0; job < jobs; job++) {
                if (!mutable_mask[job]) assert(repaired.option_ids[job] == base.option_ids[job]);
            }
            (void)optimum;
        }
    }

    // signed_and_boundary_tests
    {
        using solver_type = generalized_assignment_solver;
        solver_type signed_problem(2, 2, {1, 1});
        signed_problem.add_option(0, 0, 1, 0);
        signed_problem.add_option(0, 1, 1, 10);
        signed_problem.add_option(1, 0, 1, -1000);
        signed_problem.add_option(1, 1, 1, 0);
        // 途中コストだけの枝刈りでは-990を取り逃がす回帰例
        assert(exact_cost(signed_problem) == -990);

        std::mt19937_64 rng(0x16a7928fULL);
        for (int tc = 0; tc < 200; tc++) {
            int jobs = 2 + tc % 5;
            solver_type solver(jobs, 3, std::vector<long long>(3, jobs * 3));
            for (int job = 0; job < jobs; job++) {
                for (int agent = 0; agent < 3; agent++) {
                    for (int mode = 0; mode < 2; mode++) {
                        solver.add_option(job, agent, static_cast<long long>(rng() % 4),
                                          static_cast<long long>(rng() % 101) - 50);
                    }
                }
            }
            long long optimum = exact_cost(solver);
            for (int mode = 0; mode < 5; mode++) {
                solver_type::solve_options options;
                options.algorithm = static_cast<solver_type::profile>(mode);
                options.seed = tc % 2 == 0 ? 0 : ULLONG_MAX;
                options.iteration_limit = tc % 3 == 0 ? 0 : 100;
                options.maximum_ruin_jobs = tc % 2 == 0 ? 1 : 200;
                options.local_search_interval = tc % 2 == 0 ? 0 : 1;
                auto result = solver.solve(options);
                check_result(solver, result);
                assert(result.feasible() && result.objective >= optimum);
                auto prepared = solver.make_session();
                auto copied = prepared;
                assert(copied.solve(options).option_ids == result.option_ids);

                // 期限切れでも、有効な初期解は保持される
                options.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
                auto preserved = prepared.improve(result.option_ids, options);
                assert(preserved.initial_solution_used);
                assert(preserved.option_ids == result.option_ids);
                auto unchanged = prepared.repair(result.option_ids, {}, options);
                assert(unchanged.option_ids == result.option_ids);
                assert(unchanged.repair_state == solver_type::repair_status::completed);
            }
            (void)optimum;
        }
    }

    // mode_and_certificate_tests
    {
        using S = generalized_assignment_solver;
        // 21仕事なので小規模fallbackには頼れず、既存仕事の省容量モードへの変更が必要
        S modes(21, 1, {21});
        modes.add_option(0, 0, 2, 0);
        int light = modes.add_option(0, 0, 1, 1000000);
        for (int j = 1; j < 21; j++) modes.add_option(j, 0, 1, 0);
        S::solve_options o; o.algorithm = S::profile::greedy; o.iteration_limit = 0;
        auto r = modes.solve(o);
        check_result(modes,r);
        assert(r.feasible() && r.option_ids[0] == light);

        S independent(40, 2, {40,40});
        std::vector<int> cheapest;
        long long sum = 0;
        for (int j = 0; j < 40; j++) {
            cheapest.push_back(independent.add_option(j, 0, 1, j - 20));
            independent.add_option(j, 1, 1, j - 10);
            sum += j - 20;
        }
        for (int p = 0; p < 5; p++) {
            o.algorithm = static_cast<S::profile>(p); o.iteration_limit = 1000;
            r = independent.solve(o);
            assert(r.feasible() && r.objective == sum && r.iterations == 0);
            assert(r.lower_bound == static_cast<double>(sum));
            auto session = independent.make_session();
            for (int id : cheapest) session.set_option_cost(id, session.option_cost(id) + 100);
            auto changed = session.solve(o);
            assert(changed.feasible() && changed.objective == sum + 400 && changed.iterations == 0);
            assert(changed.lower_bound == static_cast<double>(changed.objective));
            session.reset_costs();
            assert(session.solve(o).option_ids == r.option_ids);
            session.set_capacity(0,0);
            auto repaired = session.repair(r.option_ids, std::vector<int>{0}, o);
            assert(repaired.repair_state == S::repair_status::fixed_part_infeasible);
            std::vector<int> all(40); std::iota(all.begin(),all.end(),0);
            repaired = session.repair(r.option_ids,all,o);
            assert(repaired.feasible() && repaired.objective == sum + 400);
        }
    }
    std::cerr << "OK\n";
    return 0;
}
#endif
