
#include <array>
#include <bitset>
#include <bit>
#include <cassert>
#include <span>
#include <random>
#include <algorithm>
#include <limits>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <utility>
#include <type_traits>

#define main ga_lite_test_main

#include <bits/stdc++.h>

using namespace std;

namespace ga {

inline mt19937 rng(42);

using Fitness = double;

inline constexpr Fitness kNaN = numeric_limits<Fitness>::quiet_NaN();
inline constexpr Fitness kInf = numeric_limits<Fitness>::infinity();

template<class T>
inline void normalize_range(T& lo, T& hi) {
    if (hi < lo) swap(lo, hi);
}

template<class T>
inline T clamp_range(T x, T lo, T hi) {
    normalize_range(lo, hi);
    return clamp(x, lo, hi);
}

inline double rand01() { return generate_canonical<double, 53>(rng); }
inline float  rand01f() { return generate_canonical<float, 24>(rng); }

inline uint32_t rand_u32() {

    return (uint32_t)rng();
}

inline int rand_int(int lo, int hi) {
    if (hi <= lo) return lo;

    const uint64_t range64 = (uint64_t)((int64_t)hi - (int64_t)lo) + 1ull;

    if (range64 == 0ull || range64 > (uint64_t)numeric_limits<uint32_t>::max()) {
        uniform_int_distribution<int> dist(lo, hi);
        return dist(rng);
    }

    const uint32_t range = (uint32_t)range64;

    if ((range & (range - 1u)) == 0u) return lo + (int)(rand_u32() & (range - 1u));

    const uint32_t limit = numeric_limits<uint32_t>::max()
                         - (numeric_limits<uint32_t>::max() % range);

    uint32_t x;
    do { x = rand_u32(); } while (x >= limit);

    return lo + (int)(x % range);
}

template<class Int>
inline Int rand_int(Int lo, Int hi) {
    static_assert(is_integral_v<Int> || is_enum_v<Int>,
                  "rand_int(lo,hi): Int must be integral or enum(int-like).");
    static_assert(!is_same_v<remove_cv_t<Int>, bool>,
                  "rand_int(lo,hi): use rand_bool(p) for bool.");

    if (hi <= lo) return lo;

    if constexpr (is_enum_v<Int>) {
        using Under = underlying_type_t<Int>;
        const Under ulo = (Under)lo;
        const Under uhi = (Under)hi;

        using DistT = conditional_t<
            (sizeof(Under) < sizeof(int)),
            int,
            conditional_t<(sizeof(Under) < sizeof(long long)), long long, Under>
        >;

        uniform_int_distribution<DistT> dist((DistT)ulo, (DistT)uhi);
        return (Int)(Under)dist(rng);
    } else {
        using DistT = conditional_t<
            (sizeof(Int) < sizeof(int)),
            int,
            conditional_t<(sizeof(Int) < sizeof(long long)), long long, Int>
        >;

        uniform_int_distribution<DistT> dist((DistT)lo, (DistT)hi);
        return (Int)dist(rng);
    }
}

template<class Real>
inline Real rand_real(Real lo, Real hi) {
    static_assert(is_floating_point_v<Real>, "rand_real(lo,hi): Real must be floating point.");
    if (!(hi > lo)) return lo;

    if constexpr (is_same_v<Real, float>) return lo + (hi - lo) * rand01f();
    else                                  return lo + (hi - lo) * (Real)rand01();
}

inline bool rand_bool(double p) {
    if (!(p > 0.0)) return false;
    if (p >= 1.0) return true;
    return rand01() < p;
}

template<class Int>
inline Int rand_index(Int n) {
    static_assert(is_integral_v<Int>, "rand_index(n): Int must be integral.");
    if (n <= 1) return 0;
    return rand_int<Int>(0, n - 1);
}

template<class W>
inline int weighted_index(span<const W> weights) {
    const int n = (int)weights.size();
    if (n <= 0) return -1;

    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        const double w = (double)weights[i];
        if (w > 0.0) sum += w;
    }

    if (!(sum > 0.0)) return rand_int(0, n - 1);

    const double r = rand_real(0.0, sum);
    double acc = 0.0;
    for (int i = 0; i < n; ++i) {
        const double w = (double)weights[i];
        if (w > 0.0) {
            acc += w;
            if (r < acc) return i;
        }
    }
    return n - 1;
}

template<class Weights>
inline int weighted_index(const Weights& weights) {
    using W = typename Weights::value_type;
    return weighted_index<W>(span<const W>(weights.data(), weights.size()));
}

inline float rand_uniform(float lo, float hi) { return lo + (hi - lo) * rand01f(); }

inline double now_ms() {
    using clock = chrono::steady_clock;
    return chrono::duration<double, milli>(clock::now().time_since_epoch()).count();
}

struct Stats {
    uint64_t population_set = 0;
    uint64_t population_add = 0;
    uint64_t population_update = 0;

    void reset() { *this = Stats{}; }

    void dump_stderr(const char* name = "ga::stats") const {
        fprintf(
            stderr,
            "%s {\n"
            "  population_set    : %llu\n"
            "  population_add    : %llu\n"
            "  population_update : %llu\n"
            "}\n",
            name,
            (unsigned long long)population_set,
            (unsigned long long)population_add,
            (unsigned long long)population_update
        );
    }
};

inline Stats stats{};

inline void reset_stats() { stats.reset(); }
inline void debug_print_stats() { stats.dump_stderr(); }

namespace detail {
template<class...> inline constexpr bool dependent_false_v = false;

template<class T> struct is_std_array : false_type {};
template<class T, size_t N> struct is_std_array<array<T, N>> : true_type {};
template<class T> inline constexpr bool is_std_array_v = is_std_array<remove_cv_t<T>>::value;

template<class T> struct is_std_bitset : false_type {};
template<size_t N> struct is_std_bitset<bitset<N>> : true_type {};
template<class T> inline constexpr bool is_std_bitset_v = is_std_bitset<remove_cv_t<T>>::value;

template<class T>
using remove_cvref_t = remove_cv_t<remove_reference_t<T>>;

template<class F, class Gene>
using accessor_ret_t = remove_cvref_t<invoke_result_t<F, const Gene&>>;
}

