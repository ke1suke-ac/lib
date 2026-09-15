
#include <array>
#include <bitset>
#include <tuple>
#include <span>
#include <random>
#include <algorithm>
#include <limits>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <utility>

#define main ga__header_main

#include <bits/stdc++.h>

namespace ga {

inline std::mt19937 rng(42);

inline double now_ms() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double, std::milli>(clock::now().time_since_epoch()).count();
}

struct Stats {
    std::uint64_t population_set = 0;
    std::uint64_t population_add = 0;
    std::uint64_t population_update = 0;
    std::uint64_t population_rebuild_diversity = 0;
    std::uint64_t population_apply_rolling_horizon = 0;

    std::uint64_t apply_crossover = 0;
    std::uint64_t apply_mutation = 0;

    void reset() { *this = Stats{}; }

    void dump_stderr(const char* name = "ga::stats") const {
        std::fprintf(
            stderr,
            "%s {\n"
            "  population_set                 : %llu\n"
            "  population_add                 : %llu\n"
            "  population_update              : %llu\n"
            "  population_rebuild_diversity   : %llu\n"
            "  population_apply_rolling_horizon: %llu\n"
            "  apply_crossover                : %llu\n"
            "  apply_mutation                 : %llu\n"
            "}\n",
            name,
            (unsigned long long)population_set,
            (unsigned long long)population_add,
            (unsigned long long)population_update,
            (unsigned long long)population_rebuild_diversity,
            (unsigned long long)population_apply_rolling_horizon,
            (unsigned long long)apply_crossover,
            (unsigned long long)apply_mutation
        );
    }
};

inline Stats stats{};

inline void reset_stats() { stats.reset(); }
inline void debug_print_stats() { stats.dump_stderr(); }

template<class Arr>
constexpr Arr init_array(typename Arr::value_type init) {
    Arr a{};
    a.fill(init);
    return a;
}

template<class BS>
constexpr BS init_bs(bool on) {
    BS a{};
    if (on) a.set();
    return a;
}

template<class V> inline constexpr bool is_arr_v = false;
template<class T, std::size_t N> inline constexpr bool is_arr_v<std::array<T, N>> = true;

template<class V> inline constexpr bool is_bs_v = false;
template<std::size_t N> inline constexpr bool is_bs_v<std::bitset<N>> = true;

template<class Spec>
inline constexpr bool is_perm_v = []() constexpr {
    if constexpr (requires { typename Spec::val_t; } && ga::is_arr_v<typename Spec::val_t>) {
        using V = typename Spec::val_t;
        using E = typename V::value_type;

        if constexpr (std::is_integral_v<E> &&
                      requires { { Spec::is_perm } -> std::convertible_to<bool>; }) {
            return static_cast<bool>(Spec::is_perm);
        }
    }
    return false;
}();

template<class Arr>
constexpr Arr init_perm() {
    static_assert(ga::is_arr_v<Arr>, "init_perm: Arr must be std::array");
    using E = typename Arr::value_type;
    static_assert(std::is_integral_v<E>, "init_perm: Arr::value_type must be integral");
    Arr a{};
    for (std::size_t i = 0; i < a.size(); ++i) a[i] = static_cast<E>(i);
    return a;
}

template<class V> struct static_len : std::integral_constant<std::size_t, 1> {};
template<class T, std::size_t N> struct static_len<std::array<T, N>> : std::integral_constant<std::size_t, N> {};
template<std::size_t N> struct static_len<std::bitset<N>> : std::integral_constant<std::size_t, N> {};
template<class V> inline constexpr std::size_t static_len_v = static_len<V>::value;

template<class V> struct elem_of { using type = V; };
template<class T, std::size_t N> struct elem_of<std::array<T, N>> { using type = T; };
template<std::size_t N> struct elem_of<std::bitset<N>> { using type = bool; };
template<class V> using elem_of_t = typename elem_of<V>::type;

template<class> inline constexpr bool dependent_false_v = false;

template<class S>
using spec_elem_t = ga::elem_of_t<typename S::val_t>;

template<class T>
inline constexpr bool is_enum_v = requires {
    T::options;
    requires ga::is_arr_v<std::remove_cvref_t<decltype(T::options)>>;
};

template<class Spec>
inline constexpr std::size_t n_options_v = []() constexpr -> std::size_t {
    if constexpr (ga::is_enum_v<Spec>) {
        return std::tuple_size_v<std::remove_cvref_t<decltype(Spec::options)>>;
    } else {
        return 0;
    }
}();

template<class S>
concept BasicSpecLike =
    requires { typename S::val_t; } &&
    std::default_initializable<S> &&
    requires(S s) {
        { s.v } -> std::same_as<typename S::val_t&>;
    } &&
    (!requires { S::def; } || std::same_as<std::remove_cvref_t<decltype(S::def)>, spec_elem_t<S>>) &&
    (!requires { S::lo;  } || std::same_as<std::remove_cvref_t<decltype(S::lo )>, spec_elem_t<S>>) &&
    (!requires { S::hi;  } || std::same_as<std::remove_cvref_t<decltype(S::hi )>, spec_elem_t<S>>) &&
    (((requires { S::lo; } && requires { S::hi; }) || (!requires { S::lo; } && !requires { S::hi; }))) &&
    (!requires { S::options; } ||
        (ga::is_arr_v<std::remove_cvref_t<decltype(S::options)>> &&
         std::same_as<typename std::remove_cvref_t<decltype(S::options)>::value_type, spec_elem_t<S>> &&
         (ga::n_options_v<S> > 0)));

template<BasicSpecLike Spec>
inline constexpr ga::spec_elem_t<Spec> def_v = []() constexpr {
    using E = ga::spec_elem_t<Spec>;
    if constexpr (requires { Spec::def; }) {
        return static_cast<E>(Spec::def);
    } else {
        return E{};
    }
}();

template<class T>
concept NBinSpec = BasicSpecLike<T> && (
    std::same_as<std::remove_cv_t<spec_elem_t<T>>, bool> ||
    ga::is_enum_v<T> ||
    ga::is_perm_v<T> ||
    requires { T::n_bins; }
);

template<NBinSpec T>
inline constexpr std::size_t n_bins_v = []() constexpr -> std::size_t {
    using V = typename T::val_t;
    using E = std::remove_cv_t<spec_elem_t<T>>;

    if constexpr (std::is_same_v<E, bool>) {
        return 2;
    } else if constexpr (ga::is_enum_v<T>) {
        return ga::n_options_v<T>;
    } else if constexpr (ga::is_perm_v<T>) {

        return std::min<std::size_t>(ga::static_len_v<V>, 10u);
    } else {
        return static_cast<std::size_t>(T::n_bins);
    }
}();

template<class Spec>
concept NumericRangeSpec = BasicSpecLike<Spec> &&
    (std::is_integral_v<std::remove_cv_t<spec_elem_t<Spec>>> ||
     std::is_floating_point_v<std::remove_cv_t<spec_elem_t<Spec>>>) &&
    (!std::is_same_v<std::remove_cv_t<spec_elem_t<Spec>>, bool>) &&
    (!ga::is_enum_v<Spec>) &&
    (!ga::is_perm_v<Spec>);

template<class Spec>
concept RandomElemValueSpec = BasicSpecLike<Spec> && (
    std::same_as<std::remove_cv_t<spec_elem_t<Spec>>, bool> ||
    ga::is_enum_v<Spec> ||
    (NumericRangeSpec<Spec> && requires { Spec::lo; Spec::hi; }) ||
    (!NumericRangeSpec<Spec> && requires { Spec::random_elem(); })
);

