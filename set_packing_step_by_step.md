# SetPackingSolver ステップバイステップガイド

候補の中から、条件を守りながら利益の合計が大きい組み合わせを選ぶための、C++ライブラリの入門ガイドです。数学は高校程度を前提にし、最小例から、資源の取り合い、前の解の再利用、部分問題としての利用へ進みます。

対象は同梱の `set_packing_solver_v03.hpp` です。過去の設計や会話を読む必要はありません。本文の17個のC++プログラムは、それぞれ独立した完全なプログラムです。前の例の一部を貼り足す必要はありません。

## 読み進め方

| 節 | 学ぶこと |
|---|---|
| 1 | 何を選び、何を最大にするのか |
| 2 | 厳密解・類似ソルバーとの使い分け |
| 3 | 実行の準備 |
| 4：最小コード | 候補、利益、競合、解の取り出し |
| 5〜9：発展①〜⑤ | 反復数、資源、容量、必須・禁止、解なし |
| 10〜13：発展⑥〜⑨ | 実数の利益、解の検査、初期解、継続探索 |
| 14〜17：発展⑩〜⑬ | 数値更新、制約更新、締切、構造変更 |
| 18〜20：発展⑭〜⑯ | 複数の開始状態、候補生成、局所的な解き直し |
| 21 | APIの索引と入力上の注意 |
| 付録 | 実装アルゴリズムの概要と詳細 |

初めてなら第13節までを順に読み、ターン制の問題では第14〜17節を続けて読むとよいでしょう。内部の探索方法を知らなくても、本文のAPIは利用できます。

## 1. 何を解くライブラリか？

### 1.1 「候補を作る」と「候補を選ぶ」を分ける

たとえば、いくつかの作業案があります。それぞれの作業案には得点が付いています。しかし、同じ機械を取り合う作業や、同時には実行できない作業もあります。

このとき必要なのは「一番得点の高い作業を一つ探す」ことではなく、**実行できる組み合わせの中で、合計得点をできるだけ大きくする**ことです。

| 候補ID | 作業を選んだときの利益 | 同時には選べない候補 |
|---|---:|---|
| 0 | 8 | 1、2 |
| 1 | 7 | 0 |
| 2 | 6 | 0 |

候補0だけなら利益は8です。候補1と2を一緒に選ぶと、利益は7＋6＝13です。候補0と1を選ぶ組み合わせは条件違反なので、利益が15でも解として使えません。

このガイドでいう「候補」は、一つの完成した選択肢です。作業、配送案、配置案、複数のマスを使う操作などを候補として登録できます。候補の内容を作るのは呼び出し側です。ライブラリが座標や経路を自動生成するわけではありません。

### 1.2 目的関数：選んだ候補の利益を足す

目的関数とは、「どの解がよいか」を数字で表す式です。本ライブラリの目的関数は次の形です。

$$\max_{x} F(x), \qquad F(x)=\sum_{i=0}^{N-1} w_ {i}x_ {i}$$

登場する記号を一つずつ説明します。

| 記号 | 意味 |
|---|---|
| $N$ | 候補の総数。例では3 |
| $i$ | 候補の番号。0から $N-1$ まで動く |
| $w_ {i}$ | 候補 $i$ を選ぶと得られる利益。入力として与える数字 |
| $x_ {i}$ | 候補 $i$ を選ぶなら1、選ばないなら0にする数字 |
| $x$ | 全候補の選ぶ・選ばないを並べたもの。$x=(x_ {0},x_ {1},\ldots,x_ {N-1})$ |
| $F(x)$ | 選び方 $x$ による利益の合計 |
| $\sum_{i=0}^{N-1}$ | $i=0,1,\ldots,N-1$ の各項を全部足す、という記号 |
| $\max_{x}$ | 条件を満たす選び方 $x$ の中で、後ろの値を最大にしたい、という意味 |

$w_ {i}$ は入力として固定され、$x_ {i}$ が決めたい数字です。$w_ {i}x_ {i}$ は、選んだときだけ $w_ {i}$、選ばなければ0になります。一つの候補を2回選ぶことはできません。

先ほどの候補1と2を選ぶ場合は $x=(0,1,1)$ なので、次の計算になります。

$$F(0,1,1)=8\times0+7\times1+6\times1=13$$

「最大にしたい」という問題設定と、「必ず最大値が求まる」という保証は別です。本ライブラリは限られた計算時間でよい実行可能解を探すものであり、一般の場合に最適解を保証するものではありません。

### 1.3 守らなければならない条件

まず、各候補は選ぶか選ばないかの二択です。

$$x_ {i}\in\lbrace0,1\rbrace\qquad(0\le i<N)$$

ここで $\in$ は「右側の集合に含まれる」を表します。$\lbrace0,1\rbrace$ は0と1からなる集合です。

次に、同時に選べない候補の組を指定できます。

$$x_ {i}+x_ {j}\le1\qquad((i,j)\in E)$$

$j$ も候補番号です。$E$ は同時に選べない候補の組を集めたものです。$(i,j)$ は候補 $i$ と候補 $j$ の組を表します。両方を選ぶと左辺が2になるので、この式に違反します。片方だけ、または両方とも選ばない場合は条件を満たします。

また、「同じ資源を使う候補を、上限以内に収める」という条件があります。

$$\sum_{i=0}^{N-1} a_ {ri}x_ {i}\le c_ {r}\qquad(0\le r<M)$$

| 新しい記号 | 意味 |
|---|---|
| $M$ | 資源の総数 |
| $r$ | 資源番号。0から $M-1$ まで |
| $c_ {r}$ | 資源 $r$ の容量。0以上の整数 |
| $a_ {ri}$ | 候補 $i$ が資源 $r$ を使うなら1、使わなければ0 |

左辺は「資源 $r$ を使う選択済み候補が何個あるか」です。一つの候補は、登録した資源をそれぞれ**1単位ずつ**使います。容量1ならその資源を使う候補は高々一つ、容量2なら高々二つです。資源を二つ使う候補は、それぞれの資源を1単位ずつ使います。

最後に、候補を必須または禁止にできます。固定指定を $f_ {i}$ と書くと、次の対応です。

| $f_ {i}$ | 意味 | 選び方への条件 |
|---:|---|---|
| -1 | 自由 | 選んでも選ばなくてもよい |
| 0 | 禁止 | $x_ {i}=0$ |
| 1 | 必須 | $x_ {i}=1$ |

ライブラリは、これらの条件を**すべて同時に満たす**候補集合を扱います。このような集合を「実行可能解」と呼びます。その中で目的関数が最大のものが「最適解」です。

### 1.4 表現できること・直接は表現できないこと

標準的な集合パッキングは、互いに重ならない集合を選ぶ問題です。本ライブラリは、その考え方に、任意の競合ペア、容量が2以上の資源、必須・禁止の指定を組み合わせたものです。

| したいこと | 表現方法・注意 |
|---|---|
| 同じ注文に対する複数案から高々一つ選ぶ | 注文を容量1の資源にする |
| 同じマスを二つの配置で使わない | マスを容量1の資源にする |
| 全体で高々 $K$ 個選ぶ | 全候補に共通の資源を加え、その容量を $K$ にする。$K$ は選択数の上限 |
| 資源では表しにくい特定の二案を排他にする | 競合ペアに登録する |
| 同じ候補が資源を3単位使う | 直接は表現できない。資源IDを3回書いても1単位に正規化される |
| ちょうど $K$ 個選ぶ／少なくとも一つ選ぶ | 直接は表現できない。容量は上限だけ |
| Aを選んだらBも選ぶ／実行順序を決める | 直接は表現できない |
| 二つを一緒に選んだ場合だけ追加点が入る | 単純な利益の足し算ではないため、そのままでは扱えない |

自由候補の利益が0以下なら、選ばなくても損をせず、条件は緩くなるだけです。そのため本ライブラリは通常の選択対象から外します。ただし、必須候補は利益が負でも選びます。

必須候補がなければ、何も選ばない集合も実行可能です。必須候補がある場合も、必須候補だけで条件を満たせるなら、少なくともその集合が解になります。

## 2. 類似ソルバーと、厳密解を優先したい場合

「厳密解アルゴリズム」は、完了すれば最適解を求められる方法です。同じモデルを正しく解ければ、その解の値は本ライブラリの解以上になります。同じ値になる場合もあり、厳密解だから常に厳密に高得点になるわけではありません。

### 2.1 制約の形が限られている場合

以下は、記載した条件以外の追加制約がない場合の使い分けです。必須候補があるなら、最初にその矛盾を確認し、必須と両立しない候補を除外し、消費済み容量を引いてから残りを考えます。

| 問題の制限 | 検討したい方法 | 使い分けのポイント |
|---|---|---|
| 競合も資源制約もない | 正の利益を持つ自由候補を全部選ぶ | 探索ライブラリを使う必要がない |
| 共通の容量だけがあり、各候補の消費が1 | 正の利益の自由候補を、大きい順に残り容量まで選ぶ | 「高々何個」という条件だけなら単純に解ける |
| 各候補が時間区間で、競合がその区間の重なりだけ | 重み付き区間スケジューリングの動的計画法 | 任意の追加競合や別資源を足すと、この専用解法の対象から外れることがある [1] |
| 各候補が異なる二つの資源を使い、全資源の容量が1。追加競合なし | 最大重みマッチング | 資源を点、候補を二点を結ぶ組とみなせる。「完全マッチング」や「最大個数」とは目的が違う [2] |
| 上の二資源が、必ず左のグループと右のグループから一つずつ | 重み付き二部マッチング、割当・最小費用流ソルバー | 「人と仕事」の対応付けなど。選ばない自由や選択数もモデルに含める必要がある [3][4] |
| 候補数が十分少ない | 全列挙、分枝限定など | 小さい検証問題の正解値を作る用途にも向く。候補数だけで一律に速度を保証できるわけではない |

上の変換条件は、本ガイドの制約式と各問題の定義を照合したものです。たとえば「候補が二つの資源を使う」だけでは不十分で、任意の追加競合があると、通常のマッチングではその条件を扱えません。

この節ではアルゴリズムの中身は説明しません。まず「自分の問題が専用の厳密解法の形に収まっていないか」を確認するのが目的です。

### 2.2 近い分野のソルバー

| 選択肢 | 向いている用途 | 本ライブラリとの違い |
|---|---|---|
| 最大重み独立集合ソルバー | 競合ペアだけで表せる候補選択 | 容量1の資源は利用候補を全ペア競合にすれば表せる。ただし大きなグループでは辺が増える。容量2以上を単純な全ペア競合にしてはいけない |
| 割当・最小費用流ソルバー | 対応付けなど、流れのモデルに収まる問題 | 構造が合えば厳密解を狙える。汎用の競合や任意の資源の重なりまで自動で扱えるわけではない [3][4] |
| OR-Tools CP-SAT | 0/1の選択に、整数の追加条件・等式などを組み合わせたい場合 | 制約は整数でモデル化する。結果には最適性を確認できたかを区別する状態がある。本ライブラリの `feasible` とは契約が異なる [5] |
| SCIPなどの混合整数最適化ソルバー | 線形式で表せる、より広い制約を扱う場合 | 集合パッキング制約を扱う機能もある。外部ソフトウェアの導入・連携が必要 [6] |
| ナップサック用ソルバー | 候補ごとに異なる消費量を、一つまたは少数の予算内に収めたい場合 | 本ライブラリの「候補ごとに1単位」と異なる問題。容量欄へそのまま置き換えない |

