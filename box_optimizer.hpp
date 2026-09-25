#pragma once
#include <bits/stdc++.h>

// 箱制約付きの決定的な非線形最小化、C++20、標準ライブラリのみ
// 勾配ありは投影L-BFGS型（スペクトル勾配も選択可）、勾配なしは座標・焼きなまし・対角CMA系
// 有限な上下限・目的値・勾配を要求し、幅・逆数・内積は通常のdoubleで表現できる範囲に限る
// n<=100000、自由変数d<=10000、solveからの累計評価<=10億を想定し、不正入力はassertで扱う
// resumeは目的関数とその捕捉データが不変の場合のみ使用する、同じオブジェクトへの再入は不可
struct BoxBound { double lower, upper; };
struct BoxProblem { std::vector<BoxBound> bounds; };
struct BoxBudget {
    // 負値で時間制限なし、0で新しい評価なし、負値なら有限の評価上限が必要
    double time_limit_us = 1'950'000;
    long long max_evaluations = LLONG_MAX;
    std::optional<double> target_cost;
};
struct BoxParam {
    enum class Search { Explore, Refine };
    // SpectralGradientは二引数の目的関数を要求する（全固定の場合を除く）
    enum class Method { Auto, Coordinate, DiagonalCma, SpectralGradient };
    Search search = Search::Explore;
    uint64_t seed = 1;
    // 空なら自動、それ以外は元の単位で長さn、固定成分は無視
    std::vector<double> initial_step;
    Method method = Method::Auto;
};
struct BoxResult {
    enum class Stop { TargetReached, TimeLimit, EvaluationLimit, LocalStop, AllFixed };
    std::vector<double> x;
    double cost = std::numeric_limits<double>::infinity();
    long long evaluations = 0, gradient_evaluations = 0, total_evaluations = 0;
    double elapsed_us = 0;
    Stop stop = Stop::EvaluationLimit;
    // 評価済み解が存在するか返す O(1)
    bool has_value() const { return std::isfinite(cost); }
};
class BoxOptimizer {
#if __INCLUDE_LEVEL__ == 0
    friend struct BoxOptimizerTest;
#endif
    using Vec = std::vector<double>;
    enum class Method { Coordinate, Gradient, DiagonalCma };
    static constexpr int history_size = 8;
    using Clock = std::chrono::steady_clock;
    struct Random {
        uint64_t state = 1;
        bool spare_valid = false;
        double spare = 0;
        uint64_t next() {
            uint64_t x = (state += 0x9e3779b97f4a7c15ULL);
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
            return x ^ (x >> 31);
        }
        double uniform() { return static_cast<double>(next() >> 11) * 0x1.0p-53; }
        int index(int size) { return static_cast<int>(next() % static_cast<uint64_t>(size)); }
        double normal() {
            if (spare_valid) { spare_valid = false; return spare; }
            double u, v, s;
            do { u = 2 * uniform() - 1; v = 2 * uniform() - 1; s = u*u + v*v; }
            while (s == 0 || s >= 1);
            s = std::sqrt(-2 * std::log(s) / s);
            spare = v * s; spare_valid = true;
            return u * s;
        }
    } rng_;
    struct Coordinate {
        Vec step, direction;
        int axis = 0, side = 0, previous = 0;
        int quiet = 0;
        double sweep_cost = 0;
    } cd_;
    struct Gradient {
        Vec gradient, direction, s, y, rho, alpha;
        int count = 0, head = 0, backtracks = 0;
        double scale = 1, rate = 1, dot = 0;
        bool direction_ready = false;
    } gd_;
    struct Cma {
        Vec mean, covariance, axes, path_c, path_s, weights;
        Vec points, costs, delta;
        std::vector<int> order;
        int lambda = 0, mu = 0, child = 0, generation = 0, stale = 0;
        double sigma = 0.25, mueff = 0, cc = 0, cs = 0, c1 = 0, cmu = 0, damping = 0, chi = 0;
        double generation_best = 0;
    } cm_;
    BoxProblem problem_;
    BoxParam param_;
    std::vector<int> free_;
    Vec widths_, steps_, x_, best_x_, best_z_, current_, trial_, gradient_x_, gradient_z_;
    double best_cost_ = std::numeric_limits<double>::infinity(), current_cost_ = 0;
    long long total_ = 0, run_evals_ = 0, run_gradients_ = 0;
    int n_ = 0, d_ = 0, restarts_ = 0, mapped_axis_ = -1;
    bool started_ = false, oracle_gradient_ = false, first_ = true, pending_ = false;
    // ready_がfalseの候補は探索状態の初期化前の点。need_gradient_は探索中不変
    bool ready_ = false, need_gradient_ = false, local_stop_ = false;
    Method method_ = Method::Coordinate;
    double anneal_temperature_ = 0, anneal_decay_ = 1;
    int anneal_left_ = 0, cma_left_ = 0;

