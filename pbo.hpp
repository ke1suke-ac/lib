#pragma once
#include <bits/stdc++.h>

// 疎な0–1最適化。正負の整数利益、上下限、必須条件、線形・一律減点を扱う
// 全ての行和・減点・差分・探索用上界はlong longに収まること。LLONG_MINは使用しない
// 時間はマイクロ秒。構築・初期評価・終了処理による超過は許容する前提で余裕を取る
enum class PboRowKind { Hard, LinearPenalty, FixedPenalty };

struct PboTerm {
    int var;
    long long coef;
};

struct PboRow {
    std::vector<PboTerm> terms;
    std::optional<long long> lower, upper;
    PboRowKind kind = PboRowKind::Hard;
    long long weight = 1;
};

struct PboProblem {
    std::vector<long long> profit;
    std::vector<PboRow> rows;
    std::vector<int8_t> fixed;
    long long constant = 0;

    // 行を追加してIDを返す O(1)、再確保時O(行数)
    int add_row(PboRow row) {
        rows.push_back(std::move(row));
        return (int)rows.size() - 1;
    }

    // 上限制約または上限超過の減点を追加する O(1)、再確保時O(行数)
    int add_le(std::vector<PboTerm> terms, long long upper,
               PboRowKind kind = PboRowKind::Hard, long long weight = 1) {
        return add_row({std::move(terms), std::nullopt, upper, kind, weight});
    }

    // 下限制約または下限不足の減点を追加する O(1)、再確保時O(行数)
    int add_ge(std::vector<PboTerm> terms, long long lower,
               PboRowKind kind = PboRowKind::Hard, long long weight = 1) {
        return add_row({std::move(terms), lower, std::nullopt, kind, weight});
    }

    // 等式制約または目標からの減点を追加する O(1)、再確保時O(行数)
    int add_eq(std::vector<PboTerm> terms, long long value,
               PboRowKind kind = PboRowKind::Hard, long long weight = 1) {
        return add_row({std::move(terms), value, value, kind, weight});
    }

    // 省略可能な上下限を持つ行を追加する O(1)、再確保時O(行数)
    int add_range(std::vector<PboTerm> terms, std::optional<long long> lower,
                  std::optional<long long> upper, PboRowKind kind = PboRowKind::Hard,
                  long long weight = 1) {
        return add_row({std::move(terms), lower, upper, kind, weight});
    }

    // 指定変数を全て選ぶ場合の正負の利益を追加する O(k log k)、kは指定数
    int add_all_profit(std::vector<int> vars, long long value) {
        // 重複変数を除き、定数と一律減点に変換する
        std::sort(vars.begin(), vars.end());
        vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
        std::vector<PboTerm> terms;
        for (int v : vars) terms.push_back({v, 1});
        long long count = (long long)vars.size();
        if (value >= 0) {
            constant += value;
            return add_ge(std::move(terms), count, PboRowKind::FixedPenalty, value);
        }
        return add_le(std::move(terms), count - 1, PboRowKind::FixedPenalty, -value);
    }
};

struct PboEvaluation {
    long long objective = 0;
    int violated_hard_rows = 0;
    int violated_fixed_variables = 0;

    // 必須条件と固定値を満たしているか返す O(1)
    bool feasible() const {
        return violated_hard_rows == 0 && violated_fixed_variables == 0;
    }
};

struct PboResult {
    std::vector<uint8_t> x;
    PboEvaluation evaluation;
    int64_t iterations = 0;
    double elapsed_us = 0;
};

struct PboParam {
    double time_limit_us = 1'950'000.0;
    std::optional<std::chrono::steady_clock::time_point> deadline;
    int64_t max_iterations = -1;
};

class PboSolver {
    using Clock = std::chrono::steady_clock;
    struct Term { long long coef; int var, row; };
    struct Row {
        long long lower, upper; // 省略境界はLLONG_MIN/MAX。差を取る前に値を挟む
        PboRowKind kind;
        long long weight;
        long long amount = 0; // 現在の行違反量。load_stateと反転で更新
        double scale = 1, search_weight = 1;
    };
    struct Rng {
        uint64_t state;
        uint64_t next() {
            uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return z ^ (z >> 31);
        }
        int pick(int n) { return (int)(((next() >> 32) * (uint64_t)n) >> 32); }
        double unit() { return (double)(next() >> 11) * 0x1.0p-53; }
    };
    struct Delta {
        long long objective = 0;
        double violation = 0, gain = 0;
    };

    // 比較実験で選んだ小さな固定構成を使う
    static constexpr int CHAIN_DEPTH = 12;
    static constexpr int CANDIDATES = 12;
    static constexpr int LNS_NODES = 256;
    static constexpr int LNS_INTERVAL = 512;
    static constexpr int TEMPERATURE_PERIOD = 4096;

    std::vector<long long> profit_;
    std::vector<int8_t> fixed_;
    long long constant_ = 0;
    std::vector<Row> rows_;
    std::vector<Term> terms_;
    std::vector<long long> column_coef_;
    std::vector<int> column_row_;
    std::vector<int> row_begin_, col_begin_, col_pos_, soft_rows_;
    std::vector<uint8_t> x_, movable_, locked_;
    std::vector<long long> sums_;
    std::vector<int> bad_rows_, bad_pos_, free_;
    long long linear_ = 0, objective_ = 0;
    double violation_ = 0, best_violation_ = 0, objective_scale_ = 1;
    PboResult best_;
    bool initialized_ = false, dirty_ = true, stopped_ = false;
    Rng rng_;
    Clock::time_point end_;
    int64_t iterations_ = 0, limit_ = -1, last_improvement_ = 0;

    static long long violation(const Row& row, long long value) {
        return std::max(row.lower, value) - std::min(row.upper, value);
    }

    static long long penalty(const Row& row, long long v) {
        if (row.kind == PboRowKind::Hard) return 0;
        return row.weight * (row.kind == PboRowKind::FixedPenalty ? (v != 0) : v);
    }

    void validate_bits(std::span<const uint8_t> x) const {
        assert(x.size() == profit_.size());
        for ([[maybe_unused]] auto v : x) assert(v <= 1);
    }

    void compile(PboProblem problem) {
        // 重複項を集約し、行IDと変数IDを保った連続配列を作る
        profit_ = std::move(problem.profit);
        fixed_ = std::move(problem.fixed);
        constant_ = problem.constant;
        int n = (int)profit_.size();
        if (fixed_.empty()) fixed_.assign(n, -1);
        assert((int)fixed_.size() == n);
        for ([[maybe_unused]] int f : fixed_) assert(-1 <= f && f <= 1);
        rows_.clear(); terms_.clear(); row_begin_.clear();
        row_begin_.push_back(0);
        for (auto& row : problem.rows) {
            assert(!row.lower || !row.upper || *row.lower <= *row.upper);
            assert(row.kind == PboRowKind::Hard || row.weight >= 0);
            int id = (int)rows_.size();
            rows_.push_back({row.lower.value_or(LLONG_MIN), row.upper.value_or(LLONG_MAX),
                             row.kind, row.weight});
            std::sort(row.terms.begin(), row.terms.end(),
                 [](const auto& a, const auto& b) { return a.var < b.var; });
            for (int i = 0; i < (int)row.terms.size();) {
                int var = row.terms[i].var;
                assert(0 <= var && var < n);
                long long coef = 0;
                do { coef += row.terms[i++].coef; }
                while (i < (int)row.terms.size() && row.terms[i].var == var);
                if (coef) terms_.push_back({coef, var, id});
            }
            row_begin_.push_back((int)terms_.size());
        }
        // 列ごとの係数・行IDを連続配置し、係数変更用に行側からの位置も保持する
        col_begin_.assign(n + 1, 0);
        for (const auto& term : terms_) ++col_begin_[term.var + 1];
        std::partial_sum(col_begin_.begin(), col_begin_.end(), col_begin_.begin());
        auto next = col_begin_;
        col_pos_.resize(terms_.size());
        column_coef_.resize(terms_.size()); column_row_.resize(terms_.size());
        for (int t = 0; t < (int)terms_.size(); ++t) {
            col_pos_[t] = next[terms_[t].var]++;
            column_coef_[col_pos_[t]] = terms_[t].coef;
            column_row_[col_pos_[t]] = terms_[t].row;
        }
        sums_.resize(rows_.size());
        bad_pos_.resize(rows_.size());
        movable_.resize(n); locked_.assign(n, 0);
        initialized_ = false;
        dirty_ = true;
    }

