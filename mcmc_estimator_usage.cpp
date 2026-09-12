#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

// 入力: (input=x, value=y, scale=既知の観測誤差の標準偏差) の観測列。
// 出力: y=a*x+b の推定値 [a,b]。時間切れで初期評価もできなければnullopt。
std::optional<std::array<double,2>> fit_line(std::vector<Row> rows, std::int64_t budget_us) {
    Solver::Parameters p;
    p.real(0,-10,10); p.real(0,-10,10);
    auto fit = Solver::make_numeric_session(p,Solver::make_observation_model(std::move(rows),
        [](double x,const McmcState& s) { return s.real[0]*x+s.real[1]; }));
    const auto result = fit.solve(Solver::Budget::for_us(budget_us));
    if (!result.state) return std::nullopt;
    return std::array<double,2>{result.state->real[0],result.state->real[1]};
}

// 入力: 各ターンで新しく得た観測だけ。隠れた係数a,bはターン間で共通。
// 出力: 各ターンの推定 [a,b]。複数のaddは次回のsolveでまとめて反映する。
std::vector<std::array<double,2>> fit_turns(const std::vector<std::vector<Row>>& turns,
                                          std::uint64_t steps_per_turn) {
    Solver::Parameters p; p.real(0,-10,10); p.real(0,-10,10);
    auto fit = Solver::make_numeric_session(p,Solver::make_observation_model(std::vector<Row>{},
        [](double x,const McmcState& s) { return s.real[0]*x+s.real[1]; }),
        {.cooling_steps=steps_per_turn});
    std::vector<std::array<double,2>> estimates;
    for (const auto& rows : turns) {
        fit.add_observations(rows);
        const auto r = fit.solve(Solver::Budget::for_steps(steps_per_turn));
        if (r.state) estimates.push_back({r.state->real[0],r.state->real[1]});
    }
    return estimates;
}

// 入力: 成否(0/1)、問い合わせx、採取に使う総遷移数(準備2000遷移を含む)。
// モデル: 成功確率=sigmoid(a*x+b)。出力: 状態ごとの成功確率を平均した値。
// 準備だけで予算を使い切った場合はnullopt。平均係数をsigmoidに代入する方法とは異なる。
std::optional<double> success_probability(std::vector<Row> rows, double query,
                                          std::uint64_t total_steps) {
    Solver::Parameters p; p.real(0,-5,5); p.real(0,-5,5);
    auto fit = Solver::make_numeric_session(p,Solver::make_observation_model(std::move(rows),
        [](double x,const McmcState& s) { return s.real[0]*x+s.real[1]; },Solver::BernoulliLogit{},
        [](const McmcState& s) { return 0.5*(s.real[0]*s.real[0]+s.real[1]*s.real[1]); }),
        {.warmup_steps=2000});
    const auto r = fit.sample(Solver::Budget::for_steps(total_steps),[&](const McmcState& s) {
        return fit.model().mean_prediction(query,s);
    });
    return r.summary.mean();
}

// 入力: 既知の区間長と、区間IDごとの所要時間の観測。隠れパラメータは距離あたりの時間。
// corrected_lengthsは同じ区間IDに対する「計測入力の訂正」。過去の観測も訂正後の距離で再評価する。
// 環境そのものがターンごとに変わる場合は、当時の距離を各観測.inputに保存すること。
std::optional<double> fit_travel_rate(std::vector<double> lengths,
    std::vector<Solver::Observation<std::size_t>> rows, std::vector<double> corrected_lengths) {
    Solver::Parameters p; p.real(1,0.01,10);
    auto fit = Solver::make_numeric_session(p,Solver::make_context_model(std::move(rows),std::move(lengths),
        [](const std::vector<double>& distance, std::size_t edge, const McmcState& s) {
            assert(edge < distance.size()); return distance[edge]*s.real[0];
        }));
    fit.solve_view(Solver::Budget::for_steps(2000));
    fit.update_model([&](auto& model) { model.context() = std::move(corrected_lengths); });
    const auto result = fit.solve(Solver::Budget::for_steps(2000));
    if (!result.state) return std::nullopt;
    return result.state->real[0];
}

// 入力: 元素iを位置jに置いたときのコスト表(正方行列)。出力: 各位置の元素ID。
// 任意の独自状態・全体評価にも同じ所有APIが使える例。観測尤度を総和した評価へも置換できる。
std::optional<std::vector<int>> fit_assignment(std::vector<std::vector<double>> cost,
                                               std::uint64_t steps) {
    using S = McmcEstimator<std::vector<int>>;
    const int n = static_cast<int>(cost.size()); assert(n > 0);
    for (const auto& row : cost) { assert(static_cast<int>(row.size()) == n); (void)row; }
    std::vector<int> initial(static_cast<std::size_t>(n)); std::iota(initial.begin(),initial.end(),0);
    auto energy = [cost=std::move(cost)](const std::vector<int>& order) {
        double total = 0;
        for (std::size_t j = 0; j < order.size(); ++j) total += cost[static_cast<std::size_t>(order[j])][j];
        return total;
    };
    auto move = S::symmetric_move([](const std::vector<int>&,std::vector<int>& next,S::Rng& rng,const S::MoveContext&) {
        const auto a = static_cast<std::size_t>(rng.integer(static_cast<int>(next.size())));
        const auto b = static_cast<std::size_t>(rng.integer(static_cast<int>(next.size())));
        std::swap(next[a],next[b]); return true;
    });
    auto fit = S::make_session({initial},std::move(energy),std::move(move),{.cooling_steps=steps});
    return fit.solve(S::Budget::for_steps(steps)).state;
}

int main() {
    std::vector<Row> rows{{-2,-3,0.1},{-1,-1,0.1},{0,1,0.1},{1,3,0.1},{2,5,0.1}};
    const auto line = fit_line(rows,20000);
    const auto turns = fit_turns({{rows[0],rows[1]},{rows[2],rows[3],rows[4]}},10000);
    const auto probability = success_probability({{-1,0},{0,0},{1,1},{2,1}},0.5,30000);
    const auto travel = fit_travel_rate({1,2},{{0,2,0.1},{1,4,0.1}},{1,2});
    const auto order = fit_assignment({{3,0},{0,3}},1000);
    if (!line || turns.size()!=2 || !probability || !travel || !order) return 1;
    if (std::abs((*line)[0]-2)>0.1 || std::abs((*line)[1]-1)>0.1 || *probability<0 || *probability>1
        || std::abs(*travel-2)>0.1 || *order != std::vector<int>({1,0})) return 2;
    std::cout << "PASS 5 usage scenarios\n";
}
