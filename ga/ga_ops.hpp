#pragma once

#include <bits/stdc++.h>

// ga 側の #if 1 テスト main() と衝突しないように、インクルード中だけ main を別名にする。
// (ga.hpp 側の #if 1 ポリシーを維持したまま、ga_ops 側で別 main を定義できるようにするため)
#define main ga__header_main
#include "ga.hpp"
#undef main

// ------------------------------------------------------------
// Operations (Selection / Crossover / Mutation)
//  - C++20 (gcc12.2) / single-thread
//  - stdlib only, gcc dependent allowed
// ------------------------------------------------------------

// ============================================================
// Crossovers
// ============================================================

template<ga::SpecLike Spec>
struct OnePointCrossover {
    using S = Spec;
    using V = typename Spec::val_t;
    static constexpr std::size_t L = ga::static_len_v<V>;

    static_assert((ga::is_arr_v<V> && std::is_trivially_copyable_v<V>) || ga::is_bs_v<V>);

    void apply(const Spec& p1, const Spec& p2, Spec& c) const requires (ga::is_arr_v<V>) {
        static std::uniform_int_distribution<std::size_t> cut_dist(0, L);
        const std::size_t k = cut_dist(ga::rng);

        std::memcpy(c.v.data(),     p1.v.data(),     k * sizeof(typename V::value_type));
        std::memcpy(c.v.data() + k, p2.v.data() + k, (L - k) * sizeof(typename V::value_type));
    }

    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const requires (ga::is_arr_v<V>) {
        static std::uniform_int_distribution<std::size_t> cut_dist(0, L);
        const std::size_t k = cut_dist(ga::rng);

        std::memcpy(c1.v.data(),     p1.v.data(),     k * sizeof(typename V::value_type));
        std::memcpy(c1.v.data() + k, p2.v.data() + k, (L - k) * sizeof(typename V::value_type));

        std::memcpy(c2.v.data(),     p2.v.data(),     k * sizeof(typename V::value_type));
        std::memcpy(c2.v.data() + k, p1.v.data() + k, (L - k) * sizeof(typename V::value_type));
    }

    void apply(const Spec& p1, const Spec& p2, Spec& c) const requires (ga::is_bs_v<V>) {
        static std::uniform_int_distribution<std::size_t> cut_dist(0, L);
        const std::size_t k = cut_dist(ga::rng);

        const auto lo = ga::low_mask<L>(k);
        const auto hi = ~lo;
        c.v = (p1.v & lo) | (p2.v & hi);
    }

    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const requires (ga::is_bs_v<V>) {
        static std::uniform_int_distribution<std::size_t> cut_dist(0, L);
        const std::size_t k = cut_dist(ga::rng);

        const auto lo = ga::low_mask<L>(k);
        const auto hi = ~lo;

        c1.v = (p1.v & lo) | (p2.v & hi);
        c2.v = (p2.v & lo) | (p1.v & hi);
    }
};

template<ga::SpecLike Spec>
struct TwoPointCrossover {
    using S = Spec;
    using V = typename Spec::val_t;
    static constexpr std::size_t L = ga::static_len_v<V>;

    static_assert((ga::is_arr_v<V> && std::is_trivially_copyable_v<V>) || ga::is_bs_v<V>);

    void apply(const Spec& p1, const Spec& p2, Spec& c) const requires (ga::is_arr_v<V>) {
        std::uniform_int_distribution<std::size_t> a_dist(0, L);
        const std::size_t a = a_dist(ga::rng);
        std::uniform_int_distribution<std::size_t> b_dist(a, L);
        const std::size_t b = b_dist(ga::rng);

        // middle [a,b) from p1, others from p2
        std::memcpy(c.v.data(),         p2.v.data(),         a * sizeof(typename V::value_type));
        std::memcpy(c.v.data() + a,     p1.v.data() + a,     (b - a) * sizeof(typename V::value_type));
        std::memcpy(c.v.data() + b,     p2.v.data() + b,     (L - b) * sizeof(typename V::value_type));
    }

    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const requires (ga::is_arr_v<V>) {
        std::uniform_int_distribution<std::size_t> a_dist(0, L);
        const std::size_t a = a_dist(ga::rng);
        std::uniform_int_distribution<std::size_t> b_dist(a, L);
        const std::size_t b = b_dist(ga::rng);

        // c1: middle from p1, others from p2
        std::memcpy(c1.v.data(),         p2.v.data(),         a * sizeof(typename V::value_type));
        std::memcpy(c1.v.data() + a,     p1.v.data() + a,     (b - a) * sizeof(typename V::value_type));
        std::memcpy(c1.v.data() + b,     p2.v.data() + b,     (L - b) * sizeof(typename V::value_type));

        // c2: middle from p2, others from p1
        std::memcpy(c2.v.data(),         p1.v.data(),         a * sizeof(typename V::value_type));
        std::memcpy(c2.v.data() + a,     p2.v.data() + a,     (b - a) * sizeof(typename V::value_type));
        std::memcpy(c2.v.data() + b,     p1.v.data() + b,     (L - b) * sizeof(typename V::value_type));
    }

    void apply(const Spec& p1, const Spec& p2, Spec& c) const requires (ga::is_bs_v<V>) {
        std::uniform_int_distribution<std::size_t> a_dist(0, L);
        std::size_t a = a_dist(ga::rng);
        std::uniform_int_distribution<std::size_t> b_dist(a, L);
        std::size_t b = b_dist(ga::rng);

        const auto mask = ga::low_mask<L>(b) ^ ga::low_mask<L>(a); // [a,b)
        c.v = (p1.v & mask) | (p2.v & ~mask);
    }

    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const requires (ga::is_bs_v<V>) {
        std::uniform_int_distribution<std::size_t> a_dist(0, L);
        std::size_t a = a_dist(ga::rng);
        std::uniform_int_distribution<std::size_t> b_dist(a, L);
        std::size_t b = b_dist(ga::rng);

        const auto mask = ga::low_mask<L>(b) ^ ga::low_mask<L>(a); // [a,b)
        c1.v = (p1.v & mask) | (p2.v & ~mask);
        c2.v = (p2.v & mask) | (p1.v & ~mask);
    }
};

