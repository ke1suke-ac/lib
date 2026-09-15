// ga_population_lite.hpp
#pragma once
#include <bits/stdc++.h>

using namespace std;

namespace ga {

// ------------------------------------------------------------
// RNG (header-only safe)
// ------------------------------------------------------------
// ヘッダ単体運用でも ODR を回避するため inline 変数。
// 競技向けの再現性確保のため固定 seed をデフォルトにしている。
// 必要ならユーザー側で ga::rng.seed(...) を呼ぶ。
inline mt19937 rng(42);

// ------------------------------------------------------------
// Fitness
// ------------------------------------------------------------
// GA で使う評価値。最大化を想定（大きいほど良い）。
// float / double / long double などに差し替えてもそのまま使えるようにする。
using Fitness = double;

// 短い別名（コード量節約）
inline constexpr Fitness kNaN = numeric_limits<Fitness>::quiet_NaN();
inline constexpr Fitness kInf = numeric_limits<Fitness>::infinity();

// ------------------------------------------------------------
// Range helpers
// ------------------------------------------------------------
template<class T>
inline void normalize_range(T& lo, T& hi) {
    if (hi < lo) swap(lo, hi);
}

template<class T>
inline T clamp_range(T x, T lo, T hi) {
    normalize_range(lo, hi);
    return clamp(x, lo, hi);
}

// ------------------------------------------------------------
// 乱数ユーティリティ（軽量）
//  - std::uniform_*_distribution を hot loop で濫用しないため、
//    [0,1) は generate_canonical で作る。
// ------------------------------------------------------------
inline double rand01() { return generate_canonical<double, 53>(rng); }
inline float  rand01f() { return generate_canonical<float, 24>(rng); }

inline uint32_t rand_u32() {
    // mt19937::result_type は環境で異なるので明示キャスト
    return (uint32_t)rng();
}

// modulo bias を避ける軽量な整数レンジ乱数（int 用：hot loop 向け）
inline int rand_int(int lo, int hi) {
    if (hi <= lo) return lo;

    // signed overflow を避けるため 64bit で差分を計算（hi-lo+1 が int を超えても UB にしない）
    const uint64_t range64 = (uint64_t)((int64_t)hi - (int64_t)lo) + 1ull;

    // range64 が 2^32（= uint32_t に収まらない）等の特殊ケースは fallback。
    // この用途（小さい離散値）では通常ここに来ない。
    if (range64 == 0ull || range64 > (uint64_t)numeric_limits<uint32_t>::max()) {
        uniform_int_distribution<int> dist(lo, hi);
        return dist(rng);
    }

    const uint32_t range = (uint32_t)range64;

    // 2^k のときは bitmask（高速）
    if ((range & (range - 1u)) == 0u) return lo + (int)(rand_u32() & (range - 1u));

    // rejection (modulo bias を避ける)
    const uint32_t limit = numeric_limits<uint32_t>::max()
                         - (numeric_limits<uint32_t>::max() % range);

    uint32_t x;
    do { x = rand_u32(); } while (x >= limit);

    return lo + (int)(x % range);
}

// 汎用 rand_int（小さい整数型・enum に配慮して DistT を使う）
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
    if (!(hi > lo)) return lo; // hi<=lo, NaN を含むケースは lo を返す

    if constexpr (is_same_v<Real, float>) return lo + (hi - lo) * rand01f();
    else                                  return lo + (hi - lo) * (Real)rand01();
}

// bernoulli(p)
inline bool rand_bool(double p) {
    if (!(p > 0.0)) return false;
    if (p >= 1.0) return true;
    return rand01() < p;
}

// 0..n-1
template<class Int>
inline Int rand_index(Int n) {
    static_assert(is_integral_v<Int>, "rand_index(n): Int must be integral.");
    if (n <= 1) return 0;
    return rand_int<Int>(0, n - 1);
}

// weights: 負は0扱い、sum<=0 なら一様
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

    const double r = rand_real(0.0, sum); // [0,sum)
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

// ------------------------------------------------------------
// Timing helper
// ------------------------------------------------------------
// 現在時刻を double ミリ秒で返す（小数も保持）
// - 競プロ/ゲームAI（CodinGame 等）での簡易計測向け（steady_clock: 単調増加）
//
// 例) double t0 = ga::now_ms(); ... double dt = ga::now_ms() - t0;
inline double now_ms() {
    using clock = chrono::steady_clock;
    return chrono::duration<double, milli>(clock::now().time_since_epoch()).count();
}

// ------------------------------------------------------------
// Stats
// ------------------------------------------------------------
// Population の主要操作の呼び出し回数だけを記録する軽量統計。
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

// ヘッダ単体運用のため inline 変数にして ODR を回避
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
} // namespace detail

// ------------------------------
// Population (lightweight)
// ------------------------------
// ・Gene は制約なし（任意型）。
// ・各個体について Gene / Fitness / born_at(tick) を並列配列で保持。
// ・Fitness が NaN の個体は「未評価/無効」として常に最下位扱い。
// ・同点 tie-break は born_at が新しいほど良い → それも同じなら index が小さい方。
// ・多様性(Diversity) / rolling horizon は持たない。
// ・乱数は常に ga::rng を使う（外部から受け取らない）。
template<class Gene>
struct Population {
private:
    vector<Gene>    gene_;
    vector<Fitness> fitness_;
    vector<int>     born_at_;
    int current_tick_ = 0;

public:
    // ---- 基本情報 ----
    [[nodiscard]] int size() const noexcept { return (int)gene_.size(); }
    [[nodiscard]] bool empty() const noexcept { return gene_.empty(); }
    [[nodiscard]] int current_tick() const noexcept { return current_tick_; }

    // ---- 参照取得（読み取り専用）----
    [[nodiscard]] const vector<Gene>& gene() const noexcept { return gene_; }
    [[nodiscard]] const vector<Fitness>& fitness() const noexcept { return fitness_; }
    [[nodiscard]] const vector<int>& born_at() const noexcept { return born_at_; }

    // ---- 要素取得 ----
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

    // ---- 容量管理 ----
    void reserve(int n) {
        if (n <= 0) return;
        gene_.reserve(n);
        fitness_.reserve(n);
        born_at_.reserve(n);
    }

    // ---- 初期化 ----
    void clear() {
        gene_.clear();
        fitness_.clear();
        born_at_.clear();
        current_tick_ = 0;
    }

    // ---- tick 管理 ----
    // 個体の born_at は変えず、現在時刻だけ進める。
    void advance_tick(int dt = 1) {
        if (dt <= 0) return;
        current_tick_ += dt;
    }

    // ---- 追加/置換 ----
    // 追加：末尾に push。戻り値は追加された index。
    int add(const Gene& g, Fitness f) { return add_impl_(g, f); }
    int add(Gene&& g, Fitness f) { return add_impl_(move(g), f); }

    // Fitness が float 等になったとき、double リテラル等からの暗黙変換で
    // -Wfloat-conversion が出ないようにするための受け口。
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

    // 置換：既存 index を上書き。born_at は current_tick_ に更新。
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

    // Fitness のみ更新（born_at は変えない）。
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

    // ---- in-place 更新 ----
    // callback は (Gene&) または (Gene&, int) を受け取れる。
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

    // ---- 比較（a が b より良いか）----
    bool better(int a, int b) const {
        assert(0 <= a && a < size());
        assert(0 <= b && b < size());

        const Fitness fa = fitness_[a];
        const Fitness fb = fitness_[b];

        const bool na = isnan(fa);
        const bool nb = isnan(fb);
        if (na != nb) return !na;            // NaN は常に悪い
        if (!na && fa != fb) return fa > fb; // 大きいほど良い

        const int ba = born_at_[a];
        const int bb = born_at_[b];
        if (ba != bb) return ba > bb;        // 新しいほど良い

        return a < b;                        // 最後は index で安定化
    }

