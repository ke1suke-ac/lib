#pragma once
#include <bits/stdc++.h>

// 任意の負の対数密度から最良候補を探索し、固定したMH遷移で標本を得る。
// C++20、単スレッド。評価は有限doubleまたは+inf。NaN/-infは禁止。
// 有限項の和・温度計算などの中間値もdoubleに収まる範囲で使い、fast-mathは指定しない。
// 状態はコピー可能、形・次元は固定。同じ状態・目的関数では評価値を固定する。
// 通常はmake_numeric_session / make_sessionでモデルと提案を所有させる。
// 下位APIを直接使う場合だけ、共有データ変更後のrefresh等を利用側で管理する。
struct McmcState {
    std::vector<double> real;
    std::vector<std::int64_t> discrete;
};

template<class State = McmcState>
class McmcEstimator {
public:
    template<class Model, class Move> class Session;
    using Clock = std::chrono::steady_clock;
    enum class Phase { Search, Warmup, Sample };
    enum class Stop { Budget, NoFiniteState };

    class Rng {
        std::uint64_t state_;
        double spare_ = 0;
        bool has_spare_ = false;
    public:
        // 乱数を初期化する。O(1)
        explicit Rng(std::uint64_t seed = 1) : state_(seed) {}
        // SplitMix64の64bit乱数を返す。O(1)
        std::uint64_t next() {
            auto z = (state_ += 0x9e3779b97f4a7c15ULL);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return z ^ (z >> 31);
        }
        // [0,1)の一様乱数を返す。O(1)
        double uniform() { return static_cast<double>(next() >> 11) * 0x1p-53; }
        // (0,1]の一様乱数の対数を返す。O(1)
        double log_uniform() { return std::log(static_cast<double>((next() >> 11) + 1) * 0x1p-53); }
        // [0,n)の整数を偏りなく返す。期待O(1)。0<n<=2^31-1。
        int integer(int n) {
            assert(n > 0);
            const auto bound = static_cast<std::uint64_t>(n);
            const auto threshold = -bound % bound;
            std::uint64_t x;
            do { x = next(); } while (x < threshold);
            return static_cast<int>(x % bound);
        }
        // 標準正規乱数を返す。期待O(1)
        double normal() {
            if (has_spare_) { has_spare_ = false; return spare_; }
            double x, y, r;
            do { x = 2 * uniform() - 1; y = 2 * uniform() - 1; r = x*x + y*y; }
            while (r == 0 || r >= 1);
            const double a = std::sqrt(-2 * std::log(r) / r);
            spare_ = y*a; has_spare_ = true;
            return x*a;
        }
    };

    struct Budget {
        Clock::time_point deadline = Clock::time_point::max();
        std::uint64_t remaining_steps = std::numeric_limits<std::uint64_t>::max();
        // マイクロ秒の予算を作る。O(1)。期限はコールバックの強制停止を保証しない。
        static Budget for_us(std::int64_t us) {
            assert(us >= 0);
            return {Clock::now() + std::chrono::microseconds(us)};
        }
        // 絶対期限と任意の遷移数上限を指定する。O(1)
        static Budget until(Clock::time_point end, std::uint64_t steps = UINT64_MAX) { return {end, steps}; }
        // 再現可能な完了遷移数の予算を作る。初期評価は遷移数に含めない。O(1)
        static Budget for_steps(std::uint64_t steps) { return {Clock::time_point::max(), steps}; }
        // 時間が残っているか調べる。O(1)
        bool time_left() const { return deadline == Clock::time_point::max() || Clock::now() < deadline; }
        // 新たな計算を始められるか調べる。O(1)
        bool available() const { return remaining_steps != 0 && time_left(); }
    };

    struct Param {
        std::uint64_t seed = 1;
        std::uint64_t sample_stride = 1; // 鎖ごとの完了遷移数で記録する。
        double start_temperature = 1;
        double end_temperature = 0.001;
        // 0なら最初のSearchの予算から決定。分割する場合は共通の総予定を指定する。
        std::uint64_t cooling_steps = 0;
        Clock::time_point cooling_deadline = Clock::time_point::max();
        std::size_t observation_check_interval = 32; // 観測項の時計確認間隔。重い項では1を指定。
        // Session::sampleの準備量。未指定:数値はmax(512,64*可動変数数*鎖数)、全固定は0、独自状態は512*鎖数。
        std::optional<std::uint64_t> warmup_steps = std::nullopt; // 収束の保証ではない。明示した0で準備を省略する。
    };
    struct Report {
        Stop stop = Stop::Budget;
        std::uint64_t steps = 0, accepted = 0, evaluations = 0, terms = 0, emitted = 0;
        std::int64_t elapsed_us = 0;
        bool synchronized = false, has_estimate = false;
    };
    struct Best { const State* state; double energy; };
    struct MoveContext {
        Phase phase;
        double temperature;
        std::size_t chain;
        std::span<const State> population;
        std::uint64_t revision;
    };
    struct Domain {
        struct Real { double lower, upper, scale; };
        // 整数の絶対値・幅は10^9以下を想定。カテゴリも[lower,upper]のID。
        struct Discrete { std::int64_t lower, upper; bool categorical = false; };
        std::vector<Real> real;
        std::vector<Discrete> discrete;
    };
    struct MoveParam {
        bool adapt = true;
        double difference_probability = 0; // Warmup/Sampleだけで使う。3鎖以上が必要。
        bool cauchy = true; // Searchの変更幅に重い裾を持たせる。
        double stretch_probability = 0.3; // Warmup/Sampleの伸縮提案。2鎖以上が必要。
    };

