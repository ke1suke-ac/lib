#pragma once

#include <bits/stdc++.h>
#include "ga_lite.hpp" // v3: namespace ga

// ------------------------------------------------------------
// ga_ops (lite)
//  - ga.hpp への依存を排除し、ga_lite.hpp 前提で動く軽量版。
//  - v3 では namespace が ga に変更されたため、それに追従する。
//  - Selection 系は Population 側に移した想定なので、このヘッダには置かない。
//  - Crossover / Mutation は「構造体」ではなく「関数」として提供する。
//  - 多態性はテンプレート引数とオーバーロードで実現し、型判定用 trait は持たない。
//  - lo/hi は関数引数で受け取り、ヘッダ側で勝手なデフォルトを決めない。
//  - ga_lite.hpp(v3) の rand_* / clamp_range / normalize_range / weighted_index 等を
//    優先的に利用し、ライブラリ全体としての統一性を高める。
// ------------------------------------------------------------


// ============================================================
// Crossover（交叉）
// ============================================================

// ------------------------------
// 1点交叉（std::array）
// ------------------------------
template<class T, std::size_t N>
inline void one_point_crossover(
    const std::array<T, N>& p1,
    const std::array<T, N>& p2,
    std::array<T, N>& c
) {
    if constexpr (N == 0) return;

    const std::size_t k = ga::rand_int<std::size_t>(0, N);

    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(c.data(),     p1.data(),     k * sizeof(T));
        std::memcpy(c.data() + k, p2.data() + k, (N - k) * sizeof(T));
    } else {
        for (std::size_t i = 0; i < k; ++i) c[i] = p1[i];
        for (std::size_t i = k; i < N; ++i) c[i] = p2[i];
    }
}

template<class T, std::size_t N>
inline void one_point_crossover(
    const std::array<T, N>& p1,
    const std::array<T, N>& p2,
    std::array<T, N>& c1,
    std::array<T, N>& c2
) {
    if constexpr (N == 0) return;

    const std::size_t k = ga::rand_int<std::size_t>(0, N);

    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(c1.data(),     p1.data(),     k * sizeof(T));
        std::memcpy(c1.data() + k, p2.data() + k, (N - k) * sizeof(T));

        std::memcpy(c2.data(),     p2.data(),     k * sizeof(T));
        std::memcpy(c2.data() + k, p1.data() + k, (N - k) * sizeof(T));
    } else {
        for (std::size_t i = 0; i < k; ++i) { c1[i] = p1[i]; c2[i] = p2[i]; }
        for (std::size_t i = k; i < N; ++i) { c1[i] = p2[i]; c2[i] = p1[i]; }
    }
}

// ------------------------------
// 1点交叉（std::bitset）
// ------------------------------
template<std::size_t N>
inline void one_point_crossover(
    const std::bitset<N>& p1,
    const std::bitset<N>& p2,
    std::bitset<N>& c
) {
    if constexpr (N == 0) return;

    const std::size_t k = ga::rand_int<std::size_t>(0, N);

    // 下位 k ビットが 1 のマスク（index0 が LSB の慣習）
    auto low_mask = [&](std::size_t kk) {
        std::bitset<N> m;
        if (kk >= N) { m.set(); return m; }
        m.set();
        m >>= (N - kk);
        return m;
    };

    const auto lo = low_mask(k);
    const auto hi = ~lo;
    c = (p1 & lo) | (p2 & hi);
}

template<std::size_t N>
inline void one_point_crossover(
    const std::bitset<N>& p1,
    const std::bitset<N>& p2,
    std::bitset<N>& c1,
    std::bitset<N>& c2
) {
    if constexpr (N == 0) return;

    const std::size_t k = ga::rand_int<std::size_t>(0, N);

    auto low_mask = [&](std::size_t kk) {
        std::bitset<N> m;
        if (kk >= N) { m.set(); return m; }
        m.set();
        m >>= (N - kk);
        return m;
    };

    const auto lo = low_mask(k);
    const auto hi = ~lo;

    c1 = (p1 & lo) | (p2 & hi);
    c2 = (p2 & lo) | (p1 & hi);
}

// ------------------------------
// 2点交叉（std::array）
// ------------------------------
template<class T, std::size_t N>
inline void two_point_crossover(
    const std::array<T, N>& p1,
    const std::array<T, N>& p2,
    std::array<T, N>& c
) {
    if constexpr (N == 0) return;

    const std::size_t a = ga::rand_int<std::size_t>(0, N);
    const std::size_t b = ga::rand_int<std::size_t>(a, N);

    // [a,b) を p1、それ以外を p2
    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(c.data(),         p2.data(),         a * sizeof(T));
        std::memcpy(c.data() + a,     p1.data() + a,     (b - a) * sizeof(T));
        std::memcpy(c.data() + b,     p2.data() + b,     (N - b) * sizeof(T));
    } else {
        for (std::size_t i = 0; i < a; ++i) c[i] = p2[i];
        for (std::size_t i = a; i < b; ++i) c[i] = p1[i];
        for (std::size_t i = b; i < N; ++i) c[i] = p2[i];
    }
}

template<class T, std::size_t N>
inline void two_point_crossover(
    const std::array<T, N>& p1,
    const std::array<T, N>& p2,
    std::array<T, N>& c1,
    std::array<T, N>& c2
) {
    if constexpr (N == 0) return;

    const std::size_t a = ga::rand_int<std::size_t>(0, N);
    const std::size_t b = ga::rand_int<std::size_t>(a, N);

    if constexpr (std::is_trivially_copyable_v<T>) {
        // c1: [a,b) を p1
        std::memcpy(c1.data(),         p2.data(),         a * sizeof(T));
        std::memcpy(c1.data() + a,     p1.data() + a,     (b - a) * sizeof(T));
        std::memcpy(c1.data() + b,     p2.data() + b,     (N - b) * sizeof(T));

        // c2: [a,b) を p2
        std::memcpy(c2.data(),         p1.data(),         a * sizeof(T));
        std::memcpy(c2.data() + a,     p2.data() + a,     (b - a) * sizeof(T));
        std::memcpy(c2.data() + b,     p1.data() + b,     (N - b) * sizeof(T));
    } else {
        for (std::size_t i = 0; i < a; ++i) { c1[i] = p2[i]; c2[i] = p1[i]; }
        for (std::size_t i = a; i < b; ++i) { c1[i] = p1[i]; c2[i] = p2[i]; }
        for (std::size_t i = b; i < N; ++i) { c1[i] = p2[i]; c2[i] = p1[i]; }
    }
}

