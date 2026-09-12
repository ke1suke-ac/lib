// McmcEstimator guide v06: mcmc_estimator_v09.hppに対応。
// MCMC_GUIDE_EXAMPLE: 基本U01〜U17は1〜17、U16-A/B/Cは161/162/163。既定はU01。
// 例: g++ -std=c++20 -O2 -DMCMC_GUIDE_EXAMPLE=161 mcmc_estimator_guide_examples_v06.cpp -o mcmc_guide_demo_v06
#ifndef MCMC_GUIDE_EXAMPLE
#define MCMC_GUIDE_EXAMPLE 1
#endif

#if MCMC_GUIDE_EXAMPLE == 1
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

// 入力: input=加えた電圧x>=0、value=表示値y、scale=既知の測定誤差。
// query: 次に予測したい電圧。budget: 今回の計算予算。
// 出力: [増加幅a, 曲がり方k, 基準値b, queryでの予測]。未評価ならnullopt。
std::optional<std::array<double,4>> fit_response(
    std::vector<Row> rows, double query, Solver::Budget budget) {
    Solver::Parameters p;
    const auto a = p.real(2, 0.1, 10);  // 初期値、下限、上限。幅は自動設定。
    const auto k = p.real(1, 0.1, 10);  // k>0にして、分母が0になることを防ぐ。
    const auto b = p.real(0, -5, 5);
    auto predict = [a,k,b](double x, const McmcState& s) {
        assert(x >= 0);
        return s.real[b] + s.real[a]*x/(s.real[k]+x);
    };
    auto model = Solver::make_observation_model(std::move(rows), predict);
    auto fit = Solver::make_numeric_session(p, std::move(model));
    const auto r = fit.solve(budget);   // 初期評価と探索をまとめて行う。
    if (!r.state) return std::nullopt;
    const auto& s = *r.state;          // rが所有する最良候補。
    return std::array<double,4>{s.real[a], s.real[k], s.real[b],
                               fit.model().prediction(query,s)};
}

int main() {
    // 例では a=4, k=2, b=1 の装置から、誤差なしの値を作る。
    std::vector<Row> rows;
    for (double x : {0.0, 0.5, 1.0, 2.0, 4.0, 8.0})
        rows.push_back({x, 1+4*x/(2+x), 0.1});
    const auto r = fit_response(rows, 3, Solver::Budget::for_steps(80000));
    // 実時間で制限する場合は、直前にBudget::for_us(20000)などを渡す。
    if (!r) return 1;
    for (double x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 2
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Point = std::array<double,2>;
struct PointRow {
    Point input, value;                    // 変換前と変換後の座標。
    Point sigma{1,1};                      // 各軸の既知の標準偏差。
    std::array<bool,2> present{true,true};  // 測れなかった軸はfalse。
    double rho = 0;                        // 両軸の誤差相関。-1<rho<1。
};

// 出力: [倍率c, 横ずれdx, 縦ずれdy, queryの予測横座標, 予測縦座標]。
std::optional<std::array<double,5>> fit_coordinates(
    std::vector<PointRow> rows, Point query, Solver::Budget budget) {
    Solver::Parameters p;
    const auto c = p.real(1, 0.2, 3);
    const auto dx = p.real(0, -5, 5), dy = p.real(0, -5, 5);
    auto predict = [c,dx,dy](const Point& x, const McmcState& s) {
        return Point{s.real[c]*x[0]+s.real[dx], s.real[c]*x[1]+s.real[dy]};
    };
    auto loss = [](const McmcState&, const PointRow& row, const Point& y) {
        if (!row.present[0] && !row.present[1]) return 0.0;
        if (!row.present[0]) return Solver::gaussian_loss(row.value[1],y[1],row.sigma[1]);
        if (!row.present[1]) return Solver::gaussian_loss(row.value[0],y[0],row.sigma[0]);
        assert(row.sigma[0]>0 && row.sigma[1]>0 && std::abs(row.rho)<1);
        const double z0 = (row.value[0]-y[0])/row.sigma[0];
        const double z1 = (row.value[1]-y[1])/row.sigma[1];
        const double v = 1-row.rho*row.rho;
        // 同時に観測できた二軸は、相関を含む一つの損失にする。
        return (z0*z0-2*row.rho*z0*z1+z1*z1)/(2*v)
             + std::log(row.sigma[0])+std::log(row.sigma[1])+0.5*std::log(v);
    };
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict,loss));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    const auto& s = *r.state;
    const auto y = fit.model().prediction(query,s); // 独自損失なので生の予測を使う。
    return std::array<double,5>{s.real[c],s.real[dx],s.real[dy],y[0],y[1]};
}

int main() {
    std::vector<PointRow> rows{
        {{0,0},{2,-1},{0.1,0.2},{true,true},0.3},
        {{1,2},{3.5,2},{0.1,0.2},{true,true},0.3},
        {{2,-1},{5,0},{0.1,0.2},{true,false},0.3}, // 縦のvalue=0は未使用。
        {{-1,3},{0,3.5},{0.1,0.2},{false,true},0.3}};
    const auto r = fit_coordinates(rows,{2,2},Solver::Budget::for_steps(40000));
    if (!r) return 1;
    for (double x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 3
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

// 入力: input=仕事量、value=処理時間、scale=通常時の誤差の大きさ。
// thresholdは「scaleの何倍から外れ値として影響を抑えるか」。正の固定値。
// 出力: [仕事量あたりの時間a, 固定時間b]。
std::optional<std::array<double,2>> fit_robust_time(
    std::vector<Row> rows, Solver::Budget budget, double threshold=1.345) {
    Solver::Parameters p;
    const auto a = p.real(1, 0, 5), b = p.real(0, -10, 10);
    auto predict = [a,b](double x, const McmcState& s) {
        return s.real[a]*x+s.real[b];
    };
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict,Solver::Huber{threshold}));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    return std::array<double,2>{r.state->real[a],r.state->real[b]};
}