    // ---- best/worst ----
    // 空なら -1。
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

    // ---- 並べ替え index（best first）----
    void get_sorted_indices(vector<int>& out) const {
        const int n = size();
        out.resize(n);
        iota(out.begin(), out.end(), 0);
        sort(out.begin(), out.end(), [&](int a, int b) { return better(a, b); });
    }

    // ---- best fitness ----
    // best index の fitness を返す（空なら NaN）。
    Fitness get_best_fitness() const {
        const int best = get_best_index();
        if (best < 0) return kNaN;
        return fitness_[best];
    }

    // ---- 要約統計 ----
    // fitness は NaN を無視して min/max/avg を計算（全て NaN なら NaN のまま）。
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

        // fitness
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

        // born_at
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

    // ---- トーナメント選択（k サンプル）----
    // k<=1 の場合は一様ランダムに 1 個体。
    int select_tournament_best(int k) const { return select_tournament_impl_(k, true); }

    // k 個体から逆トーナメント（worst）を選ぶ。
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
                if (better(pick, cand)) pick = cand; // cand の方が悪ければ入れ替え
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

// ------------------------------
// fill_random (std::array)
// ------------------------------
// arr の全要素を [lo, hi] の乱数で埋める（整数/浮動小数点/enum(int相当)）
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

// ------------------------------
// fill_perm (std::array)
// ------------------------------
// arr を 0..N-1 で埋める（0-indexed 固定）。順列のデフォルト初期化用。
// - T は整数型または enum(int相当) を想定。
template<class T, size_t N>
inline void fill_perm(array<T, N>& arr) {
    static_assert(is_integral_v<T> || is_enum_v<T>,
                  "fill_perm(array): T must be integral or enum(int-like).");
    if constexpr (N == 0) return;
    for (size_t i = 0; i < N; ++i) arr[i] = (T)i;
}

// ------------------------------
// fill_random_perm (std::array)
// ------------------------------
// 0..N-1 の順列を生成して arr に格納する（0-indexed 固定）。
// - T は整数型または enum(int相当) を想定。
// - 生成は Fisher-Yates shuffle。
template<class T, size_t N>
inline void fill_random_perm(array<T, N>& arr) {
    static_assert(is_integral_v<T> || is_enum_v<T>,
                  "fill_random_perm(array): T must be integral or enum(int-like).");

    if constexpr (N == 0) return;
    fill_perm(arr);

    // Fisher-Yates: i=N-1..1, swap(i, j in [0,i])
    for (size_t i = N - 1; i > 0; --i) {
        const size_t j = rand_index<size_t>(i + 1);
        swap(arr[i], arr[j]);
    }
}

// ------------------------------
// fill_random (std::bitset)
// ------------------------------
// 32bit 単位で乱数を取り、bitset に詰める
template<size_t N>
inline void fill_random(bitset<N>& bs) {
    if constexpr (N == 0) return;

    constexpr size_t kWordBits = 32;
    constexpr size_t kWords = (N + kWordBits - 1) / kWordBits;

    size_t bit = 0;
    for (size_t w = 0; w < kWords; ++w) {
        const uint32_t r = rand_u32();
        // 32bit 分を LSB→MSB の順で詰める（index0 が LSB の慣習に合わせる）
        for (size_t j = 0; j < kWordBits && bit < N; ++j, ++bit) {
            bs.set(bit, ((r >> j) & 1u) != 0u);
        }
    }
}

// ------------------------------------------------------------
// Rolling horizon (shift by 1)
// ------------------------------------------------------------
// ・第1引数: array<T,N>& または bitset<N>&
// ・第2引数: 末尾に埋める値（array: T, bitset: bool）
// ・更新は in-place。
// ・新しい値は new[i] = old[i+1]（つまり new[0] に old[1] が来る）
// ・空いた末尾 new[N-1] に fill を入れる
//
// array の要素が trivially_copyable の場合は memmove を使用する。
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

    // new[i] = old[i+1] を実現（index0 が LSB） -> >>= 1
    b >>= 1;
    b.set(N - 1, fill);
}

// DiversityCalculator<Gene>
// ------------------------
// 個体群(population)の「多様性」を、複数の特徴量(feature)のスコア(0..1)の平均として評価する軽量ユーティリティ。
// CodinGame 等ではシミュレーション(sim)が支配的になりやすいので、ここは「読みやすさ優先・十分速い」実装を狙う。
template<class Gene>
struct DiversityCalculator {
    double sum = 0.0;
    int n = 0;

    void reset() { sum = 0.0; n = 0; }

    double diversity_rate() const { return (n > 0) ? (sum / (double)n) : 0.0; }