    class NumericMove {
        template<class, class> friend class Session;
        Domain domain_;
        MoveParam param_;
        std::vector<int> active_;
        std::vector<double> scale_, initial_;
        std::vector<int> attempts_, accepts_;
        std::uint64_t revision_ = UINT64_MAX;
        int real_count_ = 0, touched_ = -1, reverse_ = -1;
        double last_step_ = 0;
        std::size_t reverse_chain_ = 0;
        bool self_ = false;
        std::vector<int> next_discrete_;
        std::uint64_t warmup_size(std::size_t chains) const {
            return active_.empty() ? 0 : std::max<std::uint64_t>(512,64*active_.size()*chains);
        }
        void validate(const State& s) const {
            assert(s.real.size() == domain_.real.size() && s.discrete.size() == domain_.discrete.size());
            for (std::size_t i = 0; i < domain_.real.size()+domain_.discrete.size(); ++i)
                assert(bounded(s,static_cast<int>(i)));
            (void)s;
        }
        bool bounded(const State& s, int j) const {
            if (j < static_cast<int>(domain_.real.size())) {
                const auto& a = domain_.real[static_cast<std::size_t>(j)];
                const double x = s.real[static_cast<std::size_t>(j)];
                if (!std::isfinite(x)) return false;
                return a.lower <= x && x <= a.upper;
            }
            const auto k = static_cast<std::size_t>(j)-domain_.real.size();
            return domain_.discrete[k].lower <= s.discrete[k] && s.discrete[k] <= domain_.discrete[k].upper;
        }
    public:
        static constexpr bool sampling_safe = true;
        // 数値提案を準備する。O(d)。実数scaleは正、固定変数では使用しない。
        explicit NumericMove(Domain domain, MoveParam param = {}) : domain_(std::move(domain)), param_(param) {
            assert(param.difference_probability >= 0 && param.stretch_probability >= 0
                && param.difference_probability + param.stretch_probability <= 1);
            for (const auto& a : domain_.real) {
                assert(a.lower <= a.upper && std::isfinite(a.scale) && a.scale > 0);
                if (a.lower != a.upper) active_.push_back(static_cast<int>(scale_.size()));
                scale_.push_back(a.scale);
            }
            real_count_ = static_cast<int>(active_.size());
            for (const auto& a : domain_.discrete) {
                assert(a.lower <= a.upper && a.upper-a.lower <= 1000000000);
                if (a.lower != a.upper) active_.push_back(static_cast<int>(scale_.size()));
                scale_.push_back(std::max(1.0, static_cast<double>(a.upper-a.lower)*0.1));
            }
            if (param_.adapt) {
                initial_ = scale_;
                attempts_.resize(scale_.size()); accepts_.resize(scale_.size());
            }
        }
        // 数値提案を作る。Searchは最適化用、Warmup/Sampleは正しい提案比を返す。座標O(1)、集団操作・改訂直後O(d)。
        double operator()(const State& current, State& candidate, Rng& rng, const MoveContext& ctx) {
            assert(candidate.real.size() == domain_.real.size() && candidate.discrete.size() == domain_.discrete.size());
            if (revision_ != ctx.revision) {
                revision_ = ctx.revision; reverse_ = -1; next_discrete_.clear();
                std::fill(attempts_.begin(), attempts_.end(), 0);
                std::fill(accepts_.begin(), accepts_.end(), 0);
            }
            self_ = false;
            touched_ = -1;
            const int reverse = std::exchange(reverse_,-1);
            const double previous_step = std::exchange(last_step_,0);
            // Searchの棄却後、次も同じ鎖なら一度だけ反対方向を試す。
            if (ctx.phase == Phase::Search && reverse >= 0 && reverse_chain_ == ctx.chain) {
                touched_ = reverse;
                const auto k = static_cast<std::size_t>(reverse);
                if (k < candidate.real.size()) candidate.real[k] += previous_step;
                else candidate.discrete[k-candidate.real.size()] += static_cast<std::int64_t>(previous_step);
                return bounded(candidate,reverse) ? 0 : -INFINITY;
            }
            if (active_.empty()) return -INFINITY; // 全固定なら現状態を記録し、同じ評価を繰り返さない。
            const double difference = ctx.phase == Phase::Search ? 0 : param_.difference_probability;
            const double kind = rng.uniform();
            const double stretch = ctx.phase == Phase::Search ? 0 : param_.stretch_probability;
            // 他の1鎖を中心に伸縮し、動かす実数の次元数で提案比を補正する。
            if (kind < stretch && ctx.population.size() >= 2 && real_count_ > 0) {
                int a;
                do { a = rng.integer(static_cast<int>(ctx.population.size())); } while (a == static_cast<int>(ctx.chain));
                const double u = 1+rng.uniform(), z = u*u/2; // 密度は1/sqrt(z)、1/2<=z<2。
                for (int k = 0; k < real_count_; ++k) {
                    const auto j = static_cast<std::size_t>(active_[static_cast<std::size_t>(k)]);
                    const double partner = ctx.population[static_cast<std::size_t>(a)].real[j];
                    candidate.real[j] = partner + z*(candidate.real[j]-partner);
                    if (!bounded(candidate,static_cast<int>(j))) return -INFINITY;
                }
                return (real_count_-1)*std::log(z);
            }
            // 他の2鎖を順序付きで対称に選ぶ。対象鎖を含めず1鎖ずつ更新する。
            if (kind >= stretch && kind < stretch+difference && ctx.population.size() >= 3 && real_count_ > 0) {
                const int count = static_cast<int>(ctx.population.size());
                int a, b;
                do { a = rng.integer(count); } while (a == static_cast<int>(ctx.chain));
                do { b = rng.integer(count); } while (b == a || b == static_cast<int>(ctx.chain));
                const double gamma = 2.38 / std::sqrt(2.0*real_count_);
                for (int k = 0; k < real_count_; ++k) {
                    const auto j = static_cast<std::size_t>(active_[static_cast<std::size_t>(k)]);
                    candidate.real[j] += gamma*(ctx.population[static_cast<std::size_t>(a)].real[j]-ctx.population[static_cast<std::size_t>(b)].real[j]) + 0.001*scale_[j]*rng.normal();
                    if (!bounded(candidate, static_cast<int>(j))) return -INFINITY;
                }
                return 0;
            }
            // 動かす変数がすべて離散ならWarmup/Sampleで鎖ごとに巡回する。
            int coordinate;
            if (ctx.phase != Phase::Search && real_count_ == 0) {
                if (next_discrete_.size() <= ctx.chain) next_discrete_.resize(ctx.chain+1);
                auto& next = next_discrete_[ctx.chain];
                coordinate = next;
                if (++next == static_cast<int>(active_.size())) next = 0;
            } else coordinate = rng.integer(static_cast<int>(active_.size()));
            touched_ = active_[static_cast<std::size_t>(coordinate)];
            const auto k = static_cast<std::size_t>(touched_);
            auto step = [&] {
                double factor = 1;
                if (param_.cauchy && ctx.phase == Phase::Search) factor /= std::max(1e-100,std::abs(rng.normal()));
                return last_step_ = scale_[k]*factor*rng.normal();
            };
            if (k < candidate.real.size()) {
                candidate.real[k] += step();
                if (ctx.phase != Phase::Search && std::isfinite(candidate.real[k]) && !bounded(candidate,touched_)) { // 反射する幅の2倍もdoubleに収まること。
                    const auto& a = domain_.real[k]; auto& x = candidate.real[k];
                    if (std::isfinite(a.lower) && std::isfinite(a.upper)) {
                        const double w = a.upper-a.lower, v = std::fmod(x-a.lower,2*w);
                        x = a.lower+w-std::abs((v < 0 ? v+2*w : v)-w);
                    } else if (x < a.lower) x = 2*a.lower-x;
                    else if (x > a.upper) x = 2*a.upper-x;
                }
            }
            else {
                const auto i = k-candidate.real.size();
                const auto& a = domain_.discrete[i];
                if (a.categorical) {
                    touched_ = -1;
                    if (ctx.phase == Phase::Search || rng.uniform() < 0.9) { // Sampleは自己遷移の確率を残す。
                        const auto value = a.lower + rng.integer(static_cast<int>(a.upper-a.lower));
                        candidate.discrete[i] = value + (value >= candidate.discrete[i]);
                    } else candidate.discrete[i] = a.lower + rng.integer(static_cast<int>(a.upper-a.lower+1));
                    return candidate.discrete[i] == current.discrete[i] ? -INFINITY : 0;
                }
                const double change = step();
                if (std::abs(change) > 2e9) { last_step_ = 0; return -INFINITY; }
                last_step_ = static_cast<double>(std::llround(change));
                if (last_step_ == 0 && rng.uniform() < 0.9) last_step_ = std::copysign(1.0,change); // 丸めて0になる試行の90%を±1へ。
                self_ = last_step_ == 0;
                if (self_) return -INFINITY;
                candidate.discrete[i] += static_cast<std::int64_t>(last_step_);
            }
            return bounded(candidate,touched_) ? 0 : -INFINITY;
        }
        // 完了した試行を記録し、座標尺度を調整する。Sampleでは適応しない。O(1)
        void feedback(bool accepted, const MoveContext& ctx) {
            // 整数の自己遷移は評価を省くが、尺度適応では元通り受理として数える。
            if (self_) accepted = true;
            if (ctx.phase == Phase::Search && touched_ >= 0 && !accepted && last_step_ != 0) {
                reverse_ = touched_; reverse_chain_ = ctx.chain; last_step_ = -last_step_;
            }
            if (!param_.adapt || ctx.phase == Phase::Sample || touched_ < 0) return;
            const auto k = static_cast<std::size_t>(touched_);
            accepts_[k] += accepted;
            if (++attempts_[k] == 32) {
                scale_[k] *= std::exp(static_cast<double>(accepts_[k])/32-0.44);
                scale_[k] = std::clamp(scale_[k], initial_[k]*1e-9, initial_[k]*1000);
                attempts_[k] = accepts_[k] = 0;
            }
        }
    };

private:
    struct ZeroPrior { template<class... A> double operator()(const A&...) const { return 0; } };
    struct NoContext {};
    template<class Function, bool Symmetric>
    class SamplingMove {
        Function function_;
    public:
        static constexpr bool sampling_safe = true;
        explicit SamplingMove(Function function) : function_(std::move(function)) {}
        double operator()(const State& s, State& next, Rng& rng, const MoveContext& ctx) {
            if constexpr (Symmetric) {
                static_assert(std::is_same_v<decltype(function_(s,next,rng,ctx)),bool>);
                return function_(s,next,rng,ctx) ? 0 : -INFINITY;
            } else return function_(s,next,rng,ctx);
        }
        void feedback(bool accepted, const MoveContext& ctx) {
            if constexpr (requires { function_.feedback(accepted,ctx); }) function_.feedback(accepted,ctx);
        }
    };
public:
    // 実数・整数・カテゴリの初期値と範囲を一度に宣言する。追加時O(1)償却。
    // 戻り値はreal/discreteそれぞれの添字。カテゴリIDは[0,count)。構造変更にはSessionを再構築する。
    class Parameters {
        State initial_;
        Domain domain_;
    public:
        std::size_t real(double initial, double lower, double upper, double scale = 0) {
            assert(std::isfinite(initial) && lower <= initial && initial <= upper);
            if (scale == 0) {
                assert(std::isfinite(lower) && std::isfinite(upper));
                scale = lower == upper ? 1 : (upper-lower)*0.1;
            }
            assert(std::isfinite(scale) && scale > 0);
            initial_.real.push_back(initial); domain_.real.push_back({lower,upper,scale});
            return initial_.real.size()-1;
        }
        std::size_t integer(std::int64_t initial, std::int64_t lower, std::int64_t upper) {
            assert(lower <= initial && initial <= upper && upper-lower <= 1000000000);
            initial_.discrete.push_back(initial); domain_.discrete.push_back({lower,upper,false});
            return initial_.discrete.size()-1;
        }
        std::size_t category(std::int64_t initial, int count) {
            assert(count > 0);
            const auto i = integer(initial,0,count-1); domain_.discrete.back().categorical = true; return i;
        }
        const State& initial() const { return initial_; }
        const Domain& domain() const { return domain_; }
    };
    template<class Input> struct Observation {
        Input input;
        double value;
        double scale = 1; // Gaussian/Huberだけが使用する既知の誤差尺度。
        double weight = 1; // 尤度の重み。0なら損失0。総和を観測数で自動除算しない。
    };
    struct Gaussian {
    private:
        mutable double last_scale_ = 0, last_log_scale_ = 0; // 尺度の値が変われば再計算する。
    public:
        template<class O> double operator()(const State&, const O& row, double predicted) const {
            assert(row.weight >= 0 && std::isfinite(row.weight));
            if (row.weight == 0) return 0;
            assert(row.scale > 0 && std::isfinite(row.scale));
            if (row.scale != last_scale_) { last_scale_ = row.scale; last_log_scale_ = std::log(row.scale); }
            const double z = (row.value-predicted)/row.scale;
            return row.weight*(0.5*z*z+last_log_scale_);
        }
        static double mean(double predicted) { return predicted; }
    };
    struct Huber {
        double threshold = 1.345; // 固定値として使う。推定する場合は正規化定数も独自損失に含める。
        template<class O> double operator()(const State&, const O& row, double predicted) const {
            assert(row.weight >= 0 && std::isfinite(row.weight));
            assert(threshold > 0 && std::isfinite(threshold) && row.scale > 0 && std::isfinite(row.scale));
            if (row.weight == 0) return 0;
            const double z = std::abs((row.value-predicted)/row.scale);
            return row.weight*((z <= threshold ? 0.5*z*z : threshold*(z-0.5*threshold))+std::log(row.scale));
        }
        static double mean(double predicted) { return predicted; }
    };
    struct BernoulliLogit {
        template<class O> double operator()(const State&, const O& row, double logit) const {
            assert((row.value == 0 || row.value == 1) && row.weight >= 0 && std::isfinite(row.weight));
            assert(std::isfinite(logit));
            return row.weight == 0 ? 0 : row.weight*((row.value == 0 ? std::max(logit,0.0) : std::max(-logit,0.0))
                + std::log1p(std::exp(-std::abs(logit))));
        }
        static double mean(double logit) {
            const double e = std::exp(-std::abs(logit)); return logit >= 0 ? 1/(1+e) : e/(1+e);
        }
    };
    struct PoissonLogMean {
        template<class O> double operator()(const State&, const O& row, double log_mean) const {
            assert(row.value >= 0 && std::isfinite(row.value) && std::floor(row.value) == row.value);
            assert(row.weight >= 0 && std::isfinite(row.weight) && std::isfinite(log_mean));
            return row.weight == 0 ? 0 : row.weight*(std::exp(log_mean)-row.value*log_mean);
        }
        static double mean(double log_mean) { return std::exp(log_mean); }
    };

    template<class Observation, class Predict, class Loss, class Prior, class Context = NoContext>
    class ObservationModel {
        friend class McmcEstimator;
        template<class, class> friend class Session;
        using ObservationTag = void;
        std::vector<Observation> data_;
        Predict predict_;
        Loss loss_;
        Prior prior_;
        [[no_unique_address]] Context context_;
        template<class Self, class Input>
        static decltype(auto) predict(Self& self, const Input& input, const State& state) {
            if constexpr (std::is_same_v<Context,NoContext>) return self.predict_(input,state);
            else return self.predict_(self.context_,input,state);
        }
        double term(const State& state, std::size_t i) {
            if (i == 0) {
                if constexpr (std::is_same_v<Context,NoContext>) return prior_(state);
                else return prior_(context_,state);
            }
            const auto& row = data_[i-1];
            return loss_(state, row, predict(*this,row.input,state));
        }
    public:
        // 独立な観測列と関数を所有する。O(n)、vector移動時はO(1)。
        using observation_type = Observation;
        ObservationModel(std::vector<Observation> data, Predict predictor, Loss loss, Prior prior, Context context = {})
            : data_(std::move(data)), predict_(std::move(predictor)), loss_(std::move(loss)), prior_(std::move(prior)), context_(std::move(context)) {}
        // 観測全体を評価する。O(全観測の予測・損失計算)
        double operator()(const State& state) {
            double sum = 0;
            for (std::size_t i = 0; i <= data_.size(); ++i) {
                const double e = term(state, i); check_energy(e); sum += e;
                if (!std::isfinite(sum)) { check_energy(sum); break; }
            }
            return sum;
        }
        // 観測件数を返す。O(1)
        std::size_t size() const { return data_.size(); }
        // 観測の読み取り専用ビューを返す。更新まで有効。O(1)
        std::span<const Observation> observations() const { return data_; }
        // 生の予測値と、標準損失のリンク変換後の平均。モデルを変更しない予測関数で使う。
        template<class Input> decltype(auto) prediction(const Input& input, const State& state) const { return predict(*this,input,state); }
        template<class Input> auto mean_prediction(const Input& input, const State& state) const
            requires requires { loss_.mean(prediction(input,state)); } { return loss_.mean(prediction(input,state)); }
        // 所有Contextを更新する場合はSession::update_modelのコールバック内でのみ変更する。
        Context& context() { return context_; }
        const Context& context() const { return context_; }
    };

