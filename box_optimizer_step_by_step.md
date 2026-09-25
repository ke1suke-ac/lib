# BoxOptimizer ステップバイステップガイド

対象：`box_optimizer_v06.hpp`。C++20 / GCC 12.2で動作確認した、外部Markdownリーダー向けの学習用ガイドです。

必要な数学は、高校で扱う関数・二次関数・微分・確率を出発点にします。複数変数の微分や行列が必要になった箇所では、その意味から説明します。前半は使い方に集中し、内部アルゴリズムは最後の付録で扱います。

このガイドの目的は、「目的関数をどう書き、何をライブラリへ渡し、返された結果をどう使うか」を順番に理解することです。関数の式から答えが分かる小さな例も、動作を確かめやすい教材として使います。

## 1. 何を解くライブラリか

### 1.1 値を小さくする設定を探す

例えば、ある装置の出力を0から10の範囲で調整できるとします。出力を3に近づけたいなら、「3からのずれの二乗」を小さくすればよいでしょう。

$$f(x)=(x-3)^2,\qquad 0\le x\le 10$$

ここで、$x$は調整する出力、3は目標値、$f(x)$はその出力の悪さを表す数値です。二乗するのは、目標より大きくても小さくても悪さを正の値として数えるためです。$x=2$なら悪さは1、$x=5$なら4、$x=3$なら0です。

この「小さくしたい数値を計算する関数」を**目的関数**と呼びます。本ライブラリは、値が小さいほど良いという約束で動きます。

### 1.2 複数の設定値を同時に扱う

一般には、次の問題を扱います。添字はC++に合わせて0から始めます。

$$\min_ {x} f(x),\qquad x=(x_ {0},x_ {1},\ldots,x_ {n-1}),\qquad l_ {i}\le x_ {i}\le u_ {i}\quad (i=0,\ldots,n-1)$$

式に登場する記号は次の意味です。

| 記号 | 意味 |
|---|---|
| $n$ | 設定値の個数 |
| $i$ | 何番目の設定値かを表す番号 |
| $x_ {i}$ | その番号の設定値。小数を含む実数として扱う |
| $x$ | 全設定値を順番に並べた配列 |
| $l_ {i}$ | その設定値の下限 |
| $u_ {i}$ | その設定値の上限 |
| $f$ | 設定値の配列を受け取り、悪さを1個の数値で返す目的関数 |
| $\min$ | 条件を守りながら、その右側の値をできるだけ小さくするという意味 |

2変数なら許される範囲は長方形、3変数なら直方体になります。このように、各変数の上下限を独立に指定する制約を**箱制約**と呼びます。下限と上限が同じ変数は、その値に固定できます。

例えば、2つの設定値をそれぞれ2と−1へ近づけ、さらに合計を1へ近づけたいなら、次の目的関数を作れます。

$$f(x_ {0},x_ {1})=(x_ {0}-2)^2+4(x_ {1}+1)^2+\frac{1}{2}(x_ {0}+x_ {1}-1)^2$$

最初の項は1番目の設定値のずれ、2番目の項は2番目の設定値のずれです。係数4は、同じ大きさのずれでも2番目を4倍重く数える指定です。最後の項は合計のずれで、係数1/2はその重みです。目標値2、−1、合計1は利用者が決めた定数で、探索する変数ではありません。

この例では両変数が最後の項で結び付いています。一般に、設定値を1個変えたときの良し悪しが、ほかの設定値にも左右される問題を扱えます。

### 1.3 利用者が渡すもの、受け取るもの

利用者が用意するのは、各変数の上下限と、設定値から目的値を計算するC++の関数です。必要なら、初期解や使ってよい時間なども指定します。

ライブラリが返すのは、**実際に目的関数で評価した点のうち、最も目的値が小さかった点**と、その目的値です。一般の非線形関数では、全範囲で最も良い点を必ず見つける保証はありません。まず実用的な時間内で良い点を得るための道具です。

例えば、限られた時間内で良い解を競うAtCoder Heuristic Contest（AHC）では、配置の座標、速度や出力、評価式の係数、固定した離散構造に付随する連続パラメータなどに使えます。順列や集合そのものを直接扱うインターフェースはありません。

目的関数は、同じ設定値に対して同じ値を返す必要があります。値と上下限は有限の数値にします。目的関数を呼ぶたびに新しい乱数を使う評価は、そのままではこの約束に合いません。固定したシナリオを使う例を後で示します。

## 2. 本ライブラリを使う前に、専用の解き方を考える

### 2.1 条件が限られると、もっと確実に解ける

汎用的な探索を使う前に、問題の形を確認します。

| 問題の形 | 先に検討する解き方 | 判断の理由 |
|---|---|---|
| 1変数で、正の二次係数を持つ二次関数 | 頂点を計算し、上下限へ収める | 式から最良値を直接決められる |
| 各変数の二乗誤差の和で、変数同士が結び付かない | 各変数を個別に最適化する | まとめて探索する必要がない |
| 目的関数が一次式で、制約が箱だけ | 係数の符号に応じて上下端を選ぶ | 各変数の最良端点を直接決められる |
| 目的関数も追加の制約も一次式 | 線形計画法（LP）の専用ソルバー | 構造を使って大域的な最適解を求める問題として扱える |
| 目的関数が凸な二次関数、制約が一次式 | 凸二次計画法（QP）の専用ソルバー | 最適性を評価しながら、指定した数値精度まで解く仕組みを使える |
| 1変数で、区間内に1つの谷しかない | 黄金分割探索などの1変数用手法 | 多変数向けの仕組みを持つ必要がない |
| 変数が本質的に整数・0/1・順列 | 整数計画、動的計画法、問題別の探索 | 実数を最後に丸めるだけでは、離散的な構造を十分に利用できない |

「凸」とは、ここでは大まかに、途中に別の谷が入り込まない、お椀のような形と考えてください。条件を満たす凸最適化では、局所的に最良の点が全体でも最良になります。ただし、本ライブラリが関数の凸性を判定したり、最適性を証明したりするわけではありません。

例えば、正の定数$a$と定数$b$に対する次の問題は、$b$を区間内へ収めれば解けます。

$$f(x)=a(x-b)^2,\qquad x^{\ast}=\min(u,\max(l,b)),\qquad a>0$$

$l,u$は上下限、$x^{\ast}$は最適な設定値です。$b$が区間内なら$b$、下限より小さければ$l$、上限より大きければ$u$を選びます。後の最小例も、このように正解が分かる関数を教材として使います。

なお、専用ソルバーの「厳密解」と、浮動小数点で誤差ゼロの値が出ることは別です。LPや凸QPでは、数学的には大域最適解を求める問題として扱えても、実際の計算は許容誤差や終了状態を確認します。OR-ToolsのLP例、CVXPYの凸QP例が参考になります。[資料1]、[資料2]

### 2.2 類似のソルバーとの使い分け

| 選択肢 | 向いている場面 | 本ライブラリとの違い |
|---|---|---|
| BoxOptimizer | C++の目的関数をそのまま渡し、有限な箱内で短時間の改善を行いたい | 標準ライブラリだけのヘッダ。一般の追加制約を受け取るAPIはない |
| SciPyの`minimize` | Python上で、勾配の有無や制約に合わせて手法を比較したい | 手法ごとに対応する制約が異なる。L-BFGS-Bなどがある [資料3] |
| SciPyの`differential_evolution` | 勾配なしで、複数の谷を持つ関数を広く探す候補も比較したい | 集団を使う手法。一般に多くの目的評価を要し得る [資料4] |
| NLopt | C/C++などで、多様な非線形最適化手法や追加制約を使いたい | 選択する手法に応じて等式・不等式制約にも対応する外部ライブラリ [資料5] |
| 自分で近傍を設計する焼きなまし | 順列・集合・離散状態や、高速な差分評価を直接扱いたい | 状態変更や元に戻す処理を自分で定められる。本ライブラリは設定値全体を目的関数へ渡す |

これは機能と組み込み方による使い分けであり、速度順位を実測した表ではありません。外部ライブラリを利用する場合は、提出先の言語環境で使えるかも確認してください。

BoxOptimizerの勾配経路は投影を組み合わせたL-BFGS型ですが、SciPyのL-BFGS-Bと同一実装ではありません。同名に近い用語があっても、結果や停止条件まで同じとは考えないでください。

## 3. 読み進め方と実行準備

### 3.1 学習順序

| 段階 | 新しく学ぶ内容 |
|---|---|
| 01〜04 | 最小実行、初期解、複数変数、目標値 |
| 05〜09 | 結果の読み方、再開、時間制限、探索方針、初期幅 |
| 10〜15 | 固定変数、勾配、勾配検算、各探索手法 |
| 16〜18 | ターンごとの再計算、箱の変更、全固定・空の問題 |
| 19〜23 | 最大化、整数化、代理目的、制約の変数変換、固定シナリオ |
| 24〜25 | 部分問題の評価削減、外側の締切を共有する複数実行 |
| 付録 | APIの一覧、実装の全体像と各アルゴリズムの詳細 |

各段階に**完全な1本のプログラム**を載せます。前のコードを貼り合わせる必要はありません。変更点は本文で示しますが、コードはその段階のものだけでコンパイルできます。

### 3.2 ヘッダとコードを同じフォルダへ置く

`box_optimizer_v06.hpp`と、試したいコードを保存した`main.cpp`を同じフォルダに置きます。ターミナルで次を実行します。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined main.cpp -o main
./main
```

入力ファイルは不要です。各例の問題データはコード内にあります。同梱の例集では、`step01_minimal.cpp`のような名前で保存してあります。その場合はコンパイル対象の`main.cpp`を置き換えます。

例では`using namespace std;`により、`std::vector`を`vector`などと短く書きます。`double`は小数を扱う型です。`cout`は結果を画面へ表示します。ヘッダが`bits/stdc++.h`を読み込むため、例側に標準ヘッダの一覧を追加する必要はありません。

ヘッダは`#include`で使ってください。ヘッダ全文をそのまま`main.cpp`へ貼ると、末尾の直接実行用テストの`main`も有効になります。1ファイルにまとめる方法は最後に説明します。

## 4. Step 01：1変数の最小コード

**目的：出力を0〜10の範囲で動かし、3へ近づけます。**

保存名：`step01_minimal.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}};

    auto objective = [](span<const double> x) -> double {
        double d = x[0] - 3.0;
        return d * d;
    };

    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 200;

    auto result = box_optimize(problem, objective, budget);
    if (!result.has_value()) return 0;
    cout << "x = " << result.x[0] << '\n';
    cout << "cost = " << result.cost << '\n';
}
```

### 4.1 BoxProblemは「動かせる範囲」

`problem.bounds`は、各変数の`{下限, 上限}`を並べた配列です。`{{0.0, 10.0}}`は要素が1個なので、変数は1個です。内側の`{0.0, 10.0}`が`BoxBound`という1変数分の境界を表します。

`bounds`の長さから変数数が決まるため、別に「変数数1」を指定する引数はありません。0と10の両端も評価の対象に含まれます。

### 4.2 objectiveは「候補を採点する関数」

`[](span<const double> x) -> double { ... }`は、名前を付けて変数へ保存した小さな関数、C++のラムダです。ここでは`objective`という名前にしています。

`span<const double>`は、配列を読み取るための窓口です。配列全体をコピーせずに受け取ります。`const`が付いているので、候補をこの関数から書き換えません。今回の要素は1個なので、`x[0]`が出力値です。

`d`は出力と目標3の差です。`d * d`はその二乗で、関数の戻り値が目的値になります。式の文字列を渡すのではなく、**候補を受け取ったら数値を計算して返す処理**を渡しています。

候補の配列はライブラリ側の作業領域です。`span`自体を保存して後から使わないでください。保存が必要なら、自分の`vector`などへ値をコピーします。

### 4.3 budgetは「使ってよい計算量」

このコードでは、次の2項目だけ変更しています。

| 設定 | 今回の値 | 意味 |
|---|---:|---|
| `time_limit_us` | −1 | 時間による終了を使わない |
| `max_evaluations` | 200 | 目的関数を呼ぶ回数を最大200回にする |

1回の目的評価とは、`objective`を1回呼ぶことです。200回の反復や200世代という意味ではありません。ライブラリ内部の処理量を数えているわけでもありません。

時間制限を無効にする場合は、この例のように有限の評価回数上限を必ず指定します。`BoxBudget`には目標値による終了設定もありますが、既定では未指定なので今は考えなくて構いません。

### 4.4 box_optimizeは「1回だけ解く」

引数は順に、問題、目的関数、予算です。初期解を省略しているため、この例では箱の中央、つまり5から始まります。

戻り値の`result.x`は見つけた設定値の配列、`result.cost`はその設定値の目的値です。典型的には`x`が3の近く、`cost`が0の近くになります。小数点以下の末尾が完全に3や0にならなくても、それだけで誤りではありません。

`has_value()`は、「少なくとも1回評価できたか」を表します。時間や評価回数を使い切って1回も評価していない結果には解が入らないため、配列へアクセスする前に確認する習慣を付けます。

この例は、最小限の指定で使い方を覚えるためのものです。実際にこの二次関数だけを解くなら、前節の式から直接3を選べます。

## 5. Step 02：初期解を指定し、solverオブジェクトを使う

**変更点：出力9を出発点として指定します。後で再開できる書き方にも変更します。**

保存名：`step02_initial.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}};
    auto objective = [](span<const double> x) -> double {
        double d = x[0] - 3.0;
        return d * d;
    };
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 200;
    vector<double> initial = {9.0};

    BoxOptimizer solver(problem);
    auto result = solver.solve(objective, budget, initial);
    if (!result.has_value()) return 0;
    cout << "initial = " << initial[0] << '\n';
    cout << "best = " << result.x[0] << '\n';
    cout << "cost = " << result.cost << '\n';
}
```

`initial`は最初に評価する設定値です。候補の並び順と同じ長さの`vector<double>`を渡します。今回は1変数なので`{9.0}`です。値は指定した箱の中にある必要があります。

`BoxOptimizer solver(problem)`は、問題を持つ探索オブジェクトを作ります。`solver.solve(objective, budget, initial)`が新しい探索の開始です。引数は順に、目的関数、予算、初期解です。問題はコンストラクタへ渡したため、`solve`には渡しません。