CP-SATや混合整数最適化でも、短い時間で必ず最適性の確認まで終わるわけではありません。使用するソルバーの終了状態や上限時間の扱いを確認してください。

本ライブラリは「利益の足し算＋競合＋単位資源容量」に問題が収まり、単一ヘッダで組み込みたい、短い探索を繰り返したい、前の選択を使い回したい場合の候補です。一方、必須の下限制約などがあるのに無理に近似して入れると、元問題では使えない解を返すおそれがあります。

AHCなどへ外部ソルバーを導入できるかは、その開催環境とルールを確認してください。本ガイドは外部ソルバーの利用可否や、問題ごとの速度優位性を保証しません。

### 2.3 参考資料

以下は定義・機能を確認するための一次資料です。リンク先の実装に本文のコードが依存することはありません。

1. [Kevin Wayne, Dynamic Programming I — Weighted Interval Scheduling](https://www.cs.princeton.edu/~wayne/kleinberg-tardos/pdf/06DynamicProgrammingI.pdf)
2. [LEDA, Maximum Weighted Matching in General Graphs](https://leda.uni-trier.de/leda/guide/graph_algorithms/maximum_weighted_matching.html)
3. [OR-Tools, Assignment as a Minimum Cost Flow Problem](https://developers.google.com/optimization/flow/assignment_min_cost_flow)
4. [OR-Tools, Linear Sum Assignment Solver](https://developers.google.com/optimization/assignment/linear_assignment)
5. [OR-Tools, CP-SAT Solver](https://developers.google.com/optimization/cp/cp_solver)
6. [SCIP, Constraint Handlers — Set Packing/Partitioning/Covering Constraints](https://www.scipopt.org/doc/html/group__CONSHDLRS.php)

## 3. 実行の準備

GCCのC++20対応環境と、ヘッダファイル `set_packing_solver_v03.hpp` を用意します。ライブラリはGNU系の `bits/stdc++.h` を使うため、C++20対応というだけですべてのコンパイラにそのまま対応するわけではありません。

各例を `main.cpp` として保存し、ヘッダを同じディレクトリに置くと、次のコマンドで実行できます。標準入力や追加ライブラリのリンクは不要です。

```sh
g++ -std=c++20 -O2 -Wall -Wextra main.cpp -o example
./example
```

同梱の例ファイルを直接使う場合は、ZIPを展開したディレクトリで次のようにします。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -I. examples/01_minimal.cpp -o example
./example
```

このガイドでは、C++の変数、配列、`if`、`for` が読めることを前提にします。`std::vector` は長さを変えられる配列、`auto` は右辺から型を決める書き方です。初登場のライブラリAPIは、その都度説明します。

`assert(条件)` は、開発中に「この条件は成り立っているはず」と確認するC++の機能です。成り立たない場合は処理を止めます。このガイドの確認用コードでは `-DNDEBUG` を付けず、検査を有効にしてください。

例の出力は検証時の一例です。特に時間で止める実行では、同じコードでも結果や反復数が変わり得ます。選択IDの並び順にも意味はありません。出力の文字列は学習用であり、特定のコンテストの提出形式ではありません。

## 4. 最小コード：競合しない作業を選ぶ

第1節の3候補を、そのまま入力します。資源や必須条件はまだ使いません。

### コード：01_minimal.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    Solver::Problem p;
    p.weight = {8, 7, 6};
    p.conflicts = {{0, 1}, {0, 2}};

    Solver solver(p);
    auto result = solver.solve({});

    if (!result.feasible) {
        std::cout << "no feasible solution\n";
        return 0;
    }
    std::cout << "value=" << result.value << "\nselected:";
    for (int i : result.selected) std::cout << ' ' << i;
    std::cout << '\n';
}
```

### 入力から出力までを順に読む

1. `#include` でライブラリを読み込みます。ヘッダ側に実装が入っているため、別の実装ファイルは不要です。
2. `using Solver = SetPackingSolver<>;` は長い型名に `Solver` という別名を付けます。`<>` の既定値では、利益を `long long` 型の整数で扱います。
3. `Solver::Problem p;` は入力を入れる箱です。空の配列を持つ状態から作られます。`::` は、その型の中で定義された名前を参照する記号です。
4. `p.weight` の長さが候補数です。配列の0番目が候補0なので、`{8, 7, 6}` は順に候補0、1、2の利益になります。候補数を別に指定する引数はありません。
5. `p.conflicts` の各 `{i, j}` は、同時に選べない二つの候補IDです。`{0, 1}` と `{0, 2}` を登録しています。向きはなく、逆向きの組を追加する必要はありません。
6. `Solver solver(p);` で問題を受け取るソルバーを作ります。ソルバーは必要な入力を自分で保持します。元の `p` を後で書き換えても、ソルバーの入力は自動では変わりません。
7. `solver.solve({})` で解を求めます。`{}` は空の問題ではなく、**既定の探索設定**を表す `Options` です。ここでは約1.95秒の時間設定を使います。詳しい停止条件は次節で説明します。
8. `result.feasible` を確認してから、利益 `result.value` と候補ID列 `result.selected` を使います。`!` は真偽の反転で、`!result.feasible` は「実行可能解ではないなら」です。

まだ指定していない `resources`、`capacity`、`fixed` は空です。そのため資源制約はなく、全候補を自由に選べます。コンストラクタの乱数seedも省略しており、既定値1が使われます。

出力例は次のようになります。

```text
value=13
selected: 1 2
```

ID1と2を選び、利益は13です。この小さい問題では手で列挙して最適値13と確認できますが、`solve` が一般の入力でも最適性を証明するわけではありません。

## 5. 発展①：計算量を指定し、結果の情報を読む

最小例から変えるのは、乱数seedと `Options` の指定です。学習中は、時間によるばらつきを避けるために反復回数で止めます。

### コード：02_options.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    Solver::Problem p;
    p.weight = {8, 7, 6};
    p.conflicts = {{0, 1}, {0, 2}};
    Solver solver(p, 42);

    Solver::Options options;
    options.time_limit_us = -1;
    options.iteration_limit = 1'000;
    auto result = solver.solve(options);

    if (!result.feasible) return 1;
    std::cout << "value=" << result.value << '\n';
    std::cout << "iterations=" << result.iterations << '\n';
    std::cout << "elapsed_us=" << result.elapsed_us << '\n';
}
```

### 新しい引数とフィールド

| 指定 | 意味 |
|---|---|
| `42` | 乱数の初期seed。同じ問題・同じ実装環境・同じ操作列・同じ固定反復設定で再現させるための値。42に品質上の特別な意味はない |
| `options.time_limit_us = -1` | 相対時間の制限を無効にする。時間制限なし、という特別な値 |
| `options.iteration_limit = 1'000` | この呼び出しの探索反復を高々1000回にする。桁区切りの `'` は数字の値を変えない |
| `result.iterations` | 今回行った探索反復の数。累計ではない |
| `result.elapsed_us` | 今回の呼び出しにかかった時間。単位はマイクロ秒 |

反復1回は「候補を一つ抽出して変更を試す機会」です。すでに選んでいる候補が抽出され、何もしなかった回も数えます。初期化や最初の解の構築は含まれないため、1000反復が必ず一定時間という意味ではありません。探索するものがなくなれば、上限より早く終わります。

`Options` の全フィールドは次の三つです。

| フィールド | 既定値 | 指定できる内容 |
|---|---|---|
| `time_limit_us` | `1'950'000.0` | 呼び出し開始からの相対時間。`-1` で無効、`0` も指定可能 |
| `deadline` | なし | 絶対的な締切時刻。第16節で使用 |
| `iteration_limit` | `-1` | 今回の反復上限。`-1` で無効、`0` なら探索をしない |

有効な停止条件のどれかに到達すると探索を止めます。三つすべてを無効にしてはいけません。この例は「相対時間なし・締切なし・1000反復」なので、停止条件があります。

実時間で止めたいときは、同じコードの `time_limit_us` を例えば `10'000.0` に、`iteration_limit` を `-1` に変更します。10,000マイクロ秒は10ミリ秒、つまり0.01秒です。時間指定は厳密な打ち切り保証ではなく、詳細は第16節にあります。

以後、特に断らない例ではこの固定反復設定を使います。

## 6. 発展②：共有資源で競合を表す

候補0が機械AとBを使い、候補1はAだけ、候補2はBだけを使うとします。各機械は同時に一つの候補にしか使えません。

前節の `conflicts` を、`resources` と `capacity` に置き換えます。この例では選べる組み合わせは前節と同じです。

### コード：03_resources.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    Solver::Problem p;
    p.weight = {8, 7, 6};
    p.resources = {{0, 1}, {0}, {1}};
    p.capacity = {1, 1};
    Solver solver(p, 42);

    Solver::Options options;
    options.time_limit_us = -1;
    options.iteration_limit = 1'000;
    auto result = solver.solve(options);

    if (!result.feasible) return 1;
    std::cout << "value=" << result.value << "\nselected:";
    for (int i : result.selected) std::cout << ' ' << i;
    std::cout << '\n';
}
```

### 二種類のIDを区別する

候補IDと資源IDは別の番号です。この例では資源0が機械A、資源1が機械Bです。

| 入力箇所 | 中身 | 意味 |
|---|---|---|
| `resources[0]` | `{0, 1}` | 候補0は資源0と資源1を1単位ずつ使う |
| `resources[1]` | `{0}` | 候補1は資源0だけを1単位使う |
| `resources[2]` | `{1}` | 候補2は資源1だけを1単位使う |
| `capacity[0]` | `1` | 資源0を使う候補を高々一つ選ぶ |
| `capacity[1]` | `1` | 資源1を使う候補を高々一つ選ぶ |

`capacity.size()` が資源数です。`resources` を指定する場合、その外側の配列の長さは候補数と一致させます。資源を使わない候補には空の行 `{}` を割り当てます。全候補が資源を使わないなら、`resources` 全体を空のままにできます。

候補1と2は別の資源を使うので、一緒に選べて利益は13です。候補0と1は資源0を合計2単位使うため、容量1を超えます。

多数の候補が同じ資源を使う場合も、その資源を各候補に一回ずつ書くだけで済みます。全候補対を競合に展開する必要はありません。

## 7. 発展③：容量を2にし、個別の競合と組み合わせる

ここからは、同じ設備を使う候補が三つあり、同時に二つまで受け入れられる例に変えます。ただし候補0と1だけは別の事情で両立しません。資源を使わない候補3も一つ加えます。

### コード：04_capacity.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    Solver::Problem p;
    p.weight = {8, 7, 6, 5};
    p.conflicts = {{0, 1}};
    p.resources = {{0}, {0}, {0}, {}};
    p.capacity = {2};
    Solver solver(p, 42);

    Solver::Options options;
    options.time_limit_us = -1;
    options.iteration_limit = 1'000;
    auto result = solver.solve(options);

    if (!result.feasible) return 1;
    std::cout << "value=" << result.value << "\nselected:";
    for (int i : result.selected) std::cout << ' ' << i;
    std::cout << '\n';
}
```

新しい入力を順に確認します。

1. `weight` の末尾の `5` は、新しく加えた候補3の利益です。
2. `capacity = {2}` は、資源が一種類で、その容量が2という指定です。「資源IDが2」という意味ではありません。
3. `resources` の最初の三行は同じ `{0}` です。候補0、1、2のうち高々二つが資源0を使えます。
4. 最後の `{}` は候補3が資源を使わないことを表します。容量の数には入らず、競合もないので他の候補と組み合わせられます。
5. `conflicts = {{0, 1}}` は資源制約に**追加される条件**です。容量に余裕があっても候補0と1は一緒に選べません。

候補0、2、3を選ぶと利益は8＋6＋5＝19です。資源0の使用数は2です。容量2のグループを全ペア競合にしてしまうと「高々一つ」になり、別の問題を解いてしまいます。

容量0も有効です。その資源を使う自由候補は一つも選べません。

## 8. 発展④：必須・禁止を指定する

前節で、候補1は利用禁止、候補3は必ず行う作業になったとします。また候補3の利益を、作業コストを表す `-2` に変更します。

### コード：05_fixed.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    Solver::Problem p;
    p.weight = {8, 7, 6, -2};
    p.conflicts = {{0, 1}};
    p.resources = {{0}, {0}, {0}, {}};
    p.capacity = {2};
    p.fixed = {-1, 0, -1, 1};
    Solver solver(p, 42);

    Solver::Options options;
    options.time_limit_us = -1;
    options.iteration_limit = 1'000;
    auto result = solver.solve(options);

    if (!result.feasible) return 1;
    std::cout << "value=" << result.value << "\nselected:";
    for (int i : result.selected) std::cout << ' ' << i;
    std::cout << '\n';
}
```

`fixed` は候補と同じ長さの配列です。

| 候補 | `fixed` | 読み方 |
|---|---:|---|
| 0 | -1 | 自由。利益と他の条件に応じて選ぶ |
| 1 | 0 | 禁止。利益が7でも選べない |
| 2 | -1 | 自由 |
| 3 | 1 | 必須。利益が-2でも必ず選ぶ |

ここでは候補0、2、3で利益は8＋6−2＝12になります。必須の負の利益も `result.value` に含まれます。

`fixed` を省略して空にすると、全候補が `-1` と同じ扱いになります。逆に、`fixed` を0で埋めると全候補が禁止です。「値を指定していないつもりで0にする」と意味が変わるので注意してください。

必須指定は、結果の一部を固定したい場合にも使えます。ただし「今の解に含まれていた」という理由だけで必須にすると、その候補を外す改善はできなくなります。前の解を出発点にしたいだけなら、第12節の `improve` を使います。

## 9. 発展⑤：「解がない」と「空の解」を区別する

前節の候補0と1を両方必須にすると、競合と必須指定が矛盾します。このような入力は、ID間違いではなく、条件を満たす解が存在しない問題です。

### コード：06_infeasible.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    Solver::Problem p;
    p.weight = {8, 7, 6, -2};
    p.conflicts = {{0, 1}};
    p.resources = {{0}, {0}, {0}, {}};
    p.capacity = {2};
    p.fixed = {1, 1, -1, 1};
    Solver solver(p, 42);

    Solver::Options options;
    options.time_limit_us = -1;
    options.iteration_limit = 1'000;
    auto result = solver.solve(options);
    assert(!result.feasible);
    std::cout << "contradictory: feasible=" << result.feasible << '\n';

    Solver::Problem empty_problem;
    Solver empty_solver(empty_problem);
    auto empty = empty_solver.solve(options);
    assert(empty.feasible && empty.value == 0 && empty.selected.empty());
    std::cout << "empty: feasible=" << empty.feasible
              << " value=" << empty.value << '\n';
}
```

前半で変更した `fixed = {1, 1, -1, 1}` は、候補0、1、3を必須にします。しかし `conflicts` に `{0, 1}` があるため、答えは `feasible == false` です。

後半の `empty_problem` は候補すらない空の入力です。何も選ばない解が条件を満たすので、`feasible == true`、値0、空の `selected` が返ります。`selected.empty()` は配列が空かを調べるC++のメソッドです。

```text
contradictory: feasible=0
empty: feasible=1 value=0
```

`bool` を通常の設定で出力すると、偽は0、真は1になります。

`feasible == false` は、必須候補の集合が矛盾しているという意味です。**時間切れで探索が足りないことを表すものではありません。** このときも `value` は0、`selected` は空ですが、0点の実行可能解として比較してはいけません。

資源容量を必須候補だけで超える場合も同じです。反対に、必須候補だけで条件を満たせるなら、それだけを選ぶ解があるため、解の存在自体は確定します。

## 10. 発展⑥：小数の利益を使う

次は第5節の3候補に戻り、利益だけを小数に変えます。容量と選択数は、小数の利益を使う場合も整数のままです。

### コード：07_double.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<double>;
    SetPackingProblem<double> p;
    p.weight = {0.8, 0.7, 0.6};
    p.conflicts = {{0, 1}, {0, 2}};
    Solver solver(p, 42);

    Solver::Options options;
    options.time_limit_us = -1;
    options.iteration_limit = 1'000;
    SetPackingResult<double> result = solver.solve(options);

    if (!result.feasible) return 1;
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "value=" << result.value << "\nselected:";
    for (int i : result.selected) std::cout << ' ' << i;
    std::cout << '\n';
}
```

変更点は三つです。

1. `SetPackingSolver<double>` により、利益と解の値に `double` を使います。対応する型は `long long` と `double` の二種類です。`int` や `long double` を指定するAPIではありません。
2. `SetPackingProblem<double>` は、この `Solver::Problem` と同じ型です。`SetPackingResult<double>` も `Solver::Result` と同じです。入力・ソルバー・結果の利益型を揃えます。普段は前の例の別名や `auto` を使えば十分です。
3. `0.8`、`0.7`、`0.6` が新しい利益です。`std::fixed` と `std::setprecision(6)` は、小数点以下を6桁で出力するC++の設定で、計算精度を変えるものではありません。

この例の最適な利益は0.7＋0.6＝1.3で、出力例は `value=1.300000` です。

`double` は実数を有限の桁数で近似します。例えば計算結果と `1.3` の完全一致を `==` で確認するよりも、差の絶対値が用途に合った小さな許容誤差以内かを確認します。利益が小さな整数で表せるなら、まず `long long` を選ぶと値の比較が分かりやすくなります。

## 11. 発展⑦：自分で作った候補集合を検査する

探索の前後で「この組み合わせは何点か」「条件を満たしているか」を確認できます。この二つは別の質問なので、APIも分かれています。

### コード：08_validate.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    Solver::Problem p;
    p.weight = {8, 7, 6};
    p.conflicts = {{0, 1}, {0, 2}};
    Solver solver(p);

    std::vector<int> valid = {1, 2};
    std::vector<int> invalid = {0, 1};
    std::cout << "valid: value=" << solver.evaluate(valid)
              << " feasible=" << solver.is_feasible(valid) << '\n';
    std::cout << "invalid: value=" << solver.evaluate(invalid)
              << " feasible=" << solver.is_feasible(invalid) << '\n';

    assert(solver.evaluate(valid) == 13);
    assert(solver.is_feasible(valid));
    assert(solver.evaluate(invalid) == 15);
    assert(!solver.is_feasible(invalid));
}
```

`valid` と `invalid` は候補IDの配列です。どちらもIDとしては正しく、重複もありません。ただし後者は候補0と1の競合に違反します。

| API | 引数 | 返すもの |
|---|---|---|
| `evaluate(selected)` | 候補ID列 | **現在の入力重み**で計算した合計。制約は確認しない |
| `is_feasible(selected)` | 候補ID列 | 必須・禁止・競合・容量をすべて満たすなら `true` |

この例では解を探す必要がないので、`solve` も `Options` も使っていません。入力をソルバーに渡した時点で、検査APIは使用できます。

```text
valid: value=13 feasible=1
invalid: value=15 feasible=0
```

`evaluate` は違反の罰点を付けません。15点という数字だけを見て、その集合を採用してはいけません。

両APIの引数の型は `std::span<const int>` です。これは、既存の連続したID列をコピーせずに参照する仕組みです。この例のように `std::vector<int>` をそのまま渡せます。呼び出し側が参照される配列を保持し、APIはその内容を書き換えません。

候補IDの範囲外や、同じIDの重複は入力契約違反です。「実行不可能な組み合わせ」とは別であり、`is_feasible` に何を渡しても安全に `false` が返るという契約ではありません。

## 12. 発展⑧：初期解から改善する・違反した初期案を修復する

自分で作った解や、別の探索で得た解を出発点にできます。これを「初期解」と呼びます。`solve` を `improve` に替え、候補ID列を第一引数に渡します。

### コード：09_improve.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    Solver::Problem p;
    p.weight = {8, 7, 6};
    p.conflicts = {{0, 1}, {0, 2}};
    Solver solver(p, 42);

    Solver::Options options;
    options.time_limit_us = -1;
    options.iteration_limit = 1'000;
    std::vector<int> initial = {0};
    assert(solver.is_feasible(initial));
    auto result = solver.improve(initial, options);
    assert(result.feasible && result.value >= solver.evaluate(initial));
    std::cout << "improved=" << result.value << '\n';

    Solver repair_solver(p, 42);
    Solver::Options repair_only = options;
    repair_only.iteration_limit = 0;
    std::vector<int> hint = {0, 1, 2};
    auto repaired = repair_solver.improve(hint, repair_only);
    assert(repaired.feasible && repair_solver.is_feasible(repaired.selected));
    std::cout << "repaired=" << repaired.value << "\nselected:";
    for (int i : repaired.selected) std::cout << ' ' << i;
    std::cout << '\n';
}
```

### 実行可能な初期解

前半の `initial = {0}` は候補0だけを選ぶ8点の解です。`improve(initial, options)` は、この解を使って改善を試みます。実行可能な初期解の値を下回らない結果を返します。以前に同じ問題で得た最良解があれば、それも比較対象に残ります。

これは「初期解の候補を全部固定する」という意味ではありません。候補0を外し、候補1と2に入れ替えてもかまいません。また、自由な負の利益の候補は除かれることがあるので、初期解のID列そのものを保持する保証でもありません。`double` には通常の丸め誤差が伴います。

### 実行不可能な初期案

後半は、候補0、1、2を全部選びたいという、違反を含む案です。

1. `repair_solver` を新しく作り、前半で得た最良解の影響をなくします。
2. `repair_only = options` は設定のコピーです。その反復上限だけを0にして、改善探索は行わないようにします。
3. `hint = {0, 1, 2}` のIDは正しく、重複もありません。組み合わせが競合している点だけが問題です。
4. `improve` は必須候補を先に採用し、初期案を入力順に見て、追加できる自由候補だけを残します。この例では先頭の候補0が入り、候補1と2は入りません。

したがって、修復だけの出力は `repaired=8`、選択は候補0です。実行不可能な初期案に対しては、その見かけの合計値以上になるという保証はありません。また、この修復が最も高得点な修復とは限りません。

集合としての意味に順序はありませんが、**違反を含む初期案を修復するときは入力順が影響します。** より残したい候補を先に置くのは、呼び出し側でできる簡単な工夫です。

反復0でも、必須条件の検査、初期案の修復、値の計算は行います。初回の `solve` を反復0で呼ぶと、通常は必須候補だけの解が返り、自由候補を埋める最初の構築も行いません。

## 13. 発展⑨：探索を継続し、結果のコピーを使い分ける

同じソルバーで `solve` をもう一度呼ぶと、前回の状態を再利用して探索を続けます。毎回新しいソルバーを作る必要はありません。

### コード：10_continue_view.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    Solver::Problem p;
    p.weight = {8, 7, 6};
    p.conflicts = {{0, 1}, {0, 2}};
    Solver solver(p, 42);

    Solver::Options options;
    options.time_limit_us = -1;
    options.iteration_limit = 1'000;
    auto first = solver.solve(options);
    auto second = solver.solve(options);
    assert(first.feasible && second.feasible && second.value >= first.value);

    Solver::Options no_search;
    no_search.time_limit_us = 0;
    no_search.iteration_limit = 0;
    const auto& view = solver.solve_view(no_search);
    Solver::Result saved = view;
    std::cout << "first=" << first.value << " second=" << second.value
              << " view=" << view.value << '\n';

    auto third = solver.improve(view.selected, options);
    // この先ではviewを使わず、独立したコピーsavedを使う。
    assert(third.feasible && third.value >= saved.value);
    std::cout << "saved=" << saved.value << " third=" << third.value << '\n';
}
```

最初の `solve` と二回目の `solve` は、それぞれ高々1000反復です。入力が変わらない限り、返す最良値を下げません。ただし、追加の時間で必ず値が上がるわけではありません。

新しい `solve_view` は、同じ探索処理を行い、内部に保存した結果を**参照**として返します。

| 書き方 | 結果を所有するのは誰か | 後で保存して使えるか |
|---|---|---|
| `auto result = solver.solve(options);` | 呼び出し側 | ソルバーを変更・破棄しても使える |
| `const auto& view = solver.solve_view(options);` | ソルバー | 次の非const操作までに読む |
| `Solver::Result saved = view;` | 呼び出し側へコピー | コピー後は独立して使える |

`const` は参照先を書き換えない指定、`&` はコピーではなく参照を受け取る指定です。`auto view = solver.solve_view(options);` のように `&` を落とすとコピーになります。

参照の利用可能期間は、次の非constメソッドの呼び出し、ソルバーの移動、破棄までです。`solve`、`improve`、`update`、`rebuild`、`seed` などを呼んだ後で古い参照を使わないでください。`evaluate` と `is_feasible` は読み取り専用なので、その限りではありません。

この例の `improve(view.selected, options)` は対応している使い方です。`improve` は、参照先を更新する前に渡された初期案を自分で保存します。ただし呼び出し後は、古い `view` ではなく `third` または `saved` を使います。

`no_search` は時間0・反復0です。変更のない、すでに解を持っている状態で結果を読み出す例ですが、初回や更新直後なら必要な初期化・修復・再評価が発生します。「時間0だから処理も必ずゼロ」ではありません。

`solve_view` で省けるのは結果のコピーです。探索や、最良解が更新された際の内部保存までなくなるわけではありません。結果を保存するなら、通常の `solve` を選ぶ方が扱いやすいでしょう。

## 14. 発展⑩：利益だけを更新して次のターンへ進む

ターンごとに作業の価値だけが変わる場合、候補や競合を作り直す必要はありません。`Update` で変更した利益だけを渡します。

### コード：11_update_weights.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    Solver::Problem p;
    p.weight = {8, 7, 6};
    p.conflicts = {{0, 1}, {0, 2}};
    Solver solver(p, 42);

    Solver::Options options;
    options.time_limit_us = -1;
    options.iteration_limit = 1'000;
    auto first = solver.solve(options);
    if (!first.feasible) return 1;
    std::cout << "initial=" << first.value << '\n';

    for (long long new_weight : {10LL, 20LL, 0LL}) {
        Solver::Update changes;
        changes.weights = {{0, new_weight}};
        solver.update(changes);
        const auto& result = solver.solve_view(options);
        if (!result.feasible) return 1;
        assert(solver.is_feasible(result.selected));
        assert(result.value == solver.evaluate(result.selected));
        std::cout << "weight[0]=" << new_weight
                  << " value=" << result.value << '\n';
    }
}
```

新しく出てきた指定を分解します。

| 指定 | 意味 |
|---|---|
| `Solver::Update changes` | 差分を入れる箱。指定しないフィールドは変更しない |
| `changes.weights` | 候補の利益を変更する配列 |
| `{0, new_weight}` | 候補ID0の利益を `new_weight` に**置き換える**。加算ではない |
| `10LL, 20LL, 0LL` | 次の各ターンの利益。`LL` は `long long` の整数リテラルを表す |
| `solver.update(changes)` | 差分を反映する。これだけでは改善探索をしない |

この例は実際の対話通信ではなく、三つのターンの入力を配列で模擬しています。実問題では、毎ターンの入力を読み、変更が必要な項目だけを `Update` に入れます。

利益が8から10、10から20に変わる段階では、自由候補の正・非正が変わりません。制約に関する選択状態を保ち、次回の探索で保持中の解の値を新しい利益で計算し直します。

20から0になると、候補0が通常の選択対象から外れます。この場合は次の探索で選択状態を修復します。ただし競合・所属の構造は使い回します。

更新前の `first` は独立した結果のコピーです。その `value` は更新前の利益の合計であり、自動では書き換わりません。新しい入力で以前の選択を評価したければ、`solver.evaluate(first.selected)` を使います。異なるターンの `value` を単純に比較して「改善した」と判断しないでください。

ループ中の参照 `result` は、そのターンで読み終えます。次の `update` 後に持ち越して使う必要があるなら、ID列か結果をコピーしてください。

## 15. 発展⑪：容量・必須条件も更新する

次は第7節の容量2の例に戻ります。設備の容量や、実行が確定した作業もターンごとに変えられます。

### コード：12_update_constraints.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    Solver::Problem p;
    p.weight = {8, 7, 6, 5};
    p.conflicts = {{0, 1}};
    p.resources = {{0}, {0}, {0}, {}};
    p.capacity = {2};
    Solver solver(p, 42);

    Solver::Options options;
    options.time_limit_us = -1;
    options.iteration_limit = 1'000;
    auto first = solver.solve(options);
    if (!first.feasible) return 1;
    std::cout << "initial=" << first.value << '\n';

    Solver::Update turn;
    turn.capacities = {{0, 1}};
    turn.fixed = {{0, 0}, {1, 1}};
    solver.update(turn);
    auto second = solver.solve(options);
    if (!second.feasible) return 1;
    assert(solver.is_feasible(second.selected));
    std::cout << "restricted=" << second.value << '\n';

    Solver::Update close;
    close.capacities = {{0, 0}};
    solver.update(close);
    auto impossible = solver.solve(options);
    assert(!impossible.feasible);
    std::cout << "closed: feasible=" << impossible.feasible << '\n';

    Solver::Update release;
    release.fixed = {{1, -1}};
    solver.update(release);
    auto recovered = solver.solve(options);
    assert(recovered.feasible && solver.is_feasible(recovered.selected));
    std::cout << "recovered=" << recovered.value << '\n';
}
```

