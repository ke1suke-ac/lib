# ConnectedPartitionSolver ステップバイステップガイド

対象実装：`connected_partition_solver_v08.hpp`。C++20。確認日：2026-10-07。

このガイドでは、「つながった区画を作り、その分け方を良くする」という問題を、小さな実行例で学びます。必要な数学は、和・不等式・二乗・平均です。C++ の変数、配列、ループ、関数は知っているものとして、ライブラリ固有の概念と C++ の `optional`・`span` はその都度説明します。

コードは全30例です。各例は独立した `main` を含み、前の例のコードを手で継ぎ足す必要はありません。新しい点を見比べやすくするため、共通部分も省略せず掲載します。設定値は学習用であり、未知の問題に対する最良の調整値ではありません。

| 学ぶ順序 | 内容 |
|---|---|
| 第1〜3節 | 対象問題、目的関数、類似ソルバー、実行準備 |
| ステップ01〜09 | 最小例から、個数・費用・負荷・固定・許可集合へ |
| ステップ10〜15 | 盤面、辺の役割、接触、任意領域、背景の表現 |
| ステップ16〜21 | 構築、修復、焼きなまし、時間、部分改善、整数費用 |
| ステップ22〜28 | 独自評価、追加制約、割当先依存の負荷、モデル変更 |
| ステップ29〜30 | ターン更新と複数回の探索 |
| 第9節、付録A〜H | API索引、実装の概要と詳細、参照資料と検証 |

## 1. 何を解くライブラリか

### 1.1 点に番号を付けて、領域に分ける

たとえば、6個の区画が道でつながっているとします。区画を**頂点**、道を**辺**と呼びます。この点と線の組を**グラフ**と呼びます。盤面のマス、作業、施設なども頂点として表せます。

各頂点に `0` や `1` といった**領域番号（ラベル）**を1個ずつ付けます。同じラベルの頂点の集まりが1つの領域です。「頂点番号」と「領域番号」は別の番号です。6頂点・2領域なら、頂点番号は `0`〜`5`、領域番号は `0`〜`1` です。

以下のグラフでは、隣り合う番号の頂点を辺で結びます。

| 頂点番号 | 0 | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|---|
| 分け方Aの領域番号 | 0 | 0 | 0 | 1 | 1 | 1 |
| 分け方Bの領域番号 | 0 | 1 | 0 | 1 | 1 | 1 |

Aの領域0は、頂点0・1・2が互いにつながっています。Bの領域0は頂点0と2ですが、間の頂点1は領域1です。領域0の頂点だけを通って移動できないため、領域0は**連結ではありません**。

既定では、すべての領域について「空ではない」「領域内の頂点だけを通ってつながる」を要求します。他の領域を通る道が存在しても、その領域が連結であることにはなりません。領域ごとに連結性を要求しない設定も可能です。

このライブラリが求めるのは、条件を守りながら、目的値が小さくなるラベルの配列です。辺の選択、各領域の代表点、訪問順序そのものを返すライブラリではありません。

### 1.2 必須条件と、良さを測る値

**制約**は必ず守る条件です。たとえば「各領域は連結」「領域0には2〜4頂点」「頂点0のラベルは0」のように指定します。すべての制約を満たす割当を、このガイドでは**合法解**と呼びます。

**目的関数**は合法解どうしの良さを比較する式です。このライブラリでは**小さいほど良い**値を使います。制約に違反した解を、目的値が小さいから採用することはありません。高いスコアを最大化したい場合は、たとえば「目的値＝スコアの符号を反転したもの」として表します。ただし、目的関数を足し合わせられる形で表現する必要があります。独自の全体評価も後半で扱います。

たとえば負荷を「必ず3〜5にする」なら制約、「4に近いほど良い」なら目的関数です。両方を同時に設定することもできます。

### 1.3 既定の目的関数

まず、独自の評価関数を追加しない場合を説明します。設定した制約を満たす割当 $x$ の中から、次の値を小さくします。

$$\min_ {x\in\mathcal{F}} F(x)$$

$$\begin{aligned}F(x)&=\sum_ {v=0}^{N-1} U_ {v,x_ {v}}\\&\quad+\sum_ {e\in E_ {\mathrm{cost}}} c_ {e}[x_ {u_ {e}}\ne x_ {v_ {e}}]\\&\quad+\sum_ {r=0}^{K-1}[n_ {r}>0]a_ {r}\\&\quad+\sum_ {t\in\mathcal{B}}[n_ {r_ {t}}>0]\lambda_ {t}(L_ {r_ {t},d_ {t}}-T_ {t})^{2}\end{aligned}$$

領域の頂点数と負荷の合計は、次の式で定義します。

$$n_ {r}=\sum_ {v=0}^{N-1}[x_ {v}=r],\qquad L_ {r,d}=\sum_ {v=0}^{N-1}[x_ {v}=r]w_ {v,d}$$

`[...]` は「中の条件が正しければ1、そうでなければ0」です。たとえば $[2\ne 3]=1$、$[2\ne 2]=0$ です。$\sum$ は、指定した範囲の値を全部足す記号です。

| 記号 | 意味 | APIとの対応 |
|---|---|---|
| $N$ | 頂点の個数 | `Problem.n` |
| $K$ | 使用可能な領域番号の個数 | `Problem.k` |
| $v$ | 頂点番号。0から $N-1$ | 配列の添字など |
| $r$ | 領域番号。0から $K-1$ | `regions[r]` など |
| $x$ | 全頂点のラベルを並べた配列 | `initial`、`best_labels()` |
| $x_ {v}$ | 頂点 $v$ の領域番号 | `labels[v]` |
| $\mathcal{F}$ | すべての制約を満たす割当の集合 | 連結性・個数・負荷・固定・接触などの制約 |
| $F(x)$ | 割当 $x$ の目的値 | `cost`、`best_cost` |
| $U_ {v,r}$ | 頂点 $v$ を領域 $r$ に置く費用 | `unary_cost[v * k + r]` |
| $E_ {\mathrm{cost}}$ | 有効で、`PairCost` の役割を持つ辺の集合 | `edges` の一部 |
| $e$ | その集合に含まれる辺 | `edges` の添字 |
| $u_ {e},v_ {e}$ | 辺 $e$ の両端の頂点番号 | `Edge.u`, `Edge.v` |
| $c_ {e}$ | 両端のラベルが異なるときの費用 | `Edge.cut_cost` |
| $n_ {r}$ | 領域 $r$ に属する頂点数 | 領域集計の `vertices` |
| $a_ {r}$ | 領域 $r$ を使用するときの固定費 | `activation_cost[r]` |
| $D$ | 負荷の種類の数 | `load_dim` |
| $d$ | 負荷の種類の番号。0から $D-1$ | `load_bounds[d]` など |
| $w_ {v,d}$ | 頂点 $v$ が持つ種類 $d$ の整数負荷 | `load[v * load_dim + d]` |
| $L_ {r,d}$ | 領域 $r$ 内での種類 $d$ の負荷合計 | 領域集計の `load[d]` |
| $\mathcal{B}$ | 設定した均衡項の集まり | `balance_terms` |
| $t$ | 均衡項1個を指す添字 | `balance_terms[t]` |
| $r_ {t},d_ {t}$ | 均衡項 $t$ が対象とする領域・負荷の番号 | `BalanceTerm.region`, `.dim` |
| $T_ {t}$ | 目指す負荷合計 | `BalanceTerm.target` |
| $\lambda_ {t}$ | 目標からのずれをどれほど重く見るか | `BalanceTerm.coefficient` |

式を左から読むと、次の4つの費用の合計です。

1. **頂点の配置費用**：各頂点について、割り当てた先の費用を加えます。
2. **分割された辺の費用**：両端が異なる領域に分かれた辺の費用を加えます。同じ領域なら0です。
3. **領域の使用費用**：頂点が1個以上入っている領域だけ、固定費を加えます。
4. **負荷の偏りの費用**：使用中の領域について、目標値からのずれを二乗し、係数を掛けます。

配置費用・使用費用・均衡項は、省略すると0です。辺の `cut_cost` は省略すると1です。したがって最初の例では、異なる領域を結ぶ辺の費用だけを考えれば十分です。

二乗する理由は、正負のずれが打ち消し合わないようにするためです。目標4に対して合計3でも5でも、ずれの二乗は1です。合計2なら4になります。係数2なら、それぞれ費用2、2、8を加えます。通常、偏りを減らす目的には非負の係数を使います。

辺の `length` は `cut_cost` と別の値で、既定の目的関数には直接入りません。実数特徴 `feature` も、設定しただけでは費用を生みません。これらを目的値に使う方法は、独自モデルの例で扱います。

### 1.4 表せる条件と用途

領域ごとの頂点数や複数の整数負荷、固定頂点、頂点ごとの割当先候補、領域間の接触、使用領域数などを表せます。連結な担当区域、連結なマス集合、負荷を分散した区画、前のターンから少し調整する領域分割が用途の例です。

ただし、最適解を保証するライブラリではありません。また、制限内に合法解を見つけられなくても、「合法解が存在しない」と証明されたわけではありません。強い制約がある問題では、利用者が合法な初期解を用意できるかも、使いやすさを左右します。

## 2. 類似ソルバーとの使い分け

問題に制限がある場合は、より専用の方法で最適解を求められます。厳密解が求まれば、その問題の目的値は本ライブラリの合法解と同じか、それより良くなります。ただし、計算時間も必ず短くなるという意味ではありません。

次の表は、文献・公式仕様に基づいて、本ライブラリの設定をどこまで単純化できるか整理したものです。条件を外すと、同じ結論は使えません。

| 問題の条件 | 候補 | 使い分け |
|---|---|---|
| 頂点どうしを結び付ける費用・制約がなく、費用が配置費用だけ。領域の個数・負荷・使用数にも制約がない | 頂点ごとに許された最安の領域を選ぶ | この場合は頂点ごとに独立して最適解が決まります。本ライブラリで探索する必要はありません。 |
| 連結性・接触・辺費用を除き、配置費用と、各領域に常時適用する頂点数の上限・下限だけを扱う | 最小費用流。1対1割当なら線形割当ソルバーも候補 | 固定・許可ラベルは割当候補の制限として表せます。頂点数の下限には、下限付きフローへの変換などが必要です。「空なら下限を免除」、一般の負荷、使用時だけ払う固定費まで同じ単純な形で扱えるわけではありません。[1][2] |
| 2ラベル、非負の切断費用と配置費用だけ。連結性・頂点数・負荷・接触・使用領域数などの追加条件はない | s-t最小カット | この条件では厳密解を求められます。各ラベルを使うことを要求する場合も、その条件を別途表現する必要があります。「2領域だから」というだけでは適用できません。[3][4] |
| 小規模で、取り得る割当が十分少ない | 全列挙 | 全候補を検査し切れば厳密解が得られます。制約を無視した候補数は $K^{N}$ なので、頂点数を増やすと急激に難しくなります。小さな検証用の正解作りにも使えます。 |
| 連結性を含む条件・目的値を整数の制約式に書き直せる | CP-SAT、混合整数計画（MIP）など | 連結性を含めた定式化を利用者が用意します。整数への変換が必要な場合、丸めた目的と元の目的は別物です。最適性の証明を得たい場合や、比較用の基準解を作る場合に候補になります。[5] |

CP-SATの `FEASIBLE` は「合法解を発見」、`OPTIMAL` は「最適性まで確認」という区別があります。時間を切って使用した結果を、常に厳密解と呼ばないようにします。[5]

変換した数式を、選んだ実装が扱える型で表すことも必要です。たとえばAtCoder Libraryのフローは整数容量を、最小費用流は整数費用を扱います。元の費用を整数倍して正確に表せる場合と、丸めによって違う目的関数になる場合を区別してください。[1][3]

一般的なグラフ分割ソルバーとしては **METIS** も候補です。グラフをバランスよく分ける用途向けの専用実装を検討したい場合に比較対象になります。公式実装は複数の分割方式を提供しています。本ガイドのように接触ペアの範囲、領域別の許可条件、独自の全体評価、短いターンごとの更新を組み合わせる場合は、必要な条件を同じ意味で表現できるか個別に確認してください。[6]

また、配置費用とラベル間の相互作用を主とする問題には、グラフカットを使う多ラベルの近似法もあります。2ラベルの厳密な最小カットとは区別します。相互作用に対する条件があり、本ライブラリの任意の制約付き分割にそのまま使える方法ではありません。[4]