初期解は初回にそのまま目的関数へ渡されます。ライブラリ内部の座標変換で丸め直してから渡すわけではありません。初期解の評価も200回のうちの1回です。確認のため`max_evaluations`を1に変えると、最初の評価だけなので`best=9`、`cost=36`になります。

`solve`をもう一度呼ぶと、前回の探索を続けるのではなく、目的値・探索履歴・累計評価回数をリセットして新しく始めます。作業配列の容量は再利用できるため、オブジェクトを作り直す必要はありません。

1回だけ使う`box_optimize`でも、第4引数へ`initial`を渡せます。使い方を先に増やしすぎないため、以降は基本的に`solver.solve`へ揃えます。

## 6. Step 03：2変数と、結び付いた目的関数

**変更点：1個の出力から、互いに関係する2個の設定値へ拡張します。**

保存名：`step03_two_variables.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}, {-5.0, 5.0}};
    auto objective = [](span<const double> x) -> double {
        double a = x[0] - 2.0;
        double b = x[1] + 1.0;
        double c = x[0] + x[1] - 1.0;
        return a * a + 4.0 * b * b + 0.5 * c * c;
    };
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 2000;
    vector<double> initial = {8.0, 4.0};

    BoxOptimizer solver(problem);
    auto result = solver.solve(objective, budget, initial);
    if (!result.has_value()) return 0;
    cout << result.x[0] << ' ' << result.x[1] << '\n';
    cout << "cost = " << result.cost << '\n';
}
```

目的関数は冒頭で説明したものです。`a`、`b`、`c`はそれぞれ、1番目の目標からのずれ、2番目の目標からのずれ、合計の目標からのずれです。

| 変更した値 | 意味 |
|---|---|
| `bounds[0] = {0, 10}` | `x[0]`を0〜10で動かす |
| `bounds[1] = {-5, 5}` | `x[1]`を−5〜5で動かす |
| `initial = {8, 4}` | 最初の候補を`x[0]=8, x[1]=4`にする |
| 評価上限2000 | 教材の2変数を十分に改善するための上限。変数数から自動決定される値ではない |

結果の`x[0]`と`x[1]`は、入力と同じ並び順です。今回の正解は2と−1で、3つのずれがすべて0になります。最適値があらかじめ分かるので、自分の目的関数の書き方を確認できます。

`x[0]`を変えると、最初の項と最後の項が変わります。変数同士の関係は、利用者が`objective`の中に書きます。ライブラリへ関係式の一覧を別に登録する必要はありません。

## 7. Step 04：十分良くなったら終了する

**変更点：目的値が0.00000001以下になったら、予算を使い切る前に終了します。**

保存名：`step04_target.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}, {-5.0, 5.0}};
    auto objective = [](span<const double> x) -> double {
        double a = x[0] - 2.0;
        double b = x[1] + 1.0;
        double c = x[0] + x[1] - 1.0;
        return a * a + 4.0 * b * b + 0.5 * c * c;
    };
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 2000;
    budget.target_cost = 1e-8;
    vector<double> initial = {8.0, 4.0};

    BoxOptimizer solver(problem);
    auto result = solver.solve(objective, budget, initial);
    if (!result.has_value()) return 0;
    cout << "cost = " << result.cost << '\n';
    cout << "evaluations = " << result.evaluations << '\n';
    cout << "target_reached = "
         << (result.stop == BoxResult::Stop::TargetReached) << '\n';
}
```

`1e-8`はC++の指数表記で、$10^{-8}$です。`target_cost`を設定すると、「評価済みの最良目的値がこの値以下」という条件で終了します。この例の出力`target_reached`は、条件を満たしたら1です。

これは「真の最適値との差が1e-8以下」という精度指定ではありません。ライブラリは真の最適値を知りません。今回は最適値が0と分かっているから、0に近い小さな値を目標にできるだけです。

目的関数に定数100を加えると、同じ解の良し悪しでも目的値の尺度が変わります。そのときは目標値も100に合わせて変更します。目的値が負になる関数にも、通常の「以下」の比較を使います。

`target_cost`の型は`optional<double>`です。「数値が入っている状態」と「未指定の状態」があります。既定は未指定です。設定後に外したい場合は`budget.target_cost.reset()`、または`budget.target_cost = nullopt`とします。

目標が厳しすぎて到達しない場合でも、最大2000回という上限は有効です。

## 8. Step 05：結果と終了理由を読み取る

**変更点：評価回数0で開始した場合と、通常に評価した場合を比較します。**

保存名：`step05_result.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

const char* stop_name(BoxResult::Stop stop) {
    switch (stop) {
        case BoxResult::Stop::TargetReached: return "TargetReached";
        case BoxResult::Stop::TimeLimit: return "TimeLimit";
        case BoxResult::Stop::EvaluationLimit: return "EvaluationLimit";
        case BoxResult::Stop::LocalStop: return "LocalStop";
        case BoxResult::Stop::AllFixed: return "AllFixed";
    }
    return "Unknown";
}

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}};
    auto objective = [](span<const double> x) -> double {
        double d = x[0] - 3.0;
        return d * d;
    };
    BoxOptimizer solver(problem);
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 0;

    auto empty_result = solver.solve(objective, budget);
    cout << "first_has_value = " << empty_result.has_value() << '\n';
    cout << "first_stop = " << stop_name(empty_result.stop) << '\n';

    budget.max_evaluations = 200;
    auto result = solver.solve(objective, budget);
    if (!result.has_value()) return 0;
    cout << "x = " << result.x[0] << '\n';
    cout << "cost = " << result.cost << '\n';
    cout << "evaluations = " << result.evaluations << '\n';
    cout << "gradient_evaluations = " << result.gradient_evaluations << '\n';
    cout << "total_evaluations = " << result.total_evaluations << '\n';
    cout << "elapsed_us = " << result.elapsed_us << '\n';
    cout << "stop = " << stop_name(result.stop) << '\n';
}
```

### 8.1 解が入っていない結果も正常な結果

最初の`max_evaluations = 0`では目的関数を呼べません。したがって`first_has_value`は0、終了理由は`EvaluationLimit`です。

この場合の`x`は空、`cost`は正の無限大です。無限大を目的関数が返したのではなく、「まだ評価済みの解がない」という結果の表現です。

2回目は新しい`solve`なので、評価上限200で最初から解き直します。この例では目標値を設定せず、既定の探索方針で続けるため、通常は`evaluations=200`になります。

### 8.2 BoxResultの全項目

| 項目 | 読み方 |
|---|---|
| `x` | 実際に評価した最良点。元の単位・元の添字順の所有配列 |
| `cost` | その点の目的値 |
| `evaluations` | 今回の`solve`または`resume`で目的関数を呼んだ回数 |
| `gradient_evaluations` | 今回、勾配も要求した回数。後で説明する機能で、この例では0 |
| `total_evaluations` | 直近の`solve`からの累計評価回数 |
| `elapsed_us` | 今回の呼び出しの実測時間。単位はマイクロ秒 |
| `stop` | どの条件で終了したか |
| `has_value()` | 評価済みの解があるか |

勾配評価は目的評価の内数です。例えば`evaluations=100, gradient_evaluations=100`でも、関数呼び出しが合計200回という意味にはなりません。

### 8.3 Stopの全種類

| 終了理由 | 意味 |
|---|---|
| `TargetReached` | 最良目的値が指定した目標以下になった |
| `TimeLimit` | 時間上限に到達した |
| `EvaluationLimit` | 今回の評価回数上限に到達した |
| `LocalStop` | その探索が局所的な終了条件に到達した |
| `AllFixed` | 動かせる変数がなく、初回評価が終わった |

`stop_name`は、この列挙値を表示用の文字列へ置き換えるだけの自作関数です。ライブラリを使うための必須部品ではありません。

`LocalStop`は「もう少し近くを調べても変化しにくい、と実装の条件が判断した」という意味で、全体最適解の証明ではありません。

同じ判定時点で複数の条件が成立すると、目標到達、全固定、局所停止、評価回数、時間の順に優先されます。例えば最後の1回で目標に達した場合は、`EvaluationLimit`より`TargetReached`が優先されます。

## 9. Step 06：同じ問題へ追加予算を与える

**変更点：10回だけ評価して一度戻り、その続きを190回の予算で再開します。**

保存名：`step06_resume.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}, {-5.0, 5.0}};
    auto objective = [](span<const double> x) -> double {
        double a = x[0] - 2.0;
        double b = x[1] + 1.0;
        double c = x[0] + x[1] - 1.0;
        return a * a + 4.0 * b * b + 0.5 * c * c;
    };
    vector<double> initial = {8.0, 4.0};
    BoxOptimizer solver(problem);
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 10;

    auto first = solver.solve(objective, budget, initial);
    budget.max_evaluations = 190;
    auto second = solver.resume(objective, budget);
    if (!first.has_value() || !second.has_value()) return 0;
    cout << "first_cost = " << first.cost << '\n';
    cout << "second_cost = " << second.cost << '\n';
    cout << "first_evaluations = " << first.evaluations << '\n';
    cout << "second_evaluations = " << second.evaluations << '\n';
    cout << "total_evaluations = " << second.total_evaluations << '\n';
}
```

`resume(objective, budget)`は、保持している状態から続けます。引数に初期解はありません。最良点だけでなく、現在の探索状態、乱数の続き、途中の候補や世代も保持します。

今回の予算190は、最初からの累計上限ではなく**追加の上限**です。この例では、最初が10回、次が190回、累計が200回になります。再開後の最良目的値は、再開前より悪くなりません。

`first`は独立した所有配列を持つ結果です。`resume`を呼んでも、`first.x`や`first.cost`が自動的に書き換わるわけではありません。最新結果が必要なら`second`を使います。

同じ初期点・設定・目的関数のまま、時間制限を無効にして評価回数で区切る場合、途中再開しても連続実行と同じ評価順を保つ設計です。一方、時間制限で終了する評価回数は実行環境によって変わり得ます。

**resumeを使えるのは、同じ問題を続けるときだけです。** ラムダが参照する需要、グラフの重み、観測値などが変わる場合は`solve`で開始し直します。関数オブジェクトの名前が同じでも、その中の計算内容が変われば別の目的関数です。

目標到達後も、目標を厳しくするか外して`resume`できます。`LocalStop`の後は、その状態のままでは追加の探索を行いません。`resume`を呼ぶだけで再始動する仕様ではありません。

## 10. Step 07：時間制限と、評価できなかったときの代替解

**変更点：評価回数だけでなく、2ミリ秒という時間上限を加えます。**

保存名：`step07_time_limit.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}, {-5.0, 5.0}};
    auto objective = [](span<const double> x) -> double {
        double a = x[0] - 2.0;
        double b = x[1] + 1.0;
        double c = x[0] + x[1] - 1.0;
        return a * a + 4.0 * b * b + 0.5 * c * c;
    };
    vector<double> initial = {8.0, 4.0};
    vector<double> answer = initial;
    double answer_cost = objective(answer);

    BoxBudget budget;
    budget.time_limit_us = 2000;
    budget.max_evaluations = 100000;
    BoxOptimizer solver(problem);
    auto result = solver.solve(objective, budget, initial);
    if (result.has_value()) {
        answer = result.x;
        answer_cost = result.cost;
    }
    cout << answer[0] << ' ' << answer[1] << '\n';
    cout << "cost = " << answer_cost << '\n';
    cout << "evaluations = " << result.evaluations << '\n';
    cout << "elapsed_us = " << result.elapsed_us << '\n';
}
```

`time_limit_us`の`us`はマイクロ秒を表します。1000マイクロ秒が1ミリ秒です。`2000`は2ミリ秒であり、2秒ではありません。

今回は時間2000マイクロ秒と評価100000回の両方を指定しています。どちらかの停止条件に達すると終了します。`max_evaluations`を既定の`LLONG_MAX`のままにすると、実用上は時間中心の制限にできます。ただし時間を負にして無効化する場合は、`LLONG_MAX`より小さい有限の評価上限が必要です。

`time_limit_us = 0`は「制限なし」ではなく、「新しい評価を行わない」です。負値が時間制限なしを意味します。

`answer`と`answer_cost`は、評価済み結果を得られなかったときに使う初期解です。最初に自分で呼んだ`objective(answer)`は、ライブラリの評価回数にも`solve`の実測時間にも含まれません。実際の提出プログラムでは、このような準備時間も外側の締切に含めて管理します。

時刻は初回と各評価の直前に確認しますが、実行中の目的関数を途中で中断できません。内部の候補作成や結果コピーにも時間がかかります。したがって、2ミリ秒を指定しても、実測時間が2ミリ秒を少し超える場合があります。重いシミュレーションなら、その1回分の時間超過が大きくなり得ます。

`solve`の時間には探索の開始準備と結果作成を含み、`BoxOptimizer`の構築や`reset`は含みません。`box_optimize`なら構築・探索・オブジェクトの破棄も含む時間として扱います。

予算を省略したときの時間上限は1,950,000マイクロ秒、つまり1950ミリ秒です。複数回の呼び出しそれぞれにこの既定値を与えると、全体で1950ミリ秒に収まるわけではありません。外側の締切を共有する例はStep 25で扱います。

## 11. Step 08：探索方針とseedを指定する

**変更点：初期解の局所的な改善を優先し、探索が局所的に止まったら終了する設定にします。**

保存名：`step08_search_and_seed.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}, {-5.0, 5.0}};
    auto objective = [](span<const double> x) -> double {
        double a = x[0] - 2.0;
        double b = x[1] + 1.0;
        double c = x[0] + x[1] - 1.0;
        return a * a + 4.0 * b * b + 0.5 * c * c;
    };
    BoxParam param;
    param.search = BoxParam::Search::Refine;
    param.seed = 42;
    BoxOptimizer solver(problem, param);
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 10000;
    vector<double> initial = {8.0, 4.0};

    auto result = solver.solve(objective, budget, initial);
    if (!result.has_value()) return 0;
    cout << "cost = " << result.cost << '\n';
    cout << "evaluations = " << result.evaluations << '\n';
    cout << "local_stop = "
         << (result.stop == BoxResult::Stop::LocalStop) << '\n';
}
```

`BoxParam`は、箱と一緒にsolverへ持たせる探索設定です。呼び出しごとの予算を持つ`BoxBudget`とは役割が異なります。

| `search` | 意味 |
|---|---|
| `Explore` | 既定。停滞後も、別の出発点や探索区間を使いながら続ける |
| `Refine` | 初期解からの局所的な改善を行い、その探索が停止したら終了する |

