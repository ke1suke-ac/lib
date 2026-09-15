// 個体群 SA のスケルトン。propose で仮適用し、finalize(false) で巻き戻す
// TODO を問題に合わせて実装する。未編集では全個体が空の解・コスト 0 のまま
#include <bits/stdc++.h>
#include "sa_pop_v04.hpp"  // sa_func_v12.hpp と同じディレクトリに配置する
using namespace std;

using Cost = long long;  // 差分は負になり得るため、整数なら符号付き型を使う

struct Input {
    int n = 0;
    // TODO: 問題固有の入力データ
};

// 出力・復元に必要な最小限の情報。探索状態を参照せず、値として保存する
struct Snapshot {
    vector<int> solution;

    // 保存した解を出力する O(N)、N は出力する要素数
    void print() const {
        // TODO: 問題の出力形式に合わせる
        // for (int value : solution) cout << value << '\n';
    }
};

struct Move {
    int l = -1;
    int r = -1;
    Cost delta = 0;
    Cost cost_before = 0;
};

// 解全体から絶対コストを再計算する O(C)、C は問題固有の評価処理量
Cost compute_cost(const Input& input, const vector<int>& solution) {
    (void)input;
    (void)solution;
    // TODO: input と solution から計算する。差分で更新したキャッシュは使わない
    return 0;
}

// 解・差分評価用キャッシュ・近傍生成用乱数・巻き戻し情報は個体ごとに持つ
struct WorkState {
    const Input* input;  // 探索終了まで生存する入力を参照する
    vector<int> solution;
    Cost cost = 0;
    mt19937_64 rng;
    Move pending_move;

    // seed ごとに初期解を構築する O(B+C)、B は構築、C は全体評価の処理量
    WorkState(const Input& problem_input, uint64_t seed) : input(&problem_input), rng(seed) {
        // TODO: input と rng を使い、個体ごとに異なる初期解を solution に構築する
        cost = compute_cost(*input, solution);
    }

    // 現在解を独立した値としてコピーする O(N)、N は保存する要素数
    Snapshot get_snapshot() const {
        return {solution};
    }

    // 近傍を仮適用し、そのコスト差分を返す O(P)、P は近傍生成・差分評価・仮適用の処理量
    Cost propose(const sa::SaRuntime<Cost>& runtime) {
        (void)runtime;
        // temperature / iteration / current_cost / best_cost / progress() を参照できる
        // これは内部 SA の Runtime。progress() は内部 SA ごとに始まり、全体進捗ではない
        pending_move = {};
        pending_move.cost_before = cost;

        // TODO: rng で近傍を選び、仮適用前に delta = 提案後コスト - cost を計算する
        // pending_move.l / r / delta と、必要なキャッシュの巻き戻し情報を保存する
        // 近傍がない場合は delta=0 の無操作にし、finalize でも解を変更しない

        // TODO: solution と評価用キャッシュへ仮適用する
        // 自動温度推定中も呼ばれ、その直後は必ず finalize(false) となる
        cost += pending_move.delta;
        return pending_move.delta;
    }

    // 非受理なら提案前の解・評価用キャッシュ・コストへ戻す O(F)、F は巻き戻しの処理量
    void finalize(bool accepted) {
        if (accepted) return;  // 仮適用済みなので、そのまま確定する

        // TODO: pending_move を使って solution と評価用キャッシュを戻す
        // 乱数列は進めたままにする。解に関係する変更は全て元に戻す
        cost = pending_move.cost_before;  // 浮動小数型へ変更しても、加減算の誤差を残さず復元する
    }
};

// 問題入力を読み込む O(R)、R は入力サイズ
Input read_input() {
    Input input;
    // TODO: 入力を読む
    // cin >> input.n;
    return input;
}

// 個体ごとに seed を変えて初期解群を構築する O(K(B+C))、K は個体数
vector<WorkState> make_initial_work_states(const Input& input, int initial_state_count, uint64_t seed) {
    assert(1 <= initial_state_count && initial_state_count <= 256);
    vector<WorkState> work_states;
    work_states.reserve(static_cast<size_t>(initial_state_count));
    for (int i = 0; i < initial_state_count; ++i) {
        const uint64_t state_seed = seed ^ (0x9E3779B97F4A7C15ULL * static_cast<uint64_t>(i + 1));
        work_states.emplace_back(input, state_seed);
    }
    return work_states;
}

