#pragma once

#include <bits/stdc++.h>
using namespace std;

namespace ga {
inline std::mt19937 rng(42);

template<class Arr>
constexpr Arr init_array(typename Arr::value_type init) { Arr a{}; a.fill(init); return a; }

template<class BS>
constexpr BS init_bs(bool on) { BS a{}; if (on) { a.set(); } return a; }

// is_arr_v
template<class V> inline constexpr bool is_arr_v = false;
template<class T, std::size_t N> inline constexpr bool is_arr_v<std::array<T, N>> = true;

// is_bs_v
template<class V> inline constexpr bool is_bs_v = false;
template<std::size_t N> inline constexpr bool is_bs_v<std::bitset<N>> = true;

// static_len_v
template<class V> struct static_len : std::integral_constant<std::size_t, 1> {};
template<class T, std::size_t N> struct static_len<std::array<T, N>> : std::integral_constant<std::size_t, N> {};
template<std::size_t N> struct static_len<std::bitset<N>> : std::integral_constant<std::size_t, N> {};
template<class V> inline constexpr std::size_t static_len_v = static_len<V>::value;

// elem_of_t
template<class V> struct elem_of { using type = V; };
template<class T, std::size_t N> struct elem_of<std::array<T, N>> { using type = T; };
template<std::size_t N> struct elem_of<std::bitset<N>> { using type = bool; };
template<class V> using elem_of_t = typename elem_of<V>::type;

// is_enum_v (Spec 側が options を持つ形を想定)
template<class T>
inline constexpr bool is_enum_v = requires {
    T::options;
    requires is_arr_v<std::remove_cvref_t<decltype(T::options)>>;
};

// def_v
template<class Spec>
inline constexpr ga::elem_of_t<typename Spec::val_t> def_v = []() constexpr {
    using E = ga::elem_of_t<typename Spec::val_t>;
    if constexpr (requires { Spec::def; }) {
        return static_cast<E>(Spec::def);
    } else {
        return E{};
    }
}();

// 依存 false（static_assert 用）
template<class> inline constexpr bool dependent_false_v = false;

// n_bins_v (引数は常に Spec と決める)
template<class T>
inline constexpr size_t n_bins_v = []() constexpr -> std::size_t {
    using E = std::remove_cv_t<elem_of_t<typename T::val_t>>;

    if constexpr (std::is_same_v<E, bool>) {
        return 2;
    } else if constexpr (is_enum_v<T>) {
        return std::tuple_size_v<std::remove_cvref_t<decltype(T::options)>>;
    } else if constexpr (requires { T::n_bins; }) {
        return static_cast<std::size_t>(T::n_bins);
    } else {
        static_assert(dependent_false_v<T>,
                      "n_bins_v<Spec>: elem_of_t<typename Spec::val_t> is not bool, and Spec::options or Spec::n_bins is missing.");
        return 0;
    }
}();

// ---------------------------------------------
// bitset 用 low_mask（free関数）
// ---------------------------------------------
template<std::size_t N>
inline std::bitset<N> low_mask(std::size_t k) {
    if (k >= N) { std::bitset<N> m; m.set(); return m; }
    if (k == 0) return std::bitset<N>{};

    std::bitset<N> m;
    m.set();
    m >>= (N - k); // 下位kだけ1
    return m;
}

// ---------------------------------------------
// Spec 単位の要素走査 visit_elems（統一）
//  - array/scalar: 参照を渡す
//  - bitset: set(i,...) を内部で吸収する BitRef を渡す
//  - idx も渡す（不要なら無視してOK）
// ---------------------------------------------
template<std::size_t N>
struct BitRef {
    std::bitset<N>* bs = nullptr;
    std::size_t i = 0;

    BitRef() = default;
    BitRef(std::bitset<N>& b, std::size_t idx) : bs(&b), i(idx) {}

    operator bool() const { return bs->test(i); }

    BitRef& operator=(bool v) {
        bs->set(i, v);
        return *this;
    }