最初の差分 `turn` は、三つの変更をまとめています。

| 差分 | 読み方 |
|---|---|
| `capacities = {{0, 1}}` | 資源0の容量を1にする |
| `fixed` の `{0, 0}` | 候補0を禁止する |
| `fixed` の `{1, 1}` | 候補1を必須にする |

ここで `capacities` の先頭の0は資源ID、`fixed` の先頭の0や1は候補IDです。更新の配列名は入力 `Problem` の `capacity`、`weight` と異なり、`capacities`、`weights` である点にも注意してください。

前の解が制約違反になる可能性があるため、次の `solve` は必須を優先して前の選択を修復します。このターンでは候補1が資源0を使い、資源を使わない候補3と合わせて12点になります。

その次の `close.capacities = {{0, 0}}` は資源0を使用不可にします。必須候補1がその資源を使うので解がありません。さらに `release.fixed = {{1, -1}}` で候補1の必須を解除すると、解が存在する状態に戻ります。容量0と候補0の禁止は解除していないので、そのまま残ります。この例では候補3だけの5点が得られます。

同じ `Update` の同じフィールド内に、同じIDを二回書かないでください。例えば一つの `weights` 内で候補0を二回指定するのは契約違反です。異なるフィールドで同じ候補IDを扱うことや、別々の `update` 呼び出しで同じIDを変更することは可能です。

