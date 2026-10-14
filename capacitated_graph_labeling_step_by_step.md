# 小さな実行例で学ぶ、容量制約付きグラフラベリング

ステップバイステップガイド v02 — 対象：`capacitated_graph_labeling_v25.hpp`  
2026-10-14／高校数学とC++の基本文法から読み進めるための教材

## この教材の読み方

「仕事を担当者に割り当てる」という例から始め、条件を少しずつ増やします。数学は足し算、大小比較、関数を中心に説明します。グラフ理論や最適化の予備知識は要りません。C++では、変数、`if`、`for`、配列に触れたことがあると読みやすくなります。

各ステップのC++コードは、`main`を含む独立したプログラムです。前のステップのコードを継ぎ足さなくても実行できます。最初にライブラリのヘッダを同じフォルダへ置いてください。「変更点」は学習上の差分を示し、掲載コード自体は省略のない完全版です。コードの前に目的を読み、実行してから、その後の説明で新しい部分を確認します。

| 範囲 | 学ぶこと |
|---|---|
| 1〜2章 | 解く問題、目的関数、他の解法を選ぶ条件 |
| ステップ1〜6 | 最小の割当から、関係・工数・件数・許可・固定へ |
| ステップ7〜11 | 解の検査、実行設定、既存解・一部分の改善、入力の更新 |
| ステップ12〜16 | 行列、自作の費用、小数、負の費用、一対一配置 |
| ステップ17〜21 | 時間管理、複数試行、調整項目、失敗と境界条件、デバッグ出力 |
| API早見表 | 全公開型・関数・フィールドの振り返り |
| 最後の付録 | 実装アルゴリズムの概要と詳細 |

## 1. 何を解くライブラリか？

### 1.1 「一つずつ割り当てる」問題

4つの仕事を2人へ割り当てるとします。仕事を分割することはできず、各仕事を必ずどちらか1人へ任せます。

- 仕事の番号を**頂点**と呼びます。例では0、1、2、3です。
- 担当先の番号を**ラベル**と呼びます。例では担当者0、担当者1です。
- 「仕事0と仕事2は連携が必要」という関係を**辺**と呼びます。
- 全仕事の担当先を並べた配列が**解**です。例えば`{0, 0, 1, 1}`です。

頂点と辺を合わせたものがグラフです。グラフは図を描かなければ使えないものではなく、コードでは仕事の個数と関係のリストとして入力します。

安く済ませたいだけでなく、担当者ごとの仕事量にも限界があります。「各担当者の工数は4以下」「担当件数は2件ずつ」「仕事0は担当者0に固定」といった条件を満たす割当を探します。

条件をすべて満たす解を**実行可能解**と呼びます。その中で合計費用が最も小さい解が**最適解**です。このライブラリは良い実行可能解を探しますが、最適であることを証明するものではありません。解が見つからなくても、解が存在しないと分かったわけではありません。

### 1.2 目的関数を読む

小さくしたい値を表す式を**目的関数**と呼びます。本ライブラリの目的関数は次です。

$$\min_ {\ell} C(\ell), \qquad C(\ell)= \sum_ {v=0}^{N-1} U(v,\ell_ {v}) +\sum_ {e=0}^{M-1} P_ {e}(\ell_ {u_ {e}},\ell_ {v_ {e}}).$$

式に現れる記号を、一つずつ読みます。

| 記号 | 意味 |
|---|---|
| $N$ | 頂点数。仕事の個数 |
| $K$ | ラベル数。担当先の個数。1以上。式の各ラベルが取れる範囲を決める |
| $M$ | 辺数。登録した関係の個数 |
| $v$ | 仕事の番号。$0,1,\ldots,N-1$ |
| $e$ | 関係の番号。$0,1,\ldots,M-1$。辺配列の順番 |
| $u_ {e},v_ {e}$ | 関係$e$の第1・第2端点。どちらも仕事の番号 |
| $\ell_ {v}$ | 仕事$v$の担当先。$0,1,\ldots,K-1$のどれか |
| $\ell$ | 全仕事の担当先を並べた列$(\ell_ {0},\ldots,\ell_ {N-1})$ |
| $U(v,a)$ | 仕事$v$だけを担当先$a$へ置く費用。**単項コスト** |
| $P_ {e}(a,b)$ | 関係$e$の第1端点を$a$、第2端点を$b$へ置いたときの費用。**二項コスト** |
| $a,b$ | 担当先を表す仮の引数。$0$以上$K-1$以下 |
| $C(\ell)$ | 割当$\ell$の合計費用。目的値とも呼ぶ |
| $\sum$ | 指定した範囲について全部足す記号 |
| $\min_ {\ell}$ | 条件を満たす割当$\ell$の中で、右の値を最小にしたいという意味 |

第1の和は「個々の仕事を誰に任せるか」の費用、第2の和は「関係する仕事をどう組み合わせて任せるか」の費用です。

例えば解が`{0, 0, 1, 1}`で、担当費用が順に0、1、0、1なら、第1の和は2です。辺が仕事0と2、仕事1と3の2本で、別担当に分かれるたび5の引継ぎ費用がかかるなら、第2の和は10です。合計は12になります。辺は登録した1本につき1回数えます。

### 1.3 容量と件数は別の条件

仕事$v$の工数を$d_ {v}$とします。同じ仕事の工数は、どの担当者へ置いても同じです。担当先$a$の工数合計$W_ {a}$と担当件数$n_ {a}$は次です。

$$W_ {a}=\sum_ {v=0}^{N-1}d_ {v}[\ell_ {v}=a], \qquad n_ {a}=\sum_ {v=0}^{N-1}[\ell_ {v}=a].$$

角括弧$[\ell_ {v}=a]$は、条件が成り立てば1、そうでなければ0です。担当先$a$の仕事だけが和に残ります。

守るべき上下限を次のように書けます。

$$L_ {a}\le W_ {a}\le R_ {a},\qquad A_ {a}\le n_ {a}\le B_ {a}.$$

| 記号 | 意味 |
|---|---|
| $d_ {v}$ | 仕事$v$の需要量。ここでは工数。0以上の整数 |
| $W_ {a}$ | 担当先$a$の総工数 |
| $n_ {a}$ | 担当先$a$の件数 |
| $L_ {a},R_ {a}$ | 工数合計の下限・上限 |
| $A_ {a},B_ {a}$ | 件数の下限・上限 |

工数0の仕事も1件です。すべての工数を1にすると、工数と件数は同じ量になります。件数や容量の下限を0にすると、空の担当先があっても構いません。

さらに、仕事ごとに許可された担当先を制限したり、担当先を固定したりできます。本APIでは**固定先が許可表より優先**します。両方の条件の共通部分を使う仕様ではないので、後のステップ6で具体的に確認します。

### 1.4 希望と必須条件を区別する

「同じ担当にしてほしい」は費用で表せます。「必ず同じ担当」は、単に大きな費用を設定しただけでは本ライブラリの実行可能性条件にはなりません。

直接指定できる必須条件は、1頂点1ラベル、需要量・件数の上下限、固定・許可ラベルです。グループの連結性、仕事間の厳密な先行順序、CPUとメモリのような複数の重み付き資源、3仕事以上を一度に見た条件は、そのまま指定するAPIがありません。有限の罰金を払ってでも希望を破る解は、これらの必須条件を満たしていれば`feasible=true`になります。

この章で扱ったのは問題の意味です。どうやって解を探すかは、最後の付録で説明します。

## 2. 類似ソルバー：このライブラリを使う前に

### 2.1 条件が合えば厳密解を検討する

**厳密解法**は、計算が完了すれば最適解を求められる方法です。本ライブラリより良い値が得られることも、同じ最適値になることもあります。いつでも高速とは限らないので、まず問題の条件を確認します。ここではアルゴリズムそのものの説明はしません。

| 問題の条件 | 候補 | 注意 |
|---|---|---|
| 二項コストも全体容量・件数制約もない | 仕事ごとに最も安い許可先を選ぶ | 仕事間で選択が独立していることが必要 |
| 二項コストなし、全需要量が1、担当先ごとの件数上下限 | 下限付き最小費用流 | 需要量と件数の上下限を統合できる。固定・許可先も反映する |
| 二項コストなし、各位置に1個ずつ配置 | 線形割当 | 二項費用が加わった一般の一対一配置とは別問題 |
| 2ラベル、非負重みのPotts費用、全体容量・件数制約なし | $s$-$t$最小カット | 固定・許可先は扱えるが、均等人数などを足すと同じ方法では解けない |
| 2ラベル、次の不等式を満たす一般二項費用、全体容量・件数制約なし | $s$-$t$最小カット | 端点順をそろえて費用表を確認する |
| 順序付きラベル、非負重み付きの凸なラベル差費用、全体容量・件数制約なし | 対応するグラフカットの厳密解法 | 例えば絶対差や差の2乗。任意の行列ではない |
| 相互作用が木・森で、全体容量・件数制約なし | 木の動的計画法 | 容量を足すと仕事どうしが結び付き、同じ小さな状態では足りない |
| 可変頂点がごく少ない | 全探索 | 最大$K^F$通り。$F$は動かしてよい頂点数 |

一般の2ラベル費用についての十分条件は、同じ頂点対の費用を同じ端点順へ合算した表$P$が

$$P(0,0)+P(1,1)\le P(0,1)+P(1,0)$$

