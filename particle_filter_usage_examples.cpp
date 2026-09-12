// C++20 / GCC 12.2。U01〜U16の各caseは独立してコピーできる関数形式のソルバー。
// 応用例のworkflow_exampleはcase01、sampled_routesはcase15、streaming_routesはcase02も必要。
// PARTICLE_FILTER_USAGE_SELF_TESTを定義すると全例の検証用mainを有効にする。

// BEGIN CASE01
#include "particle_filter_v09.hpp"
namespace case01 {
// 入力: 既知の操作量x、測定値y、測定誤差の標準偏差sigma。
struct Observation { double x, y, sigma; };
struct Model : UniformBox<2> {
    using Observation = case01::Observation;
    Model(std::array<double, 2> lower, std::array<double, 2> upper)
        : UniformBox(lower, upper) {}
    // 粒子p=[倍率a,固定ずれb]から中心a*x+bを予測し、今回の測定yの対数密度を返す。
    double log_likelihood(const Particle& p, const Observation& o) const {
        const double z = (o.y - (p[0] * o.x + p[1])) / o.sigma;
        return -0.5 * z * z - std::log(o.sigma) - 0.5 * std::log(2 * std::numbers::pi);
    }
};
struct Result {
    std::array<double, 2> mean;      // [操作量の倍率a, 固定ずれb]の全粒子平均
    std::array<double, 2> top_mean;  // 上位top_k粒子内で再正規化した平均
    double top_mass;                // 選んだ粒子が元の全重みに占める割合
};
// lower/upperは[a,b]の事前範囲。top_k=0は全粒子、1..countは上位k。
// nulloptは、観測を取り込めなかったことを示す。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<Result> solve(const std::vector<Observation>& observations,
                           std::array<double, 2> lower, std::array<double, 2> upper,
                           int top_k = 0, int count = 1024, std::uint64_t seed = 0) {
    assert(0 <= top_k && top_k <= count);
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model(lower, upper), {.count = count}, std::mt19937_64(seed));
    for (const auto& o : observations) {
        assert(o.sigma > 0);
        // 新しい観測を1回だけ追加する。失敗ならこの関数例では後続の観測を処理しない。
        if (!filter.observe(o).ok) return std::nullopt;
    }
    // 全平均と上位k平均は違う代表値。上位の選択質量も返し、どこまで捨てたかを読めるようにする。
    const auto top = filter.estimate(top_k);
    return Result{filter.estimate().mean, top.mean, top.selected_mass};
}
}
// END CASE01

// BEGIN CASE02
#include "particle_filter_v09.hpp"
namespace case02 {
// length[g]は経路中の地域gの距離。fixedは既知の固定時間。
struct Observation { std::vector<double> length; double fixed, time, sigma; };
struct Route { std::vector<double> length; double fixed = 0; };
struct Model : UniformBox<> {
    using Observation = case02::Observation;
    Model(std::vector<double> lower, std::vector<double> upper)
        : UniformBox(std::move(lower), std::move(upper)) {}
    // 地域ごとの「単位時間×通った距離」と固定時間を合計し、今回の所要時間の対数密度を返す。
    double log_likelihood(const Particle& p, const Observation& o) const {
        const double predicted = o.fixed + std::inner_product(p.begin(), p.end(), o.length.begin(), 0.0);
        const double z = (o.time - predicted) / o.sigma;
        return -0.5 * z * z - std::log(o.sigma) - 0.5 * std::log(2 * std::numbers::pi);
    }
};
struct Result {
    std::vector<double> unit_time;        // 地域ごとの単位距離あたり時間の推定
    std::vector<double> expected_time;    // 候補経路の真の所要時間の期待値
    std::vector<double> late_probability; // 真の所要時間がdeadlineを超える確率
    int selected_route;                  // 期待時間 + penalty * 超過確率が最小の経路
};
// lower/upperの長さが地域数G。全lengthも長さG。
// 測定ノイズと走行時間自体の変動は区別する。この例の遅延確率はパラメータ不確実性のみ。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<Result> solve(const std::vector<Observation>& observations,
                           std::vector<double> lower, std::vector<double> upper,
                           const std::vector<Route>& routes, double deadline, double penalty,
                           int count = 1024, std::uint64_t seed = 0) {
    assert(!routes.empty() && penalty >= 0);
    const int groups = int(lower.size());
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model(std::move(lower), std::move(upper)),
                                 {.count = count}, std::mt19937_64(seed));
    for (const auto& o : observations) {
        assert(int(o.length.size()) == groups && o.sigma > 0);
        // 新しい観測を1回だけ追加する。失敗ならこの関数例では後続の観測を処理しない。
        if (!filter.observe(o).ok) return std::nullopt;
    }
    for (const auto& route : routes) assert(int(route.length.size()) == groups);
    Result result{filter.estimate().mean, std::vector<double>(routes.size()),
                  std::vector<double>(routes.size()), 0};
    filter.for_each_weighted([&](const Model::Particle& p, double weight) {
        for (int r = 0; r < int(routes.size()); ++r) {
            const double time = routes[r].fixed + std::inner_product(p.begin(), p.end(), routes[r].length.begin(), 0.0);
            result.expected_time[r] += weight * time;
            result.late_probability[r] += weight * (time > deadline);
        }
    });
    // 問題の罰点に合わせてpenaltyを指定する。締切を超えない制約を自動保証する処理ではない。
    auto loss = [&](int r) { return result.expected_time[r] + penalty * result.late_probability[r]; };
    for (int r = 1; r < int(routes.size()); ++r)
        if (loss(r) < loss(result.selected_route)) result.selected_route = r;
    return result;
}
}
// END CASE02

// BEGIN CASE03
#include "particle_filter_v09.hpp"
namespace case03 {
// (sx,sy)は測距位置、rangeは発信源までの距離の測定値。
struct Observation { double sx, sy, range, sigma; };
struct Model : UniformBox<2> {
    using Observation = case03::Observation;
    Model(std::array<double, 2> lower, std::array<double, 2> upper) : UniformBox(lower, upper) {}
    // 粒子の発信源座標から測定地点までの距離を計算し、測距結果の対数密度を返す。
    double log_likelihood(const Particle& p, const Observation& o) const {
        const double z = (o.range - std::hypot(p[0] - o.sx, p[1] - o.sy)) / o.sigma;
        return -0.5 * z * z - std::log(o.sigma) - 0.5 * std::log(2 * std::numbers::pi);
    }
};
// 入力: 発信源の存在範囲と測距履歴。出力: [x,y]の事後平均。
// 対称な候補が残ると平均は候補間に来る。行動評価には粒子ごとの評価も検討する。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<std::array<double, 2>> solve(const std::vector<Observation>& observations,
                                        std::array<double, 2> lower, std::array<double, 2> upper,
                                        int count = 1024, std::uint64_t seed = 0) {
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model(lower, upper), {.count = count}, std::mt19937_64(seed));
    for (const auto& o : observations) {
        assert(o.sigma > 0);
        // 新しい観測を1回だけ追加する。失敗ならこの関数例では後続の観測を処理しない。
        if (!filter.observe(o).ok) return std::nullopt;
    }
    return filter.estimate().mean;
}
}
// END CASE03

// BEGIN CASE04
#include "particle_filter_v09.hpp"
namespace case04 {
// センサーは a*幅+b*高さ+正規ノイズ を最も近い整数に丸めて返す。
struct Observation { double a, b, sigma; int rounded; };
struct Model : UniformBox<2> {
    using Observation = case04::Observation;
    Model(std::array<double, 2> lower, std::array<double, 2> upper) : UniformBox(lower, upper) {}
    // 整数表示の一点での密度ではなく、その整数に丸められる区間全体の確率を返す。
    double likelihood(const Particle& p, const Observation& o) const {
        const double mean = o.a * p[0] + o.b * p[1];
        const double lo = (o.rounded - 0.5 - mean) / o.sigma;
        const double hi = (o.rounded + 0.5 - mean) / o.sigma;
        const double s = std::sqrt(2.0);
        // 同じ側の裾では、小さい裾確率どうしを引き、1からの差し引きを避ける。
        if (lo >= 0) return 0.5 * (std::erfc(lo / s) - std::erfc(hi / s));
        if (hi <= 0) return 0.5 * (std::erfc(-hi / s) - std::erfc(-lo / s));
        return 1 - 0.5 * std::erfc(hi / s) - 0.5 * std::erfc(-lo / s);
    }
};
// lower/upper=[幅,高さ]の実数範囲。出力も実数。真に整数の寸法を扱う場合は離散粒子が必要。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<std::array<double, 2>> solve(const std::vector<Observation>& observations,
                                        std::array<double, 2> lower, std::array<double, 2> upper,
                                        int count = 1024, std::uint64_t seed = 0) {
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model(lower, upper), {.count = count}, std::mt19937_64(seed));
    for (const auto& o : observations) {
        assert(o.sigma > 0);
        // 新しい観測を1回だけ追加する。失敗ならこの関数例では後続の観測を処理しない。
        if (!filter.observe(o).ok) return std::nullopt;
    }
    return filter.estimate().mean;
}
}
// END CASE04

