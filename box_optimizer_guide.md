# BoxOptimizer v06 利用ガイド

有限な上下限を持つ連続変数について、任意の決定的な目的関数を最小化する、C++20のヘッダオンリーライブラリです。GCC 13.3で検証しました。標準ライブラリ以外の依存関係はありません。

AHCでは、配置、出力、速度、方策の係数などの調整や、離散構造を固定した後の部分改善に使います。順列・集合・一般の厳密制約を直接解くライブラリではありません。最大化する場合は、目的値と勾配の符号を反転してください。

## 導入

`box_optimizer_v06.hpp` を置き、次のようにincludeします。ヘッダをincludeしたときは、末尾のテストとmainは組み込まれません。

```cpp
#include "box_optimizer_v06.hpp"

int main() {
    BoxProblem problem{{{-3,3},{-3,3}}};
    std::vector<double> initial{-1.2,1};
    auto objective = [](std::span<const double> x) {
        double a = 1-x[0], b = x[1]-x[0]*x[0];
        return a*a+100*b*b;
    };
    auto result = box_optimize(problem,objective,
                               BoxBudget{2000,LLONG_MAX,{}},initial);
    if (result.has_value()) {
        // result.xとresult.costを利用する
    }
}
```

AHCの単一ファイルにまとめる場合は、同梱ツールを使えます。

```sh
python3 tools/amalgamate.py path/to/main.cpp > submission.cpp
g++ -std=c++20 -O2 submission.cpp -o submission
```

このツールは、main内の `#include "box_optimizer_v06.hpp"` を、直接実行用テストを除いた本体に置換します。相対パス付きincludeにも対応します。ヘッダ全文をそのままmain.cppへ貼ると、`__INCLUDE_LEVEL__ == 0` のテストmainも有効になるので、貼り付けにはこのツールか、テスト部分を除いた本体を使ってください。

## 勾配を渡す

目的関数を二引数にすると、既定設定では勾配を利用します。勾配は**元の座標・単位**で記入します。空のgradientには書き込みません。

```cpp
auto objective = [](std::span<const double> x,
                    std::span<double> gradient) {
    double a = 1-x[0], b = x[1]-x[0]*x[0];
    if (!gradient.empty()) {
        gradient[0] = -2*a-400*x[0]*b;
        gradient[1] = 200*b;
    }
    return a*a+100*b*b;
};
```

非空のgradientの長さは、固定変数を含む全変数数nです。全成分を書き込んでください。固定変数の成分は0で構いません。値と勾配を返す一回のコールバックは、評価1回・勾配評価1回と数えます。勾配法のラインサーチでも値と勾配を一緒に要求します。AutoのL-BFGS型の曲率履歴は最大8組です。SpectralGradientでは履歴を使いません。

一引数・二引数の両方で呼べるオブジェクトは、二引数を優先します。二引数形式を渡していても、勾配不要の手法を明示した場合や全変数固定の場合は、空のgradientで値だけを要求します。数値微分は自動実行しません。

## API

| API | 用途 | 再利用するもの |
|---|---|---|
| `BoxOptimizer(problem,param)` | 箱と設定を保持 | 前処理を作成 |
| `solve(objective,budget,initial)` | 新しい探索を開始 | 箱の前処理、作業領域の容量 |
| `resume(objective,budget)` | 同じ探索へ追加予算 | 最良点、乱数、探索履歴、未完了の候補・世代 |
| `reset(problem,param)` | 箱・設定を変更 | 保持できる作業領域の容量 |
| `box_optimize(problem,objective,budget,initial,param)` | 一度だけ実行 | 呼び出しごとに構築 |

`initial` は省略すると箱の中央です。指定した初期点は、正規化の往復による丸めを挟まず、最初のコールバックへそのまま渡します。初期値の長さ・範囲が不正ならassertです。初期値を自動的に切り詰める処理はありません。

`solve` は、以前の目的値・勾配・探索履歴・累計回数を破棄し、乱数を設定したseedに戻します。目的関数は保持しません。各呼び出しで渡すため、ラムダをsolverより長生きさせる必要はありません。

`BoxResult::x` は独立した所有配列です。次の実行後も保持できます。元の添字順・元の単位で、固定変数も含みます。