    void scales() {
        // 利益と行の尺度は真の整数評価を書き換えず、探索にだけ使う
        double sum = 0;
        for (long long p : profit_) sum += std::abs((double)p);
        soft_rows_.clear();
        for (int j = 0; j < (int)rows_.size(); ++j) {
            auto& row = rows_[j];
            double a = 0;
            int count = 0;
            for (int t = row_begin_[j]; t < row_begin_[j + 1]; ++t) {
                a += std::abs((double)terms_[t].coef);
                count += terms_[t].coef != 0;
            }
            row.scale = 1 / std::max(1.0, a / std::max(1, count));
            row.search_weight = 1;
            if (row.kind != PboRowKind::Hard && row.weight) {
                soft_rows_.push_back(j);
                sum += (double)row.weight *
                       (row.kind == PboRowKind::LinearPenalty ? a : count);
            }
        }
        objective_scale_ = 1 / std::max(1.0, sum / std::max(1, (int)profit_.size()));
    }

    void load_state(std::vector<uint8_t> x) {
        // 行和と違反集合を再構築する。恒久固定は常に反映する
        x_ = std::move(x);
        linear_ = constant_;
        for (int i = 0; i < (int)x_.size(); ++i) {
            if (fixed_[i] >= 0) x_[i] = (uint8_t)fixed_[i];
            linear_ += profit_[i] * x_[i];
        }
        std::fill(sums_.begin(), sums_.end(), 0);
        for (const auto& t : terms_) sums_[t.row] += t.coef * x_[t.var];
        objective_ = linear_;
        violation_ = 0;
        bad_rows_.clear();
        std::fill(bad_pos_.begin(), bad_pos_.end(), -1);
        for (int j = 0; j < (int)rows_.size(); ++j) {
            auto v = rows_[j].amount = violation(rows_[j], sums_[j]);
            objective_ -= penalty(rows_[j], v);
            if (rows_[j].kind == PboRowKind::Hard) {
                violation_ += (double)v * rows_[j].scale;
                if (v) { bad_pos_[j] = (int)bad_rows_.size(); bad_rows_.push_back(j); }
            }
        }
    }

    void remember() {
        // 実行可能性を優先し、同じ違反量なら真の目的値を比較する
        int bad = (int)bad_rows_.size();
        auto& e = best_.evaluation;
        bool better = !initialized_ || bad < e.violated_hard_rows;
        if (initialized_ && bad == e.violated_hard_rows) {
            double eps = 1e-10 * std::max(1.0, std::abs(best_violation_));
            better = violation_ < best_violation_ - eps ||
                     (std::abs(violation_ - best_violation_) <= eps && objective_ > e.objective);
        }
        if (!better) return;
        best_.x = x_;
        best_.evaluation = {objective_, bad, 0};
        best_violation_ = violation_;
        initialized_ = true;
        last_improvement_ = iterations_;
    }

    template<bool Apply = false>
    Delta delta(int var) {
        // 評価と反転で行の差分計算を共有し、適用時は同じ走査で状態を更新する
        Delta d;
        int sign = 1 - 2 * x_[var];
        d.objective = profit_[var] * sign;
        double weighted = 0;
        for (int c = col_begin_[var]; c < col_begin_[var + 1]; ++c) {
            int j = column_row_[c]; long long coef = column_coef_[c];
            const auto& row = rows_[j];
            long long old_v = row.amount;
            long long new_v = violation(row, sums_[j] + coef * sign);
            d.objective += penalty(row, old_v) - penalty(row, new_v);
            if (row.kind == PboRowKind::Hard) {
                double v = (double)(new_v - old_v) * row.scale;
                d.violation += v;
                weighted += v * row.search_weight;
                if constexpr (Apply) {
                    int pos = bad_pos_[j];
                    if (new_v && pos < 0) {
                        bad_pos_[j] = (int)bad_rows_.size(); bad_rows_.push_back(j);
                    } else if (!new_v && pos >= 0) {
                        int back = bad_rows_.back();
                        bad_rows_[pos] = back; bad_pos_[back] = pos;
                        bad_rows_.pop_back(); bad_pos_[j] = -1;
                    }
                }
            }
            if constexpr (Apply) { sums_[j] += coef * sign; rows_[j].amount = new_v; }
        }
        d.gain = (double)d.objective * objective_scale_ - weighted;
        if constexpr (Apply) {
            x_[var] ^= 1;
            linear_ += profit_[var] * sign;
            objective_ += d.objective;
            violation_ += d.violation;
            if (bad_rows_.empty()) violation_ = 0;
        }
        return d;
    }

    double flip(int var) { return delta<true>(var).gain; }

#if __INCLUDE_LEVEL__ == 0
    int64_t test_stop_iteration_ = -1;
    std::vector<uint8_t> test_stopped_state_;
#endif

    bool check_time() {
#if __INCLUDE_LEVEL__ == 0
        // 実時間に依存せず、中断後の復元と新規処理の抑止を検査する
        if (!stopped_ && test_stop_iteration_ >= 0 && iterations_ >= test_stop_iteration_) {
            stopped_ = true; test_stopped_state_ = x_;
        }
#endif
        if (stopped_) return false;
        if (Clock::now() >= end_) stopped_ = true;
        return !stopped_;
    }

    bool tick() {
        if (stopped_ || (limit_ >= 0 && iterations_ >= limit_)) return false;
        if ((iterations_ & 31) == 0 && !check_time()) return false;
        ++iterations_;
        return true;
    }

    int pick_from_row(int row) {
        // 長い行は標本化し、短い行は全項を調べる
        int begin = row_begin_[row], len = row_begin_[row + 1] - begin;
        int best = -1;
        double score = -std::numeric_limits<double>::infinity();
        for (int q = 0; q < std::min(len, CANDIDATES); ++q) {
            const auto& t = terms_[begin + (len <= CANDIDATES ? q : rng_.pick(len))];
            int v = t.var;
            if (!movable_[v] || locked_[v]) continue;
            if (violation(rows_[row], sums_[row] + t.coef * (1 - 2 * x_[v])) >= rows_[row].amount) continue;
            auto d = delta(v);
            double value = d.gain + rng_.unit() * 1e-7;
            if (value > score) { score = value; best = v; }
        }
        return best;
    }

    double chain(int first, int soft_target, std::vector<int>& moves) {
        // 反転列を再利用し、最良の途中候補より後ろだけを巻き戻す
        moves.clear();
        int best_size = 0;
        bool best_feasible = false;
        double total = 0, best_gain = -std::numeric_limits<double>::infinity();
        for (int depth = 0; depth < CHAIN_DEPTH && first >= 0; ++depth) {
            locked_[first] = 1;
            moves.push_back(first);
            total += flip(first);
            remember();
            bool feasible = bad_rows_.empty();
            // 実行可能な途中解を優先し、同じ可否なら探索用利得で比較する
            if ((feasible && !best_feasible) || (feasible == best_feasible && total > best_gain)) {
                best_feasible = feasible; best_gain = total; best_size = (int)moves.size();
            }
            if (depth + 1 == CHAIN_DEPTH || !check_time()) break;
            int row = -1;
            // 新しい変更で壊れた行を先に修復し、無ければ指定ソフト行を進める
            for (int c = col_begin_[first]; c < col_begin_[first + 1]; ++c) {
                int j = column_row_[c];
                if (bad_pos_[j] >= 0) { row = j; break; }
            }
            if (row < 0 && !bad_rows_.empty()) row = bad_rows_[rng_.pick((int)bad_rows_.size())];
            if (row < 0 && soft_target >= 0 && rows_[soft_target].amount)
                row = soft_target;
            if (row < 0) break;
            first = pick_from_row(row);
        }
        // 採否判定まで最良の反転列を適用したまま保ち、ロックは全て解除する
        for (int q = (int)moves.size() - 1; q >= 0; --q) {
            if (q >= best_size) flip(moves[q]);
            locked_[moves[q]] = 0;
        }
        moves.resize(best_size);
        return best_gain;
    }