// BEGIN CASE05
#include "particle_filter_v09.hpp"
namespace case05 {
struct Observation { int successes, trials; };
struct Model {
    using Particle = double; // 未知の成功確率p
    using Observation = case05::Observation;
    double alpha, beta;      // Beta事前分布。どちらも正。1,1なら一様。
    Particle sample(std::mt19937_64& rng) {
        const double a = std::gamma_distribution<double>(alpha, 1)(rng);
        const double b = std::gamma_distribution<double>(beta, 1)(rng);
        assert(a + b > 0);
        return a / (a + b);
    }
    // 今回のtrials回にsuccesses回成功する確率の対数。過去の成功数を重ねて加えない。
    double log_likelihood(Particle p, const Observation& o) const {
        double value = std::lgamma(o.trials + 1.0) - std::lgamma(o.successes + 1.0)
                     - std::lgamma(o.trials - o.successes + 1.0);
        if (o.successes > 0) value += o.successes * std::log(p);
        if (o.trials > o.successes) value += (o.trials - o.successes) * std::log1p(-p);
        return value;
    }
    // proposeを省略。再探索の際はsampleからの提案をライブラリが処理する。
};
struct Result { double probability, variance; };
// 入力: 独立試行の成功数と試行数。出力: 成功確率の平均と分散。
// この単純なBeta-Binomial問題は厳密更新も可能。複雑な観測へ拡張する際の基本形として示す。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<Result> solve(const std::vector<Observation>& observations, double alpha = 1, double beta = 1,
                           int count = 1024, std::uint64_t seed = 0) {
    assert(alpha > 0 && beta > 0);
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model{alpha, beta}, {.count = count}, std::mt19937_64(seed));
    for (const auto& o : observations) {
        assert(0 <= o.successes && o.successes <= o.trials);
        // 新しい観測を1回だけ追加する。失敗ならこの関数例では後続の観測を処理しない。
        if (!filter.observe(o).ok) return std::nullopt;
    }
    const double mean = filter.estimate().mean;
    // 分散=二乗の平均-平均の二乗。観測ノイズではなく、未知の成功確率に残る不確実性。
    const double second = filter.estimate([](double p) { return p * p; }).mean;
    return Result{mean, std::max(0.0, second - mean * mean)};
}
}
// END CASE05

// BEGIN CASE06
#include "particle_filter_v09.hpp"
namespace case06 {
struct Model {
    using Particle = int;     // 鉱石の種類の番号
    using Observation = int;  // スキャナーが返した記号の番号
    std::vector<double> prior_weight;
    std::vector<std::vector<double>> emission; // [種類][記号]の確率、各行の和は1
    Particle sample(std::mt19937_64& rng) {
        return std::discrete_distribution<int>(prior_weight.begin(), prior_weight.end())(rng);
    }
    // 行が仮説の種類、列が実際の表示。返すのは「その種類ならこの表示が出る確率」。
    double likelihood(Particle type, Observation symbol) const { return emission[type][symbol]; }
};
struct Result { std::vector<double> probability; int selected_type; };
// prior_weightは非負で総和が正。emissionの行数=種類数、全行の列数=記号数。
// 出力のprobability[k]は「種類kである」確率。番号自体の平均は取らない。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<Result> solve(const std::vector<int>& symbols, std::vector<double> prior_weight,
                           std::vector<std::vector<double>> emission,
                           int count = 1024, std::uint64_t seed = 0) {
    const int types = int(prior_weight.size());
    assert(types > 0 && int(emission.size()) == types && !emission[0].empty());
    const int alphabet = int(emission[0].size());
    assert(std::accumulate(prior_weight.begin(), prior_weight.end(), 0.0) > 0);
    for (double p : prior_weight) assert(p >= 0);
    for (const auto& row : emission) {
        assert(int(row.size()) == alphabet);
        assert(std::abs(std::accumulate(row.begin(), row.end(), 0.0) - 1) < 1e-9);
        for (double p : row) assert(p >= 0);
    }
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model{std::move(prior_weight), std::move(emission)},
                                 {.count = count}, std::mt19937_64(seed));
    for (int symbol : symbols) {
        assert(0 <= symbol && symbol < alphabet);
        if (!filter.observe(symbol).ok) return std::nullopt;
    }
    Result result{std::vector<double>(types), 0};
    filter.for_each_weighted([&](int type, double weight) { result.probability[type] += weight; });
    result.selected_type = int(std::max_element(result.probability.begin(), result.probability.end()) - result.probability.begin());
    return result;
}
}
// END CASE06

// BEGIN CASE07
#include "particle_filter_v09.hpp"
namespace case07 {
struct Observation { double input, measured, sigma; };
struct Model {
    struct Particle { bool reversed; double gain; };
    using Observation = case07::Observation;
    UniformBox<1> gain_range;
    double fault_prior;
    Model(double lower, double upper, double prior)
        : gain_range({lower}, {upper}), fault_prior(prior) {}
    Particle sample(std::mt19937_64& rng) {
        return {std::bernoulli_distribution(fault_prior)(rng), gain_range.sample(rng)[0]};
    }
    double log_prior(const Particle& p) const {
        return gain_range.log_prior({p.gain}) + std::log(p.reversed ? fault_prior : 1 - fault_prior);
    }
    void propose(Particle& p, std::mt19937_64& rng) {
        if (std::bernoulli_distribution(0.2)(rng)) p.reversed = !p.reversed;
        else {
            std::array<double, 1> gain{p.gain};
            gain_range.propose(gain, rng);
            p.gain = gain[0];
        }
    }
    // 反転の有無と正の倍率から予測値を作り、今回の測定出力の対数密度を返す。
    double log_likelihood(const Particle& p, const Observation& o) const {
        const double predicted = (p.reversed ? -1 : 1) * p.gain * o.input;
        const double z = (o.measured - predicted) / o.sigma;
        return -0.5 * z * z - std::log(o.sigma) - 0.5 * std::log(2 * std::numbers::pi);
    }
    std::array<double, 2> project(const Particle& p) const { return {double(p.reversed), p.gain}; }
};
struct Result { double reversed_probability, mean_gain; };
// 正のgain範囲と、反転故障の事前確率0<fault_prior<1を指定する。
// 故障状態は全観測中一定。このコードは途中で故障が発生・回復するモデルではない。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<Result> solve(const std::vector<Observation>& observations, double lower, double upper,
                           double fault_prior, int count = 1024, std::uint64_t seed = 0) {
    assert(lower > 0 && 0 < fault_prior && fault_prior < 1);
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model(lower, upper, fault_prior), {.count = count}, std::mt19937_64(seed));
    for (const auto& o : observations) {
        assert(o.sigma > 0);
        // 新しい観測を1回だけ追加する。失敗ならこの関数例では後続の観測を処理しない。
        if (!filter.observe(o).ok) return std::nullopt;
    }
    // Model.projectが真偽を0/1へ写すため、その平均が反転確率になる。
    const auto mean = filter.estimate().mean;
    return Result{mean[0], mean[1]};
}
}
// END CASE07

// BEGIN CASE08
#include "particle_filter_v09.hpp"
namespace case08 {
struct Model : UniformBox<2> {
    using Observation = double;
    // p[0]=真の値mu、p[1]=log(sigma)。sigma自身への一様事前ではない。
    Model(double mu_lower, double mu_upper, double sigma_lower, double sigma_upper)
        : UniformBox({mu_lower, std::log(sigma_lower)}, {mu_upper, std::log(sigma_upper)}) {}
    // sigma自体も粒子により変わるので、密度の正規化項である-log(sigma)も必ず含める。
    double log_likelihood(const Particle& p, double measured) const {
        const double z = (measured - p[0]) / std::exp(p[1]);
        return -0.5 * z * z - p[1] - 0.5 * std::log(2 * std::numbers::pi);
    }
    std::array<double, 2> project(const Particle& p) const { return {p[0], std::exp(p[1])}; }
};
// 入力: 同じ真の値の反復測定と事前範囲。出力: [muの平均, sigmaの平均]。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<std::array<double, 2>> solve(const std::vector<double>& measurements,
                                        double mu_lower, double mu_upper, double sigma_lower, double sigma_upper,
                                        int count = 1024, std::uint64_t seed = 0) {
    assert(0 < sigma_lower && sigma_lower < sigma_upper);
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model(mu_lower, mu_upper, sigma_lower, sigma_upper),
                                 {.count = count}, std::mt19937_64(seed));
    for (double y : measurements) if (!filter.observe(y).ok) return std::nullopt;
    return filter.estimate().mean;
}
}
// END CASE08

