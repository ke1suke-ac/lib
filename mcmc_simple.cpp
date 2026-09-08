#include <bits/stdc++.h>
using namespace std;

static constexpr size_t DIM = 100;
static constexpr size_t NUM_SAMPLES = 500;

/// 解を表す構造体
struct Solution {
    array<double, DIM> theta;
};

/// ベンチマーク問題を表すクラス
class BenchmarkProblem {
public:
    array<array<double, DIM>, NUM_SAMPLES> x;      // 設計行列 X
    array<double, NUM_SAMPLES> y;                  // 出力ベクトル y
    array<double, DIM> theta_true;                 // 真のパラメータ

    /// コンストラクタ：seed から問題を生成する
    BenchmarkProblem(uint64_t seed) {
        mt19937_64 rng(seed);
        uniform_real_distribution<double> unif_theta(-10.0, 10.0);
        // 真のパラメータを一様乱数で生成
        for (size_t i = 0; i < DIM; ++i) {
            theta_true[i] = unif_theta(rng);
        }
        uniform_real_distribution<double> unif_x(-5.0, 5.0);
        // 設計行列 X を一様乱数で生成
        for (size_t i = 0; i < NUM_SAMPLES; ++i) {
            for (size_t j = 0; j < DIM; ++j) {
                x[i][j] = unif_x(rng);
            }
        }
        // ノイズ生成用分布
        const double noise_std = 0.1;
        normal_distribution<double> noise_dist(0.0, noise_std);
        // 出力 y = X*theta_true + ノイズ
        for (size_t i = 0; i < NUM_SAMPLES; ++i) {
            double sum = 0.0;
            for (size_t j = 0; j < DIM; ++j) {
                sum += x[i][j] * theta_true[j];
            }
            y[i] = sum + noise_dist(rng);
        }
    }

    /// 推定解のスコア（平均二乗誤差）を計算する
    double score(const Solution &sol) const {
        double mse = 0.0;
        for (size_t i = 0; i < DIM; ++i) {
            double e = sol.theta[i] - theta_true[i];
            mse += e * e;
        }
        return mse / static_cast<double>(DIM);
    }
};

/// MCMC（焼きなまし法）による最適化
Solution solve_mcmc(
    const BenchmarkProblem &problem,
    uint64_t time_limit_ms,
    pair<double, double> bounds,
    double initial_temperature,
    double cooling_rate,
    double perturbation_std,
    uint64_t seed
) {
    const double min_b = bounds.first, max_b = bounds.second;
    mt19937_64 rng(seed + 20000);

    // 初期解を一様乱数で生成
    uniform_real_distribution<double> unif_bound(min_b, max_b);
    array<double, DIM> current;
    for (size_t k = 0; k < DIM; ++k) {
        current[k] = unif_bound(rng);
    }

    // 残差 r[i] = (X*current - y)
    array<double, NUM_SAMPLES> r;
    for (size_t i = 0; i < NUM_SAMPLES; ++i) {
        double s = 0.0;
        for (size_t j = 0; j < DIM; ++j) {
            s += problem.x[i][j] * current[j];
        }
        r[i] = s - problem.y[i];
    }

    // 目的関数値（残差二乗和）
    double curr_obj = 0.0;
    for (size_t i = 0; i < NUM_SAMPLES; ++i) {
        curr_obj += r[i] * r[i];
    }

    // 差分評価用 A[j] = sum_i r[i] * X[i][j]
    array<double, DIM> A;
    for (size_t j = 0; j < DIM; ++j) {
        double s = 0.0;
        for (size_t i = 0; i < NUM_SAMPLES; ++i) {
            s += r[i] * problem.x[i][j];
        }
        A[j] = s;
    }

    // 定数 B[k] = sum_i X[i][k]^2
    array<double, DIM> B;
    for (size_t k = 0; k < DIM; ++k) {
        double s = 0.0;
        for (size_t i = 0; i < NUM_SAMPLES; ++i) {
            s += problem.x[i][k] * problem.x[i][k];
        }
        B[k] = s;
    }

    // クロス項 Q[k][j] = sum_i X[i][k] * X[i][j]
    array<array<double, DIM>, DIM> Q;
    for (size_t k = 0; k < DIM; ++k) {
        for (size_t j = 0; j < DIM; ++j) {
            double s = 0.0;
            for (size_t i = 0; i < NUM_SAMPLES; ++i) {
                s += problem.x[i][k] * problem.x[i][j];
            }
            Q[k][j] = s;
        }
    }

    // 最良解の保持
    array<double, DIM> best = current;
    double best_obj = curr_obj;

    normal_distribution<double> perturb_dist(0.0, perturbation_std);
    auto t0 = chrono::steady_clock::now();
    double temperature = initial_temperature;

    // 時間制限内ループ
    while (chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - t0).count()
           < static_cast<long long>(time_limit_ms)) {
        // 更新次元をランダムに選択
        uniform_int_distribution<int> unif_dim(0, DIM - 1);
        int k = unif_dim(rng);
        double noise = perturb_dist(rng);

        // 枠内にクリッピングした候補解
        double candidate = current[k] + noise;
        if (candidate < min_b) candidate = min_b;
        else if (candidate > max_b) candidate = max_b;

        double delta = candidate - current[k];
        double delta_obj = 2.0 * delta * A[k] + delta * delta * B[k];
        double new_obj = curr_obj + delta_obj;

        // 改善 or 確率的受容
        if (delta_obj < 0.0 ||
            uniform_real_distribution<double>(0.0, 1.0)(rng) < exp(-delta_obj / temperature)) {
            // 更新適用
            current[k] = candidate;
            curr_obj = new_obj;
            // 最良解更新
            if (curr_obj < best_obj) {
                best = current;
                best_obj = curr_obj;
            }
            // A を差分更新
            for (size_t j = 0; j < DIM; ++j) {
                A[j] += delta * Q[k][j];
            }
        }

        // 温度を減衰
        temperature *= cooling_rate;
    }

    Solution sol;
    sol.theta = best;
    return sol;
}