    // 代入チェーン等を許す
    BitRef& operator=(const BitRef& other) {
        return (*this) = (bool)other;
    }

    void flip() { bs->flip(i); }
};

template<class Spec, class F>
inline void visit_elems(Spec& s, F&& f) {
    using V = typename Spec::val_t;

    if constexpr (is_arr_v<V>) {
        constexpr std::size_t L = static_len_v<V>;
        for (std::size_t i = 0; i < L; ++i) f(s.v[i], i);
    } else if constexpr (is_bs_v<V>) {
        constexpr std::size_t L = static_len_v<V>;
        for (std::size_t i = 0; i < L; ++i) f(BitRef<L>(s.v, i), i);
    } else {
        f(s.v, (std::size_t)0);
    }
}

template<class Spec, class F>
inline void visit_elems(const Spec& s, F&& f) {
    using V = typename Spec::val_t;

    if constexpr (is_arr_v<V>) {
        constexpr std::size_t L = static_len_v<V>;
        for (std::size_t i = 0; i < L; ++i) f(s.v[i], i);
    } else if constexpr (is_bs_v<V>) {
        constexpr std::size_t L = static_len_v<V>;
        for (std::size_t i = 0; i < L; ++i) f(s.v.test(i), i);
    } else {
        f(s.v, (std::size_t)0);
    }
}

// ---------------------------------------------
// 互換用：V単位の要素走査（既存のまま残す）
// ---------------------------------------------
template<class V, class F>
inline void for_each_elem(V& v, F&& f) {
    if constexpr (is_arr_v<V>) {
        constexpr std::size_t N = static_len_v<V>;
        for (std::size_t i = 0; i < N; ++i) f(v[i]);
    } else {
        f(v);
    }
}

template<class V, class F>
inline void for_each_elem2(const V& a, V& b, F&& f) {
    if constexpr (is_arr_v<V>) {
        constexpr std::size_t N = static_len_v<V>;
        for (std::size_t i = 0; i < N; ++i) f(a[i], b[i]);
    } else {
        f(a, b);
    }
}

// ---------------------------------------------
// init_default / random
// ---------------------------------------------
template<class Spec>
inline void init_default(Spec& s) {
    using V = typename Spec::val_t;

    if constexpr (is_bs_v<V>) {
        if (static_cast<bool>(def_v<Spec>)) s.v.set();
        else s.v.reset();
    } else {
        using E = elem_of_t<V>;
        const E dv = static_cast<E>(def_v<Spec>);

        if constexpr (is_arr_v<V>) {
            s.v.fill(dv);
        } else {
            s.v = static_cast<V>(dv);
        }
    }
}

// Spec の「要素型」(scalar/array/bitset の要素) を 1つランダム生成する
//    - Spec::random_elem() があればそれを優先（elem 1つ）
//    - なければ bool / enum(options) / integral(lo,hi) / float(lo,hi)
template<class Spec>
inline ga::elem_of_t<typename Spec::val_t> random_elem_value() {
    using V = typename Spec::val_t;
    using E = ga::elem_of_t<V>;

    if constexpr (requires { Spec::random_elem(); }) {
        return static_cast<E>(Spec::random_elem());
    } else if constexpr (std::is_same_v<std::remove_cv_t<E>, bool>) {
        std::bernoulli_distribution d(0.5);
        return static_cast<E>(d(ga::rng));
    } else if constexpr (ga::is_enum_v<Spec>) {
        using Opt = std::remove_cvref_t<decltype(Spec::options)>;
        constexpr std::size_t M = std::tuple_size_v<Opt>;
        static_assert(M > 0, "Spec::options must be non-empty");

        // Spec ごとに M が固定なら static でOK（1回だけ生成）
        static std::uniform_int_distribution<std::size_t> pick(0, M - 1);
        return static_cast<E>(Spec::options[pick(ga::rng)]);
    } else if constexpr (std::is_integral_v<E>) {
        static_assert(requires { Spec::lo; Spec::hi; }, "integral Spec must define lo/hi");
        E lo = static_cast<E>(Spec::lo);
        E hi = static_cast<E>(Spec::hi);
        if (hi < lo) std::swap(lo, hi);

        std::uniform_int_distribution<E> dist(lo, hi);
        return dist(ga::rng);
    } else if constexpr (std::is_floating_point_v<E>) {
        static_assert(requires { Spec::lo; Spec::hi; }, "float Spec must define lo/hi");
        E lo = static_cast<E>(Spec::lo);
        E hi = static_cast<E>(Spec::hi);
        if (hi < lo) std::swap(lo, hi);

        std::uniform_real_distribution<E> dist(lo, hi);
        return dist(ga::rng);
    } else {
        static_assert(ga::dependent_false_v<Spec>, "random_elem_value: unsupported Spec");
        return E{};
    }
}

template<class Spec>
inline void init_random(Spec& s) {
    // bitset を含めて visit_elems で統一
    ga::visit_elems(s, [&](auto elem, std::size_t /*i*/) {
        using ElemT = std::remove_cvref_t<decltype(elem)>;
        if constexpr (std::is_same_v<ElemT, ga::BitRef<ga::static_len_v<typename Spec::val_t>>>) {
            // BitRef は bool 代入対応なので OK
            elem = static_cast<bool>(ga::random_elem_value<Spec>());
        } else {
            using V = typename Spec::val_t;
            using E = ga::elem_of_t<V>;
            elem = static_cast<std::remove_cvref_t<decltype(elem)>>(static_cast<E>(ga::random_elem_value<Spec>()));
        }
    });
}

// Gene(tuple<Spec...>) をデフォルト初期化
template<class Gene, std::size_t... I>
inline void init_gene_default_impl(Gene& g, std::index_sequence<I...>) {
    (init_default(std::get<I>(g)), ...);
}
template<class Gene>
inline void init_gene_default(Gene& g) {
    init_gene_default_impl(g, std::make_index_sequence<std::tuple_size_v<Gene>>{});
}

// Gene をランダム初期化
template<class Gene, std::size_t... I>
inline void init_gene_random_impl(Gene& g, std::index_sequence<I...>) {
    (init_random(std::get<I>(g)), ...);
}
template<class Gene>
inline void init_gene_random(Gene& g) {
    init_gene_random_impl(g, std::make_index_sequence<std::tuple_size_v<Gene>>{});
}

// ---------------------------------------------
// crossover / mutation の適用
// ---------------------------------------------
template<class C> concept HasS = requires { typename C::S; };

template<class Gene, class Methods, std::size_t... I>
constexpr void check_spec_match(std::index_sequence<I...>) {
    static_assert((HasS<std::tuple_element_t<I, Methods>> && ...), "Method must define `using S = ...;`");
    static_assert(
        (std::is_same_v<std::tuple_element_t<I, Gene>, typename std::tuple_element_t<I, Methods>::S> && ...),
        "Gene element type and Method::S do not match"
    );
}

template<class Gene, class Crossovers, std::size_t... I>
void apply_crossover_impl(const Gene& p1, const Gene& p2, Gene& c, const Crossovers& cross, std::index_sequence<I...>) {
    ((std::get<I>(cross).apply(std::get<I>(p1), std::get<I>(p2), std::get<I>(c))), ...);
}

template<class Gene, class Crossovers, std::size_t... I>
void apply_crossover2_impl(const Gene& p1, const Gene& p2, Gene& c1, Gene& c2, const Crossovers& cross, std::index_sequence<I...>) {
    ((std::get<I>(cross).apply(std::get<I>(p1), std::get<I>(p2), std::get<I>(c1), std::get<I>(c2))), ...);
}

template<class Gene, class Crossovers>
void apply_crossover(const Crossovers& crossovers, const Gene& p1, const Gene& p2, Gene& c) {
    static_assert(std::tuple_size_v<Gene> == std::tuple_size_v<Crossovers>, "Gene and Crossovers must have the same tuple size.");
    check_spec_match<Gene, Crossovers>(std::make_index_sequence<std::tuple_size_v<Gene>>{});
    apply_crossover_impl(p1, p2, c, crossovers, std::make_index_sequence<std::tuple_size_v<Gene>>{});
}

template<class Gene, class Crossovers>
void apply_crossover(const Crossovers& crossovers, const Gene& p1, const Gene& p2, Gene& c1, Gene& c2) {
    static_assert(std::tuple_size_v<Gene> == std::tuple_size_v<Crossovers>, "Gene and Crossovers must have the same tuple size.");
    check_spec_match<Gene, Crossovers>(std::make_index_sequence<std::tuple_size_v<Gene>>{});
    apply_crossover2_impl(p1, p2, c1, c2, crossovers, std::make_index_sequence<std::tuple_size_v<Gene>>{});
}

template<class Gene, class Mutations, std::size_t... I>
void apply_mutation_impl(const Gene& p, Gene& c, const Mutations& muts, std::index_sequence<I...>) {
    ((std::get<I>(muts).apply(std::get<I>(p), std::get<I>(c))), ...);
}

template<class Gene, class Mutations>
void apply_mutation(const Mutations& muts, const Gene& p, Gene& c) {
    static_assert(std::tuple_size_v<Gene> == std::tuple_size_v<Mutations>, "Gene and Mutations must have the same tuple size.");
    check_spec_match<Gene, Mutations>(std::make_index_sequence<std::tuple_size_v<Gene>>{});
    apply_mutation_impl(p, c, muts, std::make_index_sequence<std::tuple_size_v<Gene>>{});
}

using Fitness = double;

// ---------------------------------------------
// Diversity（世代全体の多様性）
// ---------------------------------------------
template<class Gene>
struct Diversity {
    float diversity_rate = 0.0f;