template<ga::SpecLike Spec>
struct UniformCrossover {
    using S = Spec;
    using V = typename Spec::val_t;
    using E = ga::elem_of_t<V>;

    double p_pick = 0.5; // parent1 を選ぶ確率

    void apply(const Spec& p1, const Spec& p2, Spec& c) const {
        std::bernoulli_distribution pick(p_pick);

        if constexpr (ga::is_bs_v<V>) {
            constexpr std::size_t N = ga::static_len_v<V>;
            for (std::size_t i = 0; i < N; ++i) c.v.set(i, pick(ga::rng) ? p1.v.test(i) : p2.v.test(i));
        } else if constexpr (ga::is_arr_v<V>) {
            constexpr std::size_t N = ga::static_len_v<V>;
            for (std::size_t i = 0; i < N; ++i) c.v[i] = pick(ga::rng) ? p1.v[i] : p2.v[i];
        } else {
            c.v = pick(ga::rng) ? p1.v : p2.v;
        }
    }

    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const {
        std::bernoulli_distribution pick(p_pick);

        if constexpr (ga::is_bs_v<V>) {
            constexpr std::size_t N = ga::static_len_v<V>;
            for (std::size_t i = 0; i < N; ++i) {
                const bool b = pick(ga::rng);
                c1.v.set(i, b ? p1.v.test(i) : p2.v.test(i));
                c2.v.set(i, b ? p2.v.test(i) : p1.v.test(i));
            }
        } else if constexpr (ga::is_arr_v<V>) {
            constexpr std::size_t N = ga::static_len_v<V>;
            for (std::size_t i = 0; i < N; ++i) {
                const bool b = pick(ga::rng);
                c1.v[i] = b ? p1.v[i] : p2.v[i];
                c2.v[i] = b ? p2.v[i] : p1.v[i];
            }
        } else {
            const bool b = pick(ga::rng);
            c1.v = b ? p1.v : p2.v;
            c2.v = b ? p2.v : p1.v;
        }
    }
};

// Discrete Pick Crossover: Uniform の別名（p_pick を明示）
template<ga::SpecLike Spec>
using DiscretePickCrossover = UniformCrossover<Spec>;

template<ga::SpecLike Spec>
struct ArithmeticCrossover {
    using S = Spec;
    using V = typename Spec::val_t;
    using E = ga::elem_of_t<V>;

    static_assert(!ga::is_bs_v<V>, "ArithmeticCrossover does not support bitset");
    static_assert(std::is_integral_v<E> || std::is_floating_point_v<E>, "ArithmeticCrossover requires numeric element type");

    double alpha = -1.0; // [0,1] を指定すると固定。負なら乱数。

    void apply(const Spec& p1, const Spec& p2, Spec& c) const {
        const double a = pick_alpha_();
        mix_to_(p1.v, p2.v, c.v, a);
    }

    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const {
        const double a = pick_alpha_();
        mix_to_(p1.v, p2.v, c1.v, a);
        mix_to_(p1.v, p2.v, c2.v, 1.0 - a);
    }

private:
    double pick_alpha_() const {
        if (alpha >= 0.0) return alpha <= 1.0 ? alpha : 1.0;
        static std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(ga::rng);
    }

    static E clamp_(E x) {
        if constexpr (requires { Spec::lo; Spec::hi; }) {
            E lo = static_cast<E>(Spec::lo);
            E hi = static_cast<E>(Spec::hi);
            if (hi < lo) std::swap(lo, hi);
            if (x < lo) return lo;
            if (x > hi) return hi;
            return x;
        } else {
            return x;
        }
    }

    static E mix_elem_(E a, E b, double w) {
        if constexpr (std::is_floating_point_v<E>) {
            return clamp_(static_cast<E>(a * w + b * (1.0 - w)));
        } else {
            const double x = (double)a * w + (double)b * (1.0 - w);
            long long y = std::llround(x);
            return clamp_(static_cast<E>(y));
        }
    }

    static void mix_to_(const V& a, const V& b, V& out, double w) {
        if constexpr (ga::is_arr_v<V>) {
            constexpr std::size_t N = ga::static_len_v<V>;
            for (std::size_t i = 0; i < N; ++i) out[i] = mix_elem_(a[i], b[i], w);
        } else {
            out = mix_elem_(a, b, w);
        }
    }
};

// Blend Crossover (BLX-α)
template<ga::SpecLike Spec>
struct BlendCrossover {
    using S = Spec;
    using V = typename Spec::val_t;
    using E = ga::elem_of_t<V>;

    static_assert(!ga::is_bs_v<V>, "BlendCrossover does not support bitset");
    static_assert(std::is_integral_v<E> || std::is_floating_point_v<E>, "BlendCrossover requires numeric element type");

    double alpha = 0.5;

    void apply(const Spec& p1, const Spec& p2, Spec& c) const {
        blend_to_(p1.v, p2.v, c.v);
    }

    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const {
        blend_to_(p1.v, p2.v, c1.v);
        blend_to_(p2.v, p1.v, c2.v);
    }

private:
    static E clamp_(E x) {
        if constexpr (requires { Spec::lo; Spec::hi; }) {
            E lo = static_cast<E>(Spec::lo);
            E hi = static_cast<E>(Spec::hi);
            if (hi < lo) std::swap(lo, hi);
            if (x < lo) return lo;
            if (x > hi) return hi;
            return x;
        } else {
            return x;
        }
    }

    E sample_(E a, E b) const {
        double mn = (double)std::min(a, b);
        double mx = (double)std::max(a, b);
        double d  = mx - mn;

        if (!(d > 0.0)) return clamp_(a);

        const double lo = mn - alpha * d;
        const double hi = mx + alpha * d;

        static std::uniform_real_distribution<double> u01(0.0, 1.0);
        const double t = u01(ga::rng);
        const double x = lo + t * (hi - lo);

        if constexpr (std::is_floating_point_v<E>) {
            return clamp_(static_cast<E>(x));
        } else {
            return clamp_(static_cast<E>(std::llround(x)));
        }
    }