template<class Gene>
struct Population {
private:
    vector<Gene>    gene_;
    vector<Fitness> fitness_;
    vector<int>     born_at_;
    int current_tick_ = 0;

public:

    [[nodiscard]] int size() const noexcept { return (int)gene_.size(); }
    [[nodiscard]] bool empty() const noexcept { return gene_.empty(); }
    [[nodiscard]] int current_tick() const noexcept { return current_tick_; }

    [[nodiscard]] const vector<Gene>& gene() const noexcept { return gene_; }
    [[nodiscard]] const vector<Fitness>& fitness() const noexcept { return fitness_; }
    [[nodiscard]] const vector<int>& born_at() const noexcept { return born_at_; }

    [[nodiscard]] const Gene& gene(int index) const {
        assert(0 <= index && index < size());
        return gene_[index];
    }
    [[nodiscard]] Fitness fitness(int index) const {
        assert(0 <= index && index < size());
        return fitness_[index];
    }
    [[nodiscard]] int born_at(int index) const {
        assert(0 <= index && index < size());
        return born_at_[index];
    }

    void reserve(int n) {
        if (n <= 0) return;
        gene_.reserve(n);
        fitness_.reserve(n);
        born_at_.reserve(n);
    }

    void clear() {
        gene_.clear();
        fitness_.clear();
        born_at_.clear();
        current_tick_ = 0;
    }

    void advance_tick(int dt = 1) {
        if (dt <= 0) return;
        current_tick_ += dt;
    }

    int add(const Gene& g, Fitness f) { return add_impl_(g, f); }
    int add(Gene&& g, Fitness f) { return add_impl_(move(g), f); }

    template<class F2>
    int add(const Gene& g, F2 f)
        requires (!is_same_v<detail::remove_cvref_t<F2>, Fitness> && is_convertible_v<F2, Fitness>) {
        return add_impl_(g, (Fitness)f);
    }
    template<class F2>
    int add(Gene&& g, F2 f)
        requires (!is_same_v<detail::remove_cvref_t<F2>, Fitness> && is_convertible_v<F2, Fitness>) {
        return add_impl_(move(g), (Fitness)f);
    }

    void set(int index, const Gene& g, Fitness f) { set_impl_(index, g, f); }
    void set(int index, Gene&& g, Fitness f) { set_impl_(index, move(g), f); }

    template<class F2>
    void set(int index, const Gene& g, F2 f)
        requires (!is_same_v<detail::remove_cvref_t<F2>, Fitness> && is_convertible_v<F2, Fitness>) {
        set_impl_(index, g, (Fitness)f);
    }
    template<class F2>
    void set(int index, Gene&& g, F2 f)
        requires (!is_same_v<detail::remove_cvref_t<F2>, Fitness> && is_convertible_v<F2, Fitness>) {
        set_impl_(index, move(g), (Fitness)f);
    }

    void set_fitness(int index, Fitness f) {
        assert(0 <= index && index < size());
        fitness_[index] = f;
    }

    template<class F2>
    void set_fitness(int index, F2 f)
        requires (!is_same_v<detail::remove_cvref_t<F2>, Fitness> && is_convertible_v<F2, Fitness>) {
        assert(0 <= index && index < size());
        fitness_[index] = (Fitness)f;
    }

    template<class F>
    void update(int index, F&& callback) {
        ++stats.population_update;

        assert(0 <= index && index < size());
        assert(fitness_.size() == gene_.size());
        assert(born_at_.size() == gene_.size());

        auto& g = gene_[index];
        if constexpr (invocable<F, Gene&, int>) {
            invoke(forward<F>(callback), g, index);
        } else if constexpr (invocable<F, Gene&>) {
            invoke(forward<F>(callback), g);
        } else {
            static_assert(detail::dependent_false_v<F>,
                          "Population::update callback must be invocable with (Gene&) or (Gene&, int).");
        }
    }