「局所的」は、初期解から一定距離以内という意味ではありません。`Refine`でも箱の中を大きく移動することがあります。移動可能な範囲を限定したいなら、上下限そのものを狭くします。

`seed`は内部乱数の初期状態を決める整数です。既定は1で、今回は例として42にしています。42が特別に良い値という意味ではありません。

同じ実装・実行条件で、同じseed、初期解、目的関数、評価回数を使えば再現しやすくなります。ただし時間制限による停止や、コンパイラ・浮動小数点計算の違いまで同じにする指定ではありません。この例のように乱数を使う段階へ進まない経路では、seedを変えても結果が変わらないことがあります。

`param`の内容は構築時にsolverへ渡されます。後から手元の`param.seed`を書き換えるだけでは、既に作ったsolverの設定は変わりません。設定を変える方法はStep 17の`reset`で扱います。

## 12. Step 09：初期の探索幅を元の単位で指定する

**変更点：単位や範囲の異なる2変数を扱い、初期幅を指定します。**

保存名：`step09_initial_step.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 1000.0}, {-2.0, 2.0}};
    auto objective = [](span<const double> x) -> double {
        double a = (x[0] - 200.0) / 100.0;
        double b = x[1] - 0.5;
        return a * a + b * b;
    };
    BoxParam param;
    param.search = BoxParam::Search::Refine;
    param.initial_step = {50.0, 0.2};
    BoxOptimizer solver(problem, param);
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 3000;
    vector<double> initial = {800.0, -1.0};

    auto result = solver.solve(objective, budget, initial);
    if (!result.has_value()) return 0;
    cout << result.x[0] << ' ' << result.x[1] << '\n';
    cout << "cost = " << result.cost << '\n';
}
```

1番目を200へ、2番目を0.5へ近づけます。1番目のずれを100で割っているのは、100のずれを2番目の1のずれと同程度に数える、目的関数側の重み付けです。これは探索幅の指定とは別の考え方です。

| 変更した設定 | 意味 |
|---|---|
| `bounds = {{0, 1000}, {-2, 2}}` | 1番目は0〜1000、2番目は−2〜2の範囲で動かす |
| `initial = {800, -1}` | 目標から離れた、箱内の点から始める |
| `max_evaluations = 3000` | この教材で使う評価回数の上限。初期幅から自動計算される値ではない |

`initial_step = {50.0, 0.2}`は、1番目の初期幅50、2番目の初期幅0.2を**元の座標の単位**で指定しています。0〜1に変換した比率を渡すAPIではありません。

指定する場合の長さは変数数と同じにし、動かせる各変数の幅は正で、箱の幅以下にします。この例なら50は1000以下、0.2は4以下です。空のまま省略すると、各自由変数の箱幅の1/4が使われます。

初期幅は探索を始めるときの尺度です。「1回の移動が必ずこの値以下」「最良解が初期点からこの距離以内」という制約ではありません。探索中に幅は変わり、勾配を使う経路では最大の正規化幅が最初の方向の尺度に使われます。

まず既定値で試し、変数の単位や目的関数に合わせて変える、という使い方ができます。戻り値の`x`は今回も200や0.5のような元の単位です。

## 13. Step 10：一部の変数だけを動かす

**変更点：4個の設定のうち、中央の2個だけを改善します。**

保存名：`step10_fixed_variables.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    vector<double> base = {0.1, 0.3, 0.4, 0.8};
    vector<double> target = {0.2, 0.5, 0.6, 0.9};
    BoxProblem problem;
    for (double v : base) problem.bounds.push_back({v, v});
    problem.bounds[1] = {0.0, 1.0};
    problem.bounds[2] = {0.0, 1.0};

    auto objective = [&](span<const double> x) -> double {
        double cost = 0;
        for (int i = 0; i < 4; ++i) {
            double d = x[i] - target[i];
            cost += d * d;
        }
        for (int i = 0; i < 3; ++i) {
            double d = x[i] - x[i + 1];
            cost += 0.5 * d * d;
        }
        return cost;
    };
    BoxParam param;
    param.search = BoxParam::Search::Refine;
    BoxOptimizer solver(problem, param);
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 3000;

    auto result = solver.solve(objective, budget, base);
    if (!result.has_value()) return 0;
    for (double v : result.x) cout << v << ' ';
    cout << "\ncost = " << result.cost << '\n';
}
```

目的関数は、自分の目標からのずれと、隣り合う設定同士の差を合計しています。

$$f(x)=\sum_ {i=0}^{3}(x_ {i}-t_ {i})^2+\frac{1}{2}\sum_ {i=0}^{2}(x_ {i}-x_ {i+1})^2$$

$t_ {i}$は`target[i]`で与えた各要素の目標値です。$i$は位置の番号です。2番目の和では、0–1、1–2、2–3という隣接関係を数えています。$\sum$は、指定した範囲の式を全部足す記号です。

最初に`{v, v}`という同じ上下限を与え、すべてを元の値に固定しています。その後、添字1と2だけ`{0, 1}`へ変更しています。

| 添字 | 下限・上限 | 状態 |
|---:|---|---|
| 0 | 0.1, 0.1 | 0.1に固定 |
| 1 | 0, 1 | 自由に変更可能 |
| 2 | 0, 1 | 自由に変更可能 |
| 3 | 0.8, 0.8 | 0.8に固定 |

全変数数は4ですが、自由変数数は2です。目的関数へ渡る`x`や結果の`x`は、固定成分も含む長さ4のままです。既存の添字を変更せず使えます。

ラムダの`[&]`は、外側の`target`などを参照して使う指定です。この例では、その内容を探索中に変更しません。`base`を初期解として渡しているので、固定成分の値も境界と一致しています。

固定変数はライブラリ内部の探索対象から除かれます。ただし、この目的関数は毎回4要素と3本の関係を計算しています。固定変数を増やすだけで、利用者側の全走査まで自動で省略されるわけではありません。Step 24で、影響する部分だけを評価する形へ変更します。

## 14. Step 11：勾配を一緒に渡す

**変更点：Step 03の目的関数へ、各変数についての傾きも渡します。**

### 14.1 勾配は、変数ごとの「傾き」の配列

1変数では、関数を微分すると傾きが分かります。例えば$(x-3)^2$の微分は$2(x-3)$です。正の傾きなら、その近くでは$x$を増やすと目的値が増えます。

複数変数でも、**ほかの変数を固定して1個だけを少し動かしたときの傾き**を考えられます。これを偏微分といい、全変数分を並べたものが勾配です。

Step 03の関数について、各変数の傾きは次のようになります。

$$g_ {0}=2(x_ {0}-2)+(x_ {0}+x_ {1}-1),\qquad g_ {1}=8(x_ {1}+1)+(x_ {0}+x_ {1}-1)$$

$g_ {0}$が`x[0]`の傾き、$g_ {1}$が`x[1]`の傾きです。最後の項はどちらの変数についても、二乗の微分で2倍され、係数1/2と打ち消し合います。

ライブラリへこの情報を渡すには、関数の引数を1個増やします。

保存名：`step11_gradient.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}, {-5.0, 5.0}};
    auto objective = [](span<const double> x, span<double> gradient) -> double {
        double a = x[0] - 2.0;
        double b = x[1] + 1.0;
        double c = x[0] + x[1] - 1.0;
        if (!gradient.empty()) {
            gradient[0] = 2.0 * a + c;
            gradient[1] = 8.0 * b + c;
        }
        return a * a + 4.0 * b * b + 0.5 * c * c;
    };
    BoxParam param;
    param.search = BoxParam::Search::Refine;
    BoxOptimizer solver(problem, param);
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 2000;
    budget.target_cost = 1e-10;
    vector<double> initial = {8.0, 4.0};

    auto result = solver.solve(objective, budget, initial);
    if (!result.has_value()) return 0;
    cout << "cost = " << result.cost << '\n';
    cout << "evaluations = " << result.evaluations << '\n';
    cout << "gradient_evaluations = " << result.gradient_evaluations << '\n';
}
```

### 14.2 値はreturn、勾配は配列への書き込み

`span<const double> x`は従来どおり、読み取り専用の候補です。追加した`span<double> gradient`には`const`がないので、ライブラリが用意した配列へ値を書き込めます。

戻り値は、勾配ではなく従来と同じ目的値です。1回の関数呼び出しで、目的値の返却と、必要なら勾配の書き込みを両方行います。

勾配が要求されると、`gradient`の長さは固定変数を含む全変数数になります。**全成分を代入**してください。固定変数の成分は0で構いません。前回の値を残してよい部分配列ではありません。

勾配が不要な呼び出しでは、長さ0の`gradient`が渡されます。そのため、`if (!gradient.empty())`の内側で書き込みます。長さ0のまま`gradient[0]`へ書くと範囲外アクセスになります。

### 14.3 Autoと、元の単位の勾配

手法の指定を省略しているので、`Method::Auto`です。Autoは、関数がこの二引数形式を受け取れる場合、自由変数があれば勾配を使う経路を選びます。設定のための別フラグはありません。

値だけの一引数形式から、ライブラリが自動で数値微分する機能はありません。勾配を計算できない関数なら、一引数形式のままで使えます。

ここで渡す勾配は、`x`と同じ**元の単位についての傾き**です。内部では箱を正規化しますが、そのための倍率はライブラリが処理します。利用者がさらに箱幅を掛けると、倍率を二重に適用してしまいます。

この例の目標値`1e-10`は、二次関数の計算結果が十分小さくなったら終了するための指定です。勾配を渡すと必ずその精度まで達する、という設定ではありません。

## 15. Step 12：勾配の式を小さな差分で検算する

**変更点：探索を始める前に、書いた微分の式が大きく間違っていないか確認します。**

少し右と左の目的値を比べると、傾きを近似できます。変数$i$だけを幅$h$ずつ動かした点を$x^{+}$、$x^{-}$とすると、次の式です。

$$g_ {i}\approx\frac{f(x^{+})-f(x^{-})}{2h}$$

$h$は検算用の小さな幅です。$x^{+}$と$x^{-}$は、ほかの成分を元の$x$と同じに保ちます。この式を中央差分と呼びます。

保存名：`step12_check_gradient.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}, {-5.0, 5.0}};
    auto objective = [](span<const double> x, span<double> gradient) -> double {
        double a = x[0] - 2.0;
        double b = x[1] + 1.0;
        double c = x[0] + x[1] - 1.0;
        if (!gradient.empty()) {
            gradient[0] = 2.0 * a + c;
            gradient[1] = 8.0 * b + c;
        }
        return a * a + 4.0 * b * b + 0.5 * c * c;
    };
    vector<double> point = {1.5, 0.2};
    vector<double> gradient(2);
    objective(point, gradient);
    double h = 1e-6;
    double max_error = 0;
    for (int i = 0; i < 2; ++i) {
        auto left = point;
        auto right = point;
        left[i] -= h;
        right[i] += h;
        double numerical = (objective(right, {}) - objective(left, {})) / (2 * h);
        max_error = max(max_error, abs(numerical - gradient[i]));
    }
    cout << "max_gradient_error = " << max_error << '\n';

    BoxParam param;
    param.search = BoxParam::Search::Refine;
    BoxOptimizer solver(problem, param);
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 2000;
    auto result = solver.solve(objective, budget, point);
    if (result.has_value()) cout << "cost = " << result.cost << '\n';
}
```

`point`は箱の内側の検算点、`gradient(2)`は勾配を書き込んでもらう長さ2の配列です。`objective(point, gradient)`では両成分の傾きを計算します。

`objective(right, {})`の`{}`は、空の`span<double>`です。「目的値だけ計算してほしい」という呼び出しになり、Step 11の空チェックをそのまま使えます。

`h = 1e-6`は今回の尺度に合わせた検算幅です。小さすぎると、非常に近い値の引き算で丸め誤差が目立ちます。大きすぎると、目的の点の傾きとは違ってきます。問題の単位が違う場合は、幅も変えます。

この例は両側へ動かしても箱内に残る点を選んでいます。境界のすぐ近くで同じ検算を行うと箱の外へ出るので、そのまま真似せず、内側の点を選ぶか片側の差分を使います。

検算の評価は利用者が直接呼んだものなので、`result.evaluations`には含まれません。また、この検算を探索のたびに自動実行するようにしたわけではありません。

丸め・絶対値の折れ曲がり・条件分岐の切替点など、傾きが定まらない場所ではこの比較だけで正しさを判断できません。滑らかでない関数には、勾配なしの経路も選択肢になります。

## 16. Step 13：勾配付き関数のまま座標探索を指定する

**変更点：目的関数はStep 11の二引数形式のまま、座標探索へ切り替えて改善の様子を確認します。**

保存名：`step13_coordinate.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}, {-5.0, 5.0}};
    auto objective = [](span<const double> x, span<double> gradient) -> double {
        double a = x[0] - 2.0;
        double b = x[1] + 1.0;
        double c = x[0] + x[1] - 1.0;
        if (!gradient.empty()) {
            gradient[0] = 2.0 * a + c;
            gradient[1] = 8.0 * b + c;
        }
        return a * a + 4.0 * b * b + 0.5 * c * c;
    };
    BoxParam param;
    param.search = BoxParam::Search::Refine;
    param.method = BoxParam::Method::Coordinate;
    BoxOptimizer solver(problem, param);
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 3000;
    vector<double> initial = {8.0, 4.0};

    auto result = solver.solve(objective, budget, initial);
    if (!result.has_value()) return 0;
    cout << "cost = " << result.cost << '\n';
    cout << "gradient_evaluations = " << result.gradient_evaluations << '\n';
}
```

`param.method`は使う手法を選ぶ設定です。既定の`Auto`は、関数が勾配を受け取れるかどうかで経路が変わります。今回は`Coordinate`を明示して、1座標ずつの変更を中心にする経路へ固定しています。詳しい動きは付録で説明します。

この経路では勾配を使いません。二引数の関数でも`gradient`は空で渡されるため、条件付きにしてある微分の計算は実行されません。出力の`gradient_evaluations`は0です。

`search=Refine`は停滞したら終了する方針、`method=Coordinate`は調べ方の指定です。別々の軸の設定なので、組み合わせて指定します。

今回は目標値を指定せず、最大3000評価までにどこまで改善できるかを見ます。`Refine`の局所停止が先に起きれば、3000回より前に終了します。時間制限は引き続き−1で無効です。

