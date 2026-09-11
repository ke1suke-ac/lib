#pragma once
#include <bits/stdc++.h>

// 既知の一次式に対する観測・事前情報から、実数・整数・有限候補の係数を推定する
// C++20 / 単一スレッド / double。整数と厳密な整数積和は絶対値 2^50 未満を前提とする
// 自由な実数の厳密等式は事前に消去する。sample は正規化可能な分布を前提とする
// 時間制限は協調的停止。custom 評価器とフックの途中では停止できない

template <class CustomLoss = std::nullptr_t>
class LinearEstimator {
public:
    struct Domain {
        enum class Kind { Real, Integer, Values, Fixed };
        Kind kind = Kind::Real;
        double lower = -std::numeric_limits<double>::infinity();
        double upper = std::numeric_limits<double>::infinity();
        std::vector<double> candidates;

        // 実数区間を作る O(1)
        static Domain real(double lo = -INFINITY, double hi = INFINITY) {
            assert(lo < hi);
            return {Kind::Real, lo, hi, {}};
        }

        // 整数区間を作る O(1)
        static Domain integer(int64_t lo, int64_t hi) {
            assert(lo <= hi && lo > -(int64_t(1) << 50) && hi < (int64_t(1) << 50));
            if (lo == hi) return fixed(static_cast<double>(lo));
            return {Kind::Integer, static_cast<double>(lo), static_cast<double>(hi), {}};
        }

        // 異なる有限候補の集合を作る O(k log k)
        static Domain values(std::vector<double> v) {
            assert(!v.empty());
            for (double x : v) {
                assert(std::isfinite(x));
                (void)x;
            }
            std::sort(v.begin(), v.end());
            v.erase(std::unique(v.begin(), v.end()), v.end());
            if (v.size() == 1) return fixed(v.front());
            const double lo = v.front(), hi = v.back();
            return {Kind::Values, lo, hi, std::move(v)};
        }

        // 既知の固定値を作る O(1)
        static Domain fixed(double x) {
            assert(std::isfinite(x));
            return {Kind::Fixed, x, x, {}};
        }

        // 値域に含まれるかを判定する O(log k)、候補集合以外は O(1)
        bool contains(double x) const {
            if (!std::isfinite(x) || x < lower || x > upper) return false;
            if (kind == Kind::Integer) return x == std::floor(x);
            if (kind == Kind::Values) return std::binary_search(candidates.begin(), candidates.end(), x);
            return true;
        }
    };

    struct Term {
        int variable;
        double coefficient;
    };

    struct LinearForm {
        std::vector<Term> terms;
        double offset = 0;

        // 指定した変数の和を作る O(k)
        static LinearForm sum(std::span<const int> indices) {
            LinearForm f;
            f.terms.reserve(indices.size());
            for (int i : indices)
                f.terms.push_back({i, 1});
            return f;
        }

        // 左の合計から右の合計を引く一次式を作る O(k)
        static LinearForm difference(std::span<const int> left, std::span<const int> right) {
            auto f = sum(left);
            for (int i : right)
                f.terms.push_back({i, -1});
            return f;
        }

        // 一次式を評価する O(k)
        double evaluate(std::span<const double> x) const {
            double z = offset;
            for (auto [j, a] : terms) {
                assert(j >= 0 && j < static_cast<int>(x.size()));
                z += a * x[j];
            }
            return z;
        }
    };

    struct Loss {
        enum class Kind { Gaussian, Laplace, StudentT, GaussianInterval, HardInterval, SoftInterval, Custom };
        Kind kind = Kind::Gaussian;
        double target = 0, scale = 1, lower = -INFINITY, upper = INFINITY, df = 1;
        int custom_id = 0;

        // 正規観測の負の対数尤度を作る O(1)
        static Loss gaussian(double y, double sigma) { return location(Kind::Gaussian, y, sigma); }

        // Laplace 観測の負の対数尤度を作る O(1)
        static Loss laplace(double y, double s) { return location(Kind::Laplace, y, s); }

        // Student t 観測の負の対数尤度を作る O(1)
        static Loss student_t(double y, double s, double nu) {
            assert(std::isfinite(nu) && nu > 0);
            auto f = location(Kind::StudentT, y, s);
            f.df = nu;
            return f;
        }

        // 正規ノイズを加えた値が区間に入る観測を作る O(1)
        static Loss gaussian_interval(double lo, double hi, double sigma) {
            assert(lo < hi);
            return interval(Kind::GaussianInterval, lo, hi, sigma);
        }

        // ノイズを含まない一次式のハード区間を作る O(1)
        static Loss hard_interval(double lo, double hi) { return interval(Kind::HardInterval, lo, hi, 1); }

        // 区間までの距離の二乗を罰する評価を作る O(1)
        static Loss soft_interval(double lo, double hi, double s) {
            return interval(Kind::SoftInterval, lo, hi, s);
        }

        // 利用者の評価器に渡す ID を指定する O(1)
        static Loss custom(int id) {
            Loss f;
            f.kind = Kind::Custom;
            f.custom_id = id;
            return f;
        }

        // 標準因子を評価する O(1)、custom は推定器の評価器を使う
        double evaluate(double z) const {
            const double t = (z - target) / scale;
            switch (kind) {
            case Kind::Gaussian:
                return 0.5 * t * t;
            case Kind::Laplace:
                return std::abs(t);
            case Kind::StudentT:
                return 0.5 * (df + 1) * std::log1p(t * t / df);
            case Kind::HardInterval:
                return lower <= z && z <= upper ? 0 : INFINITY;
            case Kind::SoftInterval: {
                const double d = (z - std::clamp(z, lower, upper)) / scale;
                return 0.5 * d * d;
            }
            case Kind::GaussianInterval: {
                double a = (lower - z) / scale, b = (upper - z) / scale;
                if (a >= 0) {
                    const double old = a;
                    a = -b;
                    b = -old;
                }
                // 非常に狭い区間では、中点密度の積分展開で差の桁落ちを避ける
                const double width = b - a, mid = a + width * 0.5;
                if (std::isfinite(width) && width * (1 + std::abs(mid)) < 1e-3) {
                    const double correction = (mid * mid - 1) * width * width / 24;
                    return 0.5 * mid * mid + 0.9189385332046727418 - std::log(width) - std::log1p(correction);
                }
                // 中央では確率を直接計算し、狭い区間と裾だけを安定した別式へ回す。
                if (b >= -10) {
                    const double probability =
                        b <= 0
                            ? 0.5 * (std::erfc(-b * 0.7071067811865475244)
                                     - std::erfc(-a * 0.7071067811865475244))
                            : 0.5 * (std::erf(b * 0.7071067811865475244)
                                     - std::erf(a * 0.7071067811865475244));
                    return -std::log(probability);
                }
                const double u = log_cdf_tail(b), v = log_cdf_tail(a);
                return -u - std::log(-std::expm1(v - u));
            }
            case Kind::Custom:
                assert(false);
                return INFINITY;
            }
            assert(false);
            return INFINITY;
        }

    private:
        static Loss location(Kind k, double y, double s) {
            assert(std::isfinite(y) && std::isfinite(s) && s > 0);
            Loss f;
            f.kind = k;
            f.target = y;
            f.scale = s;
            return f;
        }

        static Loss interval(Kind k, double lo, double hi, double s) {
            assert(lo <= hi && lo < INFINITY && hi > -INFINITY && std::isfinite(s) && s > 0);
            Loss f;
            f.kind = k;
            f.lower = lo;
            f.upper = hi;
            f.scale = s;
            return f;
        }

        static double log_cdf_tail(double x) {
            // 呼出し側で通常領域を処理済み。ここでは負の裾だけを扱う。
            if (x == -INFINITY) return -INFINITY;
            double s = 1, term = 1;
            for (int k = 1; k <= 50; ++k) {
                const double next = term * (-(2 * k - 1) / (x * x));
                if (std::abs(next) >= std::abs(term)) break;
                s += next;
                term = next;
                if (std::abs(term) < 1e-17) break;
            }
            return -0.5 * x * x - std::log(-x) - 0.9189385332046727418 + std::log(s);
        }
    };

    enum class Method { Automatic, Metropolis, Conditional, Slice };
    enum class StopReason { TimeLimit, StepLimit, NoMovableVariable, NoFeasibleState, Solved };

    struct OptimizeParam {
        int64_t time_limit_us = 2'000, max_steps = -1;
        Method method = Method::Automatic;
        bool auto_directions = true;
        double direction_probability = 0.25, step_scale = 1;
        int enumeration_limit = 32;
        double temperature_start = 1, temperature_end = 0.01, greedy_probability = 0.25;
    };

    struct SampleParam {
        int64_t time_limit_us = 2'000, max_steps = -1, warmup_steps = 128;
        Method method = Method::Automatic;
        bool auto_directions = true;
        double direction_probability = 0.25, step_scale = 1;
        int enumeration_limit = 32;
        int sample_stride = 0; // 0 は可動変数数ごと、1 以上は指定試行数ごと
        bool collect_moments = true;
        int sample_capacity = 0;
    };

    struct Runtime {
        StopReason stop_reason = StopReason::StepLimit;
        int64_t elapsed_us = 0, steps = 0, accepted = 0, samples = 0, repair_steps = 0;
        bool warming_up = false;
    };

    struct SampleView {
        std::span<const double> values;
        double energy;
    };

    struct Moments {
        int64_t count = 0;
        std::vector<double> mean, variance;
    };

    struct Prediction {
        int64_t count = 0;
        double mean = 0, variance = 0;
    };

    // 値域・乱数 seed・任意の評価器を所有する O(n)
    explicit LinearEstimator(std::vector<Domain> domains, uint64_t seed = 0, CustomLoss custom = {})
        : domains_(std::move(domains)), custom_(std::move(custom)), rng_(seed), incidence_(domains_.size()),
          scales_(domains_.size(), 1) {
        assert(!domains_.empty());
        current_.x.reserve(domains_.size());
        for (const auto& d : domains_)
            current_.x.push_back(project(d, 0));
        best_ = current_;
    }

    // 一次式と因子を追加して安定 ID を返す O(k log k)、候補置換時は O(n+R)
    int add_factor(LinearForm form, Loss loss) {
        normalize(form);
        const int id = static_cast<int>(factors_.size());
        double norm = 0;
        for (auto [j, a] : form.terms) {
            norm += a * a;
            incidence_[j].push_back({id, a});
        }
        factors_.push_back({std::move(form), loss, true, std::max(norm, 1.0)});
        for (State* s : {&current_, &best_}) {
            const double z = factors_.back().form.evaluate(s->x), v = cost(loss, z);
            s->z.push_back(z);
            s->v.push_back(v);
            add_cost(*s, v);
        }
        changed();
        select_candidates();
        return id;
    }

    // 因子の評価を変更する O(1)、候補置換時は O(n+R)、尺度・種類変更時の準備は次の実行で行う
    void set_loss(int id, Loss loss) {
        check_id(id);
        auto& f = factors_[id];
        const Loss old = f.loss;
        f.loss = loss;
        // 有効因子の線形値は保たれているため、目標値の訂正で項を走査しない
        for (State* state : {&current_, &best_})
            replace_cost(*state, id, f.enabled ? cost(loss, state->z[id]) : 0);
        const bool same_geometry =
            old.kind == loss.kind && old.scale == loss.scale
            && (old.kind != Loss::Kind::GaussianInterval || old.upper - old.lower == loss.upper - loss.lower)
            && (old.kind != Loss::Kind::HardInterval
                || ((old.lower == old.upper) == (loss.lower == loss.upper)));
        if (!same_geometry) prepared_ = false;
        // 正規観測の目標値だけの訂正では精度行列が変わらない
        if (!same_geometry || old.kind != Loss::Kind::Gaussian) dense_ready_ = false;
        dirty_ready_ = dense_mean_ready_ = false;
        reset_sampling();
        select_candidates();
    }