を満たすことです。これは「同じラベルにそろえる費用の合計が、分ける費用の合計より大きくない」という条件です。**劣モジュラ**という名称を知らなくても、この4つの数値の比較として読めます。[Kolmogorov・Zabihの原論文](https://www.cs.cornell.edu/~rdz/Papers/KZ-PAMI04.pdf)が適用条件を扱っています。順序付きラベルの凸費用については[石川の原論文の著者所属機関による紹介](https://waseda.elsevierpure.com/en/publications/exact-optimization-for-markov-random-fields-with-convex-priors/)を参照できます。

異なる工数を持つ仕事を、工数ぶんの流量として流すだけでは、1仕事が複数担当へ分かれることがあります。それはこの教材の「仕事を丸ごと一つの担当へ」という条件に一致しません。[OR-Toolsの最小費用流による割当](https://developers.google.com/optimization/flow/assignment_min_cost_flow)は、帰着が成立する場合の比較対象です。

### 2.2 より一般的なソルバーとの使い分け

| 候補 | 向いている検討 | このヘッダと比較するときのポイント |
|---|---|---|
| [OR-Tools CP-SAT](https://developers.google.com/optimization/cp/cp_solver) | 整数の費用と複雑な論理・割当制約を一緒に扱いたい | 別途モデル化が必要。最適性の証明を求められるが、制限時間内に証明が終わる保証はない |
| [SCIP](https://scipopt.org/) | 整数計画などへ定式化して解・下界・最適性の差を調べたい | 本ヘッダの構造体をそのまま渡す互換品ではない。変数と制約へ書き直す |
| [KaHIP](https://github.com/KaHIP/KaHIP) | 大規模なグラフ分割を専用ソルバーと比較したい | 使う分割モデルの条件を確認する。本ライブラリの任意の費用表・許可先・独立した件数上下限と同じ問題とは限らない |
| 本ライブラリ | 単項・二項費用に容量・件数・固定・許可を組み合わせ、C++単一ヘッダで繰り返し改善したい | 最適性の証明や下界は返さない。指定範囲だけを変更する使い方もできる |

CP-SATでは、実行可能解を得た`FEASIBLE`と、最適と証明された`OPTIMAL`を区別します。一般形の本問題も、頂点・ラベルの選択と辺のラベル組合せを変数にして整数計画へモデル化できますが、問題が大きくなります。小数を整数に直すときは、丸めで別の問題にしないことも必要です。

この使い分けは、上記の公式説明と本ヘッダのAPIを比較した判断です。これらのソルバーと速度・解品質を実測比較した結果ではありません。参照先は2026-10-12に確認しました。本ライブラリが条件を判定して他のソルバーへ自動的に切り替えることもありません。

## 3. 実行の準備

以下を同じフォルダに置きます。

- 配布ヘッダ`capacitated_graph_labeling_v25.hpp`
- 選んだステップのC++コードを保存した`main.cpp`

GNU拡張とlibstdc++が使えるGCC系のC++20環境を使います。本教材の確認環境はGCC 13.3です。ヘッダが`<bits/stdc++.h>`、128bit整数、`__INCLUDE_LEVEL__`などを使うため、標準C++20に対応しているだけのすべての環境で動くとは限りません。ACL、追加のハッシュマップ、Pythonは実行に不要です。

```bash
g++ -std=c++20 -O2 -Wall -Wextra main.cpp -o main
./main
```

`-std=c++20`は言語の版、`-O2`は最適化、`-Wall -Wextra`は警告、`-o main`は実行ファイル名です。各ステップは個別に実行します。複数の`main`を一つのファイルへ連結しないでください。

## ステップ1：最小コード — 4仕事を2人へ割り当てる

### 目的

各担当者は最大2件まで。仕事ごとの担当費用だけを考えます。工数はまだ指定せず、1仕事を1単位として扱います。

| 仕事 | 担当者0の費用 | 担当者1の費用 |
|---|---:|---:|
| 0 | 0 | 6 |
| 1 | 1 | 4 |
| 2 | 5 | 0 |
| 3 | 4 | 1 |

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.upper_capacity = {2, 2};
    p.unary_cost = {
        0, 6,
        1, 4,
        5, 0,
        4, 1
    };

    const auto r = solve_capacitated_graph_labeling(
        p, capacitated_potts_cost<long long>{});
    if (!r.feasible) {
        std::cerr << "No feasible solution found\n";
        return 1;
    }
    std::cout << "cost=" << r.objective << '\n';
    for (int v = 0; v < p.vertex_count; ++v) {
        std::cout << "job " << v << " -> person " << r.label[v] << '\n';
    }
}
```

### 順番に読む

1. `p`は問題を入れる入れ物です。型名の`<long long>`は、費用を符号付きの整数で扱う指定です。需要量・容量も整数ですが、こちらの型はライブラリで`long long`に決まっています。
2. `vertex_count=4`は仕事数、`label_count=2`は担当者数です。番号は0始まりなので、仕事3まで、担当者1までです。
3. `upper_capacity={2,2}`の第1要素が担当者0、第2要素が担当者1の上限です。`demand`を省略すると各仕事の需要量が1なので、ここでは「2件以下」を表します。4仕事を2人へすべて入れるため、結果的に2件ずつになります。
4. `unary_cost`は上の表を行ごとに並べた配列です。仕事$v$、担当先$a$の費用は`p.unary_cost[v * p.label_count + a]`です。例えば仕事2・担当者1は添字`2*2+1=5`で、値は0です。
5. `capacitated_potts_cost<long long>{}`は2仕事間の費用規則です。今回は辺がないため、目的値への寄与はありません。ただし`solve`の第2引数として規則を渡す必要があります。
6. `solve_capacitated_graph_labeling`が解を探し、`auto`で受けた`r`に結果を入れます。第1引数は問題、第2引数は二項コスト規則です。設定を省略したこの呼出しは、既定の相対時間1,000ms、seed 1を使います。
7. `r.feasible`を先に見ます。`false`の結果からラベルや目的値を使ってはいけません。エラー文の「見つからなかった」は「存在しない」とは違います。
8. 成功時の`r.objective`が合計費用、`r.label[v]`が仕事$v$の担当先です。`'\n'`は改行です。

この例で`{0,0,1,1}`なら費用は$0+1+0+1=2$です。各仕事で最も安い担当を選んだ費用の合計も2なので、この入力では最適と手で確認できます。ただし、この手計算による証明と、ライブラリが最適性を保証することは別です。

未指定の容量下限は0、固定・許可の制限はなく、辺もありません。このように不要な配列は空のままにできます。**すべてのパラメータを最初から埋める必要はありません。**

## ステップ2：関係のある仕事に引継ぎ費用を加える

ステップ1に`edges`だけを追加します。仕事0と2、仕事1と3は連携が必要で、別担当なら5の費用がかかるとします。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.upper_capacity = {2, 2};
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}}; // 追加

    const auto r = solve_capacitated_graph_labeling(
        p, capacitated_potts_cost<long long>{});
    if (!r.feasible) return 1;
    std::cout << "cost=" << r.objective << '\n';
    for (int a : r.label) std::cout << a << ' ';
    std::cout << '\n';
}
```

辺の`{0,2,5,0}`は、順に`{u,v,weight,type}`です。

| 要素 | この値の意味 |
|---|---|
| `u=0` | 第1端点は仕事0 |
| `v=2` | 第2端点は仕事2 |
| `weight=5` | 別担当へ分かれたときの費用5 |
| `type=0` | 辺種別。組込みPotts規則は使わないので0を入れる |

Potts規則は$P_ {e}(a,b)=w_ {e}[a\ne b]$です。$w_ {e}$は`weight`、$a,b$は辺の両端のラベルです。同担当なら0、別担当なら5になります。

以前の解`{0,0,1,1}`は、担当費用2＋引継ぎ10＝12です。`{0,1,0,1}`なら担当費用10＋引継ぎ0＝10になります。「各仕事を最も安い担当へ置く」だけでは全体費用が最小とは限らないことが分かります。

無向の関係は1本だけ登録します。逆向きも登録すると2本ぶん費用がかかります。`for (int a : r.label)`は、ラベル配列の各要素を順に表示するC++の書き方です。

## ステップ3：仕事の重さを変え、工数の下限も付ける

ステップ2から、`demand`と`lower_capacity`を追加し、`upper_capacity`を変更します。仕事0、2は工数2、仕事1、3は工数1。各担当者の工数合計を2〜4にします。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.demand = {2, 1, 2, 1};       // 追加：仕事ごとの工数
    p.lower_capacity = {2, 2};    // 追加：担当先ごとの下限
    p.upper_capacity = {4, 4};    // 変更：件数ではなく総工数
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}};

    const auto r = solve_capacitated_graph_labeling(
        p, capacitated_potts_cost<long long>{});
    if (!r.feasible) return 1;
    std::cout << "cost=" << r.objective << '\n';
    for (int v = 0; v < p.vertex_count; ++v)
        std::cout << "job " << v << " -> " << r.label[v] << '\n';
    for (int a = 0; a < p.label_count; ++a)
        std::cout << "load[" << a << "]=" << r.load[a] << '\n';
}
```

`demand`の添字は仕事番号、上下限配列の添字は担当先番号です。混同しないでください。工数の合計は6なので、例えば担当先ごとに4と2、または3と3なら条件を満たします。

`r.load[a]`は返された解の担当先$a$の総需要量です。費用でも担当件数でもありません。`lower_capacity`と`upper_capacity`はそれぞれ独立に省略できます。省略時は下限0、上限は全仕事の総需要量です。

上下限を同じ値にすると、ちょうどその値にする制約になります。ただし、工数は分割できないため、上下限の合計が合っていても実行可能解があるとは限りません。

## ステップ4：工数と件数を同時に管理する

ステップ3に件数の上下限を追加します。工数だけでは、小さな仕事を一人へ集中させることを防げません。ここでは各担当者2件にします。また仕事3の工数を0へ変え、工数0でも1件として数えることを確認します。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.demand = {2, 1, 2, 0};      // 変更：仕事3は工数0
    p.lower_capacity = {2, 2};
    p.upper_capacity = {4, 4};
    p.lower_count = {2, 2};       // 追加
    p.upper_count = {2, 2};       // 追加
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}};

    const auto r = solve_capacitated_graph_labeling(
        p, capacitated_potts_cost<long long>{});
    if (!r.feasible) return 1;
    for (int a = 0; a < p.label_count; ++a) {
        std::cout << "person=" << a
                  << " load=" << r.load[a]
                  << " count=" << r.count[a] << '\n';
    }
    std::cout << "cost=" << r.objective << '\n';
}
```

`lower_count[a]`が最低件数、`upper_count[a]`が最大件数です。どちらも`int`で、0〜仕事数の範囲です。上下限を2でそろえたので、両担当者が正確に2件になります。例えば`{0,0,1,1}`なら、工数は3と2、件数は2と2です。

`r.count`は件数制約を片方でも指定したときだけ入ります。両方を省略すると空配列なので、その場合に`r.count[a]`を読んではいけません。件数制約を追加せず結果の件数だけ欲しいなら、`p.upper_count.assign(p.label_count, p.vertex_count)`とすれば、最大N件という実質的に制限しない条件を明示できます。

同じ需要量と件数を混ぜて1個の数で管理する必要はありません。両方の配列をそのまま指定します。

## ステップ5：資格に合う担当先だけを許可する

ステップ4へ`allowed`を追加します。仕事1は資格の関係で担当者1しか扱えないとします。他の仕事は両方へ置けます。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.demand = {2, 1, 2, 0};
    p.lower_capacity = {2, 2};
    p.upper_capacity = {4, 4};
    p.lower_count = {2, 2};
    p.upper_count = {2, 2};
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}};
    p.allowed.assign(p.vertex_count * p.label_count, 1); // 全て許可
    p.allowed[1 * p.label_count + 0] = 0;               // 仕事1→担当0を禁止

    const auto r = solve_capacitated_graph_labeling(
        p, capacitated_potts_cost<long long>{});
    if (!r.feasible) return 1;
    for (int a : r.label) std::cout << a << ' ';
    std::cout << "\njob1=" << r.label[1] << '\n';
}
```

`allowed`は`std::vector<unsigned char>`です。要素が0なら禁止、0以外なら許可です。`assign(個数, 値)`は、その個数の要素を同じ値で埋めます。ここでは$4\times2=8$要素を1で埋めてから、仕事1・担当先0に対応する1要素だけを0にします。

添字の式は単項費用と同じ`仕事番号 * K + 担当先番号`です。これは費用ではなく必須条件なので、担当者0へ置くと安くなるとしても禁止されます。

空の`allowed`は「全て許可」です。「何も許可しない」の意味ではありません。ある自由な仕事の行をすべて0にすると、その仕事の置き先がなくなります。

## ステップ6：既に決まった担当を固定する

ステップ5へ`fixed_label`を追加します。仕事0は担当者0で確定です。優先順位を確認するため、例ではその固定先を許可表で0にします。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.demand = {2, 1, 2, 0};
    p.lower_capacity = {2, 2};
    p.upper_capacity = {4, 4};
    p.lower_count = {2, 2};
    p.upper_count = {2, 2};
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}};
    p.allowed.assign(4 * 2, 1);
    p.allowed[1 * 2 + 0] = 0;
    p.fixed_label = {0, -1, -1, -1}; // 追加
    p.allowed[0 * 2 + 0] = 0;       // 優先順位を見るための意図的な矛盾

    const auto r = solve_capacitated_graph_labeling(
        p, capacitated_potts_cost<long long>{});
    if (!r.feasible) return 1;
    for (int a : r.label) std::cout << a << ' ';
    std::cout << "\nfixed job0=" << r.label[0] << '\n';
}
```