// ------------------------------
// 2点交叉（std::bitset）
// ------------------------------
template<std::size_t N>
inline void two_point_crossover(
    const std::bitset<N>& p1,
    const std::bitset<N>& p2,
    std::bitset<N>& c
) {
    if constexpr (N == 0) return;

    const std::size_t a = ga::rand_int<std::size_t>(0, N);
    const std::size_t b = ga::rand_int<std::size_t>(a, N);

    auto low_mask = [&](std::size_t kk) {
        std::bitset<N> m;
        if (kk >= N) { m.set(); return m; }
        m.set();
        m >>= (N - kk);
        return m;
    };

    // [a,b) が 1 のマスク
    const auto mask = low_mask(b) ^ low_mask(a);
    c = (p1 & mask) | (p2 & ~mask);
}

template<std::size_t N>
inline void two_point_crossover(
    const std::bitset<N>& p1,
    const std::bitset<N>& p2,
    std::bitset<N>& c1,
    std::bitset<N>& c2
) {
    if constexpr (N == 0) return;

    const std::size_t a = ga::rand_int<std::size_t>(0, N);
    const std::size_t b = ga::rand_int<std::size_t>(a, N);

    auto low_mask = [&](std::size_t kk) {
        std::bitset<N> m;
        if (kk >= N) { m.set(); return m; }
        m.set();
        m >>= (N - kk);
        return m;
    };

    const auto mask = low_mask(b) ^ low_mask(a);
    c1 = (p1 & mask) | (p2 & ~mask);
    c2 = (p2 & mask) | (p1 & ~mask);
}

// ------------------------------
// 一様交叉（scalar）
// ------------------------------
template<class T>
inline void uniform_crossover(
    const T& p1,
    const T& p2,
    T& c,
    double p_pick
) {
    c = ga::rand_bool(p_pick) ? p1 : p2;
}

template<class T>
inline void uniform_crossover(
    const T& p1,
    const T& p2,
    T& c1,
    T& c2,
    double p_pick
) {
    const bool b = ga::rand_bool(p_pick);
    c1 = b ? p1 : p2;
    c2 = b ? p2 : p1;
}

// ------------------------------
// 一様交叉（std::array）
// ------------------------------
template<class T, std::size_t N>
inline void uniform_crossover(
    const std::array<T, N>& p1,
    const std::array<T, N>& p2,
    std::array<T, N>& c,
    double p_pick
) {
    for (std::size_t i = 0; i < N; ++i) c[i] = ga::rand_bool(p_pick) ? p1[i] : p2[i];
}

template<class T, std::size_t N>
inline void uniform_crossover(
    const std::array<T, N>& p1,
    const std::array<T, N>& p2,
    std::array<T, N>& c1,
    std::array<T, N>& c2,
    double p_pick
) {
    for (std::size_t i = 0; i < N; ++i) {
        const bool b = ga::rand_bool(p_pick);
        c1[i] = b ? p1[i] : p2[i];
        c2[i] = b ? p2[i] : p1[i];
    }
}

// ------------------------------
// 一様交叉（std::bitset）
// ------------------------------
template<std::size_t N>
inline void uniform_crossover(
    const std::bitset<N>& p1,
    const std::bitset<N>& p2,
    std::bitset<N>& c,
    double p_pick
) {
    for (std::size_t i = 0; i < N; ++i) c.set(i, ga::rand_bool(p_pick) ? p1.test(i) : p2.test(i));
}

template<std::size_t N>
inline void uniform_crossover(
    const std::bitset<N>& p1,
    const std::bitset<N>& p2,
    std::bitset<N>& c1,
    std::bitset<N>& c2,
    double p_pick
) {
    for (std::size_t i = 0; i < N; ++i) {
        const bool b = ga::rand_bool(p_pick);
        c1.set(i, b ? p1.test(i) : p2.test(i));
        c2.set(i, b ? p2.test(i) : p1.test(i));
    }
}

// ------------------------------
// 算術交叉（scalar）
//  - alpha < 0 の場合は alpha を [0,1] の乱数で決める
//  - 2-child 版は alpha を 1 回だけ決め、両方の子に共有する（再現性・対称性）
//  - 整数型は「clamp→丸め→cast」の順で安全側にする
// ------------------------------
template<class T>
inline void arithmetic_crossover(
    const T& p1,
    const T& p2,
    T& c,
    double alpha,
    T lo,
    T hi
) requires (std::is_integral_v<T> || std::is_floating_point_v<T>) {
    ga::normalize_range(lo, hi);

    double a = alpha;
    if (!(a >= 0.0)) {
        // alpha < 0 または NaN を乱数扱い
        a = ga::rand01();
    } else {
        if (a > 1.0) a = 1.0;
    }

    if constexpr (std::is_floating_point_v<T>) {
        const T x = static_cast<T>((double)p1 * a + (double)p2 * (1.0 - a));
        c = ga::clamp_range(x, lo, hi);
    } else {
        double x = (double)p1 * a + (double)p2 * (1.0 - a);
        x = ga::clamp_range(x, (double)lo, (double)hi);
        c = static_cast<T>(std::llround(x));
    }
}

template<class T>
inline void arithmetic_crossover(
    const T& p1,
    const T& p2,
    T& c1,
    T& c2,
    double alpha,
    T lo,
    T hi
) requires (std::is_integral_v<T> || std::is_floating_point_v<T>) {
    ga::normalize_range(lo, hi);

    double a = alpha;
    if (!(a >= 0.0)) {
        a = ga::rand01();
    } else {
        if (a > 1.0) a = 1.0;
    }
    const double b = 1.0 - a;

    if constexpr (std::is_floating_point_v<T>) {
        const T x1 = static_cast<T>((double)p1 * a + (double)p2 * (1.0 - a));
        const T x2 = static_cast<T>((double)p1 * b + (double)p2 * (1.0 - b));
        c1 = ga::clamp_range(x1, lo, hi);
        c2 = ga::clamp_range(x2, lo, hi);
    } else {
        double x1 = (double)p1 * a + (double)p2 * (1.0 - a);
        double x2 = (double)p1 * b + (double)p2 * (1.0 - b);
        x1 = ga::clamp_range(x1, (double)lo, (double)hi);
        x2 = ga::clamp_range(x2, (double)lo, (double)hi);
        c1 = static_cast<T>(std::llround(x1));
        c2 = static_cast<T>(std::llround(x2));
    }
}