手法を変えて比べるときは、同じ関数、初期解、予算で比べます。ある教材で評価回数が少なかった手法が、ほかの目的関数でも速いとは限りません。

## 17. Step 14：複数の谷がある関数でDiagonalCmaを指定する

**変更点：滑らかな1つのお椀から、細かい起伏のある関数へ変えます。勾配は渡しません。**

保存名：`step14_diagonal_cma.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{-2.0, 2.0}, {-2.0, 2.0}};
    auto objective = [](span<const double> x) -> double {
        double a = x[0] - 0.3;
        double b = x[1] + 0.2;
        return a * a + b * b
             + 0.2 * (2.0 - cos(10.0 * a) - cos(10.0 * b));
    };
    BoxParam param;
    param.method = BoxParam::Method::DiagonalCma;
    param.seed = 7;
    BoxOptimizer solver(problem, param);
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 10000;
    budget.target_cost = 1e-8;
    vector<double> initial = {1.5, -1.4};

    auto result = solver.solve(objective, budget, initial);
    if (!result.has_value()) return 0;
    cout << result.x[0] << ' ' << result.x[1] << '\n';
    cout << "cost = " << result.cost << '\n';
    cout << "evaluations = " << result.evaluations << '\n';
}
```

`cos`は余弦です。`10.0`は起伏の細かさ、`0.2`は起伏の重みを調整する、この教材の目的関数側の定数です。ライブラリの探索パラメータではありません。

`a=b=0`、つまり`x[0]=0.3, x[1]=-0.2`なら目的値は0です。二乗の項も、`2-cos(...)-cos(...)`も負にならないため、ここが大域最適点だと確認できます。ただし、途中には別の谷があるため、どの探索でも必ずすぐに到達するわけではありません。

`DiagonalCma`は複数候補の結果から、次に試す範囲を調整する対角CMA系の経路です。勾配は使いません。二引数形式の関数を渡した場合も、`gradient`は空になります。

`search`は指定していないので既定の`Explore`です。停滞しても、残り予算があれば再始動します。`seed=7`は再現用の任意の値です。

両変数の範囲は−2〜2、初期解は1.5, −1.4です。時間制限は無効のまま、最大10000評価を使い、目的値が`1e-8`以下になれば早く終了します。評価上限は探索量、目標値は十分な良さを判断する値で、別々の設定です。

この例でCMAが必須という意味ではありません。既定のAutoやCoordinateも候補になります。DiagonalCmaを選ぶ意味は、Autoの勾配なし経路がまず座標探索から始めるのに対し、最初からこの経路を使うことです。

名前の「対角」は、変数ごとの広がりを調整するものの、全変数間の傾いた相関を密な行列として持たないことを表します。複数変数が強く結び付いた谷で、通常の密なCMAと同等の表現力があるわけではありません。

## 18. Step 15：履歴を持たないSpectralGradientを選ぶ

**変更点：Step 11の勾配付き例で、手法だけをSpectralGradientへ変えます。**

保存名：`step15_spectral_gradient.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}, {-5.0, 5.0}};
    auto objective = [](span<const double> x, span<double> gradient) -> double {
        double a = x[0] - 2.0;
        double b = x[1] + 1.0;
        double c = x[0] + x[1] - 1.0;
        if (!gradient.empty()) {
            gradient[0] = 2.0 * a + c;
            gradient[1] = 8.0 * b + c;
        }
        return a * a + 4.0 * b * b + 0.5 * c * c;
    };
    BoxParam param;
    param.method = BoxParam::Method::SpectralGradient;
    param.search = BoxParam::Search::Refine;
    BoxOptimizer solver(problem, param);
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 2000;
    budget.target_cost = 1e-10;
    vector<double> initial = {8.0, 4.0};

    auto result = solver.solve(objective, budget, initial);
    if (!result.has_value()) return 0;
    cout << "cost = " << result.cost << '\n';
    cout << "evaluations = " << result.evaluations << '\n';
    cout << "gradient_evaluations = " << result.gradient_evaluations << '\n';
}
```

Autoの勾配経路は、過去の移動と勾配変化を最大8組保持して、次の方向に利用します。SpectralGradientは、この複数組の履歴を持たず、直近の移動と勾配変化から歩幅の尺度を更新する経路です。

履歴を使わない分、内部処理や新規に必要な記憶領域を小さくできます。一方で、目的関数の形によっては改善効率が落ちます。「軽い内部処理」と「同じ時間で良い解を得ること」は同じではないため、利用する問題と予算でAutoと比べます。

自由変数がある場合、SpectralGradientには二引数形式の目的関数が必要です。一引数の関数を渡すと、勾配を自動生成せず、不正な使い方として`assert`の対象になります。全変数固定・変数数0だけは例外で、値の評価だけで済みます。

ここまでで公開されている4種類の`Method`、Auto・Coordinate・DiagonalCma・SpectralGradientが一通り登場しました。

## 19. Step 16：目的関数が変わるターンではsolveし直す

**変更点：目標値がターンごとに少し動く問題にし、前の解を新しい初期解へ使います。**

保存名：`step16_turns.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}, {-5.0, 5.0}};
    vector<double> target(2);
    auto objective = [&](span<const double> x, span<double> gradient) -> double {
        double a = x[0] - target[0];
        double b = x[1] - target[1];
        double c = a + b;
        if (!gradient.empty()) {
            gradient[0] = 2.0 * a + c;
            gradient[1] = 8.0 * b + c;
        }
        return a * a + 4.0 * b * b + 0.5 * c * c;
    };
    BoxParam param;
    param.search = BoxParam::Search::Refine;
    BoxOptimizer solver(problem, param);
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 2000;
    budget.target_cost = 1e-10;
    vector<double> previous;

    for (int turn = 0; turn < 5; ++turn) {
        target[0] = 2.0 + 0.1 * turn;
        target[1] = -1.0 + 0.05 * turn;
        auto result = solver.solve(objective, budget, previous);
        if (!result.has_value()) continue;
        previous = result.x;
        cout << turn << ' ' << result.x[0] << ' ' << result.x[1]
             << ' ' << result.cost << ' ' << result.total_evaluations << '\n';
    }
}
```

`target`を更新すると、同じ`x`でも返す目的値が変わります。そのため、このコードでは毎ターン`solve`を使います。`resume`で古い目的値や勾配の履歴を続けて使ってはいけません。

`previous`は前ターンの解です。最初は空なので、初回は箱の中央から始まります。以降は`result.x`をコピーして次の初期解にします。このように前の解から始める使い方を、ウォームスタートと呼ぶことがあります。

| 今回の値 | 役割 |
|---|---|
| 5ターン | 教材の繰り返し回数 |
| `0.1 * turn` | 1番目の目標がターンごとに動く量 |
| `0.05 * turn` | 2番目の目標が動く量 |
| 各回2000評価 | そのターンの上限。5ターン合計の上限ではない |
| 各回の目標`1e-10` | そのターンの新しい目的関数に対する到達判定 |

`c=a+b`は、2変数の合計が「そのターンの目標値の合計」からどれだけずれたかを表します。したがって目標点そのものなら毎ターン目的値0です。

前の解は、新しい目的関数で必ず評価し直します。前ターンの目的値をそのまま再利用するわけではありません。`total_evaluations`も各`solve`から数え直します。

`previous`に古い解を残せるのは、今回の箱が変わらず、その点が引き続き箱内にあるためです。箱が変わる場合は次の手順を使います。

## 20. Step 17：箱や設定を変更するreset

**変更点：途中で使える範囲が狭まったので、問題を入れ替えます。**

保存名：`step17_reset.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}, {-5.0, 5.0}};
    auto objective = [](span<const double> x, span<double> gradient) -> double {
        double a = x[0] - 2.0;
        double b = x[1] + 1.0;
        double c = x[0] + x[1] - 1.0;
        if (!gradient.empty()) {
            gradient[0] = 2.0 * a + c;
            gradient[1] = 8.0 * b + c;
        }
        return a * a + 4.0 * b * b + 0.5 * c * c;
    };
    BoxParam param;
    param.search = BoxParam::Search::Refine;
    BoxOptimizer solver(problem, param);
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 2000;
    vector<double> initial = {8.0, 4.0};
    auto first = solver.solve(objective, budget, initial);
    if (first.has_value()) initial = first.x;

    BoxProblem smaller;
    smaller.bounds = {{0.0, 1.0}, {-0.5, 0.5}};
    for (int i = 0; i < 2; ++i) {
        initial[i] = clamp(initial[i], smaller.bounds[i].lower,
                                      smaller.bounds[i].upper);
    }
    solver.reset(smaller, param);
    auto second = solver.solve(objective, budget, initial);
    if (!second.has_value()) return 0;
    cout << second.x[0] << ' ' << second.x[1] << '\n';
    cout << "cost = " << second.cost << '\n';
    cout << "total_evaluations = " << second.total_evaluations << '\n';
}
```

`reset(problem, param)`は、箱と設定を変更し、それまでの探索履歴を破棄します。次は`resume`ではなく`solve`で新しく開始します。

`smaller`では、1番目の上限を1、2番目の範囲を−0.5〜0.5にしています。古い最良点2, −1は新しい箱の外です。そこで、`clamp`を使って初期点を新しい上下限に収めています。

`clamp(value, lower, upper)`は、範囲内ならそのまま、下限未満なら下限、上限を超えたら上限を返します。これは**利用者側の初期解調整**です。ライブラリが範囲外の初期解を自動修正する仕様ではありません。

`reset(smaller, param)`には、引き続き使いたい`param`も渡しています。`reset(smaller)`と第2引数を省略すると、古い設定を引き継がず、既定の`BoxParam`へ戻ります。

変数数や固定する成分を変更する場合も`reset`を使えます。その場合は、初期解や目的関数側の配列アクセスも新しい問題に合わせます。単に需要などの評価データだけが変わり、箱と設定が同じなら、Step 16の`solve`だけで十分です。

## 21. Step 18：全固定と、変数数0

**変更点：動かせる変数がない場合の結果を確認します。**

保存名：`step18_no_free_variables.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 5;

    BoxProblem fixed;
    fixed.bounds = {{2.0, 2.0}, {-1.0, -1.0}};
    auto objective = [](span<const double> x) -> double {
        double a = x[0] - 2.0;
        double b = x[1] + 1.0;
        return a * a + b * b;
    };
    auto first = box_optimize(fixed, objective, budget);
    cout << "fixed_cost = " << first.cost << '\n';
    cout << "fixed_evaluations = " << first.evaluations << '\n';
    cout << "fixed_stop = "
         << (first.stop == BoxResult::Stop::AllFixed) << '\n';

    BoxProblem empty;
    auto constant = [](span<const double> x) -> double {
        assert(x.empty());
        return 7.0;
    };
    auto second = box_optimize(empty, constant, budget);
    cout << "empty_has_value = " << second.has_value() << '\n';
    cout << "empty_size = " << second.x.size() << '\n';
    cout << "empty_cost = " << second.cost << '\n';
    cout << "empty_evaluations = " << second.evaluations << '\n';
}
```

全固定でも、目的関数の値はライブラリには分かりません。そのため、予算があれば1回評価して`AllFixed`で終了します。上限5だから5回呼ぶわけではありません。

`empty`は変数数0の問題です。許される設定は「要素のない配列」1通りだけと考えられます。今回の定数目的関数では、1回評価すると`has_value=true`、`x.size()=0`、`cost=7`です。

したがって、**`x.empty()`だけで未評価かどうかを判断してはいけません。** 有効な0変数の解も空配列です。`has_value()`を使います。

`assert(x.empty())`は、この教材で想定どおり空配列を受け取ったかを確かめる検査です。結果を作るための必須処理ではありません。

評価回数上限0、または時間上限0なら、全固定でも初回評価は行いません。評価後の再開では、同じ点を繰り返し評価しません。目標到達も同時に成立した場合は、先に説明した優先順位により`TargetReached`になります。

## 22. Step 19：最大化したい得点を、最小化へ変える

**変更点：悪さを小さくする代わりに、得点を大きくしたい問題にします。**

保存名：`step19_maximize.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}};
    auto score = [](span<const double> x) -> double {
        double d = x[0] - 3.0;
        return 100.0 - d * d;
    };
    auto objective = [&](span<const double> x) -> double {
        return -score(x);
    };
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 500;
    budget.target_cost = -99.9;

    auto result = box_optimize(problem, objective, budget);
    if (!result.has_value()) return 0;
    cout << "x = " << result.x[0] << '\n';
    cout << "score = " << -result.cost << '\n';
    cout << "cost = " << result.cost << '\n';
}
```

元の得点を$S(x)$とすると、目的関数を次のようにします。

$$f(x)=-S(x)$$

得点が99から100へ上がると、目的値は−99から−100へ下がります。小さいほど良いというライブラリの約束に合います。

今回の`target_cost = -99.9`は、得点99.9以上に達したら終了する指定です。得点を反転したのに、目標値を正の99.9のままにしないようにします。

`result.cost`は反転後の目的値なので、表示する得点は`-result.cost`です。勾配も渡す場合は、得点の勾配の各成分にもマイナスを掛けます。目的値だけを反転し、勾配を反転し忘れると整合しなくなります。

## 23. Step 20：整数化した結果を直接採点する

**変更点：内部では実数を動かし、提出する2個の設定は整数へ丸めます。**

保存名：`step20_rounding.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}, {0.0, 10.0}};
    auto decode = [](span<const double> x) -> array<int, 2> {
        return {static_cast<int>(lround(x[0])),
                static_cast<int>(lround(x[1]))};
    };
    auto objective = [&](span<const double> x) -> double {
        auto output = decode(x);
        double a = output[0] - 2.0;
        double b = output[1] - 7.0;
        return a * a + b * b;
    };
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 5000;
    budget.target_cost = 0.0;
    vector<double> initial = {5.0, 5.0};

    auto result = box_optimize(problem, objective, budget, initial);
    if (!result.has_value()) return 0;
    auto output = decode(result.x);
    cout << output[0] << ' ' << output[1] << '\n';
    cout << "cost = " << result.cost << '\n';
}
```

`decode`は、実数の候補を実際の出力形式へ変換する、自分で用意した関数です。ここでは`lround`で最も近い整数へ丸めます。今回の範囲が0〜10なので、変換後の整数も0〜10に収まります。

`array<int, 2>`は整数2個を持つ固定長配列です。ライブラリがこの型を操作するのではなく、利用者側の評価と出力に使っています。