// 個体群 SA を実行して保存解を返す O(初期解群の構築処理量 + SA の実行処理量)
Snapshot solve(
    const Input& input,
    double time_limit_ms,
    int initial_state_count = 32,
    uint64_t seed = 1,
    sa::SaPopCsvStatHook<Cost> csv_hook = sa::SaPopCsvStatHook<Cost>{}
) {
    // 初期解構築は sa_pop に渡す時間予算の外側で行う
    vector<WorkState> work_states = make_initial_work_states(input, initial_state_count, seed);

    // 共通パラメータは既定値。seed は受理判定用で、近傍生成は各個体の rng を使う
    sa::SaParam param;
    param.seed = seed;
    param.auto_mode = true;
    param.samples = 300;  // 各個体へ均等配分するため、合計回数は切り上げられる
    param.start_accept_prob = 0.8;
    param.end_accept_prob = 0.01;
    param.start_temp = 100.0;  // 手動時、および自動推定失敗時に使用
    param.end_temp = 1.0;
    param.enable_end_cost_check = true;  // LOCAL 時、最終 SA だけで終了診断する

    // 個体群用パラメータも既定値。個体数自体は initial_state_count で指定する
    sa::SaPopParam pop_param;
    pop_param.selection_time_ratio = 0.75;  // 個体選抜に割り当てる時間の割合
    pop_param.max_state_count = 256;       // 受け付ける個体数の上限
    pop_param.selection_policy = sa::SaPopSelectionPolicy::Combination;
    pop_param.selection_current_weight = 0.25;
    // 方針は Current / LifetimeBest / Combination。重みは Combination のみで使用する

    // コールバック順序は単独 SA と同じ。先頭に対象の WorkState を受け取る
    auto get_snapshot = [](const WorkState& state) -> Snapshot {
        return state.get_snapshot();
    };
    auto get_cost = [](const WorkState& state) -> Cost {
        // 各内部 SA の開始時などに呼ばれる。差分と独立した評価で整合性を確認する
        // state.cost を返せば軽いが、解本体の更新ミスを終了診断で検出できない場合がある
        return compute_cost(*state.input, state.solution);
    };
    auto propose = [](WorkState& state, const sa::SaRuntime<Cost>& runtime) -> Cost {
        return state.propose(runtime);
    };
    auto finalize = [](WorkState& state, bool accepted) -> void {
        state.finalize(accepted);
    };

    // 既定設定: sa_pop_trace.csv / sa_pop_phase.csv / sa_pop_summary.csv
    // 単独版の周期記録とは異なり、個体・フェーズ単位で記録し、RunEnd で一括出力する
    auto debug_hook = [&](sa::SaPopEventType event_type, const sa::SaPopRuntime<Cost>& runtime) -> void {
        csv_hook(event_type, runtime);
        // TODO: 必要なら追加の診断を行う。非 LOCAL ではこのラムダ自体が呼ばれない
    };

    // Snapshot, Cost の順に指定し、WorkState は推論に任せる
    // move 後の work_states は使わない。返却解は最終 SA 内の最良で、全個体・全期間の最良とは限らない
    auto [best_cost, best_snapshot] = sa::sa_pop<Snapshot, Cost>(
        param,
        time_limit_ms,
        move(work_states),
        get_snapshot,
        get_cost,
        propose,
        finalize,
        pop_param,
        debug_hook
    );

#ifdef LOCAL
    cerr << "best_cost = " << best_cost << '\n';
#endif
    return best_snapshot;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    const Input input = read_input();

    // 入力・初期解構築・出力の時間も考慮して、制限時間には余裕を取る
    // 全引数を明示するが、CSV Hook のコンストラクタは既定設定のままにする
    auto best_snapshot = solve(input, 1900.0, 32, 1, sa::SaPopCsvStatHook<Cost>{});
    best_snapshot.print();
    return 0;
}
