// ssga_sample_v7.cpp
// ------------------------------------------------------------
// Codingame を想定した「本気の SSGA (Steady-State GA)」サンプル。
//  - 最新リファクタ後の ga.hpp / ga_ops.hpp に対応（互換性は考えない）
//  - ヘッダにある処理を最大限活用するデモ
//  - 評価関数（Evaluator）は問題ごとに差し替え前提のダミー実装
//
// 命名方針:
//  - GA の反復単位は iter と呼ぶ（iters に統一）
//
// 注意:
//  - ga.hpp / ga_ops.hpp はヘッダ内に #if 1 のテスト main() を持つ。
//    そのまま include すると main が二重定義になり得るため、
//      * ga.hpp は #define main ... でリネームして include
//      * ga_ops.hpp は「ヘッダ全体を名前空間に入れて」 main を回避
//    という方針にしている。
// ------------------------------------------------------------

// （このサンプルが単体でコンパイルできるように、必要な標準ヘッダを明示）
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

// ------------------------------------------------------------
// include ga.hpp / ga_ops.hpp
// ------------------------------------------------------------

// ga.hpp 側の #if 1 テスト main() を衝突回避する
#define main ga__header_main
#include "ga.hpp"
#undef main

// ga_ops.hpp 側もヘッダ内に #if 1 のテスト main() を持つ。
// さらにヘッダ自身が「ga.hpp を include する間だけ main を別名にする」処理を含むため、
// こちら側で #define main ... を仕込んでも「ga_ops.hpp 内のテスト main」までは回避できない。
// そのため include 全体を名前空間に入れて、グローバル main と衝突しないようにする。
namespace gaop {
#include "ga_ops.hpp"
} // namespace gaop

// ------------------------------------------------------------
// グローバル設定
// ------------------------------------------------------------
bool debug_mode = true; // デバッグモードのグローバル定義

// ------------------------------------------------------------
// calibrate
//  - 指定した実行時間(ms)の範囲で、ランダム Gene を生成して evaluate を繰り返す
//  - evaluate の平均実行時間(ms)を返す
//  - trial は最大 10000 回まで（評価が軽すぎる場合は上限で先に止まる）
//
// 実装メモ:
//  - now_ms() の呼び出し自体もオーバーヘッドになるので、1回ずつ計測せずに
//    バッチ単位で evaluate だけをまとめて計測する（gene 生成は計測外）
// ------------------------------------------------------------
template<class Gene, class Evaluator>
static double calibrate(Evaluator& ev, double budget_ms) {
    constexpr int kMaxTrials = 10000;
    constexpr int kBatch = 32;

    if (!(budget_ms > 0.0)) budget_ms = 1.0;

    // 最適化で評価が消えないようにする簡易シンク
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

        // まずはランダム個体を作る（計測外）
        for (int i = 0; i < batch; ++i) {
            ga::init_gene_random(buf[(std::size_t)i]);
        }

        // evaluate だけまとめて計測
        const double t0 = ga::now_ms();
        for (int i = 0; i < batch; ++i) {
            sink = sink + ev.template evaluate<Gene>(buf[(std::size_t)i]);
        }
        const double t1 = ga::now_ms();

        eval_total_ms += (t1 - t0);
        trials += batch;
    }

    // 予算が小さすぎて 0 回になった場合の保険
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

// ------------------------------------------------------------
// ユーザが定義する評価関数（Codingame なら State に相当）
//  - 最低限必要: template<class Gene> double evaluate(const Gene&)
//  - 本番ではゲーム状態を保持し、evaluate 内でシミュレータを回す
//
// このサンプルの重要ポイント:
//  - ホライズン長を変える Gene 型を比較するデモがあるため、
//    ここでは「ホライズン長でスコアが単純に増えない」ように
//    割引付き平均（discounted average）で評価値を正規化している。
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
        const auto& seq   = std::get<0>(g).v; // horizon action sequence
        const auto  param = std::get<1>(g).v; // float parameter
        const auto& flags = std::get<2>(g).v; // bitset
        const auto& route = std::get<3>(g).v; // permutation (target order)

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
//  - ga の SpecLike に従って Spec を用意する
//  - ActionPlan は is_horizon=true にして rolling horizon 対応
// ------------------------------------------------------------
static constexpr int kActions = 6;
static constexpr int kFlagsN  = 32;
static constexpr int kRouteN  = Evaluator::kRouteN;