    static double dot(const Vec& a, const Vec& b) {
        return std::inner_product(a.begin(), a.end(), b.begin(), 0.0);
    }
    static double max_abs(const Vec& a) {
        double r = 0;
        for (double v : a) r = std::max(r, std::abs(v));
        return r;
    }
    void begin(std::span<const double> initial, bool gradient) {
        assert(initial.empty() || static_cast<int>(initial.size()) == n_);
        assert(param_.method != BoxParam::Method::SpectralGradient || gradient || d_ == 0);
        // 初期点の元の値を保存し、初回は往復変換をしない
        for (int i = 0; i < n_; ++i) {
            auto b = problem_.bounds[i];
            x_[i] = initial.empty() ? b.lower + (b.upper-b.lower)*0.5 : initial[i];
            assert(std::isfinite(x_[i]) && b.lower <= x_[i] && x_[i] <= b.upper);
        }
        for (int j = 0; j < d_; ++j) current_[j] = (x_[free_[j]]-problem_.bounds[free_[j]].lower)/widths_[j];
        best_x_.clear(); best_z_.clear(); best_cost_ = std::numeric_limits<double>::infinity();
        total_ = 0; restarts_ = 0; anneal_left_ = 0; cma_left_ = 0; rng_ = {}; rng_.state = param_.seed;
        started_ = true; oracle_gradient_ = gradient; first_ = true; pending_ = true;
        ready_ = false; local_stop_ = false; mapped_axis_ = -1;
        method_ = param_.method == BoxParam::Method::DiagonalCma ? Method::DiagonalCma
            : (param_.method == BoxParam::Method::Auto || param_.method == BoxParam::Method::SpectralGradient) && gradient ? Method::Gradient : Method::Coordinate;
        need_gradient_ = method_ == Method::Gradient && d_ > 0;
        trial_ = current_;
    }
    void initialize_coordinate() {
        cd_.step = steps_; cd_.direction.assign(d_, 1);
        cd_.axis = 0; cd_.side = 0; cd_.previous = 0; trial_ = current_; cd_.quiet = 0; cd_.sweep_cost = current_cost_;
    }
    void initialize_gradient() {
        // 箱の外へ向く成分を除き、初回の歩幅の尺度を決める
        gd_.gradient = gradient_z_; gd_.count = 0; gd_.head = 0;
        gd_.direction.resize(d_);
        if (param_.method != BoxParam::Method::SpectralGradient) {
            gd_.s.resize(history_size*d_); gd_.y.resize(history_size*d_);
            gd_.rho.resize(history_size); gd_.alpha.resize(history_size);
        }
        gd_.direction_ready = false;
        double norm = 0;
        for (int i = 0; i < d_; ++i)
            if (!((current_[i] == 0 && gd_.gradient[i] > 0) || (current_[i] == 1 && gd_.gradient[i] < 0)))
                norm = std::max(norm,std::abs(gd_.gradient[i]));
        gd_.scale = *std::max_element(steps_.begin(), steps_.end()) / std::max(norm, 1e-300);
    }
    void end_cma_phase() {
        cma_left_ = 0; method_ = Method::Coordinate; ready_ = true;
        current_ = best_z_; current_cost_ = best_cost_; initialize_coordinate();
    }
    void end_local() {
        if (cma_left_) { end_cma_phase(); propose_coordinate(); return; }
        if (param_.search == BoxParam::Search::Refine) { local_stop_ = true; return; }
        if (method_ == Method::Coordinate && param_.method == BoxParam::Method::Auto) {
            current_ = best_z_; current_cost_ = best_cost_; anneal_temperature_ = 0;
            anneal_left_ = 50*d_; anneal_decay_ = std::exp(-8.0/(50*d_)); propose_anneal(); return;
        }
        // 再始動点は未評価の候補として保持する
        ++restarts_; ready_ = false;
        for (int i = 0; i < d_; ++i)
            trial_[i] = restarts_ % 2 ? rng_.uniform() : std::clamp(best_z_[i]+0.15*rng_.normal(), 0.0, 1.0);
        if (method_ == Method::Gradient && restarts_ % 8 != 0) {
            trial_ = best_z_; int i = rng_.index(d_); trial_[i] = rng_.uniform();
        }
        pending_ = true;
    }
    void propose_coordinate() {
        // 同一点になる端点側は評価せず、反対側へ進める
        for (;;) {
            if (cd_.step[cd_.axis] < 1e-10 && max_abs(cd_.step) < 1e-10) { end_local(); return; }
            // 前回の変更軸を受理済みの値へ戻し、次の軸だけ変更する
            trial_[cd_.previous] = current_[cd_.previous];
            int i = cd_.axis; cd_.previous = i;
            trial_[i] = std::clamp(current_[i]+(cd_.side ? -1 : 1)*cd_.direction[i]*cd_.step[i], 0.0, 1.0);
            if (trial_[i] != current_[i]) break;
            consume_coordinate(std::numeric_limits<double>::infinity());
        }
        pending_ = true;
    }
    void consume_coordinate(double f) {
        // 座標ごとに成功方向と幅を保持する
        int i = cd_.axis;
        if (f <= current_cost_) {
            current_[i] = trial_[i]; current_cost_ = f;
            if (cd_.side) cd_.direction[i] = -cd_.direction[i];
            cd_.step[i] = std::min(0.5, cd_.step[i]*1.2);
        } else if (cd_.side == 0) { cd_.side = 1; return; }
        else cd_.step[i] *= 0.5;
        cd_.side = 0;
        if (++cd_.axis == d_) {
            cd_.axis = 0;
            cd_.quiet = current_cost_ < cd_.sweep_cost ? 0 : cd_.quiet+1;
            cd_.sweep_cost = current_cost_;
            if (cd_.quiet >= 4) for (double& v : cd_.step) v *= 0.5;
        }
    }
    void propose_gradient() {
        if (!gd_.direction_ready) {
            // 有効な投影勾配に二重ループを適用。SpectralGradientは履歴数0
            gd_.direction = gd_.gradient;
            for (int i = 0; i < d_; ++i)
                if ((current_[i] == 0 && gd_.direction[i] > 0) || (current_[i] == 1 && gd_.direction[i] < 0)) gd_.direction[i] = 0;
            if (max_abs(gd_.direction) == 0) { end_local(); return; }
            for (int k = 0; k < gd_.count; ++k) {
                int h = (gd_.head-1-k+history_size)%history_size;
                double a = 0;
                for (int i = 0; i < d_; ++i) a += gd_.s[h*d_+i]*gd_.direction[i];
                gd_.alpha[h] = a*gd_.rho[h];
                for (int i = 0; i < d_; ++i) gd_.direction[i] -= gd_.alpha[h]*gd_.y[h*d_+i];
            }
            for (double& v : gd_.direction) v *= gd_.scale;
            for (int k = gd_.count-1; k >= 0; --k) {
                int h = (gd_.head-1-k+history_size)%history_size;
                double b = 0;
                for (int i = 0; i < d_; ++i) b += gd_.y[h*d_+i]*gd_.direction[i];
                b *= gd_.rho[h];
                for (int i = 0; i < d_; ++i) gd_.direction[i] += (gd_.alpha[h]-b)*gd_.s[h*d_+i];
            }
            for (int i = 0; i < d_; ++i) {
                gd_.direction[i] = -gd_.direction[i];
                if ((current_[i] == 0 && gd_.direction[i] < 0) || (current_[i] == 1 && gd_.direction[i] > 0)) gd_.direction[i] = 0;
            }
            if (dot(gd_.direction, gd_.gradient) >= 0)
                for (int i = 0; i < d_; ++i) gd_.direction[i] = std::clamp(current_[i]-gd_.scale*gd_.gradient[i], 0.0, 1.0)-current_[i];
            gd_.rate = 1; gd_.backtracks = 0; gd_.direction_ready = true;
        }
        // 投影後も降下することを確認し、不適切な履歴では投影勾配へ戻す
        double moved = 0;
        for (int attempt = 0; attempt < 2; ++attempt) {
            moved = 0; gd_.dot = 0;
            for (int i = 0; i < d_; ++i) {
                trial_[i] = std::clamp(current_[i]+gd_.rate*gd_.direction[i], 0.0, 1.0);
                moved = std::max(moved, std::abs(trial_[i]-current_[i]));
                gd_.dot += (trial_[i]-current_[i])*gd_.gradient[i];
            }
            if (gd_.dot < 0 || moved < 1e-13) break;
            gd_.count = 0; gd_.rate = 1;
            for (int i = 0; i < d_; ++i)
                gd_.direction[i] = std::clamp(current_[i]-gd_.scale*gd_.gradient[i],0.0,1.0)-current_[i];
        }
        if (moved < 1e-13 || gd_.dot >= 0 || gd_.backtracks >= 40) { end_local(); return; }
        pending_ = true;
    }
    void consume_gradient(double f) {
        if (f <= current_cost_+1e-4*gd_.dot) {
            // 正の曲率を持つ履歴のみ保存する
            double sy = 0, ss = 0, yy = 0;
            for (int i = 0; i < d_; ++i) {
                double s = trial_[i]-current_[i], y = gradient_z_[i]-gd_.gradient[i];
                sy += s*y; ss += s*s; yy += y*y;
            }
            if (sy > 1e-10*std::sqrt(ss*yy)) {
                // BB1、BB1/BB2の幾何平均、従来のBB2を用途に応じて選ぶ
                gd_.scale = param_.method == BoxParam::Method::SpectralGradient ? ss/sy
                    : param_.search == BoxParam::Search::Explore ? std::sqrt(ss/yy) : sy/yy;
                if (param_.method != BoxParam::Method::SpectralGradient) {
                    int h = gd_.head;
                    for (int i = 0; i < d_; ++i) {
                        gd_.s[h*d_+i] = trial_[i]-current_[i]; gd_.y[h*d_+i] = gradient_z_[i]-gd_.gradient[i];
                    }
                    gd_.rho[h] = 1/sy; gd_.head = (h+1)%history_size; gd_.count = std::min(gd_.count+1, history_size);
                }
            }
            current_ = trial_; current_cost_ = f;
            gd_.gradient = gradient_z_; gd_.direction_ready = false;
        } else { gd_.rate *= 0.5; ++gd_.backtracks; }
    }
    void initialize_cma() {
        // 正の重みのみを持つ共分散適応の係数を準備する
        auto& c = cm_;
        c.lambda = 4+static_cast<int>(3*std::log(d_));
        c.mu = c.lambda/2; c.weights.resize(c.mu);
        double sum = 0;
        for (int k = 0; k < c.mu; ++k) sum += c.weights[k] = std::log(c.mu+0.5)-std::log(k+1.0);
        for (double& w : c.weights) w /= sum;
        c.mueff = 1/dot(c.weights,c.weights);
        c.cc = (4+c.mueff/d_)/(d_+4+2*c.mueff/d_);
        c.cs = (c.mueff+2)/(d_+c.mueff+5);
        c.c1 = 2/(std::pow(d_+1.3,2)+c.mueff);
        c.cmu = std::min(1-c.c1, 2*(c.mueff-2+1/c.mueff)/(std::pow(d_+2.0,2)+c.mueff));
        double factor = (d_+1.5)/3;
        c.c1 = std::min(0.5, c.c1*factor); c.cmu = std::min(1-c.c1, c.cmu*factor);
        c.damping = 1+2*std::max(0.0, std::sqrt((c.mueff-1)/(d_+1))-1)+c.cs;
        c.chi = std::sqrt(static_cast<double>(d_))*(1-1.0/(4*d_)+1.0/(21*d_*d_));
        c.mean = current_; c.sigma = 0.25;
        c.covariance.resize(d_); c.axes.resize(d_);
        for (int i = 0; i < d_; ++i) {
            c.axes[i] = steps_[i]/c.sigma;
            c.covariance[i] = c.axes[i]*c.axes[i];
        }
        c.path_c.assign(d_,0); c.path_s.assign(d_,0); c.delta.resize(d_);
        c.points.resize(c.lambda*d_); c.costs.resize(c.lambda); c.order.resize(c.lambda);
        std::iota(c.order.begin(),c.order.end(),0);
        c.child = 0; c.generation = 0; c.stale = 0; c.generation_best = std::numeric_limits<double>::infinity();
    }
    void update_cma() {
        // 評価済みの世代だけを順位付けし、分布と進化パスを更新する
        auto& c = cm_;
        std::sort(c.order.begin(),c.order.end(),[&](int a,int b){ return c.costs[a] != c.costs[b] ? c.costs[a] < c.costs[b] : a < b; });
        std::fill(c.delta.begin(),c.delta.end(),0);
        for (int k = 0; k < c.mu; ++k) for (int i = 0; i < d_; ++i)
            c.delta[i] += c.weights[k]*(c.points[c.order[k]*d_+i]-c.mean[i])/c.sigma;
        for (int i = 0; i < d_; ++i) c.path_s[i] = (1-c.cs)*c.path_s[i]+std::sqrt(c.cs*(2-c.cs)*c.mueff)*(c.delta[i]/c.axes[i]);
        double norm = std::sqrt(dot(c.path_s,c.path_s));
        ++c.generation;
        bool hs = norm/std::sqrt(1-std::pow(1-c.cs,2*c.generation)) < (1.4+2.0/(d_+1))*c.chi;
        for (int i = 0; i < d_; ++i) c.path_c[i] = (1-c.cc)*c.path_c[i]+(hs ? std::sqrt(c.cc*(2-c.cc)*c.mueff)*c.delta[i] : 0);
        double decay = 1-c.c1-c.cmu+(hs ? 0 : c.c1*c.cc*(2-c.cc));
        // 境界修正後の実際の変位を正の重みで学習する
        for (int i = 0; i < d_; ++i) {
            double v = 0;
            for (int k = 0; k < c.mu; ++k) {
                int p = c.order[k]*d_;
                v += c.weights[k]*((c.points[p+i]-c.mean[i])/c.sigma)*((c.points[p+i]-c.mean[i])/c.sigma);
            }
            c.covariance[i] = decay*c.covariance[i]+c.c1*c.path_c[i]*c.path_c[i]+c.cmu*v;
        }
        for (int i = 0; i < d_; ++i) c.mean[i] += c.sigma*c.delta[i];
        c.sigma *= std::exp(std::min(0.6,(c.cs/c.damping)*(norm/c.chi-1)));
        for (int i = 0; i < d_; ++i) c.axes[i] = std::sqrt(c.covariance[i]);
        c.stale = c.costs[c.order[0]] < c.generation_best ? 0 : c.stale+1; c.generation_best = std::min(c.generation_best,c.costs[c.order[0]]);
        c.child = 0;
        if (c.sigma*max_abs(c.axes) < 1e-10 || c.stale > 20+20*d_/c.lambda) end_local();
    }
    void propose_cma() {
        auto& c = cm_;
        if (c.child == c.lambda) update_cma();
        if (pending_ || local_stop_) return;
        for (int i = 0; i < d_; ++i) {
            // 直前の候補を平均点に対して反転し、箱の中へ収める
            double delta = c.child%2 ? -(c.points[(c.child-1)*d_+i]-c.mean[i])
                                     : c.sigma*c.axes[i]*rng_.normal();
            trial_[i] = std::clamp(c.mean[i]+delta,0.0,1.0);
        }
        pending_ = true;
    }
    void propose_anneal() {
        trial_ = current_;
        int i = rng_.index(d_);
        trial_[i] = std::clamp(trial_[i]+steps_[i]*std::pow(0.01,rng_.uniform())*rng_.normal(),0.0,1.0);
        pending_ = true;
    }
    void consume_anneal(double f) {
        // 同一尺度内で温度を推定し、終了後は共分散適応の短い区間へ移る
        double delta = f-current_cost_;
        if (anneal_temperature_ == 0 && delta != 0) anneal_temperature_ = std::abs(delta);
        if (delta <= 0 || rng_.uniform() < std::exp(-delta/std::max(1e-300,anneal_temperature_))) { current_ = trial_; current_cost_ = f; }
        anneal_temperature_ *= anneal_decay_;
        if (--anneal_left_ == 0) {
            current_ = best_z_; current_cost_ = best_cost_;
            method_ = Method::DiagonalCma; ready_ = false; cma_left_ = 100*d_;
        }
    }
    void propose() {
        if (anneal_left_ > 0) { propose_anneal(); return; }
        if (!ready_) {
            if (method_ == Method::Coordinate) initialize_coordinate();
            else if (method_ == Method::Gradient) initialize_gradient();
            else initialize_cma();
            ready_ = true;
        }
        if (method_ == Method::Coordinate) propose_coordinate();
        else if (method_ == Method::Gradient) propose_gradient();
        else propose_cma();
    }
    template<class Objective>
    void evaluate(Objective& objective) {
        // 候補を元の座標へ戻し、全ての評価を同じ窓口で数える
        int axis = ready_ && method_ == Method::Coordinate && anneal_left_ == 0 ? cd_.axis : -1;
        auto map = [&](int j) {
            int i = free_[j]; auto b = problem_.bounds[i];
            x_[i] = trial_[j] == 1 ? b.upper : std::clamp(b.lower+widths_[j]*trial_[j],b.lower,b.upper);
        };
        // 座標探索が続く場合、前回と今回の変更軸以外は同じ値を保つ
        if (!first_) {
            if (axis >= 0 && mapped_axis_ >= 0) {
                map(mapped_axis_);
                if (axis != mapped_axis_) map(axis);
            } else for (int j = 0; j < d_; ++j) map(j);
        }
        mapped_axis_ = axis;
        double f;
        if constexpr (std::is_invocable_r_v<double,Objective&,std::span<const double>,std::span<double>>) {
            if (need_gradient_ && first_) { gradient_x_.resize(n_); gradient_z_.resize(d_); }
            f = objective(std::span<const double>(x_),need_gradient_ ? std::span<double>(gradient_x_) : std::span<double>{});
            if (need_gradient_) {
                ++run_gradients_;
                for (int i = 0; i < n_; ++i) assert(std::isfinite(gradient_x_[i]));
                for (int j = 0; j < d_; ++j) { gradient_z_[j] = widths_[j]*gradient_x_[free_[j]]; assert(std::isfinite(gradient_z_[j])); }
            }
        } else f = objective(std::span<const double>(x_));
        assert(std::isfinite(f)); ++run_evals_; ++total_;
        if (f < best_cost_) { best_cost_ = f; best_x_ = x_; best_z_ = trial_; }
        pending_ = false;
        if (!ready_) { current_ = trial_; current_cost_ = f; first_ = false; return; }
        if (anneal_left_ > 0) consume_anneal(f);
        else if (method_ == Method::Coordinate) consume_coordinate(f);
        else if (method_ == Method::Gradient) consume_gradient(f);
        else {
            std::copy(trial_.begin(),trial_.end(),cm_.points.begin()+cm_.child*d_); cm_.costs[cm_.child++] = f;
            if (cma_left_ && --cma_left_ == 0) end_cma_phase();
        }
    }
    template<class Objective>
    BoxResult run(Objective& objective, const BoxBudget& budget, Clock::time_point start) {
        assert(std::isfinite(budget.time_limit_us) && budget.max_evaluations >= 0);
        assert(budget.time_limit_us >= 0 || budget.max_evaluations < LLONG_MAX);
        assert(!budget.target_cost || std::isfinite(*budget.target_cost));
        run_evals_ = run_gradients_ = 0;
        auto elapsed = [&] { return std::chrono::duration<double,std::micro>(Clock::now()-start).count(); };
        auto stopped = [&](bool check_time = true) -> std::optional<BoxResult::Stop> {
            if (budget.target_cost && best_cost_ <= *budget.target_cost) return BoxResult::Stop::TargetReached;
            if (!first_ && d_ == 0) return BoxResult::Stop::AllFixed;
            if (local_stop_) return BoxResult::Stop::LocalStop;
            if (run_evals_ >= budget.max_evaluations) return BoxResult::Stop::EvaluationLimit;
            if (check_time && budget.time_limit_us >= 0 && elapsed() >= budget.time_limit_us) return BoxResult::Stop::TimeLimit;
            return {};
        };
        // 一つの評価要求を保持し、予算不足なら次回へそのまま渡す
        BoxResult result;
        // 時刻は初回と各評価の直前に確認し、提案の前後での重複取得を避ける
        for (;;) {
            if (auto s = stopped(run_evals_ == 0)) { result.stop = *s; break; }
            if (!pending_) {
                propose();
                if (auto s = stopped()) { result.stop = *s; break; }
            }
            assert(pending_); evaluate(objective);
        }
        result.x = best_x_; result.cost = best_cost_; result.evaluations = run_evals_;
        result.gradient_evaluations = run_gradients_; result.total_evaluations = total_;
        result.elapsed_us = elapsed();
        return result;
    }
public:
    // 箱と設定を前処理する O(n)
    explicit BoxOptimizer(BoxProblem problem, BoxParam param = {}) { reset(std::move(problem),std::move(param)); }
    // 箱と設定を変更して探索履歴を破棄する O(n)
    void reset(BoxProblem problem, BoxParam param = {}) {
        problem_ = std::move(problem); param_ = std::move(param); n_ = static_cast<int>(problem_.bounds.size());
        assert(param_.initial_step.empty() || static_cast<int>(param_.initial_step.size()) == n_);
        free_.clear(); widths_.clear(); steps_.clear();
        // 通常の倍精度で幅、勾配変換、内積を表現できる入力に限る
        for (int i = 0; i < n_; ++i) {
            auto b = problem_.bounds[i]; assert(std::isfinite(b.lower) && std::isfinite(b.upper) && b.lower <= b.upper);
            if (b.lower == b.upper) continue;
            double width = b.upper-b.lower; assert(std::isfinite(width) && std::isfinite(1/width));
            double step = param_.initial_step.empty() ? width*0.25 : param_.initial_step[i];
            assert(step > 0 && step <= width);
            free_.push_back(i); widths_.push_back(width); steps_.push_back(step/width);
        }
        d_ = static_cast<int>(free_.size()); x_.resize(n_); current_.resize(d_); trial_.resize(d_);
        started_ = false;
    }
    // 初期解から新しい探索を開始する O(n+探索に使う時間)
    template<class Objective>
    BoxResult solve(Objective&& objective, const BoxBudget& budget = {}, std::span<const double> initial = {}) {
        auto start = Clock::now();
        begin(initial,std::is_invocable_r_v<double,Objective&,std::span<const double>,std::span<double>>);
        return run(objective,budget,start);
    }
    // 同一の目的関数に追加予算を与えて再開する O(n+探索に使う時間)
    template<class Objective>
    BoxResult resume(Objective&& objective, const BoxBudget& budget = {}) {
        auto start = Clock::now();
        assert(started_ && (oracle_gradient_ == std::is_invocable_r_v<double,Objective&,std::span<const double>,std::span<double>>));
        return run(objective,budget,start);
    }
};
// 構築を含む予算で一度だけ最適化する O(n+探索に使う時間)
template<class Objective>
BoxResult box_optimize(const BoxProblem& problem, Objective&& objective, const BoxBudget& budget = {},
                       std::span<const double> initial = {}, const BoxParam& param = {}) {
    auto start = std::chrono::steady_clock::now();
    BoxResult result;
    {
        BoxOptimizer optimizer(problem,param);
        auto remaining = budget;
        if (remaining.time_limit_us >= 0) remaining.time_limit_us = std::max(0.0,remaining.time_limit_us-
            std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
        result = optimizer.solve(std::forward<Objective>(objective),remaining,initial);
    }
    result.elapsed_us = std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
    return result;
}

#if __INCLUDE_LEVEL__ == 0
// 単体・シナリオテストは直接コンパイルした場合だけ組み込む
struct BoxOptimizerTest {
    using Stop = BoxResult::Stop;
    using Method = BoxParam::Method;
    static inline long long checks = 0;
    static void require(bool condition, const char* label) {
        ++checks;
        if (!condition) { std::cerr << "FAILED: " << label << '\n'; std::abort(); }
    }
    static bool close(double a, double b, double tolerance = 1e-9) {
        return std::abs(a-b) <= tolerance*(1+std::abs(a)+std::abs(b));
    }
    static BoxBudget count(long long n) { return {-1,n,{}}; }
    static void basics() {
        // 空の問題、全固定、ゼロ予算、目標値の優先順位
        int calls = 0;
        auto empty = [&](std::span<const double> x) { require(x.empty(),"empty dimension"); ++calls; return -3.0; };
        BoxOptimizer zero(BoxProblem{});
        auto r = zero.solve(empty,count(0));
        require(!r.has_value() && r.x.empty() && std::isinf(r.cost),"unevaluated result");
        require(r.evaluations == 0 && r.total_evaluations == 0 && r.stop == Stop::EvaluationLimit,"zero counts");
        r = zero.resume(empty,count(1));
        require(r.has_value() && r.x.empty() && r.cost == -3 && calls == 1 && r.stop == Stop::AllFixed,"empty optimum");
        r = zero.resume(empty,{0,0,-3});
        require(r.stop == Stop::TargetReached && r.evaluations == 0,"cached target priority");
        BoxProblem fixed{{{2,2},{-5,-5}}};
        BoxOptimizer solver(fixed);
        auto constant = [&](std::span<const double> x,std::span<double> g) {
            require(x[0] == 2 && x[1] == -5 && g.empty(),"all fixed oracle"); return 7.0;
        };
        r = solver.solve(constant,{0,10,{}});
        require(!r.has_value() && r.stop == Stop::TimeLimit,"zero time");
        r = solver.resume(constant,{-1,1,7});
        require(r.stop == Stop::TargetReached && r.evaluations == 1 && r.gradient_evaluations == 0,"all fixed target");
        auto saved = r;
        r = solver.resume(constant,count(2));
        require(r.stop == Stop::AllFixed && r.evaluations == 0 && r.total_evaluations == 1,"all fixed no duplicate");
        solver.reset(BoxProblem{{{-1,1}}});
        auto f = [](std::span<const double> x) { return (x[0]-0.25)*(x[0]-0.25); };
        r = solver.solve(f,count(1));
        require(r.x == std::vector<double>{0} && r.cost == 0.0625,"midpoint");
        require(saved.x == std::vector<double>({2,-5}) && saved.cost == 7,"owned result");
        r = solver.resume(f,{-1,1000,1e-10});
        require(r.has_value() && r.cost <= 1e-10 && r.stop == Stop::TargetReached,"target reached");
        auto target_count = r.total_evaluations;
        r = solver.resume(f,{-1,0,r.cost});
        require(r.evaluations == 0 && r.total_evaluations == target_count && r.stop == Stop::TargetReached,"cached target zero budget");
        r = solver.resume(f,count(20));
        require(r.evaluations == 20 && r.total_evaluations == target_count+20,"target removal");
        r = box_optimize(BoxProblem{{{-1,1}}},f,count(1),std::vector<double>{0.75});
        require(r.cost == 0.25 && r.x[0] == 0.75 && r.evaluations == 1,"one shot initial");
        r = box_optimize(BoxProblem{{{-1,1}}},f,{0,0,{}});
        require(!r.has_value() && r.stop == Stop::EvaluationLimit,"both exhausted priority");
    }
    static void initialization_and_oracles() {
        // 初期点を往復変換せず、そのまま最初の評価へ渡す
        BoxProblem problem{{{1e6,1e6+0.1},{-1,1},{4,4}}};
        std::vector<double> initial{std::nextafter(1e6+0.03,1e6),std::nextafter(0.3,0.0),4};
        BoxParam param; param.initial_step = {0.01,0.2,std::numeric_limits<double>::quiet_NaN()};
        BoxOptimizer solver(problem,param);
        auto f = [&](std::span<const double> x) {
            require(std::equal(x.begin(),x.end(),initial.begin()),"exact caller initial"); return 5.0;
        };
        auto r = solver.solve(f,count(1),initial);
        require(r.x == initial && solver.d_ == 2,"fixed mapping");
        require(close(solver.steps_[0],0.01/(problem.bounds[0].upper-problem.bounds[0].lower)),"step normalization");
        // 両形式がある関数では二引数を優先し、勾配なしの指定なら空spanにする
        struct Both {
            int one = 0, two = 0, gradients = 0;
            double operator()(std::span<const double>) { ++one; return 99; }
            double operator()(std::span<const double> x,std::span<double> g) {
                ++two; if (!g.empty()) { ++gradients; g[0] = 2*x[0]; } return x[0]*x[0];
            }
        } both;
        BoxOptimizer one(BoxProblem{{{-2,3}}});
        r = one.solve(both,count(1));
        require(both.one == 0 && both.two == 1 && both.gradients == 1 && r.gradient_evaluations == 1,"two argument preference");
        require(close(one.gradient_z_[0],5),"gradient chain rule");
        param = {}; param.method = Method::Coordinate;
        one.reset(BoxProblem{{{-2,3}}},param); r = one.solve(both,count(5));
        require(r.gradient_evaluations == 0 && both.gradients == 1 && both.one == 0,"forced derivative free");
        // move専用の関数オブジェクトも保持やコピーを要求しない
        auto movable = [p=std::make_unique<double>(0.2)](std::span<const double> x) { return (x[0]-*p)*(x[0]-*p); };
        r = one.solve(std::move(movable),count(4));
        require(r.evaluations == 4,"move only objective");
    }
    static void traces_and_resume() {
        // 途中停止を含む全経路で、評価列・最良点・回数を照合する
        std::mt19937_64 rng(54812);
        for (int test = 0; test < 120; ++test) {
            int n = 1+static_cast<int>(rng()%12);
            BoxProblem problem; std::vector<double> target(n),initial(n);
            BoxParam p; p.seed = rng(); p.method = static_cast<Method>(test%4);
            bool gradient = p.method == Method::SpectralGradient || (test/4)%2 != 0;
            for (int i = 0; i < n; ++i) {
                double lo = -7+static_cast<double>(rng()%1000)*0.01;
                double w = std::pow(10.0,static_cast<int>(rng()%5)-2);
                if (rng()%4 == 0) w = 0;
                problem.bounds.push_back({lo,lo+w});
                target[i] = lo+w*0.27; initial[i] = lo+w*0.79;
            }
            BoxOptimizer a(problem,p), b(problem,p);
            bool need_gradient = gradient && (p.method == Method::Auto || p.method == Method::SpectralGradient) && a.d_ > 0;
            std::vector<std::vector<double>> trace_a, trace_b;
            auto value = [&](std::span<const double> x,std::span<double> g) {
                double f = 0;
                for (int i = 0; i < n; ++i) {
                    require(x[i] >= problem.bounds[i].lower && x[i] <= problem.bounds[i].upper,"box feasibility");
                    double w = problem.bounds[i].upper-problem.bounds[i].lower;
                    double z = w == 0 ? 0 : (x[i]-target[i])/w;
                    f += z*z; if (!g.empty()) g[i] = w == 0 ? 0 : 2*z/w;
                }
                return f;
            };
            auto ga = [&](std::span<const double> x,std::span<double> g) { trace_a.emplace_back(x.begin(),x.end()); return value(x,g); };
            auto gb = [&](std::span<const double> x,std::span<double> g) { trace_b.emplace_back(x.begin(),x.end()); return value(x,g); };
            auto fa = [&](std::span<const double> x) { return ga(x,{}); };
            auto fb = [&](std::span<const double> x) { return gb(x,{}); };
            constexpr int evaluations = 777;
            BoxResult r1 = gradient ? a.solve(ga,count(evaluations),initial) : a.solve(fa,count(evaluations),initial);
            BoxResult r2 = gradient ? b.solve(gb,count(0),initial) : b.solve(fb,count(0),initial);
            for (int k = 0; k < evaluations; ++k) {
                r2 = gradient ? b.resume(gb,count(1)) : b.resume(fb,count(1));
                require(r2.evaluations <= 1,"strict slice count");
                require(!b.pending_ && b.need_gradient_ == need_gradient,"consumed candidate and invariant gradient mode");
            }
            require(trace_a == trace_b && r1.x == r2.x && r1.cost == r2.cost,"identical resumed trace");
            require(r1.total_evaluations == static_cast<long long>(trace_a.size()) && r1.total_evaluations == r2.total_evaluations,"cumulative count");
            double best = std::numeric_limits<double>::infinity(); std::vector<double> point;
            for (const auto& x : trace_a) { double f = value(x,{}); if (f < best) { best = f; point = x; } }
            require(r1.cost == best && r1.x == point,"best actual evaluation");
            require(r1.cost == value(r1.x,{}),"matching result cost");
            auto copy = b;
            auto r3 = gradient ? copy.resume(gb,count(17)) : copy.resume(fb,count(17));
            r2 = gradient ? b.resume(gb,count(17)) : b.resume(fb,count(17));
            require(r2.x == r3.x && r2.cost == r3.cost,"independent state copy");
            trace_b.clear();
            auto again = gradient ? b.solve(gb,count(evaluations),initial) : b.solve(fb,count(evaluations),initial);
            require(trace_a == trace_b && again.total_evaluations == r1.total_evaluations,"solve resets rng and counters");
        }
    }
    static std::vector<double> linear_solve(std::vector<double> a, std::vector<double> b) {
        int n = static_cast<int>(b.size());
        // テスト用の小さい連立方程式を部分ピボット付きで解く
        for (int i = 0; i < n; ++i) {
            int p = i;
            for (int j = i+1; j < n; ++j) if (std::abs(a[j*n+i]) > std::abs(a[p*n+i])) p = j;
            for (int j = 0; j < n; ++j) std::swap(a[i*n+j],a[p*n+j]);
            std::swap(b[i],b[p]);
            double pivot = a[i*n+i]; require(std::abs(pivot) > 1e-12,"reference pivot");
            for (int j = i; j < n; ++j) a[i*n+j] /= pivot;
            b[i] /= pivot;
            for (int k = 0; k < n; ++k) if (k != i) {
                double v = a[k*n+i];
                for (int j = i; j < n; ++j) a[k*n+j] -= v*a[i*n+j];
                b[k] -= v*b[i];
            }
        }
        return b;
    }
    static void convex_reference() {
        std::mt19937_64 rng(67413);
        // 活性制約の全列挙を独立した参照実装にする
        for (Method method : {Method::Auto,Method::SpectralGradient}) for (int test = 0; test < 120; ++test) {
            int n = 1+test%4;
            std::vector<double> matrix(n*n),a(n*n),b(n);
            for (double& v : matrix) v = static_cast<double>(rng()%2001)/1000-1;
            for (int i = 0; i < n; ++i) {
                b[i] = static_cast<double>(rng()%4001)/1000-2;
                for (int j = 0; j < n; ++j) {
                    for (int k = 0; k < n; ++k) a[i*n+j] += matrix[k*n+i]*matrix[k*n+j];
                    if (i == j) a[i*n+j] += 0.3;
                }
            }
            auto f = [&](std::span<const double> x,std::span<double> g) {
                double value = 0;
                for (int i = 0; i < n; ++i) {
                    double ax = 0;
                    for (int j = 0; j < n; ++j) ax += a[i*n+j]*x[j];
                    value += 0.5*x[i]*ax+b[i]*x[i]; if (!g.empty()) g[i] = ax+b[i];
                }
                return value;
            };
            int cases = 1; for (int i = 0; i < n; ++i) cases *= 3;
            double exact = std::numeric_limits<double>::infinity();
            for (int mask = 0; mask < cases; ++mask) {
                int code = mask; std::vector<int> free; std::vector<double> x(n);
                for (int i = 0; i < n; ++i) {
                    int state = code%3; code /= 3;
                    if (state == 0) free.push_back(i); else x[i] = state == 1 ? -1 : 1;
                }
                int d = static_cast<int>(free.size()); std::vector<double> mat(d*d),rhs(d);
                for (int i = 0; i < d; ++i) {
                    rhs[i] = -b[free[i]];
                    for (int j = 0; j < n; ++j) rhs[i] -= a[free[i]*n+j]*x[j];
                    for (int j = 0; j < d; ++j) mat[i*d+j] = a[free[i]*n+free[j]];
                }
                auto answer = linear_solve(mat,rhs);
                bool feasible = true;
                for (int i = 0; i < d; ++i) { x[free[i]] = answer[i]; feasible &= std::abs(answer[i]) <= 1; }
                if (feasible) exact = std::min(exact,f(x,{}));
            }
            BoxParam param; param.search = BoxParam::Search::Refine; param.method = method;
            BoxOptimizer solver(BoxProblem{std::vector<BoxBound>(n,{-1,1})},param);
            auto r = solver.solve(f,count(4000));
            require(close(r.cost,exact,2e-7),"active set reference optimum");
            require(close(r.cost,f(r.x,{})),"reference result evaluation");
            for (int i = 0; i < solver.gd_.count; ++i) require(solver.gd_.rho[i] > 0,"positive curvature");
        }
        // 境界で大きい勾配を持つ成分が、内部の成分の進行を妨げない
        BoxParam param; param.search = BoxParam::Search::Refine;
        BoxOptimizer solver(BoxProblem{{{0,1},{0,1}}},param);
        auto blocked = [](std::span<const double> x,std::span<double> g) {
            if (!g.empty()) { g[0] = 1e8; g[1] = 2*(x[1]-0.2); }
            return 1e8*x[0]+(x[1]-0.2)*(x[1]-0.2);
        };
        auto r = solver.solve(blocked,count(100),std::vector<double>{0,0.8});
        require(r.cost < 1e-14 && std::abs(r.x[1]-0.2) < 1e-7,"blocked gradient scaling");
        // 投影前に降下方向でも、投影後に上昇する方向は使わない
        auto linear = [](std::span<const double> x,std::span<double> g) {
            if (!g.empty()) { g[0] = -1; g[1] = 2; } return -x[0]+2*x[1];
        };
        solver.solve(linear,count(1),std::vector<double>{0.9,0.1}); solver.initialize_gradient();
        solver.gd_.direction = {100,10}; solver.gd_.direction_ready = true; solver.gd_.rate = 1;
        solver.propose_gradient();
        require(solver.gd_.dot < 0 && solver.pending_,"descent after projection");
    }
    static void internal_invariants() {
        // 乱数の再現性と正規分布の基本統計を固定seedで確認する
        BoxOptimizer::Random a,b; double sum = 0, square = 0;
        for (int i = 0; i < 50000; ++i) {
            double v = a.normal(); require(v == b.normal() && std::isfinite(v),"normal reproducibility");
            sum += v; square += v*v;
        }
        require(std::abs(sum/50000) < 0.025 && std::abs(square/50000-1) < 0.04,"normal moments");
        for (int d : {1,2,8,64,512}) {
            BoxParam p; p.method = Method::DiagonalCma;
            BoxOptimizer solver(BoxProblem{std::vector<BoxBound>(d,{0,1})},p);
            auto f = [](std::span<const double> x) { double v = 0; for (double z : x) v += (z-0.31)*(z-0.31); return v; };
            auto r = solver.solve(f,count(1));
            require(solver.cm_.points.empty(),"lazy population allocation");
            for (int k = 0; k < 120; ++k) {
                r = solver.resume(f,count(1));
                auto& c = solver.cm_;
                require(c.child >= 0 && c.child <= c.lambda,"partial generation");
                require(c.sigma > 0 && std::isfinite(c.sigma),"positive cma sigma");
                for (double v : c.covariance) require(v > 0 && std::isfinite(v),"positive diagonal covariance");
                require(close(std::accumulate(c.weights.begin(),c.weights.end(),0.0),1),"normalized cma weights");
            }
            require(r.cost == f(r.x),"cma evaluated incumbent");
        }
        // 焼きなまし中も一回ずつの再開で全く同じ列を得る
        BoxProblem box{{{0,1},{0,1}}}; BoxOptimizer sa(box),sb(box);
        std::vector<std::vector<double>> x,y;
        auto fa = [&](std::span<const double> p) { x.emplace_back(p.begin(),p.end()); return 3.0; };
        auto fb = [&](std::span<const double> p) { y.emplace_back(p.begin(),p.end()); return 3.0; };
        auto r = sa.solve(fa,count(2200)); sb.solve(fb,count(0));
        bool saw_anneal = false, saw_cma = false;
        for (int i = 0; i < 2200; ++i) {
            sb.resume(fb,count(1));
            saw_anneal |= sb.anneal_left_ > 0; saw_cma |= sb.method_ == BoxOptimizer::Method::DiagonalCma;
            require(!sb.pending_ && !sb.need_gradient_,"phase changes preserve derivative-free mode");
        }
        require(saw_anneal && saw_cma && x == y,"annealing and cma resume progression");
        require(r.x == std::vector<double>({0.5,0.5}),"equal costs preserve first incumbent");
    }
    static void scenarios() {
        // 目的関数を更新するターンではsolveを使い、前解だけを引き継ぐ
        BoxParam p; p.search = BoxParam::Search::Refine;
        BoxOptimizer solver(BoxProblem{std::vector<BoxBound>(8,{0,1})},p);
        std::vector<double> previous;
        double optimum = 0.2, offset = 0;
        auto demand = [&](std::span<const double> x,std::span<double> g) {
            double value = offset;
            for (int i = 0; i < 8; ++i) { double v = x[i]-optimum; value += v*v; if (!g.empty()) g[i] = 2*v; }
            return value;
        };
        auto r = solver.solve(demand,count(100)); previous = r.x;
        auto capacity = solver.gd_.s.capacity();
        for (int turn = 1; turn <= 30; ++turn) {
            optimum = turn%7 == 0 ? 0.8 : 0.3+0.01*(turn%9); offset = 100+turn;
            r = solver.solve(demand,count(1),previous);
            require(r.evaluations == 1 && r.total_evaluations == 1 && r.x == previous,"new turn evaluates warm point");
            require(r.cost == demand(previous,{}),"discard old objective value");
            r = solver.resume(demand,count(100));
            require(close(r.cost,offset,1e-10),"warm turn converges");
            require(solver.gd_.s.capacity() >= capacity,"reuse allocated memory");
            previous = r.x;
        }
        // 倍率と定数項を変えても、定常点に到達する
        for (double scale : {1e-4,1.0,1e5}) for (double shift : {0.0,300.0}) {
            auto transformed = [&](std::span<const double> x,std::span<double> g) {
                double f = 0;
                for (int i = 0; i < 8; ++i) {
                    double z = x[i]-(0.1+0.07*i); f += (i+1)*z*z;
                    if (!g.empty()) g[i] = scale*2*(i+1)*z;
                }
                return shift+scale*f;
            };
            r = solver.solve(transformed,{-1,2000,shift+scale*1e-7});
            require((r.cost-shift)/scale < 1.1e-7,"objective scale and shift");
        }
        auto flat = [](std::span<const double>) { return 9.0; };
        r = solver.solve(flat,count(10000));
        require(r.stop == Stop::LocalStop,"refine finite stop on plateau");
        require(solver.resume(flat,count(10)).evaluations == 0,"local stop resume");
        // 丸め後の提出得点を直接目的関数にする
        BoxOptimizer rounded(BoxProblem{std::vector<BoxBound>(4,{0,10})});
        auto discrete = [](std::span<const double> x) {
            double f = 0;
            for (int i = 0; i < 4; ++i) { double z = std::round(x[i])-(2*i+1); f += z*z; }
            return f;
        };
        r = rounded.solve(discrete,{-1,4000,0});
        require(r.cost == 0 && discrete(r.x) == 0,"decoded actual score");
        auto original = std::vector<double>{1,3,5,7};
        auto surrogate = [](std::span<const double> x) { double f = 0; for (double v : x) f += (v-5)*(v-5); return f; };
        auto proxy = rounded.solve(surrogate,count(300),original);
        auto submitted = discrete(proxy.x) < discrete(original) ? proxy.x : original;
        require(discrete(submitted) == 0,"retain original real score outside surrogate");
        // 初期解での目標到達後、厳しい目標で進行を再開する
        auto quadratic = [](std::span<const double> x,std::span<double> g) {
            double f = 0;
            for (int i = 0; i < static_cast<int>(x.size()); ++i) { f += x[i]*x[i]; if (!g.empty()) g[i] = 2*x[i]; }
            return f;
        };
        solver.reset(BoxProblem{{{-1,1},{3,3},{-1,1}}});
        r = solver.solve(quadratic,{-1,1,20},std::vector<double>{0.7,3,0.4});
        require(r.stop == Stop::TargetReached && r.total_evaluations == 1,"initial target");
        r = solver.resume(quadratic,{-1,1000,9+1e-10});
        require(r.cost <= 9+1e-10 && r.x[1] == 3 && r.total_evaluations > 1,"stricter target and changed bounds");
    }
    static void partial_problem() {
        // 影響する辺だけの評価と全体評価をランダムな候補で照合する
        constexpr int n = 200, d = 8;
        std::vector<double> base(n),target(n);
        std::vector<int> index(n,-1),active;
        std::vector<std::pair<int,int>> edges;
        std::mt19937_64 rng(93054);
        for (int i = 0; i < n; ++i) { base[i] = static_cast<double>(rng()%1000)/1000; target[i] = static_cast<double>(rng()%1000)/1000; }
        for (int i = 0; i < d; ++i) { int j = i*19+3; index[j] = i; active.push_back(j); }
        for (int i = 0; i < 600; ++i) edges.emplace_back(static_cast<int>(rng()%n),static_cast<int>(rng()%n));
        auto full = [&](std::span<const double> x) {
            double f = 0;
            for (int i = 0; i < n; ++i) f += (x[i]-target[i])*(x[i]-target[i]);
            for (auto [a,b] : edges) f += 0.2*(x[a]-x[b])*(x[a]-x[b]);
            return f;
        };
        double constant = 0; std::vector<std::pair<int,int>> relevant;
        for (int i = 0; i < n; ++i) if (index[i] < 0) constant += (base[i]-target[i])*(base[i]-target[i]);
        for (auto [a,b] : edges) {
            if (index[a] < 0 && index[b] < 0) constant += 0.2*(base[a]-base[b])*(base[a]-base[b]);
            else relevant.emplace_back(a,b);
        }
        auto partial = [&](std::span<const double> x) {
            double f = constant;
            for (int i = 0; i < d; ++i) f += (x[i]-target[active[i]])*(x[i]-target[active[i]]);
            for (auto [a,b] : relevant) {
                double v = (index[a] < 0 ? base[a] : x[index[a]])-(index[b] < 0 ? base[b] : x[index[b]]);
                f += 0.2*v*v;
            }
            return f;
        };
        for (int test = 0; test < 100; ++test) {
            auto all = base; std::vector<double> x(d);
            for (int i = 0; i < d; ++i) all[active[i]] = x[i] = static_cast<double>(rng()%1000)/1000;
            require(close(full(all),partial(x),1e-12),"partial affected terms equality");
        }
        BoxProblem large{std::vector<BoxBound>(n)};
        for (int i = 0; i < n; ++i) large.bounds[i] = index[i] < 0 ? BoxBound{base[i],base[i]} : BoxBound{0,1};
        std::vector<double> initial(d);
        for (int i = 0; i < d; ++i) initial[i] = base[active[i]];
        auto a = box_optimize(large,full,count(2000),base);
        auto b = box_optimize(BoxProblem{std::vector<BoxBound>(d,{0,1})},partial,count(2000),initial);
        require(close(a.cost,b.cost,1e-8),"compact versus fixed optimization");
        for (int i = 0; i < n; ++i) if (index[i] < 0) require(a.x[i] == base[i],"unchanged fixed graph state");
    }
    static void reset_across_modes() {
        // 次元・固定集合・勾配形式を変え、0予算から再開しても古い状態を使わない
        BoxOptimizer solver(BoxProblem{});
        for (int n : {0,5,200,3}) for (int method = 0; method < 4; ++method) for (bool gradient : {true,false}) {
            if (method == 3 && !gradient) continue;
            BoxProblem problem{std::vector<BoxBound>(n,{-2,3})};
            for (int i = 0; i < n; i += 3) problem.bounds[i] = {1,1};
            BoxParam p; p.method = static_cast<Method>(method); p.search = BoxParam::Search::Refine;
            solver.reset(problem,p);
            int calls = 0;
            auto fg = [&](std::span<const double> x,std::span<double> g) {
                require(static_cast<int>(x.size()) == n,"reset original dimension");
                require(g.empty() || g.size() == x.size(),"reset gradient dimension");
                double cost = 0;
                for (int i = 0; i < n; ++i) {
                    require(problem.bounds[i].lower <= x[i] && x[i] <= problem.bounds[i].upper,"reset changed box");
                    double z = x[i]-1; cost += z*z; if (!g.empty()) g[i] = 2*z;
                }
                ++calls; return cost;
            };
            auto f = [&](std::span<const double> x) { return fg(x,{}); };
            auto r = gradient ? solver.solve(fg,count(0)) : solver.solve(f,count(0));
            require(!r.has_value() && calls == 0 && r.total_evaluations == 0,"reset zero budget clears incumbent");
            r = gradient ? solver.resume(fg,count(1)) : solver.resume(f,count(1));
            require(r.has_value() && r.evaluations == 1 && r.total_evaluations == 1,"reset first evaluation");
            require(r.gradient_evaluations == (gradient && (method == 0 || method == 3) && n > 1 ? 1 : 0),"reset gradient request");
            auto initial_cost = r.cost;
            r = gradient ? solver.resume(fg,count(40)) : solver.resume(f,count(40));
            require(r.cost <= initial_cost && r.total_evaluations == calls,"reset resumed result");
        }
    }
    static void cma_pairs() {
        for (int d : {1,3,8,12,48}) {
            BoxParam p; p.method = Method::DiagonalCma;
            BoxOptimizer solver(BoxProblem{std::vector<BoxBound>(d,{0,1})},p);
            std::vector<double> initial(d);
            for (int i = 0; i < d; ++i) initial[i] = i%3 == 0 ? 0 : i%3 == 1 ? 0.99 : 0.5;
            auto f = [](std::span<const double> x) {
                double cost = 0; for (double v : x) cost += (v-0.3)*(v-0.3); return cost;
            };
            solver.solve(f,count(2),initial);
            require(solver.cm_.child == 1,"first paired child consumed");
            auto random_state = solver.rng_.state;
            auto mean = solver.cm_.mean, first = solver.cm_.points;
            solver.propose_cma();
            require(solver.rng_.state == random_state,"mirrored child needs no random draw");
            for (int i = 0; i < d; ++i)
                require(solver.trial_[i] == std::clamp(mean[i]-(first[i]-mean[i]),0.0,1.0),"clipped mirror position");
            auto paused = solver.resume(f,{0,1,{}});
            require(paused.evaluations == 0 && paused.stop == Stop::TimeLimit && solver.pending_,"zero time preserves pending candidate");
            auto r = solver.resume(f,count(1));
            require(r.evaluations == 1 && r.total_evaluations == 3 && r.cost == f(r.x),"pending mirror resumes once");
        }
    }
    static void spectral_scenarios() {
        for (auto search : {BoxParam::Search::Explore,BoxParam::Search::Refine}) {
            BoxParam p; p.method = Method::SpectralGradient; p.search = search;
            BoxOptimizer solver(BoxProblem{{{-2,3},{0,1},{7,7}}},p);
            double target = 0.2,offset = 0;
            auto f = [&](std::span<const double> x,std::span<double> g) {
                require(g.size() == x.size(),"spectral requests analytic gradient");
                g[0] = 2*(x[0]-target); g[1] = 8*(x[1]-0.3); g[2] = 0;
                require(x[2] == 7,"spectral preserves fixed coordinate");
                return offset+(x[0]-target)*(x[0]-target)+4*(x[1]-0.3)*(x[1]-0.3);
            };
            auto r = solver.solve(f,{0,100,{}});
            require(!r.has_value() && r.evaluations == 0,"spectral zero time");
            r = solver.resume(f,{-1,5000,1e-12});
            require(r.stop == Stop::TargetReached && r.cost < 1e-12,"spectral quadratic target");
            require(r.evaluations == r.gradient_evaluations && solver.gd_.s.empty(),"spectral no new history storage");
            auto previous = r.x; auto owned = r;
            auto capacity = solver.gd_.direction.capacity();
            for (int turn = 1; turn <= 8; ++turn) {
                target = 0.2+0.03*turn; offset = turn;
                r = solver.solve(f,count(1),previous);
                require(r.x == previous && r.cost > offset && r.total_evaluations == 1,"spectral changed objective reevaluates");
                r = solver.resume(f,{-1,5000,offset+1e-11});
                require(r.cost <= offset+1e-11,"spectral warm turn converges");
                require(solver.gd_.direction.capacity() >= capacity,"spectral reuses work capacity");
                previous = r.x;
            }
            require(owned.cost < 1e-12 && owned.x[2] == 7,"spectral owned result survives turns");
            solver.reset(BoxProblem{{{2,2}}},p);
            auto fixed = [](std::span<const double> x) { return x[0]; };
            r = solver.solve(fixed,count(10));
            require(r.cost == 2 && r.evaluations == 1 && r.gradient_evaluations == 0 && r.stop == Stop::AllFixed,"spectral all fixed needs no gradient");
            r = box_optimize(BoxProblem{},[](std::span<const double> x) { return static_cast<double>(x.size()); },count(1),{},p);
            require(r.cost == 0 && r.stop == Stop::AllFixed,"spectral empty one shot");
        }
    }
    static void time_budget() {
        // 時計精度そのものは要求せず、評価開始と非中断コールバックの扱いを確認する
        int calls = 0;
        BoxOptimizer solver(BoxProblem{{{0,1}}});
        auto heavy = [&](std::span<const double> x) {
            ++calls; std::this_thread::sleep_for(std::chrono::milliseconds(3)); return x[0]*x[0];
        };
        auto r = solver.solve(heavy,{1000,10,{}});
        require(r.evaluations <= 1 && r.evaluations == calls && r.stop == Stop::TimeLimit,"uninterruptible callback");
        require(r.elapsed_us >= 1000,"wall time reports overrun");
        auto cached = r.x;
        r = solver.resume(heavy,{0,10,{}});
        require(r.evaluations == 0 && r.x == cached,"zero time reuses incumbent");
        auto fast = [](std::span<const double> x) { return x[0]*x[0]; };
        auto start = std::chrono::steady_clock::now();
        r = box_optimize(BoxProblem{{{0,1}}},fast,{500,LLONG_MAX,{}});
        double elapsed = std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
        require(r.elapsed_us >= 0 && r.elapsed_us <= elapsed+1,"wrapper timing includes construction");
    }
    static void invalid(int id) {
        auto f = [](std::span<const double> x) { return x[0]*x[0]; };
        auto g = [](std::span<const double> x,std::span<double> grad) { if (!grad.empty()) grad[0] = 2*x[0]; return x[0]*x[0]; };
        BoxProblem problem{{{0,1}}};
        BoxOptimizer solver(problem);
        if (id == 0) { BoxOptimizer bad(BoxProblem{{{1,0}}}); }
        if (id == 1) solver.solve(f,count(1),std::vector<double>{0,0});
        if (id == 2) solver.solve(f,count(1),std::vector<double>{2});
        if (id == 3) { BoxParam p; p.initial_step = {-1}; solver.reset(problem,p); }
        if (id == 4) solver.solve(f,{-1,-1,{}});
        if (id == 5) solver.solve(f,{-1,LLONG_MAX,{}});
        if (id == 6) solver.solve([](std::span<const double>){ return std::numeric_limits<double>::infinity(); },count(1));
        if (id == 7) solver.solve([](std::span<const double>,std::span<double> grad){ grad[0] = std::numeric_limits<double>::quiet_NaN(); return 0.0; },count(1));
        if (id == 8) solver.resume(f,count(1));
        if (id == 9) { solver.solve(f,count(1)); solver.resume(g,count(1)); }
        if (id == 10) { solver.solve(f,count(1)); solver.reset(problem); solver.resume(f,count(1)); }
        if (id == 11) { BoxOptimizer bad(BoxProblem{{{0,std::numeric_limits<double>::quiet_NaN()}}}); }
        if (id == 12) { BoxParam p; p.initial_step = {0.1,0.2}; solver.reset(problem,p); }
        if (id == 13) solver.solve(f,{std::numeric_limits<double>::quiet_NaN(),1,{}});
        if (id == 14) solver.solve(f,{-1,1,std::numeric_limits<double>::quiet_NaN()});
        if (id == 15) { BoxParam p; p.method = Method::SpectralGradient; solver.reset(problem,p); solver.solve(f,count(1)); }
    }
    static int main(int argc,char** argv) {
        if (argc == 3 && std::string(argv[1]) == "--invalid") { invalid(std::stoi(argv[2])); return 1; }
        basics(); initialization_and_oracles(); traces_and_resume(); convex_reference();
        internal_invariants(); scenarios(); partial_problem(); reset_across_modes(); cma_pairs(); spectral_scenarios(); time_budget();
        std::cout << "PASS " << checks << " checks\n";
        return 0;
    }
};
int main(int argc,char** argv) { return BoxOptimizerTest::main(argc,argv); }
#endif