    // ------------------------------------------------------------
    // process #1: numeric / enum / array
    // ------------------------------------------------------------
    template<class F, class V>
    void process(const vector<Gene>& pop, F&& accessor, V lo, V hi, bool is_enum) {
        using Ret = detail::accessor_ret_t<F, Gene>;

        normalize_range(lo, hi);

        // ---------- integral / enum ----------
        if constexpr (is_integral_v<V> || is_enum_v<V>) {
            const long long span = (long long)hi - (long long)lo;
            if (span == 0) return; // 正規化不能：スキップ

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
                    // enum unique:
                    // - 値域が狭い場合は seen 方式（O(pop*N)）で unique 数を数える。
                    // - 値域が広い場合は sort+unique に fallback。
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

        // ---------- floating ----------
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

    // ------------------------------------------------------------
    // process #2: bool / bitset
    //  - bool: empty->0, both->1.0, else->0.5
    //  - bitset: OR/AND で一度だけ走査し、bitごとの bool多様性を平均
    // ------------------------------------------------------------
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
            // per-bit: both->1.0, only_one->0.5  => 0.5 + 0.5*(both_fraction)
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

} // namespace ga

#if 0
// ------------------------------------------------------------
// Test / Benchmark (disable by changing 1 -> 0)
// ------------------------------------------------------------

namespace ga_test {

// ------------------------------------------------------------
// Minimal test framework
// ------------------------------------------------------------
struct Runner {
    int passed = 0;
    int failed = 0;

    void expect(bool cond, const char* expr, const char* file, int line) {
        if (cond) {
            ++passed;
        } else {
            ++failed;
            cerr << "[FAIL] " << file << ":" << line << " : " << expr << "\n";
        }
    }

    template<class A, class B>
    void expect_eq(const A& a, const B& b, const char* expr_a, const char* expr_b, const char* file, int line) {
        if (a == b) {
            ++passed;
        } else {
            ++failed;
            cerr << "[FAIL] " << file << ":" << line << " : (" << expr_a << " == " << expr_b << ")\n"
                      << "       lhs=" << a << " rhs=" << b << "\n";
        }
    }

    void expect_near(double a, double b, double eps, const char* expr_a, const char* expr_b, const char* file, int line) {
        const double diff = fabs(a - b);
        if (diff <= eps) {
            ++passed;
        } else {
            ++failed;
            cerr << "[FAIL] " << file << ":" << line << " : near(" << expr_a << "," << expr_b << ",eps)\n"
                      << "       lhs=" << a << " rhs=" << b << " diff=" << diff << " eps=" << eps << "\n";
        }
    }
};

#define GL_EXPECT(R, COND) (R).expect((COND), #COND, __FILE__, __LINE__)
#define GL_EXPECT_EQ(R, A, B) (R).expect_eq((A), (B), #A, #B, __FILE__, __LINE__)
#define GL_EXPECT_NEAR(R, A, B, EPS) (R).expect_near((double)(A), (double)(B), (double)(EPS), #A, #B, __FILE__, __LINE__)

// ------------------------------------------------------------
// Compile-time checks for internal helpers (detail::*)
// ------------------------------------------------------------
static_assert(ga::detail::is_std_array_v<array<int, 3>>);
static_assert(!ga::detail::is_std_array_v<int>);
static_assert(ga::detail::is_std_bitset_v<bitset<7>>);
static_assert(!ga::detail::is_std_bitset_v<vector<int>>);

struct AccessorGene {
    int x = 0;
    array<float, 2> a = {0.0f, 0.0f};
};
static_assert(is_same_v<ga::detail::remove_cvref_t<const int&>, int>);
static_assert(is_same_v<ga::detail::accessor_ret_t<decltype([](const AccessorGene& g)->const int&{return g.x;}), AccessorGene>, int>);
static_assert(is_same_v<ga::detail::accessor_ret_t<decltype([](const AccessorGene& g)->array<float,2>{return g.a;}), AccessorGene>, array<float,2>>);

// ------------------------------------------------------------
// Utilities for tests
// ------------------------------------------------------------
struct RngGuard {
    mt19937 saved;
    explicit RngGuard(uint32_t seed) : saved(ga::rng) { ga::rng.seed(seed); }
    ~RngGuard() { ga::rng = saved; }
};

template<class T>
static inline bool is_nan(T x) {
    static_assert(is_floating_point_v<T>, "is_nan: T must be floating point.");
    return isnan(x);
}

// ------------------------------------------------------------
// Tests: RNG / timing / stats
// ------------------------------------------------------------
static void test_rng_determinism(Runner& tr) {
    const mt19937 saved = ga::rng;

    ga::rng.seed(12345);
    const auto a0 = ga::rng();
    const auto a1 = ga::rng();

    ga::rng.seed(12345);
    const auto b0 = ga::rng();
    const auto b1 = ga::rng();

    GL_EXPECT_EQ(tr, a0, b0);
    GL_EXPECT_EQ(tr, a1, b1);

    ga::rng = saved;
}

static void test_now_ms_monotonic(Runner& tr) {
    const double t0 = ga::now_ms();
    // small work
    volatile uint64_t s = 0;
    for (int i = 0; i < 1000; ++i) s = s + static_cast<uint64_t>(i);
    const double t1 = ga::now_ms();
    (void)s;
    GL_EXPECT(tr, isfinite(t0));
    GL_EXPECT(tr, isfinite(t1));
    GL_EXPECT(tr, t1 >= t0);
}

static void test_stats_reset_and_dump_smoke(Runner& tr) {
    ga::reset_stats();
    GL_EXPECT_EQ(tr, ga::stats.population_add, 0ull);
    GL_EXPECT_EQ(tr, ga::stats.population_set, 0ull);
    GL_EXPECT_EQ(tr, ga::stats.population_update, 0ull);

    // Smoke: dump は内容検証が難しい（stderr 直書きのため）ので、呼べることだけ確認
    ga::stats.dump_stderr("ga_test::stats_smoke");
    ga::debug_print_stats();

    ga::stats.population_add = 7;
    ga::stats.population_set = 11;
    ga::stats.population_update = 13;
    ga::reset_stats();
    GL_EXPECT_EQ(tr, ga::stats.population_add, 0ull);
    GL_EXPECT_EQ(tr, ga::stats.population_set, 0ull);
    GL_EXPECT_EQ(tr, ga::stats.population_update, 0ull);
}

// ------------------------------------------------------------
// Tests: Population (unit + edge + scenarios)
// ------------------------------------------------------------
static void test_population_basic_and_edges(Runner& tr) {
    ga::reset_stats();

    ga::Population<int> pop;
    GL_EXPECT(tr, pop.empty());
    GL_EXPECT_EQ(tr, pop.size(), 0);
    GL_EXPECT_EQ(tr, pop.current_tick(), 0);
    GL_EXPECT_EQ(tr, pop.get_best_index(), -1);
    GL_EXPECT_EQ(tr, pop.get_worst_index(), -1);
    GL_EXPECT(tr, is_nan(pop.get_best_fitness()));

    {
        const auto st = pop.get_stats();
        GL_EXPECT_EQ(tr, st.n, 0u);
        GL_EXPECT(tr, is_nan(st.fitness_min));
        GL_EXPECT(tr, is_nan(st.fitness_max));
        GL_EXPECT(tr, is_nan(st.fitness_avg));
        // born_at_* are unspecified for empty (current implementation: 0/0/NaN)
        GL_EXPECT(tr, is_nan(st.born_at_avg));
    }

    // reserve edge
    pop.reserve(0);
    pop.reserve(-1);
    GL_EXPECT_EQ(tr, pop.size(), 0);
    pop.reserve(16);

    // add (lvalue/rvalue)
    int g = 10;
    const int i0 = pop.add(g, 1.0);
    const int i1 = pop.add(20, 2.0);
    GL_EXPECT_EQ(tr, i0, 0);
    GL_EXPECT_EQ(tr, i1, 1);

    GL_EXPECT_EQ(tr, pop.size(), 2);
    GL_EXPECT(tr, !pop.empty());

    GL_EXPECT_EQ(tr, pop.gene(i0), 10);
    GL_EXPECT_EQ(tr, pop.gene(i1), 20);
    GL_EXPECT_NEAR(tr, pop.fitness(i0), 1.0, 1e-12);
    GL_EXPECT_NEAR(tr, pop.fitness(i1), 2.0, 1e-12);

    // accessors
    GL_EXPECT_EQ(tr, (int)pop.gene().size(), 2);
    GL_EXPECT_EQ(tr, (int)pop.fitness().size(), 2);
    GL_EXPECT_EQ(tr, (int)pop.born_at().size(), 2);
    GL_EXPECT_EQ(tr, pop.born_at(i0), 0);
    GL_EXPECT_EQ(tr, pop.born_at(i1), 0);

    // advance_tick edge
    pop.advance_tick(0);
    pop.advance_tick(-7);
    GL_EXPECT_EQ(tr, pop.current_tick(), 0);
    pop.advance_tick(3);
    GL_EXPECT_EQ(tr, pop.current_tick(), 3);

    // set updates born_at
    pop.set(i0, 99, 1.5);
    GL_EXPECT_EQ(tr, pop.gene(i0), 99);
    GL_EXPECT_NEAR(tr, pop.fitness(i0), 1.5, 1e-12);
    GL_EXPECT_EQ(tr, pop.born_at(i0), 3);

    // set_fitness does not update born_at
    pop.set_fitness(i0, 9.0);
    GL_EXPECT_NEAR(tr, pop.fitness(i0), 9.0, 1e-12);
    GL_EXPECT_EQ(tr, pop.born_at(i0), 3);

    // update (Gene&)
    pop.update(i1, [](int& x) { x += 1; });
    GL_EXPECT_EQ(tr, pop.gene(i1), 21);

    // update (Gene&, int)
    pop.update(i1, [](int& x, int idx) { x += idx; });
    GL_EXPECT_EQ(tr, pop.gene(i1), 22);

    // stats counters
    GL_EXPECT_EQ(tr, ga::stats.population_add, 2ull);
    GL_EXPECT_EQ(tr, ga::stats.population_set, 1ull);
    GL_EXPECT_EQ(tr, ga::stats.population_update, 2ull);

    // better / best / worst
    // i0 fitness=9.0, i1 fitness=2.0 -> i0 better
    GL_EXPECT(tr, pop.better(i0, i1));
    GL_EXPECT(tr, !pop.better(i1, i0));
    GL_EXPECT_EQ(tr, pop.get_best_index(), i0);
    GL_EXPECT_EQ(tr, pop.get_worst_index(), i1);
    GL_EXPECT_NEAR(tr, pop.get_best_fitness(), 9.0, 1e-12);

    // sorted indices best-first
    vector<int> order;
    pop.get_sorted_indices(order);
    GL_EXPECT_EQ(tr, (int)order.size(), 2);
    GL_EXPECT_EQ(tr, order[0], i0);
    GL_EXPECT_EQ(tr, order[1], i1);

    // get_stats (non-NaN only)
    {
        const auto st = pop.get_stats();
        GL_EXPECT_EQ(tr, st.n, 2u);
        GL_EXPECT_NEAR(tr, st.fitness_min, 2.0, 1e-12);
        GL_EXPECT_NEAR(tr, st.fitness_max, 9.0, 1e-12);
        GL_EXPECT_NEAR(tr, st.fitness_avg, 5.5, 1e-12);

        GL_EXPECT_EQ(tr, st.born_at_min, 0);
        GL_EXPECT_EQ(tr, st.born_at_max, 3);
        GL_EXPECT_NEAR(tr, st.born_at_avg, 1.5, 1e-12);
    }

    // clear
    pop.clear();
    GL_EXPECT(tr, pop.empty());
    GL_EXPECT_EQ(tr, pop.size(), 0);
    GL_EXPECT_EQ(tr, pop.current_tick(), 0);
}

static void test_population_nan_and_tiebreaks(Runner& tr) {
    ga::reset_stats();

    ga::Population<int> pop;
    pop.add(1, ga::kNaN);
    pop.add(2, 10.0);
    pop.add(3, ga::kNaN);

    // NaN are always worst
    const int best = pop.get_best_index();
    GL_EXPECT_EQ(tr, best, 1);
    const int worst = pop.get_worst_index();
    GL_EXPECT(tr, worst == 0 || worst == 2);

    // tie break: same fitness -> born_at newer is better, else index smaller
    ga::Population<int> pop2;
    pop2.add(10, 5.0); // idx0 born_at 0
    pop2.add(11, 5.0); // idx1 born_at 0
    GL_EXPECT(tr, pop2.better(0, 1)); // same fitness, same born_at, smaller index wins
    pop2.advance_tick(1);
    pop2.set(1, 11, 5.0); // update born_at of idx1 to 1
    GL_EXPECT(tr, pop2.better(1, 0)); // newer born_at wins
    GL_EXPECT_EQ(tr, pop2.get_best_index(), 1);

    // stats: add=5, set=1
    GL_EXPECT_EQ(tr, ga::stats.population_add, 5ull);
    GL_EXPECT_EQ(tr, ga::stats.population_set, 1ull);
}

static void test_population_tournament_selection(Runner& tr) {
    ga::Population<int> pop;
    for (int i = 0; i < 10; ++i) pop.add(i, (double)i); // fitness ascending

    // k<=1 : uniform draw
    {
        RngGuard g(7);
        const int idx = pop.select_tournament_best(1);
        GL_EXPECT(tr, 0 <= idx && idx < pop.size());
        // replicate expected draw
        ga::rng.seed(7);
        const int expected = ga::rand_int(0, pop.size() - 1);
        GL_EXPECT_EQ(tr, idx, expected);
    }

    // k>n : clamp to n
    {
        RngGuard g(9);
        const int idx_best = pop.select_tournament_best(1000);
        GL_EXPECT(tr, 0 <= idx_best && idx_best < pop.size());
        // If k==n, best is global best with probability 1 (because all indices sampled with replacement is not guaranteed),
        // but this implementation samples with replacement, so it is not guaranteed. We instead validate the sampling logic deterministically.
        ga::rng.seed(9);
        int expected = ga::rand_int(0, pop.size() - 1);
        for (int i = 1; i < pop.size(); ++i) {
            const int cand = ga::rand_int(0, pop.size() - 1);
            if (pop.better(cand, expected)) expected = cand;
        }
        GL_EXPECT_EQ(tr, idx_best, expected);
    }

    // worst selection
    {
        RngGuard g(11);
        const int idx_worst = pop.select_tournament_worst(4);
        GL_EXPECT(tr, 0 <= idx_worst && idx_worst < pop.size());

        ga::rng.seed(11);
        int expected = ga::rand_int(0, pop.size() - 1);
        for (int i = 1; i < 4; ++i) {
            const int cand = ga::rand_int(0, pop.size() - 1);
            if (pop.better(expected, cand)) expected = cand;
        }
        GL_EXPECT_EQ(tr, idx_worst, expected);
    }

    // empty edge
    ga::Population<int> empty;
    GL_EXPECT_EQ(tr, empty.select_tournament_best(3), -1);
    GL_EXPECT_EQ(tr, empty.select_tournament_worst(3), -1);
}

// ---- Population scenario tests (5 use cases) ----

// Scenario 1: Simple elitism replacement loop (best preserved, worst replaced)
static void scenario_population_elitism_replacement(Runner& tr) {
    ga::Population<int> pop;
    pop.reserve(8);
    for (int i = 0; i < 8; ++i) pop.add(i, (double)i);

    const int elite = pop.get_best_index();
    const int worst = pop.get_worst_index();
    GL_EXPECT_EQ(tr, elite, 7);
    GL_EXPECT_EQ(tr, worst, 0);

    pop.advance_tick(1);
    pop.set(worst, 999, pop.get_best_fitness() - (ga::Fitness)0.1); // replace worst with strong near-elite
    GL_EXPECT(tr, pop.get_best_index() == elite); // elite should remain best
    GL_EXPECT_EQ(tr, pop.gene(worst), 999);
    GL_EXPECT_EQ(tr, pop.born_at(worst), 1);
}

// Scenario 2: Age-based tie-breaking (same fitness)
static void scenario_population_age_tiebreak(Runner& tr) {
    ga::Population<int> pop;
    pop.add(1, 1.0);
    pop.add(2, 1.0);
    GL_EXPECT_EQ(tr, pop.get_best_index(), 0); // same tick, smaller index

    pop.advance_tick(10);
    pop.set(1, 2, 1.0); // same fitness, but newer
    GL_EXPECT_EQ(tr, pop.get_best_index(), 1);
}

// Scenario 3: Handling unevaluated individuals (NaN fitness)
static void scenario_population_nan_management(Runner& tr) {
    ga::Population<int> pop;
    pop.add(1, ga::kNaN); // not evaluated
    pop.add(2, 0.0);
    pop.add(3, -1.0);
    GL_EXPECT_EQ(tr, pop.get_best_index(), 1);
    GL_EXPECT(tr, pop.get_worst_index() == 0); // NaN should be worst
}

// Scenario 4: Mutation via update callback without touching fitness/born_at
static void scenario_population_mutation_update(Runner& tr) {
    ga::Population<int> pop;
    pop.add(10, 1.0);
    const int born0 = pop.born_at(0);

    pop.update(0, [](int& g) { g *= 2; });
    GL_EXPECT_EQ(tr, pop.gene(0), 20);
    GL_EXPECT_EQ(tr, pop.born_at(0), born0); // update does not change born_at
    GL_EXPECT_NEAR(tr, pop.fitness(0), 1.0, 1e-12);
}

// Scenario 5: Tournament selection pressure (best of k should not be worse than a single draw in expectation; deterministic check)
static void scenario_population_tournament_pressure(Runner& tr) {
    ga::Population<int> pop;
    for (int i = 0; i < 20; ++i) pop.add(i, (double)i);

    RngGuard g(123);
    const int k1 = pop.select_tournament_best(1);
    const int k5 = pop.select_tournament_best(5);
    GL_EXPECT(tr, pop.better(k5, k1) || k5 == k1); // with same RNG stream, not guaranteed strictly, but should be comparable
}

// ------------------------------------------------------------
// Tests: fill_random / rolling_horizon
// ------------------------------------------------------------
static void test_fill_random_array(Runner& tr) {
    {
        RngGuard g(1);
        array<int, 16> a{};
        ga::fill_random(a, -3, 7);
        for (int x : a) GL_EXPECT(tr, (-3 <= x && x <= 7));

        // determinism on reseed
        array<int, 16> b{};
        ga::rng.seed(1);
        ga::fill_random(b, -3, 7);
        GL_EXPECT(tr, a == b);
    }

    // edge N==0
    {
        RngGuard g(2);
        array<int, 0> a0{};
        ga::fill_random(a0, 0, 1); // should not crash
        GL_EXPECT_EQ(tr, a0.size(), 0u);
    }

    // float
    {
        RngGuard g(3);
        array<float, 32> a{};
        ga::fill_random(a, -1.0f, 1.0f);
        for (float x : a) GL_EXPECT(tr, (-1.0f <= x && x <= 1.0f));

        array<float, 32> b{};
        ga::rng.seed(3);
        ga::fill_random(b, -1.0f, 1.0f);
        GL_EXPECT(tr, a == b);
    }
    // enum (int-like)
    {
        enum class E : int { A = 0, B = 1, C = 2, D = 3 };
        RngGuard g(4);
        array<E, 64> a{};
        ga::fill_random(a, E::B, E::D);
        for (E x : a) {
            const int v = static_cast<int>(x);
            GL_EXPECT(tr, 1 <= v && v <= 3);
        }

        // determinism on reseed
        array<E, 64> b{};
        ga::rng.seed(4);
        ga::fill_random(b, E::B, E::D);
        GL_EXPECT(tr, a == b);
    }

    // enum edge: swapped bounds should still work (normalize_range inside fill_random)
    {
        enum class E : int { A = 0, B = 1, C = 2 };
        RngGuard g(5);
        array<E, 32> a{};
        ga::fill_random(a, E::C, E::A);
        for (E x : a) {
            const int v = static_cast<int>(x);
            GL_EXPECT(tr, 0 <= v && v <= 2);
        }
    }

}

static void test_fill_random_bitset(Runner& tr) {
    // deterministic replicate
    {
        RngGuard g(7);
        bitset<64> bs{};
        ga::fill_random(bs);

        ga::rng.seed(7);
        const uint32_t r0 = ga::rand_u32();
        const uint32_t r1 = ga::rand_u32();

        bitset<64> expected{};
        for (size_t j = 0; j < 32; ++j) expected.set(j, ((r0 >> j) & 1u) != 0u);
        for (size_t j = 0; j < 32; ++j) expected.set(32 + j, ((r1 >> j) & 1u) != 0u);

        GL_EXPECT(tr, bs == expected);
    }

    // edge N==1
    {
        RngGuard g(9);
        bitset<1> b{};
        ga::fill_random(b);
        GL_EXPECT(tr, b.test(0) == false || b.test(0) == true);
    }

    // edge N==0
    {
        RngGuard g(10);
        bitset<0> b{};
        ga::fill_random(b); // should not crash
        GL_EXPECT_EQ(tr, b.size(), 0u);
    }
}


static void test_fill_perm(Runner& tr) {
    // normal int
    {
        array<int, 8> a{};
        ga::fill_perm(a);
        for (int i = 0; i < (int)a.size(); ++i) GL_EXPECT_EQ(tr, a[i], i);
    }

    // enum
    {
        enum class E : int { V0=0, V1=1, V2=2, V3=3, V4=4 };
        array<E, 5> a{};
        ga::fill_perm(a);
        for (int i = 0; i < 5; ++i) GL_EXPECT_EQ(tr, (int)a[i], i);
    }

    // edge N==1
    {
        array<int, 1> a{};
        ga::fill_perm(a);
        GL_EXPECT_EQ(tr, a[0], 0);
    }

    // edge N==0
    {
        array<int, 0> a{};
        ga::fill_perm(a); // should not crash
        GL_EXPECT_EQ(tr, a.size(), 0u);
    }
}

static void test_fill_random_perm(Runner& tr) {
    // normal: permutation properties + determinism
    {
        RngGuard g(11);
        array<int, 16> a{};
        ga::fill_random_perm(a);

        array<int, 16> cnt{};
        for (int v : a) {
            GL_EXPECT(tr, 0 <= v && v < 16);
            ++cnt[v];
        }
        for (int c : cnt) GL_EXPECT_EQ(tr, c, 1);

        array<int, 16> b{};
        ga::rng.seed(11);
        ga::fill_random_perm(b);
        GL_EXPECT(tr, a == b);
    }

    // enum permutation
    {
        enum class E : int { V0=0, V1=1, V2=2, V3=3, V4=4, V5=5, V6=6, V7=7 };
        RngGuard g(12);
        array<E, 8> a{};
        ga::fill_random_perm(a);

        array<int, 8> cnt{};
        for (E x : a) {
            const int v = static_cast<int>(x);
            GL_EXPECT(tr, 0 <= v && v < 8);
            ++cnt[v];
        }
        for (int c : cnt) GL_EXPECT_EQ(tr, c, 1);
    }

    // edge N==1
    {
        RngGuard g(13);
        array<int, 1> a{};
        ga::fill_random_perm(a);
        GL_EXPECT_EQ(tr, a[0], 0);
    }

    // edge N==0
    {
        RngGuard g(14);
        array<int, 0> a{};
        ga::fill_random_perm(a); // should not crash
        GL_EXPECT_EQ(tr, a.size(), 0u);
    }
}

struct NonTrivial {
    int x = 0;
    string s;
};

static void test_apply_rolling_horizon_array(Runner& tr) {
    // trivially copyable
    {
        array<int, 5> a = {1,2,3,4,5};
        ga::apply_rolling_horizon(a, 99);
        GL_EXPECT(tr, (a == array<int,5>{2,3,4,5,99}));
    }

    // non-trivial (assignment loop)
    {
        array<NonTrivial, 3> a = {NonTrivial{1,"a"}, NonTrivial{2,"b"}, NonTrivial{3,"c"}};
        ga::apply_rolling_horizon(a, NonTrivial{9,"z"});
        GL_EXPECT_EQ(tr, a[0].x, 2);
        GL_EXPECT_EQ(tr, a[0].s, string("b"));
        GL_EXPECT_EQ(tr, a[1].x, 3);
        GL_EXPECT_EQ(tr, a[1].s, string("c"));
        GL_EXPECT_EQ(tr, a[2].x, 9);
        GL_EXPECT_EQ(tr, a[2].s, string("z"));
    }

    // edge N==1
    {
        array<int, 1> a = {5};
        ga::apply_rolling_horizon(a, 42);
        GL_EXPECT_EQ(tr, a[0], 42);
    }

    // edge N==0
    {
        array<int, 0> a0{};
        ga::apply_rolling_horizon(a0, 1); // should not crash
        GL_EXPECT_EQ(tr, a0.size(), 0u);
    }
}

static void test_apply_rolling_horizon_bitset(Runner& tr) {
    {
        bitset<8> b(string("10010110")); // MSB..LSB string ctor
        // bit indices: 0 is rightmost
        const bitset<8> old = b;
        ga::apply_rolling_horizon(b, true);

        // b >>= 1 then set bit7
        bitset<8> expected = old >> 1;
        expected.set(7, true);
        GL_EXPECT(tr, b == expected);
    }

    // edge N==1
    {
        bitset<1> b{};
        ga::apply_rolling_horizon(b, true);
        GL_EXPECT(tr, b.test(0));
    }

    // edge N==0
    {
        bitset<0> b{};
        ga::apply_rolling_horizon(b, true);
        GL_EXPECT_EQ(tr, b.size(), 0u);
    }
}

// ------------------------------------------------------------
// Tests: DiversityCalculator (scenarios + edges for various accessor types)
// ------------------------------------------------------------
enum class TestEnum : int { A = 0, B = 1, C = 2, D = 3 };

struct DGene {
    bool flag = false;
    int iv = 0;
    float fv = 0.0f;
    bitset<8> bv{};
    TestEnum ev = TestEnum::A;
    array<int, 3> ai{};
    array<float, 3> af{};
    array<TestEnum, 3> ae{};
};

static void test_diversity_bool(Runner& tr) {
    ga::DiversityCalculator<DGene> dc;

    // scenario: both true/false -> 1.0
    {
        vector<DGene> pop = {DGene{false}, DGene{true}};
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.flag; });
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 1.0, 1e-12);
    }