template<RandomElemValueSpec Spec>
ga::spec_elem_t<Spec> random_elem_value() {
    using E = ga::spec_elem_t<Spec>;

    if constexpr (requires { Spec::random_elem(); }) {
        return static_cast<E>(Spec::random_elem());
    } else if constexpr (std::is_same_v<std::remove_cv_t<E>, bool>) {
        static std::bernoulli_distribution d(0.5);
        return static_cast<E>(d(ga::rng));
    } else if constexpr (ga::is_enum_v<Spec>) {
        constexpr std::size_t M = ga::n_options_v<Spec>;
        static_assert(M > 0, "Spec::options must be non-empty");
        static std::uniform_int_distribution<std::size_t> pick(0, M - 1);
        return static_cast<E>(Spec::options[pick(ga::rng)]);
    } else if constexpr (std::is_integral_v<E>) {
        static const std::pair<E, E> range = []() {
            const E lo0 = static_cast<E>(Spec::lo);
            const E hi0 = static_cast<E>(Spec::hi);
            return (hi0 < lo0) ? std::pair<E, E>{hi0, lo0} : std::pair<E, E>{lo0, hi0};
        }();
        static std::uniform_int_distribution<E> dist(range.first, range.second);
        return dist(ga::rng);
    } else if constexpr (std::is_floating_point_v<E>) {
        static const std::pair<E, E> range = []() {
            const E lo0 = static_cast<E>(Spec::lo);
            const E hi0 = static_cast<E>(Spec::hi);
            return (hi0 < lo0) ? std::pair<E, E>{hi0, lo0} : std::pair<E, E>{lo0, hi0};
        }();
        static std::uniform_real_distribution<E> dist(range.first, range.second);
        return dist(ga::rng);
    } else {
        static_assert(ga::dependent_false_v<Spec>, "random_elem_value: unsupported Spec");
        return E{};
    }
}

template<class S>
concept SpecLike = BasicSpecLike<S> && NBinSpec<S> && (ga::is_perm_v<S> || RandomElemValueSpec<S>);

template<class Gene>
concept GeneLike =
    requires { typename std::tuple_size<std::remove_cvref_t<Gene>>::type; } &&
    []<std::size_t... I>(std::index_sequence<I...>) constexpr {
        return (SpecLike<std::tuple_element_t<I, std::remove_cvref_t<Gene>>> && ...);
    }(std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<Gene>>>{});

template<std::size_t N>
std::bitset<N> low_mask(std::size_t k) {
    if (k >= N) {
        std::bitset<N> m;
        m.set();
        return m;
    }
    if (k == 0) return std::bitset<N>{};

    std::bitset<N> m;
    m.set();
    m >>= (N - k);
    return m;
}

template<SpecLike Spec, class F>
void visit_elems_ref(Spec& s, F&& f) {
    using V = typename Spec::val_t;

    if constexpr (ga::is_arr_v<V>) {
        constexpr std::size_t L = ga::static_len_v<V>;
        for (std::size_t i = 0; i < L; ++i) f(s.v[i], i);
    } else if constexpr (ga::is_bs_v<V>) {
        constexpr std::size_t L = ga::static_len_v<V>;
        for (std::size_t i = 0; i < L; ++i) f(s.v[i], i);
    } else {
        f(s.v, std::size_t{0});
    }
}

template<SpecLike Spec, class F>
void visit_elems_val(const Spec& s, F&& f) {
    using V = typename Spec::val_t;

    if constexpr (ga::is_arr_v<V>) {
        constexpr std::size_t L = ga::static_len_v<V>;
        for (std::size_t i = 0; i < L; ++i) f(s.v[i], i);
    } else if constexpr (ga::is_bs_v<V>) {
        constexpr std::size_t L = ga::static_len_v<V>;
        for (std::size_t i = 0; i < L; ++i) f(s.v.test(i), i);
    } else {
        f(s.v, std::size_t{0});
    }
}

template<SpecLike Spec>
void init_default(Spec& s) {
    using V = typename Spec::val_t;

    if constexpr (ga::is_perm_v<Spec>) {
        s.v = ga::init_perm<V>();
    } else if constexpr (ga::is_bs_v<V>) {
        if (static_cast<bool>(ga::def_v<Spec>)) s.v.set();
        else s.v.reset();
    } else {
        using E = ga::elem_of_t<V>;
        const E dv = static_cast<E>(ga::def_v<Spec>);

        if constexpr (ga::is_arr_v<V>) {
            s.v.fill(dv);
        } else {
            s.v = static_cast<V>(dv);
        }
    }
}

template<SpecLike Spec>
void init_random(Spec& s) {
    using V = typename Spec::val_t;

    if constexpr (ga::is_perm_v<Spec>) {

        s.v = ga::init_perm<V>();
        std::shuffle(s.v.begin(), s.v.end(), ga::rng);
    } else if constexpr (ga::is_bs_v<V>) {

        constexpr std::size_t L = ga::static_len_v<V>;
        std::size_t i = 0;
        while (i < L) {
            const std::uint_fast32_t r = ga::rng();
            const std::size_t chunk = std::min<std::size_t>(32, L - i);
            for (std::size_t b = 0; b < chunk; ++b) {
                s.v.set(i + b, ((r >> b) & 1u) != 0u);
            }
            i += chunk;
        }
    } else {
        ga::visit_elems_ref(s, [&](auto&& elem, std::size_t ) {
            elem = ga::random_elem_value<Spec>();
        });
    }
}

namespace detail {

template<class Tuple, class F, std::size_t... I>
constexpr void for_each_indexed_impl(Tuple&& t, F&& f, std::index_sequence<I...>) {
    (f(std::integral_constant<std::size_t, I>{}, std::get<I>(std::forward<Tuple>(t))), ...);
}

template<class Tuple, class F>
constexpr void for_each_indexed(Tuple&& t, F&& f) {
    using Tup = std::remove_reference_t<Tuple>;
    constexpr std::size_t N = std::tuple_size_v<Tup>;
    for_each_indexed_impl(std::forward<Tuple>(t), std::forward<F>(f), std::make_index_sequence<N>{});
}

}

template<GeneLike Gene>
void init_gene_default(Gene& g) {
    ga::detail::for_each_indexed(g, [&](auto , auto& spec) {
        ga::init_default(spec);
    });
}

template<GeneLike Gene>
void init_gene_random(Gene& g) {
    ga::detail::for_each_indexed(g, [&](auto , auto& spec) {
        ga::init_random(spec);
    });
}

template<class C>
concept HasS = requires { typename C::S; };

template<GeneLike Gene, class Methods, std::size_t... I>
constexpr void check_spec_match(std::index_sequence<I...>) {
    static_assert((HasS<std::tuple_element_t<I, Methods>> && ...), "Method must define `using S = ...;`");
    static_assert(
        (std::is_same_v<std::tuple_element_t<I, Gene>, typename std::tuple_element_t<I, Methods>::S> && ...),
        "Gene element type and Method::S do not match"
    );
}

template<GeneLike Gene, class Crossovers>
void apply_crossover(const Crossovers& crossovers, const Gene& p1, const Gene& p2, Gene& c) {
    static_assert(std::tuple_size_v<Gene> == std::tuple_size_v<Crossovers>, "Gene and Crossovers must have the same tuple size.");
    check_spec_match<Gene, Crossovers>(std::make_index_sequence<std::tuple_size_v<Gene>>{});
    ++ga::stats.apply_crossover;

    ga::detail::for_each_indexed(c, [&](auto idx_c, auto& cs) {
        constexpr std::size_t i = decltype(idx_c)::value;
        std::get<i>(crossovers).apply(std::get<i>(p1), std::get<i>(p2), cs);
    });
}

template<GeneLike Gene, class Crossovers>
void apply_crossover(const Crossovers& crossovers, const Gene& p1, const Gene& p2, Gene& c1, Gene& c2) {
    static_assert(std::tuple_size_v<Gene> == std::tuple_size_v<Crossovers>, "Gene and Crossovers must have the same tuple size.");
    check_spec_match<Gene, Crossovers>(std::make_index_sequence<std::tuple_size_v<Gene>>{});
    ++ga::stats.apply_crossover;

    ga::detail::for_each_indexed(c1, [&](auto idx_c, auto& ) {
        constexpr std::size_t i = decltype(idx_c)::value;
        std::get<i>(crossovers).apply(std::get<i>(p1), std::get<i>(p2), std::get<i>(c1), std::get<i>(c2));
    });
}