    bool better(int a, int b) const {
        assert(0 <= a && a < size());
        assert(0 <= b && b < size());

        const Fitness fa = fitness_[a];
        const Fitness fb = fitness_[b];

        const bool na = isnan(fa);
        const bool nb = isnan(fb);
        if (na != nb) return !na;
        if (!na && fa != fb) return fa > fb;

        const int ba = born_at_[a];
        const int bb = born_at_[b];
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

    void get_sorted_indices(vector<int>& out) const {
        const int n = size();
        out.resize(n);
        iota(out.begin(), out.end(), 0);
        sort(out.begin(), out.end(), [&](int a, int b) { return better(a, b); });
    }

    Fitness get_best_fitness() const {
        const int best = get_best_index();
        if (best < 0) return kNaN;
        return fitness_[best];
    }

    struct SummaryStats {
        size_t n = 0;

        Fitness fitness_min = kNaN;
        Fitness fitness_max = kNaN;
        Fitness fitness_avg = kNaN;

        int born_at_min = 0;
        int born_at_max = 0;
        double born_at_avg = (double)kNaN;
    };

    [[nodiscard]] SummaryStats get_stats() const {
        SummaryStats s{};
        const size_t n = gene_.size();
        s.n = n;

        assert(fitness_.size() == n);
        assert(born_at_.size() == n);

        if (n > 0) {
            Fitness mn = (Fitness)kInf;
            Fitness mx = (Fitness)-kInf;
            Fitness sum = 0;
            size_t cnt = 0;

            for (size_t i = 0; i < n; ++i) {
                const Fitness f = fitness_[i];
                if (isnan(f)) continue;
                if (f < mn) mn = f;
                if (f > mx) mx = f;
                sum += f;
                ++cnt;
            }
            if (cnt > 0) {
                s.fitness_min = mn;
                s.fitness_max = mx;
                s.fitness_avg = sum / (Fitness)cnt;
            }
        }

        if (n > 0) {
            int mn = born_at_[0];
            int mx = born_at_[0];
            long long sum = 0;
            for (size_t i = 0; i < n; ++i) {
                const int b = born_at_[i];
                if (b < mn) mn = b;
                if (b > mx) mx = b;
                sum += (long long)b;
            }
            s.born_at_min = mn;
            s.born_at_max = mx;
            s.born_at_avg = (double)sum / (double)n;
        }

        return s;
    }

    int select_tournament_best(int k) const { return select_tournament_impl_(k, true); }

    int select_tournament_worst(int k) const { return select_tournament_impl_(k, false); }

private:
    int select_tournament_impl_(int k, bool want_best) const {
        const int n = size();
        if (n <= 0) return -1;

        if (k <= 1) return rand_int(0, n - 1);
        if (k > n) k = n;

        int pick = rand_int(0, n - 1);
        for (int i = 1; i < k; ++i) {
            const int cand = rand_int(0, n - 1);
            if (want_best) {
                if (better(cand, pick)) pick = cand;
            } else {
                if (better(pick, cand)) pick = cand;
            }
        }
        return pick;
    }

    template<class G>
    void set_impl_(int index, G&& g, Fitness f) {
        ++stats.population_set;

        assert(0 <= index && index < size());
        assert(fitness_.size() == gene_.size());
        assert(born_at_.size() == gene_.size());

        gene_[index] = forward<G>(g);
        fitness_[index] = f;
        born_at_[index] = current_tick_;
    }

    template<class G>
    int add_impl_(G&& g, Fitness f) {
        ++stats.population_add;

        const int idx = (int)gene_.size();
        gene_.push_back(forward<G>(g));
        fitness_.push_back(f);
        born_at_.push_back(current_tick_);
        return idx;
    }
};

template<class T, size_t N>
inline void fill_random(array<T, N>& arr, T lo, T hi) {
    static_assert(is_integral_v<T> || is_floating_point_v<T> || is_enum_v<T>,
                  "fill_random(array): T must be integral, floating point, or enum(int-like).");

    if constexpr (N == 0) return;
    normalize_range(lo, hi);

    if constexpr (is_enum_v<T>) {
        using Under = underlying_type_t<T>;
        using DistT = conditional_t<
            (sizeof(Under) < sizeof(int)),
            int,
            conditional_t<(sizeof(Under) < sizeof(long long)), long long, Under>
        >;

        uniform_int_distribution<DistT> dist((DistT)(Under)lo, (DistT)(Under)hi);
        for (auto& x : arr) x = (T)(Under)dist(rng);

    } else if constexpr (is_integral_v<T>) {
        using DistT = conditional_t<
            (sizeof(T) < sizeof(int)),
            int,
            conditional_t<(sizeof(T) < sizeof(long long)), long long, T>
        >;

        uniform_int_distribution<DistT> dist((DistT)lo, (DistT)hi);
        for (auto& x : arr) x = (T)dist(rng);

    } else {
        uniform_real_distribution<T> dist(lo, hi);
        for (auto& x : arr) x = dist(rng);
    }
}

template<class T, size_t N>
inline void fill_perm(array<T, N>& arr) {
    static_assert(is_integral_v<T> || is_enum_v<T>,
                  "fill_perm(array): T must be integral or enum(int-like).");
    if constexpr (N == 0) return;
    for (size_t i = 0; i < N; ++i) arr[i] = (T)i;
}

template<class T, size_t N>
inline void fill_random_perm(array<T, N>& arr) {
    static_assert(is_integral_v<T> || is_enum_v<T>,
                  "fill_random_perm(array): T must be integral or enum(int-like).");

    if constexpr (N == 0) return;
    fill_perm(arr);

    for (size_t i = N - 1; i > 0; --i) {
        const size_t j = rand_index<size_t>(i + 1);
        swap(arr[i], arr[j]);
    }
}

template<size_t N>
inline void fill_random(bitset<N>& bs) {
    if constexpr (N == 0) return;

    constexpr size_t kWordBits = 32;
    constexpr size_t kWords = (N + kWordBits - 1) / kWordBits;

    size_t bit = 0;
    for (size_t w = 0; w < kWords; ++w) {
        const uint32_t r = rand_u32();

        for (size_t j = 0; j < kWordBits && bit < N; ++j, ++bit) {
            bs.set(bit, ((r >> j) & 1u) != 0u);
        }
    }
}

template<class T, size_t N>
inline void apply_rolling_horizon(array<T, N>& a, const T& fill) {
    if constexpr (N == 0) return;
    if constexpr (N == 1) { a[0] = fill; return; }

    if constexpr (is_trivially_copyable_v<T>) memmove(a.data(), a.data() + 1, (N - 1) * sizeof(T));
    else for (size_t i = 0; i + 1 < N; ++i) a[i] = a[i + 1];
    a[N - 1] = fill;
}

template<size_t N>
inline void apply_rolling_horizon(bitset<N>& b, bool fill) {
    if constexpr (N == 0) return;
    if constexpr (N == 1) { b.set(0, fill); return; }

    b >>= 1;
    b.set(N - 1, fill);
}

template<class Gene>
struct DiversityCalculator {
    double sum = 0.0;
    int n = 0;

    void reset() { sum = 0.0; n = 0; }

    double diversity_rate() const { return (n > 0) ? (sum / (double)n) : 0.0; }

