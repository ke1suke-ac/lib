// main.cpp
#include <array>
#include <random>
#include <cmath>
#include <chrono>
#include <iostream>
#include <algorithm>

constexpr std::size_t DIM = 100;
constexpr std::size_t NUM_SAMPLES = 500;
constexpr double PI = std::acos(-1.0);

struct Solution {
    std::array<double, DIM> theta;
};

struct MultimodalBenchmarkProblem {
    std::array<std::array<double, DIM>, NUM_SAMPLES> x;
    std::array<double, NUM_SAMPLES> y;
    std::array<double, DIM> theta_true;

    MultimodalBenchmarkProblem(uint64_t seed) {
        std::mt19937_64 rng(seed);
        std::uniform_real_distribution<double> uni_dist(-5.0, 5.0);

        // 真のパラメータを生成
        for (std::size_t j = 0; j < DIM; ++j) {
            theta_true[j] = uni_dist(rng);
        }
        // 入力行列 X を生成
        for (std::size_t i = 0; i < NUM_SAMPLES; ++i) {
            for (std::size_t j = 0; j < DIM; ++j) {
                x[i][j] = uni_dist(rng);
            }
        }
        // ノイズ分布
        std::normal_distribution<double> noise_dist(0.0, 0.1);

        // 出力 y を生成
        for (std::size_t i = 0; i < NUM_SAMPLES; ++i) {
            double sum = 0.0;
            for (std::size_t j = 0; j < DIM; ++j) {
                double diff = x[i][j] - theta_true[j];
                sum += diff * diff - 10.0 * std::cos(2.0 * PI * diff);
            }
            sum += 10.0 * static_cast<double>(DIM);
            double noise = noise_dist(rng);
            y[i] = sum + noise;
        }
    }

    double score(const Solution& sol) const {
        double mse = 0.0;
        for (std::size_t j = 0; j < DIM; ++j) {
            double err = sol.theta[j] - theta_true[j];
            mse += err * err;
        }
        return mse / static_cast<double>(DIM);
    }
};

double objective(const MultimodalBenchmarkProblem& problem,
                 const std::array<double, DIM>& theta) {
    double total_error = 0.0;
    for (std::size_t i = 0; i < NUM_SAMPLES; ++i) {
        double pred = 0.0;
        for (std::size_t j = 0; j < DIM; ++j) {
            double diff = problem.x[i][j] - theta[j];
            pred += diff * diff - 10.0 * std::cos(2.0 * PI * diff);
        }
        pred += 10.0 * static_cast<double>(DIM);
        double err = pred - problem.y[i];
        total_error += err * err;
    }
    return total_error;
}

Solution solve_mcmc(
    const MultimodalBenchmarkProblem& problem,
    std::size_t max_iter,
    std::pair<double, double> bounds,
    double initial_temperature,
    double cooling_rate,
    double perturbation_std,
    uint64_t seed
) {
    double min_bound = bounds.first;
    double max_bound = bounds.second;

    std::mt19937_64 rng(seed + 20000);
    std::uniform_real_distribution<double> uni_init(min_bound, max_bound);
    std::normal_distribution<double> perturb_dist(0.0, perturbation_std);
    std::uniform_real_distribution<double> uni_zeroone(0.0, 1.0);

    // 初期解
    std::array<double, DIM> current;
    for (std::size_t i = 0; i < DIM; ++i) {
        current[i] = uni_init(rng);
    }
    double current_value = objective(problem, current);

    // ベスト解
    std::array<double, DIM> best = current;
    double best_value = current_value;

    double temperature = initial_temperature;

    for (std::size_t iter = 0; iter < max_iter; ++iter) {
        // 候補解生成
        std::array<double, DIM> candidate = current;
        for (std::size_t i = 0; i < DIM; ++i) {
            candidate[i] += perturb_dist(rng);
            candidate[i] = std::clamp(candidate[i], min_bound, max_bound);
        }
        double candidate_value = objective(problem, candidate);
        double delta = candidate_value - current_value;

        if (delta < 0.0) {
            current = candidate;
            current_value = candidate_value;
            if (candidate_value < best_value) {
                best = candidate;
                best_value = candidate_value;
            }
        } else {
            double acceptance = std::exp(-delta / temperature);
            if (uni_zeroone(rng) < acceptance) {
                current = candidate;
                current_value = candidate_value;
            }
        }
        temperature *= cooling_rate;
    }

    Solution sol;
    sol.theta = best;
    return sol;
}

void run_mcmc_benchmark(
    std::size_t max_iter,
    std::pair<double, double> bounds,
    double initial_temperature,
    double cooling_rate,
    double perturbation_std,
    uint64_t seed_start,
    uint64_t seed_end
) {
    using clock = std::chrono::high_resolution_clock;
    auto t0 = clock::now();

    double total_score = 0.0;
    uint64_t count = 0;

    for (uint64_t seed = seed_start; seed <= seed_end; ++seed) {
        MultimodalBenchmarkProblem problem(seed);
        Solution sol = solve_mcmc(
            problem,
            max_iter,
            bounds,
            initial_temperature,
            cooling_rate,
            perturbation_std,
            seed
        );
        total_score += problem.score(sol);
        ++count;
    }

    auto t1 = clock::now();
    std::chrono::duration<double> elapsed = t1 - t0;

    std::cout << "平均スコア (MSE): " << (total_score / static_cast<double>(count)) << "\n";
    std::cout << "処理時間: " << elapsed.count() << " 秒\n";
}

int main() {
    std::size_t max_iter = 1000;
    std::pair<double, double> bounds = { -15.0, 15.0 };
    double initial_temperature = 1.0;
    double cooling_rate = 0.995;
    double perturbation_std = 0.5;

    run_mcmc_benchmark(
        max_iter,
        bounds,
        initial_temperature,
        cooling_rate,
        perturbation_std,
        1, 100
    );

    return 0;
}

// 実行結果
// 平均スコア (MSE): 13.0293
// 処理時間: 155.223 秒