template<GeneLike Gene, class Mutations>
void apply_mutation(const Mutations& muts, Gene& g) {
    static_assert(std::tuple_size_v<Gene> == std::tuple_size_v<Mutations>, "Gene and Mutations must have the same tuple size.");
    check_spec_match<Gene, Mutations>(std::make_index_sequence<std::tuple_size_v<Gene>>{});
    ++ga::stats.apply_mutation;

    ga::detail::for_each_indexed(g, [&](auto idx_c, auto& spec) {
        constexpr std::size_t i = decltype(idx_c)::value;
        std::get<i>(muts).apply(spec);
    });
}

using Fitness = double;

template<GeneLike Gene>
struct Diversity {
    double diversity_rate = 0.0;

    void reset() {
        diversity_rate = 0.0;
        std::apply([](auto&... d) { (d.clear(), ...); }, spec_div_);
    }

    void rebuild(const std::vector<Gene>& genes) {
        reset();
        for (const auto& g : genes) update(g, +1);
    }

    void update(const Gene& g, int delta) {
        assert(delta == 1 || delta == -1);
        update_impl_(g, delta, std::make_index_sequence<GeneN_>{});
    }

private:
    static constexpr std::size_t GeneN_ = std::tuple_size_v<Gene>;

    template<std::size_t I>
    struct SpecDiv {
        using Spec = std::tuple_element_t<I, Gene>;
        using V = typename Spec::val_t;
        using E = ga::elem_of_t<V>;

        static constexpr std::size_t len = ga::static_len_v<V>;
        static constexpr std::size_t nb  = ga::n_bins_v<Spec>;

        static_assert(len > 0);
        static_assert(nb > 0);

        static constexpr double w = 1.0 / (double)GeneN_ / (double)len;

        static constexpr double inv_nb_minus_1 = (nb > 1) ? (1.0 / (double)(nb - 1)) : 0.0;

        static double score(int n_present) {

            if constexpr (nb <= 1) return 0.0;
            if (n_present <= 1) return 0.0;
            return (double)(n_present - 1) * inv_nb_minus_1;
        }

        std::array<int, len * nb> cnt{};
        std::array<int, len> present{};

        void clear() { cnt.fill(0); present.fill(0); }

        static std::size_t enum_index(E x) {
            if constexpr (requires { Spec::option_index(x); }) {
                return static_cast<std::size_t>(Spec::option_index(x));
            } else {
                constexpr auto& opt = Spec::options;
                constexpr std::size_t M = ga::n_options_v<Spec>;
                for (std::size_t i = 0; i < M; ++i) if (opt[i] == x) return i;
                return 0;
            }
        }

        struct NumCache {
            double lo = 0.0;
            double nb_d = 0.0;
            double scale = 0.0;
            bool valid = false;
        };

        static inline constexpr NumCache num_cache = []() constexpr {
            NumCache c{};
            if constexpr (ga::NumericRangeSpec<Spec>) {
                double lo = static_cast<double>(Spec::lo);
                double hi = static_cast<double>(Spec::hi);
                if (hi < lo) {
                    const double tmp = lo;
                    lo = hi;
                    hi = tmp;
                }
                const double span = hi - lo;

                c.lo = lo;
                c.nb_d = static_cast<double>(nb);
                c.valid = span > 0.0;
                c.scale = c.valid ? (c.nb_d / span) : 0.0;
            }
            return c;
        }();

        static std::size_t bin_of(E x) {
            if constexpr (ga::is_enum_v<Spec>) {
                std::size_t idx = enum_index(x);
                if (idx >= nb) idx = nb - 1;
                return idx;
            } else if constexpr (std::is_same_v<std::remove_cv_t<E>, bool>) {
                return x ? 1u : 0u;
            } else if constexpr (ga::is_perm_v<Spec>) {

                if constexpr (std::is_signed_v<E>) {
                    if (x < 0) return 0;
                }
                return static_cast<std::size_t>(x) % nb;
            } else if constexpr (ga::NumericRangeSpec<Spec>) {

                if constexpr (nb <= 1) return 0;

                const double xv = static_cast<double>(x);
                if constexpr (std::is_floating_point_v<E>) {
                    if (!std::isfinite(xv)) return 0;
                }
                if (!num_cache.valid) return 0;

                const double pos = (xv - num_cache.lo) * num_cache.scale;

                if (pos <= 0.0) return 0;
                if (pos >= num_cache.nb_d) return nb - 1;
                return static_cast<std::size_t>(pos);
            } else {
                static_assert(ga::dependent_false_v<Spec>, "bin_of: unsupported Spec/elem type");
                return 0;
            }
        }
    };

    template<std::size_t... I>
    static auto make_spec_div_tuple_(std::index_sequence<I...>) {
        return std::tuple<SpecDiv<I>...>{};
    }
    decltype(make_spec_div_tuple_(std::make_index_sequence<GeneN_>{})) spec_div_{};

    template<std::size_t I>
    void update_one_spec_(const typename std::tuple_element_t<I, Gene>& s, int delta) {
        using D = SpecDiv<I>;
        using E = typename D::E;

        auto& d = std::get<I>(spec_div_);
        constexpr std::size_t nb = D::nb;

        auto touch = [&](std::size_t elem_i, std::size_t bin, int dd) {
            int& c = d.cnt[elem_i * nb + bin];
            if (dd > 0) {
                if (c++ == 0) {
                    const int p0 = d.present[elem_i];
                    const double s0 = D::score(p0);

                    const int p1 = p0 + 1;
                    d.present[elem_i] = p1;
                    assert(d.present[elem_i] >= 0 && d.present[elem_i] <= (int)nb);

                    const double s1 = D::score(p1);
                    diversity_rate += (s1 - s0) * D::w;
                }
            } else {
                assert(c > 0);
                if (--c == 0) {
                    const int p0 = d.present[elem_i];
                    const double s0 = D::score(p0);

                    const int p1 = p0 - 1;
                    d.present[elem_i] = p1;
                    assert(d.present[elem_i] >= 0 && d.present[elem_i] <= (int)nb);

                    const double s1 = D::score(p1);
                    diversity_rate += (s1 - s0) * D::w;
                }
            }
        };

        ga::visit_elems_val(s, [&](auto elem, std::size_t i) {
            touch(i, D::bin_of(static_cast<E>(elem)), delta);
        });
    }

    template<std::size_t... I>
    void update_impl_(const Gene& g, int delta, std::index_sequence<I...>) {
        (update_one_spec_<I>(std::get<I>(g), delta), ...);
    }
};

template<GeneLike Gene>
struct Population {

private:
    std::vector<Gene> gene_;
    std::vector<Fitness> fitness_;
    std::vector<int> born_at_;

    int current_tick_ = 0;

public:
    Diversity<Gene> diversity;
    bool diversity_enabled = true;

    [[nodiscard]] int size() const { return static_cast<int>(gene_.size()); }

    [[nodiscard]] int current_tick() const { return current_tick_; }

    [[nodiscard]] double diversity_rate() const {
        return diversity_enabled
            ? diversity.diversity_rate
            : std::numeric_limits<double>::quiet_NaN();
    }

    const std::vector<Gene>& gene() const { return gene_; }
    const std::vector<Fitness>& fitness() const { return fitness_; }
    const std::vector<int>& born_at() const { return born_at_; }

    const Gene& gene(int index) const {
        assert(0 <= index && index < size());
        return gene_[static_cast<std::size_t>(index)];
    }
    Fitness fitness(int index) const {
        assert(0 <= index && index < size());
        return fitness_[static_cast<std::size_t>(index)];
    }
    int born_at(int index) const {
        assert(0 <= index && index < size());
        return born_at_[static_cast<std::size_t>(index)];
    }

    void clear() {
        gene_.clear();
        fitness_.clear();
        born_at_.clear();
        diversity.reset();
        current_tick_ = 0;
    }