    void reset() {
        diversity_rate = 0.0f;
        std::apply([](auto&... d) { (d.clear(), ...); }, spec_div_);
    }

    void rebuild(const std::vector<Gene>& genes) {
        reset();
        for (const auto& g : genes) update(g, +1);
    }

    void update(const Gene& g, int delta) {
        update_impl_(g, delta, std::make_index_sequence<GeneN_>{});
    }

private:
    static constexpr size_t GeneN_ = std::tuple_size_v<Gene>;

    template<size_t I>
    struct SpecDiv {
        using Spec = std::tuple_element_t<I, Gene>;
        using V = typename Spec::val_t;
        using E = ga::elem_of_t<V>;

        static constexpr size_t len = ga::static_len_v<V>;
        static constexpr size_t nb  = ga::n_bins_v<Spec>;

        static_assert(len > 0);
        static_assert(nb > 0);

        static constexpr float w = 1.0f / float(GeneN_) / float(len) / float(nb);

        std::array<int, len * nb> cnt{};
        std::array<int, len> present{};

        void clear() { cnt.fill(0); present.fill(0); }

        static size_t enum_index(E x) {
            if constexpr (requires { Spec::option_index(x); }) {
                return (size_t)Spec::option_index(x);
            } else {
                constexpr auto& opt = Spec::options;
                constexpr size_t M = std::tuple_size_v<std::remove_cvref_t<decltype(opt)>>;
                for (size_t i = 0; i < M; ++i) if (opt[i] == x) return i;
                return 0;
            }
        }