    // scenario: only one value -> 0.5
    {
        vector<DGene> pop(5);
        for (auto& g : pop) g.flag = true;
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.flag; });
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.5, 1e-12);
    }

    // edge: empty -> 0.0
    {
        vector<DGene> pop;
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.flag; });
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.0, 1e-12);
    }
}

static void test_diversity_int(Runner& tr) {
    ga::DiversityCalculator<DGene> dc;

    // scenario: range max-min
    {
        vector<DGene> pop(3);
        pop[0].iv = 0;
        pop[1].iv = 5;
        pop[2].iv = 10;
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.iv; }, 0, 10, false);
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 1.0, 1e-12);
    }

// scenario: clamp to [lo,hi] before scoring (out-of-range values should not inflate score)
{
    vector<DGene> pop(3);
    pop[0].iv = -5;
    pop[1].iv = 2;
    pop[2].iv = 20;
    dc.reset();
    dc.process(pop, [](const DGene& g) { return g.iv; }, 0, 10, false);
    // after clamp: {0,2,10} => (10-0)/(10-0)=1.0
    GL_EXPECT_NEAR(tr, dc.diversity_rate(), 1.0, 1e-12);
}

    // scenario: enum-mode unique count
    {
        vector<DGene> pop(4);
        pop[0].iv = 0;
        pop[1].iv = 0;
        pop[2].iv = 1;
        pop[3].iv = 3;
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.iv; }, 0, 3, true);
        // uniq={0,1,3} => (3-1)/(3-0)=2/3
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 2.0 / 3.0, 1e-12);
    }

