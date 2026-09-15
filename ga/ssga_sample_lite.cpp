// ssga_sample_lite_v8.cpp
// ------------------------------------------------------------
// Codingame を想定した「本気の SSGA (Steady-State GA)」サンプル。
//  - ga.hpp / ga_ops.hpp の代わりに ga_lite.hpp を使用
//  - Gene を tuple から struct にして可読性を上げた版
//  - ga_lite.hpp の Population / fill_random / fill_random_perm / apply_rolling_horizon / DiversityCalculator を活用
//
// 注意:
//  - ga_lite.hpp は rolling horizon / diversity / crossover / mutation を全部は提供しないため、
//    このサンプル側で「教育的で汎用的」な形の実装を追加している。
//  - 速度面では「シミュレーションが支配的」な Codingame を前提に、
//    余計なヒープ確保を避けつつ簡潔さと読みやすさを優先する。
// ------------------------------------------------------------

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

// ga_lite.hpp(v3) は末尾にテスト/ベンチ用の main() を含む。
// ライブラリとして include して使う場合はヘッダ側の #if 1 を #if 0 にして無効化する想定。
// ここではサンプル単体でコンパイルできるよう、ヘッダ内の main を一時的にリネームして取り込む。
#define main ga_lite_test_main
#include "ga_lite.hpp"
#undef main

// ga_lite.hpp(v3) では名前空間が ga に変更された。
// 以降、このサンプルでは ga:: をそのまま利用する。


// ------------------------------------------------------------
// グローバル設定
// ------------------------------------------------------------
bool debug_mode = true; // デバッグモードのグローバル定義

// ------------------------------------------------------------
// calibrate
//  - evaluate() が「重い」問題向けの簡易キャリブレーション。
//    （evaluate が軽い場合は GA 側のオーバーヘッドが支配的になり、測る意味が薄い）
//  - 指定した実行時間(ms)の範囲で、ランダム Gene を生成して evaluate を繰り返す
//  - evaluate の平均実行時間(ms)を返す
// ------------------------------------------------------------
template<class Gene, class Evaluator>
static double calibrate(Evaluator& ev, double budget_ms);

// ------------------------------------------------------------
// ユーザが定義する評価関数（Codingame なら State に相当）
// ------------------------------------------------------------
struct Evaluator {
    // --- ダミー状態（実戦なら入力で得た状態をここに保持） ---
    struct State {
        int frame = 0;   // ゲーム側のターン/フレーム
        int x = 0;
        int y = 0;
    } st;

    static constexpr int kRouteN = 8;

    // ターゲット（固定。実戦なら敵/資源/チェックポイントなど）
    std::array<int, kRouteN> base_tx{};
    std::array<int, kRouteN> base_ty{};

    Evaluator() {
        // 再現性のある固定初期化（本番なら入力由来）
        std::mt19937 local(1234);
        std::uniform_int_distribution<int> dist(-15, 15);
        for (int i = 0; i < kRouteN; ++i) {
            base_tx[(std::size_t)i] = dist(local);
            base_ty[(std::size_t)i] = dist(local);
        }
    }

    // 実戦なら「このターンに出力した行動」を反映して state を更新する
    void apply_real_action(int action) {
        // 0:UP 1:RIGHT 2:DOWN 3:LEFT 4:WAIT 5:DASH(右上)
        switch (action) {
            case 0: st.y -= 1; break;
            case 1: st.x += 1; break;
            case 2: st.y += 1; break;
            case 3: st.x -= 1; break;
            case 4: /* wait */ break;
            default: st.x += 1; st.y -= 1; break;
        }
        st.frame++;
    }

    // ターゲットがターンで少し動く（ダミー）
    inline std::pair<int, int> target_pos(int route_idx, int local_t) const {
        const int drift = ((st.frame + local_t) % 7) - 3; // -3..+3
        const int i = route_idx % kRouteN;
        return { base_tx[(std::size_t)i] + drift, base_ty[(std::size_t)i] - drift };
    }