    bool exact_block(const std::vector<int>& vars, int64_t node_limit) {
        // 外側を固定した残余区間を作り、外側の減点も上界に含める
        std::vector<uint8_t> seen(rows_.size(), 0);
        std::vector<int> touched;
        std::vector<long long> low(rows_.size()), high(rows_.size());
        long long upper_profit = linear_, outside_penalty = linear_ - objective_;
        for (int v : vars) {
            upper_profit += std::max(0LL, profit_[v]) - profit_[v] * x_[v];
            for (int c = col_begin_[v]; c < col_begin_[v + 1]; ++c) {
                int j = column_row_[c]; long long coef = column_coef_[c];
                if (!seen[j]) {
                    seen[j] = 1; touched.push_back(j);
                    low[j] = high[j] = sums_[j];
                    outside_penalty -= penalty(rows_[j], rows_[j].amount);
                }
                low[j] += std::min(0LL, coef) - coef * x_[v];
                high[j] += std::max(0LL, coef) - coef * x_[v];
            }
        }
        int64_t nodes = 0;
        bool complete = true;
        // 楽観的な利益上界と各行の到達区間で枝刈りする
        auto visit = [&](auto&& self, int depth, long long upper) -> void {
            if (nodes >= node_limit || !tick()) { complete = false; return; }
            ++nodes;
            long long bound = upper - outside_penalty;
            for (int j : touched) {
                const auto& row = rows_[j];
                long long v = (std::max(row.lower, high[j]) - high[j])
                            + (low[j] - std::min(row.upper, low[j]));
                if (row.kind == PboRowKind::Hard && v) return;
                bound -= penalty(row, v);
            }
            if (best_.evaluation.feasible() && bound <= best_.evaluation.objective) return;
            if (depth == (int)vars.size()) { remember(); return; }
            int var = vars[depth], original = x_[var];
            for (int k = 0; k < 2; ++k) {
                int value = original ^ k;
                if (k) flip(var);
                for (int c = col_begin_[var]; c < col_begin_[var + 1]; ++c) {
                    int j = column_row_[c]; long long coef = column_coef_[c];
                    low[j] += coef * value - std::min(0LL, coef);
                    high[j] += coef * value - std::max(0LL, coef);
                }
                self(self, depth + 1, upper + profit_[var] * value - std::max(0LL, profit_[var]));
                for (int c = col_begin_[var]; c < col_begin_[var + 1]; ++c) {
                    int j = column_row_[c]; long long coef = column_coef_[c];
                    low[j] -= coef * value - std::min(0LL, coef);
                    high[j] -= coef * value - std::max(0LL, coef);
                }
                if (k) flip(var);
                if (!complete) break;
            }
        };
        visit(visit, 0, upper_profit);
        return complete;
    }

    void lns() {
        // 未解消の違反行の周囲を厳密に修復し、全ペア・全近傍は展開しない
        load_state(best_.x);
        std::vector<int> vars;
        vars.reserve(16);
        auto add = [&](int v) {
            if (movable_[v] && !locked_[v] && vars.size() < 16) {
                locked_[v] = 1; vars.push_back(v);
            }
        };
        assert(!bad_rows_.empty());
        int bad_row = bad_rows_[rng_.pick((int)bad_rows_.size())];
        for (int t = row_begin_[bad_row]; t < row_begin_[bad_row + 1] && vars.size() < 16; ++t)
            add(terms_[t].var);
        for (int q = 0; q < (int)vars.size() && vars.size() < 16; ++q) {
            int var = vars[q];
            int len = col_begin_[var + 1] - col_begin_[var];
            for (int k = 0; k < std::min(len, 4) && vars.size() < 16; ++k) {
                int row = column_row_[col_begin_[var] + rng_.pick(len)];
                int rb = row_begin_[row], count = row_begin_[row + 1] - rb;
                for (int t = 0; t < std::min(count, 16); ++t)
                    add(terms_[rb + (count <= 16 ? t : rng_.pick(count))].var);
            }
        }
        for (int v : vars) locked_[v] = 0;
        exact_block(vars, LNS_NODES);
        load_state(best_.x);
    }

    bool reachable() const {
        // 現在の自由変数で各ハード行の境界へ届くか必要条件を調べる
        for (int j = 0; j < (int)rows_.size(); ++j) {
            const auto& row = rows_[j];
            if (row.kind != PboRowKind::Hard) continue;
            long long low = sums_[j], high = low;
            for (int t = row_begin_[j]; t < row_begin_[j + 1]; ++t) {
                const auto& a = terms_[t];
                if (!movable_[a.var]) continue;
                low += std::min(0LL, a.coef) - a.coef * x_[a.var];
                high += std::max(0LL, a.coef) - a.coef * x_[a.var];
            }
            if (high < row.lower || low > row.upper) return false;
        }
        return true;
    }

