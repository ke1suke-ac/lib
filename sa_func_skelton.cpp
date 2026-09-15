// 単独 SA のスケルトン。近傍は propose で決め、finalize(true) で適用する
// TODO を問題に合わせて実装する。未編集では空の解・コスト 0 を探索するだけ
#include <bits/stdc++.h>
#include "sa_func_v12.hpp"  // 利用するヘッダ名に合わせる
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
};

// 解全体から絶対コストを再計算する O(C)、C は問題固有の評価処理量
Cost compute_cost(const Input& input, const vector<int>& solution) {
    (void)input;
    (void)solution;
    // TODO: input と solution から計算する。差分で更新したキャッシュは使わない
    return 0;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // 入力・初期解構築・出力は SA の時間予算外なので、制限時間には余裕を取る
    Input input;
    // TODO: 入力を読む
    // cin >> input.n;

    // 近傍生成用の乱数はユーザー側で用意する。param.seed は SA の受理判定用
    constexpr uint64_t seed = 1;
    [[maybe_unused]] mt19937_64 rng(seed);
    vector<int> cur_solution;
    // TODO: input と rng を使って初期解を構築する
    Cost cur_cost = compute_cost(input, cur_solution);
    Move pending_move;

    // SA パラメータ。以下は既定値で、必要な項目だけ変更する
    sa::SaParam param;
    param.seed = seed;
    param.auto_mode = true;
    param.samples = 300;
    param.start_accept_prob = 0.8;
    param.end_accept_prob = 0.01;
    param.start_temp = 100.0;  // 手動時、および自動推定失敗時に使用
    param.end_temp = 1.0;
    param.enable_end_cost_check = true;  // LOCAL 時の終了診断だけを制御する

    // 初期時と最良更新時だけ、現在の解を値としてコピーする
    auto get_snapshot = [&]() -> Snapshot {
        return {cur_solution};
    };

    // 差分計算と独立に再評価することで、LOCAL の終了診断で更新ミスを検出する
    // cur_cost をそのまま返すと、解本体への適用・巻き戻しミスを検出できない場合がある
    auto get_cost = [&]() -> Cost {
        return compute_cost(input, cur_solution);
    };

    // 近傍を決め、delta = 提案後コスト - 提案前コストを返す。ここでは適用しない
    auto propose = [&](const sa::SaRuntime<Cost>& runtime) -> Cost {
        (void)runtime;
        // runtime.temperature / current_cost / best_cost / progress() を参照できる
        // progress() は 0～1。時計は追加取得せず、探索中は 32 反復ごとの計測値を使う
        pending_move = {};

        // TODO: rng で近傍を選び、pending_move.l / r / delta を設定する
        // 近傍がない場合は delta=0 の無操作とし、finalize でも何も変更しない
        // 自動温度推定中も呼ばれ、その直後は必ず finalize(false) となる
        return pending_move.delta;
    };

    // 受理されたときだけ、解・評価用キャッシュ・現在コストを更新する
    auto finalize = [&](bool accepted) -> void {
        if (!accepted) return;

        // TODO: pending_move を cur_solution と評価用キャッシュへ適用する
        // 例: 有効な区間 [l, r] を反転する近傍なら、
        // if (pending_move.l >= 0) reverse(cur_solution.begin() + pending_move.l,
        //                                 cur_solution.begin() + pending_move.r + 1);
        cur_cost += pending_move.delta;
    };

    // 既定設定: sa_stat.csv に 50ms ごとに記録し、終了時にまとめて書き出す
    sa::SaCsvStatHook<Cost> csv_hook{};
    auto debug_hook = [&](sa::SaEventType event_type, const sa::SaRuntime<Cost>& runtime) -> void {
        csv_hook(event_type, runtime);
        // TODO: 必要なら追加の診断を行う。非 LOCAL ではこのラムダ自体が呼ばれない
    };

    // 全引数を明示。返るのは最良時点の Snapshot であり、終了時の cur_solution ではない
    auto [best_cost, best_snapshot] = sa::sa<Snapshot, Cost>(
        param,
        1950.0,  // time_limit_ms: 小数ミリ秒も指定可能
        get_snapshot,
        get_cost,
        propose,
        finalize,
        debug_hook
    );

#ifdef LOCAL
    cerr << "best_cost = " << best_cost << '\n';
#endif
    best_snapshot.print();
    return 0;
}