### BoxBudget

| フィールド | 既定値 | 意味 |
|---|---|---|
| `time_limit_us` | `1'950'000` | 一回の呼び出しの時間上限、単位はµs |
| `max_evaluations` | `LLONG_MAX` | 一回のコールバック回数の上限 |
| `target_cost` | 未指定 | この値以下の評価済み解で停止 |

```cpp
BoxBudget by_time{500,LLONG_MAX,{}};  // 500µs
BoxBudget by_count{-1,10000,{}};      // 時計による打ち切りを無効化
BoxBudget until_target{2000,5000,1e-8};
```

時間指定が負なら、有限の評価上限も指定してください。0なら新しい評価は行いません。評価回数上限は0以上です。予算は `resume` のたびに追加されます。

評価回数上限は厳密に守ります。時刻は初回と各評価の直前にsteady_clockで確認します。**コールバックを中断することはできません**。実行中の評価、内部の更新、結果作成の分だけ超過することがあります。µs指定は、µs精度で必ず終了する保証ではありません。

`solve` の計測には探索の初期化と返却用配列の作成を含み、コンストラクタと `reset` は含みません。`box_optimize` は構築・探索・solverの破棄まで計測します。呼び出し側の丸め、採点、出力、I/Oには別に時間を残してください。

### BoxParam

| フィールド | 既定値 | 意味 |
|---|---|---|
| `search` | `Search::Explore` | 停滞後も探索を続ける |
| `seed` | `1` | solver専用の乱数seed |
| `initial_step` | 空 | 初期の探索幅、指定するなら長さn・元の単位 |
| `method` | `Method::Auto` | 手法の選択 |

`Search::Refine` は、与えた初期点の改善を優先し、局所的な停止で終了します。全域への再始動はしません。初期点からの距離制限ではないので、動かせる範囲を限定する場合は箱を狭めます。

`initial_step` の自由成分は、正・有限・箱幅以下にしてください。固定成分は無視します。空なら正規化座標で0.25を使います。勾配法では最大の正規化幅を初回のステップ尺度に使います。成分ごとの最大移動距離を保証する指定ではありません。

| Method | 動作 |
|---|---|
| `Auto`、勾配あり | 投影付きL-BFGS型。Exploreでは初期逆ヘッセ尺度にBB1/BB2の幾何平均を使い、再始動も行う。RefineではBB2尺度を使う |
| `Auto`、勾配なし、Explore | 座標探索を起点に、停滞後に焼きなましと対角CMA系を挟む |
| `Auto`、勾配なし、Refine | 座標探索で局所改善する |
| `Coordinate` | 座標探索。Exploreでは別の初期点から再始動する |
| `DiagonalCma` | 対角の共分散適応。Exploreでは停滞後に再始動する |
| `SpectralGradient` | BB1尺度の投影勾配法。二引数の目的関数が必要（全固定を除く）。Exploreは再始動、Refineは局所停止 |

対角CMA系では、通常の候補の次に、その候補を平均点に対して反転して箱内へ収めた候補を評価します。世代の途中や候補の間で停止しても、resumeでそのまま続けられます。

未知の関数に常に最適な自動選択はできないため、有効だった手法を明示的にも選べる形にしています。DE、密なCMA、近傍比率、温度、履歴数などの公開設定は増やしていません。

短い予算ではAutoが座標探索から始める利点があります。一方、今回の10ms比較には、対角CMA単独がAutoを上回る条件もあります。連動した多峰関数などでは `DiagonalCma` も同じ予算で比較してください。正しい勾配があるなら、まずAutoを試す構成です。

短いターンで勾配付きの探索を繰り返す場合は、`SpectralGradient`も比較できます。最大8組の曲率履歴を使わず、方向作成O(d)で投影・ラインサーチ・再始動を行います。

```cpp
BoxParam param;
param.method = BoxParam::Method::SpectralGradient;
BoxOptimizer solver(problem,param);
auto result = solver.solve(objective,BoxBudget{2000,LLONG_MAX,{}},initial);
```

