// 個体群 SA。TODO を埋めて使う。未編集では全個体が空の解・コスト 0 で動作する。
// この例は propose で仮適用し、finalize(false) で巻き戻す方式。
#include "sa_pop_v09.hpp" // sa_func_v17.hpp も同じディレクトリに置く。
using namespace std;

using Cost = long long; // 整数は符号付き。小数コストなら double に変更する。

struct Input {
    int n = 0;
    // TODO: 入力データを追加する。
};

struct Snapshot {
    vector<int> solution; // 探索状態を参照せず、出力に必要な情報を値として保存する。

    void print() const {
        // TODO: 問題の出力形式に合わせる。
        // for (int value : solution) cout << value << '\n';
    }
};

struct Move {
    int l = -1;
    int r = -1;
    Cost delta = 0;
    Cost cost_before = 0;
    bool applied = false; // 仮適用前の nullopt では巻き戻しを行わない。
};

// 差分更新のキャッシュを使わず、解から絶対コストを再計算する。
Cost compute_cost(const Input& input, const vector<int>& solution) {
    (void)input;
    (void)solution;
    // TODO: コストを計算する。最大化問題なら、例えば評価値にマイナスを付ける。
    return 0;
}

// 解・キャッシュ・近傍生成用乱数・巻き戻し情報を個体ごとに持つ。
struct WorkState {
    const Input* input; // 探索終了まで生存する入力。
    vector<int> solution;
    Cost cost = 0;
    mt19937_64 rng;
    Move pending_move;

    WorkState(const Input& problem_input, uint64_t seed) : input(&problem_input), rng(seed) {
        // TODO: rng を使って初期解とキャッシュを構築する。個体ごとに初期解を変える。
        cost = compute_cost(*input, solution);
    }
    Snapshot get_snapshot() const {
        return {solution};
    }
    optional<Cost> propose(const sa::SaRuntime<Cost>& runtime) {
        (void)runtime;
        // temperature / progress() / best_cost は今回の内部 SA の値（個体の全履歴ではない）。
        pending_move = {};
        pending_move.cost_before = cost;
        // TODO: 近傍を選び、操作と delta = 提案後 - 提案前、および巻き戻し情報を保存する。
        if (pending_move.l < 0) return nullopt; // TODO: 自分の操作の有効性判定に置き換える。
        pending_move.applied = true;
        // TODO: 解とキャッシュへ仮適用する。仮適用後の nullopt も finalize(false) で戻せるようにする。
        cost += pending_move.delta;
        return pending_move.delta;
    }
    void finalize(bool accepted) {
        if (accepted || !pending_move.applied) return; // 受理は確定、未変更の棄却は何もしない。
        // TODO: 解とキャッシュを提案前へ戻す。事前採取では常にここを通る。
        // 近傍生成用の乱数列は巻き戻さない。
        cost = pending_move.cost_before;
    }
};

Input read_input() {
    Input input;
    // TODO: 入力を読む。
    // cin >> input.n;
    return input;
}

vector<WorkState> make_initial_work_states(const Input& input, int count, uint64_t seed) {
    assert(1 <= count && count <= 256);
    vector<WorkState> states;
    states.reserve(count);
    for (int i = 0; i < count; ++i) {
        const uint64_t state_seed = seed ^ (0x9E3779B97F4A7C15ULL * uint64_t(i + 1));
        states.emplace_back(input, state_seed);
    }
    return states;
}