探索を挟まずに `update` を何回か呼び、最後に一回解き直す使い方もできます。同じ値への更新は不要な再初期化を起こしません。大量の辺を変更するためのAPIではなく、数値と固定指定を変えるためのAPIです。

## 16. 発展⑫：マイクロ秒の予算と、共通の締切を使う

相対時間だけでは、ソルバーを作る前の入力変換時間は含まれません。ターン全体の予算を意識する場合は、先に締切時刻を作り、その時刻を共有できます。

### コード：13_deadline.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    auto deadline = Solver::Clock::now() + std::chrono::microseconds(10'000);

    Solver::Problem p;
    p.weight = {8, 7, 6};
    p.conflicts = {{0, 1}, {0, 2}};
    Solver solver(p, 42);

    Solver::Options options;
    options.time_limit_us = 2'000.0;
    options.deadline = deadline;
    auto first = solver.solve(options);

    options.time_limit_us = -1;
    auto second = solver.solve(options);
    if (!first.feasible || !second.feasible) return 1;
    assert(second.value >= first.value);
    assert(solver.is_feasible(second.selected));
    std::cout << "first=" << first.value << " second=" << second.value << '\n';
    std::cout << "elapsed_us=" << second.elapsed_us << '\n';
}
```

各パラメータを順に説明します。

1. `Solver::Clock` は `std::chrono::steady_clock` の別名です。経過時間の計測に使う時計で、締切もこの時計の時刻を使います。
2. `Clock::now()` は現在の時刻です。`std::chrono::microseconds(10'000)` は10,000マイクロ秒の長さなので、足すと「ここから10ミリ秒後」の時刻になります。
3. この時刻は `Problem` の準備とソルバー構築より前に作っています。したがって、それらに時間を使った分だけ、探索に残される時間が減ります。
4. 最初の `time_limit_us = 2'000.0` は、一回目の探索について2ミリ秒の相対予算を与えます。`deadline` と両方を設定した場合は、早い方を終了の目安にします。
5. 二回目は `time_limit_us = -1` で相対予算を無効にします。`deadline` は同じままで、共通の締切まで継続探索します。`iteration_limit` は既定の `-1` なので、反復数の上限は設けていません。

`deadline` の型は `std::optional<Clock::time_point>` です。時刻を設定しなければ「締切なし」です。一度設定したものを外すには `options.deadline.reset()` を使います。これはC++の `optional` の操作であり、他の停止条件は別に残しておく必要があります。

### 時間指定は「厳密な停止命令」ではない

ライブラリは一定の間隔で時計を確認します。初期化、ソート、一つの操作、変更の復元、結果のコピーが終わるまで、設定した予算を超えることがあります。締切が過ぎていても、解を返すために必要な検査・修復は行います。

したがって、マイクロ秒単位で設定できることと、誤差数マイクロ秒で必ず停止することは別です。外側の処理、入出力、変換、時間超過の余裕を残して予算を決めてください。

`result.elapsed_us` は、その `solve` 系呼び出しの開始からの時間です。ソルバーのコンストラクタや、それより前の入力変換は含みません。`solve` と `improve` は結果のコピーまで、`solve_view` は内部結果を返す準備までを含みます。

時刻で止める実行では、同じseedでも実行できる反復数が変わります。デバッグで同じ探索を再現したい場合は、第5節の固定反復設定へ戻してください。

## 17. 発展⑬：候補や競合が変わったら再構築する

利益や容量の変更と違い、候補の追加・削除、競合ペアや資源所属の変更には `rebuild` を使います。前の解は消えるため、使いたい選択は先に保存し、新しいIDへ変換します。

### コード：14_rebuild.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    Solver::Problem p;
    p.weight = {8, 7, 6};
    p.conflicts = {{0, 1}, {0, 2}};
    Solver solver(p, 42);

    Solver::Options options;
    options.time_limit_us = -1;
    options.iteration_limit = 1'000;
    auto old_result = solver.solve(options);
    if (!old_result.feasible) return 1;

    Solver::Problem changed;
    changed.weight = {6, 7, 10};
    changed.conflicts = {{0, 2}};
    std::vector<int> old_to_new = {-1, 1, 0};
    std::vector<int> hint;
    for (int old_id : old_result.selected) {
        int new_id = old_to_new[old_id];
        if (new_id >= 0) hint.push_back(new_id);
    }

    solver.rebuild(changed);
    auto result = solver.improve(hint, options);
    if (!result.feasible) return 1;
    assert(solver.is_feasible(result.selected));
    std::cout << "before=" << old_result.value << '\n';
    std::cout << "after=" << result.value << '\n';
}
```

### IDの変換を省略しない

この例では、古い候補0を削除し、古い候補2、1をこの順に並べ、新しい候補を末尾に加えています。

| 古い候補ID | 新しい候補ID | 新しい利益 | `old_to_new` の値 |
|---|---|---:|---:|
| 0 | 削除 | — | -1 |
| 1 | 1 | 7 | 1 |
| 2 | 0 | 6 | 0 |
| 新規 | 2 | 10 | 古いIDがないため配列には書かない |

`old_to_new` の長さは古い候補数です。`-1` はこの例の呼び出し側が決めた「削除された」という目印で、ライブラリへ渡す候補IDではありません。

`hint` には、前の選択のうち残った候補を、新IDで入れます。`rebuild(changed)` は新しい問題をすべて受け取り、旧問題の選択状態と最良解を破棄します。その後 `improve(hint, options)` を呼ぶことで、前の解に含まれていた候補を出発点にできます。新しい制約に違反する場合も、初期案として修復されます。

`changed.conflicts = {{0, 2}}` は**新ID**の競合です。新候補2と、新ID0になった古い候補2が両立しない、という指定です。

`rebuild` は乱数の状態を維持します。ソルバーを初めて作ったときと同じ状態へ完全に戻す操作ではありません。次節の `seed` と組み合わせるか、新しいソルバーを作ると、独立した開始状態にできます。

なお、候補生成をやり直すたびに前の選択候補を消してしまうと、その前の解を下限として使えなくなります。残せるなら候補プールに残す、変更後の利益と制約で再評価する、といった管理は呼び出し側の役割です。

## 18. 発展⑭：複数のseedで最初から試す

同じ開始状態で長く探索する方法のほかに、いくつかのseedで独立に試し、最良の結果を選ぶ使い方もできます。どちらがよいかは入力と計算予算によるため、実測で比較します。

### コード：15_multistart.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    Solver::Problem p;
    p.weight = {8, 7, 6};
    p.conflicts = {{0, 1}, {0, 2}};

    Solver::Options options;
    options.time_limit_us = -1;
    options.iteration_limit = 600;
    Solver::Result best;

    for (std::uint64_t seed_value : {1ULL, 2ULL, 3ULL}) {
        Solver trial(p);
        trial.seed(seed_value);
        auto result = trial.solve(options);
        if (result.feasible && (!best.feasible || result.value > best.value)) {
            best = result;
        }
        std::cout << "seed=" << seed_value << " feasible=" << result.feasible
                  << " value=" << result.value << '\n';
    }

    if (!best.feasible) return 1;
    std::cout << "best=" << best.value << '\n';
}
```