    // 一次式を含め因子を置換する O(n+R+K+k log k)
    void replace_factor(int id, LinearForm form, Loss loss) {
        check_id(id);
        normalize(form);
        double norm = 0;
        for (auto [j, a] : form.terms) {
            (void)j;
            norm += a * a;
        }
        factors_[id].form = std::move(form);
        factors_[id].loss = loss;
        factors_[id].norm = std::max(norm, 1.0);
        incidence_.assign(domains_.size(), {});
        for (int r = 0; r < static_cast<int>(factors_.size()); ++r)
            for (auto [j, a] : factors_[r].form.terms)
                incidence_[j].push_back({r, a});
        refresh_factor(id);
        changed();
        select_candidates();
    }

    // 因子を無効化または再有効化する O(k)、候補置換時は O(n+R)
    void set_enabled(int id, bool enabled) {
        check_id(id);
        if (factors_[id].enabled == enabled) return;
        factors_[id].enabled = enabled;
        refresh_factor(id);
        changed();
        select_candidates();
    }

    // 値域を変更し、射影と関係因子だけを再評価する。候補置換時は O(n+R) も必要
    void set_domain(int j, Domain domain) {
        assert(j >= 0 && j < static_cast<int>(domains_.size()));
        domains_[j] = std::move(domain);
        for (State* s : {&current_, &best_})
            s->x[j] = project(domains_[j], s->x[j]);
        for (auto [r, a] : incidence_[j])
            refresh_factor(r);
        changed();
        select_candidates();
    }

    // 値域内の初期状態を指定する O(n+R+K)、連結制約違反なら修復候補として保持する
    void set_state(std::span<const double> values) {
        assert(values.size() == domains_.size());
        for (int j = 0; j < static_cast<int>(domains_.size()); ++j)
            assert(domains_[j].contains(values[j]));
        current_.x.assign(values.begin(), values.end());
        refresh(current_);
        best_ = current_;
        states_same_ = true;
        dirty_ready_ = false;
        reset_sampling();
    }

    // 疎な固定方向を登録する O(総項数 log 最大項数)、実数と整数の混在方向は指定しない
    void set_directions(std::vector<std::vector<Term>> directions) {
        for (auto& terms : directions) {
            LinearForm f{std::move(terms), 0};
            normalize(f);
            assert(!f.terms.empty());
            terms = std::move(f.terms);
            [[maybe_unused]] bool integral = false, real = false;
            for (auto [j, a] : terms) {
                assert(domains_[j].kind != Domain::Kind::Values && domains_[j].kind != Domain::Kind::Fixed);
                if (domains_[j].kind == Domain::Kind::Integer) {
                    assert(a == std::round(a));
                    integral = true;
                } else
                    real = true;
            }
            assert(!(integral && real));
        }
        user_directions_ = std::move(directions);
        prepared_ = false;
        reset_sampling();
    }

    // 現行モデルの実行可能候補の有無を返す O(1)
    bool has_solution() const { return current_.bad == 0; }

    // 現在候補を参照する O(1)、次の変更・実行まで有効
    std::span<const double> current() const {
        assert(has_solution());
        return current_.x;
    }

    // 最良候補を参照する O(1)、次の変更・実行まで有効
    std::span<const double> best() const {
        assert(has_solution());
        return best_.x;
    }

    // 現行モデルでの最良評価値を返す O(1)
    double best_energy() const {
        assert(has_solution());
        return best_.sum;
    }

    // 現行採取区間の統計を参照する O(1)、count が 0 なら平均・分散は無効
    const Moments& moments() const { return moments_; }

    // 保存中の全体サンプル数を返す O(1)
    int stored_sample_count() const { return stored_count_; }

    // 古い順に保存サンプルを参照する O(1)、次の変更・実行まで有効
    std::span<const double> stored_sample(int index) const {
        assert(index >= 0 && index < stored_count_);
        const int pos = (stored_head_ + index) % capacity_;
        return {stored_.data() + static_cast<size_t>(pos) * domains_.size(), domains_.size()};
    }

    // 保存サンプルから一次式の平均・分散を求める O(Sk)、観測ノイズは含まない
    Prediction predict(const LinearForm& form) const {
        Prediction p;
        double m2 = 0;
        for (int i = 0; i < stored_count_; ++i) {
            const double z = form.evaluate(stored_sample(i));
            ++p.count;
            const double delta = z - p.mean;
            p.mean += delta / static_cast<double>(p.count);
            m2 += delta * (z - p.mean);
        }
        if (p.count) p.variance = m2 / static_cast<double>(p.count);
        return p;
    }

    // 任意の値域内外の状態を全評価する O(n+R+K)
    double evaluate(std::span<const double> values) const {
        assert(values.size() == domains_.size());
        double e = 0;
        for (int j = 0; j < static_cast<int>(domains_.size()); ++j)
            if (!domains_[j].contains(values[j])) return INFINITY;
        for (const auto& f : factors_)
            if (f.enabled) e += cost(f.loss, f.form.evaluate(values));
        return e;
    }

    // MAP 候補を探索する O(準備+試行ごとの関係因子数)、最良保存・統計の費用を含む
    template <class DebugHook = std::nullptr_t>
    Runtime optimize(const OptimizeParam& param = {}, DebugHook debug_hook = {}) {
        return run<false>(param, nullptr, std::move_if_noexcept(debug_hook));
    }

    // 温度 1 の分布を採取する O(準備+関係因子評価+採取数*n)、フック費用は別途加算
    template <class SampleHook = std::nullptr_t, class DebugHook = std::nullptr_t>
    Runtime sample(const SampleParam& param = {}, SampleHook sample_hook = {}, DebugHook debug_hook = {}) {
        return run<true>(param, std::move_if_noexcept(sample_hook), std::move_if_noexcept(debug_hook));
    }

private:
    struct Factor {
        LinearForm form;
        Loss loss;
        bool enabled;
        double norm;
    };

    struct State {
        std::vector<double> x, z, v;
        double sum = 0;
        int bad = 0;
    };

    struct Move {
        std::vector<Term> terms, effects;
        double h = 0, approximate_h = 0, scale = 1;
        bool integral = false, nonquadratic = false, nonconvex = false, has_reference = false;
        int finite_variable = -1, finite_partner = -1, adaptation = 0;
    };

    struct Rng {
        uint64_t state;
        bool spare_ready = false;
        double spare = 0;

        explicit Rng(uint64_t seed) : state(seed + 0x9e3779b97f4a7c15ULL) {}

        uint64_t next() {
            state += 0x9e3779b97f4a7c15ULL;
            uint64_t z = state;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return z ^ (z >> 31);
        }

        double uniform() { return (static_cast<double>(next() >> 12) + 0.5) * 0x1p-52; }

        int index(int n) {
            const uint64_t size = static_cast<uint64_t>(n), threshold = -size % size;
            uint64_t x;
            do {
                x = next();
            } while (x < threshold);
            return static_cast<int>(x % size);
        }

        double normal() {
            if (spare_ready) {
                spare_ready = false;
                return spare;
            }
            double a, b, r;
            do {
                a = 2 * uniform() - 1;
                b = 2 * uniform() - 1;
                r = a * a + b * b;
            } while (r >= 1 || r == 0);
            const double m = std::sqrt(-2 * std::log(r) / r);
            spare = b * m;
            spare_ready = true;
            return a * m;
        }
    };

    struct Budget {
        std::chrono::steady_clock::time_point start;
        int64_t limit;
        Runtime& runtime;

