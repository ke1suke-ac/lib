#include "particle_filter_v10.hpp"

// AHC061: 公開された盤面とAIの選択先だけを使う、内部パラメータ推定の利用例。
// 補助の型・関数はこのクラスの中へまとめる。
class Ahc061Example {
public:
    static constexpr int side = 10, cells = side * side, max_players = 8;
    using Parameters = std::array<double, 5>; // [wa, wb, wc, wd, epsilon]
    using Rng = std::mt19937_64;

    struct Board {
        int players = 0;        // M: 自分を含む人数。2〜8。
        int level_limit = 1;    // U: レベル上限。1〜5。
        std::array<int, cells> value{}; // V: ゲーム中不変のマス価値。
        std::array<int, cells> owner{}; // -1: 空き地、0: 自分、1以上: AI。
        std::array<int, cells> level{};
        std::array<int, max_players> position{}; // 行*10+列。各ターン開始時の駒位置。
    };

    // category: 0=空き地、1=自領土・上限未満、2=敵領土Lv1、
    //           3=敵領土Lv2以上、4=自領土・上限到達。
    struct Candidate {
        int cell, category, value; // category=4のvalueは評価用に0とする。
    };
    struct ChoiceSet {
        std::vector<Candidate> candidates;
        std::array<int, 5> maximum_value{};
        std::array<int, 5> maximum_count{}; // 各種類の最大価値を持つ候補数。
    };
    struct Observation {
        // 過去の盤面を参照せず、そのターンの評価に必要な値を所有する。
        std::array<int, 5> maximum_value{}, maximum_count{};
        int candidate_count = 0;
        int chosen_category = 0, chosen_value = 0;
    };
    struct Prediction {
        std::array<double, cells> probability{}; // 非候補は0、全マス合計は概ね1。
        double selected_mass = 1; // top_k=0なら1。上位集合を使った場合は元の重みの合計。
    };

private:
    struct Greedy {
        double best = -1;
        int count = 0;
    };
    static double evaluation(const Parameters& p, int category, int value) {
        return category == 4 ? 0.0 : p[category] * value;
    }
    static Greedy greedy(const Parameters& p, const std::array<int, 5>& values,
                         const std::array<int, 5>& counts) {
        Greedy result;
        // 係数が正なので、同じ種類ではVが最大のマスだけを比べればよい。
        for (int kind = 0; kind < 5; ++kind) {
            if (counts[kind] == 0) continue; // その種類の候補がない。
            const double score = evaluation(p, kind, values[kind]);
            if (score > result.best) {
                result.best = score;
                result.count = counts[kind];
            } else if (score == result.best) {
                result.count += counts[kind]; // 種類をまたぐ同点も全て数える。
            }
        }
        assert(result.count > 0);
        return result;
    }

    // 事前分布と候補生成だけを共通化する。ここには尤度関数を置かない。
    struct Prior : UniformBox<5> {
        using Observation = Ahc061Example::Observation;
        explicit Prior(double move_scale = 0.2)
            : UniformBox({0.3, 0.3, 0.3, 0.3, 0.1},
                         {1.0, 1.0, 1.0, 1.0, 0.5}, move_scale) {}
    };

public:
    struct ProbabilityModel : Prior {
        explicit ProbabilityModel(double move_scale = 0.2) : Prior(move_scale) {}
        double likelihood(const Particle& p, const Observation& o) const {
            // 1. この仮説で最高評価になる候補と、その個数gを求める。
            const auto best = greedy(p, o.maximum_value, o.maximum_count);
            const bool is_best = evaluation(p, o.chosen_category, o.chosen_value) == best.best;
            // 2. ランダム行動なら、n候補の各マスへepsilon/nの確率がある。
            const double random_part = p[4] / o.candidate_count;
            // 3. 貪欲行動なら、最高評価のgマスへ(1-epsilon)/gずつ配る。
            const double greedy_part = is_best ? (1 - p[4]) / best.count : 0;
            // 4. 二つの行動モードは排他的なので、今回の観測の確率は和になる。
            return random_part + greedy_part;
        }
    };

    struct LogModel : Prior {
        explicit LogModel(double move_scale = 0.2) : Prior(move_scale) {}
        double log_likelihood(const Particle& p, const Observation& o) const {
            // 1. 通常版と同じ最高評価・同点数を計算する。
            const auto best = greedy(p, o.maximum_value, o.maximum_count);
            const bool is_best = evaluation(p, o.chosen_category, o.chosen_value) == best.best;
            // 2. log(epsilon/n)を、対数の差として計算する。
            const double random_log = std::log(p[4]) - std::log(double(o.candidate_count));
            if (!is_best) return random_log; // 貪欲では選ばれない観測。
            // 3. log((1-epsilon)/g)を計算する。log1p(-e)はlog(1-e)。
            const double greedy_log = std::log1p(-p[4]) - std::log(double(best.count));
            // 4. log(a+b)はlog(a)+log(b)ではない。大きい項をくくって安定に足す。
            const double high = std::max(random_log, greedy_log);
            const double low = std::min(random_log, greedy_log);
            return high + std::log1p(std::exp(low - high));
        }
    };