`600` は一回の試行の反復上限です。三つのseedを試すので、合計の上限は1800反復です。ただし初期構築なども三回発生します。同じ合計反復数で一回探索した場合と、実時間まで同じになるとは限りません。

`std::uint64_t` はseedを保持する64ビット符号なし整数の型、数値の `ULL` は符号なし `long long` の指定です。ここでは1、2、3という三つの異なるseedを試しています。

`trial.seed(seed_value)` は乱数状態だけを設定します。この例では毎回新しいソルバーを作った直後なので、`Solver trial(p, seed_value);` と書くのと同じ初期seedになります。

探索済みのソルバーへ `seed` を呼んでも、保存解や探索の途中状態は消えません。「seedを替えて継続」と「最初からやり直す」は別です。完全に始め直すなら、この例のように新しく作るか、`rebuild(p)` と `seed(seed_value)` の両方を行います。

`Solver::Result best;` は最初は `feasible == false` です。最初の実行可能解を保存し、以後は値が厳密に大きい場合に更新します。`best = result` は結果のコピーなので、`trial` がループの終わりで破棄されても、最良解を保持できます。

実時間で複数試行を行う場合は、試行数だけ既定の約1.95秒を使ってしまわないようにします。第16節の共通締切と、各試行の相対予算を組み合わせてください。新しいソルバーを作る費用も含めて比較することが大切です。

## 19. 発展⑮：元の問題の「作業案」を入力へ変換する

ここまでは数字の配列を直接書きました。実問題では、意味のある作業案を作り、その情報から配列を組み立てます。

例として、三つの仕事があり、仕事0には二つの実行案があるとします。各案は盤面のマスを使います。制約は「同じ仕事の案は高々一つ」「各マスは高々一回」「採用する案は全体で高々二つ」です。

### コード：16_modeling.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    struct Plan {
        int job;
        std::vector<int> cells;
        long long gain;
    };
    std::vector<Plan> plans = {
        {0, {0, 1}, 9},
        {0, {2}, 6},
        {1, {1}, 8},
        {2, {3}, 5}
    };
    const int job_count = 3;
    const int cell_count = 4;
    const int total_resource = job_count + cell_count;

    Solver::Problem p;
    p.capacity.assign(total_resource + 1, 1);
    p.capacity[total_resource] = 2;
    for (const Plan& plan : plans) {
        p.weight.push_back(plan.gain);
        p.resources.push_back({plan.job, total_resource});
        for (int cell : plan.cells) {
            p.resources.back().push_back(job_count + cell);
        }
    }

    Solver solver(p, 42);
    Solver::Options options;
    options.time_limit_us = -1;
    options.iteration_limit = 1'000;
    auto result = solver.solve(options);
    if (!result.feasible) return 1;
    assert(solver.is_feasible(result.selected));
    std::cout << "value=" << result.value << '\n';
    for (int i : result.selected) {
        std::cout << "plan=" << i << " job=" << plans[i].job
                  << " gain=" << plans[i].gain << '\n';
    }
}
```

### まず候補の意味を定義する

`Plan` はこの例だけで使う作業案の型です。ライブラリのAPIではありません。

| フィールド | 意味 |
|---|---|
| `job` | 何番の仕事を実行する案か |
| `cells` | その案が使うマスIDの配列 |
| `gain` | 案を採用すると得られる利益 |

`plans` の配列の添字が候補IDになります。例えば先頭の `{0, {0, 1}, 9}` は「仕事0について、マス0と1を使い、9点を得る候補0」です。二行目は同じ仕事0の別案です。

### 異なる意味の資源に別々のIDを割り当てる

| 資源ID | 意味 | 容量 |
|---|---|---:|
| 0〜2 | 仕事0〜2について、同じ仕事の二案を採用しないための資源 | 1 |
| 3〜6 | マス0〜3の使用枠 | 1 |
| 7 | 全体の採用数を数える共通枠 | 2 |

`job_count = 3` と `cell_count = 4` は、仕事数とマス数です。`total_resource = 7` は**全体枠の資源ID**であり、資源の総数ではありません。総数はそれに1を足した8です。

`capacity.assign(total_resource + 1, 1)` は、8個の容量をすべて1で初期化します。その後、全体枠の容量だけ2に変更します。

各候補は、自分の仕事に対応する資源 `plan.job` と、全体枠 `total_resource` を使います。さらに使う各マスを `job_count + cell` で資源IDへ変換して追加します。これにより、仕事IDとマスIDが同じ0であっても、同じ資源と誤解されません。

`push_back` は末尾への追加、`back()` は最後の要素の参照です。このループでは、今追加した候補の資源行へ、その候補が使うマスを追加しています。

結果のIDから `plans[i]` を参照すれば、元の作業案に戻せます。この例は候補0と3、または候補1と2で14点です。同じ得点の解が複数ある場合、どちらを返すかを指定するAPIではありません。

### 元問題と目的関数の対応を確かめる

ライブラリが最大にするのは、あくまで `gain` の合計です。案同士を組み合わせたときに利益が変わる問題なら、そのまま加算利益を入れるだけでは元のスコアと一致しません。元問題のスコアで別途評価するか、問題を適切に切り出す必要があります。

全候補の利益に同じ定数を足すと、たくさん選ぶ解が余分に有利になり、一般には元と違う目的になります。「負の利益をなくすため一律に足す」といった変換は、選択数が固定されているなどの理由がなければ行いません。

また、このモデルは各仕事を**高々一つ**実行するもので、すべての仕事を必ず処理するモデルではありません。候補が列挙されていない案も選べません。候補生成の質と、制約・利益の写し方は、探索時間と同じくらい重要です。

## 20. 発展⑯：外側の解を保ち、一部だけ解き直す

大きな問題の一部分だけを選び直したいことがあります。ここでは、新しいAPIを増やさず、`improve` に渡す小さい問題の作り方を学びます。

### 何を固定し、何を解き直すか

全体の実行可能な現在解を $S$、選び直してよい候補の集合を $U$ とします。$U$ には未選択の候補を含めてかまいません。外側で選択を変えない候補集合を $T$ と呼びます。

$$T=S\setminus U$$

$\setminus$ は「左の集合から右に含まれるものを取り除く」という記号です。したがって $T$ は、現在選択している候補のうち、今回の変更範囲の外にあるものです。$U$ の外にある未選択候補も、未選択のままにします。

局所問題では次の三点が必要です。

1. 資源容量から、外側で選択している候補 $T$ の使用量を引く。
2. $T$ のどれかと競合する局所候補を禁止する。
3. 局所候補自身の必須・禁止指定と、局所候補同士の競合を残す。

局所問題の資源容量を $c'_ {r}$ とすると、計算は次の形です。$c_ {r}$、$a_ {ri}$、$r$、$i$ は第1節と同じ意味です。

$$c'_ {r}=c_ {r}-\sum_{i\in T} a_ {ri}$$

$c'_ {r}$ の右上の `'` は「局所問題用に変えた値」という目印です。微分ではありません。右辺は元の容量から、固定する外側の使用数を引いています。