    void blend_to_(const V& a, const V& b, V& out) const {
        if constexpr (ga::is_arr_v<V>) {
            constexpr std::size_t N = ga::static_len_v<V>;
            for (std::size_t i = 0; i < N; ++i) out[i] = sample_(a[i], b[i]);
        } else {
            out = sample_(a, b);
        }
    }
};

// Permutation crossovers (PMX / OX)
template<ga::SpecLike Spec>
struct PMXCrossover {
    using S = Spec;
    using V = typename Spec::val_t;
    using E = typename V::value_type;
    static constexpr std::size_t N = ga::static_len_v<V>;

    static_assert(ga::is_perm_v<Spec>, "PMXCrossover requires ga::is_perm_v<Spec> == true");

    void apply(const Spec& p1, const Spec& p2, Spec& c) const { pmx_one_(p1.v, p2.v, c.v); }

    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const {
        pmx_one_(p1.v, p2.v, c1.v);
        pmx_one_(p2.v, p1.v, c2.v);
    }

private:
    static std::size_t idx_(E x) {
        return static_cast<std::size_t>(x);
    }

    static void pmx_one_(const V& a, const V& b, V& out) {
        if constexpr (N == 0) return;

        std::uniform_int_distribution<std::size_t> ldist(0, N - 1);
        const std::size_t l = ldist(ga::rng);
        std::uniform_int_distribution<std::size_t> rdist(l + 1, N);
        const std::size_t r = rdist(ga::rng); // [l,r)

        std::array<unsigned char, N> used{};

        // map: a[value] -> b[value] within segment (only meaningful for values in segment)
        std::array<E, N> map;

        for (std::size_t i = l; i < r; ++i) {
            const std::size_t ia = idx_(a[i]);
            out[i] = a[i];
            if (ia < N) used[ia] = 1;
            if (ia < N) map[ia] = b[i];
        }

        for (std::size_t i = 0; i < N; ++i) {
            if (l <= i && i < r) continue;

            E v = b[i];
            std::size_t iv = idx_(v);

            // Resolve conflicts via mapping until unused.
            while (iv < N && used[iv]) {
                v = map[iv];
                iv = idx_(v);
            }

            out[i] = v;
            if (iv < N) used[iv] = 1;
        }
    }
};

template<ga::SpecLike Spec>
struct OXCrossover {
    using S = Spec;
    using V = typename Spec::val_t;
    using E = typename V::value_type;
    static constexpr std::size_t N = ga::static_len_v<V>;

    static_assert(ga::is_perm_v<Spec>, "OXCrossover requires ga::is_perm_v<Spec> == true");

    void apply(const Spec& p1, const Spec& p2, Spec& c) const { ox_one_(p1.v, p2.v, c.v); }

    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const {
        ox_one_(p1.v, p2.v, c1.v);
        ox_one_(p2.v, p1.v, c2.v);
    }

private:
    static std::size_t idx_(E x) { return static_cast<std::size_t>(x); }

    static void ox_one_(const V& a, const V& b, V& out) {
        if constexpr (N == 0) return;

        std::uniform_int_distribution<std::size_t> ldist(0, N - 1);
        const std::size_t l = ldist(ga::rng);
        std::uniform_int_distribution<std::size_t> rdist(l + 1, N);
        const std::size_t r = rdist(ga::rng); // [l,r)

        out = a; // copy then fix outside segment
        std::array<unsigned char, N> used{};
        used.fill(0);

        for (std::size_t i = l; i < r; ++i) {
            const std::size_t ia = idx_(a[i]);
            if (ia < N) used[ia] = 1;
        }

        auto in_seg = [&](std::size_t i) { return l <= i && i < r; };

        std::size_t pos = r % N;
        for (std::size_t t = 0; t < N; ++t) {
            const E v = b[(r + t) % N];
            const std::size_t iv = idx_(v);
            if (iv < N && used[iv]) continue;

            while (in_seg(pos)) pos = (pos + 1) % N;
            out[pos] = v;
            if (iv < N) used[iv] = 1;
            pos = (pos + 1) % N;
        }
    }
};

// ============================================================
// Mutations (in-place)
// ============================================================

template<ga::SpecLike Spec>
struct RandomResetMutation {
    using S = Spec;
    double p_reset = 0.01;

    void apply(Spec& s) const {
        std::bernoulli_distribution do_reset(p_reset);
        ga::visit_elems_ref(s, [&](auto&& elem, std::size_t /*i*/) {
            if (do_reset(ga::rng)) {
                elem = ga::random_elem_value<Spec>();
            }
        });
    }
};

template<ga::SpecLike Spec>
struct BitFlipMutation {
    using S = Spec;
    using V = typename Spec::val_t;
    using E = ga::elem_of_t<V>;

    static_assert(std::is_same_v<std::remove_cv_t<E>, bool>, "BitFlipMutation requires bool element type");

    double p_flip = 0.01;

    void apply(Spec& s) const {
        std::bernoulli_distribution flip(p_flip);

        if constexpr (ga::is_bs_v<V>) {
            constexpr std::size_t N = ga::static_len_v<V>;
            for (std::size_t i = 0; i < N; ++i) if (flip(ga::rng)) s.v.flip(i);
        } else if constexpr (ga::is_arr_v<V>) {
            constexpr std::size_t N = ga::static_len_v<V>;
            for (std::size_t i = 0; i < N; ++i) if (flip(ga::rng)) s.v[i] = !static_cast<bool>(s.v[i]);
        } else {
            if (flip(ga::rng)) s.v = !static_cast<bool>(s.v);
        }
    }
};

template<ga::SpecLike Spec>
struct SwapMutation {
    using S = Spec;
    using V = typename Spec::val_t;

    static_assert(ga::is_arr_v<V>, "SwapMutation requires val_t == std::array");

    double p_swap = 0.1;

