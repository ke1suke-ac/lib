// ------------------------------------------------------------
// CodinGame 利用を想定した SSGA (Steady-State Genetic Algorithm) のサンプル
//  - C++20 (gcc 12.2), single-thread
//  - 標準ライブラリのみ（ただし gcc 依存として <bits/stdc++.h> を使用）
//  - ga.hpp / ga_ops.hpp を「できるだけそのまま」活用するデモ
//
// このファイルで意識していること（ログから分かった改善点）:
// - ループ内で ga::now_ms() を毎回呼ぶと重いので、一定間隔でのみ時間確認する
// - 「near end ログ」を毎反復で出すと 5ms 程度の間に大量ログが出るため、近終端ログはやめる
// - immigration / shake / local search などを毎反復で判定すると回数が膨大になるため、間引く＆クールダウン
// - Replacement を「改善した時だけ置換」にすると集団が急速に同一化しやすいので、探索性を少し足す
//
// 注意（重要）:
// 1) ga.hpp は末尾に #if 1 のテスト main() を持つため、通常の .cpp で使う場合は
//    include 時に main を別名へ置き換えて衝突回避します（define/undef）。
//
// 2) ga_ops.hpp も末尾に #if 1 のテスト main() を持つため、
//    そのまま include すると main 衝突します。
//    （ga_ops.hpp 内部で main マクロを undef するため、外側 define/undef だけでは回避できません）
//    このサンプルは「GA 本体の組み立て方のデモ」を主目的にし、オペレータは最小限をこの .cpp 側に用意します。
// ------------------------------------------------------------

#include <bits/stdc++.h>

// ---------- ga ヘッダの main 衝突回避 ----------
#define main ga__header_main
#include "ga.hpp"
#undef main

// ---------- ga_ops を使うか（0=使わない/この .cpp の最小版を使う, 1=ga_ops の実装を使う） ----------
#ifndef SSGA_USE_GA_OPS_HEADER
#define SSGA_USE_GA_OPS_HEADER 0
#endif

#if SSGA_USE_GA_OPS_HEADER
// ※ ga_ops.hpp 側のテスト main() を無効化している前提（ヘッダ側を #if 0 などへ変更）
// ※ ga_ops.hpp 側の include が ga.hpp を参照するよう更新されている前提
#include "ga_ops.hpp"
#endif

// ============================================================
// グローバル設定
// ============================================================

bool debug_mode = true; // デバッグモード（stderr ログを出す）

// ============================================================
// ユーザーが定義する評価関数（CodinGame では State に近い）
// ============================================================
//
// - 実戦では、ここに「ゲーム状態」「乱数」「シミュレータ」「キャッシュ」などを持たせる想定。
// - このサンプルでは「仮の問題」を置いて、Gene を評価して fitness(double) を返す。
//   （値が大きいほど良い）
//
// ============================================================

struct Evaluater {
    // 仮の「ゲーム状態」
    std::array<double, 2> start_pos{0.0, 0.0};
    std::array<double, 2> target_pos{10.0, 7.0};

    void advance_state(int turn) {
        // 実際の CodinGame なら入力から state を更新する
        target_pos[0] = 10.0 + 0.1 * turn;
        target_pos[1] =  7.0 + 0.05 * turn;
    }

    template<class Gene>
    double evaluate(const Gene& gene) {
        // 仮定:
        // - 遺伝子0: 未来 H 手分の離散アクション列（0..6）
        //   6 は「停止（stay）」にして、ホライズンが長くても無駄に悪化しないようにする
        // - 遺伝子1: 連続パラメータ（-1..1）
        //
        // 目的:
        // - 最終位置が target に近いほど高得点
        // - 消費エネルギー（移動量）や急旋回が少ないほど高得点

        const auto& act = std::get<0>(gene).v;
        const double param = std::get<1>(gene).v;

        double x = start_pos[0];
        double y = start_pos[1];

        double energy = 0.0;
        int prev_a = -1;

        // param によって移動の大きさを微調整
        const double step = 1.0 + 0.25 * param;

        for (std::size_t i = 0; i < act.size(); ++i) {
            const int a = act[i];

            double dx = 0.0, dy = 0.0;
            switch (a) {
                case 0: dx = +1; dy =  0; break;
                case 1: dx = -1; dy =  0; break;
                case 2: dx =  0; dy = +1; break;
                case 3: dx =  0; dy = -1; break;
                case 4: dx = +1; dy = +1; break;
                case 5: dx = -1; dy = +1; break;
                case 6: dx =  0; dy =  0; break; // stay
                default: dx = 0; dy = 0; break;
            }

            x += dx * step;
            y += dy * step;
            energy += std::abs(dx) + std::abs(dy);

            // 急旋回ペナルティ（前の行動と違い過ぎると減点）
            if (prev_a >= 0) {
                const int diff = std::abs(a - prev_a);
                energy += 0.05 * diff;
            }
            prev_a = a;

            // 「障害物っぽい」ペナルティ（適当）
            if (x > 6.0 && x < 7.0 && y > 3.0 && y < 5.0) {
                energy += 1.0;
            }
        }

        const double ddx = x - target_pos[0];
        const double ddy = y - target_pos[1];
        const double dist = std::sqrt(ddx * ddx + ddy * ddy);

        // cost = dist + energy*0.1 のような素朴な形
        const double cost = dist + 0.1 * energy;
        return -cost; // 大きいほど良い
    }
};