`fixed_label`は長さNの配列です。`-1`は固定しない、0以上はそのラベルへ固定する指定です。`{0,-1,-1,-1}`では仕事0だけを担当者0へ固定します。

この例でも仕事0は担当者0へ入ります。ライブラリは固定頂点について許可表との共通部分を取りません。固定を例外扱いにしたくない業務では、ライブラリへ渡す前に「固定先が許可されているか」を別途検査してください。通常の入力で意図的に矛盾を作ることを勧めているわけではありません。

固定しても容量・件数は消費します。固定済みの工数だけで上限を超えている入力を、ほかの仕事の配置で解消することはできません。

## ステップ7：自分で作った割当を検査・採点する

ここまでは`solve`の結果を受け取りました。次はステップ6の問題に、手作業で作った割当を渡します。「条件を満たすか」と「費用はいくらか」は別の質問なので、別々の関数を使います。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.demand = {2, 1, 2, 0};
    p.lower_capacity = {2, 2};
    p.upper_capacity = {4, 4};
    p.lower_count = {2, 2};
    p.upper_count = {2, 2};
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}};
    p.allowed.assign(8, 1);
    p.allowed[2] = 0; // 仕事1、担当先0
    p.fixed_label = {0, -1, -1, -1};
    p.allowed[0] = 0;
    const capacitated_potts_cost<long long> cost;
    const std::vector<int> labels = {0, 1, 1, 0};

    if (!is_feasible_capacitated_graph_labeling(p, labels)) return 1;
    const auto value = evaluate_capacitated_graph_labeling(p, labels, cost);
    std::cout << "manual cost=" << value << '\n';

    const std::vector<int> bad = {0, 0, 1, 1};
    std::cout << "bad feasible="
              << is_feasible_capacitated_graph_labeling(p, bad) << '\n';
}
```

`labels`は、仕事0から3の担当先を順番に並べた完全な解です。`is_feasible_capacitated_graph_labeling(p, labels)`は、長さ、ラベル範囲、固定・許可、需要量・件数の条件を検査して`bool`を返します。費用規則は引数に取りません。

`evaluate_capacitated_graph_labeling(p, labels, cost)`は、第1章の目的関数を計算して`long long`を返します。解を探す関数ではなく、制約を検査する関数でもありません。**有効なラベル列かを先に確認**してから採点します。

この割当の単項費用は$0+4+0+4=8$、辺費用は$5+5=10$なので18です。`bad`は仕事1を禁止された担当者0へ置くため、検査結果は0（false）になります。

なお、問題自体の配列長や辺番号などが壊れている場合まで、安全なfalseで処理されるわけではありません。問題の形の検査は主に`assert`で、リリース設定の`NDEBUG`では無効になります。

## ステップ8：同じ条件で比較するための設定を入れる

ここから実行方法を学びます。入力は見通しのよいステップ2へ戻し、設定`options`だけを追加します。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.upper_capacity = {2, 2};
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}};
    const capacitated_potts_cost<long long> cost;

    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1.0;
    opt.iteration_limit = 20000;
    opt.seed = 7;
    opt.initial_trials = 8;
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);
    if (!r.feasible) return 1;
    std::cout << "initial=" << r.initial_objective
              << " best=" << r.objective
              << " iterations=" << r.iterations
              << " ms=" << r.elapsed_ms
              << " cache=" << r.used_potts_cache << '\n';
}
```

| 設定 | 今回の意味 | 既定値 |
|---|---|---|
| `time_limit_ms=-1.0` | 相対時間による打切りを使わない | 1000.0ms |
| `iteration_limit=20000` | 目的値を改善する提案を最大20,000回まで行う | -1＝反復数による制限なし |
| `seed=7` | 擬似乱数列の出発点を7にする | 1 |
| `initial_trials=8` | 初期の貪欲構築を最大8回試す | 8 |

「提案回数」は、採用された変更の数ではありません。動けない提案や採用されなかった提案も数えます。初期構築、v24の制約修復、温度の準備は`iterations`に含みません。`initial_trials`が0以下でも構築は最低1回試します。詳細は付録で説明します。

`initial_objective`は採用した初期解の費用、`objective`は返した最良解の費用です。`elapsed_ms`は呼出し内部の準備を含む経過時間です。`used_potts_cache`は高速化用の差分表を作ったかどうかで、最適性を表すフラグではありません。

固定反復・時間制限なし・同じseedは再現比較に向きます。ただし入力順、費用規則、キャッシュ設定、実行環境もそろえる必要があります。時間制限を使うと、同じseedでも進められる回数が変わります。

**時間と反復数の両方を負にし、後述のdeadlineも未設定にしてはいけません。** 通常の探索が停止する条件を失います。

## ステップ9：既存の実行可能解を出発点にする

ステップ8の`solve`を`improve`へ変更します。例えば前の担当表`{0,0,1,1}`がすでにあるなら、最初から作り直す必要はありません。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.upper_capacity = {2, 2};
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}};
    const capacitated_potts_cost<long long> cost;
    const std::vector<int> initial = {0, 0, 1, 1};
    if (!is_feasible_capacitated_graph_labeling(p, initial)) return 1;

    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1.0;
    opt.iteration_limit = 20000;
    opt.seed = 7;
    const auto r = improve_capacitated_graph_labeling(p, initial, cost, opt);
    if (!r.feasible) return 1;
    std::cout << "before=" << r.initial_objective
              << " after=" << r.objective << '\n';
    for (int a : r.label) std::cout << a << ' ';
    std::cout << '\n';
}
```

引数の順は、問題、初期ラベル列、費用規則、設定です。初期ラベル列は長さNで、全体が実行可能でなければなりません。関数が入力配列を書き換えることはなく、新しい結果を返します。

整数の費用で桁あふれがなければ、返却費用は初期費用以下です。改善を見つけられなければ初期解を保持します。最適性の保証ではなく、「持っている実行可能解を失わない」という性質です。

この呼出しでは`initial_trials`は使いません。不正な初期解を修復するAPIでもありません。v24の自動制約修復は`solve`の初期構築失敗時に限られます。

## ステップ10：仕事1と2だけを変更する

ステップ9から変更してよい頂点集合を追加し、subset版を呼びます。仕事0、3の担当先はそのままにします。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.upper_capacity = {2, 2};
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}};
    const capacitated_potts_cost<long long> cost;
    std::vector<int> labels = {0, 0, 1, 1};
    if (!is_feasible_capacitated_graph_labeling(p, labels)) return 1;
    const auto before = evaluate_capacitated_graph_labeling(p, labels, cost);
    const std::vector<int> scope = {1, 2};

    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1.0;
    opt.iteration_limit = 20000;
    opt.seed = 7;
    const auto patch = improve_capacitated_graph_labeling_subset(
        p, labels, scope, cost, opt);
    if (patch.status != capacitated_graph_labeling_status::success) return 1;
    for (std::size_t i = 0; i < patch.changed_vertices.size(); ++i)
        labels[patch.changed_vertices[i]] = patch.new_labels[i];
    if (!is_feasible_capacitated_graph_labeling(p, labels)) return 1;
    const auto after = evaluate_capacitated_graph_labeling(p, labels, cost);
    if (after - before != patch.objective_delta) return 1;
    std::cout << "before=" << before << " delta=" << patch.objective_delta
              << " after=" << after << '\n';
    for (int a : labels) std::cout << a << ' ';
    std::cout << '\n';
}
```

`scope={1,2}`は**頂点番号**です。ラベル番号ではありません。引数は問題、完全解、変更可能頂点列、費用規則、設定の順です。範囲には重複、範囲外番号、`fixed_label`で固定した頂点を入れられません。

subsetは完全解ではなく変更点を返すため、結果の型が異なります。

| 結果 | 意味 |
|---|---|
| `status` | 成功かを確認する。subset結果に`feasible`はない |
| `changed_vertices[i]` | 変更する仕事の番号 |
| `new_labels[i]` | その仕事の新しい担当先 |
| `objective_delta` | 同じ問題での「変更後の費用−変更前の費用」 |

2本の配列は同じ長さです。添字`i`は変更一覧の位置で、頂点番号そのものではありません。上のループが変更を呼出し側の完全解へ適用します。成功でも、より良い解が見つからなければ変更一覧は空、差分は0です。

この例では仕事0—2と仕事1—3の辺は、範囲の内外をまたぐ**境界辺**です。subsetはそれらの費用も計算し、範囲外の仕事を含む全体容量も守ります。範囲だけを切り取って外側を忘れる処理ではありません。

初期状態で各担当者が2件ずつなら、1仕事だけを別担当へ動かすと上限を破ります。そこで異なる担当の仕事1と2を両方範囲へ入れています。範囲が狭すぎると、改善の余地があっても変更できません。

## ステップ11：費用や制約が変わった次のターン