    void apply(Spec& s) const {
        constexpr std::size_t N = ga::static_len_v<V>;
        if constexpr (N < 2) return;

        std::bernoulli_distribution do_it(p_swap);
        if (!do_it(ga::rng)) return;

        std::uniform_int_distribution<std::size_t> dist(0, N - 1);
        std::size_t i = dist(ga::rng);
        std::size_t j = dist(ga::rng);
        if (i == j) j = (j + 1) % N;

        std::swap(s.v[i], s.v[j]);
    }
};

template<ga::SpecLike Spec>
struct InversionMutation {
    using S = Spec;
    using V = typename Spec::val_t;

    static_assert(ga::is_arr_v<V>, "InversionMutation requires val_t == std::array");

    double p_inv = 0.1;

    void apply(Spec& s) const {
        constexpr std::size_t N = ga::static_len_v<V>;
        if constexpr (N < 2) return;

        std::bernoulli_distribution do_it(p_inv);
        if (!do_it(ga::rng)) return;

        std::uniform_int_distribution<std::size_t> ldist(0, N - 1);
        const std::size_t l = ldist(ga::rng);
        std::uniform_int_distribution<std::size_t> rdist(l + 1, N);
        const std::size_t r = rdist(ga::rng);

        std::reverse(s.v.begin() + (std::ptrdiff_t)l, s.v.begin() + (std::ptrdiff_t)r);
    }
};

template<ga::SpecLike Spec>
struct ScrambleMutation {
    using S = Spec;
    using V = typename Spec::val_t;

    static_assert(ga::is_arr_v<V>, "ScrambleMutation requires val_t == std::array");

    double p_scramble = 0.1;

    void apply(Spec& s) const {
        constexpr std::size_t N = ga::static_len_v<V>;
        if constexpr (N < 2) return;

        std::bernoulli_distribution do_it(p_scramble);
        if (!do_it(ga::rng)) return;

        std::uniform_int_distribution<std::size_t> ldist(0, N - 1);
        const std::size_t l = ldist(ga::rng);
        std::uniform_int_distribution<std::size_t> rdist(l + 1, N);
        const std::size_t r = rdist(ga::rng);

        std::shuffle(s.v.begin() + (std::ptrdiff_t)l, s.v.begin() + (std::ptrdiff_t)r, ga::rng);
    }
};

template<ga::SpecLike Spec>
struct InsertionMutation {
    using S = Spec;
    using V = typename Spec::val_t;

    static_assert(ga::is_arr_v<V>, "InsertionMutation requires val_t == std::array");

    double p_insert = 0.1;

    void apply(Spec& s) const {
        constexpr std::size_t N = ga::static_len_v<V>;
        if constexpr (N < 2) return;

        std::bernoulli_distribution do_it(p_insert);
        if (!do_it(ga::rng)) return;

        std::uniform_int_distribution<std::size_t> dist(0, N - 1);
        std::size_t i = dist(ga::rng);
        std::size_t j = dist(ga::rng);
        if (i == j) j = (j + 1) % N;

        auto tmp = s.v[i];
        if (i < j) {
            for (std::size_t k = i; k < j; ++k) s.v[k] = s.v[k + 1];
            s.v[j] = tmp;
        } else {
            for (std::size_t k = i; k > j; --k) s.v[k] = s.v[k - 1];
            s.v[j] = tmp;
        }
    }
};

template<ga::SpecLike Spec>
struct GaussianMutation {
    using S = Spec;
    using V = typename Spec::val_t;
    using E = ga::elem_of_t<V>;

    static_assert(!ga::is_bs_v<V>, "GaussianMutation does not support bitset");
    static_assert(std::is_floating_point_v<E>, "GaussianMutation requires floating-point element type");

    double p_mut = 0.1;
    double sigma = 1.0;

    void apply(Spec& s) const {
        if (!(sigma > 0.0)) return;

        std::bernoulli_distribution do_it(p_mut);
        std::normal_distribution<double> n01(0.0, sigma);

        auto clamp = [](E x) {
            if constexpr (requires { Spec::lo; Spec::hi; }) {
                E lo = static_cast<E>(Spec::lo);
                E hi = static_cast<E>(Spec::hi);
                if (hi < lo) std::swap(lo, hi);
                if (x < lo) return lo;
                if (x > hi) return hi;
            }
            return x;
        };

        if constexpr (ga::is_arr_v<V>) {
            constexpr std::size_t N = ga::static_len_v<V>;
            for (std::size_t i = 0; i < N; ++i) {
                if (do_it(ga::rng)) s.v[i] = clamp(static_cast<E>(s.v[i] + (E)n01(ga::rng)));
            }
        } else {
            if (do_it(ga::rng)) s.v = clamp(static_cast<E>(s.v + (E)n01(ga::rng)));
        }
    }
};

template<ga::SpecLike Spec>
struct CreepMutation {
    using S = Spec;
    using V = typename Spec::val_t;
    using E = ga::elem_of_t<V>;

    static_assert(!ga::is_bs_v<V>, "CreepMutation does not support bitset");
    static_assert(std::is_integral_v<E>, "CreepMutation requires integral element type");

    double p_mut = 0.1;
    int step_max = 1; // 1 なら ±1

    void apply(Spec& s) const {
        if (step_max <= 0) return;

        std::bernoulli_distribution do_it(p_mut);
        std::uniform_int_distribution<int> step_dist(-step_max, step_max);

        auto clamp = [](E x) {
            if constexpr (requires { Spec::lo; Spec::hi; }) {
                E lo = static_cast<E>(Spec::lo);
                E hi = static_cast<E>(Spec::hi);
                if (hi < lo) std::swap(lo, hi);
                if (x < lo) return lo;
                if (x > hi) return hi;
            }
            return x;
        };

        auto apply_one = [&](E x) {
            int d = 0;
            while (d == 0) d = step_dist(ga::rng);
            long long y = (long long)x + (long long)d;
            return clamp(static_cast<E>(y));
        };

        if constexpr (ga::is_arr_v<V>) {
            constexpr std::size_t N = ga::static_len_v<V>;
            for (std::size_t i = 0; i < N; ++i) if (do_it(ga::rng)) s.v[i] = apply_one(s.v[i]);
        } else {
            if (do_it(ga::rng)) s.v = apply_one(s.v);
        }
    }
};