    const PboResult& run(const PboParam& param, std::span<const uint8_t> initial,
                         std::optional<std::span<const int>> subset) {
        // 入力が自身の返却領域を参照していても先に退避する
        auto start = Clock::now(); end_ = Clock::time_point::max();
        assert(std::isfinite(param.time_limit_us) &&
               (param.time_limit_us >= 0 || param.time_limit_us == -1));
        assert(param.max_iterations >= -1);
        assert(param.time_limit_us >= 0 || param.deadline || param.max_iterations >= 0);
        if (param.time_limit_us >= 0)
            end_ = start + std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double, std::micro>(param.time_limit_us));
        if (param.deadline) end_ = std::min(end_, *param.deadline);
        iterations_ = last_improvement_ = 0; limit_ = param.max_iterations;
        stopped_ = false;
        bool supplied = !initial.empty() || subset.has_value();
        std::vector<uint8_t> input;
        if (supplied) { validate_bits(initial); input.assign(initial.begin(), initial.end()); }
        if (subset) {
            for (int i = 0; i < (int)fixed_.size(); ++i)
                assert(fixed_[i] < 0 || input[i] == fixed_[i]);
            initialized_ = false;
        }
        // モデル更新後は保存解と現在解を新しい評価へ戻す
        if (dirty_ || !initialized_) {
            scales();
            if (initialized_) {
                auto previous = std::move(x_);
                load_state(best_.x);
                initialized_ = false; remember();
                if (previous != x_) { load_state(std::move(previous)); remember(); }
            }
            dirty_ = false;
        }
        bool fresh = !initialized_;
        if (!initialized_) {
            load_state(supplied ? std::move(input) : std::vector<uint8_t>(profit_.size(), 0));
            remember();
        } else if (supplied) { load_state(std::move(input)); remember(); }
        std::fill(movable_.begin(), movable_.end(), subset ? 0 : 1);
        if (subset) for (int v : *subset) {
            assert(0 <= v && v < (int)profit_.size());
            movable_[v] = 1;
        }
        free_.clear();
        for (int i = 0; i < (int)profit_.size(); ++i) {
            if (fixed_[i] >= 0) movable_[i] = 0;
            if (movable_[i]) free_.push_back(i);
        }
        bool can_search = !free_.empty() && param.max_iterations != 0 && check_time() && reachable();
        if (can_search && free_.size() <= 18) {
            if (exact_block(free_, 1'000'000)) can_search = false;
        }
        // 全0の候補を保持した上で、初回は利益に沿う状態からも始める
        if (can_search && fresh && !supplied && check_time()) {
            auto seed = x_;
            for (int i : free_) seed[i] = profit_[i] > 0;
            load_state(std::move(seed)); remember();
        }
        int64_t next_lns = iterations_ + LNS_INTERVAL;
        std::vector<int> moves;
        if (can_search) moves.reserve(CHAIN_DEPTH);
        while (can_search && tick()) {
            // 違反行の修復候補と無作為の候補を利得で比較する
            int var = -1;
            double gain = -std::numeric_limits<double>::infinity();
            auto consider = [&](int v) {
                if (v < 0) return;
                auto d = delta(v);
                double score = d.gain + rng_.unit() * 1e-7;
                if (score > gain) { gain = score; var = v; }
            };
            if (!bad_rows_.empty())
                consider(pick_from_row(bad_rows_[rng_.pick((int)bad_rows_.size())]));
            for (int k = 0; k < CANDIDATES; ++k)
                consider(free_[rng_.pick((int)free_.size())]);
            int soft = -1;
            if (!soft_rows_.empty() && rng_.pick(4) == 0) {
                soft = soft_rows_[rng_.pick((int)soft_rows_.size())];
                int v = pick_from_row(soft);
                if (v >= 0) var = v;
            }
            assert(var >= 0); // 自由変数から必ず候補を評価している
            // 連鎖全体をひとつの提案として受理し、途中の良い解も保存する
            double total = chain(var, soft, moves);
            if (total < 0) {
                double phase = (double)(iterations_ % TEMPERATURE_PERIOD) / TEMPERATURE_PERIOD;
                if (rng_.unit() >= std::exp(total / std::pow(0.01, phase)))
                    for (int k = (int)moves.size() - 1; k >= 0; --k) flip(moves[k]);
            }
            if (stopped_) break;
            // 違反行へ重点を移し、蓄積した重みは定期的に緩める
            if (iterations_ % 32 == 0) {
                double increment = best_.evaluation.feasible() ? 0.25 : 1.0;
                for (int j : bad_rows_) rows_[j].search_weight += increment;
                if (iterations_ % 1024 == 0)
                    for (auto& row : rows_) row.search_weight = 1 + (row.search_weight - 1) * 0.5;
            }
            if (!best_.evaluation.feasible() && iterations_ - last_improvement_ > 128 &&
                iterations_ >= next_lns && check_time()) {
                lns(); next_lns = iterations_ + LNS_INTERVAL;
            }
            if (iterations_ - last_improvement_ > 512 && iterations_ % 128 == 0 && check_time()) {
                load_state(best_.x);
                int count = std::min((int)free_.size(), 2 + rng_.pick(5));
                for (int k = 0; k < count; ++k) {
                    flip(free_[rng_.pick((int)free_.size())]); remember();
                }
            }
        }
        // 可変集合は次の呼出しの入口で設定し直す
        best_.iterations = iterations_;
        best_.elapsed_us = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
        return best_;
    }

#if __INCLUDE_LEVEL__ == 0
    friend struct PboSolverTest;
#endif

public:
    // 問題を所有して疎な両方向参照を構築する O(n+m+Σ k_j log k_j)
    explicit PboSolver(PboProblem problem, uint64_t seed = 1) : rng_{seed} {
        compile(std::move(problem));
    }

    // 構築・改善・修復または継続探索する O(n+m+K+探索量)、返却参照は次の非const操作まで
    const PboResult& solve(const PboParam& param = {}, std::span<const uint8_t> initial = {}) {
        return run(param, initial, std::nullopt);
    }

    // 指定変数だけを変更する新しい部分探索 O(n+m+K+探索量)、initialは恒久固定を満たすこと
    const PboResult& improve(std::span<const uint8_t> initial, std::span<const int> free_variables,
                             const PboParam& param = {}) {
        return run(param, initial, free_variables);
    }

    // 指定解を変更せず全評価する O(n+m+K)、モデル更新後も直ちに使用可能
    PboEvaluation evaluate(std::span<const uint8_t> x) const {
        validate_bits(x);
        PboEvaluation result{constant_, 0, 0};
        for (int i = 0; i < (int)profit_.size(); ++i) {
            result.objective += profit_[i] * x[i];
            result.violated_fixed_variables += fixed_[i] >= 0 && x[i] != fixed_[i];
        }
        // 保存している探索状態を参照せず、指定解から行和を作る
        for (int j = 0; j < (int)rows_.size(); ++j) {
            long long value = 0;
            for (int t = row_begin_[j]; t < row_begin_[j + 1]; ++t)
                value += terms_[t].coef * x[terms_[t].var];
            auto v = violation(rows_[j], value);
            result.objective -= penalty(rows_[j], v);
            result.violated_hard_rows += rows_[j].kind == PboRowKind::Hard && v != 0;
        }
        return result;
    }

    // 乱数系列を再設定する O(1)
    void reseed(uint64_t seed) { rng_.state = seed; }

    // 問題構造を残して探索状態を破棄する O(1)、次回初期化はO(n+m+K)
    void reset_search() { initialized_ = false; }

    // 利益を変更する O(1)、次回solveで保存候補も再評価
    void set_profit(int var, long long profit) {
        assert(0 <= var && var < (int)profit_.size());
        profit_[var] = profit; dirty_ = true;
    }

    // 目的値の定数を変更する O(1)、次回solveで保存候補も再評価
    void set_constant(long long constant) { constant_ = constant; dirty_ = true; }

    // 自由=-1、0固定、1固定を設定する O(1)、次回solveで固定値を反映
    void set_fixed(int var, int value) {
        assert(0 <= var && var < (int)profit_.size() && -1 <= value && value <= 1);
        fixed_[var] = (int8_t)value; dirty_ = true;
    }

    // 行の上下限を変更する O(1)、nulloptは境界なし
    void set_row_bounds(int row, std::optional<long long> lower, std::optional<long long> upper) {
        assert(0 <= row && row < (int)rows_.size());
        assert(!lower || !upper || *lower <= *upper);
        rows_[row].lower = lower.value_or(LLONG_MIN);
        rows_[row].upper = upper.value_or(LLONG_MAX); dirty_ = true;
    }

    // 必須条件または減点方式を変更する O(1)
    void set_row_penalty(int row, PboRowKind kind, long long weight = 1) {
        assert(0 <= row && row < (int)rows_.size());
        assert(kind == PboRowKind::Hard || weight >= 0);
        rows_[row].kind = kind; rows_[row].weight = weight; dirty_ = true;
    }

    // 既存の正規化済み項の係数を変更する O(log k_j)、0化した項の再変更も可能
    void set_coefficient(int row, int var, long long coef) {
        assert(0 <= row && row < (int)rows_.size());
        auto begin = terms_.begin() + row_begin_[row], end = terms_.begin() + row_begin_[row + 1];
        auto it = std::lower_bound(begin, end, var, [](const Term& t, int v) { return t.var < v; });
        assert(it != end && it->var == var);
        it->coef = coef; column_coef_[col_pos_[it - terms_.begin()]] = coef;
        dirty_ = true;
    }

    // 構造を再構築する O(n+m+Σ k_j log k_j)、引継ぎ時は変数数とIDの意味を保つ
    void rebuild(PboProblem problem, bool keep_solution = true) {
        assert(!keep_solution || problem.profit.size() == profit_.size());
        // 旧構造を破棄する前に、引継ぎ対象の候補だけを退避する
        bool keep = keep_solution && initialized_;
        auto saved = keep ? std::move(best_.x) : std::vector<uint8_t>{};
        auto current = keep ? std::move(x_) : std::vector<uint8_t>{};
        compile(std::move(problem));
        if (keep) {
            // 新しい固定条件と評価尺度を反映して候補を選び直す
            scales(); dirty_ = false;
            load_state(std::move(saved)); remember();
            if (current != x_) { load_state(std::move(current)); remember(); }
        }
    }
};