目的関数は、実数のずれではなく、丸めた整数と目標2、7とのずれを採点します。したがって、`result.cost`は実際に出力する整数を採点した値と一致します。

丸めると、多くの異なる実数が同じ整数になります。目的値が一定の平らな区間や、急に変わる境目が生まれます。そのため、この例は一引数の目的関数にして、勾配を使いません。

これは汎用の整数最適化を厳密に解く仕組みではありません。整数同士の強い制約や順列を扱うなら、専用の整数・離散ソルバーも検討します。連続パラメータから出力を作る既存の仕組みがあり、その出力を直接評価したい場面の使い方です。

## 24. Step 21：滑らかな代理目的を使うなら、本来の最良出力も保存する

**変更点：探索用の目的値と、実際に提出する出力の良さが違う場合を扱います。**

前の例は実際の整数を直接採点しました。一方、探索しやすくするために、滑らかな別の目的関数を使いたいことがあります。これを代理目的と呼びます。

代理目的が改善しても、本来の得点が改善するとは限りません。この例では、その違いがはっきり出るよう、代理目的は5,5へ近づけ、本来の目標は2,7のままにします。

保存名：`step21_surrogate.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}, {0.0, 10.0}};
    auto decode = [](span<const double> x) -> array<int, 2> {
        return {static_cast<int>(lround(x[0])),
                static_cast<int>(lround(x[1]))};
    };
    auto actual_cost = [](const array<int, 2>& output) -> double {
        double a = output[0] - 2.0;
        double b = output[1] - 7.0;
        return a * a + b * b;
    };
    vector<double> initial = {2.0, 7.0};
    auto best_output = decode(initial);
    double best_actual_cost = actual_cost(best_output);

    auto surrogate = [&](span<const double> x) -> double {
        auto output = decode(x);
        double actual = actual_cost(output);
        if (actual < best_actual_cost) {
            best_actual_cost = actual;
            best_output = output;
        }
        double a = x[0] - 5.0;
        double b = x[1] - 5.0;
        return a * a + b * b;
    };
    BoxParam param;
    param.search = BoxParam::Search::Refine;
    BoxOptimizer solver(problem, param);
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 3000;
    auto result = solver.solve(surrogate, budget, initial);
    if (result.has_value()) {
        cout << "proxy_cost = " << result.cost << '\n';
        cout << "actual_at_proxy_best = " << actual_cost(decode(result.x)) << '\n';
    }
    cout << best_output[0] << ' ' << best_output[1] << '\n';
    cout << "best_actual_cost = " << best_actual_cost << '\n';
}
```

`best_output`と`best_actual_cost`は、ライブラリの外側で保持する、本来の目的に対する最良結果です。初期出力から用意してあるので、ライブラリが1回も評価できなくても出力できます。

コールバックが呼ばれるたびに、候補を整数化して本来の評価を確認します。それとは別に、`return`するのは5,5への滑らかな二乗誤差です。ライブラリの`result.cost`は、こちらの代理目的に対する最良値です。

この例では初期出力2,7の本来の目的値が既に0です。代理目的に従って5,5へ近づくと、本来の目的値は悪化します。最後に採用するのは`result.x`を無条件に整数化したものではなく、保存した`best_output`です。

毎回の評価で最良出力を記録するという副作用はあっても、同じ`x`に返す代理目的値は同じです。`best_actual_cost`によって戻り値の計算方法を変えてはいけません。また、本来の評価を追加した分の時間も予算に影響します。

予算の都合で全候補を本来の評価にかけられない場合、少なくとも初期出力と最終候補を比較してから採用します。ただし、その方法では探索途中に得た本来の良い出力を見逃す可能性があります。

## 25. Step 22：合計制約を満たす変数へ変換する

**変更点：3か所への配分を非負、合計10に保ちながら、目標2,3,5へ近づけます。**

BoxOptimizerへ「合計10」という一般制約を直接登録するAPIはありません。そこで、箱内の変数を、必ず条件を満たす配分へ変換します。

2変数$u,v$を0〜1で動かし、配分$a,b,c$を次のように作ります。

$$a=10u,\qquad b=10(1-u)v,\qquad c=10(1-u)(1-v),\qquad 0\le u,v\le 1$$

まず10のうち割合$u$を$a$へ配り、残った分を割合$v$と$1-v$で$b,c$へ分けると考えます。どれも非負になり、式の上では合計が必ず10です。$u=1$のときは全部を$a$へ配るので、$v$によらず$b=c=0$になります。

保存名：`step22_total_constraint.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 1.0}, {0.0, 1.0}};
    auto decode = [](span<const double> x) -> array<double, 3> {
        double u = x[0];
        double v = x[1];
        return {10.0 * u, 10.0 * (1.0 - u) * v,
                10.0 * (1.0 - u) * (1.0 - v)};
    };
    auto objective = [&](span<const double> x) -> double {
        auto allocation = decode(x);
        double a = allocation[0] - 2.0;
        double b = allocation[1] - 3.0;
        double c = allocation[2] - 5.0;
        return a * a + b * b + c * c;
    };
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 10000;
    budget.target_cost = 1e-10;
    vector<double> initial = {0.3, 0.5};

    auto result = box_optimize(problem, objective, budget, initial);
    if (!result.has_value()) return 0;
    auto allocation = decode(result.x);
    cout << allocation[0] << ' ' << allocation[1] << ' ' << allocation[2] << '\n';
    cout << "sum = " << allocation[0] + allocation[1] + allocation[2] << '\n';
    cout << "cost = " << result.cost << '\n';
}
```

目的関数は、変換した配分の目標からの二乗誤差の和です。ライブラリが扱う変数数は2、出力の配分数は3です。

| 値 | 意味 |
|---|---|
| `x[0]` | 最初の配分割合$u$ |
| `x[1]` | 残りを分ける割合$v$ |
| `10.0` | 配る総量 |
| `2.0, 3.0, 5.0` | 目標配分。合計10 |
| `initial = {0.3, 0.5}` | 初期配分を3,3.5,3.5にする |

`result.x`は配分そのものではなく、$u,v$です。必ず`decode`して利用します。正解の配分2,3,5は、$u=0.2,v=0.375$に対応します。

この変換は、非負・合計10という条件を扱うものです。配分ごとに追加の上下限がある問題まで、自動的に満たすわけではありません。浮動小数点では合計に丸め誤差が出ることもあるので、提出形式に厳密な整数総量が必要なら、整数への変換と合計の修復を別に設計して、その後の値を評価します。

勾配も使う場合は、配分$a,b,c$についての勾配をそのまま渡さず、入力である$u,v$について微分した値を渡します。ライブラリが行う自動変換は箱の正規化だけで、利用者の`decode`の微分までは行いません。

制約違反へ大きな罰点を加える方法もありますが、有限の罰点では違反した点が最良になる場合があります。今回のように実行可能な出力だけを作る方法や、一般制約を扱う専用ソルバーと使い分けます。

## 26. Step 23：不確実な状況を、固定したシナリオ群で評価する

**変更点：装置の効き方が状況によって変わるので、複数状況の平均的なずれを小さくします。**

保存名：`step23_fixed_scenarios.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    BoxProblem problem;
    problem.bounds = {{0.0, 10.0}};
    const array<double, 4> efficiency = {0.7, 0.9, 1.1, 1.3};
    const double target = 5.0;
    auto objective = [&](span<const double> x) -> double {
        double cost = 0;
        for (double e : efficiency) {
            double response = e * x[0];
            double error = response - target;
            cost += error * error;
        }
        return cost / static_cast<double>(efficiency.size());
    };
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 2000;

    auto result = box_optimize(problem, objective, budget);
    if (!result.has_value()) return 0;
    cout << "x = " << result.x[0] << '\n';
    cout << "cost = " << result.cost << '\n';
}
```

`efficiency`は4つの状況に対応する効率です。例えば効率0.7、出力5なら、実際の応答は3.5になります。目標応答はどの状況でも5です。

目的関数は、4状況を等しい重みで扱った平均二乗誤差です。$m$を状況数、$e_ {s}$を状況$s$の効率、$t$を目標応答とすると、式は次のとおりです。

$$f(x)=\frac{1}{m}\sum_ {s=0}^{m-1}(e_ {s}x-t)^2,\qquad m=4,\quad t=5$$

`static_cast<double>(efficiency.size())`は、要素数を小数計算の型へ明示的に変換しています。シナリオが増えても、合計をその個数で割ることで平均にできます。

毎回同じ4状況で評価するので、同じ`x`なら同じ値です。より複雑なシミュレーションでも、探索開始前に乱数で状況を生成して固定しておき、各候補で同じ状況を使う方法にできます。

候補を評価するたびに新たな状況を生成すると、偶然良かった候補と本当に良い候補を区別しにくくなり、このライブラリの決定的な目的関数という契約にも合いません。`param.seed`は探索内部の乱数用であり、目的関数側の乱数を自動で固定する機能ではありません。

シナリオ群を変更したら、新しい目的関数なので`solve`をやり直します。また、固定した状況への適合が、未知の状況での良さを保証するわけではありません。実用では、探索に使っていない別の状況群で結果を確認することも有効です。

## 27. Step 24：部分問題は、変わる項だけを計算する

**変更点：Step 10と同じ4要素の問題を、自由な2変数だけの目的関数へ書き換えます。**

Step 10では固定成分も含めて、全要素を毎回走査しました。今回は添字1と2だけを入力として受け取り、固定部分の寄与を前計算します。

保存名：`step24_compact_problem.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    vector<double> base = {0.1, 0.3, 0.4, 0.8};
    vector<double> target = {0.2, 0.5, 0.6, 0.9};
    auto full_cost = [&](span<const double> x) -> double {
        double cost = 0;
        for (int i = 0; i < 4; ++i) {
            double d = x[i] - target[i];
            cost += d * d;
        }
        for (int i = 0; i < 3; ++i) {
            double d = x[i] - x[i + 1];
            cost += 0.5 * d * d;
        }
        return cost;
    };
    double d0 = base[0] - target[0];
    double d3 = base[3] - target[3];
    double constant = d0 * d0 + d3 * d3;
    auto partial_cost = [&](span<const double> z) -> double {
        double a = z[0] - target[1];
        double b = z[1] - target[2];
        double left = base[0] - z[0];
        double middle = z[0] - z[1];
        double right = z[1] - base[3];
        return constant + a * a + b * b
             + 0.5 * (left * left + middle * middle + right * right);
    };
    BoxProblem problem;
    problem.bounds = {{0.0, 1.0}, {0.0, 1.0}};
    BoxParam param;
    param.search = BoxParam::Search::Refine;
    BoxOptimizer solver(problem, param);
    BoxBudget budget;
    budget.time_limit_us = -1;
    budget.max_evaluations = 3000;
    vector<double> initial = {base[1], base[2]};

    auto result = solver.solve(partial_cost, budget, initial);
    if (!result.has_value()) return 0;
    auto output = base;
    output[1] = result.x[0];
    output[2] = result.x[1];
    for (double v : output) cout << v << ' ';
    cout << "\ncost = " << result.cost << '\n';
    cout << "full_cost = " << full_cost(output) << '\n';
}
```

### 27.1 添字の対応を明確にする

| 今回の入力 | 元の問題の要素 |
|---|---|
| `z[0]` | 元の`x[1]` |
| `z[1]` | 元の`x[2]` |
| `base[0]` | 動かさない元の`x[0]` |
| `base[3]` | 動かさない元の`x[3]` |

結果も2要素なので、`output[1]`と`output[2]`へ戻して使います。Step 10では結果が4要素でした。この違いはライブラリが自動で判断するものではなく、利用者が選んだ問題の表現方法によるものです。

### 27.2 固定部分の値を毎回計算しない

`constant`は、固定した0番目と3番目の目標からのずれです。候補がどう変わっても同じなので、探索前に一度だけ計算します。

`left`、`middle`、`right`は、変更する変数に接する3本の関係の寄与です。自由変数同士を結ぶ1–2の項を、両側から2回数えないようにしています。

今回は`constant`も戻り値へ加えるので、`partial_cost`と`full_cost`は同じ数学的な目的値です。加算順が違うため、浮動小数点の末尾には差が出る場合があります。定数項を完全に省いても最良点は変わりませんが、`cost`や`target_cost`の尺度が変わるので、そのまま比較しないでください。

大きなグラフで一部だけを変更するなら、固定された項と、変更部分に影響される項を前処理で分けます。候補ごとに巨大な状態全体をコピーして復元すると、その分の費用が残るので、今回のように必要な値だけ参照する方法を考えます。

これは「前回の候補との差分」を渡すAPIではありません。BoxOptimizerは毎回、現在の候補の配列を目的関数へ渡します。利用者側で、目的関数の式を等価な小さい計算へ書き換えています。評価順を勝手に仮定して、採用されたか不明な前候補を基準に差分を計算しないようにします。

## 28. Step 25：外側の締切を共有して、複数の初期解を試す

**変更点：Step 14の関数を3回に分けて解き、全体20ミリ秒の枠内で予算を配ります。**

保存名：`step25_shared_deadline.cpp`

```cpp
#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    using Clock = chrono::steady_clock;
    auto start = Clock::now();
    const double total_limit_us = 20000;
    BoxProblem problem;
    problem.bounds = {{-2.0, 2.0}, {-2.0, 2.0}};
    auto objective = [](span<const double> x) -> double {
        double a = x[0] - 0.3;
        double b = x[1] + 0.2;
        return a * a + b * b
             + 0.2 * (2.0 - cos(10.0 * a) - cos(10.0 * b));
    };
    const array<array<double, 2>, 3> starts = {{
        {1.5, -1.4}, {-1.2, 1.1}, {0.0, 0.0}
    }};
    vector<double> best(starts[0].begin(), starts[0].end());
    double best_cost = objective(best);
    int completed = 0;

    for (int k = 0; k < 3; ++k) {
        double used = chrono::duration<double, micro>(Clock::now() - start).count();
        double remaining = total_limit_us - used;
        if (remaining <= 0) break;
        BoxBudget budget;
        budget.time_limit_us = remaining / (3 - k);
        budget.max_evaluations = 100000;
        BoxParam param;
        param.method = BoxParam::Method::DiagonalCma;
        param.seed = static_cast<uint64_t>(100 + k);

        auto result = box_optimize(problem, objective, budget, starts[k], param);
        if (result.has_value() && result.cost < best_cost) {
            best_cost = result.cost;
            best = result.x;
        }
        ++completed;
    }
    cout << best[0] << ' ' << best[1] << '\n';
    cout << "cost = " << best_cost << '\n';
    cout << "completed_runs = " << completed << '\n';
}
```