本ライブラリは、これらの単純な条件に収まらず、合法な分割を出発点に、独自評価や制約を組み合わせて改善したい場合に検討します。

## 3. 実行の準備

`connected_partition_solver_v08.hpp` と、例を貼り付けた `main.cpp` を同じフォルダーに置きます。以下でコンパイルして実行できます。

```sh
g++ -std=c++20 -O2 -Wall -Wextra main.cpp -o example
./example
```

ヘッダは標準ライブラリを使いますが、`bits/stdc++.h` を含むため、ここではGCCとlibstdc++の環境を前提にします。別途リンクする外部最適化ライブラリは不要です。C++17では動きません。

付属ZIPでは、ヘッダがルート、例が `examples` フォルダーにあります。展開したフォルダーからは、たとえば次のように実行します。

```sh
g++ -std=c++20 -O2 -I. examples/01_minimal.cpp -o example
./example
```

入力ファイルは必要ありません。全例で小さな入力をコードに含めています。基本例の既定の実行予算は1秒です。以後は説明しやすいように主に試行数で終了させ、時間指定を学ぶ例だけ実時間を使います。

AHCの提出用に1ファイルへまとめるときは、ヘッダ内のクラス定義を提出コードへ取り込みます。この配布ヘッダの末尾には「単独コンパイル時のテスト」が条件付きで含まれています。ヘッダ全体を単純に貼り付けるとテスト側の `main` も有効になるため、`#if __INCLUDE_LEVEL__ == 0` 以降のテスト部分を除き、先頭の `#pragma once` と例側のヘッダ読み込み行も除いてください。通常の `#include` による利用では、この作業は不要です。

## 4. 基本となる分割を動かす

### ステップ01：境界の費用だけを小さくする

まず、第1節の6頂点を2領域に分けます。辺の切断費用を次のようにします。

| 辺の両端 | 0と1 | 1と2 | 2と3 | 3と4 | 4と5 |
|---|---|---|---|---|---|
| 異なる領域に分けたときの費用 | 1 | 8 | 10 | 8 | 1 |

初期解は `{0, 0, 0, 1, 1, 1}` です。異なるラベルを結ぶのは頂点2と3の辺だけなので、目的値は10です。端の費用1の辺で分ければ、目的値1の合法解になります。各領域の大きさをそろえる条件は、まだ入れていません。

**ファイル：`examples/01_minimal.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};
    auto result = solver.improve(initial);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

コードを上から順に読みます。

1. `using Solver = ConnectedPartitionSolver<double>` は、長い型名に `Solver` という短い名前を付けています。`double` は目的値を小数も扱える型で計算する指定です。
2. `Problem p(6, 2)` の第1引数 `6` は頂点数、第2引数 `2` は領域番号の個数です。この時点で2領域分の既定ルールも作られます。
3. 辺の `{0, 1, 1}` は順に、端点 `u=0`、端点 `v=1`、切断費用 `cut_cost=1` です。他の4本も同じ読み方です。省略した `length` は1、`enabled` はtrue、`roles` は接続・接触・費用の3役すべてです。辺は無向として接続を表します。
4. `Solver solver(p)` は問題をsolverに渡します。solverは問題のコピーを所有するので、後から `p` だけ書き換えてもsolverの問題は変わりません。
5. `initial[v]` が頂点 `v` の初期ラベルです。配列の長さは6、値は0か1にします。すべての頂点を割り当てます。
6. `improve(initial)` はこの合法解を出発点に改善します。オプションの省略時は最大1,000,000マイクロ秒、つまり1秒の相対予算です。得られた最良値を `result.best_cost` に持ちます。
7. `best_cost` は「値がある場合とない場合」を表す `std::optional<double>` です。`if (!result.best_cost)` は値がない場合を判定します。目的値0は正常な値であり、「解なし」ではありません。
8. 値があれば `*result.best_cost` で目的値を取り出します。`best_labels()` で最良解のラベルを順に表示します。

目的値1の出力では、たとえば `{0, 1, 1, 1, 1, 1}` のような分割になります。領域番号を反転した解なども同じ目的値です。時間で止める探索なので、表示される配列がいつも同じである必要はありません。最適値1はこの小さな問題について手計算で分かる値であり、solverが最適性を証明したという意味ではありません。

### ステップ02：評価だけを行い、結果を確かめる

ステップ01に、探索前後の検査を追加します。併せて、毎回ほぼ同じ条件で試すために、時間ではなく試行数で終了させます。

**ファイル：`examples/02_evaluate.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};
    auto before = solver.evaluate(initial);
    std::cout << "initial feasible = " << before.feasible << '\n';
    if (before.cost) std::cout << "initial cost = " << *before.cost << '\n';
    std::vector<int> broken{0, 1, 0, 1, 1, 1};
    auto bad = solver.evaluate(broken);
    std::cout << "broken feasible = " << bad.feasible << '\n';
    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    opt.seed = 42;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';

    auto checked = solver.evaluate(solver.best_labels());
    assert(checked.feasible && checked.cost);
    assert(std::abs(*checked.cost - *result.best_cost) < 1e-9);
    auto view = solver.best_labels();
    std::vector<int> saved(view.begin(), view.end());
    std::cout << "saved vertices = " << saved.size() << '\n';
}
```

`evaluate(labels)` は、渡した配列について制約と目的値を全体から計算します。探索は行いません。戻り値 `Evaluation` の `feasible` が合法性、`cost` が合法な場合の目的値です。初期解は `feasible=1, cost=10`、`broken` は領域0が分断されるので `feasible=0`、`cost` に値はありません。

`Options` には探索の設定を入れます。

| 設定 | この例の値 | 意味 |
|---|---|---|
| `budget_us` | `-1` | 相対時間による終了を無効にする |
| `max_steps` | `2'000` | 候補を試す回数を最大2,000回にする。`'` はC++の数字の区切りで、値は2000 |
| `seed` | `42` | 乱数列の初期値。42に特別な意味はなく、比較時に固定するための値 |

`seed` を省略した場合の初期値は0です。以後、明示的に値を変えない例ではこの既定値を使います。

`improve(initial, opt)` の第2引数がこの設定です。同じ実装・入力・設定で、時間制限が割り込まなければ、試行数固定の比較は再現しやすくなります。別バージョンや実行環境をまたぐ完全一致を保証するものではありません。

後半の `assert` は、この教材の想定が成り立つか確認しています。`std::abs(...) < 1e-9` は差の絶対値が0.000000001未満という意味です。実数には丸め誤差があるため、この例では完全一致を要求していません。一般の大きな目的値では、その大きさに応じた許容誤差を考えます。

`best_labels()` の戻り値は `std::span<const int>`、つまり配列の実体をコピーしない**読み取り用の窓**です。`Result` 自体もラベルを保存していません。次の変更操作の後まで解を残したければ、例の `saved` のように `vector<int>` へコピーします。`begin()` と `end()` はコピーする範囲の始まりと終わりを指します。

`evaluate` は探索状態を初期化しません。`evaluate` だけ呼んだ直後に、後で学ぶ `resume` を呼ぶことはできません。

### ステップ03：領域の大きさに上下限を付ける

ステップ01では1頂点と5頂点に偏ってもよい問題でした。ここではステップ02の探索設定を使い、各領域を2〜4頂点に制限します。

**ファイル：`examples/03_vertex_bounds.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    for (auto& rule : p.regions) {
        rule.min_vertices = 2;
        rule.max_vertices = 4;
    }
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`p.regions` は領域ごとのルールの配列です。`auto& rule` の `&` により、コピーではなく配列内のルールを書き換えています。

`min_vertices=2` は各非空領域の下限、`max_vertices=4` は上限です。どちらも端の値を含みます。既定の `required=true` により、領域を空にはできません。既定の `connected=true` も引き続き有効です。

初期解の3対3は合法です。1対5は違反します。2対4または4対2の境界費用8が、この例の最適値になります。条件に違反する費用1の解を採用しないことを確認してください。

同じ配列の各要素を別々に設定すれば、領域0だけ2〜3頂点、領域1だけ3〜4頂点、といった異なるルールにもできます。領域番号は、単なる色の名前ではなく、異なる条件を持てる識別番号です。

### ステップ04：頂点ごとの「向いている領域」を費用にする

今度はステップ01の問題に戻り、配置費用だけを追加します。頂点0〜2は領域0、頂点3〜5は領域1に置くのを好ましいとします。

**ファイル：`examples/04_unary.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    p.unary_cost = {0, 5, 0, 5, 0, 5,
                    5, 0, 5, 0, 5, 0};
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`unary_cost` は、頂点数×領域数、ここでは12個の費用です。頂点ごとに、領域0の費用、領域1の費用、という順で平たく並べます。

| 頂点 | 領域0に置く費用 | 領域1に置く費用 |
|---|---|---|
| 0, 1, 2 | 0 | 5 |
| 3, 4, 5 | 5 | 0 |

具体的な添字は `unary_cost[v * p.k + r]` です。頂点2を領域1へ置く費用は `unary_cost[2 * 2 + 1]`、つまり6個目の値です。

費用5は「望ましくないが許す」の強さです。禁止ではありません。初期解では配置費用が0なので合計10です。境界を1頂点ずらすと辺費用は8になりますが、配置費用5が加わって13になります。この例では、単に境界の費用を下げるだけでは全体の改善になりません。

配置費用と辺費用は足し合わせます。異なる単位の指標を組み合わせるときは、利用者が係数を決めて比較可能な費用にします。

### ステップ05：頂点数とは別の負荷を制限する

ステップ01の問題に、「各頂点の仕事量」を加えます。領域ごとの仕事量合計を3〜5にします。

**ファイル：`examples/05_load.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    p.load_dim = 1;
    p.load = {2, 1, 1, 1, 1, 2};
    for (auto& rule : p.regions) rule.load_bounds = {{3, 5}};
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`load_dim=1` は負荷の種類を1種類にする指定です。`load` の値は頂点0から順に2、1、1、1、1、2です。型は `int64_t`、符号付き64ビット整数です。

`load_bounds={{3, 5}}` の外側の波括弧は「種類ごとの範囲の配列」、内側の `{3, 5}` は `IntRange{lower, upper}` です。下限3・上限5を、唯一の種類0へ設定しています。

初期解では領域0の負荷は $2+1+1=4$、領域1も $1+1+2=4$ で合法です。同じ3頂点でも、負荷が同じとは限りません。`min_vertices` と `load_bounds` は別々の条件です。

`load_bounds` を空にすると、その領域の負荷制約はありません。`IntRange` の既定値は64ビット整数の最小値から最大値までなので、片側だけ制限したいときは、もう片側を既定相当の値にできます。上限・下限を付けても、その値へ近づく目的関数は自動では追加されません。

### ステップ06：複数種類と、符号付きの負荷

ステップ05に2種類目を追加します。種類0は仕事量、種類1は収支とします。収入を正、支出を負で表し、領域内の収支を−1〜1にします。

**ファイル：`examples/06_multiple_loads.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    p.load_dim = 2;
    p.load = {2, 2, 1, -1, 1, -1,
              1, 1, 1, -1, 2, 0};
    for (auto& rule : p.regions) rule.load_bounds = {{3, 5}, {-1, 1}};
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`load_dim=2` なので、各頂点について「仕事量、収支」の2個を並べます。

| 頂点 | 仕事量：種類0 | 収支：種類1 |
|---|---|---|
| 0 | 2 | 2 |
| 1 | 1 | −1 |
| 2 | 1 | −1 |
| 3 | 1 | 1 |
| 4 | 1 | −1 |
| 5 | 2 | 0 |

添字は `load[v * load_dim + d]` です。`load_bounds={{3, 5}, {-1, 1}}` は、種類0を3〜5、種類1を−1〜1にします。範囲を指定する領域では、種類数と同じ2個の `IntRange` を用意します。

初期解の収支は領域0で $2-1-1=0$、領域1で $1-1+0=0$ です。負荷は負の値も扱えます。頂点を増やせば合計が増える、という前提はありません。逆に、比率や最大値は単純な合計では表せません。必要なら後半の特徴集計や独自の条件を使います。

### ステップ07：負荷を目標へ近づける

ステップ05の仕事量を使い、今回は3〜5という範囲を外します。その代わり、4から離れると費用を加えます。

**ファイル：`examples/07_balance.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    p.load_dim = 1;
    p.load = {2, 1, 1, 1, 1, 2};
    p.balance_terms = {{0, 0, 4.0, 2.0}, {1, 0, 4.0, 2.0}};
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`balance_terms` の1要素 `{0, 0, 4.0, 2.0}` は、`BalanceTerm` の4項目です。