        bool expired(bool force = false) {
            if (limit < 0 && !force) return false;
            runtime.elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::steady_clock::now() - start)
                                     .count();
            return limit >= 0 && runtime.elapsed_us >= limit;
        }
    };

    std::vector<Domain> domains_;
    CustomLoss custom_;
    Rng rng_;
    std::vector<Factor> factors_;
    std::vector<std::vector<Term>> incidence_, user_directions_;
    State current_, best_;
    std::vector<Move> moves_;
    std::vector<double> scales_, choices_, choice_energy_;
    std::vector<double> evaluated_costs_;
    const Move* evaluated_move_ = nullptr;
    double evaluated_t_ = 0;
    bool evaluated_partial_ = false;
    std::vector<int> hard_ids_, finite_rows_;
    int single_count_ = 0, scan_index_ = 0;
    bool prepared_ = false, prepared_auto_ = false, sampling_ = false, states_same_ = true;
    bool dense_mean_ready_ = false, dense_ready_ = false, dense_active_ = false, dense_decided_ = false,
         dirty_ready_ = false;
    std::vector<int> dirty_variables_, dirty_factors_;
    std::vector<unsigned char> variable_mark_, factor_mark_;
    std::vector<int> dense_variables_;
    std::vector<double> dense_cholesky_, dense_mean_, dense_work_;
    double dense_min_energy_ = 0;
    bool dense_stale_ = false;
    SampleParam previous_sample_;
    int64_t warmup_done_ = 0, stride_done_ = 0;
    Moments moments_;
    std::vector<double> m2_, stored_;
    int capacity_ = 0, stored_count_ = 0, stored_head_ = 0;

    void check_id(int id) const {
        assert(id >= 0 && id < static_cast<int>(factors_.size()));
        (void)id;
    }

    double cost(const Loss& loss, double z) const {
        double v;
        if (loss.kind != Loss::Kind::Custom)
            v = loss.evaluate(z);
        else if constexpr (!std::is_same_v<CustomLoss, std::nullptr_t>)
            v = custom_(loss.custom_id, z);
        else {
            assert(false);
            v = INFINITY;
        }
        assert(!std::isnan(v) && v != -INFINITY);
        return v;
    }

    static void add_cost(State& s, double v) {
        if (std::isfinite(v))
            s.sum += v;
        else
            ++s.bad;
    }

    static void replace_cost(State& s, int r, double v) {
        if (std::isfinite(s.v[r]))
            s.sum -= s.v[r];
        else
            --s.bad;
        s.v[r] = v;
        add_cost(s, v);
    }

    static double project(const Domain& d, double x) {
        x = std::clamp(x, d.lower, d.upper);
        if (d.kind == Domain::Kind::Integer) return std::round(x);
        if (d.kind != Domain::Kind::Values) return x;
        auto it = std::lower_bound(d.candidates.begin(), d.candidates.end(), x);
        if (it == d.candidates.begin()) return *it;
        if (it == d.candidates.end()) return d.candidates.back();
        return x - *(it - 1) <= *it - x ? *(it - 1) : *it;
    }

    void normalize(LinearForm& f) const {
        assert(std::isfinite(f.offset));
        for (auto [j, a] : f.terms) {
            assert(j >= 0 && j < static_cast<int>(domains_.size()) && std::isfinite(a));
            (void)j;
            (void)a;
        }
        std::sort(f.terms.begin(), f.terms.end(), [](Term a, Term b) { return a.variable < b.variable; });
        int count = 0;
        for (auto t : f.terms) {
            if (count && f.terms[count - 1].variable == t.variable)
                f.terms[count - 1].coefficient += t.coefficient;
            else
                f.terms[count++] = t;
        }
        f.terms.resize(count);
        std::erase_if(f.terms, [](Term t) { return t.coefficient == 0; });
    }

    void refresh(State& s) {
        s.sum = 0;
        s.bad = 0;
        s.z.resize(factors_.size());
        s.v.resize(factors_.size());
        for (int r = 0; r < static_cast<int>(factors_.size()); ++r) {
            const auto& f = factors_[r];
            s.z[r] = f.enabled ? f.form.evaluate(s.x) : 0;
            s.v[r] = f.enabled ? cost(f.loss, s.z[r]) : 0;
            add_cost(s, s.v[r]);
        }
    }

    void refresh_factor(int r) {
        const auto& f = factors_[r];
        for (State* s : {&current_, &best_}) {
            s->z[r] = f.enabled ? f.form.evaluate(s->x) : 0;
            replace_cost(*s, r, f.enabled ? cost(f.loss, s->z[r]) : 0);
        }
    }

    void select_candidates() {
        if (states_same_) return;
        if (current_.bad && !best_.bad) {
            current_ = best_;
            states_same_ = true;
            dirty_ready_ = false;
        } else if (!current_.bad && (best_.bad || current_.sum <= best_.sum)) {
            best_ = current_;
            states_same_ = true;
        }
    }

    void reset_sampling() {
        scan_index_ = 0;
        sampling_ = false;
        dense_active_ = dense_decided_ = false;
        warmup_done_ = stride_done_ = 0;
        moments_.count = 0;
        stored_count_ = stored_head_ = 0;
    }

    void changed() {
        dirty_ready_ = false;
        prepared_ = false;
        dense_ready_ = false;
        reset_sampling();
    }

    bool prepare(bool automatic, Budget& budget) {
        if (prepared_ && prepared_auto_ == automatic) return true;
        prepared_ = false; // 中断した作業配列を、前の設定で再利用しない
        moves_.clear();
        hard_ids_.clear();
        variable_mark_.assign(domains_.size(), 0);
        factor_mark_.assign(factors_.size(), 0);
        dirty_variables_.clear();
        dirty_factors_.clear();
        dirty_ready_ = states_same_;
        std::vector<double> effect(factors_.size(), 0);
        std::vector<int> mark(factors_.size(), -1), touched;
        int stamp = 0;
        // 方向ごとの因子係数と正規曲率を一度だけ集約する
        auto append = [&](std::vector<Term> terms) {
            Move m;
            m.terms = std::move(terms);
            bool real = false;
            for (auto [j, a] : m.terms) {
                if (domains_[j].kind == Domain::Kind::Fixed) return;
                if (domains_[j].kind == Domain::Kind::Real) real = true;
                if (domains_[j].kind == Domain::Kind::Integer) {
                    if (a != std::round(a)) return;
                    m.integral = true;
                }
                if (domains_[j].kind == Domain::Kind::Values) {
                    if (m.terms.size() != 1 || a != 1) return;
                    m.finite_variable = j;
                }
            }
            if (real && m.integral) return;
            if (m.integral) {
                int64_t divisor = 0;
                for (auto t : m.terms)
                    divisor = std::gcd(divisor, static_cast<int64_t>(std::abs(t.coefficient)));
                for (auto& t : m.terms)
                    t.coefficient /= static_cast<double>(divisor);
            }
            touched.clear();
            ++stamp;
            for (auto [j, a] : m.terms)
                for (auto [r, b] : incidence_[j])
                    if (factors_[r].enabled) {
                        if (mark[r] != stamp) {
                            mark[r] = stamp;
                            effect[r] = 0;
                            touched.push_back(r);
                        }
                        effect[r] += a * b;
                    }
            for (int r : touched)
                if (effect[r] != 0) {
                    m.effects.push_back({r, effect[r]});
                    const auto& f = factors_[r];
                    if (m.finite_variable >= 0 && f.loss.kind == Loss::Kind::HardInterval
                        && m.finite_partner < 0)
                        for (auto [j, a] : f.form.terms)
                            if (j != m.finite_variable && domains_[j].kind == Domain::Kind::Values) {
                                m.finite_partner = j;
                                break;
                            }
                    if (f.loss.kind == Loss::Kind::Gaussian) {
                        const double a = effect[r] / f.loss.scale;
                        m.h += a * a;
                        m.approximate_h += a * a;
                    } else if (f.loss.kind != Loss::Kind::HardInterval) {
                        m.nonquadratic = true;
                        const double precision = gaussian_reference(f.loss).second;
                        if (precision > 0) {
                            m.h += effect[r] * effect[r] * precision;
                            m.has_reference = true;
                        }
                        m.nonconvex |=
                            f.loss.kind == Loss::Kind::StudentT || f.loss.kind == Loss::Kind::Custom;
                        if (f.loss.kind != Loss::Kind::Custom) {
                            const double a = effect[r] / f.loss.scale;
                            m.approximate_h += precision > 0 ? effect[r] * effect[r] * precision : a * a;
                        }
                    }
                }
            if (m.terms.size() == 1) m.scale = scales_[m.terms[0].variable];
            moves_.push_back(std::move(m));
        };
        for (int j = 0; j < static_cast<int>(domains_.size()); ++j) {
            if ((j & 63) == 0 && budget.expired()) return false;
            if (domains_[j].kind != Domain::Kind::Fixed) append({{j, 1}});
        }
        single_count_ = static_cast<int>(moves_.size());
        for (const auto& d : user_directions_)
            append(d);
        // 各変数につき強い因子で隣接する一組を候補にし、二乗個の方向を作らない
        if (automatic) {
            struct Edge {
                double score = -1;
                int u = -1, v = -1;
                double ratio = 0;
            };

            std::vector<Edge> edges(domains_.size());
            std::vector<double> diagonal(domains_.size(), 0);
            for (int k = 0; k < single_count_; ++k)
                diagonal[moves_[k].terms[0].variable] = moves_[k].h;
            for (const auto& f : factors_)
                if (f.enabled) {
                    if (f.loss.kind != Loss::Kind::Gaussian
                        && !(f.loss.kind == Loss::Kind::HardInterval && f.loss.lower == f.loss.upper))
                        continue;
                    const double weight =
                        f.loss.kind == Loss::Kind::Gaussian ? 1 / (f.loss.scale * f.loss.scale) : 1;
                    for (size_t k = 1; k < f.form.terms.size(); ++k) {
                        auto [u, a] = f.form.terms[k - 1];
                        auto [v, b] = f.form.terms[k];
                        const auto ku = domains_[u].kind, kv = domains_[v].kind;
                        if (ku != kv || (ku != Domain::Kind::Real && ku != Domain::Kind::Integer)) continue;
                        if (ku == Domain::Kind::Integer && std::abs(a) != std::abs(b)) continue;
                        const double score = std::abs(a * b) * weight, ratio = -a / b;
                        for (int j : {u, v})
                            if (edges[j].score < score) edges[j] = {score, u, v, ratio};
                    }
                }
            std::set<std::pair<int, int>> seen;
            for (const auto& e : edges)
                if (e.u >= 0 && seen.emplace(e.u, e.v).second) {
                    const auto& a = incidence_[e.u];
                    const auto& b = incidence_[e.v];
                    size_t i = 0, j = 0;
                    double coupling = 0;
                    bool equality = false;
                    while (i < a.size() && j < b.size()) {
                        if (a[i].variable < b[j].variable) {
                            ++i;
                            continue;
                        }
                        if (a[i].variable > b[j].variable) {
                            ++j;
                            continue;
                        }
                        const auto& f = factors_[a[i].variable];
                        if (f.enabled && f.loss.kind == Loss::Kind::Gaussian)
                            coupling += a[i].coefficient * b[j].coefficient / (f.loss.scale * f.loss.scale);
                        if (f.enabled && f.loss.kind == Loss::Kind::HardInterval
                            && f.loss.lower == f.loss.upper)
                            equality = true;
                        ++i;
                        ++j;
                    }
                    if (equality || std::abs(coupling) > 0.3 * std::sqrt(diagonal[e.u] * diagonal[e.v]))
                        append({{e.u, 1}, {e.v, e.ratio}});
                }
        }
        for (int r = 0; r < static_cast<int>(factors_.size()); ++r)
            if (factors_[r].enabled && factors_[r].loss.kind == Loss::Kind::HardInterval)
                hard_ids_.push_back(r);
        prepared_ = true;
        prepared_auto_ = automatic;
        return true;
    }

    std::pair<double, double> bounds(const Move& m, bool hard = true) const {
        double lo = -INFINITY, hi = INFINITY;
        auto intersect = [&](double a, double b, double coefficient) {
            a /= coefficient;
            b /= coefficient;
            if (coefficient < 0) std::swap(a, b);
            lo = std::max(lo, a);
            hi = std::min(hi, b);
        };
        for (auto [j, a] : m.terms)
            intersect(domains_[j].lower - current_.x[j], domains_[j].upper - current_.x[j], a);
        if (hard)
            for (auto [r, c] : m.effects) {
                const auto& f = factors_[r];
                if (f.loss.kind == Loss::Kind::HardInterval)
                    intersect(f.loss.lower - current_.z[r], f.loss.upper - current_.z[r], c);
            }
        if (m.integral) {
            lo = std::ceil(lo);
            hi = std::floor(hi);
        }
        return {lo, hi};
    }

    // 有限区間の観測を畳み込みの平均・分散で近似する。提案だけに使い、MHで元の尤度へ補正する。
    static std::pair<double, double> gaussian_reference(const Loss& l) {
        if (l.kind == Loss::Kind::Gaussian) return {l.target, 1 / (l.scale * l.scale)};
        if (l.kind == Loss::Kind::GaussianInterval && std::isfinite(l.lower) && std::isfinite(l.upper)) {
            const double width = l.upper - l.lower;
            return {l.lower + 0.5 * width, 1 / (l.scale * l.scale + width * width / 12)};
        }
        return {0, 0};
    }

    double gradient(const Move& m) const {
        double g = 0;
        for (auto [r, c] : m.effects) {
            const auto& l = factors_[r].loss;
            const auto [center, precision] = gaussian_reference(l);
            g += (current_.z[r] - center) * c * precision;
        }
        return g;
    }

    double delta(const Move& m, double t, bool residual_only = false) {
        double d = 0;
        evaluated_move_ = m.nonquadratic ? &m : nullptr;
        size_t pos = 0;
        if (evaluated_move_) {
            evaluated_t_ = t;
            evaluated_partial_ = residual_only;
            evaluated_costs_.resize(m.effects.size());
        }
        int exact_variable = m.finite_variable;
        double exact_value = 0;
        if (m.terms.size() == 1 && !m.integral) {
            const auto [j, a] = m.terms.front();
            const double x = current_.x[j] + a * t;
            exact_value = project(domains_[j], x);
            if (exact_value != x) exact_variable = j;
        } else if (!m.integral) {
            // 同時更新は方向を崩す射影をせず、丸めによる値域外も棄却する
            for (auto [j, a] : m.terms)
                if (!domains_[j].contains(current_.x[j] + a * t)) return INFINITY;
        }
        for (auto [r, c] : m.effects) {
            const auto& l = factors_[r].loss;
            if (residual_only && l.kind == Loss::Kind::Gaussian) {
                ++pos;
                continue;
            }
            double z = current_.z[r] + c * t;
            if (exact_variable >= 0) {
                // 有限候補と単独更新の境界丸めは、確定する値をそのまま代入する
                z = factors_[r].form.offset;
                for (auto [j, a] : factors_[r].form.terms)
                    z += a * (j == exact_variable ? exact_value : current_.x[j]);
            }
            const double v = cost(l, z);
            if (evaluated_move_) evaluated_costs_[pos] = v;
            ++pos;
            if (!std::isfinite(v)) return INFINITY;
            d += v - current_.v[r];
            if (residual_only && l.kind == Loss::Kind::GaussianInterval) {
                const auto [center, precision] = gaussian_reference(l);
                d -= 0.5 * precision * (z - current_.z[r]) * (z + current_.z[r] - 2 * center);
            }
        }
        return d;
    }

    void save_best() {
        if (!dirty_ready_) {
            best_ = current_;
            variable_mark_.assign(domains_.size(), 0);
            factor_mark_.assign(factors_.size(), 0);
            dirty_variables_.clear();
            dirty_factors_.clear();
            dirty_ready_ = true;
        } else {
            for (int j : dirty_variables_) {
                best_.x[j] = current_.x[j];
                variable_mark_[j] = 0;
            }
            for (int r : dirty_factors_) {
                best_.z[r] = current_.z[r];
                best_.v[r] = current_.v[r];
                factor_mark_[r] = 0;
            }
            best_.sum = current_.sum;
            best_.bad = current_.bad;
            dirty_variables_.clear();
            dirty_factors_.clear();
        }
        states_same_ = true;
    }

    void commit(const Move& m, double t) {
        // 直前に評価した同じ候補だけを再利用し、部分評価で省いた正規因子は補う
        const bool reuse = evaluated_move_ == &m && evaluated_t_ == t;
        size_t pos = 0;
        states_same_ = false;
        bool exact = m.finite_variable >= 0;
        for (auto [j, a] : m.terms) {
            if (dirty_ready_ && !variable_mark_[j]) {
                variable_mark_[j] = 1;
                dirty_variables_.push_back(j);
            }
            const double x = current_.x[j] + a * t;
            current_.x[j] = m.terms.size() == 1 && !m.integral ? project(domains_[j], x) : x;
            exact |= current_.x[j] != x;
        }
        for (auto [r, c] : m.effects) {
            if (dirty_ready_ && !factor_mark_[r]) {
                factor_mark_[r] = 1;
                dirty_factors_.push_back(r);
            }
            current_.z[r] = exact ? factors_[r].form.evaluate(current_.x) : current_.z[r] + c * t;
            replace_cost(current_,
                         r,
                         reuse && !(evaluated_partial_ && factors_[r].loss.kind == Loss::Kind::Gaussian)
                             ? evaluated_costs_[pos]
                             : cost(factors_[r].loss, current_.z[r]));
            ++pos;
        }
        evaluated_move_ = nullptr;
        if (!current_.bad && (best_.bad || current_.sum < best_.sum)) save_best();
    }

    // 棄却法は区間外の値を境界へ押し戻さず、元の切断分布を保つ
    double truncated_normal(double mu, double sd, double lo, double hi, Budget& budget, bool& aborted) {
        double a = (lo - mu) / sd, b = (hi - mu) / sd, sign = 1;
        if (b < 0) {
            const double old = a;
            a = -b;
            b = -old;
            sign = -1;
        }
        int attempt = 0;
        for (;;) {
            if ((++attempt & 63) == 0 && budget.expired()) {
                aborted = true;
                return 0;
            }
            double z;
            if (std::isfinite(b - a) && b - a < 1.5 / std::max(1.0, a)) {
                z = a + (b - a) * rng_.uniform();
                const double mode = std::clamp(0.0, a, b);
                if (std::log(rng_.uniform()) > -0.5 * (z * z - mode * mode)) continue;
            } else if (a > 0.5) {
                const double rate = (a + std::sqrt(a * a + 4)) * 0.5;
                z = a - std::log(rng_.uniform()) / rate;
                if (z > b || std::log(rng_.uniform()) > -0.5 * (z - rate) * (z - rate)) continue;
            } else {
                z = rng_.normal();
                if (z < a || z > b) continue;
            }
            const double t = mu + sign * sd * z;
            if (t >= lo && t <= hi) return t;
        }
    }

    // 二つの幾何乱数の差を包絡分布として、整数上の正規重みから正確に採取する
    double discrete_normal(double mu, double h, double lo, double hi, Budget& budget, bool& aborted) {
        const double mode = std::clamp(std::round(mu), lo, hi);
        const double slope = std::max(std::sqrt(h), h * std::abs(mode - mu));
        const bool uniform = (hi - lo + 1) * slope < 2;
        const double rate = std::min(1.0, slope);
        auto f = [&](double t) {
            return -0.5 * h * (t - mu) * (t - mu) + (uniform ? 0 : rate * std::abs(t - mode));
        };
        double envelope = f(mode);
        if (!uniform)
            for (double q : {std::clamp(mu + rate / h, mode, hi), std::clamp(mu - rate / h, lo, mode)})
                envelope = std::max({envelope, f(std::floor(q)), f(std::ceil(q))});
        for (int attempt = 1;; ++attempt) {
            if ((attempt & 63) == 0 && budget.expired()) {
                aborted = true;
                return 0;
            }
            const double t = uniform ? lo + std::floor((hi - lo + 1) * rng_.uniform())
                                     : mode + std::floor(-std::log(rng_.uniform()) / rate)
                                           - std::floor(-std::log(rng_.uniform()) / rate);
            if (t >= lo && t <= hi && std::log(rng_.uniform()) <= f(t) - envelope) return t;
        }
    }

    double move_width(const Move& m, double multiplier) const {
        if (m.approximate_h > 0) return multiplier * m.scale / std::sqrt(m.approximate_h);
        double w = INFINITY;
        for (auto [j, a] : m.terms) {
            const auto& d = domains_[j];
            const double range = std::isfinite(d.upper - d.lower) ? (d.upper - d.lower) * 0.1 : 1;
            w = std::min(w, range / std::abs(a));
        }
        return std::max(1e-12, w * multiplier * m.scale);
    }

    // 最小候補は評価時に求め、Boltzmann重みからの採取を二つの離散経路で共用する。
    int choose(double temperature, int best) {
        if (temperature == 0) return best;
        const double minimum = choice_energy_[best];
        double total = 0;
        for (double& e : choice_energy_) {
            e = std::exp(-(e - minimum) / temperature);
            total += e;
        }
        double u = rng_.uniform() * total;
        for (int k = 0; k < static_cast<int>(choice_energy_.size()); ++k)
            if ((u -= choice_energy_[k]) <= 0) return k;
        return static_cast<int>(choice_energy_.size()) - 1;
    }

    bool update(Move& m,
                Method method,
                int enumeration,
                double multiplier,
                double temperature,
                Budget& budget,
                bool& aborted) {
        const bool greedy = temperature == 0;
        auto [lo, hi] = m.finite_variable >= 0 ? std::pair<double, double>{-INFINITY, INFINITY}
                                               : bounds(m, method != Method::Metropolis);
        if (lo >= hi) return false;
        double t = 0;
        bool gaussian_proposal = false;
        // 小さな離散集合は全候補の条件付き重みを評価する
        choices_.clear();
        if (method != Method::Metropolis) {
            if (m.finite_variable >= 0) {
                const auto& d = domains_[m.finite_variable];
                if (static_cast<int>(d.candidates.size()) <= enumeration)
                    for (double x : d.candidates)
                        choices_.push_back(x - current_.x[m.finite_variable]);
            } else if (m.integral && hi - lo < enumeration) {
                for (double v = lo; v <= hi; ++v)
                    choices_.push_back(v);
            }
        }
        if (!choices_.empty()) {
            choice_energy_.resize(choices_.size());
            int best = 0;
            const double g = m.nonquadratic || m.finite_variable >= 0 ? 0 : gradient(m);
            for (size_t k = 0; k < choices_.size(); ++k) {
                choice_energy_[k] = m.nonquadratic || m.finite_variable >= 0
                                        ? delta(m, choices_[k])
                                        : g * choices_[k] + 0.5 * m.h * choices_[k] * choices_[k];
                if (choice_energy_[k] < choice_energy_[best]) best = static_cast<int>(k);
            }
            t = choices_[choose(temperature, best)];
            if (t != 0 && std::isfinite(delta(m, t))) {
                commit(m, t);
                return true;
            }
            return false;
        }
        // 非正規目的の最適化では三点の値から局所的な二次近似を試す
        if (greedy && method == Method::Automatic && m.nonquadratic && m.finite_variable < 0) {
            const double width = move_width(m, multiplier);
            double a = std::max(lo, -width), b = std::min(hi, width);
            if (m.integral) {
                a = std::floor(a);
                b = std::ceil(b);
                a = std::max(a, lo);
                b = std::min(b, hi);
            }
            double best_delta = 0, chosen = 0;
            const double fa = delta(m, a), fb = delta(m, b);
            if (fa < best_delta) {
                best_delta = fa;
                chosen = a;
            }
            if (fb < best_delta) {
                best_delta = fb;
                chosen = b;
            }
            if (a < 0 && b > 0 && std::isfinite(fa + fb)) {
                const double curvature = 2 * (fb / b - fa / a) / (b - a);
                const double slope = fb / b - 0.5 * curvature * b;
                if (curvature > 0) {
                    double q =
                        std::clamp(-slope / curvature, std::max(lo, -4 * width), std::min(hi, 4 * width));
                    if (m.integral) q = std::clamp(std::round(q), lo, hi);
                    const double fq = delta(m, q);
                    if (fq < best_delta) chosen = q;
                }
            }
            if (chosen != 0) {
                commit(m, chosen);
                return true;
            }
            return false;
        }
        // 二次曲率のない一様な条件付き分布は、許容区間から直接採取する
        if (method != Method::Metropolis && !m.nonquadratic && m.h == 0 && m.finite_variable < 0
            && std::isfinite(lo) && std::isfinite(hi)) {
            t = m.integral ? lo + std::floor((hi - lo + 1) * rng_.uniform())
                           : lo + (hi - lo) * rng_.uniform();
        }
        // 正規部分の条件付き提案は、残りの因子だけを MH 補正する
        else if (method != Method::Metropolis && m.h > 0
                 && (method != Method::Automatic || m.h >= 0.25 * m.approximate_h) && m.finite_variable < 0
                 && !(method == Method::Slice && m.nonquadratic && !m.integral)) {
            const double mu = -gradient(m) / m.h;
            if (greedy) {
                t = std::clamp(mu, lo, hi);
                if (m.integral) t = std::round(t);
            } else if (m.integral)
                t = discrete_normal(mu, m.h / temperature, lo, hi, budget, aborted);
            else
                t = truncated_normal(mu, std::sqrt(temperature / m.h), lo, hi, budget, aborted);
            if (aborted) return false;
            gaussian_proposal = !greedy;
        } else if (method == Method::Slice && !m.integral && m.finite_variable < 0 && !greedy) {
            // Neal の stepping-out: 左右への拡張回数を乱数で分割して可逆性を保つ
            const double width = 8 * move_width(m, multiplier);
            const double level = -temperature * std::log(rng_.uniform());
            double left = -width * rng_.uniform(), right = left + width;
            int l = rng_.index(16), r = 15 - l;
            while (l-- > 0 && left > lo && delta(m, left) < level)
                left -= width;
            while (r-- > 0 && right < hi && delta(m, right) < level)
                right += width;
            left = std::max(left, lo);
            right = std::min(right, hi);
            for (int attempt = 1;; ++attempt) {
                if ((attempt & 31) == 0 && budget.expired()) {
                    aborted = true;
                    return false;
                }
                t = left + (right - left) * rng_.uniform();
                if (delta(m, t) <= level) {
                    if (t != 0) commit(m, t);
                    return t != 0;
                }
                if (t < 0)
                    left = t;
                else
                    right = t;
            }
        } else if (m.finite_variable >= 0) {
            const int j = m.finite_variable;
            const auto& v = domains_[j].candidates;
            t = v[rng_.index(static_cast<int>(v.size()))] - current_.x[j];
        } else if (m.integral) {
            const double radius = std::max(1.0, std::ceil(move_width(m, multiplier)));
            t = std::floor((2 * radius + 1) * rng_.uniform()) - radius;
        } else
            t = rng_.normal() * move_width(m, multiplier);
        if (t == 0 || t < lo || t > hi) return false;
        const double d = delta(m, t, gaussian_proposal);
        const bool accept =
            std::isfinite(d) && (greedy ? d <= 0 : std::log(rng_.uniform()) <= -d / temperature);
        if (accept) commit(m, t);
        return accept;
    }

    // 準備時に固定したペアを全列挙する。状態に依存してペアを選ばない。
    bool finite_block(int j, int k, int limit, double temperature) {
        const auto& a = domains_[j].candidates;
        const auto& b = domains_[k].candidates;
        if (a.size() * b.size() > static_cast<size_t>(limit)) return false;
        auto& rows = finite_rows_;
        rows.clear();
        for (int v : {j, k})
            for (auto [id, c] : incidence_[v])
                if (factors_[id].enabled) rows.push_back(id);
        std::sort(rows.begin(), rows.end());
        rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
        auto energy = [&](double x, double y) {
            double sum = 0;
            for (int id : rows) {
                const auto& factor = factors_[id];
                double z = factor.form.offset;
                for (auto [v, c] : factor.form.terms)
                    z += c * (v == j ? x : v == k ? y : current_.x[v]);
                sum += cost(factor.loss, z);
            }
            return sum;
        };
        choice_energy_.clear();
        int best = 0;
        for (double x : a)
            for (double y : b) {
                choice_energy_.push_back(energy(x, y));
                if (choice_energy_.back() < choice_energy_[best])
                    best = static_cast<int>(choice_energy_.size()) - 1;
            }
        const int chosen = choose(temperature, best);
        const double x = a[static_cast<size_t>(chosen) / b.size()],
                     y = b[static_cast<size_t>(chosen) % b.size()];
        if (x == current_.x[j] && y == current_.x[k]) return false;
        current_.x[j] = x;
        current_.x[k] = y;
        for (int id : rows) {
            current_.z[id] = factors_[id].form.evaluate(current_.x);
            replace_cost(current_, id, cost(factors_[id].loss, current_.z[id]));
        }
        states_same_ = false;
        dirty_ready_ = false;
        if (!current_.bad && (best_.bad || current_.sum < best_.sum)) save_best();
        return true;
    }

    bool repair_step() {
        if (hard_ids_.empty()) return false;
        int violated = -1, start = rng_.index(static_cast<int>(hard_ids_.size()));
        for (int k = 0; k < static_cast<int>(hard_ids_.size()); ++k) {
            const int r = hard_ids_[(start + k) % static_cast<int>(hard_ids_.size())];
            if (!std::isfinite(current_.v[r])) {
                violated = r;
                break;
            }
        }
        if (violated < 0) return false;
        const auto& f = factors_[violated];
        if (f.form.terms.empty()) return false;
        auto [j, a] = f.form.terms[rng_.index(static_cast<int>(f.form.terms.size()))];
        if (domains_[j].kind == Domain::Kind::Fixed) return true;
        const double target = std::clamp(current_.z[violated], f.loss.lower, f.loss.upper);
        // 整数修復は過大な往復を抑える。有限候補への射影幅は縮めない
        double value = project(domains_[j],
                               current_.x[j]
                                   + (domains_[j].kind == Domain::Kind::Integer ? 0.5 : 1)
                                         * (target - current_.z[violated]) / a);
        if (domains_[j].kind == Domain::Kind::Integer && value == current_.x[j])
            value = project(domains_[j], value + ((target - current_.z[violated]) / a > 0 ? 1 : -1));
        const double t = value - current_.x[j];
        const Move& m =
            *std::lower_bound(moves_.begin(), moves_.begin() + single_count_, j, [](const Move& move, int v) {
                return move.terms.front().variable < v;
            });
        double difference = 0;
        for (auto [r, c] : m.effects) {
            const auto& row = factors_[r];
            if (row.loss.kind != Loss::Kind::HardInterval) continue;
            auto penalty = [&](double z) {
                const double d = z - std::clamp(z, row.loss.lower, row.loss.upper);
                return d * d / row.norm;
            };
            difference += penalty(current_.z[r] + c * t) - penalty(current_.z[r]);
        }
        if (difference <= 0 || std::log(rng_.uniform()) < -difference / 0.1) commit(m, t);
        return true;
    }

    // 連続する行の内積を、独立な部分和で計算する
    static double dot(const double* a, const double* b, int n) {
        double s0 = 0, s1 = 0, s2 = 0, s3 = 0;
        int k = 0;
        for (; k + 3 < n; k += 4) {
            s0 += a[k] * b[k];
            s1 += a[k + 1] * b[k + 1];
            s2 += a[k + 2] * b[k + 2];
            s3 += a[k + 3] * b[k + 3];
        }
        double sum = (s0 + s1) + (s2 + s3);
        for (; k < n; ++k)
            sum += a[k] * b[k];
        return sum;
    }

    bool prepare_dense(Budget& budget) {
        if (dense_ready_ && dense_mean_ready_) return true;
        dense_variables_.clear();
        for (int j = 0; j < static_cast<int>(domains_.size()); ++j) {
            const auto& d = domains_[j];
            if (d.kind == Domain::Kind::Fixed) continue;
            if (d.kind != Domain::Kind::Real || std::isfinite(d.lower) || std::isfinite(d.upper))
                return false;
            dense_variables_.push_back(j);
        }
        const int n = static_cast<int>(dense_variables_.size());
        if (n == 0 || n > 256) return false;
        for (const auto& f : factors_)
            if (f.enabled && f.loss.kind != Loss::Kind::Gaussian) return false;
        if (budget.limit >= 0 && budget.limit - budget.runtime.elapsed_us < 50) return false;
        std::vector<int> index(domains_.size(), -1);
        for (int j = 0; j < n; ++j)
            index[dense_variables_[j]] = j;
        if (!dense_ready_) dense_cholesky_.assign(static_cast<size_t>(n) * n, 0);
        dense_mean_.assign(n, 0);
        auto at = [&](int i, int j) -> double& {
            return dense_cholesky_[static_cast<size_t>(i) * n + j];
        };
        // 正規因子の行を集約し、固定変数を定数項へ移す
        int processed = 0;
        for (const auto& f : factors_)
            if (f.enabled) {
                if ((++processed & 15) == 0 && budget.expired()) return false;
                double y = f.loss.target - f.form.offset;
                for (auto [j, a] : f.form.terms)
                    if (index[j] < 0) y -= a * current_.x[j];
                const double w = 1 / (f.loss.scale * f.loss.scale);
                for (auto [j, a] : f.form.terms)
                    if (index[j] >= 0) {
                        dense_mean_[index[j]] += w * a * y;
                        if (!dense_ready_)
                            for (auto [k, b] : f.form.terms)
                                if (index[k] >= 0) at(index[j], index[k]) += w * a * b;
                    }
            }
        // 途中で期限に達した分解は公開せず、既存候補を保つ
        if (!dense_ready_)
            for (int i = 0; i < n; ++i) {
                if ((i & 15) == 0 && budget.expired()) return false;
                for (int j = 0; j <= i; ++j) {
                    double v = at(i, j) - dot(&at(i, 0), &at(j, 0), j);
                    if (i == j) {
                        if (!(v > 0)) return false;
                        at(i, j) = std::sqrt(v);
                    } else
                        at(i, j) = v / at(j, j);
                }
            }
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < i; ++j)
                dense_mean_[i] -= at(i, j) * dense_mean_[j];
            dense_mean_[i] /= at(i, i);
        }
        for (int i = n - 1; i >= 0; --i) {
            for (int j = i + 1; j < n; ++j)
                dense_mean_[i] -= at(j, i) * dense_mean_[j];
            dense_mean_[i] /= at(i, i);
        }
        // 未使用の上三角へLの転置を置き、独立採取時の列アクセスを避ける
        if (!dense_ready_)
            for (int i = 0; i < n; ++i)
                for (int j = i + 1; j < n; ++j)
                    at(i, j) = at(j, i);
        dense_work_.resize(n);
        dense_ready_ = dense_mean_ready_ = true;
        return true;
    }

    bool conjugate_gradient(Budget& budget, int64_t max_iterations, bool initialization) {
        if (domains_.size() <= 256) return false;
        for (const auto& d : domains_)
            if (d.kind != Domain::Kind::Fixed
                && (d.kind != Domain::Kind::Real || std::isfinite(d.lower) || std::isfinite(d.upper)))
                return false;
        for (const auto& f : factors_)
            if (f.enabled && f.loss.kind != Loss::Kind::Gaussian) return false;
        const size_t n = domains_.size();
        std::vector<double> x = current_.x, residual(n, 0), diagonal(n, 0), direction(n), product(n),
                            preconditioned(n);
        // 正規方程式の行列を作らず、疎な行から残差と対角前処理を求める
        int processed = 0;
        for (const auto& f : factors_)
            if (f.enabled) {
                if ((++processed & 1023) == 0 && budget.expired()) return true;
                const double w = 1 / (f.loss.scale * f.loss.scale),
                             error = f.loss.target - f.form.evaluate(x);
                for (auto [j, a] : f.form.terms)
                    if (domains_[j].kind != Domain::Kind::Fixed) {
                        residual[j] += w * a * error;
                        diagonal[j] += w * a * a;
                    }
            }
        for (size_t j = 0; j < n; ++j)
            direction[j] = preconditioned[j] = diagonal[j] > 0 ? residual[j] / diagonal[j] : 0;
        double rz = std::inner_product(residual.begin(), residual.end(), preconditioned.begin(), 0.0);
        const double initial_rz = rz;
        budget.expired(true);
        const int64_t deadline =
            initialization && budget.limit >= 0
                ? budget.runtime.elapsed_us
                      + std::max<int64_t>(0, budget.limit - budget.runtime.elapsed_us) / 3
                : budget.limit;
        for (int64_t iteration = 0; iteration < std::min<int64_t>(128, max_iterations); ++iteration) {
            if (rz <= 1e-20 * std::max(1.0, initial_rz)) {
                budget.runtime.stop_reason = StopReason::Solved;
                break;
            }
            if (budget.expired() || (deadline >= 0 && budget.runtime.elapsed_us >= deadline)) break;
            std::fill(product.begin(), product.end(), 0);
            for (const auto& f : factors_)
                if (f.enabled) {
                    double v = 0;
                    for (auto [j, a] : f.form.terms)
                        v += a * direction[j];
                    v /= f.loss.scale * f.loss.scale;
                    for (auto [j, a] : f.form.terms)
                        if (domains_[j].kind != Domain::Kind::Fixed) product[j] += a * v;
                }
            const double denominator =
                std::inner_product(direction.begin(), direction.end(), product.begin(), 0.0);
            if (!(denominator > 0)) break;
            const double alpha = rz / denominator;
            for (size_t j = 0; j < n; ++j) {
                x[j] += alpha * direction[j];
                residual[j] -= alpha * product[j];
                preconditioned[j] = diagonal[j] > 0 ? residual[j] / diagonal[j] : 0;
            }
            const double next = std::inner_product(
                             residual.begin(), residual.end(), preconditioned.begin(), 0.0),
                         beta = next / rz;
            for (size_t j = 0; j < n; ++j)
                direction[j] = preconditioned[j] + beta * direction[j];
            rz = next;
            ++budget.runtime.steps;
            ++budget.runtime.accepted;
        }
        current_.x = std::move(x);
        refresh(current_);
        dirty_ready_ = false;
        states_same_ = false;
        if (best_.bad || current_.sum < best_.sum) {
            best_ = current_;
            states_same_ = true;
        }
        return true;
    }

    void dense_update(bool sampling) {
        const int n = static_cast<int>(dense_variables_.size());
        double energy = dense_min_energy_;
        if (!sampling)
            std::fill(dense_work_.begin(), dense_work_.end(), 0);
        else {
            for (int i = n - 1; i >= 0; --i) {
                double v = rng_.normal();
                energy += 0.5 * v * v;
                v -= dot(dense_cholesky_.data() + static_cast<size_t>(i) * n + i + 1,
                         dense_work_.data() + i + 1,
                         n - i - 1);
                dense_work_[i] = v / dense_cholesky_[static_cast<size_t>(i) * n + i];
            }
        }
        for (int j = 0; j < n; ++j)
            current_.x[dense_variables_[j]] = dense_mean_[j] + dense_work_[j];
        // 正規モードからの増分は標準正規乱数の二乗和。因子キャッシュはrunの終了前に再評価する。
        if (sampling) {
            current_.sum = energy;
            states_same_ = false;
            dense_stale_ = true;
            return;
        }
        refresh(current_);
        dense_min_energy_ = current_.sum;
        states_same_ = false;
        dirty_ready_ = false;
        if (best_.bad || current_.sum < best_.sum) {
            best_ = current_;
            states_same_ = true;
        }
    }

    template <class SampleHook>
    void collect(const SampleParam& p, SampleHook& hook) {
        const auto& x = current_.x;
        if (p.collect_moments) {
            if (!moments_.count) {
                moments_.mean = x;
                moments_.variance.assign(x.size(), 0);
                m2_.assign(x.size(), 0);
                moments_.count = 1;
            } else {
                ++moments_.count;
                const double inv = 1 / static_cast<double>(moments_.count);
                for (size_t j = 0; j < x.size(); ++j) {
                    const double d = x[j] - moments_.mean[j];
                    moments_.mean[j] += d * inv;
                    m2_[j] += d * (x[j] - moments_.mean[j]);
                    moments_.variance[j] = m2_[j] * inv;
                }
            }
        }
        if (capacity_) {
            if (stored_.size() != static_cast<size_t>(capacity_) * x.size())
                stored_.resize(static_cast<size_t>(capacity_) * x.size());
            const int pos = (stored_head_ + stored_count_) % capacity_;
            std::copy(x.begin(), x.end(), stored_.begin() + static_cast<size_t>(pos) * x.size());
            if (stored_count_ < capacity_)
                ++stored_count_;
            else
                stored_head_ = (stored_head_ + 1) % capacity_;
        }
        if constexpr (!std::is_same_v<SampleHook, std::nullptr_t>) hook(SampleView{x, current_.sum});
    }

    template <bool Sampling, class Param, class SampleHook, class DebugHook>
    Runtime run(const Param& p, SampleHook hook, DebugHook debug) {
        evaluated_move_ = nullptr;
        Runtime rt;
        Budget budget{{std::chrono::steady_clock::now()}, p.time_limit_us, rt};
        assert(p.time_limit_us >= -1 && p.max_steps >= -1 && (p.time_limit_us >= 0 || p.max_steps >= 0));
        assert(p.direction_probability >= 0 && p.direction_probability <= 1 && p.step_scale > 0
               && p.enumeration_limit >= 0);
        // 予算 0 の呼出しでは準備・統計・乱数を変更しない
        auto finish = [&]() {
            if (dense_stale_) {
                refresh(current_);
                dense_stale_ = false;
            }
            if constexpr (Sampling) rt.warming_up = !dense_active_ && warmup_done_ < p.warmup_steps;
            if (!has_solution()) rt.stop_reason = StopReason::NoFeasibleState;
            budget.expired(true);
            if constexpr (!std::is_same_v<DebugHook, std::nullptr_t>) {
                debug(rt);
                budget.expired(true);
            }
            return rt;
        };
        if (p.time_limit_us == 0 || p.max_steps == 0) {
            rt.stop_reason = p.time_limit_us == 0 ? StopReason::TimeLimit : StopReason::StepLimit;
            return finish();
        }
        if constexpr (Sampling) {
            assert(p.warmup_steps >= 0 && p.sample_stride >= 0 && p.sample_capacity >= 0);
            if (!sampling_ || p.method != previous_sample_.method
                || p.auto_directions != previous_sample_.auto_directions
                || p.direction_probability != previous_sample_.direction_probability
                || p.step_scale != previous_sample_.step_scale
                || p.enumeration_limit != previous_sample_.enumeration_limit
                || p.warmup_steps != previous_sample_.warmup_steps
                || p.sample_stride != previous_sample_.sample_stride)
                reset_sampling();
            if (p.collect_moments != previous_sample_.collect_moments) moments_.count = 0;
            if (capacity_ != p.sample_capacity) {
                capacity_ = p.sample_capacity;
                stored_count_ = stored_head_ = 0;
                if (!capacity_) stored_.clear();
            }
            sampling_ = true;
            previous_sample_ = p;
        } else {
            assert(p.temperature_start >= 0 && p.temperature_end >= 0 && p.greedy_probability >= 0
                   && p.greedy_probability <= 1);
            reset_sampling();
        }
        if (p.method == Method::Automatic && (!Sampling || !dense_decided_)) {
            dense_active_ = prepare_dense(budget);
            dense_decided_ = true;
            if (!dense_active_ && has_solution()) {
                if constexpr (Sampling) {
                    if (p.warmup_steps >= 2) {
                        const int64_t cap =
                            std::min<int64_t>(p.warmup_steps / 2, p.max_steps < 0 ? 128 : p.max_steps);
                        conjugate_gradient(budget, cap, true);
                        warmup_done_ += rt.steps;
                        rt.stop_reason = StopReason::StepLimit;
                    }
                } else if (conjugate_gradient(budget, p.max_steps < 0 ? 128 : p.max_steps, false)) {
                    if (budget.expired()) rt.stop_reason = StopReason::TimeLimit;
                    return finish();
                }
            }
            if constexpr (Sampling)
                if (dense_active_) dense_update(false);
        }
        if constexpr (!Sampling) {
            if (dense_active_) {
                dense_update(false);
                rt.steps = 1;
                rt.accepted = 1;
                rt.stop_reason = StopReason::Solved;
                return finish();
            }
        }
        if (budget.expired()) {
            rt.stop_reason = StopReason::TimeLimit;
            return finish();
        }
        if (dense_active_)
            single_count_ = static_cast<int>(dense_variables_.size());
        else if (!prepare(p.auto_directions, budget)) {
            rt.stop_reason = StopReason::TimeLimit;
            return finish();
        }
        if (!single_count_) {
            rt.stop_reason = StopReason::NoMovableVariable;
            return finish();
        }
        const int check_interval =
            dense_active_ ? 1 : (p.time_limit_us >= 0 && p.time_limit_us <= 500 ? 8 : 64);
        int since_check = check_interval;
        double annealing_temperature = 1;
        // 呼出しを分割しても、採取時の更新規則・乱数・間隔を継続する
        while (p.max_steps < 0 || rt.steps < p.max_steps) {
            if (since_check >= check_interval) {
                if (budget.expired()) {
                    rt.stop_reason = StopReason::TimeLimit;
                    break;
                }
                since_check = 0;
                if constexpr (!Sampling) {
                    const double fraction =
                        p.max_steps > 0
                            ? static_cast<double>(rt.steps) / static_cast<double>(p.max_steps)
                            : static_cast<double>(rt.elapsed_us) / static_cast<double>(p.time_limit_us);
                    annealing_temperature =
                        p.temperature_start > 0 && p.temperature_end > 0
                            ? p.temperature_start
                                  * std::pow(p.temperature_end / p.temperature_start, fraction)
                            : 0;
                }
                if constexpr (!std::is_same_v<DebugHook, std::nullptr_t>) debug(rt);
            }
            if (!has_solution()) {
                if (!repair_step()) {
                    rt.stop_reason = StopReason::NoFeasibleState;
                    break;
                }
                ++rt.steps;
                ++rt.repair_steps;
                ++since_check;
                continue;
            }
            if constexpr (Sampling) {
                if (dense_active_) {
                    dense_update(true);
                    ++rt.steps;
                    ++rt.accepted;
                    ++since_check;
                    // 独立な正規生成は warmup 不要。明示した採取間隔は尊重する
                    const int stride = p.sample_stride ? p.sample_stride : 1;
                    if (++stride_done_ >= stride) {
                        stride_done_ = 0;
                        collect(p, hook);
                        ++rt.samples;
                    }
                    continue;
                }
            }
            // Automaticは座標を一巡する。scan_index_を保持して分割呼出しでも継続する。
            int id = p.method == Method::Automatic ? scan_index_++ : rng_.index(single_count_);
            if (scan_index_ == single_count_) scan_index_ = 0;
            if (static_cast<int>(moves_.size()) > single_count_ && rng_.uniform() < p.direction_probability)
                id = single_count_ + rng_.index(static_cast<int>(moves_.size()) - single_count_);
            if constexpr (Sampling) {
                if (p.method == Method::Automatic
                    && warmup_done_ < std::min<int64_t>(p.warmup_steps / 2, single_count_))
                    id = static_cast<int>(warmup_done_);
            }
            auto& m = moves_[id];
            double temperature = 1;
            if constexpr (Sampling) {
                if (p.method == Method::Automatic && warmup_done_ < p.warmup_steps / 2) temperature = 0;
            }
            if constexpr (!Sampling) {
                temperature = annealing_temperature;
                if (rng_.uniform() < p.greedy_probability) temperature = 0;
            }
            bool aborted = false;
            Method method = p.method;
            double multiplier = p.step_scale;
            if constexpr (Sampling) {
                if (method == Method::Automatic && temperature > 0 && m.nonquadratic && !m.integral
                    && m.finite_variable < 0) {
                    method = m.has_reference && !m.nonconvex && m.h >= 0.25 * m.approximate_h
                                 ? Method::Conditional
                                 : Method::Slice;
                    if (m.nonconvex && rng_.uniform() < 0.05) {
                        method = Method::Metropolis;
                        multiplier *= 8;
                    }
                }
            }
            const bool block = p.method == Method::Automatic && p.auto_directions && m.finite_partner >= 0
                               && rng_.uniform() < p.direction_probability;
            const bool accepted =
                block ? finite_block(m.finite_variable, m.finite_partner, p.enumeration_limit, temperature)
                      : update(m, method, p.enumeration_limit, multiplier, temperature, budget, aborted);
            if (aborted) {
                rt.stop_reason = StopReason::TimeLimit;
                break;
            }
            ++rt.steps;
            ++since_check;
            rt.accepted += accepted;
            if constexpr (Sampling) {
                if (warmup_done_ < p.warmup_steps) {
                    ++warmup_done_;
                    if (temperature > 0
                        && (method == Method::Metropolis
                            || (method == Method::Automatic && m.h < 0.25 * m.approximate_h)
                            || (m.h == 0 && method != Method::Slice))) {
                        const double eta = std::min(0.1, 1 / std::sqrt(static_cast<double>(++m.adaptation)));
                        m.scale = std::clamp(
                            m.scale * std::exp(eta * (static_cast<double>(accepted) - 0.44)), 1e-6, 1e6);
                        if (id < single_count_) scales_[m.terms[0].variable] = m.scale;
                    }
                } else {
                    const int stride = p.sample_stride ? p.sample_stride : single_count_;
                    if (++stride_done_ >= stride) {
                        stride_done_ = 0;
                        collect(p, hook);
                        ++rt.samples;
                    }
                }
                rt.warming_up = warmup_done_ < p.warmup_steps;
            }
        }
        return finish();
    }
};
#if __INCLUDE_LEVEL__ == 0
#include <sys/wait.h>
#include <unistd.h>