ステップ9を2ターン繰り返します。1ターン目は費用を更新、2ターン目は仕事0の現在の担当先を禁止します。前の解が使えるかを毎回確認します。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.upper_capacity = {2, 2};
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}};
    const capacitated_potts_cost<long long> cost;
    std::vector<int> labels = {0, 0, 1, 1};
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1.0;
    opt.iteration_limit = 20000;
    opt.seed = 7;

    for (int turn = 0; turn < 2; ++turn) {
        if (turn == 0) {
            p.unary_cost[1 * 2 + 0] = 3; // 費用1→3
        } else {
            p.allowed.assign(8, 1);
            p.allowed[0 * 2 + labels[0]] = 0;
        }
        const bool reusable = is_feasible_capacitated_graph_labeling(p, labels);
        if (reusable)
            std::cout << "new-input baseline="
                      << evaluate_capacitated_graph_labeling(p, labels, cost) << '\n';
        const auto r = reusable
            ? improve_capacitated_graph_labeling(p, labels, cost, opt)
            : solve_capacitated_graph_labeling(p, cost, opt);
        if (!r.feasible) {
            std::cerr << "No valid plan for this turn\n";
            return 1;
        }
        labels = r.label;
        std::cout << "turn=" << turn << " reused=" << reusable
                  << " cost=" << r.objective << '\n';
    }
}
```

`reusable`がtrueなら前の完全解を改善し、falseなら`solve`から作り直します。`条件 ? A : B`は、条件が真ならA、偽ならBを実行する書き方です。この例では費用だけを変える1ターン目は再利用でき、以前の担当を禁止する2ターン目は再利用できません。

費用を更新したら、比較の基準も**新しい問題で採点した前の解**です。特にsubsetの差分は、新しい問題内での改善分です。前ターンの古い目的値へそのまま加えると、入力変更による費用差を落としてしまいます。

呼出しが終わった後なら辺の追加・削除、重み・種類、単項費用、容量、許可・固定も変更できます。ただし配列サイズと番号は整合させます。辺番号を使う自作費用では、辺の並べ替えに合わせて外部データも並べ替えます。

再利用するのは主に問題データと完全ラベル列です。内部のキャッシュや乱数・探索状態をそのまま次回へ継続する公開APIはありません。呼出し中に入力データを書き換えてもいけません。

## ステップ12：担当先の組合せごとに費用を変える

ここから費用の表現を広げます。ステップ8のPotts規則を、2×2の表を使う規則へ変更します。例えば担当0から担当1への引継ぎ単価は1、逆方向は4とします。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.upper_capacity = {2, 2};
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}};
    const std::vector<long long> matrix = {
        0, 1,
        4, 0
    };
    const capacitated_label_matrix_cost<long long> cost{2, &matrix};

    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1.0;
    opt.iteration_limit = 20000;
    opt.seed = 7;
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);
    if (!r.feasible) return 1;
    std::cout << "cost=" << r.objective << '\n';
    for (int a : r.label) std::cout << a << ' ';
    std::cout << '\n';
}
```

`matrix`はラベル対の単価表です。行が辺の`u`側のラベル、列が`v`側のラベルです。平らな配列では`matrix[a*K+b]`になります。

| 行・列 | 単価 |
|---|---:|
| 担当0→担当0 | 0 |
| 担当0→担当1 | 1 |
| 担当1→担当0 | 4 |
| 担当1→担当1 | 0 |

`cost{2,&matrix}`の2は表の幅Kで、`p.label_count`と一致させます。`&matrix`は表の場所を渡すポインタです。表をコピーして保持する型ではないため、表は呼出しが終わるまで存在し、内容が変わらない必要があります。今回のように`main`内で表を作り、その後で呼び出せば有効です。デフォルトの幅0・ポインタ`nullptr`のまま使ってはいけません。

各辺の費用は`edge.weight * matrix[a*K+b]`です。重み5の辺が担当0→1なら5、担当1→0なら20です。表が非対称なので、辺の端点を逆にすると別の意味になります。

ラベルを日付や位置の番号と考えれば、この同じAPIで距離や順序に応じた費用も表せます。ただし「前に置いてほしい」という費用は、「必ず前でなければならない」という必須条件ではありません。

## ステップ13：辺の種類ごとに違う費用規則を使う

ステップ12の共通行列を、自分で定義する関数に置き換えます。連携関係は分離を罰し、競合関係は同居を罰するとします。あわせて、辺番号ごとの追加係数を読む例にします。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.upper_capacity = {2, 2};
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {
        {0, 2, 5, 0},
        {1, 3, 5, 1},
        {0, 0, 3, 1},
        {1, 3, 1, 0}
    };
    const std::vector<long long> extra = {0, 2, 0, 0};
    const auto cost = [&extra](
        int id, const capacitated_graph_labeling_edge<long long>& e,
        int a, int b) -> long long {
        const long long w = e.weight + extra[id];
        if (e.type == 0) return a != b ? w : 0LL;
        return a == b ? w : 0LL;
    };

    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1.0;
    opt.iteration_limit = 20000;
    opt.seed = 7;
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);
    if (!r.feasible) return 1;
    if (!is_feasible_capacitated_graph_labeling(p, r.label)) return 1;
    const auto exact = evaluate_capacitated_graph_labeling(p, r.label, cost);
    if (r.objective != exact) return 1;
    std::cout << "cost=" << exact << '\n';
}
```

`[&extra](引数) -> long long { ... }`は、その場で費用関数を作る**ラムダ式**です。`&extra`は外側の配列を参照して使う指定、`-> long long`は戻り値の型です。`extra`は呼出しが終わるまで生存させます。

引数は4つです。

| 引数 | 意味 |
|---|---|
| `id` | `p.edges`の0始まりの添字 |
| `e` | その辺。`u`、`v`、`weight`、`type`を読むことができる |
| `a` | 元の辺の`u`側のラベル |
| `b` | 元の辺の`v`側のラベル |

今回の規則では、重みと`extra[id]`を足した値を罰金にします。`type=0`なら異なるラベル、`type=1`なら同じラベルのときだけ罰金です。入力の`type`は0か1だけにしています。

| 辺番号 | この例での意味 |
|---|---|
| 0 | 仕事0と2が別担当なら5 |
| 1 | 仕事1と3が同担当なら$5+2=7$ |
| 2 | 仕事0と自分自身の関係。常に同担当なので3 |
| 3 | 仕事1と3の追加関係。別担当なら1。辺1と両方を数える |

辺2のような**自己辺**、辺1と3のような**多重辺**も入力できます。自己辺は規則へ`a,a`を渡して1回評価します。Pottsなら自己辺の費用は0です。

自作規則が返す値が、その辺の費用そのものです。ライブラリが後からさらに`weight`を掛けることはありません。したがって重みを使わない規則も書けます。

費用は、同じ辺・同じ両端ラベルには常に同じ値を返す必要があります。他の仕事の現在ラベル、現在の総工数、乱数、時刻によって戻り値を変えてはいけません。

組込みPotts型には`is_potts=true`という高速化の目印があります。自作規則には付けないのが安全です。厳密に`weight * [a != b]`と等しい規則だけがtrueを宣言できます。定数を足しただけでも別物なので、今回の規則には付けません。

## ステップ14：小数の費用を使う

ステップ12から費用の型を`double`へ変え、単項費用と行列の値を10分の1にします。需要量・容量は小数型にはなりません。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>

int main() {
    capacitated_graph_labeling_problem<double> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.upper_capacity = {2, 2};
    p.unary_cost = {0.0, 0.6, 0.1, 0.4, 0.5, 0.0, 0.4, 0.1};
    p.edges = {{0, 2, 5.0, 0}, {1, 3, 5.0, 0}};
    const std::vector<double> matrix = {0.0, 0.1, 0.4, 0.0};
    const capacitated_label_matrix_cost<double> cost{2, &matrix};
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1.0;
    opt.iteration_limit = 20000;
    opt.seed = 7;

    const auto r = solve_capacitated_graph_labeling(p, cost, opt);
    if (!r.feasible) return 1;
    if (!is_feasible_capacitated_graph_labeling(p, r.label)) return 1;
    const double exact = evaluate_capacitated_graph_labeling(p, r.label, cost);
    const double tolerance = 1e-9 * std::max(1.0, std::abs(exact));
    if (std::abs(r.objective - exact) > tolerance) return 1;
    std::cout << std::setprecision(12)
              << "reported=" << r.objective << " recalculated=" << exact << '\n';
}
```

問題型、行列の要素型、行列コスト型の3か所を`double`にそろえます。辺重みも問題の費用型に従います。ここでは辺重み5は維持し、単価表だけを10分の1にしたので、二項費用も10分の1です。

コンピュータの小数には丸め誤差があります。0.1も二進数では有限桁で厳密に表せないため、計算順が違うと小さな差が出ます。`exact`という変数名は「全体を再計算した値」という意味で、実数として誤差ゼロという意味ではありません。

例では、差の絶対値が`1e-9 * max(1, |再計算値|)`以下なら一致と扱います。`1e-9`は$10^{-9}$、`std::abs`は絶対値、`std::max`は大きい方です。これはこの小さな教材用の許容誤差で、どんな費用規模・反復数にも適切とは限りません。`setprecision(12)`は表示桁数の指定です。

NaN・無限大を費用に入れません。正確な小数単位が決まっているなら、例えば円ではなく銭にそろえるように整数化し、`long long`を使う選択もあります。いずれの場合も積・途中の和・差分が数値範囲に収まる必要があります。

## ステップ15：負の費用で「別担当にしたい」を表す

ステップ8の問題から単項費用を省き、辺重みを負にします。干渉する仕事を別の時間枠へ分ける例です。担当先を色と見れば、2色のMax-Cutにも対応します。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.upper_capacity = {2, 2};
    p.edges = {{0, 2, -5, 0}, {1, 3, -5, 0}};
    const capacitated_potts_cost<long long> cost;
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1.0;
    opt.iteration_limit = 20000;
    opt.seed = 7;
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);
    if (!r.feasible) return 1;

    long long total_penalty = 0;
    for (const auto& e : p.edges) total_penalty += -e.weight;
    const long long cut_reward = -r.objective;
    const long long conflict_penalty = total_penalty + r.objective;
    std::cout << "library=" << r.objective
              << " cut_reward=" << cut_reward
              << " conflicts=" << conflict_penalty << '\n';
}
```

重み-5のPotts辺は、同担当なら0、別担当なら-5です。最小化は-5の方を好むので、別担当への報酬になります。今回の目的値は負になっても正常です。

同担当のとき罰金$w$、別担当なら0にしたいとします。$w$は0以上です。

$$w[a=b]=w-w[a\ne b].$$

つまり負のPotts費用$-w[a\ne b]$へ定数$w$を足せば元の罰金です。定数を足しても、同じ問題内でどの解が良いかは変わりません。

この例では罰金総額の基準が$5+5=10$なので、`conflict_penalty = 10 + r.objective`です。切り離せた重みの合計は`-r.objective`です。単項費用や別の費用項を入れた場合、この単純な変換を目的値全体へそのまま適用してはいけません。

「必ず隣接する仕事を異色にする」制約ではありません。罰金0が必要なら`conflict_penalty==0`を別に確認します。正のPotts辺と負のPotts辺を混ぜれば、同居希望と分離希望が混在したグループ分けも表せます。

## ステップ16：1つの位置へ1つの設備を置く

ステップ12の行列費用を使い、件数上下限を1へ変更します。今度は仕事を設備、ラベルを配置位置と考えます。例を3設備・3位置へ小さく組み替えます。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 3;
    p.label_count = 3;
    p.lower_count = {1, 1, 1};
    p.upper_count = {1, 1, 1};
    p.edges = {{0, 1, 8, 0}, {1, 2, 2, 0}};
    const std::vector<long long> distance = {
        0, 1, 3,
        1, 0, 2,
        3, 2, 0
    };
    const capacitated_label_matrix_cost<long long> cost{3, &distance};
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1.0;
    opt.iteration_limit = 20000;
    opt.seed = 7;
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);
    if (!r.feasible) return 1;
    for (int v = 0; v < p.vertex_count; ++v)
        std::cout << "machine " << v << " -> position " << r.label[v] << '\n';
    std::cout << "transport=" << r.objective << '\n';
}
```