| 項目 | 値 | 意味 |
|---|---|---|
| `region` | `0` | 領域0を対象とする |
| `dim` | `0` | 仕事量、つまり負荷の種類0を見る |
| `target` | `4.0` | 負荷合計4を目標にする |
| `coefficient` | `2.0` | ずれの二乗を2倍して目的値へ加える |

2個目は領域1に同じ目標を設定しています。この例の追加費用は $2(L_ {0,0}-4)^2+2(L_ {1,0}-4)^2$ です。

3と5へ分けるなら追加費用は4、辺費用は8で合計12です。4と4なら追加費用0、辺費用10で合計10です。したがって、費用の合計としては3対3の初期解が良いことが分かります。

同じ領域・種類に複数の均衡項を登録した場合も、登録したすべての項を加えます。未使用を許した領域が空のときは、既定の均衡項と使用費用を加えません。

### ステップ08：動かしてはいけない頂点

ステップ01に固定条件を加えます。左端の頂点0は領域0、右端の頂点5は領域1から動かしません。

**ファイル：`examples/08_fixed.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    p.fixed.assign(p.n, -1);
    p.fixed[0] = 0;
    p.fixed[5] = 1;
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`fixed.assign(p.n, -1)` は、長さ6の配列を作り、全要素を−1にします。`fixed` に限って、−1は「固定しない」です。その後、頂点0の値を0、頂点5の値を1に変えています。

`fixed` が空なら、固定頂点はありません。指定するなら必ず頂点数分を用意します。

初期解の `initial` に−1を入れて「未割当」を表すことはできません。`fixed` の−1と、解のラベルの値の範囲を混同しないでください。初期解は全頂点を0〜 `k-1` に割り当てる必要があります。

### ステップ09：頂点ごとの割当先候補

ステップ08に、「頂点1は領域0だけ」「頂点4は領域1だけ」という制限を追加します。領域が多い問題では、複数候補の指定にも使えます。

**ファイル：`examples/09_domains.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    p.fixed.assign(p.n, -1);
    p.fixed[0] = 0;
    p.fixed[5] = 1;
    p.domains = {{1, {0}}, {2, {0, 1}}, {4, {1}}};
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`LabelDomain{vertex, labels}` は、指定頂点の許可ラベルの集合です。`{1, {0}}` は頂点1の許可先が0だけ、`{2, {0, 1}}` は頂点2が0か1、`{4, {1}}` は頂点4が1だけ、という意味です。この例の頂点2の指定は、集合の書き方を示すために明示しています。省略しても同じ自由度です。

登録しない頂点は、すべての領域を許可します。登録した空集合は「許可先なし」であり、自由という意味ではありません。固定条件もあれば、固定と許可集合を両方満たす必要があります。

1頂点について登録する `LabelDomain` は1個です。ラベルの順序は自由で、重複ラベルは内部で整理されます。頂点そのものの重複登録は入力形式の誤りです。

## 5. 盤面・接触・背景へ広げる

### ステップ10：2次元の盤面をグラフにする

ステップ03の頂点数の下限を使い、入力グラフを2行3列の盤面に変えます。新しいsolver APIは必要ありません。マスを頂点、上下左右の隣接を辺として登録します。

**ファイル：`examples/10_grid.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    const int height = 2, width = 3;
    Solver::Problem p(height * width, 2);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int v = y * width + x;
            if (x + 1 < width) p.edges.push_back({v, v + 1, 1});
            if (y + 1 < height) p.edges.push_back({v, v + width, 1});
        }
    }

    for (auto& rule : p.regions) rule.min_vertices = 2;
    Solver solver(p);
    std::vector<int> initial{0, 0, 1, 0, 0, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';

    auto labels = solver.best_labels();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) std::cout << labels[y * width + x] << ' ';
        std::cout << '\n';
    }
}
```

`height=2` が行数、`width=3` が列数です。上からの行番号 `y`、左からの列番号 `x` から、頂点番号 `v=y*width+x` を作ります。

| 座標 | (0,0) | (0,1) | (0,2) | (1,0) | (1,1) | (1,2) |
|---|---|---|---|---|---|---|
| 頂点番号 | 0 | 1 | 2 | 3 | 4 | 5 |
| 初期ラベル | 0 | 0 | 1 | 0 | 0 | 1 |

`v+1` は右隣、`v+width` は下隣です。端を越えない場合だけ辺を加えます。無向辺なので、右と下だけ登録すれば十分です。左と上も同時に登録すると平行辺になり、費用と接触数が重複します。

各辺の費用は1、各領域の最小頂点数は2です。初期解は左右に分かれ、領域をまたぐ辺は2本なので目的値2です。最後の二重ループは、一次元の結果を元の行・列に戻して表示しています。

斜め方向も移動可能な問題なら、その辺を追加します。どの辺を追加するかが、「つながっている」の定義を決めます。

### ステップ11：接続・接触・費用の辺を分ける

ステップ08の1列の問題に戻ります。隣接はそのままに、離れた頂点1と4を同じ領域にしたい、という費用を追加します。この費用用の辺によって、移動可能な道を増やさないようにします。

**ファイル：`examples/11_edge_roles.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    for (auto& e : p.edges) {
        e.cut_cost = 0;
        e.length = 2.0;
        e.roles = Solver::Connect | Solver::Contact;
    }
    p.edges.push_back({1, 4, 20.0, 0.0, true, Solver::PairCost});
    p.edges.push_back({0, 5, 0.0, 0.0, false, Solver::Connect});
    p.fixed.assign(p.n, -1);
    p.fixed[0] = 0;
    p.fixed[5] = 1;
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

辺には次の3つの独立した役割があります。

| 役割 | 何に使うか |
|---|---|
| `Connect` | 領域内の連結性を調べる道 |
| `Contact` | 異なる領域どうしの接触辺数と境界長の集計 |
| `PairCost` | 両端のラベルを使った目的関数 |

`Solver::Connect | Solver::Contact` の `|` は、役割を組み合わせるビット演算です。既存の5本は接続と接触だけにし、`cut_cost=0`、接触長 `length=2.0` とします。`PairCost` がないので、これらの辺の切断費用は目的値に使われません。

追加した `{1, 4, 20.0, 0.0, true, Solver::PairCost}` は、順に次の6項目です。

1. `u=1`：第1端点。
2. `v=4`：第2端点。
3. `cut_cost=20.0`：端点のラベルが違うと費用20。
4. `length=0.0`：接触長。この辺には `Contact` がないので集計されません。
5. `enabled=true`：辺を有効にする。
6. `roles=PairCost`：目的関数だけに使い、連結性や接触には使わない。

最後の `{0, 5, 0.0, 0.0, false, Solver::Connect}` は、無効な接続辺です。`enabled=false` なので、どの役割にも使われません。将来、辺を有効化する問題を表せます。実際に変更を反映する手順はステップ29で学びます。

初期解では頂点1と4のラベルが違うので費用20です。頂点0だけを領域0にすれば、1と4は領域1にそろい、固定条件と連結性を満たしたまま費用0になります。

長さ0は「無効」を意味しません。有効な `Contact` 辺なら、長さ0でも接触辺数は1本増えます。有効な `Connect` 辺なら、長さ0でもつながります。自己ループは登録できません。平行辺は登録できますが、1本ずつ個別に数えます。

### ステップ12：領域どうしの接触を制限する

ステップ08の1列の問題を3領域にします。領域0と1、1と2は接触し、0と2は直接接触しないようにします。

**ファイル：`examples/12_contacts.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 3);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};
    p.contacts = {{0, 1, 1, 1}, {1, 2, 1, 1}};
    p.allow_unlisted_contacts = false;
    p.fixed = {0, -1, -1, -1, -1, 2};
    Solver solver(p);
    std::vector<int> initial{0, 0, 1, 1, 2, 2};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`Problem(6, 3)` で使用可能ラベルを0・1・2に増やしました。初期解も `{0,0,1,1,2,2}` に変え、左端を0、右端を2に固定しています。

`ContactRule{a, b, min_edges, max_edges}` の `{0, 1, 1, 1}` は、領域0と1を結ぶ有効な `Contact` 辺を、ちょうど1本要求します。`{1, 2, 1, 1}` も同様です。接触条件は領域番号のペアであり、頂点番号のペアではありません。

`allow_unlisted_contacts=false` は、登録していない領域ペアの接触を禁止します。この例では0と2です。trueのままなら未登録ペアも許可され、登録ペアだけに範囲を適用します。

`a,b` の順序は接触条件の意味を変えません。同じペアを二重登録することはできません。上下限を省略した `{0,1}` は0〜 `INT_MAX` で、制限を加えず集計対象として登録する書き方です。

元の分割の「どの領域どうしが隣接するか」を保存したければ、元から隣接するペアだけ登録して下限1、上限は省略相当、未登録を禁止とします。これで隣接の有無を保存できます。境界の位置や長さまで保存する指定ではありません。

### ステップ13：使わない領域を許す

ステップ12と同じ6頂点・3ラベルで、接触条件を外します。今度は、3個の候補領域のうち2個だけを使うことも許します。

**ファイル：`examples/13_optional_regions.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 3);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};
    for (auto& rule : p.regions) {
        rule.required = false;
        rule.min_vertices = 2;
        rule.max_vertices = 4;
    }
    p.min_active_regions = 2;
    p.max_active_regions = 3;
    p.activation_cost = {0, 0, 6};
    Solver solver(p);
    std::vector<int> initial{0, 0, 1, 1, 2, 2};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`required=false` にすると、その領域を空にできます。`min_vertices=2, max_vertices=4` は、**使用する場合に**2〜4頂点という条件です。空の領域には頂点数・負荷の上下限を適用しません。連結性も空の領域を理由に失敗しません。

`min_active_regions=2`、`max_active_regions=3` は、頂点が入った領域の数を2〜3に制限します。ラベル候補の総数 `k=3` と、実際の使用数は別です。

`activation_cost={0,0,6}` は、領域0と1の使用費用を0、領域2だけ6にしています。領域2に何頂点入っても使用費用は6、空なら0です。初期解の費用は、2本の境界の8+8と使用費用6で22です。領域2を空にして、領域0と1だけで分ける選択肢ができます。

`required=true` のまま `min_vertices=0` にしても空にはできません。空を許すスイッチは `required` です。また、接触の下限は領域が空でも有効です。ある領域を空にする可能性があるのに、その領域との接触を1本以上必須にすると、結果として空にはできなくなります。

### ステップ14：背景だけは分断していてよい

ステップ08から、領域0を背景とみなす例を作ります。中央の頂点2・3を領域1に固定し、両端は背景に固定します。

**ファイル：`examples/14_background.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    p.regions[0].connected = false;
    p.fixed = {0, -1, 1, 1, -1, 0};
    Solver solver(p);
    std::vector<int> initial{0, 0, 1, 1, 0, 0};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`p.regions[0].connected=false` により、領域0だけ連結性を要求しません。領域1には既定の `connected=true` が残ります。

`fixed={0,-1,1,1,-1,0}` は、両端を0、中央を1に固定し、頂点1・4を自由にします。初期解の背景は左右に分かれていますが、背景には連結性を要求していないので合法です。

この指定が変えるのは連結性の条件だけです。背景との接触や、異なるラベルを結ぶ辺の費用まで無効にはなりません。領域0も `required=true` のままなので、完全に空にはできません。

### ステップ15：盤面の外を通って背景がつながる

ステップ14の背景を、「盤面内で自由に分断してよい」から「盤面の外側を通るならつながってよい」へ変えます。外側を表す補助頂点を1個追加します。

**ファイル：`examples/15_outside_vertex.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(7, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};
    p.edges.push_back({6, 0, 0.0, 0.0, true, Solver::Connect});
    p.edges.push_back({6, 5, 0.0, 0.0, true, Solver::Connect});
    p.fixed = {0, -1, 1, 1, -1, 0, 0};
    Solver solver(p);
    std::vector<int> initial{0, 0, 1, 1, 0, 0, 0};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';

    std::cout << "board: ";
    for (int v = 0; v < 6; ++v) std::cout << solver.best_labels()[v] << ' ';
    std::cout << '\n';
}
```