// ------------------------------
// 算術交叉（std::array）
// ------------------------------
template<class T, std::size_t N>
inline void arithmetic_crossover(
    const std::array<T, N>& p1,
    const std::array<T, N>& p2,
    std::array<T, N>& c,
    double alpha,
    T lo,
    T hi
) requires (std::is_integral_v<T> || std::is_floating_point_v<T>) {
    ga::normalize_range(lo, hi);

    double a = alpha;
    if (!(a >= 0.0)) {
        a = ga::rand01();
    } else {
        if (a > 1.0) a = 1.0;
    }

    if constexpr (std::is_floating_point_v<T>) {
        for (std::size_t i = 0; i < N; ++i) {
            const T x = static_cast<T>((double)p1[i] * a + (double)p2[i] * (1.0 - a));
            c[i] = ga::clamp_range(x, lo, hi);
        }
    } else {
        for (std::size_t i = 0; i < N; ++i) {
            double x = (double)p1[i] * a + (double)p2[i] * (1.0 - a);
            x = ga::clamp_range(x, (double)lo, (double)hi);
            c[i] = static_cast<T>(std::llround(x));
        }
    }
}

template<class T, std::size_t N>
inline void arithmetic_crossover(
    const std::array<T, N>& p1,
    const std::array<T, N>& p2,
    std::array<T, N>& c1,
    std::array<T, N>& c2,
    double alpha,
    T lo,
    T hi
) requires (std::is_integral_v<T> || std::is_floating_point_v<T>) {
    ga::normalize_range(lo, hi);

    double a = alpha;
    if (!(a >= 0.0)) {
        a = ga::rand01();
    } else {
        if (a > 1.0) a = 1.0;
    }
    const double b = 1.0 - a;

    if constexpr (std::is_floating_point_v<T>) {
        for (std::size_t i = 0; i < N; ++i) {
            const T x1 = static_cast<T>((double)p1[i] * a + (double)p2[i] * (1.0 - a));
            const T x2 = static_cast<T>((double)p1[i] * b + (double)p2[i] * (1.0 - b));
            c1[i] = ga::clamp_range(x1, lo, hi);
            c2[i] = ga::clamp_range(x2, lo, hi);
        }
    } else {
        for (std::size_t i = 0; i < N; ++i) {
            double x1 = (double)p1[i] * a + (double)p2[i] * (1.0 - a);
            double x2 = (double)p1[i] * b + (double)p2[i] * (1.0 - b);
            x1 = ga::clamp_range(x1, (double)lo, (double)hi);
            x2 = ga::clamp_range(x2, (double)lo, (double)hi);
            c1[i] = static_cast<T>(std::llround(x1));
            c2[i] = static_cast<T>(std::llround(x2));
        }
    }
}

// ------------------------------
// BLX-α 交叉（Blend crossover / scalar）
//  - p1,p2 の min/max を [mn,mx] として区間を α だけ広げ、そこから一様サンプル。
//  - 結果は [lo,hi] にクランプする。
//  - 整数型は「clamp→丸め→cast」の順で安全側にする
// ------------------------------
template<class T>
inline void blend_crossover(
    const T& p1,
    const T& p2,
    T& c,
    double alpha,
    T lo,
    T hi
) requires (std::is_integral_v<T> || std::is_floating_point_v<T>) {
    ga::normalize_range(lo, hi);

    const double a = (double)p1;
    const double b = (double)p2;
    const double mn = std::min(a, b);
    const double mx = std::max(a, b);
    const double d  = mx - mn;

    if (!(d > 0.0)) {
        if constexpr (std::is_floating_point_v<T>) {
            c = ga::clamp_range(static_cast<T>(a), lo, hi);
        } else {
            c = ga::clamp_range(p1, lo, hi);
        }
        return;
    }

    double lo2 = mn - alpha * d;
    double hi2 = mx + alpha * d;
    ga::normalize_range(lo2, hi2);

    double x = ga::rand_real(lo2, hi2);
    x = ga::clamp_range(x, (double)lo, (double)hi);

    if constexpr (std::is_floating_point_v<T>) {
        c = static_cast<T>(x);
    } else {
        c = static_cast<T>(std::llround(x));
    }
}

template<class T>
inline void blend_crossover(
    const T& p1,
    const T& p2,
    T& c1,
    T& c2,
    double alpha,
    T lo,
    T hi
) requires (std::is_integral_v<T> || std::is_floating_point_v<T>) {
    // 2-child 版も独立に実装（片方だけコピペしやすい）
    blend_crossover(p1, p2, c1, alpha, lo, hi);
    blend_crossover(p2, p1, c2, alpha, lo, hi);
}

// ------------------------------
// BLX-α 交叉（std::array）
// ------------------------------
template<class T, std::size_t N>
inline void blend_crossover(
    const std::array<T, N>& p1,
    const std::array<T, N>& p2,
    std::array<T, N>& c,
    double alpha,
    T lo,
    T hi
) requires (std::is_integral_v<T> || std::is_floating_point_v<T>) {
    ga::normalize_range(lo, hi);

    for (std::size_t i = 0; i < N; ++i) {
        const double a = (double)p1[i];
        const double b = (double)p2[i];
        const double mn = std::min(a, b);
        const double mx = std::max(a, b);
        const double d  = mx - mn;

        if (!(d > 0.0)) {
            if constexpr (std::is_floating_point_v<T>) {
                c[i] = ga::clamp_range(static_cast<T>(a), lo, hi);
            } else {
                c[i] = ga::clamp_range(p1[i], lo, hi);
            }
            continue;
        }

        double lo2 = mn - alpha * d;
        double hi2 = mx + alpha * d;
        ga::normalize_range(lo2, hi2);

        double x = ga::rand_real(lo2, hi2);
        x = ga::clamp_range(x, (double)lo, (double)hi);

        if constexpr (std::is_floating_point_v<T>) {
            c[i] = static_cast<T>(x);
        } else {
            c[i] = static_cast<T>(std::llround(x));
        }
    }
}

