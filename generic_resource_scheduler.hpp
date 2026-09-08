/*
 * generic_resource_scheduler v07 (2026-09-08):
 *   先行制約DAGと再生可能資源容量を持つ非プリエンプティブなタスクを
 *   priority list + Serial Schedule Generation Schemeでスケジュールする
 *   AHC向けヒューリスティックsolver。
 *
 * 主な対象:
 *   - RCPSP / Multi-mode RCPSP
 *   - Job Shop（機械を容量1資源として表現）
 *   - Flexible Job Shop（代替機械を実行モードとして表現）
 *   - release time、重み付き完了時刻、tardiness、mode costを持つ工程計画
 *
 * 前提:
 *   - タスクは中断しない
 *   - 資源容量は全期間で一定
 *   - 時刻に関する目的は、単なる遅延で改善しないregular objective
 *   - hard deadline、sequence-dependent setup、非再生可能資源は扱わない
 *
 * 探索:
 *   トポロジカル順序と各タスクのmodeを状態とし、precedence-safe relocate、
 *   swap、mode changeを焼きなましで探索する。各候補はSSGSで復号するため、
 *   探索中も先行制約・release time・資源容量を常に満たす。
 *
 * resource calendar:
 *   短い処理時間ではtimestamp付き時刻配列、長い処理時間・大きな時刻値では
 *   一定負荷区間の整列配列を自動選択する。共通prefixは配置を再利用する。
 */
#if __INCLUDE_LEVEL__ > 0
#pragma once
#endif

#include <bits/stdc++.h>

struct scheduling_resource {
    int capacity = 1;
};

struct scheduling_resource_use {
    int resource = 0;
    int amount = 0;
};

struct scheduling_mode {
    int duration = 1;
    std::vector<scheduling_resource_use> renewable_uses;
    long double fixed_cost = 0;
};

struct scheduling_task {
    int release_time = 0;
    int due_time = -1;
    std::vector<scheduling_mode> modes;
    long double completion_weight = 0;
    long double tardiness_weight = 0;
};

struct scheduling_precedence {
    int before = 0;
    int after = 0;
};

struct resource_schedule {
    std::vector<int> task_order;
    std::vector<int> mode_of_task;
    std::vector<long long> start_time;
};

struct resource_scheduling_options {
    double time_limit_ms = 1950.0;
    std::optional<std::chrono::steady_clock::time_point> deadline;
    std::uint64_t seed = 1;
    long long max_iterations = -1;
};

struct resource_scheduling_result {
    resource_schedule schedule;
    long double objective = 0;
    long long iterations = 0;
    long long decoded_schedules = 0;
    long long accepted_moves = 0;
};

struct resource_scheduling_problem;
template<class Evaluator>
resource_scheduling_result resource_scheduling_solve(
    const resource_scheduling_problem&, Evaluator,
    const resource_scheduling_options& = {});
template<class Evaluator>
resource_scheduling_result resource_scheduling_improve(
    const resource_scheduling_problem&, const resource_schedule&, Evaluator,
    const resource_scheduling_options& = {});

struct resource_scheduling_problem {
    std::vector<scheduling_resource> resources;
    std::vector<scheduling_task> tasks;
    std::vector<scheduling_precedence> precedences;
    long double makespan_weight = 1;
    long double mode_cost_weight = 1;
private:
    class implementation;
    friend struct resource_scheduling_test_access;
    friend void resource_scheduling_validate_problem(const resource_scheduling_problem&);
    friend bool resource_scheduling_is_feasible(const resource_scheduling_problem&, const resource_schedule&);
    friend long double resource_scheduling_evaluate(const resource_scheduling_problem&, const resource_schedule&);
    template<class Evaluator> friend long double resource_scheduling_evaluate(
        const resource_scheduling_problem&, const resource_schedule&, Evaluator);
    friend resource_scheduling_result resource_scheduling_solve(
        const resource_scheduling_problem&, const resource_scheduling_options&);
    template<class Evaluator> friend resource_scheduling_result resource_scheduling_solve(
        const resource_scheduling_problem&, Evaluator, const resource_scheduling_options&);
    friend resource_scheduling_result resource_scheduling_improve(
        const resource_scheduling_problem&, const resource_schedule&, const resource_scheduling_options&);
    template<class Evaluator> friend resource_scheduling_result resource_scheduling_improve(
        const resource_scheduling_problem&, const resource_schedule&, Evaluator, const resource_scheduling_options&);
};

class resource_scheduling_problem::implementation {
public:
    struct random_engine {
        std::uint64_t state;

        explicit random_engine(std::uint64_t seed)
            : state(seed + 0x9e3779b97f4a7c15ULL) {}

        std::uint64_t next_u64() {
            std::uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return z ^ (z >> 31);
        }

        int uniform_int(int bound) {
            assert(bound > 0);
            return static_cast<int>((static_cast<__uint128_t>(next_u64()) *
                                     static_cast<unsigned>(bound)) >> 64);
        }

        double uniform01() {
            return static_cast<double>(next_u64() >> 11) * 0x1.0p-53;
        }
    };

    struct problem_data {
        int task_count = 0;
        int resource_count = 0;
        std::vector<std::vector<int>> predecessors;
        std::vector<std::vector<int>> successors;
        std::vector<int> topological_order;
        long long horizon = 1;
        // Kahn法の各段階で選択肢が1個、かつ全taskのmodeが1個のときだけtrue。
        bool single_search_state = true;
        explicit problem_data(const resource_scheduling_problem& problem) {
            task_count = static_cast<int>(problem.tasks.size());
            resource_count = static_cast<int>(problem.resources.size());
            predecessors.assign(task_count, {});
            successors.assign(task_count, {});
            std::vector<int> indegree(task_count, 0);

            for (const scheduling_precedence& edge : problem.precedences) {
                successors[edge.before].push_back(edge.after);
                predecessors[edge.after].push_back(edge.before);
                ++indegree[edge.after];
            }

            std::queue<int> queue;
            for (int task = 0; task < task_count; ++task) {
                if (indegree[task] == 0) queue.push(task);
            }
            while (!queue.empty()) {
                if (queue.size() != 1) single_search_state = false;
                const int task = queue.front();
                queue.pop();
                topological_order.push_back(task);
                for (int next : successors[task]) {
                    if (--indegree[next] == 0) queue.push(next);
                }
            }

            long long max_release = 0;
            long long duration_sum = 0;
            for (const scheduling_task& task : problem.tasks) {
                max_release = std::max(max_release, static_cast<long long>(task.release_time));
                if (task.modes.size() != 1) single_search_state = false;
                int maximum_duration = 0;
                for (const scheduling_mode& mode : task.modes) {
                    maximum_duration = std::max(maximum_duration, mode.duration);
                }
                if (duration_sum > std::numeric_limits<long long>::max() - maximum_duration) {
                    throw std::invalid_argument("durationの総和がlong longの範囲を超える");
                }
                duration_sum += maximum_duration;
            }
            if (max_release > std::numeric_limits<long long>::max() - duration_sum - 1) {
                throw std::invalid_argument("schedule horizonがlong longの範囲を超える");
            }
            horizon = std::max(1LL, max_release + duration_sum + 1);
        }
    };

    static problem_data validate_problem_or_throw(
        const resource_scheduling_problem& problem) {
        if (!std::isfinite(problem.makespan_weight) || problem.makespan_weight < 0) {
            throw std::invalid_argument("makespan_weightは有限かつ0以上でなければならない");
        }
        if (!std::isfinite(problem.mode_cost_weight) || problem.mode_cost_weight < 0) {
            throw std::invalid_argument("mode_cost_weightは有限かつ0以上でなければならない");
        }
        for (const scheduling_resource& resource : problem.resources) {
            if (resource.capacity <= 0) {
                throw std::invalid_argument("resource capacityは正でなければならない");
            }
        }

        const int task_count = static_cast<int>(problem.tasks.size());
        const int resource_count = static_cast<int>(problem.resources.size());
        for (int task_id = 0; task_id < task_count; ++task_id) {
            const scheduling_task& task = problem.tasks[task_id];
            if (task.release_time < 0) {
                throw std::invalid_argument("release_timeは0以上でなければならない");
            }
            if (task.due_time < -1) {
                throw std::invalid_argument("due_timeは-1または0以上でなければならない");
            }
            if (!std::isfinite(task.completion_weight) || task.completion_weight < 0 ||
                !std::isfinite(task.tardiness_weight) || task.tardiness_weight < 0) {
                throw std::invalid_argument("task weightは有限かつ0以上でなければならない");
            }
            if (task.modes.empty()) {
                throw std::invalid_argument("各taskには1個以上のmodeが必要");
            }
            for (const scheduling_mode& mode : task.modes) {
                if (mode.duration < 0) {
                    throw std::invalid_argument("durationは0以上でなければならない");
                }
                if (!std::isfinite(mode.fixed_cost)) {
                    throw std::invalid_argument("fixed_costは有限でなければならない");
                }
                std::vector<int> used_resources;
                used_resources.reserve(mode.renewable_uses.size());
                for (const scheduling_resource_use& use : mode.renewable_uses) {
                    if (use.resource < 0 || use.resource >= resource_count) {
                        throw std::invalid_argument("resource idが範囲外");
                    }
                    if (use.amount <= 0 ||
                        use.amount > problem.resources[use.resource].capacity) {
                        throw std::invalid_argument(
                            "resource amountは1以上capacity以下でなければならない");
                    }
                    used_resources.push_back(use.resource);
                }
                std::sort(used_resources.begin(), used_resources.end());
                if (std::adjacent_find(used_resources.begin(), used_resources.end()) !=
                    used_resources.end()) {
                    throw std::invalid_argument("同じmode内でresourceを重複指定できない");
                }
            }
        }

        for (const scheduling_precedence& edge : problem.precedences) {
            if (edge.before < 0 || edge.before >= task_count || edge.after < 0 ||
                edge.after >= task_count || edge.before == edge.after) {
                throw std::invalid_argument("precedence edgeが不正");
            }
        }
        const problem_data data(problem);
        if (static_cast<int>(data.topological_order.size()) != task_count) {
            throw std::invalid_argument("precedence graphはDAGでなければならない");
        }
        return data;
    }