`Problem(7,2)` の頂点6が補助頂点です。`{6,0,...}` と `{6,5,...}` は外側から両端への辺で、接続だけに使います。補助頂点を `fixed[6]=0` とし、背景専用にします。今度は `connected=false` を設定していないので、背景も連結である必要があります。

初期解では、左側の背景から頂点6を通って右側の背景へ行けるので合法です。2次元盤面なら、外周のマスを同じ補助頂点へ接続する発想になります。内部に閉じ込められた背景まで自動的につながるわけではありません。

`length=0`、`cut_cost=0` に加えて役割も `Connect` だけにして、補助的な接続を費用・接触へ混ぜないようにしています。

補助頂点も領域の頂点数に数えます。個数制約を使う場合は、その1個を考慮します。負荷・特徴・配置費用の配列を指定する場合も7頂点分を用意し、補助頂点の値を意図した値、通常は0にします。最後の `board:` の表示は元の6頂点だけです。

## 6. 探索の始め方・止め方・続け方

### ステップ16：初期解の構築も任せる

ステップ03の「各領域2頂点以上」という問題を使い、`initial` を用意する代わりに `solve` を使います。

**ファイル：`examples/16_solve.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    for (auto& rule : p.regions) rule.min_vertices = 2;
    Solver solver(p);

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.solve(opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

変更は `solver.solve(opt)` です。引数は探索設定だけで、初期ラベルを渡しません。solverは割当を構築し、必要なら修復し、合法解が得られれば改善します。

この例の `max_steps=2'000` は探索・修復の試行数を制限します。初期構築そのものの処理回数を2,000に切る指定ではありません。`max_steps=0` にしても、`solve` は最初の構築と評価を行います。

`solve` が合法解を見つける保証はありません。特に、狭い許可集合・厳しい接触・複数負荷などを組み合わせると、構築が難しいことがあります。ユーザー側で合法解を作れるなら `improve` が自然です。すでにある解を継続したい場合に `solve` を呼ぶと、新しい構築からやり直します。

### ステップ17：違反した割当を修復する

ステップ16と同じ問題に、分断した配列を渡します。`improve` と `repair` の違いを確認します。

**ファイル：`examples/17_repair.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    for (auto& rule : p.regions) rule.min_vertices = 2;
    Solver solver(p);
    std::vector<int> broken{0, 1, 0, 1, 0, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;

    auto rejected = solver.improve(broken, opt);
    assert(!rejected.best_cost);
    assert(rejected.reason == Solver::StopReason::invalid_initial);
    auto result = solver.repair(broken, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`broken={0,1,0,1,0,1}` は配列の長さもラベル範囲も正しいですが、両領域とも分断しています。`improve` は修復を行わず、`best_cost` なし、終了理由 `invalid_initial` を返します。

`repair(broken,opt)` は、違反を直すことから始めます。合法解を見つけると、残った予算で目的値も改善します。合法解が見つからなければ `best_cost` に値がありません。今回は0〜1という範囲内の完全な割当なので修復の入力にできますが、未割当の−1を含む配列は受け付けません。

修復は「違反のある解を必ず直す関数」ではありません。違反が少ないほど通常は使いやすいものの、成功は問題と時間に依存します。初期解の不正と、存在しないラベル番号などの入力形式の誤りも別です。後者は `assert` の対象で、戻り値による通常の失敗通知ではありません。

### ステップ18：焼きなましと、変更の種類を選ぶ

ステップ03を出発点に、探索中の採用方針を変えます。ここでは利用に必要な意味だけを扱い、具体的な変更の作り方は付録で説明します。

**ファイル：`examples/18_annealing.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    for (auto& rule : p.regions) rule.min_vertices = 2;
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;

    opt.acceptance = Solver::Acceptance::annealing;
    opt.temperature = 0;
    opt.final_temperature_ratio = 0.02;
    opt.weights[Solver::Move] = 60;
    opt.weights[Solver::Swap] = 18;
    opt.weights[Solver::Block] = 12;
    opt.weights[Solver::Relabel] = 2;
    opt.weights[Solver::Recombine] = 7;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

**近傍**とは、今の割当を少し変更して作る候補です。既定の `Acceptance::greedy` は目的値が悪くなる候補を採用しません。同じ値の候補は採用できます。`annealing` は焼きなましで、合法な候補のうち、悪化するものも確率的に採用して別の改善へ進む可能性を残します。

一時的に悪化しても、返すのはその実行で確認した最良解です。制約違反を許して探索する指定ではありません。

| 項目 | この例の値 | 意味 |
|---|---|---|
| `acceptance` | `annealing` | 悪化候補も確率的に採用する |
| `temperature` | `0` | 悪化量から温度の大きさを自動推定する。正値なら利用者が初期温度を指定 |
| `final_temperature_ratio` | `0.02` | 終盤の温度を基準の2%へ下げる割合。0より大きく1以下 |
| `weights[Move]` | `60` | 1頂点の移動を選ぶ重み |
| `weights[Swap]` | `18` | 2頂点の入れ替えを選ぶ重み |
| `weights[Block]` | `12` | 小さな連結ブロックの移動・交換を選ぶ重み |
| `weights[Relabel]` | `2` | 領域全体の番号交換・併合を選ぶ重み |
| `weights[Recombine]` | `7` | 2領域をまとめて分け直す変更を選ぶ重み |

この重みは既定値を明示したもので、合計は99です。60は60%ではなく、提案の種類を選ぶ段階で60/99の割合という意味です。0にすればその種類を無効にできます。非負整数を使い、合計が通常の `int` に収まる範囲にします。

候補を作れなかった試行や、制約で棄却された試行もあります。選択割合と実際に採用された変更の割合は異なります。焼きなましが常に良いとは限りません。問題ごとに、同じ時間予算で目的値と実行時間を測って決めます。

### ステップ19：マイクロ秒の予算と探索統計

ステップ02の試行数固定から、短い実時間予算へ切り替えます。時間指定には相対予算と絶対締切の2種類があります。

**ファイル：`examples/19_budget_statistics.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    Solver::Options opt;
    opt.budget_us = 5'000;
    opt.max_steps = 1'000'000;
    opt.deadline = Solver::Clock::now() + std::chrono::microseconds(8'000);
    opt.profile = true;
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';

    switch (result.reason) {
        case Solver::StopReason::time_limit: std::cout << "time limit\n"; break;
        case Solver::StopReason::step_limit: std::cout << "step limit\n"; break;
        case Solver::StopReason::no_moves: std::cout << "no moves\n"; break;
        case Solver::StopReason::invalid_initial: std::cout << "invalid initial\n"; break;
    }
    const auto& s = result.statistics;
    std::cout << "steps = " << s.steps << ", constructions = " << s.construction_attempts
              << ", elapsed_us = " << s.elapsed_us << '\n';
    const char* names[] = {"Move", "Swap", "Block", "Relabel", "Recombine"};
    for (int i = 0; i < Solver::NeighborhoodCount; ++i) {
        const auto& n = s.neighborhood[i];
        std::cout << names[i] << ' ' << n.proposed << ' ' << n.feasible << ' '
                  << n.accepted << ' ' << n.improved << ' ' << n.elapsed_ns << '\n';
    }
}
```

`budget_us=5'000` は、この探索呼出しに5,000マイクロ秒、つまり5ミリ秒を指定します。`max_steps=1'000'000` は試行数側の上限です。両方あれば、どちらかに達した時点で終了へ進みます。

`Clock` は `std::chrono::steady_clock` です。時刻調整の影響を受けにくい経過時間用の時計で、カレンダー上の日時ではありません。`deadline=Clock::now()+microseconds(8'000)` は、その行から8ミリ秒後を締切にしています。コンストラクタより前に置いたので、準備で使った時間も締切までの残時間を減らします。

相対予算と絶対締切は早い方が有効です。相対予算を `-1`、試行数を `-1` にすれば、それぞれの上限を無効にできます。ただし、時間か試行数の少なくとも1つには上限が必要です。

マイクロ秒は**指定単位**であり、その時刻までの厳密な復帰保証ではありません。初期解の取り込み・検査・採点は、予算0や期限切れでも行います。重い処理や利用者の評価関数を途中で強制停止することもできません。提出では入出力や他の計算の余裕も確保し、実機の計測で予算を決めます。

終了理由 `StopReason` は、成功・失敗とは別の情報です。合法解の有無は常に `best_cost` を見ます。

| 終了理由 | 意味 |
|---|---|
| `time_limit` | 時間上限に達した |
| `step_limit` | 試行数上限に達した |
| `no_moves` | 可動頂点がない、全重み0、ラベル候補が1個など、自明に探索しない状態 |
| `invalid_initial` | `improve`・`resume` の出発点が非合法だった |

`no_moves` は、すべての近傍を調べ尽くしたという証明ではありません。

`profile=true` にすると、近傍別の時間も測ります。計測自体の費用があるため、通常利用では既定のfalseを使います。

| 統計 | 数えているもの |
|---|---|
| `statistics.steps` | 修復を含めた試行数。棄却したものも含む |
| `construction_attempts` | 初期構築・追加構築・再構築の回数。`improve` では通常0 |
| `elapsed_us` | そのsolver呼出し全体の実時間。外で行ったコンストラクタや `update` は含まない |
| `neighborhood[i].proposed` | 改善探索で、その種類を提案しようとした回数 |
| `.feasible` | 合法であるところまで確認した候補数 |
| `.accepted` | 採用した候補数。同じ目的値の採用も含む |
| `.improved` | 最良目的値を厳密に小さくした回数 |
| `.elapsed_ns` | その種類の提案・検査などに使ったナノ秒。`profile=true` 時だけ測る |

表示の近傍ごとの5個の数は、表の `proposed` から `elapsed_ns` の順です。既定の貪欲探索では、目的値で先に棄却して連結性を調べない候補があります。このため `feasible/proposed` を「すべての候補の真の合法率」と読むことはできません。修復の内訳もこの近傍別統計には入りません。

### ステップ20：一部分だけを動かし、続きから改善する

ステップ08の固定条件を使い、変更できる頂点の集合を呼出しごとに指定します。ターンごとに注目箇所だけ調整する基礎になります。

**ファイル：`examples/20_partial_resume.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    p.fixed = {0, -1, -1, -1, -1, 1};
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;

    std::vector<int> focus{1, 2, 3, 4};
    opt.movable = std::span<const int>(focus);
    auto first = solver.improve(initial, opt);
    if (!first.best_cost) return 1;
    auto first_view = solver.best_labels();
    std::vector<int> saved(first_view.begin(), first_view.end());
    focus = {2, 3};
    opt.movable = std::span<const int>(focus);
    auto second = solver.resume(opt);
    if (!second.best_cost) return 1;
    for (int v : {0, 1, 4, 5}) assert(solver.best_labels()[v] == saved[v]);
    opt.movable = std::span<const int>{};
    auto frozen = solver.resume(opt);
    assert(frozen.best_cost && frozen.reason == Solver::StopReason::no_moves);
    opt.movable.reset();
    auto result = solver.resume(opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`movable` は `optional<span<const int>>` です。最初は頂点番号1・2・3・4を指定します。ラベル番号の集合ではありません。固定条件も併用され、可動集合に含めても固定頂点は動かせません。

`resume(opt)` は、前回から保持している状態を使って続行します。初期解を渡し直しません。同じ問題・同じ評価であれば、前回までの最良解を引き継ぎます。乱数の状態も続くため、`resume` に渡す `seed` は新たな乱数列の開始には使われません。

2回目は可動集合を `{2,3}` に変えています。このとき外側の頂点0・1・4・5は、前回返却された**最良解**のラベルに固定されます。焼きなまし内部の、一時的に悪い現在解を基準に外側を固定するわけではありません。コード中の `assert` は、この契約を確かめます。

| 指定 | 意味 |
|---|---|
| `opt.movable = span(focus)` | 指定頂点だけ変更可能 |
| `opt.movable = span<const int>{}` | 明示的な空集合。どの頂点も変更しない |
| `opt.movable.reset()` | 指定自体を外す。固定条件を除き、全頂点を変更可能 |

`span` は元の配列を所有しません。`focus` は呼出し中に生存している必要があります。`focus` の中身を代入し直すと配列の場所が変わる可能性があるため、例ではその後にspanを設定し直しています。可動頂点の順序・重複は内部で整理されます。

全体のグラフは残るため、変更しない頂点を通る接続も考慮されます。局所領域を別グラフに切り出す方法とは条件が違います。また、`solve` は可動集合を指定しないことを要求します。部分改善には `improve`・`resume`、部分修復には `repair` を使います。

### ステップ21：目的値を整数で正確に計算する

ステップ02と同じ問題を、64ビット整数の目的値で解きます。

**ファイル：`examples/21_integer_cost.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<int64_t>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