`starts`は3個の初期解です。どれも箱内に用意します。配列`starts[k]`は`array<double, 2>`ですが、初期解を受け取る`span<const double>`へ渡せます。これまでの`vector<double>`だけに限定されたAPIではありません。

`Clock::now() - start`で、外側の開始からの経過時間を測っています。`duration<double, micro>`は、それを小数のマイクロ秒へ変換する書き方です。

`remaining / (3 - k)`は、残り時間をまだ開始していない回数へ均等に配る単純な方針です。前の実行が早く終われば、次の実行で残り時間を再計算して配り直します。20ミリ秒を各回へ渡して合計60ミリ秒にしてしまうことを防ぎます。

各回の評価上限100000は、時間上限に加えて設ける回数の上限です。目標値は指定していません。`seed=100+k`により、3回にはそれぞれ100、101、102を使い、初期解と乱数列の両方を変えます。`static_cast<uint64_t>`は、整数の型をseed用の型へ明示的に変換しています。

`box_optimize`には今回、5引数をすべて渡しています。

| 順番 | 引数 | 意味 |
|---:|---|---|
| 1 | `problem` | 箱 |
| 2 | `objective` | 目的関数 |
| 3 | `budget` | 今回の実行に割り当てる時間・評価回数 |
| 4 | `starts[k]` | 今回の初期解 |
| 5 | `param` | 手法・seedなどの設定 |

内部のsolverの構築や破棄も一括APIの計測に含まれます。それでも、コールバックを中断できないことや、呼び出しの前後に処理があることから、外側の厳密な締切を数学的に保証するコードではありません。提出ではI/Oや最終的な出力作成の時間も別に確保します。

`best`は全実行を通した最良解です。初回実行前に有効な初期解で用意しているので、時間が尽きても使えます。各回を独立した探索にするため、`resume`ではなく`box_optimize`を呼び直します。

Explore自体にも停滞後の探索継続があるため、外側の複数実行が常に有利とは限りません。異なる初期解を明示的に試したい場合や、複数手法へ予算を分けたい場合の組み込みパターンです。

## 29. 実用時に選ぶ入口

ここまでの例を、自分の問題へ置き換えるときの目安です。

| やりたいこと | 基本形 |
|---|---|
| 1回だけ解く | `box_optimize` |
| 同じ問題を少しずつ続ける | 初回`solve`、次から`resume` |
| 需要や採点データが変わる | 前解を初期解にして新しい`solve` |
| 箱・変数数・固定する成分・設定が変わる | `reset`の後に`solve` |
| 正しい勾配を簡単に計算できる | 二引数形式で、まずAutoを比較候補にする |
| 勾配が分からない・不連続な採点を使う | 一引数形式から始める |
| 既に良い解を局所改善したい | `Search::Refine`と初期解 |
| 整数化や修復が必要 | 変換後を直接採点するか、本来の最良出力を別途保存する |
| 大きな問題の一部だけ変更する | 固定変数、または自由成分だけの小さい問題 |

目的関数の1回が重いなら、ライブラリ内部の小さな処理より、Step 24のような目的関数側の整理が効く場合があります。どの手法が良いかは、実際に使う問題群と同じ時間制限で、複数のseed・初期解を使って比較します。

## 付録A. APIと入力契約の一覧

ここからは、読み終えた後に引くための一覧と、実装を理解するための説明です。新しい使い方を試す場合は、対応するStepへ戻れます。

### A.1 公開されている型

| 型 | 持つ内容 | 初出 |
|---|---|---|
| `BoxBound` | `double lower, upper`。1変数の上下限 | Step 01 |
| `BoxProblem` | `vector<BoxBound> bounds`。全変数の箱 | Step 01 |
| `BoxBudget` | 時間・評価回数・目標値 | Step 01、04、07 |
| `BoxParam` | 探索方針・seed・初期幅・手法 | Step 08〜15 |
| `BoxResult` | 最良点・目的値・回数・時間・終了理由 | Step 05 |
| `BoxOptimizer` | 箱と設定、継続可能な探索状態 | Step 02、06、17 |

### A.2 呼び出しの対応

| 呼び出し | 省略時の扱い | 役割 |
|---|---|---|
| `BoxOptimizer(problem, param)` | `param`は既定設定 | 箱と設定を保持する |
| `reset(problem, param)` | `param`を省略すると既定設定へ戻る | 箱と設定を入れ替え、探索を未開始へ戻す |
| `solve(objective, budget, initial)` | `budget`は既定予算、`initial`は空で箱の中央 | 新しい探索を開始する |
| `resume(objective, budget)` | `budget`は既定予算 | 直前の探索へ追加予算を与える |
| `box_optimize(problem, objective, budget, initial, param)` | 後ろの3引数は順に既定予算・空初期解・既定設定 | 構築から1回の探索までまとめる |

`solve`と`box_optimize`の初期解は`span<const double>`で受け取ります。要素型が`double`の`vector`や`array`を渡せます。未指定を表す空の初期解は、非空の問題でも「箱の中央」を意味します。

関数オブジェクトはsolver内に保存しません。`solve`や`resume`の各呼び出しで渡します。コピーできないラムダでも利用できます。参照している外部データは、その呼び出し中に有効である必要があります。

`resume`では目的関数の意味と、一引数・二引数の形式を保ちます。solverは外部データの同一性を自動判定できません。同じオブジェクトに目的関数の内部から再入する使い方もできません。

### A.3 予算の既定値

| 項目 | 既定値 | 指定上の注意 |
|---|---|---|
| `time_limit_us` | 1,950,000 | 有限値。負で時間制限なし、0で新規評価なし |
| `max_evaluations` | `LLONG_MAX` | 0以上。時間無制限なら`LLONG_MAX`未満が必要 |
| `target_cost` | 未指定 | 指定するなら有限値。最良目的値がこれ以下で終了 |

予算は毎回の呼び出しに対する値です。`resume`の時間や評価回数も新たに数えます。過去の最良点は保持するので、再開時点で目標に達していれば、新しい評価なしで`TargetReached`を返します。

### A.4 探索設定の既定値

| 項目 | 既定値 | 指定上の注意 |
|---|---|---|
| `search` | `Explore` | `Refine`は局所停止で終了 |
| `seed` | 1 | `uint64_t`。新しい`solve`ごとにこのseedへ戻す |
| `initial_step` | 空 | 指定時は全変数数と同じ長さ。固定成分の値は使わない |
| `method` | `Auto` | 下表の4種類 |

| 手法 | 一引数の目的関数 | 二引数の目的関数 |
|---|---|---|
| `Auto` | 座標探索。Exploreでは停滞後に焼きなまし・対角CMA区間も使う | 自由変数があれば投影L-BFGS型 |
| `Coordinate` | 座標探索 | 座標探索。勾配配列は空 |
| `DiagonalCma` | 対角CMA系 | 対角CMA系。勾配配列は空 |
| `SpectralGradient` | 自由変数があれば不正 | 投影付きスペクトル勾配 |

全固定・空の問題では、どの手法でも予算があれば1回だけ値を評価し、勾配は要求しません。一引数と二引数の両方で呼べる関数オブジェクトでは、二引数形式が優先されます。

### A.5 有効な入力の範囲

上下限は有限で、下限が上限を超えないようにします。両者が等しい成分は固定変数です。初期解を指定するなら長さは全変数数で、各成分は箱内です。目的値は、評価される箱内・境界上で有限にします。無効な候補を表すために`NaN`や無限大を返す契約ではありません。

初期幅を指定した自由成分は、正で箱幅以下にします。固定成分の幅は無視します。二引数形式で非空の勾配を要求されたら、全変数分を有限値で埋めます。

実装は通常の`double`を使います。箱の幅と逆数、正規化した初期幅の二乗、勾配の単位変換、内積などを無理なく表現できる尺度にしてください。例えば、有限値同士でも、差や二乗が表現範囲を超える入力は対象にしません。

想定範囲は全変数数100,000以下、自由変数数10,000以下、直近の`solve`からの累計評価10億回以下です。これは、その最大規模で短時間に良い解が求まるという性能保証ではありません。

不正入力は主に`assert`で扱います。`-DNDEBUG`で検査を無効にしても、入力契約が緩くなるわけではありません。例外を捕まえて復旧するAPIや、目的値の異常を自動で修復する機能はありません。

## 付録B. 実装の全体像

### B.1 4つの役割に分けて読む

実装は、次の役割に分けると追いやすくなります。

| 役割 | 主な関数 | 処理 |
|---|---|---|
| 問題を準備する | `reset`、`begin` | 自由変数を取り出し、新しい探索の初期点を作る |
| 次の候補を作る | `propose`、各`propose_*` | 選択中の手法で未評価の候補を作る |
| 評価して状態へ反映する | `evaluate`、各`consume_*` | 目的関数を呼び、最良点・現在点・手法の状態を更新する |
| 予算と再開を管理する | `run` | 評価前後の論理的な停止条件と、評価直前の時刻を確認する |

一回の新しい探索は、概ね次の順で進みます。

1. 初期解を準備し、過去の最良値・乱数状態・探索履歴を新しい探索の状態へ戻す。
2. 予算があれば初期解を評価し、最初の最良点として保存する。
3. 選ばれた手法の状態を準備する。
4. 次の候補を作り、評価を開始できるかを確認する。
5. 目的関数を呼び、必要なら最良点を更新する。
6. その手法の受理・棄却や世代処理へ結果を渡し、停止条件まで繰り返す。

`resume`では1からやり直さず、保持している状態へ新しい予算を適用します。

### B.2 全変数と自由変数を分ける

ここでは全変数数を$n$、自由変数数を$d$とします。固定変数は、下限と上限が同じ成分です。

自由変数は内部で0〜1へ変換します。元の添字$i$の箱幅を$w_ {i}=u_ {i}-l_ {i}$とすると、正規化値$z_ {j}$と元の値$x_ {i}$の関係は次のとおりです。$j$は自由変数だけを詰めた配列の添字です。

$$z_ {j}=\frac{x_ {i}-l_ {i}}{w_ {i}},\qquad x_ {i}=l_ {i}+w_ {i}z_ {j}$$

`free_`が$j$から$i$への対応、`widths_`が箱幅を持ちます。固定変数はこの配列に入らないので、箱幅0で割りません。初期幅も箱幅で割って、正規化した尺度へ直します。

目的関数へ渡すのは、自由変数だけの正規化配列ではなく、固定成分も含む元単位の`x_`です。上端を表す正規化値1は、元の上限そのものへ戻します。丸めでわずかに箱外へ出ることを避ける処理も入ります。

初期解だけは、正規化して元へ戻す往復変換を通さず、呼び出し側が渡した値を最初の評価へそのまま渡します。

### B.3 現在点と最良点は別

`current_`は次の提案の出発点となる現在点、`trial_`は未評価または評価中の候補です。`best_z_`は最良点の正規化座標、`best_x_`は実際に評価した元座標です。

座標探索や勾配探索では、現在点は受理条件に従って更新されます。焼きなましでは、悪くなる候補を現在点として受理する場合もあります。CMAでは複数の評価点を使って分布を更新します。このため「現在点」と「これまでの最良点」を1つにまとめられません。

最良点の更新は全手法に共通で、`evaluate`内で行います。候補の値が過去の最良値より**厳密に小さい**場合だけ更新するので、同値の点では先に得た点を保持します。

ラインサーチで現在点として受理されなかった候補や、途中で終了したCMA世代の候補も、実際に評価して最良値を更新したなら結果になります。未評価の平均点を結果として返すことはありません。

### B.4 途中の候補を消さない

`pending_`は未評価候補の有無、`ready_`は現在の探索法の状態を初期化済みか、`first_`は初回評価前かを表します。

候補を作ったところで時間が切れた場合、その候補を保持して返ります。`resume`では、その候補を使って続けます。乱数を引き直して別の候補に差し替えません。

`need_gradient_`は新しい探索の開始時に決まり、その探索中は不変です。Autoの勾配なし経路が手法を切り替えても、途中から勾配を要求する経路へ変わることはありません。

## 付録C. 座標探索の詳細

### C.1 1座標ずつ、両側を調べる

現在の正規化点を$z$、調べる座標を$i$、その座標の幅を$h_ {i}$、成功しやすかった方向を$q_ {i}$とします。$q_ {i}$は+1または−1です。

最初に$z_ {i}+q_ {i}h_ {i}$を試し、悪くなれば反対側$z_ {i}-q_ {i}h_ {i}$を試します。値は0〜1へ収めます。ほかの座標は現在点と同じです。

境界へ収めた結果が現在点と同じなら、その側は目的関数を呼ばずに飛ばします。全く同じ点を評価して回数を使う必要がないためです。

### C.2 成功と失敗で幅を調整する

| 結果 | 現在点 | 次の幅・方向 |
|---|---|---|
| 目的値が現在値以下 | 候補へ更新する | 幅を1.2倍にする。ただし正規化幅0.5まで |
| 最初の側で悪化 | 変えない | 同じ座標の反対側を試す |
| 両側とも悪化 | 変えない | その座標の幅を半分にする |

反対側が成功した場合は、次からそちらを最初に試すよう、方向の符号を反転します。同値も現在点として受理しますが、最良点の保存は厳密改善だけなので、2つの条件は異なります。

全座標を1巡しても目的値の厳密な改善がなければ、停滞した巡回数を増やします。4巡以上続けて改善がない間は、各巡回の終わりに全幅を半分にします。

すべての幅が正規化座標で$10^{-10}$未満になると局所停止条件です。現在調べる幅がまだ大きければ全幅の最大値を走査する必要がないため、まずその座標だけを見ます。

### C.3 候補のコピーと単位変換を減らす

普通に書くと、1座標だけの変更でも、毎回$d$要素すべてをコピーして元単位へ戻せます。この実装では、連続する座標探索について、変更した部分だけを扱います。

候補を作るときは前回提案で触った座標を現在点の値へ戻し、今回の座標を書き換えます。目的関数へ渡す配列への変換も、前回評価した軸と今回の軸だけ更新します。

「前回提案した軸」と「前回評価した軸」は、境界で候補を飛ばすと異なるので、別々に記録しています。手法切替や再始動時は全座標を変換します。