template<class T, std::size_t N>
inline void blend_crossover(
    const std::array<T, N>& p1,
    const std::array<T, N>& p2,
    std::array<T, N>& c1,
    std::array<T, N>& c2,
    double alpha,
    T lo,
    T hi
) requires (std::is_integral_v<T> || std::is_floating_point_v<T>) {
    blend_crossover(p1, p2, c1, alpha, lo, hi);
    blend_crossover(p2, p1, c2, alpha, lo, hi);
}

// ------------------------------
// PMX 交叉（順列用 / std::array）
//  - 値が 0..N-1 の順列であることを前提にした実装。
// ------------------------------
template<class T, std::size_t N>
inline void pmx_crossover(
    const std::array<T, N>& p1,
    const std::array<T, N>& p2,
    std::array<T, N>& c
) requires (std::is_integral_v<T> || std::is_enum_v<T>) {
    if constexpr (N == 0) return;

    auto idx = [&](T x) -> std::size_t { return static_cast<std::size_t>(x); };

    const std::size_t l = ga::rand_int<std::size_t>(0, N - 1);
    const std::size_t r = ga::rand_int<std::size_t>(l + 1, N); // [l,r)

    std::array<unsigned char, N> used{};
    used.fill(0);
    std::array<T, N> map{}; // map[value_from_p1] = value_from_p2（切り出し区間のみ有効）

    // 切り出し区間は p1 をコピー
    for (std::size_t i = l; i < r; ++i) {
        c[i] = p1[i];
        const std::size_t a = idx(p1[i]);
        if (a < N) {
            used[a] = 1;
            map[a] = p2[i];
        }
    }

    // それ以外は p2 を埋めつつ衝突を解決
    for (std::size_t i = 0; i < N; ++i) {
        if (l <= i && i < r) continue;

        T v = p2[i];
        std::size_t iv = idx(v);

        while (iv < N && used[iv]) {
            v = map[iv];
            iv = idx(v);
        }

        c[i] = v;
        if (iv < N) used[iv] = 1;
    }
}

template<class T, std::size_t N>
inline void pmx_crossover(
    const std::array<T, N>& p1,
    const std::array<T, N>& p2,
    std::array<T, N>& c1,
    std::array<T, N>& c2
) requires (std::is_integral_v<T> || std::is_enum_v<T>) {
    pmx_crossover(p1, p2, c1);
    pmx_crossover(p2, p1, c2);
}

// ------------------------------
// OX 交叉（順列用 / std::array）
//  - 値が 0..N-1 の順列であることを前提にした実装。
// ------------------------------
template<class T, std::size_t N>
inline void ox_crossover(
    const std::array<T, N>& p1,
    const std::array<T, N>& p2,
    std::array<T, N>& c
) requires (std::is_integral_v<T> || std::is_enum_v<T>) {
    if constexpr (N == 0) return;

    auto idx = [&](T x) -> std::size_t { return static_cast<std::size_t>(x); };

    const std::size_t l = ga::rand_int<std::size_t>(0, N - 1);
    const std::size_t r = ga::rand_int<std::size_t>(l + 1, N); // [l,r)

    c = p1;

    std::array<unsigned char, N> used{};
    used.fill(0);
    for (std::size_t i = l; i < r; ++i) {
        const std::size_t a = idx(p1[i]);
        if (a < N) used[a] = 1;
    }

    auto in_seg = [&](std::size_t i) { return l <= i && i < r; };

    std::size_t pos = r % N;
    for (std::size_t t = 0; t < N; ++t) {
        const T v = p2[(r + t) % N];
        const std::size_t iv = idx(v);
        if (iv < N && used[iv]) continue;

        while (in_seg(pos)) pos = (pos + 1) % N;
        c[pos] = v;
        if (iv < N) used[iv] = 1;
        pos = (pos + 1) % N;
    }
}

template<class T, std::size_t N>
inline void ox_crossover(
    const std::array<T, N>& p1,
    const std::array<T, N>& p2,
    std::array<T, N>& c1,
    std::array<T, N>& c2
) requires (std::is_integral_v<T> || std::is_enum_v<T>) {
    ox_crossover(p1, p2, c1);
    ox_crossover(p2, p1, c2);
}


// ============================================================
// Mutation（突然変異）
// ============================================================

// ------------------------------
// ランダムリセット（scalar / 整数）
// ------------------------------
template<class T>
inline void random_reset_mutation(
    T& v,
    double p_reset,
    T lo,
    T hi
) requires (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
    ga::normalize_range(lo, hi);
    if (!ga::rand_bool(p_reset)) return;
    v = ga::rand_int<T>(lo, hi);
}

// ------------------------------
// ランダムリセット（scalar / 浮動小数）
// ------------------------------
template<class T>
inline void random_reset_mutation(
    T& v,
    double p_reset,
    T lo,
    T hi
) requires (std::is_floating_point_v<T>) {
    ga::normalize_range(lo, hi);
    if (!ga::rand_bool(p_reset)) return;
    v = ga::rand_real<T>(lo, hi);
}

// ------------------------------
// ランダムリセット（scalar / bool）
//  - lo==hi の場合はその値に固定
//  - lo!=hi の場合は {0,1} を一様に出す
// ------------------------------
inline void random_reset_mutation(
    bool& v,
    double p_reset,
    bool lo,
    bool hi
) {
    ga::normalize_range(lo, hi);
    if (!ga::rand_bool(p_reset)) return;
    v = (lo == hi) ? lo : ga::rand_bool(0.5);
}

// ------------------------------
// ランダムリセット（std::array / 整数）
// ------------------------------
template<class T, std::size_t N>
inline void random_reset_mutation(
    std::array<T, N>& v,
    double p_reset,
    T lo,
    T hi
) requires (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
    ga::normalize_range(lo, hi);
    for (std::size_t i = 0; i < N; ++i) {
        if (!ga::rand_bool(p_reset)) continue;
        v[i] = ga::rand_int<T>(lo, hi);
    }
}

