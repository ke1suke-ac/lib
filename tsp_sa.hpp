#pragma once

// TSP / Hamilton path 用の焼きなまし法ライブラリ。
// 想定用途は AtCoder Heuristic Contest での再利用であり、
// ヘッダオンリー・単一ファイル・シングルスレッドで扱いやすく、
// かつ十分高速に動くことを重視している。
// 本ファイルは sa / SAParam のみを提供する。

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace tsp {

struct SAParam {
    std::uint64_t seed = 1;
    bool auto_mode = true;
    int samples = 300;
    double start_accept_prob = 0.8;
    double end_accept_prob = 0.01;
    double start_temp = 100.0;
    double end_temp = 1.0;
};

namespace sa_detail {

// 距離型 D は整数型・浮動小数点型を想定する。
template <class D>
inline constexpr bool k_supported_distance_v = std::is_arithmetic_v<D>;

// 差分計算用の型。
// 整数距離でも負の差分が安全に扱えるよう、内部ではより広い型を使う。
template <class D>
using diff_t = std::conditional_t<
    std::is_floating_point_v<D>, double,
    std::conditional_t<(sizeof(D) <= 4), std::int64_t, __int128_t>>;

template <class D>
constexpr diff_t<D> to_diff(D x) {
    return static_cast<diff_t<D>>(x);
}

template <class D>
constexpr D from_diff(diff_t<D> x) {
    return static_cast<D>(x);
}

inline int movable_vertex_count(int n, bool cycle, bool fixed_start, bool fixed_end) {
    int fixed = fixed_start ? 1 : 0;
    if (!cycle && fixed_end) ++fixed;
    return n - fixed;
}

// SA のベンチ用に、最後の総反復回数と受理回数を保持する。
inline std::uint64_t g_last_sa_iterations = 0;
inline std::uint64_t g_last_sa_accepted = 0;

// span 上の現在順序に対する距離和を返す。
template <std::integral T, class D, class F>
D total_dist_local(std::span<const T> order, F dist, bool cycle) {
    const int n = static_cast<int>(order.size());
    if (n <= 1) return D{};
    D sum = D{};
    for (int i = 0; i + 1 < n; ++i) sum += dist(order[i], order[i + 1]);
    if (cycle) sum += dist(order[n - 1], order[0]);
    return sum;
}

// mt19937_64 から [0,1) 一様乱数を得る。
template <class RNG>
inline double rand_unit(RNG& rng) {
    return static_cast<double>((rng() >> 11) * (1.0 / 9007199254740992.0));
}

// [lo, hi] の一様乱数を返す。hi >= lo を前提とする。
template <class RNG>
inline int rand_int(RNG& rng, int lo, int hi) {
    assert(lo <= hi);
    const std::uint64_t width = static_cast<std::uint64_t>(hi - lo + 1);
    return lo + static_cast<int>(rng() % width);
}

// SA 内部専用の軽量乱数生成器。
// 近傍提案では非常に多くの乱数を消費するため、mt19937_64 より軽い生成器を使う。
// splitmix64 系の更新則で、再現性を保ちつつコストを抑える。
struct FastRng {
    std::uint64_t state;

    explicit FastRng(std::uint64_t seed = 1) : state(seed) {}

    inline std::uint64_t operator()() {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
};

enum class Neighborhood : unsigned char {
    TwoOpt,
    OrOpt1,
    OrOpt2,
    OrOpt3,
    DoubleBridge,
};

template <class Diff>
struct Move {
    Neighborhood kind = Neighborhood::TwoOpt;
    int x = 0;
    int y = 0;
    int z = 0;
    Diff delta = Diff{};
};

struct WeightedNeighborhood {
    Neighborhood kind;
    int weight;
};

// パラメータの妥当性を検査する。
inline void validate_param(const SAParam& param) {
    assert(param.start_temp > 0.0);
    assert(param.end_temp > 0.0);
    assert(param.end_temp <= param.start_temp);
    if (param.auto_mode) {
        assert(param.samples > 0);
        assert(0.0 < param.end_accept_prob && param.end_accept_prob < param.start_accept_prob);
        assert(param.start_accept_prob < 1.0);
    }
}

// cycle/path と固定端点の組に対して 2-opt が存在し得るかを判定する。
inline bool has_two_opt_move(int n, bool cycle, bool fixed_start, bool fixed_end) {
    if (cycle) return n >= 3;
    const int l_begin = fixed_start ? 1 : 0;
    const int r_end = fixed_end ? (n - 2) : (n - 1);
    for (int l = l_begin; l < n - 1; ++l) {
        for (int r = l + 1; r <= r_end; ++r) {
            if (l == 0 && r == n - 1) continue;
            return true;
        }
    }
    return false;
}

// path 版 Or-opt-k が存在し得るかを判定する。
inline bool has_path_or_opt_move(int n, int k, bool fixed_start, bool fixed_end) {
    if (n <= k) return false;
    const int l_begin = fixed_start ? 1 : 0;
    const int l_end = fixed_end ? (n - k - 1) : (n - k);
    if (l_begin > l_end) return false;
    const int m = n - k;
    const int p_begin = fixed_start ? 1 : 0;
    const int p_end = fixed_end ? (m - 1) : m;
    if (p_begin > p_end) return false;
    for (int l = l_begin; l <= l_end; ++l) {
        if (p_begin < l || l < p_end) return true;
    }
    return false;
}

// cycle 版 Or-opt-k が存在し得るかを判定する。
inline bool has_cycle_or_opt_move(int n, int k, bool fixed_start) {
    if (n < k + 2) return false;
    const int l_begin = fixed_start ? 1 : 0;
    const int l_end = n - k;
    return l_begin <= l_end;
}

// 本実装の double-bridge(A B C D -> A C B D) が使えるかを判定する。
inline bool has_double_bridge_move(int n) {
    return n >= 4;
}

// 利用可能な近傍種別を重み付きで構築する。
inline std::vector<WeightedNeighborhood> build_neighborhoods(
    int n, bool cycle, bool fixed_start, bool fixed_end) {
    std::vector<WeightedNeighborhood> v;
    if (has_two_opt_move(n, cycle, fixed_start, fixed_end)) {
        v.push_back({Neighborhood::TwoOpt, 65});
    }
    if (cycle) {
        if (has_cycle_or_opt_move(n, 1, fixed_start)) v.push_back({Neighborhood::OrOpt1, 20});
        if (has_cycle_or_opt_move(n, 2, fixed_start)) v.push_back({Neighborhood::OrOpt2, 10});
        if (has_double_bridge_move(n)) v.push_back({Neighborhood::DoubleBridge, 5});
    } else {
        if (has_path_or_opt_move(n, 1, fixed_start, fixed_end)) v.push_back({Neighborhood::OrOpt1, 20});
        if (has_path_or_opt_move(n, 2, fixed_start, fixed_end)) v.push_back({Neighborhood::OrOpt2, 10});
        if (has_path_or_opt_move(n, 3, fixed_start, fixed_end)) v.push_back({Neighborhood::OrOpt3, 5});
    }
    return v;
}

// 重みに従って近傍種別を 1 つ選ぶ。
template <class RNG>
Neighborhood choose_neighborhood(const std::vector<WeightedNeighborhood>& neighborhoods, RNG& rng) {
    assert(!neighborhoods.empty());
    int sum = 0;
    for (const auto& e : neighborhoods) sum += e.weight;
    int x = rand_int(rng, 1, sum);
    for (const auto& e : neighborhoods) {
        x -= e.weight;
        if (x <= 0) return e.kind;
    }
    return neighborhoods.back().kind;
}

// path の「block を除いた残り列」の idx 番目の頂点を返す。
template <std::integral T>
inline T remaining_at(std::span<const T> order, int l, int k, int idx) {
    return order[(idx < l) ? idx : (idx + k)];
}

// 2-opt の差分を計算する。
template <class D, std::integral T, class F>
diff_t<D> delta_two_opt(std::span<const T> order, F dist, bool cycle, int l, int r) {
    using Diff = diff_t<D>;
    const int n = static_cast<int>(order.size());
    if (cycle) {
        const int prev = (l == 0 ? n - 1 : l - 1);
        const int next = (r + 1 == n ? 0 : r + 1);
        const Diff old_cost = static_cast<Diff>(dist(order[prev], order[l])) +
                              static_cast<Diff>(dist(order[r], order[next]));
        const Diff new_cost = static_cast<Diff>(dist(order[prev], order[r])) +
                              static_cast<Diff>(dist(order[l], order[next]));
        return new_cost - old_cost;
    }

    Diff old_cost = 0;
    Diff new_cost = 0;
    if (l == 0) {
        old_cost = static_cast<Diff>(dist(order[r], order[r + 1]));
        new_cost = static_cast<Diff>(dist(order[0], order[r + 1]));
    } else if (r == n - 1) {
        old_cost = static_cast<Diff>(dist(order[l - 1], order[l]));
        new_cost = static_cast<Diff>(dist(order[l - 1], order[n - 1]));
    } else {
        old_cost = static_cast<Diff>(dist(order[l - 1], order[l])) +
                   static_cast<Diff>(dist(order[r], order[r + 1]));
        new_cost = static_cast<Diff>(dist(order[l - 1], order[r])) +
                   static_cast<Diff>(dist(order[l], order[r + 1]));
    }
    return new_cost - old_cost;
}

// 2-opt を適用する。
template <std::integral T>
inline void apply_two_opt(std::span<T> order, int l, int r) {
    std::reverse(order.begin() + l, order.begin() + (r + 1));
}

// cycle 版 Or-opt-k の差分を計算する。after は挿入先の直前頂点の index。
template <class D, std::integral T, class F>
diff_t<D> delta_or_opt_cycle(std::span<const T> order, F dist, int l, int k, int after) {
    using Diff = diff_t<D>;
    const int n = static_cast<int>(order.size());
    const int r = l + k - 1;
    const int prev = (l == 0 ? n - 1 : l - 1);
    const int next = (r + 1 == n ? 0 : r + 1);
    const int after_next = (after + 1 == n ? 0 : after + 1);

    const Diff old_cost = static_cast<Diff>(dist(order[prev], order[l])) +
                          static_cast<Diff>(dist(order[r], order[next])) +
                          static_cast<Diff>(dist(order[after], order[after_next]));
    const Diff new_cost = static_cast<Diff>(dist(order[prev], order[next])) +
                          static_cast<Diff>(dist(order[after], order[l])) +
                          static_cast<Diff>(dist(order[r], order[after_next]));
    return new_cost - old_cost;
}

// cycle 版 Or-opt-k を適用する。after は元列上での挿入先の直前 index。
template <std::integral T>
inline void apply_or_opt_cycle(std::span<T> order, int l, int k, int after) {
    const int r = l + k - 1;
    if (after < l - 1) {
        std::rotate(order.begin() + (after + 1), order.begin() + l, order.begin() + (r + 1));
    } else {
        std::rotate(order.begin() + l, order.begin() + (r + 1), order.begin() + (after + 1));
    }
}

// path 版 Or-opt-k の差分を計算する。p は「block を除いた残り列」への挿入位置。
template <class D, std::integral T, class F>
diff_t<D> delta_or_opt_path(std::span<const T> order, F dist, int l, int k, int p) {
    using Diff = diff_t<D>;
    const int n = static_cast<int>(order.size());
    const int r = l + k - 1;
    const int m = n - k;

    const T first = order[l];
    const T last = order[r];

    Diff delta = 0;

    // 切り出しによる変化。
    if (l > 0) delta -= static_cast<Diff>(dist(order[l - 1], first));
    if (r + 1 < n) delta -= static_cast<Diff>(dist(last, order[r + 1]));
    if (l > 0 && r + 1 < n) delta += static_cast<Diff>(dist(order[l - 1], order[r + 1]));

    // 挿入による変化。
    if (p == 0) {
        delta += static_cast<Diff>(dist(last, remaining_at(order, l, k, 0)));
    } else if (p == m) {
        delta += static_cast<Diff>(dist(remaining_at(order, l, k, m - 1), first));
    } else {
        const T left = remaining_at(order, l, k, p - 1);
        const T right = remaining_at(order, l, k, p);
        delta -= static_cast<Diff>(dist(left, right));
        delta += static_cast<Diff>(dist(left, first));
        delta += static_cast<Diff>(dist(last, right));
    }
    return delta;
}

// path 版 Or-opt-k を適用する。p は「block を除いた残り列」への挿入位置。
template <std::integral T>
inline void apply_or_opt_path(std::span<T> order, int l, int k, int p) {
    const int r = l + k - 1;
    if (p < l) {
        std::rotate(order.begin() + p, order.begin() + l, order.begin() + (r + 1));
    } else {
        std::rotate(order.begin() + l, order.begin() + (r + 1), order.begin() + (p + k));
    }
}

// double-bridge(A B C D -> A C B D) の差分を計算する。
template <class D, std::integral T, class F>
diff_t<D> delta_double_bridge(std::span<const T> order, F dist, int a, int b, int c) {
    using Diff = diff_t<D>;
    const Diff old_cost = static_cast<Diff>(dist(order[a - 1], order[a])) +
                          static_cast<Diff>(dist(order[b - 1], order[b])) +
                          static_cast<Diff>(dist(order[c - 1], order[c]));
    const Diff new_cost = static_cast<Diff>(dist(order[a - 1], order[b])) +
                          static_cast<Diff>(dist(order[c - 1], order[a])) +
                          static_cast<Diff>(dist(order[b - 1], order[c]));
    return new_cost - old_cost;
}

// double-bridge(A B C D -> A C B D) を適用する。
template <std::integral T>
inline void apply_double_bridge(std::span<T> order, int a, int b, int c, std::vector<T>& scratch) {
    const int n = static_cast<int>(order.size());
    if (static_cast<int>(scratch.size()) != n) scratch.resize(n);
    int idx = 0;
    for (int i = 0; i < a; ++i) scratch[idx++] = order[i];
    for (int i = b; i < c; ++i) scratch[idx++] = order[i];
    for (int i = a; i < b; ++i) scratch[idx++] = order[i];
    for (int i = c; i < n; ++i) scratch[idx++] = order[i];
    for (int i = 0; i < n; ++i) order[i] = scratch[i];
}

// 2-opt をランダムに 1 手提案する。
template <class D, std::integral T, class F, class RNG>
bool propose_two_opt(std::span<const T> order, F dist, bool cycle,
                     bool fixed_start, bool fixed_end, RNG& rng,
                     Move<diff_t<D>>& mv) {
    const int n = static_cast<int>(order.size());
    if (!has_two_opt_move(n, cycle, fixed_start, fixed_end)) return false;

    if (cycle) {
        const int l_begin = fixed_start ? 1 : 0;
        for (int attempt = 0; attempt < 64; ++attempt) {
            const int l = rand_int(rng, l_begin, n - 2);
            const int r = rand_int(rng, l + 1, n - 1);
            if (l == 0 && r == n - 1) continue;
            mv.kind = Neighborhood::TwoOpt;
            mv.x = l;
            mv.y = r;
            mv.delta = delta_two_opt<D>(order, dist, true, l, r);
            return true;
        }
        return false;
    }

    const int l_begin = fixed_start ? 1 : 0;
    const int r_end = fixed_end ? (n - 2) : (n - 1);
    for (int attempt = 0; attempt < 64; ++attempt) {
        const int l = rand_int(rng, l_begin, n - 2);
        if (l + 1 > r_end) continue;
        const int r = rand_int(rng, l + 1, r_end);
        if (l == 0 && r == n - 1) continue;
        mv.kind = Neighborhood::TwoOpt;
        mv.x = l;
        mv.y = r;
        mv.delta = delta_two_opt<D>(order, dist, false, l, r);
        return true;
    }
    return false;
}

// cycle 版 Or-opt-k をランダムに 1 手提案する。
template <class D, std::integral T, class F, class RNG>
bool propose_or_opt_cycle(std::span<const T> order, F dist, int k,
                          bool fixed_start, RNG& rng, Move<diff_t<D>>& mv) {
    const int n = static_cast<int>(order.size());
    if (!has_cycle_or_opt_move(n, k, fixed_start)) return false;
    const int l_begin = fixed_start ? 1 : 0;
    const int l_end = n - k;

    for (int attempt = 0; attempt < 64; ++attempt) {
        const int l = rand_int(rng, l_begin, l_end);
        const int r = l + k - 1;
        const int after = rand_int(rng, 0, n - 1);
        if (l <= after && after <= r) continue;
        const int old_prev = (l == 0 ? n - 1 : l - 1);
        if (after == old_prev) continue;
        mv.kind = (k == 1 ? Neighborhood::OrOpt1 : Neighborhood::OrOpt2);
        mv.x = l;
        mv.y = k;
        mv.z = after;
        mv.delta = delta_or_opt_cycle<D>(order, dist, l, k, after);
        return true;
    }
    return false;
}

// path 版 Or-opt-k をランダムに 1 手提案する。
template <class D, std::integral T, class F, class RNG>
bool propose_or_opt_path(std::span<const T> order, F dist, int k,
                         bool fixed_start, bool fixed_end,
                         RNG& rng, Move<diff_t<D>>& mv) {
    const int n = static_cast<int>(order.size());
    if (!has_path_or_opt_move(n, k, fixed_start, fixed_end)) return false;
    const int l_begin = fixed_start ? 1 : 0;
    const int l_end = fixed_end ? (n - k - 1) : (n - k);
    const int m = n - k;
    const int p_begin = fixed_start ? 1 : 0;
    const int p_end = fixed_end ? (m - 1) : m;

    for (int attempt = 0; attempt < 64; ++attempt) {
        const int l = rand_int(rng, l_begin, l_end);
        const int p = rand_int(rng, p_begin, p_end);
        if (p == l) continue;
        mv.kind = (k == 1 ? Neighborhood::OrOpt1 : (k == 2 ? Neighborhood::OrOpt2 : Neighborhood::OrOpt3));
        mv.x = l;
        mv.y = k;
        mv.z = p;
        mv.delta = delta_or_opt_path<D>(order, dist, l, k, p);
        return true;
    }
    return false;
}

// double-bridge をランダムに 1 手提案する。
template <class D, std::integral T, class F, class RNG>
bool propose_double_bridge(std::span<const T> order, F dist, RNG& rng, Move<diff_t<D>>& mv) {
    const int n = static_cast<int>(order.size());
    if (!has_double_bridge_move(n)) return false;
    const int a = rand_int(rng, 1, n - 3);
    const int b = rand_int(rng, a + 1, n - 2);
    const int c = rand_int(rng, b + 1, n - 1);
    mv.kind = Neighborhood::DoubleBridge;
    mv.x = a;
    mv.y = b;
    mv.z = c;
    mv.delta = delta_double_bridge<D>(order, dist, a, b, c);
    return true;
}

// 指定された近傍種別でランダムな 1 手を提案する。
template <class D, std::integral T, class F, class RNG>
bool propose_with_kind(std::span<const T> order, F dist, bool cycle,
                       bool fixed_start, bool fixed_end,
                       Neighborhood kind, RNG& rng, Move<diff_t<D>>& mv) {
    switch (kind) {
        case Neighborhood::TwoOpt:
            return propose_two_opt<D>(order, dist, cycle, fixed_start, fixed_end, rng, mv);
        case Neighborhood::OrOpt1:
            return cycle ? propose_or_opt_cycle<D>(order, dist, 1, fixed_start, rng, mv)
                         : propose_or_opt_path<D>(order, dist, 1, fixed_start, fixed_end, rng, mv);
        case Neighborhood::OrOpt2:
            return cycle ? propose_or_opt_cycle<D>(order, dist, 2, fixed_start, rng, mv)
                         : propose_or_opt_path<D>(order, dist, 2, fixed_start, fixed_end, rng, mv);
        case Neighborhood::OrOpt3:
            return propose_or_opt_path<D>(order, dist, 3, fixed_start, fixed_end, rng, mv);
        case Neighborhood::DoubleBridge:
            return propose_double_bridge<D>(order, dist, rng, mv);
    }
    return false;
}

// 利用可能な近傍からランダムに 1 手を提案する。
template <class D, std::integral T, class F, class RNG>
bool propose_random_move(std::span<const T> order, F dist, bool cycle,
                         bool fixed_start, bool fixed_end,
                         const std::vector<WeightedNeighborhood>& neighborhoods,
                         RNG& rng, Move<diff_t<D>>& mv) {
    if (neighborhoods.empty()) return false;
    for (int attempt = 0; attempt < 64; ++attempt) {
        const Neighborhood kind = choose_neighborhood(neighborhoods, rng);
        if (propose_with_kind<D>(order, dist, cycle, fixed_start, fixed_end, kind, rng, mv)) {
            return true;
        }
    }
    return false;
}

// 近傍手を実際に適用する。
template <std::integral T, class Diff>
void apply_move(std::span<T> order, const Move<Diff>& mv, bool cycle, std::vector<T>& scratch) {
    switch (mv.kind) {
        case Neighborhood::TwoOpt:
            apply_two_opt(order, mv.x, mv.y);
            return;
        case Neighborhood::OrOpt1:
        case Neighborhood::OrOpt2:
            if (cycle) {
                apply_or_opt_cycle(order, mv.x, mv.y, mv.z);
            } else {
                apply_or_opt_path(order, mv.x, mv.y, mv.z);
            }
            return;
        case Neighborhood::OrOpt3:
            apply_or_opt_path(order, mv.x, mv.y, mv.z);
            return;
        case Neighborhood::DoubleBridge:
            apply_double_bridge(order, mv.x, mv.y, mv.z, scratch);
            return;
    }
}

// 温度スケジュールは指数補間とする。
inline double current_temperature(double start_temp, double end_temp, double ratio) {
    if (ratio <= 0.0) return start_temp;
    if (ratio >= 1.0) return end_temp;
    return start_temp * std::pow(end_temp / start_temp, ratio);
}

// 自動温度推定。悪化手を十分集められない場合は manual 値へフォールバックする。
template <class D, std::integral T, class F, class RNG>
std::pair<double, double> estimate_temps(
    const SAParam& param, std::span<const T> order, F dist, bool cycle,
    bool fixed_start, bool fixed_end,
    const std::vector<WeightedNeighborhood>& neighborhoods,
    RNG& rng) {
    if (!param.auto_mode) {
        return {static_cast<double>(param.start_temp), static_cast<double>(param.end_temp)};
    }

    std::vector<double> worsening;
    worsening.reserve(param.samples);
    const int max_trials = std::max(param.samples * 50, 500);
    Move<diff_t<D>> mv;

    for (int trial = 0; trial < max_trials && static_cast<int>(worsening.size()) < param.samples; ++trial) {
        if (!propose_random_move<D>(order, dist, cycle, fixed_start, fixed_end, neighborhoods, rng, mv)) {
            break;
        }
        if (mv.delta > 0) {
            worsening.push_back(static_cast<double>(mv.delta));
        }
    }

    if (static_cast<int>(worsening.size()) < std::max(8, param.samples / 4)) {
        return {static_cast<double>(param.start_temp), static_cast<double>(param.end_temp)};
    }

    std::sort(worsening.begin(), worsening.end());
    const int count = static_cast<int>(worsening.size());
    const double start_ref = worsening[(count - 1) * 7 / 10];
    const int small_count = std::max(1, count / 20);
    double end_ref = 0.0;
    for (int i = 0; i < small_count; ++i) end_ref += worsening[i];
    end_ref /= static_cast<double>(small_count);

    if (!(start_ref > 0.0) || !(end_ref > 0.0)) {
        return {static_cast<double>(param.start_temp), static_cast<double>(param.end_temp)};
    }

    const double p_start = static_cast<double>(param.start_accept_prob);
    const double p_end = static_cast<double>(param.end_accept_prob);
    double t_start = -start_ref / std::log(p_start);
    double t_end = -end_ref / std::log(p_end);

    // サンプル分布の裾が重いと終盤温度が緩すぎることがあるため、
    // 自動推定後も一定以上は十分に冷えるよう上限制約を入れる。
    t_end = std::min(t_end, t_start * 1.0e-3);

    if (!(t_start > 0.0) || !(t_end > 0.0) || !(t_end <= t_start)) {
        return {static_cast<double>(param.start_temp), static_cast<double>(param.end_temp)};
    }
    return {t_start, t_end};
}

}  // namespace sa_detail

// 焼きなまし法。
// cycle=true のとき: two-opt 65%, Or-opt-1 20%, Or-opt-2 10%, double-bridge 5%
// cycle=false のとき: two-opt 65%, Or-opt-1 20%, Or-opt-2 10%, Or-opt-3 5%
// 差分計算は O(1) で行い、最良解を order に書き戻す。
// 計算量: 1 反復あたり差分計算 O(1)、配列更新は近傍適用コストに依存。
template <std::integral T, class D, class F>
D sa(const SAParam& param, int time_limit_ms, std::span<T> order, F dist,
     bool cycle = true, bool fixed_start = false, bool fixed_end = false) {
    static_assert(sa_detail::k_supported_distance_v<D>, "D must be an arithmetic type.");
    sa_detail::validate_param(param);

    const int n = static_cast<int>(order.size());
    if (n <= 1) {
        sa_detail::g_last_sa_iterations = 0;
        sa_detail::g_last_sa_accepted = 0;
        return D{};
    }
    if (sa_detail::movable_vertex_count(n, cycle, fixed_start, fixed_end) <= 1 || time_limit_ms <= 0) {
        sa_detail::g_last_sa_iterations = 0;
        sa_detail::g_last_sa_accepted = 0;
        return sa_detail::total_dist_local<T, D>(order, dist, cycle);
    }

    const auto neighborhoods = sa_detail::build_neighborhoods(n, cycle, fixed_start, fixed_end);
    if (neighborhoods.empty()) {
        sa_detail::g_last_sa_iterations = 0;
        sa_detail::g_last_sa_accepted = 0;
        return sa_detail::total_dist_local<T, D>(order, dist, cycle);
    }

    sa_detail::FastRng rng(param.seed);
    const auto time_begin = std::chrono::steady_clock::now();
    const auto time_deadline = time_begin + std::chrono::milliseconds(time_limit_ms);

    const auto [start_temp, end_temp] = sa_detail::estimate_temps<D>(
        param, std::span<const T>(order.data(), order.size()), dist,
        cycle, fixed_start, fixed_end, neighborhoods, rng);

    using Diff = sa_detail::diff_t<D>;
    Diff cur_score = sa_detail::to_diff(sa_detail::total_dist_local<T, D>(order, dist, cycle));
    Diff best_score = cur_score;

    std::vector<T> best_order(order.begin(), order.end());
    std::vector<T> scratch(order.size());

    std::uint64_t iterations = 0;
    std::uint64_t accepted = 0;
    sa_detail::Move<Diff> mv;
    auto now = time_begin;

    while (true) {
        now = std::chrono::steady_clock::now();
        if (now >= time_deadline) break;

        const double elapsed_ms =
            std::chrono::duration<double, std::milli>(now - time_begin).count();
        const double ratio = std::min<double>(
            1.0, elapsed_ms / static_cast<double>(time_limit_ms));
        // 時刻確認のたびに温度を 1 回だけ更新し、同じ短いバッチの間は再利用する。
        // これにより pow / 時刻差計算の呼び出し回数を大きく減らす。
        const double temp = sa_detail::current_temperature(start_temp, end_temp, ratio);
        const double inv_temp = 1.0 / temp;

        for (int batch = 0; batch < 512; ++batch) {
            if (!sa_detail::propose_random_move<D>(std::span<const T>(order.data(), order.size()), dist,
                                                   cycle, fixed_start, fixed_end,
                                                   neighborhoods, rng, mv)) {
                batch = 256;
                break;
            }

            const bool accept = (mv.delta <= 0) ||
                                (sa_detail::rand_unit(rng) < std::exp(-static_cast<double>(mv.delta) * inv_temp));
            if (accept) {
                sa_detail::apply_move(order, mv, cycle, scratch);
                cur_score += mv.delta;
                ++accepted;
                if (cur_score < best_score) {
                    best_score = cur_score;
                    std::copy(order.begin(), order.end(), best_order.begin());
                }
            }
            ++iterations;
        }
    }

    sa_detail::g_last_sa_iterations = iterations;
    sa_detail::g_last_sa_accepted = accepted;

    if (!std::equal(order.begin(), order.end(), best_order.begin())) {
        std::copy(best_order.begin(), best_order.end(), order.begin());
    }
    return sa_detail::total_dist_local<T, D>(order, dist, cycle);
}

// パラメータ省略版。D は dist の戻り値型から推論する。
template <std::integral T, class F>
auto sa(int time_limit_ms, std::span<T> order, F dist,
        bool cycle = true, bool fixed_start = false, bool fixed_end = false)
    -> std::decay_t<std::invoke_result_t<F, T, T>> {
    using D = std::decay_t<std::invoke_result_t<F, T, T>>;
    return sa<T, D>(tsp::SAParam{}, time_limit_ms, order, dist, cycle, fixed_start, fixed_end);
}

}  // namespace tsp

#if __INCLUDE_LEVEL__ == 0

#include "tsp.hpp"

#include <array>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>

namespace {

[[nodiscard]] std::uint64_t splitmix64(std::uint64_t& x) {
    std::uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

template <class D>
std::vector<std::vector<D>> random_symmetric_matrix(int n, std::uint64_t seed, D max_w) {
    std::vector<std::vector<D>> mat(n, std::vector<D>(n, D{}));
    std::uint64_t s = seed;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if constexpr (std::is_floating_point_v<D>) {
                const double r = static_cast<double>(splitmix64(s) & ((1ULL << 53) - 1)) /
                                 static_cast<double>(1ULL << 53);
                const D w = static_cast<D>(1e-3 + r * static_cast<double>(max_w));
                mat[i][j] = mat[j][i] = w;
            } else {
                const D w = static_cast<D>(1 + splitmix64(s) % static_cast<std::uint64_t>(max_w));
                mat[i][j] = mat[j][i] = w;
            }
        }
    }
    return mat;
}

bool same_multiset(std::span<const int> a, std::span<const int> b) {
    std::vector<int> x(a.begin(), a.end()), y(b.begin(), b.end());
    std::sort(x.begin(), x.end());
    std::sort(y.begin(), y.end());
    return x == y;
}

struct MoveCheckStats {
    int checked = 0;
};

// 差分計算と適用処理が整合しているかを検証する。
void run_move_delta_tests() {
    std::mt19937_64 rng(123456789ULL);
    for (int n = 4; n <= 16; ++n) {
        for (int tc = 0; tc < 80; ++tc) {
            auto mat = random_symmetric_matrix<long long>(n, 1000ULL + tc * 17 + n * 131, 1000LL);
            auto dist = [&](int a, int b) { return mat[a][b]; };
            std::vector<int> order(n);
            std::iota(order.begin(), order.end(), 0);
            std::shuffle(order.begin(), order.end(), rng);
            std::vector<int> scratch(n);

            for (bool cycle : {false, true}) {
                for (bool fixed_start : {false, true}) {
                    for (bool fixed_end : {false, true}) {
                        if (cycle && fixed_end) continue;
                        const auto neighborhoods = tsp::sa_detail::build_neighborhoods(
                            n, cycle, fixed_start, fixed_end);
                        if (neighborhoods.empty()) continue;
                        for (int rep = 0; rep < 80; ++rep) {
                            tsp::sa_detail::Move<tsp::sa_detail::diff_t<long long>> mv;
                            if (!tsp::sa_detail::propose_random_move<long long>(
                                    std::span<const int>(order.data(), order.size()), dist,
                                    cycle, fixed_start, fixed_end, neighborhoods, rng, mv)) {
                                continue;
                            }
                            const auto before = tsp::sa_detail::total_dist_local<int, long long>(order, dist, cycle);
                            auto copy = order;
                            tsp::sa_detail::apply_move<int>(copy, mv, cycle, scratch);
                            const auto after = tsp::sa_detail::total_dist_local<int, long long>(copy, dist, cycle);
                            const auto diff = tsp::sa_detail::from_diff<long long>(
                                tsp::sa_detail::to_diff(after) - tsp::sa_detail::to_diff(before));
                            assert(diff == tsp::sa_detail::from_diff<long long>(mv.delta));
                        }
                    }
                }
            }
        }
    }
}

void run_deterministic_tests() {
    {
        std::vector<int> ord = {0, 1, 2, 3, 4};
        auto dist = [](int a, int b) {
            static const long long mat[5][5] = {
                {0, 1, 100, 100, 1},
                {1, 0, 1, 100, 100},
                {100, 1, 0, 1, 100},
                {100, 100, 1, 0, 1},
                {1, 100, 100, 1, 0},
            };
            return mat[a][b];
        };
        const auto before = tsp::total_dist<int, long long>(ord, dist, true);
        const auto after = tsp::sa<int, long long>(tsp::SAParam{}, 0, ord, dist, true, false, false);
        assert(before == after);
        assert(ord == std::vector<int>({0, 1, 2, 3, 4}));
    }
    {
        // path + 両端固定でも端点が壊れないことを確認する。
        std::vector<std::vector<long long>> mat(6, std::vector<long long>(6, 100));
        for (int i = 0; i < 6; ++i) mat[i][i] = 0;
        mat[0][5] = mat[5][0] = 1;
        mat[0][1] = mat[1][0] = 1;
        mat[1][2] = mat[2][1] = 1;
        mat[2][3] = mat[3][2] = 1;
        mat[3][4] = mat[4][3] = 1;
        mat[4][5] = mat[5][4] = 1;
        auto dist = [&](int a, int b) { return mat[a][b]; };
        std::vector<int> ord = {0, 3, 2, 4, 1, 5};
        const auto before = tsp::total_dist<int, long long>(ord, dist, false);
        const auto after = tsp::sa<int, long long>(tsp::SAParam{.seed = 7, .auto_mode = false, .samples = 300,
                                                           .start_accept_prob = 0.8, .end_accept_prob = 0.01,
                                                           .start_temp = 50.0, .end_temp = 0.1},
                                                   10, ord, dist, false, true, true);
        assert(ord.front() == 0);
        assert(ord.back() == 5);
        assert(after <= before);
    }
    {
        // 既に最適な順列に対しても、最終的には悪化した解を返さないことを確認する。
        std::vector<std::vector<long long>> mat = {
            {0, 1, 100, 100},
            {1, 0, 1, 100},
            {100, 1, 0, 1},
            {100, 100, 1, 0},
        };
        auto dist = [&](int a, int b) { return mat[a][b]; };
        std::vector<int> ord = {0, 1, 2, 3};
        const auto before = tsp::total_dist<int, long long>(ord, dist, false);
        const auto after = tsp::sa<int, long long>(tsp::SAParam{.seed = 11, .auto_mode = true}, 5,
                                                   ord, dist, false, false, false);
        assert(after <= before);
    }
}

void run_randomized_consistency_tests() {
    std::mt19937_64 rng(998244353ULL);
    for (int tc = 0; tc < 140; ++tc) {
        const int n = 2 + static_cast<int>(rng() % 12);
        auto mat_ll = random_symmetric_matrix<long long>(n, 5000ULL + tc * 19, 3000LL);
        auto mat_d = random_symmetric_matrix<double>(n, 9000ULL + tc * 37, 3000.0);
        auto dist_ll = [&](int a, int b) { return mat_ll[a][b]; };
        auto dist_d = [&](int a, int b) { return mat_d[a][b]; };

        for (bool cycle : {false, true}) {
            for (bool fixed_start : {false, true}) {
                for (bool fixed_end : {false, true}) {
                    if (cycle && fixed_end) continue;

                    std::vector<int> ord(n);
                    std::iota(ord.begin(), ord.end(), 0);
                    std::shuffle(ord.begin(), ord.end(), rng);
                    const auto initial_ord = ord;
                    const auto before = tsp::total_dist<int, long long>(ord, dist_ll, cycle);
                    const auto ret = tsp::sa<int, long long>(
                        tsp::SAParam{.seed = 100000ULL + static_cast<std::uint64_t>(tc),
                                .auto_mode = true,
                                .samples = 100,
                                .start_accept_prob = 0.8,
                                .end_accept_prob = 0.01,
                                .start_temp = 100.0,
                                .end_temp = 1.0},
                        4, ord, dist_ll, cycle, fixed_start, fixed_end);
                    assert((ret == tsp::total_dist<int, long long>(ord, dist_ll, cycle)));
                    assert(ret <= before);
                    std::vector<int> base(n);
                    std::iota(base.begin(), base.end(), 0);
                    assert(same_multiset(ord, base));
                    if (fixed_start) assert(ord.front() == initial_ord.front());
                    if (!cycle && fixed_end) assert(ord.back() == initial_ord.back());

                    std::vector<int> ord2(n);
                    std::iota(ord2.begin(), ord2.end(), 0);
                    std::shuffle(ord2.begin(), ord2.end(), rng);
                    const auto before2 = tsp::total_dist<int, double>(ord2, dist_d, cycle);
                    const auto ret2 = tsp::sa<int, double>(
                        tsp::SAParam{.seed = 200000ULL + static_cast<std::uint64_t>(tc),
                                .auto_mode = false,
                                .samples = 300,
                                .start_accept_prob = 0.8,
                                .end_accept_prob = 0.01,
                                .start_temp = 50.0,
                                .end_temp = 0.1},
                        4, ord2, dist_d, cycle, fixed_start, fixed_end);
                    assert((ret2 == tsp::total_dist<int, double>(ord2, dist_d, cycle)));
                    assert(ret2 <= before2 + 1e-12);
                    assert(same_multiset(ord2, base));
                }
            }
        }
    }
}

struct BenchDataset {
    std::vector<std::vector<long long>> mat;
    std::vector<int> two_opt_initial_order;
};

std::vector<BenchDataset> make_benchmark_datasets(int cases, int n) {
    std::vector<BenchDataset> datasets;
    datasets.reserve(cases);
    for (int tc = 0; tc < cases; ++tc) {
        std::vector<std::pair<int, int>> pts(n);
        std::mt19937 rng(3000 + tc * 17 + n * 13);
        std::uniform_int_distribution<int> coord(0, 1000000);
        for (auto& [x, y] : pts) {
            x = coord(rng);
            y = coord(rng);
        }

        BenchDataset ds;
        ds.mat.assign(n, std::vector<long long>(n, 0));
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                const long long dx = static_cast<long long>(pts[i].first) - pts[j].first;
                const long long dy = static_cast<long long>(pts[i].second) - pts[j].second;
                const long long d = static_cast<long long>(
                    std::llround(std::sqrt(static_cast<double>(dx * dx + dy * dy))));
                ds.mat[i][j] = ds.mat[j][i] = d;
            }
        }

        ds.two_opt_initial_order.resize(n);
        std::iota(ds.two_opt_initial_order.begin(), ds.two_opt_initial_order.end(), 0);
        std::mt19937_64 init_rng(0x9E3779B97F4A7C15ULL + static_cast<std::uint64_t>(tc) * 911ULL +
                                 static_cast<std::uint64_t>(n) * 131ULL);
        std::shuffle(ds.two_opt_initial_order.begin(), ds.two_opt_initial_order.end(), init_rng);
        datasets.push_back(std::move(ds));
    }
    return datasets;
}