#if __INCLUDE_LEVEL__ == 0
using namespace std;
// この節はヘッダとしてincludeした場合にはコンパイルされない
struct PboSolverTest {
    static inline int checks = 0;
    static void require(bool value) {
        ++checks;
        if (!value) { cerr << "test failed at check " << checks << '\n'; abort(); }
    }
    static PboEvaluation naive(const PboProblem& p, const vector<uint8_t>& x) {
        // 入力モデルを直接走査し、正規化後の探索実装から独立して評価する
        PboEvaluation e{p.constant, 0, 0};
        for (int i = 0; i < (int)x.size(); ++i) {
            e.objective += p.profit[i] * x[i];
            if (!p.fixed.empty()) e.violated_fixed_variables += p.fixed[i] >= 0 && p.fixed[i] != x[i];
        }
        for (const auto& r : p.rows) {
            long long s = 0;
            for (auto t : r.terms) s += t.coef * x[t.var];
            long long d = 0;
            if (r.lower && s < *r.lower) d += *r.lower - s;
            if (r.upper && s > *r.upper) d += s - *r.upper;
            if (r.kind == PboRowKind::Hard) e.violated_hard_rows += d != 0;
            else e.objective -= r.weight * (r.kind == PboRowKind::LinearPenalty ? d : (d != 0));
        }
        return e;
    }
    static void equal(PboEvaluation a, PboEvaluation b) {
        require(a.objective == b.objective);
        require(a.violated_hard_rows == b.violated_hard_rows);
        require(a.violated_fixed_variables == b.violated_fixed_variables);
    }
    static PboParam deterministic(int64_t count = 1'000'000) {
        PboParam q; q.time_limit_us = -1; q.max_iterations = count; return q;
    }
    static PboProblem random_problem(mt19937_64& rng, int n, int m, bool planted) {
        // 入力の符号・重複・行種別を変え、必要なら既知の実行可能解を埋め込む
        auto pick = [&](int k) { return (int)(rng() % (uint64_t)k); };
        PboProblem p;
        p.constant = pick(101) - 50;
        p.profit.resize(n); p.fixed.assign(n, -1);
        vector<uint8_t> witness(n);
        for (int i = 0; i < n; ++i) {
            p.profit[i] = pick(41) - 20; witness[i] = (uint8_t)pick(2);
            if (pick(12) == 0) p.fixed[i] = (int8_t)witness[i];
        }
        for (int j = 0; j < m; ++j) {
            PboRow r; r.kind = (PboRowKind)pick(3); r.weight = pick(9);
            int len = pick(min(n * 2, 20) + 1); long long s = 0;
            for (int k = 0; k < len; ++k) {
                int v = pick(n), a = pick(15) - 7;
                r.terms.push_back({v, a}); s += a * witness[v];
            }
            if (!planted) s = pick(41) - 20;
            int shape = pick(5);
            if (shape != 0) r.lower = s - pick(7);
            if (shape != 1) r.upper = s + pick(7);
            if (shape == 2) r.lower = r.upper = s;
            p.add_row(move(r));
        }
        return p;
    }
    static optional<long long> exhaustive(const PboProblem& p, const vector<uint8_t>& base,
                                           const vector<int>& free_vars) {
        // 指定した自由変数の全割当を素朴に評価する
        optional<long long> best;
        auto x = base;
        for (uint64_t mask = 0; mask < (1ULL << free_vars.size()); ++mask) {
            for (int k = 0; k < (int)free_vars.size(); ++k)
                x[free_vars[k]] = (uint8_t)((mask >> k) & 1);
            auto e = naive(p, x);
            if (e.feasible() && (!best || e.objective > *best)) best = e.objective;
        }
        return best;
    }
    static void exact_tests() {
        // 小規模の符号混在・重複・固定・各減点方式を全列挙と比較する
        mt19937_64 rng(725196);
        for (int seed = 0; seed < 700; ++seed) {
            int n = (int)(rng() % 12) + 1;
            auto p = random_problem(rng, n, (int)(rng() % 16), seed % 3 != 0);
            PboSolver s(p, (uint64_t)seed);
            vector<int> vars(n); iota(vars.begin(), vars.end(), 0);
            auto optimum = exhaustive(p, vector<uint8_t>(n), vars);
            auto r = s.solve(deterministic());
            equal(r.evaluation, naive(p, r.x));
            require(r.evaluation.feasible() == optimum.has_value());
            if (optimum) require(r.evaluation.objective == *optimum);
            equal(s.evaluate(r.x), r.evaluation);
        }
    }
    static void state_tests() {
        // 全評価を独立実装にし、差分・反転・巻戻し・違反集合を検査する
        mt19937_64 rng(4920651);
        for (int trial = 0; trial < 120; ++trial) {
            int n = 32 + (int)(rng() % 225);
            auto p = random_problem(rng, n, 100 + (int)(rng() % 201), true);
            PboSolver s(p); s.scales();
            vector<uint8_t> x(n);
            for (int i = 0; i < n; ++i) x[i] = p.fixed[i] >= 0 ? (uint8_t)p.fixed[i] : (uint8_t)(rng() & 1);
            s.load_state(x); s.remember();
            fill(s.movable_.begin(), s.movable_.end(), 1);
            for (int k = 0; k < 1000; ++k) {
                int v = (int)(rng() % (uint64_t)n);
                if (p.fixed[v] >= 0) continue;
                auto before = naive(p, x); auto d = s.delta(v);
                x[v] ^= 1; auto after = naive(p, x);
                require(d.objective == after.objective - before.objective);
                s.flip(v); equal(s.evaluate(x), after);
                require(s.objective_ == after.objective);
                require((int)s.bad_rows_.size() == after.violated_hard_rows);
                require(s.x_ == x);
                double total = 0;
                for (int j = 0; j < (int)s.rows_.size(); ++j) {
                    long long sum = 0;
                    for (auto t : p.rows[j].terms) sum += t.coef * x[t.var];
                    require(sum == s.sums_[j]);
                    require(s.rows_[j].amount == s.violation(s.rows_[j], sum));
                    if (s.rows_[j].kind == PboRowKind::Hard) {
                        auto drow = s.violation(s.rows_[j], sum);
                        total += (double)drow * s.rows_[j].scale;
                        require((s.bad_pos_[j] >= 0) == (drow != 0));
                        if (drow) require(s.bad_rows_[s.bad_pos_[j]] == j);
                    }
                }
                require(abs(total - s.violation_) < 1e-7 * max(1.0, total));
                if (k % 7 == 0) { s.flip(v); x[v] ^= 1; }
            }
        }
    }
    static void api_tests() {
        // 境界条件と全公開操作を短いシナリオで確認する
        PboProblem empty; empty.constant = -17;
        PboSolver z(empty);
        auto rz = z.solve(deterministic(0));
        require(rz.x.empty() && rz.evaluation.feasible() && rz.evaluation.objective == -17);
        equal(z.evaluate({}), rz.evaluation);
        z.improve({}, {}, deterministic());
        z.rebuild(empty, false);
        empty.add_eq({}, 1); z.rebuild(empty);
        require(!z.solve(deterministic()).evaluation.feasible());
        PboProblem p; p.profit = {9, -3, 6, 4}; p.fixed = {-1, -1, -1, 0}; p.constant = -20;
        int a = p.add_le({{0, 1}, {1, 1}}, 1);
        p.add_ge({{1, -1}, {2, 2}}, -1, PboRowKind::LinearPenalty, 3);
        p.add_eq({{0, 1}, {2, 1}}, 1, PboRowKind::FixedPenalty, 11);
        p.add_range({{2, 1}, {2, -1}}, nullopt, nullopt);
        p.add_all_profit({0, 0, 1}, 23); p.add_all_profit({1, 2}, -7);
        p.add_all_profit({}, 5); p.add_all_profit({}, -8);
        PboSolver s(p, 3);
        vector<int> vars = {0, 1, 2, 3}; vector<uint8_t> zero(4);
        auto verify = [&] {
            auto r = s.solve(deterministic());
            equal(r.evaluation, naive(p, r.x));
            auto opt = exhaustive(p, zero, vars);
            require(r.evaluation.feasible() == opt.has_value());
            if (opt) require(r.evaluation.objective == *opt);
        };
        verify();
        p.profit[1] = 13; s.set_profit(1, 13); equal(s.evaluate(zero), naive(p, zero)); verify();
        p.constant = 30; s.set_constant(30); verify();
        p.fixed[0] = 1; s.set_fixed(0, 1); verify();
        require(s.evaluate(zero).violated_fixed_variables == 1);
        auto projected = s.solve(deterministic(0), zero); require(projected.x[0] == 1);
        p.fixed[0] = -1; s.set_fixed(0, -1); verify();
        p.rows[a].lower = -1; p.rows[a].upper = 2; s.set_row_bounds(a, -1, 2); verify();
        p.rows[a].lower.reset(); p.rows[a].upper.reset(); s.set_row_bounds(a, nullopt, nullopt); verify();
        p.rows[a].kind = PboRowKind::LinearPenalty; p.rows[a].weight = 7;
        s.set_row_penalty(a, PboRowKind::LinearPenalty, 7); verify();
        p.rows[a].terms[0].coef = 0; s.set_coefficient(a, 0, 0); verify();
        p.rows[a].terms[0].coef = -5; s.set_coefficient(a, 0, -5); verify();
        p.rows[a].kind = PboRowKind::Hard; s.set_row_penalty(a, PboRowKind::Hard); verify();
        auto old = s.solve(deterministic()).x;
        s.reseed(987); s.reset_search(); verify();
        s.solve(deterministic(0), old);
        s.solve(deterministic(0), s.solve(deterministic(0)).x); // 返却領域の別名
        p.add_le({{0, -2}, {3, 1}}, 0); s.rebuild(p); verify();
        p.profit.push_back(27); p.fixed.push_back(-1); vars.push_back(4); zero.push_back(0);
        s.rebuild(p, false); verify();
        // 補助変数なしの全選択利益は空集合と重複を含めて真理値表で比較する
        for (int value : {-19, 0, 23}) {
            PboProblem q; q.profit.resize(4); q.add_all_profit({0, 1, 1, 3}, value);
            PboSolver t(q);
            for (int mask = 0; mask < 16; ++mask) {
                vector<uint8_t> x(4);
                for (int i = 0; i < 4; ++i) x[i] = (uint8_t)((mask >> i) & 1);
                require(t.evaluate(x).objective == (x[0] && x[1] && x[3] ? value : 0));
            }
        }
        PboProblem impossible; impossible.profit = {1}; impossible.add_eq({{0, 2}}, 1);
        PboSolver t(impossible); require(!t.solve(deterministic()).evaluation.feasible());
    }
    static void subset_tests() {
        // 前回の保存解を混入させず、外側固定を保った厳密解を比較する
        mt19937_64 rng(89325871);
        for (int trial = 0; trial < 200; ++trial) {
            auto p = random_problem(rng, 24, 18, true);
            PboSolver s(p, (uint64_t)trial);
            s.solve(deterministic(100));
            vector<uint8_t> x(24); vector<int> free_vars;
            for (int i = 0; i < 24; ++i) {
                x[i] = p.fixed[i] >= 0 ? (uint8_t)p.fixed[i] : (uint8_t)(rng() & 1);
                if (p.fixed[i] < 0 && i % 2 == 0) free_vars.push_back(i);
            }
            auto opt = exhaustive(p, x, free_vars);
            auto duplicate = free_vars;
            if (!duplicate.empty()) duplicate.push_back(duplicate[0]);
            auto r = s.improve(x, duplicate, deterministic());
            equal(r.evaluation, naive(p, r.x));
            require(r.evaluation.feasible() == opt.has_value());
            if (opt) require(r.evaluation.objective == *opt);
            for (int i = 0; i < 24; ++i)
                if (find(free_vars.begin(), free_vars.end(), i) == free_vars.end()) require(r.x[i] == x[i]);
            auto zero = s.improve(x, {}, deterministic()); require(zero.x == x);
            s.solve(deterministic(100));
            require(count(s.movable_.begin(), s.movable_.end(), 1) == count(p.fixed.begin(), p.fixed.end(), -1));
        }
    }
    static void search_tests() {
        // 時間制限・継続・再現性・モデル更新後の評価を実際の探索経路で検証する
        mt19937_64 rng(3490802);
        for (int trial = 0; trial < 70; ++trial) {
            auto p = random_problem(rng, 60, 45, true);
            PboSolver a(p, 8), b(p, 8);
            auto ra = a.solve(deterministic(1500));
            auto rb = b.solve(deterministic(1500));
            require(ra.x == rb.x && ra.iterations == rb.iterations);
            equal(ra.evaluation, naive(p, ra.x));
            auto rb2 = b.solve(deterministic(1200));
            equal(rb2.evaluation, naive(p, rb2.x));
            if (rb.evaluation.feasible()) require(rb2.evaluation.feasible() && rb2.evaluation.objective >= rb.evaluation.objective);
            b.reset_search(); b.reseed(8);
            require(b.solve(deterministic(1500)).x == ra.x);
            vector<uint8_t> start = ra.x;
            auto r = a.improve(start, vector<int>{0, 1, 2, 3, 4}, deterministic(3));
            equal(r.evaluation, naive(p, r.x));
            for (int i = 5; i < 60; ++i) require(r.x[i] == start[i]);
            p.profit[0] += 11; a.set_profit(0, p.profit[0]);
            p.rows[0].kind = PboRowKind::FixedPenalty; p.rows[0].weight = 9;
            a.set_row_penalty(0, PboRowKind::FixedPenalty, 9);
            r = a.solve(deterministic(0)); equal(r.evaluation, naive(p, r.x));
        }
        PboProblem q; q.profit.assign(128, 1); q.add_eq({{0, 1}, {1, 1}}, 1);
        PboSolver s(q);
        PboParam zero; zero.time_limit_us = 0;
        auto r = s.solve(zero); require(r.iterations == 0);
        PboParam past; past.deadline = chrono::steady_clock::now() - chrono::seconds(1);
        require(s.solve(past).iterations == 0);
        PboParam soon; soon.time_limit_us = 1000;
        r = s.solve(soon); equal(r.evaluation, naive(q, r.x)); require(r.elapsed_us >= 0);
        vector<uint8_t> initial(128, 1); initial[0] = 0;
        r = s.solve(zero, initial); require(r.evaluation.feasible());
        require(r.evaluation.objective >= naive(q, initial).objective);
    }
    static void numeric_and_repair_tests() {
        // doubleの仮評価に関係なく、2^53を超える整数目的値を正確に比較する
        PboProblem p; p.profit = {(1LL << 54) + 3, (1LL << 54) + 4};
        p.constant = -(1LL << 55); p.add_eq({{0, 1}, {1, 1}}, 1);
        PboSolver s(p);
        auto r = s.solve(deterministic());
        require(r.x == vector<uint8_t>({0, 1}));
        require(r.evaluation.objective == -(1LL << 54) + 4);
        // 疎な大きなID、長い行、ゼロ相殺項を通して全評価を確認する
        PboProblem large; large.profit.resize(10000); large.profit[9999] = 5;
        vector<PboTerm> ts;
        for (int i = 0; i < 10000; ++i) ts.push_back({i, i % 9 - 4});
        large.add_range(ts, -10000, 10000);
        large.add_eq({{9999, 7}, {9999, -7}}, 0);
        PboSolver big(large); vector<uint8_t> x(10000);
        for (int i = 0; i < 10000; ++i) x[i] = (uint8_t)(i % 3 == 0);
        equal(big.evaluate(x), naive(large, x));
        auto rb = big.solve(deterministic(50), x);
        equal(rb.evaluation, naive(large, rb.x));
        // 一部のハード違反を外側に残しながら、部分問題の違反を解消する
        PboProblem repair; repair.profit.assign(20, 1);
        repair.add_eq({{0, 2}, {1, 3}, {2, -1}}, 4);
        repair.add_eq({{10, 1}, {11, 1}}, 1);
        PboSolver t(repair); t.scales(); t.load_state(vector<uint8_t>(20)); t.remember();
        t.end_ = PboSolver::Clock::time_point::max(); t.limit_ = -1;
        auto original = t.x_;
        t.exact_block({0, 1, 2}, 100);
        require(t.x_ == original);
        require(t.best_.evaluation.violated_hard_rows == 1);
        equal(t.best_.evaluation, naive(repair, t.best_.x));
    }
    static void timeout_regressions() {
        // 締切検出後は復元以外の初期候補作成・再出発を始めない
        PboProblem p; p.profit.assign(18, 1); PboSolver s(p);
        s.test_stop_iteration_ = 32;
        auto r = s.solve(deterministic(2000));
        require(s.stopped_ && r.iterations == 32);
        require(s.x_ == vector<uint8_t>(18));
        equal(r.evaluation, naive(p, r.x));
        PboProblem q; q.profit.assign(32, 0); PboSolver t(q);
        t.test_stop_iteration_ = 768;
        auto u = t.solve(deterministic(2000));
        require(t.stopped_ && u.iterations == 768);
        require(t.x_ == t.test_stopped_state_);
        require(count(t.locked_.begin(), t.locked_.end(), 1) == 0);
        equal(u.evaluation, naive(q, u.x));
    }
    static void verify_state(PboSolver& s, const PboProblem& p) {
        // 探索の整数状態と違反集合を元モデルから再計算して照合する
        auto e = naive(p, s.x_);
        equal(e, s.evaluate(s.x_));
        require(s.objective_ == e.objective);
        require((int)s.bad_rows_.size() == e.violated_hard_rows);
        require(count(s.locked_.begin(), s.locked_.end(), 1) == 0);
        for (int t = 0; t < (int)s.terms_.size(); ++t) {
            int c = s.col_pos_[t], v = s.terms_[t].var;
            require(s.col_begin_[v] <= c && c < s.col_begin_[v + 1]);
            require(s.column_coef_[c] == s.terms_[t].coef);
            require(s.column_row_[c] == s.terms_[t].row);
        }
        double violation_sum = 0;
        for (int j = 0; j < (int)p.rows.size(); ++j) {
            long long sum = 0;
            for (auto t : p.rows[j].terms) sum += t.coef * s.x_[t.var];
            require(sum == s.sums_[j]);
            long long amount = s.violation(s.rows_[j], sum);
            require(s.rows_[j].amount == amount);
            bool bad = p.rows[j].kind == PboRowKind::Hard && amount;
            require((s.bad_pos_[j] >= 0) == bad);
            if (bad) {
                require(s.bad_rows_[s.bad_pos_[j]] == j);
                violation_sum += (double)amount * s.rows_[j].scale;
            }
        }
        require(abs(violation_sum - s.violation_) < 1e-7 * max(1.0, violation_sum));
    }
    static void feasible_prefix_test() {
        // 大きな減点を消せてもハード制約を壊す接尾部は、実行可能な接頭部へ戻す
        PboProblem p; p.profit = {1, 100, 0};
        p.add_le({{1, 1}, {2, 1}}, 0);
        int soft = p.add_ge({{0, 1}, {1, 1}}, 2, PboRowKind::FixedPenalty, 1000);
        PboSolver s(p); s.scales(); s.load_state({0, 0, 0}); s.remember();
        fill(s.movable_.begin(), s.movable_.end(), 1);
        s.end_ = PboSolver::Clock::time_point::max();
        vector<int> moves;
        s.chain(0, soft, moves);
        require(moves == vector<int>{0});
        require(s.x_ == vector<uint8_t>({1, 0, 0}));
        require(s.objective_ == -999 && s.bad_rows_.empty());
        verify_state(s, p);
        s.flip(0);
        require(s.x_ == vector<uint8_t>({0, 0, 0}));
        verify_state(s, p);
    }
    static void compound_tests() {
        // 同じ行を共有する連鎖を採用・棄却し、途中停止でも復元可能か確認する
        mt19937_64 rng(59170832);
        for (int trial = 0; trial < 60; ++trial) {
            auto p = random_problem(rng, 40, 50, true);
            PboSolver s(p); s.solve(deterministic(0));
            vector<int> moves;
            for (int step = 0; step < 100; ++step) {
                int v = s.free_[(size_t)(rng() % s.free_.size())];
                auto before = s.x_; auto sums = s.sums_;
                int soft = s.soft_rows_.empty() ? -1 : s.soft_rows_[(size_t)(rng() % s.soft_rows_.size())];
                s.stopped_ = false;
                s.test_stop_iteration_ = step % 9 == 0 ? 0 : -1;
                s.end_ = PboSolver::Clock::time_point::max();
                s.chain(v, soft, moves);
                require(!moves.empty() && moves.size() <= PboSolver::CHAIN_DEPTH);
                auto expected = before;
                for (int changed : moves) expected[changed] ^= 1;
                require(s.x_ == expected); verify_state(s, p);
                if (step % 2 == 0) {
                    for (int k = (int)moves.size() - 1; k >= 0; --k) s.flip(moves[k]);
                    require(s.x_ == before && s.sums_ == sums); verify_state(s, p);
                }
                s.stopped_ = false; s.test_stop_iteration_ = -1;
                if (step % 11 == 0) {
                    before = s.x_; sums = s.sums_;
                    vector<int> vars(s.free_.begin(), s.free_.begin() + min<size_t>(8, s.free_.size()));
                    s.exact_block(vars, 3);
                    require(s.x_ == before && s.sums_ == sums); verify_state(s, p);
                }
            }
        }
    }
    static void metamorphic_tests() {
        // 変数・行の順序、項の分割、0項、ハード行の正倍率は意味を変えない
        mt19937_64 rng(8824903);
        for (int trial = 0; trial < 80; ++trial) {
            auto p = random_problem(rng, 9, 15, trial % 2 == 0);
            auto q = p; q.constant += 37;
            vector<int> perm(9); iota(perm.begin(), perm.end(), 0);
            shuffle(perm.begin(), perm.end(), rng);
            for (int i = 0; i < 9; ++i) { q.profit[perm[i]] = p.profit[i]; q.fixed[perm[i]] = p.fixed[i]; }
            for (auto& row : q.rows) {
                vector<PboTerm> split;
                for (auto t : row.terms) {
                    split.push_back({perm[t.var], t.coef + 2});
                    split.push_back({perm[t.var], -2});
                }
                split.push_back({0, 0}); row.terms = move(split);
                if (row.kind == PboRowKind::Hard) {
                    for (auto& t : row.terms) t.coef *= 3;
                    if (row.lower) *row.lower *= 3;
                    if (row.upper) *row.upper *= 3;
                }
                shuffle(row.terms.begin(), row.terms.end(), rng);
            }
            shuffle(q.rows.begin(), q.rows.end(), rng);
            PboSolver a(p), b(q);
            for (int mask = 0; mask < 512; ++mask) {
                vector<uint8_t> x(9), y(9);
                for (int i = 0; i < 9; ++i) y[perm[i]] = x[i] = (uint8_t)((mask >> i) & 1);
                auto e = a.evaluate(x); e.objective += 37;
                equal(e, b.evaluate(y)); equal(b.evaluate(y), naive(q, y));
            }
            auto ra = a.solve(deterministic()), rb = b.solve(deterministic());
            require(ra.evaluation.feasible() == rb.evaluation.feasible());
            if (ra.evaluation.feasible()) require(rb.evaluation.objective == ra.evaluation.objective + 37);
        }
    }
    static void mixed_update_tests() {
        // 数値変更・固定解除・構造変更・部分改善を同じsolver上で繰り返す
        mt19937_64 rng(74683910);
        auto pick = [&](int n) { return (int)(rng() % (uint64_t)n); };
        auto normalize = [](PboProblem& p) {
            for (auto& row : p.rows) {
                map<int, long long> sum;
                for (auto t : row.terms) sum[t.var] += t.coef;
                row.terms.clear();
                for (auto [v, a] : sum) if (a) row.terms.push_back({v, a});
            }
        };
        for (int trial = 0; trial < 40; ++trial) {
            auto p = random_problem(rng, 48, 40, true); normalize(p);
            PboSolver s(p); auto r = s.solve(deterministic(100));
            for (int turn = 0; turn < 60; ++turn) {
                auto old = r.x;
                int v = pick(48), j = pick((int)p.rows.size());
                switch (turn % 7) {
                case 0: p.profit[v] = pick(51) - 25; s.set_profit(v, p.profit[v]); break;
                case 1: p.constant = pick(101) - 50; s.set_constant(p.constant); break;
                case 2: p.fixed[v] = (int8_t)(pick(3) - 1); s.set_fixed(v, p.fixed[v]); break;
                case 3:
                    p.rows[j].lower = pick(11) - 10; p.rows[j].upper = pick(11);
                    if (pick(2)) p.rows[j].lower.reset();
                    if (pick(2)) p.rows[j].upper.reset();
                    s.set_row_bounds(j, p.rows[j].lower, p.rows[j].upper); break;
                case 4:
                    p.rows[j].kind = (PboRowKind)pick(3); p.rows[j].weight = pick(11);
                    s.set_row_penalty(j, p.rows[j].kind, p.rows[j].weight); break;
                case 5:
                    if (!p.rows[j].terms.empty()) {
                        auto& t = p.rows[j].terms[pick((int)p.rows[j].terms.size())];
                        t.coef = turn % 2 ? 0 : pick(13) - 6;
                        s.set_coefficient(j, t.var, t.coef);
                    }
                    break;
                case 6:
                    p.rows.erase(p.rows.begin() + j);
                    p.add_le({{v, 2}, {pick(48), -1}}, pick(4)); normalize(p);
                    s.rebuild(p); break;
                }
                // setter直後も公開evaluateは新しいモデルを評価する
                equal(s.evaluate(old), naive(p, old));
                for (int i = 0; i < 48; ++i) if (p.fixed[i] >= 0) old[i] = (uint8_t)p.fixed[i];
                auto reference = naive(p, old);
                r = s.solve(deterministic(turn % 3 == 0 ? 0 : 100));
                equal(r.evaluation, naive(p, r.x)); verify_state(s, p);
                if (reference.feasible()) require(r.evaluation.feasible() && r.evaluation.objective >= reference.objective);
                if (turn % 5 == 0) {
                    auto initial = r.x; vector<int> vars;
                    for (int k = 0; k < 12; ++k) vars.push_back(pick(48));
                    r = s.improve(initial, vars, deterministic(90));
                    equal(r.evaluation, naive(p, r.x)); verify_state(s, p);
                    for (int i = 0; i < 48; ++i)
                        if (find(vars.begin(), vars.end(), i) == vars.end()) require(r.x[i] == initial[i]);
                }
            }
        }
    }
    static void ownership_tests() {
        // 退避バッファをmoveしても、新旧候補と返却領域を参照した入力を保持する
        for (bool rebuild : {false, true}) for (bool prefer_current : {false, true})
        for (bool distinct : {false, true}) {
            PboProblem p; p.profit.assign(32, 0); p.profit[0] = 10; p.profit[1] = 1;
            vector<uint8_t> saved(32), current(32); saved[0] = current[1] = 1;
            if (!distinct) current = saved;
            auto input = saved;
            PboSolver s(p);
            require(s.solve(deterministic(0), input).x == saved && input == saved);
            s.load_state(current); s.remember();
            require(s.best_.x == saved && s.x_ == current);
            p.profit[0] = prefer_current ? -20 : 20;
            if (rebuild) s.rebuild(p);
            else s.set_profit(0, p.profit[0]);
            auto expected = prefer_current ? current : saved;
            require(s.solve(deterministic(0)).x == expected);
            require(s.x_ == current); verify_state(s, p);
            s.set_constant(17); p.constant = 17;
            require(s.solve(deterministic(0), s.best_.x).x == expected);
            verify_state(s, p);
            require(s.improve(s.best_.x, {}, deterministic(0)).x == expected);
            require(s.x_ == expected); verify_state(s, p);
            s.reset_search();
            require(s.solve(deterministic(0), s.best_.x).x == expected);
            verify_state(s, p);
            s.reset_search(); s.reseed(1);
            PboSolver fresh(p);
            require(s.solve(deterministic(200)).x == fresh.solve(deterministic(200)).x);
            verify_state(s, p);
        }
    }
    static void bound_and_column_tests() {
        // 境界省略時も、番兵との差を直接計算してオーバーフローしないこと
        for (long long coef : {100LL, -100LL, LLONG_MAX / 4, LLONG_MIN / 4}) {
            PboProblem p; p.profit = {0};
            int row = p.add_range({{0, coef}}, nullopt, nullopt);
            PboSolver s(p, 191); s.scales(); s.load_state({1});
            verify_state(s, p);
            s.flip(0); verify_state(s, p);
            s.flip(0); verify_state(s, p);
            PboParam zero; zero.time_limit_us = 0;
            s.set_row_bounds(row, coef, coef);
            p.rows[row].lower = p.rows[row].upper = coef;
            s.solve(zero, vector<uint8_t>{1}); verify_state(s, p);
            for (long long value : {0LL, coef}) {
                s.set_coefficient(row, 0, value); p.rows[row].terms[0].coef = value;
                equal(s.evaluate(vector<uint8_t>{1}), naive(p, vector<uint8_t>{1}));
                s.solve(zero); verify_state(s, p);
            }
            s.set_row_bounds(row, nullopt, nullopt);
            p.rows[row].lower.reset(); p.rows[row].upper.reset();
            s.solve(zero); verify_state(s, p);
        }
    }
    static void time_budget_tests() {
        // 相対・絶対・併用予算、短時間、中断、継続を独立評価で確認する
        PboProblem p; p.profit.resize(40);
        for (int v = 0; v < 40; ++v) {
            p.profit[v] = v % 9 - 4;
            p.add_eq({{v, 1}, {(v + 1) % 40, -1}}, 0,
                     v % 2 ? PboRowKind::LinearPenalty : PboRowKind::FixedPenalty, 5);
        }
        for (int mode = 0; mode < 6; ++mode) {
            PboSolver s(p, 781); vector<uint8_t> initial(40);
            PboParam q; q.time_limit_us = mode == 0 ? 0 : mode == 1 ? 0.5 : 2000;
            if (mode >= 3) q.deadline = PboSolver::Clock::now() + chrono::microseconds(1000);
            if (mode == 3) q.time_limit_us = -1;
            if (mode == 5) q.max_iterations = 5;
            auto result = s.solve(q, initial);
            equal(result.evaluation, naive(p, result.x)); verify_state(s, p);
            require(result.evaluation.feasible());
            require(result.evaluation.objective >= naive(p, initial).objective);
            if (mode == 0) require(result.iterations == 0);
            if (mode == 5) require(result.iterations <= 5);
            auto continued = s.solve(deterministic(100));
            equal(continued.evaluation, naive(p, continued.x)); verify_state(s, p);
            require(continued.evaluation.objective >= result.evaluation.objective);
            q.time_limit_us = -1; q.deadline = PboSolver::Clock::now() - chrono::microseconds(1);
            auto stopped = s.solve(q);
            require(stopped.iterations == 0); verify_state(s, p);
        }
    }
    static void run() {
        ownership_tests(); bound_and_column_tests(); time_budget_tests(); feasible_prefix_test(); timeout_regressions(); compound_tests(); metamorphic_tests(); mixed_update_tests();
        numeric_and_repair_tests(); api_tests(); exact_tests(); state_tests(); subset_tests(); search_tests();
        cout << "PboSolver tests passed: " << checks << " checks\n";
    }
};
int main() { PboSolverTest::run(); }
#endif