    template<class F, class V>
    void process(const vector<Gene>& pop, F&& accessor, V lo, V hi, bool is_enum) {
        using Ret = detail::accessor_ret_t<F, Gene>;

        normalize_range(lo, hi);

        if constexpr (is_integral_v<V> || is_enum_v<V>) {
            const long long span = (long long)hi - (long long)lo;
            if (span == 0) return;

            const double denom = (double)span;
            double score = 0.0;

            if constexpr (detail::is_std_array_v<Ret>) {
                using Elem = typename Ret::value_type;
                constexpr size_t N = tuple_size_v<Ret>;
                static_assert(is_same_v<Elem, V>, "process(array): V must match array element type");

                if (pop.empty() || N == 0) score = 0.0;
                else if (!is_enum) {
                    array<V, N> mn{}, mx{};
                    {
                        const Ret r0 = invoke(accessor, pop[0]);
                        for (size_t i = 0; i < N; ++i) {
                            const V v = clamp(r0[i], lo, hi);
                            mn[i] = mx[i] = v;
                        }
                    }
                    for (size_t pi = 1; pi < pop.size(); ++pi) {
                        const Ret r = invoke(accessor, pop[pi]);
                        for (size_t i = 0; i < N; ++i) {
                            const V v = clamp(r[i], lo, hi);
                            if (v < mn[i]) mn[i] = v;
                            if (v > mx[i]) mx[i] = v;
                        }
                    }
                    double acc = 0.0;
                    for (size_t i = 0; i < N; ++i) {
                        const long long d = (long long)mx[i] - (long long)mn[i];
                        acc += (double)d / denom;
                    }
                    score = acc / (double)N;

                } else {

                    constexpr long long kSeenSpanLimit = 65536;
                    constexpr uint64_t kSeenBytesLimit = 4ull * 1024ull * 1024ull;

                    const size_t domain = span + 1ll;
                    const uint64_t seen_bytes = (uint64_t)domain * (uint64_t)N;

                    if (span <= kSeenSpanLimit && seen_bytes <= kSeenBytesLimit) {
                        static thread_local vector<unsigned char> seen;
                        seen.assign(domain * N, 0u);
                        array<size_t, N> uniq{};

                        for (const auto& g : pop) {
                            const Ret r = invoke(accessor, g);
                            for (size_t i = 0; i < N; ++i) {
                                const V v = clamp(r[i], lo, hi);
                                const size_t idx = (long long)v - (long long)lo;
                                const size_t off = i * domain + idx;
                                if (!seen[off]) { seen[off] = 1u; ++uniq[i]; }
                            }
                        }

                        double acc = 0.0;
                        for (size_t i = 0; i < N; ++i) {
                            const size_t u = uniq[i];
                            acc += (u == 0) ? 0.0 : (double)(u - 1) / denom;
                        }
                        score = acc / (double)N;
                    } else {
                        static thread_local vector<vector<V>> vals;
                        if (vals.size() != N) vals.resize(N);
                        for (auto& v : vals) { v.clear(); v.reserve(pop.size()); }

                        for (const auto& g : pop) {
                            const Ret r = invoke(accessor, g);
                            for (size_t i = 0; i < N; ++i) vals[i].push_back(clamp(r[i], lo, hi));
                        }

                        double acc = 0.0;
                        for (size_t i = 0; i < N; ++i) {
                            auto& v = vals[i];
                            sort(v.begin(), v.end());
                            v.erase(unique(v.begin(), v.end()), v.end());
                            const size_t u = v.size();
                            acc += (u == 0) ? 0.0 : (double)(u - 1) / denom;
                        }
                        score = acc / (double)N;
                    }
                }

            } else if constexpr (is_same_v<Ret, V>) {
                if (pop.empty()) score = 0.0;
                else if (!is_enum) {
                    V mn = clamp(invoke(accessor, pop[0]), lo, hi);
                    V mx = mn;
                    for (size_t i = 1; i < pop.size(); ++i) {
                        const V v = clamp(invoke(accessor, pop[i]), lo, hi);
                        if (v < mn) mn = v;
                        if (v > mx) mx = v;
                    }
                    const long long d = (long long)mx - (long long)mn;
                    score = (double)d / denom;

                } else {
                    constexpr long long kSeenSpanLimit = 65536;
                    const size_t domain = span + 1ll;

                    if (span <= kSeenSpanLimit) {
                        static thread_local vector<unsigned char> seen;
                        seen.assign(domain, 0u);
                        size_t uniq = 0;

                        for (const auto& g : pop) {
                            const V v = clamp(invoke(accessor, g), lo, hi);
                            const size_t idx = (long long)v - (long long)lo;
                            if (!seen[idx]) {
                                seen[idx] = 1u;
                                ++uniq;
                                if (uniq == domain) break;
                            }
                        }
                        score = (uniq == 0) ? 0.0 : (double)(uniq - 1) / denom;
                    } else {
                        static thread_local vector<V> tmp;
                        tmp.clear();
                        tmp.reserve(pop.size());
                        for (const auto& g : pop) tmp.push_back(clamp(invoke(accessor, g), lo, hi));
                        sort(tmp.begin(), tmp.end());
                        tmp.erase(unique(tmp.begin(), tmp.end()), tmp.end());
                        const size_t uniq = tmp.size();
                        score = (uniq == 0) ? 0.0 : (double)(uniq - 1) / denom;
                    }
                }

            } else {
                static_assert(detail::dependent_false_v<Ret>,
                              "process(pop, accessor, lo, hi, is_enum): accessor must return V or array<V,N>.");
            }

            if (isfinite(score)) { sum += score; ++n; }
            return;
        }

        if constexpr (is_floating_point_v<V>) {
            assert(lo < hi);
            assert(!is_enum);
            const double denom = (double)(hi - lo);

            double score = 0.0;

            if constexpr (detail::is_std_array_v<Ret>) {
                using Elem = typename Ret::value_type;
                constexpr size_t N = tuple_size_v<Ret>;
                static_assert(is_same_v<Elem, V>, "process(array): V must match array element type");

                if (pop.empty() || N == 0) score = 0.0;
                else {
                    array<V, N> mn{}, mx{};
                    array<unsigned char, N> ok{};

                    for (const auto& g : pop) {
                        const Ret r = invoke(accessor, g);
                        for (size_t i = 0; i < N; ++i) {
                            const V v0 = r[i];
                            if (isnan(v0)) continue;
                            const V v = clamp(v0, lo, hi);
                            if (!ok[i]) { mn[i] = mx[i] = v; ok[i] = 1; }
                            else { if (v < mn[i]) mn[i] = v; if (v > mx[i]) mx[i] = v; }
                        }
                    }

                    double acc = 0.0;
                    for (size_t i = 0; i < N; ++i) acc += ok[i] ? (double)(mx[i] - mn[i]) / denom : 0.0;
                    score = acc / (double)N;
                }

            } else if constexpr (is_same_v<Ret, V>) {
                if (pop.empty()) score = 0.0;
                else {
                    bool ok = false;
                    V mn{}, mx{};
                    for (const auto& g : pop) {
                        const V v0 = invoke(accessor, g);
                        if (isnan(v0)) continue;
                        const V v = clamp(v0, lo, hi);
                        if (!ok) { mn = mx = v; ok = true; }
                        else { if (v < mn) mn = v; if (v > mx) mx = v; }
                    }
                    score = ok ? (double)(mx - mn) / denom : 0.0;
                }

            } else {
                static_assert(detail::dependent_false_v<Ret>,
                              "process(pop, accessor, lo, hi, is_enum): accessor must return V or array<V,N>.");
            }

            if (isfinite(score)) { sum += score; ++n; }
            return;
        }

        if constexpr (!is_integral_v<V> && !is_enum_v<V> && !is_floating_point_v<V>) {
            static_assert(detail::dependent_false_v<V>,
                          "process(pop, accessor, lo, hi, is_enum): V must be integral or floating point.");
        }
    }