struct ImprovementCaseResult {
    long long before = 0;
    long long after = 0;
    double improvement_percent = 0.0;
    std::uint64_t iterations = 0;
    double time_ms = 0.0;
};

struct ImprovementSummary {
    std::string name;
    double avg_before = 0.0;
    double avg_after = 0.0;
    double imp_mean = 0.0;
    double imp_min = 0.0;
    double imp_max = 0.0;
    double imp_stddev = 0.0;
    double avg_iterations = 0.0;
    double avg_time_ms = 0.0;
    double min_time_ms = 0.0;
    double max_time_ms = 0.0;
};

template <class InitialSolver>
std::vector<ImprovementCaseResult> benchmark_sa_from_solver(
    const std::vector<BenchDataset>& datasets,
    const tsp::SAParam& base_param,
    int sa_time_limit_ms,
    int solver_id,
    InitialSolver initial_solver) {
    std::vector<ImprovementCaseResult> results;
    results.reserve(datasets.size());

    for (int tc = 0; tc < static_cast<int>(datasets.size()); ++tc) {
        const auto& ds = datasets[tc];
        const int n = static_cast<int>(ds.mat.size());
        auto dist = [&](int a, int b) { return ds.mat[a][b]; };

        std::vector<int> order(n);
        std::iota(order.begin(), order.end(), 0);
        const long long before = initial_solver(ds, order, dist);
        assert((before == tsp::total_dist<int, long long>(order, dist, true)));

        tsp::SAParam param = base_param;
        param.seed += static_cast<std::uint64_t>(tc) * 1000003ULL +
                      static_cast<std::uint64_t>(solver_id) * 10007ULL;

        tsp::sa_detail::g_last_sa_iterations = 0;
        tsp::sa_detail::g_last_sa_accepted = 0;
        const auto t0 = std::chrono::steady_clock::now();
        const long long after = tsp::sa<int, long long>(param, sa_time_limit_ms, order, dist,
                                                        true, false, false);
        const auto t1 = std::chrono::steady_clock::now();
        const double time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        assert((after == tsp::total_dist<int, long long>(order, dist, true)));
        assert(after <= before);
        std::vector<int> base(n);
        std::iota(base.begin(), base.end(), 0);
        assert(same_multiset(order, base));

        const double imp = (before > 0)
                                    ? (static_cast<double>(before - after) * 100.0 /
                                       static_cast<double>(before))
                                    : 0.0;
        results.push_back(ImprovementCaseResult{
            .before = before,
            .after = after,
            .improvement_percent = imp,
            .iterations = tsp::sa_detail::g_last_sa_iterations,
            .time_ms = time_ms,
        });
    }

    return results;
}