    // 負荷一定の区間境界を整列配列で保持する。挿入・区間走査は最悪線形。
    class interval_calendars {
        struct segment {
            long long start;
            int load;
        };
        std::vector<std::vector<segment>> segments;
        long long horizon = 1;

        static int containing(const std::vector<segment>& v, long long time) {
            return static_cast<int>(std::upper_bound(v.begin(), v.end(), time,
                [](long long t, const segment& s) { return t < s.start; }) - v.begin()) - 1;
        }

        static int split(std::vector<segment>& v, long long time) {
            const int pos = containing(v, time);
            if (v[pos].start == time) return pos;
            const int load = v[pos].load;
            v.insert(v.begin() + pos + 1, {time, load});
            return pos + 1;
        }

    public:
        void reset(int resource_count, long long new_horizon) {
            horizon = new_horizon;
            segments.resize(resource_count);
            for (auto& v : segments) {
                v.clear();
                v.push_back({0, 0});
                v.push_back({horizon, 0});
            }
        }

        long long earliest_start(const std::vector<scheduling_resource_use>& uses,
                                 const std::vector<scheduling_resource>& resources,
                                 long long earliest, int duration) const {
            if (duration == 0 || uses.empty()) return earliest;
            long long start = earliest;
            while (true) {
                assert(start + duration <= horizon);
                long long next = start;
                for (const auto& use : uses) {
                    // 最初の競合で開始候補を進め、全資源を新しい時刻で検査し直す。
                    if (next != start) break;
                    const auto& v = segments[use.resource];
                    const int threshold = resources[use.resource].capacity - use.amount;
                    int i = containing(v, start);
                    while (v[i].start < start + duration) {
                        if (v[i].load > threshold) {
                            do { ++i; } while (v[i].load > threshold);
                            next = std::max(next, v[i].start);
                            break;
                        }
                        ++i;
                    }
                }
                if (next == start) return start;
                start = next;
            }
        }

        void reserve(const std::vector<scheduling_resource_use>& uses, long long start, int duration) {
            if (duration == 0) return;
            for (const auto& use : uses) {
                auto& v = segments[use.resource];
                const int left = split(v, start);
                const int right = split(v, start + duration);
                for (int i = left; i < right; ++i) v[i].load += use.amount;
            }
        }
    };

    // 上位32 bitを世代、下位32 bitを負荷に使う。世代更新で全時刻のclearを省く。
    class dense_calendars {
        std::vector<std::uint64_t> cells;
        std::uint64_t generation = 0;
        long long horizon = 1;
        std::size_t horizon_size = 1;

        int get(std::size_t base, long long time) const {
            const std::uint64_t value = cells[base + static_cast<std::size_t>(time)];
            return (value & 0xffffffff00000000ULL) == generation ?
                static_cast<int>(value & 0xffffffffULL) : 0;
        }

    public:
        void reset(int resource_count, long long new_horizon) {
            if (new_horizon > 5000000) throw std::length_error("dense horizon too large");
            horizon = new_horizon;
            horizon_size = static_cast<std::size_t>(horizon);
            const auto count = static_cast<std::size_t>(resource_count) * horizon_size;
            if (cells.size() != count) {
                cells.assign(count, 0);
                generation = 1ULL << 32;
            } else {
                generation += 1ULL << 32;
                if (generation == 0) { std::fill(cells.begin(), cells.end(), 0); generation = 1ULL << 32; }
            }
        }

        long long earliest_start(const std::vector<scheduling_resource_use>& uses,
                                 const std::vector<scheduling_resource>& resources,
                                 long long earliest, int duration) const {
            if (duration == 0 || uses.empty()) return earliest;
            long long start = earliest;
            while (true) {
                assert(start + duration <= horizon);
                long long next = start;
                for (auto use : uses) {
                    // 最初の競合で開始候補を進め、全資源を新しい時刻で検査し直す。
                    if (next != start) break;
                    const int threshold = resources[use.resource].capacity - use.amount;
                    const auto base = static_cast<std::size_t>(use.resource) * horizon_size;
                    for (long long time = start; time < start + duration; ++time) {
                        if (get(base, time) <= threshold) continue;
                        long long end = time + 1;
                        while (end < horizon && get(base, end) > threshold) ++end;
                        next = std::max(next, end);
                        break;
                    }
                }
                if (next == start) return start;
                start = next;
            }
        }

        void reserve(const std::vector<scheduling_resource_use>& uses, long long start, int duration) {
            for (auto use : uses) {
                const auto base = static_cast<std::size_t>(use.resource) * horizon_size;
                for (long long time = start; time < start + duration; ++time) {
                    const auto index = base + static_cast<std::size_t>(time);
                    cells[index] = generation | static_cast<unsigned>(get(base, time) + use.amount);
                }
            }
        }
    };

    struct builtin_evaluator {
        long double operator()(
            const resource_scheduling_problem& problem,
            const resource_schedule& schedule) const {
            long double objective = 0;
            long long makespan = 0;
            const int task_count = static_cast<int>(problem.tasks.size());
            for (int task = 0; task < task_count; ++task) {
                const scheduling_task& task_data = problem.tasks[task];
                const scheduling_mode& mode = task_data.modes[schedule.mode_of_task[task]];
                const long long completion = schedule.start_time[task] + mode.duration;
                makespan = std::max(makespan, completion);
                objective += task_data.completion_weight * completion;
                if (task_data.due_time >= 0 && completion > task_data.due_time) {
                    objective += task_data.tardiness_weight *
                                 static_cast<long double>(completion - task_data.due_time);
                }
                objective += problem.mode_cost_weight * mode.fixed_cost;
            }
            objective += problem.makespan_weight * makespan;
            return objective;
        }
    };

    template<class Evaluator, class Calendars = interval_calendars>
    struct schedule_decoder {
        const resource_scheduling_problem& problem;
        const problem_data& data;
        Evaluator& evaluator;
        Calendars calendars;
        resource_schedule schedule;

        schedule_decoder(
            const resource_scheduling_problem& problem_value,
            const problem_data& data_value,
            Evaluator& evaluator_value)
            : problem(problem_value), data(data_value), evaluator(evaluator_value) {
            schedule.start_time.resize(data.task_count);
        }

        long double decode(
            const std::vector<int>& order,
            const std::vector<int>& mode_of_task) {
            // 順序とmodeが一致する先頭部分はSSGSの配置も同一。直前候補の採否には依存しない。
            // 呼出し内だけの再利用であり、problem変更や別solveとの共有cacheではない。
            int unchanged = 0;
            while (unchanged < static_cast<int>(order.size()) &&
                   unchanged < static_cast<int>(schedule.task_order.size()) &&
                   order[unchanged] == schedule.task_order[unchanged] &&
                   mode_of_task[order[unchanged]] == schedule.mode_of_task[order[unchanged]]) ++unchanged;
            schedule.task_order = order;
            schedule.mode_of_task = mode_of_task;
            calendars.reset(data.resource_count, data.horizon);

            int order_index = 0;
            for (int task : order) {
                const scheduling_task& task_data = problem.tasks[task];
                const scheduling_mode& mode = task_data.modes[mode_of_task[task]];
                if (order_index++ < unchanged) {
                    calendars.reserve(mode.renewable_uses, schedule.start_time[task], mode.duration);
                    continue;
                }
                long long earliest = task_data.release_time;
                for (int predecessor : data.predecessors[task]) {
                    const scheduling_mode& predecessor_mode =
                        problem.tasks[predecessor].modes[mode_of_task[predecessor]];
                    earliest = std::max(
                        earliest,
                        schedule.start_time[predecessor] + predecessor_mode.duration);
                }
                const long long start = calendars.earliest_start(
                    mode.renewable_uses, problem.resources, earliest, mode.duration);
                schedule.start_time[task] = start;
                calendars.reserve(mode.renewable_uses, start, mode.duration);
            }
            const long double objective =
                static_cast<long double>(evaluator(problem, schedule));
            if (!std::isfinite(objective)) {
                throw std::invalid_argument("evaluatorは有限の目的値を返す必要がある");
            }
            return objective;
        }
    };

    enum class initial_order_rule {
        balanced,
        duration_ratio,
        weighted_shortest,
        earliest_due,
    };