    void reserve(int n) {
        if (n <= 0) return;
        const std::size_t nn = static_cast<std::size_t>(n);
        gene_.reserve(nn);
        fitness_.reserve(nn);
        born_at_.reserve(nn);
    }

    void advance_tick(int dt = 1) {
        if (dt <= 0) return;
        current_tick_ += dt;
    }

    struct SummaryStats {
        std::size_t n = 0;

        double fitness_min = std::numeric_limits<double>::quiet_NaN();
        double fitness_max = std::numeric_limits<double>::quiet_NaN();
        double fitness_avg = std::numeric_limits<double>::quiet_NaN();

        int born_at_min = 0;
        int born_at_max = 0;
        double born_at_avg = std::numeric_limits<double>::quiet_NaN();
    };

    [[nodiscard]] SummaryStats get_stats() const {
        SummaryStats s{};
        const std::size_t n = gene_.size();
        s.n = n;

        assert(fitness_.size() == n);
        assert(born_at_.size() == n);

        if (n > 0) {
            double mn = std::numeric_limits<double>::infinity();
            double mx = -std::numeric_limits<double>::infinity();
            long double sum = 0.0L;
            std::size_t cnt = 0;

            for (std::size_t i = 0; i < n; ++i) {
                const double f = fitness_[i];
                if (std::isnan(f)) continue;
                mn = std::min(mn, f);
                mx = std::max(mx, f);
                sum += static_cast<long double>(f);
                ++cnt;
            }
            if (cnt > 0) {
                s.fitness_min = mn;
                s.fitness_max = mx;
                s.fitness_avg = static_cast<double>(sum / static_cast<long double>(cnt));
            }
        }

        if (n > 0) {
            int mn = born_at_[0];
            int mx = born_at_[0];
            long long sum = 0;

            for (std::size_t i = 0; i < n; ++i) {
                const int b = born_at_[i];
                mn = std::min(mn, b);
                mx = std::max(mx, b);
                sum += static_cast<long long>(b);
            }

            s.born_at_min = mn;
            s.born_at_max = mx;
            s.born_at_avg = static_cast<double>(sum) / static_cast<double>(n);
        }

        return s;
    }

    bool better(int a, int b) const {
        assert(fitness_.size() == gene_.size());
        assert(born_at_.size() == gene_.size());

        const double fa = fitness_[static_cast<std::size_t>(a)];
        const double fb = fitness_[static_cast<std::size_t>(b)];
        const bool na = std::isnan(fa);
        const bool nb = std::isnan(fb);
        if (na != nb) return !na;
        if (!na && fa != fb) return fa > fb;

        const int ba = born_at_[static_cast<std::size_t>(a)];
        const int bb = born_at_[static_cast<std::size_t>(b)];
        if (ba != bb) return ba > bb;

        return a < b;
    }

    int get_best_index() const {
        const int n = size();
        if (n <= 0) return -1;
        int best = 0;
        for (int i = 1; i < n; ++i) if (better(i, best)) best = i;
        return best;
    }

    int get_worst_index() const {
        const int n = size();
        if (n <= 0) return -1;
        int worst = 0;
        for (int i = 1; i < n; ++i) if (better(worst, i)) worst = i;
        return worst;
    }

    void get_sorted_indices(std::vector<int>& out) const {
        const int n = size();
        out.resize(static_cast<std::size_t>(n));
        std::iota(out.begin(), out.end(), 0);
        std::sort(out.begin(), out.end(), [&](int a, int b) { return better(a, b); });
    }

    Fitness get_best_fitness() const {
        const std::size_t n = std::min<std::size_t>(gene_.size(), fitness_.size());
        if (n == 0) return std::numeric_limits<Fitness>::quiet_NaN();

        double best = -std::numeric_limits<double>::infinity();
        bool found = false;
        for (std::size_t i = 0; i < n; ++i) {
            const double f = fitness_[i];
            if (std::isnan(f)) continue;
            if (!found || f > best) {
                best = f;
                found = true;
            }
        }
        return found ? static_cast<Fitness>(best) : std::numeric_limits<Fitness>::quiet_NaN();
    }

    void set(int index, const Gene& g, Fitness f) { set_impl_(index, g, f); }
    void set(int index, Gene&& g,       Fitness f) { set_impl_(index, std::move(g), f); }

    int add(const Gene& g, Fitness f) { return add_impl_(g, f); }
    int add(Gene&& g,      Fitness f) { return add_impl_(std::move(g), f); }

    template<class F>
    void update(int index, F&& callback) {
        ++ga::stats.population_update;

        const auto n = gene_.size();
        assert(0 <= index && static_cast<std::size_t>(index) < n);
        assert(fitness_.size() == n);
        assert(born_at_.size() == n);

        if (diversity_enabled) diversity.update(gene_[static_cast<std::size_t>(index)], -1);

        if constexpr (std::invocable<F, Gene&, int>) {
            std::invoke(std::forward<F>(callback), gene_[static_cast<std::size_t>(index)], index);
        } else if constexpr (std::invocable<F, Gene&>) {
            std::invoke(std::forward<F>(callback), gene_[static_cast<std::size_t>(index)]);
        } else {
            static_assert(ga::dependent_false_v<F>, "Population::update callback must be invocable with (Gene&) or (Gene&, int)");
        }

        if (diversity_enabled) diversity.update(gene_[static_cast<std::size_t>(index)], +1);
    }

    void set_fitness(int index, Fitness f) {
        assert(0 <= index && index < size());
        assert(fitness_.size() == gene_.size());
        assert(born_at_.size() == gene_.size());
        fitness_[static_cast<std::size_t>(index)] = f;
    }

    void set_diversity_mode(bool enable) {
        if (diversity_enabled == enable) return;
        diversity_enabled = enable;
        if (diversity_enabled) rebuild_diversity();
    }

    void rebuild_diversity() {
        if (!diversity_enabled) return;
        ++ga::stats.population_rebuild_diversity;
        diversity.rebuild(gene_);
    }

    template <class F>
    void apply_rolling_horizon(F&& callback, int shift = 1, bool random_fill = false) {
        ++ga::stats.population_apply_rolling_horizon;
        if (shift < 0) shift = 0;
        if (gene_.empty()) return;

        auto& cb = callback;

        const int n = size();

        if constexpr (std::invocable<decltype(cb), Gene&, int, int>) {
            for (int i = 0; i < n; ++i) {
                auto& g = gene_[static_cast<std::size_t>(i)];
                apply_rolling_horizon_gene_(g, shift, random_fill);
                std::invoke(cb, g, i, shift);
            }
        } else if constexpr (std::invocable<decltype(cb), Gene&, int>) {
            for (int i = 0; i < n; ++i) {
                auto& g = gene_[static_cast<std::size_t>(i)];
                apply_rolling_horizon_gene_(g, shift, random_fill);
                std::invoke(cb, g, i);
            }
        } else if constexpr (std::invocable<decltype(cb), Gene&>) {
            for (int i = 0; i < n; ++i) {
                auto& g = gene_[static_cast<std::size_t>(i)];
                apply_rolling_horizon_gene_(g, shift, random_fill);
                std::invoke(cb, g);
            }
        } else {
            static_assert(
                ga::dependent_false_v<F>,
                "Population::apply_rolling_horizon callback must be invocable with (Gene&), (Gene&, int), or (Gene&, int, int)"
            );
        }

        rebuild_diversity();
    }

private:

    template<class G>
    void set_impl_(int index, G&& g, Fitness f) {
        ++ga::stats.population_set;
        const auto n = gene_.size();
        assert(0 <= index && static_cast<std::size_t>(index) < n);
        assert(fitness_.size() == n);
        assert(born_at_.size() == n);

        if (diversity_enabled) diversity.update(gene_[static_cast<std::size_t>(index)], -1);
        gene_[static_cast<std::size_t>(index)] = std::forward<G>(g);
        if (diversity_enabled) diversity.update(gene_[static_cast<std::size_t>(index)], +1);

        fitness_[static_cast<std::size_t>(index)] = f;
        born_at_[static_cast<std::size_t>(index)] = current_tick_;
    }