    // Gene を受け取ってスコアを返す（最大化問題）
    template<class Gene>
    double evaluate(const Gene& g) const {
        const auto& seq   = g.actions;  // horizon action sequence
        const float param = g.aggression;
        const auto& flags = g.boost;    // bitset
        const auto& route = g.route;    // permutation (target order)

        int x = st.x;
        int y = st.y;

        // param を「機動力係数」とみなす（ダミー）
        const double mobility = 1.0 + 0.25 * (double)param; // だいたい 0.5..1.5

        // 割引平均で正規化（ホライズン長を変えても比較しやすい）
        constexpr double gamma = 0.97;
        double w = 1.0;
        double wsum = 0.0;
        double acc = 0.0;

        for (int t = 0; t < (int)seq.size(); ++t) {
            const int act = seq[(std::size_t)t];

            // flags: このステップでブーストするか（ダミー）
            const bool boost = flags.test((std::size_t)(t % (int)flags.size()));
            const int step = boost ? 2 : 1;

            // 移動
            const int s = (int)std::llround((double)step * mobility);
            switch (act) {
                case 0: y -= s; break;
                case 1: x += s; break;
                case 2: y += s; break;
                case 3: x -= s; break;
                case 4: /* wait */ break;
                default: x += s; y -= s; break;
            }

            // route に従ってターゲットを追う（ダミー）
            const int ridx = route[(std::size_t)(t % (int)route.size())];
            const auto [tx, ty] = target_pos(ridx, t);

            const int dx = x - tx;
            const int dy = y - ty;
            const int dist = std::abs(dx) + std::abs(dy);

            double reward = 100.0 - (double)dist; // 距離が近いほど加点
            if (boost) reward += 1.5;

            // 「画面外に飛ぶ」みたいな制約ペナルティ（ダミー）
            if (std::abs(x) > 40 || std::abs(y) > 40) reward -= 20.0;

            acc  += w * reward;
            wsum += w;
            w    *= gamma;
        }

        // 末尾の位置が悪いと少しペナルティ（これも割引に乗せる）
        const double terminal = -0.05 * (double)(std::abs(x) + std::abs(y));
        acc  += w * terminal;
        wsum += w;

        return acc / wsum;
    }
};

// ------------------------------------------------------------
// Gene 定義（問題ごとに差し替える箇所）
//  - tuple ではなく struct で、メンバーを明示
// ------------------------------------------------------------
static constexpr int kActions = 6;
static constexpr int kFlagsN  = 32;
static constexpr int kRouteN  = Evaluator::kRouteN;

static constexpr int   kActionLo  = 0;
static constexpr int   kActionHi  = kActions - 1;
static constexpr int   kActionDef = 4; // WAIT

static constexpr float kAggLo  = -2.0f;
static constexpr float kAggHi  =  2.0f;
static constexpr float kAggDef =  0.0f;

template<int H>
struct GeneH {
    static_assert(H >= 0);
    std::array<int, H> actions{};            // rolling horizon 対象
    float aggression = kAggDef;              // scalar
    std::bitset<kFlagsN> boost{};            // bitset
    std::array<int, kRouteN> route{};        // permutation
};

// ------------------------------------------------------------
// Gene 初期化
//  - ga_lite.hpp の fill_random / fill_random_perm を活用
// ------------------------------------------------------------
template<int H>
static inline void init_default_gene(GeneH<H>& g) {
    g.actions.fill(kActionDef);
    g.aggression = kAggDef;
    g.boost.reset();

    // 0..N-1 の順列（デフォルト）
    ga::fill_perm(g.route);
}

template<int H>
static inline void init_random_gene(GeneH<H>& g) {
    ga::fill_random(g.actions, kActionLo, kActionHi);
    g.aggression = ga::rand_real<float>(kAggLo, kAggHi);
    ga::fill_random(g.boost);

    ga::fill_random_perm(g.route);
}

// ------------------------------------------------------------
// Gene の rolling horizon
//  - ga::apply_rolling_horizon は「shift by 1」を提供するので、shift 回繰り返す
// ------------------------------------------------------------
template<int H>
static inline void apply_rolling_horizon(GeneH<H>& g, int shift, bool random_fill) {
    if (shift <= 0) return;

    for (int s = 0; s < shift; ++s) {
        const int fill_act = random_fill ? ga::rand_int(kActionLo, kActionHi) : kActionDef;
        ga::apply_rolling_horizon(g.actions, fill_act);

        // boost も同様にずらす（教育的デモとして bitset 版の helper も活用）
        const bool fill_boost = random_fill ? ga::rand_bool(0.5) : false;
        ga::apply_rolling_horizon(g.boost, fill_boost);
    }
}

// ------------------------------------------------------------
// Crossover / Mutation（scalar / array / bitset 単位の汎用関数）
// ------------------------------------------------------------

// ---- array: uniform crossover ----
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