この削減は内部の候補準備に対するものです。目的関数の全走査や、最良点を新しく保存するときの全体コピーまで消えるわけではありません。

## 付録D. 勾配を使う探索の詳細

### D.1 記号の準備：配列の掛け算と長さ

ここでは配列をベクトルとも呼びます。2つの配列$a,b$の内積は、同じ位置の要素を掛けて足した値です。

$$a\cdot b=\sum_ {i=0}^{d-1}a_ {i}b_ {i}$$

配列$a$の長さを表す量は、次のように定めます。

$$\lVert a\rVert=\sqrt{a\cdot a}$$

2成分なら、三平方の定理で求める矢印の長さと同じです。以後、位置の変化や勾配の変化をこの記号で簡潔に表します。

### D.2 元の勾配を正規化座標の勾配へ変える

正規化座標$z_ {j}$が少し増えると、元の座標$x_ {i}$は箱幅$w_ {i}$倍だけ増えます。そのため、正規化座標での傾きは元の傾きの箱幅倍になります。

$$g^{z}_ {j}=w_ {i}g^{x}_ {i}$$

$g^{x}$が利用者から受け取る勾配、$g^{z}$が内部で使う勾配です。以下では、内部の勾配を単に$g$と書きます。

### D.3 箱の外へ向かう成分を除く

最小化では一般に勾配と反対側へ動きます。しかし、下限にいるのにさらに下げる方向へは動けません。

| 現在の位置と勾配 | 素朴な移動方向 | 実装の扱い |
|---|---|---|
| 下端0にいて勾配が正 | 小さい側へ動きたい | その成分を0にする |
| 上端1にいて勾配が負 | 大きい側へ動きたい | その成分を0にする |
| それ以外 | 箱内へ動ける可能性がある | 成分を残す |

このように、境界で動けない成分を取り除いた勾配を方向作成の出発点にします。全成分が0なら、その場所からの局所探索を終える条件になります。

初回の尺度$\gamma$は、正規化初期幅の最大値を、動ける成分の勾配絶対値の最大で割った値です。0で割らないため分母の下限に$10^{-300}$を使います。境界の外へ向かう大きな勾配だけに引きずられて、動ける変数の幅まで極端に小さくなることを避けています。

### D.4 過去の移動から「曲がり方」を推定する

Autoの勾配経路は投影L-BFGS型です。L-BFGSという名前を知らなくても、まずは「過去にどちらへ動くと、傾きがどう変わったかを使って、次の方向を作る」と考えられます。

受理した移動前後の位置を$z,z'$、勾配を$g,g'$とすると、保存候補は次の2つです。

$$s=z'-z,\qquad y=g'-g$$

$s$は位置の変化、$y$は傾きの変化です。この組を最大8組保持します。大きな正方行列を丸ごと作る代わりに、短い履歴から方向へ補正を加えます。

履歴として使うのは、次の条件を満たす場合だけです。

$$s\cdot y>10^{-10}\sqrt{(s\cdot s)(y\cdot y)}$$

左辺が正で十分大きいときだけ、正の曲率情報として利用します。例えば、1変数の上に開く二次関数で右へ動けば傾きも増えるので、この積は正になります。小さな丸め誤差だけの変化や、不適切な向きの変化をそのまま履歴に使いません。

この条件を満たさなかった場合でも、目的値による受理条件を満たした候補は現在点へ移れます。「履歴を追加しない」と「移動を棄却する」は別です。

### D.5 L-BFGS型の二重ループ

実装の`gd_.s`と`gd_.y`が履歴を持ちます。履歴の番号を$j$とし、次の値も保存します。

$$\rho_ {j}=\frac{1}{s_ {j}\cdot y_ {j}}$$

ここで$s_ {j}$と$y_ {j}$は、その履歴に対応する配列全体です。D.4の$s$の成分番号とは異なり、この節の$j$は**何組目の履歴か**を表します。

方向は次の2回の巡回で作ります。

1. 動ける成分だけ残した現在の勾配を$q$とする。
2. 履歴を新しい順に読み、各組の影響を$q$から取り除く。
3. 残った$q$へ尺度$\gamma$を掛けて$r$とする。
4. 履歴を古い順に読み、補正を$r$へ戻す。
5. 符号を反転した$-r$を、下る方向の候補にする。

新しい順の処理は次の計算です。

$$\alpha_ {j}=\rho_ {j}(s_ {j}\cdot q),\qquad q\leftarrow q-\alpha_ {j}y_ {j}$$

次に$r=\gamma q$とし、古い順に次を計算します。

$$\beta_ {j}=\rho_ {j}(y_ {j}\cdot r),\qquad r\leftarrow r+s_ {j}(\alpha_ {j}-\beta_ {j})$$

$\alpha_ {j}$と$\beta_ {j}$は、それぞれの巡回で計算する補正用の数値です。配列全体に1つの数値を掛ける計算と内積で実行できます。履歴数を$m$とすると、必要な処理はおおむね$m$回の$d$要素走査で、$d$行$d$列の行列は作りません。

作った方向にも、境界の外へ向く成分があれば0を入れます。方向と元の勾配の内積が0以上なら、その方向が下り方向とはいえないため、単純な投影勾配方向へ戻します。

### D.6 どれだけ進むかを、実際の目的値で確かめる

方向を$p$、倍率を$\eta$とします。倍率は最初1で、候補は$z+\eta p$を箱へ収めた点です。

ここで$P$は「各成分を0〜1に収める操作」を意味すると定義します。

$$z'=P(z+\eta p),\qquad \Delta=z'-z$$

箱へ収めることを投影と呼びます。境界で切り詰めると、実際の移動$\Delta$は$\eta p$と異なる場合があります。そのため、実装は投影後の移動と勾配の内積$g\cdot\Delta$を計算し、本当に下り方向かを確認します。

下り方向にならない場合は、履歴を捨て、$P(z-\gamma g)-z$という単純な方向で作り直します。それでも不適切なら局所停止です。

評価した候補は、次の条件で受理します。

$$f(z')\le f(z)+10^{-4}(g\cdot\Delta)$$

ここで$f(z)$は、正規化点を元単位へ戻して計算した目的値を略記しています。$g\cdot\Delta$が負なので、右辺は現在値より少し小さくなります。「ただ微小に良くなっただけでなく、傾きから期待した方向に一定程度改善したか」を確認する条件です。

満たさなければ$\eta$を半分にして、同じ方向を短く試します。これがバックトラック付きラインサーチです。実装では、最大成分の移動が$10^{-13}$未満、投影後が下り方向でない、または棄却が40回に達する場合などを局所停止条件とします。

### D.7 尺度の更新とSpectralGradient

良い移動が得られ、D.4の曲率条件も満たしたら、次の尺度を更新します。$s,y$は直近の受理移動です。

| 経路 | 尺度 |
|---|---|
| SpectralGradient | $(s\cdot s)/(s\cdot y)$ |
| Autoの勾配経路、Explore | $\sqrt{(s\cdot s)/(y\cdot y)}$ |
| Autoの勾配経路、Refine | $(s\cdot y)/(y\cdot y)$ |

最初と最後の式はそれぞれBB1、BB2と呼ばれる尺度です。真ん中は両者の幾何平均に対応します。固定幅だけを使い続けず、最近の位置と傾きの変化から、適切そうな幅を推定しています。

SpectralGradientでは履歴数を0のままにするので、D.5の履歴を巡るループを実行しません。一方、境界の投影、方向の確認、ラインサーチ、再始動、最良点の管理は共通です。

## 付録E. 焼きなまし区間の詳細

### E.1 この区間が使われる場面

勾配なしの`Auto + Explore`では、座標探索が局所停止すると、過去の最良点を出発点にして焼きなまし区間へ移ります。公開されている`Method`に、焼きなまし単独の指定はありません。

ここでの狙いは、小さな改善だけを受理していると抜けにくい場所から、少し悪くなる移動も交えて探索を続けることです。

### E.2 候補の作り方

自由変数から1つの座標$i$を選び、正規化初期幅$h_ {i}$を使って、次のような変更を加えます。

$$z'_ {i}=P(z_ {i}+h_ {i}\cdot 0.01^{U}N)$$

ここで$U$は0以上1未満の一様乱数、$N$は平均0・標準偏差1の正規乱数です。$P$は0〜1へ収める操作です。正規乱数は、0付近が出やすく、正負の両方向が出る乱数と考えてください。

`steps_`にある初期幅を使うので、座標探索が停止する直前に細かく縮めた幅を、そのまま使い続けるわけではありません。ほかの座標は現在点と同じです。

### E.3 悪化の受理と温度

目的値の差を$\delta=f(z')-f(z)$とします。$\delta\le0$なら受理します。悪化する場合は、次の確率に従って受理するかを決めます。

$$p=\exp\left(-\frac{\delta}{\max(10^{-300},T)}\right)$$

$p$が受理確率、$T$が温度、$\exp(a)$は自然対数の底$e$の$a$乗です。温度が高いほど悪化を受け入れやすく、低いほど受け入れにくくなります。

温度は、この区間で初めて得た0でない目的値差の絶対値から設定します。その後、1評価ごとに次の倍率を掛けます。

$$T\leftarrow T\exp\left(-\frac{8}{50d}\right)$$

$d$は自由変数数です。この区間は$50d$回の評価で終わります。温度を推定できる変化が遅く現れれば、それまで温度は0のままです。

悪化候補を現在点にしても、過去の最良点は消しません。終了時は最良点を基点として、最大$100d$評価の短い対角CMA区間へ移り、その後また座標探索へ戻ります。

## 付録F. 対角CMA系の詳細

### F.1 1つの点ではなく、候補を出す分布を調整する

CMAの区間では、候補の中心$m$、全体の幅$\sigma$、座標ごとの広がり$a_ {i}$を持ちます。$m$は$d$成分の配列、$\sigma$は1個の正の数、$a_ {i}$は各座標の正の数です。

初めは$m$を現在点、$\sigma$を0.25にします。正規化初期幅$h_ {i}$に対し、$a_ {i}=h_ {i}/\sigma$とするので、積$\sigma a_ {i}$が初期幅に一致します。

ある世代で複数の候補を評価し、良かった候補を重く扱って、次の中心と広がりを決めます。ここで「世代」とは、まとめて分布更新に使う候補の一まとまりです。

### F.2 世代サイズと重み

世代の候補数を$\lambda$、分布更新へ使う上位候補数を$\mu$とします。

$$\lambda=4+\lfloor3\log d\rfloor,\qquad \mu=\lfloor\lambda/2\rfloor$$

$\log$は自然対数、$\lfloor a\rfloor$は$a$以下の最大整数です。この処理に入る時点では$d\ge1$です。例えば$d=1$なら$\lambda=4,\mu=2$です。

上位候補を良い順に$j=0,\ldots,\mu-1$とし、次の正の重みを作ります。

$$\widetilde{w}_ {j}=\log(\mu+0.5)-\log(j+1),\qquad w_ {j}=\frac{\widetilde{w}_ {j}}{\sum_ {k=0}^{\mu-1}\widetilde{w}_ {k}}$$

$\widetilde{w}$は正規化前、$w$は合計が1になるように直した重みです。良い順位ほど重くします。$k$は合計を取るための添字です。

次の$\kappa$は、重みの偏りを考慮した「有効な候補数」を表す係数です。

$$\kappa=\frac{1}{\sum_ {j=0}^{\mu-1}w_ {j}^{2}}$$

ソース中の名前は`mueff`です。すべて同じ重みなら$\kappa=\mu$になり、1候補へ偏るほど小さくなります。

### F.3 正規乱数と、反対側の候補

各ペアの最初の候補は、座標ごとに独立した標準正規乱数$N_ {i}$を使って作ります。

$$z_ {i}=P(m_ {i}+\sigma a_ {i}N_ {i})$$

次の候補は、直前の候補を中心$m$に対して反転したものです。

$$z^{\mathrm{mirror}}_ {i}=P(2m_ {i}-z_ {i})$$

ここで使う$z$は、**箱へ収めた後の実際の候補**です。はみ出す前の乱数の符号を単に反転するものとは、境界付近で異なります。反転候補では新しい正規乱数を生成しません。世代サイズが奇数なら、最後の候補はペアの最初の側だけになります。

「対角」は、座標ごとに広がりを持ち、座標同士の共分散を全組み合わせでは持たないという意味です。共分散は、2つの座標が一緒に増減する傾向を表す量です。分布の記憶量を抑える代わりに、斜め方向へ細長く傾いた分布を自由に表すことはできません。

### F.4 良い候補から中心の移動を作る

世代全体を評価したら、目的値で順位付けします。同じ値なら元の候補番号で順序を決めます。上位$j$番目の候補を$z^{(j)}$とします。

正規化した平均移動$\Delta$は次の式です。

$$\Delta_ {i}=\sum_ {j=0}^{\mu-1}w_ {j}\frac{z^{(j)}_ {i}-m_ {i}}{\sigma}$$

中心は$m+\sigma\Delta$へ更新します。重みの合計が1なので、これは上位候補の重み付き平均と同じです。

重要なのは、箱の外に生成した仮の位置ではなく、実際に評価した箱内の位置から移動を計算することです。

### F.5 続けて同じ方向へ進んでいるかを記録する

最近の平均移動を少しずつ蓄積した2本の配列を持ちます。`path_s`に対応する$p_ {s}$と、`path_c`に対応する$p_ {c}$です。古い値を少し薄め、新しい移動を加えるので、何世代も同じ方向へ進むと大きくなりやすくなります。

幅調整用の$p_ {s}$は、座標ごとの広がりで割った移動を使います。

$$p'_ {s,i}=(1-c_ {s})p_ {s,i}+\sqrt{c_ {s}(2-c_ {s})\kappa}\frac{\Delta_ {i}}{a_ {i}}$$

この式の$c_ {s}$は、新しい移動をどの程度取り込むかを決める正の係数です。プライム記号付きが更新後です。最初は$p_ {s}=0$です。

その長さ$L=\lVert p'_ {s}\rVert$が通常より長いかを調べます。標準正規乱数からなる$d$次元ベクトルの長さの目安を、次の$\chi$で近似しています。

$$\chi=\sqrt{d}\left(1-\frac{1}{4d}+\frac{1}{21d^2}\right)$$

初期の0から積み上げたことを補正するため、更新世代数を$G$として、次の判定で$h$を1または0にします。

$$h=1\quad\text{if}\quad\frac{L}{\sqrt{1-(1-c_ {s})^{2G}}}<\left(1.4+\frac{2}{d+1}\right)\chi$$

条件を満たさなければ$h=0$です。$G$は最初の更新で1です。極端に長い経路が出たとき、そのまま次の配列へ強く足し込まないための判定です。

もう一方の経路は次のように更新します。

$$p'_ {c,i}=(1-c_ {c})p_ {c,i}+h\sqrt{c_ {c}(2-c_ {c})\kappa}\Delta_ {i}$$

$c_ {c}$も新しい移動を取り込む係数です。ここでは座標の広がりで割る前の移動を使います。

### F.6 座標ごとの広がりを更新する

座標ごとの分散に相当する値を$C_ {i}=a_ {i}^{2}$として保持します。良い候補が、その座標で中心からどの程度離れていたかも計算します。

$$v_ {i}=\sum_ {j=0}^{\mu-1}w_ {j}\left(\frac{z^{(j)}_ {i}-m_ {i}}{\sigma}\right)^2$$

ここでも$m,\sigma$は、その世代の候補を作ったときの値です。古い分散、続けて進んだ方向、良い候補の散らばりを次のように混ぜます。

$$b=1-c_ {1}-c_ {\mu}+(1-h)c_ {1}c_ {c}(2-c_ {c})$$

$$C'_ {i}=bC_ {i}+c_ {1}(p'_ {c,i})^2+c_ {\mu}v_ {i}$$

$c_ {1}$は経路からの寄与、$c_ {\mu}$は上位候補群からの寄与を決める係数です。$b$は古い分散をどれだけ残すかの係数です。更新後の広がりは$a'_ {i}=\sqrt{C'_ {i}}$です。