struct LinearEstimatorTests {
    using LE = LinearEstimator<>;
    static inline int checks = 0;

    static void check(bool ok, const char* label) {
        ++checks;
        if (!ok) {
            std::cerr << "FAIL: " << label << '\n';
            std::abort();
        }
    }

    static void near(double x, double y, double tolerance, const char* label) {
        if (!(std::abs(x - y) <= tolerance))
            std::cerr << std::setprecision(17) << "actual=" << x << " expected=" << y
                      << " tolerance=" << tolerance << '\n';
        check(std::abs(x - y) <= tolerance, label);
    }

    static LE::SampleParam param(int64_t steps = 200'000) {
        LE::SampleParam p;
        p.time_limit_us = -1;
        p.max_steps = steps;
        p.warmup_steps = 1000;
        p.sample_stride = 1;
        p.auto_directions = false;
        return p;
    }

    static void basics() {
        check(LE::Domain::real(-1, 2).contains(0.5), "real domain");
        check(!LE::Domain::integer(-2, 2).contains(0.5), "integer domain");
        auto d = LE::Domain::values({3, 1, 3, -0.5});
        check(d.candidates.size() == 3, "dedup domain");
        check(d.contains(-0.5) && !d.contains(2), "finite domain");
        check(LE::Domain::fixed(4).contains(4), "fixed domain");
        std::vector<int> a{0, 1, 1}, b{1, 2};
        std::vector<double> x{2, 3, 7};
        near(LE::LinearForm::sum(a).evaluate(x), 8, 0, "sum");
        near(LE::LinearForm::difference(a, b).evaluate(x), -2, 0, "difference");
        near(LE::Loss::gaussian(1, 2).evaluate(3), .5, 0, "gaussian");
        near(LE::Loss::laplace(1, 2).evaluate(-3), 2, 0, "laplace");
        near(LE::Loss::student_t(1, 2, 3).evaluate(3), 2 * std::log(4.0 / 3), 1e-15, "student");
        near(LE::Loss::hard_interval(1, 3).evaluate(1), 0, 0, "hard endpoint");
        check(std::isinf(LE::Loss::hard_interval(1, 3).evaluate(0)), "hard outside");
        near(LE::Loss::soft_interval(1, 3, 2).evaluate(5), .5, 0, "soft interval");
        near(LE::Loss::gaussian_interval(-INFINITY, 0, 1).evaluate(0),
             std::log(2.0),
             1e-15,
             "half normal likelihood");
        near(LE::Loss::gaussian_interval(-INFINITY, INFINITY, 1).evaluate(0), 0, 0, "full interval");
        near(LE::Loss::gaussian_interval(40, INFINITY, 1).evaluate(0),
             804.6084420137539,
             2e-12,
             "tail log probability");
        near(LE::Loss::gaussian_interval(0, 1e-8, 1).evaluate(0),
             19.33961927715704,
             2e-13,
             "tiny central interval");
    }