template <class InitialSolver>
std::vector<ImprovementCaseResult> benchmark_two_opt_from_solver(
    const std::vector<BenchDataset>& datasets,
    InitialSolver initial_solver) {
    std::vector<ImprovementCaseResult> results;
    results.reserve(datasets.size());

    for (int tc = 0; tc < static_cast<int>(datasets.size()); ++tc) {
        const auto& ds = datasets[tc];
        const int n = static_cast<int>(ds.mat.size());
        auto dist = [&](int a, int b) { return ds.mat[a][b]; };

        std::vector<int> order(n);
        std::iota(order.begin(), order.end(), 0);
        const long long before = initial_solver(ds, order, dist);
        assert((before == tsp::total_dist<int, long long>(order, dist, true)));

        const auto t0 = std::chrono::steady_clock::now();
        const long long after = tsp::two_opt<int, long long>(order, dist, true, false, false);
        const auto t1 = std::chrono::steady_clock::now();
        const double time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        assert((after == tsp::total_dist<int, long long>(order, dist, true)));
        assert(after <= before);
        std::vector<int> base(n);
        std::iota(base.begin(), base.end(), 0);
        assert(same_multiset(order, base));

        const double imp = (before > 0)
                                    ? (static_cast<double>(before - after) * 100.0 /
                                       static_cast<double>(before))
                                    : 0.0;
        results.push_back(ImprovementCaseResult{
            .before = before,
            .after = after,
            .improvement_percent = imp,
            .iterations = 0,
            .time_ms = time_ms,
        });
    }

    return results;
}