    // 数値提案の補助を作る。O(d)
    static NumericMove make_numeric_move(Domain domain, MoveParam param = {}) { return NumericMove(std::move(domain), param); }
    // 独立観測の評価関数を合成する。O(n)、観測vectorの移動時はO(1)。
    template<class O, class P, class L = Gaussian, class R = ZeroPrior>
    static auto make_observation_model(std::vector<O> data, P predict, L loss = {}, R prior = {}) {
        return ObservationModel<O,P,L,R>(std::move(data),std::move(predict),std::move(loss),std::move(prior));
    }
    // Contextも値として所有する。予測は(context,input,state)、事前項は(context,state)。
    template<class O, class C, class P, class L = Gaussian, class R = ZeroPrior>
    static auto make_context_model(std::vector<O> data, C context, P predict, L loss = {}, R prior = {}) {
        return ObservationModel<O,P,L,R,C>(std::move(data),std::move(predict),std::move(loss),std::move(prior),std::move(context));
    }
    // 対称性は利用側が保証する。関数はboolを返す(true:候補を評価、false:棄却)。
    // 独自提案のfeedback等はSample中に提案分布を適応させないこと。
    template<class F> static auto symmetric_move(F move) { return SamplingMove<F,true>(std::move(move)); }
    // 関数はlog(q(旧|新)/q(新|旧))を返す。提案比の正しさは利用側が保証する。
    template<class F> static auto hastings_move(F move) { return SamplingMove<F,false>(std::move(move)); }
    template<class Model, class Move>
    static auto make_session(std::vector<State> initial, Model model, Move move, Param param = {}) {
        return Session<Model,Move>(std::move(initial),std::move(model),std::move(move),param);
    }
    // chains個の鎖を同じ初期値で開始する。異なる初期値はrestartまたはmake_sessionで指定する。
    template<class Model>
    static auto make_numeric_session(const Parameters& parameters, Model model, Param param = {}, MoveParam move = {}, std::size_t chains = 1) {
        assert(chains > 0);
        return make_session(std::vector<State>(chains,parameters.initial()),std::move(model),make_numeric_move(parameters.domain(),move),param);
    }
    // 正規観測の負の対数尤度を定数項を除いて返す。O(1)。sigma>0。
    static double gaussian_loss(double observed, double predicted, double sigma) {
        assert(sigma > 0 && std::isfinite(sigma));
        const double z = (observed-predicted)/sigma;
        return 0.5*z*z + std::log(sigma);
    }
    // 設定と乱数を初期化する。O(1)
    explicit McmcEstimator(Param param = {}) : param_(param), rng_(param.seed) {
        assert(param.sample_stride > 0);
        assert(param.observation_check_interval > 0);
        assert(param.start_temperature > 0 && param.end_temperature > 0);
        assert(std::isfinite(param.start_temperature) && std::isfinite(param.end_temperature));
    }
    // 初期候補をコピー・評価し履歴を消去する。O(C*(d+評価時間))。
    template<class Evaluate>
    Report reset(std::span<const State> initial, Evaluate&& evaluate, Budget& budget) {
        const auto start = Clock::now();
        initialize(std::vector<State>(initial.begin(),initial.end()));
        Report report; synchronize(evaluate,budget,report);
        return finish(report,start);
    }
    // 目的関数変更後、以前の最良候補を先頭に再評価する。O((C+1)*評価時間)。
    template<class Evaluate>
    Report refresh(Evaluate&& evaluate, Budget& budget) {
        const auto start = Clock::now(); assert(!states_.empty());
        invalidate(); Report report; synchronize(evaluate,budget,report);
        return finish(report,start);
    }
    // 原子的なdeltaで正確な増分を加算。中断後はrunの全評価で残りを補う。O((C+1)*差分時間)。
    template<class Delta>
    Report append_energy(Delta&& delta, Budget& budget) {
        const auto start = Clock::now(); assert(synced() && best_);
        auto evaluate_delta = [&](const State& s) { return delta(s); };
        invalidate(); Report report; synchronize(evaluate_delta,budget,report,true);
        return finish(report,start);
    }
    // 観測を一度だけ追加。空なら履歴を保つ。償却O(batchサイズ+(C+1)*追加評価時間)、必要時は全評価。
    template<class Model>
    Report observe(Model& model, std::vector<typename Model::observation_type> batch, Budget& budget) {
        const auto start = Clock::now(); assert(!states_.empty());
        if (!batch.empty()) {
            const bool can_add = synced() && best_.has_value();
            const std::size_t first = model.data_.size()+1;
            model.data_.insert(model.data_.end(),std::make_move_iterator(batch.begin()),std::make_move_iterator(batch.end()));
            invalidate(); if (can_add) append_begin_ = first;
        }
        Report report; synchronize(model,budget,report);
        return finish(report,start);
    }
    // 観測を置換し全再評価する。O(n+C*評価時間)。
    template<class Model>
    Report replace_observations(Model& model, std::vector<typename Model::observation_type> data, Budget& budget) {
        const auto start = Clock::now(); assert(!states_.empty());
        model.data_ = std::move(data); invalidate();
        Report report; synchronize(model,budget,report);
        return finish(report,start);
    }
    // 現行の目的関数で評価済みの最良候補を返す。次の実行・更新まで有効。O(1)。
    Best best() const { return {best_ ? &*best_ : nullptr,best_energy_}; }

    // 指定区間を継続実行する。O(m*(状態コピー+提案+評価)+出力)。未完了時は同じモデル・提案を再度渡す。
    template<class Evaluate, class Propose, class Emit>
    Report run(Phase phase, Evaluate&& evaluate, Propose&& propose, Budget& budget, Emit&& emit) {
        const auto start = Clock::now(); assert(!states_.empty());
        Report report; synchronize(evaluate,budget,report);
        // 実行しないモード変更で未完了の試行・出力を捨てない。
        if (!synced() || !best_ || (phase_ != phase && !budget.available())) return finish(report,start);
        if (phase_ != phase) {
            phase_ = phase; trial_pending_ = false; emit_pending_ = false;
            if (phase == Phase::Sample) std::fill(samples_.begin(),samples_.end(),0);
        }
        if (phase == Phase::Search && !search_started_ && budget.available()) {
            search_started_ = true; search_start_ = start;
            search_end_ = param_.cooling_deadline;
            search_steps_ = param_.cooling_steps;
            if (search_end_ == Clock::time_point::max() && search_steps_ == 0) {
                search_end_ = budget.deadline;
                if (search_end_ == Clock::time_point::max()) search_steps_ = budget.remaining_steps;
            }
        }
        // 時間切れで未出力の標本は次の呼び出しの先頭で出す。
        auto output = [&] {
            if (emit_pending_ && budget.time_left()) {
                emit(states_[emit_chain_],energy_[emit_chain_],static_cast<int>(emit_chain_));
                emit_pending_ = false; ++report.emitted;
            }
        };
        output();
        while (!emit_pending_ && budget.available()) {
            const auto i = next_chain_;
            if (!trial_pending_) {
                trial_ = states_[i]; trial_temperature_ = phase == Phase::Search ? temperature() : 1;
                const MoveContext ctx{phase,trial_temperature_,i,states_,revision_};
                trial_ratio_ = propose(states_[i],*trial_,rng_,ctx);
                assert(!std::isnan(trial_ratio_) && trial_ratio_ != INFINITY);
                trial_log_u_ = rng_.log_uniform(); trial_progress_ = {}; trial_pending_ = true;
            }
            double candidate_energy = INFINITY;
            if (trial_ratio_ != -INFINITY) {
                // 任意の純粋な差分評価を利用できる。共有状態・キャッシュの採用更新は行わない。
                if constexpr (requires { evaluate.candidate(states_[i],*trial_,energy_[i]); }) {
                    auto delta = [&](const State& next) { return evaluate.candidate(states_[i],next,energy_[i]); };
                    if (!evaluate_some(delta,*trial_,budget,report,trial_progress_)) break;
                } else if (!evaluate_some(evaluate,*trial_,budget,report,trial_progress_)) break;
                candidate_energy = trial_progress_.sum;
            }
            update_best(*trial_,candidate_energy);
            const double log_alpha = -(candidate_energy-energy_[i])/trial_temperature_ + trial_ratio_;
            const bool accepted = trial_log_u_ <= log_alpha;
            if (accepted) { std::swap(states_[i],*trial_); energy_[i] = candidate_energy; ++report.accepted; }
            const MoveContext ctx{phase,trial_temperature_,i,states_,revision_};
            if constexpr (requires { propose.feedback(accepted,ctx); }) propose.feedback(accepted,ctx);
            trial_pending_ = false; ++report.steps; --budget.remaining_steps;
            if (phase == Phase::Search) ++search_done_;
            if (phase == Phase::Sample && ++samples_[i] % param_.sample_stride == 0) {
                emit_pending_ = true; emit_chain_ = i;
            }
            next_chain_ = (i+1)%states_.size(); output();
        }
        return finish(report,start);
    }
    // 出力を収集せずに指定区間を実行する。O(m*(状態コピー+提案+評価))。
    template<class Evaluate, class Propose>
    Report run(Phase phase, Evaluate&& evaluate, Propose&& propose, Budget& budget) {
        return run(phase,evaluate,propose,budget,[](const State&,double,int) {});
    }

private:
    struct Progress { std::size_t next = 0; double sum = 0; };
    Param param_;
    Rng rng_;
    std::vector<State> states_;
    std::vector<double> energy_;
    std::vector<std::uint64_t> samples_;
    std::optional<State> best_, priority_, trial_;
    double best_energy_ = INFINITY, priority_energy_ = INFINITY;
    std::ptrdiff_t sync_pos_ = 0;
    std::optional<std::size_t> append_begin_;
    Progress sync_progress_, trial_progress_;
    std::size_t next_chain_ = 0, emit_chain_ = 0;
    std::uint64_t revision_ = 0, search_done_ = 0, search_steps_ = 0;
    std::optional<Phase> phase_;
    bool trial_pending_ = false, emit_pending_ = false, search_started_ = false;
    double trial_temperature_ = 1, trial_ratio_ = 0, trial_log_u_ = 0;
    Clock::time_point search_start_, search_end_;

    void initialize(std::vector<State> initial) {
        assert(!initial.empty()); states_ = std::move(initial);
        energy_.assign(states_.size(),INFINITY); samples_.assign(states_.size(),0);
        best_.reset(); priority_.reset(); trial_.reset(); next_chain_ = 0;
        rng_ = Rng(param_.seed); invalidate();
    }

    static void check_energy(double value) { assert(!std::isnan(value) && value != -INFINITY); (void)value; }
    bool synced() const { return sync_pos_ == static_cast<std::ptrdiff_t>(states_.size()); }
    // best_が空ならbest_energy_は+inf。評価の-inf/NaNは契約外。
    void update_best(const State& s, double e) {
        if (e < best_energy_) { best_ = s; best_energy_ = e; }
    }
    void invalidate() {
        if (best_) { priority_ = std::move(best_); priority_energy_ = best_energy_; }
        best_.reset(); best_energy_ = INFINITY;
        sync_pos_ = priority_ ? -1 : 0; append_begin_.reset();
        sync_progress_ = {}; trial_pending_ = emit_pending_ = false;
        search_started_ = false; search_done_ = 0; phase_.reset(); ++revision_;
    }
    template<class Evaluate>
    bool evaluate_some(Evaluate& eval, const State& s, Budget& b, Report& r, Progress& p) {
        if (!b.available()) return false;
        if constexpr (requires { typename std::remove_cvref_t<Evaluate>::ObservationTag; }) {
            while (p.next <= eval.data_.size()) {
                const auto end = p.next+std::min(param_.observation_check_interval,eval.data_.size()+1-p.next);
                do {
                    const double value = eval.term(s,p.next++); ++r.terms; check_energy(value);
                    p.sum += value; check_energy(p.sum);
                    if (!std::isfinite(p.sum)) break;
                } while (p.next < end);
                if (!std::isfinite(p.sum) || p.next > eval.data_.size()) break;
                if (!b.available()) return false;
            }
        } else {
            p.sum = eval(s); ++r.terms; check_energy(p.sum);
        }
        ++r.evaluations; return true;
    }
    template<class Evaluate>
    void synchronize(Evaluate& eval, Budget& b, Report& r, bool delta = false) {
        if (synced()) return;
        while (!synced()) {
            const bool first = sync_pos_ < 0;
            const auto i = static_cast<std::size_t>(std::max<std::ptrdiff_t>(0,sync_pos_));
            const State& state = first ? *priority_ : states_[i];
            const double old = first ? priority_energy_ : energy_[i];
            if (sync_progress_.next == 0 && append_begin_) {
                sync_progress_ = {*append_begin_,old};
            }
            if (!evaluate_some(eval,state,b,r,sync_progress_)) return;
            double value = sync_progress_.sum;
            if (delta) { assert(std::isfinite(old)); value += old; check_energy(value); }
            if (!first) energy_[i] = value;
            update_best(state,value); ++sync_pos_; sync_progress_ = {};
        }
        append_begin_.reset(); priority_.reset();
        if (best_) for (std::size_t i = 0; i < states_.size(); ++i) {
            if (!std::isfinite(energy_[i])) { states_[i] = *best_; energy_[i] = best_energy_; }
        }
    }
    double temperature() const {
        double fraction = 0;
        if (search_end_ != Clock::time_point::max()) {
            const double span = std::chrono::duration<double>(search_end_-search_start_).count();
            fraction = span > 0 ? std::chrono::duration<double>(Clock::now()-search_start_).count()/span : 1;
        } else if (search_steps_ > 0) fraction = static_cast<double>(search_done_)/static_cast<double>(search_steps_);
        return param_.start_temperature*std::exp(std::clamp(fraction,0.0,1.0)*std::log(param_.end_temperature/param_.start_temperature));
    }
    Report finish(Report r, Clock::time_point start) const {
        r.synchronized = synced(); r.has_estimate = best_.has_value();
        if (synced() && !best_) r.stop = Stop::NoFiniteState;
        r.elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();
        return r;
    }
};

