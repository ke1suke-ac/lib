#pragma once
#include "linear_estimator_v06.hpp"

namespace linear_estimator_examples {

// BEGIN COMMON
using LE = LinearEstimator<>;

template <class Estimator = LE>
struct Estimate {
    // feasible=falseならbestは空。count=0ならmean/varianceは空。
    bool feasible = false;
    std::vector<double> best, mean, variance;
    double best_energy = INFINITY;
    int64_t count = 0;
    typename Estimator::Runtime runtime;
};

template <class Estimator>
Estimate<Estimator> read_result(const Estimator& model, typename Estimator::Runtime runtime) {
    // 実行メソッドの戻り値はRuntimeだけ。解と標準統計は推定器から別に読む。
    Estimate<Estimator> result;
    result.runtime = runtime;
    result.feasible = model.has_solution();
    if (result.feasible) {
        const auto best = model.best();
        result.best.assign(best.begin(), best.end());
        result.best_energy = model.best_energy();
    }
    const auto& moments = model.moments();
    // 標準統計を無効にした場合もcount=0。無効な平均・分散を出力しない。
    result.count = moments.count;
    if (moments.count > 0) {
        result.mean = moments.mean;
        result.variance = moments.variance;
    }
    return result;
}

template <class Estimator>
void add_normal_priors(Estimator& model, std::span<const double> mean, double sigma) {
    assert(std::isfinite(sigma) && sigma > 0);
    for (int j = 0; j < static_cast<int>(mean.size()); ++j) {
        model.add_factor({{{j, 1}}, 0}, Estimator::Loss::gaussian(mean[j], sigma));
    }
}

struct SumObservation {
    std::vector<int> items; // 合計する変数ID。経路なら辺ID。重複はその回数だけ加算。
    double value;           // 観測された合計値。
    double sigma;           // この合計観測に加わる正規ノイズの標準偏差。
};

struct FeatureObservation {
    std::vector<double> features; // 既知の特徴量。切片用の1は含めない。
    double value;                 // 観測値。
    double scale;                 // 正規なら標準偏差。頑健推定では選んだ分布の尺度。
};

// END COMMON

// BEGIN U01
Estimate<> estimate_group_weights(const std::vector<double>& prior_mean,
                                  double prior_sigma,
                                  int64_t max_weight,
                                  const std::vector<SumObservation>& observations,
                                  LE::SampleParam param = {},
                                  uint64_t seed = 0) {
    // 入力: n個の重量は整数0..max_weight。prior_meanの長さがn。
    assert(!prior_mean.empty() && max_weight >= 0);
    // 1. 変数IDはprior_meanの位置。各重量の必須の値域を指定する。
    LE model(std::vector<LE::Domain>(prior_mean.size(), LE::Domain::integer(0, max_weight)), seed);
    // 2. 事前の目安を各変数の因子へ。整数上の正規形の重みとして働く。
    add_normal_priors(model, prior_mean, prior_sigma);
    // 3. 計測1行につき、合計の一次式と計測誤差モデルを登録する。
    for (const auto& row : observations) {
        model.add_factor(LE::LinearForm::sum(row.items), LE::Loss::gaussian(row.value, row.sigma));
    }
    // 出力: bestは実行可能な整数候補。meanは整数とは限らない。
    return read_result(model, model.sample(param));
}

// END U01

// BEGIN U02
struct RouteDecision {
    Estimate<> estimate;
    int route = -1; // 候補経路の添字。推定候補がなければ-1。
    bool used_samples = false;
    std::vector<LE::Prediction> predictions; // count=0ならそのmean/varianceは無効。
};

RouteDecision choose_route(const std::vector<double>& prior_edge_cost,
                           double prior_sigma,
                           double max_edge_cost,
                           const std::vector<SumObservation>& observed_routes,
                           const std::vector<std::vector<int>>& candidate_routes,
                           LE::SampleParam param,
                           uint64_t seed = 0) {
    // 入力: 辺IDは0..n-1。経路探索は呼出し側が行い、比較したい経路を渡す。
    // predictを使うため、呼出し側でparam.sample_capacityを正にする。
    assert(!prior_edge_cost.empty() && !candidate_routes.empty() && max_edge_cost > 0);
    assert(param.sample_capacity > 0);
    // 辺費用を実数変数にする。接続関係や候補経路は呼出し側で用意する。
    LE model(std::vector<LE::Domain>(prior_edge_cost.size(), LE::Domain::real(0, max_edge_cost)), seed);
    add_normal_priors(model, prior_edge_cost, prior_sigma);
    for (const auto& row : observed_routes) {
        model.add_factor(LE::LinearForm::sum(row.items), LE::Loss::gaussian(row.value, row.sigma));
    }
    RouteDecision result;
    // sample_capacity個の全体標本を保存し、後で経路全体を同じ標本上で評価する。
    result.estimate = read_result(model, model.sample(param));
    if (!result.estimate.feasible) return result;
    double minimum = INFINITY;
    for (int k = 0; k < static_cast<int>(candidate_routes.size()); ++k) {
        const auto form = LE::LinearForm::sum(candidate_routes[k]);
        const auto prediction = model.predict(form);
        // この分散は潜在的な経路時間の分散。次回の計測ノイズは含まない。
        result.predictions.push_back(prediction);
        result.used_samples = prediction.count > 0;
        const double score = prediction.count > 0 ? prediction.mean : form.evaluate(model.best());
        if (score < minimum) {
            minimum = score;
            result.route = k;
        }
    }
    // 保存標本が0ならbestによる経路費用へ切替。分散を0とみなしてはいけない。
    return result;
}

// END U02

// BEGIN U03
Estimate<> fit_feature_costs(const std::vector<double>& coefficient_prior,
                             double prior_sigma,
                             const std::vector<FeatureObservation>& observations,
                             LE::OptimizeParam param = {},
                             uint64_t seed = 0) {
    // 入力/出力の係数順: [切片, 特徴0の係数, 特徴1の係数, ...]。
    assert(!coefficient_prior.empty());
    // この関数は係数を実数全域にする。必須の符号・上限があるならここを変更する。
    LE model(std::vector<LE::Domain>(coefficient_prior.size(), LE::Domain::real()), seed);
    add_normal_priors(model, coefficient_prior, prior_sigma);
    for (const auto& row : observations) {
        assert(row.features.size() + 1 == coefficient_prior.size());
        // 切片も未知なので変数0。既知の定数項offsetへ入れない。
        LE::LinearForm form{{{0, 1}}, 0};
        for (int k = 0; k < static_cast<int>(row.features.size()); ++k) {
            form.terms.push_back({k + 1, row.features[k]});
        }
        model.add_factor(std::move(form), LE::Loss::gaussian(row.value, row.scale));
    }
    // bestで未知タスクの費用を予測できる。optimizeなのでmean/varianceは返さない。
    return read_result(model, model.optimize(param));
}

// END U03

// BEGIN U04
struct ComparisonObservation {
    std::vector<int> left, right;
    bool left_greater;
    double sigma; // 左右の「差」に加わるノイズの標準偏差。正の有限値。
};

Estimate<> infer_from_comparisons(const std::vector<double>& prior_mean,
                                  double prior_sigma,
                                  double bound,
                                  const std::vector<ComparisonObservation>& observations,
                                  LE::SampleParam param = {},
                                  uint64_t seed = 0) {
    // 入力: 二択回答で、同点という回答はないモデル。未知値は実数[-bound,bound]。
    assert(!prior_mean.empty() && bound > 0);
    LE model(std::vector<LE::Domain>(prior_mean.size(), LE::Domain::real(-bound, bound)), seed);
    add_normal_priors(model, prior_mean, prior_sigma);
    for (const auto& row : observations) {
        // 左-右+ノイズが正か負かという観測。真の差へのhard制約ではない。
        const auto loss = row.left_greater ? LE::Loss::gaussian_interval(0, INFINITY, row.sigma)
                                           : LE::Loss::gaussian_interval(-INFINITY, 0, row.sigma);
        model.add_factor(LE::LinearForm::difference(row.left, row.right), loss);
    }
    return read_result(model, model.sample(param));
}

// END U04

// BEGIN U05
Estimate<> estimate_quantized_sizes(const std::vector<double>& prior_mean,
                                    double prior_sigma,
                                    int64_t max_size,
                                    const std::vector<SumObservation>& observations,
                                    double step,
                                    double clip_min,
                                    double clip_max,
                                    LE::SampleParam param = {},
                                    uint64_t seed = 0) {
    // 観測生成: 合計+正規ノイズ -> step単位で四捨五入 -> [clip_min,clip_max]へ制限。
    // clip_min/clip_max/valueはstepの整数倍。潜在サイズは整数0..max_size。
    assert(!prior_mean.empty() && max_size >= 0 && step > 0 && clip_min < clip_max);
    const auto on_grid = [step](double value) {
        return std::abs(value / step - std::round(value / step)) < 1e-8;
    };
    assert(on_grid(clip_min) && on_grid(clip_max));
    LE model(std::vector<LE::Domain>(prior_mean.size(), LE::Domain::integer(0, max_size)), seed);
    add_normal_priors(model, prior_mean, prior_sigma);
    for (const auto& row : observations) {
        assert(row.value >= clip_min && row.value <= clip_max && on_grid(row.value));
        // 表示値を生む、ノイズ付きの丸め前の値の範囲へ逆変換する。
        // 表示上限なら「それ以上」の情報だけなので、上端を無限大にする。
        const double lo = row.value == clip_min ? -INFINITY : row.value - step / 2;
        const double hi = row.value == clip_max ? INFINITY : row.value + step / 2;
        model.add_factor(LE::LinearForm::sum(row.items), LE::Loss::gaussian_interval(lo, hi, row.sigma));
    }
    return read_result(model, model.sample(param));
}

// END U05

// BEGIN U06
enum class RobustNoise { Laplace, StudentT };

Estimate<> fit_robust_feature_costs(const std::vector<double>& coefficient_prior,
                                    double prior_sigma,
                                    const std::vector<FeatureObservation>& observations,
                                    RobustNoise noise,
                                    double df = 4,
                                    LE::SampleParam param = {},
                                    uint64_t seed = 0) {
    // 係数順は[切片, 特徴0, ...]。row.scaleは分布の尺度で、一般には標準偏差ではない。
    // dfはStudentTだけで使用し、正にする。小さいほど裾が重い。
    assert(!coefficient_prior.empty());
    LE model(std::vector<LE::Domain>(coefficient_prior.size(), LE::Domain::real()), seed);
    add_normal_priors(model, coefficient_prior, prior_sigma);
    for (const auto& row : observations) {
        assert(row.features.size() + 1 == coefficient_prior.size());
        LE::LinearForm form{{{0, 1}}, 0};
        for (int k = 0; k < static_cast<int>(row.features.size()); ++k) {
            form.terms.push_back({k + 1, row.features[k]});
        }
        // 特徴の一次式は通常の回帰と同じ。外れ値の影響は誤差モデルで変える。
        const auto loss = noise == RobustNoise::Laplace ? LE::Loss::laplace(row.value, row.scale)
                                                        : LE::Loss::student_t(row.value, row.scale, df);
        model.add_factor(std::move(form), loss);
    }
    return read_result(model, model.sample(param));
}

// END U06

// BEGIN U07
struct CatalogPrior {
    // 変数ごとの(value, 事前重み)。各変数でvalueは重複不可、重みは正の有限値。
    std::vector<std::vector<std::pair<double, double>>> entries;