### コード：17_local.cpp

```cpp
#include "set_packing_solver_v03.hpp"

int main() {
    using Solver = SetPackingSolver<>;
    Solver::Problem global;
    global.weight = {8, 7, 6, 5, 9, 4};
    global.conflicts = {{0, 1}, {2, 4}, {1, 5}};
    global.resources = {{0}, {0}, {1}, {1, 1}, {0, 1}, {}};
    global.capacity = {2, 2};
    global.fixed = {-1, -1, -1, 1, -1, -1};

    // 呼び出し側でも資源使用数を数えるため、各行の重複を先に除く。
    for (auto& row : global.resources) {
        std::sort(row.begin(), row.end());
        row.erase(std::unique(row.begin(), row.end()), row.end());
    }
    Solver full(global);
    std::vector<int> current = {0, 2, 3, 5};
    assert(full.is_feasible(current));

    std::vector<int> local_to_global = {0, 1, 2, 4};
    const int n = static_cast<int>(global.weight.size());
    std::vector<int> global_to_local(n, -1);
    Solver::Problem local;
    local.capacity = global.capacity;
    for (int k = 0; k < static_cast<int>(local_to_global.size()); ++k) {
        int i = local_to_global[k];
        global_to_local[i] = k;
        local.weight.push_back(global.weight[i]);
        local.resources.push_back(global.resources.empty()
                                  ? std::vector<int>{} : global.resources[i]);
        local.fixed.push_back(global.fixed.empty() ? -1 : global.fixed[i]);
    }

    std::vector<int> initial, outside;
    std::vector<unsigned char> selected_outside(n, 0);
    for (int i : current) {
        if (global_to_local[i] >= 0) {
            initial.push_back(global_to_local[i]);
        } else {
            outside.push_back(i);
            selected_outside[i] = 1;
            if (!global.resources.empty()) {
                for (int r : global.resources[i]) --local.capacity[r];
            }
        }
    }
    for (auto [i, j] : global.conflicts) {
        int a = global_to_local[i], b = global_to_local[j];
        if (a >= 0 && b >= 0) local.conflicts.emplace_back(a, b);
        if (a >= 0 && selected_outside[j]) local.fixed[a] = 0;
        if (b >= 0 && selected_outside[i]) local.fixed[b] = 0;
    }

    Solver sub(local, 42);
    Solver::Options options;
    options.time_limit_us = -1;
    options.iteration_limit = 1'000;
    auto result = sub.improve(initial, options);
    if (!result.feasible) return 1;
    std::vector<int> combined = outside;
    for (int k : result.selected) combined.push_back(local_to_global[k]);
    assert(full.is_feasible(combined));
    assert(full.evaluate(combined) >= full.evaluate(current));
    std::cout << "before=" << full.evaluate(current) << '\n';
    std::cout << "after=" << full.evaluate(combined) << '\n';
}
```

### 例の入力を一つずつ確認する

このコードは、ここまでの容量・固定・初期解・ID変換を一つにまとめています。

| 入力 | 意味 |
|---|---|
| `global.weight` | 全体の候補0〜5の利益。順に8、7、6、5、9、4 |
| `global.conflicts` | 候補0と1、2と4、1と5が競合 |
| `global.resources` | 候補0と1は資源0、候補2と3は資源1、候補4は両方、候補5は資源なし |
| `global.capacity = {2, 2}` | 各資源の容量は2 |
| `global.fixed` | 候補3だけが必須 |
| `current = {0, 2, 3, 5}` | 全体の現在解 $S$。利益は23 |
| `local_to_global = {0, 1, 2, 4}` | 選び直す候補集合 $U$ を並べたもの。局所ID0〜3から全体IDへの対応も兼ねる |

候補3の資源行は、あえて `{1, 1}` と重複させています。意味は資源1を1単位使うことです。ライブラリは内部で正規化しますが、元の `Problem` は書き換えません。今回は呼び出し側も使用量を引き算するため、最初に各行をソートし、重複を除いています。これをせず二回引くと、容量が誤って減ります。

### 局所問題の構築を追う

1. `global_to_local` を `-1` で初期化し、$U$ に含めた候補だけに局所IDを振ります。第17節のID変換と同じ考え方です。
2. 局所問題へ利益、資源の行、固定指定をコピーします。資源IDは全体のものをそのまま使うため、容量配列の長さも維持します。`条件 ? A : B` は条件が真ならA、偽ならBを選ぶC++の式です。資源・固定の配列が省略されている場合も扱っています。
3. `current` を局所の初期解 `initial` と、外側の選択 `outside` に分けます。この例の外側は候補3と5です。
4. 外側の候補3が資源1を一つ使っているので、局所容量は `{2, 1}` になります。候補5は資源を使いません。元の現在解が実行可能なので、引き算で負の容量にはなりません。
5. 局所候補同士の競合は、新しいIDで `local.conflicts` に入れます。`emplace_back(a, b)` は二つのIDの組を末尾へ追加する書き方です。
6. 全体候補1は、外側で選択している候補5と競合します。そのため対応する局所候補を禁止します。外側の選択と競合する必須候補が局所に存在しないことは、元の現在解が実行可能であるという前提から分かります。
7. `improve(initial, options)` で局所解を求め、局所IDを全体IDへ戻し、外側の選択と合わせます。

この例では候補0、4を局所で選べば、外側の3、5と合わせて26点になります。コードは出力後の全体制約と、現在解以上の利益を `assert` で検査しています。

### なぜ全体の利益を悪化させないのか

局所の初期解は、元の現在解のうち今回変更する部分です。境界の容量・競合を正しく反映しているので、この初期解は局所問題で実行可能です。`improve` はその利益を下回らない解を返します。

外側の利益は一定なので、局所部分の利益が下がらなければ全体も下がりません。外側の一定利益を、局所の各候補の利益に足す必要はありません。

これは「外側を固定した問題」での改善です。全体の最適解を得る保証ではありません。実際には、どの候補を変更範囲に含めるか、局所問題の構築費用が探索時間に見合うかも重要です。非常に小さい部分問題を大量に解く場合は、使う資源だけにIDを圧縮することや、外側の使用数を使い回すことを検討します。

この例は、`current` が実行可能であり、ID変換が重複のない正しい対応であることを前提にした利用パターンです。任意の壊れた入力を修復する、汎用の切り出しAPIではありません。

## 21. APIの索引と、実装前の確認事項

### 21.1 公開されている型とメソッド

以下は本文で使った名前を探すための索引です。`Solver` は選んだ利益型の `SetPackingSolver` の別名とします。

| API・型 | 用途 | 主な登場節 |
|---|---|---|
| `SetPackingProblem<Weight>` / `Solver::Problem` | 入力。`weight`、`conflicts`、`resources`、`capacity`、`fixed` | 4、6〜8、10 |
| `SetPackingResult<Weight>` / `Solver::Result` | 結果。`feasible`、`value`、`selected`、`elapsed_us`、`iterations` | 4〜5、10、13 |
| `SetPackingSolver<Weight>` | 入力と探索状態を保持するソルバー | 4、10 |
| `Solver(problem, initial_seed = 1)` | 問題を受け取り、構造を作る | 4〜5 |
| `Solver::Options` | 相対時間、締切、反復上限 | 5、16 |
| `Solver::Clock` | 締切を作る時計の型 | 16 |
| `solve(options)` | 探索を継続し、独立した結果を返す | 4、13 |
| `solve_view(options)` | 探索を継続し、結果の参照を返す | 13〜14 |
| `improve(initial_selected, options)` | 初期案を修復・改善し、独立した結果を返す | 12、17、20 |
| `evaluate(selected)` | 現在の重みで合計利益を計算する | 11 |
| `is_feasible(selected)` | 固定・競合・容量を確認する | 11 |
| `Solver::Update` | `weights`、`capacities`、`fixed` の差分 | 14〜15 |
| `update(changes)` | 数値と固定指定を変更する | 14〜15 |
| `rebuild(problem)` | 候補・競合・所属も含めて問題を置き換える | 17 |
| `seed(value)` | 乱数状態だけを設定する | 18 |

公開APIで、探索の温度や近傍の比率を指定する項目はありません。アルゴリズムの説明に出てくる内部定数を `Options` へ追加しても使えません。

### 21.2 何を再利用するか

| 操作 | 問題の構造 | 選択状態・過去の解 |
|---|---|---|
| 変更せず再度 `solve` / `solve_view` | 再利用 | 探索途中の状態、最良解、乱数状態、温度周期を再利用 |
| `improve` に初期案を指定 | 再利用 | 初期案から選択状態を組み立てる。変更のない問題の過去の最良解も比較対象に残る |
| 同じ値への `update` | 再利用 | 不要な再初期化・再評価を避ける |
| 利益だけを変更し、自由候補の正・非正は不変 | 再利用 | 制約に関する状態は保ち、次回に現在解・最良解の利益を再評価 |
| 容量・固定指定、自由候補の正・非正を変更 | 再利用 | 次回に旧解を修復し、選択状態を作り直す |
| `rebuild` | 作り直す。内部配列の確保済み容量は可能な範囲で利用 | 旧解を破棄。乱数状態は維持 |
| `seed` | 再利用 | 解を消さず、乱数状態だけ変更 |

更新後の最良解とは「新しい問題に対して保持・探索した範囲での最良解」です。過去に一度でも試したすべての選択を保存し、新しい利益で全件を評価し直すものではありません。

### 21.3 入力契約