    template<class F>
    void process(const vector<Gene>& pop, F&& accessor) {
        using Ret = detail::accessor_ret_t<F, Gene>;

        if constexpr (is_same_v<Ret, bool>) {
            double score = 0.0;
            if (pop.empty()) score = 0.0;
            else {
                bool has0 = false, has1 = false;
                for (const auto& g : pop) {
                    const bool v = invoke(accessor, g);
                    has1 |= v;
                    has0 |= !v;
                    if (has0 && has1) break;
                }
                score = (has0 && has1) ? 1.0 : 0.5;
            }
            if (isfinite(score)) { sum += score; ++n; }
            return;
        }

        if constexpr (detail::is_std_bitset_v<Ret>) {
            process_bitset_impl_(pop, forward<F>(accessor));
            return;
        }

        if constexpr (!is_same_v<Ret, bool> && !detail::is_std_bitset_v<Ret>) {
            static_assert(detail::dependent_false_v<Ret>,
                          "process(pop, accessor): accessor must return bool or bitset<N>.");
        }
    }

private:
    template<class F, size_t N>
    void process_bitset_impl_n_(const vector<Gene>& pop, F&& accessor, bitset<N>*) {
        double score = 0.0;
        if (pop.empty() || N == 0) score = 0.0;
        else {
            bitset<N> orv{};
            bitset<N> andv; andv.set();
            for (const auto& g : pop) {
                const bitset<N> b = invoke(accessor, g);
                orv |= b;
                andv &= b;
            }
            const bitset<N> both = (orv & ~andv);
            const size_t both_cnt = both.count();
            const double frac = (double)both_cnt / (double)N;

            score = 0.5 + 0.5 * frac;
        }
        if (isfinite(score)) { sum += score; ++n; }
    }

    template<class F>
    void process_bitset_impl_(const vector<Gene>& pop, F&& accessor) {
        using Ret = detail::accessor_ret_t<F, Gene>;
        process_bitset_impl_n_(pop, forward<F>(accessor), (Ret*)nullptr);
    }
};

}

#undef main

bool debug_mode = true;

template<class Gene, class Evaluator>
static double calibrate(Evaluator& ev, double budget_ms);

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
        const auto& seq   = g.actions;
        const float param = g.aggression;
        const auto& flags = g.boost;
        const auto& route = g.route;

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

static constexpr int   kActionLo  = 0;
static constexpr int   kActionHi  = kActions - 1;
static constexpr int   kActionDef = 4;

static constexpr float kAggLo  = -2.0f;
static constexpr float kAggHi  =  2.0f;
static constexpr float kAggDef =  0.0f;

template<int H>
struct GeneH {
    static_assert(H >= 0);
    std::array<int, H> actions{};
    float aggression = kAggDef;
    std::bitset<kFlagsN> boost{};
    std::array<int, kRouteN> route{};
};

template<int H>
static inline void init_default_gene(GeneH<H>& g) {
    g.actions.fill(kActionDef);
    g.aggression = kAggDef;
    g.boost.reset();

    ga::fill_perm(g.route);
}

template<int H>
static inline void init_random_gene(GeneH<H>& g) {
    ga::fill_random(g.actions, kActionLo, kActionHi);
    g.aggression = ga::rand_real<float>(kAggLo, kAggHi);
    ga::fill_random(g.boost);

    ga::fill_random_perm(g.route);
}

template<int H>
static inline void apply_rolling_horizon(GeneH<H>& g, int shift, bool random_fill) {
    if (shift <= 0) return;

    for (int s = 0; s < shift; ++s) {
        const int fill_act = random_fill ? ga::rand_int(kActionLo, kActionHi) : kActionDef;
        ga::apply_rolling_horizon(g.actions, fill_act);

        const bool fill_boost = random_fill ? ga::rand_bool(0.5) : false;
        ga::apply_rolling_horizon(g.boost, fill_boost);
    }
}

template<class T, std::size_t N>
static inline void crossover_uniform_array(const std::array<T, N>& a, const std::array<T, N>& b, std::array<T, N>& out) {
    if constexpr (N == 0) return;

    std::uint32_t bits = 0;
    for (std::size_t i = 0; i < N; ++i) {
        if ((i & 31u) == 0u) bits = ga::rand_u32();
        const bool pick_a = (bits & 1u) != 0u;
        out[i] = pick_a ? a[i] : b[i];
        bits >>= 1;
    }
}