    static void mutation() {
        LE e({LE::Domain::integer(-10, 10), LE::Domain::integer(-10, 10)}, 7);
        const int a = e.add_factor({{{0, 1}, {0, 2}, {1, -1}, {1, 1}}, 2}, LE::Loss::gaussian(5, 2));
        std::vector<double> x{1, 3};
        e.set_state(x);
        near(e.best_energy(), 0, 0, "duplicate terms");
        e.set_loss(a, LE::Loss::gaussian(7, 2));
        near(e.best_energy(), .5, 0, "immediate loss");
        e.set_enabled(a, false);
        near(e.best_energy(), 0, 0, "disable");
        e.set_enabled(a, true);
        near(e.best_energy(), .5, 0, "enable");
        e.replace_factor(a, {{{1, 1}}, -1}, LE::Loss::gaussian(0, 2));
        near(e.best_energy(), .5, 0, "replace");
        e.set_domain(1, LE::Domain::fixed(5));
        near(e.best()[1], 5, 0, "fix projects");
        near(e.best_energy(), e.evaluate(e.best()), 1e-12, "fixed evaluated");
        e.set_domain(1, LE::Domain::integer(-10, 10));
        auto p = param(1000);
        p.sample_capacity = 5;
        e.sample(p);
        check(e.moments().count > 0 || p.max_steps == p.warmup_steps, "warmup no sample");
        p.max_steps = 100;
        e.sample(p);
        check(e.moments().count == 100, "continued samples");
        e.set_loss(a, LE::Loss::laplace(2, 2));
        check(e.moments().count == 0 && e.stored_sample_count() == 0, "reset model stats");
        auto before = std::vector<double>(e.current().begin(), e.current().end());
        p.max_steps = 0;
        e.sample(p);
        check(before == std::vector<double>(e.current().begin(), e.current().end()), "zero steps state");
        p.max_steps = 100;
        p.time_limit_us = 0;
        e.sample(p);
        check(e.moments().count == 0, "zero time statistics");
    }