    static std::vector<int> make_initial_order(
        const resource_scheduling_problem& problem,
        const problem_data& data,
        const std::vector<int>& modes,
        random_engine& random,
        initial_order_rule rule = initial_order_rule::balanced) {
        constexpr double randomness = 0.03;
        std::vector<long double> priority;
        if (rule == initial_order_rule::balanced || rule == initial_order_rule::duration_ratio) {
            priority = [&]() {
                const bool duration_ratio = rule == initial_order_rule::duration_ratio;
                const int task_count = data.task_count;
                std::vector<long double> downstream_weight(task_count, 0);
                for (int index = task_count - 1; index >= 0; --index) {
                    const int task = data.topological_order[index];
                    long double child_weight = 0;
                    for (int child : data.successors[task]) {
                        child_weight = std::max(child_weight, downstream_weight[child]);
                    }
                    downstream_weight[task] =
                        problem.tasks[task].completion_weight +
                        problem.tasks[task].tardiness_weight + child_weight;
                }

                if (duration_ratio) {
                    long double maximum = 1e-18L;
                    for (int task = 0; task < task_count; ++task) {
                        downstream_weight[task] /= std::max(1, problem.tasks[task].modes[modes[task]].duration);
                        maximum = std::max(maximum, downstream_weight[task]);
                    }
                    for (long double& weight : downstream_weight) weight /= maximum;
                    return downstream_weight;
                }

                const long double max_weight = std::max(
                    1.0L,
                    *std::max_element(downstream_weight.begin(), downstream_weight.end()));
                for (int task = 0; task < task_count; ++task) {
                    const scheduling_task& task_data = problem.tasks[task];
                    const scheduling_mode& mode = task_data.modes[modes[task]];
                    long double pressure = 0;
                    for (const scheduling_resource_use& use : mode.renewable_uses) {
                        pressure += static_cast<long double>(use.amount) /
                                    problem.resources[use.resource].capacity;
                    }
                    if (!problem.resources.empty()) pressure /= problem.resources.size();
                    long double urgency = 0;
                    if (task_data.due_time >= 0) {
                        urgency = 1.0L - std::min(
                            1.0L,
                            static_cast<long double>(task_data.due_time) /
                                std::max(1LL, data.horizon));
                    }
                    downstream_weight[task] =
                        0.85L * downstream_weight[task] / max_weight +
                        0.30L * pressure + 0.35L * urgency;
                }
                return downstream_weight;
            }();
        } else {
            priority.resize(data.task_count);
            for (int task = 0; task < data.task_count; ++task) {
                const auto& task_data = problem.tasks[task];
                const int duration = task_data.modes[modes[task]].duration;
                if (rule == initial_order_rule::weighted_shortest) {
                    priority[task] = (1.0L + task_data.completion_weight + task_data.tardiness_weight) /
                        std::max(1, duration);
                } else {
                    priority[task] = task_data.due_time < 0 ? -static_cast<long double>(data.horizon) :
                        -static_cast<long double>(task_data.due_time) + duration;
                }
            }
        }
        std::vector<int> indegree(data.task_count, 0);
        for (int task = 0; task < data.task_count; ++task) {
            indegree[task] = static_cast<int>(data.predecessors[task].size());
        }
        using entry = std::pair<long double, int>;
        std::priority_queue<entry> queue;
        for (int task = 0; task < data.task_count; ++task) {
            if (indegree[task] == 0) {
                queue.push({priority[task] + random.uniform01() * randomness, task});
            }
        }
        std::vector<int> order;
        order.reserve(data.task_count);
        while (!queue.empty()) {
            const int task = queue.top().second;
            queue.pop();
            order.push_back(task);
            for (int child : data.successors[task]) {
                if (--indegree[child] == 0) {
                    queue.push({priority[child] + random.uniform01() * randomness, child});
                }
            }
        }
        return order;
    }

    enum class move_type {
        none,
        relocate,
        swap,
        mode_change,
    };

    struct move {
        move_type type = move_type::none;
        int first = -1;
        int second = -1;
        int old_mode = -1;
    };

    static void rebuild_positions(
        const std::vector<int>& order,
        std::vector<int>& position,
        int left,
        int right) {
        for (int index = left; index <= right; ++index) {
            position[order[index]] = index;
        }
    }

    static bool task_edges_valid_after_swap(
        int task,
        const std::vector<int>& position,
        const problem_data& data) {
        for (int predecessor : data.predecessors[task]) {
            if (position[predecessor] >= position[task]) return false;
        }
        for (int successor : data.successors[task]) {
            if (position[task] >= position[successor]) return false;
        }
        return true;
    }

    enum class calendar_selection {
        automatic,
        interval,
        dense,
    };

    static bool validate_schedule(
        const resource_scheduling_problem& problem,
        const problem_data& data,
        const resource_schedule& schedule) {
        const int task_count = data.task_count;
        if (static_cast<int>(schedule.task_order.size()) != task_count ||
            static_cast<int>(schedule.mode_of_task.size()) != task_count ||
            static_cast<int>(schedule.start_time.size()) != task_count) {
            return false;
        }
        std::vector<int> position(task_count, -1);
        for (int index = 0; index < task_count; ++index) {
            const int task = schedule.task_order[index];
            if (task < 0 || task >= task_count || position[task] != -1) return false;
            position[task] = index;
        }
        for (int task = 0; task < task_count; ++task) {
            const int mode_id = schedule.mode_of_task[task];
            if (mode_id < 0 ||
                mode_id >= static_cast<int>(problem.tasks[task].modes.size()) ||
                schedule.start_time[task] < problem.tasks[task].release_time) {
                return false;
            }
            const int duration = problem.tasks[task].modes[mode_id].duration;
            if (schedule.start_time[task] >
                std::numeric_limits<long long>::max() - duration) {
                return false;
            }
        }
        for (const scheduling_precedence& edge : problem.precedences) {
            if (position[edge.before] >= position[edge.after]) return false;
            const scheduling_mode& before_mode =
                problem.tasks[edge.before].modes[schedule.mode_of_task[edge.before]];
            if (schedule.start_time[edge.before] + before_mode.duration >
                schedule.start_time[edge.after]) {
                return false;
            }
        }

        struct event {
            long long time;
            int resource;
            int delta;
        };
        std::vector<event> events;
        for (int task = 0; task < task_count; ++task) {
            const scheduling_mode& mode =
                problem.tasks[task].modes[schedule.mode_of_task[task]];
            if (mode.duration == 0) continue;
            const long long start = schedule.start_time[task];
            const long long finish = start + mode.duration;
            for (const scheduling_resource_use& use : mode.renewable_uses) {
                events.push_back({start, use.resource, use.amount});
                events.push_back({finish, use.resource, -use.amount});
            }
        }
        std::sort(events.begin(), events.end(), [](const event& lhs, const event& rhs) {
            if (lhs.time != rhs.time) return lhs.time < rhs.time;
            return lhs.resource < rhs.resource;
        });
        // 不正scheduleでは容量を大幅に超えうるため、検査途中の合計も64 bitで計算する。
        std::vector<long long> load(data.resource_count, 0);
        std::size_t index = 0;
        while (index < events.size()) {
            const long long time = events[index].time;
            const int resource = events[index].resource;
            long long delta = 0;
            while (index < events.size() && events[index].time == time && events[index].resource == resource) {
                delta += events[index].delta;
                ++index;
            }
            load[resource] += delta;
            if (load[resource] < 0 || load[resource] > problem.resources[resource].capacity) return false;
        }
        return true;
    }

    template<class Calendars, class Evaluator>
    static resource_scheduling_result run_solver_with_calendar(
        const resource_scheduling_problem& problem,
        const problem_data& data,
        const resource_schedule* initial_schedule,
        Evaluator evaluator,
        const resource_scheduling_options& options) {
        resource_scheduling_result result;
        if (data.task_count == 0) {
            result.objective =
                static_cast<long double>(evaluator(problem, result.schedule));
            if (!std::isfinite(result.objective)) {
                throw std::invalid_argument("evaluatorは有限の目的値を返す必要がある");
            }
            return result;
        }

        random_engine random(options.seed);
        random_engine initial_random(options.seed ^ 0xd1b54a32d192ed03ULL);
        std::vector<int> modes;
        std::vector<int> order;
        if (initial_schedule == nullptr) {
            modes = [&]() {
                const int task_count = static_cast<int>(problem.tasks.size());
                const int resource_count = static_cast<int>(problem.resources.size());
                long double total_time_weight = problem.makespan_weight;
                for (const scheduling_task& task : problem.tasks) {
                    total_time_weight += task.completion_weight + task.tardiness_weight;
                }
                const long double average_time_weight =
                    total_time_weight / std::max(1, task_count);

                std::vector<int> initial_modes(task_count, 0);
                for (int task_id = 0; task_id < task_count; ++task_id) {
                    const scheduling_task& task = problem.tasks[task_id];
                    const long double task_time_weight =
                        problem.makespan_weight / std::max(1, task_count) +
                        task.completion_weight + task.tardiness_weight;
                    long double best_score = std::numeric_limits<long double>::infinity();
                    for (int mode_id = 0; mode_id < static_cast<int>(task.modes.size()); ++mode_id) {
                        const scheduling_mode& mode = task.modes[mode_id];
                        long double pressure = 0;
                        for (const scheduling_resource_use& use : mode.renewable_uses) {
                            pressure += static_cast<long double>(use.amount) /
                                        problem.resources[use.resource].capacity;
                        }
                        if (resource_count > 0) pressure /= resource_count;
                        const long double score =
                            task_time_weight * mode.duration +
                            problem.mode_cost_weight * mode.fixed_cost +
                            average_time_weight * mode.duration * pressure * 0.15L;
                        if (score < best_score) {
                            best_score = score;
                            initial_modes[task_id] = mode_id;
                        }
                    }
                }
                return initial_modes;
            }();
            order = make_initial_order(problem, data, modes, initial_random);
        } else {
            modes = initial_schedule->mode_of_task;
            order = initial_schedule->task_order;
        }

        schedule_decoder<Evaluator, Calendars> decoder(problem, data, evaluator);
        long double current_objective = decoder.decode(order, modes);
        ++result.decoded_schedules;
        // 現在状態はorder/modesと目的値だけで足りる。scheduleは最良解の更新時に保存する。
        result.schedule = decoder.schedule;
        result.objective = current_objective;

        if (initial_schedule != nullptr) {
            const long double initial_objective =
                static_cast<long double>(evaluator(problem, *initial_schedule));
            if (!std::isfinite(initial_objective)) {
                throw std::invalid_argument("evaluatorは有限の目的値を返す必要がある");
            }
            if (initial_objective < result.objective) {
                result.schedule = *initial_schedule;
                result.objective = initial_objective;
            }
        }

        // 順序とmodeの両方が一意なら、初回評価で探索空間を尽くしている。
        // improveのinitialとの比較・Evaluatorの有限性検査も、この時点までに完了している。
        if (data.single_search_state) return result;

        const auto begin = std::chrono::steady_clock::now();
        auto end =
            begin + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                        std::chrono::duration<double, std::milli>(
                            std::max(0.0, options.time_limit_ms)));
        if (options.deadline.has_value()) end = std::min(end, *options.deadline);
        if (options.time_limit_ms <= 0 || options.max_iterations == 0 ||
            begin >= end) {
            return result;
        }