        static size_t bin_of(E x) {
            if constexpr (ga::is_enum_v<Spec>) {
                size_t idx = enum_index(x);
                if (idx >= nb) idx = nb - 1;
                return idx;
            } else if constexpr (std::is_same_v<std::remove_cv_t<E>, bool>) {
                return x ? 1u : 0u;
            } else if constexpr (std::is_integral_v<E> || std::is_floating_point_v<E>) {
                static_assert(requires { Spec::lo; Spec::hi; }, "numeric Spec must define lo/hi");
                const double lo = (double)Spec::lo;
                const double hi = (double)Spec::hi;
                if (!(hi > lo)) return 0;

                double t = ((double)x - lo) / (hi - lo);
                if (t <= 0.0) return 0;
                if (t >= 1.0) return nb - 1;

                size_t b = (size_t)(t * (double)nb);
                if (b >= nb) b = nb - 1;
                return b;
            } else {
                static_assert(ga::dependent_false_v<Spec>, "bin_of: unsupported Spec/elem type");
                return 0;
            }
        }
    };

    template<size_t... I>
    static auto make_spec_div_tuple_(std::index_sequence<I...>) {
        return std::tuple<SpecDiv<I>...>{};
    }
    decltype(make_spec_div_tuple_(std::make_index_sequence<GeneN_>{})) spec_div_{};