変更点は `ConnectedPartitionSolver<int64_t>` だけです。辺費用、配置費用、使用費用、均衡項の目標と係数、戻り値の目的値もこの型にそろいます。整数で表現できる費用なら、実数の丸め誤差なしに比較できます。

対応する型は `double` と `int64_t` の2種類です。`load` はどちらでも `int64_t`、辺の `length` と `feature` はどちらでも `double` です。整数型に変えたから、負荷の上限や辺の長さの型まで変わるわけではありません。

負の費用も使用できます。ただし、足し算・引き算・二乗の途中結果を含め、すべて `int64_t` の範囲へ収める必要があります。たとえば最終結果だけ小さくても、途中の大きな二乗が範囲を越える設計は使えません。実数の特徴を整数費用に使う場合は、丸め方を利用者が明示的に決めます。

## 7. 独自の評価と制約を書く

ここからは、設定配列だけでは表しにくい費用を、利用者の関数で与えます。**モデル**とは、決められた名前の関数を必要なものだけ持つオブジェクトです。特別な基底クラスを継承する必要はありません。

独自モデルでの目的関数は、次の形です。

$$F_ {\mathrm{model}}(x)=\sum_ {v=0}^{N-1} f_ {v}(v,x_ {v})+\sum_ {e\in E_ {\mathrm{cost}}} f_ {e}(e,x_ {u_ {e}},x_ {v_ {e}})+\sum_ {r=0}^{K-1} f_ {r}(r,S_ {r}(x))+f_ {g}(S(x))$$

ここで $f_ {v}$ は頂点評価、$f_ {e}$ は辺評価、$f_ {r}$ は領域評価、$f_ {g}$ は全体評価です。$S_ {r}(x)$ は領域 $r$ の頂点数・負荷・特徴・境界長の集計、$S(x)$ は全領域と全ラベル・登録接触の集計です。それ以外の記号は第1節と同じです。

関数を定義すると、対応する既定項を**置き換えます**。定義しなかった項は既定のまま残ります。

| 関数 | 置き換える項 |
|---|---|
| `vertex_cost` | `unary_cost` による配置費用 |
| `edge_cost` | `cut_cost` による辺費用 |
| `region_cost` | その領域の使用費用と、すべての均衡項 |
| `global_cost` | 既定では0の全体費用。ほかの3項はそのまま加わる |

このほか、追加制約の `extra_feasible`、割当先に応じた負荷の `load` も定義できます。関数の引数に渡るビューは、その呼出し中だけ有効です。保存して後で読んではいけません。

複数の関数を同じ `Model` に定義すれば、対応する項を同時に置き換えられます。以下では役割の違いを見やすくするため、まず1種類ずつ追加します。

モデルの関数は、同じ入力に同じ値を返すようにします。探索中に外部の評価対象やsolverの状態を書き換えず、例外を投げないものとします。関数末尾の `const` は、モデルの状態を変更しない関数であることを表すC++の指定です。

### ステップ22：配置費用を関数で書く

ステップ04の配置費用を配列ではなく式で計算します。意味は同じで、「前半は領域0、後半は領域1を好む」です。

**ファイル：`examples/22_vertex_model.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    struct Model {
        double penalty = 5;
        double vertex_cost(int vertex, int region) const {
            int preferred = vertex < 3 ? 0 : 1;
            return region == preferred ? 0.0 : penalty;
        }
    };
    Model model;
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt, model);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`Model` はこの例で利用者が定義した型です。`vertex_cost(int vertex, int region) const` を持つことで、配置費用の計算をsolverへ渡せます。戻り値は目的値と同じ `double` にします。

`vertex` は頂点番号、`region` は候補の割当先です。`preferred` はその頂点の希望先、`penalty=5` は希望先と違うときの費用です。`region == preferred ? 0.0 : penalty` は、条件が真なら0、偽なら5を返す式です。

`improve(initial,opt,model)` の第3引数がモデルです。独自評価を使うときは、`evaluate(labels,model)` にも同じモデルを渡します。省略すると標準評価になるので、同じ分割でも目的値が変わることがあります。

この関数があると `unary_cost` の既定計算は使いません。既定の配置費用に5を追加したいなら、関数の中で元の配置費用も足して返します。辺費用は上書きしていないので、ステップ01のまま残ります。

### ステップ23：ラベルの組に応じた辺費用

ステップ01の辺費用を関数に置き換えます。領域0から1への境界と、1から0への境界で費用を変える例です。

**ファイル：`examples/23_edge_model.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    struct Model {
        std::vector<double> weights;
        double edge_cost(int edge_id, int label_u, int label_v) const {
            if (label_u == label_v) return 0.0;
            return weights[edge_id] * (label_u == 0 ? 1.0 : 2.0);
        }
    };
    Model model{{1, 8, 10, 8, 1}};
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt, model);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`edge_cost` の `edge_id` は `p.edges` の添字です。`label_u` はその辺に保存した `u` のラベル、`label_v` は `v` のラベルです。小さいラベル順や、探索で見つけた向きへ並べ替えることはありません。

モデルの `weights={1,8,10,8,1}` は辺ごとの基準費用です。両端のラベルが同じなら0、異なり `label_u==0` なら1倍、それ以外なら2倍を返します。この例では2ラベルなので、後者は1から0の境界です。

接続としての辺は無向のままです。費用関数だけは、端点の保存順に応じた非対称な値を返せます。対称な費用にしたければ、自分で対称な式を書きます。同じラベルの辺にも、独自関数なら0以外の費用を返せます。

この関数が呼ばれるのは、有効で `PairCost` を持つ辺だけです。`cut_cost` はこの関数が置き換えるので、自動では加算されません。`update` で辺の並びを変えたら、辺番号を添字にするモデル側の配列も合わせて更新します。

### ステップ24：特徴の合計から、領域の広がりを測る

ステップ01の頂点番号を、1列の位置としても使います。領域内の位置のばらつきと境界長へ費用を付けます。領域ごとの平均を計算するため、位置と位置の二乗を特徴に保存します。

**ファイル：`examples/24_region_features.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    p.feature_dim = 2;
    for (int v = 0; v < p.n; ++v) {
        double x = v;
        p.feature.push_back(x);
        p.feature.push_back(x * x);
    }
    struct Model {
        double spread_weight = 1.0;
        double boundary_weight = 0.5;
        double region_cost(int, const Solver::RegionStatsView& s) const {
            if (s.vertices == 0) return 0.0;
            double sum = s.feature[0];
            double sum_sq = s.feature[1];
            double spread = std::max(0.0, sum_sq - sum * sum / s.vertices);
            return spread_weight * spread + boundary_weight * s.boundary;
        }
    };
    Model model;
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt, model);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`feature_dim=2` は特徴を2種類にします。各頂点で `x=v` を位置とし、特徴0へ `x`、特徴1へ `x*x` を入れます。配列の添字規則は負荷と同様に `feature[v*feature_dim+q]` です。`q` は特徴の種類番号です。

solverが領域内で特徴を足し合わせるので、`s.feature[0]` は位置の和、`s.feature[1]` は位置の二乗の和になります。特徴の値を設定するだけでは目的値へ加わらず、この `region_cost` が初めて費用に使っています。

領域内の頂点数を $n$、位置を $z_ {1},\ldots,z_ {n}$、平均位置を $\mu$ とします。位置の和を $S$、二乗の和を $Q$ とすると、次のように平均からのずれの二乗和を計算できます。

$$S=\sum_ {i=1}^{n}z_ {i},\qquad Q=\sum_ {i=1}^{n}z_ {i}^{2},\qquad \mu=S/n$$

$$\sum_ {i=1}^{n}(z_ {i}-\mu)^{2}=Q-\frac{S^{2}}{n}$$

左辺を展開すると $Q-2\mu S+n\mu^2$、そこへ $\mu=S/n$ を入れると右辺になります。和と二乗和さえあれば、毎回全頂点を読み直さずに広がりを計算できるわけです。これは分散そのものではなく、分散を頂点数倍した値です。

`spread_weight=1.0` はこの広がりの重み、`boundary_weight=0.5` は境界長の重みです。`max(0.0,...)` は、数学上は非負の値が実数の丸めでごく小さな負になる場合への処理です。

`RegionStatsView` は次の値を持ちます。

| フィールド | 意味 |
|---|---|
| `vertices` | 領域内の頂点数 |
| `load[d]` | 負荷の種類 $d$ の領域内合計 |
| `feature[q]` | 特徴の種類 $q$ の領域内合計 |
| `boundary` | 他領域につながる有効な `Contact` 辺の長さの和 |

`region_cost` の第1引数は領域番号です。この例は領域によらず同じ式なので、引数の名前を省略しています。空の任意領域でも領域評価は呼ばれるため、`vertices==0` なら0を返して割り算を避けています。

領域間の1本の境界は、両側の `boundary` に1回ずつ入ります。全領域で合計すると2回数えるので、この例では係数0.5を使っています。既定の辺費用も残っているため、この例の総目的は「既定の辺費用＋広がり＋境界長」です。

`boundary` は入力した辺の長さの集計で、盤面外周も含む幾何学的な周長を自動計算するものではありません。また、この `region_cost` を定義すると、既定の `activation_cost` と `balance_terms` は使われません。必要なら自分の式に含めます。

領域評価は、その領域に渡された集計値と、呼出し中に変わらないモデルのデータだけで計算してください。他領域の状態にも依存する式は、次の `global_cost` に置きます。

### ステップ25：領域どうしを比較する全体評価

ステップ05の仕事量を使い、上下限を外して「領域0と1の負荷差を小さくする」目的を追加します。さらに、登録したペアの接触長を1回だけ数えます。

**ファイル：`examples/25_global_model.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    p.load_dim = 1;
    p.load = {2, 1, 1, 1, 1, 2};
    p.contacts = {{0, 1}};
    struct Model {
        double weight = 2.0;
        double contact_weight = 0.5;
        double global_cost(const Solver::SummaryView& s) const {
            double difference = double(s.region(0).load[0]) - double(s.region(1).load[0]);
            double boundary = s.contact(0).length;
            return weight * difference * difference + contact_weight * boundary;
        }
    };
    Model model;
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt, model);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`contacts={{0,1}}` は、領域0と1の接触を集計する登録です。上下限を省略しているので、接触を必須にはしていません。配列の0番目として登録したので、モデル内では `s.contact(0)` と読みます。引数0は領域番号ではなく、登録順の添字です。

`global_cost` には、候補全体を読む `SummaryView` が渡ります。`s.region(0).load[0]` は領域0の仕事量合計、領域1も同様です。`difference` は両者の差、`weight=2.0` は差の二乗の重みです。ここではそれぞれを `double` にしてから差を取っています。

`s.contact(0).length` は、そのペアを結ぶ接触辺の長さの和です。`contact_weight=0.5` を掛けて加えます。1列のこの例では接触長が1で一定ですが、盤面などでは分割で変わる量です。既定の辺費用にもこの全体費用が加わります。

`SummaryView` で読めるAPIをまとめます。どれも1回の参照は一定時間です。

| API | 値と引数 |
|---|---|
| `size()` | 頂点数 $N$ |
| `region_count()` | 使用可能な領域番号の数 $K$ |
| `label(v)` | 頂点番号 `v` の候補ラベル |
| `active_regions()` | 頂点が入った領域数 |
| `region(r)` | 領域番号 `r` の `RegionStatsView` |
| `contact_count()` | `Problem.contacts` に登録したペアの数 |
| `contact(i).edges` | 登録番号 `i` のペアを結ぶ接触辺数 |
| `contact(i).length` | 同じペアの接触長の和 |