0.5～10msの今回の比較では大きな改善がありましたが、200ms～1.8秒の配置問題ではAutoより悪化しました。時間の長短だけで優劣が決まるわけではなく、利用する目的関数と予算で比較してください。局所解の精度を重視するRefineでは、まず既定のAutoを使います。時間に応じた自動切替はしません。自由変数があるのに一引数の目的関数をSpectralGradientへ渡すとassertです。

### BoxResult

| フィールド | 意味 |
|---|---|
| `x`, `cost` | 実際に評価した最良点と、その目的値 |
| `evaluations` | 今回のコールバック回数 |
| `gradient_evaluations` | 今回の勾配要求回数。evaluationsの内数 |
| `total_evaluations` | 直近のsolveからの累計回数 |
| `elapsed_us` | 今回の実測時間 |
| `stop` | 停止理由 |
| `has_value()` | 評価済みの解があるか |

`solve`・`resume`・`box_optimize`が返す`BoxResult`は、そのまま`cout`や`cerr`へ渡せます。

```cpp
std::cerr << result << '\n';
```

次のように、目的値、終了理由、評価回数、経過時間、解を1行で表示します。数値は表示形式を示す例です。

```text
BoxResult{cost=0.25, stop=EvaluationLimit, evaluations=200, gradient_evaluations=0, total_evaluations=200, elapsed_us=120.5, x=[2.5]}
```

`x`は先頭20要素まで表示し、超えた場合は末尾に`... (25 total)`のように全要素数を示します。空配列は`[]`、未評価の目的値は`cost=none`です。結果のデータ自体は省略・変更しません。改行は自動で付かないため、例のように`'\n'`を付けます。数値の精度などは出力先の設定に従い、`std::setprecision`も使えます。提出用の標準出力と混ざらないよう、デバッグには通常`cerr`を使います。

| Stop | 意味 |
|---|---|
| `TargetReached` | 評価済み最良値がtarget_cost以下 |
| `TimeLimit` | 時間上限 |
| `EvaluationLimit` | 評価回数上限 |
| `LocalStop` | 指定した局所探索が停止。最適性の証明ではない |
| `AllFixed` | 自由変数がなく、初回評価が完了 |

同じ判定地点では、到達済み目標が優先します。時間と評価回数が同時に尽きた場合はEvaluationLimitです。全固定・局所停止が既に確定している場合は、それぞれの終了理由を返します。

一度も評価していない結果は `cost=+infinity`、xは空、has_valueはfalseです。変数数0の正当な問題もxが空になるので、`x.empty()` で有効性を判定しないでください。全変数固定・変数数0でも、予算があれば一度は目的関数を呼びます。

同値の最良点は先に評価した点を維持します。ラインサーチで不採用の点や、途中で止まった世代の子個体も、最良点の候補になります。未評価の平均や推定点は返しません。

## 再開とターン制

同じ目的関数を続ける場合です。

```cpp
BoxOptimizer solver(problem,param);
auto result = solver.solve(objective,BoxBudget{500,LLONG_MAX,{}},initial);
result = solver.resume(objective,BoxBudget{500,LLONG_MAX,{}});
```

`resume` は、目的関数・捕捉データ・箱・設定・一引数/二引数の形式が同じ場合だけ使います。値の同一性を自動判定する仕組みはありません。目標到達後でも、目標を下げるか外して再開できます。LocalStop後は、その探索状態のまま追加評価は行いません。

需要やグラフの重みが変わった場合は、新しい `solve` を行います。

```cpp
std::vector<double> previous;
for (int turn = 0; turn < turn_count; ++turn) {
    // 需要やグラフを更新する
    auto result = solver.solve(objective,
                               BoxBudget{500,LLONG_MAX,{}},previous);
    if (result.has_value()) previous = result.x;
}
```

古い目的値と曲率は捨て、前解と作業領域を利用します。箱や固定変数が変わる場合はresetし、前解を新しい箱へ利用側で調整してから渡してください。

非常に短いresumeを大量に繰り返すと、時計確認と結果の所有配列の作成が相対的に重くなります。評価1回ずつの再開でも進捗は保ちますが、実用上はまとめて予算を与える方が効率的です。

## 問題への組み込み

グラフやシミュレータはラムダ側で保持します。専用のグラフ表現へ変換する必要はありません。