`vertex_count=label_count=3`で、各位置の件数をちょうど1にします。3つの設備が3つの位置を一つずつ使う**一対一配置**になります。容量配列は不要なので省略しています。

辺の重み8、2は設備間で運ぶ量、`distance`は位置間の距離です。例えば設備0→1が位置0→1なら、輸送費は$8\times1=8$です。表は3行×3列の9要素で、`cost`の幅も3にします。

これは二次割当（QAP）型のモデルです。二項費用がなければ2章の線形割当へ簡単になる場合がありますが、設備対と位置対が掛け合わさるこの費用は一般に別問題です。大きな密なQAPには専用ソルバーも比較対象です。

設備ごとの置きやすさを入れるなら、これまでの`unary_cost`を3×3で追加できます。設備の位置が決まっていれば`fixed_label`も同じ意味で使えます。

## ステップ17：実時間の予算と共通の締切を使う

固定反復を使ったステップ8・9を、時間予算で動かします。まず30msを目安に解を作り、残りで改善します。2回の呼出しに共通の締切も渡します。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>
#include <chrono>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.upper_capacity = {2, 2};
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}};
    const capacitated_potts_cost<long long> cost;
    using Clock = std::chrono::steady_clock;
    const auto stop = Clock::now() + std::chrono::milliseconds(100);
    capacitated_graph_labeling_options opt;
    opt.deadline = stop;
    opt.time_limit_ms = 30.0;
    opt.seed = 7;
    auto best = solve_capacitated_graph_labeling(p, cost, opt);
    if (!best.feasible) return 1;

    if (Clock::now() < stop) {
        opt.time_limit_ms = 70.0;
        opt.seed = 8;
        const auto r = improve_capacitated_graph_labeling(p, best.label, cost, opt);
        if (r.feasible && r.objective < best.objective) best = r;
    }
    std::cout << "cost=" << best.objective << '\n';
}
```

`std::chrono::steady_clock`は、経過時間の計測に使う時計です。日時を表す時計ではありません。`Clock`はその長い型名の別名、`now()`は現在の時点、`milliseconds(100)`は100msの長さです。

- `time_limit_ms`は**その呼出しの開始から**の相対予算です。最初は30、次は70にします。
- `deadline`は複数の呼出しで共有できる**絶対の終了時点**です。ここでは`stop`を共通にします。
- `iteration_limit`は指定しないので既定の-1です。今回は時間を停止条件にします。
- 2回目の`seed=8`は乱数列を変える指定です。前回の乱数状態を続きから再開するものではありません。

複数の有効な停止条件があると、最初に達した条件で止めようとします。ただし**厳密な実時間上限ではありません**。最初の構築や準備処理、間欠的な時計確認のぶんだけ超過し得ます。外側に100msの厳格な締切があるなら、それより早い時点を渡して出力・検証の余裕も取ってください。この例は締切を共有する方法を示す教材です。

時間で止める結果は実行環境によって変わります。固定反復の例と同じ出力を期待するテストには使いません。

## ステップ18：複数seedを試し、成功解の中から選ぶ

ステップ8の呼出しをループにします。比較を分かりやすくするため時間制限は使わず、seed 1〜5それぞれで5,000反復を試します。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.upper_capacity = {2, 2};
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}};
    const capacitated_potts_cost<long long> cost;
    capacitated_graph_labeling_result<long long> best;
    long long total_iterations = 0;
    int succeeded = 0;

    for (int seed = 1; seed <= 5; ++seed) {
        capacitated_graph_labeling_options opt;
        opt.time_limit_ms = -1.0;
        opt.iteration_limit = 5000;
        opt.seed = static_cast<std::uint64_t>(seed);
        const auto r = solve_capacitated_graph_labeling(p, cost, opt);
        total_iterations += r.iterations;
        if (!r.feasible) continue;
        ++succeeded;
        if (!best.feasible || r.objective < best.objective) best = r;
    }
    if (!best.feasible) return 1;
    std::cout << "successful runs=" << succeeded
              << " iterations=" << total_iterations
              << " best=" << best.objective << '\n';
}
```

`capacitated_graph_labeling_result<long long> best`は結果を保持する公開型です。初期状態では`feasible=false`です。目的値の初期値が0だからといって、費用0の解を持っているわけではありません。

まず成功したかを確認し、初めての成功またはより安い成功結果なら`best=r`で保存します。`continue`は失敗した回を飛ばして次へ進む文です。`static_cast<std::uint64_t>`はseedの型へ明示的に変換しています。

5,000は各呼出しの反復上限です。5回全体の上限ではありません。`best.iterations`も採用した1回の値なので、全体値は別の`total_iterations`で集計します。この集計にも初期構築・制約修復の手数は含まれません。

試行数を増やしても、同じ総時間の中では各回の改善時間が短くなるため、必ず良くなるとは限りません。全体の時間予算がある場合は、ステップ17の共通deadlineを各試行へ渡し、残り時間を配分します。

## ステップ19：温度とキャッシュを明示する

普段は既定値から始めれば十分です。設定の効果を調べたい場合に備え、ステップ9の改善を2回行い、温度を固定したうえでキャッシュの有無を切り替えます。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.upper_capacity = {2, 2};
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}};
    const capacitated_potts_cost<long long> cost;
    const std::vector<int> initial = {0, 0, 1, 1};
    for (std::size_t budget : std::vector<std::size_t>{0, 256ULL << 20}) {
        capacitated_graph_labeling_options opt;
        opt.time_limit_ms = -1.0;
        opt.iteration_limit = 20000;
        opt.seed = 7;
        opt.initial_temperature = 2.0;
        opt.final_temperature = 0.05;
        opt.potts_cache_max_bytes = budget;
        const auto r = improve_capacitated_graph_labeling(p, initial, cost, opt);
        if (!r.feasible) return 1;
        std::cout << "budget=" << budget
                  << " used=" << r.used_potts_cache
                  << " cost=" << r.objective << '\n';
    }
}
```

| 新しい設定 | 意味 |
|---|---|
| `initial_temperature=2.0` | 探索開始時の温度。大きいほど、費用が悪化する変更も試しやすい |
| `final_temperature=0.05` | 終了時の温度。今回は開始時より小さくして、後半ほど悪化を受け入れにくくする |
| `potts_cache_max_bytes` | Potts専用の差分表と任意の隣接フィルタに使う合計バイト上限 |

温度は費用の差と同じ尺度で考えます。物理的な気温ではありません。負の値なら自動設定で、両方の既定値は-1です。片方だけ明示することもできます。0は内部で`1e-12`へ補正され、完全な「悪化は絶対禁止」という設定とはわずかに異なります。自動値や受理式は付録で扱います。

最初のキャッシュ上限0は無効化です。次の`256ULL << 20`は$256\times2^{20}$バイト、256MiBです。`ULL`は符号なし整数リテラルの型を指定し、`<<20`は左へ20bitずらす、ここでは$2^{20}$倍の操作です。

表が上限に収まらなければ、ライブラリは表を使わない評価に切り替えます。行列や通常の自作費用にはPotts専用表を使いません。また、この値は問題データや隣接リストを含む総メモリ上限ではありません。

キャッシュ有無で乱数の整数生成経路も変わるため、同じseedでも解が同じとは限りません。メモリ設定の比較では速度だけでなく解品質も測ります。この小さな例は設定方法を示すもので、速度ベンチマークではありません。

## ステップ20：失敗の種類と「何もしない」場合を区別する

最後に戻り値の扱いを確かめます。ステップ8の問題から、意図的に初期解や範囲を変えます。今回は`iteration_limit=0`で、目的値改善をしない設定です。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>
#include <cstdlib>

int main() {
    using Status = capacitated_graph_labeling_status;
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.upper_capacity = {2, 2};
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}};
    const capacitated_potts_cost<long long> cost;
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1.0;
    opt.iteration_limit = 0;
    const auto check = [](Status got, Status expected, const char* name) {
        if (got != expected) std::exit(1);
        std::cout << name << ": expected status\n";
    };

    const auto built = solve_capacitated_graph_labeling(p, cost, opt);
    check(built.status, Status::success, "construction only");
    if (!built.feasible || built.iterations != 0) return 1;
    const auto bad_initial = improve_capacitated_graph_labeling(
        p, std::vector<int>{0, 0, 0, 0}, cost, opt);
    check(bad_initial.status, Status::infeasible_initial_solution, "bad initial");
    const std::vector<int> good = {0, 0, 1, 1};
    const auto bad_scope = improve_capacitated_graph_labeling_subset(
        p, good, std::vector<int>{1, 1}, cost, opt);
    check(bad_scope.status, Status::invalid_repair_scope, "duplicate scope");
    const auto no_change = improve_capacitated_graph_labeling_subset(
        p, good, std::vector<int>{}, cost, opt);
    check(no_change.status, Status::success, "empty scope");
    if (!no_change.changed_vertices.empty() || no_change.objective_delta != 0) return 1;

    p.upper_capacity = {1, 1};
    const auto failed = solve_capacitated_graph_labeling(p, cost, opt);
    check(failed.status, Status::construction_failed, "not found");
    if (failed.feasible || !failed.label.empty()) return 1;
    capacitated_graph_labeling_problem<long long> empty;
    empty.label_count = 2;
    const auto zero = solve_capacitated_graph_labeling(empty, cost, opt);
    check(zero.status, Status::success, "zero vertices");
    if (!zero.feasible || !zero.label.empty() || zero.objective != 0) return 1;
}
```

`Status`は列挙型の別名です。`Status::success`のように名前で結果を比較します。`check`はこの例専用の確認関数で、結果が想定と違えば終了コード1で止めます。

| 出力の名前 | 意味 |
|---|---|
| `construction only` | 反復上限0でも`solve`は初期解を構築する。v24では必要なら制約修復も行う |
| `bad initial` | 全仕事を担当0へ置く初期解は容量違反。`improve`は直さず拒否する |
| `duplicate scope` | 同じ頂点1を2回指定した範囲は不正 |
| `empty scope` | 変更範囲が空なら、実行可能な入力を変更せず正常終了 |
| `not found` | 容量1＋1に4仕事を置けない。この例は人間が不可能と分かるが、返却status自体は不可能性証明ではない |
| `zero vertices` | N=0、K=2、正の下限なしなら、空の割当が正しい実行可能解 |

失敗結果の目的値の既定値は0です。それを費用0の成功解と解釈してはいけません。一方、最後の例は空のラベル列でも成功です。**ラベルが空かどうかではなく、`feasible`または`status`で判断**します。