// 値所有の通常API。コールバックの参照キャプチャ先まで所有するものではない。
// 更新は即時に登録し、評価は次のsolve/sampleの予算で行う。単スレッド、実行中の再入・更新は禁止。
template<class State>
template<class Model, class Move>
class McmcEstimator<State>::Session {
    McmcEstimator engine_;
    Model model_;
    Move move_;
    std::uint64_t warmup_target_ = 0, warmup_left_ = 0;
    bool queued_ = false;
    static constexpr bool can_sample = [] {
        if constexpr (requires { Move::sampling_safe; }) return Move::sampling_safe;
        else return false;
    }();
    void changed(bool append = false, std::size_t first = 0) {
        if (!queued_) {
            const bool incremental = append && engine_.synced() && engine_.best_.has_value();
            engine_.invalidate();
            if (incremental) engine_.append_begin_ = first;
        } else {
            ++engine_.revision_;
            // 追加以外の変更では、開始済みの部分和も破棄する。
            if (!append) { engine_.append_begin_.reset(); engine_.sync_progress_ = {}; }
        }
        queued_ = true; warmup_left_ = warmup_target_;
    }
public:
    struct Result {
        std::optional<State> state; // 今の目的関数で評価済みの最良候補を所有する。未評価ならnullopt。
        double energy = INFINITY;
        Report report;
        std::uint64_t revision = 0;
    };
    // double / array<double,N> / vector<double>の逐次集計。標本自体は保存しない。
    // varianceはn-1で割った標本分散。平均の推定誤差や独立標本数ではない。
    template<class Value> class Moments {
        std::uint64_t count_ = 0;
        std::optional<Value> mean_, m2_;
        static std::size_t size(const Value& value) {
            if constexpr (std::is_same_v<Value,double>) { (void)value; return 1; }
            else { static_assert(std::is_same_v<typename Value::value_type,double>); return value.size(); }
        }
        template<class V> static decltype(auto) at(V& value, std::size_t i) {
            if constexpr (std::is_same_v<Value,double>) { (void)i; return (value); }
            else return (value[i]);
        }
    public:
        void add(const Value& value) {
            if (count_ == 0) {
                mean_ = m2_ = value;
                for (std::size_t i = 0; i < size(value); ++i) { assert(std::isfinite(at(value,i))); at(*m2_,i) = 0; }
            } else {
                assert(size(value) == size(*mean_));
                const double reciprocal = 1/static_cast<double>(count_+1);
                for (std::size_t i = 0; i < size(value); ++i) {
                    const double x = at(value,i), delta = x-at(*mean_,i); assert(std::isfinite(x));
                    at(*mean_,i) += delta*reciprocal;
                    at(*m2_,i) += delta*(x-at(*mean_,i));
                }
            }
            ++count_;
        }
        std::uint64_t count() const { return count_; }
        const std::optional<Value>& mean() const { return mean_; }
        std::optional<Value> variance() const {
            if (count_ < 2) return std::nullopt;
            auto v = *m2_;
            for (std::size_t i = 0; i < size(v); ++i) at(v,i) /= static_cast<double>(count_-1);
            return v;
        }
    };
    template<class Value> struct SampleResult {
        Moments<Value> summary;
        Report report;
        std::uint64_t revision, warmup_remaining;
    };

    // 初期状態を所有する。目的関数の初回評価はsolve/sampleまで実行しない。
    Session(std::vector<State> initial, Model model, Move move, Param param = {})
        : engine_(param), model_(std::move(model)), move_(std::move(move)) {
        restart(std::move(initial));
    }
    // 同じ構造の初期状態・鎖数を変更し、乱数と探索履歴を初期化する。数値提案の学習済み尺度は保持。
    // 次元・カテゴリの意味が変わる場合は、モデルと提案を含めてSessionを作り直す。
    void restart(std::vector<State> initial) {
        if constexpr (requires (const State& s) { move_.validate(s); })
            for (const auto& s : initial) move_.validate(s);
        engine_.initialize(std::move(initial)); queued_ = true;
        if (engine_.param_.warmup_steps) warmup_target_ = *engine_.param_.warmup_steps;
        else if constexpr (requires { std::as_const(move_).warmup_size(engine_.states_.size()); })
            warmup_target_ = std::as_const(move_).warmup_size(engine_.states_.size());
        else warmup_target_ = 512*engine_.states_.size();
        warmup_left_ = warmup_target_;
    }
    const Model& model() const { return model_; }
    Best best() const { return engine_.best(); } // 次の実行・変更まで有効なビュー。
    std::uint64_t revision() const { return engine_.revision_; }
    std::uint64_t warmup_remaining() const { return warmup_left_; }