int main() {
    std::vector<Row> rows;
    for (int x=0; x<=12; ++x) rows.push_back({double(x),2.0*x+1,0.2});
    rows[6].value += 50; // 一度だけ大きな待ち時間が入った。
    const auto r = fit_robust_time(rows,Solver::Budget::for_steps(40000));
    if (!r) return 1;
    std::cout << (*r)[0] << ' ' << (*r)[1] << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 4
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;
struct SuccessEstimate {
    double probability;                     // 次の作業の成功確率の推定。
    std::optional<double> probability_variance; // パラメータの不確かさによる確率の分散。
    std::uint64_t samples;                   // 独立な標本の個数とは限らない。
};

// 入力: input=難しさx、value=成功なら1/失敗なら0。scaleは使用しない。
// total_stepsは準備2000遷移を含む。準備だけで終わればnullopt。
std::optional<SuccessEstimate> predict_success(
    std::vector<Row> rows, double query, std::uint64_t total_steps) {
    Solver::Parameters p;
    const auto a = p.real(0, -8, 8), b = p.real(1, 0, 5);
    auto predict = [a,b](double x, const McmcState& s) {
        return s.real[a]-s.real[b]*x; // 確率そのものではなくlogitを返す。
    };
    auto prior = [a,b](const McmcState& s) {
        return 0.5*s.real[a]*s.real[a]/9 + 0.5*s.real[b]*s.real[b]/4;
    };
    Solver::Param settings;
    settings.warmup_steps = 2000;
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict,Solver::BernoulliLogit{},prior),
        settings, {}, 4); // 4鎖。初期値は全鎖で共通。
    const auto r = fit.sample(Solver::Budget::for_steps(total_steps),
        [&](const McmcState& s) { return fit.model().mean_prediction(query,s); });
    if (!r.summary.mean()) return std::nullopt;
    return SuccessEstimate{*r.summary.mean(),r.summary.variance(),r.summary.count()};
}

int main() {
    std::vector<Row> rows{{-2,1},{-1,1},{0,0},{0,1},{1,0},{2,0}};
    const auto r = predict_success(rows,0,60000);
    if (!r) return 1;
    std::cout << r->probability << ' ' << r->probability_variance.value_or(0)
              << ' ' << r->samples << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 5
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
struct Exposure { double load, hours; }; // loadは負荷指標、hours>0は観測時間。
using Row = Solver::Observation<Exposure>;

// 入力: value=観測された非負の整数件数。scaleは使用しない。
// 出力: queryの観測時間内に発生する件数の期待値。整数に丸めない。
std::optional<double> predict_count(
    std::vector<Row> rows, Exposure query, Solver::Budget budget) {
    Solver::Parameters p;
    const auto a = p.real(0, -6, 6), b = p.real(0, -3, 3);
    auto predict = [a,b](const Exposure& x, const McmcState& s) {
        assert(x.hours>0 && x.load>=0 && x.load<=2);
        return std::log(x.hours)+s.real[a]+s.real[b]*x.load; // 件数平均の対数。
    };
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict,Solver::PoissonLogMean{}));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    return fit.model().mean_prediction(query,*r.state); // expで件数の単位に戻す。
}

int main() {
    std::vector<Row> rows{{{0,1},2},{{0,3},6},{{1,1},4},{{1,3},12},{{2,2},16}};
    const auto r = predict_count(rows,{1,2},Solver::Budget::for_steps(30000));
    if (!r) return 1;
    std::cout << *r << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 6
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

// 入力: input=x、value=y。全観測で共通の標準偏差も推定するためscaleは使わない。
// この例はeta=log(sigma)に対して範囲内一様の事前分布を置く。
// 出力: [傾きa, 切片b, 標準偏差sigma]。sigmaの下限0.02は測定系に合わせて変える。
std::optional<std::array<double,3>> fit_unknown_noise(
    std::vector<Row> rows, Solver::Budget budget) {
    Solver::Parameters p;
    const auto a = p.real(1, -5, 5), b = p.real(0, -5, 5);
    const auto eta = p.real(std::log(0.5),std::log(0.02),std::log(3.0));
    auto predict = [a,b](double x, const McmcState& s) { return s.real[a]*x+s.real[b]; };
    auto loss = [eta](const McmcState& s, const Row& row, double predicted) {
        assert(row.weight>=0 && std::isfinite(row.weight));
        if (row.weight==0) return 0.0;
        const double z = (row.value-predicted)/std::exp(s.real[eta]);
        return row.weight*(0.5*z*z+s.real[eta]); // +log(sigma)を必ず含める。
    };
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict,loss));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    return std::array<double,3>{r.state->real[a],r.state->real[b],std::exp(r.state->real[eta])};
}