// ---- array: two-point crossover ----
template<class T, std::size_t N>
static inline void crossover_two_point_array(const std::array<T, N>& a, const std::array<T, N>& b, std::array<T, N>& out) {
    if constexpr (N == 0) {
        return;
    } else if constexpr (N == 1) {
        out[0] = (ga::rand_u32() & 1u) ? a[0] : b[0];
        return;
    } else {
        const int l = ga::rand_int(0, (int)N - 1);
        const int r = ga::rand_int(l + 1, (int)N); // [l+1, N]
        for (int i = 0; i < (int)N; ++i) {
            out[(std::size_t)i] = (l <= i && i < r) ? b[(std::size_t)i] : a[(std::size_t)i];
        }
    }
}

// ---- scalar: arithmetic crossover ----
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

// ---- bitset: uniform crossover ----
template<std::size_t N>
static inline void crossover_uniform_bitset(const std::bitset<N>& a, const std::bitset<N>& b, std::bitset<N>& out) {
    if constexpr (N == 0) return;

    std::bitset<N> mask;
    ga::fill_random(mask);
    out = (a & mask) | (b & ~mask);
}

// ---- permutation(OX) crossover ----
template<std::size_t N>
static inline void crossover_ox_perm(const std::array<int, N>& a, const std::array<int, N>& b, std::array<int, N>& out) {
    if constexpr (N == 0) return;

    const int l = ga::rand_int(0, (int)N - 1);
    const int r = ga::rand_int(l + 1, (int)N); // [l+1, N]

    out = a; // copy then fix outside segment

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

// ---- array: creep mutation (integral) ----
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

// ---- array: random reset mutation ----
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

// ---- scalar: gaussian mutation ----
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

// ---- bitset: bit-flip mutation ----
template<std::size_t N>
static inline void mutate_bitflip_bitset(std::bitset<N>& b, double p_flip) {
    if constexpr (N == 0) return;
    if (!(p_flip > 0.0)) return;

    for (std::size_t i = 0; i < N; ++i) {
        if (ga::rand_bool(p_flip)) b.flip(i);
    }
}

// ---- permutation: swap mutation ----
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

// ---- permutation: inversion mutation ----
template<std::size_t N>
static inline void mutate_inversion_perm(std::array<int, N>& a, double p_inv) {
    if constexpr (N < 2) return;
    if (!(p_inv > 0.0)) return;
    if (!ga::rand_bool(p_inv)) return;

    const int l = ga::rand_int(0, (int)N - 1);
    const int r = ga::rand_int(l + 1, (int)N);
    std::reverse(a.begin() + l, a.begin() + r);
}

// ------------------------------------------------------------
// Gene 単位の crossover / mutation（上の汎用関数を合成）
// ------------------------------------------------------------
template<int H>
struct GeneOps {
    // weighted_index を使うため、weights は配列として保持する。
    std::array<double, 2> seq_crossover_w{0.7, 0.3}; // 0: two-point, 1: uniform
    std::array<double, 2> seq_mut_w{0.75, 0.25};      // 0: creep, 1: random-reset
    std::array<double, 2> perm_mut_w{0.6, 0.4};       // 0: swap, 1: inversion

    double seq_creep_p = 0.12;
    int    seq_creep_step_max = 2;
    double seq_reset_p = 0.03;

    double agg_gauss_p = 0.35;
    double agg_gauss_sigma = 0.35;

    double boost_flip_p = 0.02;

    double perm_swap_p = 0.25;
    double perm_inv_p  = 0.15;

    void crossover(const GeneH<H>& p1, const GeneH<H>& p2, GeneH<H>& c) const {
        // actions
        if (ga::weighted_index(seq_crossover_w) == 0) {
            crossover_two_point_array(p1.actions, p2.actions, c.actions);
        } else {
            crossover_uniform_array(p1.actions, p2.actions, c.actions);
        }

        // aggression
        c.aggression = crossover_arithmetic_scalar<float>(p1.aggression, p2.aggression, kAggLo, kAggHi);

        // boost
        crossover_uniform_bitset(p1.boost, p2.boost, c.boost);

        // route
        crossover_ox_perm(p1.route, p2.route, c.route);
    }

    void mutate(GeneH<H>& g) const {
        // actions
        if (ga::weighted_index(seq_mut_w) == 0) {
            mutate_creep_array(g.actions, seq_creep_p, seq_creep_step_max, kActionLo, kActionHi);
        } else {
            mutate_random_reset_array(g.actions, seq_reset_p, kActionLo, kActionHi);
        }

        // aggression
        mutate_gaussian_scalar(g.aggression, agg_gauss_p, agg_gauss_sigma, kAggLo, kAggHi);

        // boost
        mutate_bitflip_bitset(g.boost, boost_flip_p);

        // route
        if (ga::weighted_index(perm_mut_w) == 0) {
            mutate_swap_perm(g.route, perm_swap_p);
        } else {
            mutate_inversion_perm(g.route, perm_inv_p);
        }
    }
};

// ------------------------------------------------------------
// 多様性（ga_lite.hpp の DiversityCalculator を利用）
//
// ga::DiversityCalculator は「フル計算（pop を毎回走査）」で、差分更新はしない。
// そのため SSGA の hot loop で毎 iter 呼ぶのではなく、
//   - pop_size 反復（= だいたい 1 世代ぶん個体が置換）につき 1 回程度
// の頻度で計測し、キャッシュした値を immigration / shake の判定に使う想定。
//
// ※順列(route)も「各位置の値」を enum とみなして is_enum=true で扱える。
// ------------------------------------------------------------
template<int H>
static inline double diversity_rate_manual(const ga::Population<GeneH<H>>& pop) {
    using Gene = GeneH<H>;
    const auto& genes = pop.gene();
    if (genes.empty()) return 0.0;

    ga::DiversityCalculator<Gene> dc;
    dc.reset();

    // actions: enum(0..kActions-1)
    if constexpr (H > 0) {
        dc.process(genes, [](const Gene& g) { return g.actions; }, kActionLo, kActionHi, /*is_enum=*/true);
    }

    // aggression: (max-min)/(hi-lo)
    dc.process(genes, [](const Gene& g) { return g.aggression; }, kAggLo, kAggHi, /*is_enum=*/false);

    // boost bitset: per-bit diversity
    dc.process(genes, [](const Gene& g) { return g.boost; });

    // route permutation: enum(0..kRouteN-1)
    if constexpr (kRouteN > 0) {
        dc.process(genes, [](const Gene& g) { return g.route; }, 0, kRouteN - 1, /*is_enum=*/true);
    }

    return dc.diversity_rate();
}

// ------------------------------------------------------------
// calibrate 実装
// ------------------------------------------------------------
template<class Gene, class Evaluator>
static double calibrate(Evaluator& ev, double budget_ms) {
    // calibrate は「evaluate が重い」前提で使う。
    // そのため、細かなオーバーヘッド削減のための batch 化は行わず、素直な実装にする。
    constexpr int kMaxTrials = 1'000'000;

    if (!(budget_ms > 0.0)) budget_ms = 1.0;

    // 最適化で評価が消えないようにする簡易シンク
    volatile double sink = 0.0;

    int trials = 0;
    double eval_total_ms = 0.0;

    const double start_ms = ga::now_ms();
    const double end_ms = start_ms + budget_ms;

    // budget が極端に小さくても最低 1 回は評価するため、
    // break 判定はループ末尾（t1）で行う。
    while (trials < kMaxTrials) {
        Gene g{};
        init_random_gene(g); // 計測外（本番で使うなら、ここも含めても誤差は小さい想定）

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

// ------------------------------------------------------------
// SSGA Runner
// ------------------------------------------------------------
template<class Gene, class EvaluatorT>
struct SSGARunner;

template<int H, class EvaluatorT>
struct SSGARunner<GeneH<H>, EvaluatorT> {
    using Gene = GeneH<H>;

    EvaluatorT& evaluator;
    int pop_size = 0;

    int run_no = 0; // run() の呼び出し回数
    int iters  = 0; // 反復回数（iter 単位で統一）

    ga::Population<Gene> pop;

    // 遺伝的オペレータ
    GeneOps<H> ops{};

    // selection
    int parent_tournament_k = 3;
    int victim_tournament_k = 3;

    // --- GA 戦略パラメータ（問題に応じて調整） ---
    double p_crossover = 0.90;

    // 移民
    int immigration_interval = 0;      // pop_size に依存して決める
    double immigration_chance = 0.35;  // interval 到達時に注入する確率

    // 停滞・攪拌
    int stagnation_window = 0;         // pop_size に依存して決める
    float diversity_floor = 0.20f;
    int shake_replace = 0;             // 1 回の攪拌で置換する個体数（pop_size に依存）

    // 多様性チェック（重いので毎 iter は避ける）
    int diversity_check_interval = 0;

    // ログ
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

        // pop_size に依存して「世代感」を合わせる（steady-state は 1 iter = 1 child）
        immigration_interval = std::max(16, pop_size * 2); // 2 世代に 1 回チェック
        stagnation_window   = std::max(64, pop_size * 6);  // 6 世代改善なしで停滞扱い
        shake_replace       = std::max(2, pop_size / 10);  // 10% だけ入れ替え（小さすぎないよう 2）

        // 多様性計測はフル計算なので、概ね 1 世代(pop_size 置換)に 1 回だけ計測する。
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

    // --------------------------------------------------------
    // run: SSGA 実行（ga::now_ms() が end_time_ms に達したら終了）
    // --------------------------------------------------------
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

        // 最後に 1 行だけサマリ（直前にログ済みなら控える）
        if (debug_mode) {
            const double now = ga::now_ms();
            const bool never_logged = (stats.last_log_ms == stats.run_start_ms);
            if (never_logged || (now - stats.last_log_ms) > log_interval_ms * 0.5) {
                log_stats();
            }
        }
    }

    // --------------------------------------------------------
    // init_population
    //  - 初回: 初期集団生成 + 全評価
    //  - 2回目以降: rolling horizon + 再評価
    // --------------------------------------------------------
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

        // rolling horizon: shift 1、空いた tail は random_fill
        const int n = pop.size();
        for (int i = 0; i < n; ++i) {
            pop.update(i, [&](Gene& g) {
                apply_rolling_horizon(g, 1, /*random_fill=*/true);
            });
        }

        // 状態が変わった前提で全個体を再評価
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

    // --------------------------------------------------------
    // diversity_rate (cached)
    //
    // DiversityCalculator はフル計算なので、ここで iter ベースに間引いてキャッシュする。
    // 目安: pop_size iter（= だいたい 1 世代ぶん置換）に 1 回。
    // --------------------------------------------------------
    double diversity_rate_cached(bool force = false) {
        if (force || stats.div_cache_iter < 0 || (iters - stats.div_cache_iter) >= diversity_check_interval) {
            stats.div_cache = diversity_rate_manual(pop);
            stats.div_cache_iter = iters;
        }
        return stats.div_cache;
    }

    // --------------------------------------------------------
    // maybe_make_child
    //  - selection / crossover / mutation / evaluation / replacement
    // --------------------------------------------------------
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

        // 交叉するか
        if (ga::rand_bool(p_crossover)) {
            ops.crossover(pop.gene(p1), pop.gene(p2), child);
            ++op_stats.apply_crossover;
        } else {
            const int better = pop.better(p1, p2) ? p1 : p2;
            child = pop.gene(better);
        }

        // 突然変異
        ops.mutate(child);
        ++op_stats.apply_mutation;

        // 評価
        const double f = evaluator.template evaluate<Gene>(child);
        ++stats.evals;
        ++stats.children;

        // 新生個体の born_at を進めるため tick を進めてから set
        pop.advance_tick(1);

        // 置換先（逆トーナメントで悪い個体を狙う）
        int victim = pop.select_tournament_worst(victim_tournament_k);
        if (victim < 0) victim = 0;

        // ベストはなるべく保持
        const int best_now = pop.get_best_index();
        if (victim == best_now && n >= 2) {
            for (int retry = 0; retry < 3; ++retry) {
                const int v2 = pop.select_tournament_worst(victim_tournament_k);
                if (v2 >= 0 && v2 != best_now) { victim = v2; break; }
            }
            if (victim == best_now) victim = (best_now + 1) % n;
        }

        pop.set(victim, std::move(child), f);

        // diversity cache はフル計算なので、更新は diversity_rate_cached() 側で間引く。
        // （ここでは毎 iter 呼んでも、interval に達したときだけ再計算される）
        diversity_rate_cached(/*force=*/false);

        if (f > stats.best_fitness) {
            stats.best_fitness = f;
            stats.best_iter = iters;
            stats.no_improve_iters = 0;
        } else {
            ++stats.no_improve_iters;
        }
    }

    // --------------------------------------------------------
    // maybe_immigrate
    //  - 定期的に 1 個体だけランダム注入して多様性を確保
    // --------------------------------------------------------
    void maybe_immigrate() {
        if (immigration_interval <= 0) return;
        if (iters % immigration_interval != 0) return;

        // 多様性が十分あるなら注入確率を下げる
        const double div = diversity_rate_cached(/*force=*/false);
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

    // --------------------------------------------------------
    // maybe_shake
    //  - 停滞 + 低多様性 を検知して攪拌
    // --------------------------------------------------------
    void maybe_shake() {
        const bool stagnated = (stats.no_improve_iters >= stagnation_window);

        // 低多様性チェックはフル計算なので、hot loop ではキャッシュ値を利用する。
        // キャッシュ更新は maybe_make_child() 側で間引きながら行う。
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

            // 旧実装同様: 揺すりは iter 内の攪拌扱いなので tick は進めない
            pop.set(victim, std::move(g), f);

            if (f > stats.best_fitness) {
                stats.best_fitness = f;
                stats.best_iter = iters;
                stats.no_improve_iters = 0;
            }
        }

        ++stats.shakes;

        // 連続で揺すり続けないようにカウンタを少し戻す
        if (stagnation_window > 0) {
            stats.no_improve_iters = std::min(stats.no_improve_iters, stagnation_window / 2);
        }

        // 揺すったので diversity は取り直しておく
        diversity_rate_cached(/*force=*/true);
    }

    // --------------------------------------------------------
    // logging
    // --------------------------------------------------------
    void maybe_log() {
        if (!debug_mode) return;
        const double now = ga::now_ms();
        if (now - stats.last_log_ms < log_interval_ms) return;
        stats.last_log_ms = now;
        log_stats();
    }

    void log_stats() {
        const auto ps = pop.get_stats();
        const double div = diversity_rate_cached(/*force=*/false);

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

// ------------------------------------------------------------
// choose_best_gene
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// choose_best_population_size
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// choose_best_trials
// ------------------------------------------------------------
template<class Gene, class EvaluatorT, class PrepareFn>
static int choose_best_trials(EvaluatorT& ev, int pop_size, double run_ms, int trials, PrepareFn prepare) {
    if (trials <= 0) trials = 1;

    const std::mt19937 base_rng = ga::rng;

    int best_trial = 0;
    double best_fit = -ga::kInf;

    for (int t = 0; t < trials; ++t) {
        ga::rng = base_rng;

        SSGARunner<Gene, EvaluatorT> runner(ev, pop_size);

        // 試行ごとの初期化（設定変更/seed変更/状態リセットなど）
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

// コールバック不要版（何もしない）
template<class Gene, class EvaluatorT>
static int choose_best_trials(EvaluatorT& ev, int pop_size, double run_ms, int trials) {
    return choose_best_trials<Gene>(
        ev, pop_size, run_ms, trials,
        [](int /*trial_index*/, SSGARunner<Gene, EvaluatorT>& /*runner*/, EvaluatorT& /*ev*/) {}
    );
}

// ------------------------------------------------------------
// demos
// ------------------------------------------------------------
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

    // 例: ホライズン長 8 / 16 / 24 を比較
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

    // 例: 3 回回して「一番良い run」が何番目かを返す。
    // prepare 内で、trial ごとに evaluator の初期状態や GA パラメータを少し変える想定。
    const int best_trial = choose_best_trials<Gene>(
        ev,
        64,
        15.0,   // 1 回の run 予算
        3,      // trials
        [](int t, SSGARunner<Gene, Evaluator>& runner, Evaluator& e) {
            // 例: trial ごとに RNG を少し揺らす（同じ seed を戻しているので、ここで差を作れる）
            ga::rng.seed(42u + (unsigned)t * 100u);

            // 例: evaluator の状態を trial ごとに変える（本番なら入力に相当）
            e.st.frame = 0;
            e.st.x = 0;
            e.st.y = 0;
            if (t == 1) { e.st.x = 3; e.st.y = -2; }
            if (t == 2) { e.st.x = -4; e.st.y = 1; }

            // 例: GA パラメータを少し変える（本番なら局面に応じて調整）
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

    // 1つの runner を使い回して「毎ターンGA」を想定
    SSGARunner<Gene, Evaluator> runner(ev, 64);

    for (int frame = 0; frame < 5; ++frame) {
        const double budget_ms = 20.0;

        runner.run(ga::now_ms() + budget_ms);

        const Gene& best = runner.best_gene();
        const int action0 = best.actions[0];
        const double bf = runner.best_fitness();

        std::fprintf(stderr, "[RH] frame=%d best_fitness=%.3f action0=%d\n", frame, bf, action0);

        // 実戦ならここで action0 を stdout に出力し、次の入力を受ける。
        // デモでは Evaluator の状態だけ進める。
        ev.apply_real_action(action0);
    }
}

// ------------------------------------------------------------
// main (デモ)
//  - 無効化しやすいよう #if 1 ～ #endif
// ------------------------------------------------------------
#if 1
int main() {
    debug_mode = true;

    // 乱数シードは固定（再現性重視）。本番で揺らすなら入力 frame 等で seed。
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