// BEGIN CASE09
#include "particle_filter_v09.hpp"
namespace case09 {
// センサーは真の方向(cos(theta),sin(theta))の両成分に独立な正規誤差を加える。
struct Observation { double x, y, sigma; };
struct Model {
    using Particle = double; // ラジアン角
    using Observation = case09::Observation;
    Particle sample(std::mt19937_64& rng) {
        return (2 * std::generate_canonical<double, 53>(rng) - 1) * std::numbers::pi;
    }
    double log_prior(Particle) const { return 0; } // 円周上の一様事前
    void propose(Particle& angle, std::mt19937_64& rng) {
        angle = std::remainder(angle + std::normal_distribution<double>(0, 0.3)(rng), 2 * std::numbers::pi);
    }
    // 方位角から方向の2成分を予測する。独立な2つの成分誤差の同時対数密度を返す。
    double log_likelihood(Particle angle, const Observation& o) const {
        const double dx = o.x - std::cos(angle), dy = o.y - std::sin(angle);
        return -0.5 * (dx * dx + dy * dy) / (o.sigma * o.sigma)
             - std::log(2 * std::numbers::pi * o.sigma * o.sigma);
    }
    std::array<double, 2> project(Particle angle) const { return {std::cos(angle), std::sin(angle)}; }
};
struct Result { double angle, resultant; };
// 出力angleは[-pi,pi]の円周平均、resultantは0..1。0付近なら方向の平均が不安定。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<Result> solve(const std::vector<Observation>& observations,
                           int count = 1024, std::uint64_t seed = 0) {
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model{}, {.count = count}, std::mt19937_64(seed));
    for (const auto& o : observations) {
        assert(o.sigma > 0);
        // 新しい観測を1回だけ追加する。失敗ならこの関数例では後続の観測を処理しない。
        if (!filter.observe(o).ok) return std::nullopt;
    }
    const auto mean = filter.estimate().mean;
    // 方向成分の平均から角度へ戻す。resultantが小さいときは、この角度を確定的に使わない。
    return Result{std::atan2(mean[1], mean[0]), std::hypot(mean[0], mean[1])};
}
}
// END CASE09

// BEGIN CASE10
#include "particle_filter_v09.hpp"
namespace case10 {
// difference=候補Aの特徴-候補Bの特徴。a_wonは比較結果。
// temperature>0は比較の曖昧さで、問題から既知とする。
struct Observation { std::array<double, 3> difference; bool a_won; double temperature; };
struct Model : UniformBox<3> {
    using Observation = case10::Observation;
    Model(std::array<double, 3> lower, std::array<double, 3> upper) : UniformBox(lower, upper) {}
    // 重み付き特徴差を比較の曖昧さで割り、実際に選ばれた側の確率の対数を返す。
    double log_likelihood(const Particle& p, const Observation& o) const {
        double z = std::inner_product(p.begin(), p.end(), o.difference.begin(), 0.0) / o.temperature;
        if (!o.a_won) z = -z;
        return z >= 0 ? -std::log1p(std::exp(-z)) : z - std::log1p(std::exp(z));
    }
};
// 出力: 3特徴の未知重みの平均。全特徴に差が生じる比較を集めないと識別できない。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<std::array<double, 3>> solve(const std::vector<Observation>& observations,
                                        std::array<double, 3> lower, std::array<double, 3> upper,
                                        int count = 1024, std::uint64_t seed = 0) {
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model(lower, upper), {.count = count}, std::mt19937_64(seed));
    for (const auto& o : observations) {
        assert(o.temperature > 0);
        // 新しい観測を1回だけ追加する。失敗ならこの関数例では後続の観測を処理しない。
        if (!filter.observe(o).ok) return std::nullopt;
    }
    return filter.estimate().mean;
}
}
// END CASE10

// BEGIN CASE11
#include "particle_filter_v09.hpp"
namespace case11 {
struct Model {
    using Particle = std::array<double, 3>; // 非負、合計1の材料構成比
    using Observation = std::array<int, 3>; // 標本中の種類別個数
    Particle sample(std::mt19937_64& rng) {
        Particle p;
        for (double& x : p) x = std::exponential_distribution<double>(1)(rng);
        const double sum = std::accumulate(p.begin(), p.end(), 0.0);
        assert(sum > 0);
        for (double& x : p) x /= sum;
        return p; // Dirichlet(1,1,1)、三角形上の一様事前
    }
    double log_prior(const Particle& p) const {
        if (std::abs(p[0] + p[1] + p[2] - 1) > 1e-9 || *std::min_element(p.begin(), p.end()) < 0)
            return -std::numeric_limits<double>::infinity();
        return 0;
    }
    void propose(Particle& p, std::mt19937_64& rng) {
        const int i = std::uniform_int_distribution<int>(0, 2)(rng);
        const int j = (i + 1 + std::uniform_int_distribution<int>(0, 1)(rng)) % 3;
        const double total = p[i] + p[j];
        p[i] = total * std::generate_canonical<double, 53>(rng);
        p[j] = total - p[i]; // 2成分の合計を保つ対称提案
    }
    // 今回の種類別個数が出る確率の対数。標本内の並び方の数も含める。
    double log_likelihood(const Particle& p, const Observation& counts) const {
        const int n = std::accumulate(counts.begin(), counts.end(), 0);
        double result = std::lgamma(n + 1.0);
        for (int j = 0; j < 3; ++j) {
            result -= std::lgamma(counts[j] + 1.0);
            if (counts[j] > 0) result += counts[j] * std::log(p[j]);
        }
        return result;
    }
};
// 入力: 独立に抽出した標本の種類別個数。出力: 3種類の比率の平均。
// 単純なDirichlet-Multinomial問題は厳密更新も可能。制約付き粒子のモデル化例。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<std::array<double, 3>> solve(const std::vector<std::array<int, 3>>& batches,
                                        int count = 1024, std::uint64_t seed = 0) {
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model{}, {.count = count}, std::mt19937_64(seed));
    for (const auto& counts : batches) {
        for (int x : counts) assert(x >= 0);
        if (!filter.observe(counts).ok) return std::nullopt;
    }
    return filter.estimate().mean;
}
}
// END CASE11

// BEGIN CASE12
#include "particle_filter_v09.hpp"
namespace case12 {
// 1行=dt時間の移動と、その移動後の位置測定。欠測でも1行を渡す。
struct Observation { double dt, ax, ay, x, y, sigma; bool measured; };
struct Model {
    using Particle = std::array<double, 4>; // [x,y,vx,vy]
    using Observation = case12::Observation;
    Particle initial_mean, initial_sd;
    double position_noise, velocity_noise; // 時間1あたりの遷移ノイズ標準偏差
    Particle sample(std::mt19937_64& rng) {
        Particle p = initial_mean;
        std::normal_distribution<double> normal;
        for (int j = 0; j < 4; ++j) p[j] += initial_sd[j] * normal(rng);
        return p;
    }
    // 現実の状態変化を1ターン分だけ表す。observeがこの遷移の後に観測尤度を評価する。
    void transition(Particle& p, const Observation& o, std::mt19937_64& rng) {
        std::normal_distribution<double> normal;
        const double dt = o.dt, root_dt = std::sqrt(dt);
        p[0] += p[2] * dt + 0.5 * o.ax * dt * dt + position_noise * root_dt * normal(rng);
        p[1] += p[3] * dt + 0.5 * o.ay * dt * dt + position_noise * root_dt * normal(rng);
        p[2] += o.ax * dt + velocity_noise * root_dt * normal(rng);
        p[3] += o.ay * dt + velocity_noise * root_dt * normal(rng);
    }
    // この関数が呼ばれる時点では移動済み。欠測は0を返し、重みに情報を追加しない。
    double log_likelihood(const Particle& p, const Observation& o) const {
        if (!o.measured) return 0; // 欠測は追加情報なし。ただし遷移は既に実行済み。
        const double dx = o.x - p[0], dy = o.y - p[1];
        return -0.5 * (dx * dx + dy * dy) / (o.sigma * o.sigma)
             - std::log(2 * std::numbers::pi * o.sigma * o.sigma);
    }
};
// 出力: 各ターンの移動・観測後の[x,y,vx,vy]推定。initialは最初の移動より前。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<std::vector<std::array<double, 4>>> solve(
    const std::vector<Observation>& observations, std::array<double, 4> initial_mean,
    std::array<double, 4> initial_sd, double position_noise, double velocity_noise,
    int count = 1024, std::uint64_t seed = 0) {
    assert(position_noise >= 0 && velocity_noise >= 0);
    for (double s : initial_sd) assert(s >= 0);
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model{initial_mean, initial_sd, position_noise, velocity_noise},
                                 {.count = count}, std::mt19937_64(seed));
    std::vector<std::array<double, 4>> trajectory;
    for (const auto& o : observations) {
        assert(o.dt >= 0 && (!o.measured || o.sigma > 0));
        // 新しい観測を1回だけ追加する。失敗ならこの関数例では後続の観測を処理しない。
        if (!filter.observe(o).ok) return std::nullopt;
        trajectory.push_back(filter.estimate().mean);
    }
    return trajectory;
}
}
// END CASE12