// scenario: enum-mode also clamps out-of-range values
{
    vector<DGene> pop(4);
    pop[0].iv = -1;
    pop[1].iv = 0;
    pop[2].iv = 3;
    pop[3].iv = 5;
    dc.reset();
    dc.process(pop, [](const DGene& g) { return g.iv; }, 0, 3, true);
    // after clamp: {0,0,3,3} => uniq={0,3} => (2-1)/(3-0)=1/3
    GL_EXPECT_NEAR(tr, dc.diversity_rate(), 1.0 / 3.0, 1e-12);
}

    // edge: lo==hi -> skipped (n stays 0 => diversity_rate 0)
    {
        vector<DGene> pop(3);
        pop[0].iv = 7;
        pop[1].iv = 7;
        pop[2].iv = 7;
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.iv; }, 7, 7, false);
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.0, 1e-12);
        GL_EXPECT_EQ(tr, dc.n, 0);
    }

    // edge: empty pop -> 0
    {
        vector<DGene> pop;
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.iv; }, 0, 10, false);
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.0, 1e-12);
    }
}

static void test_diversity_float(Runner& tr) {
    ga::DiversityCalculator<DGene> dc;

    // scenario: ignores NaN
    {
        vector<DGene> pop(3);
        pop[0].fv = numeric_limits<float>::quiet_NaN();
        pop[1].fv = 1.0f;
        pop[2].fv = 3.0f;
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.fv; }, 0.0f, 10.0f, false);
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.2, 1e-12);
    }

