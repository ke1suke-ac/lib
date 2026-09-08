#pragma once

#include <bits/stdc++.h>
using namespace std;

namespace tsp_problem {

namespace detail {

// 問題生成用の軽量乱数。
// sa 側の乱数と同じ splitmix64 系を使い、再現性を保つ。
struct FastRng {
    uint64_t state;

    explicit FastRng(uint64_t seed = 1) : state(seed) {}

    inline uint64_t operator()() {
        uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
};

}  // namespace detail

// 入力の 1 点を表す。
struct Point {
    int x = 0;
    int y = 0;
};

// TSP 問題入力。
// 比較実験でそのまま使いやすいよう、点列と距離行列を前計算して持つ。
struct Input {
    int n = 0;
    vector<Point> points;
    vector<vector<int>> dist;
};

// TSP の出力。
// route は巡回順序を表す 0-indexed の順列を想定する。
struct Output {
    vector<int> route;
};

// seed から決定的に問題インスタンスを生成する。
// 旧 sa_func のテストと同じく、120 点・座標 [0, 10000) を既定にする。
inline Input gen(uint64_t seed, int n = 120, int coord_max = 10000) {
    assert(n >= 2);
    assert(coord_max >= 1);

    detail::FastRng rng(seed);
    Input input;
    input.n = n;
    input.points.resize(n);
    input.dist.assign(n, vector<int>(n, 0));

    for (int i = 0; i < n; ++i) {
        input.points[i].x = static_cast<int>(rng() % static_cast<uint64_t>(coord_max));
        input.points[i].y = static_cast<int>(rng() % static_cast<uint64_t>(coord_max));
    }

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            const long long dx = static_cast<long long>(input.points[i].x) - input.points[j].x;
            const long long dy = static_cast<long long>(input.points[i].y) - input.points[j].y;
            input.dist[i][j] = static_cast<int>(llround(sqrt(static_cast<double>(dx * dx + dy * dy))));
        }
    }
    return input;
}

// 出力に対する巡回長を返す。
// route が不正なときは大きなペナルティ値を返す。
inline long long compute_score(const Input& input, const Output& output) {
    constexpr long long kInvalidScore = numeric_limits<long long>::max() / 4;
    if (static_cast<int>(output.route.size()) != input.n) return kInvalidScore;

    vector<unsigned char> seen(static_cast<size_t>(input.n), 0);
    long long score = 0;
    for (int i = 0; i < input.n; ++i) {
        const int v = output.route[i];
        if (v < 0 || v >= input.n) return kInvalidScore;
        if (seen[static_cast<size_t>(v)] != 0) return kInvalidScore;
        seen[static_cast<size_t>(v)] = 1;

        const int to = output.route[(i + 1) % input.n];
        if (to < 0 || to >= input.n) return kInvalidScore;
        score += input.dist[v][to];
    }
    return score;
}

}  // namespace tsp_problem
