#include <bits/stdc++.h>
#include "sa_pop.hpp"
using namespace std;

using Cost = long long;

// 問題入力
struct Input {
    int n;
    // TODO: 問題固有の入力データを持たせる
};

// 最終的に出力したい解
struct Answer {
    // TODO: 出力に必要な最小限の情報を持たせる

    void print() const {
        // TODO: 出力処理
    }
};

// 探索中に使う状態
struct State {
    const Input* input = nullptr;

    Cost cost = 0;

    // propose で入れた仮変更を reject 時に戻すための情報
    // TODO: 問題に応じて変更
    int last_a = -1;
    int last_b = -1;
    Cost last_delta = 0;

    State() = default;

    State(const Input& input_, uint64_t seed) : input(&input_) {
        // TODO: seed を使って初期解を作る
        // TODO: cost を初期化する
    }

    Answer snapshot() const {
        Answer ans;
        // TODO: 現在の state から出力用 Answer を作る
        return ans;
    }

    Cost propose(const sa::SaRuntime<Cost>& runtime) {
        // runtime.temperature
        // runtime.iteration
        // runtime.current_cost
        // runtime.best_cost
        // などを見られる

        (void)runtime;

        // TODO:
        // 1. 近傍を1つ選ぶ
        // 2. state に仮変更を入れる
        // 3. delta = new_cost - old_cost を返す
        //
        // 最小化問題なので、
        //   delta < 0 なら改善
        //   delta > 0 なら悪化
        //
        // reject 時に戻せるよう、last_* に情報を保存しておく。

        Cost delta = 0;

        last_delta = delta;
        cost += delta; // 仮変更を current に反映する想定

        return delta;
    }

    void finalize(bool accepted) {
        if (accepted) {
            // TODO:
            // 仮変更を確定する。
            // すでに propose 内で state を変更済みなら何もしなくてよい。
        } else {
            // TODO:
            // propose で入れた仮変更を戻す。
            cost -= last_delta;
        }
    }
};

Input read_input() {
    Input input;

    // TODO: 入力を読む
    // cin >> input.n;

    return input;
}

vector<State> make_initial_states(
    const Input& input,
    int K,
    uint64_t seed
) {
    vector<State> states;
    states.reserve(K);

    for (int i = 0; i < K; ++i) {
        uint64_t state_seed =
            seed ^ (0x9E3779B97F4A7C15ULL * static_cast<uint64_t>(i + 1));

        states.emplace_back(input, state_seed);
    }

    return states;
}

// CSV hook はデフォルト引数として持たせる。
// LOCAL ビルド時だけ sa_pop 内部から hook が呼ばれ、
// デフォルトでは以下の3ファイルが RunEnd で一括出力される。
//   sa_pop_trace.csv
//   sa_pop_phase.csv
//   sa_pop_summary.csv
Answer solve(
    const Input& input,
    double time_limit_ms,
    int K = 32,
    uint64_t seed = 1,
    sa::SaPopCsvHook<Cost> csv_hook = sa::SaPopCsvHook<Cost>{}
) {
    vector<State> states = make_initial_states(input, K, seed);

    sa::SaParam sa_param;
    sa_param.seed = seed;
    sa_param.auto_mode = true;
    sa_param.samples = 300;
    sa_param.start_accept_prob = 0.8;
    sa_param.end_accept_prob = 0.01;
    sa_param.start_temp = 100.0; // auto 失敗時の fallback
    sa_param.end_temp = 1.0;     // auto 失敗時の fallback

    sa::SaPopParam pop_param;
    pop_param.selection_ratio = 0.75;
    pop_param.max_population = 256;
    pop_param.selection_policy = sa::SaPopSelectionPolicy::Combination;
    pop_param.selection_current_weight = 0.25;

    auto [best_cost, best_answer] =
        sa::sa_pop<State, Cost, Answer>(
            sa_param,
            time_limit_ms,
            std::move(states),

            // get_cost
            [](const State& state) -> Cost {
                return state.cost;
            },

            // get_snapshot
            [](const State& state) -> Answer {
                return state.snapshot();
            },

            // propose
            // 一番引数が多い形式:
            //   Cost(State&, const sa::SaRuntime<Cost>&)
            [](State& state, const sa::SaRuntime<Cost>& runtime) -> Cost {
                return state.propose(runtime);
            },

            // finalize
            // 一番引数が多い形式:
            //   void(State&, bool)
            [](State& state, bool accepted) -> void {
                state.finalize(accepted);
            },

            pop_param,

            // CSV hook
            // solve のデフォルト引数なので、呼び出し側は省略できる。
            csv_hook
        );

    (void)best_cost;
    return best_answer;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Input input = read_input();

    constexpr double TIME_LIMIT_MS = 1900.0;
    constexpr int K = 32;
    constexpr uint64_t SEED = 1;

#ifdef LOCAL
    // CSV名を変えたい場合。
    // 省略すると solve のデフォルト引数により "sa_pop_" prefix になる。
    auto answer = solve(
        input,
        TIME_LIMIT_MS,
        K,
        SEED,
        sa::SaPopCsvHook<Cost>{"sa_pop_"}
    );
#else
    // 提出時は hook 呼び出し自体が無効化される想定。
    auto answer = solve(input, TIME_LIMIT_MS, K, SEED);
#endif

    answer.print();
    return 0;
}