ImprovementSummary summarize_improvement(const std::string& name,
                                         const std::vector<ImprovementCaseResult>& results) {
    ImprovementSummary s;
    s.name = name;
    if (results.empty()) return s;

    double sum_before = 0.0;
    double sum_after = 0.0;
    double sum_imp = 0.0;
    double sum_imp_sq = 0.0;
    double sum_iter = 0.0;
    double sum_time = 0.0;
    double min_time = std::numeric_limits<double>::infinity();
    double max_time = 0.0;
    double min_imp = std::numeric_limits<double>::infinity();
    double max_imp = -std::numeric_limits<double>::infinity();

    for (const auto& r : results) {
        sum_before += static_cast<double>(r.before);
        sum_after += static_cast<double>(r.after);
        sum_imp += r.improvement_percent;
        sum_imp_sq += r.improvement_percent * r.improvement_percent;
        sum_iter += static_cast<double>(r.iterations);
        sum_time += r.time_ms;
        min_time = std::min(min_time, r.time_ms);
        max_time = std::max(max_time, r.time_ms);
        min_imp = std::min(min_imp, r.improvement_percent);
        max_imp = std::max(max_imp, r.improvement_percent);
    }

    const double inv = 1.0 / static_cast<double>(results.size());
    s.avg_before = sum_before * inv;
    s.avg_after = sum_after * inv;
    s.imp_mean = sum_imp * inv;
    s.imp_min = min_imp;
    s.imp_max = max_imp;
    s.imp_stddev = std::sqrt(std::max<double>(0.0, sum_imp_sq * inv - s.imp_mean * s.imp_mean));
    s.avg_iterations = sum_iter * inv;
    s.avg_time_ms = sum_time * inv;
    s.min_time_ms = min_time;
    s.max_time_ms = max_time;
    return s;
}