`construction_failed`の原因には、本当に不可能、探索が見つけられなかった、時間が足りなかった、などがあります。入力を確認し、余裕があるなら時間・seedを変えたり、外部で実行可能解を作って`improve`へ渡したりします。

`time_limit_ms=0`または期限切れでも、`solve`は最初の貪欲構築を1回試します。期限切れなら制約修復は開始しません。`improve`とsubsetは実行可能な初期解を確認し、変更なしで返します。0msを「関数内部で仕事を一切しない」指示として使わないでください。

## ステップ21：結果をまとめてデバッグ出力する

これまでは`r.objective`などの値を一つずつ出力しました。処理が期待どおりか調べるときは、戻り値をそのまま`std::cout`や`std::cerr`へ渡せます。ステップ9・10と同じ4仕事の例で、`solve`、全体`improve`、subsetの結果を表示します。問題の条件と探索の意味はそのままで、結果の出し方を広げます。

```cpp
#include "capacitated_graph_labeling_v25.hpp"
#include <iostream>

int main() {
    capacitated_graph_labeling_problem<long long> p;
    p.vertex_count = 4;
    p.label_count = 2;
    p.upper_capacity = {2, 2};
    p.unary_cost = {0, 6, 1, 4, 5, 0, 4, 1};
    p.edges = {{0, 2, 5, 0}, {1, 3, 5, 0}};
    const capacitated_potts_cost<long long> cost;
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1;
    opt.iteration_limit = 20000;
    opt.seed = 7;

    const auto solved = solve_capacitated_graph_labeling(p, cost, opt);
    std::cout << "solve: " << solved << '\n';
    if (!solved.feasible) return 1;

    const std::vector<int> initial = {0, 0, 1, 1};
    const auto improved = improve_capacitated_graph_labeling(p, initial, cost, opt);
    std::cout << "improve: " << improved << '\n';
    if (!improved.feasible) return 1;

    const std::vector<int> mutable_vertices = {1, 2};
    const auto repaired = improve_capacitated_graph_labeling_subset(
        p, initial, mutable_vertices, cost, opt);
    std::cout << "subset: " << repaired << '\n';
    std::cout << "status: " << repaired.status << '\n';
    if (repaired.status != capacitated_graph_labeling_status::success) return 1;
}
```

### 順番に読む

1. 問題は4仕事・2担当者です。`upper_capacity={2,2}`は1人2件まで、`unary_cost`は仕事順・担当者順の費用です。各辺の4値は仕事番号2つ、別担当にしたときの費用5、未使用の種類0です。`demand`を省略したので各仕事は1工数です。
2. `cost`はステップ2で使ったPotts規則です。`time_limit_ms=-1`で相対時間制限を無効にし、`iteration_limit=20000`で提案回数を制限します。`seed=7`は乱数列の出発点です。指定しない項目には既定値を使います。
3. `solved`は`solve`が新しく構築・改善した完全解です。`improved`は費用12の`initial={0,0,1,1}`を出発点に改善した完全解です。いずれも`capacitated_graph_labeling_result<long long>`です。
4. `repaired`は、同じ`initial`のうち`mutable_vertices={1,2}`だけを変更する結果です。型は`capacitated_graph_labeling_repair_result<long long>`で、変更点と費用差分を持ちます。この例では仕事1を担当1、仕事2を担当0へ変えると費用が2減ります。
5. `std::cout << solved`と書くと、その型に対応する`operator<<`という関数が呼ばれます。関数の第1引数は出力先`std::ostream&`、第2引数は`const 結果型&`です。結果をコピーしたり変更したりせずに読みます。
6. 返り値は同じ出力先への参照です。そのため、`"solve: "`、結果、`'\n'`を`<<`で順につなげられます。演算子自身は改行もflushも追加しません。
7. `std::cout`は標準出力、`std::cerr`は標準エラー出力です。デバッグログを標準エラーへ出したい場合は、上の出力先を`std::cerr`に置き換えるだけです。たとえば`std::cerr << solved << '\n';`と書きます。

### 表示を読む

完全解は`result{status=success, feasible=true, objective=10, ...}`のように始まり、初期目的値、反復数、経過ms、キャッシュ使用の有無、ラベル列、負荷、件数が続きます。配列は`label=[0, 1, 0, 1]`の形です。件数制約を指定していないので、この例の`count=[]`は正常です。

subsetは`repair_result{status=success, objective_delta=-2, ...}`の形で、`changed_vertices=[1, 2]`と`new_labels=[1, 0]`が対応します。経過時間は実行ごとに変わります。`repaired.status`だけを表示した最後の行は`status: success`になります。

| 新しい書き方・仕様 | 意味 |
|---|---|
| `出力先 << 結果` | 通常結果・subset結果の全フィールドを名前付きで1行に表示 |
| `出力先 << 結果.status` | 数値ではなく`success`などの名前を表示 |
| `feasible=true`、`used_potts_cache=false` | 真偽値は`true`／`false`で表示 |
| 各配列の先頭20要素 | 長い配列でも出力を一定の要素数に抑える |
| `... (+5 more)` | 21番目以降の5要素を省略した、という意味 |
| `std::fixed`・`std::setprecision` | 数値に適用したい場合は呼出し側で指定でき、出力演算子はflags・precisionを変更しない |

たとえば25要素の配列は、0始まりの添字0〜19を表示し、末尾に`... (+5 more)`と出します。20要素ちょうどなら省略表示はありません。元の配列から要素が消えるわけではないので、解の適用には元の配列全体を使います。

失敗結果も同じ書き方で出力できます。失敗時の目的値0は有効な解の費用とは限りません。表示の先頭にある`feasible`／`status`を見てから内容を判断してください。

## 4. API早見表と実用上の確認

### 4.1 公開型と関数の対応

ここまでのコードで公開APIを一通り使いました。名前を暗記するより、用途から選んでください。

| 公開型 | 役割 | 主に学んだステップ |
|---|---|---|
| `capacitated_graph_labeling_problem<Cost>` | 問題データ | 1〜6 |
| `capacitated_graph_labeling_edge<Cost>` | `u,v,weight,type`を持つ辺 | 2、13 |
| `capacitated_potts_cost<Cost>` | 異なるラベルに重みを課す規則 | 2、15 |
| `capacitated_label_matrix_cost<Cost>` | 共通行列を使う規則 | 12 |
| `capacitated_graph_labeling_options` | 実行設定 | 8、17、19 |
| `capacitated_graph_labeling_result<Cost>` | 完全解の結果 | 1、18、21 |
| `capacitated_graph_labeling_repair_result<Cost>` | subsetの変更点と差分 | 10、21 |
| `capacitated_graph_labeling_status` | 成功・失敗理由の列挙型 | 20、21 |

辺型を除く上のテンプレート型は`Cost`を省略すると`long long`です。`capacitated_graph_labeling_edge`の型引数は省略しません。問題型の`edge_type`も対応する辺型の別名です。

| 公開関数 | 引数の順序 | 戻り値 |
|---|---|---|
| `solve_capacitated_graph_labeling` | 問題、規則、設定 | 完全解の結果 |
| `improve_capacitated_graph_labeling` | 問題、初期完全解、規則、設定 | 完全解の結果 |
| `improve_capacitated_graph_labeling_subset` | 問題、初期完全解、変更可能頂点列、規則、設定 | 変更点と差分 |
| `is_feasible_capacitated_graph_labeling` | 問題、ラベル列 | `bool` |
| `evaluate_capacitated_graph_labeling` | 問題、ラベル列、規則 | `Cost` |

上の3つの探索関数だけが設定を取り、その最後の設定引数は省略できます。費用規則は省略できません。`problem.solve()`のようなメソッドを呼ぶ形ではありません。入力はconst参照で受け取られ、探索関数が呼出し側の入力を書き換えることはありません。

このほか、通常結果・subset結果・statusを`std::ostream`へ出す`operator<<`があります。通常結果・subset結果はconst参照で受け取り、いずれも出力先の`std::ostream&`を返します。使い方と表示上限はステップ21を参照してください。

### 4.2 問題の全フィールド

Nは頂点数、Kはラベル数です。「空」は空のvectorです。N×K表は常に`v*K+a`で読みます。

| フィールド | 型・長さ | 初期値／空の意味 |
|---|---|---|
| `vertex_count` | `int`、N≥0 | 0 |
| `label_count` | `int`、K>0 | 0なので、呼出し前に必ず正の値へ設定 |
| `demand` | `vector<long long>`、空またはN | 全頂点1 |
| `lower_capacity` | `vector<long long>`、空またはK | 全ラベル0 |
| `upper_capacity` | `vector<long long>`、空またはK | 全ラベルが総需要量 |
| `lower_count` | `vector<int>`、空またはK | 全ラベル0 |
| `upper_count` | `vector<int>`、空またはK | 全ラベルN |
| `unary_cost` | `vector<Cost>`、空またはN×K | 全費用0 |
| `edges` | `vector<edge_type>`、M本 | 辺なし |
| `fixed_label` | `vector<int>`、空またはN | 全頂点自由。指定時は-1が自由、0〜K-1が固定先 |
| `allowed` | `vector<unsigned char>`、空またはN×K | 全て許可。指定時は0が禁止、0以外が許可 |

需要量・容量は非負です。各ラベルで下限≤上限、件数は0〜Nにします。費用は負でも構いません。辺型の初期値は`u=0,v=0,weight=1,type=0`です。端点は0〜N-1にします。組込みの2つの費用規則は`type`を使いません。

行列規則の公開データは`label_count`と`matrix`です。前者はK、後者はK×K要素を持つ費用vectorへのポインタを与えます。`is_potts`は組込み型のコンパイル時定数であり、実行時に変更する設定ではありません。Pottsはtrue、行列型はfalseです。

### 4.3 設定の全フィールド

| フィールド | 型 | 既定値 | 読み方 |
|---|---|---|---|
| `time_limit_ms` | `double` | 1000.0 | 相対時間。0以上で有効、負で無効 |
| `iteration_limit` | `long long` | -1 | 目的値改善の提案上限。0以上で有効、負で無効 |
| `seed` | `uint64_t` | 1 | 乱数列の出発点 |
| `initial_trials` | `int` | 8 | 貪欲構築回数。最低1回。修復再開回数にも影響 |
| `initial_temperature` | `double` | -1.0 | 開始温度。負なら自動 |
| `final_temperature` | `double` | -1.0 | 終了温度。負なら自動 |
| `potts_cache_max_bytes` | `size_t` | `256ULL << 20` | Potts表・隣接フィルタのバイト上限 |
| `deadline` | `steady_clock::time_point` | `time_point::max()` | 共通締切。既定値は制限なし |

時間・温度のdoubleには有限値を使います。特に無限大を無制限の代わりに入れず、相対時間は負の有限値、絶対締切は既定の最大時点を使います。

### 4.4 結果の全フィールド