        // 改善時間内で追加順序を評価し、最も良いSSGS状態からSAを開始する。
        // resultは外部initialも含む最良解なので、現在のSSGS状態と別に保持する。
        auto best_order = order;
        long double best_ssgs_objective = current_objective;
        for (initial_order_rule rule : {initial_order_rule::duration_ratio,
                initial_order_rule::weighted_shortest, initial_order_rule::earliest_due}) {
            if (std::chrono::steady_clock::now() >= end ||
                (options.max_iterations >= 0 && result.iterations >= options.max_iterations)) break;
            order = make_initial_order(problem, data, modes, initial_random, rule);
            const long double value = decoder.decode(order, modes);
            ++result.decoded_schedules;
            ++result.iterations;
            if (value < best_ssgs_objective) {
                best_ssgs_objective = value;
                best_order = order;
            }
            if (value < result.objective) {
                result.objective = value;
                result.schedule = decoder.schedule;
            }
        }
        if (std::chrono::steady_clock::now() >= end ||
            (options.max_iterations >= 0 && result.iterations >= options.max_iterations)) return result;
        order = std::move(best_order);
        current_objective = best_ssgs_objective;
        std::vector<int> position(data.task_count);
        for (int i = 0; i < data.task_count; ++i) position[order[i]] = i;

        constexpr double relocate_probability = 0.55;
        constexpr double swap_probability = 0.25;
        constexpr long double temperature_scale = 0.02L;
        constexpr long double temperature_end_ratio = 1e-4L;
        const long double start_temperature = std::max(
            1e-12L,
            std::abs(current_objective) * temperature_scale / data.task_count);
        const long double end_temperature =
            start_temperature * temperature_end_ratio;
        long double temperature = start_temperature;

        const auto propose_relocate = +[](
            std::vector<int>& move_order,
            std::vector<int>& move_position,
            const problem_data& move_data,
            random_engine& move_random,
            int move_task) -> move {
            const int old_position = move_position[move_task];
            int first_position = 0;
            int last_position = move_data.task_count - 1;
            for (int predecessor : move_data.predecessors[move_task]) {
                first_position = std::max(first_position, move_position[predecessor] + 1);
            }
            for (int successor : move_data.successors[move_task]) {
                last_position = std::min(last_position, move_position[successor] - 1);
            }
            const int choice_count = last_position - first_position;
            if (choice_count <= 0) return {};
            int new_position = first_position + move_random.uniform_int(choice_count);
            if (new_position >= old_position) ++new_position;
            if (new_position < old_position) {
                std::rotate(
                    move_order.begin() + new_position,
                    move_order.begin() + old_position,
                    move_order.begin() + old_position + 1);
            } else {
                std::rotate(
                    move_order.begin() + old_position,
                    move_order.begin() + old_position + 1,
                    move_order.begin() + new_position + 1);
            }
            rebuild_positions(
                move_order, move_position, std::min(old_position, new_position),
                std::max(old_position, new_position));
            return {move_type::relocate, old_position, new_position, -1};
        };

        const auto propose_swap = +[](
            std::vector<int>& move_order,
            std::vector<int>& move_position,
            const problem_data& move_data,
            random_engine& move_random) -> move {
            if (move_data.task_count < 2) return {};
            const int first_position = move_random.uniform_int(move_data.task_count);
            int second_position = move_random.uniform_int(move_data.task_count - 1);
            if (second_position >= first_position) ++second_position;
            const int first_task = move_order[first_position];
            const int second_task = move_order[second_position];
            std::swap(move_order[first_position], move_order[second_position]);
            move_position[first_task] = second_position;
            move_position[second_task] = first_position;
            if (!task_edges_valid_after_swap(first_task, move_position, move_data) ||
                !task_edges_valid_after_swap(second_task, move_position, move_data)) {
                std::swap(move_order[first_position], move_order[second_position]);
                move_position[first_task] = first_position;
                move_position[second_task] = second_position;
                return {};
            }
            return {move_type::swap, first_position, second_position, -1};
        };

        const auto propose_mode_change = +[](
            const resource_scheduling_problem& move_problem,
            std::vector<int>& move_modes,
            random_engine& move_random,
            int move_task) -> move {
            const int mode_count = static_cast<int>(move_problem.tasks[move_task].modes.size());
            if (mode_count < 2) return {};
            const int old_mode = move_modes[move_task];
            int new_mode = move_random.uniform_int(mode_count - 1);
            if (new_mode >= old_mode) ++new_mode;
            move_modes[move_task] = new_mode;
            return {move_type::mode_change, move_task, new_mode, old_mode};
        };

        while (options.max_iterations < 0 ||
               result.iterations < options.max_iterations) {
            // 停止判定は8提案ごと、温度更新は64提案ごと。decode途中には割り込まない。
            if ((result.iterations & 7) == 0) {
                const auto now = std::chrono::steady_clock::now();
                if (now >= end) break;
                if ((result.iterations & 63) == 0) {
                    const double time_progress =
                        std::chrono::duration<double>(now - begin).count() /
                        std::max(
                            1e-12,
                            std::chrono::duration<double>(end - begin).count());
                    const double iteration_progress =
                        options.max_iterations > 0
                            ? static_cast<double>(result.iterations) /
                                  static_cast<double>(options.max_iterations)
                            : 0;
                    const double progress = std::clamp(
                        options.max_iterations > 0
                            ? std::max(time_progress, iteration_progress)
                            : time_progress,
                        0.0,
                        1.0);
                    temperature =
                        start_temperature *
                        std::pow(end_temperature / start_temperature, progress);
                }
            }

            ++result.iterations;
            move candidate;
            const double selector = random.uniform01();
            if (selector < relocate_probability) {
                candidate = propose_relocate(
                    order,
                    position,
                    data,
                    random,
                    random.uniform_int(data.task_count));
            } else if (selector < relocate_probability + swap_probability) {
                candidate = propose_swap(order, position, data, random);
            } else {
                candidate = propose_mode_change(
                    problem,
                    modes,
                    random,
                    random.uniform_int(data.task_count));
            }
            if (candidate.type == move_type::none) continue;

            const long double candidate_objective = decoder.decode(order, modes);
            ++result.decoded_schedules;
            const long double delta = candidate_objective - current_objective;
            bool accept = delta <= 0;
            if (!accept) {
                const long double exponent = -delta / temperature;
                if (exponent > -50 && random.uniform01() < std::exp(exponent)) {
                    accept = true;
                }
            }

            if (accept) {
                current_objective = candidate_objective;
                ++result.accepted_moves;
                if (candidate_objective < result.objective) {
                    result.objective = candidate_objective;
                    result.schedule = decoder.schedule;
                }
            } else {
                if (candidate.type == move_type::relocate) {
                    const int old_position = candidate.first;
                    const int new_position = candidate.second;
                    if (old_position < new_position) {
                        std::rotate(
                            order.begin() + old_position,
                            order.begin() + new_position,
                            order.begin() + new_position + 1);
                    } else {
                        std::rotate(
                            order.begin() + new_position,
                            order.begin() + new_position + 1,
                            order.begin() + old_position + 1);
                    }
                    rebuild_positions(
                        order, position, std::min(old_position, new_position),
                        std::max(old_position, new_position));
                } else if (candidate.type == move_type::swap) {
                    const int first_position = candidate.first;
                    const int second_position = candidate.second;
                    const int first_task = order[first_position];
                    const int second_task = order[second_position];
                    std::swap(order[first_position], order[second_position]);
                    position[first_task] = second_position;
                    position[second_task] = first_position;
                } else if (candidate.type == move_type::mode_change) {
                    modes[candidate.first] = candidate.old_mode;
                }
            }
        }
        return result;
    }

    static bool select_dense_calendar(
        const resource_scheduling_problem& problem,
        const problem_data& data) {
        if (problem.resources.empty()) return false;
        long double duration_use_sum = 0;
        long long use_count = 0;
        for (const scheduling_task& task : problem.tasks) {
            for (const scheduling_mode& mode : task.modes) {
                duration_use_sum +=
                    static_cast<long double>(mode.duration) *
                    static_cast<long double>(mode.renewable_uses.size());
                use_count +=
                    static_cast<long long>(mode.renewable_uses.size());
            }
        }
        if (use_count == 0) return false;
        constexpr long double maximum_dense_cells = 4'000'000;
        const long double average_duration =
            duration_use_sum / static_cast<long double>(use_count);
        const long double maximum_average_duration =
            14.0L * std::log2(static_cast<long double>(data.task_count) + 1.0L);
        const long double dense_cells =
            static_cast<long double>(data.horizon) * data.resource_count;
        return average_duration <= maximum_average_duration &&
               dense_cells <= maximum_dense_cells;
    }

    template<class Evaluator>
    static resource_scheduling_result run_solver(
        const resource_scheduling_problem& problem,
        const resource_schedule* initial_schedule,
        Evaluator evaluator,
        const resource_scheduling_options& options,
        calendar_selection selection = calendar_selection::automatic) {
        const problem_data data = validate_problem_or_throw(problem);
        if (!std::isfinite(options.time_limit_ms)) {
            throw std::invalid_argument("time_limit_msは有限でなければならない");
        }
        if (options.max_iterations < -1) {
            throw std::invalid_argument("max_iterationsは-1以上でなければならない");
        }
        if (initial_schedule != nullptr &&
            !validate_schedule(problem, data, *initial_schedule)) {
            throw std::invalid_argument("initial scheduleが実行可能でない");
        }

        const bool use_dense =
            selection == calendar_selection::dense ||
            (selection == calendar_selection::automatic &&
             select_dense_calendar(problem, data));
        if (use_dense) {
            return run_solver_with_calendar<dense_calendars>(
                problem,
                data,
                initial_schedule,
                std::move(evaluator),
                options);
        }
        return run_solver_with_calendar<interval_calendars>(
            problem,
            data,
            initial_schedule,
            std::move(evaluator),
            options);
    }
};