template<class T, std::size_t N>
static inline void crossover_two_point_array(const std::array<T, N>& a, const std::array<T, N>& b, std::array<T, N>& out) {
    if constexpr (N == 0) {
        return;
    } else if constexpr (N == 1) {
        out[0] = (ga::rand_u32() & 1u) ? a[0] : b[0];
        return;
    } else {
        const int l = ga::rand_int(0, (int)N - 1);
        const int r = ga::rand_int(l + 1, (int)N);
        for (int i = 0; i < (int)N; ++i) {
            out[(std::size_t)i] = (l <= i && i < r) ? b[(std::size_t)i] : a[(std::size_t)i];
        }
    }
}

template<class T>
static inline T crossover_arithmetic_scalar(T a, T b, T lo, T hi, double alpha = -1.0) {
    const double w = (alpha >= 0.0) ? std::clamp(alpha, 0.0, 1.0) : ga::rand01();
    if constexpr (std::is_floating_point_v<T>) {
        const T x = static_cast<T>(a * (T)w + b * (T)(1.0 - w));
        return ga::clamp_range(x, lo, hi);
    } else {
        const double x = (double)a * w + (double)b * (1.0 - w);
        const long long y = std::llround(x);
        return ga::clamp_range(static_cast<T>(y), lo, hi);
    }
}

template<std::size_t N>
static inline void crossover_uniform_bitset(const std::bitset<N>& a, const std::bitset<N>& b, std::bitset<N>& out) {
    if constexpr (N == 0) return;

    std::bitset<N> mask;
    ga::fill_random(mask);
    out = (a & mask) | (b & ~mask);
}

template<std::size_t N>
static inline void crossover_ox_perm(const std::array<int, N>& a, const std::array<int, N>& b, std::array<int, N>& out) {
    if constexpr (N == 0) return;

    const int l = ga::rand_int(0, (int)N - 1);
    const int r = ga::rand_int(l + 1, (int)N);

    out = a;

    std::array<unsigned char, N> used{};
    used.fill(0);

    for (int i = l; i < r; ++i) {
        const int v = a[(std::size_t)i];
        if (0 <= v && v < (int)N) used[(std::size_t)v] = 1;
    }

    auto in_seg = [&](int i) { return l <= i && i < r; };

    int pos = r % (int)N;
    for (int t = 0; t < (int)N; ++t) {
        const int v = b[(std::size_t)((r + t) % (int)N)];
        if (0 <= v && v < (int)N) {
            if (used[(std::size_t)v]) continue;
        }

        while (in_seg(pos)) pos = (pos + 1) % (int)N;
        out[(std::size_t)pos] = v;
        if (0 <= v && v < (int)N) used[(std::size_t)v] = 1;
        pos = (pos + 1) % (int)N;
    }
}

template<class T, std::size_t N>
static inline void mutate_creep_array(std::array<T, N>& a, double p_mut, int step_max, T lo, T hi) {
    if constexpr (N == 0) return;
    if (!(p_mut > 0.0) || step_max <= 0) return;

    for (std::size_t i = 0; i < N; ++i) {
        if (!ga::rand_bool(p_mut)) continue;

        int d = 0;
        while (d == 0) d = ga::rand_int(-step_max, step_max);

        long long y = (long long)a[i] + (long long)d;
        a[i] = ga::clamp_range(static_cast<T>(y), lo, hi);
    }
}

template<class T, std::size_t N>
static inline void mutate_random_reset_array(std::array<T, N>& a, double p_reset, T lo, T hi) {
    if constexpr (N == 0) return;
    if (!(p_reset > 0.0)) return;

    for (std::size_t i = 0; i < N; ++i) {
        if (!ga::rand_bool(p_reset)) continue;

        if constexpr (std::is_enum_v<T> || std::is_integral_v<T>) {
            a[i] = ga::rand_int<T>(lo, hi);
        } else {
            a[i] = ga::rand_real<T>(lo, hi);
        }
    }
}

template<class T>
static inline void mutate_gaussian_scalar(T& x, double p_mut, double sigma, T lo, T hi) {
    if (!(p_mut > 0.0) || !(sigma > 0.0)) return;
    if (!ga::rand_bool(p_mut)) return;

    std::normal_distribution<double> nd(0.0, sigma);
    const double d = nd(ga::rng);

    if constexpr (std::is_floating_point_v<T>) {
        x = ga::clamp_range(static_cast<T>(x + (T)d), lo, hi);
    } else {
        const long long y = (long long)x + (long long)std::llround(d);
        x = ga::clamp_range(static_cast<T>(y), lo, hi);
    }
}

template<std::size_t N>
static inline void mutate_bitflip_bitset(std::bitset<N>& b, double p_flip) {
    if constexpr (N == 0) return;
    if (!(p_flip > 0.0)) return;

    for (std::size_t i = 0; i < N; ++i) {
        if (ga::rand_bool(p_flip)) b.flip(i);
    }
}

template<std::size_t N>
static inline void mutate_swap_perm(std::array<int, N>& a, double p_swap) {
    if constexpr (N < 2) return;
    if (!(p_swap > 0.0)) return;
    if (!ga::rand_bool(p_swap)) return;

    int i = ga::rand_int(0, (int)N - 1);
    int j = ga::rand_int(0, (int)N - 1);
    if (i == j) j = (j + 1) % (int)N;
    std::swap(a[(std::size_t)i], a[(std::size_t)j]);
}

template<std::size_t N>
static inline void mutate_inversion_perm(std::array<int, N>& a, double p_inv) {
    if constexpr (N < 2) return;
    if (!(p_inv > 0.0)) return;
    if (!ga::rand_bool(p_inv)) return;

    const int l = ga::rand_int(0, (int)N - 1);
    const int r = ga::rand_int(l + 1, (int)N);
    std::reverse(a.begin() + l, a.begin() + r);
}