    template<class G>
    int add_impl_(G&& g, Fitness f) {
        ++ga::stats.population_add;
        const int idx = static_cast<int>(gene_.size());

        gene_.push_back(std::forward<G>(g));
        fitness_.push_back(f);
        born_at_.push_back(current_tick_);

        if (diversity_enabled) diversity.update(gene_.back(), +1);
        return idx;
    }

    static void apply_rolling_horizon_gene_(Gene& g, int shift, bool random_fill) {
        ga::detail::for_each_indexed(g, [&](auto , auto& spec) {
            apply_rolling_horizon_spec_(spec, shift, random_fill);
        });
    }

    template<class Spec>
    static void fill_tail_array(Spec& s, std::size_t from, std::size_t to, bool random_fill) {
        using V = typename Spec::val_t;
        static_assert(ga::is_arr_v<V>, "fill_tail_array: val_t must be std::array");
        using E = typename V::value_type;

        if (from >= to) return;

        if (random_fill) {
            if constexpr (ga::RandomElemValueSpec<Spec>) {
                for (std::size_t i = from; i < to; ++i) s.v[i] = ga::random_elem_value<Spec>();
                return;
            }
        }

        const E fill = static_cast<E>(ga::def_v<Spec>);
        std::fill(
            s.v.begin() + static_cast<std::ptrdiff_t>(from),
            s.v.begin() + static_cast<std::ptrdiff_t>(to),
            fill
        );
    }

    template<class Spec>
    static void fill_tail_bitset(Spec& s, std::size_t from, std::size_t to, bool random_fill) {
        using V = typename Spec::val_t;
        static_assert(ga::is_bs_v<V>, "fill_tail_bitset: val_t must be std::bitset");

        if (from >= to) return;

        if (random_fill) {
            if constexpr (ga::RandomElemValueSpec<Spec>) {
                for (std::size_t i = from; i < to; ++i) {
                    s.v.set(i, static_cast<bool>(ga::random_elem_value<Spec>()));
                }
                return;
            }
        }

        const bool fill = static_cast<bool>(ga::def_v<Spec>);
        if (fill) {
            for (std::size_t i = from; i < to; ++i) s.v.set(i, true);
        }

    }

    template<class Spec>
    static void apply_rolling_horizon_spec_(Spec& s, int shift, bool random_fill) {
        using V = typename Spec::val_t;

        if constexpr (requires { Spec::is_horizon; }) {
            if constexpr ((bool)Spec::is_horizon) {
                if constexpr (ga::is_arr_v<V>) {
                    using E = typename V::value_type;

                    constexpr std::size_t L = ga::static_len_v<V>;
                    const std::size_t k = static_cast<std::size_t>(shift);

                    if (k == 0) return;

                    if (k >= L) {
                        fill_tail_array(s, 0, L, random_fill);
                        return;
                    }

                    if constexpr (std::is_trivially_copyable_v<E>) {
                        std::memmove(s.v.data(), s.v.data() + k, (L - k) * sizeof(E));
                    } else {
                        for (std::size_t i = 0; i + k < L; ++i) s.v[i] = s.v[i + k];
                    }

                    fill_tail_array(s, L - k, L, random_fill);
                } else if constexpr (ga::is_bs_v<V>) {
                    constexpr std::size_t L = ga::static_len_v<V>;
                    const std::size_t k = static_cast<std::size_t>(shift);

                    if (k == 0) return;

                    if (k >= L) {
                        if (random_fill) {
                            if constexpr (ga::RandomElemValueSpec<Spec>) {
                                for (std::size_t i = 0; i < L; ++i) {
                                    s.v.set(i, static_cast<bool>(ga::random_elem_value<Spec>()));
                                }
                                return;
                            }
                        }
                        const bool fill = static_cast<bool>(ga::def_v<Spec>);
                        if (fill) s.v.set();
                        else s.v.reset();
                        return;
                    }

                    s.v >>= k;

                    fill_tail_bitset(s, L - k, L, random_fill);
                } else {
                    static_assert(ga::dependent_false_v<Spec>, "is_horizon=true requires val_t to be std::array or std::bitset");
                }
            }
        }
    }

};

}

#undef main

namespace gaop {

#define main ga__header_main
#undef main

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

        std::memcpy(c.v.data(),         p2.v.data(),         a * sizeof(typename V::value_type));
        std::memcpy(c.v.data() + a,     p1.v.data() + a,     (b - a) * sizeof(typename V::value_type));
        std::memcpy(c.v.data() + b,     p2.v.data() + b,     (L - b) * sizeof(typename V::value_type));
    }

    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const requires (ga::is_arr_v<V>) {
        std::uniform_int_distribution<std::size_t> a_dist(0, L);
        const std::size_t a = a_dist(ga::rng);
        std::uniform_int_distribution<std::size_t> b_dist(a, L);
        const std::size_t b = b_dist(ga::rng);

        std::memcpy(c1.v.data(),         p2.v.data(),         a * sizeof(typename V::value_type));
        std::memcpy(c1.v.data() + a,     p1.v.data() + a,     (b - a) * sizeof(typename V::value_type));
        std::memcpy(c1.v.data() + b,     p2.v.data() + b,     (L - b) * sizeof(typename V::value_type));

        std::memcpy(c2.v.data(),         p1.v.data(),         a * sizeof(typename V::value_type));
        std::memcpy(c2.v.data() + a,     p2.v.data() + a,     (b - a) * sizeof(typename V::value_type));
        std::memcpy(c2.v.data() + b,     p1.v.data() + b,     (L - b) * sizeof(typename V::value_type));
    }

    void apply(const Spec& p1, const Spec& p2, Spec& c) const requires (ga::is_bs_v<V>) {
        std::uniform_int_distribution<std::size_t> a_dist(0, L);
        std::size_t a = a_dist(ga::rng);
        std::uniform_int_distribution<std::size_t> b_dist(a, L);
        std::size_t b = b_dist(ga::rng);

        const auto mask = ga::low_mask<L>(b) ^ ga::low_mask<L>(a);
        c.v = (p1.v & mask) | (p2.v & ~mask);
    }

    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const requires (ga::is_bs_v<V>) {
        std::uniform_int_distribution<std::size_t> a_dist(0, L);
        std::size_t a = a_dist(ga::rng);
        std::uniform_int_distribution<std::size_t> b_dist(a, L);
        std::size_t b = b_dist(ga::rng);

        const auto mask = ga::low_mask<L>(b) ^ ga::low_mask<L>(a);
        c1.v = (p1.v & mask) | (p2.v & ~mask);
        c2.v = (p2.v & mask) | (p1.v & ~mask);
    }
};

template<ga::SpecLike Spec>
struct UniformCrossover {
    using S = Spec;
    using V = typename Spec::val_t;
    using E = ga::elem_of_t<V>;

    double p_pick = 0.5;

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

template<ga::SpecLike Spec>
using DiscretePickCrossover = UniformCrossover<Spec>;

template<ga::SpecLike Spec>
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
        const std::size_t r = rdist(ga::rng);

        std::array<unsigned char, N> used{};

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
        const std::size_t r = rdist(ga::rng);

        out = a;
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

template<ga::SpecLike Spec>
struct RandomResetMutation {
    using S = Spec;
    double p_reset = 0.01;