// BEGIN CASE13
#include "particle_filter_v09.hpp"
namespace case13 {
// 真の温度は「前温度+加熱量+正規過程ノイズ」。測定は移動後の温度に対するもの。
struct Observation { double heating, measured_temperature, sigma; bool measured; };
struct Model {
    using Particle = double;
    using Observation = case13::Observation;
    double initial_mean, initial_sd, process_variance;
    static double log_normal(double x, double mean, double variance) {
        return -0.5 * ((x - mean) * (x - mean) / variance + std::log(2 * std::numbers::pi * variance));
    }
    Particle sample(std::mt19937_64& rng) {
        return initial_mean + initial_sd * std::normal_distribution<double>{}(rng);
    }
    // 高精度観測に合わせた次状態の候補を生成し、真の遷移/提案の対数比だけを返す。
    double propose_transition(Particle& candidate, const Particle& previous,
                              const Observation& o, std::mt19937_64& rng) {
        const double mean = previous + o.heating, q = process_variance;
        if (!o.measured) {
            candidate = mean + std::sqrt(q) * std::normal_distribution<double>{}(rng);
            return 0; // 真の遷移から生成したのでp/q=1
        }
        const double r = o.sigma * o.sigma;
        const double proposed_mean = mean + q / (q + r) * (o.measured_temperature - mean);
        const double proposed_variance = q * r / (q + r);
        candidate = proposed_mean + std::sqrt(proposed_variance) * std::normal_distribution<double>{}(rng);
        // 観測尤度をここへ足さない。ライブラリが次のlog_likelihoodを別途加える。
        return log_normal(candidate, mean, q) - log_normal(candidate, proposed_mean, proposed_variance);
    }
    // 提案補正とは別に、今回の温度測定の対数密度だけを返す。欠測なら0。
    double log_likelihood(Particle temperature, const Observation& o) const {
        return o.measured ? log_normal(o.measured_temperature, temperature, o.sigma * o.sigma) : 0;
    }
};
// 入力process_varianceは標準偏差ではなく分散Q>0。initial_sdは標準偏差。
// 出力: 各ターンの温度平均。狭い観測に合わせた提案を利用する発展例。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<std::vector<double>> solve(const std::vector<Observation>& observations,
                                       double initial_mean, double initial_sd, double process_variance,
                                       int count = 1024, std::uint64_t seed = 0) {
    assert(initial_sd >= 0 && process_variance > 0);
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model{initial_mean, initial_sd, process_variance},
                                 {.count = count}, std::mt19937_64(seed));
    std::vector<double> estimates;
    for (const auto& o : observations) {
        assert(!o.measured || o.sigma > 0);
        // 新しい観測を1回だけ追加する。失敗ならこの関数例では後続の観測を処理しない。
        if (!filter.observe(o).ok) return std::nullopt;
        estimates.push_back(filter.estimate().mean);
    }
    return estimates;
}
}
// END CASE13

// BEGIN CASE14
#include "particle_filter_v09.hpp"
namespace case14 {
struct Observation {
    enum class Kind { exact, below, above };
    Kind kind;
    double value; // exactは測定値、below/aboveは打ち切り閾値
    double sigma;
};
struct Model : UniformBox<1> {
    using Observation = case14::Observation;
    Model(double lower, double upper) : UniformBox({lower}, {upper}) {}
    // 通常表示は密度、上限・下限への張り付きは対応する側の全確率として扱う。
    double likelihood(const Particle& p, const Observation& o) const {
        const double z = (o.value - p[0]) / o.sigma;
        if (o.kind == Observation::Kind::below) return 0.5 * std::erfc(-z / std::sqrt(2.0));
        if (o.kind == Observation::Kind::above) return 0.5 * std::erfc(z / std::sqrt(2.0));
        return std::exp(-0.5 * z * z) / (o.sigma * std::sqrt(2 * std::numbers::pi));
    }
};
// 入力: 真の圧力muの事前範囲、各測定の値または「閾値以下/以上」という情報。
// 出力: muの平均。上限100への張り付きをexact=100に変換してはいけない。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<double> solve(const std::vector<Observation>& observations, double lower, double upper,
                            int count = 1024, std::uint64_t seed = 0) {
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model(lower, upper), {.count = count}, std::mt19937_64(seed));
    for (const auto& o : observations) {
        assert(o.sigma > 0);
        // 新しい観測を1回だけ追加する。失敗ならこの関数例では後続の観測を処理しない。
        if (!filter.observe(o).ok) return std::nullopt;
    }
    return filter.estimate().mean[0];
}
}
// END CASE14

// BEGIN CASE15
#include "particle_filter_v09.hpp"
namespace case15 {
// route_maskのbit j=1なら通路jを通る。全通路が開いていると経路は通行可能。
// successの表示が確率sensor_errorで反転する。通路状態は全期間一定。
struct Observation { std::uint32_t route_mask; bool success; };
struct Route { std::uint32_t route_mask; double success_cost, failure_cost; };
struct Model {
    using Particle = std::uint32_t; // bit j=1なら通路jは開いている
    using Observation = case15::Observation;
    int passages;
    double open_prior, sensor_error;
    Particle sample(std::mt19937_64& rng) {
        Particle p = 0;
        for (int j = 0; j < passages; ++j)
            if (std::bernoulli_distribution(open_prior)(rng)) p |= std::uint32_t{1} << j;
        return p;
    }
    double log_prior(Particle p) const {
        const int opened = std::popcount(p);
        return opened * std::log(open_prior) + (passages - opened) * std::log1p(-open_prior);
    }
    void propose(Particle& p, std::mt19937_64& rng) {
        p ^= std::uint32_t{1} << std::uniform_int_distribution<int>(0, passages - 1)(rng);
    }
    // 配置全体で経路が通れるか判定し、その結果から今回の表示成否の確率を求める。
    double likelihood(Particle p, const Observation& o) const {
        const bool passable = (p & o.route_mask) == o.route_mask;
        return passable == o.success ? 1 - sensor_error : sensor_error;
    }
};
struct Result {
    std::vector<double> open_probability; // 通路ごとの周辺確率
    std::vector<double> expected_cost;    // 通路間の相関も使って計算した候補経路の期待費用
    int selected_route;
};
// passagesは1..20程度を想定。先頭passagesビットだけを使う。
// 観測の反転率0<sensor_error<0.5、独立事前の開通率0<open_prior<1を指定。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<Result> solve(const std::vector<Observation>& observations, int passages,
                           double open_prior, double sensor_error, const std::vector<Route>& routes,
                           int count = 1024, std::uint64_t seed = 0) {
    assert(1 <= passages && passages <= 20 && !routes.empty());
    assert(0 < open_prior && open_prior < 1 && 0 < sensor_error && sensor_error < 0.5);
    const std::uint32_t allowed = (std::uint32_t{1} << passages) - 1;
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model{passages, open_prior, sensor_error},
                                 {.count = count}, std::mt19937_64(seed));
    for (const auto& o : observations) {
        assert((o.route_mask & ~allowed) == 0);
        // 新しい観測を1回だけ追加する。失敗ならこの関数例では後続の観測を処理しない。
        if (!filter.observe(o).ok) return std::nullopt;
    }
    for (const auto& r : routes) assert((r.route_mask & ~allowed) == 0);
    Result result{std::vector<double>(passages), std::vector<double>(routes.size()), 0};
    filter.for_each_weighted([&](std::uint32_t p, double weight) {
        for (int j = 0; j < passages; ++j) result.open_probability[j] += weight * ((p >> j) & 1);
        for (int r = 0; r < int(routes.size()); ++r) {
            const bool passable = (p & routes[r].route_mask) == routes[r].route_mask;
            result.expected_cost[r] += weight * (passable ? routes[r].success_cost : routes[r].failure_cost);
        }
    });
    result.selected_route = int(std::min_element(result.expected_cost.begin(), result.expected_cost.end()) - result.expected_cost.begin());
    return result;
}
}
// END CASE15

