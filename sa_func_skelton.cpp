// 単独 SA。TODO を埋めて使う。未編集では空の解・コスト 0 で動作する。
// この例は propose で提案だけ作り、finalize(true) で適用する方式。
#include "sa_func_v17.hpp"
using namespace std;

using Cost = long long; // 整数は符号付き。小数コストなら double に変更する。

struct Input {
    int n = 0;
    // TODO: 入力データを追加する。
};

// 保存後に探索を進めても変化しない、出力に必要な情報だけを持つ。
struct Snapshot {
    vector<int> solution;

    void print() const {
        // TODO: 問題の出力形式に合わせる。
        // for (int value : solution) cout << value << '\n';
    }
};

struct Move {
    int l = -1;
    int r = -1;
    Cost delta = 0;
};

// 差分更新のキャッシュを使わず、解から絶対コストを再計算する。
Cost compute_cost(const Input& input, const vector<int>& solution) {
    (void)input;
    (void)solution;
    // TODO: コストを計算する。最大化問題なら、例えば評価値にマイナスを付ける。
    return 0;
}

int main() {
    const auto started = chrono::steady_clock::now();
    constexpr double time_limit_ms = 1950.0; // 出力等の余裕を含め、問題の制限に合わせる。
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Input input;
    // TODO: 入力を読む。
    // cin >> input.n;

    constexpr uint64_t seed = 1;
    [[maybe_unused]] mt19937_64 rng(seed); // 近傍生成用。SA の受理乱数とは別。
    vector<int> cur_solution;
    // TODO: 初期解と差分計算用のキャッシュを構築する。
    Cost cur_cost = compute_cost(input, cur_solution);
    Move pending_move;

    // 最初はこの既定値で使い、必要な項目だけ変更する。
    sa::SaParam param;
    param.seed = seed;
    param.auto_mode = true;             // 手動温度を使うときは false。
    param.samples = 300;
    param.start_accept_prob = 0.8;
    param.end_accept_prob = 0.01;
    param.start_temp = 1000.0;           // 手動時・自動推定失敗時の開始温度。
    param.end_temp = 0.1;                // 手動時・自動推定失敗時の終了温度。
    param.acceptance_width_scale = 1.0;  // 0: 悪化拒否、+inf: 幅制限なし。
    param.enable_end_cost_check = true;  // LOCAL: get_cost() と内部コストを照合。
    param.enable_temperature_report = false; // -DLOCAL で true にすると、終了後に推奨温度を stderr へ出す。
    // 温度見積もり後は、出力された auto_mode/start_temp/end_temp の代入 3 行をここへ貼り付ける。
    // 推奨値は次回用。初期解で悪化操作がなくても、探索中に観測できれば見積もれる。

    auto get_snapshot = [&]() -> Snapshot {
        return {cur_solution}; // 初期時・最良更新時に必要な場合だけコピーされる。
    };
    auto get_cost = [&]() -> Cost {
        return compute_cost(input, cur_solution); // 独立した再評価で、適用ミスも診断できる。
    };

    // delta = 提案後コスト - 提案前コスト。負なら改善。ここでは状態を変えない。
    auto propose = [&](const sa::SaRuntime<Cost>& runtime) -> Cost {
        (void)runtime;
        // temperature / current_cost / best_cost / progress() を参照できる。
        pending_move = {};
        // TODO: rng で近傍を選び、pending_move に操作と delta を保存する。
        // 操作できないときは delta=0 の無操作にする。事前採取中も呼ばれる。
        return pending_move.delta;
    };
    auto finalize = [&](bool accepted) -> void {
        if (!accepted) return; // 事前採取では常に false。
        // TODO: pending_move を解とキャッシュへ適用する。無操作の場合は何も変えない。
        cur_cost += pending_move.delta;
    };

    sa::SaCsvStatHook<Cost> csv_hook{}; // LOCAL: sa_stat.csv、50ms ごとに記録して最後に保存。
    auto debug_hook = [&](sa::SaEventType event, const sa::SaRuntime<Cost>& runtime) -> void {
        csv_hook(event, runtime);
        // 必要なら診断を追加する。非 LOCAL では呼ばれない。
    };

    // 入力・初期解構築で消費した時間を差し引く。SA 内の初期評価・出力等にも余裕を取る。
    const double elapsed_ms = chrono::duration<double, milli>(chrono::steady_clock::now() - started).count();
    optional<Cost> existing_best_cost = nullopt; // 単独実行は空。マルチスタートなら保存済み全体ベストを渡す。
    auto result = sa::sa<Snapshot, Cost>(
        param, max(0.0, time_limit_ms - elapsed_ms),
        get_snapshot, get_cost, propose, finalize,
        existing_best_cost, debug_hook);

#ifdef LOCAL
    cerr << "best_cost = " << result.best_cost << '\n';
#endif
    // 現在解でなく、最良時点の Snapshot を出力する。基準未指定なら必ず存在する。
    // マルチスタートで空だった場合は、外側で保持している全体ベストをそのまま使う。
    if (result.best_snapshot) result.best_snapshot->print();
    return 0;
}