    template<size_t I>
    void update_one_spec_(const typename std::tuple_element_t<I, Gene>& s, int delta) {
        using D = SpecDiv<I>;
        using V = typename D::V;
        using E = typename D::E;

        auto& d = std::get<I>(spec_div_);
        constexpr size_t nb = D::nb;

        auto touch = [&](size_t elem_i, size_t bin, int dd) {
            int& c = d.cnt[elem_i * nb + bin];
            if (dd > 0) {
                if (c++ == 0) {
                    d.present[elem_i]++;
                    assert(d.present[elem_i] >= 0 && d.present[elem_i] <= (int)nb);
                    diversity_rate += D::w;
                }
            } else {
                assert(c > 0);
                if (--c == 0) {
                    d.present[elem_i]--;
                    assert(d.present[elem_i] >= 0 && d.present[elem_i] <= (int)nb);
                    diversity_rate -= D::w;
                }
            }
        };

        // Spec 単位で統一（const版 visit_elems）
        ga::visit_elems(s, [&](auto elem, std::size_t i) {
            if constexpr (ga::is_bs_v<V>) {
                const bool bit = (bool)elem;
                touch(i, D::bin_of(static_cast<E>(bit)), delta);
            } else {
                touch(i, D::bin_of(static_cast<E>(elem)), delta);
            }
        });
    }

    template<size_t... I>
    void update_impl_(const Gene& g, int delta, std::index_sequence<I...>) {
        (update_one_spec_<I>(std::get<I>(g), delta), ...);
    }
};

template<class Gene>
struct Population {
    vector<Gene> gene;
    vector<Fitness> fitness;
    vector<int> age;

    Diversity<Gene> diversity;

    bool better(int a, int b) const {
        if (fitness[a] != fitness[b]) return fitness[a] > fitness[b];
        if (age[a] != age[b]) return age[a] < age[b];
        return a < b;
    }

    void set(int index, const Gene& g, Fitness f, int a = 0) { set_impl_(index, g, f, a); }
    void set(int index, Gene&& g,       Fitness f, int a = 0) { set_impl_(index, std::move(g), f, a); }

    int add(const Gene& g, Fitness f, int a = 0) { return add_impl_(g, f, a); }
    int add(Gene&& g,      Fitness f, int a = 0) { return add_impl_(std::move(g), f, a); }

    void set_fitness(int index, Fitness f) {
        const auto n = gene.size();
        assert(0 <= index && (size_t)index < n);
        sync_sizes_();
        fitness[index] = f;
    }

    void increment_age(int increase = 1) {
        if (increase == 0) return;
        sync_sizes_();
        for (auto& x : age) x += increase;
    }

    void rebuild_diversity() {
        diversity.rebuild(gene);
    }

    template <class F>
    void apply_rolling_horizon(F&& callback, int shift = 1) {
        if (shift < 0) shift = 0;
        if (gene.empty()) { increment_age(1); return; }

        for (auto& g : gene) {
            diversity.update(g, -1);
            apply_rolling_horizon_gene_(g, shift);
            callback(g, shift);
            diversity.update(g, +1);
        }

        increment_age(1);
    }

private:
    void sync_sizes_() {
        const auto n = gene.size();
        if (fitness.size() != n) fitness.resize(n, Fitness{});
        if (age.size() != n) age.resize(n, 0);
    }