// BEGIN CASE16
#include "particle_filter_v09.hpp"
namespace case16 {
// regionは測定対象の地点番号。measuredは「領域内の最高温度」の測定値、sigmaは測定誤差の標準偏差。
// 各地点の温度は観測期間中一定。測定誤差は最高温度を求めた後に加わるとする。
struct Observation { std::vector<int> region; double measured, sigma; };
struct Model {
    struct Particle {
        std::vector<double> temperature;
        double log_prior_cache; // 温度に対応する対数事前密度。共通の定数だけ省く。
    };
    struct Undo { int cell; double temperature, log_prior_cache; };
    using Observation = case16::Observation;
    int cells;
    double prior_mean, prior_sd, step_scale;
    std::normal_distribution<double> normal;

    Model(int cells, double prior_mean, double prior_sd, double step_scale)
        : cells(cells), prior_mean(prior_mean), prior_sd(prior_sd), step_scale(step_scale) {
        assert(cells > 0 && prior_sd > 0 && step_scale > 0);
    }
    Particle sample(std::mt19937_64& rng) {
        Particle p{std::vector<double>(cells), 0};
        for (double& x : p.temperature) {
            x = prior_mean + prior_sd * normal(rng);
            const double z = (x - prior_mean) / prior_sd;
            p.log_prior_cache -= 0.5 * z * z;
        }
        return p;
    }
    double log_prior(const Particle& p) const { return p.log_prior_cache; }
    // 仮説の領域内最高温度を計算した後に測定誤差を置き、今回の表示の対数密度を返す。
    double log_likelihood(const Particle& p, const Observation& o) const {
        double highest = -std::numeric_limits<double>::infinity();
        for (int cell : o.region) highest = std::max(highest, p.temperature[cell]);
        const double z = (o.measured - highest) / o.sigma;
        return -0.5 * z * z - std::log(o.sigma) - 0.5 * std::log(2 * std::numbers::pi);
    }
    auto propose_in_place(Particle& p, std::mt19937_64& rng) {
        const int cell = std::uniform_int_distribution<int>(0, cells - 1)(rng);
        const Undo old{cell, p.temperature[cell], p.log_prior_cache};
        p.temperature[cell] += prior_sd * step_scale * normal(rng);
        const double before = (old.temperature - prior_mean) / prior_sd;
        const double after = (p.temperature[cell] - prior_mean) / prior_sd;
        p.log_prior_cache += 0.5 * (before * before - after * after);
        // 対称な候補生成なのでlog_correctionは既定値0。尤度や事前の差をここへ返さない。
        return ParticleCloud<Particle>::MoveProposal<Undo>{old};
    }
    void undo(Particle& p, const Undo& old) {
        p.temperature[old.cell] = old.temperature;
        p.log_prior_cache = old.log_prior_cache; // 棄却時にはキャッシュも元の値へ戻す。
    }
    const std::vector<double>& project(const Particle& p) const { return p.temperature; }
};
// 入力: 地点数、各領域の観測、各地点で独立な正規事前の平均と標準偏差。
// step_scaleは候補の移動幅/事前標準偏差。extra_movesは全観測後の追加再探索回数。
// 出力: 地点ごとの温度の事後平均。最高温度の平均が欲しい場合は、各粒子でmaxを取ってから平均する。
// countは推定に使う仮説の個数、seedは再現用。まず既定値で確認し、時間と誤差を見て調整する。
std::optional<std::vector<double>> solve(
    int cells, const std::vector<Observation>& observations, double prior_mean, double prior_sd,
    double step_scale = 0.4, int extra_moves = 4, int count = 1024, std::uint64_t seed = 0) {
    assert(extra_moves >= 0);
    // 事前から粒子を作る。未指定のParamは既定値。ターンごとにこの構築を繰り返さない。
    ParticleFilter<Model> filter(Model(cells, prior_mean, prior_sd, step_scale),
                                 {.count = count}, std::mt19937_64(seed));
    for (const auto& o : observations) {
        assert(!o.region.empty() && o.sigma > 0);
        for (int cell : o.region) assert(0 <= cell && cell < cells);
        // 新しい観測を1回だけ追加する。失敗ならこの関数例では後続の観測を処理しない。
        if (!filter.observe(o).ok) return std::nullopt;
    }
    filter.refine(extra_moves);
    std::vector<double> output(cells);
    filter.estimate_into(output);
    return output;
}
}
// END CASE16


// BEGIN HISTORY
#include "particle_filter_v09.hpp"
namespace history_example {
// 一定バイアスmuを、既知で共通の標準偏差sigmaの正規測定から推定する。
// 観測には、今回までのsum(y),sum(y*y),件数を含める。1件尤度はyだけを使う。
struct Observation { double y, sum, square_sum; int count; };
struct Model : UniformBox<1> {
    using Observation = history_example::Observation;
    double sigma;
    Model(double lower, double upper, double sd) : UniformBox({lower}, {upper}), sigma(sd) {}
    double log_likelihood(const Particle& p, const Observation& o) const {
        const double z = (o.y - p[0]) / sigma;
        return -0.5 * z * z - std::log(sigma) - 0.5 * std::log(2 * std::numbers::pi);
    }
    double log_likelihood_history(const Particle& p, std::span<const Observation> history) const {
        if (history.empty()) return 0;
        const auto& last = history.back();
        const double sse = last.square_sum - 2 * p[0] * last.sum + last.count * p[0] * p[0];
        return -sse / (2 * sigma * sigma) - last.count * (std::log(sigma) + 0.5 * std::log(2 * std::numbers::pi));
    }
};
// 出力はmuの平均。値のスケールは通常のAHC程度を想定。大きい共通オフセットは先に引く。
std::optional<double> solve(const std::vector<double>& measurements, double lower, double upper,
                            double sigma, int count = 1024, std::uint64_t seed = 0) {
    assert(sigma > 0);
    ParticleFilter<Model> filter(Model(lower, upper, sigma), {.count = count}, std::mt19937_64(seed));
    double sum = 0, squares = 0;
    int accepted = 0;
    for (double y : measurements) {
        const Observation next{y, sum + y, squares + y * y, accepted + 1};
        if (!filter.observe(next).ok) return std::nullopt;
        sum = next.sum; squares = next.square_sum; ++accepted;
    }
    return filter.estimate().mean[0];
}
}
// END HISTORY

// BEGIN WORKFLOW
// 5.1のcase01も一緒に使う例。モデルの式は同じで、継続・分岐・計算量調整を示す。
namespace workflow_example {
struct Result { std::array<double, 2> actual, hypothetical; bool branch_ok; };
std::optional<Result> solve(const std::vector<case01::Observation>& actual_observations,
                           const case01::Observation& hypothetical_observation) {
    // [-3,3]はa,bの事前範囲の例。実問題の既知範囲に直す。粒子数1024、seed 0で開始する。
    ParticleFilter<case01::Model> filter(case01::Model({-3, -3}, {3, 3}));
    for (const auto& o : actual_observations) {
        assert(o.sigma > 0);
        if (!filter.observe(o).ok) return std::nullopt;
    }
    // 次の設定値1,2,512は時間配分の例。観測の確率モデルを変える設定ではない。
    auto parameters = filter.param();
    parameters.move_steps = 1; // 今後の計算量を減らす。これだけなら粒子は変わらない。
    filter.set_param(parameters);
    filter.refine(2); // 同じ観測を再投入せず、今までの観測で追加探索する。
    const auto actual = filter.estimate().mean;
    auto branch = filter; // この先の仮観測は元のfilterへ反映しない。
    parameters.count = 512; // 分岐側だけ粒子数を減らす。即時再サンプリングされる。
    branch.set_param(parameters);
    assert(hypothetical_observation.sigma > 0);
    const bool ok = branch.observe(hypothetical_observation).ok;
    return Result{actual, branch.estimate().mean, ok};
}
}
// END WORKFLOW