    static ChoiceSet make_choices(const Board& before, int player) {
        assert(2 <= before.players && before.players <= max_players);
        assert(0 <= player && player < before.players);
        const int start = before.position[player];
        assert(0 <= start && start < cells && before.owner[start] == player);
        // 1. 現在の駒から、自分の領土だけを通るBFSを行う。
        std::array<bool, cells> reachable{}, candidate{}, occupied_by_other{};
        std::array<int, cells> queue{};
        int head = 0, tail = 0;
        queue[tail++] = start;
        reachable[start] = true;
        for (int q = 0; q < before.players; ++q)
            if (q != player) occupied_by_other[before.position[q]] = true;
        while (head < tail) {
            const int cell = queue[head++];
            candidate[cell] = true; // 到達可能領土の内部にも移動できる。
            const int row = cell / side, column = cell % side;
            const int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
            for (int k = 0; k < 4; ++k) {
                const int r = row + dr[k], c = column + dc[k];
                if (r < 0 || r >= side || c < 0 || c >= side) continue;
                const int next = r * side + c;
                candidate[next] = true; // 到達可能領土に隣接する1マスも候補。
                if (before.owner[next] == player && !reachable[next]) {
                    reachable[next] = true;
                    queue[tail++] = next;
                }
            }
        }
        ChoiceSet result;
        // 2. 他プレイヤーの現在の駒位置を除き、候補を5種類へ分類する。
        for (int cell = 0; cell < cells; ++cell) {
            if (!candidate[cell] || occupied_by_other[cell]) continue;
            int kind;
            if (before.owner[cell] < 0) kind = 0;
            else if (before.owner[cell] == player)
                kind = before.level[cell] < before.level_limit ? 1 : 4;
            else kind = before.level[cell] == 1 ? 2 : 3;
            const int value = kind == 4 ? 0 : before.value[cell];
            result.candidates.push_back({cell, kind, value});
            // 3. 履歴評価用に、種類別の最大Vとその同点数を集約する。
            if (result.maximum_count[kind] == 0 || value > result.maximum_value[kind]) {
                result.maximum_value[kind] = value;
                result.maximum_count[kind] = 1;
            } else if (value == result.maximum_value[kind]) {
                ++result.maximum_count[kind];
            }
        }
        assert(!result.candidates.empty()); // 自分の現在位置には留まれる。
        return result;
    }

    static Observation make_observation(const ChoiceSet& choices, int chosen_cell) {
        // chosen_cellはtx*10+ty。ターン終了後のex*10+eyではない。
        const auto it = std::find_if(choices.candidates.begin(), choices.candidates.end(),
                                    [&](const Candidate& c) { return c.cell == chosen_cell; });
        assert(it != choices.candidates.end());
        return {choices.maximum_value, choices.maximum_count, int(choices.candidates.size()),
                it->category, it->value};
    }

    // AIごとに独立の推定器を保持する。自分(0)の推定器は作らない。
    template<class Model = LogModel>
    class Estimator {
    public:
        using Filter = ParticleFilter<Model>;
        using Param = typename Filter::Param;
    private:
        std::vector<Filter> filters_;
    public:
        explicit Estimator(int players, Param param = {}, std::uint64_t seed = 0,
                           double move_scale = 0.2) {
            assert(2 <= players && players <= max_players);
            filters_.reserve(players - 1);
            // seedから相手別の乱数器を作る。同じ構成なら再現可能。
            Rng seeder(seed);
            for (int p = 1; p < players; ++p)
                filters_.emplace_back(Model(move_scale), param, Rng(seeder()));
        }
        Filter& filter(int player) {
            assert(1 <= player && player <= int(filters_.size()));
            return filters_[player - 1];
        }
        const Filter& filter(int player) const {
            assert(1 <= player && player <= int(filters_.size()));
            return filters_[player - 1];
        }
        std::vector<typename Filter::ObserveResult> observe(
            const Board& before, std::span<const int> chosen_cells) {
            assert(before.players == int(filters_.size()) + 1);
            assert(int(chosen_cells.size()) == before.players);
            std::vector<typename Filter::ObserveResult> result;
            for (int p = 1; p < before.players; ++p) {
                // 前の盤面から条件を固定してから、今回の選択先を1回だけ取り込む。
                const auto choices = make_choices(before, p);
                result.push_back(filter(p).observe(make_observation(choices, chosen_cells[p])));
            }
            // 要素p-1はAI pの結果。全員を一括で巻き戻すトランザクションではない。
            return result;
        }
        Prediction predict(const Board& before, int player, int top_k = 0) const {
            // 1. 次の行動を予測したい時点の候補を、現在の盤面から作る。
            const auto choices = make_choices(before, player);
            Prediction result;
            // 2. 平均パラメータでargmaxを取らず、粒子ごとの行動確率を平均する。
            result.selected_mass = filter(player).for_each_weighted(
                [&](const Parameters& p, double weight) {
                    const auto best = greedy(p, choices.maximum_value, choices.maximum_count);
                    const double random_part = p[4] / choices.candidates.size();
                    const double greedy_part = (1 - p[4]) / best.count;
                    for (const auto& c : choices.candidates) {
                        const bool is_best = evaluation(p, c.category, c.value) == best.best;
                        result.probability[c.cell] += weight * (random_part + (is_best ? greedy_part : 0));
                    }
                }, top_k);
            return result;
        }
    };