| 項目 | 条件 |
|---|---|
| 利益型 | `long long` または `double`。既定は `long long` |
| 候補ID | 0から `weight.size() - 1`。解・初期案のIDは重複させない |
| 資源ID | 0から `capacity.size() - 1` |
| `resources` | 空、または候補数と同じ行数。各行の資源ID重複は内部で除く |
| `conflicts` | 異なる二候補の組。自己ループは禁止。重複や逆向きの重複は内部で除く |
| `capacity` | 0以上の整数 |
| `fixed` | 空、または候補数と同じ長さ。値は-1、0、1 |
| `Update` | 一つのフィールド内では、対象IDを重複させない |
| 停止条件 | 相対時間、絶対締切、反復上限のうち、少なくとも一つは有効 |

ライブラリは最小限の検査を `assert` で行い、自身の入力検査から例外を `throw` しません。標準ライブラリのメモリ確保なども絶対に例外を出さない、という意味ではありません。すべての契約違反が常時検出される設計でもありません。

入力規模については、次の範囲を守って使います。巨大入力に対応するための特別な拡張処理はありません。

- 候補数は $10^7$ 以下。
- 資源数と、重複除去前の内部配列要素数が `int` の範囲内。競合辺は無向なので、入力の辺数の2倍を数える。候補・資源の所属数も数える。
- 整数利益の絶対値の合計は `LLONG_MAX / 4` 以下。`LLONG_MAX` は `long long` の最大値。
- `double` の利益は有限値とし、その絶対値の合計も有限値となる範囲。
- `time_limit_us` は `-1`、または0以上 $10^{12}$ 以下の有限値。
- 反復上限は `-1`、または0以上。通常のAHC規模の計算予算を想定する。

### 21.4 導入時の短いチェックリスト

- 元問題の利益は、選んだ候補の利益の足し算になっているか。
- 必要な制約が「競合・単位資源の上限・候補の固定」で表せているか。
- 「高々一つ」と「必ず一つ」を取り違えていないか。
- 厳密解が簡単に使える特殊な構造ではないか。
- `feasible` を確認してから結果を使っているか。
- ターンごとに変わる情報を、`update` または `rebuild` で反映しているか。
- 再構築前後で候補IDの意味を保つか、正しく変換しているか。
- コピーを省くための参照を、次の更新後まで持ち越していないか。
- 候補生成・構築・入出力も含めて時間に余裕を持たせているか。

ここまでで、現行の公開APIと主要な利用パターンを一通り扱いました。以下は、内部で何を行っているかを知りたい読者向けの付録です。

## 付録A. ライブラリ実装：全体の流れ

ここから初めて、本ライブラリ自身の探索アルゴリズムを説明します。対象は本ガイドに同梱した現行ヘッダであり、以前の版の手法選定履歴ではありません。

### A.1 二つの解を分けて持つ

内部では「今、変更を試している現在解」と、「これまでに保存した最良解」を別々に持ちます。

現在解は、一時的に利益の低いものへ移る場合があります。ある状態から、最終的にはよい組み合わせへ行くために、途中で不利な入れ替えが必要になることがあるからです。呼び出し側へ返すのは別に保存した最良解なので、同じ問題に対する継続探索の返却値は下がりません。

ただし、不利な変更を許すのは利益についてだけです。保存・返却する解では、必須・禁止・競合・容量を守ります。

### A.2 呼び出しの流れ

1. **構造の準備**：構築時や `rebuild` 時に、競合と資源の所属を整理し、探索用の配列を用意する。
2. **選択状態の準備**：初回や必要な更新後に、必須候補を入れて矛盾を調べ、使える自由候補を絞る。前の解や指定された初期案を、必要なら修復して入れる。
3. **最初の追加**：探索予算があれば、優先度の高い候補から、追加できるものを入れる。
4. **入れ替えの提案**：候補を一つ抽出し、それを入れるために邪魔になる自由候補を外す。
5. **空きの再利用**：外したことで追加可能になった周辺候補を探し、空いた部分を埋める。
6. **提案の採否**：利益が増えれば採用する。減る場合も、温度に応じた確率で採用する。採用しなければ元へ戻す。
7. **最良解の保存と反復**：採用した解が記録を更新すれば保存し、予算の範囲で4〜6を繰り返す。
8. **結果の返却**：保存した最良解と、今回の経過時間・反復数を返す。

入力が変わらない継続探索では、1〜3の多くをやり直しません。これが同じソルバーを持ち続ける利点です。

## 付録B. データを小さな配列へまとめる

### B.1 CSR：多数のリストを一列に詰める

内部には、次の三種類の参照関係があります。

| 内部名 | あるIDから取り出せるもの |
|---|---|
| `adjacent_` | 候補から、その候補と競合する候補一覧 |
| `resources_` | 候補から、その候補が使う資源一覧 |
| `members_` | 資源から、その資源を使う候補一覧 |

これらはCSRという配列表現で保持します。CSRは、多数の短い配列を一つの `data` 配列へ連結し、各行の開始位置を `offset` に保存する方法です。行ごとに別々のメモリを確保する必要がなくなります。

例えば、候補ごとの一覧が「候補0は1、2」「候補1は0」「候補2は0」なら、`data` は `[1, 2, 0, 0]`、`offset` は `[0, 2, 3, 4]` です。候補1の一覧は、開始位置2から、次の開始位置3の直前までなので `[0]` と読めます。

構築は次の手順です。

1. 各行に何要素入るかを数える。
2. 個数の累積和から各行の開始位置を決める。
3. 確保した配列の該当位置へ、入力を直接書き込む。
4. 必要な行をソートし、重複IDを除いて詰め直す。

競合は両方向へ入れます。候補内の資源行も重複を除きます。その正規化済みの所属から逆引き `members_` を作ります。入力全体を別の二重配列へコピーしてから変換する必要がありません。

内部配列の確保済み容量は毎回縮めません。大量の重複を含む入力では、正規化後の要素数より大きなメモリ容量が残る場合があります。割り当て回数と構築費用を抑えるための選択です。

### B.2 候補の追加・除去を局所的に更新する

候補を一つ変えるたびに、選択全体を最初から検査すると高くつきます。内部では、必要な情報を保持して差分だけ更新します。

| 内部名 | 保持するもの |
|---|---|
| `selected_` | 現在選択している候補IDの配列 |
| `position_` | 各候補が `selected_` のどこにいるか。未選択なら-1 |
| `block_` | その候補と競合する、選択済み候補の数 |
| `use_` | 各資源を現在何候補が使っているか |
| `occupants_` | 各資源を現在使っている所属の一覧 |
| `owner_` | 一つの所属がどの候補のものか |
| `location_` | その所属が現在の使用者一覧のどこにあるか |
| `value_` | 現在解の利益合計 |

候補を追加すると、その候補と競合する各候補の `block_` を1増やし、使う各資源の使用数を1増やします。除去ではその逆を行います。

配列の途中の候補を除くときは、末尾の要素をその場所へ移してから末尾を削ります。後続の全要素をずらす必要がありません。資源の使用者一覧も、逆引き位置を使って同じ考え方で更新します。このため、内部の選択順は変わることがあります。

候補を追加できるか調べる `can_add` は、「未選択」「競合する選択済み候補がない」「使う資源のすべてに空きがある」を確認します。必須・禁止や正の利益の確認は、後述する対象候補の絞り込みと組み合わせて守っています。

## 付録C. 初期化と、最初の解の構築

### C.1 必須候補を先に確定する

`prepare` は使用数・競合数・選択位置などを初期化し、必須候補を追加します。一つでも追加できなければ、必須同士の競合か容量超過があるため、問題に実行可能解がありません。

必須を入れた後、次の条件を満たす自由候補を `active_` に残します。

- 利益が正である。
- 必須候補と競合しない。
- 必須の資源使用を差し引いた後、その候補を単独では追加できる。

ここでは自由候補同士の競合によって候補を除きません。自由候補同士は、後から入れ替えれば使える可能性があるためです。

### C.2 圧迫度当たりの利益で並べる

最初の追加順を決めるため、各有効候補について次の優先度を計算します。

$$P_ {i}=\frac{w_ {i}}{1+d_ {i}+\sum_{r\in R_ {i}}\frac{m_ {r}-1}{c_ {r}-u_ {r}}}$$

| 記号 | 意味 |
|---|---|
| $P_ {i}$ | 候補 $i$ の初期構築用の優先度 |
| $w_ {i}$ | 候補 $i$ の利益 |
| $d_ {i}$ | 候補 $i$ と競合する候補の数。重複除去後の数 |
| $R_ {i}$ | 候補 $i$ が使う資源IDの集合 |
| $m_ {r}$ | 資源 $r$ を使う候補の総数。重複除去後の所属で数える |
| $c_ {r}$ | 資源 $r$ の容量 |
| $u_ {r}$ | この優先度を最初に計算する時点で、必須候補が資源 $r$ を使っている数 |

分母の `1` は、競合も資源もない場合に分母が0になるのを避け、基本の大きさを与えます。競合相手が多い候補や、多くの候補が取り合う資源を使う候補は、他の選択を圧迫しやすいため優先度を下げます。

資源の項は「自分以外の利用候補数」を「必須を除いた残り容量」で割っています。残り容量が少ない資源ほど圧迫度が大きくなります。対象候補は必須だけの状態へ追加できるものに絞ってあるため、この計算の残り容量は正です。

これは近似的な順序の目安です。競合相手や資源の利用候補には、禁止や別の理由で使えない候補も含まれるため、正確な機会損失ではありません。また、この値を最大化しているのではなく、最終的な評価は元の利益の合計で行います。

### C.3 初期案の修復と貪欲な追加

初期案があれば、必須候補の後に、初期案のID順で追加可能な正の自由候補を採用します。これが第12節の修復です。

その後、探索予算がある場合に `greedy` が有効候補を優先度の降順に並べ、追加できるものを順に入れます。同じ優先度なら候補IDの小さい方を先にします。このように、その時点での基準に沿って一つずつ選ぶ方法を「貪欲法」と呼びます。

初期化中は、追加する順に利益を加算しています。同じ順で直後に利益をもう一回集計する必要はありません。実数の最良解保存時など、別に再計算の意味がある箇所とは区別しています。

## 付録D. 入れ替えと再充填

### D.1 一つの候補を入れるための除去

`exchange` は、有効な候補の中から抽出した一候補を入れようとします。すでに選択中なら何もせず戻ります。この空振りも探索反復に数えます。

未選択の候補なら、次の順です。

1. 明示的に競合する選択済み候補をすべて外す。
2. その候補が使う各資源を調べ、容量が一杯なら使用中の自由候補を一つ外す。
3. 必要な空きを作れたら、目的の候補を追加する。

必須候補は外しません。対象候補は必須と両立するものに絞ってあるため、必須と競合する候補を無理に入れることもありません。

容量が一杯の資源では、使用者全員を毎回比較する代わりに、基本的には最大8件を調べます。使用者数が8を超える場合は開始位置を乱択し、そこから連続する8件を巡回して見ます。その中の自由候補で利益が最も小さいものを除去候補にします。「8件を独立に抽選する」方式ではありません。