void print_sa_improvement_table(int n, int cases, int sa_time_limit_ms,
                                const std::vector<ImprovementSummary>& summary) {
    std::cout << "\n[SA improvement benchmark]\n";
    std::cout << "N = " << n << ", cases = " << cases
              << ", SA time_limit_ms = " << sa_time_limit_ms << "\n";
    std::cout << std::left << std::setw(28) << "initial solver"
              << std::right << std::setw(16) << "avg before"
              << std::setw(16) << "avg after"
              << std::setw(14) << "imp mean[%]"
              << std::setw(14) << "imp min[%]"
              << std::setw(14) << "imp max[%]"
              << std::setw(14) << "imp sd[%]"
              << std::setw(18) << "avg iterations"
              << std::setw(16) << "avg time[ms]"
              << std::setw(16) << "max time[ms]" << "\n";
    std::cout << std::string(150, '-') << "\n";
    std::cout << std::fixed << std::setprecision(3);
    for (const auto& row : summary) {
        std::cout << std::left << std::setw(28) << row.name
                  << std::right << std::setw(16) << row.avg_before
                  << std::setw(16) << row.avg_after
                  << std::setw(14) << row.imp_mean
                  << std::setw(14) << row.imp_min
                  << std::setw(14) << row.imp_max
                  << std::setw(14) << row.imp_stddev
                  << std::setw(18) << row.avg_iterations
                  << std::setw(16) << row.avg_time_ms
                  << std::setw(16) << row.max_time_ms << "\n";
    }
}