    double operator()(int id, double z) const {
        for (const auto& [value, weight] : entries[id]) {
            if (z == value) return -std::log(weight);
        }
        return INFINITY;
    }
};

using CatalogEstimator = LinearEstimator<CatalogPrior>;

struct CatalogResult {
    Estimate<CatalogEstimator> estimate;
    // 入力entriesと同じ順。採取0件なら外側vectorも空。
    std::vector<std::vector<double>> probability;
};

CatalogResult infer_catalog_values(CatalogPrior prior,
                                   const std::vector<SumObservation>& observations,
                                   CatalogEstimator::SampleParam param = {},
                                   uint64_t seed = 0) {
    using E = CatalogEstimator;
    assert(!prior.entries.empty());
    std::vector<E::Domain> domains;
    for (const auto& entries : prior.entries) {
        std::vector<double> values;
        for (const auto& [value, weight] : entries) {
            assert(std::isfinite(weight) && weight > 0);
            values.push_back(value);
        }
        auto sorted = values;
        std::sort(sorted.begin(), sorted.end());
        assert(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
        domains.push_back(E::Domain::values(std::move(values)));
    }
    E model(std::move(domains), seed, prior);
    // 値域は「存在する候補」、custom因子は「各候補の選ばれやすさ」を担当する。
    for (int j = 0; j < static_cast<int>(prior.entries.size()); ++j) {
        model.add_factor({{{j, 1}}, 0}, E::Loss::custom(j));
    }
    for (const auto& row : observations) {
        model.add_factor(E::LinearForm::sum(row.items), E::Loss::gaussian(row.value, row.sigma));
    }
    std::vector<std::vector<double>> counts;
    for (const auto& entries : prior.entries)
        counts.emplace_back(entries.size(), 0);
    int64_t count = 0;
    // 候補確率はhookで数える。標準の平均・分散や保存容量には依存しない。
    const auto runtime = model.sample(param, [&](const E::SampleView& sample) {
        ++count;
        for (size_t j = 0; j < prior.entries.size(); ++j) {
            for (size_t k = 0; k < prior.entries[j].size(); ++k) {
                if (sample.values[j] == prior.entries[j][k].first) ++counts[j][k];
            }
        }
    });
    CatalogResult result;
    result.estimate = read_result(model, runtime);
    if (count > 0) {
        for (auto& row : counts)
            for (double& value : row)
                value /= static_cast<double>(count);
        result.probability = std::move(counts);
    }
    return result;
}

// END U07

// BEGIN U08
struct VertexObservation {
    int vertex;
    double value, sigma;
};

Estimate<> estimate_smooth_field(const std::vector<double>& prior_mean,
                                 double prior_sigma,
                                 const std::vector<std::pair<int, int>>& edges,
                                 double difference_sigma,
                                 const std::vector<VertexObservation>& observations,
                                 LE::SampleParam param = {},
                                 uint64_t seed = 0) {
    // 入力: n頂点の既知グラフ。difference_sigmaは隣接値の差に対する事前標準偏差。
    // 各頂点の有限な事前分散が、未観測の連結成分の位置も定める。
    assert(!prior_mean.empty());
    LE model(std::vector<LE::Domain>(prior_mean.size(), LE::Domain::real()), seed);
    add_normal_priors(model, prior_mean, prior_sigma);
    // 隣接差0は計測値ではなく、「近隣は似る」という事前情報の中心。
    for (const auto& [u, v] : edges) {
        model.add_factor({{{u, 1}, {v, -1}}, 0}, LE::Loss::gaussian(0, difference_sigma));
    }
    for (const auto& row : observations) {
        // センサーのない頂点に架空の観測を置く必要はない。
        model.add_factor({{{row.vertex, 1}}, 0}, LE::Loss::gaussian(row.value, row.sigma));
    }
    return read_result(model, model.sample(param));
}

// END U08

// BEGIN U09
Estimate<> infer_conserved_inventory(const std::vector<double>& prior_mean,
                                     double prior_sigma,
                                     int64_t max_per_item,
                                     int64_t total,
                                     const std::vector<double>& feasible_initial,
                                     const std::vector<SumObservation>& observations,
                                     LE::SampleParam param = {},
                                     uint64_t seed = 0) {
    // 入力: initial[j]は整数0..max_per_itemで、その合計がtotal。
    // 出力bestも同じ総数を保つ。meanを個別に丸めると総数を壊すことがある。
    assert(!prior_mean.empty() && feasible_initial.size() == prior_mean.size() && max_per_item > 0);
    const double total_value = static_cast<double>(total);
    assert(std::accumulate(feasible_initial.begin(), feasible_initial.end(), 0.0) == total_value);
    LE model(std::vector<LE::Domain>(prior_mean.size(), LE::Domain::integer(0, max_per_item)), seed);
    add_normal_priors(model, prior_mean, prior_sigma);
    std::vector<int> all(prior_mean.size());
    std::iota(all.begin(), all.end(), 0);
    // 正確な総数はhard等式。sigma=0の正規因子にはしない。
    model.add_factor(LE::LinearForm::sum(all), LE::Loss::hard_interval(total_value, total_value));
    model.set_state(feasible_initial);
    std::vector<std::vector<LE::Term>> directions;
    for (int j = 1; j < static_cast<int>(prior_mean.size()); ++j) {
        // 前の色をt増やし、次の色をt減らす。この方向なら総数は変わらない。
        directions.push_back({{j - 1, 1}, {j, -1}});
    }
    model.set_directions(std::move(directions));
    param.auto_directions = false; // この例は明示した隣接移送方向を使う。
    assert(prior_mean.size() == 1 || param.direction_probability > 0);
    for (const auto& row : observations) {
        model.add_factor(LE::LinearForm::sum(row.items), LE::Loss::gaussian(row.value, row.sigma));
    }
    return read_result(model, model.sample(param));
}

// END U09

// BEGIN U10
struct RangeObservation {
    LE::LinearForm form;
    double lower, upper, scale; // 許容範囲と、範囲外のずれを評価する尺度。
};

Estimate<> fit_tolerance_ranges(const std::vector<double>& preferred,
                                double preference_sigma,
                                double lower_bound,
                                double upper_bound,
                                const std::vector<RangeObservation>& ranges,
                                LE::OptimizeParam param = {},
                                uint64_t seed = 0) {
    // 入力: 各係数の希望値preferredと、一次式ごとの許容範囲。
    // 範囲はsoft制約なので、互いに矛盾していても違反量を比較できる。
    assert(!preferred.empty());
    // 値域は必須条件。下のsoft範囲は、外れた量を評価する希望条件。
    LE model(std::vector<LE::Domain>(preferred.size(), LE::Domain::real(lower_bound, upper_bound)), seed);
    add_normal_priors(model, preferred, preference_sigma);
    for (const auto& row : ranges) {
        model.add_factor(row.form, LE::Loss::soft_interval(row.lower, row.upper, row.scale));
    }
    return read_result(model, model.optimize(param));
}

// END U10

// BEGIN U11
struct BinaryObservation {
    std::vector<double> features; // 切片用の1を除く既知特徴。
    bool success;
};

struct LogisticLoss {
    std::vector<int> labels;

    double operator()(int id, double z) const {
        // -log P(label | logit=z)。大きい正負のzでもexpのオーバーフローを避ける。
        return std::max(z, 0.0) - labels[id] * z + std::log1p(std::exp(-std::abs(z)));
    }
};

using LogisticEstimator = LinearEstimator<LogisticLoss>;

struct LogisticResult {
    Estimate<LogisticEstimator> estimate;
    int64_t prediction_count = 0;
    double mean_probability = 0; // prediction_count=0なら無効。
};

LogisticResult predict_success_probability(const std::vector<double>& coefficient_prior,
                                           double prior_sigma,
                                           const std::vector<BinaryObservation>& observations,
                                           const std::vector<double>& query_features,
                                           LogisticEstimator::SampleParam param = {},
                                           uint64_t seed = 0) {
    using E = LogisticEstimator;
    assert(!coefficient_prior.empty() && query_features.size() + 1 == coefficient_prior.size());
    LogisticLoss loss;
    for (const auto& row : observations)
        loss.labels.push_back(row.success ? 1 : 0);
    E model(std::vector<E::Domain>(coefficient_prior.size(), E::Domain::real()), seed, std::move(loss));
    // 係数の極端な値を抑える事前。各試行の尤度とは別の因子にする。
    add_normal_priors(model, coefficient_prior, prior_sigma);
    for (int id = 0; id < static_cast<int>(observations.size()); ++id) {
        const auto& row = observations[id];
        assert(row.features.size() + 1 == coefficient_prior.size());
        E::LinearForm form{{{0, 1}}, 0};
        for (int k = 0; k < static_cast<int>(row.features.size()); ++k) {
            form.terms.push_back({k + 1, row.features[k]});
        }
        model.add_factor(std::move(form), E::Loss::custom(id));
    }
    LogisticResult result;
    // 次の操作の成功確率を、係数候補ごとに計算してから平均する。
    const auto runtime = model.sample(param, [&](const E::SampleView& sample) {
        double z = sample.values[0];
        for (size_t k = 0; k < query_features.size(); ++k)
            z += query_features[k] * sample.values[k + 1];
        const double probability = z >= 0 ? 1 / (1 + std::exp(-z)) : std::exp(z) / (1 + std::exp(z));
        ++result.prediction_count;
        result.mean_probability +=
            (probability - result.mean_probability) / static_cast<double>(result.prediction_count);
    });
    result.estimate = read_result(model, runtime);
    return result;
}

// END U11

// BEGIN U12
struct RelativeUniformLoss {
    std::vector<std::pair<double, double>> observations; // (観測値y, 相対誤差幅epsilon)。

    double operator()(int id, double z) const {
        const auto [y, epsilon] = observations[id];
        if (!(z > 0) || y < (1 - epsilon) * z || y > (1 + epsilon) * z) return INFINITY;
        return std::log(z) + std::log(2 * epsilon);
    }
};

using RelativeEstimator = LinearEstimator<RelativeUniformLoss>;

Estimate<RelativeEstimator> infer_relative_measurement(RelativeUniformLoss loss,
                                                       double lower,
                                                       double upper,
                                                       double feasible_initial,
                                                       double prior_mean,
                                                       double prior_sigma,
                                                       RelativeEstimator::SampleParam param = {},
                                                       uint64_t seed = 0) {
    // 生成モデル: y=z*(1+u)、uは[-epsilon,epsilon]の一様乱数。
    // 入力initialは全観測の支持区間を満たす正の実数。custom支持の自動修復はない。
    using E = RelativeEstimator;
    assert(lower > 0 && lower < upper);
    for (const auto& [y, epsilon] : loss.observations) {
        assert(y > 0 && epsilon > 0 && epsilon < 1);
    }
    const int count = static_cast<int>(loss.observations.size());
    E model({E::Domain::real(lower, upper)}, seed, std::move(loss));
    model.add_factor({{{0, 1}}, 0}, E::Loss::gaussian(prior_mean, prior_sigma));
    for (int id = 0; id < count; ++id)
        model.add_factor({{{0, 1}}, 0}, E::Loss::custom(id));
    const std::array<double, 1> initial{feasible_initial};
    // customの支持は自動修復されない。全計測と両立する初期値が必要。
    assert(std::isfinite(model.evaluate(initial)));
    model.set_state(initial);
    return read_result(model, model.sample(param));
}

// END U12

// BEGIN U13
struct SplitObservation {
    double first_coefficient, second_coefficient, offset, value, sigma;
};

Estimate<> infer_real_split(double total,
                            const std::array<double, 2>& prior_mean,
                            double prior_sigma,
                            const std::vector<SplitObservation>& observations,
                            LE::SampleParam param = {},
                            uint64_t seed = 0) {
    // 入力: 実数x0,x1>=0かつx0+x1=total。x1をtotal-x0へ置換し、自由変数1個で解く。
    assert(total > 0);
    LE model({LE::Domain::real(0, total)}, seed);
    // 元の2変数の事前を、x0とtotal-x0の各一次式へ変換する。
    model.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(prior_mean[0], prior_sigma));
    model.add_factor({{{0, -1}}, total}, LE::Loss::gaussian(prior_mean[1], prior_sigma));
    for (const auto& row : observations) {
        // a*x0+b*x1+c = (a-b)*x0+(c+b*total)。定数の変化も忘れない。
        LE::LinearForm form{{{0, row.first_coefficient - row.second_coefficient}},
                            row.offset + row.second_coefficient * total};
        model.add_factor(std::move(form), LE::Loss::gaussian(row.value, row.sigma));
    }
    auto result = read_result(model, model.sample(param));
    // 出力は元の2変数の順へ復元する。2変数の共分散は-variance[0]。
    if (result.feasible) result.best.push_back(total - result.best[0]);
    if (result.count > 0) {
        result.mean.push_back(total - result.mean[0]);
        result.variance.push_back(result.variance[0]);
    }
    return result;
}

// END U13

} // namespace linear_estimator_examples