// scenario: clamp to [lo,hi] before scoring
{
    vector<DGene> pop(3);
    pop[0].fv = -5.0f;
    pop[1].fv = 2.0f;
    pop[2].fv = 20.0f;
    dc.reset();
    dc.process(pop, [](const DGene& g) { return g.fv; }, 0.0f, 10.0f, false);
    // after clamp: {0,2,10} => (10-0)/10 = 1.0
    GL_EXPECT_NEAR(tr, dc.diversity_rate(), 1.0, 1e-12);
}

    // edge: all NaN -> score 0 but counted (n==1)
    {
        vector<DGene> pop(4);
        for (auto& g : pop) g.fv = numeric_limits<float>::quiet_NaN();
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.fv; }, -1.0f, 1.0f, false);
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.0, 1e-12);
        GL_EXPECT_EQ(tr, dc.n, 1);
    }

    // edge: empty
    {
        vector<DGene> pop;
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.fv; }, 0.0f, 1.0f, false);
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.0, 1e-12);
    }
}

static void test_diversity_bitset(Runner& tr) {
    ga::DiversityCalculator<DGene> dc;

    // scenario: 8 bits, 4 bits vary => 0.5 + 0.5*(4/8)=0.75
    {
        vector<DGene> pop(2);
        pop[0].bv = bitset<8>(0x00);
        pop[1].bv = bitset<8>(0xF0);
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.bv; });
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.75, 1e-12);
    }

    // scenario: all identical => 0.5
    {
        vector<DGene> pop(3);
        for (auto& g : pop) g.bv = bitset<8>(0xAA);
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.bv; });
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.5, 1e-12);
    }

    // edge: empty => 0
    {
        vector<DGene> pop;
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.bv; });
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.0, 1e-12);
    }

    // edge: bitset<0>
    {
        struct G0 { bitset<0> b{}; };
        vector<G0> pop(2);
        ga::DiversityCalculator<G0> dc0;
        dc0.reset();
        dc0.process(pop, [](const G0& g) { return g.b; });
        GL_EXPECT_NEAR(tr, dc0.diversity_rate(), 0.0, 1e-12);
    }
}

static void test_diversity_enum(Runner& tr) {
    ga::DiversityCalculator<DGene> dc;

    // scenario: enum unique count (is_enum=true)
    {
        vector<DGene> pop(3);
        pop[0].ev = TestEnum::A;
        pop[1].ev = TestEnum::C;
        pop[2].ev = TestEnum::D;
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.ev; }, TestEnum::A, TestEnum::D, true);
        // uniq=3 => (3-1)/(D-A)=2/3
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 2.0 / 3.0, 1e-12);
    }

    // scenario: enum treated as numeric span (is_enum=false)
    {
        vector<DGene> pop(2);
        pop[0].ev = TestEnum::B;
        pop[1].ev = TestEnum::D;
        ga::DiversityCalculator<DGene> dc2;
        dc2.reset();
        dc2.process(pop, [](const DGene& g) { return g.ev; }, TestEnum::A, TestEnum::D, false);
        // max-min=(D-B)=2, denom=(D-A)=3 => 2/3
        GL_EXPECT_NEAR(tr, dc2.diversity_rate(), 2.0 / 3.0, 1e-12);
    }

    // edge: lo==hi skip
    {
        vector<DGene> pop(2);
        pop[0].ev = TestEnum::B;
        pop[1].ev = TestEnum::B;
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.ev; }, TestEnum::B, TestEnum::B, true);
        GL_EXPECT_EQ(tr, dc.n, 0);
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.0, 1e-12);
    }
}

static void test_diversity_array_int(Runner& tr) {
    ga::DiversityCalculator<DGene> dc;

    // scenario: per-index min/max average
    {
        vector<DGene> pop(2);
        pop[0].ai = {0,0,0};
        pop[1].ai = {10,5,0};
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.ai; }, 0, 10, false);
        // diffs: [10,5,0] / 10 => [1,0.5,0], avg=0.5
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.5, 1e-12);
    }

    // scenario: enum unique count per index
    {
        vector<DGene> pop(3);
        pop[0].ai = {0,0,0};
        pop[1].ai = {0,1,0};
        pop[2].ai = {2,1,0};
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.ai; }, 0, 2, true);
        // idx0 uniq {0,2}=2 => (2-1)/2=0.5
        // idx1 uniq {0,1}=2 => 0.5
        // idx2 uniq {0}=1 => 0
        // avg=(0.5+0.5+0)/3=1/3
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 1.0 / 3.0, 1e-12);
    }

    // edge: empty
    {
        vector<DGene> pop;
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.ai; }, 0, 10, false);
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.0, 1e-12);
    }
}