// ------------------------------
// ランダムリセット（std::array / 浮動小数）
// ------------------------------
template<class T, std::size_t N>
inline void random_reset_mutation(
    std::array<T, N>& v,
    double p_reset,
    T lo,
    T hi
) requires (std::is_floating_point_v<T>) {
    ga::normalize_range(lo, hi);
    for (std::size_t i = 0; i < N; ++i) {
        if (!ga::rand_bool(p_reset)) continue;
        v[i] = ga::rand_real<T>(lo, hi);
    }
}

// ------------------------------
// ランダムリセット（std::array / bool）
// ------------------------------
template<std::size_t N>
inline void random_reset_mutation(
    std::array<bool, N>& v,
    double p_reset,
    bool lo,
    bool hi
) {
    ga::normalize_range(lo, hi);
    for (std::size_t i = 0; i < N; ++i) {
        if (!ga::rand_bool(p_reset)) continue;
        v[i] = (lo == hi) ? lo : ga::rand_bool(0.5);
    }
}

// ------------------------------
// ランダムリセット（std::bitset）
// ------------------------------
template<std::size_t N>
inline void random_reset_mutation(
    std::bitset<N>& v,
    double p_reset,
    bool lo,
    bool hi
) {
    ga::normalize_range(lo, hi);
    for (std::size_t i = 0; i < N; ++i) {
        if (!ga::rand_bool(p_reset)) continue;
        v.set(i, (lo == hi) ? lo : ga::rand_bool(0.5));
    }
}

// ------------------------------
// ビット反転（scalar bool）
// ------------------------------
inline void bit_flip_mutation(
    bool& v,
    double p_flip
) {
    if (ga::rand_bool(p_flip)) v = !v;
}

// ------------------------------
// ビット反転（std::array<bool,N>）
// ------------------------------
template<std::size_t N>
inline void bit_flip_mutation(
    std::array<bool, N>& v,
    double p_flip
) {
    for (std::size_t i = 0; i < N; ++i) if (ga::rand_bool(p_flip)) v[i] = !v[i];
}

// ------------------------------
// ビット反転（std::bitset）
// ------------------------------
template<std::size_t N>
inline void bit_flip_mutation(
    std::bitset<N>& v,
    double p_flip
) {
    for (std::size_t i = 0; i < N; ++i) if (ga::rand_bool(p_flip)) v.flip(i);
}

// ------------------------------
// 2点スワップ（std::array）
// ------------------------------
template<class T, std::size_t N>
inline void swap_mutation(
    std::array<T, N>& v,
    double p_swap
) {
    if constexpr (N < 2) return;
    if (!ga::rand_bool(p_swap)) return;

    std::size_t i = ga::rand_int<std::size_t>(0, N - 1);
    std::size_t j = ga::rand_int<std::size_t>(0, N - 1);
    if (i == j) j = (j + 1) % N;
    std::swap(v[i], v[j]);
}

// ------------------------------
// 反転（区間 reverse / std::array）
// ------------------------------
template<class T, std::size_t N>
inline void inversion_mutation(
    std::array<T, N>& v,
    double p_inv
) {
    if constexpr (N < 2) return;
    if (!ga::rand_bool(p_inv)) return;

    const std::size_t l = ga::rand_int<std::size_t>(0, N - 1);
    const std::size_t r = ga::rand_int<std::size_t>(l + 1, N);
    std::reverse(v.begin() + (std::ptrdiff_t)l, v.begin() + (std::ptrdiff_t)r);
}

// ------------------------------
// シャッフル（区間 shuffle / std::array）
// ------------------------------
template<class T, std::size_t N>
inline void scramble_mutation(
    std::array<T, N>& v,
    double p_scramble
) {
    if constexpr (N < 2) return;
    if (!ga::rand_bool(p_scramble)) return;

    const std::size_t l = ga::rand_int<std::size_t>(0, N - 1);
    const std::size_t r = ga::rand_int<std::size_t>(l + 1, N);
    std::shuffle(v.begin() + (std::ptrdiff_t)l, v.begin() + (std::ptrdiff_t)r, ga::rng);
}

// ------------------------------
// 挿入（1要素を抜いて別位置へ / std::array）
// ------------------------------
template<class T, std::size_t N>
inline void insertion_mutation(
    std::array<T, N>& v,
    double p_insert
) {
    if constexpr (N < 2) return;
    if (!ga::rand_bool(p_insert)) return;

    std::size_t i = ga::rand_int<std::size_t>(0, N - 1);
    std::size_t j = ga::rand_int<std::size_t>(0, N - 1);
    if (i == j) j = (j + 1) % N;

    const T tmp = v[i];
    if (i < j) {
        for (std::size_t k = i; k < j; ++k) v[k] = v[k + 1];
        v[j] = tmp;
    } else {
        for (std::size_t k = i; k > j; --k) v[k] = v[k - 1];
        v[j] = tmp;
    }
}

// ------------------------------
// ガウス変異（scalar / 浮動小数）
// ------------------------------
template<class T>
inline void gaussian_mutation(
    T& v,
    double p_mut,
    double sigma,
    T lo,
    T hi
) requires (std::is_floating_point_v<T>) {
    if (!(sigma > 0.0)) return;
    ga::normalize_range(lo, hi);
    if (!ga::rand_bool(p_mut)) return;

    std::normal_distribution<double> nd(0.0, sigma);
    const T x = static_cast<T>(v + (T)nd(ga::rng));
    v = ga::clamp_range(x, lo, hi);
}

// ------------------------------
// ガウス変異（std::array / 浮動小数）
// ------------------------------
template<class T, std::size_t N>
inline void gaussian_mutation(
    std::array<T, N>& v,
    double p_mut,
    double sigma,
    T lo,
    T hi
) requires (std::is_floating_point_v<T>) {
    if (!(sigma > 0.0)) return;
    ga::normalize_range(lo, hi);

    std::normal_distribution<double> nd(0.0, sigma);
    for (std::size_t i = 0; i < N; ++i) {
        if (!ga::rand_bool(p_mut)) continue;
        const T x = static_cast<T>(v[i] + (T)nd(ga::rng));
        v[i] = ga::clamp_range(x, lo, hi);
    }
}