// time_limit_ms はこの関数に残された時間。初期個体の構築時間もここで差し引く。
Snapshot solve(const Input& input, double time_limit_ms, int initial_state_count = 32,
               uint64_t seed = 1, sa::SaPopCsvStatHook<Cost> csv_hook = sa::SaPopCsvStatHook<Cost>{}) {
    const auto started = chrono::steady_clock::now();
    auto remaining_ms = [&]() -> double {
        return max(0.0, time_limit_ms - chrono::duration<double, milli>(chrono::steady_clock::now() - started).count());
    };

    // 共通設定は既定値。seed は受理用、近傍生成用は各個体の rng。
    sa::SaParam param;
    param.seed = seed;
    param.auto_mode = true;
    param.samples = 300;
    param.start_accept_prob = 0.8;
    param.end_accept_prob = 0.01;
    param.start_temp = 1000.0; // 手動時・自動推定失敗時に使用。
    param.end_temp = 0.1;
    param.enable_end_cost_check = true;
    param.enable_temperature_report = false; // 群全体の見積もりにはならない。下の LOCAL 専用分岐を使う。

    sa::SaPopParam pop_param;
    pop_param.selection_time_ratio = 0.75; // 選別に使う時間の割合。残りは最後の 1 個体に使う。
    pop_param.max_state_count = 256;       // 上限。実際の数は initial_state_count。
    pop_param.selection_policy = sa::SaPopSelectionPolicy::Combination;
    pop_param.selection_current_weight = 0.25; // Combination で現在コストを考慮する重み。
    pop_param.auto_end_temp_scale = 0.1;   // 自動推定時だけ終端温度に掛ける倍率。
    pop_param.acceptance_width_scale = 1.0; // 受理幅はこちらに設定する（SaParam 側より優先）。
    // 手動温度は auto_mode=false と start_temp/end_temp を設定する。
    // 温度見積もりの出力 3 行を使う場合は、ここへ貼り付ける。

    auto get_snapshot = [](const WorkState& state) -> Snapshot { return state.get_snapshot(); };
    auto get_cost = [](const WorkState& state) -> Cost {
        return compute_cost(*state.input, state.solution); // 独立した再評価で、仮適用・巻き戻しを検証する。
    };
    auto propose = [](WorkState& state, const sa::SaRuntime<Cost>& runtime) -> optional<Cost> { return state.propose(runtime); };
    auto finalize = [](WorkState& state, bool accepted) -> void { state.finalize(accepted); };

#ifdef LOCAL
    // true にして -DLOCAL で実行すると、1 個体を単独 SA で走らせ、次回用の推奨温度を stderr へ出す。
    // sa_pop 全体の最適温度ではなく参考値。見積もり後は false に戻し、出力 3 行を上へ貼り付ける。
    constexpr bool estimate_temperature = false;
    if constexpr (estimate_temperature) {
        auto states = make_initial_work_states(input, 1, seed);
        auto& state = states.front();
        param.enable_temperature_report = true;
        param.acceptance_width_scale = pop_param.acceptance_width_scale;
        sa::SaCsvStatHook<Cost> temperature_csv{};
        optional<Cost> existing_best_cost = nullopt;
        auto result = sa::sa<Snapshot, Cost>(
            param, remaining_ms(),
            [&]() -> Snapshot { return get_snapshot(state); },
            [&]() -> Cost { return get_cost(state); },
            [&](const sa::SaRuntime<Cost>& runtime) -> optional<Cost> { return propose(state, runtime); },
            [&](bool accepted) -> void { finalize(state, accepted); },
            existing_best_cost, ref(temperature_csv));
        return move(*result.best_snapshot); // 保存基準を指定していないので必ず存在する。
    }
#endif

    vector<WorkState> states = make_initial_work_states(input, initial_state_count, seed);
    auto debug_hook = [&](sa::SaPopEventType event, const sa::SaPopRuntime<Cost>& runtime) -> void {
        csv_hook(event, runtime); // LOCAL: sa_pop_trace.csv / sa_pop_phase.csv / sa_pop_summary.csv。
        // 必要なら診断を追加する。非 LOCAL では呼ばれない。
    };
    auto [best_cost, best_snapshot] = sa::sa_pop<Snapshot, Cost>(
        param, remaining_ms(), move(states),
        get_snapshot, get_cost, propose, finalize,
        pop_param, debug_hook);
#ifdef LOCAL
    cerr << "best_cost = " << best_cost << '\n';
#endif
    return best_snapshot; // 消えた個体を含む全探索の最良解。move 後の states は使わない。
}

int main() {
    const auto started = chrono::steady_clock::now();
    constexpr double time_limit_ms = 1950.0; // 出力等の余裕を含め、問題の制限に合わせる。
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    const Input input = read_input();
    const double elapsed_ms = chrono::duration<double, milli>(chrono::steady_clock::now() - started).count();
    auto best_snapshot = solve(input, max(0.0, time_limit_ms - elapsed_ms), 32, 1, sa::SaPopCsvStatHook<Cost>{});
    best_snapshot.print();
    return 0;
}