// 問題だけを検証する。不正入力ではinvalid_argument。全modeの要求とDAGを走査する。
inline void resource_scheduling_validate_problem(
    const resource_scheduling_problem& problem) {
    resource_scheduling_problem::implementation::validate_problem_or_throw(problem);
}

// 問題を検証後、scheduleを独立なイベント走査で検査する。schedule不正ならfalse。
inline bool resource_scheduling_is_feasible(
    const resource_scheduling_problem& problem,
    const resource_schedule& schedule) {
    const auto data =
        resource_scheduling_problem::implementation::validate_problem_or_throw(problem);
    return resource_scheduling_problem::implementation::validate_schedule(problem, data, schedule);
}

// 実行可能性を検査して組み込み目的値を返す。検査費用は概ねO(N+E+U log U)。
inline long double resource_scheduling_evaluate(
    const resource_scheduling_problem& problem,
    const resource_schedule& schedule) {
    const auto data =
        resource_scheduling_problem::implementation::validate_problem_or_throw(problem);
    if (!resource_scheduling_problem::implementation::validate_schedule(problem, data, schedule)) {
        throw std::invalid_argument("scheduleが実行可能でない");
    }
    return resource_scheduling_problem::implementation::builtin_evaluator{}(problem, schedule);
}

template<class Evaluator>
long double resource_scheduling_evaluate(
    const resource_scheduling_problem& problem,
    const resource_schedule& schedule,
    Evaluator evaluator) {
    const auto data =
        resource_scheduling_problem::implementation::validate_problem_or_throw(problem);
    if (!resource_scheduling_problem::implementation::validate_schedule(problem, data, schedule)) {
        throw std::invalid_argument("scheduleが実行可能でない");
    }
    const long double objective =
        static_cast<long double>(evaluator(problem, schedule));
    if (!std::isfinite(objective)) {
        throw std::invalid_argument("evaluatorは有限の目的値を返す必要がある");
    }
    return objective;
}

// 初期構築後にSA改善を行う。最適性・厳密な時間上限は保証しない。計算量は探索量に依存。
inline resource_scheduling_result resource_scheduling_solve(
    const resource_scheduling_problem& problem,
    const resource_scheduling_options& options = {}) {
    return resource_scheduling_problem::implementation::run_solver(
        problem,
        nullptr,
        resource_scheduling_problem::implementation::builtin_evaluator{},
        options);
}

template<class Evaluator>
resource_scheduling_result resource_scheduling_solve(
    const resource_scheduling_problem& problem,
    Evaluator evaluator,
    const resource_scheduling_options& options) {
    return resource_scheduling_problem::implementation::run_solver(
        problem,
        nullptr,
        std::move(evaluator),
        options);
}

// 実行可能な既存解から改善する。同じ決定的Evaluatorなら元の目的値以下を返す。
inline resource_scheduling_result resource_scheduling_improve(
    const resource_scheduling_problem& problem,
    const resource_schedule& initial,
    const resource_scheduling_options& options = {}) {
    return resource_scheduling_problem::implementation::run_solver(
        problem,
        &initial,
        resource_scheduling_problem::implementation::builtin_evaluator{},
        options);
}

template<class Evaluator>
resource_scheduling_result resource_scheduling_improve(
    const resource_scheduling_problem& problem,
    const resource_schedule& initial,
    Evaluator evaluator,
    const resource_scheduling_options& options) {
    return resource_scheduling_problem::implementation::run_solver(
        problem,
        &initial,
        std::move(evaluator),
        options);
}

// ヘッダを直接コンパイルしたときだけ全回帰テストを実行する。
#if __INCLUDE_LEVEL__ == 0


// Test-only access to private static implementation; absent from ordinary inclusion.
struct resource_scheduling_test_access {
    using implementation = resource_scheduling_problem::implementation;
    using problem_data = implementation::problem_data;
    using builtin_evaluator = implementation::builtin_evaluator;
    using calendar_selection = implementation::calendar_selection;
    using interval_calendars = implementation::interval_calendars;
    using dense_calendars = implementation::dense_calendars;
    static problem_data make_problem_data(const resource_scheduling_problem& p) {
        return implementation::validate_problem_or_throw(p);
    }
    static problem_data validate_problem_or_throw(const resource_scheduling_problem& p) {
        return implementation::validate_problem_or_throw(p);
    }
    static bool validate_schedule(const resource_scheduling_problem& p, const problem_data& d, const resource_schedule& s) {
        return implementation::validate_schedule(p, d, s);
    }
    static bool select_dense_calendar(const resource_scheduling_problem& p, const problem_data& d) {
        return implementation::select_dense_calendar(p, d);
    }
    template<class Evaluator, class Calendars = interval_calendars>
    using schedule_decoder = implementation::schedule_decoder<Evaluator, Calendars>;
    template<class Evaluator>
    static resource_scheduling_result run_solver(const resource_scheduling_problem& p, const resource_schedule* s,
        Evaluator e, const resource_scheduling_options& o, calendar_selection c = calendar_selection::automatic) {
        return implementation::run_solver(p, s, std::move(e), o, c);
    }
};


#include <cassert>
#include <iostream>