// ------------------------------
// クリープ変異（scalar / 整数）
//  - step_max>=1 のとき、[-step_max, step_max] から 0 以外を選んで加算する。
// ------------------------------
template<class T>
inline void creep_mutation(
    T& v,
    double p_mut,
    int step_max,
    T lo,
    T hi
) requires (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
    if (step_max <= 0) return;
    ga::normalize_range(lo, hi);
    if (!ga::rand_bool(p_mut)) return;

    int d = 0;
    while (d == 0) d = ga::rand_int(-step_max, step_max);

    // 競プロ用途では T は通常 int 近傍なので long long で十分。
    long long y = (long long)v + (long long)d;
    y = ga::clamp_range<long long>(y, (long long)lo, (long long)hi);
    v = static_cast<T>(y);
}

// ------------------------------
// クリープ変異（std::array / 整数）
// ------------------------------
template<class T, std::size_t N>
inline void creep_mutation(
    std::array<T, N>& v,
    double p_mut,
    int step_max,
    T lo,
    T hi
) requires (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
    if (step_max <= 0) return;
    ga::normalize_range(lo, hi);

    for (std::size_t i = 0; i < N; ++i) {
        if (!ga::rand_bool(p_mut)) continue;

        int d = 0;
        while (d == 0) d = ga::rand_int(-step_max, step_max);

        long long y = (long long)v[i] + (long long)d;
        y = ga::clamp_range<long long>(y, (long long)lo, (long long)hi);
        v[i] = static_cast<T>(y);
    }
}


// ============================================================
// 高次オペレータ（choice / pipeline）
//  - 「構造体」ではなく「関数」
//  - 交叉/変異の関数やラムダ（パラメータを束縛したもの）を渡して組み合わせる。
// ============================================================

// ------------------------------
// 交叉：重み付きで 1 つ選ぶ（子 1 個）
//  - ops は (p1,p2,c) を受け取れる callable
// ------------------------------
template<class V, class... Ops>
inline void choice_crossover(
    const V& p1,
    const V& p2,
    V& c,
    const std::array<double, sizeof...(Ops)>& weights,
    Ops&&... ops
) {
    constexpr std::size_t M = sizeof...(Ops);
    static_assert(M > 0);

    const int pick = ga::weighted_index(weights);
    const std::size_t idx = (pick < 0) ? 0u : (std::size_t)pick;

    auto tup = std::forward_as_tuple(std::forward<Ops>(ops)...);
    auto dispatch = [&]<std::size_t... I>(std::index_sequence<I...>) {
        ((idx == I ? (void)std::invoke(std::get<I>(tup), p1, p2, c) : (void)0), ...);
    };
    dispatch(std::make_index_sequence<M>{});
}

// ------------------------------
// 交叉：重み付きで 1 つ選ぶ（子 2 個）
//  - ops は (p1,p2,c1,c2) を受け取れる callable
// ------------------------------
template<class V, class... Ops>
inline void choice_crossover(
    const V& p1,
    const V& p2,
    V& c1,
    V& c2,
    const std::array<double, sizeof...(Ops)>& weights,
    Ops&&... ops
) {
    constexpr std::size_t M = sizeof...(Ops);
    static_assert(M > 0);

    const int pick = ga::weighted_index(weights);
    const std::size_t idx = (pick < 0) ? 0u : (std::size_t)pick;

    auto tup = std::forward_as_tuple(std::forward<Ops>(ops)...);
    auto dispatch = [&]<std::size_t... I>(std::index_sequence<I...>) {
        ((idx == I ? (void)std::invoke(std::get<I>(tup), p1, p2, c1, c2) : (void)0), ...);
    };
    dispatch(std::make_index_sequence<M>{});
}

// ------------------------------
// 変異：複数を順に適用（パイプライン）
//  - ops は (v) を受け取れる callable
// ------------------------------
template<class V, class... Ops>
inline void pipeline_mutation(V& v, Ops&&... ops) {
    (std::invoke(std::forward<Ops>(ops), v), ...);
}

// ------------------------------
// 変異：重み付きで 1 つ選ぶ
//  - ops は (v) を受け取れる callable
// ------------------------------
template<class V, class... Ops>
inline void choice_mutation(
    V& v,
    const std::array<double, sizeof...(Ops)>& weights,
    Ops&&... ops
) {
    constexpr std::size_t M = sizeof...(Ops);
    static_assert(M > 0);

    const int pick = ga::weighted_index(weights);
    const std::size_t idx = (pick < 0) ? 0u : (std::size_t)pick;

    auto tup = std::forward_as_tuple(std::forward<Ops>(ops)...);
    auto dispatch = [&]<std::size_t... I>(std::index_sequence<I...>) {
        ((idx == I ? (void)std::invoke(std::get<I>(tup), v) : (void)0), ...);
    };
    dispatch(std::make_index_sequence<M>{});
}


// ============================================================
// Tests (ga_ops)
//  - 無効化しやすいよう #if 1 ～ #endif
//  - テストでのみ使用する include はここに置く
// ============================================================

#if 1
#include <iostream>
#include <iomanip>
#include <string_view>

namespace ga_ops_test {

struct TestRunner {
    int checks = 0;
    int fails = 0;

    void check(bool ok, std::string_view expr, std::string_view file, int line, std::string_view msg = {}) {
        ++checks;
        if (!ok) {
            ++fails;
            std::cerr << "[FAIL] " << file << ":" << line << "  " << expr;
            if (!msg.empty()) std::cerr << "  (" << msg << ")";
            std::cerr << "\n";
        }
    }