接触数と接触長は別です。長さ2の接触辺が3本なら、`edges=3, length=6` です。未登録のペアは `contact(i)` で直接参照できないため、モデルで必要なペアは先に登録します。

`global_cost` から `label(v)` を全頂点分読むこともできます。ただし候補のたびに全走査の費用が掛かります。負荷や特徴の集計で表現できるなら、それを読む方が通常は軽くなります。

### ステップ26：独自の必須条件を追加する

ステップ01へ、「領域0と1の頂点数の差は2以下」という条件を追加します。費用としてではなく、超えたら非合法とします。

**ファイル：`examples/26_extra_constraint.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    struct Model {
        int max_gap = 2;
        bool extra_feasible(const Solver::SummaryView& s) const {
            int difference = s.region(0).vertices - s.region(1).vertices;
            return std::abs(difference) <= max_gap;
        }
    };
    Model model;
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt, model);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`extra_feasible` の戻り値は `bool` です。trueなら追加条件を満たし、falseなら違反です。`max_gap=2` は許す個数差の上限です。差は負にもなるため、`abs` で絶対値を取り、2以下か調べます。

この6頂点・2領域の例だけなら、ステップ03の2〜4頂点の範囲でも同じ条件を表せます。まず組込み制約を優先し、複数の領域の関係など、そこに収まらない条件へこのAPIを使います。

solverは組込み条件を通った完全な割当へ `extra_feasible` を適用し、それも通った場合だけ目的関数を呼びます。ただし、追加条件を書いても、その条件を保つ専用の変更や修復方法が自動で用意されるわけではありません。

たとえば「2領域の負荷が完全に等しい」といった非常に狭い条件を追加すると、ほとんどの変更が違反になる場合があります。表現できることと、短時間で良い解が見つかることは分けて考えます。

### ステップ27：割当先で変わる仕事量

ステップ05では、頂点の負荷はどの領域に置いても同じでした。ここでは、得意な担当なら時間1、そうでなければ時間2とし、各領域の作業時間を4以下にします。

**ファイル：`examples/27_label_dependent_load.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    p.load_dim = 1;
    for (auto& rule : p.regions) rule.load_bounds = {{0, 4}};
    struct Model {
        int64_t load(int vertex, int region, int dim) const {
            assert(dim == 0);
            int preferred = vertex < 3 ? 0 : 1;
            return region == preferred ? 1 : 2;
        }
    };
    Model model;
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;
    auto result = solver.improve(initial, opt, model);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`load_dim=1` と `load_bounds={{0,4}}` はこれまでと同じです。今回は `p.load` を設定せず、モデルの `load` が値を返します。この関数が存在すると `Problem.load` の値を置き換えます。

引数 `vertex` は頂点番号、`region` は候補の割当先、`dim` は負荷の種類番号です。今回は1種類なので `assert(dim==0)` で確認しています。前半の頂点は領域0、後半は領域1が得意とし、対応する担当なら整数1、それ以外なら整数2を返します。戻り値の型は目的値型ではなく `int64_t` です。

この場合、第1節の負荷式は、割当先にも依存する値を使って次のようになります。

$$L_ {r,d}=\sum_ {v=0}^{N-1}[x_ {v}=r]w_ {v,r,d}$$

$w_ {v,r,d}$ は頂点 $v$ を領域 $r$ に置いたときの種類 $d$ の負荷で、`model.load(v,r,d)` の戻り値です。割当先を変えると、移動元で引く量と移動先で加える量が違う場合があります。

`load` は合法性を調べる材料なので、非合法な割当の評価や修復でも呼ばれます。「合法な解でしか呼ばれない」と仮定せず、範囲内の頂点・領域・負荷番号の組で値を定義してください。

### ステップ28：評価基準を変えて続行する

ステップ22の配置費用モデルを使います。探索後、希望先から外れる費用を5から20へ変更して、保持した解を新しい基準で改善します。

**ファイル：`examples/28_model_resume.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    struct Model {
        double penalty = 5;
        double vertex_cost(int vertex, int region) const {
            int preferred = vertex < 3 ? 0 : 1;
            return region == preferred ? 0.0 : penalty;
        }
    };
    Model model;
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;

    auto first = solver.improve(initial, opt, model);
    if (!first.best_cost) return 1;
    model.penalty = 20;
    auto result = solver.resume(opt, model);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';

    auto checked = solver.evaluate(solver.best_labels(), model);
    assert(checked.feasible && checked.cost);
    assert(std::abs(*checked.cost - *result.best_cost) < 1e-9);
    auto standard = solver.resume(opt);
    if (!standard.best_cost) return 1;
    std::cout << "standard cost = " << *standard.best_cost << '\n';
}
```

`model.penalty=20` は利用者のモデルを変更しています。問題グラフは変えていないので、`update` は不要です。次の `resume(opt,model)` は独自モデルで保持候補を再評価してから続行します。変更前の目的値を、そのまま変更後の目的値と比較するわけではありません。

独自モデルを渡す `resume` では、外部データの変更を検知する専用通知は要りません。その代わり、モデルが同じに見える場合でも全体評価の費用が掛かります。モデルに外部配列への参照を持たせる場合は、呼出し中の寿命と値の不変性を利用者が守ります。

後半の `resume(opt)` はモデルを省略し、標準の目的へ戻しています。配置費用モデルが外れるので、新しい `standard.best_cost` は切断費用だけです。これはAPIの違いを示すための切替です。意図せずモデルを渡し忘れないようにしてください。

標準モデルで、問題も可動範囲も変えない `resume` は準備を軽くできます。一方、焼きなましの温度の進み具合は呼出しごとの予算で計算し直します。小さな呼出しを多数つなぐことと、1回の長い呼出しは、まったく同じ探索にはなりません。

## 8. ターン更新と複数回の実行を組み合わせる

### ステップ29：制約が変わったら、保持候補を再評価・修復する

ステップ08の分割が終わった後、新しい固定条件が届いた場面です。前の境界付近の1頂点を反対側へ固定し、辺費用も変えます。外部との通信は行わず、1ターンの更新をコード内で再現します。

**ファイル：`examples/29_turn_update.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    p.fixed = {0, -1, -1, -1, -1, 1};
    Solver solver(p);
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;

    auto first = solver.improve(initial, opt);
    if (!first.best_cost) return 1;
    auto labels = solver.best_labels();
    std::vector<int> saved(labels.begin(), labels.end());
    int cut = 0;
    while (saved[cut] == saved[cut + 1]) ++cut;
    int change_vertex = cut == 0 ? 1 : cut;
    int new_region = cut == 0 ? 0 : 1;
    p.fixed[change_vertex] = new_region;
    p.edges[2].cut_cost = 3;
    opt.budget_us = -1;
    opt.max_steps = 100'000;
    opt.deadline = Solver::Clock::now() + std::chrono::microseconds(20'000);
    solver.update(p);
    auto continued = solver.resume(opt);
    assert(!continued.best_cost);
    assert(continued.reason == Solver::StopReason::invalid_initial);
    auto result = solver.repair(opt);
    if (!result.best_cost) {
        std::vector<int> fallback(p.n, 1);
        for (int v = 0; v <= cut; ++v) fallback[v] = 0;
        fallback[change_vertex] = new_region;
        auto valid = solver.evaluate(fallback);
        if (!valid.feasible) return 1;
        Solver::Options check_only;
        check_only.budget_us = -1;
        check_only.max_steps = 0;
        result = solver.improve(fallback, check_only);
    }

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';

    assert(solver.best_labels()[change_vertex] == new_region);
    assert(solver.evaluate(solver.best_labels()).feasible);
}
```

前半は通常の `improve` です。`saved` へ解をコピーしておくのは、更新後も前の境界を使って代替解を作るためです。更新のために常に解全体のコピーが必須というわけではありません。

`cut` は異なるラベルに切り替わる辺の左端の番号です。両端が0と1に固定され、1列で両領域が連結なので、この例には境界が1つあります。`change_vertex` と `new_region` は、その境界を1頂点ずらす固定条件です。境界が最も左なら頂点1を0へ、それ以外なら境界の左端を1へ固定します。これにより、必ず前の解が新しい条件に違反し、かつ手で修復できる教材になります。

`p.edges[2].cut_cost=3` は頂点2と3の辺の費用変更です。これらの変更を `solver.update(p)` でsolverへ渡します。

`update` は前の合法性と目的値を失効させます。その直後は、古い `best_cost` が手元に残っていても、`best_labels()` を呼んで最新の合法解として使ってはいけません。新しい探索結果の `best_cost` を確認します。

この例の `resume` は、新しい固定条件への違反を確認して `invalid_initial` を返します。そこで `repair(opt)` を呼びます。ラベルを渡さない方の `repair` は、solver内に保持した候補を修復するAPIです。固定違反を直し、合法になれば残予算で改善します。

`deadline` は更新前に20,000マイクロ秒後へ設定しました。`update`・`resume`・`repair` が同じ絶対締切を共有するので、呼出しごとに20ミリ秒を新しく与えてしまうことを避けられます。試行数は各呼出しで新しく数えるため、`max_steps=100'000` 自体は合算の上限ではありません。

もし修復で合法解を得られなければ、この1列の構造を使い、境界を1頂点ずらした `fallback` を作ります。`evaluate` で新しい問題の合法性を確かめ、`max_steps=0` の `improve` で結果として登録します。これはこの例に固有の代替解であり、一般の問題なら利用者が別途用意します。期限後の代替解の作成・検査にも時間が必要なので、その分の余裕は予算設計に含めます。

`update` が再利用する範囲は次のとおりです。

| 変更 | 再利用の扱い |
|---|---|
| 頂点数・領域数が同じ | 前のラベル候補を保持できる |
| 頂点数、辺数、辺の順序と両端点が同じ | 隣接関係を読むための内部配列を再利用できる |
| 上記を保って費用・長さ・有効状態・役割を変更 | 隣接用配列を再利用し、新しい条件で再評価する |
| 頂点数または領域数が変わる | 旧候補は保持しない。`solve`、または新しい完全な配列を使う `improve`・`repair` から始める |

`update` は問題全体を受け取り、ほかの索引や作業領域を準備し直します。変更した辺数だけに比例する差分更新APIではありません。可動集合を狭くしたまま外側に新しい違反ができた場合は、修復もその集合の外を勝手に変更しません。必要な頂点まで可動範囲を広げます。

### ステップ30：複数の開始点を比較して、良い解を引き継ぐ

ステップ16の `solve` を、乱数の初期値を変えて3回行います。複数の独立した探索の中で最良解を保存し、最後にその解から追加で改善します。

**ファイル：`examples/30_multistart.cpp`**

```cpp
#include "connected_partition_solver_v08.hpp"
#include <iostream>
#include <vector>

int main() {
    using Solver = ConnectedPartitionSolver<double>;
    Solver::Problem p(6, 2);
    p.edges = {{0, 1, 1}, {1, 2, 8}, {2, 3, 10},
               {3, 4, 8}, {4, 5, 1}};

    for (auto& rule : p.regions) rule.min_vertices = 2;
    Solver solver(p);

    Solver::Options opt;
    opt.budget_us = -1;
    opt.max_steps = 2'000;

    std::optional<double> best_cost;
    std::vector<int> best;
    for (uint64_t seed : {1ULL, 2ULL, 3ULL}) {
        opt.seed = seed;
        auto trial = solver.solve(opt);
        if (trial.best_cost && (!best_cost || *trial.best_cost < *best_cost)) {
            best_cost = trial.best_cost;
            auto view = solver.best_labels();
            best.assign(view.begin(), view.end());
        }
    }
    if (!best_cost) return 1;
    auto result = solver.improve(best, opt);

    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) std::cout << r << ' ';
    std::cout << '\n';
}
```

`seed` に1、2、3を使い、それぞれ `max_steps=2'000` の新しい探索を行います。これらの数値は例示であり、選び抜いた乱数値ではありません。各 `solve` は以前の最良解を自動で混ぜないので、全実行を通した最良値 `best_cost` と配列 `best` を利用者側で保持します。

`1ULL` などの `ULL` は、符号なしの整数リテラルを表すC++の接尾辞です。ここでは64ビットの `seed` へ渡す正の値として使っています。