    template<class Model = LogModel>
    static int run_interactive(std::istream& in, std::ostream& out,
                               int count = 256, std::uint64_t seed = 0) {
        // 1. ジャッジが公開する初期入力だけを読む。秘密パラメータや秘密乱数は読まない。
        int n, turns;
        Board board;
        if (!(in >> n)) return 0;
        if (!(in >> board.players >> turns >> board.level_limit)) return 1;
        assert(n == side && turns == 100);
        for (int& v : board.value) if (!(in >> v)) return 1;
        board.owner.fill(-1);
        for (int p = 0; p < board.players; ++p) {
            int row, column;
            if (!(in >> row >> column)) return 1;
            const int cell = row * side + column;
            board.position[p] = cell;
            board.owner[cell] = p;
            board.level[cell] = 1;
        }
        // 2. 全ターンで同じ推定器を使う。countはAI1人あたりの粒子数。
        typename Estimator<Model>::Param param;
        param.count = count;
        param.move_steps = 1; // 時間配分の例。ライブラリの既定値は2。
        Estimator<Model> estimator(board.players, param, seed);
        for (int turn = 0; turn < turns; ++turn) {
            // 3. 今の盤面で各AIの移動先分布を予測する。
            std::array<Prediction, max_players> prediction{};
            for (int p = 1; p < board.players; ++p)
                prediction[p] = estimator.predict(board, p);
            // 4. 接続例として、自分の手による即時の獲得・強化分を競合確率で補正して選ぶ。
            // 自領土の防衛効果や他マスへの攻撃による損失は、この簡易評価では省く。
            // 長期計画やS0/max(Sp)の最適化は、この部分を置き換えて実装する。
            const auto choices = make_choices(board, 0);
            int selected = choices.candidates.front().cell;
            double best_gain = -1;
            for (const auto& c : choices.candidates) {
                double gain = c.category <= 2 ? board.value[c.cell] : 0.0;
                if (board.owner[c.cell] != 0) {
                    // 空き地・敵領土では、誰かと衝突すると自分は回収される。
                    // AI間の行動は、公開履歴を条件にした独立な推定モデルで扱う。
                    for (int p = 1; p < board.players; ++p)
                        gain *= 1 - prediction[p].probability[c.cell];
                } // 自領土での衝突は所有者である自分が残る。
                if (gain > best_gain) { best_gain = gain; selected = c.cell; }
            }
            // 5. 移動先を1行出力し、必ずflushしてジャッジへ渡す。
            out << selected / side << ' ' << selected % side << std::endl;

            // 6. 選択先tx,tyと終了位置ex,eyを別々に読む。次盤面へはまだ置き換えない。
            std::array<int, max_players> chosen{};
            Board next = board;
            for (int p = 0; p < board.players; ++p) {
                int row, column;
                if (!(in >> row >> column)) return 1;
                chosen[p] = row * side + column;
            }
            for (int p = 0; p < board.players; ++p) {
                int row, column;
                if (!(in >> row >> column)) return 1;
                next.position[p] = row * side + column;
            }
            for (int& owner : next.owner) if (!(in >> owner)) return 1;
            for (int& level : next.level) if (!(in >> level)) return 1;

            // 7. 古い盤面と、相手が実際に選んだ移動先から観測を作る。
            const auto status = estimator.observe(board, std::span<const int>(chosen.data(), board.players));
            for (const auto& s : status) if (!s.ok) return 1;
            // 8. 更新後ならパラメータの平均も読める。ここではAI 1を例にする。
            const auto mean = estimator.filter(1).estimate().mean; // 順番は[wa,wb,wc,wd,epsilon]。
            (void)mean; // 診断を出すならstderrへ。stdoutは移動先の出力専用。
            // 9. 推定に前の条件を保存し終えてから、次のターンの盤面へ進める。
            board = std::move(next);
        }
        return 0;
    }
};

#if defined(AHC061_PARTICLE_FILTER_MAIN)
int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    // 通常版を使う場合はProbabilityModelへ変更する。同じModelに両方の尤度を定義しない。
    return Ahc061Example::run_interactive<Ahc061Example::LogModel>(std::cin, std::cout);
}
#endif