template<int H>
struct ActionPlan {
    using elem_t = int;
    static constexpr elem_t lo = 0;
    static constexpr elem_t hi = kActions - 1;
    static constexpr std::size_t n_bins = kActions;
    static constexpr elem_t def = 4;              // WAIT をデフォルト
    static constexpr bool is_horizon = true;      // rolling horizon 対象
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
    static constexpr bool is_perm = true; // 順列として扱う（init_random が shuffle になる想定）
    using val_t = std::array<elem_t, N>;
    val_t v = ga::init_perm<val_t>();
};

// ホライズン長を型で変える例（choose_best_gene 用）
template<int H>
using GeneH = std::tuple<ActionPlan<H>, Aggression, BoostMask, RouteOrder<kRouteN>>;

// ------------------------------------------------------------
// SSGA Runner
//  - 可能な限り ga / ga_ops の機能を使って実装
// ------------------------------------------------------------
template<class Gene, class EvaluatorT>
struct SSGARunner {
    EvaluatorT& evaluator;
    int pop_size = 0;

    int run_no = 0; // run() の呼び出し回数
    int iters  = 0; // 反復回数（iter 単位で統一）

    ga::Population<Gene> pop;

    // --- 遺伝的オペレータ（ga_ops.hpp の内容は gaop 名前空間に入れている） ---
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

    // --- GA 戦略パラメータ（問題に応じて調整） ---
    double p_crossover = 0.90;

    // 移民
    int immigration_interval = 0;      // pop_size に依存して決める
    double immigration_chance = 0.35;  // interval 到達時に注入する確率

    // 停滞・攪拌
    int stagnation_window = 0;         // pop_size に依存して決める
    float diversity_floor = 0.20f;
    int shake_replace = 0;             // 1 回の攪拌で置換する個体数（pop_size に依存）

    // ログ
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

        // 新 API: gene/fitness/born_at は private なので Population::reserve を使う
        pop.reserve(pop_size);

        // 選択圧
        parent_sel.k = 3;
        victim_sel.k = 3;

        // pop_size に依存して「世代感」を合わせる（steady-state は 1 iter = 1 child）
        immigration_interval = std::max(16, pop_size * 2); // 2 世代に 1 回チェック
        stagnation_window   = std::max(64, pop_size * 6);  // 6 世代改善なしで停滞扱い
        shake_replace       = std::max(2, pop_size / 10);  // 10% だけ入れ替え（小さすぎないよう 2）

        // crossover 設定
        {
            auto& cc = std::get<0>(cross);
            cc.weights = {0.7, 0.3}; // TwoPoint を多め
        }

        // mutation 設定
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

    // --------------------------------------------------------
    // run: SSGA 実行（ga::now_ms() が end_time_ms に達したら終了）
    // --------------------------------------------------------
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
    //
    // 新 API メモ:
    //  - Population は born_at (tick) を内部で持つ。旧 age は廃止されたため、
    //    iter ごとの aging は advance_tick() に置き換える。
    //  - apply_rolling_horizon は (index, shift) 付きのコールバックに変更され、
    //    さらに random_fill で tail をランダム埋めできるようになった。
    // --------------------------------------------------------
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

        // rolling horizon:
        //  - is_horizon=true の Spec を shift し、空いた tail を random_fill=true でランダム埋め
        pop.apply_rolling_horizon(
            [](Gene& /*g*/, int /*index*/, int /*shift*/) {
                // 必要ならここで補修・制約修正などを行う
            },
            1,
            /*random_fill=*/true
        );