    template<class G>
    void set_impl_(int index, G&& g, Fitness f, int a) {
        const auto n = gene.size();
        assert(0 <= index && (size_t)index < n);

        diversity.update(gene[index], -1);
        gene[index] = std::forward<G>(g);
        diversity.update(gene[index], +1);

        sync_sizes_();
        fitness[index] = f;
        age[index] = a;
    }

    template<class G>
    int add_impl_(G&& g, Fitness f, int a) {
        const int idx = (int)gene.size();
        gene.push_back(std::forward<G>(g));

        diversity.update(gene[idx], +1);

        sync_sizes_();
        fitness[idx] = f;
        age[idx] = a;
        return idx;
    }

    template<std::size_t... I>
    static void apply_rolling_horizon_gene_impl_(Gene& g, int shift, std::index_sequence<I...>) {
        (apply_rolling_horizon_spec_<std::tuple_element_t<I, Gene>>(std::get<I>(g), shift), ...);
    }

    static void apply_rolling_horizon_gene_(Gene& g, int shift) {
        apply_rolling_horizon_gene_impl_(g, shift, std::make_index_sequence<std::tuple_size_v<Gene>>{});
    }

    template<class Spec>
    static void apply_rolling_horizon_spec_(Spec& s, int shift) {
        using V = typename Spec::val_t;

        if constexpr (!(requires { Spec::is_horizon; } && (bool)Spec::is_horizon)) {
            return;
        } else {
            if constexpr (ga::is_arr_v<V>) {
                using E = typename V::value_type;

                constexpr std::size_t L = ga::static_len_v<V>;
                std::size_t k = (std::size_t)shift;
                const E fill = static_cast<E>(ga::def_v<Spec>);

                if (k >= L) { s.v.fill(fill); return; }
                if (k == 0) return;

                if constexpr (std::is_trivially_copyable_v<E>) {
                    std::memmove(s.v.data(), s.v.data() + k, (L - k) * sizeof(E));
                    std::fill(s.v.begin() + (L - k), s.v.end(), fill);
                } else {
                    for (std::size_t i = 0; i + k < L; ++i) s.v[i] = s.v[i + k];
                    for (std::size_t i = L - k; i < L; ++i) s.v[i] = fill;
                }
            } else if constexpr (ga::is_bs_v<V>) {
                constexpr std::size_t L = ga::static_len_v<V>;
                std::size_t k = (std::size_t)shift;
                const bool fill = static_cast<bool>(ga::def_v<Spec>);

                if (k >= L) { if (fill) s.v.set(); else s.v.reset(); return; }
                if (k == 0) return;

                for (std::size_t i = 0; i + k < L; ++i) s.v.set(i, s.v.test(i + k));
                for (std::size_t i = L - k; i < L; ++i) s.v.set(i, fill);
            } else {
                static_assert(ga::dependent_false_v<Spec>, "is_horizon=true requires val_t to be std::array or std::bitset");
            }
        }
    }
};

} // namespace ga

// --------- Operations ---------
template<class Spec>
struct OnePointCrossover {
    using S = Spec;
    using V = typename Spec::val_t;
    static constexpr std::size_t L = ga::static_len_v<V>;

    static_assert((ga::is_arr_v<V> && std::is_trivially_copyable_v<V>) || ga::is_bs_v<V>);