| 完全解の結果 | 型 | 意味 |
|---|---|---|
| `label` | `vector<int>` | 成功時はN個のラベル |
| `load` | `vector<long long>` | 成功時はK個の総需要量 |
| `count` | `vector<int>` | 件数制約があればK個、なければ空 |
| `objective` | `Cost` | 返した解の費用 |
| `initial_objective` | `Cost` | 構築した初期解、または渡した初期解の費用 |
| `iterations` | `long long` | 目的値改善で数えた提案回数 |
| `elapsed_ms` | `double` | 内部準備を含む経過時間 |
| `feasible` | `bool` | 実行可能解を返せたか |
| `used_potts_cache` | `bool` | Potts差分表を作ったか |
| `status` | status型 | 成功または失敗理由 |

初期値は配列が空、数値が0、boolがfalse、statusが`construction_failed`です。失敗時に費用の数値を解として使いません。

| subsetの結果 | 型 | 意味 |
|---|---|---|
| `changed_vertices` | `vector<int>` | 初期解からラベルが変わる頂点 |
| `new_labels` | `vector<int>` | 同じ位置の頂点へ設定するラベル |
| `objective_delta` | `Cost` | 入力した同じ問題での新費用−初期費用 |
| `iterations` | `long long` | 目的値改善の提案回数 |
| `elapsed_ms` | `double` | 内部経過時間 |
| `used_potts_cache` | `bool` | Potts差分表を作ったか |
| `status` | status型 | 成功または失敗理由 |

subsetの初期statusは`invalid_repair_scope`、ほかは空配列・0・falseです。完全解・全体目的値・load・count・feasibleは持ちません。

statusの4種類は次です。

| status | 意味 |
|---|---|
| `success` | 正常終了。探索なし・変更なしも含む |
| `construction_failed` | `solve`が実行可能初期解を見つけられなかった |
| `infeasible_initial_solution` | `improve`またはsubsetの初期完全解が実行不可能 |
| `invalid_repair_scope` | subsetの範囲が不正。重複・範囲外・固定頂点を含む |

subsetで初期解と範囲の両方が不正なら、範囲のエラーが先です。これらは「問題構造体のあらゆる入力不備」を捕捉するエラーコードではありません。

### 4.5 入力を大きくする前の確認

1. 各仕事が必ず一つのラベルへ入るモデルでよいか。未割当を許すなら、それを別のラベルとして費用・容量を定義する。
2. 工数、件数、費用の単位を混同していないか。小数の工数は意味を保つ整数単位へそろえる。
3. 固定が許可より優先する仕様でよいか。
4. 辺の向き、重複、行列の行・列の意味が合っているか。
5. 成功判定をしてから結果を使い、独立した計算で費用を確認したか。
6. `long long`の需要量合計と容量の演算、`Cost`の積・和・差分が範囲内か。
7. N×K、行列のK×K、辺数、隣接配列の添字が`int`の範囲に収まるか。メモリも別途足りるか。
8. 実行時間・メモリ・解品質を、実際に使う入力で測ったか。

本教材は小さく手計算できる入力でAPIを理解するためのものです。例で最適値が得られても、一般の入力で同じ保証はありません。

### 4.6 配布と検証の範囲

同時配布の`capacitated_graph_labeling_guides_v25.zip`には、本教材、更新済みユーザーガイド、v25ヘッダ、21個の教材用`.cpp`、既存ガイドの関数例をまとめたヘッダ、検証用コードと実行記録を収録します。教材のコードと`.cpp`は同じ内容です。

検証では、MarkdownからC++ブロックをそのまま取り出して個別にコンパイル・実行します。通常ビルドと、ラベルからの独立した制約・目的値の確認を行います。時間制限の例は、経過時間やラベル列の完全一致を合格条件にはしません。

GCC 13.3で、独立検証付き通常実行21/21、既存ガイドの11関数例、ライブラリ内蔵テスト、デバッグ出力の検査が成功しました。内蔵テストはASan/UBSanでも実行し、配列の空・20・21・100,000要素、失敗status、小数、書式設定の保持を確認しました。LeakSanitizerは無効です。複数の翻訳単位からの利用も確認しています。実行済みコードと掲載コードの一致はSHA-256で確認しています。

参照するv25ヘッダのSHA-256は

`6fda295d7404b5680a6839f4c4b85be1f0b041db422022d5219a96442d6cc2c1`

です。v25は結果のデバッグ出力を追加した版で、探索アルゴリズムはv24と同じです。

## 付録：ライブラリの実装

ここからは使うために必須ではない説明です。「なぜそのような戻り値や制約になるのか」を知りたいときに読みます。APIとして保証する入力・出力と、今の版の内部の工夫は分けて考えてください。

### A.1 全体の流れ

処理には、**実行可能な初期解を用意する段階**と、**その解の目的値を改善する段階**があります。

1. 入力の形を検査し、各頂点につながる辺を調べやすい配列と変更可能範囲を作る。
2. `solve`なら、複数の貪欲な配置で実行可能初期解を探す。
3. 貪欲配置がすべて失敗した場合だけ、v24の制約修復を試す。作れなければ失敗で返す。
4. `improve`・subsetなら2〜3をせず、渡された完全解の実行可能性を確認して読み込む。
5. 反復上限0、期限切れ、可変頂点なし、K=1などなら、改善せず結果を返す。
6. 必要な高速化の表を作り、探索の温度を準備する。
7. 1頂点の移動または2頂点の交換を提案し、終了条件まで繰り返す。
8. 最後の状態ではなく、記録していた最良解を返す。subsetなら初期解からの改善差分を返す。

重要な区別があります。3の制約修復では容量・件数違反を一時的に許しますが、7の目的値改善では必ず実行可能性を守ります。制約修復中の不正な割当を、成功解として返すことはありません。

### A.2 辺へ素早くアクセスする配列

ある仕事の担当先を変えると、変化するのはその仕事の単項費用と、つながっている辺の費用です。全辺を毎回調べる必要はありません。

内部では各頂点に接続する辺を連続して並べ、その区切り位置を別の配列へ保存します。これはCSR形式の隣接リストと呼ばれます。各要素は相手頂点と元の辺番号を持ちます。

普通の辺は両端から見られるよう2か所へ登録し、自己辺は1か所です。目的値へ辺を足す回数とは別で、目的値では入力辺ごとに1回だけ数えます。

subsetでも全体の隣接情報と完全ラベル列が必要です。変更可能範囲の頂点を選別し、その範囲に一端でも接する辺を集めます。外側だけで決まる費用は差分から省けますが、境界辺と全体容量は省けません。

探索エンジンは問題型のprivate内部にあり、各公開関数の呼出し中だけ使います。永続的なエンジンをユーザーが保持する設計ではありません。

### A.3 貪欲な初期配置

**貪欲**とは、仕事を一つずつ扱い、その時点でよい置き先を選んでいくことです。一度決めた配置をこの段階では後戻りしません。

まず固定頂点を置き、その負荷・件数を数えます。残りは許可先が少ない仕事、需要量が大きい仕事を優先します。置き先の選択肢が少ない仕事を後回しにしないためです。

同じ順だけを繰り返さないよう、試行番号を4で割った余りで順序と選び方を変えます。

| 余り | 追加の優先順と選び方 |
|---|---|
| 0 | 関係する辺が多い頂点を優先。同順位は乱数 |
| 1 | 幅優先で近い頂点を先にたどった順序 |
| 2 | 乱数順。配置後の残容量が小さい先を優先 |
| 3 | 乱数順。費用の小さい先を優先 |

幅優先とは、ある頂点から距離1の相手、距離2の相手、という順に調べる方法です。ここでは配置順を作るだけで、ラベル領域の連結性を保証するものではありません。

候補の許可条件と容量・件数の上限を確認します。残りの工数が下限の不足分を満たすのに使い切られる段階では、不足している担当先を優先します。件数下限でも同じ考え方を使います。工数0の頂点は工数の不足分を減らせないので、既に工数下限を満たした担当先へ置くことも許します。

通常は、単項費用と配置済み隣接頂点への費用が小さい候補を選びます。余り2の試行ではまず残容量を比べ、同じなら費用を比べます。さらに同点なら乱数で決めます。

Potts費用では、配置済み隣接頂点への重みをラベル別にまとめ、候補ラベルごとの辺費用を素早く計算します。一般の二項規則では、候補ラベルに応じて辺の費用を評価します。

最後に全制約を検査し、成功した構築のうち最小費用の解を選びます。2回目以降の試行開始時には期限を確認しますが、1回目の構築途中を細かく中断しません。

### A.4 v24の制約修復：置き方の行き詰まりをほどく

この段階は**貪欲配置がすべて失敗した場合だけ**です。目的は費用の最小化ではなく、まず容量・件数の違反をゼロにすることです。固定・許可条件はここでも守ります。

許可先を列挙し、置き先がない仕事、固定などによる負荷だけで上限超過するラベル、入り得る仕事を全部集めても下限未満になるラベルなどを調べます。これらは失敗を早く判断するための必要条件で、通れば成功する十分条件ではありません。

第1章の記号で、違反量を次の式で測ります。

$$V= \sum_ {a=0}^{K-1} \left[ \max(0,L_ {a}-W_ {a})+\max(0,W_ {a}-R_ {a}) +\lambda\left\{\max(0,A_ {a}-n_ {a})+\max(0,n_ {a}-B_ {a})\right\} \right].$$

- $V$は今回の総違反量です。目的関数$C$とは別の値です。
- $W_ {a},n_ {a}$は現在の負荷と件数、$L_ {a},R_ {a},A_ {a},B_ {a}$は指定した上下限です。
- $\max(0,x)$は、$x$が正なら$x$、そうでなければ0です。
- $\lambda=\max(1,\lfloor(\sum_ {v} d_ {v})/\max(1,N)\rfloor)$は件数違反の重みです。平均需要量を切り捨て、最低1にします。$\lfloor x\rfloor$は小数点以下の切捨てです。

例えば容量2〜4の担当先へ工数5を置けば、上限違反は1です。工数1なら下限違反が1です。範囲内なら両方0です。すべての担当先で容量・件数が範囲内になれば$V=0$です。

最初は最後の失敗した貪欲配置を使い、未配置の仕事を許可先へ補います。次のような1手を繰り返します。

1. 実効的な許可先が2つ以上ある頂点を最大64回抽選し、各頂点の全許可先への移動を比較する。
2. そのような頂点どうしの交換を最大256回抽選して比較する。標本は重複し得る。
3. 違反量の差が最小の候補を選び、同点なら乱数で決める。差が正の一時的な悪化も許す。
4. 動いた先・元の負荷と件数、違反量だけを更新する。

有効な許可先が2つ以上ある頂点数をFとします。1回の試行は最大$1024+32F$手です。最良違反量が改善されない期間が$4F+256$手を超えた場合も、次の試行へ進みます。2回目以降は許可先から配置を選び直します。