    // コールバックでモデル/所有Contextを編集し、全候補の評価を自動的に無効化する。
    // 観測内の過去の環境は利用側が値または不変IDで保持する。現在Contextへの自動置換は行わない。
    template<class Edit> void update_model(Edit&& edit) { edit(model_); changed(); }
    template<class M = Model> requires std::is_same_v<M,Model>
    void add_observations(std::vector<typename M::observation_type> batch) {
        if (batch.empty()) return;
        const auto first = model_.data_.size()+1;
        model_.data_.insert(model_.data_.end(),std::make_move_iterator(batch.begin()),std::make_move_iterator(batch.end()));
        changed(true,first);
    }
    template<class M = Model> requires std::is_same_v<M,Model>
    void replace_observations(std::vector<typename M::observation_type> data) { model_.data_ = std::move(data); changed(); }
    void retain_last(std::size_t count) requires requires { typename Model::observation_type; } {
        if (count >= model_.data_.size()) return;
        model_.data_.erase(model_.data_.begin(),model_.data_.end()-static_cast<std::ptrdiff_t>(count)); changed();
    }
    // Budgetは値渡し。同期・探索を同じ期限内で実行する。旧最良候補も現行モデルで再評価する。
    // Reportはこの呼び出しの総計。stepsは完了遷移数で、初期評価/再評価を含めない。
    Report solve_view(Budget budget) {
        if (!budget.available()) return engine_.finish({},Clock::now());
        queued_ = false;
        auto report = engine_.run(Phase::Search,model_,move_,budget);
        if (report.steps != 0) warmup_left_ = warmup_target_;
        return report;
    }
    Result solve(Budget budget) {
        const auto start = Clock::now();
        auto report = solve_view(budget);
        Result result{engine_.best_,engine_.best_energy_,report,revision()};
        result.report.elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();
        return result;
    }
    // 準備遷移は目的関数ごとに通算し、複数回の小予算でも先に進む。Search実行後は再準備する。
    // emit(state,energy,chain)はSampleの標本だけを受け取る。棄却時も現状態を記録する。
    // 外部に累積する場合、目的関数revisionと予測条件の異なる集計を混ぜないこと。
    // 実行時コールバックはコピー・保存しない。参照先の寿命は呼び出し中だけ必要。
    template<class Emit> Report sample_each(Budget budget, Emit&& emit) requires(can_sample) {
        const auto start = Clock::now(); Report report;
        if (budget.available()) {
            queued_ = false;
            if (warmup_left_ != 0) {
                auto warm = budget; warm.remaining_steps = std::min(warm.remaining_steps,warmup_left_);
                report = engine_.run(Phase::Warmup,model_,move_,warm);
                warmup_left_ -= report.steps; budget.remaining_steps -= report.steps;
            }
        }
        if (warmup_left_ == 0) {
            const auto part = engine_.run(Phase::Sample,model_,move_,budget,emit);
            report.steps += part.steps; report.accepted += part.accepted; report.evaluations += part.evaluations;
            report.terms += part.terms; report.emitted += part.emitted;
        }
        return engine_.finish(report,start);
    }
    // 集計は呼び出しごとに新規。project(state)はdouble/array/vectorを返す。
    // 非線形予測は状態ごとにprojectで計算する。標本0件ならmean/varianceはnullopt。
    template<class Project> auto sample(Budget budget, Project&& project) requires(can_sample) {
        using Value = std::remove_cvref_t<std::invoke_result_t<Project&,const State&>>;
        Moments<Value> summary;
        const auto report = sample_each(budget,[&](const State& state,double,int) { summary.add(project(state)); });
        return SampleResult<Value>{std::move(summary),report,revision(),warmup_left_};
    }
};
#if __INCLUDE_LEVEL__ == 0
// 単体コンパイル時だけ実行するテスト。提出コードには含まれない。
struct McmcEstimatorTests {
    using S = McmcEstimator<>;
    int checks = 0;
    void check(bool ok, const char* message) {
        ++checks;
        if (!ok) { std::cerr << "FAILED: " << message << '\n'; std::abort(); }
    }
    static bool same(const McmcState& a, const McmcState& b) { return a.real == b.real && a.discrete == b.discrete; }
    void rng() {
        S::Rng a(0), b(0);
        for (int i = 0; i < 100; ++i) check(a.next() == b.next(),"rng seed");
        double sum = 0, square = 0;
        std::array<int,7> hist{};
        for (int i = 0; i < 200000; ++i) {
            const double x = a.normal(); sum += x; square += x*x;
            const double u = a.uniform(), l = a.log_uniform();
            check(u >= 0 && u < 1 && std::isfinite(l) && l <= 0,"rng range");
            ++hist[static_cast<std::size_t>(a.integer(7))];
        }
        check(std::abs(sum/200000) < 0.012 && std::abs(square/200000-1) < 0.018,"normal moments");
        for (int count : hist) check(std::abs(count-200000.0/7) < 700,"integer uniform");
    }
    void generic_and_asymmetric() {
        using G = McmcEstimator<int>;
        G solver({.seed=71,.sample_stride=1});
        std::vector<int> initial{0};
        const double weights[3]{0.1,0.3,0.6};
        const double q[3][3]{{0.1,0.2,0.7},{0.6,0.3,0.1},{0.2,0.5,0.3}};
        auto energy = [&](const int& x) { return -std::log(weights[x]); };
        auto move = [&](const int& x,int& y,G::Rng& r,const G::MoveContext&) {
            const double u = r.uniform(); y = u < q[x][0] ? 0 : (u < q[x][0]+q[x][1] ? 1 : 2);
            return std::log(q[y][x]/q[x][y]);
        };
        auto budget = G::Budget::for_steps(400000);
        solver.reset(initial,energy,budget);
        std::array<int,3> visits{}; int transitions[3][3]{}; int previous = 0;
        const auto report = solver.run(G::Phase::Sample,energy,move,budget,[&](const int& x,double e,int index) {
            check(e == energy(x) && index == 0,"generic emit");
            ++visits[static_cast<std::size_t>(x)]; ++transitions[previous][x]; previous = x;
        });
        check(report.steps == 400000 && report.emitted == 400000,"sample counts");
        for (int i = 0; i < 3; ++i) {
            check(std::abs(visits[static_cast<std::size_t>(i)]/400000.0-weights[i]) < 0.008,"asymmetric stationary mass");
            double row_sum = 0;
            for (int j = 0; j < 3; ++j) if (i != j) {
                const double pij = q[i][j]*std::min(1.0,weights[j]*q[j][i]/(weights[i]*q[i][j]));
                const double pji = q[j][i]*std::min(1.0,weights[i]*q[i][j]/(weights[j]*q[j][i]));
                check(std::abs(weights[i]*pij-weights[j]*pji) < 1e-14,"detailed balance");
                const int total = transitions[i][0]+transitions[i][1]+transitions[i][2];
                check(std::abs(static_cast<double>(transitions[i][j])/total-pij) < 0.012,"implemented transition matrix");
                row_sum += pij;
            }
            check(row_sum <= 1,"transition row");
        }
        // 棄却した候補でも正確な評価があれば最良候補にできる。
        auto e = [](const int& x) { return static_cast<double>(x*x); };
        auto reject = [](const int&,int& y,G::Rng&,const G::MoveContext&) { y=0; return -1000.0; };
        initial={10}; budget=G::Budget::for_steps(1); solver.reset(initial,e,budget);
        int emitted = -1;
        solver.run(G::Phase::Sample,e,reject,budget,[&](const int& x,double,int) { emitted=x; });
        check(*solver.best().state == 0 && emitted == 10,"best differs from chain");
    }
    void gaussian_and_numeric() {
        check(std::abs(S::gaussian_loss(2,0,2)-(0.5+std::log(2.0))) < 1e-14,"sigma normalization");
        S::Domain domain{{{-INFINITY,INFINITY,1},{-INFINITY,INFINITY,1}}, {}};
        auto move = S::make_numeric_move(domain,{.adapt=true,.difference_probability=0.25});
        std::vector<McmcState> initial{{{2,3},{}},{{-2,-3},{}},{{1,-1},{}},{{-1,1},{}}};
        auto eval = [](const McmcState& x) { return (x.real[0]*x.real[0]-1.8*x.real[0]*x.real[1]+x.real[1]*x.real[1])/(2*0.19); };
        S solver({.seed=124}); auto budget = S::Budget::for_steps(30000);
        solver.reset(initial,eval,budget); solver.run(S::Phase::Warmup,eval,move,budget);
        budget=S::Budget::for_steps(400000); double sx=0,sy=0,xx=0,xy=0;
        solver.run(S::Phase::Sample,eval,move,budget,[&](const McmcState& x,double,int) {
            sx+=x.real[0]; sy+=x.real[1]; xx+=x.real[0]*x.real[0]; xy+=x.real[0]*x.real[1];
        });
        check(std::abs(sx/400000) < 0.07 && std::abs(sy/400000) < 0.07,"gaussian mean");
        check(std::abs(xx/400000-1) < 0.10 && std::abs(xy/400000-0.9) < 0.10,"gaussian covariance");
        domain={{{-2,2,0.8},{7,7,1}},{{0,6,false},{0,3,true},{5,5,false}}};
        move=S::make_numeric_move(domain,{.adapt=true,.difference_probability=0.25});
        initial.assign(4,McmcState{{0,7},{3,1,5}});
        auto mixed = [](const McmcState& x) { return x.real[0]*x.real[0]+std::abs(static_cast<double>(x.discrete[0])-2); };
        budget=S::Budget::for_steps(15000); solver.reset(initial,mixed,budget);
        solver.run(S::Phase::Sample,mixed,move,budget,[&](const McmcState& x,double e,int) {
            check(x.real[0]>=-2 && x.real[0]<=2 && x.real[1]==7,"real bounds and fixed");
            check(x.discrete[0]>=0 && x.discrete[0]<=6 && x.discrete[1]>=0 && x.discrete[1]<=3 && x.discrete[2]==5,"discrete bounds");
            check(e==mixed(x),"mixed energy");
        });
        domain={{{2,2,1}},{{3,3,true}}}; move=S::make_numeric_move(domain);
        initial={{{2},{3}}}; budget=S::Budget::for_steps(23); solver.reset(initial,mixed,budget);
        auto report=solver.run(S::Phase::Sample,mixed,move,budget);
        check(report.accepted==0 && report.emitted==23 && report.evaluations==0,"all fixed self transitions without reevaluation");
    }
    void continuation() {
        S::Domain domain{{{-8,8,2},{-8,8,0.1}}, {}};
        auto eval=[](const McmcState& x) { const double a=x.real[0]-1,b=x.real[1]+2; return a*a+4*b*b; };
        std::vector<McmcState> initial{{{3,3},{}},{{-3,-3},{}}};
        S a({.seed=42,.sample_stride=3,.cooling_steps=7000}),b({.seed=42,.sample_stride=3,.cooling_steps=7000});
        auto ma=S::make_numeric_move(domain),mb=S::make_numeric_move(domain);
        auto ba=S::Budget::for_steps(7000),bb=S::Budget::for_steps(1);
        a.reset(initial,eval,ba); b.reset(initial,eval,bb);
        a.run(S::Phase::Search,eval,ma,ba);
        for (int i=0;i<100;++i) { bb=S::Budget::for_steps(70); b.run(S::Phase::Search,eval,mb,bb); }
        check(same(*a.best().state,*b.best().state) && a.best().energy==b.best().energy,"search split exact");
        std::vector<McmcState> va,vb;
        auto ea=[&](const McmcState& x,double,int) {va.push_back(x);};
        auto eb=[&](const McmcState& x,double,int) {vb.push_back(x);};
        ba=S::Budget::for_steps(700); a.run(S::Phase::Sample,eval,ma,ba,ea);
        for (int i=0;i<100;++i) {bb=S::Budget::for_steps(7);b.run(S::Phase::Sample,eval,mb,bb,eb);}
        check(va.size()==vb.size(),"split stride size");
        for (std::size_t i=0;i<va.size();++i) check(same(va[i],vb[i]),"split sample exact");
        // 負の対数比-infは評価不要の棄却を表す。
        auto invalid=[](const McmcState&,McmcState&,S::Rng&,const S::MoveContext&) {return -INFINITY;};
        ba=S::Budget::for_steps(17); auto r=a.run(S::Phase::Sample,eval,invalid,ba);
        check(r.steps==17 && r.evaluations==0 && r.accepted==0,"invalid proposal");
    }
    struct Observation { double input, observed, sigma=1; };
    void observations_and_sync() {
        int predictions=0,priors=0;
        auto predict=[&](double input,const McmcState& x) {++predictions;return input*x.real[0];};
        auto loss=[](const McmcState&,const Observation& o,double y) {return S::gaussian_loss(o.observed,y,o.sigma);};
        double prior_weight=0.2;
        auto prior=[&](const McmcState& x) {++priors;return prior_weight*x.real[0]*x.real[0];};
        auto model=S::make_observation_model(std::vector<Observation>{{1,2},{2,4}},predict,loss,prior);
        S solver; std::vector<McmcState> initial{{{0},{}},{{1},{}},{{2},{}}};
        auto budget=S::Budget::for_steps(10); auto r=solver.reset(initial,model,budget);
        check(r.evaluations==3 && predictions==6 && priors==3 && r.synchronized,"model init calls");
        check(model.size()==2 && model.observations()[1].observed==4,"model read API");
        predictions=priors=0;
        r=solver.observe(model,std::vector<Observation>{{3,6}},budget);
        check(r.evaluations==4 && predictions==4 && priors==0,"automatic additive cost");
        check(std::abs(solver.best().energy-model(*solver.best().state))<1e-12,"additive matches full");
        budget=S::Budget::for_us(0);
        r=solver.observe(model,std::vector<Observation>{{4,8}},budget);
        check(model.size()==4 && !r.synchronized && !solver.best().state,"zero update registered");
        // さらに追加する場合は二重加算せず全評価へ戻す。
        r=solver.observe(model,std::vector<Observation>{{5,10}},budget);
        auto move=S::make_numeric_move(S::Domain{{{-10,10,1}}, {}});
        budget=S::Budget::for_steps(1); r=solver.run(S::Phase::Search,model,move,budget);
        check(r.synchronized && model.size()==5,"resume update once");
        check(std::abs(solver.best().energy-model(*solver.best().state))<1e-12,"overlapping updates exact");
        budget=S::Budget::for_steps(20);
        r=solver.replace_observations(model,std::vector<Observation>{{1,-2,0.5}},budget);
        check(model.size()==1 && std::abs(solver.best().energy-model(*solver.best().state))<1e-12,"replace full");
        prior_weight=3; solver.refresh(model,budget);
        check(solver.best().energy==model(*solver.best().state),"external prior refresh");
        // 現行評価済みの1候補は全鎖の同期前でも取り出せる。
        using G=McmcEstimator<int>; G g;
        std::vector<int> ini{1,2,3}; auto e=[](const int& x){return static_cast<double>(x*x);};
        auto bg=G::Budget::for_steps(1);g.reset(ini,e,bg);
        int calls=0; auto revised=[&](const int& x){if(++calls==1)bg.deadline=G::Clock::now();return e(x)+10;};
        auto rr=g.refresh(revised,bg);
        check(!rr.synchronized && rr.has_estimate && g.best().energy==11,"first current best");
        auto stay=[](const int&,int&,G::Rng&,const G::MoveContext&){return 0.0;};
        bg=G::Budget::for_steps(1);rr=g.run(G::Phase::Sample,revised,stay,bg);
        check(rr.synchronized && rr.emitted==1,"sync before sample");
        auto impossible=[](const int&){return INFINITY;};
        bg=G::Budget::for_steps(1);rr=g.refresh(impossible,bg);
        check(rr.stop==G::Stop::NoFiniteState && !g.best().state,"all invalid");
        bg=G::Budget::for_steps(1);rr=g.refresh(e,bg);
        check(rr.has_estimate && g.best().energy==1,"full refresh revives");
        auto delta=[](const int& x){return -0.5*x;};
        g.append_energy(delta,bg);
        check(std::abs(g.best().energy-0.5)<1e-12,"negative exact delta");
        bg=G::Budget::for_steps(1);calls=0;
        auto next_delta=[&](const int&x){if(++calls==1)bg.deadline=G::Clock::now();return 2.0*x;};
        rr=g.append_energy(next_delta,bg);
        check(!rr.synchronized && g.best().energy==2.5,"raw delta timeout current best");
        auto combined=[&](const int&x){return e(x)+1.5*x;};
        bg=G::Budget::for_steps(20);
        g.run(G::Phase::Sample,combined,stay,bg,[&](const int&x,double en,int){check(en==combined(x),"raw delta resume full");});
    }
    void interrupted_evaluation() {
        std::vector<Observation> rows;
        for(int i=1;i<=31;++i)rows.push_back({i/31.0,2*i/31.0});
        S::Budget* running=nullptr; int calls=0; bool sliced=false;
        auto predict=[&](double t,const McmcState& x){
            if(sliced && ++calls%7==0)running->deadline=S::Clock::now();
            return t*x.real[0];
        };
        auto loss=[](const McmcState&,const Observation&o,double y){return (y-o.observed)*(y-o.observed);};
        auto prior=[](const McmcState&){return 0.0;};
        auto model=S::make_observation_model(rows,predict,loss,prior);
        auto full=S::make_observation_model(rows,[](double t,const McmcState&x){return t*x.real[0];},loss,prior);
        S a({.seed=54,.observation_check_interval=1}),b({.seed=54,.observation_check_interval=1});
        auto ma=S::make_numeric_move(S::Domain{{{-5,5,1}}, {}}),mb=ma;
        std::vector<McmcState> initial{{{1},{}}};
        auto ba=S::Budget::for_steps(200),bb=S::Budget::for_steps(200);running=&bb;
        a.reset(initial,full,ba);b.reset(initial,model,bb);
        std::vector<double> va,vb;
        a.run(S::Phase::Sample,full,ma,ba,[&](const McmcState&x,double,int){va.push_back(x.real[0]);});
        sliced=true; std::uint64_t done=0;
        while(done<200 || vb.size()<va.size()) {
            bb=S::Budget::for_steps(200-done);
            auto r=b.run(S::Phase::Sample,model,mb,bb,[&](const McmcState&x,double,int){vb.push_back(x.real[0]);});
            done+=r.steps;
        }
        check(va==vb && a.best().energy==b.best().energy,"term pause resumes identical trial");
        // 中断途中でモデルを変えた場合は旧部分和と旧試行を破棄する。
        bb=S::Budget::for_steps(100); b.run(S::Phase::Warmup,model,mb,bb);
        sliced=false; bb=S::Budget::for_steps(100);
        b.observe(model,std::vector<Observation>{{1,-8}},bb);
        b.run(S::Phase::Sample,model,mb,bb,[&](const McmcState&x,double e,int){check(std::abs(e-model(x))<1e-10,"cancel old partial");});
        check(std::abs(b.best().energy-model(*b.best().state))<1e-10,"updated partial best");
    }
    void deferred_output_and_constraints() {
        using G=McmcEstimator<int>;G g;
        bool stop=false;auto b=G::Budget::for_steps(1);
        auto e=[&](const int& x){if(stop)b.deadline=G::Clock::now();return static_cast<double>(x*x);};
        std::vector<int> ini{1};g.reset(ini,e,b);
        auto move=[](const int&,int& y,G::Rng&,const G::MoveContext&){y=0;return 0.0;};
        stop=true;int emitted=0;
        auto out=[&](const int&,double,int){++emitted;};
        auto r=g.run(G::Phase::Sample,e,move,b,out);
        check(r.steps==1 && emitted==0,"deferred emission");
        b=G::Budget::for_steps(0);r=g.run(G::Phase::Sample,e,move,b,out);
        check(r.steps==0 && emitted==1,"flush completed sample");
        using P=McmcEstimator<std::array<int,5>>;P p;
        std::vector<std::array<int,5>> initial{{4,3,2,1,0}};
        auto residual=[](const std::array<int,5>&x){double v=0;for(int i=0;i<5;++i)v+=std::abs(x[static_cast<std::size_t>(i)]-i);return v;};
        auto swap=[](const std::array<int,5>&,std::array<int,5>&y,P::Rng&rng,const P::MoveContext&){std::swap(y[static_cast<std::size_t>(rng.integer(5))],y[static_cast<std::size_t>(rng.integer(5))]);return 0.0;};
        auto bp=P::Budget::for_steps(5000);p.reset(initial,residual,bp);p.run(P::Phase::Search,residual,swap,bp);
        check(p.best().energy==0,"permutation initialization");
        auto hard=[&](const std::array<int,5>&x){return residual(x)==0?0.0:INFINITY;};
        bp=P::Budget::for_steps(30);p.refresh(hard,bp);p.run(P::Phase::Warmup,hard,swap,bp);
        bp=P::Budget::for_steps(30);r={};
        const auto rp=p.run(P::Phase::Sample,hard,swap,bp,[&](const std::array<int,5>&x,double en,int){check(residual(x)==0 && en==0,"hard support sample");});
        check(rp.steps==30,"hard support transitions");
    }
    void candidate_difference() {
        using G=McmcEstimator<int>;
        struct Energy {
            int offset=0,candidates=0;
            double operator()(const int&x) const {return static_cast<double>((x-offset)*(x-offset));}
            double candidate(const int&old,const int&next,double current) {
                assert(current==(*this)(old));++candidates;
                return current+(next-old)*static_cast<double>(next+old-2*offset);
            }
        } energy;
        G a({.seed=93}),b({.seed=93});std::vector<int> initial{3,-5};
        auto raw=[&](const int&x){return energy(x);};
        auto move=[](const int&,int&next,G::Rng&r,const G::MoveContext&){next+=r.integer(7)-3;return 0.0;};
        auto ba=G::Budget::for_steps(1000),bb=ba;a.reset(initial,energy,ba);b.reset(initial,raw,bb);
        std::vector<int> va,vb;
        auto ea=[&](const int&x,double,int){va.push_back(x);};
        auto eb=[&](const int&x,double,int){vb.push_back(x);};
        a.run(G::Phase::Sample,energy,move,ba,ea);b.run(G::Phase::Sample,raw,move,bb,eb);
        check(va==vb && energy.candidates==1000,"pure delta identical transitions");
        energy.offset=8;ba=G::Budget::for_steps(1000);a.refresh(energy,ba);
        a.run(G::Phase::Search,energy,move,ba);check(a.best().energy==0,"pure delta after refresh");
    }
    void boundaries_and_random_updates() {
        check(!S::Budget::until(S::Clock::now()).available(),"absolute expired deadline");
        check(!S::Budget::for_steps(0).available(),"zero step budget");
        std::vector<Observation> data;
        auto pred=[](double x,const McmcState&s){return x*s.real[0]+s.real[1];};
        auto loss=[](const McmcState&,const Observation&o,double y){return S::gaussian_loss(o.observed,y,o.sigma);};
        auto prior=[](const McmcState&s){return 0.1*(s.real[0]*s.real[0]+s.real[1]*s.real[1]);};
        auto model=S::make_observation_model(data,pred,loss,prior);
        auto naive=[&](const McmcState&s){double e=prior(s);for(const auto&o:data)e+=loss(s,o,pred(o.input,s));return e;};
        S solver;auto proposal=S::make_numeric_move(S::Domain{{{-4,4,0.4},{-4,4,0.4}},{}});
        std::vector<McmcState> initial{{{1,2},{}},{{-1,-2},{}}};
        auto budget=S::Budget::for_steps(1);auto report=solver.reset(initial,model,budget);
        check(report.terms==2 && solver.best().energy==0.5,"zero observations prior only");
        S::Rng rng(194);
        for(int operation=0;operation<500;++operation) {
            budget=operation%11==0?S::Budget::for_us(0):S::Budget::for_steps(3);
            if(operation%9==0 && !data.empty()) {
                data.erase(data.begin()+rng.integer(static_cast<int>(data.size())));
                solver.replace_observations(model,data,budget);
            } else if(operation%7==0 && !data.empty()) {
                data[static_cast<std::size_t>(rng.integer(static_cast<int>(data.size())))].observed=rng.normal();
                solver.replace_observations(model,data,budget);
            } else {
                Observation o{rng.normal(),rng.normal(),0.5+rng.uniform()};data.push_back(o);
                solver.observe(model,std::vector<Observation>{o},budget);
            }
            if(auto best=solver.best();best.state)check(std::abs(best.energy-naive(*best.state))<1e-8,"random update current best");
            budget=S::Budget::for_steps(5);
            solver.run(S::Phase::Sample,model,proposal,budget,[&](const McmcState&s,double e,int){check(std::abs(e-naive(s))<1e-8,"random operation sample energy");});
            check(model.size()==data.size(),"random update data count");
        }
        // デフォルトコンストラクタのないユーザー型も利用できる。
        struct NoDefault {int value;explicit NoDefault(int x):value(x){}};
        using G=McmcEstimator<NoDefault>;G g;std::vector<NoDefault> start{NoDefault(3)};
        auto eval=[](const NoDefault&x){return static_cast<double>(x.value*x.value);};
        auto move=[](const NoDefault&,NoDefault&y,G::Rng&,const G::MoveContext&){y.value=0;return 0.0;};
        auto b=G::Budget::for_steps(1);g.reset(start,eval,b);g.run(G::Phase::Search,eval,move,b);
        check(g.best().energy==0,"non default state");
        // 推定値に応じた観測選択と、既知の経路情報の訂正。
        bool shortcut=true;double known_multiplier=1;
        struct Query {int input;double observed;};
        auto graph_predict=[&](int input,const McmcState&s){
            if(input)return s.real[static_cast<std::size_t>(input-1)];
            const double via=s.real[0]+s.real[1];
            return shortcut?std::min(via,known_multiplier*s.real[2]):via;
        };
        auto graph_loss=[](const McmcState&,const Query&q,double y){return (q.observed-y)*(q.observed-y);};
        auto graph_model=S::make_observation_model(std::vector<Query>{{0,2}},graph_predict,graph_loss,[](const McmcState&){return 0.0;});
        auto graph_move=S::make_numeric_move(S::Domain{{{0.1,5,0.5},{0.1,5,0.5},{0.1,5,0.5}},{}});
        const McmcState hidden{{1,2,2},{}};S graph_solver;
        std::vector<McmcState> graph_start{{{3,3,4},{}}};
        auto gb=S::Budget::for_steps(100);graph_solver.reset(graph_start,graph_model,gb);
        for(int turn=0;turn<16;++turn) {
            const auto&x=*graph_solver.best().state;
            const int query=1+static_cast<int>(std::max_element(x.real.begin(),x.real.end())-x.real.begin());
            gb=S::Budget::for_steps(100);
            graph_solver.observe(graph_model,std::vector<Query>{{query,graph_predict(query,hidden)}},gb);
            if(turn==8){shortcut=false;known_multiplier=2;graph_solver.refresh(graph_model,gb);}
            graph_solver.run(S::Phase::Search,graph_model,graph_move,gb);
            check(std::abs(graph_solver.best().energy-graph_model(*graph_solver.best().state))<1e-9,"adaptive input and graph refresh");
        }
    }
    void noop_updates_and_cooling(bool skip_cooling = false) {
        using G=McmcEstimator<int>;
        auto energy=[](const int& x){return static_cast<double>(x*x);};
        if (!skip_cooling) for (bool expired_time : {false,true}) {
            G g; std::vector<int> initial{1}; auto budget=G::Budget::for_steps(1);
            g.reset(initial,energy,budget);
            std::vector<double> temperatures;
            auto stay=[&](const int&,int&,G::Rng&,const G::MoveContext& ctx){temperatures.push_back(ctx.temperature);return 0.0;};
            budget=expired_time?G::Budget::for_us(0):G::Budget::for_steps(0);
            g.run(G::Phase::Search,energy,stay,budget);
            check(temperatures.empty(),"zero budget starts no trial");
            budget=G::Budget::for_steps(4);g.run(G::Phase::Search,energy,stay,budget);
            for (int i=0;i<4;++i)
                check(std::abs(temperatures[static_cast<std::size_t>(i)]-std::exp(i/4.0*std::log(0.001)))<1e-14,"zero budget preserves automatic cooling");
        }
        auto predict=[](double x,const McmcState&s){return x*s.real[0];};
        auto loss=[](const McmcState&,const Observation&o,double y){return (y-o.observed)*(y-o.observed);};
        auto model=S::make_observation_model(std::vector<Observation>{{1,2}},predict,loss,[](const McmcState&){return 0.0;});
        S g({.sample_stride=3});std::vector<McmcState> initial{{{1},{}}};auto b=S::Budget::for_steps(2);
        g.reset(initial,model,b);
        auto stay=[](const McmcState&,McmcState&,S::Rng&,const S::MoveContext&){return 0.0;};
        g.run(S::Phase::Sample,model,stay,b);
        const auto before=g.best();b=S::Budget::for_us(0);
        auto r=g.observe(model,std::vector<Observation>{},b);
        check(r.synchronized && r.evaluations==0 && g.best().state==before.state,"empty observe keeps current result");
        b=S::Budget::for_steps(1);r=g.run(S::Phase::Sample,model,stay,b);
        check(r.emitted==1,"empty observe keeps sample stride");
        // 更新待ちの空追加は、登録し直さず同期だけを継続する。
        b=S::Budget::for_us(0);g.observe(model,std::vector<Observation>{{2,5}},b);
        b=S::Budget::for_steps(1);r=g.observe(model,std::vector<Observation>{},b);
        check(r.synchronized && model.size()==2 && g.best().energy==model(*g.best().state),"empty observe resumes pending update");

        // 評価途中と未出力標本がある場合も空追加で捨てない。
        bool slice=false;int terms=0,proposals=0;
        auto slow_predict=[&](double x,const McmcState&s){if(slice && ++terms==1)b.deadline=S::Clock::now();return predict(x,s);};
        auto slow=S::make_observation_model(std::vector<Observation>{{1,2},{2,5}},slow_predict,loss,[](const McmcState&){return 0.0;});
        S h({.observation_check_interval=1});b=S::Budget::for_steps(1);h.reset(initial,slow,b);
        auto move=[&](const McmcState&,McmcState&x,S::Rng&,const S::MoveContext&){++proposals;x.real[0]=2;return 0.0;};
        slice=true;auto first=h.run(S::Phase::Sample,slow,move,b);
        check(first.steps==0 && proposals==1,"partial candidate prepared");
        h.observe(slow,std::vector<Observation>{},b);
        slice=false;b=S::Budget::for_steps(1);r=h.run(S::Phase::Sample,slow,move,b);
        check(r.steps==1 && r.terms==1 && proposals==1,"empty observe keeps partial trial");
        auto expiring=[&](const McmcState&x){b.deadline=S::Clock::now();return slow(x);};
        b=S::Budget::for_steps(1);r=h.run(S::Phase::Sample,expiring,move,b);
        check(r.steps==1 && r.emitted==0,"sample waiting for output");
        h.observe(slow,std::vector<Observation>{},b);
        b=S::Budget::for_steps(0);r=h.run(S::Phase::Sample,slow,move,b);
        check(r.steps==0 && r.emitted==1,"empty observe keeps deferred output");
    }
    void delta_model_timeout() {
        using G=McmcEstimator<int>;
        struct Row {int input;};
        G g({.observation_check_interval=1});auto b=G::Budget::for_steps(1);
        auto predict=[](int input,const int&x){return static_cast<double>(input*x*x);};
        auto loss=[](const int&,const Row&,double y){return y;};
        auto prior=[](const int&x){return 5.0*x*x;};
        auto full=G::make_observation_model(std::vector<Row>{{1},{2},{3}},predict,loss,prior);
        std::vector<int> initial{3};g.reset(initial,full,b);
        auto reject=[](const int&,int&next,G::Rng&,const G::MoveContext&){next=1;return -1000.0;};
        g.run(G::Phase::Sample,full,reject,b);
        check(*g.best().state==1,"priority candidate outside chain");
        int calls=0;
        auto stop=[&](int input,const int&x){if(++calls==1)b.deadline=G::Clock::now();return predict(input,x);};
        auto delta=G::make_observation_model(std::vector<Row>{{10},{20},{30}},stop,loss,[](const int&){return 0.0;});
        b=G::Budget::for_steps(1);g.append_energy(delta,b);
        auto updated=G::make_observation_model(std::vector<Row>{{1},{2},{3},{10},{20},{30}},predict,loss,prior);
        auto stay=[](const int&,int&,G::Rng&,const G::MoveContext&){return 0.0;};
        b=G::Budget::for_steps(2);
        g.run(G::Phase::Sample,updated,stay,b,[&](const int&x,double e,int){check(e==updated(x),"delta model resumed chain");});
        check(g.best().energy==updated(*g.best().state) && g.best().energy==71,"delta model timeout keeps full energy");
    }
    void fixed_dimensions() {
        S::Domain small{{{-INFINITY,INFINITY,1}},{}},padded=small;
        padded.real.resize(33,{5,5,1});
        auto a=S::make_numeric_move(small,{.adapt=false,.difference_probability=1,.stretch_probability=0});
        auto b=S::make_numeric_move(padded,{.adapt=false,.difference_probability=1,.stretch_probability=0});
        std::vector<McmcState> pa,pb;
        for(int i=0;i<4;++i){pa.push_back({{i-1.5},{}});pb.push_back({std::vector<double>(33,5),{}});pb.back().real[0]=pa.back().real[0];}
        S::Rng ra(514),rb(514);
        for(int i=0;i<200;++i){
            const auto j=static_cast<std::size_t>(i%4);auto x=pa[j],y=pb[j];
            const double qa=a(pa[j],x,ra,{S::Phase::Sample,1,j,pa,1});
            const double qb=b(pb[j],y,rb,{S::Phase::Sample,1,j,pb,1});
            check(qa==qb && x.real[0]==y.real[0],"fixed dimensions do not shrink difference proposal");
            check(std::all_of(y.real.begin()+1,y.real.end(),[](double v){return v==5;}),"difference preserves fixed coordinates");
        }
        // 実数が全固定なら、カテゴリの座標提案へ進む。
        S::Domain fixed{{{5,5,1}},{{0,2,true}}};
        auto categorical=S::make_numeric_move(fixed,{.difference_probability=1,.stretch_probability=0});
        std::vector<McmcState> population(4,McmcState{{5},{0}});S::Rng r(997);int changed=0;
        for(int i=0;i<300;++i){auto candidate=population[0];categorical(population[0],candidate,r,{S::Phase::Sample,1,0,population,1});changed+=candidate.discrete[0]!=0;}
        check(changed>150,"fixed real coordinates cannot block categorical moves");
    }
    void structured_observations() {
        using G=McmcEstimator<int>;
        struct Row {std::array<int,2> input;std::array<std::optional<int>,2> observed;};
        auto pred=[](const std::array<int,2>&x,const int&s){return std::array<int,2>{(x[0]+s)%3,(x[1]+2*s)%3};};
        auto loss=[](const int&,const Row&o,const std::array<int,2>&p){double e=0;for(std::size_t i=0;i<2;++i)if(o.observed[i])e-=std::log(*o.observed[i]==p[i]?0.8:0.1);return e;};
        auto prior=[](const int&s){return -std::log((s+1)/6.0);};
        std::vector<Row> rows{{{0,1},{1,std::nullopt}},{{2,0},{0,2}}};
        auto model=G::make_observation_model(rows,pred,loss,prior);
        auto naive=[&](const int&s){double e=prior(s);for(const auto&row:rows)e+=loss(s,row,pred(row.input,s));return e;};
        G g;auto b=G::Budget::for_steps(10);std::vector<int> initial{0,1,2};g.reset(initial,model,b);
        Row row{{1,1},{std::nullopt,0}};rows.push_back(row);g.observe(model,std::vector<Row>{row},b);
        for(int i=0;i<3;++i)check(model(i)==naive(i),"structured partial categorical observation");
        auto move=[](const int&,int&next,G::Rng&r,const G::MoveContext&){next=r.integer(3);return 0.0;};
        g.run(G::Phase::Sample,model,move,b,[&](const int&s,double e,int){check(e==naive(s),"structured observation sample energy");});
    }
    void reverse_proposals() {
        S::Domain domain{{{-INFINITY,INFINITY,0.5}}, {}};
        auto move=S::make_numeric_move(domain);
        std::vector<McmcState> states{{{0},{}}};
        S::Rng rng(950);
        S::MoveContext ctx{S::Phase::Search,1,0,states,1};
        auto next=states[0]; move(states[0],next,rng,ctx);
        const double first=next.real[0]; check(first!=0,"nonzero initial proposal");
        move.feedback(false,ctx);next=states[0];move(states[0],next,rng,ctx);
        check(next.real[0]==-first,"rejected search proposal tries opposite direction");
        move.feedback(false,ctx);next=states[0];move(states[0],next,rng,ctx);
        check(next.real[0]!=first && next.real[0]!=-first,"only one reverse attempt");
        move.feedback(false,ctx);
        ctx.phase=S::Phase::Sample;
        auto reference=move;S::Rng reference_rng=rng;
        next=states[0];move(states[0],next,rng,ctx);
        auto expected=states[0];reference(states[0],expected,reference_rng,ctx);
        for(int i=0;i<100;++i){
            move.feedback(i%2==0,ctx);next=states[0];expected=states[0];
            move(states[0],next,rng,ctx);reference(states[0],expected,reference_rng,ctx);
            check(next.real==expected.real,"sampling does not use acceptance history");
        }
        ctx.phase=S::Phase::Search;move.feedback(false,ctx);
        const double queued=-next.real[0];++ctx.revision;
        next=states[0];move(states[0],next,rng,ctx);
        check(next.real[0]!=queued,"revision discards queued search direction");
        domain={ {},{{-5,5,false}} };move=S::make_numeric_move(domain,{.adapt=false});states={{{},{0}}};
        ctx={S::Phase::Search,1,0,states,2};
        bool checked=false;
        for(int i=0;i<100 && !checked;++i){next=states[0];move(states[0],next,rng,ctx);
            if(next.discrete[0]!=0 && std::abs(next.discrete[0])<=5){const auto delta=next.discrete[0];move.feedback(false,ctx);next=states[0];move(states[0],next,rng,ctx);check(next.discrete[0]==-delta,"integer reverse uses rounded step");checked=true;}
            else move.feedback(true,ctx);
        }
        check(checked,"integer reverse exercised");
        // A pending reverse attempt and its adaptation counters survive slicing.
        domain={{{-8,8,1},{-8,8,1}}, {}};states={{{3,3},{}}};
        auto eval=[](const McmcState&x){return std::pow(x.real[0]-0.7,2)+4*std::pow(x.real[1]+1,2)+0.2*std::sin(x.real[0]*x.real[1]);};
        S a({.seed=843,.cooling_steps=5000}),b({.seed=843,.cooling_steps=5000});
        auto ma=S::make_numeric_move(domain),mb=S::make_numeric_move(domain);
        auto ba=S::Budget::for_steps(5000),bb=S::Budget::for_steps(1);a.reset(states,eval,ba);b.reset(states,eval,bb);
        a.run(S::Phase::Search,eval,ma,ba);
        for(int i=0;i<1000;++i){bb=S::Budget::for_steps(5);b.run(S::Phase::Search,eval,mb,bb);}
        check(a.best().energy==b.best().energy && same(*a.best().state,*b.best().state),"single-chain search slicing");
        std::vector<double> xa,xb;ba=S::Budget::for_steps(300);bb=S::Budget::for_steps(300);
        a.run(S::Phase::Sample,eval,ma,ba,[&](const McmcState&x,double,int){xa.insert(xa.end(),x.real.begin(),x.real.end());});
        b.run(S::Phase::Sample,eval,mb,bb,[&](const McmcState&x,double,int){xb.insert(xb.end(),x.real.begin(),x.real.end());});
        check(xa==xb,"sliced search retains RNG and scales before Sample");
    }
    void stretch_geometry() {
        S::Domain domain{{{-INFINITY,INFINITY,1},{-INFINITY,INFINITY,1},{7,7,1}}, {}};
        auto move=S::make_numeric_move(domain,{.adapt=false,.stretch_probability=1});
        std::vector<McmcState> states{{{2,4,7},{}},{{0,0,7},{}}};S::Rng rng(257);
        S::MoveContext ctx{S::Phase::Sample,1,0,states,1};
        double sum=0;
        for(int i=0;i<20000;++i){auto next=states[0];const double q=move(states[0],next,rng,ctx);const double z=next.real[0]/2;
            check(z>=0.5 && z<=2 && next.real[1]==2*next.real[0] && next.real[2]==7,"stretch geometry and fixed coordinate");
            check(std::abs(q-std::log(z))<1e-14,"stretch ratio counts only active dimensions");sum+=z;
        }
        check(std::abs(sum/20000-7.0/6)<0.01,"stretch factor density");
        // The stretched kernel must retain boundary rejections, not clamp them.
        domain={{{0,1,0.2},{0,1,0.2}}, {}};move=S::make_numeric_move(domain,{.adapt=false,.stretch_probability=1});
        states={{{0.9,0.9},{}},{{0.1,0.1},{}}};ctx={S::Phase::Sample,1,0,states,2};int rejected=0;
        for(int i=0;i<300;++i){auto next=states[0];const double q=move(states[0],next,rng,ctx);if(q==-INFINITY)++rejected;}
        check(rejected>30,"stretch rejects points outside the domain");
    }
    void stretch_stationary() {
        S::Domain domain{{{0,INFINITY,0.5},{0,INFINITY,0.5},{0,INFINITY,0.5},{0,INFINITY,0.5}},{{0,1,true}}};
        auto move=S::make_numeric_move(domain,{.stretch_probability=0.5});
        auto energy=[](const McmcState&x){double e=x.discrete[0]?-std::log(3.0):0;for(double z:x.real){if(z<=0)return INFINITY+0.0;e+=z-std::log(z);}return e;};
        std::vector<McmcState> states(6,McmcState{{1,1,1,1},{0}});S solver({.seed=521});
        auto b=S::Budget::for_steps(50000);solver.reset(states,energy,b);solver.run(S::Phase::Warmup,energy,move,b);
        std::array<double,4> sum{},square{};int one=0;
        b=S::Budget::for_steps(500000);
        solver.run(S::Phase::Sample,energy,move,b,[&](const McmcState&x,double,int){one+=x.discrete[0]!=0;for(std::size_t j=0;j<4;++j){sum[j]+=x.real[j];square[j]+=x.real[j]*x.real[j];}});
        for(std::size_t j=0;j<4;++j){check(std::abs(sum[j]/500000-2)<0.08,"Gamma mean with stretch");check(std::abs(square[j]/500000-6)<0.45,"Gamma second moment with stretch");}
        check(std::abs(one/500000.0-0.75)<0.025,"mixed categorical stationary mass");
    }
    void discrete_kernel() {
        // A lazy categorical proposal remains symmetric; Search may always change ID.
        S::Domain domain{{},{{-2,1,true}}};
        auto move=S::make_numeric_move(domain,{.adapt=false});
        S::Rng rng(67143);std::vector<McmcState> states{{{}, {-2}}};
        for(int source=-2;source<=1;++source) {
            states[0].discrete[0]=source;std::array<int,4> counts{};
            for(int i=0;i<40000;++i) {
                auto next=states[0];const double q=move(states[0],next,rng,{S::Phase::Sample,1,0,states,1});
                ++counts[static_cast<std::size_t>(next.discrete[0]+2)];
                check(q==0 || (q==-INFINITY && next.discrete==states[0].discrete),"category ratio and unchanged rejection");
            }
            for(int destination=-2;destination<=1;++destination) {
                const double expected=destination==source ? 0.025 : 0.325;
                check(std::abs(counts[static_cast<std::size_t>(destination+2)]/40000.0-expected)<0.012,"lazy categorical proposal mass");
            }
            for(int i=0;i<100;++i) {
                auto next=states[0];move(states[0],next,rng,{S::Phase::Search,1,0,states,1});
                check(next.discrete[0]!=source,"categorical Search avoids self proposals");
            }
        }
        // Even thinning must not lock a binary variable to one parity.
        for(bool categorical:{false,true})for(std::uint64_t stride:{2,6,11}) {
            domain={{},{{0,1,categorical}}};move=S::make_numeric_move(domain);
            S solver({.seed=7361+stride,.sample_stride=stride});states={{{},{0}}};
            auto energy=[](const McmcState&){return 0.0;};auto b=S::Budget::for_steps(20000);
            solver.reset(states,energy,b);solver.run(S::Phase::Warmup,energy,move,b);
            std::uint64_t count=0,one=0;b=S::Budget::for_steps(180000);
            auto r=solver.run(S::Phase::Sample,energy,move,b,[&](const McmcState&x,double,int){++count;one+=x.discrete[0]!=0;});
            check(count==180000/stride && r.emitted==count,"binary sample stride");
            check(std::abs(static_cast<double>(one)/static_cast<double>(count)-0.5)<0.04,"binary lazy stationary mass after thinning");
        }
        // Nonuniform integer target, normalized independently by enumeration.
        domain={{},{{-4,4,false}}};move=S::make_numeric_move(domain);states={{{},{-4}},{{},{4}}};
        auto energy=[](const McmcState&x){return 0.5*std::pow((static_cast<double>(x.discrete[0])-0.7)/0.55,2);};
        S solver({.seed=731942});auto b=S::Budget::for_steps(50000);solver.reset(states,energy,b);
        solver.run(S::Phase::Warmup,energy,move,b);std::array<double,9> counts{},expected{};double sum=0;
        for(int i=-4;i<=4;++i)sum+=(expected[static_cast<std::size_t>(i+4)]=std::exp(-energy(McmcState{{},{i}})));
        b=S::Budget::for_steps(400000);
        solver.run(S::Phase::Sample,energy,move,b,[&](const McmcState&x,double e,int){
            ++counts[static_cast<std::size_t>(x.discrete[0]+4)];check(e==energy(x),"integer sample energy");
        });
        for(std::size_t i=0;i<9;++i)check(std::abs(counts[i]/400000-expected[i]/sum)<0.015,"integer target probability");
    }
    void self_evaluation_elision() {
        // Re-evaluating identical proposals must produce exactly the same trajectory.
        struct Reevaluate {
            S::NumericMove move;
            double operator()(const McmcState&old,McmcState&next,S::Rng&rng,const S::MoveContext&ctx) {
                const double q=move(old,next,rng,ctx);
                return q==-INFINITY && same(old,next) ? 0 : q;
            }
            void feedback(bool accepted,const S::MoveContext&ctx){move.feedback(accepted,ctx);}
        };
        S::Domain domain{{},{{-4,4,false},{0,2,true}}};
        auto ma=S::make_numeric_move(domain);Reevaluate mb{S::make_numeric_move(domain)};
        S a({.seed=17,.sample_stride=3,.cooling_steps=10000}),b({.seed=17,.sample_stride=3,.cooling_steps=10000});
        std::vector<McmcState> initial{{{},{1,0}}};
        auto eval=[](const McmcState&x){return 0.5*std::pow((static_cast<double>(x.discrete[0])-0.3)/0.7,2)+0.6*static_cast<double>(x.discrete[1]);};
        auto ba=S::Budget::for_steps(10000),bb=ba;a.reset(initial,eval,ba);b.reset(initial,eval,bb);
        std::uint64_t avoided=0;
        for(auto phase:{S::Phase::Search,S::Phase::Warmup,S::Phase::Sample}) {
            std::vector<McmcState> xa,xb;ba=S::Budget::for_steps(30000);bb=ba;
            auto ra=a.run(phase,eval,ma,ba,[&](const McmcState&x,double,int){xa.push_back(x);});
            auto rb=b.run(phase,eval,mb,bb,[&](const McmcState&x,double,int){xb.push_back(x);});
            check(a.best().energy==b.best().energy && same(*a.best().state,*b.best().state),"elision retains best");
            check(ra.steps==rb.steps && ra.emitted==rb.emitted && xa.size()==xb.size(),"elision retains step and sample counts");
            for(std::size_t i=0;i<xa.size();++i)check(same(xa[i],xb[i]),"elision exact sample trajectory");
            check(ra.evaluations<rb.evaluations,"self proposals skip expensive evaluation");
            avoided+=rb.evaluations-ra.evaluations;
        }
        check(avoided>100,"self elision exercised");
    }
    void reflected_real_kernel() {
        // Both finite bounds, either one-sided bound, and a fixed coordinate.
        for(int mode=0;mode<3;++mode) {
            S::Domain domain{{{mode==2?-INFINITY:0,mode==1?INFINITY:1,20},{7,7,1}}, {}};
            auto move=S::make_numeric_move(domain,{.adapt=false,.stretch_probability=0});
            std::vector<McmcState> initial{{{0.4,7},{}}};S::Rng rng(8231+mode);
            int rejected=0;
            for(int i=0;i<1000;++i) {
                auto next=initial[0];const double q=move(initial[0],next,rng,{S::Phase::Sample,1,0,initial,1});
                check(q==0 && next.real[0]>=domain.real[0].lower && next.real[0]<=domain.real[0].upper && next.real[1]==7,"coordinate reflection stays within bounds");
                next=initial[0];rejected+=move(initial[0],next,rng,{S::Phase::Search,1,0,initial,1})==-INFINITY;
            }
            check(rejected>200,"Search retains original boundary rejection");
        }
        // Check a nonuniform bounded density, not just Uniform stationarity.
        S::Domain domain{{{0,1,0.4}}, {}};auto move=S::make_numeric_move(domain);
        std::vector<McmcState> initial{{{0.1},{}},{{0.9},{}}};
        auto beta=[](const McmcState&x){const double z=x.real[0];return z>0 && z<1 ? -std::log(z)-4*std::log1p(-z) : INFINITY;};
        S solver({.seed=2583});auto b=S::Budget::for_steps(20000);solver.reset(initial,beta,b);solver.run(S::Phase::Warmup,beta,move,b);
        double sum=0,square=0;b=S::Budget::for_steps(400000);
        solver.run(S::Phase::Sample,beta,move,b,[&](const McmcState&x,double e,int){sum+=x.real[0];square+=x.real[0]*x.real[0];check(e==beta(x),"Beta reflected sample energy");});
        check(std::abs(sum/400000-2.0/7)<0.006,"Beta mean under reflection");
        check(std::abs(square/400000-3.0/28)<0.006,"Beta second moment under reflection");
    }
    void run() {
        discrete_kernel();self_evaluation_elision();reflected_real_kernel();
        rng();generic_and_asymmetric();gaussian_and_numeric();continuation();
        observations_and_sync();interrupted_evaluation();deferred_output_and_constraints();candidate_difference();boundaries_and_random_updates();
        noop_updates_and_cooling();delta_model_timeout();fixed_dimensions();structured_observations();
        reverse_proposals();stretch_geometry();stretch_stationary();
        std::cout << "PASS " << checks << " checks\n";
    }
};
int main(int argc,char**argv) {
    if(argc>1) {
        const std::string name=argv[1];
        if(name=="review_noop")McmcEstimatorTests{}.noop_updates_and_cooling();
        if(name=="review_empty")McmcEstimatorTests{}.noop_updates_and_cooling(true);
        if(name=="review_delta")McmcEstimatorTests{}.delta_model_timeout();
        if(name=="review_fixed")McmcEstimatorTests{}.fixed_dimensions();
        if(name=="bad_sigma")McmcEstimator<>::gaussian_loss(0,0,0);
        if(name=="bad_rng")McmcEstimator<>::Rng(1).integer(0);
        if(name=="bad_stride"){McmcEstimator<> solver({.sample_stride=0});(void)solver;}
        if(name=="bad_mixture"){auto move=McmcEstimator<>::make_numeric_move({}, {.difference_probability=0.8,.stretch_probability=0.3});(void)move;}
        if(name=="bad_energy"){
            McmcEstimator<int> solver;std::vector<int> initial{1};auto b=McmcEstimator<int>::Budget::for_steps(1);
            auto eval=[](const int&){return std::numeric_limits<double>::quiet_NaN();};solver.reset(initial,eval,b);
        }
        return 0;
    }
    McmcEstimatorTests{}.run();
}
#endif