    template<class A, class B>
    void eq(const A& a, const B& b, std::string_view expr, std::string_view file, int line) {
        ++checks;
        if (!(a == b)) {
            ++fails;
            std::cerr << "[FAIL] " << file << ":" << line << "  " << expr
                      << "  lhs=" << a << " rhs=" << b << "\n";
        }
    }
};

#define GAOPS_CHECK(tr, expr) (tr).check((expr), #expr, __FILE__, __LINE__)
#define GAOPS_CHECK_MSG(tr, expr, msg) (tr).check((expr), #expr, __FILE__, __LINE__, (msg))
#define GAOPS_EQ(tr, a, b) (tr).eq((a), (b), #a " == " #b, __FILE__, __LINE__)

struct RngGuard {
    std::mt19937 saved;
    explicit RngGuard(uint32_t seed) : saved(ga::rng) { ga::rng.seed(seed); }
    ~RngGuard() { ga::rng = saved; }
};

template<class T, std::size_t N>
static bool is_perm_0n(const std::array<T, N>& a) {
    std::array<unsigned char, N> seen{};
    seen.fill(0);
    for (std::size_t i = 0; i < N; ++i) {
        const std::size_t v = (std::size_t)a[i];
        if (v >= N) return false;
        if (seen[v]) return false;
        seen[v] = 1;
    }
    return true;
}

template<class T, std::size_t N>
static std::array<T, N> init_perm_default() {
    std::array<T, N> a{};
    ga::fill_perm(a);
    return a;
}

template<class T, std::size_t N>
static std::array<T, N> init_perm_random() {
    std::array<T, N> a{};
    ga::fill_random_perm(a);
    return a;
}

// -------------------- Tests --------------------

static void test_perm_init(TestRunner& tr) {
    using V = std::array<int, 10>;

    {
        V a = init_perm_default<int, 10>();
        GAOPS_CHECK(tr, is_perm_0n(a));
        for (int i = 0; i < 10; ++i) GAOPS_CHECK(tr, a[(std::size_t)i] == i);
    }
    {
        RngGuard rg(123);
        V a = init_perm_random<int, 10>();
        GAOPS_CHECK(tr, is_perm_0n(a));
    }
}

static void test_two_point_crossover_array(TestRunner& tr) {
    using V = std::array<int, 10>;

    V p1{}, p2{}, c{};
    for (std::size_t i = 0; i < V{}.size(); ++i) {
        p1[i] = (int)i;
        p2[i] = 100 + (int)i;
    }

    {
        RngGuard rg(7);
        std::mt19937 local = ga::rng;

        // expected: 内部の乱数生成をエミュレート（ga::rand_int<size_t> は uniform_int_distribution を使う）
        std::uniform_int_distribution<std::size_t> a_dist(0, 10);
        const std::size_t a = a_dist(local);
        std::uniform_int_distribution<std::size_t> b_dist(a, 10);
        const std::size_t b = b_dist(local);

        V exp{};
        std::memcpy(exp.data(),         p2.data(),         a * sizeof(int));
        std::memcpy(exp.data() + a,     p1.data() + a,     (b - a) * sizeof(int));
        std::memcpy(exp.data() + b,     p2.data() + b,     (10 - b) * sizeof(int));

        two_point_crossover(p1, p2, c);
        GAOPS_CHECK(tr, c == exp);
    }
}

static void test_uniform_crossover_bitset(TestRunner& tr) {
    using V = std::bitset<16>;
    V p1{}, p2{}, c1{}, c2{};

    p1.reset();
    p2.set();
    {
        RngGuard rg(1);
        uniform_crossover(p1, p2, c1, c2, 0.5);

        // p1=0, p2=1 なので、c2 は c1 のビット反転になるはず
        for (std::size_t i = 0; i < 16; ++i) {
            GAOPS_CHECK(tr, c2.test(i) == !c1.test(i));
        }
    }
}

static void test_blend_crossover_clamp(TestRunner& tr) {
    using V = std::array<float, 8>;

    V p1{}, p2{}, c{};
    for (std::size_t i = 0; i < 8; ++i) {
        p1[i] = -10.0f;
        p2[i] =  10.0f;
    }

    {
        RngGuard rg(42);
        blend_crossover(p1, p2, c, 0.5, -10.0f, 10.0f);

        for (std::size_t i = 0; i < 8; ++i) {
            GAOPS_CHECK(tr, c[i] >= -10.0f - 1e-6f && c[i] <= 10.0f + 1e-6f);
        }
    }
}

// arithmetic_crossover（2-child, alpha<0）のバグ検出用
static void test_arithmetic_crossover_two_child_alpha_random(TestRunner& tr) {
    using V = std::array<int, 8>;

    V p1{}, p2{}, c1{}, c2{};
    for (std::size_t i = 0; i < 8; ++i) {
        p1[i] = 0;
        p2[i] = 100;
    }

    // 期待値を「同じ乱数alphaを1回だけ引く」実装で作る
    {
        RngGuard rg(202);
        std::mt19937 local = ga::rng;

        const double a = std::generate_canonical<double, 53>(local);
        const double b = 1.0 - a;

        V e1{}, e2{};
        for (std::size_t i = 0; i < 8; ++i) {
            double x1 = (double)p1[i] * a + (double)p2[i] * (1.0 - a);
            double x2 = (double)p1[i] * b + (double)p2[i] * (1.0 - b);
            x1 = ga::clamp_range(x1, 0.0, 100.0);
            x2 = ga::clamp_range(x2, 0.0, 100.0);
            e1[i] = (int)std::llround(x1);
            e2[i] = (int)std::llround(x2);
        }

        arithmetic_crossover(p1, p2, c1, c2, -1.0, 0, 100);
        GAOPS_CHECK(tr, c1 == e1);
        GAOPS_CHECK(tr, c2 == e2);
    }
}

static void test_pmx_ox_valid_perm(TestRunner& tr) {
    using V = std::array<int, 10>;
    V p1{}, p2{}, c1{}, c2{};

    // 2つの既知な順列
    ga::fill_perm(p1);
    for (int i = 0; i < 10; ++i) p2[(std::size_t)i] = 9 - i;

    {
        RngGuard rg(3);
        pmx_crossover(p1, p2, c1, c2);
        GAOPS_CHECK(tr, is_perm_0n(c1));
        GAOPS_CHECK(tr, is_perm_0n(c2));
    }
    {
        RngGuard rg(3);
        ox_crossover(p1, p2, c1, c2);
        GAOPS_CHECK(tr, is_perm_0n(c1));
        GAOPS_CHECK(tr, is_perm_0n(c2));
    }
}

static void test_mutations(TestRunner& tr) {
    // BitFlip (bitset)
    {
        using V = std::bitset<16>;
        V s{};
        s.reset();

        RngGuard rg(0);
        bit_flip_mutation(s, 1.0);
        GAOPS_CHECK(tr, s.all());
    }

    // Swap / Inversion / Scramble / Insertion（順列が壊れないこと）
    {
        using V = std::array<int, 10>;
        V s = init_perm_default<int, 10>();
        GAOPS_CHECK(tr, is_perm_0n(s));

        {
            RngGuard rg(11);
            swap_mutation(s, 1.0);
            GAOPS_CHECK(tr, is_perm_0n(s));
        }
        {
            RngGuard rg(12);
            inversion_mutation(s, 1.0);
            GAOPS_CHECK(tr, is_perm_0n(s));
        }
        {
            RngGuard rg(13);
            scramble_mutation(s, 1.0);
            GAOPS_CHECK(tr, is_perm_0n(s));
        }
        {
            RngGuard rg(14);
            insertion_mutation(s, 1.0);
            GAOPS_CHECK(tr, is_perm_0n(s));
        }
    }

    // Gaussian / Creep のエッジケース
    {
        using V = std::array<float, 8>;
        V s{};
        for (auto& x : s) x = 0.0f;

        RngGuard rg(1);

        auto before = s;
        gaussian_mutation(s, 0.0, 10.0, -100.0f, 100.0f);
        GAOPS_CHECK(tr, s == before);

        gaussian_mutation(s, 1.0, 0.0, -100.0f, 100.0f); // sigma=0 は変化なし
        GAOPS_CHECK(tr, s == before);
    }
    {
        using V = std::array<int, 10>;
        V s{};
        for (auto& x : s) x = 50;

        RngGuard rg(2);
        creep_mutation(s, 1.0, 1, 0, 100);

        for (std::size_t i = 0; i < 10; ++i) {
            GAOPS_CHECK(tr, s[i] >= 0 && s[i] <= 100);
            GAOPS_CHECK(tr, std::abs(s[i] - 50) <= 1);
        }
    }
}

static void test_choice_pipeline(TestRunner& tr) {
    using V = std::array<int, 10>;

    V p1{}, p2{}, c{};
    for (std::size_t i = 0; i < 10; ++i) { p1[i] = (int)i; p2[i] = 100 + (int)i; }

    {
        RngGuard rg(5);
        std::array<double, 2> w = {0.0, 1.0}; // 2点交叉を必ず選ぶ

        choice_crossover(
            p1, p2, c, w,
            [&](const V& a, const V& b, V& out) { one_point_crossover(a, b, out); },
            [&](const V& a, const V& b, V& out) { two_point_crossover(a, b, out); }
        );

        // 基本不変条件：各要素はどちらかの親由来
        for (std::size_t i = 0; i < 10; ++i) {
            GAOPS_CHECK(tr, (c[i] == p1[i]) || (c[i] == p2[i]));
        }
    }

    {
        RngGuard rg(6);

        V s{};
        for (auto& x : s) x = 50;

        pipeline_mutation(
            s,
            [&](V& x) { random_reset_mutation(x, 0.0, 0, 100); }, // 変化なし
            [&](V& x) { creep_mutation(x, 1.0, 1, 0, 100); }      // ±1
        );

        for (std::size_t i = 0; i < 10; ++i) GAOPS_CHECK(tr, std::abs(s[i] - 50) <= 1);
    }

    {
        RngGuard rg(7);

        V s{};
        for (auto& x : s) x = 50;

        std::array<double, 2> w = {1.0, 0.0}; // creep を必ず選ぶ
        choice_mutation(
            s, w,
            [&](V& x) { creep_mutation(x, 1.0, 1, 0, 100); },
            [&](V& x) { random_reset_mutation(x, 1.0, 0, 100); }
        );

        for (std::size_t i = 0; i < 10; ++i) GAOPS_CHECK(tr, std::abs(s[i] - 50) <= 1);
    }
}

static void test_selections(TestRunner& tr) {
    // Selection 系は Population 側のメソッドでテストする。
    using Gene = int;
    ga::Population<Gene> pop;
    pop.reserve(4);

    for (int i = 0; i < 4; ++i) {
        pop.add(i, (double)(i + 1)); // fitness: 1..4
    }

    GAOPS_EQ(tr, pop.get_best_index(), 3);
    GAOPS_EQ(tr, pop.get_worst_index(), 0);

    // tournament best: best が多く選ばれるはず
    {
        RngGuard rg(123);
        std::array<int, 4> cnt{};
        cnt.fill(0);
        for (int t = 0; t < 2000; ++t) {
            int idx = pop.select_tournament_best(2);
            GAOPS_CHECK(tr, 0 <= idx && idx < 4);
            cnt[(std::size_t)idx]++;
        }
        GAOPS_CHECK(tr, cnt[3] > cnt[2] && cnt[2] >= cnt[1] && cnt[1] >= cnt[0]);
    }

    // tournament worst: worst が多く選ばれるはず
    {
        RngGuard rg(456);
        std::array<int, 4> cnt{};
        cnt.fill(0);
        for (int t = 0; t < 2000; ++t) {
            int idx = pop.select_tournament_worst(2);
            GAOPS_CHECK(tr, 0 <= idx && idx < 4);
            cnt[(std::size_t)idx]++;
        }
        GAOPS_CHECK(tr, cnt[0] > cnt[1] && cnt[1] >= cnt[2] && cnt[2] >= cnt[3]);
    }

    // edge: empty pop -> -1
    {
        ga::Population<Gene> empty;
        GAOPS_EQ(tr, empty.get_best_index(), -1);
        GAOPS_EQ(tr, empty.get_worst_index(), -1);
        GAOPS_EQ(tr, empty.select_tournament_best(3), -1);
        GAOPS_EQ(tr, empty.select_tournament_worst(3), -1);
    }
}

} // namespace ga_ops_test

int main() {
    ga_ops_test::TestRunner tr;

    ga_ops_test::test_perm_init(tr);
    ga_ops_test::test_two_point_crossover_array(tr);
    ga_ops_test::test_uniform_crossover_bitset(tr);
    ga_ops_test::test_blend_crossover_clamp(tr);
    ga_ops_test::test_arithmetic_crossover_two_child_alpha_random(tr);
    ga_ops_test::test_pmx_ox_valid_perm(tr);
    ga_ops_test::test_mutations(tr);
    ga_ops_test::test_choice_pipeline(tr);
    ga_ops_test::test_selections(tr);

    std::cerr << "[ga_ops_test] checks=" << tr.checks << " fails=" << tr.fails << "\n";
    return tr.fails ? 1 : 0;
}
#endif