全体の幅$\sigma$は、経路の長さ$L$と基準$\chi$を比較して更新します。

$$\sigma'=\sigma\exp\left(\min\left(0.6,\frac{c_ {s}}{D}\left(\frac{L}{\chi}-1\right)\right)\right)$$

$D$は変化を抑える係数です。経路が基準より長ければ幅を広げる側、短ければ狭める側へ働きます。1世代の拡大に使う指数の上限を0.6にしています。

### F.7 コードで使う係数

ここは、数式と実装を対応させたいときの参照用です。利用者が公開APIから設定するパラメータではありません。

まず$d$と$\kappa$から、次の係数を作ります。

$$c_ {c}=\frac{4+\kappa/d}{d+4+2\kappa/d},\qquad c_ {s}=\frac{\kappa+2}{d+\kappa+5}$$

初期の学習率を$\widehat{c}_ {1},\widehat{c}_ {\mu}$とします。

$$\widehat{c}_ {1}=\frac{2}{(d+1.3)^2+\kappa}$$

$$\widehat{c}_ {\mu}=\min\left(1-\widehat{c}_ {1},\frac{2(\kappa-2+1/\kappa)}{(d+2)^2+\kappa}\right)$$

この実装では、対角成分の学習に使う倍率$r=(d+1.5)/3$を掛けてから上限を適用します。

$$c_ {1}=\min(0.5,r\widehat{c}_ {1}),\qquad c_ {\mu}=\min(1-c_ {1},r\widehat{c}_ {\mu})$$

幅の変化を抑える係数は次のとおりです。

$$D=1+2\max\left(0,\sqrt{\frac{\kappa-1}{d+1}}-1\right)+c_ {s}$$

F.5〜F.7をまとめると、良い候補の平均方向を作り、その方向がどの程度続いているかを記録し、座標別の幅と全体の幅を更新する流れです。

### F.8 停滞と途中停止

分布更新は、1世代分を評価し終えた場合だけ行います。途中で予算が切れたら、既に評価した候補や次の候補番号を保持し、`resume`で続けます。最良点の更新は各評価ですでに済むため、途中世代の良い候補も結果に入ります。

世代の最良値が、このCMA区間内のそれまでの世代最良値を更新しなければ、停滞世代数を増やします。この比較対象は、その探索全体の最良点とは別に持ちます。

局所停止は、$\sigma$と最大の広がりの積が$10^{-10}$未満、または停滞世代数が$20+\lfloor20d/\lambda\rfloor$を超える場合です。Auto内の短い区間なら、別途設定した評価回数を使い切った場合も座標探索へ戻ります。

## 付録G. 再始動・乱数・停止管理の詳細

### G.1 局所停止後の行き先

| 現在の使い方 | 局所停止したとき |
|---|---|
| `Search::Refine` | `LocalStop`で終了する |
| 勾配なしの`Auto + Explore`の座標区間 | 焼きなまし、短い対角CMA、座標探索の順で続ける |
| Auto内の短いCMA区間 | 最良点から座標探索へ戻る |
| 明示したCoordinate・DiagonalCmaのExplore | 別の初期点を提案して再始動する |
| 勾配経路のExplore | 勾配用の再始動点を提案する |

再始動点は、作っただけでは最良点になりません。通常の候補と同じく、予算を確認した後で目的関数を呼びます。

### G.2 再始動点の作り方

一般の再始動では、回数が奇数なら箱内の一様乱数、偶数なら過去の最良正規化点へ標準偏差0.15の正規乱数を足し、箱内へ収めた点を候補にします。

勾配経路では、その後さらに候補を調整します。8回に1回を除き、最良点の1座標だけを選んで一様乱数へ変更した候補を使います。8回目ごとは、全座標へ正規乱数を足した候補になります。

現在点や各手法の状態は新しい点から作り直しますが、探索全体での最良点は保持します。Exploreの再始動は、`solve`を最初から呼び直す操作とは異なります。

### G.3 乱数の保持

内部乱数は64bit状態のSplitMix64系です。0〜1の一様乱数は53bitを使って作り、正規乱数は一様乱数から2個ずつ作って、余った1個を次の呼び出し用に保持します。

新しい`solve`では、乱数状態も`param.seed`からやり直します。`resume`ではその状態を続けます。目的関数の外側で使う乱数までは管理しません。

同じ候補が必要な検証では、時間上限ではなく評価回数上限を使うと、実行時間の揺れによる停止位置の変化を切り離せます。ただし、浮動小数点や標準ライブラリの関数の違いがある別環境で、末尾まで同一になる保証ではありません。

### G.4 評価の窓口を1つにする

全手法の目的評価は`evaluate`を通ります。ここで、元の単位への変換、必要な勾配の準備、目的関数呼び出し、回数の加算、最良点の更新をまとめます。

二引数で呼べる関数には、常に二引数で呼び出します。勾配不要なら第2引数を空にします。勾配が必要な探索での配列準備は、最初に評価するときに行うため、初期点を評価する前に予算が尽きれば、そのための配列を新しく確保せずに済みます。

### G.5 時計と、論理的な停止条件

`run`は、目標値・全固定・局所停止・評価回数を先に判定します。時刻は、runの初回と、その後の各候補の評価直前に確認します。同じ評価のために候補作成の前後で重複して時計を読むことを避けています。

このため、ある目的評価の最中に時間切れになった場合、次の候補を作る内部処理が1回入ってから時刻を確認する場合があります。候補を作っただけでは評価回数は増えません。その候補は未評価状態として保持されます。

結果を返すときは、最良点の元座標を独立した配列へコピーします。このコピーも時間を使います。時間上限を「その瞬間に処理を強制中断する仕組み」と解釈しない理由は、これらの内部処理と利用者のコールバックにあります。

## 付録H. 計算量と、目的関数側でできる工夫

この表では$n$を全変数数、$d$を自由変数数、$m$をL-BFGS履歴数、$\lambda$をCMAの世代サイズとします。$m$は最大8です。

`O(d)`は、変数数$d$におおむね比例する量という意味です。実測時間そのものではなく、問題が大きくなったときの増え方を表します。

| 処理 | 主な計算量。目的関数の評価は別 |
|---|---|
| 問題の前処理・初期解の用意 | `O(n)` |
| 通常の座標候補作成・連続時の座標変換・受理 | `O(1)`。巡回終了時や幅の確認などで`O(d)`の処理が入る |
| 焼きなましの候補作成・変換 | `O(d)` |
| L-BFGS型の方向作成 | `O(md)` |
| SpectralGradientの方向作成 | `O(d)` |
| 勾配経路の各候補作成・変換 | `O(d)` |
| 対角CMAの1世代 | `O(λd + λ log λ)` |
| 最良点の保存 | `O(n+d)` |
| 結果の所有配列を作る | `O(n)` |

勾配経路では、利用者の目的関数自身も全変数分の勾配を書きます。デバッグビルドでは勾配成分の有限値検査も行います。したがって内部の自由変数数だけを見て、全体の評価費用を見積もらないようにします。

保持する作業領域は、座標探索・焼きなまし・SpectralGradientが概ね`O(n+d)`、L-BFGS型が`O(n+md)`、対角CMAが`O(n+λd)`です。過去の実行で確保した容量は再利用のため残る場合があります。例えばAutoの勾配経路からSpectralGradientへresetしても、以前確保した履歴用容量が即座に解放されるという意味ではありません。

実用上の改善では、次の順に考えやすいでしょう。

1. 目的関数が毎回、同じ定数項や同じグラフ情報を作り直していないか確認する。
2. 動かさない変数が多ければ、固定するか自由成分だけに詰める。
3. 正しい勾配を安く計算できるなら、同じ中間計算を値と勾配で共有する。
4. 時間制限・評価回数・初期解を揃えて手法を比べる。
5. コードが複雑になる変更は、実際の問題群で効果を確認してから採用する。

目的関数の中で使う大きな配列は、可能なら外側で準備して再利用します。ただし、配列を再利用することと、候補に依存する値を古いまま残すことは別です。候補の採否を目的関数側が勝手に仮定してはいけません。

## 付録I. つまずきやすい点

| 症状・疑問 | 確認すること |
|---|---|
| 1回も評価されない | 時間0、評価回数0、または非常に短い時間を指定していないか |
| 2秒のつもりなのにすぐ終わる | `time_limit_us`はマイクロ秒。2000は2ミリ秒 |
| `resume`しても評価が増えない | 目標到達・LocalStop・全固定の状態がそのままではないか |
| ターンが変わった後の結果がおかしい | 評価データが変わったのに`resume`していないか |
| パラメータを書き換えても変わらない | 構築後の外側の`param`だけを変えていないか。`reset`へ渡す |
| 勾配を書いたら範囲外アクセスになる | `gradient.empty()`を確認し、非空のときだけ書いているか |
| 勾配を渡したのに改善しない | 符号、添字、重み、元単位での微分、変数変換の連鎖を検算する |
| 結果の`x`が空 | 未評価だけでなく変数数0もある。`has_value()`で判断する |
| 目的値は改善したのに提出得点が悪化 | 代理目的・丸め・修復後の本来の得点を別に確認する |
| 固定変数を増やしてもあまり速くならない | 目的関数が全体走査や大きなコピーを続けていないか |
| 同じseedでも時間制限では結果が変わる | 時間内に進める評価回数が変わり得る。回数固定でも比較する |
| 勾配なしでSpectralGradientを使えない | 自由変数があれば二引数形式が必要 |
| 無効点に無限大を返すとassertになる | 有限な目的値が契約。実行可能な点への変換や適切な制約付きソルバーを考える |

## 付録J. 例集の使い方、1ファイル化、確認結果

### J.1 同梱ファイル

例集にはこのMarkdown、`box_optimizer_v06.hpp`、全25個の`step*.cpp`、検証用スクリプト、実行結果を収録しています。各コードは本文のC++ブロックをそのまま取り出したものです。

好きな例を`main.cpp`へコピーしても、ファイル名を指定してそのままコンパイルしても使えます。

```sh
g++ -std=c++20 -O2 step11_gradient.cpp -o step11
./step11
```

全例をまとめて確認する場合は、例集のフォルダで次を実行します。

```sh
CXX=g++ python3 verify_examples.py
```

この検証はC++20コンパイラとPython標準ライブラリで動きます。数学表示の検証に使った環境は別で、例をコンパイルするだけならMarkdown処理系は不要です。

### J.2 AHC提出用にまとめる

同梱の`amalgamate.py`は、`#include "box_optimizer_v06.hpp"`を、直接実行用テストを除いた本体へ置き換えます。

```sh
python3 amalgamate.py step11_gradient.cpp > submission.cpp
g++ -std=c++20 -O2 submission.cpp -o submission
./submission
```

各`step*.cpp`はそれぞれ`main`を持つので、複数例をそのまま連結しません。必要なパターンを理解した後、自分の`main`へ組み込みます。

### J.3 このガイドの確認範囲

GCC 12.2.0、C++20、O2、`-Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined`で全25例をコンパイルし、警告なしで実行しました。既知の最適点、再開時の回数、固定成分、勾配検算、変換後の制約、元の目的値との整合などを確認しています。

時間制限を使う例の評価回数や、小数点以下の末尾は環境で変わるため、固定の出力文字列を正解とはしていません。時間内に必ず目標へ届く、という品質保証を行うテストでもありません。

数式は外部リーダー向けにインラインのドル区切りと、1行の表示式で記述しています。Markdown処理後に式の添字や制御記号が変形していないことを確認しています。閲覧にはTeX形式の数式表示に対応したMarkdownリーダーを使ってください。

## 参考資料

本ライブラリ固有のAPI、条件、定数、実装の流れは、同梱の`box_optimizer_v06.hpp`を基準に説明しています。外部ソルバーの比較には、次の公式資料を参照しました。参照日：2026年9月25日。

1. [OR-Tools：線形計画問題の例][資料1]。専用LPソルバーを選ぶ場合の入口。
2. [CVXPY：凸二次計画問題の例][資料2]。二次目的と一次制約を持つ問題の形。
3. [SciPy：minimize][資料3]。勾配や境界、追加制約に応じた手法のインターフェース。
4. [SciPy：differential_evolution][資料4]。勾配なしで広い範囲を探索する比較候補。
5. [NLopt：Introduction][資料5]。非線形最適化、箱制約、一般制約、局所解と大域解の説明。

[資料1]: https://developers.google.com/optimization/lp/lp_example
[資料2]: https://www.cvxpy.org/examples/basic/quadratic_program.html
[資料3]: https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.minimize.html
[資料4]: https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.differential_evolution.html
[資料5]: https://nlopt.readthedocs.io/en/latest/NLopt_Introduction/