`trial.best_cost && (...)` で、合法解が見つかり、かつ外側の最良値を改善した場合だけ保存します。`best.assign(view.begin(),view.end())` で配列の実体をコピーするため、次の `solve` で内部状態が変わっても失われません。

最後は `improve(best,opt)` により、全実行の最良解を初期解として、さらに最大2,000試行改善します。`resume` なら最後の `solve` の内部状態から続くので、外で選んだ最良解を戻したいこの場面とは使い方が異なります。

この例は総試行数が増えます。方法を比較するときは、開始点を増やす側と1回を長くする側で、同じ総時間予算を使います。目的値だけでなく、構築・再評価・コピーの時間も含めて比較します。

## 9. APIを使う前に確認すること

ここまでの例で、公開されている操作と設定を一通り扱いました。実問題へ導入するときの参照用にまとめます。

| 操作 | 出発点 | 主な説明 |
|---|---|---|
| `Solver(problem)` | 問題データ | ステップ01 |
| `evaluate(labels, model)` | 完全な割当。合法でなくてもよい | ステップ02、22 |
| `improve(labels, options, model)` | 合法な初期解 | ステップ01、18 |
| `solve(options, model)` | 初期解なし | ステップ16 |
| `repair(labels, options, model)` | 完全だが違反し得る割当 | ステップ17 |
| `repair(options, model)` | 保持中の候補 | ステップ29 |
| `resume(options, model)` | 保持中の探索状態 | ステップ20、28 |
| `update(problem)` | 更新後の問題全体 | ステップ29 |
| `best_labels()` | 最後に確認済みの最良解 | ステップ01、02 |

表の `model` は省略可能です。探索APIの `options` も省略すると既定設定になります。モデルを使う場合は、`evaluate` は第2引数、探索APIは設定に続く引数へ渡します。

| 設定・型 | 説明したステップ |
|---|---|
| `Problem.n`, `k`, `regions`, `edges`; `Edge.u`, `v`, `cut_cost` | 01 |
| `Evaluation`, `Result`; `best_cost`, `cost`, `feasible` | 01〜02 |
| `RegionRule.min_vertices`, `max_vertices` | 03 |
| `unary_cost` | 04 |
| `load_dim`, `load`, `load_bounds`, `IntRange.lower`, `upper` | 05〜06 |
| `balance_terms`, `BalanceTerm` の4項目 | 07 |
| `fixed` | 08 |
| `domains`, `LabelDomain.vertex`, `labels` | 09 |
| `Edge.length`, `enabled`, `roles`; `Connect`, `Contact`, `PairCost` | 11 |
| `contacts`, `ContactRule` の4項目、`allow_unlisted_contacts` | 12 |
| `RegionRule.required`, `min_active_regions`, `max_active_regions`, `activation_cost` | 13 |
| `RegionRule.connected` | 14〜15 |
| `Options.acceptance`, `weights`, `temperature`, `final_temperature_ratio`; `Neighborhood` | 18 |
| `Options.budget_us`, `max_steps`, `deadline`, `profile`; `Clock`, `StopReason`, 全統計項目 | 02、19 |
| `Options.seed`, `movable` | 02、20、30 |
| `Cost` の型 | 01、21 |
| `vertex_cost`, `edge_cost`, `region_cost`, `global_cost` | 22〜25 |
| `feature_dim`, `feature`, `RegionStatsView` の全項目 | 24 |
| `SummaryView` の全メソッド、`ContactStats` の2項目 | 25 |
| `extra_feasible`, モデルの `load` | 26〜27 |

入力の前提も確認します。

- `n>=0`、`k>0`。頂点・領域・次元・辺の添字は対応する範囲内にします。通常は `n>0` を使いますが、空の合法解も表現できるため、結果のspanが空かどうかを「解なし」の判定にしません。
- 配列は省略可能なものだけ空にできます。`regions` は常に `k` 個、`fixed` を指定するなら `n` 個、`unary_cost` は `n*k` 個、`activation_cost` は `k` 個です。負荷・特徴は頂点数×次元数です。
- `load`・`feature` を省略した場合、その合計値は0です。`load_dim`・`feature_dim` を増やしただけでは値は入りません。
- 個数・負荷・接触の下限は上限以下、辺の長さは非負にします。`double` の入力や目的値は有限値で扱います。NaNや無限大で禁止費用を表現する使い方はしません。
- 不正な配列サイズやIDは `assert` で確認する入力前提です。明示的な `throw` による入力検査APIはありません。通常の「合法解未発見」は `best_cost` に値がないことで扱います。
- 同じsolverインスタンスは単一スレッドで使用します。探索を並行実行するための共有状態の同期APIはありません。

## 付録A. 実装アルゴリズムの全体像

ここからは、使い方ではなく実装の仕組みを説明します。対象はv08です。パラメータや内部処理は、将来の版で変更される可能性があります。

### A.1 入口から結果まで

処理は大きく5段階です。

1. **問題の索引を作る**：各頂点から隣接辺を素早く読める形にし、許可集合や接触ペアを検索できるようにします。
2. **出発する割当を用意する**：`improve`・`repair` は利用者の配列、`solve` は内部構築、`resume` は保持状態を使います。
3. **必要なら合法性を回復する**：`repair`・`solve` では、違反を減らす探索を行います。`improve`・`resume` は非合法な出発点を自動修復しません。
4. **合法な候補で目的値を改善する**：変更を提案し、制約・費用を調べ、採用するか戻すか決めます。現在解と別に最良解を保持します。
5. **結果を返す**：停止理由、最良目的値、統計を `Result` へ返します。ラベル配列はsolver内に残し、`best_labels()` から参照します。

`evaluate` は出発点の全体評価だけを行い、改善探索へは入りません。標準的な実行中は、毎候補について全グラフを作り直さず、影響する部分の集計を更新します。

### A.2 主な内部データ

内部の型や作業関数はsolverクラス内の非公開部分に置かれています。利用者がそれらを操作する必要はありません。

| データ | 目的 |
|---|---|
| ラベル配列 | 各頂点の現在の所属を読む |
| 領域ごとの頂点数・負荷・特徴・境界長 | 領域条件と目的値を素早く計算する |
| 領域ごとの所属頂点リストと、各頂点のリスト内位置 | 領域から頂点を選ぶ、所属を更新する |
| 登録ペアごとの接触数・接触長 | 接触制約と全体評価に使う |
| 現在解と最良解 | 焼きなましの一時的な悪化と返却用の解を分ける |
| 旧値の保存領域 | 候補を棄却したときに集計を正確に戻す |
| 可動範囲と固定条件から作る頂点リスト | 変更を開始できる頂点を選ぶ |

現在解の所属リストから1頂点を消すときは、末尾の頂点をその場所へ移し、末尾を削除します。順序が重要でないリストなので、途中以降の全要素をずらす必要がありません。

## 付録B. 索引構築と全体評価

### B.1 隣接辺を連続配列へまとめる

各辺を両端から読めるように、辺番号と相手頂点の組を2個作ります。頂点ごとの開始位置を別配列に保存し、隣接情報自体を1本の連続した配列へ詰めます。これはCSRと呼ばれる格納方法です。

辺が無効になっても、この配列から要素を削除せず、役割の判定で読み飛ばします。したがって、辺の並びと端点が変わらなければ、費用や有効状態の更新時に隣接配列を作り直す必要がありません。無効な辺の役割は、solverが所有するコピー内で0へ正規化します。更新は新しい入力から正規化し直します。

許可ラベル集合は並べ替えて重複を除き、二分探索します。接触ペアは小さい領域番号を先にそろえて検索用のキーにします。検索用の順序と、APIから見える `contacts` の登録順は区別して管理します。均衡項も領域ごとに参照できる索引を作ります。

### B.2 完全な割当を一度評価する

初期解の評価や `evaluate` では、次の順に全体を組み立てます。

1. ラベルの長さと範囲を確認します。
2. 全頂点を読み、所属数、負荷、特徴を領域ごとに足します。固定・許可集合への違反も数えます。
3. 必要な接触辺を読み、境界長、接触数・長さ、禁止された未登録ペアの接触を集計します。
4. 領域ごとの個数・負荷、使用領域数、接触範囲を確認します。
5. 同じ領域内の接続辺を使って連結成分数を調べます。
6. 組込み条件を満たせば `extra_feasible` を確認します。
7. 合法なら、頂点費用・辺費用・領域費用・全体費用を加えます。

連結成分とは、同じ領域の中で互いに行き来できる頂点のまとまりです。1つなら連結、2つ以上なら分断しています。実装では、集合を統合するUnion-Findを使います。各頂点を最初は別集合にし、同じラベルの接続辺を見つけるたびに集合を統合して、残ったまとまりを数えます。小さい集合を大きい集合へ付け、検索経路も短くすることで負担を抑えます。

すべての領域で `connected=false` なら、この連結性用の処理を省略します。また、標準モデルで接触制約もなければ、参照されない境界・接触の集計を省きます。後で独自モデルに切り替えるときは、必要な集計を作り直します。

`evaluate` だけの場合、探索で使う所属リストと逆引き位置は作りません。評価のための作業状態は局所的なもので、保持している探索状態を置き換えません。

## 付録C. 初期解の構築と修復

### C.1 初期領域を成長させる

`solve` は、まず固定頂点を指定ラベルへ置きます。次に、必須領域と使用領域数の下限を満たすため、未割当頂点から領域の出発点を選びます。許可集合はこの段階でも確認します。

各領域の隣にある未割当頂点を候補とし、領域を少しずつ成長させます。頂点数の目安に対して小さい領域を優先し、最大頂点数を越えて成長させないようにします。負荷が符号付きの場合もあるため、「頂点を増やせば負荷も増える」という仮定には頼りません。

接続辺をたどって割り当てられなかった頂点も、最後には許可先を探して完全な配列にします。この構築だけで、連結性・負荷・接触などの全制約が必ず満たされるわけではありません。完成後に全体評価し、違反があれば修復へ進みます。

合法解を確保でき、まだ予算があれば、もう1つの構築候補も試します。この追加構築では、各成長先で最大12個のランダムな候補を比べ、既定の配置費用・辺費用を手掛かりにします。独自関数で置換された費用は、この部分割当の段階では呼びません。最終的な比較には、完成した候補の全制約と独自モデルを含む目的値を使います。

追加構築が制限時刻を越えて完成した場合、その候補は採用しません。ただし、構築処理そのものを常に即時中断できるという意味ではありません。

### C.2 修復で使う「違反量」

修復中は、目的値の良さより先に、合法な割当を得ることを目指します。そのため、固定・許可違反、頂点数の不足や超過、負荷範囲からのずれ、接触違反、余分な連結成分などをまとめた内部の違反量を使います。

負荷範囲の判定自体は整数で行います。その後、修復の案内に使うずれの大きさだけを実数へ変換し、負荷の単位が大きいだけでその違反ばかりが支配しないよう、領域の規模を使って調整します。追加条件だけが違反する場合の違反量は1です。独自の追加条件の「あとどれくらいで満たせるか」までは分かりません。

最初に、修復対象範囲内の固定・許可違反を、許可されるラベルへ直します。その後は候補を作るたびに完全な割当を全体評価します。合法でない状態では連結構造も壊れているため、改善探索の部分更新と同じ前提は使いません。

修復では、Move・Swap・Block・Recombineを内部の固定比率6:2:1:1で選びます。`Options.weights` は合法解の改善探索用で、この修復比率は変更しません。違反量が減る候補・同じ候補を採用し、ときには違反量の増える候補も採用します。この採用方針も修復用で、`Acceptance::greedy` を指定していても修復が常に単調に違反を減らすわけではありません。

違反量の最小だった候補を保存し、進展が256試行停滞するとそこへ戻します。`solve` の修復では、新しく構築し直す選択を行います。合法解を得たら、その時点から通常の目的値による改善へ切り替わります。

## 付録D. 5種類の近傍

### D.1 移動先の選び方

まず、変更可能で固定されていない頂点から1つ選びます。移動先は、主として接続辺の先にある別領域から選びます。隣へ動かす方が、連結条件を満たす候補になりやすいためです。

一定割合では全ラベルからランダムに選ぶので、空領域を新しく使う候補や、連結性を要求しない領域への移動も残ります。隣接先を探す場合も、隣接配列の読み始めをランダムにずらします。

### D.2 Move：1頂点を移す