namespace {
long long actual_search_checks = 0;

resource_scheduling_problem make_small_problem() {
    resource_scheduling_problem problem;
    problem.resources = {{2}, {1}};
    problem.tasks.resize(6);
    for (int task = 0; task < 6; ++task) {
        problem.tasks[task].release_time = task % 2;
        problem.tasks[task].due_time = 8 + task;
        problem.tasks[task].completion_weight = task == 5 ? 3 : 0;
        problem.tasks[task].tardiness_weight = task == 5 ? 5 : 1;
        problem.tasks[task].modes = {
            {2 + task % 3, {{0, 1}}, static_cast<long double>(task % 2)},
            {1 + task % 2, {{0, 2}, {1, 1}}, static_cast<long double>(3 + task % 2)},
        };
    }
    problem.precedences = {{0, 2}, {1, 2}, {2, 4}, {3, 5}, {4, 5}};
    return problem;
}

void test_chain() {
    resource_scheduling_problem problem;
    problem.tasks.resize(4);
    for (auto& task : problem.tasks) task.modes = {{3, {}, 0}};
    problem.precedences = {{0, 1}, {1, 2}, {2, 3}};
    resource_scheduling_options options;
    options.time_limit_ms = 0;
    const auto result = resource_scheduling_solve(problem, options);
    assert(resource_scheduling_is_feasible(problem, result.schedule));
    assert(result.schedule.start_time[0] == 0);
    assert(result.schedule.start_time[1] == 3);
    assert(result.schedule.start_time[2] == 6);
    assert(result.schedule.start_time[3] == 9);
    assert(result.objective == 12);
}

void test_capacity() {
    resource_scheduling_problem problem;
    problem.resources = {{2}};
    problem.tasks.resize(3);
    for (auto& task : problem.tasks) task.modes = {{4, {{0, 1}}, 0}};
    resource_scheduling_options options;
    options.time_limit_ms = 0;
    const auto result = resource_scheduling_solve(problem, options);
    assert(resource_scheduling_is_feasible(problem, result.schedule));
    std::vector<long long> starts = result.schedule.start_time;
    std::sort(starts.begin(), starts.end());
    assert(starts == std::vector<long long>({0, 0, 4}));
    assert(result.objective == 8);
}

void test_search_and_improve() {
    const auto problem = make_small_problem();
    resource_scheduling_options initial_options;
    initial_options.time_limit_ms = 0;
    const auto initial = resource_scheduling_solve(problem, initial_options);
    assert(resource_scheduling_is_feasible(problem, initial.schedule));
    assert(resource_scheduling_evaluate(problem, initial.schedule) == initial.objective);

    resource_scheduling_options options;
    options.time_limit_ms = 10000;
    options.max_iterations = 1000;
    options.seed = 17;
    const auto improved = resource_scheduling_improve(problem, initial.schedule, options);
    assert(resource_scheduling_is_feasible(problem, improved.schedule));
    assert(resource_scheduling_evaluate(problem, improved.schedule) == improved.objective);
    assert(improved.objective <= initial.objective);
}

void test_custom_evaluator() {
    const auto problem = make_small_problem();
    struct evaluator {
        long double operator()(
            const resource_scheduling_problem& problem,
            const resource_schedule& schedule) const {
            long double value = 0;
            for (int task = 0; task < static_cast<int>(problem.tasks.size()); ++task) {
                const auto& mode =
                    problem.tasks[task].modes[schedule.mode_of_task[task]];
                value += (task + 1) * (schedule.start_time[task] + mode.duration);
            }
            return value;
        }
    };
    resource_scheduling_options options;
    options.time_limit_ms = 10000;
    options.max_iterations = 300;
    const auto result = resource_scheduling_solve(problem, evaluator{}, options);
    assert(resource_scheduling_is_feasible(problem, result.schedule));
    assert(resource_scheduling_evaluate(problem, result.schedule, evaluator{}) ==
           result.objective);
}

void test_invalid_problem() {
    resource_scheduling_problem problem;
    problem.tasks.resize(2);
    for (auto& task : problem.tasks) task.modes = {{1, {}, 0}};
    problem.precedences = {{0, 1}, {1, 0}};
    bool thrown = false;
    try {
        resource_scheduling_validate_problem(problem);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    assert(thrown);
}

void test_large_time_values() {
    resource_scheduling_problem problem;
    problem.resources = {{1}};
    problem.tasks.resize(3);
    for (int task = 0; task < 3; ++task) {
        problem.tasks[task].release_time = 1'000'000'000;
        problem.tasks[task].modes = {{1'000'000'000, {{0, 1}}, 0}};
    }
    resource_scheduling_options options;
    options.time_limit_ms = 0;
    const auto result = resource_scheduling_solve(problem, options);
    assert(resource_scheduling_is_feasible(problem, result.schedule));
    assert(result.objective == 4'000'000'000LL);
}

void test_empty_and_zero_duration() {
    resource_scheduling_problem empty;
    resource_scheduling_options options;
    options.time_limit_ms = 0;
    const auto empty_result = resource_scheduling_solve(empty, options);
    assert(empty_result.schedule.task_order.empty());
    assert(empty_result.schedule.mode_of_task.empty());
    assert(empty_result.schedule.start_time.empty());
    assert(empty_result.objective == 0);

    resource_scheduling_problem problem;
    problem.resources = {{1}};
    problem.tasks.resize(3);
    for (auto& task : problem.tasks) {
        task.modes = {{0, {{0, 1}}, 0}};
    }
    const auto result = resource_scheduling_solve(problem, options);
    assert(resource_scheduling_is_feasible(problem, result.schedule));
    assert(result.schedule.start_time == std::vector<long long>({0, 0, 0}));
    assert(result.objective == 0);
}

void test_option_and_schedule_validation() {
    resource_scheduling_problem problem;
    problem.tasks.resize(1);
    problem.tasks[0].modes = {{1, {}, 0}};

    resource_scheduling_options bad_options;
    bad_options.time_limit_ms = std::numeric_limits<double>::infinity();
    bool thrown = false;
    try {
        (void)resource_scheduling_solve(problem, bad_options);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    assert(thrown);

    resource_schedule overflow;
    overflow.task_order = {0};
    overflow.mode_of_task = {0};
    overflow.start_time = {std::numeric_limits<long long>::max()};
    assert(!resource_scheduling_is_feasible(problem, overflow));
    thrown = false;
    try {
        (void)resource_scheduling_evaluate(problem, overflow);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    assert(thrown);

    resource_schedule malformed;
    malformed.task_order = {0};
    malformed.mode_of_task = {1};
    malformed.start_time = {0};
    thrown = false;
    try {
        (void)resource_scheduling_improve(problem, malformed, {});
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    assert(thrown);
}

void test_nonfinite_evaluator_and_determinism() {
    const auto problem = make_small_problem();
    struct nonfinite_evaluator {
        long double operator()(
            const resource_scheduling_problem&,
            const resource_schedule&) const {
            return std::numeric_limits<long double>::infinity();
        }
    };
    resource_scheduling_options zero_options;
    zero_options.time_limit_ms = 0;
    bool thrown = false;
    try {
        (void)resource_scheduling_solve(
            problem, nonfinite_evaluator{}, zero_options);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    assert(thrown);
    const auto feasible = resource_scheduling_solve(problem, zero_options);
    thrown = false;
    try {
        (void)resource_scheduling_evaluate(
            problem, feasible.schedule, nonfinite_evaluator{});
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    assert(thrown);

    resource_scheduling_options options;
    options.time_limit_ms = 10000;
    options.max_iterations = 500;
    options.seed = 987654321;
    const auto first = resource_scheduling_solve(problem, options);
    const auto second = resource_scheduling_solve(problem, options);
    assert(first.objective == second.objective);
    assert(first.iterations == second.iterations);
    assert(first.schedule.task_order == second.schedule.task_order);
    assert(first.schedule.mode_of_task == second.schedule.mode_of_task);
    assert(first.schedule.start_time == second.schedule.start_time);
}

void test_calendar_cost_model() {
    resource_scheduling_problem problem;
    problem.resources = {{2}, {2}};
    problem.tasks.resize(128);
    for (auto& task : problem.tasks) {
        task.modes = {{2, {{0, 1}, {1, 1}}, 0}};
    }
    auto data = resource_scheduling_test_access::make_problem_data(problem);
    assert(resource_scheduling_test_access::select_dense_calendar(problem, data));
    for (auto& task : problem.tasks) task.modes[0].duration = 1000;
    data = resource_scheduling_test_access::make_problem_data(problem);
    assert(!resource_scheduling_test_access::select_dense_calendar(problem, data));
}

void test_random_decoder_against_dense_reference() {
    std::mt19937 random(1234567);
    for (int test = 0; test < 500; ++test) {
        resource_scheduling_problem problem;
        const int n = 2 + static_cast<int>(random() % 10);
        const int resource_count = 1 + static_cast<int>(random() % 4);
        problem.resources.resize(resource_count);
        for (auto& resource : problem.resources) {
            resource.capacity = 1 + static_cast<int>(random() % 4);
        }
        problem.tasks.resize(n);
        for (int task = 0; task < n; ++task) {
            problem.tasks[task].release_time = static_cast<int>(random() % 8);
            const int mode_count = 1 + static_cast<int>(random() % 3);
            for (int mode_id = 0; mode_id < mode_count; ++mode_id) {
                scheduling_mode mode;
                mode.duration = static_cast<int>(random() % 6);
                for (int resource = 0; resource < resource_count; ++resource) {
                    if (random() % 2 == 0) continue;
                    mode.renewable_uses.push_back({
                        resource,
                        1 + static_cast<int>(
                                random() % problem.resources[resource].capacity),
                    });
                }
                problem.tasks[task].modes.push_back(std::move(mode));
            }
        }
        for (int before = 0; before < n; ++before) {
            for (int after = before + 1; after < n; ++after) {
                if (random() % 7 == 0) problem.precedences.push_back({before, after});
            }
        }
        resource_scheduling_validate_problem(problem);
        const auto data = resource_scheduling_test_access::make_problem_data(problem);
        std::vector<int> order = data.topological_order;
        std::vector<int> modes(n);
        for (int task = 0; task < n; ++task) {
            modes[task] = static_cast<int>(random() % problem.tasks[task].modes.size());
        }

        resource_scheduling_test_access::builtin_evaluator evaluator;
        resource_scheduling_test_access::schedule_decoder decoder(problem, data, evaluator);
        decoder.decode(order, modes);
        resource_scheduling_test_access::schedule_decoder<
            resource_scheduling_test_access::builtin_evaluator,
            resource_scheduling_test_access::dense_calendars>
            dense_decoder(problem, data, evaluator);
        dense_decoder.decode(order, modes);

        std::vector<std::vector<int>> load(
            resource_count, std::vector<int>(static_cast<std::size_t>(data.horizon), 0));
        std::vector<long long> dense_start(n);
        for (int task : order) {
            const auto& task_data = problem.tasks[task];
            const auto& mode = task_data.modes[modes[task]];
            long long start = task_data.release_time;
            for (int predecessor : data.predecessors[task]) {
                start = std::max(
                    start,
                    dense_start[predecessor] +
                        problem.tasks[predecessor].modes[modes[predecessor]].duration);
            }
            while (true) {
                bool fits = true;
                for (const auto& use : mode.renewable_uses) {
                    for (long long time = start; time < start + mode.duration; ++time) {
                        if (load[use.resource][static_cast<std::size_t>(time)] +
                                use.amount >
                            problem.resources[use.resource].capacity) {
                            fits = false;
                            break;
                        }
                    }
                    if (!fits) break;
                }
                if (fits) break;
                ++start;
            }
            dense_start[task] = start;
            for (const auto& use : mode.renewable_uses) {
                for (long long time = start; time < start + mode.duration; ++time) {
                    load[use.resource][static_cast<std::size_t>(time)] += use.amount;
                }
            }
        }
        assert(decoder.schedule.start_time == dense_start);
        assert(dense_decoder.schedule.start_time == dense_start);
        assert(resource_scheduling_is_feasible(problem, decoder.schedule));
    }
}

}  // namespace


namespace {

// 不正解の資源使用量がintで循環して、容量内と誤判定されないことを確認する
void test_capacity_overflow_rejection() {
    resource_scheduling_problem p;
    p.resources = {{2000000000}};
    p.tasks.resize(5);
    for (auto& t : p.tasks) t.modes = {{1, {{0, 1000000000}}, 0}};
    resource_schedule s;
    s.task_order = {0, 1, 2, 3, 4};
    s.mode_of_task.assign(5, 0);
    s.start_time.assign(5, 0);
    assert(!resource_scheduling_is_feasible(p, s));
}

// イベント候補を全走査する低速SSGS。内部calendarを使わない
std::vector<long long> reference_starts(const resource_scheduling_problem& p,
                                      const std::vector<int>& order,
                                      const std::vector<int>& modes) {
    const int n = static_cast<int>(p.tasks.size());
    std::vector<long long> starts(n, 0), finish(n, 0);
    std::vector<int> placed;
    for (int t : order) {
        const auto& mode = p.tasks[t].modes[modes[t]];
        long long earliest = p.tasks[t].release_time;
        for (auto e : p.precedences) if (e.after == t) earliest = std::max(earliest, finish[e.before]);
        std::vector<long long> candidates = {earliest};
        for (int a : placed) if (finish[a] >= earliest) candidates.push_back(finish[a]);
        std::sort(candidates.begin(), candidates.end());
        for (long long start : candidates) {
            bool fits = true;
            if (mode.duration > 0) for (auto use : mode.renewable_uses) {
                std::vector<long long> times = {start};
                for (int a : placed) {
                    if (starts[a] > start && starts[a] < start + mode.duration) times.push_back(starts[a]);
                    if (finish[a] > start && finish[a] < start + mode.duration) times.push_back(finish[a]);
                }
                for (auto time : times) {
                    long long load = use.amount;
                    for (int a : placed) if (starts[a] <= time && time < finish[a]) {
                        for (auto old_use : p.tasks[a].modes[modes[a]].renewable_uses)
                            if (old_use.resource == use.resource) load += old_use.amount;
                    }
                    if (load > p.resources[use.resource].capacity) { fits = false; break; }
                }
                if (!fits) break;
            }
            if (fits) { starts[t] = start; finish[t] = start + mode.duration; break; }
        }
        placed.push_back(t);
    }
    return starts;
}

// 資源数0、大capacity、大時刻、duration0、代替資源を交え、候補移動後も照合する
void test_expanded_random_differential() {
    std::mt19937_64 random(78123819);
    auto integer = [&](int bound) { return static_cast<int>(random() % static_cast<unsigned>(bound)); };
    for (int trial = 0; trial < 3000; ++trial) {
        const int n = 1 + integer(16), r = integer(6);
        const int time_scale = trial % 13 == 0 ? 1000000 : 1;
        const int release_offset = trial % 17 == 0 ? 1000000000 : 0;
        resource_scheduling_problem p;
        p.resources.resize(r);
        for (auto& resource : p.resources) resource.capacity = 1 + integer(9);
        p.tasks.resize(n);
        for (int t = 0; t < n; ++t) {
            auto& task = p.tasks[t];
            task.release_time = release_offset + integer(20) * time_scale;
            task.due_time = trial % 2 ? -1 : release_offset + 10 * time_scale;
            task.completion_weight = integer(4);
            task.tardiness_weight = integer(4);
            const int count = 1 + integer(4);
            for (int m = 0; m < count; ++m) {
                scheduling_mode mode;
                mode.duration = integer(9) * time_scale;
                mode.fixed_cost = integer(21) - 10;
                for (int j = 0; j < r; ++j) if (integer(3) != 0)
                    mode.renewable_uses.push_back({j, 1 + integer(p.resources[j].capacity)});
                task.modes.push_back(std::move(mode));
            }
            for (int a = 0; a < t; ++a) if (integer(7) == 0) p.precedences.push_back({a, t});
        }
        // IDを反転することで、task ID順の処理に依存した誤実装を検出する
        std::reverse(p.tasks.begin(), p.tasks.end());
        for (auto& e : p.precedences) { e.before = n - 1 - e.before; e.after = n - 1 - e.after; }
        const auto data = resource_scheduling_test_access::validate_problem_or_throw(p);
        const auto verify = [&](const auto& problem, const resource_schedule& schedule) {
            assert(schedule.start_time == reference_starts(problem, schedule.task_order, schedule.mode_of_task));
            assert(resource_scheduling_is_feasible(problem, schedule));
            ++actual_search_checks;
            return resource_scheduling_test_access::builtin_evaluator{}(problem, schedule);
        };
        resource_scheduling_options options;
        options.seed = random();
        options.time_limit_ms = 1e9;
        options.max_iterations = 32;
        resource_scheduling_test_access::run_solver(p, nullptr, verify, options,
            resource_scheduling_test_access::calendar_selection::interval);
        if (data.horizon < 100000) resource_scheduling_test_access::run_solver(p, nullptr, verify, options,
            resource_scheduling_test_access::calendar_selection::dense);
    }
}

// 全mode・全順序を列挙する小規模oracle。近似解を厳密解と取り違えない
void test_tiny_exhaustive_oracle() {
    std::mt19937 random(288291);
    for (int trial = 0; trial < 30; ++trial) {
        const int n = 2 + static_cast<int>(random() % 4);
        resource_scheduling_problem p;
        p.resources = {{2}, {1}};
        p.tasks.resize(n);
        for (int t = 0; t < n; ++t) {
            p.tasks[t].completion_weight = 1 + t;
            p.tasks[t].modes = {{1 + static_cast<int>(random() % 6), {{0, 1}}, 2},
                                {1 + static_cast<int>(random() % 6), {{0, 2}, {1, 1}}, -1}};
        }
        resource_scheduling_test_access::builtin_evaluator evaluator;
        std::vector<int> order(n);
        std::iota(order.begin(), order.end(), 0);
        long double optimum = std::numeric_limits<long double>::infinity();
        do {
            for (int mask = 0; mask < (1 << n); ++mask) {
                resource_schedule s;
                s.task_order = order;
                s.mode_of_task.resize(n);
                for (int t = 0; t < n; ++t) s.mode_of_task[t] = (mask >> t) & 1;
                s.start_time = reference_starts(p, order, s.mode_of_task);
                optimum = std::min(optimum, evaluator(p, s));
            }
        } while (std::next_permutation(order.begin(), order.end()));
        resource_scheduling_options o;
        o.time_limit_ms = 100000;
        o.max_iterations = 300;
        const auto result = resource_scheduling_solve(p, o);
        assert(result.objective >= optimum);
        assert(resource_scheduling_is_feasible(p, result.schedule));
    }
}

void test_deadline_and_changed_problem() {
    auto p = make_small_problem();
    resource_scheduling_options o;
    o.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    const auto initial = resource_scheduling_solve(p, o);
    assert(initial.iterations == 0 && initial.decoded_schedules == 1);
    assert(resource_scheduling_is_feasible(p, initial.schedule));
    p.resources[0].capacity += 1;
    p.tasks[0].completion_weight += 10;
    const auto improved = resource_scheduling_improve(p, initial.schedule, o);
    assert(improved.objective <= resource_scheduling_evaluate(p, initial.schedule));
    p.tasks[0].release_time = static_cast<int>(initial.schedule.start_time[0]) + 1;
    bool thrown = false;
    try { (void)resource_scheduling_improve(p, initial.schedule, o); }
    catch (const std::invalid_argument&) { thrown = true; }
    assert(thrown);
}

void test_calendar_memory_boundary_and_zero_objective() {
    resource_scheduling_problem p;
    p.resources.assign(8, {1});
    p.tasks.resize(16);
    for (auto& t : p.tasks) t.modes = {{1, {{0, 1}}, 0}};
    p.tasks[0].release_time = 499983;
    auto data = resource_scheduling_test_access::make_problem_data(p);
    assert(data.horizon * 8 == 4000000);
    assert(resource_scheduling_test_access::select_dense_calendar(p, data));
    ++p.tasks[0].release_time;
    data = resource_scheduling_test_access::make_problem_data(p);
    assert(!resource_scheduling_test_access::select_dense_calendar(p, data));
    p.makespan_weight = p.mode_cost_weight = 0;
    resource_scheduling_options o;
    o.max_iterations = 100;
    o.time_limit_ms = 100000;
    const auto result = resource_scheduling_solve(p, o);
    assert(result.objective == 0 && result.iterations == 100);
    assert(resource_scheduling_is_feasible(p, result.schedule));
}

}  // namespace

namespace {
template<class Calendar>
void check_round3_decode_paths(const resource_scheduling_problem& p) {
    const auto verify = [&](const auto& problem, const resource_schedule& schedule) {
        assert(schedule.start_time == reference_starts(problem, schedule.task_order, schedule.mode_of_task));
        assert(resource_scheduling_is_feasible(problem, schedule));
        ++actual_search_checks;
        return resource_scheduling_test_access::builtin_evaluator{}(problem, schedule);
    };
    resource_scheduling_options options;
    options.seed = 312871;
    options.time_limit_ms = 1e9;
    options.max_iterations = 400;
    resource_scheduling_test_access::run_solver(p, nullptr, verify, options,
        std::is_same_v<Calendar, resource_scheduling_test_access::dense_calendars> ?
        resource_scheduling_test_access::calendar_selection::dense : resource_scheduling_test_access::calendar_selection::interval);
}

void test_round3_paths() {
    auto p = make_small_problem();
    check_round3_decode_paths<resource_scheduling_test_access::interval_calendars>(p);
    check_round3_decode_paths<resource_scheduling_test_access::dense_calendars>(p);
    auto large_capacity = p;
    large_capacity.resources = {{2000000000}};
    large_capacity.tasks.assign(5, {});
    large_capacity.precedences.clear();
    for (auto& task : large_capacity.tasks)
        task.modes = {{2, {{0, 1000000000}}, 0}, {1, {{0, 2000000000}}, 1}};
    check_round3_decode_paths<resource_scheduling_test_access::interval_calendars>(large_capacity);
    check_round3_decode_paths<resource_scheduling_test_access::dense_calendars>(large_capacity);
    p.resources = {{2}};
    p.tasks.assign(8, {});
    p.precedences = {{0, 4}, {2, 5}, {4, 7}};
    for (int t = 0; t < 8; ++t) {
        p.tasks[t].release_time = 1000000000;
        p.tasks[t].modes = {{700000000, {{0, 1}}, 1}, {900000000, {{0, 2}}, -1}};
    }
    const auto data = resource_scheduling_test_access::make_problem_data(p);
    assert(data.horizon > INT_MAX);
    check_round3_decode_paths<resource_scheduling_test_access::interval_calendars>(p);
    resource_scheduling_options o;
    o.time_limit_ms = 100000;
    o.max_iterations = 200;
    const auto r = resource_scheduling_solve(p, o);
    assert(resource_scheduling_is_feasible(p, r.schedule));

    // 短いdurationでも疎な時刻領域を持つ問題を正しく扱えること。
    p.tasks.assign(32, {});
    p.precedences.clear();
    p.resources = {{1}};
    for (auto& t : p.tasks) t.modes = {{2, {{0, 1}}, 0}};
    p.tasks[0].release_time = 1500000;
    o.max_iterations = 8;
    const auto sparse = resource_scheduling_solve(p, o);
    assert(resource_scheduling_is_feasible(p, sparse.schedule));
}
}

namespace {
void test_single_state_against_enumeration() {
    int checked = 0;
    for (int n = 1; n <= 5; ++n) {
        const int possible_edges = n * (n - 1) / 2;
        for (int mask = 0; mask < (1 << possible_edges); ++mask) {
            resource_scheduling_problem p;
            p.resources = {{2}, {1}};
            p.tasks.resize(n);
            for (int t = 0; t < n; ++t) {
                p.tasks[t].release_time = t % 3;
                p.tasks[t].completion_weight = t + 1;
                p.tasks[t].modes = {{t + 1, {{0, 1}, {1, 1}}, -2}};
            }
            int bit = 0;
            for (int a = 0; a < n; ++a) for (int b = a + 1; b < n; ++b, ++bit)
                if (mask & (1 << bit)) p.precedences.push_back({n - 1 - a, n - 1 - b});
            std::vector<int> order(n), position(n);
            std::iota(order.begin(), order.end(), 0);
            int topological_count = 0;
            do {
                for (int i = 0; i < n; ++i) position[order[i]] = i;
                if (std::all_of(p.precedences.begin(), p.precedences.end(),
                    [&](const scheduling_precedence& e) { return position[e.before] < position[e.after]; })) ++topological_count;
                if (topological_count > 1) break;
            } while (std::next_permutation(order.begin(), order.end()));
            const bool single = topological_count == 1;
            const auto data = resource_scheduling_test_access::make_problem_data(p);
            assert(data.single_search_state == single);
            resource_scheduling_options o;
            o.time_limit_ms = 100000;
            o.max_iterations = 64;
            o.seed = 3137;
            const auto result = resource_scheduling_solve(p, o);
            assert(resource_scheduling_is_feasible(p, result.schedule));
            assert(result.objective == resource_scheduling_evaluate(p, result.schedule));
            if (single) {
                assert(result.iterations == 0 && result.decoded_schedules == 1 && result.accepted_moves == 0);
                assert(result.schedule.start_time == reference_starts(p, data.topological_order, std::vector<int>(n, 0)));
                auto warm = result.schedule;
                for (auto& time : warm.start_time) time += 7;
                auto shifted = [](const resource_scheduling_problem& problem, const resource_schedule& schedule) {
                    return resource_scheduling_test_access::builtin_evaluator{}(problem, schedule) - 1000000;
                };
                const auto improved = resource_scheduling_improve(p, warm, shifted, o);
                assert(improved.iterations == 0 && improved.decoded_schedules == 1);
                assert(improved.objective <= shifted(p, warm));
                assert(improved.objective == shifted(p, improved.schedule));
                // 待ち時間を好むEvaluatorでは、SSGSより良い外部解をそのまま守る。
                auto prefer_later = [](const resource_scheduling_problem& problem, const resource_schedule& schedule) {
                    return -resource_scheduling_test_access::builtin_evaluator{}(problem, schedule) - 1000000;
                };
                const auto retained = resource_scheduling_improve(p, warm, prefer_later, o);
                assert(retained.iterations == 0 && retained.decoded_schedules == 1);
                assert(retained.schedule.start_time == warm.start_time);
                assert(retained.objective == prefer_later(p, warm));
            } else assert(result.iterations == o.max_iterations);
            // 複数modeなら順序が一意でも探索状態は1個ではない。
            p.tasks[0].modes.push_back({n + 3, {{0, 2}}, -4});
            assert(!resource_scheduling_test_access::make_problem_data(p).single_search_state);
            const auto multi = resource_scheduling_solve(p, o);
            assert(multi.iterations == o.max_iterations);
            assert(resource_scheduling_is_feasible(p, multi.schedule));
            ++checked;
        }
    }
    assert(checked == 1099);
}
}

#define TEST_PORTFOLIO_PHASE
namespace {
void test_quality_candidates_boundaries() {
    resource_scheduling_problem p;
    p.resources = {{3}, {2}};
    p.tasks.resize(7);
    for (int t = 0; t < 7; ++t) {
        p.tasks[t].modes = {{t + 1, {{0, 1}}, -2}, {8 - t, {{1, 1}}, 3}};
        p.tasks[t].completion_weight = t + 1;
        p.tasks[t].due_time = 12 + t;
        p.tasks[t].tardiness_weight = 2;
    }
    p.precedences = {{0, 3}, {1, 3}, {2, 4}, {3, 5}, {4, 6}};
    for (long long cap = 0; cap <= 8; ++cap) {
        resource_scheduling_options o;
        o.max_iterations = cap;
        o.time_limit_ms = 1000000;
        o.seed = 317;
        const auto result = resource_scheduling_solve(p, o);
        assert(result.iterations <= cap && result.accepted_moves <= result.iterations);
        assert(resource_scheduling_is_feasible(p, result.schedule));
        assert(result.objective == resource_scheduling_evaluate(p, result.schedule));
        if (cap == 0) assert(result.iterations == 0 && result.decoded_schedules == 1);
    }
    resource_scheduling_options o;
    o.max_iterations = 0;
    auto warm = resource_scheduling_solve(p, o).schedule;
    for (auto& time : warm.start_time) time += 1000;
    auto prefer_later = [](const resource_scheduling_problem& problem, const resource_schedule& schedule) {
        return -resource_scheduling_test_access::builtin_evaluator{}(problem, schedule) - 1000000;
    };
    o.max_iterations = 300;
    o.time_limit_ms = 1000000;
    const auto improved = resource_scheduling_improve(p, warm, prefer_later, o);
    assert(resource_scheduling_is_feasible(p, improved.schedule));
    assert(improved.objective <= prefer_later(p, warm));
    assert(improved.objective == prefer_later(p, improved.schedule));
    o.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    const auto expired = resource_scheduling_improve(p, warm, prefer_later, o);
    assert(expired.iterations == 0 && expired.decoded_schedules == 1);
    assert(expired.objective == prefer_later(p, warm));
#ifdef TEST_PORTFOLIO_PHASE
    // 追加順序のdecode中に期限を過ぎたら、SAへ入って余分な8提案をしない。
    auto slow = [](const resource_scheduling_problem& problem, const resource_schedule& schedule) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return resource_scheduling_test_access::builtin_evaluator{}(problem, schedule);
    };
    o.deadline.reset();
    o.time_limit_ms = 1;
    o.max_iterations = 100;
    const auto stopped = resource_scheduling_solve(p, slow, o);
    assert(stopped.iterations <= 1 && stopped.decoded_schedules <= 2);
    assert(resource_scheduling_is_feasible(p, stopped.schedule));
#endif
}
}

int main() {
    test_quality_candidates_boundaries();
    test_single_state_against_enumeration();
    test_round3_paths();
    test_chain();
    test_capacity();
    test_search_and_improve();
    test_custom_evaluator();
    test_invalid_problem();
    test_large_time_values();
    test_empty_and_zero_duration();
    test_option_and_schedule_validation();
    test_nonfinite_evaluator_and_determinism();
    test_calendar_cost_model();
    test_random_decoder_against_dense_reference();
    test_capacity_overflow_rejection();
    test_expanded_random_differential();
    test_tiny_exhaustive_oracle();
    test_deadline_and_changed_problem();
    test_calendar_memory_boundary_and_zero_objective();
    assert(actual_search_checks > 24000);
    std::cout << "actual search callbacks checked: " << actual_search_checks << "\n";
    std::cout << "all tests passed: legacy + overflow + actual-search SSGS checks + 30 exact oracles + prefix/boundary tests + 1099 exhaustive uniqueness cases\n";
}

#endif