        // 状態が変わった前提で全個体を再評価
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

    // --------------------------------------------------------
    // maybe_make_child
    //  - selection / crossover / mutation / evaluation / replacement
    // --------------------------------------------------------
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

        // 交叉するか
        {
            std::bernoulli_distribution do_cross(p_crossover);
            if (do_cross(ga::rng)) {
                ga::apply_crossover(cross, pop.gene(p1), pop.gene(p2), child);
            } else {
                const int better = pop.better(p1, p2) ? p1 : p2;
                child = pop.gene(better);
            }
        }

        // 突然変異
        ga::apply_mutation(muts, child);

        // 評価
        const double f = evaluator.template evaluate<Gene>(child);
        ++stats.evals;
        ++stats.children;

        // 旧 API の age 更新 (全員 +1) は、新 API では advance_tick() で代替
        //  - set()/add() は born_at を current_tick に更新するため、
        //    ここで tick を進めてから set() することで「新生個体」を表現できる。
        pop.advance_tick(1);

        // 置換先（逆トーナメントで悪い個体を狙う）
        int victim = victim_sel.apply(pop);
        if (victim < 0) victim = 0;

        // ベストはなるべく保持
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

    // --------------------------------------------------------
    // maybe_immigrate
    //  - 定期的に 1 個体だけランダム注入して多様性を確保
    // --------------------------------------------------------
    void maybe_immigrate() {
        if (immigration_interval <= 0) return;
        if (iters % immigration_interval != 0) return;

        // 多様性が十分あるなら注入確率を下げる
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

        // age(+1) 相当
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

    // --------------------------------------------------------
    // maybe_shake
    //  - 停滞 + 低多様性 を検知して攪拌
    // --------------------------------------------------------
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

            // 旧実装同様: 揺すりは iter 内の攪拌扱いなので tick は進めない
            // (全置換が同じ「世代時刻」で生まれるイメージ)
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


// ------------------------------------------------------------
// choose_best_gene
//  - Gene 候補（ホライズン長など）を複数試して、最も良かった Gene 型の index を返す
//  - 比較の公平性のため、各候補の実行前に RNG 状態を揃える
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// choose_best_population_size
//  - pop_size 候補を複数試して、最も良かった pop_size を返す
//  - 比較の公平性のため、各候補の実行前に RNG 状態を揃える
// ------------------------------------------------------------
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


// ------------------------------------------------------------
// choose_best_trials
//  - Gene 型固定で、同じ Gene/個体数の SSGA を複数回まわして比較する汎用版
//
// 引数:
//  - ev        : 評価器（State 相当）
//  - pop_size  : 集団サイズ
//  - run_ms    : 1 回あたりの実行時間（ms）
//  - trials    : 実行回数（<=0 なら 1）
//  - prepare   : 各 trial の直前に呼ばれるコールバック
//                prepare(trial_index, runner, ev)
//                trial_index は 0-based（0,1,2,...）
//                ここで runner / ev のパラメータや状態を trial ごとに変えることを想定。
//
// 返り値:
//  - 全 trial のうち best_fitness が最大だった trial の index（0-based）
//
// 補足:
//  - 比較の公平性のため、各 trial の開始前に RNG 状態を base に戻す。
//    ただし prepare 内で seed を変えるなどは自由。
// ------------------------------------------------------------
template<class Gene, class EvaluatorT, class PrepareFn>
static int choose_best_trials(EvaluatorT& ev, int pop_size, double run_ms, int trials, PrepareFn prepare) {
    if (trials <= 0) trials = 1;

    const std::mt19937 base_rng = ga::rng;

    int best_trial = 0;
    double best_fit = -std::numeric_limits<double>::infinity();

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
        const int action0 = std::get<0>(best).v[0];
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