template<int H>
struct GeneOps {

    std::array<double, 2> seq_crossover_w{0.7, 0.3};
    std::array<double, 2> seq_mut_w{0.75, 0.25};
    std::array<double, 2> perm_mut_w{0.6, 0.4};

    double seq_creep_p = 0.12;
    int    seq_creep_step_max = 2;
    double seq_reset_p = 0.03;

    double agg_gauss_p = 0.35;
    double agg_gauss_sigma = 0.35;

    double boost_flip_p = 0.02;

    double perm_swap_p = 0.25;
    double perm_inv_p  = 0.15;

    void crossover(const GeneH<H>& p1, const GeneH<H>& p2, GeneH<H>& c) const {

        if (ga::weighted_index(seq_crossover_w) == 0) {
            crossover_two_point_array(p1.actions, p2.actions, c.actions);
        } else {
            crossover_uniform_array(p1.actions, p2.actions, c.actions);
        }

        c.aggression = crossover_arithmetic_scalar<float>(p1.aggression, p2.aggression, kAggLo, kAggHi);

        crossover_uniform_bitset(p1.boost, p2.boost, c.boost);

        crossover_ox_perm(p1.route, p2.route, c.route);
    }

    void mutate(GeneH<H>& g) const {

        if (ga::weighted_index(seq_mut_w) == 0) {
            mutate_creep_array(g.actions, seq_creep_p, seq_creep_step_max, kActionLo, kActionHi);
        } else {
            mutate_random_reset_array(g.actions, seq_reset_p, kActionLo, kActionHi);
        }

        mutate_gaussian_scalar(g.aggression, agg_gauss_p, agg_gauss_sigma, kAggLo, kAggHi);

        mutate_bitflip_bitset(g.boost, boost_flip_p);

        if (ga::weighted_index(perm_mut_w) == 0) {
            mutate_swap_perm(g.route, perm_swap_p);
        } else {
            mutate_inversion_perm(g.route, perm_inv_p);
        }
    }
};

template<int H>
static inline double diversity_rate_manual(const ga::Population<GeneH<H>>& pop) {
    using Gene = GeneH<H>;
    const auto& genes = pop.gene();
    if (genes.empty()) return 0.0;

    ga::DiversityCalculator<Gene> dc;
    dc.reset();

    if constexpr (H > 0) {
        dc.process(genes, [](const Gene& g) { return g.actions; }, kActionLo, kActionHi, true);
    }

    dc.process(genes, [](const Gene& g) { return g.aggression; }, kAggLo, kAggHi, false);

    dc.process(genes, [](const Gene& g) { return g.boost; });

    if constexpr (kRouteN > 0) {
        dc.process(genes, [](const Gene& g) { return g.route; }, 0, kRouteN - 1, true);
    }

    return dc.diversity_rate();
}

template<class Gene, class Evaluator>
static double calibrate(Evaluator& ev, double budget_ms) {

    constexpr int kMaxTrials = 1'000'000;

    if (!(budget_ms > 0.0)) budget_ms = 1.0;

    volatile double sink = 0.0;

    int trials = 0;
    double eval_total_ms = 0.0;

    const double start_ms = ga::now_ms();
    const double end_ms = start_ms + budget_ms;

    while (trials < kMaxTrials) {
        Gene g{};
        init_random_gene(g);

        const double t0 = ga::now_ms();
        sink = sink + ev.template evaluate<Gene>(g);
        const double t1 = ga::now_ms();

        eval_total_ms += (t1 - t0);
        ++trials;

        if (t1 >= end_ms) break;
    }

    (void)sink;
    return eval_total_ms / (double)trials;
}

template<class Gene, class EvaluatorT>
struct SSGARunner;

template<int H, class EvaluatorT>
struct SSGARunner<GeneH<H>, EvaluatorT> {
    using Gene = GeneH<H>;

    EvaluatorT& evaluator;
    int pop_size = 0;

    int run_no = 0;
    int iters  = 0;

    ga::Population<Gene> pop;

    GeneOps<H> ops{};

    int parent_tournament_k = 3;
    int victim_tournament_k = 3;

    double p_crossover = 0.90;

    int immigration_interval = 0;
    double immigration_chance = 0.35;

    int stagnation_window = 0;
    float diversity_floor = 0.20f;
    int shake_replace = 0;

    int diversity_check_interval = 0;

    double log_interval_ms = 10.0;

    struct OpStats {
        std::uint64_t apply_crossover = 0;
        std::uint64_t apply_mutation = 0;
    } op_stats{};

    struct Stats {
        double run_start_ms = 0.0;
        double last_log_ms = 0.0;

        std::uint64_t evals = 0;
        std::uint64_t children = 0;
        std::uint64_t immigrants = 0;
        std::uint64_t shakes = 0;

        double best_fitness = -ga::kInf;
        int best_iter = 0;
        int no_improve_iters = 0;

        ga::Stats header_stats_begin{};
        OpStats  header_ops_begin{};

        double div_cache = 0.0;
        int div_cache_iter = -1;
    } stats{};

    SSGARunner(EvaluatorT& ev, int n)
        : evaluator(ev), pop_size(n) {
        if (pop_size < 2) pop_size = 2;

        pop.reserve(pop_size);

        immigration_interval = std::max(16, pop_size * 2);
        stagnation_window   = std::max(64, pop_size * 6);
        shake_replace       = std::max(2, pop_size / 10);

        diversity_check_interval = std::max(2, pop_size);
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

        op_stats = OpStats{};
        stats = Stats{};

        stats.run_start_ms = ga::now_ms();
        stats.last_log_ms = stats.run_start_ms;
        stats.header_stats_begin = ga::stats;
        stats.header_ops_begin = op_stats;

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
                if (i == 0) init_default_gene(g);
                else        init_random_gene(g);

                const double f = evaluator.template evaluate<Gene>(g);
                ++stats.evals;
                pop.add(std::move(g), f);
            }

