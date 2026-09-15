#pragma once

#include <bits/stdc++.h>

namespace ga {

inline std::mt19937 rng(42);

// ------------------------------------------------------------
// Timing helper
// ------------------------------------------------------------
// 現在時刻を double ミリ秒で返す（ミリ秒以下も保持、切り捨てない）
// - CodinGame 等の「経過時間計測」用途を想定（steady_clock: 単調増加）
//
// 例) double t0 = ga::now_ms(); ... double dt = ga::now_ms() - t0;
inline double now_ms() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double, std::milli>(clock::now().time_since_epoch()).count();
}

// ------------------------------------------------------------
// Stats
// ------------------------------------------------------------
// ライブラリ内部で呼ばれた回数を記録する簡易統計。
// 必要ならユーザー側で ga::stats を参照して計測/デバッグに使える。
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

// ヘッダ単体運用のため inline 変数にして ODR を回避
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

// is_arr_v
template<class V> inline constexpr bool is_arr_v = false;
template<class T, std::size_t N> inline constexpr bool is_arr_v<std::array<T, N>> = true;

// is_bs_v
template<class V> inline constexpr bool is_bs_v = false;
template<std::size_t N> inline constexpr bool is_bs_v<std::bitset<N>> = true;


// is_perm_v
// 順列用の Spec であることをチェックする。
// 条件:
// - Spec::is_perm が存在し true
// - val_t が整数型の std::array
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

// 0..N-1 の順列で初期化する
template<class Arr>
constexpr Arr init_perm() {
    static_assert(ga::is_arr_v<Arr>, "init_perm: Arr must be std::array");
    using E = typename Arr::value_type;
    static_assert(std::is_integral_v<E>, "init_perm: Arr::value_type must be integral");
    Arr a{};
    for (std::size_t i = 0; i < a.size(); ++i) a[i] = static_cast<E>(i);
    return a;
}

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

// 依存 false（static_assert 用）
template<class> inline constexpr bool dependent_false_v = false;

// ---------------------------------------------
// Spec constraints (C++20 Concepts)
// ---------------------------------------------

template<class S>
using spec_elem_t = ga::elem_of_t<typename S::val_t>;

template<class T>
inline constexpr bool is_enum_v = requires {
    T::options;
    requires ga::is_arr_v<std::remove_cvref_t<decltype(T::options)>>;
};

// n_options_v
// - enum(options) の選択肢数を返す（is_enum_v==false の場合は常に 0）
// - Spec::options が存在しない場合も 0
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

// def_v
template<BasicSpecLike Spec>
inline constexpr ga::spec_elem_t<Spec> def_v = []() constexpr {
    using E = ga::spec_elem_t<Spec>;
    if constexpr (requires { Spec::def; }) {
        return static_cast<E>(Spec::def);
    } else {
        return E{};
    }
}();

// n_bins_v
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
        // permutation: reduce bins to save Diversity memory
        return std::min<std::size_t>(ga::static_len_v<V>, 10u);
    } else {
        return static_cast<std::size_t>(T::n_bins);
    }
}();

// random_elem_value
// NOTE:
// - Diversity を必ず使う前提のため、数値型（integral/floating, bool を除く）の Spec は常に lo/hi が必須。
//   （ただし enum(options) と permutation(is_perm) は例外）
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

// Full SpecLike: Basic + n_bins_v + (random init)
// - 数値型（integral/floating, bool を除く）の Spec は常に lo/hi 必須（enum / permutation は除く）
// - permutation Spec は init_default/init_random で専用処理を行うため、random_elem_value は必須ではない
template<class S>
concept SpecLike = BasicSpecLike<S> && NBinSpec<S> && (ga::is_perm_v<S> || RandomElemValueSpec<S>);

// ---------------------------------------------
// Gene constraints
//  - Gene は std::tuple<Spec...> を想定（tuple_size/tuple_element が使える型）
//  - 各要素 Spec は SpecLike を満たす
// ---------------------------------------------
template<class Gene>
concept GeneLike =
    requires { typename std::tuple_size<std::remove_cvref_t<Gene>>::type; } &&
    []<std::size_t... I>(std::index_sequence<I...>) constexpr {
        return (SpecLike<std::tuple_element_t<I, std::remove_cvref_t<Gene>>> && ...);
    }(std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<Gene>>>{});


// ---------------------------------------------
// bitset 用 low_mask（free関数）
// ---------------------------------------------
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
    m >>= (N - k); // 下位kだけ1
    return m;
}