void print_two_opt_improvement_table(int n, int cases,
                                     const std::vector<ImprovementSummary>& summary) {
    std::cout << "\n[two_opt improvement benchmark]\n";
    std::cout << "N = " << n << ", cases = " << cases << "\n";
    std::cout << std::left << std::setw(28) << "initial solver"
              << std::right << std::setw(16) << "avg before"
              << std::setw(16) << "avg after"
              << std::setw(14) << "imp mean[%]"
              << std::setw(14) << "imp min[%]"
              << std::setw(14) << "imp max[%]"
              << std::setw(14) << "imp sd[%]"
              << std::setw(16) << "avg time[ms]"
              << std::setw(16) << "min time[ms]"
              << std::setw(16) << "max time[ms]" << "\n";
    std::cout << std::string(148, '-') << "\n";
    std::cout << std::fixed << std::setprecision(3);
    for (const auto& row : summary) {
        std::cout << std::left << std::setw(28) << row.name
                  << std::right << std::setw(16) << row.avg_before
                  << std::setw(16) << row.avg_after
                  << std::setw(14) << row.imp_mean
                  << std::setw(14) << row.imp_min
                  << std::setw(14) << row.imp_max
                  << std::setw(14) << row.imp_stddev
                  << std::setw(16) << row.avg_time_ms
                  << std::setw(16) << row.min_time_ms
                  << std::setw(16) << row.max_time_ms << "\n";
    }
}