            stats.best_fitness = (double)pop.get_best_fitness();
            stats.best_iter = 0;
            stats.no_improve_iters = 0;

            stats.div_cache = diversity_rate_manual(pop);
            stats.div_cache_iter = 0;
            return;
        }

        const int n = pop.size();
        for (int i = 0; i < n; ++i) {
            pop.update(i, [&](Gene& g) {
                apply_rolling_horizon(g, 1, true);
            });
        }

        for (int i = 0; i < n; ++i) {
            const double f = evaluator.template evaluate<Gene>(pop.gene(i));
            ++stats.evals;
            pop.set_fitness(i, f);
        }

        stats.best_fitness = (double)pop.get_best_fitness();
        stats.best_iter = 0;
        stats.no_improve_iters = 0;

        stats.div_cache = diversity_rate_manual(pop);
        stats.div_cache_iter = 0;
    }

    double diversity_rate_cached(bool force = false) {
        if (force || stats.div_cache_iter < 0 || (iters - stats.div_cache_iter) >= diversity_check_interval) {
            stats.div_cache = diversity_rate_manual(pop);
            stats.div_cache_iter = iters;
        }
        return stats.div_cache;
    }

    void maybe_make_child() {
        const int n = pop.size();
        if (n <= 1) return;

        const int p1 = pop.select_tournament_best(parent_tournament_k);
        int p2 = pop.select_tournament_best(parent_tournament_k);
        if (p2 == p1) {
            p2 = pop.select_tournament_best(parent_tournament_k);
            if (p2 == p1) p2 = (p1 + 1) % n;
        }

        Gene child{};

        if (ga::rand_bool(p_crossover)) {
            ops.crossover(pop.gene(p1), pop.gene(p2), child);
            ++op_stats.apply_crossover;
        } else {
            const int better = pop.better(p1, p2) ? p1 : p2;
            child = pop.gene(better);
        }

        ops.mutate(child);
        ++op_stats.apply_mutation;

        const double f = evaluator.template evaluate<Gene>(child);
        ++stats.evals;
        ++stats.children;

        pop.advance_tick(1);

        int victim = pop.select_tournament_worst(victim_tournament_k);
        if (victim < 0) victim = 0;

        const int best_now = pop.get_best_index();
        if (victim == best_now && n >= 2) {
            for (int retry = 0; retry < 3; ++retry) {
                const int v2 = pop.select_tournament_worst(victim_tournament_k);
                if (v2 >= 0 && v2 != best_now) { victim = v2; break; }
            }
            if (victim == best_now) victim = (best_now + 1) % n;
        }

        pop.set(victim, std::move(child), f);

        diversity_rate_cached(false);

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

        const double div = diversity_rate_cached(false);
        double p = immigration_chance;
        if (div > 0.45) p *= 0.25;

        if (!ga::rand_bool(p)) return;

        Gene immigrant{};
        init_random_gene(immigrant);
        const double f = evaluator.template evaluate<Gene>(immigrant);
        ++stats.evals;
        ++stats.immigrants;

        pop.advance_tick(1);

        int victim = pop.select_tournament_worst(victim_tournament_k);
        if (victim < 0) victim = pop.get_worst_index();
        if (victim < 0) victim = 0;

        const int best_now = pop.get_best_index();
        if (victim == best_now && pop.size() >= 2) {
            victim = (best_now + 1) % pop.size();
        }

        pop.set(victim, std::move(immigrant), f);

        if (f > stats.best_fitness) {
            stats.best_fitness = f;
            stats.best_iter = iters;
            stats.no_improve_iters = 0;
        }
    }

    void maybe_shake() {
        const bool stagnated = (stats.no_improve_iters >= stagnation_window);

        const double div = stats.div_cache;
        const bool lowdiv = (div < (double)diversity_floor);

        if (!stagnated && !lowdiv) return;

        const int n = pop.size();
        if (n <= 2) return;

        const int best_now = pop.get_best_index();
        const int replace_n = std::min(std::max(1, shake_replace), n - 1);

        for (int rep = 0; rep < replace_n; ++rep) {
            int victim = pop.select_tournament_worst(victim_tournament_k);
            if (victim < 0) victim = 0;
            if (victim == best_now) victim = (best_now + 1) % n;

            Gene g{};
            init_random_gene(g);
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

        diversity_rate_cached(true);
    }

    void maybe_log() {
        if (!debug_mode) return;
        const double now = ga::now_ms();
        if (now - stats.last_log_ms < log_interval_ms) return;
        stats.last_log_ms = now;
        log_stats();
    }

    void log_stats() {
        const auto ps = pop.get_stats();
        const double div = diversity_rate_cached(false);

        const ga::Stats cur = ga::stats;
        const std::uint64_t d_set = cur.population_set - stats.header_stats_begin.population_set;
        const std::uint64_t d_add = cur.population_add - stats.header_stats_begin.population_add;

        const std::uint64_t d_cross = op_stats.apply_crossover - stats.header_ops_begin.apply_crossover;
        const std::uint64_t d_mut   = op_stats.apply_mutation  - stats.header_ops_begin.apply_mutation;

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
            (unsigned long long)d_cross,
            (unsigned long long)d_mut,
            (unsigned long long)d_set,
            (unsigned long long)d_add
        );
    }
};

template<class EvaluatorT, class... Genes>
static int choose_best_gene(EvaluatorT& ev, int pop_size, double run_ms) {
    constexpr int N = (int)sizeof...(Genes);
    static_assert(N > 0);

    const std::mt19937 base_rng = ga::rng;

    int best_idx = 0;
    double best_fit = -ga::kInf;

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
    double best_fit = -ga::kInf;

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
    double best_fit = -ga::kInf;

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
        const int action0 = best.actions[0];
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