試行の再開回数は最大`max(1, min(8, initial_trials))`です。したがって`initial_trials`を8より増やしても、この修復の回数上限は増えません。期限も共有し、修復中は32手ごと、候補表の準備では64頂点ごとに時刻を確認します。チェック間の仕事は中断できないので、厳密な締切保証ではありません。

違反量とその差分は`__int128_t`で計算します。ただし、需要量・容量そのものの演算は`long long`なので、入力の桁あふれ対策が不要になるわけではありません。

$V=0$になったら、全体の実行可能性を再検査し、元の目的関数を計算して通常の改善へ進みます。修復手数は`iterations`へ入りません。成功を保証する手法でもなく、厳しい等式容量、多数の固定、狭い許可先などでは依然として見つからない場合があります。

### A.5 実行可能な範囲での移動と交換

初期解ができた後は、次の2種類を使います。

| 操作 | 変更 | 制約の確認 |
|---|---|---|
| recolor | 1頂点を別ラベルへ移動 | 許可先、移動元・移動先の需要量と件数 |
| swap | 異なるラベルの2頂点を交換 | 両方の許可先と交換後の需要量。件数は変わらない |

最初に試す種類はrecolor 55%、swap 45%です。有効候補がなければもう一方も試すので、採用された変更の比率がこの割合になるとは限りません。

動かす頂点は、全可変頂点から選ぶ方法と、関係する辺の端点から選ぶ方法を50%ずつ混ぜます。後者は多くの関係を持つ頂点が選ばれやすくなります。subsetなら範囲外頂点は選びません。

recolorは最大`min(K-1,4)`候補、swapは相手を最大8候補比べます。すべての変更を網羅するわけではなく、重複抽選もあります。

Potts型では87.5%で隣接関係を使って行先を選びます。正・ゼロの辺なら隣接頂点と同じラベル、負の辺なら隣接頂点と異なるラベルを候補にします。残りは現在と異なるラベルから一様に選びます。一般の二項規則や孤立頂点では、現在と異なるラベルから選びます。

各ラベルに属する可変頂点のリストも持ちます。交換相手をそこから選び、頂点の移動後はリストの末尾を使って穴を埋めるため、毎回全頂点を並べ直す必要はありません。

件数を厳密に固定するとrecolorができなくなることがあります。さらに重み付き容量を厳密に固定すると、工数の異なる2頂点のswapもできません。**目的値改善中に容量違反を経由したり、3頂点以上をまとめて循環交換したりする機能はありません。** v24の初期制約修復と混同しないでください。

### A.6 費用の差分を計算する

差分を$\Delta=C(\text{変更後})-C(\text{変更前})$とします。負なら改善、0なら同じ、正なら悪化です。

一般の費用規則では、recolorした頂点の単項と接続辺だけを調べます。辺の向きは元の`u,v`を保ち、自己辺は両端のラベルを同時に変えて評価します。多重辺はそれぞれ数えます。

swapで2つのrecolor差分を何も考えずに足すと、2頂点を直接つなぐ辺を誤って数えることがあります。実装は片方が既に変更された状態としてもう片方を計算し、整合する差分を得ます。

v24では一般差分関数に`[[gnu::noinline]]`を指定しています。大きな隣接走査を呼出し側へ過剰に展開しないための実装上の工夫で、APIの意味や費用式を変えるものではありません。

### A.7 Potts専用の差分表と隣接フィルタ

Pottsでは、頂点vからラベルaの隣接頂点へつながる辺の重み合計$H(v,a)$を表にします。自己辺は除き、多重辺はすべて足します。

$$H(v,a)=\sum_ {e\in I(v)}w_ {e}[\ell_ {o(e,v)}=a].$$

$I(v)$は頂点vにつながる自己辺以外の辺集合、$o(e,v)$はその辺の反対側の頂点、$w_ {e}$は辺重みです。$H$は利用者の新しい費用ではなく、同じ費用を速く計算する内部の表です。

頂点vをラベルbからaへ変える差分は

$$\Delta=U(v,a)-U(v,b)+H(v,b)-H(v,a)$$

です。同じラベルだった相手への費用が増え、新しく同じになる相手への費用が減る、と読めます。表があれば候補の評価は短い計算になります。ただし変更を採用したら、隣接頂点側の表を更新する必要があります。

異なるラベルのu、vを交換するときは、両recolor差分に、u—v間の直接辺の重み総和の2倍を補正として加えます。これは直接辺の重複評価を直すためです。

直接辺がないと先に分かれば、補正のための隣接走査を省けます。そこで頂点ごとに64bitの**隣接フィルタ**を1本持ち、相手番号の下位6bitに対応する位置を立てます。

別の頂点が同じbitへ重なることはあります。その場合は「辺があるかもしれない」として実際に辺を調べます。一方、bitが立っていなければ確実に非隣接です。費用を近似するのではなく、不必要な走査だけを省きます。

実際に動かせる頂点数をQとすると、表に`sizeof(Cost)*Q*K`バイト、フィルタに`8*Q`バイトを使います。上限に表だけ入れば表だけを使い、表も入らなければ一般の隣接走査へ切り替えます。固定頂点は表の対象から除き、subsetでは指定範囲だけが対象です。

### A.8 温度と、一時的な悪化を受け入れる理由

今すぐ費用を下げる変更だけを許すと、いったん悪化しないと抜けられない配置に止まることがあります。そこで実行可能性は守ったまま、費用が悪化する候補を確率的に受け入れます。これを焼きなましと呼びます。

探索前に最大256提案を調べ、有効で差分が0でないものを最大128個集めます。まだ解へ適用せず、差分の絶対値を小さい順に並べた中央付近の値を尺度にします。標本がなければ尺度1を使い、尺度は最低`1e-12`にします。

自動の開始温度は尺度の0.5倍、終了温度は0.03倍です。温度を両方明示した場合でも、この標本採取は実施します。実際に使う温度は最低`1e-12`です。

開始温度を$T_ {0}$、終了温度を$T_ {1}$、進捗を$p$とすると、

$$T(p)=T_ {0}\left(\frac{T_ {1}}{T_ {0}}\right)^p$$

で温度を変えます。$p$は0〜1で、指定された反復上限に対する割合と、指定された時間予算に対する割合の大きい方です。使わない停止条件の割合は加えません。準備時間も時間側の進捗に含みます。

$\Delta\le0$なら候補を受け入れ、$\Delta>0$なら次の確率で受け入れます。

$$\Pr(\text{受理})=\exp(-\Delta/T).$$

$\exp(x)$は$e^x$、$e$は約2.718の定数です。式を暗記する必要はありません。「悪化幅$\Delta$が大きいと受け入れにくい」「温度Tが高いと受け入れやすい」と読めば十分です。容量・許可違反をこの確率で認めることはありません。

温度・時計はまとめて更新します。正の反復上限をIとすると、更新間隔は`min(1024, max(1, I/256))`反復です。正の反復上限がなければ1,024反復ごとです。この間欠検査も実時間超過の理由になります。

### A.9 最良解の保持、subsetの出力、乱数

現在の状態が一時的に悪化しても、これまでで最小だった費用とラベル列を別に残します。最良解が更新されたときだけ、前の更新以降に動いた頂点を同期します。負荷・件数はラベル数ぶん保存します。

終了時に返すのは現在の状態ではなく最良解です。subsetでは初期費用より厳密に小さくなった場合だけ、初期ラベルと異なる頂点を抽出します。そのため同費用の別解へ移りたいという用途には向きません。

subset内部では、変更可能頂点の単項と、そこへ接続する辺だけの費用を扱えます。外側だけの費用は変化しない定数だからです。返した差分は、数学的には全目的値の変化と一致します。小数では全体再計算との丸め差に注意します。

乱数は64bit状態から作ります。範囲付き整数は、Potts差分表を使う探索では128bit積と必要な再抽選による方法、それ以外では剰余による方法を使います。後者には範囲によってごく小さい偏りがあります。制約修復も64bit状態を引き継ぎ、剰余による範囲変換を使います。

固定seedだけで異なる環境・入力順・キャッシュ設定・制限時間の結果が一致するわけではありません。乱数の再現性と、最適解の保証も別です。

### A.10 計算量・メモリ・限界

計算量の$O(\cdot)$は、入力が大きくなったときの作業量の増え方を表します。例えば$O(N)$はNにほぼ比例する作業、$O(NK)$はN×Kにほぼ比例する作業です。実行ミリ秒の保証ではありません。

ここでNは頂点数、Mは辺数、Kはラベル数、Qは実際の可変頂点数、$M_ {S}$は可変頂点へ一端でも接続する辺数、$\deg(v)$は頂点vにつながる辺の数です。費用規則1回を一定時間で評価できると仮定します。

| 処理 | 作業量の目安 |
|---|---|
| 全体の目的値を再計算 | $O(N+M)$ |
| ラベル列の実行可能性確認 | $O(N+K)$。assert有効時の問題検査には辺走査も加わる |
| 隣接配列・範囲・初期解の準備 | $O(N+M+K)$ |
| Pottsの貪欲構築1回 | $O(N\log N+NK+M+K)$。最初の項は配置順の整列 |
| 一般規則の貪欲構築1回 | $O(N\log N+NK+KM+K)$ |
| Potts表の準備 | $O(QK+M_ {S})$ |
| 一般のrecolor差分 | $O(\deg(v))$ |
| 一般のswap差分 | $O(\deg(u)+\deg(v))$ |
| Potts表によるrecolor差分 | $O(1)$ |
| Potts表によるswap差分 | 非隣接確定なら$O(1)$、直接辺の確認が必要なら$O(\min(\deg(u),\deg(v)))$ |
| 受理後のPotts表更新 | 動いた頂点の次数に比例 |
| 最良解の保存 | 前回以降に変更した頂点数＋Kに比例 |

v24の制約修復では、許可先表の準備に$O(NK+K)$、最大で$O(NK+N+K)$程度の追加メモリを使います。実効許可先が2つ以上の頂点数をF、再開回数をR、1回の手数をJとすると、探索はおおむね

$$O\!\left(R\{N+K+J\min(64,F)(K+1)\}\right)$$

です。$R\le8$、$J\le1024+32F$です。成功時の全体確認と目的値再計算も必要です。目的費用が複雑でも、修復の候補比較自体は容量・件数の違反量で行います。

通常の主な追加メモリは、隣接配列に$O(N+M)$、ラベル・負荷・可変範囲の管理に$O(N+K)$、Potts表に$O(QK)$です。入力に持つ単項表・許可表は別にN×K要素です。subsetでも全体準備のコストが消えるわけではありません。

最も大切な限界は、実行可能解の存在や最適性を保証しないことです。改善段階が使う移動・交換だけでは、別の良い解へ到達できないこともあります。制約を弱めてよいかは元の問題の判断です。勝手に上下限を緩めて得た解を、元の問題の成功解として扱わないでください。厳密性が必要なら2章の手法、実行可能初期解があるならステップ9、部分変更ならステップ10へ戻って選び直します。