static void test_diversity_array_float(Runner& tr) {
    ga::DiversityCalculator<DGene> dc;

    // scenario: per-index min/max ignores NaN
    {
        vector<DGene> pop(2);
        pop[0].af = {numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f};
        pop[1].af = {1.0f,  2.0f, numeric_limits<float>::quiet_NaN()};
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.af; }, 0.0f, 10.0f, false);
        // idx0: ok (1-1)/10=0
        // idx1: (2-0)/10=0.2
        // idx2: ok only 0 =>0
        // avg ~ 0.066666...
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.2 / 3.0, 1e-12);
    }

    // edge: all NaN => 0 (n==1)
    {
        vector<DGene> pop(3);
        for (auto& g : pop) g.af = {numeric_limits<float>::quiet_NaN(),
                                    numeric_limits<float>::quiet_NaN(),
                                    numeric_limits<float>::quiet_NaN()};
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.af; }, -1.0f, 1.0f, false);
        GL_EXPECT_EQ(tr, dc.n, 1);
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.0, 1e-12);
    }
}

static void test_diversity_array_enum(Runner& tr) {
    ga::DiversityCalculator<DGene> dc;

    // scenario: per-index unique count
    {
        vector<DGene> pop(3);
        pop[0].ae = {TestEnum::A, TestEnum::A, TestEnum::A};
        pop[1].ae = {TestEnum::B, TestEnum::A, TestEnum::A};
        pop[2].ae = {TestEnum::D, TestEnum::C, TestEnum::A};
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.ae; }, TestEnum::A, TestEnum::D, true);
        // idx0 uniq {A,B,D}=3 => (3-1)/3=2/3
        // idx1 uniq {A,C}=2 => (2-1)/3=1/3
        // idx2 uniq {A}=1 => 0
        // avg = (2/3 + 1/3 + 0)/3 = 1/3
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 1.0 / 3.0, 1e-12);
    }

    // edge: empty
    {
        vector<DGene> pop;
        dc.reset();
        dc.process(pop, [](const DGene& g) { return g.ae; }, TestEnum::A, TestEnum::D, true);
        GL_EXPECT_NEAR(tr, dc.diversity_rate(), 0.0, 1e-12);
    }
}

// ------------------------------------------------------------
// Tests: random helpers (rand01/rand_int/etc.)
// ------------------------------------------------------------
static void test_rand_helpers(Runner& tr) {
    // rand01 / rand01f ranges and determinism
    {
        RngGuard g(123);
        const double a0 = ga::rand01();
        const double a1 = ga::rand01();
        GL_EXPECT(tr, 0.0 <= a0 && a0 < 1.0);
        GL_EXPECT(tr, 0.0 <= a1 && a1 < 1.0);

        ga::rng.seed(123);
        const double b0 = ga::rand01();
        const double b1 = ga::rand01();
        GL_EXPECT_NEAR(tr, a0, b0, 0.0);
        GL_EXPECT_NEAR(tr, a1, b1, 0.0);
    }
    {
        RngGuard g(456);
        const float a0 = ga::rand01f();
        const float a1 = ga::rand01f();
        GL_EXPECT(tr, 0.0f <= a0 && a0 < 1.0f);
        GL_EXPECT(tr, 0.0f <= a1 && a1 < 1.0f);

        ga::rng.seed(456);
        const float b0 = ga::rand01f();
        const float b1 = ga::rand01f();
        GL_EXPECT_NEAR(tr, a0, b0, 0.0);
        GL_EXPECT_NEAR(tr, a1, b1, 0.0);
    }

    // rand_u32 is rng() truncated
    {
        RngGuard g(7);
        const uint32_t r0 = ga::rand_u32();
        ga::rng.seed(7);
        const uint32_t e0 = static_cast<uint32_t>(ga::rng());
        GL_EXPECT_EQ(tr, r0, e0);
    }

    // rand_int normal range
    {
        RngGuard g(9);
        for (int i = 0; i < 1000; ++i) {
            const int x = ga::rand_int(-3, 7);
            GL_EXPECT(tr, -3 <= x && x <= 7);
        }
    }

    // rand_int edge hi<=lo
    {
        RngGuard g(1);
        GL_EXPECT_EQ(tr, ga::rand_int(5, 5), 5);
        GL_EXPECT_EQ(tr, ga::rand_int(5, 4), 5);
    }

    // rand_int edge huge range triggers fallback (int32 extremes)
    {
        RngGuard g(2);
        const int x = ga::rand_int(numeric_limits<int>::min(), numeric_limits<int>::max());
        GL_EXPECT(tr, numeric_limits<int>::min() <= x && x <= numeric_limits<int>::max());
    }

    // rand_uniform
    {
        RngGuard g(3);
        for (int i = 0; i < 100; ++i) {
            const float x = ga::rand_uniform(-2.0f, 3.0f);
            GL_EXPECT(tr, -2.0f <= x && x <= 3.0f);
        }
    }

    // clamp_range normal and swapped bounds
    {
        GL_EXPECT_EQ(tr, ga::clamp_range(5, 0, 10), 5);
        GL_EXPECT_EQ(tr, ga::clamp_range(-1, 0, 10), 0);
        GL_EXPECT_EQ(tr, ga::clamp_range(99, 0, 10), 10);

        // swapped
        GL_EXPECT_EQ(tr, ga::clamp_range(5, 10, 0), 5);
        GL_EXPECT_EQ(tr, ga::clamp_range(-1, 10, 0), 0);
        GL_EXPECT_EQ(tr, ga::clamp_range(99, 10, 0), 10);
    }
    // normalize_range / clamp_range
    {
        int lo = 0, hi = 10;
        ga::normalize_range(lo, hi);
        GL_EXPECT_EQ(tr, lo, 0);
        GL_EXPECT_EQ(tr, hi, 10);

        lo = 10; hi = 0;
        ga::normalize_range(lo, hi);
        GL_EXPECT_EQ(tr, lo, 0);
        GL_EXPECT_EQ(tr, hi, 10);

        GL_EXPECT_EQ(tr, ga::clamp_range(5, 0, 10), 5);
        GL_EXPECT_EQ(tr, ga::clamp_range(-1, 0, 10), 0);
        GL_EXPECT_EQ(tr, ga::clamp_range(99, 0, 10), 10);

        // swapped bounds
        GL_EXPECT_EQ(tr, ga::clamp_range(5, 10, 0), 5);
        GL_EXPECT_EQ(tr, ga::clamp_range(-1, 10, 0), 0);
        GL_EXPECT_EQ(tr, ga::clamp_range(99, 10, 0), 10);
    }

    // rand_int<T> for small integer types
    {
        RngGuard g(20);
        for (int i = 0; i < 1000; ++i) {
            const int8_t x = ga::rand_int<int8_t>(-5, 5);
            GL_EXPECT(tr, -5 <= x && x <= 5);
        }
        // edge hi<=lo
        GL_EXPECT_EQ(tr, (int)ga::rand_int<int8_t>(7, 7), 7);
        GL_EXPECT_EQ(tr, (int)ga::rand_int<int8_t>(7, 6), 7);
    }

    // rand_int<enum>
    {
        enum class E : int { A = 0, B = 1, C = 2 };
        RngGuard g(21);
        for (int i = 0; i < 100; ++i) {
            const E x = ga::rand_int<E>(E::A, E::C);
            const int v = static_cast<int>(x);
            GL_EXPECT(tr, 0 <= v && v <= 2);
        }
        // edge hi<=lo
        GL_EXPECT(tr, ga::rand_int<E>(E::B, E::A) == E::B);
    }

    // rand_real
    {
        RngGuard g(30);
        for (int i = 0; i < 1000; ++i) {
            const double x = ga::rand_real(-1.5, 2.5);
            GL_EXPECT(tr, -1.5 <= x && x <= 2.5);
        }
        // edge hi<=lo
        GL_EXPECT_EQ(tr, ga::rand_real(1.0, 1.0), 1.0);
        GL_EXPECT_EQ(tr, ga::rand_real(2.0, 1.0), 2.0);
    }

    // rand_bool (bernoulli)
    {
        RngGuard g(40);
        for (int i = 0; i < 100; ++i) GL_EXPECT(tr, ga::rand_bool(0.0) == false);
        for (int i = 0; i < 100; ++i) GL_EXPECT(tr, ga::rand_bool(1.0) == true);

        // determinism check: rand_bool(p) == (rand01()<p)
        ga::rng.seed(41);
        const bool a0 = ga::rand_bool(0.25);
        ga::rng.seed(41);
        const bool b0 = (ga::rand01() < 0.25);
        GL_EXPECT_EQ(tr, a0, b0);
    }

    // rand_index
    {
        RngGuard g(50);
        for (int i = 0; i < 1000; ++i) {
            const int x = ga::rand_index(10);
            GL_EXPECT(tr, 0 <= x && x < 10);
        }
        // edge n<=1
        GL_EXPECT_EQ(tr, ga::rand_index(1), 0);
        GL_EXPECT_EQ(tr, ga::rand_index(0), 0);
    }

    // weighted_index
    {
        // edge empty
        {
            vector<double> w;
            RngGuard g(60);
            GL_EXPECT_EQ(tr, ga::weighted_index(w), -1);
        }

        // only one positive weight -> always that index
        {
            array<double, 4> w = {0.0, 0.0, 5.0, 0.0};
            RngGuard g(61);
            for (int i = 0; i < 100; ++i) {
                const int idx = ga::weighted_index(w);
                GL_EXPECT_EQ(tr, idx, 2);
            }
        }

        // negative treated as 0 -> only index 1 can be chosen
        {
            vector<float> w = {-1.0f, 2.0f, 0.0f};
            RngGuard g(62);
            for (int i = 0; i < 200; ++i) {
                const int idx = ga::weighted_index(w);
                GL_EXPECT_EQ(tr, idx, 1);
            }
        }

        // sum<=0 -> uniform (in range)
        {
            array<double, 3> w = {-1.0, 0.0, -2.0};
            RngGuard g(63);
            for (int i = 0; i < 200; ++i) {
                const int idx = ga::weighted_index(span<const double>(w.data(), w.size()));
                GL_EXPECT(tr, 0 <= idx && idx < 3);
            }
        }
    }

}