// ------------------------------------------------------------
// Higher-order operators
// ------------------------------------------------------------

// Weighted random choice of one crossover operator.
template<ga::SpecLike Spec, class... Crossovers>
struct ChoiceCrossover {
    using S = Spec;
    static constexpr std::size_t N = sizeof...(Crossovers);
    static_assert(N > 0);

    std::tuple<Crossovers...> crossovers{};
    std::array<double, N> weights{};

    ChoiceCrossover() { weights.fill(1.0); }

    explicit ChoiceCrossover(Crossovers... ops)
        : crossovers(std::move(ops)...) {
        weights.fill(1.0);
    }

    ChoiceCrossover(std::array<double, N> w, Crossovers... ops)
        : crossovers(std::move(ops)...), weights(w) {}

    void apply(const Spec& p1, const Spec& p2, Spec& c) const {
        const std::size_t idx = pick_index_();
        visit_(idx, [&](const auto& op) { op.apply(p1, p2, c); });
    }

    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const {
        const std::size_t idx = pick_index_();
        visit_(idx, [&](const auto& op) { op.apply(p1, p2, c1, c2); });
    }

private:
    std::size_t pick_index_() const {
        double sum = 0.0;
        for (double w : weights) if (w > 0.0) sum += w;

        if (!(sum > 0.0)) {
            std::uniform_int_distribution<std::size_t> uni(0, N - 1);
            return uni(ga::rng);
        }

        std::uniform_real_distribution<double> dist(0.0, sum);
        double r = dist(ga::rng);
        for (std::size_t i = 0; i < N; ++i) {
            const double w = weights[i] > 0.0 ? weights[i] : 0.0;
            if (r < w) return i;
            r -= w;
        }
        return N - 1;
    }

    template<class F, std::size_t... I>
    void visit_impl_(std::size_t idx, F&& f, std::index_sequence<I...>) const {
        ((idx == I ? (void)f(std::get<I>(crossovers)) : (void)0), ...);
    }

    template<class F>
    void visit_(std::size_t idx, F&& f) const {
        visit_impl_(idx, std::forward<F>(f), std::make_index_sequence<N>{});
    }
};

// Apply multiple mutations in sequence.
template<ga::SpecLike Spec, class... Mutations>
struct PipelineMutation {
    using S = Spec;
    std::tuple<Mutations...> muts{};

    PipelineMutation() = default;

    explicit PipelineMutation(Mutations... ms)
        : muts(std::move(ms)...) {}

    void apply(Spec& s) const {
        std::apply([&](const auto&... m) { (m.apply(s), ...); }, muts);
    }
};

// Weighted random choice of one mutation operator.
template<ga::SpecLike Spec, class... Mutations>
struct ChoiceMutation {
    using S = Spec;
    static constexpr std::size_t N = sizeof...(Mutations);
    static_assert(N > 0);

    std::tuple<Mutations...> muts{};
    std::array<double, N> weights{};

    ChoiceMutation() { weights.fill(1.0); }

    explicit ChoiceMutation(Mutations... ms)
        : muts(std::move(ms)...) {
        weights.fill(1.0);
    }

    ChoiceMutation(std::array<double, N> w, Mutations... ms)
        : muts(std::move(ms)...), weights(w) {}

    void apply(Spec& s) const {
        const std::size_t idx = pick_index_();
        visit_(idx, [&](const auto& m) { m.apply(s); });
    }

private:
    std::size_t pick_index_() const {
        double sum = 0.0;
        for (double w : weights) if (w > 0.0) sum += w;

        if (!(sum > 0.0)) {
            std::uniform_int_distribution<std::size_t> uni(0, N - 1);
            return uni(ga::rng);
        }

        std::uniform_real_distribution<double> dist(0.0, sum);
        double r = dist(ga::rng);
        for (std::size_t i = 0; i < N; ++i) {
            const double w = weights[i] > 0.0 ? weights[i] : 0.0;
            if (r < w) return i;
            r -= w;
        }
        return N - 1;
    }

    template<class F, std::size_t... I>
    void visit_impl_(std::size_t idx, F&& f, std::index_sequence<I...>) const {
        ((idx == I ? (void)f(std::get<I>(muts)) : (void)0), ...);
    }

    template<class F>
    void visit_(std::size_t idx, F&& f) const {
        visit_impl_(idx, std::forward<F>(f), std::make_index_sequence<N>{});
    }
};

// ============================================================
// Selections
// ============================================================

template<ga::GeneLike Gene>
struct TournamentSelection {
    int k = 3;

    void prepare(const ga::Population<Gene>& /*pop*/) const {}

    int apply(const ga::Population<Gene>& pop) const {
        const int n = pop.size();
        if (n <= 0) return -1;
        if (k <= 1) {
            std::uniform_int_distribution<int> dist(0, n - 1);
            return dist(ga::rng);
        }

        std::uniform_int_distribution<int> dist(0, n - 1);
        int best = dist(ga::rng);
        for (int i = 1; i < k; ++i) {
            const int cand = dist(ga::rng);
            if (pop.better(cand, best)) best = cand;
        }
        return best;
    }
};

template<ga::GeneLike Gene>
struct ReverseTournamentSelection {
    int k = 3;

    void prepare(const ga::Population<Gene>& /*pop*/) const {}

    int apply(const ga::Population<Gene>& pop) const {
        const int n = pop.size();
        if (n <= 0) return -1;

        std::uniform_int_distribution<int> dist(0, n - 1);
        if (k <= 1) return dist(ga::rng);

        int worst = dist(ga::rng);
        for (int i = 1; i < k; ++i) {
            const int cand = dist(ga::rng);
            if (pop.better(worst, cand)) worst = cand;
        }
        return worst;
    }
};

template<ga::GeneLike Gene>
struct WorstSelection {
    void prepare(const ga::Population<Gene>& /*pop*/) const {}