// ============================================================
// calibrate: evaluate の平均実行時間（ms）を推定
// ============================================================
//
// ログの「evaluate avg = 0.000 ms」問題への対応:
// - 表示桁を増やす
// - 評価が軽すぎる場合は繰り返し回数を増やして測定時間を稼ぐ
// - ランダム Gene 生成は事前に行い、評価ループの時間だけ測る
//
template<class Gene, class EvaluaterT>
static double calibrate(EvaluaterT& ev, int iters = 10) {
    iters = std::max(iters, 1);

    std::vector<Gene> genes(static_cast<std::size_t>(iters));
    for (auto& g : genes) {
        g = Gene{};
        ga::init_gene_random(g);
    }

    volatile double sink = 0.0;

    int repeats = 1;
    double dt = 0.0;

    // 目標測定時間（軽い evaluate でも 5ms 程度は測る）
    constexpr double target_ms = 5.0;

    while (true) {
        const double t0 = ga::now_ms();
        for (int r = 0; r < repeats; ++r) {
            for (const auto& g : genes) sink = sink + ev.evaluate(g);
        }
        const double t1 = ga::now_ms();
        dt = t1 - t0;

        if (dt >= target_ms) break;
        if (repeats >= (1 << 20)) break; // 念のため暴走防止

        repeats *= 2;
    }

    (void)sink;
    const double denom = static_cast<double>(iters) * static_cast<double>(repeats);
    return dt / denom;
}

// ============================================================
// 最小限の GA オペレータ（ga_ops の方針に合わせた薄い版）
// ============================================================

#if !SSGA_USE_GA_OPS_HEADER

namespace ssga_ops {

// -------------------- Selection --------------------

template<class Gene>
struct TournamentSelection {
    int k = 3;

    void prepare(const ga::Population<Gene>&) const {}

    int apply(const ga::Population<Gene>& pop) const {
        const int n = pop.size();
        if (n <= 0) return -1;

        std::uniform_int_distribution<int> dist(0, n - 1);
        int best = dist(ga::rng);

        for (int i = 1; i < k; ++i) {
            const int cand = dist(ga::rng);
            if (pop.better(cand, best)) best = cand;
        }
        return best;
    }
};

// 「置換対象」を選ぶための負トーナメント（k 人から worst を選ぶ）
template<class Gene>
struct LoserTournamentSelection {
    int k = 3;

    int apply(const ga::Population<Gene>& pop) const {
        const int n = pop.size();
        if (n <= 0) return -1;

        std::uniform_int_distribution<int> dist(0, n - 1);
        int worst = dist(ga::rng);

        for (int i = 1; i < k; ++i) {
            const int cand = dist(ga::rng);
            // pop.better(a,b) が true なら a が b より良い
            // worst が cand より良いなら、cand の方が悪いので worst を更新
            if (pop.better(worst, cand)) worst = cand;
        }
        return worst;
    }
};

template<class Gene>
struct WorstSelection {
    void prepare(const ga::Population<Gene>&) const {}
    int apply(const ga::Population<Gene>& pop) const { return pop.get_worst_index(); }
};

// -------------------- Crossovers --------------------

// 2点交叉（array / bitset / scalar 対応の最小版）
template<ga::SpecLike Spec>
struct TwoPointCrossover {
    using S = Spec;
    using V = typename Spec::val_t;
    static constexpr std::size_t L = ga::static_len_v<V>;

    void apply(const Spec& p1, const Spec& p2, Spec& c) const {
        c = p1;

        if constexpr (ga::is_arr_v<V>) {
            if (L <= 1) return;
            std::uniform_int_distribution<std::size_t> dist(0, L - 1);
            std::size_t a = dist(ga::rng);
            std::size_t b = dist(ga::rng);
            if (a > b) std::swap(a, b);
            for (std::size_t i = a; i <= b; ++i) c.v[i] = p2.v[i];

        } else if constexpr (ga::is_bs_v<V>) {
            if (L <= 1) return;
            std::uniform_int_distribution<std::size_t> dist(0, L - 1);
            std::size_t a = dist(ga::rng);
            std::size_t b = dist(ga::rng);
            if (a > b) std::swap(a, b);
            for (std::size_t i = a; i <= b; ++i) c.v.set(i, p2.v.test(i));

        } else {
            // scalar は片親を選ぶだけ
            std::bernoulli_distribution pick(0.5);
            c.v = pick(ga::rng) ? p1.v : p2.v;
        }
    }
};

// 一様交叉（scalar/array/bitset）
template<ga::SpecLike Spec>
struct UniformCrossover {
    using S = Spec;
    using V = typename Spec::val_t;
    static constexpr std::size_t L = ga::static_len_v<V>;