    void apply(Spec& s) const {
        std::bernoulli_distribution do_reset(p_reset);
        ga::visit_elems_ref(s, [&](auto&& elem, std::size_t ) {
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
    int step_max = 1;

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

template<ga::GeneLike Gene>
struct TournamentSelection {
    int k = 3;

    void prepare(const ga::Population<Gene>& ) const {}

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

    void prepare(const ga::Population<Gene>& ) const {}

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
    void prepare(const ga::Population<Gene>& ) const {}

    int apply(const ga::Population<Gene>& pop) const {
        return pop.get_worst_index();
    }
};

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
            if (!std::isfinite(f)) continue;
            has_finite = true;
            mn = std::min(mn, (long double)f);
            mx = std::max(mx, (long double)f);
        }
        if (!has_finite) {

            prefix.clear();
            total = 0.0;
            return;
        }

        const long double shift = (mn < 0.0L) ? -mn : 0.0L;
        constexpr long double eps = 1e-12L;

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

}

bool debug_mode = true;

template<class Gene, class Evaluator>
static double calibrate(Evaluator& ev, double budget_ms) {
    constexpr int kMaxTrials = 10000;
    constexpr int kBatch = 32;

    if (!(budget_ms > 0.0)) budget_ms = 1.0;

    volatile double sink = 0.0;

    std::array<Gene, kBatch> buf{};

    int trials = 0;
    double eval_total_ms = 0.0;

    const double start_ms = ga::now_ms();

    while (trials < kMaxTrials) {
        const double now = ga::now_ms();
        if (now - start_ms >= budget_ms) break;

        int batch = kBatch;
        if (trials + batch > kMaxTrials) batch = kMaxTrials - trials;

        for (int i = 0; i < batch; ++i) {
            ga::init_gene_random(buf[(std::size_t)i]);
        }

        const double t0 = ga::now_ms();
        for (int i = 0; i < batch; ++i) {
            sink = sink + ev.template evaluate<Gene>(buf[(std::size_t)i]);
        }
        const double t1 = ga::now_ms();

        eval_total_ms += (t1 - t0);
        trials += batch;
    }

    if (trials == 0) {
        Gene g{};
        ga::init_gene_random(g);
        const double t0 = ga::now_ms();
        sink = sink + ev.template evaluate<Gene>(g);
        const double t1 = ga::now_ms();
        eval_total_ms = t1 - t0;
        trials = 1;
    }

    (void)sink;
    return eval_total_ms / (double)trials;
}

struct Evaluator {

    struct State {
        int frame = 0;
        int x = 0;
        int y = 0;
    } st;

    static constexpr int kRouteN = 8;

    std::array<int, kRouteN> base_tx{};
    std::array<int, kRouteN> base_ty{};

    Evaluator() {

        std::mt19937 local(1234);
        std::uniform_int_distribution<int> dist(-15, 15);
        for (int i = 0; i < kRouteN; ++i) {
            base_tx[(std::size_t)i] = dist(local);
            base_ty[(std::size_t)i] = dist(local);
        }
    }

    void apply_real_action(int action) {

        switch (action) {
            case 0: st.y -= 1; break;
            case 1: st.x += 1; break;
            case 2: st.y += 1; break;
            case 3: st.x -= 1; break;
            case 4:  break;
            default: st.x += 1; st.y -= 1; break;
        }
        st.frame++;
    }

    inline std::pair<int, int> target_pos(int route_idx, int local_t) const {
        const int drift = ((st.frame + local_t) % 7) - 3;
        const int i = route_idx % kRouteN;
        return { base_tx[(std::size_t)i] + drift, base_ty[(std::size_t)i] - drift };
    }

    template<class Gene>
    double evaluate(const Gene& g) const {
        const auto& seq   = std::get<0>(g).v;
        const auto  param = std::get<1>(g).v;
        const auto& flags = std::get<2>(g).v;
        const auto& route = std::get<3>(g).v;

        int x = st.x;
        int y = st.y;

        const double mobility = 1.0 + 0.25 * (double)param;

        constexpr double gamma = 0.97;
        double w = 1.0;
        double wsum = 0.0;
        double acc = 0.0;

        for (int t = 0; t < (int)seq.size(); ++t) {
            const int act = seq[(std::size_t)t];

            const bool boost = flags.test((std::size_t)(t % (int)flags.size()));
            const int step = boost ? 2 : 1;

            const int s = (int)std::llround((double)step * mobility);
            switch (act) {
                case 0: y -= s; break;
                case 1: x += s; break;
                case 2: y += s; break;
                case 3: x -= s; break;
                case 4:  break;
                default: x += s; y -= s; break;
            }

            const int ridx = route[(std::size_t)(t % (int)route.size())];
            const auto [tx, ty] = target_pos(ridx, t);

            const int dx = x - tx;
            const int dy = y - ty;
            const int dist = std::abs(dx) + std::abs(dy);

            double reward = 100.0 - (double)dist;
            if (boost) reward += 1.5;

            if (std::abs(x) > 40 || std::abs(y) > 40) reward -= 20.0;

            acc  += w * reward;
            wsum += w;
            w    *= gamma;
        }

        const double terminal = -0.05 * (double)(std::abs(x) + std::abs(y));
        acc  += w * terminal;
        wsum += w;

        return acc / wsum;
    }
};

static constexpr int kActions = 6;
static constexpr int kFlagsN  = 32;
static constexpr int kRouteN  = Evaluator::kRouteN;

template<int H>
struct ActionPlan {
    using elem_t = int;
    static constexpr elem_t lo = 0;
    static constexpr elem_t hi = kActions - 1;
    static constexpr std::size_t n_bins = kActions;
    static constexpr elem_t def = 4;
    static constexpr bool is_horizon = true;
    using val_t = std::array<elem_t, H>;
    val_t v = ga::init_array<val_t>(def);
};

struct Aggression {
    using elem_t = float;
    static constexpr elem_t lo = -2.0f;
    static constexpr elem_t hi =  2.0f;
    static constexpr std::size_t n_bins = 9;
    static constexpr elem_t def = 0.0f;
    using val_t = elem_t;
    val_t v = def;
};

struct BoostMask {
    using elem_t = bool;
    static constexpr elem_t def = false;
    using val_t = std::bitset<kFlagsN>;
    val_t v = ga::init_bs<val_t>(def);
};

template<int N>
struct RouteOrder {
    using elem_t = int;
    static constexpr elem_t lo = 0;
    static constexpr elem_t hi = N - 1;
    static constexpr std::size_t n_bins = (std::size_t)N;
    static constexpr elem_t def = 0;
    static constexpr bool is_perm = true;
    using val_t = std::array<elem_t, N>;
    val_t v = ga::init_perm<val_t>();
};

template<int H>
using GeneH = std::tuple<ActionPlan<H>, Aggression, BoostMask, RouteOrder<kRouteN>>;

template<class Gene, class EvaluatorT>
struct SSGARunner {
    EvaluatorT& evaluator;
    int pop_size = 0;

    int run_no = 0;
    int iters  = 0;

    ga::Population<Gene> pop;

    gaop::TournamentSelection<Gene> parent_sel{};
    gaop::ReverseTournamentSelection<Gene> victim_sel{};

    using SeqSpec   = std::tuple_element_t<0, Gene>;
    using ParamSpec = std::tuple_element_t<1, Gene>;
    using FlagSpec  = std::tuple_element_t<2, Gene>;
    using PermSpec  = std::tuple_element_t<3, Gene>;

    using Crossovers = std::tuple<
        gaop::ChoiceCrossover<SeqSpec, gaop::TwoPointCrossover<SeqSpec>, gaop::UniformCrossover<SeqSpec>>,
        gaop::ArithmeticCrossover<ParamSpec>,
        gaop::UniformCrossover<FlagSpec>,
        gaop::OXCrossover<PermSpec>
    >;

    using Mutations = std::tuple<
        gaop::ChoiceMutation<SeqSpec, gaop::CreepMutation<SeqSpec>, gaop::RandomResetMutation<SeqSpec>>,
        gaop::GaussianMutation<ParamSpec>,
        gaop::BitFlipMutation<FlagSpec>,
        gaop::ChoiceMutation<PermSpec, gaop::SwapMutation<PermSpec>, gaop::InversionMutation<PermSpec>>
    >;

    Crossovers cross{};
    Mutations muts{};

    double p_crossover = 0.90;

    int immigration_interval = 0;
    double immigration_chance = 0.35;

    int stagnation_window = 0;
    float diversity_floor = 0.20f;
    int shake_replace = 0;

    double log_interval_ms = 10.0;

    struct Stats {
        double run_start_ms = 0.0;
        double last_log_ms = 0.0;

        std::uint64_t evals = 0;
        std::uint64_t children = 0;
        std::uint64_t immigrants = 0;
        std::uint64_t shakes = 0;

        double best_fitness = -std::numeric_limits<double>::infinity();
        int best_iter = 0;
        int no_improve_iters = 0;

        ga::Stats header_stats_begin{};
    } stats{};

    SSGARunner(EvaluatorT& ev, int n)
        : evaluator(ev), pop_size(n) {
        if (pop_size < 2) pop_size = 2;

        pop.reserve(pop_size);

        parent_sel.k = 3;
        victim_sel.k = 3;

        immigration_interval = std::max(16, pop_size * 2);
        stagnation_window   = std::max(64, pop_size * 6);
        shake_replace       = std::max(2, pop_size / 10);

        {
            auto& cc = std::get<0>(cross);
            cc.weights = {0.7, 0.3};
        }

        {
            auto& cm = std::get<0>(muts);
            cm.weights = {0.75, 0.25};
            auto& creep = std::get<0>(cm.muts);
            creep.p_mut = 0.12;
            creep.step_max = 2;
            auto& reset = std::get<1>(cm.muts);
            reset.p_reset = 0.03;
        }
        {
            auto& gm = std::get<1>(muts);
            gm.p_mut = 0.35;
            gm.sigma = 0.35;
        }
        {
            auto& bf = std::get<2>(muts);
            bf.p_flip = 0.02;
        }
        {
            auto& pm = std::get<3>(muts);
            pm.weights = {0.6, 0.4};
            auto& sw = std::get<0>(pm.muts);
            sw.p_swap = 0.25;
            auto& inv = std::get<1>(pm.muts);
            inv.p_inv = 0.15;
        }
    }

    const Gene& best_gene() const {
        static const Gene dummy{};
        const int idx = pop.get_best_index();
        if (idx < 0) return dummy;
        return pop.gene(idx);
    }

    double best_fitness() const {
        return (double)pop.get_best_fitness();
    }

    void run(double end_time_ms) {
        ++run_no;
        iters = 0;

        stats = Stats{};
        stats.run_start_ms = ga::now_ms();
        stats.last_log_ms = stats.run_start_ms;
        stats.header_stats_begin = ga::stats;

        init_population();

        while (ga::now_ms() < end_time_ms) {
            ++iters;
            maybe_make_child();
            maybe_immigrate();
            maybe_shake();
            maybe_log();
        }

        if (debug_mode) {
            const double now = ga::now_ms();
            const bool never_logged = (stats.last_log_ms == stats.run_start_ms);
            if (never_logged || (now - stats.last_log_ms) > log_interval_ms * 0.5) {
                log_stats();
            }
        }
    }

    void init_population() {
        if (run_no == 1) {
            pop.clear();
            pop.reserve(pop_size);

            for (int i = 0; i < pop_size; ++i) {
                Gene g{};
                if (i == 0) ga::init_gene_default(g);
                else        ga::init_gene_random(g);

                const double f = evaluator.template evaluate<Gene>(g);
                ++stats.evals;
                pop.add(std::move(g), f);
            }

            stats.best_fitness = (double)pop.get_best_fitness();
            stats.best_iter = 0;
            stats.no_improve_iters = 0;
            return;
        }

        pop.apply_rolling_horizon(
            [](Gene& , int , int ) {

            },
            1,
            true
        );

        const int n = pop.size();
        for (int i = 0; i < n; ++i) {
            const double f = evaluator.template evaluate<Gene>(pop.gene(i));
            ++stats.evals;
            pop.set_fitness(i, f);
        }

        stats.best_fitness = (double)pop.get_best_fitness();
        stats.best_iter = 0;
        stats.no_improve_iters = 0;
    }

    void maybe_make_child() {
        const int n = pop.size();
        if (n <= 1) return;

        const int p1 = parent_sel.apply(pop);
        int p2 = parent_sel.apply(pop);
        if (p2 == p1) {
            p2 = parent_sel.apply(pop);
            if (p2 == p1) p2 = (p1 + 1) % n;
        }

        Gene child{};

        {
            std::bernoulli_distribution do_cross(p_crossover);
            if (do_cross(ga::rng)) {
                ga::apply_crossover(cross, pop.gene(p1), pop.gene(p2), child);
            } else {
                const int better = pop.better(p1, p2) ? p1 : p2;
                child = pop.gene(better);
            }
        }

        ga::apply_mutation(muts, child);

        const double f = evaluator.template evaluate<Gene>(child);
        ++stats.evals;
        ++stats.children;

        pop.advance_tick(1);

        int victim = victim_sel.apply(pop);
        if (victim < 0) victim = 0;

        const int best_now = pop.get_best_index();
        if (victim == best_now && n >= 2) {
            for (int retry = 0; retry < 3; ++retry) {
                const int v2 = victim_sel.apply(pop);
                if (v2 >= 0 && v2 != best_now) { victim = v2; break; }
            }
            if (victim == best_now) victim = (best_now + 1) % n;
        }

        pop.set(victim, std::move(child), f);

        if (f > stats.best_fitness) {
            stats.best_fitness = f;
            stats.best_iter = iters;
            stats.no_improve_iters = 0;
        } else {
            ++stats.no_improve_iters;
        }
    }

    void maybe_immigrate() {
        if (immigration_interval <= 0) return;
        if (iters % immigration_interval != 0) return;

        const double div = pop.diversity_rate();
        double p = immigration_chance;
        if (div > 0.45) p *= 0.25;

        std::bernoulli_distribution do_it(p);
        if (!do_it(ga::rng)) return;

        Gene immigrant{};
        ga::init_gene_random(immigrant);
        const double f = evaluator.template evaluate<Gene>(immigrant);
        ++stats.evals;
        ++stats.immigrants;

        pop.advance_tick(1);

        int victim = victim_sel.apply(pop);
        if (victim < 0) victim = pop.get_worst_index();
        if (victim < 0) victim = 0;

        const int best_now = pop.get_best_index();
        const int n = pop.size();
        if (victim == best_now && n >= 2) {
            victim = (best_now + 1) % n;
        }

        pop.set(victim, std::move(immigrant), f);

        if (f > stats.best_fitness) {
            stats.best_fitness = f;
            stats.best_iter = iters;
            stats.no_improve_iters = 0;
        }
    }

    void maybe_shake() {
        const double div = pop.diversity_rate();

        const bool stagnated = (stats.no_improve_iters >= stagnation_window);
        const bool lowdiv    = (div < (double)diversity_floor);
        if (!stagnated && !lowdiv) return;

        const int n = pop.size();
        if (n <= 2) return;

        const int best_now = pop.get_best_index();

        const int replace_n = std::min(std::max(1, shake_replace), n - 1);
        for (int rep = 0; rep < replace_n; ++rep) {
            int victim = victim_sel.apply(pop);
            if (victim < 0) victim = 0;
            if (victim == best_now) victim = (best_now + 1) % n;

            Gene g{};
            ga::init_gene_random(g);
            const double f = evaluator.template evaluate<Gene>(g);
            ++stats.evals;

            pop.set(victim, std::move(g), f);

            if (f > stats.best_fitness) {
                stats.best_fitness = f;
                stats.best_iter = iters;
                stats.no_improve_iters = 0;
            }
        }

        ++stats.shakes;

        if (stagnation_window > 0) {
            stats.no_improve_iters = std::min(stats.no_improve_iters, stagnation_window / 2);
        }
    }

    void maybe_log() {
        if (!debug_mode) return;
        const double now = ga::now_ms();
        if (now - stats.last_log_ms < log_interval_ms) return;
        stats.last_log_ms = now;
        log_stats();
    }

    static ga::Stats diff_stats_(const ga::Stats& a, const ga::Stats& b) {
        ga::Stats d{};
        d.population_set = b.population_set - a.population_set;
        d.population_add = b.population_add - a.population_add;
        d.population_update = b.population_update - a.population_update;
        d.population_rebuild_diversity = b.population_rebuild_diversity - a.population_rebuild_diversity;
        d.population_apply_rolling_horizon = b.population_apply_rolling_horizon - a.population_apply_rolling_horizon;
        d.apply_crossover = b.apply_crossover - a.apply_crossover;
        d.apply_mutation = b.apply_mutation - a.apply_mutation;
        return d;
    }

    void log_stats() {
        const auto ps = pop.get_stats();
        const double div = pop.diversity_rate();

        const ga::Stats cur = ga::stats;
        const ga::Stats d = diff_stats_(stats.header_stats_begin, cur);

        std::fprintf(
            stderr,
            "[SSGA] run=%d iter=%d pop=%zu  best=%.3f avg=%.3f worst=%.3f  div=%.3f  eval=%llu  child=%llu imm=%llu shake=%llu  (cross=%llu mut=%llu set=%llu add=%llu)\n",
            run_no,
            iters,
            ps.n,
            ps.fitness_max,
            ps.fitness_avg,
            ps.fitness_min,
            div,
            (unsigned long long)stats.evals,
            (unsigned long long)stats.children,
            (unsigned long long)stats.immigrants,
            (unsigned long long)stats.shakes,
            (unsigned long long)d.apply_crossover,
            (unsigned long long)d.apply_mutation,
            (unsigned long long)d.population_set,
            (unsigned long long)d.population_add
        );
    }
};

template<class EvaluatorT, class... Genes>
static int choose_best_gene(EvaluatorT& ev, int pop_size, double run_ms) {
    constexpr int N = (int)sizeof...(Genes);
    static_assert(N > 0);

    const std::mt19937 base_rng = ga::rng;

    int best_idx = 0;
    double best_fit = -std::numeric_limits<double>::infinity();

    int i = 0;
    ([&] {
        using G = Genes;
        ga::rng = base_rng;

        SSGARunner<G, EvaluatorT> runner(ev, pop_size);
        runner.run(ga::now_ms() + run_ms);

        const double f = runner.best_fitness();
        std::fprintf(stderr, "[choose_best_gene] idx=%d best_fitness=%.3f\n", i, f);

        if (i == 0 || f > best_fit) {
            best_fit = f;
            best_idx = i;
        }
        ++i;
    }(), ...);

    std::fprintf(stderr, "[choose_best_gene] best_index=%d best_fitness=%.3f\n", best_idx, best_fit);
    return best_idx;
}

template<class Gene, class EvaluatorT>
static int choose_best_population_size(EvaluatorT& ev, std::span<int> candidates, double run_ms) {
    const std::mt19937 base_rng = ga::rng;

    int best_pop = 0;
    double best_fit = -std::numeric_limits<double>::infinity();

    for (int n : candidates) {
        if (n < 2) continue;
        ga::rng = base_rng;

        SSGARunner<Gene, EvaluatorT> runner(ev, n);
        runner.run(ga::now_ms() + run_ms);

        const double f = runner.best_fitness();
        std::fprintf(stderr, "[choose_best_pop] pop=%d best_fitness=%.3f\n", n, f);

        if (best_pop == 0 || f > best_fit) {
            best_fit = f;
            best_pop = n;
        }
    }

    std::fprintf(stderr, "[choose_best_pop] best_pop=%d best_fitness=%.3f\n", best_pop, best_fit);
    return best_pop;
}

template<class Gene, class EvaluatorT, class PrepareFn>
static int choose_best_trials(EvaluatorT& ev, int pop_size, double run_ms, int trials, PrepareFn prepare) {
    if (trials <= 0) trials = 1;

    const std::mt19937 base_rng = ga::rng;

    int best_trial = 0;
    double best_fit = -std::numeric_limits<double>::infinity();

    for (int t = 0; t < trials; ++t) {
        ga::rng = base_rng;

        SSGARunner<Gene, EvaluatorT> runner(ev, pop_size);

        prepare(t, runner, ev);

        runner.run(ga::now_ms() + run_ms);

        const double f = runner.best_fitness();
        std::fprintf(stderr, "[choose_best_trials] trial=%d best_fitness=%.3f\n", t, f);

        if (t == 0 || f > best_fit) {
            best_fit = f;
            best_trial = t;
        }
    }

    std::fprintf(stderr, "[choose_best_trials] best_trial=%d best_fitness=%.3f\n", best_trial, best_fit);
    return best_trial;
}

template<class Gene, class EvaluatorT>
static int choose_best_trials(EvaluatorT& ev, int pop_size, double run_ms, int trials) {
    return choose_best_trials<Gene>(
        ev, pop_size, run_ms, trials,
        [](int , SSGARunner<Gene, EvaluatorT>& , EvaluatorT& ) {}
    );
}

static void demo_calibrate() {
    std::fprintf(stderr, "\n=== demo_calibrate ===\n");
    Evaluator ev;
    using G = GeneH<16>;
    const double ms = calibrate<G>(ev, 5.0);
    std::fprintf(stderr, "calibrate: GeneH<16> evaluate avg = %.4f ms\n", ms);
}

static void demo_choose_best_gene() {
    std::fprintf(stderr, "\n=== demo_choose_best_gene (horizon length) ===\n");
    Evaluator ev;

    const int idx = choose_best_gene<Evaluator, GeneH<8>, GeneH<16>, GeneH<24>>(ev, 64, 25.0);
    std::fprintf(stderr, "demo_choose_best_gene: selected idx=%d (0:8, 1:16, 2:24)\n", idx);
}

static void demo_choose_best_population_size() {
    std::fprintf(stderr, "\n=== demo_choose_best_population_size (population size) ===\n");
    Evaluator ev;
    using G = GeneH<16>;

    std::array<int, 5> candidates = {16, 32, 48, 64, 96};
    const int best_n = choose_best_population_size<G>(ev, std::span<int>(candidates), 25.0);
    std::fprintf(stderr, "demo_choose_best_population_size: selected pop=%d\n", best_n);
}

static void demo_choose_best_trials() {
    std::fprintf(stderr, "\n=== demo_choose_best_trials (multi-run with prepare callback) ===\n");

    Evaluator ev;
    using Gene = GeneH<16>;

    const int best_trial = choose_best_trials<Gene>(
        ev,
        64,
        15.0,
        3,
        [](int t, SSGARunner<Gene, Evaluator>& runner, Evaluator& e) {

            ga::rng.seed(42u + (unsigned)t * 100u);

            e.st.frame = 0;
            e.st.x = 0;
            e.st.y = 0;
            if (t == 1) { e.st.x = 3; e.st.y = -2; }
            if (t == 2) { e.st.x = -4; e.st.y = 1; }

            runner.p_crossover = (t == 0) ? 0.90 : (t == 1 ? 0.95 : 0.85);
            runner.diversity_floor = (t == 2) ? 0.15f : 0.20f;
        }
    );

    std::fprintf(stderr, "demo_choose_best_trials: selected best_trial=%d\n", best_trial);
}

static void demo_rolling_horizon() {
    std::fprintf(stderr, "\n=== demo_rolling_horizon (SSGARunner reuse + rolling horizon) ===\n");

    Evaluator ev;
    using Gene = GeneH<16>;

    SSGARunner<Gene, Evaluator> runner(ev, 64);

    for (int frame = 0; frame < 5; ++frame) {
        const double budget_ms = 20.0;

        runner.run(ga::now_ms() + budget_ms);

        const Gene& best = runner.best_gene();
        const int action0 = std::get<0>(best).v[0];
        const double bf = runner.best_fitness();

        std::fprintf(stderr, "[RH] frame=%d best_fitness=%.3f action0=%d\n", frame, bf, action0);

        ev.apply_real_action(action0);
    }
}

#if 1
int main() {
    debug_mode = true;

    ga::rng.seed(42);

    demo_calibrate();
    demo_choose_best_gene();
    demo_choose_best_population_size();
    demo_choose_best_trials();
    demo_rolling_horizon();

    std::fprintf(stderr, "\n[done] ssga_sample\n");
    return 0;
}
#endif