// ------------------------------------------------------------
// Benchmarks: DiversityCalculator accessor return types
// ------------------------------------------------------------
static void bench_header(const char* title) {
    cout << "\n=== " << title << " ===\n";
}

template<class F>
static void run_bench(const char* name, int iters, F&& fn) {
    volatile double sink = 0.0;
    const double t0 = ga::now_ms();
    for (int i = 0; i < iters; ++i) sink = sink + fn();
    const double t1 = ga::now_ms();
    cout << "[bench] " << name << " : " << (t1 - t0) << " ms (iters=" << iters << ", sink=" << (double)sink << ")\n";
}

static vector<DGene> make_bench_pop(int n) {
    RngGuard g(2024);
    vector<DGene> pop;
    pop.resize(n);

    for (int i = 0; i < n; ++i) {
        auto& x = pop[i];
        x.flag = (ga::rand_int(0, 1) != 0);
        x.iv = ga::rand_int(0, 100);
        x.fv = ga::rand_uniform(-1.0f, 1.0f);

        bitset<8> b{};
        ga::fill_random(b);
        x.bv = b;

        x.ev = static_cast<TestEnum>(ga::rand_int(0, 3));

        x.ai = {ga::rand_int(0, 100), ga::rand_int(0, 100), ga::rand_int(0, 100)};
        x.af = {ga::rand_uniform(-1.0f, 1.0f), ga::rand_uniform(-1.0f, 1.0f), ga::rand_uniform(-1.0f, 1.0f)};
        x.ae = {static_cast<TestEnum>(ga::rand_int(0, 3)),
                static_cast<TestEnum>(ga::rand_int(0, 3)),
                static_cast<TestEnum>(ga::rand_int(0, 3))};
    }
    return pop;
}

static void bench_diversity_accessor_types() {
    const auto pop = make_bench_pop(512);

    bench_header("DiversityCalculator benchmarks (per accessor return type)");

    // bool
    run_bench("bool", 20000, [&]() -> double {
        ga::DiversityCalculator<DGene> dc;
        dc.process(pop, [](const DGene& g) { return g.flag; });
        return dc.diversity_rate();
    });

    // int
    run_bench("int", 20000, [&]() -> double {
        ga::DiversityCalculator<DGene> dc;
        dc.process(pop, [](const DGene& g) { return g.iv; }, 0, 100, false);
        return dc.diversity_rate();
    });

    // float
    run_bench("float", 20000, [&]() -> double {
        ga::DiversityCalculator<DGene> dc;
        dc.process(pop, [](const DGene& g) { return g.fv; }, -1.0f, 1.0f, false);
        return dc.diversity_rate();
    });

    // bitset
    run_bench("bitset<8>", 20000, [&]() -> double {
        ga::DiversityCalculator<DGene> dc;
        dc.process(pop, [](const DGene& g) { return g.bv; });
        return dc.diversity_rate();
    });

    // enum
    run_bench("enum(int)", 20000, [&]() -> double {
        ga::DiversityCalculator<DGene> dc;
        dc.process(pop, [](const DGene& g) { return g.ev; }, TestEnum::A, TestEnum::D, true);
        return dc.diversity_rate();
    });

    // array<int>
    run_bench("array<int,3>", 20000, [&]() -> double {
        ga::DiversityCalculator<DGene> dc;
        dc.process(pop, [](const DGene& g) { return g.ai; }, 0, 100, false);
        return dc.diversity_rate();
    });

    // array<float>
    run_bench("array<float,3>", 20000, [&]() -> double {
        ga::DiversityCalculator<DGene> dc;
        dc.process(pop, [](const DGene& g) { return g.af; }, -1.0f, 1.0f, false);
        return dc.diversity_rate();
    });

    // array<enum>
    run_bench("array<enum(int),3>", 20000, [&]() -> double {
        ga::DiversityCalculator<DGene> dc;
        dc.process(pop, [](const DGene& g) { return g.ae; }, TestEnum::A, TestEnum::D, true);
        return dc.diversity_rate();
    });
}

// ------------------------------------------------------------
// Entry point: run all tests + benches
// ------------------------------------------------------------
static int run_all() {
    Runner tr;

    test_rng_determinism(tr);
    test_now_ms_monotonic(tr);
    test_stats_reset_and_dump_smoke(tr);

    test_population_basic_and_edges(tr);
    test_population_nan_and_tiebreaks(tr);
    test_population_tournament_selection(tr);

    scenario_population_elitism_replacement(tr);
    scenario_population_age_tiebreak(tr);
    scenario_population_nan_management(tr);
    scenario_population_mutation_update(tr);
    scenario_population_tournament_pressure(tr);

    test_fill_random_array(tr);
    test_fill_random_bitset(tr);
    test_fill_perm(tr);
    test_fill_random_perm(tr);
    test_apply_rolling_horizon_array(tr);
    test_apply_rolling_horizon_bitset(tr);

    test_diversity_bool(tr);
    test_diversity_int(tr);
    test_diversity_float(tr);
    test_diversity_bitset(tr);
    test_diversity_enum(tr);
    test_diversity_array_int(tr);
    test_diversity_array_float(tr);
    test_diversity_array_enum(tr);

    test_rand_helpers(tr);

    cout << "\n=== Test Summary ===\n";
    cout << "passed: " << tr.passed << "\n";
    cout << "failed: " << tr.failed << "\n";

    bench_diversity_accessor_types();

    return (tr.failed == 0) ? 0 : 1;
}

} // namespace ga_test

int main() { return ga_test::run_all(); }

#endif