    double p_pick = 0.5;

    void apply(const Spec& p1, const Spec& p2, Spec& c) const {
        if constexpr (ga::is_arr_v<V>) {
            std::bernoulli_distribution pick(p_pick);
            for (std::size_t i = 0; i < L; ++i) c.v[i] = pick(ga::rng) ? p1.v[i] : p2.v[i];

        } else if constexpr (ga::is_bs_v<V>) {
            std::bernoulli_distribution pick(p_pick);
            for (std::size_t i = 0; i < L; ++i) c.v.set(i, pick(ga::rng) ? p1.v.test(i) : p2.v.test(i));

        } else {
            std::bernoulli_distribution pick(p_pick);
            c.v = pick(ga::rng) ? p1.v : p2.v;
        }
    }
};

// BLX-α（数値向け）
template<ga::SpecLike Spec>
struct BlendCrossover {
    using S = Spec;
    using V = typename Spec::val_t;
    using E = ga::elem_of_t<V>;

    static_assert(!ga::is_bs_v<V>, "BlendCrossover does not support bitset");
    static_assert(std::is_arithmetic_v<E>, "BlendCrossover requires arithmetic type");

    double alpha = 0.5;

    void apply(const Spec& p1, const Spec& p2, Spec& c) const {
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

        std::uniform_real_distribution<double> u01(0.0, 1.0);

        auto mix_one = [&](E a, E b) -> E {
            double lo = (double)std::min(a, b);
            double hi = (double)std::max(a, b);
            const double d = hi - lo;
            const double l = lo - alpha * d;
            const double r = hi + alpha * d;
            const double t = l + (r - l) * u01(ga::rng);
            return clamp(static_cast<E>(t));
        };

        if constexpr (ga::is_arr_v<V>) {
            for (std::size_t i = 0; i < ga::static_len_v<V>; ++i) c.v[i] = mix_one(p1.v[i], p2.v[i]);
        } else {
            c.v = mix_one(p1.v, p2.v);
        }
    }
};

// 複数の交叉から重み付きに1つ選ぶ（ga_ops の ChoiceCrossover と同目的）
template<ga::SpecLike Spec, class... Crossovers>
struct ChoiceCrossover {
    using S = Spec;
    static constexpr std::size_t N = sizeof...(Crossovers);
    static_assert(N > 0);

    std::tuple<Crossovers...> ops{};
    std::array<double, N> weights{};

    ChoiceCrossover() { weights.fill(1.0); }

    ChoiceCrossover(std::array<double, N> ws, Crossovers... cs)
        : ops(std::move(cs)...), weights(ws) {}

    void apply(const Spec& p1, const Spec& p2, Spec& c) const {
        const std::size_t idx = pick_();
        visit_(idx, [&](const auto& op) { op.apply(p1, p2, c); });
    }

private:
    std::size_t pick_() const {
        double sum = 0.0;
        for (double x : weights) if (x > 0.0) sum += x;
        if (!(sum > 0.0)) {
            std::uniform_int_distribution<std::size_t> uni(0, N - 1);
            return uni(ga::rng);
        }
        std::uniform_real_distribution<double> dist(0.0, sum);
        double r = dist(ga::rng);
        for (std::size_t i = 0; i < N; ++i) {
            const double wi = weights[i] > 0.0 ? weights[i] : 0.0;
            if (r < wi) return i;
            r -= wi;
        }
        return N - 1;
    }

    template<class F, std::size_t... I>
    void visit_impl_(std::size_t idx, F&& f, std::index_sequence<I...>) const {
        ((idx == I ? (void)f(std::get<I>(ops)) : (void)0), ...);
    }

    template<class F>
    void visit_(std::size_t idx, F&& f) const {
        visit_impl_(idx, std::forward<F>(f), std::make_index_sequence<N>{});
    }
};

// -------------------- Mutations (in-place) --------------------

// ランダムリセット（要素ごとに一定確率で random_elem_value）
template<ga::SpecLike Spec>
struct RandomResetMutation {
    using S = Spec;
    double p_reset = 0.01;

    void apply(Spec& s) const {
        std::bernoulli_distribution do_reset(p_reset);
        ga::visit_elems_ref(s, [&](auto&& elem, std::size_t /*i*/) {
            if (do_reset(ga::rng)) elem = ga::random_elem_value<Spec>();
        });
    }
};

// 整数の微小変化（±step_max）
// ※ 離散アクションでも「近い番号へ少し動かす」用途には使える（完全な置換より局所探索寄り）
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

// ガウスノイズ（浮動小数向け）
template<ga::SpecLike Spec>
struct GaussianMutation {
    using S = Spec;
    using V = typename Spec::val_t;
    using E = ga::elem_of_t<V>;