    int apply(const ga::Population<Gene>& pop) const {
        return pop.get_worst_index();
    }
};

// Roulette selection (fitness proportionate, robust to negative / NaN)
template<ga::GeneLike Gene>
struct RouletteSelection {
    std::vector<double> prefix{};
    double total = 0.0;

    void prepare(const ga::Population<Gene>& pop) {
        const std::size_t n = pop.gene().size();
        prefix.assign(n, 0.0);
        total = 0.0;
        if (n == 0) return;

        const std::size_t fn = std::min(n, pop.fitness().size());

        // +inf fitness should dominate roulette.
        // If any +inf exists, select uniformly among the +inf indices.
        std::size_t inf_cnt = 0;
        for (std::size_t i = 0; i < fn; ++i) {
            const double f = pop.fitness()[i];
            if (std::isinf(f) && f > 0.0) ++inf_cnt;
        }
        if (inf_cnt > 0) {
            double acc = 0.0;
            for (std::size_t i = 0; i < fn; ++i) {
                const double f = pop.fitness()[i];
                if (std::isinf(f) && f > 0.0) acc += 1.0;
                prefix[i] = acc;
            }
            for (std::size_t i = fn; i < n; ++i) prefix[i] = acc;
            total = acc;
            return;
        }

        bool has_finite = false;
        long double mn = std::numeric_limits<long double>::infinity();
        long double mx = -std::numeric_limits<long double>::infinity();
        for (std::size_t i = 0; i < fn; ++i) {
            const double f = pop.fitness()[i];
            if (!std::isfinite(f)) continue; // NaN / +/-inf are treated as 0 weight (handled above for +inf)
            has_finite = true;
            mn = std::min(mn, (long double)f);
            mx = std::max(mx, (long double)f);
        }
        if (!has_finite) {
            // all NaN / +/-inf -> uniform fallback
            prefix.clear();
            total = 0.0;
            return;
        }

        const long double shift = (mn < 0.0L) ? -mn : 0.0L;
        constexpr long double eps = 1e-12L;

        // Normalize weights by a positive scale to avoid overflow when summing.
        long double scale = mx + shift + eps;
        if (!(scale > 0.0L)) scale = 1.0L;

        double acc = 0.0;
        for (std::size_t i = 0; i < fn; ++i) {
            const double f = pop.fitness()[i];
            double w = 0.0;
            if (std::isfinite(f)) {
                const long double wld = ((long double)f + shift + eps) / scale;
                w = (double)wld;
                if (!(w > 0.0)) w = 0.0;
            }
            acc += w;
            prefix[i] = acc;
        }
        // if fitness size < gene size: remaining are 0 weight
        for (std::size_t i = fn; i < n; ++i) prefix[i] = acc;

        total = acc;
        if (!(total > 0.0) || !std::isfinite(total)) {
            prefix.clear();
            total = 0.0;
        }
    }

    int apply(const ga::Population<Gene>& pop) const {
        const int n = pop.size();
        if (n <= 0) return -1;

        if (!(total > 0.0) || !std::isfinite(total) || prefix.size() != (std::size_t)n) {
            std::uniform_int_distribution<int> uni(0, n - 1);
            return uni(ga::rng);
        }

        std::uniform_real_distribution<double> dist(0.0, total);
        double r = dist(ga::rng);
        if (r <= 0.0) r = std::nextafter(0.0, 1.0);

        const auto it = std::lower_bound(prefix.begin(), prefix.end(), r);
        const std::size_t idx = (it == prefix.end()) ? (std::size_t)(n - 1) : (std::size_t)std::distance(prefix.begin(), it);
        return (int)idx;
    }
};




// Rank selection (roulette on ranks; higher fitness -> higher rank via pop.better)
template<ga::GeneLike Gene>
struct RankSelection {
    std::vector<int> order{};
    std::vector<double> prefix{};
    double total = 0.0;

    void prepare(const ga::Population<Gene>& pop) {
        const int n = pop.size();
        order.resize(std::max(n, 0));
        std::iota(order.begin(), order.end(), 0);

        std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return pop.better(a, b); });

        prefix.assign((std::size_t)std::max(n, 0), 0.0);
        total = 0.0;

        // weight: best gets n, worst gets 1
        for (int r = 0; r < n; ++r) {
            const double w = (double)(n - r);
            total += w;
            prefix[(std::size_t)r] = total;
        }
        if (!(total > 0.0)) {
            order.clear();
            prefix.clear();
            total = 0.0;
        }
    }

    int apply(const ga::Population<Gene>& pop) const {
        const int n = pop.size();
        if (n <= 0) return -1;

        if (!(total > 0.0) || (int)order.size() != n || (int)prefix.size() != n) {
            std::uniform_int_distribution<int> uni(0, n - 1);
            return uni(ga::rng);
        }

        std::uniform_real_distribution<double> dist(0.0, total);
        const double r = dist(ga::rng);
        const auto it = std::lower_bound(prefix.begin(), prefix.end(), r);
        const int pos = (it == prefix.end()) ? (n - 1) : (int)std::distance(prefix.begin(), it);
        return order[(std::size_t)pos];
    }
};

#if 0
// ------------------------------------------------------------
// Tests (ga_ops)
//  - 無効化しやすいよう #if 1 ～ #endif
//  - テストでのみ使用する include はここに置く
// ------------------------------------------------------------
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