大きい状態の一部だけを変更するときは、元の添字を保ちたいなら固定変数を使い、評価を軽くしたいなら自由変数だけの短いベクトルと影響項だけの目的関数を用意します。固定変数を除いても、利用側が全グラフを毎回走査すれば、その評価費用は減りません。固定部分の和と、変更部分に接続する辺を前計算する使い方が有効です。共有辺を二重計上しないでください。

整数化・修復した提出値を直接評価する場合は、`actual_cost(decode(x))` を一引数の目的関数にします。滑らかな代理関数を使う場合は、元の提出解を外側で保持し、変換後の実得点を確認して採否を決めます。代理関数の改善だけでは提出得点の改善を保証できません。

シミュレータは、各候補で同じ固定シナリオ群を評価してください。候補ごとに外部乱数を進めると、同じxに同じ値を返すという契約を満たしません。シナリオ群を更新した場合も新しいsolveです。

正の値を桁違いに動かしたい場合は対数変数、角度は前解を中心とする連続した区間、総量制約は制約を満たす変数変換で表現できます。勾配も変換した変数に合わせます。solver自体は一般制約や周期境界を管理しません。

## 計算量と実装範囲

nは全変数数、dは自由変数数、mは履歴数8、λはCMAの子個体数で、おおよそ `4+floor(3*log(d))` です。

| 経路 | 主な内部計算、目的評価を除く | 保持メモリの目安 |
|---|---|---|
| 座標探索 | 通常の候補作成・単位変換・受理はO(1)、幅の全走査や巡回処理はO(d) | O(n+d) |
| 焼きなまし | 候補ごとにO(d) | O(n+d) |
| 投影L-BFGS型 | 方向作成O(md)、候補変換O(d) | O(n+md) |
| スペクトル勾配 | 方向作成O(d)、候補変換O(d) | O(n+d) |
| 対角CMA系 | 1世代O(λd+λ log λ) | O(n+λd) |
| 全経路共通 | 初期化・結果のコピーO(n)、最良点更新O(n+d) | 過去に確保した容量を保持可能 |

勾配用の配列は、最初に勾配を要求する評価のときに準備します。勾配なしの実行では新規に確保しません。SpectralGradientではL-BFGS用の履歴配列を新規に確保しません。過去に確保した容量は、再利用のために保持します。

本体は `double` と通常の整数型を使い、long double・__int128・直接のAVX・std::function・virtual・detail namespaceは使っていません。公表した結果は1スレッドです。探索用の補助定義はBoxOptimizerの内部に置いています。

入力はn≤100000、自由変数d≤10000、solveからの累計評価回数≤10億の範囲を想定します。これは性能を保証するサイズではありません。速度比較で実測した自由変数数は最大1024です。上下限の差・逆数、正規化幅の二乗、勾配変換、内積などを、通常のdoubleで無理なく表現できる尺度にしてください。極端な値のための型拡張や特殊処理はありません。目的値は箱内・境界上で有限、勾配を渡す場合も全成分有限という契約です。

任意のコールバックの実行時間は制御できず、大域最適性も保証しません。勾配は利用者が正しさを保証します。入力不正の扱いはassertで、例外による回復処理は追加していません。目的関数内で入力spanを変更・保持したり、同じsolverへ再入したりしないでください。

## 検証と再現

```sh
make check CXX=g++
make examples CXX=g++
make quality CXX=g++
make perf CXX=g++
python3 tools/token_count_lite.py box_optimizer_v06.hpp --json
```

`make check` はデバッグ、NDEBUG、ASan/UBSan、assert拒否ケース、複数翻訳単位でのincludeを確認します。NTCは添付のntc-lite-v1で計測でき、Clangは不要です。

v06はv05の動作・APIを保った簡素化版です。全文レビューの範囲、削除した重複処理、回帰・性能検証は `box_optimizer_review_v06.md` を参照してください。

速度比較・問題生成器・CSVはexperimentsとresultsにあります。測定条件、全候補の採否、時間制限下の解品質は `box_optimizer_performance_v05.md` を参照してください。コンテストの公式問題を実測した結果ではなく、数理関数と自作のAHC風シナリオによる評価です。