void run_benchmarks_main() {
    std::cout << "\n[Benchmark]\n";
    std::cout << "setting: cycle=true, fixed_start=false, fixed_end=false\n";
    std::cout << "N=100 の同一データセット群に対して、各初期ソルバーの出力を SA(auto_mode=true) / two_opt で改善した結果を比較する。\n";
    std::cout << "表示する改善率は (before - after) / before * 100 [%]。\n";

    constexpr int n = 100;
    constexpr int cases = 20;
    constexpr int sa_time_limit_ms = 100;
    const auto datasets = make_benchmark_datasets(cases, n);

    tsp::SAParam base_param;
    base_param.seed = 1;
    base_param.auto_mode = true;
    base_param.samples = 300;
    base_param.start_accept_prob = 0.8;
    base_param.end_accept_prob = 0.01;
    base_param.start_temp = 100.0;
    base_param.end_temp = 1.0;

    std::vector<ImprovementSummary> sa_summary;
    std::vector<ImprovementSummary> two_opt_summary;

    auto initial_nn = [](const BenchDataset&, std::vector<int>& ord, auto dist) {
        return tsp::nearest_neighbor<int, long long>(ord, dist, true, false, false);
    };
    auto initial_fi = [](const BenchDataset&, std::vector<int>& ord, auto dist) {
        return tsp::farthest_insertion<int, long long>(ord, dist, true, false, false);
    };
    auto initial_r2 = [](const BenchDataset&, std::vector<int>& ord, auto dist) {
        return tsp::regret2_insertion<int, long long>(ord, dist, true, false, false);
    };
    auto initial_to = [](const BenchDataset& ds, std::vector<int>& ord, auto dist) {
        ord = ds.two_opt_initial_order;
        return tsp::two_opt<int, long long>(ord, dist, true, false, false);
    };

    sa_summary.push_back(summarize_improvement(
        "nearest_neighbor -> sa",
        benchmark_sa_from_solver(datasets, base_param, sa_time_limit_ms, 0, initial_nn)));
    sa_summary.push_back(summarize_improvement(
        "farthest_insertion -> sa",
        benchmark_sa_from_solver(datasets, base_param, sa_time_limit_ms, 1, initial_fi)));
    sa_summary.push_back(summarize_improvement(
        "regret2_insertion -> sa",
        benchmark_sa_from_solver(datasets, base_param, sa_time_limit_ms, 2, initial_r2)));
    sa_summary.push_back(summarize_improvement(
        "two_opt -> sa",
        benchmark_sa_from_solver(datasets, base_param, sa_time_limit_ms, 3, initial_to)));

    two_opt_summary.push_back(summarize_improvement(
        "nearest_neighbor -> two_opt",
        benchmark_two_opt_from_solver(datasets, initial_nn)));
    two_opt_summary.push_back(summarize_improvement(
        "farthest_insertion -> two_opt",
        benchmark_two_opt_from_solver(datasets, initial_fi)));
    two_opt_summary.push_back(summarize_improvement(
        "regret2_insertion -> two_opt",
        benchmark_two_opt_from_solver(datasets, initial_r2)));
    two_opt_summary.push_back(summarize_improvement(
        "two_opt -> two_opt",
        benchmark_two_opt_from_solver(datasets, initial_to)));

    print_sa_improvement_table(n, cases, sa_time_limit_ms, sa_summary);
    print_two_opt_improvement_table(n, cases, two_opt_summary);
}
}  // namespace