    static_assert(!ga::is_bs_v<V>, "GaussianMutation does not support bitset");
    static_assert(std::is_floating_point_v<E>, "GaussianMutation requires floating point element type");

    double p_mut = 0.1;
    double sigma = 0.1;

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

// 直列パイプラインで複数 mutation を適用
template<ga::SpecLike Spec, class... Mutations>
struct PipelineMutation {
    using S = Spec;
    std::tuple<Mutations...> muts{};

    PipelineMutation() = default;
    explicit PipelineMutation(Mutations... ms) : muts(std::move(ms)...) {}

    void apply(Spec& s) const { std::apply([&](const auto&... m) { (m.apply(s), ...); }, muts); }
};

// 複数 mutation から重み付きに1つ選ぶ
template<ga::SpecLike Spec, class... Mutations>
struct ChoiceMutation {
    using S = Spec;
    static constexpr std::size_t N = sizeof...(Mutations);
    static_assert(N > 0);

    std::tuple<Mutations...> muts{};
    std::array<double, N> weights{};

    ChoiceMutation() { weights.fill(1.0); }

    ChoiceMutation(std::array<double, N> ws, Mutations... ms)
        : muts(std::move(ms)...), weights(ws) {}

    void apply(Spec& s) const {
        const std::size_t idx = pick_();
        visit_(idx, [&](const auto& op) { op.apply(s); });
    }

private:
    std::size_t pick_() const {
        double sum = 0.0;
        for (double x : weights) if (x > 0.0) sum += x;
        if (!(sum > 0.0)) {
            std::uniform_int_distribution<std::size_t> uni(0, N - 1);
            return uni(ga::rng);
        }
        std::uniform_real_distribution<double> dist(0.0, sum);
        double r = dist(ga::rng);
        for (std::size_t i = 0; i < N; ++i) {
            const double wi = weights[i] > 0.0 ? weights[i] : 0.0;
            if (r < wi) return i;
            r -= wi;
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

} // namespace ssga_ops

#else  // SSGA_USE_GA_OPS_HEADER

// ga_ops 版を使う場合：必要な型だけ ssga_ops 名前空間へ集約して呼び出し側を共通化する
namespace ssga_ops {
using ::TournamentSelection;
using ::WorstSelection;
using ::TwoPointCrossover;
using ::UniformCrossover;
using ::BlendCrossover;
using ::ChoiceCrossover;
using ::RandomResetMutation;
using ::CreepMutation;
using ::GaussianMutation;
using ::PipelineMutation;
using ::ChoiceMutation;

// 置換対象選択：ga_ops 側では「ReverseTournamentSelection」を使う（最悪寄りを選ぶ）
template<class Gene>
using LoserTournamentSelection = ::ReverseTournamentSelection<Gene>;
} // namespace ssga_ops

#endif // SSGA_USE_GA_OPS_HEADER

// ============================================================
// Gene / Spec 定義（仮問題用）
// ============================================================

template<int H>
struct ActionSeq {
    using val_t = std::array<int, H>;
    val_t v{};

    // 0..6（6=stay）
    static constexpr int lo = 0;
    static constexpr int hi = 6;
    static constexpr int def = 6;

    static constexpr std::size_t n_bins = 7;

    // Rolling Horizon 対応
    static constexpr bool is_horizon = true;
};

struct Param {
    using val_t = double;
    val_t v{};

    static constexpr double lo = -1.0;
    static constexpr double hi = +1.0;
    static constexpr double def = 0.0;

    static constexpr std::size_t n_bins = 64;
};

template<int H>
using GenePlan = std::tuple<ActionSeq<H>, Param>;

// ============================================================
// SSGA Runner
// ============================================================

template<class Gene, class EvaluaterT>
struct SSGARunner {
    EvaluaterT& ev;
    int pop_size = 30;

    int runs = 0;   // run() 呼び出し回数
    int turns = 0;  // run() 中の反復回数（子生成回数の目安）

    ga::Population<Gene> pop;

    // ---------- 使うオペレータ ----------
    ssga_ops::TournamentSelection<Gene> parent_sel{};
    ssga_ops::LoserTournamentSelection<Gene> replace_sel{};

    using ActSpec   = std::tuple_element_t<0, Gene>;
    using ParamSpec = std::tuple_element_t<1, Gene>;

    using Crossovers = std::tuple<
        ssga_ops::ChoiceCrossover<ActSpec,
            ssga_ops::TwoPointCrossover<ActSpec>,
            ssga_ops::UniformCrossover<ActSpec>
        >,
        ssga_ops::BlendCrossover<ParamSpec>
    >;

    using Mutations = std::tuple<
        ssga_ops::ChoiceMutation<ActSpec,
            ssga_ops::RandomResetMutation<ActSpec>,
            ssga_ops::CreepMutation<ActSpec>
        >,
        ssga_ops::GaussianMutation<ParamSpec>
    >;

    Crossovers crossovers{};
    Mutations  mutations{};
    Mutations  shake_mutations{}; // 強め攪拌

    // ---------- デバッグ用統計 ----------
    struct Stats {
        long long eval_calls = 0;
        long long children_generated = 0;
        long long children_accepted = 0;
        long long children_accepted_worse = 0;

        long long immigrants = 0;
        long long shakes = 0;

        long long local_improve_trials = 0;
        long long local_improve_accept = 0;

        int stagnation = 0;
        double best_fitness = -std::numeric_limits<double>::infinity();
        float diversity = 0.0f;

        // ログ用
        double next_log_ms = 0.0;
    } stats{};

    // --- 実行制御（軽量化のため間引く） ---
    double log_interval_ms = 100.0; // interval ログ間隔
    int time_check_stride = 128;    // ga::now_ms() を呼ぶ間隔（反復数、2の冪推奨）

    int imm_check_stride   = 64;    // immigration 判定間隔
    int shake_check_stride = 256;   // shake 判定間隔
    int ls_check_stride    = 128;   // local search 判定間隔

    // cooldown（チェック回数単位で管理）
    int imm_cooldown = 0;
    int shake_cooldown = 0;

    explicit SSGARunner(EvaluaterT& e, int n)
        : ev(e), pop_size(n) {

        // 交叉パラメータ
        std::get<0>(crossovers).weights = { 1.0, 1.0 };
        std::get<1>(crossovers).alpha = 0.5;

        // mutation（通常）
        {
            auto& cm = std::get<0>(mutations);
            std::get<0>(cm.muts).p_reset = 0.02;
            std::get<1>(cm.muts).p_mut = 0.08;
            std::get<1>(cm.muts).step_max = 1;

            std::get<1>(mutations).p_mut = 0.12;
            std::get<1>(mutations).sigma = 0.12;
        }

        // mutation（shake 用に強め）
        shake_mutations = mutations;
        {
            auto& cm = std::get<0>(shake_mutations);
            std::get<0>(cm.muts).p_reset = 0.20;
            std::get<1>(cm.muts).p_mut = 0.25;
            std::get<1>(cm.muts).step_max = 2;

            std::get<1>(shake_mutations).p_mut = 0.30;
            std::get<1>(shake_mutations).sigma = 0.25;
        }

        // 選択パラメータ
        parent_sel.k = 3;
        replace_sel.k = 3;
    }

    // SSGA を実行（ga::now_ms() が end_ms に達したら終了）
    void run(double end_ms) {
        ++runs;
        turns = 0;

        init(); // 初回 or Rolling Horizon

        // ログの初期スケジュール
        if (debug_mode) {
            const double now = ga::now_ms();
            stats.next_log_ms = now + log_interval_ms;
        }

        // ループ中の now_ms 呼び出しを減らすため、一定間隔でだけ時刻確認する
        const int mask = std::max(1, time_check_stride) - 1;
        const int imm_mask   = std::max(1, imm_check_stride) - 1;
        const int shake_mask = std::max(1, shake_check_stride) - 1;
        const int ls_mask    = std::max(1, ls_check_stride) - 1;

        // 旧版の「age 全個体インクリメント（O(pop)）」は廃止し、
        // refactored 版の tick（O(1)）で「個体の新しさ/古さ」を管理する。
        // - pop.advance_tick(1) を回すことで、set()/add() 時の born_at が自然に更新される。

        while (true) {
            ++turns;

            // tick を進める（新規個体の born_at に反映）
            pop.advance_tick(1);

            maybe_have_new_child();

            // 高コスト処理は間引く（反復が多いと回数が爆発しやすい）
            if ((turns & imm_mask) == 0)   maybe_have_immigration();
            if ((turns & shake_mask) == 0) maybe_shake();
            if ((turns & ls_mask) == 0)    maybe_local_improve();

            // 時刻確認＆ログ
            if ((turns & mask) == 0) {
                const double now = ga::now_ms();
                if (now >= end_ms) break;
                if (debug_mode && now >= stats.next_log_ms) {
                    log("interval");
                    // まとめて追いつく（遅延時のスパム防止）
                    while (stats.next_log_ms <= now) stats.next_log_ms += log_interval_ms;
                }
            }
        }

        if (debug_mode) log("run end");
    }

private:
    // 初期化 / ローリングホライズン
    void init() {
        if (runs == 1) {
            // 初回：ランダム初期集団
            pop.clear();
            pop.reserve(pop_size);

            for (int i = 0; i < pop_size; ++i) {
                Gene g{};
                ga::init_gene_random(g);
                const double f = ev.evaluate(g);
                ++stats.eval_calls;
                pop.add(std::move(g), f);
            }

        } else {
            // 2回目以降：ホライズンを1つ進める
            // refactored 版は apply_rolling_horizon(shift, random_fill) を持つので、
            // 末尾の補完（旧 refresh_horizon_tail 相当）は random_fill=true で置き換える。
            pop.apply_rolling_horizon(
                [&](Gene& /*g*/, int /*idx*/, int /*shift*/) {
                    // 追加の「修復」や「ヒューリスティック」を入れたい場合はここ
                },
                1,
                true // vacated tail を random_elem_value で埋める
            );

            // 「外側のターンが進んだ」扱いとして tick も少し進める（O(1)）
            pop.advance_tick(1);

            // 状態が変わった想定なので再評価（本番なら差分評価も検討）
            reevaluate_all();
        }

        // best fitness の初期化
        const double bf = pop.get_best_fitness();
        if (!std::isnan(bf)) stats.best_fitness = bf;

        stats.diversity = (float)pop.diversity_rate();
        stats.stagnation = 0;

        imm_cooldown = 0;
        shake_cooldown = 0;
    }

    void reevaluate_all() {
        const int n = pop.size();
        for (int i = 0; i < n; ++i) {
            const double f = ev.evaluate(pop.gene(i));
            ++stats.eval_calls;
            pop.set_fitness(i, f);
        }
    }

    // 子生成（selection/crossover/mutation/evaluate/replace）
    void maybe_have_new_child() {
        if (pop.size() <= 0) return;

        parent_sel.prepare(pop);
        const int p1 = parent_sel.apply(pop);
        int p2 = parent_sel.apply(pop);
        if (p1 < 0 || p2 < 0) return;

        if (p2 == p1 && pop.size() >= 2) {
            // 同一親を軽く避ける
            p2 = (p1 + 1) % pop.size();
        }

        Gene child{};
        ga::apply_crossover(crossovers, pop.gene(p1), pop.gene(p2), child);
        ga::apply_mutation(mutations, child);

        ++stats.children_generated;

        const double cf = ev.evaluate(child);
        ++stats.eval_calls;

        // 置換対象の選択（負トーナメント）
        int loser = replace_sel.apply(pop);
        if (loser < 0) return;

        // best は基本保護（探索で best を消しにくくする）
        const int best = pop.get_best_index();
        if (best >= 0 && loser == best && pop.size() >= 2) {
            std::uniform_int_distribution<int> dist(0, pop.size() - 2);
            const int r = dist(ga::rng);
            loser = (r >= best) ? (r + 1) : r;
        }

        const double wf = pop.fitness(loser);

        bool accept = false;
        if (std::isnan(wf)) {
            accept = !std::isnan(cf);
        } else {
            accept = (!std::isnan(cf) && cf >= wf);
        }

        // 探索性を足す：悪化でも低確率で置換（停滞時・多様性低下時に強める）
        if (!accept) {
            double p = 0.01; // 基本は低め
            if (stats.stagnation >= 300) p = 0.05;
            if (pop.diversity_rate() < 0.20f) p = std::max(p, 0.03);

            std::bernoulli_distribution do_accept(p);
            if (do_accept(ga::rng)) {
                accept = true;
                ++stats.children_accepted_worse;
            }
        }

        if (accept) {
            pop.set(loser, std::move(child), cf);
            ++stats.children_accepted;
            update_stagnation_();
        } else {
            // 受理しない場合でも stagnation は進む
            ++stats.stagnation;
        }

        stats.diversity = (float)pop.diversity_rate();
    }

    void update_stagnation_() {
        const double bf = pop.get_best_fitness();
        if (std::isnan(bf)) {
            ++stats.stagnation;
            return;
        }

        // 小さな揺れに敏感すぎないよう epsilon
        const double eps = 1e-12;
        if (bf > stats.best_fitness + eps) {
            stats.best_fitness = bf;
            stats.stagnation = 0;
        } else {
            ++stats.stagnation;
        }
    }

    // 移民注入（多様性が低い or 停滞が長い時）
    void maybe_have_immigration() {
        if (pop.size() <= 0) return;

        if (imm_cooldown > 0) {
            --imm_cooldown;
            return;
        }

        const float div = pop.diversity_rate();
        const bool low_div   = (!std::isnan((double)div) && div < 0.18f);
        const bool long_stag = (stats.stagnation >= 400);

        if (!(low_div || long_stag)) return;

        // チェック頻度が低いので、確率もそれに合わせて控えめに
        const double p = low_div ? 0.25 : 0.10;
        std::bernoulli_distribution do_imm(p);
        if (!do_imm(ga::rng)) return;

        const int w = pop.get_worst_index();
        if (w < 0) return;

        // refactored 版 Population::set で gene+fitness+born_at を一括更新できるので、
        // 旧「update + set_fitness」を置き換える。
        Gene immigrant{};
        ga::init_gene_random(immigrant);
        const double f = ev.evaluate(immigrant);
        ++stats.eval_calls;

        pop.set(w, std::move(immigrant), f);
        ++stats.immigrants;

        // クールダウン（連発防止）
        imm_cooldown = 8;

        update_stagnation_();
        stats.diversity = (float)pop.diversity_rate();
    }

    // 攪拌（大停滞時のリセット処理）
    void maybe_shake() {
        if (pop.size() <= 0) return;

        if (shake_cooldown > 0) {
            --shake_cooldown;
            return;
        }

        if (stats.stagnation < 900) return;

        ++stats.shakes;

        const int n = pop.size();

        // エリート保護：上位 10%（最大4）
        const int elite_cnt = std::clamp(n / 10, 1, 4);
        std::vector<int> sorted;
        pop.get_sorted_indices(sorted);

        std::vector<char> is_elite((std::size_t)n, 0);
        for (int k = 0; k < elite_cnt && k < (int)sorted.size(); ++k) {
            const int idx = sorted[k];
            if (0 <= idx && idx < n) is_elite[idx] = 1;
        }

        std::bernoulli_distribution do_shake(0.50);

        for (int i = 0; i < n; ++i) {
            if (is_elite[i]) continue;
            if (!do_shake(ga::rng)) continue;

            // pop.update を使って gene[i] を安全に in-place 改変（diversity を自動調整）
            pop.update(i, [&](Gene& g) {
                ga::apply_mutation(shake_mutations, g);
            });

            const double f = ev.evaluate(pop.gene(i));
            ++stats.eval_calls;
            pop.set_fitness(i, f);
        }

        // 大きく動かしたので停滞カウンタをリセット
        stats.stagnation = 0;
        update_stagnation_();

        // クールダウン（連発防止）
        shake_cooldown = 32;

        stats.diversity = (float)pop.diversity_rate();
    }

    // ローカル改善（best 近傍を少し探索）
    void maybe_local_improve() {
        if (pop.size() <= 0) return;

        // ある程度停滞してからだけ行う（軽量化＋効果が出やすい）
        if (stats.stagnation < 80) return;

        std::bernoulli_distribution do_ls(0.20);
        if (!do_ls(ga::rng)) return;

        const int best = pop.get_best_index();
        if (best < 0) return;

        ++stats.local_improve_trials;

        // best のコピーに対して「軽い変異」だけかける（主に Param を微調整）
        Gene trial = pop.gene(best);

        // ActionSeq は触りすぎると崩れやすいので低確率で
        {
            auto& act = std::get<0>(trial);
            ssga_ops::RandomResetMutation<ActSpec> rr;
            rr.p_reset = 0.01;
            rr.apply(act);
        }
        {
            auto& prm = std::get<1>(trial);
            ssga_ops::GaussianMutation<ParamSpec> gm;
            gm.p_mut = 1.0;
            gm.sigma = 0.05;
            gm.apply(prm);
        }

        const double tf = ev.evaluate(trial);
        ++stats.eval_calls;

        const double bf = pop.fitness(best);
        if (std::isnan(bf) || (!std::isnan(tf) && tf > bf)) {
            pop.set(best, std::move(trial), tf);
            ++stats.local_improve_accept;
            update_stagnation_();
            stats.diversity = (float)pop.diversity_rate();
        }
    }

    // ログ出力（stderr）
    void log(const char* reason) const {
        const auto s = pop.get_stats();

        // refactored 版は age を持たない代わりに born_at + current_tick を持つ。
        // age = current_tick - born_at としてログに表示する。
        const int ct = pop.current_tick();

        const int age_min = (s.n > 0) ? (ct - s.born_at_max) : 0;
        const int age_max = (s.n > 0) ? (ct - s.born_at_min) : 0;
        const double age_avg = (s.n > 0 && !std::isnan(s.born_at_avg))
            ? ((double)ct - s.born_at_avg)
            : std::numeric_limits<double>::quiet_NaN();

        std::fprintf(
            stderr,
            "[SSGA] reason=%s runs=%d turns=%d pop=%zu  tick=%d  best=%.6f  "
            "fit(min/avg/max)=%.6f/%.6f/%.6f  "
            "age(min/avg/max)=%d/%.2f/%d  born_at(min/avg/max)=%d/%.2f/%d  "
            "div=%.3f  stag=%d  "
            "eval=%lld acc=%lld(worse=%lld) imm=%lld shake=%lld ls=%lld/%lld\n",
            reason,
            runs,
            turns,
            s.n,
            ct,
            stats.best_fitness,
            s.fitness_min, s.fitness_avg, s.fitness_max,
            age_min, age_avg, age_max,
            s.born_at_min, s.born_at_avg, s.born_at_max,
            (double)pop.diversity_rate(),
            stats.stagnation,
            stats.eval_calls,
            stats.children_accepted,
            stats.children_accepted_worse,
            stats.immigrants,
            stats.shakes,
            stats.local_improve_accept,
            stats.local_improve_trials
        );
    }
};

// ============================================================
// デモ関数
// ============================================================

static void demo_calibrate() {
    std::fprintf(stderr, "\n=== demo_calibrate ===\n");
    Evaluater ev;
    using G = GenePlan<24>;
    const double ms = calibrate<G>(ev, 64);
    std::fprintf(stderr, "evaluate avg = %.6f ms\n", ms);
}

template<class EvaluaterT, class... Genes>
static int choose_best_gene(EvaluaterT& ev, int pop_size, double time_ms_per_gene) {
    std::fprintf(stderr, "\n=== choose_best_gene ===\n");

    int best_idx = -1;
    double best_fit = -std::numeric_limits<double>::infinity();

    int idx = 0;

    auto run_one = [&](auto tag) {
        using G = decltype(tag);

        ga::reset_stats();

        SSGARunner<G, EvaluaterT> runner(ev, pop_size);
        // このデモでは「差だけ見たい」のでログ間隔を長くして run end のみに寄せる
        runner.log_interval_ms = std::max(200.0, time_ms_per_gene);

        const double t0 = ga::now_ms();
        runner.run(t0 + time_ms_per_gene);
        const double bf = runner.pop.get_best_fitness();

        std::fprintf(stderr,
            "  candidate[%d] best_fitness=%.6f  (ga::stats.apply_crossover=%llu apply_mutation=%llu)\n",
            idx, bf,
            (unsigned long long)ga::stats.apply_crossover,
            (unsigned long long)ga::stats.apply_mutation
        );

        if (!std::isnan(bf) && bf > best_fit) {
            best_fit = bf;
            best_idx = idx;
        }
        ++idx;
    };

    (void)std::initializer_list<int>{ (run_one(Genes{}), 0)... };

    std::fprintf(stderr, "=> best gene index = %d (fitness=%.6f)\n", best_idx, best_fit);
    return best_idx;
}

template<class Gene, class EvaluaterT>
static int choose_best_population_size(EvaluaterT& ev, std::span<const int> candidates, double time_ms) {
    std::fprintf(stderr, "\n=== choose_best_population_size ===\n");

    int best_n = -1;
    double best_fit = -std::numeric_limits<double>::infinity();

    for (int n : candidates) {
        ga::reset_stats();

        SSGARunner<Gene, EvaluaterT> runner(ev, n);
        runner.log_interval_ms = std::max(200.0, time_ms);

        const double t0 = ga::now_ms();
        runner.run(t0 + time_ms);
        const double bf = runner.pop.get_best_fitness();

        std::fprintf(stderr, "  pop=%d best_fitness=%.6f  (div=%.3f)\n",
            n, bf, (double)runner.pop.diversity_rate()
        );

        if (!std::isnan(bf) && bf > best_fit) {
            best_fit = bf;
            best_n = n;
        }
    }

    std::fprintf(stderr, "=> best pop size = %d (fitness=%.6f)\n", best_n, best_fit);
    return best_n;
}

static void demo_choose_best_gene() {
    std::fprintf(stderr, "\n=== demo_choose_best_gene ===\n");
    Evaluater ev;

    const int pop_size = 40;
    const double budget_ms = 120.0;

    const int best = choose_best_gene<Evaluater,
        GenePlan<8>,
        GenePlan<16>,
        GenePlan<24>,
        GenePlan<32>
    >(ev, pop_size, budget_ms);

    std::fprintf(stderr, "best gene candidate idx = %d\n", best);
}

static void demo_choose_best_pop_size() {
    std::fprintf(stderr, "\n=== demo_choose_best_pop_size ===\n");
    Evaluater ev;

    std::array<int, 6> cand{ 16, 24, 32, 40, 56, 72 };
    const int best_n = choose_best_population_size<GenePlan<24>>(ev, cand, 120.0);
    std::fprintf(stderr, "best pop size = %d\n", best_n);
}

static void demo_runner_with_rolling_horizon() {
    std::fprintf(stderr, "\n=== demo_runner_with_rolling_horizon ===\n");

    Evaluater ev;

    SSGARunner<GenePlan<24>, Evaluater> runner(ev, 48);
    runner.log_interval_ms = 100.0; // ここは推移も見たいので interval も少し出す

    for (int turn = 0; turn < 10; ++turn) {
        ev.advance_state(turn);

        // ターンごとの時間予算（例）
        const double budget_ms = (turn == 0) ? 120.0 : 45.0;

        const double t0 = ga::now_ms();
        runner.run(t0 + budget_ms);

        // CodinGame ならここで「1手」を出す
        const int best = runner.pop.get_best_index();
        if (best >= 0) {
            const int action0 = std::get<0>(runner.pop.gene(best)).v[0];
            std::fprintf(stderr, "[turn %d] best_action0=%d best_fit=%.6f div=%.3f\n",
                turn, action0, runner.pop.fitness(best), (double)runner.pop.diversity_rate()
            );
        }
    }
}

// ============================================================
// main（デモ）
// ============================================================

int main() {
    // 乱数 seed
    if (debug_mode) {
        ga::rng.seed(42); // 再現性重視
    } else {
        std::random_device rd;
        ga::rng.seed((std::uint32_t)rd());
    }

    demo_calibrate();
    demo_choose_best_gene();
    demo_choose_best_pop_size();
    demo_runner_with_rolling_horizon();

    std::fprintf(stderr, "\n=== done ===\n");
    return 0;
}