    void apply(const Spec& p1, const Spec& p2, Spec& c) const requires (ga::is_arr_v<V>) {
        std::uniform_int_distribution<std::size_t> cut_dist(0, L);
        const std::size_t k = cut_dist(ga::rng);

        std::memcpy(c.v.data(),     p1.v.data(),     k * sizeof(typename V::value_type));
        std::memcpy(c.v.data() + k, p2.v.data() + k, (L - k) * sizeof(typename V::value_type));
    }

    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const requires (ga::is_arr_v<V>) {
        std::uniform_int_distribution<std::size_t> cut_dist(0, L);
        const std::size_t k = cut_dist(ga::rng);

        std::memcpy(c1.v.data(),     p1.v.data(),     k * sizeof(typename V::value_type));
        std::memcpy(c1.v.data() + k, p2.v.data() + k, (L - k) * sizeof(typename V::value_type));

        std::memcpy(c2.v.data(),     p2.v.data(),     k * sizeof(typename V::value_type));
        std::memcpy(c2.v.data() + k, p1.v.data() + k, (L - k) * sizeof(typename V::value_type));
    }

    template<std::size_t N>
    struct CrossoverMasks {
        std::array<std::bitset<N>, N + 1> low{};
        std::array<std::bitset<N>, N + 1> high{};

        CrossoverMasks() {
            for (std::size_t i = 0; i <= N; ++i) {
                low[i]  = ga::low_mask<N>(i);
                high[i] = ~low[i];
            }
        }
    };

    void apply(const Spec& p1, const Spec& p2, Spec& c) const requires (ga::is_bs_v<V>) {
        static const CrossoverMasks<L> masks{};
        std::uniform_int_distribution<std::size_t> cut_dist(0, L);
        const std::size_t k = cut_dist(ga::rng);

        c.v = (p1.v & masks.low[k]) | (p2.v & masks.high[k]);
    }

    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const requires (ga::is_bs_v<V>) {
        static const CrossoverMasks<L> masks{};
        std::uniform_int_distribution<std::size_t> cut_dist(0, L);
        const std::size_t k = cut_dist(ga::rng);

        const auto& lo = masks.low[k];
        const auto& hi = masks.high[k];

        c1.v = (p1.v & lo) | (p2.v & hi);
        c2.v = (p2.v & lo) | (p1.v & hi);
    }
};

template<class Spec>
struct ArithmeticCrossover {
    using S = Spec;
    using V = typename Spec::val_t;
    using E = ga::elem_of_t<V>;

    static_assert(!ga::is_bs_v<V>, "ArithmeticCrossover does not support bitset");
    static_assert(std::is_integral_v<E> || std::is_floating_point_v<E>, "ArithmeticCrossover requires numeric element type");

    double alpha = -1.0;

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
            long long y = llround(x);
            E r = static_cast<E>(y);
            return clamp_(r);
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

template<class Spec>
struct RandomResetMutation {
    using S = Spec;
    using V = typename Spec::val_t;
    using E = ga::elem_of_t<V>;

    static_assert(requires { ga::random_elem_value<Spec>(); },
                  "RandomResetMutation requires ga::random_elem_value<Spec>() to be available");

    double p_reset = 0.01;

    void apply(const Spec& p, Spec& c) const {
        c.v = p.v;

        std::bernoulli_distribution do_reset(p_reset);

        // visit_elems で bitset を含め統一
        ga::visit_elems(c, [&](auto elem, std::size_t /*i*/) {
            if (do_reset(ga::rng)) {
                elem = ga::random_elem_value<Spec>();
                (void)elem;
            }
        });
    }
};

template<class Gene>
struct TournamentSelection {
    int k = 3;

    void prepare(const ga::Population<Gene>& /*pop*/) const {}