// ---------------------------------------------
// Spec 単位の要素走査
//  - visit_elems_ref : 可変参照（array/scalar: 参照、bitset: std::bitset::reference）
//  - visit_elems_val : 読み取り（bitset は bool を渡す）
//  - idx も渡す（不要なら無視してOK）
// ---------------------------------------------
template<SpecLike Spec, class F>
void visit_elems_ref(Spec& s, F&& f) {
    using V = typename Spec::val_t;

    if constexpr (ga::is_arr_v<V>) {
        constexpr std::size_t L = ga::static_len_v<V>;
        for (std::size_t i = 0; i < L; ++i) f(s.v[i], i);
    } else if constexpr (ga::is_bs_v<V>) {
        constexpr std::size_t L = ga::static_len_v<V>;
        for (std::size_t i = 0; i < L; ++i) f(s.v[i], i); // std::bitset::reference
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

// ---------------------------------------------
// init_default / random
// ---------------------------------------------
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
        // NOTE:
        // - permutation は「0..N-1 を shuffle」するのが最も自然で高速（重複も起きない）。
        // - Spec::random_elem() が定義されていても、ここでは意図的に使用しない。
        s.v = ga::init_perm<V>();
        std::shuffle(s.v.begin(), s.v.end(), ga::rng);
    } else if constexpr (ga::is_bs_v<V>) {
        // NOTE:
        // - std::bitset は random_elem_value() を使わず、32bit 単位の乱数で高速に埋める。
        // - Spec::random_elem() が定義されていても、ここでは意図的に使用しない。
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
        ga::visit_elems_ref(s, [&](auto&& elem, std::size_t /*i*/) {
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

} // namespace detail

// Gene(tuple<Spec...>) をデフォルト初期化
template<GeneLike Gene>
void init_gene_default(Gene& g) {
    ga::detail::for_each_indexed(g, [&](auto /*i*/, auto& spec) {
        ga::init_default(spec);
    });
}

// Gene をランダム初期化
template<GeneLike Gene>
void init_gene_random(Gene& g) {
    ga::detail::for_each_indexed(g, [&](auto /*i*/, auto& spec) {
        ga::init_random(spec);
    });
}

// ---------------------------------------------
// crossover / mutation の適用
// ---------------------------------------------
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

    ga::detail::for_each_indexed(c1, [&](auto idx_c, auto& /*cs1*/) {
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

// ---------------------------------------------
// Diversity（世代全体の多様性）
// ---------------------------------------------
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

        // element weight:
        // - Specごとの評価値は要素ごとに [0,1] に正規化して平均する。
        // - ここでは各要素の重みを 1/(GeneN*len) とする（bin数には依存させない）。
        static constexpr double w = 1.0 / (double)GeneN_ / (double)len;

        static constexpr double inv_nb_minus_1 = (nb > 1) ? (1.0 / (double)(nb - 1)) : 0.0;

        static double score(int n_present) {
            // n_bins_v <= 1 の Spec は「多様性を計算しても意味が薄い」ため常に 0 とする。
            if constexpr (nb <= 1) return 0.0;
            if (n_present <= 1) return 0.0;
            return (double)(n_present - 1) * inv_nb_minus_1; // (present-1)/(nb-1)
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

        // numeric binning cache (per Spec)
        // - lo/hi の swap と span 計算、scale=nb/span を事前に保持して hot path を軽くする
        struct NumCache {
            double lo = 0.0;
            double nb_d = 0.0;
            double scale = 0.0; // nb/span
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
                // permutation:
                // - 本来は値そのもの（0..len-1）を bin として扱えるが、
                //   Diversity のメモリ節約のため、bin 数を min(len, 10) に落とし、
                //   値 % nb の mod で潰す。
                if constexpr (std::is_signed_v<E>) {
                    if (x < 0) return 0;
                }
                return static_cast<std::size_t>(x) % nb;
            } else if constexpr (ga::NumericRangeSpec<Spec>) {
                // numeric:
                // - pos is in [0, nb] where xv==hi -> nb
                // - lo/span/scale are cached in num_cache (per Spec)
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
    // NOTE:
    // - Population は内部に Diversity（大きな配列）を持つためコピーは高コスト。
    //   可能な限り参照渡し（Population&）で扱うことを推奨。
    //
    // - 初期構築は add() で行うこと（diversity との整合を保つため）。
    // - set()/update() は（diversity_enabled==true のとき）diversity が既に gene と整合している前提。
    //   つまり、add()/rebuild_diversity()/set_diversity_mode(true) で整合させてから使う。
    //
    // - born_at は「この個体が生まれたタイミング（tick）」を表す。
    //   add()/set() で対象インデックスの born_at が current_tick に更新される。
    //   better() では born_at が大きいほど良い（新しいほど良い）として tie-break に用いる。
    //
    // - gene/fitness/born_at は安全のため private。
    //   外部から参照したい場合は getter を使うこと。
private:
    std::vector<Gene> gene_;
    std::vector<Fitness> fitness_;
    std::vector<int> born_at_;

    int current_tick_ = 0;

public:
    Diversity<Gene> diversity;
    bool diversity_enabled = true; // true: diversity を自動更新 / false: 更新しない（有効化時に rebuild）

    // --------------------
    // Basic getters
    // --------------------
    [[nodiscard]] int size() const { return static_cast<int>(gene_.size()); }

    [[nodiscard]] int current_tick() const { return current_tick_; }

    // diversity rate helper
    // - diversity_mode OFF (diversity_enabled==false) の場合は NaN を返す
    [[nodiscard]] double diversity_rate() const {
        return diversity_enabled
            ? diversity.diversity_rate
            : std::numeric_limits<double>::quiet_NaN();
    }

    // vector getters (read-only)
    const std::vector<Gene>& gene() const { return gene_; }
    const std::vector<Fitness>& fitness() const { return fitness_; }
    const std::vector<int>& born_at() const { return born_at_; }

    // element getters
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

    // --------------------
    // Utilities
    // --------------------
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

    // tick を進める（個体の born_at は変えない）
    void advance_tick(int dt = 1) {
        if (dt <= 0) return;
        current_tick_ += dt;
    }

    // Population の要約統計（gene/fitness/born_at）
    // - fitness は NaN を無視して集計（全て NaN/未設定なら NaN のまま）
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

        // gene_/fitness_/born_at_ は add()/set() により常に同サイズで管理される前提
        assert(fitness_.size() == n);
        assert(born_at_.size() == n);

        // fitness stats (ignore NaN)
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

        // born_at stats
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
        if (na != nb) return !na; // NaN is always worse
        if (!na && fa != fb) return fa > fb;

        const int ba = born_at_[static_cast<std::size_t>(a)];
        const int bb = born_at_[static_cast<std::size_t>(b)];
        if (ba != bb) return ba > bb; // larger is better (newer)

        return a < b;
    }

    // best / worst index helpers
    // - return -1 if empty
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

    // sorted indices (best first)
    void get_sorted_indices(std::vector<int>& out) const {
        const int n = size();
        out.resize(static_cast<std::size_t>(n));
        std::iota(out.begin(), out.end(), 0);
        std::sort(out.begin(), out.end(), [&](int a, int b) { return better(a, b); });
    }

    // best fitness helper
    // - NaN は無視（全て NaN / 未設定なら NaN を返す）
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

    // gene[index] を安全に更新する（diversity を自動更新）
    // callback は (Gene&) または (Gene&, int index) を受け取れる。
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

    // diversity の自動更新を ON/OFF する（デフォルト ON）
    // - OFF: add/set/update/rolling_horizon で diversity を更新しない（diversity は stale になりうる）
    // - ON : 有効化したタイミングで 1 回だけ rebuild_diversity() して整合を取る
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

        // NOTE:
        // - コールバックを複数回呼ぶため、std::forward<F>(callback) は使わない。
        //   （rvalue で渡されたコールバックがループ中にムーブされて壊れるのを防ぐ）
        auto& cb = callback;

        const int n = size();

        // callback: update() と同じ並び（index が後ろ）、かつ最後に shift を付ける
        //  - (Gene&, int index, int shift)
        //  - (Gene&, int index)
        //  - (Gene&)
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
        ga::detail::for_each_indexed(g, [&](auto /*i*/, auto& spec) {
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
        // fill=false の場合、shift で 0 埋めされているので何もしない
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

                    // k >= L: all elements are replaced by fill/random
                    if (k >= L) {
                        fill_tail_array(s, 0, L, random_fill);
                        return;
                    }

                    // shift: new[i] = old[i + k]
                    if constexpr (std::is_trivially_copyable_v<E>) {
                        std::memmove(s.v.data(), s.v.data() + k, (L - k) * sizeof(E));
                    } else {
                        for (std::size_t i = 0; i + k < L; ++i) s.v[i] = s.v[i + k];
                    }

                    // fill tail
                    fill_tail_array(s, L - k, L, random_fill);
                } else if constexpr (ga::is_bs_v<V>) {
                    constexpr std::size_t L = ga::static_len_v<V>;
                    const std::size_t k = static_cast<std::size_t>(shift);

                    if (k == 0) return;

                    // k >= L: all bits are replaced by fill/random
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

                    // std::bitset は index0 が LSB。
                    // rolling horizon は「index0 が先頭(t=0)」という仕様。
                    // したがって new[i]=old[i+k] を実現するには >>=k が正しい。
                    s.v >>= k;

                    // fill the vacated (high) bits
                    fill_tail_bitset(s, L - k, L, random_fill);
                } else {
                    static_assert(ga::dependent_false_v<Spec>, "is_horizon=true requires val_t to be std::array or std::bitset");
                }
            }
        }
    }


};

} // namespace ga

#if 0
// ------------------------------------------------------------
// Tests / Benchmarks / Demos
// ------------------------------------------------------------
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <string_view>

// --------- Spec Samples ---------
struct Float {
    using elem_t = float;
    static constexpr elem_t hi = 100.0;
    static constexpr elem_t lo = 0.0;
    static constexpr std::size_t n_bins = 5;
    static constexpr elem_t def = 0.0;
    using val_t = elem_t;
    val_t v = def;
};

struct FloatArray {
    using elem_t = float;
    static constexpr elem_t hi = 100.0;
    static constexpr elem_t lo = 0.0;
    static constexpr std::size_t n_bins = 5;
    static constexpr elem_t def = 0.0;
    using val_t = std::array<elem_t, 10>;
    val_t v = ga::init_array<val_t>(def);
};

struct Int {
    using elem_t = int;
    static constexpr elem_t hi = 100;
    static constexpr elem_t lo = 0;
    static constexpr std::size_t n_bins = 5;
    static constexpr elem_t def = 0;
    using val_t = elem_t;
    val_t v = def;
};

struct IntArray {
    using elem_t = int;
    static constexpr elem_t hi = 100;
    static constexpr elem_t lo = 0;
    static constexpr std::size_t n_bins = 5;
    static constexpr elem_t def = 0;
    using val_t = std::array<elem_t, 10>;
    val_t v = ga::init_array<val_t>(def);
};

struct Bool {
    using elem_t = bool;
    static constexpr elem_t def = false;
    using val_t = elem_t;
    val_t v = def;

    static elem_t random_elem() {
        static std::bernoulli_distribution d(0.6);
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
    static constexpr std::size_t option_index(int x) {
        constexpr std::array<int, 5> dic = { 0, 0, 1, 2, 3 };
        return static_cast<std::size_t>(x) >= dic.size() ? 0 : dic[x];
    }
    using val_t = elem_t;
    val_t v = def;
};

struct EnumArray {
    using elem_t = int;
    static constexpr elem_t def = 1;
    static constexpr auto options = std::to_array<elem_t>({1, 2, 3, 4});
    static constexpr std::size_t option_index(int x) {
        constexpr std::array<int, 5> dic = { 0, 0, 1, 2, 3 };
        return static_cast<std::size_t>(x) >= dic.size() ? 0 : dic[x];
    }
    using val_t = std::array<elem_t, 10>;
    val_t v = ga::init_array<val_t>(def);
};


namespace ga_test {

using std::cerr;
using std::cout;
using std::endl;
using std::size_t;

struct TestRunner {
    int checks = 0;
    int fails = 0;

    void check(bool ok, std::string_view expr, std::string_view file, int line, std::string_view msg = {}) {
        ++checks;
        if (!ok) {
            ++fails;
            cerr << "[FAIL] " << file << ":" << line << "  " << expr;
            if (!msg.empty()) cerr << "  (" << msg << ")";
            cerr << "\n";
        }
    }

    template<class A, class B>
    void eq(const A& a, const B& b, std::string_view expr, std::string_view file, int line) {
        ++checks;
        if (!(a == b)) {
            ++fails;
            cerr << "[FAIL] " << file << ":" << line << "  " << expr
                 << "  lhs=" << a << " rhs=" << b << "\n";
        }
    }

    void near(double a, double b, double eps, std::string_view expr, std::string_view file, int line) {
        ++checks;
        if (!(std::abs(a - b) <= eps)) {
            ++fails;
            cerr << "[FAIL] " << file << ":" << line << "  " << expr
                 << "  lhs=" << std::setprecision(17) << a
                 << " rhs=" << std::setprecision(17) << b
                 << " eps=" << eps << "\n";
        }
    }
};

#define GA_CHECK(tr, expr) (tr).check((expr), #expr, __FILE__, __LINE__)
#define GA_CHECK_MSG(tr, expr, msg) (tr).check((expr), #expr, __FILE__, __LINE__, (msg))
#define GA_EQ(tr, a, b) (tr).eq((a), (b), #a " == " #b, __FILE__, __LINE__)
#define GA_NEAR(tr, a, b, eps) (tr).near((a), (b), (eps), #a " ~= " #b, __FILE__, __LINE__)

struct RngGuard {
    std::mt19937 saved;
    explicit RngGuard(uint32_t seed) : saved(ga::rng) { ga::rng.seed(seed); }
    ~RngGuard() { ga::rng = saved; }
};

static std::string bar(double x, int width = 40) {
    if (x < 0) x = 0;
    if (x > 1) x = 1;
    const int k = (int)std::lround(x * width);
    std::string s;
    s.reserve((size_t)width);
    for (int i = 0; i < width; ++i) s.push_back(i < k ? '#' : '.');
    return s;
}

// --------------------------------------------
// Generic helpers for binning (mirrors ga::Diversity rules)
// --------------------------------------------
template<class Spec>
static size_t enum_index_of(ga::elem_of_t<typename Spec::val_t> x) {
    using E = ga::elem_of_t<typename Spec::val_t>;
    if constexpr (requires { Spec::option_index(x); }) {
        return static_cast<std::size_t>(Spec::option_index(x));
    } else {
        constexpr auto& opt = Spec::options;
        constexpr size_t M = ga::n_options_v<Spec>;
        for (size_t i = 0; i < M; ++i) if (opt[i] == (E)x) return i;
        return 0;
    }
}

template<class Spec>
static size_t bin_of_elem(ga::elem_of_t<typename Spec::val_t> x) {
    using V = typename Spec::val_t;
    using E = ga::elem_of_t<V>;
    constexpr size_t nb = ga::n_bins_v<Spec>;

    if constexpr (ga::is_enum_v<Spec>) {
        size_t idx = enum_index_of<Spec>((E)x);
        if (idx >= nb) idx = nb - 1;
        return idx;
    } else if constexpr (std::is_same_v<std::remove_cv_t<E>, bool>) {
        return x ? 1u : 0u;
    } else if constexpr (ga::is_perm_v<Spec>) {
        if constexpr (std::is_signed_v<E>) {
            if (x < 0) return 0;
        }
        // permutation: bin 数を節約するため mod で潰す（nb=min(len,10)）
        return static_cast<size_t>(x) % nb;
    } else if constexpr (ga::NumericRangeSpec<Spec>) {
        const double lo0 = (double)Spec::lo;
        const double hi0 = (double)Spec::hi;
        double lo = lo0, hi = hi0;
        if (hi < lo) {
            const double tmp = lo;
            lo = hi;
            hi = tmp;
        }
        const double span = hi - lo;
        if (!(span > 0.0)) return 0;

        const double xv = (double)x;
        if constexpr (std::is_floating_point_v<E>) {
            if (!std::isfinite(xv)) return 0;
        }

        const double pos = (xv - lo) * ((double)nb / span);

        if (pos <= 0.0) return 0;
        if (pos >= (double)nb) return nb - 1;
        return (size_t)pos;
    } else {
        static_assert(ga::dependent_false_v<Spec>, "bin_of_elem: unsupported Spec/elem type");
        return 0;
    }
}

template<class Spec>
static double normalized_diversity_for_population(const std::vector<Spec>& specs) {
    using V = typename Spec::val_t;
    using E = ga::elem_of_t<V>;
    constexpr size_t len = ga::static_len_v<V>;
    constexpr size_t nb  = ga::n_bins_v<Spec>;

    std::array<std::bitset<nb>, len> present{};
    for (auto& p : present) p.reset();

    for (const auto& s : specs) {
        ga::visit_elems_val(s, [&](auto elem, std::size_t i) {
            present[i].set(bin_of_elem<Spec>((E)elem));
        });
    }

    // score_i = (present_i-1)/(nb-1) （nb>1 && present_i>1） else 0
    if constexpr (nb <= 1) return 0.0;
    constexpr double inv = 1.0 / (double)(nb - 1);

    double sum = 0.0;
    for (size_t i = 0; i < len; ++i) {
        const size_t p = present[i].count();
        if (p > 1) sum += (double)(p - 1) * inv;
    }
    return sum / (double)len;
}

template<class Gene, size_t... I>
static double diversity_rate_manual_impl(const std::vector<Gene>& genes, std::index_sequence<I...>) {
    const double inv_gn = 1.0 / (double)std::tuple_size_v<Gene>;
    double sum = 0.0;

    auto one = [&](auto tag) {
        constexpr size_t J = decltype(tag)::value;
        using Spec = std::tuple_element_t<J, Gene>;

        // Collect only this Spec from each gene for bin counting.
        std::vector<Spec> tmp;
        tmp.reserve(genes.size());
        for (const auto& g : genes) tmp.push_back(std::get<J>(g));

        sum += normalized_diversity_for_population<Spec>(tmp) * inv_gn;
    };

    (one(std::integral_constant<size_t, I>{}), ...);
    return sum;
}

template<class Gene>
static double diversity_rate_manual(const std::vector<Gene>& genes) {
    return diversity_rate_manual_impl(genes, std::make_index_sequence<std::tuple_size_v<Gene>>{});
}

template<class Spec>
static bool elem_in_domain(ga::elem_of_t<typename Spec::val_t> x) {
    using E = ga::elem_of_t<typename Spec::val_t>;
    if constexpr (std::is_same_v<std::remove_cv_t<E>, bool>) {
        return (x == false) || (x == true);
    } else if constexpr (ga::is_enum_v<Spec>) {
        constexpr auto& opt = Spec::options;
        constexpr size_t M = ga::n_options_v<Spec>;
        for (size_t i = 0; i < M; ++i) if ((E)opt[i] == (E)x) return true;
        return false;
    } else if constexpr (std::is_integral_v<E> || std::is_floating_point_v<E>) {
        E lo = (E)Spec::lo;
        E hi = (E)Spec::hi;
        if (hi < lo) std::swap(lo, hi);
        return !(x < lo) && !(hi < x);
    } else {
        return true;
    }
}

// --------------------------------------------
// Specs for edge cases / scenarios
// --------------------------------------------
struct BoolTrue {
    using elem_t = bool;
    static constexpr elem_t def = true;
    using val_t = elem_t;
    val_t v = def;
};

struct DegenerateRange {
    using elem_t = int;
    static constexpr elem_t lo = 5;
    static constexpr elem_t hi = 5;      // hi == lo (degenerate)
    static constexpr std::size_t n_bins = 7;
    static constexpr elem_t def = 5;
    using val_t = elem_t;
    val_t v = def;
};

struct ReversedRange {
    using elem_t = int;
    static constexpr elem_t lo = 10;
    static constexpr elem_t hi = 0;      // hi < lo (reversed)
    static constexpr std::size_t n_bins = 5;
    static constexpr elem_t def = 0;
    using val_t = elem_t;
    val_t v = def;
};

struct HorizonIntArray {
    using elem_t = int;
    static constexpr elem_t lo = 0;
    static constexpr elem_t hi = 9;
    static constexpr std::size_t n_bins = 5;
    static constexpr elem_t def = 0;
    static constexpr bool is_horizon = true;
    using val_t = std::array<elem_t, 8>;
    val_t v = ga::init_array<val_t>(def);
};

struct HorizonBitset {
    using elem_t = bool;
    static constexpr elem_t def = false;
    static constexpr bool is_horizon = true;
    using val_t = std::bitset<8>;
    val_t v = ga::init_bs<val_t>(def);
};

struct EnumNoIndex {
    using elem_t = int;
    static constexpr elem_t def = 1;
    static constexpr auto options = std::to_array<elem_t>({1, 3, 7});
    using val_t = elem_t;
    val_t v = def;
};

struct EnumNoIndexArray {
    using elem_t = int;
    static constexpr elem_t def = 1;
    static constexpr auto options = std::to_array<elem_t>({1, 3, 7});
    using val_t = std::array<elem_t, 5>;
    val_t v = ga::init_array<val_t>(def);
};

// --------------------------------------------
// Operations (Selection / Crossover / Mutation) - sample implementations
// (No include of ga_ops.hpp; this is a self-contained test/demo.)
// --------------------------------------------
template<ga::GeneLike Gene>
struct TournamentSelection {
    int k = 3;
    void prepare(const ga::Population<Gene>&) const {}

    int apply(const ga::Population<Gene>& pop) const {
        const int n = pop.size();
        if (n <= 0) return -1;
        std::uniform_int_distribution<int> dist(0, n - 1);
        if (k <= 1) return dist(ga::rng);

        int best = dist(ga::rng);
        for (int i = 1; i < k; ++i) {
            int cand = dist(ga::rng);
            if (pop.better(cand, best)) best = cand;
        }
        return best;
    }
};

template<ga::GeneLike Gene>
struct ReverseTournamentSelection {
    int k = 3;
    void prepare(const ga::Population<Gene>&) const {}

    int apply(const ga::Population<Gene>& pop) const {
        const int n = pop.size();
        if (n <= 0) return -1;
        std::uniform_int_distribution<int> dist(0, n - 1);
        if (k <= 1) return dist(ga::rng);

        int worst = dist(ga::rng);
        for (int i = 1; i < k; ++i) {
            int cand = dist(ga::rng);
            if (pop.better(worst, cand)) worst = cand;
        }
        return worst;
    }
};

template<class Spec>
struct CopyCrossover {
    using S = Spec;
    void apply(const Spec& p1, const Spec& /*p2*/, Spec& c) const { c.v = p1.v; }
    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const { c1.v = p1.v; c2.v = p2.v; }
};

template<class Spec>
struct PickParentCrossover {
    using S = Spec;
    void apply(const Spec& p1, const Spec& p2, Spec& c) const {
        std::bernoulli_distribution pick(0.5);
        c.v = pick(ga::rng) ? p1.v : p2.v;
    }
    void apply(const Spec& p1, const Spec& p2, Spec& c1, Spec& c2) const {
        std::bernoulli_distribution pick(0.5);
        c1.v = pick(ga::rng) ? p1.v : p2.v;
        c2.v = pick(ga::rng) ? p1.v : p2.v;
    }
};

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
    double p_reset = 0.01;

    void apply(Spec& s) const {
        std::bernoulli_distribution do_reset(p_reset);
        ga::visit_elems_ref(s, [&](auto&& elem, std::size_t /*i*/) {
            if (do_reset(ga::rng)) elem = ga::random_elem_value<Spec>();
        });
    }
};

// --------------------------------------------
// Pretty print
// --------------------------------------------
template<class Spec>
static void print_spec(std::ostream& os, const Spec& s) {
    using V = typename Spec::val_t;
    if constexpr (ga::is_bs_v<V>) {
        os << s.v;
    } else if constexpr (ga::is_arr_v<V>) {
        os << "[";
        constexpr size_t N = ga::static_len_v<V>;
        for (size_t i = 0; i < N; ++i) {
            if (i) os << ",";
            os << s.v[i];
        }
        os << "]";
    } else {
        os << s.v;
    }
}

template<class Gene, size_t... I>
static void print_gene_impl(std::ostream& os, const Gene& g, std::index_sequence<I...>) {
    ((os << (I ? " | " : ""), print_spec(os, std::get<I>(g))), ...);
}

template<class Gene>
static std::string gene_to_string(const Gene& g) {
    std::ostringstream oss;
    print_gene_impl(oss, g, std::make_index_sequence<std::tuple_size_v<Gene>>{});
    return oss.str();
}

// --------------------------------------------
// Fitness examples
// --------------------------------------------

template<class Gene>
struct FitnessA;

using GeneA = std::tuple<Float, Int, Enum, Bitset>;

template<>
struct FitnessA<GeneA> {
    static double eval(const GeneA& g) {
        const double f = std::get<0>(g).v;
        const double i = (double)std::get<1>(g).v;
        const double e = (double)std::get<2>(g).v;
        const double ones = (double)std::get<3>(g).v.count();

        // Targets: f->100, i->100, e->4, ones->10
        const double s =
            1.0 - std::abs(f - 100.0) / 100.0 +
            1.0 - std::abs(i - 100.0) / 100.0 +
            (e / 4.0) +
            (ones / 10.0);
        return s;
    }
};

using GeneB = std::tuple<FloatArray, EnumArray, Bitset, ReversedRange, DegenerateRange, EnumNoIndex, EnumNoIndexArray>;

template<>
struct FitnessA<GeneB> {
    static double eval(const GeneB& g) {
        const auto& fa = std::get<0>(g).v;
        const auto& ea = std::get<1>(g).v;
        const auto& bs = std::get<2>(g).v;
        const int rr = std::get<3>(g).v;
        const int dr = std::get<4>(g).v;
        const int en = std::get<5>(g).v;
        const auto& ena = std::get<6>(g).v;

        double sum = 0.0;
        for (float x : fa) sum += 1.0 - std::abs((double)x - 100.0) / 100.0;
        for (int x : ea) sum += (double)x / 4.0;             // prefer 4
        sum += (double)bs.count() / 10.0;                    // prefer ones
        sum += 1.0 - std::abs((double)rr - 10.0) / 10.0;     // prefer 10 (note: reversed range spec)
        sum += (double)dr / 5.0;                             // degenerate always 5
        sum += (en == 7 ? 1.0 : 0.0);                        // prefer 7
        for (int x : ena) sum += (x == 7 ? 0.2 : 0.0);
        return sum;
    }
};

using GeneH = std::tuple<HorizonIntArray, HorizonBitset, Int>;

template<>
struct FitnessA<GeneH> {
    static double eval(const GeneH& g) {
        // Prefer increasing plan values and many ones in horizon.
        const auto& plan = std::get<0>(g).v;
        const auto& bs = std::get<1>(g).v;
        const int x = std::get<2>(g).v;

        double s = 0.0;
        for (size_t i = 0; i < plan.size(); ++i) s += (double)plan[i] / 9.0 * (1.0 + 0.1 * (double)i);
        s += (double)bs.count() / (double)bs.size();
        s += (double)x / 100.0;
        return s;
    }
};

// --------------------------------------------
// Assertions about library traits (compile-time)
// --------------------------------------------
static void test_traits_compile_time() {
    static_assert(ga::is_arr_v<std::array<int, 3>>);
    static_assert(!ga::is_arr_v<int>);
    static_assert(ga::is_bs_v<std::bitset<7>>);
    static_assert(!ga::is_bs_v<std::array<int, 2>>);

    static_assert(ga::static_len_v<int> == 1);
    static_assert(ga::static_len_v<std::array<int, 5>> == 5);
    static_assert(ga::static_len_v<std::bitset<9>> == 9);

    static_assert(std::is_same_v<ga::elem_of_t<int>, int>);
    static_assert(std::is_same_v<ga::elem_of_t<std::array<float, 2>>, float>);
    static_assert(std::is_same_v<ga::elem_of_t<std::bitset<2>>, bool>);

    static_assert(ga::n_bins_v<Bool> == 2);
    static_assert(ga::n_bins_v<Enum> == ga::n_options_v<Enum>);
    static_assert(ga::n_bins_v<Int> == (size_t)Int::n_bins);
}

// --------------------------------------------
// Unit tests for small helpers / utilities
// --------------------------------------------
static void test_low_mask(TestRunner& tr) {
    std::bitset<8> m0 = ga::low_mask<8>(0);
    std::bitset<8> m1 = ga::low_mask<8>(1);
    std::bitset<8> m8 = ga::low_mask<8>(8);
    std::bitset<8> m9 = ga::low_mask<8>(9);

    GA_EQ(tr, m0.to_ulong(), 0u);
    GA_EQ(tr, m1.to_ulong(), 1u);
    GA_EQ(tr, m8.to_ulong(), 255u);
    GA_EQ(tr, m9.to_ulong(), 255u);
}

static void test_visit_elems_for_each(TestRunner& tr) {
    {
        Int s;
        s.v = 7;
        int seen = 0;
        ga::visit_elems_ref(s, [&](auto& e, std::size_t idx) {
            GA_EQ(tr, idx, (size_t)0);
            e += 1;
            ++seen;
        });
        GA_EQ(tr, seen, 1);
        GA_EQ(tr, s.v, 8);
    }
    {
        IntArray a;
        for (size_t i = 0; i < a.v.size(); ++i) a.v[i] = (int)i;

        size_t sum_i = 0;
        ga::visit_elems_ref(a, [&](auto& e, std::size_t idx) {
            GA_EQ(tr, (size_t)e, idx);
            sum_i += idx;
            e = (int)(e + 10);
        });
        GA_EQ(tr, sum_i, (size_t)45);
        GA_EQ(tr, a.v[0], 10);
        GA_EQ(tr, a.v[9], 19);
    }
    {
        Bitset bs;
        bs.v.reset();
        int cnt = 0;

        ga::visit_elems_ref(bs, [&](auto e, std::size_t i) {
            if (i % 2 == 0) e = true;
            cnt++;
        });

        GA_EQ(tr, cnt, (int)bs.v.size());
        GA_EQ(tr, bs.v.count(), (size_t)5);

        size_t cnt_const_true = 0;
        const Bitset& cbs = bs;
        ga::visit_elems_val(cbs, [&](auto bit, std::size_t /*i*/) { if (bit) ++cnt_const_true; });
        GA_EQ(tr, cnt_const_true, (size_t)5);
    }
}

static void test_init_default_random(TestRunner& tr) {
    {
        BoolTrue b;
        ga::init_default(b);
        GA_CHECK(tr, b.v == true);
    }
    {
        Bitset b;
        b.v.set();
        ga::init_default(b);
        GA_EQ(tr, b.v.count(), (size_t)0);
    }
    {
        IntArray a;
        for (auto& x : a.v) x = 999;
        ga::init_default(a);
        for (auto x : a.v) GA_EQ(tr, x, 0);
    }

    {
        RngGuard rg(123);
        ReversedRange r;
        for (int it = 0; it < 1000; ++it) {
            int x = ga::random_elem_value<ReversedRange>();
            GA_CHECK(tr, elem_in_domain<ReversedRange>(x));
        }
    }
    {
        RngGuard rg(456);
        EnumNoIndex e;
        for (int it = 0; it < 200; ++it) {
            int x = ga::random_elem_value<EnumNoIndex>();
            GA_CHECK(tr, elem_in_domain<EnumNoIndex>(x));
        }
    }
    {
        // init_random: all elements should satisfy domain constraints
        RngGuard rg(42);
        GeneB g{};
        ga::init_gene_random(g);

        // FloatArray elements in [0,100]
        ga::visit_elems_ref(std::get<0>(g), [&](auto elem, std::size_t) {
            GA_CHECK(tr, elem_in_domain<Float>(elem)); // same range
        });

        // EnumArray is in {1,2,3,4}
        ga::visit_elems_ref(std::get<1>(g), [&](auto elem, std::size_t) {
            GA_CHECK(tr, elem_in_domain<Enum>(elem));
        });

        // Bitset bool ok
        GA_CHECK(tr, std::get<2>(g).v.count() <= std::get<2>(g).v.size());

        // ReversedRange in [0,10] due to swap in random_elem_value
        GA_CHECK(tr, elem_in_domain<ReversedRange>(std::get<3>(g).v));

        // DegenerateRange always 5
        GA_EQ(tr, std::get<4>(g).v, 5);

        // EnumNoIndex
        GA_CHECK(tr, elem_in_domain<EnumNoIndex>(std::get<5>(g).v));

        // EnumNoIndexArray
        ga::visit_elems_ref(std::get<6>(g), [&](auto elem, std::size_t) {
            GA_CHECK(tr, elem_in_domain<EnumNoIndex>(elem));
        });
    }
}

static void test_gene_init_and_ops(TestRunner& tr) {
    RngGuard rg(1);

    using Gene = GeneA;
    Gene p1{}, p2{}, c{}, c1{}, c2{};
    ga::init_gene_random(p1);
    ga::init_gene_random(p2);
    ga::init_gene_default(c);

    // Crossovers: numeric=arithmetic, enum=pick parent, bitset=one-point
    auto cross = std::make_tuple(
        ArithmeticCrossover<Float>{0.5},
        ArithmeticCrossover<Int>{0.5},
        PickParentCrossover<Enum>{},
        OnePointCrossover<Bitset>{}
    );

    ga::apply_crossover(cross, p1, p2, c);
    GA_CHECK(tr, elem_in_domain<Float>(std::get<0>(c).v));
    GA_CHECK(tr, elem_in_domain<Int>(std::get<1>(c).v));
    GA_CHECK(tr, elem_in_domain<Enum>(std::get<2>(c).v));

    ga::apply_crossover(cross, p1, p2, c1, c2);
    GA_CHECK(tr, elem_in_domain<Float>(std::get<0>(c1).v));
    GA_CHECK(tr, elem_in_domain<Float>(std::get<0>(c2).v));

    // Mutation: random reset w/ higher rate for testing
    auto muts = std::make_tuple(
        RandomResetMutation<Float>{0.5},
        RandomResetMutation<Int>{0.5},
        RandomResetMutation<Enum>{0.5},
        RandomResetMutation<Bitset>{0.5}
    );

    Gene m = c;
    ga::apply_mutation(muts, m);

    GA_CHECK(tr, elem_in_domain<Float>(std::get<0>(m).v));
    GA_CHECK(tr, elem_in_domain<Int>(std::get<1>(m).v));
    GA_CHECK(tr, elem_in_domain<Enum>(std::get<2>(m).v));
    GA_CHECK(tr, std::get<3>(m).v.count() <= std::get<3>(m).v.size());
}

static void test_diversity_basic(TestRunner& tr) {
    using Gene = GeneB;

    // Case 1: all default => very low diversity.
    {
        std::vector<Gene> genes(50);
        for (auto& g : genes) ga::init_gene_default(g);

        ga::Diversity<Gene> d;
        d.rebuild(genes);

        const double manual = diversity_rate_manual(genes);
        GA_NEAR(tr, d.diversity_rate, manual, 1e-6);

        GA_CHECK(tr, d.diversity_rate >= 0.0f);
        GA_CHECK(tr, d.diversity_rate <= 1.0f);
    }

    // Case 2: random => higher diversity, and rebuild == incremental
    {
        RngGuard rg(7);
        std::vector<Gene> genes(200);
        for (auto& g : genes) ga::init_gene_random(g);

        ga::Diversity<Gene> d1;
        d1.rebuild(genes);

        ga::Diversity<Gene> d2;
        d2.reset();
        for (const auto& g : genes) d2.update(g, +1);

        GA_NEAR(tr, d1.diversity_rate, d2.diversity_rate, 1e-6);
        GA_NEAR(tr, d1.diversity_rate, diversity_rate_manual(genes), 1e-6);

        // Removing everything returns to 0.
        for (const auto& g : genes) d2.update(g, -1);
        GA_NEAR(tr, d2.diversity_rate, 0.0, 1e-6);
    }

    // Case 3: update(+1) twice doesn't change diversity, then -1 twice restores.
    {
        RngGuard rg(9);
        Gene g{};
        ga::init_gene_random(g);

        ga::Diversity<Gene> d;
        d.reset();
        d.update(g, +1);
        const double a = d.diversity_rate;
        d.update(g, +1);
        const double b = d.diversity_rate;
        GA_NEAR(tr, a, b, 0.0);

        d.update(g, -1);
        const double c = d.diversity_rate;
        GA_NEAR(tr, c, a, 0.0);

        d.update(g, -1);
        GA_NEAR(tr, d.diversity_rate, 0.0, 1e-6);
    }
}

static void test_population_basic(TestRunner& tr) {
    using Gene = GeneA;
    RngGuard rg(1234);

    ga::Population<Gene> pop;
    GA_EQ(tr, pop.gene().size(), (size_t)0);
    GA_EQ(tr, pop.size(), 0);
    GA_EQ(tr, pop.current_tick(), 0);

    Gene g0{};
    ga::init_gene_random(g0);
    const int i0 = pop.add(g0, 1.0);
    GA_EQ(tr, i0, 0);
    GA_EQ(tr, pop.gene().size(), (size_t)1);
    GA_EQ(tr, pop.fitness().size(), (size_t)1);
    GA_EQ(tr, pop.born_at().size(), (size_t)1);
    GA_EQ(tr, pop.born_at(0), 0);
    GA_NEAR(tr, pop.diversity.diversity_rate, diversity_rate_manual(pop.gene()), 1e-6);

    // add() updates born_at to current_tick
    pop.advance_tick(10);
    GA_EQ(tr, pop.current_tick(), 10);

    Gene g1{};
    ga::init_gene_random(g1);
    const int i1 = pop.add(std::move(g1), 2.0);
    GA_EQ(tr, i1, 1);
    GA_EQ(tr, pop.born_at(1), 10);
    GA_CHECK(tr, pop.better(1, 0)); // fitness 2.0 > 1.0
    GA_NEAR(tr, pop.diversity.diversity_rate, diversity_rate_manual(pop.gene()), 1e-6);

    // set() updates diversity and born_at (= current_tick)
    pop.advance_tick(5);
    GA_EQ(tr, pop.current_tick(), 15);

    Gene g2{};
    ga::init_gene_default(g2);
    pop.set(0, g2, 2.0);
    GA_EQ(tr, pop.fitness(0), 2.0);
    GA_EQ(tr, pop.born_at(0), 15);
    GA_NEAR(tr, pop.diversity.diversity_rate, diversity_rate_manual(pop.gene()), 1e-6);

    // tie-break by born_at (larger born_at is better)
    pop.set_fitness(0, 10.0);
    pop.set_fitness(1, 10.0);

    pop.advance_tick(1); // make index 1 newer via set()
    pop.set(1, pop.gene(1), 10.0);
    GA_CHECK(tr, pop.born_at(1) > pop.born_at(0));
    GA_CHECK(tr, pop.better(1, 0));

    // advance_tick changes only current_tick (born_at does not change)
    const int b0 = pop.born_at(0);
    const int b1 = pop.born_at(1);
    pop.advance_tick(3);
    GA_EQ(tr, pop.current_tick(), 19);
    GA_EQ(tr, pop.born_at(0), b0);
    GA_EQ(tr, pop.born_at(1), b1);

    // rebuild_diversity is consistent
    const double before = pop.diversity.diversity_rate;
    pop.rebuild_diversity();
    GA_NEAR(tr, pop.diversity.diversity_rate, before, 1e-6);

    // apply_rolling_horizon on non-horizon specs does not change
    auto g_old = pop.gene(0);
    pop.apply_rolling_horizon([](Gene&, int, int) {}, 2);
    GA_EQ(tr, gene_to_string(pop.gene(0)), gene_to_string(g_old));
}




// A safer comparison helper for rolling horizon (avoid tuple::get confusion).
static void test_population_rolling_horizon2(TestRunner& tr) {
    using Gene = GeneH;

    ga::Population<Gene> pop;
    Gene g{};
    ga::init_gene_default(g);
    for (size_t i = 0; i < std::get<0>(g).v.size(); ++i) std::get<0>(g).v[i] = (int)i;
    std::get<1>(g).v.reset();
    std::get<1>(g).v.set(7, true);
    std::get<2>(g).v = 42;

    pop.add(g, FitnessA<Gene>::eval(g));

    pop.apply_rolling_horizon([](Gene&, int, int) {}, 3);
    const auto after = pop.gene(0);

    // HorizonIntArray: [0,1,2,3,4,5,6,7] -> shift 3 -> [3,4,5,6,7,0,0,0]
    GA_EQ(tr, std::get<0>(after).v[0], 3);
    GA_EQ(tr, std::get<0>(after).v[4], 7);
    GA_EQ(tr, std::get<0>(after).v[5], 0);
    GA_EQ(tr, std::get<0>(after).v[7], 0);

    // HorizonBitset: bit7 set -> shift 3 -> bit4 set
    GA_EQ(tr, std::get<1>(after).v.test(4), true);
    GA_EQ(tr, std::get<1>(after).v.count(), (size_t)1);

    // Int (non-horizon) unchanged
    GA_EQ(tr, std::get<2>(after).v, 42);
}

static void test_population_rolling_horizon_random_fill(TestRunner& tr) {
    using Gene = GeneH;

    ga::Population<Gene> pop;
    Gene g{};
    ga::init_gene_default(g);
    for (size_t i = 0; i < std::get<0>(g).v.size(); ++i) std::get<0>(g).v[i] = (int)i;
    std::get<1>(g).v.reset();
    std::get<1>(g).v.set(7, true);
    std::get<2>(g).v = 42;

    pop.add(g, FitnessA<Gene>::eval(g));

    constexpr int shift = 3;
    std::array<int, shift> expect_tail{};
    std::array<bool, shift> expect_bits{};

    {
        // deterministic
        RngGuard guard(12345);
        pop.apply_rolling_horizon([](Gene&, int, int) {}, shift, true);
    }
    {
        // reproduce the exact random sequence used to fill the vacated tail
        RngGuard guard(12345);
        for (int j = 0; j < shift; ++j) expect_tail[(size_t)j] = ga::random_elem_value<HorizonIntArray>();
        for (int j = 0; j < shift; ++j) expect_bits[(size_t)j] = static_cast<bool>(ga::random_elem_value<HorizonBitset>());
    }

    const auto& after = pop.gene(0);

    // HorizonIntArray: [0,1,2,3,4,5,6,7] -> shift 3 -> [3,4,5,6,7,?, ?, ?]
    GA_EQ(tr, std::get<0>(after).v[0], 3);
    GA_EQ(tr, std::get<0>(after).v[4], 7);
    GA_EQ(tr, std::get<0>(after).v[5], expect_tail[0]);
    GA_EQ(tr, std::get<0>(after).v[6], expect_tail[1]);
    GA_EQ(tr, std::get<0>(after).v[7], expect_tail[2]);

    // HorizonBitset: bit7 set -> shift 3 -> bit4 set, and vacated bits [5..7] are random-filled
    GA_EQ(tr, std::get<1>(after).v.test(4), true);
    GA_EQ(tr, std::get<1>(after).v.test(5), expect_bits[0]);
    GA_EQ(tr, std::get<1>(after).v.test(6), expect_bits[1]);
    GA_EQ(tr, std::get<1>(after).v.test(7), expect_bits[2]);

    // Int (non-horizon) unchanged
    GA_EQ(tr, std::get<2>(after).v, 42);
}



// --------------------------------------------
// Scenario tests + demo (Population & Diversity)
// --------------------------------------------
template<class Gene, class Cross, class Mut, class Sel, class ReplaceSel, class Fit>
static void evolve_demo(
    const char* title,
    int pop_n,
    int generations,
    const Cross& cross,
    const Mut& mut,
    Sel sel,
    ReplaceSel replace_sel,
    Fit&& fit_eval,
    int report_every = 5
) {
    cout << "\n=== Demo: " << title << " ===\n";
    ga::Population<Gene> pop;
    pop.reserve(pop_n);

    // init
    for (int i = 0; i < pop_n; ++i) {
        Gene g{};
        ga::init_gene_random(g);
        pop.add(std::move(g), 0.0);
    }
    for (int i = 0; i < pop_n; ++i) pop.set_fitness(i, fit_eval(pop.gene(i)));
    pop.rebuild_diversity();

    auto report = [&](int gen) {
        int best = 0;
        double sum = 0;
        const auto& fit = pop.fitness();
        for (int i = 0; i < pop_n; ++i) {
            sum += fit[(std::size_t)i];
            if (pop.better(i, best)) best = i;
        }
        const double avg = sum / (double)pop_n;
        cout << "gen " << std::setw(3) << gen
             << "  best=" << std::setw(10) << std::fixed << std::setprecision(4) << fit[(std::size_t)best]
             << "  avg="  << std::setw(10) << std::fixed << std::setprecision(4) << avg
             << "  div="  << std::setw(7)  << std::fixed << std::setprecision(4) << pop.diversity.diversity_rate
             << "  " << bar(pop.diversity.diversity_rate, 30)
             << "\n";

        if (gen == 0 || gen == generations || gen % (report_every * 4) == 0) {
            cout << "      best gene: " << gene_to_string(pop.gene(best)) << "\n";
        }
    };

    sel.prepare(pop);
    replace_sel.prepare(pop);

    report(0);

    for (int gen = 1; gen <= generations; ++gen) {
        // advance time for this generation (newborn individuals get born_at=current_tick)
        pop.advance_tick(1);

        // steady-state: 2 parents -> 1 child
        const int p1 = sel.apply(pop);
        const int p2 = sel.apply(pop);

        Gene child{};
        ga::apply_crossover(cross, pop.gene(p1), pop.gene(p2), child);
        Gene mutated{};
        mutated = child;
        ga::apply_mutation(mut, mutated);

        const double f = fit_eval(mutated);

        const int victim = replace_sel.apply(pop);
        pop.set(victim, std::move(mutated), f);

        // Occasionally inject random immigrant (diversity scenario)
        if ((gen % 37) == 0) {
            Gene immigrant{};
            ga::init_gene_random(immigrant);
            const double fi = fit_eval(immigrant);
            const int v = replace_sel.apply(pop);
            pop.set(v, std::move(immigrant), fi);
        }

        if (gen % report_every == 0 || gen == generations) report(gen);
    }

    cout << "=== End Demo: " << title << " ===\n";
}


static void scenario_tests_and_demos() {
    {
        RngGuard rg(20260127);

        // Demo 1: mixed scalar/enum/bitset
        auto cross = std::make_tuple(
            ArithmeticCrossover<Float>{0.7},
            ArithmeticCrossover<Int>{0.7},
            PickParentCrossover<Enum>{},
            OnePointCrossover<Bitset>{}
        );
        auto mut = std::make_tuple(
            RandomResetMutation<Float>{0.05},
            RandomResetMutation<Int>{0.05},
            RandomResetMutation<Enum>{0.03},
            RandomResetMutation<Bitset>{0.02}
        );

        evolve_demo<GeneA>(
            "GeneA (Float/Int/Enum/Bitset) - optimize towards targets",
            200, 120, cross, mut,
            TournamentSelection<GeneA>{3},
            ReverseTournamentSelection<GeneA>{3},
            [](const GeneA& g) { return FitnessA<GeneA>::eval(g); },
            10
        );
    }

    {
        RngGuard rg(20260128);

        // Demo 2: large gene with edge-case specs; observe diversity stabilizing and fitness improving.
        auto cross = std::make_tuple(
            OnePointCrossover<FloatArray>{},
            OnePointCrossover<EnumArray>{},
            OnePointCrossover<Bitset>{},
            ArithmeticCrossover<ReversedRange>{0.5},
            CopyCrossover<DegenerateRange>{},
            PickParentCrossover<EnumNoIndex>{},
            OnePointCrossover<EnumNoIndexArray>{}
        );
        auto mut = std::make_tuple(
            RandomResetMutation<FloatArray>{0.02},
            RandomResetMutation<EnumArray>{0.02},
            RandomResetMutation<Bitset>{0.02},
            RandomResetMutation<ReversedRange>{0.05},
            RandomResetMutation<DegenerateRange>{0.00},
            RandomResetMutation<EnumNoIndex>{0.02},
            RandomResetMutation<EnumNoIndexArray>{0.02}
        );

        evolve_demo<GeneB>(
            "GeneB (array/bitset/enum + reversed/degenerate ranges) - diversity monitoring",
            180, 80, cross, mut,
            TournamentSelection<GeneB>{4},
            ReverseTournamentSelection<GeneB>{4},
            [](const GeneB& g) { return FitnessA<GeneB>::eval(g); },
            10
        );
    }

    {
        RngGuard rg(20260129);

        // Demo 3: rolling horizon; show that horizon parts shift each gen while GA optimizes.
        using Gene = GeneH;
        auto cross = std::make_tuple(
            OnePointCrossover<HorizonIntArray>{},
            OnePointCrossover<HorizonBitset>{},
            ArithmeticCrossover<Int>{0.6}
        );
        auto mut = std::make_tuple(
            RandomResetMutation<HorizonIntArray>{0.03},
            RandomResetMutation<HorizonBitset>{0.03},
            RandomResetMutation<Int>{0.02}
        );

        cout << "\n=== Demo: GeneH (Rolling Horizon) ===\n";
        ga::Population<Gene> pop;
        const int pop_n = 120;
        const int gens = 50;
        pop.reserve(pop_n);

        for (int i = 0; i < pop_n; ++i) {
            Gene g{};
            ga::init_gene_random(g);
            pop.add(std::move(g), 0.0);
        }
        for (int i = 0; i < pop_n; ++i) pop.set_fitness(i, FitnessA<Gene>::eval(pop.gene(i)));
        pop.rebuild_diversity();

        TournamentSelection<Gene> sel{3};
        ReverseTournamentSelection<Gene> rep{3};
        sel.prepare(pop);
        rep.prepare(pop);

        auto report = [&](int gen) {
            int best = 0;
            double sum = 0;
            const auto& fit = pop.fitness();
            for (int i = 0; i < pop_n; ++i) {
                sum += fit[(std::size_t)i];
                if (pop.better(i, best)) best = i;
            }
            cout << "gen " << std::setw(3) << gen
                 << "  best=" << std::setw(10) << std::fixed << std::setprecision(4) << fit[(std::size_t)best]
                 << "  avg="  << std::setw(10) << std::fixed << std::setprecision(4) << (sum / pop_n)
                 << "  div="  << std::setw(7)  << std::fixed << std::setprecision(4) << pop.diversity.diversity_rate
                 << "  plan0=" << std::get<0>(pop.gene(best)).v[0]
                 << "  bits="  << std::get<1>(pop.gene(best)).v.count()
                 << "\n";
        };

        report(0);

        for (int gen = 1; gen <= gens; ++gen) {
            // advance time for this generation (newborn individuals get born_at=current_tick)
            pop.advance_tick(1);

            // Shift horizon first (use-case: MPC; environment advances by 1 step).
            pop.apply_rolling_horizon([&](Gene& g, int /*index*/, int shift) {
                // Example: after shift, set the last action to something simple (greedy repair).
                // This callback is a typical place to enforce constraints or inject heuristics.
                (void)shift;
                std::get<0>(g).v.back() = 9;
                std::get<1>(g).v.set(std::get<1>(g).v.size() - 1, true);
            }, 1);

            // 1 offspring
            const int p1 = sel.apply(pop);
            const int p2 = sel.apply(pop);
            Gene child{};
            ga::apply_crossover(cross, pop.gene(p1), pop.gene(p2), child);
            Gene mutated{};
            mutated = child;
            ga::apply_mutation(mut, mutated);

            const double f = FitnessA<Gene>::eval(mutated);
            const int victim = rep.apply(pop);
            pop.set(victim, std::move(mutated), f);

            if (gen % 5 == 0 || gen == gens) report(gen);
        }

        cout << "=== End Demo: GeneH (Rolling Horizon) ===\n";
    }
}

// --------------------------------------------
// Micro-benchmarks (quick, deterministic-ish)
// --------------------------------------------
template<class F>
static long long bench_ns(F&& f, int iters) {
    using clock = std::chrono::high_resolution_clock;
    const auto t0 = clock::now();
    for (int i = 0; i < iters; ++i) f();
    const auto t1 = clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
}

static void benchmarks() {
    cout << "\n=== Benchmarks (ns, smaller is better) ===\n";
    RngGuard rg(777);

    using Gene = GeneB;
    Gene g1{}, g2{}, c{};
    ga::init_gene_random(g1);
    ga::init_gene_random(g2);

    auto cross = std::make_tuple(
        OnePointCrossover<FloatArray>{},
        OnePointCrossover<EnumArray>{},
        OnePointCrossover<Bitset>{},
        ArithmeticCrossover<ReversedRange>{0.5},
        CopyCrossover<DegenerateRange>{},
        PickParentCrossover<EnumNoIndex>{},
        OnePointCrossover<EnumNoIndexArray>{}
    );
    auto mut = std::make_tuple(
        RandomResetMutation<FloatArray>{0.02},
        RandomResetMutation<EnumArray>{0.02},
        RandomResetMutation<Bitset>{0.02},
        RandomResetMutation<ReversedRange>{0.05},
        RandomResetMutation<DegenerateRange>{0.00},
        RandomResetMutation<EnumNoIndex>{0.02},
        RandomResetMutation<EnumNoIndexArray>{0.02}
    );

    const int it = 200000;

    const auto t_init = bench_ns([&]() { ga::init_gene_random(c); }, it);
    const auto t_cross = bench_ns([&]() { ga::apply_crossover(cross, g1, g2, c); }, it);
    const auto t_mut = bench_ns([&]() { Gene out = c; ga::apply_mutation(mut, out); }, it);

    ga::Diversity<Gene> div;
    std::vector<Gene> pool(256);
    for (auto& x : pool) ga::init_gene_random(x);
    div.rebuild(pool);

    const auto t_div_upd = bench_ns([&]() { div.update(pool[0], +1); div.update(pool[0], -1); }, it);

    cout << "init_gene_random : " << t_init / it << " ns/op\n";
    cout << "apply_crossover  : " << t_cross / it << " ns/op\n";
    cout << "apply_mutation   : " << t_mut / it << " ns/op\n";
    cout << "diversity update : " << t_div_upd / it << " ns/op\n";
}

} // namespace ga_test

int main() {
    using namespace ga_test;

    cout << "=== ga.hpp test / demo (C++20, single-thread, gcc) ===\n";

    test_traits_compile_time();

    TestRunner tr;
    test_low_mask(tr);
    test_visit_elems_for_each(tr);
    test_init_default_random(tr);
    test_gene_init_and_ops(tr);
    test_diversity_basic(tr);
    test_population_basic(tr);
    test_population_rolling_horizon2(tr);
    test_population_rolling_horizon_random_fill(tr);

    cout << "\n=== Unit Test Summary ===\n";
    cout << "checks: " << tr.checks << "\n";
    cout << "fails : " << tr.fails  << "\n";

    if (tr.fails == 0) cout << "[OK] All tests passed.\n";
    else cout << "[NG] Some tests failed.\n";

    scenario_tests_and_demos();
    benchmarks();

    return tr.fails == 0 ? 0 : 1;
}
#endif