選んだ頂点を別の領域へ移します。変化が小さく、費用差分も小さな範囲で計算できます。一方、ぴったりの個数・負荷を要求する問題では、1頂点だけ動かすと違反になりやすくなります。

### D.3 Swap：2頂点を交換する

2領域から1頂点ずつ選び、所属を入れ替えます。両領域の頂点数が変わらないのが利点です。負荷や連結性が保たれるとは限らないため、それらは通常どおり検査します。交換相手は必ずしも隣接頂点ではありません。

### D.4 Block：小さなまとまりを移す

同じ領域の変更可能な頂点を、接続辺に沿って広げ、小さな連結ブロックを作ります。通常は2〜8頂点程度を目安にします。空領域へ移す場合は、その領域の最小頂点数も考慮して大きさを増やします。

そのブロックを別領域へ移すか、相手領域にもブロックを作って交換します。ブロック自身が連結でも、抜かれた側や受け取る側が全体として連結になるとは限らないため、最終検査が必要です。

### D.5 Relabel：領域全体の番号を変える

領域全体のラベルを別番号へ変えます。実装は、2領域の番号交換を多く選び、ときには片方へ併合します。

ラベルごとの配置費用・容量・固定条件が異なる問題では、形が同じでも番号の交換に意味があります。併合すると空領域が生じる場合もあるため、任意領域と使用数条件が関係します。2領域の連結性ルールが異なる場合もあるので、「番号を変えただけなら必ず合法」とは扱いません。

### D.6 Recombine：2領域をまとめて分け直す

2領域の頂点を合わせ、その内部の接続辺を深さ優先でたどり、すべての頂点を結ぶ**木**を作ります。木とは、頂点どうしがつながり、輪がないグラフです。まとめた範囲がそもそも連結でない場合、この方法では候補を作りません。

木の辺を1本切ると、2つの連結した部分に分かれます。そこで、切る位置と、どちら側へどのラベルを付けるかを調べます。頂点数と負荷の範囲に合う候補から、ランダムに1つ選びます。

深さ優先探索では、ある頂点以下の部分木が訪問順の連続区間になります。区間の負荷を素早く求めるため、先頭からの累積和を用意します。割当先に依存する負荷にも対応するため、2つのラベルについてそれぞれ累積和を作ります。符号付き負荷もそのまま区間の和で扱います。

これは、2領域の全分割を列挙して最良を選ぶ厳密法ではありません。1つのランダムな木から作れる切り方に限った候補です。木上の両側が連結でも、固定・許可集合・接触・追加条件などはまだ残るため、共通の検査へ渡します。

## 付録E. 候補の差分評価と連結性

### E.1 必要な部分だけを更新する

候補が「頂点2を領域0から1へ移す」なら、変わるのは、その頂点の配置費用、頂点2に接する辺の費用、領域0と1の集計、関係する接触などです。関係しない頂点や辺を再計算する必要はありません。

複数頂点を同時に変える場合も同じ考え方です。影響する頂点・辺・領域・接触ペアを集め、辺が複数の変更頂点に接していても1回だけ処理します。毎回大きな訪問フラグ配列を0に戻す代わりに、試行の世代番号を記録して重複を判定します。

変更前の集計値を保存してから、候補のラベルを一度に適用します。その後に制約を検査するため、Swapの片側だけ適用した中間状態を完成候補と誤認しません。

標準の貪欲探索では、安い個数・負荷・接触などの検査を済ませたあと、費用が悪化するなら連結性の検査前に棄却できます。独自モデルの費用は、連結性と追加条件にも通った場合だけ呼びます。

領域費用の差分は、変更後の領域集計による値から、保存した変更前の集計による値を引いて求めます。そのため `region_cost` は渡された集計に従って計算する必要があります。現在の全体状態を別の場所から読んでしまうと、変更前の評価が正しく計算できません。

採用しなければ、ラベルと保存した集計値を戻します。実数について「足した量を引けば元どおり」とすると丸め誤差が残る場合があるため、逆演算ではなく保存値で復元します。ただし、採用した変更の差分集計には丸め誤差があり、最後に自動で全体再評価する処理はありません。必要な照合には `evaluate` を使います。

### E.2 連結性の検査を絞る

全領域を調べるのではなく、変更された領域を調べます。連結性を要求しない領域、頂点数が0または1の領域では、連結探索を省けます。

1頂点を追加しただけなら、追加先はもともと連結です。追加頂点から、その領域の頂点へ接続辺が1本あれば全体も連結になります。

1頂点を除いた場合、残ったすべての頂点を最初から数える代わりに、「除いた頂点の、同じ領域に属していた隣接頂点どうしが、残った領域内でつながるか」を調べます。もとの領域が連結なので、この確認で十分です。対象の隣接頂点をすべて発見できた時点で終了できます。

より大きな変更では、候補の領域内を幅優先探索して全頂点へ届くか調べます。可動範囲の外側も同じラベルなら経路として使います。この検査は大きな領域で重くなり得るので、1候補の費用が常に一定時間とは限りません。

### E.3 最良解の保存

採用した候補が最良値以下なら、返却用の最良解も更新します。同じ値の別解へ更新することもあります。最良値更新回数の統計は、値が厳密に小さくなった場合だけ増やします。

現在解と最良解が同じ配列だと分かっている場合は、変更した頂点だけ最良解へ反映できます。焼きなましで現在解が最良解から離れている場合は、必要になった時点で全ラベルをコピーします。

## 付録F. 焼きなまし・乱数・停止

### F.1 悪化をどれくらい許すか

現在の目的値を $C$、候補の目的値を $C'$ とし、悪化量を $\Delta=C'-C$ とします。$\Delta\le0$ なら採用します。焼きなましで $\Delta>0$ の場合は、次の確率で採用します。

$$p=\exp(-\Delta/T)$$

$p$ は採用確率、$T>0$ はその時点の温度です。`exp` は指数関数で、$\exp(z)=e^z$、$e$ は約2.71828という定数です。ここでの $e$ は第1節の辺の添字とは別の意味です。

温度が大きいと悪化を許しやすく、小さいと許しにくくなります。たとえば悪化量2に対して、温度2なら約0.368、温度0.2なら約0.000045の確率です。これは最適解発見の確率ではなく、1個の悪化候補を採用する確率です。

自動温度では、正の費用差を変更頂点数で割った値を観測し、前の推定値を98%、新しい観測を2%の重みで混ぜます。最初の観測はその値を使います。初期温度を正値で指定すれば、その値を基準にします。

進行度を $q$、終了温度の割合を $\rho$、温度の基準値を $T_ {0}$ とすると、冷却は次の形です。

$$T=T_ {0}\rho^{q},\qquad 0\le q\le1,\quad 0<\rho\le1$$

$q$ は時間予算と試行数予算のうち、より上限へ近づいている方の割合です。$\rho$ が `final_temperature_ratio` です。$T_ {0}$ は明示温度なら固定値、自動温度なら探索中にも更新される推定値です。そのため自動温度は、厳密に単調減少するとは限りません。

### F.2 乱数と再現性

実装は、内部に状態を持つSplitMix64系の64ビット乱数生成を使います。新規探索では `seed` から始め、`resume` ではその状態を引き継ぎます。

試行数を固定し、入力と実装もそろえると比較しやすくなります。時間で止める場合は、CPUの混雑や計測費用で到達する試行数が変わり、同じseedでも最終配列が変わることがあります。

### F.3 終了判定の位置

改善探索は、提案、候補検査、コールバックの前後などで時刻を確認します。大きな連結探索や木の探索でも、一定間隔で期限を確認します。コールバックの内部を分割して中断する仕組みはありません。

初期評価や一部の構築・再準備はまとまった処理なので、非常に小さい時間予算ではそれだけで超過することがあります。`elapsed_us` には呼出し内のその費用も含みます。小さな時間指定が有用かどうかは、入力サイズとモデルの費用を含めて測定します。

## 付録G. 性能を考えるときの見方

初期評価は概ね頂点・辺・領域集計を全体にわたって読みます。改善候補では、変更頂点、その隣接辺、変更領域の集計に主な費用が掛かり、必要な連結探索や独自モデルの費用が加わります。修復は全体再評価を繰り返すため、小さな合法変更の改善より重くなりやすい処理です。

メモリには頂点・辺の情報だけでなく、領域数×負荷次元数、領域数×特徴次元数、登録接触ペア、許可集合、各種の作業配列が必要です。`unary_cost` を使うなら頂点数×領域数の入力配列も必要です。どの設定でも単純に「頂点数と辺数にしか依存しない」とは限りません。

性能調整では、入力種別ごとに同じ総時間で解品質を比べ、複数の入力とseedを使います。計時を有効にした測定と、通常設定の最終測定を分けます。大きな近傍を増やせば試行数は減りますが、1回の改善幅は増える可能性があります。試行数だけを性能指標にしないようにします。

特徴を足し算で管理して領域評価を軽くする、合法な初期解を利用する、問題を変えないターンでは `resume` を使う、といった利用側の表現も費用に影響します。`update` の内部再利用は有用ですが、全更新が一定時間になるものではありません。

v08のホットパスは通常の整数・`double` を使い、`long double`・`__int128` を既定利用していません。IDと要素数は `int`、整数の合計・差・中間積は `int64_t` に収まる入力を前提にします。内部の世代番号の累積増分も、それぞれ $2^{32}$ 未満を前提とします。これらの制限を越える極端な実行を、この小さなライブラリ内で特別扱いする設計ではありません。

## 付録H. 参照資料と検証情報

類似ソルバーの節で参照した資料です。APIと内部アルゴリズムの説明の一次資料は、付属のv08ヘッダそのものです。

1. [AtCoder Library — MinCostFlow](https://atcoder.github.io/ac-library/production/document_ja/mincostflow.html)。最小費用流のAPIと入力制約。下限付き容量やモデル変換は利用者側の作業です。
2. [OR-Tools — Assignment as a Minimum Cost Flow Problem](https://developers.google.com/optimization/flow/assignment_min_cost_flow)。割当問題の特殊ケースと最小費用流との対応。
3. [AtCoder Library — MaxFlow](https://atcoder.github.io/ac-library/production/document_ja/maxflow.html)。最大フローと `min_cut` のAPI。
4. [Boykov, Veksler, Zabih — Fast Approximate Energy Minimization via Graph Cuts, ICCV 1999](https://www.cs.cornell.edu/rdz/Papers/BVZ-iccv99.pdf)。2値の特殊ケース、多ラベルの近似法と相互作用の条件。
5. [OR-Tools — CP-SAT Solver](https://developers.google.com/optimization/cp/cp_solver)。整数によるモデル化と `OPTIMAL`・`FEASIBLE` などの状態の意味。
6. [KarypisLab — METIS](https://github.com/KarypisLab/METIS)。グラフ分割の公式実装と対象用途。

検証環境は `g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0` です。全30例をC++20・C++23の両方でコンパイル・実行し、計60実行が成功しました。`-O2 -Wall -Wextra -pedantic` を使い、警告はありませんでした。各例の出力について、別実装のPythonで連結性・主要な制約・目的値を照合しています。例内の `assert` では再評価、可動範囲、更新後の固定条件なども確認しています。これは教材の実行検証であり、未知の入力に対する解品質保証や厳密な時間保証ではありません。

付属ZIPの `validation/examples-both.json` に各例の実際の出力があります。時間依存の値や、同点の解のラベル配列は実行ごとに変わる場合があります。GCC15.2など、上記以外のコンパイラでは今回の実行検証を行っていません。

本文のC++コードは付属の `.cpp` と同じ内容です。数式は `$...$` または `$$...$$` に対応するMarkdown表示器を想定します。表示数式のソースは各1行とし、数式内の下付き添字がMarkdownの強調へ変わらない形を使用しています。数式対応のCommonMark処理で、数式の内容・コードブロック・見出しを確認し、全97箇所の数式をKaTeXでエラーなく処理できることを確認しています。数式非対応の表示器ではLaTeX記法のまま表示されます。

対象ヘッダのSHA-256：`9f909389b841a7ffbd8fdea1b22b72254d1b1127fa8958a23a93d93f0ea3f8cd`。

再検証はZIPのルートから次のコマンドで行えます。Python 3とGCCが必要です。コンパイルが30例×2回あるため、例を1個実行するより時間が掛かります。

```sh
python3 verify_examples.py --std both --jobs 3
```