// BEGIN SAMPLING
#include "particle_filter_v09.hpp"
// case15のModel、Observation、Routeも一緒に使う。
namespace sampled_routes {
struct Result { std::vector<double> expected_cost; int selected_route; };
// 入力: 観測済みのfilter、同じ通路番号を使う非空の候補経路、正の標本数、行動評価専用RNG。
// 出力: 現在の粒子分布から復元抽出した標本による平均費用と、最小費用の候補番号。
// filterは変更しない。返った後の次ターンも同じfilterへobserveできる。
Result evaluate(const ParticleFilter<case15::Model>& filter, const std::vector<case15::Route>& routes,
                int samples, std::mt19937_64& planning_rng) {
    assert(!routes.empty() && samples > 0);
    std::vector<int> indices(samples);
    filter.sample_indices(indices, planning_rng);
    const auto particles = filter.particles();
    Result result{std::vector<double>(routes.size()), 0};
    // 全候補を同じ標本で比較する。重複インデックスもそのまま使う。
    for (int index : indices) {
        const std::uint32_t map = particles[index];
        for (int r = 0; r < int(routes.size()); ++r) {
            const bool passable = (map & routes[r].route_mask) == routes[r].route_mask;
            result.expected_cost[r] += (passable ? routes[r].success_cost : routes[r].failure_cost) / samples;
        }
    }
    // 元のweights()[index]を再び掛けない。抽出頻度に重みが既に反映されている。
    result.selected_route = int(std::min_element(result.expected_cost.begin(), result.expected_cost.end())
                                - result.expected_cost.begin());
    return result;
}
// U15と同じ観測・事前・経路を指定する。samplesは粒子数countと独立に設定する。
// planning_seedは行動評価用のseedで、推定用のseedと役割を分ける。
std::optional<Result> solve(
    const std::vector<case15::Observation>& observations, int passages, double open_prior, double sensor_error,
    const std::vector<case15::Route>& routes, int samples = 64, int count = 1024,
    std::uint64_t seed = 0, std::uint64_t planning_seed = 100003) {
    assert(1 <= passages && passages <= 20 && !routes.empty() && samples > 0);
    assert(0 < open_prior && open_prior < 1 && 0 < sensor_error && sensor_error < 0.5);
    const std::uint32_t allowed = (std::uint32_t{1} << passages) - 1;
    ParticleFilter<case15::Model> filter(case15::Model{passages, open_prior, sensor_error},
                                         {.count = count}, std::mt19937_64(seed));
    for (const auto& o : observations) {
        assert((o.route_mask & ~allowed) == 0);
        if (!filter.observe(o).ok) return std::nullopt;
    }
    for (const auto& route : routes) assert((route.route_mask & ~allowed) == 0);
    std::mt19937_64 planning_rng(planning_seed);
    return evaluate(filter, routes, samples, planning_rng);
}
}
// END SAMPLING

// BEGIN BUFFER
#include "particle_filter_v09.hpp"
// case02のModel、Observation、Routeも一緒に使う。
namespace streaming_routes {
struct Turn {
    case02::Observation observation; // 今回走った経路の測定。各turnは新しい観測を1件含む。
    std::vector<case02::Route> candidates; // 観測後に比較する次の経路。毎ターン変更してよい。
};
struct Decision {
    std::vector<double> expected_time; // このターンの各候補の期待所要時間
    int selected_route;
    double selected_mass; // top_kを指定した場合の元の重みの合計。全粒子なら1。
};
// 入力: U02と同じ地域別時間の事前範囲。目的は期待時間だけの最小化。
// top_k=0が全粒子。上位kを使った結果は全分布の期待値とは違う。
// 出力: ターンごとの選択。地域コストの平均用バッファはturnをまたいで使い回す。
std::optional<std::vector<Decision>> solve(
    const std::vector<Turn>& turns, std::vector<double> lower, std::vector<double> upper,
    int top_k = 0, int count = 1024, std::uint64_t seed = 0) {
    assert(0 <= top_k && top_k <= count);
    const std::size_t groups = lower.size();
    ParticleFilter<case02::Model> filter(case02::Model(std::move(lower), std::move(upper)),
                                         {.count = count}, std::mt19937_64(seed));
    std::vector<double> mean(groups); // reserveだけでは不足。出力先の要素数を先に決める。
    std::vector<Decision> result;
    for (const auto& turn : turns) {
        assert(turn.observation.length.size() == groups && turn.observation.sigma > 0);
        assert(!turn.candidates.empty());
        if (!filter.observe(turn.observation).ok) return std::nullopt;
        const double mass = filter.estimate_into(mean, top_k); // ゼロクリアも含めて上書きされる。
        Decision decision{std::vector<double>(turn.candidates.size()), 0, mass};
        for (int r = 0; r < int(turn.candidates.size()); ++r) {
            const auto& route = turn.candidates[r];
            assert(route.length.size() == groups);
            decision.expected_time[r] = route.fixed
                + std::inner_product(mean.begin(), mean.end(), route.length.begin(), 0.0);
        }
        decision.selected_route = int(std::min_element(decision.expected_time.begin(), decision.expected_time.end())
                                       - decision.expected_time.begin());
        result.push_back(std::move(decision));
    }
    return result;
}
}
// END BUFFER

// BEGIN CLOUD
#include "particle_filter_v09.hpp"
namespace finite_types {
struct Result { std::vector<double> probability; int selected_type; double log_predictive_sum; };
// U06と同じ問題。全種類を1個ずつ登録し、再サンプリングせず有限分布を直接更新する。
// prior_weightは非負、総和が正。emission[type][symbol]は表示確率で、各行の和は1。
// 出力は種類別確率、最多の種類、全観測列の確率の対数。更新失敗ならnullopt。
std::optional<Result> solve(const std::vector<int>& symbols, const std::vector<double>& prior_weight,
                           const std::vector<std::vector<double>>& emission) {
    const int types = int(prior_weight.size());
    assert(types > 0 && int(emission.size()) == types && !emission[0].empty());
    assert(std::accumulate(prior_weight.begin(), prior_weight.end(), 0.0) > 0);
    const int alphabet = int(emission[0].size());
    std::vector<int> particles(types);
    std::iota(particles.begin(), particles.end(), 0);
    std::vector<double> log_weights(types);
    for (int i = 0; i < types; ++i) {
        assert(prior_weight[i] >= 0 && int(emission[i].size()) == alphabet);
        assert(std::abs(std::accumulate(emission[i].begin(), emission[i].end(), 0.0) - 1) < 1e-9);
        for (double p : emission[i]) assert(p >= 0);
        log_weights[i] = prior_weight[i] > 0 ? std::log(prior_weight[i])
                                           : -std::numeric_limits<double>::infinity();
    }
    ParticleCloud<int> cloud(0);
    const bool initialized = cloud.assign(std::move(particles), log_weights);
    assert(initialized); // assignの呼び出し自体をassert内へ入れない。
    (void)initialized;
    double evidence = 0;
    for (int symbol : symbols) {
        assert(0 <= symbol && symbol < alphabet);
        const auto update = cloud.update_log([&](const int& type) {
            const double p = emission[type][symbol];
            return p > 0 ? std::log(p) : -std::numeric_limits<double>::infinity();
        });
        if (!update.ok) return std::nullopt;
        evidence += update.log_normalizer;
    }
    const auto weights = cloud.weights();
    std::vector<double> probability(weights.begin(), weights.end());
    const int selected = int(std::max_element(probability.begin(), probability.end()) - probability.begin());
    return Result{std::move(probability), selected, evidence};
}
}
// END CLOUD