int main() {
    std::cout << "[Test] move delta consistency...\n";
    run_move_delta_tests();
    std::cout << "[Test] deterministic cases...\n";
    run_deterministic_tests();
    std::cout << "[Test] randomized consistency...\n";
    run_randomized_consistency_tests();
    std::cout << "All tests passed.\n";
    run_benchmarks_main();
    return 0;
}

#endif

// 実行結果 (chatgpt)
// [Test] move delta consistency...
// [Test] deterministic cases...
// [Test] randomized consistency...
// All tests passed.

// [Benchmark]
// setting: cycle=true, fixed_start=false, fixed_end=false
// N=100 の同一データセット群に対して、各初期ソルバーの出力を SA(auto_mode=true) / two_opt で改善した結果を比較する。
// 表示する改善率は (before - after) / before * 100 [%]。

// [SA improvement benchmark]
// N = 100, cases = 20, SA time_limit_ms = 100
// initial solver                    avg before       avg after   imp mean[%]    imp min[%]    imp max[%]     imp sd[%]    avg iterations    avg time[ms]    max time[ms]
// ------------------------------------------------------------------------------------------------------------------------------------------------------
// nearest_neighbor -> sa           9925052.250     7933537.400        19.852        12.174        27.682         4.481       1786547.200         100.012         100.022
// farthest_insertion -> sa         8475610.100     7946716.850         6.204         3.242        12.932         2.649       1628518.400         100.022         100.139
// regret2_insertion -> sa          8208689.600     7934550.350         3.321         0.258         7.902         1.913       1689728.000         100.018         100.034
// two_opt -> sa                    8367263.200     7920173.300         5.300         0.672         8.345         2.098       1844377.600         100.015         100.029

// [two_opt improvement benchmark]
// N = 100, cases = 20
// initial solver                    avg before       avg after   imp mean[%]    imp min[%]    imp max[%]     imp sd[%]    avg time[ms]    min time[ms]    max time[ms]
// ----------------------------------------------------------------------------------------------------------------------------------------------------
// nearest_neighbor -> two_opt      9925052.250     8322530.950        15.958         7.697        23.898         4.370           0.222           0.105           0.322
// farthest_insertion -> two_opt    8475610.100     8346990.300         1.488         0.000         7.554         1.827           0.048           0.012           0.124
// regret2_insertion -> two_opt     8208689.600     8195479.800         0.162         0.000         1.184         0.297           0.019           0.012           0.035
// two_opt -> two_opt               8367263.200     8367263.200         0.000         0.000         0.000         0.000           0.012           0.011           0.020