    static void gaussian() {
        for (auto method : {LE::Method::Metropolis, LE::Method::Conditional, LE::Method::Slice}) {
            LE e({LE::Domain::real(), LE::Domain::real()}, 5);
            e.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(1, 1));
            e.add_factor({{{1, 1}}, 0}, LE::Loss::gaussian(-2, 2));
            e.add_factor({{{0, 1}, {1, 1}}, 0}, LE::Loss::gaussian(0, 1));
            auto p = param(350'000);
            p.method = method;
            p.sample_capacity = 1000;
            e.sample(p);
            near(e.moments().mean[0], 7.0 / 6, .035, "correlated normal mean0");
            near(e.moments().mean[1], -4.0 / 3, .05, "correlated normal mean1");
            near(e.moments().variance[0], 5.0 / 6, .07, "correlated normal variance0");
            near(e.moments().variance[1], 4.0 / 3, .1, "correlated normal variance1");
            near(e.best_energy(), e.evaluate(e.best()), 1e-7, "cached best normal");
        }
        LE e({LE::Domain::real(-1, 2)}, 9);
        e.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(0, 1));
        auto p = param(300'000);
        e.sample(p);
        near(e.moments().mean[0], .22963717909132902, .01, "truncated mean");
        near(e.moments().variance[0], .5197625392115339, .01, "truncated variance");
        LE tail({LE::Domain::real(8, 9)}, 11);
        tail.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(0, 1));
        tail.sample(p);
        near(tail.moments().mean[0], 8.121188992979869, .002, "tail sampler");
    }

    static void discrete() {
        for (bool wide : {false, true}) {
            LE e({LE::Domain::integer(-60, 60)}, 23);
            e.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(.31, 2.1));
            auto p = param(300'000);
            p.enumeration_limit = wide ? 0 : 200;
            e.sample(p);
            double mass = 0, mean = 0, second = 0;
            for (int x = -60; x <= 60; ++x) {
                const double w = std::exp(-.5 * std::pow((x - .31) / 2.1, 2));
                mass += w;
                mean += w * x;
                second += w * x * x;
            }
            near(e.moments().mean[0], mean / mass, .025, "integer gaussian mean");
            near(e.moments().variance[0],
                 second / mass - std::pow(mean / mass, 2),
                 .065,
                 "integer gaussian variance");
        }
        LE e({LE::Domain::values({-1, .3, 2}), LE::Domain::integer(-2, 2)}, 17);
        e.add_factor({{{0, 1}, {1, 1}}, 0}, LE::Loss::gaussian(.7, 1));
        e.add_factor({{{1, 1}}, 0}, LE::Loss::laplace(.1, 1.5));
        auto p = param(500'000);
        e.sample(p);
        double z = 0, a = 0, b = 0;
        for (double x : {-1.0, .3, 2.0})
            for (int y = -2; y <= 2; ++y) {
                const std::array<double, 2> v{x, static_cast<double>(y)};
                const double w = std::exp(-e.evaluate(v));
                z += w;
                a += w * x;
                b += w * y;
            }
        near(e.moments().mean[0], a / z, .025, "finite candidates posterior");
        near(e.moments().mean[1], b / z, .025, "mixed discrete posterior");
        check(LE::Domain::values({-1, .3, 2}).contains(e.current()[0]), "candidate value remains exact");
    }

    static void generic() {
        for (auto loss : {LE::Loss::laplace(2, .8), LE::Loss::student_t(2, .8, 5)}) {
            LE e({LE::Domain::real()}, 45);
            e.add_factor({{{0, 1}}, 0}, loss);
            auto p = param(250'000);
            p.method = LE::Method::Slice;
            e.sample(p);
            near(e.moments().mean[0], 2, .035, "slice nonquadratic mean");
            const double variance = loss.kind == LE::Loss::Kind::Laplace ? 1.28 : .64 * 5 / 3;
            near(e.moments().variance[0], variance, .09, "slice nonquadratic variance");
        }

        struct Custom {
            std::shared_ptr<double> target;

            double operator()(int, double z) const { return .5 * (z - *target) * (z - *target); }
        };

        auto target = std::make_shared<double>(1);
        using C = LinearEstimator<Custom>;
        C e({C::Domain::real()}, 0, {target});
        int id = e.add_factor({{{0, 1}}, 0}, C::Loss::custom(0));
        near(e.best_energy(), .5, 0, "custom initial");
        *target = 3;
        e.set_loss(id, C::Loss::custom(0));
        near(e.best_energy(), 4.5, 0, "custom external notification");
        C::SampleParam p;
        p.method = C::Method::Slice;
        p.time_limit_us = -1;
        p.max_steps = 100'000;
        p.sample_stride = 1;
        e.sample(p);
        near(e.moments().mean[0], 3, .04, "custom sampled");
    }

    static void constraints() {
        LE e({LE::Domain::integer(0, 10), LE::Domain::integer(0, 10)}, 101);
        e.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(3, 3));
        e.add_factor({{{1, 1}}, 0}, LE::Loss::gaussian(5, 3));
        e.add_factor({{{0, 1}, {1, 1}}, 0}, LE::Loss::hard_interval(7, 7));
        e.set_directions({{{0, 1}, {1, -1}}});
        check(!e.has_solution(), "infeasible hidden");
        auto p = param(150'000);
        p.direction_probability = .5;
        p.sample_capacity = 64;
        int count = 0;
        auto rt = e.sample(p, [&](const LE::SampleView& s) {
            check(s.values[0] + s.values[1] == 7, "equality samples");
            ++count;
        });
        check(e.has_solution() && rt.repair_steps > 0 && count > 0, "repair success");
        std::vector<int> both{0, 1};
        auto prediction = e.predict(LE::LinearForm::sum(both));
        near(prediction.mean, 7, 0, "prediction sum mean");
        near(prediction.variance, 0, 0, "prediction covariance");
        double z = 0, mean = 0;
        for (int x = 0; x <= 7; ++x) {
            std::array<double, 2> v{static_cast<double>(x), 7.0 - x};
            const double w = std::exp(-e.evaluate(v));
            z += w;
            mean += w * x;
        }
        near(e.moments().mean[0], mean / z, .035, "integer equality posterior");
        e.add_factor({{}, 0}, LE::Loss::hard_interval(1, 2));
        rt = e.sample(p);
        check(!e.has_solution() && rt.stop_reason == LE::StopReason::NoFeasibleState,
              "impossible constant status");
        LE f({LE::Domain::fixed(2)});
        auto r = f.sample(param(10));
        check(r.stop_reason == LE::StopReason::NoMovableVariable && f.has_solution(), "all fixed");
    }

    static void continuation() {
        auto make = [] {
            LE e({LE::Domain::real(), LE::Domain::real()}, 9);
            e.add_factor({{{0, 1}, {1, 1}}, 0}, LE::Loss::gaussian(2, .5));
            e.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(0, 2));
            e.add_factor({{{1, 1}}, 0}, LE::Loss::gaussian(0, 2));
            return e;
        };
        auto a = make(), b = make();
        auto p = param(10000);
        p.sample_stride = 3;
        p.auto_directions = true;
        p.sample_capacity = 17;
        a.sample(p);
        p.max_steps = 100;
        for (int i = 0; i < 100; ++i)
            b.sample(p);
        check(std::equal(a.current().begin(), a.current().end(), b.current().begin()), "split chain");
        check(a.moments().mean == b.moments().mean && a.moments().variance == b.moments().variance,
              "split moments");
        check(a.moments().count == b.moments().count, "split counts");
        for (int i = 0; i < a.stored_sample_count(); ++i)
            check(
                std::equal(a.stored_sample(i).begin(), a.stored_sample(i).end(), b.stored_sample(i).begin()),
                "split ring");
        auto c = make(), d = make();
        p.max_steps = 10000;
        p.sample_capacity = 0;
        c.sample(p);
        p.sample_capacity = 100;
        d.sample(p);
        check(std::equal(c.current().begin(), c.current().end(), d.current().begin()),
              "buffer no rng changes");
        p.sample_capacity = 1;
        p.max_steps = 3;
        d.sample(p);
        check(d.stored_sample_count() == 1, "capacity one");
        const int64_t before = d.moments().count;
        p.collect_moments = false;
        d.sample(p);
        check(d.moments().count == 0 && before > 0, "disable moments");
        check(c.predict({{{0, 1}}, 0}).count == 0, "empty prediction");
    }

    static void random_mutations() {
        std::mt19937 rng(31);
        std::vector<LE::Domain> domains(7, LE::Domain::integer(-5, 5));
        LE e(domains, 9);

        struct Row {
            LE::LinearForm f;
            LE::Loss l;
            bool on;
        };

        std::vector<Row> rows;
        auto naive = [&](std::span<const double> x) {
            double sum = 0;
            for (const auto& r : rows)
                if (r.on) sum += r.l.evaluate(r.f.evaluate(x));
            return sum;
        };
        LE::OptimizeParam p;
        p.time_limit_us = -1;
        p.max_steps = 37;
        for (int k = 0; k < 1200; ++k) {
            const int action = static_cast<int>(rng() % 5);
            LE::LinearForm form;
            form.offset = static_cast<int>(rng() % 3) - 1;
            for (int t = 0; t < 4; ++t)
                form.terms.push_back(
                    {static_cast<int>(rng() % 7), static_cast<double>(static_cast<int>(rng() % 5) - 2)});
            auto loss =
                LE::Loss::gaussian(static_cast<int>(rng() % 15) - 7, static_cast<double>(1 + rng() % 4));
            if (rows.empty() || (action == 0 && rows.size() < 80)) {
                rows.push_back({form, loss, true});
                e.add_factor(form, loss);
            } else {
                int id = static_cast<int>(rng() % rows.size());
                if (action == 1) {
                    rows[id].l = loss;
                    e.set_loss(id, loss);
                }
                if (action == 2) {
                    rows[id].on = !rows[id].on;
                    e.set_enabled(id, rows[id].on);
                }
                if (action == 3) {
                    rows[id].f = form;
                    rows[id].l = loss;
                    e.replace_factor(id, form, loss);
                }
                if (action == 4) {
                    std::vector<double> x(7);
                    for (auto& v : x)
                        v = static_cast<int>(rng() % 11) - 5;
                    e.set_state(x);
                }
            }
            near(e.best_energy(), naive(e.best()), 1e-7, "mutation naive best");
            e.optimize(p);
            near(e.best_energy(), naive(e.best()), 1e-7, "delta best");
            near(e.evaluate(e.current()), naive(e.current()), 1e-12, "naive current");
        }
    }

    static void extra_paths() {
        for (double sigma : {.01, 1e6}) {
            LE e({LE::Domain::integer(-100, 100)}, 19);
            e.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(.5, sigma));
            auto p = param(150'000);
            p.method = LE::Method::Conditional;
            p.enumeration_limit = 0;
            e.sample(p);
            near(e.moments().mean[0],
                 sigma < 1 ? .5 : 0,
                 sigma < 1 ? .01 : .5,
                 "sharp or flat integer normal");
            near(e.moments().variance[0],
                 sigma < 1 ? .25 : 10100.0 / 3,
                 sigma < 1 ? .005 : 30,
                 "integer envelope variance");
        }
        LE strong({LE::Domain::integer(-100, 100), LE::Domain::integer(-100, 100)}, 91);
        strong.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(0, 4));
        strong.add_factor({{{1, 1}}, 0}, LE::Loss::gaussian(0, 4));
        strong.add_factor({{{0, 1}, {1, 1}}, 0}, LE::Loss::gaussian(0, .01));
        auto p = param(150'000);
        p.auto_directions = true;
        strong.sample(p);
        near(strong.moments().mean[0], 0, .1, "automatic correlated integer direction mean");
        near(strong.moments().variance[0], 8, .25, "automatic correlated integer direction variance");
        LE e({LE::Domain::real(), LE::Domain::fixed(3)}, 1);
        e.add_factor({{{0, 1}, {1, 2}}, 1}, LE::Loss::gaussian(9, 2));
        LE::OptimizeParam op;
        op.time_limit_us = -1;
        op.max_steps = 100;
        auto rt = e.optimize(op);
        check(rt.stop_reason == LE::StopReason::Solved, "dense exact optimize");
        near(e.best()[0], 2, 1e-12, "dense fixed contribution");
        auto sp = param(100'000);
        e.sample(sp);
        near(e.moments().mean[0], 2, .03, "dense sampling mean");
        near(e.moments().variance[0], 4, .08, "dense sampling variance");
        e.set_loss(0, LE::Loss::gaussian(10, 2));
        e.optimize(op);
        near(e.best()[0], 3, 1e-12, "dense invalidate rhs");
        e.set_domain(0, LE::Domain::real(0, 4));
        e.sample(sp);
        check(e.current()[0] >= 0 && e.current()[0] <= 4, "dense fallback bounds");
        LE uniform({LE::Domain::real(-3, 3)});
        uniform.sample(sp);
        near(uniform.moments().variance[0], 3, .06, "uniform conditional");
        // 真の事後分布を指定した初期状態から検証し、初期化の近似を混ぜない
        LE large(std::vector<LE::Domain>(300, LE::Domain::real()), 5);
        std::vector<int> all(300);
        std::iota(all.begin(), all.end(), 0);
        double sum = 0;
        for (int j = 0; j < 300; ++j) {
            sum += j % 7;
            large.add_factor({{{j, 1}}, 0}, LE::Loss::gaussian(j % 7, 1));
        }
        large.add_factor(LE::LinearForm::sum(all), LE::Loss::gaussian(0, 2));
        op.time_limit_us = -1;
        op.max_steps = 100;
        large.optimize(op);
        for (int j = 0; j < 300; ++j)
            near(large.best()[j], j % 7 - sum / 304, 1e-9, "matrix free conjugate gradient");
        near(large.best_energy(), large.evaluate(large.best()), 1e-8, "cg cache");
        sp.max_steps = 1000;
        large.sample(sp);
        near(large.best_energy(), large.evaluate(large.best()), 1e-8, "cg sampling initialization");
        LE hooked({LE::Domain::integer(-2, 2)}, 2);
        hooked.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(0, 1));
        auto hp = param(64);
        hp.warmup_steps = 0;
        hp.sample_capacity = 4;
        int hook_samples = 0, debug_calls = 0;
        auto hr = hooked.sample(
            hp,
            [&](const LE::SampleView& view) {
                near(view.energy, hooked.evaluate(view.values), 1e-12, "sample hook evaluated state");
                ++hook_samples;
            },
            [&](const LE::Runtime& runtime) {
                check(runtime.steps >= 0 && runtime.elapsed_us >= 0, "debug runtime");
                ++debug_calls;
            });
        check(hook_samples == hr.samples && debug_calls >= 2, "hook counts");
        const int64_t kept_count = hooked.moments().count;
        hooked.set_enabled(0, true);
        check(hooked.moments().count == kept_count, "idempotent enabled");
        hooked.set_state(std::array<double, 1>{1});
        check(hooked.moments().count == 0, "set state resets statistics");
        op.time_limit_us = -1;
        op.max_steps = 10;
        hooked.optimize(op, [&](const LE::Runtime&) { ++debug_calls; });
        LE changed_direction({LE::Domain::real()}, 41);
        changed_direction.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(2, 1));
        changed_direction.set_directions({{{0, 2}}});
        changed_direction.set_domain(0, LE::Domain::values({.3, 2, 7}));
        auto dp = param(30'000);
        changed_direction.sample(dp);
        double mass = 0, expected = 0;
        for (double v : {.3, 2.0, 7.0}) {
            const double w = std::exp(-.5 * (v - 2) * (v - 2));
            mass += w;
            expected += w * v;
        }
        near(changed_direction.moments().mean[0],
             expected / mass,
             .025,
             "direction invalidated by finite domain");
        LE boundary({LE::Domain::values({.3, 2.0})}, 9);
        boundary.add_factor({{{0, 1}}, 0}, LE::Loss::hard_interval(-INFINITY, .3));
        boundary.set_state(std::array<double, 1>{2.0});
        auto bp = param(100);
        bp.warmup_steps = 0;
        boundary.sample(bp);
        check(boundary.has_solution(), "decimal candidate boundary repair");
        near(boundary.current()[0], .3, 0, "decimal candidate exact value");
        near(boundary.evaluate(boundary.current()), 0, 0, "decimal candidate exact cached support");
        LE box({LE::Domain::real(-1, .3)}, 0);
        box.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(5, 1));
        box.set_state(std::array<double, 1>{-1});
        LE::OptimizeParam box_param;
        box_param.method = LE::Method::Conditional;
        box_param.time_limit_us = -1;
        box_param.max_steps = 1;
        box_param.temperature_start = box_param.temperature_end = 0;
        box.optimize(box_param);
        check(box.has_solution() && std::isfinite(box.evaluate(box.current())),
              "decimal real bound feasible");
        near(box.current()[0], .3, 0, "decimal real bound optimum");
        near(box.best_energy(), box.evaluate(box.best()), 1e-12, "decimal real bound cache");
        LE zero_budget({LE::Domain::real()});
        op.time_limit_us = 0;
        op.max_steps = 0;
        rt = zero_budget.optimize(op);
        check(rt.steps == 0, "zero budget no preparation");
    }

    static void multimodal() {
        struct Mixture {
            double operator()(int, double z) const {
                const double a = -2 * (z - 3) * (z - 3), b = -2 * (z + 3) * (z + 3), m = std::max(a, b);
                return -m - std::log(.5 * std::exp(a - m) + .5 * std::exp(b - m));
            }
        };

        using M = LinearEstimator<Mixture>;
        M e({M::Domain::real(-8, 8)}, 209333, {});
        e.add_factor({{{0, 1}}, 0}, M::Loss::custom(0));
        e.set_state(std::array<double, 1>{3});
        M::SampleParam p;
        p.time_limit_us = -1;
        p.max_steps = 200'000;
        p.warmup_steps = 1000;
        p.sample_stride = 1;
        e.sample(p);
        near(e.moments().mean[0], 0, .5, "mixture crosses modes");
        near(e.moments().variance[0], 9.25, .4, "mixture variance");
    }

    static void asserts() {
        auto must_abort = [](auto function) {
            const pid_t pid = fork();
            check(pid >= 0, "fork");
            if (pid == 0) {
                if (!std::freopen("/dev/null", "w", stderr)) _exit(2);
                function();
                _exit(0);
            }
            int status = 0;
            waitpid(pid, &status, 0);
            check(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT, "assert misuse");
        };
#ifndef NDEBUG
        must_abort([] {
            LE e({LE::Domain::integer(0, 3)});
            e.set_state(std::array<double, 1>{.5});
        });
        must_abort([] {
            LE e({LE::Domain::real()});
            LE::SampleParam p;
            p.max_steps = p.time_limit_us = -1;
            e.sample(p);
        });
        must_abort([] {
            LE e({LE::Domain::real()});
            (void)e.stored_sample(0);
        });
        must_abort([] { (void)LE::Loss::gaussian(0, 0); });
#endif
        (void)must_abort;
    }

    static int run() {
        basics();
        mutation();
        gaussian();
        discrete();
        generic();
        constraints();
        continuation();
        random_mutations();
        extra_paths();
        multimodal();
        asserts();
        std::cout << "PASS " << checks << " checks\n";
        return 0;
    }
};

int main() {
    return LinearEstimatorTests::run();
}
#endif