#if defined(PARTICLE_FILTER_USAGE_SELF_TEST)
struct UsageChecks {
    inline static int checks = 0;
    static void require(bool value) {
        ++checks;
        if (!value) { std::cerr << "failed check " << checks << '\n'; std::exit(1); }
    }
    static void near(double actual, double expected, double tolerance) {
        if (!(std::abs(actual - expected) <= tolerance))
            std::cerr << "actual=" << actual << " expected=" << expected << " tolerance=" << tolerance << '\n';
        require(std::abs(actual - expected) <= tolerance);
    }
    static void run(std::uint64_t seed) {
        std::mt19937_64 data(1000 + seed);
        std::normal_distribution<double> normal;
        std::vector<case01::Observation> calibration;
        for (int i = 0; i < 24; ++i) {
            const double x = (i % 7 - 3) * 0.5;
            calibration.push_back({x, 1.4 * x - 0.6 + 0.2 * normal(data), 0.2});
        }
        auto c1 = case01::solve(calibration, {-3,-3}, {3,3}, 128, 2048, seed);
        require(c1.has_value()); near(c1->mean[0], 1.4, 0.2); near(c1->mean[1], -0.6, 0.2);
        require(0 < c1->top_mass && c1->top_mass <= 1);
        auto work = workflow_example::solve(calibration, {1, 0.8, 0.2});
        require(work.has_value() && work->branch_ok);

        std::vector<case02::Observation> roads;
        for (int i = 0; i < 24; ++i) {
            const double a = 1 + i % 3, b = 0.5 + i % 4;
            roads.push_back({{a,b}, 0.7, 0.7 + 1.2*a + 2.8*b + 0.15*normal(data), 0.15});
        }
        auto c2 = case02::solve(roads, {0.2,0.2}, {4,4}, {{{2,0},0}, {{0,2},0}}, 4, 10, 2048, seed);
        require(c2.has_value()); near(c2->unit_time[0], 1.2, 0.2); near(c2->unit_time[1], 2.8, 0.2);
        require(c2->selected_route == 0 && c2->late_probability[0] < 0.1 && c2->late_probability[1] > 0.9);

        std::vector<case03::Observation> ranges;
        for (int i = 0; i < 24; ++i) {
            const double x = (i % 2 ? 3 : -3), y = (i % 4 < 2 ? -3 : 3);
            ranges.push_back({x,y,std::hypot(1.2-x,-0.7-y)+0.2*normal(data),0.2});
        }
        auto c3 = case03::solve(ranges, {-4,-4}, {4,4}, 2048, seed);
        require(c3.has_value()); near((*c3)[0],1.2,0.3); near((*c3)[1],-0.7,0.3);

        std::vector<case04::Observation> rounded;
        for (int i = 0; i < 36; ++i) {
            const double a = i % 3 == 0 ? 0 : 1, b = i % 3 == 1 ? 0 : 1;
            rounded.push_back({a,b,0.3,int(std::round(a*5.2+b*3.3+0.3*normal(data)))});
        }
        auto c4 = case04::solve(rounded, {1,1}, {8,8}, 2048, seed);
        require(c4.has_value()); near((*c4)[0],5.2,0.4); near((*c4)[1],3.3,0.4);
        case04::Model round_model({1,1},{8,8});
        double mass = 0;
        for (int i = -30; i <= 30; ++i) mass += round_model.likelihood({5.2,3.3},{1,0,0.8,i});
        near(mass,1,1e-12);

        auto c5 = case05::solve({{7,10},{6,10},{8,10}},2,3,4096,seed);
        require(c5.has_value()); near(c5->probability,23.0/35,0.035);
        near(c5->variance,23.0*12/(35*35*36),0.002);
        near(case05::Model{1,1}.log_likelihood(0,{0,10}),0,1e-12);
        near(case05::Model{1,1}.log_likelihood(1,{10,10}),0,1e-12);

        auto c6 = case06::solve({1,1,0,1},{0.3,0.7},{{0.8,0.2},{0.15,0.85}},4096,seed);
        require(c6.has_value());
        const double p0=0.3*0.2*0.2*0.8*0.2, p1=0.7*0.85*0.85*0.15*0.85;
        near(c6->probability[1],p1/(p0+p1),0.03); require(c6->selected_type==1);

        std::vector<case07::Observation> faults;
        for (int i=0;i<20;++i) { double x=0.5+i%4; faults.push_back({x,-1.5*x+0.2*normal(data),0.2}); }
        auto c7=case07::solve(faults,0.3,3,0.2,2048,seed);
        require(c7.has_value()); require(c7->reversed_probability>0.99); near(c7->mean_gain,1.5,0.15);

        std::vector<double> readings;
        for(int i=0;i<100;++i)readings.push_back(2+0.6*normal(data));
        auto c8=case08::solve(readings,-1,4,0.2,2,4096,seed);
        require(c8.has_value()); near((*c8)[0],2,0.25); near((*c8)[1],0.6,0.2);

        std::vector<case09::Observation> directions;
        for(int i=0;i<24;++i)directions.push_back({std::cos(3.1)+0.2*normal(data),std::sin(3.1)+0.2*normal(data),0.2});
        auto c9=case09::solve(directions,2048,seed);
        require(c9.has_value()); near(std::remainder(c9->angle-3.1,2*std::numbers::pi),0,0.2); require(c9->resultant>0.95);

        std::vector<case10::Observation> comparisons;
        for(int i=0;i<180;++i){
            std::array<double,3> d{normal(data),normal(data),normal(data)};
            double p=1/(1+std::exp(-(d[0]-0.8*d[1]+0.4*d[2])/0.7));
            comparisons.push_back({d,std::bernoulli_distribution(p)(data),0.7});
        }
        auto c10=case10::solve(comparisons,{-2,-2,-2},{2,2,2},1024,seed);
        require(c10.has_value()); near((*c10)[0],1,0.7); near((*c10)[1],-0.8,0.7); near((*c10)[2],0.4,0.7);
        case10::Model preference({-2,-2,-2},{2,2,2});
        near(std::exp(preference.log_likelihood({1,-0.8,0.4},{{1,2,3},true,0.7}))+
             std::exp(preference.log_likelihood({1,-0.8,0.4},{{1,2,3},false,0.7})),1,1e-12);

        auto c11=case11::solve({{5,2,3},{6,1,3},{4,3,3}},4096,seed);
        require(c11.has_value()); near((*c11)[0],16.0/33,0.04); near((*c11)[1],7.0/33,0.04); near((*c11)[2],10.0/33,0.04);
        near((*c11)[0]+(*c11)[1]+(*c11)[2],1,1e-12);

        std::vector<case12::Observation> tracking;
        for(int t=1;t<=20;++t)tracking.push_back({1,0,0,0.4*t+0.3*normal(data),-0.2*t+0.3*normal(data),0.3,t%4!=0});
        auto c12=case12::solve(tracking,{0,0,0.4,-0.2},{0.4,0.4,0.2,0.2},0.03,0.02,4096,seed);
        require(c12.has_value()&&c12->size()==tracking.size()); near(c12->back()[0],8,0.7);near(c12->back()[1],-4,0.7);
        near(c12->back()[2],0.4,0.25);near(c12->back()[3],-0.2,0.25);

        std::vector<case13::Observation> temperatures;
        std::vector<double> exact;
        double true_value=0,mean=0,variance=0;
        for(int t=0;t<25;++t){
            true_value+=0.1+std::sqrt(0.5)*normal(data);
            const bool measured=t%5!=0;
            const double y=true_value+0.1*normal(data);
            temperatures.push_back({0.1,y,0.1,measured});
            mean+=0.1;variance+=0.5;
            if(measured){double k=variance/(variance+0.01);mean+=k*(y-mean);variance*=1-k;}
            exact.push_back(mean);
        }
        auto c13=case13::solve(temperatures,0,0,0.5,4096,seed);
        require(c13.has_value()&&c13->size()==exact.size());
        for(int i=0;i<int(exact.size());++i)near((*c13)[i],exact[i],0.08);
        case13::Model guided{0,1,0.5};case13::Observation o{0.1,1.8,0.2,true};
        for(int i=0;i<32;++i){double candidate=0;double correction=guided.propose_transition(candidate,0.2,o,data);
            near(correction+guided.log_likelihood(candidate,o),case13::Model::log_normal(1.8,0.3,0.54),1e-11);}

        std::vector<case14::Observation> censored;
        for(int i=0;i<70;++i){double y=5.5+normal(data);censored.push_back({y>=6?case14::Observation::Kind::above:case14::Observation::Kind::exact,std::min(y,6.0),1});}
        auto c14=case14::solve(censored,2,9,2048,seed);require(c14.has_value());near(*c14,5.5,0.45);
        case14::Model pressure(2,9);
        near(pressure.likelihood({5},{case14::Observation::Kind::below,5,1}),0.5,1e-12);
        near(pressure.likelihood({5},{case14::Observation::Kind::above,5,1}),0.5,1e-12);

        const std::vector<case15::Observation> attempts{{1,true},{2,false},{3,false}};
        const std::vector<case15::Route> choices{{1,1,10},{2,0.5,10},{4,2,10}};
        auto c15=case15::solve(attempts,3,0.65,0.15,choices,4096,seed);require(c15.has_value());
        case15::Model map{3,0.65,0.15};std::array<double,3> marginal{},cost{};double total=0;
        for(std::uint32_t p=0;p<8;++p){double w=std::exp(map.log_prior(p));for(const auto& obs:attempts)w*=map.likelihood(p,obs);total+=w;
            for(int j=0;j<3;++j)marginal[j]+=w*((p>>j)&1);
            for(int j=0;j<3;++j)cost[j]+=w*((p&choices[j].route_mask)==choices[j].route_mask?choices[j].success_cost:choices[j].failure_cost);}
        for(int j=0;j<3;++j){near(c15->open_probability[j],marginal[j]/total,0.04);near(c15->expected_cost[j],cost[j]/total,0.4);}

        std::vector<history_example::Observation> history;
        double sum=0,square_sum=0;for(double y:std::vector<double>{1,1.2,0.8,1.1}){sum+=y;square_sum+=y*y;history.push_back({y,sum,square_sum,int(history.size())+1});}
        history_example::Model cached(-3,3,0.4);
        for(double mu:std::vector<double>{-2,-0.5,0,1,2.5}){double direct=0;for(const auto& h:history)direct+=cached.log_likelihood({mu},h);near(cached.log_likelihood_history({mu},history),direct,1e-11);}
        auto h=history_example::solve({1,1.2,0.8,1.1},-3,3,0.4,4096,seed);require(h.has_value());near(*h,1.025,0.04);
    }
};
struct AdditionalUsageChecks {
    static void run(std::uint64_t seed) {
        auto require = [](bool value) { UsageChecks::require(value); };
        auto near = [](double value, double expected, double tolerance) { UsageChecks::near(value, expected, tolerance); };

        // 差分提案のユーザーキャッシュとundoを、定義そのものの再計算と照合する。
        case16::Model model(32, 20, 3, 0.4);
        std::mt19937_64 rng(seed);
        auto p = model.sample(rng);
        const auto check_prior = [&](const case16::Model::Particle& candidate) {
            double direct = 0;
            for (double x : candidate.temperature) direct -= 0.5 * std::pow((x - 20) / 3, 2);
            near(model.log_prior(candidate), direct, 1e-10);
        };
        check_prior(p);
        for (int i = 0; i < 100; ++i) {
            const auto saved = p;
            auto proposal = model.propose_in_place(p, rng);
            check_prior(p);
            if (i % 2 == 0) {
                model.undo(p, proposal.undo);
                require(p.temperature == saved.temperature && p.log_prior_cache == saved.log_prior_cache);
            }
        }

        // 1地点なら正規事前と正規尤度の厳密な事後平均を計算できる。
        const auto temperature = case16::solve(1, {{{0}, 22, 1}}, 20, 3, 0.4, 8, 4096, seed);
        require(temperature.has_value() && temperature->size() == 1);
        near((*temperature)[0], 21.8, 0.15);
        const auto regional = case16::solve(3, {{{0, 1}, 22, 1}, {{1, 2}, 21, 1}}, 20, 3, 0.4, 4, 256, seed);
        require(regional.has_value() && regional->size() == 3);
        for (double x : *regional) require(std::isfinite(x));

        // 同じ提案をコピー方式で書いた独立なModelと、全粒子・重みを直接照合する。
        struct CopyModel {
            using Particle = case16::Model::Particle;
            using Observation = case16::Observation;
            case16::Model model;
            Particle sample(std::mt19937_64& r) { return model.sample(r); }
            double log_prior(const Particle& x) const { return model.log_prior(x); }
            double log_likelihood(const Particle& x, const Observation& o) const { return model.log_likelihood(x, o); }
            void propose(Particle& x, std::mt19937_64& r) { (void)model.propose_in_place(x, r); }
        };
        ParticleFilter<case16::Model> delta(case16::Model(32, 20, 3, 0.4), {.count = 128}, std::mt19937_64(seed));
        ParticleFilter<CopyModel> copy(CopyModel{case16::Model(32, 20, 3, 0.4)}, {.count = 128}, std::mt19937_64(seed));
        for (const auto& o : std::vector<case16::Observation>{{{0, 1}, 22, 1}, {{1, 2}, 23, 1}, {{0, 2, 3}, 21, 0.3}}) {
            const auto a = delta.observe(o);
            const auto b = copy.observe(o);
            require(a.ok && b.ok && a.moves.accepted == b.moves.accepted);
        }
        require(delta.refine(4).accepted == copy.refine(4).accepted);
        for (int i = 0; i < delta.size(); ++i) {
            require(delta.particles()[i].temperature == copy.particles()[i].temperature);
            near(delta.particles()[i].log_prior_cache, copy.particles()[i].log_prior_cache, 0);
            near(delta.weights()[i], copy.weights()[i], 0);
        }

        // 重み付き抽出の平均を、同じ粒子群での全粒子集計と比較する。
        using MapFilter = ParticleFilter<case15::Model>;
        MapFilter maps(case15::Model{3, 0.65, 0.15}, {.count = 256}, std::mt19937_64(seed));
        require(maps.observe({1, true}).ok);
        const std::vector<case15::Route> routes{{1, 1, 10}, {2, 0.5, 10}, {4, 2, 10}};
        std::vector<double> exact(routes.size());
        maps.for_each_weighted([&](std::uint32_t map, double weight) {
            for (int i = 0; i < int(routes.size()); ++i) {
                const bool passable = (map & routes[i].route_mask) == routes[i].route_mask;
                exact[i] += weight * (passable ? routes[i].success_cost : routes[i].failure_cost);
            }
        });
        auto untouched = maps;
        std::mt19937_64 planning_rng(seed + 100003);
        const auto sampled = sampled_routes::evaluate(maps, routes, 32768, planning_rng);
        for (int i = 0; i < int(routes.size()); ++i) near(sampled.expected_cost[i], exact[i], 0.15);
        require(sampled.selected_route == 0);
        const auto sampled_solve = sampled_routes::solve({{1, true}}, 3, 0.65, 0.15, routes, 128, 256, seed);
        require(sampled_solve.has_value());
        require(maps.observation_count() == untouched.observation_count());
        require(maps.observe({2, false}).ok && untouched.observe({2, false}).ok);
        for (int i = 0; i < maps.size(); ++i) {
            require(maps.particles()[i] == untouched.particles()[i]);
            near(maps.weights()[i], untouched.weights()[i], 0); // 推定用RNGも同じ続きを保つ。
        }

        // バッファをまたいだ平均が、粒子ごとに計算した経路時間の平均と一致する。
        std::vector<streaming_routes::Turn> turns{
            {{{1, 0}, 0, 1.2, 0.3}, {{{1, 2}, 0.1}, {{2, 1}, 0.2}}},
            {{{0, 1}, 0, 2.8, 0.3}, {{{0, 2}, 0}, {{2, 0}, 0}}}
        };
        auto decisions = streaming_routes::solve(turns, {0.2, 0.2}, {4, 4}, 0, 256, seed);
        require(decisions.has_value() && decisions->size() == turns.size());
        ParticleFilter<case02::Model> reference(case02::Model({0.2, 0.2}, {4, 4}),
                                               {.count = 256}, std::mt19937_64(seed));
        for (int t = 0; t < int(turns.size()); ++t) {
            require(reference.observe(turns[t].observation).ok);
            near((*decisions)[t].selected_mass, 1, 0);
            for (int r = 0; r < int(turns[t].candidates.size()); ++r) {
                const auto& route = turns[t].candidates[r];
                double time = 0;
                reference.for_each_weighted([&](const auto& costs, double weight) {
                    time += weight * (route.fixed + costs[0] * route.length[0] + costs[1] * route.length[1]);
                });
                near((*decisions)[t].expected_time[r], time, 1e-12);
            }
        }
        require(streaming_routes::solve({}, {0.2, 0.2}, {4, 4})->empty());
        const auto top_decisions = streaming_routes::solve(turns, {0.2, 0.2}, {4, 4}, 32, 256, seed);
        require(top_decisions.has_value() && 0 < top_decisions->back().selected_mass
                && top_decisions->back().selected_mass <= 1);

        // 有限候補を登録する例は、直接のベイズ更新と一致する。
        const auto finite = finite_types::solve({1, 1, 0, 1}, {0.3, 0.7}, {{0.8, 0.2}, {0.15, 0.85}});
        const double a = 0.3 * 0.2 * 0.2 * 0.8 * 0.2, b = 0.7 * 0.85 * 0.85 * 0.15 * 0.85;
        require(finite.has_value() && finite->selected_type == 1);
        near(finite->probability[0], a / (a + b), 1e-12);
        near(finite->log_predictive_sum, std::log(a + b), 1e-12);
        const auto zero_prior = finite_types::solve({0}, {0, 2}, {{1, 0}, {0.5, 0.5}});
        require(zero_prior.has_value());
        near(zero_prior->probability[1], 1, 0);
        require(!finite_types::solve({1}, {1, 0}, {{1, 0}, {0, 1}}));
    }
};

int main() {
    for (std::uint64_t seed : {0, 1, 17}) {
        UsageChecks::run(seed);
        AdditionalUsageChecks::run(seed);
    }
    std::cout << "PASS: 16 cases and 5 application examples; " << UsageChecks::checks << " checks\n";
}
#endif
