#include <bits/stdc++.h>
#include "sa_func.hpp"  // 手元のファイル名に合わせて変更
using namespace std;

struct Input {
    // TODO: 入力を入れる
};

struct State {
    // TODO: 最良解として保存したい最小情報だけを持つ
    vector<int> solution;
};

struct Move {
    // TODO: 1手ぶんの近傍情報
    int l = -1;
    int r = -1;
    long long delta = 0;
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    using Cost = long long;

    Input input;
    // TODO: input を読む

    // -----------------------------
    // 現在状態（ユーザー管理）
    // -----------------------------
    vector<int> cur_solution;
    Cost cur_cost = 0;

    // TODO:
    // - 初期解を構築
    // - cur_cost を初期解のコストにする

    // Propose で決めた近傍を Finalize へ渡すための一時領域
    Move pending_move;

    // -----------------------------
    // SA パラメータ
    // -----------------------------
    sa::SaParam param;
    param.seed = 1;
    param.auto_mode = true;
    param.samples = 300;
    param.start_accept_prob = 0.8;
    param.end_accept_prob = 0.01;
    param.start_temp = 100.0;   // auto_mode=false のとき使用
    param.end_temp = 1.0;       // auto_mode=false のとき使用
    param.enable_end_cost_check = true;  // LOCAL 時のみ有効

    // -----------------------------
    // get_state:
    // 最良解保存用のスナップショットを返す
    // -----------------------------
    auto get_state = [&]() -> State {
        State st;
        st.solution = cur_solution;
        return st;
    };

    // -----------------------------
    // get_cost:
    // 現在状態の絶対コストを返す
    // -----------------------------
    auto get_cost = [&]() -> Cost {
        return cur_cost;
    };

    // -----------------------------
    // propose:
    // 近傍を1つ決めて、その delta を返す
    // この流儀では、ここではまだ状態を変更しない
    // -----------------------------
    auto propose = [&](const sa::SaRuntime<Cost>& runtime) -> Cost {
        (void)runtime;  // 使わないなら消してよい

        Move mv;

        // TODO:
        // - 近傍を決める
        // - 現在状態 cur_solution に対するコスト差分 mv.delta を計算する
        //
        // 例:
        // mv.l = ...;
        // mv.r = ...;
        // mv.delta = ...;

        pending_move = mv;
        return pending_move.delta;
    };

    // -----------------------------
    // finalize:
    // accepted=true なら近傍を現在状態へ適用する
    // accepted=false なら何もしない
    // -----------------------------
    auto finalize = [&](bool accepted) -> void {
        if (!accepted) return;

        // TODO:
        // pending_move を cur_solution に適用する
        //
        // 例:
        // reverse(cur_solution.begin() + pending_move.l,
        //         cur_solution.begin() + pending_move.r + 1);

        cur_cost += pending_move.delta;
    };

    // -----------------------------
    // SA 実行
    // CsvStatHook はデフォルト引数のまま使う
    //   - ファイル名: sa_stat.csv
    //   - 記録周期: 50ms
    // -----------------------------
    auto [best_cost, best_state] = sa::sa<State, Cost>(
        param,
        1950.0,  // time_limit_ms
        get_state,
        get_cost,
        propose,
        finalize,
        sa::CsvStatHook<Cost>{}
    );

    // -----------------------------
    // best_state / best_cost を使う
    // -----------------------------
    cerr << "best_cost = " << best_cost << '\n';

    // TODO:
    // best_state.solution を使って出力を作る
    // 例:
    // for (int x : best_state.solution) cout << x << '\n';

    return 0;
}