/// 複数シードでベンチマーク実行
void run_mcmc_benchmark(
    uint64_t time_limit_ms,
    pair<double, double> bounds,
    double initial_temperature,
    double cooling_rate,
    double perturbation_std,
    uint64_t seed_start,
    uint64_t seed_end
) {
    auto t0 = chrono::steady_clock::now();
    double sum_score = 0.0;
    int cnt = 0;

    for (uint64_t seed = seed_start; seed <= seed_end; ++seed) {
        BenchmarkProblem prob(seed);
        Solution sol = solve_mcmc(
            prob, time_limit_ms,
            bounds, initial_temperature,
            cooling_rate, perturbation_std,
            seed
        );
        sum_score += prob.score(sol);
        ++cnt;
    }

    double avg = sum_score / cnt;
    auto elapsed = chrono::steady_clock::now() - t0;
    auto ms = chrono::duration_cast<chrono::milliseconds>(elapsed).count();

    cout << "平均スコア (MSE): " << avg << "\n";
    cout << "総処理時間: " << ms << " ms\n";
}

/// 差分更新の正しさを検証するテスト
void test_differential_update() {
    BenchmarkProblem prob(42);
    mt19937_64 rng(42 + 20000);
    uniform_real_distribution<double> unif(-15.0, 15.0);

    // ランダム初期解 theta
    array<double, DIM> theta;
    for (size_t k = 0; k < DIM; ++k) theta[k] = unif(rng);

    // 残差と目的関数を計算
    array<double, NUM_SAMPLES> r;
    for (size_t i = 0; i < NUM_SAMPLES; ++i) {
        double s = 0.0;
        for (size_t j = 0; j < DIM; ++j) s += prob.x[i][j] * theta[j];
        r[i] = s - prob.y[i];
    }
    double obj = 0.0;
    for (size_t i = 0; i < NUM_SAMPLES; ++i) obj += r[i] * r[i];

    // A, B 計算
    array<double, DIM> A, B;
    for (size_t j = 0; j < DIM; ++j) {
        double sa = 0.0, sb = 0.0;
        for (size_t i = 0; i < NUM_SAMPLES; ++i) {
            sa += r[i] * prob.x[i][j];
            sb += prob.x[i][j] * prob.x[i][j];
        }
        A[j] = sa;
        B[j] = sb;
    }

    // 1 次元 k=3, delta=0.3 の差分検証
    size_t k = 3;
    double delta = 0.3;
    double diff = 2.0 * delta * A[k] + delta * delta * B[k];
    double new_obj = obj + diff;
    assert(fabs(new_obj - (obj + diff)) < 1e-8);
}

int main() {
    // テスト実行
    test_differential_update();

    // MCMC ベンチマークパラメータ
    uint64_t time_limit_ms    = 2000;
    pair<double,double> bounds = {-15.0, 15.0};
    double initial_temperature = 1.0;
    double cooling_rate        = 0.995;
    double perturbation_std    = 0.5;

    // シード 1～100 でベンチマーク
    run_mcmc_benchmark(
        time_limit_ms,
        bounds,
        initial_temperature,
        cooling_rate,
        perturbation_std,
        1, 100
    );

    return 0;
}

// 実行結果
// 平均スコア (MSE): 2.94006e-06
// 総処理時間: 200306 ms