template<class Arr>
static bool is_perm_0n(const Arr& a) {
    constexpr std::size_t N = ga::static_len_v<Arr>;
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

// -------------------- Specs for tests --------------------
struct OpsFloatArr {
    using elem_t = float;
    static constexpr elem_t lo = -10.0f;
    static constexpr elem_t hi =  10.0f;
    static constexpr std::size_t n_bins = 8;
    static constexpr elem_t def = 0.0f;
    using val_t = std::array<elem_t, 8>;
    val_t v = ga::init_array<val_t>(def);
};

struct OpsIntArr {
    using elem_t = int;
    static constexpr elem_t lo = 0;
    static constexpr elem_t hi = 100;
    static constexpr std::size_t n_bins = 11;
    static constexpr elem_t def = 0;
    using val_t = std::array<elem_t, 10>;
    val_t v = ga::init_array<val_t>(def);
};

struct OpsBitset {
    using elem_t = bool;
    static constexpr elem_t def = false;
    using val_t = std::bitset<16>;
    val_t v = ga::init_bs<val_t>(def);
};

struct OpsPerm10 {
    using elem_t = int;
    static constexpr elem_t lo = 0;
    static constexpr elem_t hi = 9;
    static constexpr std::size_t n_bins = 10;
    static constexpr elem_t def = 0;
    static constexpr bool is_perm = true;
    using val_t = std::array<elem_t, 10>;
    val_t v = ga::init_perm<val_t>();
};

static_assert(ga::is_perm_v<OpsPerm10>);
static_assert(!ga::is_perm_v<OpsIntArr>);
static_assert(ga::init_perm<std::array<int, 5>>()[0] == 0);

// -------------------- Tests --------------------

static void test_perm_init(TestRunner& tr) {
    OpsPerm10 s{};
    ga::init_default(s);
    GAOPS_CHECK(tr, is_perm_0n(s.v));
    for (int i = 0; i < 10; ++i) GAOPS_CHECK(tr, s.v[(std::size_t)i] == i);

    {
        RngGuard rg(123);
        ga::init_random(s);
        GAOPS_CHECK(tr, is_perm_0n(s.v));
    }
}

static void test_two_point_crossover_array(TestRunner& tr) {
    using Spec = OpsIntArr;
    using V = typename Spec::val_t;

    Spec p1{}, p2{}, c{};
    for (std::size_t i = 0; i < ga::static_len_v<V>; ++i) {
        p1.v[i] = (int)i;
        p2.v[i] = 100 + (int)i;
    }

    {
        RngGuard rg(7);
        std::mt19937 local = ga::rng;

        // expected: emulate internal draws
        TwoPointCrossover<Spec> op;
        std::uniform_int_distribution<std::size_t> a_dist(0, ga::static_len_v<V>);
        const std::size_t a = a_dist(local);
        std::uniform_int_distribution<std::size_t> b_dist(a, ga::static_len_v<V>);
        const std::size_t b = b_dist(local);

        V exp{};
        std::memcpy(exp.data(),         p2.v.data(),         a * sizeof(int));
        std::memcpy(exp.data() + a,     p1.v.data() + a,     (b - a) * sizeof(int));
        std::memcpy(exp.data() + b,     p2.v.data() + b,     (ga::static_len_v<V> - b) * sizeof(int));

        op.apply(p1, p2, c);
        GAOPS_CHECK(tr, c.v == exp);
    }
}

static void test_uniform_crossover_bitset(TestRunner& tr) {
    using Spec = OpsBitset;
    Spec p1{}, p2{}, c1{}, c2{};

    p1.v.reset(); p2.v.set();
    {
        RngGuard rg(1);
        UniformCrossover<Spec> op;
        op.p_pick = 0.5;
        op.apply(p1, p2, c1, c2);

        // Invariants: each bit is from one parent, and c2 is complementary to c1 (since p1=0, p2=1).
        for (std::size_t i = 0; i < 16; ++i) {
            GAOPS_CHECK(tr, (c1.v.test(i) == false) || (c1.v.test(i) == true));
            GAOPS_CHECK(tr, c2.v.test(i) == !c1.v.test(i));
        }
    }
}

static void test_blend_crossover_clamp(TestRunner& tr) {
    using Spec = OpsFloatArr;

    Spec p1{}, p2{}, c{};
    for (std::size_t i = 0; i < 8; ++i) {
        p1.v[i] = -10.0f;
        p2.v[i] =  10.0f;
    }

    {
        RngGuard rg(42);
        BlendCrossover<Spec> op;
        op.alpha = 0.5;
        op.apply(p1, p2, c);

        for (std::size_t i = 0; i < 8; ++i) {
            GAOPS_CHECK(tr, c.v[i] >= Spec::lo - 1e-6f && c.v[i] <= Spec::hi + 1e-6f);
        }
    }
}

static void test_pmx_ox_valid_perm(TestRunner& tr) {
    using Spec = OpsPerm10;
    Spec p1{}, p2{}, c1{}, c2{};

    // create two known permutations
    for (int i = 0; i < 10; ++i) p1.v[(std::size_t)i] = i;
    for (int i = 0; i < 10; ++i) p2.v[(std::size_t)i] = 9 - i;

    {
        RngGuard rg(3);
        PMXCrossover<Spec> pmx;
        pmx.apply(p1, p2, c1, c2);
        GAOPS_CHECK(tr, is_perm_0n(c1.v));
        GAOPS_CHECK(tr, is_perm_0n(c2.v));
    }
    {
        RngGuard rg(3);
        OXCrossover<Spec> ox;
        ox.apply(p1, p2, c1, c2);
        GAOPS_CHECK(tr, is_perm_0n(c1.v));
        GAOPS_CHECK(tr, is_perm_0n(c2.v));
    }
}

static void test_mutations(TestRunner& tr) {
    // BitFlip (bitset)
    {
        using Spec = OpsBitset;
        Spec s{};
        s.v.reset();
        BitFlipMutation<Spec> m;
        m.p_flip = 1.0;

        RngGuard rg(0);
        m.apply(s);
        GAOPS_CHECK(tr, s.v.all());
    }

    // Swap / Inversion / Scramble / Insertion (permutation stays valid)
    {
        using Spec = OpsPerm10;
        Spec s{};
        ga::init_default(s);
        GAOPS_CHECK(tr, is_perm_0n(s.v));

        {
            RngGuard rg(11);
            SwapMutation<Spec> m; m.p_swap = 1.0;
            m.apply(s);
            GAOPS_CHECK(tr, is_perm_0n(s.v));
        }
        {
            RngGuard rg(12);
            InversionMutation<Spec> m; m.p_inv = 1.0;
            m.apply(s);
            GAOPS_CHECK(tr, is_perm_0n(s.v));
        }
        {
            RngGuard rg(13);
            ScrambleMutation<Spec> m; m.p_scramble = 1.0;
            m.apply(s);
            GAOPS_CHECK(tr, is_perm_0n(s.v));
        }
        {
            RngGuard rg(14);
            InsertionMutation<Spec> m; m.p_insert = 1.0;
            m.apply(s);
            GAOPS_CHECK(tr, is_perm_0n(s.v));
        }
    }

    // Gaussian / Creep edge cases
    {
        using Spec = OpsFloatArr;
        Spec s{};
        for (auto& x : s.v) x = 0.0f;

        GaussianMutation<Spec> g;
        g.p_mut = 0.0; g.sigma = 10.0;
        RngGuard rg(1);
        auto before = s.v;
        g.apply(s);
        GAOPS_CHECK(tr, s.v == before);

        g.p_mut = 1.0; g.sigma = 0.0;
        g.apply(s);
        GAOPS_CHECK(tr, s.v == before);
    }
    {
        using Spec = OpsIntArr;
        Spec s{};
        for (std::size_t i = 0; i < 10; ++i) s.v[i] = 50;

        CreepMutation<Spec> c;
        c.p_mut = 1.0; c.step_max = 1;

        RngGuard rg(2);
        c.apply(s);
        for (std::size_t i = 0; i < 10; ++i) {
            GAOPS_CHECK(tr, s.v[i] >= Spec::lo && s.v[i] <= Spec::hi);
            GAOPS_CHECK(tr, std::abs(s.v[i] - 50) <= 1);
        }
    }
}

static void test_choice_pipeline(TestRunner& tr) {
    using Spec = OpsIntArr;

    Spec p1{}, p2{}, c{};
    for (std::size_t i = 0; i < 10; ++i) { p1.v[i] = (int)i; p2.v[i] = 100 + (int)i; }

    {
        RngGuard rg(5);
        ChoiceCrossover<Spec, OnePointCrossover<Spec>, TwoPointCrossover<Spec>> cc;
        cc.weights = {0.0, 1.0}; // always pick TwoPoint
        cc.apply(p1, p2, c);

        // basic invariant: values come from either parent
        for (std::size_t i = 0; i < 10; ++i) {
            GAOPS_CHECK(tr, (c.v[i] == p1.v[i]) || (c.v[i] == p2.v[i]));
        }
    }

    {
        RngGuard rg(6);
        RandomResetMutation<Spec> rrm; rrm.p_reset = 0.0;
        CreepMutation<Spec> creep; creep.p_mut = 1.0; creep.step_max = 1;

        PipelineMutation<Spec, RandomResetMutation<Spec>, CreepMutation<Spec>> pm(rrm, creep);

        Spec s{};
        for (std::size_t i = 0; i < 10; ++i) s.v[i] = 50;
        pm.apply(s);

        for (std::size_t i = 0; i < 10; ++i) GAOPS_CHECK(tr, std::abs(s.v[i] - 50) <= 1);
    }

    {
        RngGuard rg(7);
        ChoiceMutation<Spec, CreepMutation<Spec>, RandomResetMutation<Spec>> cm;
        cm.weights = {1.0, 0.0}; // always creep

        Spec s{};
        for (std::size_t i = 0; i < 10; ++i) s.v[i] = 50;
        cm.apply(s);

        for (std::size_t i = 0; i < 10; ++i) GAOPS_CHECK(tr, std::abs(s.v[i] - 50) <= 1);
    }
}

static void test_selections(TestRunner& tr) {
    using Gene = std::tuple<OpsIntArr>;
    ga::Population<Gene> pop;
    pop.reserve(4);

    // deterministic genes (content irrelevant for selection), use add() API.
    for (int i = 0; i < 4; ++i) {
        Gene g{};
        ga::init_gene_default(g);
        pop.add(g, (double)(i + 1));
    }

    // roulette: higher fitness should be chosen more often
    {
        RngGuard rg(123);
        RouletteSelection<Gene> rs;
        rs.prepare(pop);

        std::array<int, 4> cnt{};
        cnt.fill(0);
        for (int t = 0; t < 2000; ++t) {
            int idx = rs.apply(pop);
            GAOPS_CHECK(tr, 0 <= idx && idx < 4);
            cnt[(std::size_t)idx]++;
        }
        GAOPS_CHECK(tr, cnt[3] > cnt[2] && cnt[2] > cnt[1] && cnt[1] > cnt[0]);
    }

    // rank: best should be chosen more often
    {
        RngGuard rg(456);
        RankSelection<Gene> rs;
        rs.prepare(pop);

        std::array<int, 4> cnt{};
        cnt.fill(0);
        for (int t = 0; t < 2000; ++t) {
            int idx = rs.apply(pop);
            GAOPS_CHECK(tr, 0 <= idx && idx < 4);
            cnt[(std::size_t)idx]++;
        }
        GAOPS_CHECK(tr, cnt[3] > cnt[2] && cnt[2] > cnt[1] && cnt[1] > cnt[0]);
    }

    // edge: empty pop -> -1
    {
        ga::Population<Gene> empty;
        RouletteSelection<Gene> r;
        r.prepare(empty);
        GAOPS_EQ(tr, r.apply(empty), -1);

        RankSelection<Gene> rk;
        rk.prepare(empty);
        GAOPS_EQ(tr, rk.apply(empty), -1);
    }
}

} // namespace ga_ops_test

int main() {
    ga_ops_test::TestRunner tr;

    ga_ops_test::test_perm_init(tr);
    ga_ops_test::test_two_point_crossover_array(tr);
    ga_ops_test::test_uniform_crossover_bitset(tr);
    ga_ops_test::test_blend_crossover_clamp(tr);
    ga_ops_test::test_pmx_ox_valid_perm(tr);
    ga_ops_test::test_mutations(tr);
    ga_ops_test::test_choice_pipeline(tr);
    ga_ops_test::test_selections(tr);

    std::cerr << "[ga_ops_test] checks=" << tr.checks << " fails=" << tr.fails << "\n";
    return tr.fails ? 1 : 0;
}
#endif