サンプルが必須候補だけだった場合は、自由候補を見つけるための追加走査を行います。必須を外して解を壊すことはありません。必要な処理を完了できなければ、それまでに外した候補を戻します。

### D.2 空いた場所を埋め直す

目的の候補を入れただけで終わると、除去によって空いた容量や競合の隙間が余る場合があります。そこで `refill` が追加候補を集めます。

調べるのは、主に外した候補の周辺です。

- 外した候補自身。
- 外した候補と競合する候補の一部。
- 外した候補が使っていた、空きのある資源に所属する候補の一部。

長いリストは、ここでも基本的に開始位置から巡回して最大8件を見ます。同じ候補が何度も現れないよう印を付け、集める段階で追加できないものは除きます。

候補群が512件以上になった段階で、それ以上の蓄積を止めます。一まとまりを調べた後の判定なので、512が厳密な最大件数ではありません。大きな資源を持つ問題でも、一回の提案で候補全体を際限なく走査しないための目安です。

集めた候補は乱択で並べ替えてから順に追加します。収集時には追加できても、直前に別の候補を入れたため追加できなくなる場合があるので、追加の直前にも検査します。

逆に、収集時点で追加できなかった候補は、この再充填中には追加可能になりません。再充填は候補を追加するだけで、容量の空きや競合条件を緩める操作をしないからです。そのため、収集段階の除外によって、後で追加できる候補を見落とすことにはなりません。

### D.3 候補印を毎回消さない

候補群を重複なく集めるために、候補ごとの整数の印と、今回の処理番号を使います。印が今回の番号なら、すでに確認済みです。これにより毎回、全候補分の真偽配列を0に戻す必要がありません。

内部の処理番号が一周する場合は、印の配列をクリアして再開します。この補助処理は `stamp` にまとめられています。

## 付録E. 焼きなましによる採否と、最良解の保存

### E.1 一時的な悪化を許す

入れ替えと再充填をまとめた提案について、利益の変化を $\Delta$ と書きます。$x$ は変更前の選び方、$y$ は提案された選び方です。

$$\Delta=F(y)-F(x)$$

$\Delta>0$ なら利益が増えているので採用します。$\Delta\le0$ の場合も、温度 $T>0$ のときは次の確率で採用します。

$$p=\exp(\Delta/T)$$

$p$ は採用する確率です。$\exp(z)$ は指数関数で、$e^z$ と同じ意味です。$e$ は約2.718の定数、$z$ は指数関数への入力を表す仮の記号です。ここでは入力が0以下なので、確率は0より大きく1以下になります。

例えば、利益が2減る提案で温度が1なら、確率は約0.135です。同じ悪化でも温度を高くすると受け入れやすくなります。このように温度を使って一時的な悪化を許す探索を「焼きなまし法」と呼びます。よりよい組み合わせへ移るための経路を増やすのが狙いであり、有限の実行時間で最適解を保証するものではありません。

利益差0の場合、正の温度では確率1で採用します。温度が0なら、現在の実装は正の利益差だけを採用します。

### E.2 温度を下げ、周期的に上げ直す

有効な正の自由候補の平均利益を $\bar w$、有効候補数を $A$ とします。必須候補の利益は、この平均には入れません。温度の一周期に使う反復数を $C$ とします。

$$C=\max(256,8A)$$

ここで $\max(256,8A)$ は二つの値の大きい方です。探索を開始・再準備してから進んだ反復カウンタを $s$ とし、一周期のどの位置にいるかを割合 $q$ で表します。

$$q=\frac{s\bmod C}{C},\qquad T=0.3\bar w(0.01)^q$$

$s\bmod C$ は、$s$ を $C$ で割った余りです。したがって $q$ は0以上1未満で、一周期進むと0へ戻ります。温度は平均利益の0.3倍から0.003倍付近へ下がり、次の周期で再び上がります。

温度は毎回計算せず、原則32反復ごとの更新と、呼び出し開始時の計算を行います。変更のない継続探索では反復カウンタを持ち越します。初期案から選択状態を準備し直した場合などは、周期も最初へ戻ります。

この温度式や、8件の走査、512件の候補群は内部の設定です。特定の問題に最適という保証はなく、公開APIでは変更できません。

### E.3 不採用の提案は戻し、記録を別に持つ

提案中に外した候補を `removed_`、追加した候補を `added_` に記録します。不採用なら、追加したものを逆順に除き、外したものを逆順に入れて戻します。

利益合計も変更前の値へ戻します。`double` の足し算・引き算を何度も繰り返して、元へ戻したつもりでも微小な誤差が積もるのを抑えるためです。

採用した解が最良値を上回れば `save_best` が保存します。`double` では、最良解候補の利益を選択IDから再集計してから、もう一度比較します。整数では、入力範囲内の加算・減算と利益差を `long long` で扱います。

整数の利益差や、容量のために外す候補の利益比較を、最初から `double` へ変換することはしません。温度、初期優先度、採用確率などには `double` を使います。ホットパスに `long double` や `__int128` は使っていません。

## 付録F. 更新、終了判定、計算量

### F.1 更新を必要な段階まで遅らせる

`update` は差分を反映し、必要な再評価や選択状態の修復を次の探索呼び出しまで遅らせます。複数の更新を受けてから一回解く場合、更新ごとに同じ全体処理を行う必要がありません。

利益だけの変更で有効候補の正・非正が変わらなければ、優先度と平均利益を調整し、次回に現在解と最良解の利益を再計算します。現在解と最良解は異なる場合があるため、両方が必要です。

容量や固定指定、正の自由候補の集合が変わる場合は、旧最良解を初期案にして `prepare` から組み直します。この場合も、候補の競合や資源所属を表すCSRは作り直しません。

`rebuild` は入力の構造も作り直します。補助的な定義である `Csr`、`Rng` や各処理は `Solver` の内部にあり、利用者が直接呼ぶ公開APIではありません。

### F.2 終了判定と乱数

時計は、外側の探索で原則32反復ごと、内部で数えている作業の一部では64カウントごとに確認します。すべての基本演算を一つずつ数えているわけではなく、初期化やソートを途中中断する仕組みでもありません。

処理途中で締切を検知した場合も、実行可能な状態にして返します。まだ必要な除去を終えていない提案なら復元し、すでに候補を入れて実行可能になっているなら、その時点までの再充填で提案の採否を判定する場合があります。「時間を検知したら必ず提案を全部捨てる」とは限りません。

すべての有効自由候補と必須候補を同時に選べている場合は、それ以上選択を増やす必要がないため早期終了します。

内部乱数は64ビット状態のSplitMix64形式で、整数演算から次の値を作ります。候補の抽出や巡回開始位置、候補群の並べ替え、採用確率の判定に使います。候補番号の抽出には剰余を使うため、候補数によってごく小さい分布の偏りはあります。暗号用の乱数ではありません。

### F.3 計算量の読み方

`O(...)` は、入力が増えたときに処理量がどう増えるかを示す大まかな記法です。例えば `O(K)` は、対象が2倍なら処理量も概ね2倍になるような増え方を表します。実測時間そのものや厳密な上限マイクロ秒を表すものではありません。

ここでは、候補数を `N`、資源数を `M`、入力の競合ペア数を `H`、入力の資源所属数の合計を `L`、選択数を `K` とします。`H` と `L` は入力の重複も数えます。

| 処理 | おおよその処理量 |
|---|---|
| 構築・`rebuild` | `O(N + M + H + L + 各行のソート量)` |
| 通常の `update` 自体 | `O(変更件数)`。処理番号の一周時などの例外的な配列クリアは除く |
| `evaluate` | `O(K)`。assert有効時は重複検査のため `O(N + K)` |
| `is_feasible` | `O(N + M + 選択候補の競合次数合計 + 選択候補の資源所属数合計)` |
| 一候補の内部追加・除去 | その候補の競合数と資源所属数に比例 |
| 初期貪欲法の並べ替え | 有効候補数を `A` として `O(A log A)`。追加判定は別途必要 |
| `solve` / `improve` の結果コピー | `O(K)`。初期化・探索は別途必要 |
| `solve_view` の参照返却 | `O(1)`。初期化・探索や内部での最良解保存は別途必要 |

制約変更後の修復には全体の走査があり、さらに探索を行うなら初期貪欲法の並べ替えも生じ得ます。`update` が差分件数に比例するからといって、次の `solve` まで差分件数だけで済むわけではありません。

一回の探索反復の費用も入力によって違います。競合相手が多い候補、多数の資源を使う候補、必須候補の多い資源では処理量が増えます。反復数だけで異なるデータセットの速度を比較せず、構築時間・更新時間・解品質・実時間を合わせて測定します。

## 付録G. 同梱物と検証方法

このガイドと一緒に配布するサンプルZIPには、次のものを含めます。

- `set_packing_solver_step_by_step_v01.md`：このガイド。
- `set_packing_solver_v03.hpp`：対応するライブラリ本体。ガイド作成のために本体を変更していません。
- `examples/01_minimal.cpp` から `examples/17_local.cpp`：本文からそのまま抜き出した、独立した17例。
- `verify_examples.py`：例の抽出、コンパイル、実行、出力・制約の確認を行うスクリプト。
- `verification.json`：検証時の環境、各例の出力、結果。
- `MANIFEST.json`：配布ファイルの対応を確認するためのハッシュ一覧。

ZIPを展開したディレクトリで、GCCとPython 3があれば、C++20の全例を次のように再検証できます。

```sh
python3 verify_examples.py --modes c++20
```

C++23と、AddressSanitizer・UndefinedBehaviorSanitizerを使う検査も行う場合は、対応するGCC環境で次のようにします。

```sh
python3 verify_examples.py --modes c++20,c++23,sanitize
```

これらは本文のC++例が動くことと、その例で扱う契約を確認するものです。一般の入力での最適性や、コンテスト実機での速度を保証するベンチマークではありません。

ライブラリ本体にも、ヘッダを単体コンパイルした場合だけ有効になるテストがあります。例から `#include` して利用するとき、そのテスト用 `main` は除外されます。本体のテストを実行するには次を使います。

```sh
g++ -std=c++20 -O2 -x c++ set_packing_solver_v03.hpp -o test_solver
./test_solver
```

本体を単体コンパイルした場合の `#pragma once in main file` は、GCCによる警告です。通常のinclude利用では発生しません。本体のテストも `-DNDEBUG` を付けずに実行してください。

本ガイドの検証環境はGCC 13.3.0です。17例をC++20、C++23、ASan＋UBSanでコンパイル・実行して確認しています。リーク検出は実行環境との互換性のため無効にしています。他のコンパイラ版やAtCoder実機で実行したことを意味するものではありません。

数式は `$...$` / `$$...$$` を解釈するMarkdown表示環境を想定します。通常のMarkdownだけを扱う表示環境では、数式がLaTeXの文字列として表示される場合があります。