    int apply(const ga::Population<Gene>& pop) const {
        const int n = (int)pop.gene.size();
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

template<class Gene>
struct ReverseTournamentSelection {
    int k = 3;

    void prepare(const ga::Population<Gene>& /*pop*/) const {}

    int apply(const ga::Population<Gene>& pop) const {
        const int n = (int)pop.gene.size();
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

template<class Gene>
struct WorstSelection {
    void prepare(const ga::Population<Gene>& /*pop*/) const {}

    int apply(const ga::Population<Gene>& pop) const {
        const int n = (int)pop.gene.size();
        if (n <= 0) return -1;

        int worst = 0;
        for (int i = 1; i < n; ++i) {
            if (pop.better(worst, i)) worst = i;
        }
        return worst;
    }
};

// --------- Spec Samples ---------
struct Float {
    using elem_t = float;
    static constexpr elem_t hi = 100.0;
    static constexpr elem_t lo = 0.0;
    static constexpr size_t n_bins = 5;
    static constexpr elem_t def = 0.0;
    using val_t = elem_t;
    val_t v = def;
};

struct FloatArray {
    using elem_t = float;
    static constexpr elem_t hi = 100.0;
    static constexpr elem_t lo = 0.0;
    static constexpr size_t n_bins = 5;
    static constexpr elem_t def = 0.0;
    using val_t = std::array<elem_t, 10>;
    val_t v = ga::init_array<val_t>(def);
};

struct Int {
    using elem_t = int;
    static constexpr elem_t hi = 100;
    static constexpr elem_t lo = 0;
    static constexpr size_t n_bins = 5;
    static constexpr elem_t def = 0;
    using val_t = elem_t;
    val_t v = def;
};

struct IntArray {
    using elem_t = int;
    static constexpr elem_t hi = 100;
    static constexpr elem_t lo = 0;
    static constexpr size_t n_bins = 5;
    static constexpr elem_t def = 0;
    using val_t = std::array<elem_t, 10>;
    val_t v = ga::init_array<val_t>(def);
};

struct Bool {
    using elem_t = bool;
    static constexpr elem_t def = false;
    using val_t = elem_t;
    val_t v = def;

    elem_t random_elem() {
        std::bernoulli_distribution d(0.6);
        return d(ga::rng);
    }
};

struct Bitset {
    using elem_t = bool;
    static constexpr elem_t def = false;
    using val_t = std::bitset<10>;
    val_t v = ga::init_bs<val_t>(def);
};

struct Enum {
    using elem_t = int;
    static constexpr elem_t def = 1;
    static constexpr auto options = std::to_array<elem_t>({1, 2, 3, 4});
    static constexpr size_t option_index(int x) {
        constexpr array<int, 5> dic = { 0, 0, 1, 2, 3 };
        return (size_t)x >= dic.size() ? 0 : dic[x];
    }
    using val_t = elem_t;
    val_t v = def;
};

struct EnumArray {
    using elem_t = int;
    static constexpr elem_t def = 1;
    static constexpr auto options = std::to_array<elem_t>({1, 2, 3, 4});
    static constexpr size_t option_index(int x) {
        constexpr array<int, 5> dic = { 0, 0, 1, 2, 3 };
        return (size_t)x >= dic.size() ? 0 : dic[x];
    }
    using val_t = std::array<elem_t, 10>;
    val_t v = ga::init_array<val_t>(def);
};

struct IntSpec {
    using elem_t = int;
    using val_t  = int;
    static constexpr int lo = 0;
    static constexpr int hi = 100;
    static constexpr int def = 0;
    int v = def;
};


int main() {
    {
        using Gene = std::tuple<IntSpec, IntSpec>;
        Gene p{IntSpec{10}, IntSpec{20}};
        Gene c = p; // まずコピーしてから mutate でもOK

        auto muts = std::tuple{
            RandomResetMutation<IntSpec>{.p_reset = 0.10},
            RandomResetMutation<IntSpec>{.p_reset = 0.01}
        };

        ga::apply_mutation(muts, p, c);
    }

    {
        using Gene = std::tuple<IntArray, IntArray>;
        Gene p1{IntArray{}, IntArray{}};
        Gene p2{IntArray{}, IntArray{}};
        Gene c;
        auto xo = std::tuple{
            OnePointCrossover<IntArray>{},
            OnePointCrossover<IntArray>{},
        };

        ga::apply_crossover(xo, p1, p2, c);
    }
}

// int main(){

//     return 0;
// }