int main() {
    std::vector<Row> rows;
    for (int x=-3; x<=3; ++x) {
        rows.push_back({double(x),1.5*x+0.5-0.2});
        rows.push_back({double(x),1.5*x+0.5+0.2});
    }
    const auto r = fit_unknown_noise(rows,Solver::Budget::for_steps(60000));
    if (!r) return 1;
    for (double x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 7
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;
struct DeviceEstimate { double gain; std::int64_t offset, kind; };

// 入力: input=x、value=装置の表示値、scale=既知の測定誤差。
// kind=0なら一次応答、kind=1なら二次応答。offsetは整数に限られる。
// 出力: 倍率、整数補正、装置種類。種類IDの意味は呼び出し側と共有する。
std::optional<DeviceEstimate> fit_device(std::vector<Row> rows, Solver::Budget budget) {
    Solver::Parameters p;
    const auto gain = p.real(1, 0.1, 4);
    const auto offset = p.integer(0, 0, 5);
    const auto kind = p.category(0, 2); // integerとcategoryはdiscreteの添字を共有。
    auto predict = [gain,offset,kind](double x, const McmcState& s) {
        const double response = s.discrete[kind]==0 ? x : x*x;
        return s.real[gain]*response+static_cast<double>(s.discrete[offset]);
    };
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    return DeviceEstimate{r.state->real[gain],r.state->discrete[offset],r.state->discrete[kind]};
}

int main() {
    std::vector<Row> rows;
    for (double x : {0.0,1.0,2.0,3.0,4.0}) rows.push_back({x,1.7*x*x+2,0.1});
    const auto r = fit_device(rows,Solver::Budget::for_steps(80000));
    if (!r) return 1;
    std::cout << r->gain << ' ' << r->offset << ' ' << r->kind << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 8
#include "mcmc_estimator_v09.hpp"

using Fractions = std::array<double,3>;
using Solver = McmcEstimator<Fractions>;
using Row = Solver::Observation<Fractions>;

// 入力: input[k]=染料kだけの既知の応答、value=混合物の応答、scale=測定誤差。
// 出力: 三種類の割合の標本平均。非負で合計が約1。標本0件ならnullopt。
std::optional<Fractions> fit_mixture(std::vector<Row> rows, std::uint64_t total_steps) {
    auto predict = [](const Fractions& response, const Fractions& w) {
        return response[0]*w[0]+response[1]*w[1]+response[2]*w[2];
    };
    auto move = Solver::symmetric_move([](const Fractions&, Fractions& next,
        Solver::Rng& rng, const Solver::MoveContext&) {
        const int i = rng.integer(3), j = (i+1+rng.integer(2))%3;
        const double change = 0.1*rng.normal(); // 固定幅。Sample中は変更しない。
        next[static_cast<std::size_t>(i)] += change;
        next[static_cast<std::size_t>(j)] -= change; // 一方に足した分だけ他方から引く。
        return next[static_cast<std::size_t>(i)]>=0 && next[static_cast<std::size_t>(j)]>=0;
        // 範囲外はその試行を棄却する。有効になるまで引き直さない。
    });
    Solver::Param settings;
    settings.warmup_steps = 2000;
    auto fit = Solver::make_session({Fractions{1.0/3,1.0/3,1.0/3}},
        Solver::make_observation_model(std::move(rows),predict),std::move(move),settings);
    const auto r = fit.sample(Solver::Budget::for_steps(total_steps),
                             [](const Fractions& w) { return w; });
    return r.summary.mean();
}

int main() {
    std::vector<Row> rows{{{1,0,0},0.2,0.03},{{0,1,0},0.3,0.03},{{0,0,1},0.5,0.03}};
    const auto r = fit_mixture(rows,60000);
    if (!r) return 1;
    for (double x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 9
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
struct Edge { std::size_t road_class; double length; };
using Map = std::vector<Edge>; // 辺IDはこのvectorの添字。
using Route = std::vector<std::size_t>;
using Row = Solver::Observation<Route>;

// 入力: 道路種別数classes、辺表、routeごとの所要時間、訂正した辺表。
// 訂正は過去の測定にも適用する。各辺ID・道路種別IDの意味は共通。
// 出力: 道路種別ごとの「距離1あたりの時間」。
std::optional<std::vector<double>> fit_corrected_map(std::size_t classes,
    Map map, std::vector<Row> rows, Map corrected, std::uint64_t steps_per_stage) {
    assert(classes>0 && corrected.size()==map.size());
    Solver::Parameters p;
    for (std::size_t i=0; i<classes; ++i) p.real(1,0.1,10);
    auto predict = [](const Map& current, const Route& route, const McmcState& s) {
        double time = 0;
        for (auto id : route) {
            assert(id<current.size());
            const auto& edge = current[id];
            assert(edge.road_class<s.real.size() && edge.length>0);
            time += edge.length*s.real[edge.road_class];
        }
        return time;
    };
    Solver::Param settings;
    settings.cooling_steps = steps_per_stage;
    auto fit = Solver::make_numeric_session(p,
        Solver::make_context_model(std::move(rows),std::move(map),predict),settings);
    fit.solve_view(Solver::Budget::for_steps(steps_per_stage)); // 候補のコピーを省く。
    fit.update_model([&](auto& model) { model.context() = std::move(corrected); });
    // 以前の候補を保持したまま、訂正後の地図で再評価して探索する。
    const auto r = fit.solve(Solver::Budget::for_steps(steps_per_stage));
    if (!r.state) return std::nullopt;
    return r.state->real;
}

int main() {
    Map map{{0,1},{1,1},{0,2}}, corrected{{0,1},{1,2},{0,2}};
    std::vector<Row> rows{{{0},2,0.1},{{1},8,0.1},{{0,2},6,0.1}};
    const auto r = fit_corrected_map(2,map,rows,corrected,30000);
    if (!r) return 1;
    for (double x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 10
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Jobs = std::vector<double>;
using Row = Solver::Observation<Jobs>;
struct MachineEstimate { std::array<double,2> speed; double query_finish; };

// 入力: input=既知の順番で投入する仕事量の列、value=全仕事の終了時刻。
// 各仕事は「今の終了予定時刻が早い機械」に割り当てる。同時なら機械0。
// queryも同じ割当規則で動かす仕事列。出力は速度候補とqueryの終了時刻。
std::optional<MachineEstimate> fit_machines(
    std::vector<Row> rows, Jobs query, Solver::Budget budget) {
    Solver::Parameters p;
    p.real(1,0.2,5); p.real(1,0.2,5); // 速度は正。添字0/1が機械ID。
    auto simulate = [](const Jobs& jobs, const McmcState& s) {
        std::array<double,2> finish{0,0};
        for (double work : jobs) {
            assert(work>=0);
            const std::size_t machine = finish[0]<=finish[1] ? 0 : 1;
            finish[machine] += work/s.real[machine];
        }
        return std::max(finish[0],finish[1]);
    };
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),simulate));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    return MachineEstimate{{r.state->real[0],r.state->real[1]},
                            fit.model().prediction(query,*r.state)};
}

int main() {
    // 速度[2,1]で生成された測定。一本の仕事だけの行もあり、機械0を区別できる。
    std::vector<Row> rows{{{2},1,0.05},{{2,2},2,0.05},
                          {{4,1,2},3,0.05},{{1,4,1},4,0.05}};
    const auto r = fit_machines(rows,{2,2,2},Solver::Budget::for_steps(80000));
    if (!r) return 1;
    std::cout << r->speed[0] << ' ' << r->speed[1] << ' ' << r->query_finish << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 11
#include "mcmc_estimator_v09.hpp"

using Order = std::vector<int>; // 位置から要素IDを引く。各IDが一度ずつ現れる。
using Solver = McmcEstimator<Order>;
struct Comparison { int before, after; }; // 「beforeがafterより前」と観測した。

// 入力: 要素数n、誤りを含み得る比較列、各比較が逆になる既知の確率error。
// 出力: 最もよく観測を説明した順列。n>0、0<error<0.5、IDは[0,n)。
std::optional<Order> fit_order(int n, std::vector<Comparison> comparisons,
                             double error, std::uint64_t steps) {
    assert(n>0 && error>0 && error<0.5);
    Order initial(static_cast<std::size_t>(n));
    std::iota(initial.begin(),initial.end(),0);
    auto energy = [data=std::move(comparisons),error](const Order& order) {
        std::vector<int> position(order.size());
        for (std::size_t i=0; i<order.size(); ++i)
            position[static_cast<std::size_t>(order[i])] = static_cast<int>(i);
        double total = 0;
        for (const auto& row : data) {
            assert(row.before>=0 && row.after>=0 && row.before!=row.after);
            assert(static_cast<std::size_t>(row.before)<order.size() &&
                   static_cast<std::size_t>(row.after)<order.size());
            const bool matches = position[static_cast<std::size_t>(row.before)]
                               < position[static_cast<std::size_t>(row.after)];
            total -= std::log(matches ? 1-error : error);
        }
        return total; // 観測全体の負の対数尤度。
    };
    auto move = Solver::symmetric_move([](const Order&, Order& next,
        Solver::Rng& rng, const Solver::MoveContext&) {
        const auto i = static_cast<std::size_t>(rng.integer(static_cast<int>(next.size())));
        const auto j = static_cast<std::size_t>(rng.integer(static_cast<int>(next.size())));
        std::swap(next[i],next[j]); // 逆向きも同じ確率で選べる。
        return i!=j;              // 同じ位置なら評価を省いて棄却扱い。
    });
    Solver::Param settings;
    settings.cooling_steps = steps;
    auto fit = Solver::make_session({initial},std::move(energy),std::move(move),settings);
    return fit.solve(Solver::Budget::for_steps(steps)).state;
}

int main() {
    // 比較の多数は2→0→1を支持する。0→2という誤った比較が一つある。
    std::vector<Comparison> rows{{2,0},{2,0},{2,0},{0,1},{0,1},{2,1},{0,2}};
    const auto r = fit_order(3,rows,0.1,10000);
    if (!r) return 1;
    for (int x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 12
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
struct Reading { double x, y; }; // 時間順。隣接行は同じ長さの時間間隔。

// 入力: 時間順の観測、誤差の持続率rho、毎時刻に新たに加わる誤差のsigma。
// -1<rho<1、sigma>0。queryは最後の観測の「次の時刻」の入力。
// 出力: [傾きa, 切片b, 次の表示値の条件付き予測]。
std::optional<std::array<double,3>> fit_correlated_readings(std::vector<Reading> rows,
    double rho, double sigma, double query, Solver::Budget budget) {
    assert(!rows.empty() && std::abs(rho)<1 && sigma>0);
    Solver::Parameters p;
    const auto a = p.real(1,-5,5), b = p.real(0,-5,5);
    auto energy = [data=rows,rho,sigma,a,b](const McmcState& s) {
        double total = 0, previous = 0;
        for (std::size_t i=0; i<data.size(); ++i) {
            const double residual = data[i].y-(s.real[a]*data[i].x+s.real[b]);
            // 最初は定常分散、その後は前の残差を差し引いた「新しい誤差」を評価。
            const double scale = i==0 ? sigma/std::sqrt(1-rho*rho) : sigma;
            const double innovation = i==0 ? residual : residual-rho*previous;
            total += Solver::gaussian_loss(innovation,0,scale);
            previous = residual;
        }
        return total;
    };
    auto fit = Solver::make_numeric_session(p,std::move(energy));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    const double slope = r.state->real[a], intercept = r.state->real[b];
    const double last_residual = rows.back().y-(slope*rows.back().x+intercept);
    return std::array<double,3>{slope,intercept,slope*query+intercept+rho*last_residual};
}

int main() {
    std::vector<Reading> rows;
    for (int i=0; i<12; ++i) rows.push_back({double(i%4),2.0*(i%4)+1});
    const auto r = fit_correlated_readings(rows,0.6,0.2,4,Solver::Budget::for_steps(50000));
    if (!r) return 1;
    for (double x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 13
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

// 推定器を一度だけ作る。隠れた係数a,bはターン間で共通。
auto make_turn_estimator() {
    Solver::Parameters p;
    const auto a = p.real(1,0,5), b = p.real(0,-5,5);
    auto predict = [a,b](double x, const McmcState& s) { return s.real[a]*x+s.real[b]; };
    return Solver::make_numeric_session(p,
        Solver::make_observation_model(std::vector<Row>{},predict));
}

// 入力: 同じfit、新しく届いた観測だけ、今回の予算。
// 出力: Session::Result。state->realは[a,b]、revisionは目的関数の版番号。
template<class Fit>
auto estimate_turn(Fit& fit, std::vector<Row> newly_observed, Solver::Budget budget) {
    fit.add_observations(std::move(newly_observed)); // 登録だけ行い、まだ再評価しない。
    return fit.solve(budget);                     // 再評価と探索に同じ予算を使う。
}

int main() {
    auto fit = make_turn_estimator(); // 実際の対話問題でもターンループの外に置く。
    const auto first = estimate_turn(fit,{{0,1,0.1},{1,3,0.1}},Solver::Budget::for_steps(20000));
    if (!first.state) return 1;
    // 最後の観測には転記誤りがあり、後から7.0へ訂正するとする。
    const auto second = estimate_turn(fit,{{2,5,0.1},{3,7.5,0.1}},Solver::Budget::for_steps(20000));
    if (!second.state) return 1;

    // 観測を訂正する場合は「訂正後に残す全観測」を渡す。差分だけではない。
    fit.replace_observations({{0,1,0.1},{1,3,0.1},{2,5,0.1},{3,7,0.1}});
    fit.retain_last(3); // 古い一件を除く。実際に忘却が必要なときだけ使う。
    const auto final = fit.solve(Solver::Budget::for_steps(20000));
    if (!final.state) return 1;
    std::cout << final.state->real[0] << ' ' << final.state->real[1]
              << ' ' << fit.model().size() << ' ' << final.revision << '\n';
    // 時間不足ならstateが空か確認する。空でなくても収束したとは限らない。
    // sampleの外部集計を持つ場合は、revisionが変わった時点で破棄する。
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 14
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
struct VoltageRow { std::size_t a, b; double difference, sigma; };

// 入力: 端子数n、端子aの電圧-端子bの電圧の測定。a!=b、sigma>0。
// 端子0は既知の0V。測定で全端子が端子0につながっていることを想定。
// 出力: 全端子の電圧。未知端子の範囲[-5,5]は対象に合わせて設定する。
std::optional<std::vector<double>> fit_voltages(std::size_t n,
    std::vector<VoltageRow> rows, Solver::Budget budget) {
    assert(n>0);
    struct Model {
        std::vector<VoltageRow> rows;
        std::vector<std::vector<std::size_t>> incident;
        double term(std::size_t k, const McmcState& s) const {
            const auto& row = rows[k];
            return Solver::gaussian_loss(row.difference,s.real[row.a]-s.real[row.b],row.sigma);
        }
        double operator()(const McmcState& s) const {
            double e = 0;
            for (std::size_t k=0; k<rows.size(); ++k) e += term(k,s);
            return e; // 初期評価・モデル更新には常に全評価が必要。
        }
        double candidate(const McmcState& old, const McmcState& next, double old_energy) const {
            std::optional<std::size_t> changed;
            for (std::size_t i=0; i<next.real.size(); ++i) if (old.real[i]!=next.real[i]) {
                if (changed) return (*this)(next); // 複数変数の提案にも正しく対応する。
                changed = i;
            }
            double energy = old_energy;
            if (changed) for (auto k : incident[*changed]) energy += term(k,next)-term(k,old);
            return energy; // 「増分」ではなく、候補の目的関数全体を返す。
        }
    };
    Model model{std::move(rows),std::vector<std::vector<std::size_t>>(n)};
    for (std::size_t k=0; k<model.rows.size(); ++k) {
        const auto& row = model.rows[k];
        assert(row.a<n && row.b<n && row.a!=row.b && row.sigma>0);
        model.incident[row.a].push_back(k); model.incident[row.b].push_back(k);
    }
    Solver::Parameters p;
    p.real(0,0,0); // 固定変数もrealに保持される。提案では動かない。
    for (std::size_t i=1; i<n; ++i) p.real(0,-5,5);
    auto fit = Solver::make_numeric_session(p,std::move(model));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    return r.state->real;
}

int main() {
    std::vector<VoltageRow> rows{{1,0,1,0.1},{2,1,1,0.1},{3,2,1,0.1},{3,0,3,0.1}};
    const auto r = fit_voltages(4,rows,Solver::Budget::for_steps(60000));
    if (!r) return 1;
    for (double x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 15
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;
struct Decision {
    std::size_t index;       // actionsの添字。
    double setting;          // 実際に採用する設定値。
    double mean_cost;        // この設定値での、未知パラメータに関する平均予測コスト。
    double model_cost_sd;    // 未知パラメータに由来する予測コストの標準偏差。
    std::uint64_t samples;
};

// 入力: input=試した設定、value=測ったコスト、scale=既知の測定誤差。
// actionsは選べる設定値、risk>=0は不確かさをどれだけ嫌うか。0なら平均だけで選ぶ。
// 出力: 平均コスト+risk*標準偏差が最小の設定。標本2件未満ならnullopt。
std::optional<Decision> choose_setting(std::vector<Row> rows, std::vector<double> actions,
                                      double risk, std::uint64_t total_steps) {
    assert(!actions.empty() && std::isfinite(risk) && risk>=0);
    Solver::Parameters p;
    const auto a = p.real(1,0.1,4), b = p.real(0,-5,5);
    auto predict = [a,b](double x, const McmcState& s) {
        const double d = x-s.real[b]; return s.real[a]*d*d;
    };
    Solver::Param settings;
    settings.warmup_steps = 4000;
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict),settings,{},4);
    using Fit = decltype(fit);
    Fit::Moments<std::vector<double>> moments; // この関数内では同じ目的関数・行動列を使う。
    const auto revision = fit.revision();
    while (total_steps>0) {
        const auto chunk = std::min<std::uint64_t>(total_steps,5000);
        fit.sample_each(Solver::Budget::for_steps(chunk),[&](const McmcState& s,double,int) {
            std::vector<double> costs;
            costs.reserve(actions.size());
            for (double action : actions) costs.push_back(fit.model().prediction(action,s));
            moments.add(costs); // 棄却された遷移の現状態も集計される。
        });
        if (fit.revision()!=revision) return std::nullopt; // 異なる目的関数の集計を混ぜない。
        total_steps -= chunk;
    }
    const auto variance = moments.variance();
    if (!variance) return std::nullopt;
    const auto& mean = *moments.mean();
    std::size_t best = 0;
    auto score = [&](std::size_t j) { return mean[j]+risk*std::sqrt(std::max(0.0,(*variance)[j])); };
    for (std::size_t j=1; j<actions.size(); ++j) if (score(j)<score(best)) best=j;
    return Decision{best,actions[best],mean[best],std::sqrt(std::max(0.0,(*variance)[best])),moments.count()};
}

int main() {
    std::vector<Row> rows{{-1,4,0.5},{0,1,0.5},{2,1,0.5}};
    const auto r = choose_setting(rows,{-1,0,1,2},0.5,60000);
    if (!r) return 1;
    std::cout << r->index << ' ' << r->setting << ' ' << r->mean_cost << ' '
              << r->model_cost_sd << ' ' << r->samples << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 16
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using AffineInput = std::array<double, 3>;
// 1行 = { {x1, x2, x3}, 観測値y, 誤差の標準偏差scale, 重みweight }。
// scaleとweightは省略すると1。二乗誤差を最小化するだけなら、この既定値でよい。
using AffineRow = Solver::Observation<AffineInput>;
struct AffineEstimate {
    std::array<double, 3> a; // a[0]=a1, a[1]=a2, a[2]=a3
    double b;
};

// 入力: 同じ未知係数で得た複数の観測、計算予算。
// 出力: 探索中に見つけた最良の係数。評価が完了しなければnullopt。
std::optional<AffineEstimate> fit_affine3(
    std::vector<AffineRow> rows, Solver::Budget budget) {
    assert(!rows.empty());
    Solver::Parameters p;
    // real(初期値, 下限, 上限)。範囲は問題の事前知識に合わせて変更する。
    const std::array<std::size_t, 3> ai{
        p.real(0, -10, 10), p.real(0, -10, 10), p.real(0, -10, 10)};
    const auto bi = p.real(0, -10, 10);

    // ライブラリが候補係数sを渡すので、そのときの計算結果を返す。
    auto predict = [ai, bi](const AffineInput& x, const McmcState& s) {
        return s.real[ai[0]] * x[0] + s.real[ai[1]] * x[1]
             + s.real[ai[2]] * x[2] + s.real[bi];
    };
    // 既定のGaussian損失で、予測値と各行の観測値を比較する。
    auto model = Solver::make_observation_model(std::move(rows), predict);
    auto session = Solver::make_numeric_session(p, std::move(model));
    const auto result = session.solve(budget);
    if (!result.state) return std::nullopt;
    const auto& s = *result.state;
    return AffineEstimate{{s.real[ai[0]], s.real[ai[1]], s.real[ai[2]]}, s.real[bi]};
}

int main() {
    // 動作確認用データ: y = 2*x1 - 3*x2 + 0.5*x3 + 1。
    // 実際には、手元の入力と観測値からこのvectorを作る。
    // 各入力を独立に変える。例えば常にx1=x2だとa1とa2を区別できない。
    std::vector<AffineRow> rows;
    for (double x1 : {-1.0, 0.0, 1.0})
        for (double x2 : {-1.0, 0.0, 1.0})
            for (double x3 : {-1.0, 0.0, 1.0})
                rows.push_back({{x1, x2, x3}, 2*x1 - 3*x2 + 0.5*x3 + 1});

    // 再現しやすいよう固定回数で実行。時間指定ならBudget::for_us(20'000)。
    const auto r = fit_affine3(std::move(rows), Solver::Budget::for_steps(80'000));
    if (!r) return 1;
    std::cout << "a1=" << r->a[0] << " a2=" << r->a[1]
              << " a3=" << r->a[2] << " b=" << r->b << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 161
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using AffineInput = std::array<double, 3>;
using AffineRow = Solver::Observation<AffineInput>;

struct AffineSigmaEstimate {
    std::array<double, 3> a; // a1,a2,a3
    double b;
    double sigma; // 全観測に共通するノイズの標準偏差
};

// 入力: {{x1,x2,x3}, 観測値y}の列、計算予算。
// 全行でscale=1,weight=1とする。未知のsigmaは行に指定しない。
// 出力: 予算内で見つけた最良の四係数とsigma。候補なしならnullopt。
std::optional<AffineSigmaEstimate> fit_affine3_joint_sigma(
    std::vector<AffineRow> rows, Solver::Budget budget) {
    assert(!rows.empty());
    assert(std::all_of(rows.begin(), rows.end(), [](const AffineRow& row) {
        return row.scale == 1 && row.weight == 1;
    }));
    Solver::Parameters p;
    const std::array<std::size_t, 3> ai{
        p.real(0, -10, 10), p.real(0, -10, 10), p.real(0, -10, 10)};
    const auto bi = p.real(0, -10, 10);
    // real(初期値,下限,上限)。sigmaの範囲は観測値の単位に合わせて変更する。
    const auto si = p.real(1.0, 0.001, 10.0);

    // 候補係数から平均値を予測する。ここで乱数を加えない。
    auto predict = [ai, bi](const AffineInput& x, const McmcState& s) {
        return s.real[ai[0]]*x[0] + s.real[ai[1]]*x[1]
             + s.real[ai[2]]*x[2] + s.real[bi];
    };
    // 既知のrow.scaleではなく、候補状態に入っている共通sigmaを使う。
    auto loss = [si](const McmcState& s, const AffineRow& row, double predicted) {
        // gaussian_lossは、二乗誤差項とlog(sigma)の両方を含む。
        return Solver::gaussian_loss(row.value, predicted, s.real[si]);
    };
    auto model = Solver::make_observation_model(std::move(rows), predict, loss);
    auto session = Solver::make_numeric_session(p, std::move(model));
    const auto result = session.solve(budget);
    if (!result.state) return std::nullopt;
    const auto& s = *result.state;
    return AffineSigmaEstimate{
        {s.real[ai[0]], s.real[ai[1]], s.real[ai[2]]}, s.real[bi], s.real[si]};
}

int main() {
    // 動作確認用: a=(2,-3,0.5),b=1、独立な正規ノイズの標準偏差は0.2。
    // 真値はデータ生成にだけ使う。推定関数へsigmaの真値を渡さない。
    std::mt19937 rng(1);
    std::normal_distribution<double> noise(0.0, 0.2);
    std::vector<AffineRow> rows;
    for (int repeat = 0; repeat < 4; ++repeat)
        for (double x1 : {-1.0, 0.0, 1.0})
            for (double x2 : {-1.0, 0.0, 1.0})
                for (double x3 : {-1.0, 0.0, 1.0})
                    rows.push_back({{x1,x2,x3}, 2*x1 - 3*x2 + 0.5*x3 + 1 + noise(rng)});

    // 実際には手元の観測列を渡す。時間指定ならBudget::for_us(20'000)など。
    const auto r = fit_affine3_joint_sigma(std::move(rows), Solver::Budget::for_steps(80'000));
    if (!r) return 1;
    std::cout << "a1=" << r->a[0] << " a2=" << r->a[1]
              << " a3=" << r->a[2] << " b=" << r->b << " sigma=" << r->sigma << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 162
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using AffineInput = std::array<double, 3>;
using AffineRow = Solver::Observation<AffineInput>;

struct AffineSigmaEstimate {
    std::array<double, 3> a; // a1,a2,a3
    double b;
    double sigma; // sqrt(残差平方和/観測数)。完全一致なら0になり得る。
};

// 入力: {{x1,x2,x3}, 観測値y}の列、計算予算。
// 共通の未知sigmaを推定するため、全行でscale=1,weight=1とする。
// 出力: 最良の四係数と、その残差から計算したsigma。候補なしならnullopt。
std::optional<AffineSigmaEstimate> fit_affine3_residual_sigma(
    std::vector<AffineRow> rows, Solver::Budget budget) {
    assert(!rows.empty());
    assert(std::all_of(rows.begin(), rows.end(), [](const AffineRow& row) {
        return row.scale == 1 && row.weight == 1;
    }));
    Solver::Parameters p;
    const std::array<std::size_t, 3> ai{
        p.real(0, -10, 10), p.real(0, -10, 10), p.real(0, -10, 10)};
    const auto bi = p.real(0, -10, 10);
    auto predict = [ai, bi](const AffineInput& x, const McmcState& s) {
        return s.real[ai[0]]*x[0] + s.real[ai[1]]*x[1]
             + s.real[ai[2]]*x[2] + s.real[bi];
    };
    // 探索するのは四係数だけ。既定のGaussian損失で二乗誤差を小さくする。
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows), predict));
    const auto result = session.solve(budget);
    if (!result.state) return std::nullopt;
    const auto& s = *result.state;

    // Sessionが所有する観測と予測関数を再利用し、残差平方和を計算する。
    // この全観測の走査はsolveから戻った後に行うので、実時間予算に余裕を残す。
    const auto& model = session.model();
    double rss = 0;
    for (const auto& row : model.observations()) {
        const double residual = row.value - model.prediction(row.input, s);
        rss += residual * residual;
    }
    const double sigma = std::sqrt(rss / static_cast<double>(model.size()));
    return AffineSigmaEstimate{
        {s.real[ai[0]], s.real[ai[1]], s.real[ai[2]]}, s.real[bi], sigma};
}

int main() {
    // U16-Aと同じ観測を作り、残差からのsigma推定と比較できるようにする。
    std::mt19937 rng(1);
    std::normal_distribution<double> noise(0.0, 0.2);
    std::vector<AffineRow> rows;
    for (int repeat = 0; repeat < 4; ++repeat)
        for (double x1 : {-1.0, 0.0, 1.0})
            for (double x2 : {-1.0, 0.0, 1.0})
                for (double x3 : {-1.0, 0.0, 1.0})
                    rows.push_back({{x1,x2,x3}, 2*x1 - 3*x2 + 0.5*x3 + 1 + noise(rng)});

    const auto r = fit_affine3_residual_sigma(std::move(rows), Solver::Budget::for_steps(80'000));
    if (!r) return 1;
    std::cout << "a1=" << r->a[0] << " a2=" << r->a[1]
              << " a3=" << r->a[2] << " b=" << r->b << " sigma=" << r->sigma << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 163
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using AffineInput = std::array<double, 3>;

// 入力x1,x2,x3と観測値yは実数でよい。整数に制限するのは四係数。
// 1行 = {{x1,x2,x3}, y, 誤差の標準偏差scale, 重みweight}。
// scaleとweightは省略すると1。既知の標準偏差sigma>0は第3要素に指定する。
using AffineRow = Solver::Observation<AffineInput>;

struct AffineIntegerEstimate {
    std::array<std::int64_t, 3> a; // a[0]=a1, a[1]=a2, a[2]=a3
    std::int64_t b;
};

// 入力: 全観測に共通する整数係数で得た観測、計算予算。
// 出力: 探索中に見つけた最良の整数係数。候補を得られなければnullopt。
std::optional<AffineIntegerEstimate> fit_affine3_integer(
    std::vector<AffineRow> rows, Solver::Budget budget) {

    assert(!rows.empty());
    Solver::Parameters p;

    // integer(初期値, 下限, 上限)。上下限を含む整数だけを探索する。
    // 範囲は問題の事前知識に合わせて変更する。
    const std::array<std::size_t, 3> ai{
        p.integer(0, -10, 10),
        p.integer(0, -10, 10),
        p.integer(0, -10, 10)
    };
    const auto bi = p.integer(0, -10, 10);

    // 整数係数はstate.discreteから読む。
    // 入力xがdoubleなので、予測値の計算はdoubleになる。
    auto predict = [ai, bi](const AffineInput& x, const McmcState& s) {
        return s.discrete[ai[0]] * x[0]
             + s.discrete[ai[1]] * x[1]
             + s.discrete[ai[2]] * x[2]
             + s.discrete[bi];
    };

    // 既定のGaussian損失で、予測値と実測値を比較する。
    auto model = Solver::make_observation_model(std::move(rows), predict);
    auto session = Solver::make_numeric_session(p, std::move(model));
    const auto result = session.solve(budget);
    if (!result.state) return std::nullopt;

    const auto& s = *result.state;
    return AffineIntegerEstimate{
        {s.discrete[ai[0]], s.discrete[ai[1]], s.discrete[ai[2]]},
        s.discrete[bi]
    };
}

int main() {
    // 動作確認用データ: y = 2*x1 - 3*x2 + x3 + 1。
    // 真の係数a1=2,a2=-3,a3=1,b=1は、全て整数。
    // 実際には、この生成部分を手元の入力と観測値に置き換える。
    std::vector<AffineRow> rows;
    for (double x1 : {-1.0, 0.0, 1.0})
        for (double x2 : {-0.5, 0.0, 0.5})
            for (double x3 : {-0.25, 0.0, 0.25})
                rows.push_back({{x1,x2,x3}, 2*x1 - 3*x2 + x3 + 1});

    // 固定回数で実行。時間指定ならBudget::for_us(20'000)など。
    const auto r = fit_affine3_integer(
        std::move(rows), Solver::Budget::for_steps(80'000));
    if (!r) return 1;

    std::cout << "a1=" << r->a[0] << " a2=" << r->a[1]
              << " a3=" << r->a[2] << " b=" << r->b << '\n';
    return 0;
}

#elif MCMC_GUIDE_EXAMPLE == 17
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;

// 1行 = {入力x, 観測値y, 誤差の標準偏差scale, 重みweight}。
// scaleとweightは省略すると1。通常の二乗誤差なら省略してよい。
using FunctionRow = Solver::Observation<double>;

struct FunctionEstimate {
    int t; // 1: a*x+b、2: a*x*x+b、3: a*sin(x)+b
    double a, b;
};

// 入力: 全観測に共通の未知t,a,bで得た観測、計算予算。
// xの単位はラジアン。
// 出力: 探索中に見つけた最良の組。評価が完了しなければnullopt。
std::optional<FunctionEstimate> fit_function_type(
    std::vector<FunctionRow> rows, Solver::Budget budget) {

    assert(!rows.empty());
    Solver::Parameters p;

    // category(初期ID, 候補数)。内部IDは0,1,2なので、t=内部ID+1。
    const auto ti = p.category(0, 3);

    // real(初期値, 下限, 上限)。範囲は問題に合わせて変更する。
    const auto ai = p.real(0, -10, 10);
    const auto bi = p.real(0, -10, 10);

    // 候補のt,a,bと入力xから予測値を計算する。
    auto predict = [ti, ai, bi](double x, const McmcState& s) {
        const double a = s.real[ai], b = s.real[bi];
        switch (s.discrete[ti]) {
            case 0: return a*x + b;            // t=1
            case 1: return a*x*x + b;          // t=2
            default: return a*std::sin(x) + b; // t=3
        }
    };

    // 式の種類tと実数a,bを一緒に探索する。
    auto model = Solver::make_observation_model(std::move(rows), predict);
    auto session = Solver::make_numeric_session(p, std::move(model));
    const auto result = session.solve(budget);
    if (!result.state) return std::nullopt;

    const auto& s = *result.state;
    return FunctionEstimate{
        static_cast<int>(s.discrete[ti]) + 1, s.real[ai], s.real[bi]};
}

int main() {
    // 3種類それぞれを、真の係数a=2,b=1で試す。
    // true_tは動作確認用データの生成にだけ使い、推定関数には渡さない。
    for (int true_t : {1, 2, 3}) {
        // 実際には、手元の入力と観測値からこのvectorを作る。
        std::vector<FunctionRow> rows;
        for (double x : {-3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0}) {
            const double response = true_t == 1 ? x
                                  : true_t == 2 ? x*x : std::sin(x);
            rows.push_back({x, 2*response + 1});
        }

        // 固定回数で実行。時間指定ならBudget::for_us(20'000)など。
        const auto r = fit_function_type(
            std::move(rows), Solver::Budget::for_steps(80'000));
        if (!r) return 1;

        std::cout << "true_t=" << true_t << " estimated_t=" << r->t
                  << " a=" << r->a << " b=" << r->b << '\n';
    }
    return 0;
}

#else
#error "MCMC_GUIDE_EXAMPLE must be 1..17 or 161..163"
#endif
