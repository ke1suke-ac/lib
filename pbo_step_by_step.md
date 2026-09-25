# PboSolver ステップバイステップガイド

対象ヘッダ：`pbo_solver_v06.hpp`。C++20、GCC、標準ライブラリだけで使うライブラリです。

「どの候補を選ぶか」を、0と1の変数、条件、点数で表すところから始めます。高校数学の範囲で読み進められるように、数式の記号を先に説明し、コードでは毎回少しずつ機能を増やします。C++の変数、配列、ループ、関数呼出しを知っていることを前提にします。

本文は**最小コード1本と発展コード24本**で構成します。すべて独立した`main`を持ち、標準入力は不要です。前の節のコードを継ぎ足す必要はありません。新しい内容は「モデル化 → 評価 → 初期解と再探索 → モデル更新 → 実行管理」の順に登場します。内部アルゴリズムは末尾の付録にまとめます。

## 1. 何を解くライブラリか

### 1.1 選ぶか、選ばないかを決める

たとえば、次の3件から仕事を選び、使える資源の合計を4以下に抑えたいとします。

| 仕事 | 変数ID | 選ぶと得られる利益 | 使う資源 |
|---|---:|---:|---:|
| A | 0 | 10 | 3 |
| B | 1 | 7 | 2 |
| C | 2 | 6 | 2 |

$x_ {0}$、$x_ {1}$、$x_ {2}$を、それぞれA、B、Cを選ぶかどうかとします。選ぶなら1、選ばないなら0です。値は自由な整数ではなく、この2通りだけです。

大きくしたい点数は次です。

$$F(x)=10x_ {0}+7x_ {1}+6x_ {2}$$

$x$は3個の選択値をまとめたもの、$F(x)$はその選び方の点数です。10、7、6は選択時の利益です。選ばない仕事は変数が0なので、利益も0になります。

守るべき条件は次です。

$$3x_ {0}+2x_ {1}+2x_ {2}\le 4$$

Aだけなら利益10、BとCなら資源4で利益13です。AとBは利益17ですが資源5なので、この問題の答えにはできません。**条件を守る選び方の中で、目的値を最大にする**のが基本です。

PBOはPseudo-Boolean Optimizationの略です。入力の変数は0/1ですが、評価する関数の値は0/1に限らず利益などの数値になる、という問題の呼び方です。

### 1.2 必須条件と、破ってもよい条件

このライブラリには3種類の条件があります。条件を1本書いたものを「行」と呼びます。

| 種類 | 意味 | 条件を破ったとき |
|---|---|---|
| `Hard` | 必ず守りたい条件 | その候補は実行可能解ではなくなる |
| `LinearPenalty` | なるべく守りたい条件 | 不足・超過した量に比例して減点する |
| `FixedPenalty` | なるべく守りたい条件 | 違反した行ごとに定額を一度だけ減点する |

「実行可能」は、すべての必須条件と固定値を守っている、という意味です。「最適」は、それ以上に点数の高い実行可能解が存在しない、という意味です。この2つは別です。

上限4に対して使用量が6なら、違反量は2です。重み3の`LinearPenalty`なら6点減点、重み3の`FixedPenalty`なら3点減点です。`Hard`なら「6点減点で済ませる」とは扱わず、条件違反として別に報告します。

### 1.3 一般の目的関数

まず、各行の値を次で表します。

$$s_ {j}(x)=\sum_ {i=0}^{n-1}a_ {ji}x_ {i}$$

| 記号 | 意味 |
|---|---|
| $n$ | 選ぶかどうかを決める変数の個数 |
| $i$ | 変数ID。0から$n-1$まで |
| $x_ {i}$ | 変数$i$の値。0または1 |
| $m$ | 行の本数 |
| $j$ | 行ID。0から$m-1$まで |
| $a_ {ji}$ | 行$j$で変数$i$に掛ける整数係数。正負どちらも使える |
| $s_ {j}(x)$ | 行$j$の合計値。たとえば使用資源量 |
| $l_ {j}$、$u_ {j}$ | 行$j$の下限と上限。各々省略できる |

$\sum$は「指定範囲を足し合わせる」という記号です。コードでは、係数が0の組合せまで全部書く必要はありません。実際に必要な項だけを列挙できます。

下限と上限が両方ある場合、行の違反量を次で定めます。

$$d_ {j}(x)=\max(0,l_ {j}-s_ {j}(x))+\max(0,s_ {j}(x)-u_ {j})$$

$\max$は引数の大きい方を取ります。下限に足りない分と、上限を超えた分を足したものです。下限を省略した行では第1項を0、上限を省略した行では第2項を0とします。両方を指定するときは下限が上限以下でなければなりません。上下限内なら違反量は0です。

最大化する目的関数は次です。

$$F(x)=c+\sum_ {i=0}^{n-1}p_ {i}x_ {i}-\sum_ {j\in L}w_ {j}d_ {j}(x)-\sum_ {j\in Q}w_ {j}\mathbf{1}(d_ {j}(x)>0)$$

| 記号 | 意味 |
|---|---|
| $F(x)$ | 返却される整数の目的値。大きいほどよい |
| $c$ | 選択に関係なく加算する定数。`constant` |
| $p_ {i}$ | 変数$i$を選ぶ利益。`profit[i]`。負なら費用を表せる |
| $L$ | `LinearPenalty`の行IDを集めた集合 |
| $Q$ | `FixedPenalty`の行IDを集めた集合 |
| $w_ {j}$ | 行$j$の減点の重み。0以上の整数 |
| $\mathbf{1}(d_ {j}(x)>0)$ | 違反量が正なら1、違反量0なら0となる記号 |

$c$、$p_ {i}$、各行の係数・境界・種類・重みは利用者が決めます。ライブラリが決めるのは$x_ {i}$です。$s_ {j}$、$d_ {j}$、$F$は$x$から計算される値です。

必須条件の行IDの集合を$H$とすると、実行可能解はすべての$j\in H$で$d_ {j}(x)=0$を満たします。さらに、固定する変数IDの集合を$J$、その指定値を$f_ {i}$とすれば、すべての$i\in J$で$x_ {i}=f_ {i}$が必要です。$f_ {i}$は0または1です。

必須条件の違反は、この目的関数から自動的に巨大な点数を引く仕組みではありません。したがって、**目的値が大きくても実行可能とは限りません**。結果では両方を確認します。

複数の候補をすべて選んだときだけ利益を加える`add_all_profit`もあります。これは上の目的関数で表せる形に変換されます。使い方は発展コード10で説明します。

### 1.4 向いている問題の形

容量付きの選択、仕事の割当、必要な場所を覆う候補の選択、排他関係、依存関係、ソフトな希望条件を、同じモデルにまとめられます。変数1個が少数の行にだけ関係する「疎な」問題を主な対象とします。

一方、任意のC++評価関数を渡すライブラリではありません。目的と条件を、この整数の行と利益の表現に落とし込む必要があります。連続値や経路の順番が中心の問題では、別の表現が適することもあります。

大きな問題では、最適解の取得も実行可能解の発見も保証されません。返却された解が条件を守るかは確認できますが、「未発見」と「実行可能解が存在しないことの証明」は区別します。

## 2. 類似ソルバーと使い分け

### 2.1 条件が限定されるなら、先に厳密解を検討する

以下の条件に当てはまる場合は、最適解を保証できる方法が候補です。方法の名前と適用条件だけ挙げ、アルゴリズムそのものの説明は省略します。

| 限定した問題の形 | 厳密解の候補 | 判断のポイント |
|---|---|---|
| 自由に決める0–1変数が十分少ない | 全列挙・分枝限定法 | 全組合せは変数数$r$に対して$2^{r}$個。十分な時間があれば最適性を確認できる |
| 非負整数の重量、1本の容量制約、足し算だけの利益 | 0–1ナップサックDP | 容量$B$と変数数$n$に対する$O(nB)$が実行可能なら有力。複雑な追加条件は別問題 |
| 人と仕事を1対1で対応させ、各対応の点数を足す | 線形割当法、最小費用流 | 任意の相互依存や同時選択報酬を足すと、この単純形から外れる |
| 整数容量・需給・辺ごとの費用で表せる配送や割当 | 最小費用流 | 問題全体がそのネットワーク表現に収まることが条件 |
| 各変数の利益だけで、行も相互作用もない | 正利益の自由変数を選ぶ | 固定値を守り、利益0はどちらでもよい。探索自体が不要 |

ナップサックDPの計算量は容量の数値そのものに依存します。容量が非常に大きいと、制約が1本でも簡単とは限りません。[ナップサックの計算量に関する論文](https://arxiv.org/abs/1802.06440)も参照できます。単純な割当や一部のチーム付き割当については、[線形割当の公式資料](https://developers.google.com/optimization/assignment/linear_assignment)と[最小費用流への変換例](https://developers.google.com/optimization/flow/assignment_min_cost_flow)が適用範囲の確認に役立ちます。

本ライブラリにも小規模な厳密探索はありますが、公開結果に「最適性を証明済み」というフラグはありません。証明が必要な用途では、保証と終了状態が明示される方法を選びます。

### 2.2 似た対象を扱うソルバー

| 候補 | 主に向いている状況 | PboSolverを選ぶ観点 |
|---|---|---|
| 最大重み独立集合・重み付き集合パッキング用ソルバー | 利益付き候補を、衝突や資源の重複なしで選ぶ | それ以外の容量、下限、等式、正負係数、減点を同時に扱いたい |
| QUBO用ソルバー | 0–1変数の一次・二次の点数が中心 | 必須の行条件を明示し、違反の有無と目的値を分けて扱いたい |
| 一般の焼きなまし・ビームサーチ | 状態、評価、近傍を問題に合わせて作りたい | 条件と点数だけを定義し、モデル内の差分管理も任せたい |
| OR-Tools CP-SAT、整数計画ソルバー | 外部ソフトを使え、汎用的な制約や最適性の判定が必要 | 標準ライブラリだけのヘッダとして短い時間予算に組み込みたい |
| 経路・配置・スケジューリングなどの専用ソルバー | 順序や幾何など、固有の構造が重要 | 全体のうち、候補の採否だけを切り出して解きたい |

QUBOは、たとえば$x_ {i}x_ {k}$のような2変数の積の点数を持つ形式です。PboSolverではこの同時選択利益を`add_all_profit`で表せます。逆に、必須条件をQUBOの減点に変換する場合は、減点の設定が不足すると条件違反が得になる可能性があるため、元の問題との同値性を確認します。

CP-SATは整数の変数・制約を扱い、実行可能な解を得た状態と最適性を確認できた状態を区別します。時間切れなら最適性が分からない場合もあります。[公式の終了状態の説明](https://developers.google.com/optimization/cp/cp_solver)を参照してください。どのソルバーが速いかは問題次第で、単一の優劣はありません。

## 3. コードを動かす準備

`pbo_solver_v06.hpp`と、試したい節の`.cpp`を同じフォルダに置きます。各コードは1本ずつコンパイルしてください。ヘッダをincludeしたとき、ヘッダ内部の単体テスト用`main`は無効になります。

たとえば最小コードを`01_minimal.cpp`として保存した場合は、次で実行できます。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined 01_minimal.cpp -o example
./example
```

以後のC++コードはすべて全文です。共通部分も省略しません。コード中の`using namespace std;`は例を短くするためで、ライブラリ利用に必須ではありません。

数式は外部Markdownリーダー向けの`$...$`と`$$...$$`を使っています。数式表示には、そのリーダーの数式機能を有効にしてください。表示数式は各1行です。

## 最小コード：容量の範囲内で仕事を選ぶ

第1節のA・B・Cをそのまま解きます。問題を作る、solverに渡す、解く、結果を読む、という4段階だけです。

保存名：`01_minimal.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4);

    PboSolver solver(problem);
    const auto& result = solver.solve();
    if (!result.evaluation.feasible()) {
        cout << "no feasible solution found\n";
        return 0;
    }
    cout << result.evaluation.objective << '\n';
    cout << int(result.x[0]) << ' ' << int(result.x[1]) << ' '
         << int(result.x[2]) << '\n';
}
```

出力：

```text
13
0 1 1
```

### 何を、どの順番で渡しているか

1. `PboProblem problem;`で、入力用の箱を作ります。この時点では変数も行もありません。
2. `profit`に3個の値を入れると、変数数も3になります。`profit[0]=10`は、変数0を1にしたとき10点得るという意味です。
3. `add_le`で資源の上限を追加します。`le`は「以下」の意味です。
4. `PboSolver solver(problem);`で、入力モデルをsolverへ取り込みます。
5. `solve()`が候補を調べ、結果を返します。
6. `feasible()`を確認してから、目的値と選択値を使います。

`add_le`の第1引数は、`PboTerm`を並べた配列です。**`{変数ID, 係数}`の順**であり、`{重さ, 利益}`ではありません。

| 指定 | 意味 |
|---|---|
| `{0, 3}` | $3x_ {0}$を行に足す |
| `{1, 2}` | $2x_ {1}$を行に足す |
| `{2, 2}` | $2x_ {2}$を行に足す |
| 第2引数`4` | 合計値の上限。4ちょうどは許可する |

`add_le`の第3引数は省略すると`PboRowKind::Hard`、第4引数は`1`です。第4引数は減点の重みですが、`Hard`では使いません。減点の具体例は後で扱います。戻り値は行IDですが、この例では行を後から変更しないため受け取っていません。

`PboSolver`の第1引数はモデル、第2引数は省略するとseedの`1`です。seedの役割は発展コード13で扱います。`problem`を渡すと内容がコピーされ、solverは自分のモデルを所有します。元の`problem`を後から書き換えても自動反映されません。構築後に元のモデルを使わないなら、`solver(std::move(problem))`で移動できます。

`solve()`の第1引数は実行条件、第2引数は初期候補で、どちらも省略できます。ここでは初期候補を指定せず、既定の時間予算1,950,000マイクロ秒、つまり1.95秒を使っています。これは必ず1.95秒待つという意味ではありません。小さい問題では早く終了することがあります。実行条件は発展コード21以降で詳しく扱います。

`result.x`は`vector<uint8_t>`で、0/1を保持します。`uint8_t`は文字として出力される環境があるため、表示時は`int(...)`に変換します。`const auto&`は結果をコピーせず参照する書き方です。参照の有効な扱い方は発展コード14で確認します。

この出力ではAを選ばず、BとCを選びます。資源は$2+2=4$、利益は$7+6=13$です。小さな例の答えは手計算でも確認できます。一般の大規模問題で同じ最適性が保証される、という意味ではありません。

## 発展コード01：自分で作った候補を評価する

モデルは最小コードと同じです。新しく`evaluate`を使い、AとBを選ぶ候補がなぜ使えないかを確認します。探索は行いません。

保存名：`02_evaluate.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4);
    PboSolver solver(problem);

    vector<uint8_t> candidate = {1, 1, 0};
    auto evaluation = solver.evaluate(candidate);
    cout << evaluation.objective << '\n';
    cout << evaluation.violated_hard_rows << ' '
         << evaluation.violated_fixed_variables << ' '
         << evaluation.feasible() << '\n';
}
```

出力：

```text
17
1 0 0
```

`evaluate(candidate)`の引数は、変数数と同じ長さの0/1配列です。AとBを選ぶので`{1,1,0}`です。`vector<bool>`ではなく`vector<uint8_t>`を使います。長さが合う`array<uint8_t, N>`も渡せます。内部の引数型は`span<const uint8_t>`で、配列の中身を読み取るための窓口です。呼出し側でspanを明示的に作る必要はありません。

`evaluate`は、渡した配列を書き換えずに、現在のモデルで全評価します。solverにその候補を初期解として登録する操作でもありません。

| 出力項目 | この候補の値 | 読み方 |
|---|---:|---|
| `objective` | 17 | 利益は10+7。Hard違反はここから減点されない |
| `violated_hard_rows` | 1 | 上限4を破った行が1本ある |
| `violated_fixed_variables` | 0 | 固定値の違反はない |
| `feasible()` | 0 | 必須条件を満たさない |

違反行数は「何本破ったか」です。超過量が1でも100でも、その行は1本と数えます。`PboEvaluation`が公開するのは上の3個の数値と`feasible()`です。ソフト行の違反本数や違反量の配列を返すAPIはありません。

## 発展コード02：最低限必要なものを指定する

最小コードに「AかBを少なくとも1件は選ぶ」という条件を1行追加します。

保存名：`03_lower.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4);
    problem.add_ge({{0, 1}, {1, 1}}, 1);
    PboSolver solver(problem);
    const auto& result = solver.solve();
    if (!result.evaluation.feasible()) return 1;
    cout << result.evaluation.objective << '\n';
    cout << int(result.x[0]) << ' ' << int(result.x[1]) << ' '
         << int(result.x[2]) << '\n';
}
```

出力：

```text
13
0 1 1
```

新しい条件は$x_ {0}+x_ {1}\ge1$です。`add_ge`は「以上」を表します。

第1引数の`{0,1}`と`{1,1}`は、AとBを1件ずつ数える項です。第2引数`1`が必要件数の下限です。第3・第4引数の省略値は`Hard`と`1`で、最小コードと同じく必須条件になります。

AとBを両方選ぶことまでは禁止しません。「少なくとも1つ」と「ちょうど1つ」は異なります。この例ではBとCが新しい条件も満たすため、答えは変わりません。

この形は被覆にも使えます。ある地点を覆う候補がAとBなら、同じ式で「その地点を少なくとも1回覆う」を表せます。地点ごとに行を追加します。

## 発展コード03：ちょうど1件を選ぶ

最小コードに「3件の中からちょうど1件」という条件を追加します。前の下限条件は入れず、等式を単独で学びます。

保存名：`04_equality.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4);
    problem.add_eq({{0, 1}, {1, 1}, {2, 1}}, 1);
    PboSolver solver(problem);
    const auto& result = solver.solve();
    if (!result.evaluation.feasible()) return 1;
    cout << result.evaluation.objective << '\n';
    cout << int(result.x[0]) << ' ' << int(result.x[1]) << ' '
         << int(result.x[2]) << '\n';
}
```

出力：

```text
10
1 0 0
```

`add_eq`は「等しい」を表し、この例では$x_ {0}+x_ {1}+x_ {2}=1$を追加します。

第1引数は3変数を各1回数える項、第2引数`1`は合計が一致すべき値です。第3・第4引数は省略しているので`Hard`と`1`です。`add_eq`の戻り値も行IDです。

下限1だけなら複数選べましたが、等式1では複数選択も全不選択もできません。A・B・Cのどれか1件となり、最も利益の大きいAを選びます。

## 発展コード04：上下限をまとめて指定する

前の等式を「1件以上2件以下」に緩めます。新しいAPIは`add_range`だけです。

保存名：`05_range.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4);
    problem.add_range({{0, 1}, {1, 1}, {2, 1}}, 1, 2);
    PboSolver solver(problem);
    const auto& result = solver.solve();
    if (!result.evaluation.feasible()) return 1;
    cout << result.evaluation.objective << '\n';
}
```

出力：

```text
13
```

追加した行は$1\le x_ {0}+x_ {1}+x_ {2}\le2$です。資源の行と件数の行は、別々に守ります。

`add_range`の引数は、順に「項の配列、下限、上限、行の種類、重み」です。この例の`1`が下限、`2`が上限で、どちらの端も許可します。種類と重みは省略して`Hard`、`1`です。

境界の型は`optional<long long>`です。値を渡すと境界を設定し、`nullopt`を渡すとその側の境界をなくします。下限を`nullopt`、上限を`2`にすれば「2件以下」だけになります。両方を`nullopt`にした行は常に違反量0です。

`add_eq`は上下限を同じ値にする便利な書き方です。`add_le`、`add_ge`、`add_eq`、`add_range`のどれでも同じ種類の行が作られ、行IDは追加順の0始まりになります。

## 発展コード05：依存関係と同時選択の禁止を表す

最小コードの容量を5に広げ、「AにはBが必要」「BとCは同時に選べない」を追加します。負の係数を初めて使います。

保存名：`06_logic.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 5);
    problem.add_le({{0, 1}, {1, -1}}, 0);
    problem.add_le({{1, 1}, {2, 1}}, 1);
    PboSolver solver(problem);
    const auto& result = solver.solve();
    if (!result.evaluation.feasible()) return 1;
    cout << result.evaluation.objective << '\n';
    cout << int(result.x[0]) << ' ' << int(result.x[1]) << ' '
         << int(result.x[2]) << '\n';
}
```

出力：

```text
17
1 1 0
```

依存関係は$x_ {0}-x_ {1}\le0$です。Aを選んでBを選ばないと左辺が1になり、条件を破ります。Aを選ぶならBも必要ですが、Bだけ選ぶことは許可されます。

`{{0,1},{1,-1}}`の`-1`はBの係数です。変数IDを負にしているのではありません。第2引数の`0`は行和の上限です。

同時選択禁止は$x_ {1}+x_ {2}\le1$です。2変数の係数はともに1、上限は1です。両方0、片方だけ1は許可し、両方1を禁止します。各行の種類と重みは省略値の`Hard`と`1`です。

| 意味 | 0–1変数での式 |
|---|---|
| AならB | $x_ {A}-x_ {B}\le0$ |
| AとBを同じ値にする | $x_ {A}-x_ {B}=0$ |
| AとBを同時に選ばない | $x_ {A}+x_ {B}\le1$ |
| AまたはBを選ぶ | $x_ {A}+x_ {B}\ge1$ |
| 「Aを選ばない」またはBまたはC | $-x_ {A}+x_ {B}+x_ {C}\ge0$ |

最後の行は$(1-x_ {A})+x_ {B}+x_ {C}\ge1$の定数1を右辺へ移したものです。複雑な論理を入れる前に、許したい0/1の組合せを手で確かめるとモデル化の間違いを見つけやすくなります。

## 発展コード06：利益最大化で費用最小化を扱う

下限制約の被覆という使い方を少し広げます。2地点を、費用の少ない候補で覆います。新しく負の利益と定数項を使います。

保存名：`07_minimum_cost.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {-5, -4, -6};
    problem.constant = 100;
    problem.add_ge({{0, 1}, {2, 1}}, 1);
    problem.add_ge({{1, 1}, {2, 1}}, 1);
    PboSolver solver(problem);
    const auto& result = solver.solve();
    if (!result.evaluation.feasible()) return 1;
    cout << result.evaluation.objective << '\n';
    cout << 100 - result.evaluation.objective << '\n';
    cout << int(result.x[0]) << ' ' << int(result.x[1]) << ' '
         << int(result.x[2]) << '\n';
}
```

出力：

```text
94
6
0 0 1
```

候補0は地点0だけを費用5で、候補1は地点1だけを費用4で、候補2は両地点を費用6で覆います。2本の`add_ge`は、各地点を覆う候補を最低1つ選ぶ条件です。各項の係数と下限はいずれも1です。

費用を小さくしたいので、`profit`には費用のマイナスを入れます。`constant=100`を加え、目的値を「100−費用」にしました。この100は例の表示用の基準点で、どの候補を選んでも同じなので最適な選び方を変えません。省略時は0です。

候補0と1なら費用9、候補2だけなら費用6です。目的値94が選ばれ、`100 - objective`で元の費用6へ戻せます。他の報酬・減点を追加した場合は、この単純な変換だけでは純粋な費用にならない点に注意してください。

費用だけを負の利益として入れても、必要条件を入れ忘れると何も選ばない費用0の解が最良になります。「何を達成すべきか」を行で指定することが重要です。

## 発展コード07：すでに決まった選択を固定する

最小コードへ戻り、Bは必ず選び、Cは選ばないと決めます。新しい指定は`fixed`です。

保存名：`08_fixed.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4);
    problem.fixed = {-1, 1, 0};
    PboSolver solver(problem);
    const auto& result = solver.solve();
    if (!result.evaluation.feasible()) return 1;
    cout << result.evaluation.objective << '\n';
    cout << int(result.x[0]) << ' ' << int(result.x[1]) << ' '
         << int(result.x[2]) << '\n';
    vector<uint8_t> candidate = {0, 0, 0};
    cout << solver.evaluate(candidate).violated_fixed_variables << '\n';
}
```

出力：

```text
7
0 1 0
1
```

`fixed`の位置は変数IDと対応します。値の意味は、`-1`が自由、`0`が不選択に固定、`1`が選択に固定です。今回はAが自由、Bが1、Cが0です。`fixed`は`vector<int8_t>`で、選択結果の`vector<uint8_t>`と符号が異なります。

`fixed`全体を省略、つまり空配列のままにすると全変数が自由になります。指定するなら`profit`と同じ長さが必要です。

Bを選んだ状態でAも選ぶと資源5となるため、Aは0です。最後の`evaluate`はBを選ばない候補をそのまま評価するので、固定違反が1になります。`evaluate`は固定に合わせて入力を補正しません。`solve`に初期候補を渡した場合の扱いは発展コード13で説明します。

## 発展コード08：超過量に比例して減点する

最小コードの資源上限を、絶対条件から「超過1単位につき2点減点」に変えます。変えるのは行の種類と重みです。

保存名：`09_linear_penalty.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4,
                   PboRowKind::LinearPenalty, 2);
    PboSolver solver(problem);
    const auto& result = solver.solve();
    cout << result.evaluation.feasible() << '\n';
    cout << result.evaluation.objective << '\n';
    cout << int(result.x[0]) << ' ' << int(result.x[1]) << ' '
         << int(result.x[2]) << '\n';
}
```

出力：

```text
1
17
1 1 1
```

第1引数の項と第2引数の上限4は同じです。第3引数`LinearPenalty`で減点方式を選び、第4引数`2`で超過1単位あたりの減点を指定します。重みは0以上の整数です。

3件すべてを選ぶと利益23、資源7、上限からの超過は3です。減点は$2\times3=6$なので、目的値は17になります。

このモデルにはHard行も固定値もないため、上限を超えても`feasible()`はtrueです。ソフト行の境界を満たすことと、実行可能であることを混同しないでください。

`add_ge`なら下限不足の量、`add_eq`なら目標値から離れた量、`add_range`なら許容範囲の外へ出た量に重みを掛けます。等式の目標が5で実際の値が3なら、違反量は2です。`LinearPenalty`は2乗誤差ではありません。

重みを大きくすると、その違反を避けることを目的値上で強く評価します。ただし必須条件にしたいなら`Hard`を使います。大きな重みを適当に置くだけで、元の必須条件と同値になるとは限りません。

## 発展コード09：違反したら定額だけ減点する

前のコードの`LinearPenalty`を`FixedPenalty`へ変更します。他のパラメータは同じです。

保存名：`10_fixed_penalty.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4,
                   PboRowKind::FixedPenalty, 2);
    PboSolver solver(problem);
    const auto& result = solver.solve();
    cout << result.evaluation.feasible() << '\n';
    cout << result.evaluation.objective << '\n';
    cout << int(result.x[0]) << ' ' << int(result.x[1]) << ' '
         << int(result.x[2]) << '\n';
}
```

出力：

```text
1
21
1 1 1
```

違反量は同じ3ですが、今度は「この行に違反した」という事実に対して2点だけ引きます。利益23から2点引いて目的値21です。`Fixed`は「減点額が一定」という意味で、変数を固定する`fixed`とは別の機能です。

| 上限からの超過量 | 重み2のLinearPenalty | 重み2のFixedPenalty |
|---:|---:|---:|
| 0 | 0点 | 0点 |
| 1 | 2点 | 2点 |
| 3 | 6点 | 2点 |

「追加資源を1単位借りるごとの料金」はLinearPenalty、「一度でも納期条件を破ると発生する固定料金」はFixedPenaltyに対応します。複数のソフト行に違反した場合は、それぞれの減点を足します。

## 発展コード10：一緒に選ぶときだけ利益を変える

容量を5とし、BとCを両方選ぶと5点加点、AとCを両方選ぶと4点減点するようにします。新しいAPIは`add_all_profit`です。

保存名：`11_all_profit.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 5);
    problem.add_all_profit({1, 2}, 5);
    problem.add_all_profit({0, 2}, -4);
    PboSolver solver(problem);
    const auto& result = solver.solve();
    if (!result.evaluation.feasible()) return 1;
    cout << result.evaluation.objective << '\n';
    cout << int(result.x[0]) << ' ' << int(result.x[1]) << ' '
         << int(result.x[2]) << '\n';
}
```

出力：

```text
18
0 1 1
```

自然な書き方での目的関数は次です。

$$F(x)=10x_ {0}+7x_ {1}+6x_ {2}+5x_ {1}x_ {2}-4x_ {0}x_ {2}$$

0/1の積は、すべてが1のときだけ1です。これで「両方選ぶときだけ」を表せます。

第1引数は変数IDの配列で、`PboTerm`の配列ではありません。`{1,2}`はBとCを意味します。第2引数`5`が同時選択時に加算する点数です。もう一方の`-4`は負なので減点になります。普通のソフト行の`weight`は非負ですが、このAPIの`value`は正負を指定できます。

BとCは通常利益13に5点を加えた18です。AとBは17、AとCは16から4点引いた12なので、BとCを選びます。3件以上のIDを渡せば「指定した全件が1」の場合の利益になります。少なくとも1件を選べばよい、という意味ではありません。

このAPIは内部で定数とFixedPenalty行に変換し、目的値にすでに反映します。利用側でもう一度5点を足してはいけません。重複したIDは1回として扱い、空のID配列は常に条件成立、つまり定数利益になります。

戻り値は作られた行IDです。ただし正の利益では定数も組み合わせているため、その行の重みだけを変えても同時選択利益全体の変更にはなりません。値を変更したい場合は、後で学ぶ`rebuild`でモデルを作り直すのが分かりやすい方法です。

## 発展コード11：行を構造体として組み立てる

入力を別の処理で組み立てたいときの書き方です。最小コードの資源行を`PboRow`で直接作り、同じ変数の項が重複した場合も確認します。

保存名：`12_row.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    PboRow capacity;
    capacity.terms = {{0, 1}, {0, 2}, {1, 2}, {2, 2}, {2, 0}};
    capacity.lower = nullopt;
    capacity.upper = 4;
    capacity.kind = PboRowKind::Hard;
    capacity.weight = 1;
    int capacity_id = problem.add_row(capacity);

    PboSolver solver(problem);
    const auto& result = solver.solve();
    if (!result.evaluation.feasible()) return 1;
    cout << capacity_id << '\n';
    cout << result.evaluation.objective << '\n';
}
```

出力：

```text
0
13
```

`PboRow`の5個のフィールドを明示しました。`terms`が項、`lower`と`upper`が境界、`kind`が種類、`weight`が減点の重みです。デフォルトでは項は空、両境界は省略、種類はHard、重みは1です。

変数0には係数1と2の2項があるので合計3、変数2には係数2と0があるので合計2になります。構築後は最小コードと同じ資源行です。係数が合計0になった項は構築時に除かれます。行そのものは残るため、行IDはずれません。

`add_row(capacity)`は行を追加し、行IDを返します。最初の行なので0です。元の`capacity`を書き換えても、追加済みの行は変わりません。`problem.rows`へ直接行を入れることもできますが、追加時にIDを得たいときは`add_row`が便利です。

項が空の行は行和が0です。たとえば空のHard行に下限1を与えると、どの解でも満たせません。空行を勝手に「条件なし」と解釈する仕様ではありません。

## 発展コード12：選択変数を人と仕事の組に対応させる

等式を使う用途を広げます。2人に2件の仕事を1件ずつ割り当てます。新しいAPIは増やさず、変数IDの付け方だけを変えます。

保存名：`13_assignment.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {9, 3, 4, 8};
    for (int person = 0; person < 2; ++person) {
        problem.add_eq({{person * 2, 1}, {person * 2 + 1, 1}}, 1);
    }
    for (int task = 0; task < 2; ++task) {
        problem.add_eq({{task, 1}, {2 + task, 1}}, 1);
    }
    PboSolver solver(problem);
    const auto& result = solver.solve();
    if (!result.evaluation.feasible()) return 1;
    cout << result.evaluation.objective << '\n';
    for (int person = 0; person < 2; ++person) {
        for (int task = 0; task < 2; ++task) {
            if (result.x[person * 2 + task]) cout << person << ' ' << task << '\n';
        }
    }
}
```

出力：

```text
17
0 0
1 1
```

変数IDを`person * 2 + task`としています。掛けている2は仕事の数です。

| ID | 1の意味 | 利益 |
|---:|---|---:|
| 0 | 人0に仕事0を割り当てる | 9 |
| 1 | 人0に仕事1を割り当てる | 3 |
| 2 | 人1に仕事0を割り当てる | 4 |
| 3 | 人1に仕事1を割り当てる | 8 |

最初のループは、各人について担当件数をちょうど1にします。次のループは、各仕事について担当人数をちょうど1にします。係数1は割当件数を数えるため、等式の値1は「ちょうど1」にするためです。どちらか一方を入れ忘れると、意図しない割当を許します。

この単純な1対1割当だけなら、第2節で挙げた厳密な割当法が有力です。ここへ、相性による同時選択利益や複数資源の制約などを追加したくなったとき、共通モデルとしてPboSolverを使う意味が出ます。

## 発展コード13：用意できる候補から始める

最小コードに、Aだけを選ぶ初期候補を追加します。初期候補を渡す位置を明確にするため、既定値の`PboParam`も名前を付けて渡します。

保存名：`14_initial.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4);
    PboSolver solver(problem, 42);
    PboParam param;
    vector<uint8_t> initial = {1, 0, 0};
    auto before = solver.evaluate(initial);
    const auto& result = solver.solve(param, initial);
    if (!result.evaluation.feasible()) return 1;
    cout << before.objective << ' ' << result.evaluation.objective << '\n';
}
```

出力：

```text
10 13
```

`solve(param, initial)`の第1引数が実行条件、第2引数が0/1の初期候補です。`PboParam param;`のままなら、時間1.95秒、絶対締切なし、探索量上限なしという既定値です。初期候補の型と長さは`evaluate`の配列と同じです。

`initial={1,0,0}`はAだけを選ぶ利益10の実行可能解です。同じモデルで実行可能な初期解を渡すと、返される解は実行可能で、目的値はその初期解以上になります。結果が必ず改善するという保証ではなく、同点で終わる場合もあります。

構築時の第2引数`42`は乱数seedです。候補を調べる順序などに使う乱数の初期状態を指定します。42に特別な意味はありません。省略値は1です。同じseedでも実時間で打ち切ると探索量が変わり得るため、常に同じ結果になる保証にはなりません。探索量を固定する方法は発展コード23で説明します。

初期候補はHard行に違反していても渡せます。その場合、実行可能解の発見や初期目的値以上の返却は保証されません。また`solve`は恒久固定値に合わせて内部の候補を補正します。呼出し元の`initial`自体は変更しません。`evaluate(initial)`は補正せず評価するので、両者の動作は異なります。

既存のsolverへ初期候補を渡す場合、その候補だけを残して完全に最初から始めるわけではありません。以前の保存解も比較対象です。完全にやり直す操作は後で学びます。

## 発展コード14：結果を保存し、続きを探索する

初期候補を作る代わりに、同じsolverで`solve`を2回呼びます。新しいポイントは、返却値をコピーして保存することです。

保存名：`15_continue.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4);
    PboSolver solver(problem);
    PboParam param;
    param.time_limit_us = 10'000;

    auto first = solver.solve(param);
    const auto& second = solver.solve(param);
    if (!first.evaluation.feasible() || !second.evaluation.feasible()) return 1;
    cout << first.evaluation.objective << ' ' << second.evaluation.objective << '\n';
}
```

出力：

```text
13 13
```

`time_limit_us=10'000`は、各呼出しに10,000マイクロ秒、つまり10ミリ秒を与えます。数字中の`'`はC++の桁区切りで、数値には影響しません。2回の合計で10ミリ秒、という指定ではありません。

同じsolverの`solve(param)`を初期候補なしで再度呼ぶと、現在の候補・保存した最良候補・乱数状態などを再利用します。今回の小さな例は1回目で最適解に達するため、2回目も13です。大きな問題でも改善しない場合はあります。

返却型は`const PboResult&`で、solver内部の結果への参照です。`const auto& second`はその結果を直接読み、`auto first`は`PboResult`全体をコピーします。コピーには`x`の配列も含みます。

| 保持方法 | 用途 |
|---|---|
| `const auto& result`で受ける | 次のsolverの非const操作まで、その場で読む |
| `auto saved`で受ける | 次のsolve、improve、setter、rebuildなどをまたいで保存する |

参照を取った後に次の`solve`を呼ぶと、同じ内部結果の内容が更新されます。過去の解が必要なら、操作前にコピーしてください。読み取り専用の`evaluate`は探索状態を変更しません。

モデルを変えずに継続し、すでに実行可能解があれば、保存解の目的値は悪化しません。時間・探索量のカウンタは各呼出しで数え直します。温度の位相なども呼出しごとに始まるため、「10ミリ秒を2回」と「20ミリ秒を1回」が同じ探索経路になるとは限りません。

## 発展コード15：指定した変数だけ変更する

大域探索の結果とは別に、Aを残したままBとCだけ見直します。新しいAPIは`improve`です。

保存名：`16_improve.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4);
    PboSolver solver(problem);
    auto global = solver.solve();

    vector<uint8_t> initial = {1, 0, 0};
    vector<int> free_variables = {1, 2};
    PboParam param;
    auto local = solver.improve(initial, free_variables, param);
    if (!global.evaluation.feasible() || !local.evaluation.feasible()) return 1;
    cout << global.evaluation.objective << ' ' << local.evaluation.objective << '\n';
    cout << int(local.x[0]) << ' ' << int(local.x[1]) << ' ' << int(local.x[2]) << '\n';
}
```

出力：

```text
13 10
1 0 0
```

引数は順に、初期候補、変更を許す変数IDの配列、実行条件です。`solve`とは引数の順序が違います。第3引数`param`は省略できますが、この例では位置を分かりやすくするため書いています。

`free_variables={1,2}`はBとCだけを変更可能にします。Aは渡した`initial`の値1に固定されます。残り資源は1なので、資源2を使うBもCも追加できず、部分探索の結果は利益10です。

以前の大域的な最良解13は、Aが0なので今回の部分問題の条件を守りません。`improve`はその過去解を結果に混ぜず、指定した初期候補から新しい部分探索を始めます。**旧探索状態を置き換える**ため、旧最良解を残すならこの例の`global`のようにコピーしておきます。

恒久固定の変数は、`free_variables`に入っていても動かせません。また、`improve`に渡す初期候補は恒久固定を満たす必要があります。`solve`と違って固定違反の初期候補を許す仕様ではありません。

IDの重複は許され、空配列なら何も変更せず初期候補を評価します。部分探索の範囲はその呼出しだけの指定です。次の通常の`solve`では、恒久固定以外の全変数が再び変更可能になります。ただし古い大域最良解は自動復活しません。

## 発展コード16：利益が変わったモデルを引き続き解く

最小コードを解いた後、Aの利益を20、定数項を100に変更します。solverを作り直さずに使うsetterを初めて導入します。

保存名：`17_update_profit.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4);
    PboSolver solver(problem);
    auto previous = solver.solve();

    solver.set_profit(0, 20);
    solver.set_constant(100);
    auto reevaluated = solver.evaluate(previous.x);
    const auto& result = solver.solve();
    if (!result.evaluation.feasible()) return 1;
    cout << previous.evaluation.objective << ' ' << reevaluated.objective
         << ' ' << result.evaluation.objective << '\n';
}
```

出力：

```text
13 113 120
```

`set_profit(0,20)`の第1引数は変数ID、第2引数は新しい利益です。20を加算するのではなく、利益を20に置き換えます。`set_constant(100)`も定数を100に置き換える操作です。

コピー済みの`previous.evaluation.objective`は旧モデルでの13のままです。`evaluate(previous.x)`は直ちに新しいモデルを使うため、BとCの利益13に定数100を足した113になります。その後の`solve`ではAだけを選んだ120が得られます。

setterは主に値と「変更された」という印を更新します。次の探索時に、現在候補と保存候補を新しいモデルで評価し直します。setterを何個かまとめて呼んでも、1個ごとに探索用の全評価をするわけではありません。`evaluate`を明示的に呼べば、その呼出し分の全評価は行います。

モデル変更後は、旧モデルの目的値との大小に意味がない場合があります。比較したい候補をすべて同じ新しいモデルで評価してください。`set_constant`は、`add_all_profit`が内部で足した分も含む定数全体を置き換えます。同時選択利益を使ったモデルの基準点変更では、この点に注意します。

## 発展コード17：境界や減点方式を変更する

行IDを保存し、容量4を5へ変更します。その後、同じ行をソフト制約へ変え、最後にHardへ戻します。

保存名：`18_update_row.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    int capacity_id = problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4);
    PboSolver solver(problem);
    cout << solver.solve().evaluation.objective << '\n';

    solver.set_row_bounds(capacity_id, nullopt, 5);
    cout << solver.solve().evaluation.objective << '\n';
    solver.set_row_penalty(capacity_id, PboRowKind::LinearPenalty, 2);
    cout << solver.solve().evaluation.objective << '\n';
    solver.set_row_penalty(capacity_id, PboRowKind::Hard);
    cout << solver.solve().evaluation.objective << '\n';
}
```

出力：

```text
13
17
19
17
```

`capacity_id`は行追加時の戻り値で、このモデルでは0です。境界や減点方式を変えても行IDは変わりません。

`set_row_bounds`の引数は、行ID、新しい下限、新しい上限です。`nullopt,5`は「下限なし、上限5」です。`nullopt`は「前の値のまま」ではなく「その境界をなくす」指定です。境界の変更で係数・行の種類・重みは変わりません。

`set_row_penalty`の引数は、行ID、新しい種類、新しい重みです。`LinearPenalty,2`では超過1単位につき2点を引きます。3件選ぶと使用量7、超過2、利益23から4点引いて19です。重みを省略すると1になります。Hardに戻した最後の呼出しでは、重みは目的値に使いません。

どの結果も、その行の新しい意味で評価されています。ソフト化の直前にHardで実行可能だったからといって、その逆も成り立つわけではありません。実用コードで条件を厳しくした場合は、新しい`feasible()`を確認します。

## 発展コード18：既存の項の係数を変更する

資源行にあるAの係数を3から1、次に0、最後に3へ戻します。新しいAPIは`set_coefficient`です。

保存名：`19_update_coefficient.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    int capacity_id = problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4);
    PboSolver solver(problem);
    cout << solver.solve().evaluation.objective << '\n';
    solver.set_coefficient(capacity_id, 0, 1);
    cout << solver.solve().evaluation.objective << '\n';
    solver.set_coefficient(capacity_id, 0, 0);
    cout << solver.solve().evaluation.objective << '\n';
    solver.set_coefficient(capacity_id, 0, 3);
    cout << solver.solve().evaluation.objective << '\n';
}
```

出力：

```text
13
17
23
13
```

引数は、行ID、変数ID、新しい係数です。第2引数の0はAを指し、第3引数が1、0、3と変わります。係数1ならAとBを一緒に選べ、係数0ならAを選んでも資源を消費しないため全件選べます。

変更できるのは、**構築時の集約後に存在する項だけ**です。構築時から存在しない変数をその行へ新しく追加する操作には使えません。

| 状況 | このsetterで変更できるか |
|---|---|
| 構築時に非ゼロ係数として残った項 | できる |
| その項をsetterで0にした後、再び非ゼロにする | できる |
| 構築時に係数0しかなく、除かれた項 | できない |
| 重複項が構築時に打ち消し合って合計0になった項 | できない |
| その行に一度もなかった変数 | できない |

たとえば最初から`{0,0}`だけを置いて、新しい接続用の予約として使うことはできません。項の追加・削除が必要なら、発展コード20の`rebuild`を使います。

## 発展コード19：確定した変数を固定し、後で解除する

発展コード07ではモデル構築時に固定しました。今度は構築後に`set_fixed`で変更します。

保存名：`20_update_fixed.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4);
    PboSolver solver(problem);
    cout << solver.solve().evaluation.objective << '\n';
    solver.set_fixed(0, 1);
    cout << solver.solve().evaluation.objective << '\n';
    solver.set_fixed(0, -1);
    cout << solver.solve().evaluation.objective << '\n';
    solver.set_fixed(2, 0);
    cout << solver.solve().evaluation.objective << '\n';
}
```

出力：

```text
13
10
13
10
```

第1引数は変数ID、第2引数は`-1`、`0`、`1`のいずれかです。

`set_fixed(0,1)`でAを選択に固定すると、資源の都合でB・Cを追加できず10点になります。`set_fixed(0,-1)`でAを自由へ戻すと、再びB・Cの13点が可能です。`set_fixed(2,0)`でCを禁止すると、Aの10点が最良になります。

setter直後の保存済み結果を新しい解として使うのではなく、`solve`を呼んで固定値を反映し、再評価・再探索します。固定によって条件全体が矛盾することもあります。その場合は実行可能解を返せません。

恒久固定は解除するまで後の呼出しにも残ります。一度だけ変更範囲を絞る`improve`とは、効力の続く範囲が異なります。

## 発展コード20：行や変数の構造を作り直す

係数の値だけでなく、新しい行を追加します。その後、変数を1個増やす場合も示します。新しいAPIは`rebuild`です。

保存名：`21_rebuild.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {10, 7, 6};
    problem.add_le({{0, 3}, {1, 2}, {2, 2}}, 4);
    PboSolver solver(problem);
    cout << solver.solve().evaluation.objective << '\n';

    problem.add_le({{1, 1}, {2, 1}}, 1);
    solver.rebuild(problem, true);
    cout << solver.solve().evaluation.objective << '\n';

    PboProblem expanded = problem;
    expanded.profit.push_back(12);
    expanded.rows[0].terms.push_back({3, 1});
    solver.rebuild(expanded, false);
    const auto& result = solver.solve();
    if (!result.evaluation.feasible()) return 1;
    cout << result.x.size() << ' ' << result.evaluation.objective << '\n';
}
```

出力：

```text
13
10
4 22
```

最初に追加した行はBとCの同時選択禁止です。元の`problem`を変更するだけではsolverに反映されないので、`rebuild(problem,true)`で新しいモデルを取り込みます。

第1引数は新しいモデル、第2引数`keep_solution`は候補を引き継ぐかどうかです。省略するとtrueです。trueでは、以前の保存候補と現在候補を新モデルで評価し直して利用します。**変数数と各変数IDの意味が同じ**であることが条件です。変数数だけ同じでも、IDの意味を並べ替えたならfalseにします。

行IDは新しいモデルでの追加順に従います。以前保存した行IDを、新モデルでも同じ意味だと決めつけないでください。

次に利益12の変数3を増やし、資源行へ係数1の項を追加します。`expanded.rows[0]`は新モデルの資源行です。変数数が3から4に変わったため、`rebuild(expanded,false)`で旧候補を破棄します。Aと新候補を選ぶと資源4、利益22です。この例は`fixed`を省略しているため、新変数も自由です。固定配列を使うモデルなら、その長さも新変数数に合わせます。

行・項の追加削除、変数数や意味の変更、`add_all_profit`の値変更などは、モデルを作り直すこの方法で扱えます。falseを指定しても乱数seedは自動リセットされません。乱数を戻したければ、後の`reseed`も使います。

## 発展コード21：探索しない呼出しと、実行可能性の確認

新しい実行条件として時間予算0を使います。説明のため、1変数に矛盾する必須条件を与え、返却値を「成功した解」と誤解しないことも確認します。

保存名：`22_no_search.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    problem.profit = {5};
    problem.add_ge({{0, 1}}, 1);
    problem.add_le({{0, 1}}, 0);
    PboSolver solver(problem);
    PboParam param;
    param.time_limit_us = 0;
    const auto& result = solver.solve(param);
    cout << result.iterations << '\n';
    cout << result.evaluation.feasible() << ' '
         << result.evaluation.violated_hard_rows << '\n';
    if (!result.evaluation.feasible()) cout << "no feasible solution found\n";
}
```

出力：

```text
0
0 1
no feasible solution found
```

`time_limit_us=0`では探索を進めません。ただし必要な初期化、固定値の反映、モデル更新後の再評価などは実行します。実行時間もメモリ処理も完全に0になるわけではありません。

初回で初期候補を省略したので、固定値を反映した全0候補が評価されます。この例には固定がなく、変数0は0です。下限1の行には違反し、上限0の行は満たすため、違反行数は1になります。`iterations`は今回の探索量で、この例では0です。

このモデルは、変数0を同時に1以上と0以下にするよう要求しているため、人が式を見れば不可能と分かります。しかし、APIの`feasible()==false`が一般に証明を表すわけではありません。時間不足、初期候補不足などで見つからなかっただけのモデルでもfalseになります。

実行可能解を見つけた場合は、発見済み実行可能解の中で目的値が最大の候補を返します。未発見の場合は、主に「Hard違反行数が少ない → 正規化した違反量が小さい → 目的値が大きい」の順で候補を保存します。公開結果に違反量そのものや最適性・不可能性の証明フラグはありません。

単に特定の配列を検査したいなら`evaluate`が簡単です。時間0の`solve`は、探索状態の準備や再評価まで行いたい場合の操作です。`max_iterations=0`でも探索を禁止できますが、初期化は同様に必要です。

## 発展コード22：呼出し予算と、全体の締切を両方守る

継続探索の例を24変数へ広げ、1回3ミリ秒、全体20ミリ秒を目安に繰り返します。絶対時刻の`deadline`と結果の計測値を追加します。

保存名：`23_deadline.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    using Clock = chrono::steady_clock;
    auto deadline = Clock::now() + chrono::milliseconds(20);

    PboProblem problem;
    vector<PboTerm> terms;
    for (int i = 0; i < 24; ++i) {
        problem.profit.push_back(i % 7 + 1);
        terms.push_back({i, 1});
    }
    problem.add_le(terms, 8);
    PboSolver solver(problem, 1);
    PboParam param;
    param.time_limit_us = 3'000;
    param.deadline = deadline;

    PboResult saved;
    int calls = 0;
    do {
        saved = solver.solve(param);
        ++calls;
    } while (Clock::now() < deadline);

    if (!saved.evaluation.feasible()) return 1;
    cout << "feasible=" << saved.evaluation.feasible() << '\n';
    cout << "objective=" << saved.evaluation.objective << '\n';
    cout << "calls=" << calls << '\n';
    cout << "last_iterations=" << saved.iterations << '\n';
    cout << "last_elapsed_us=" << saved.elapsed_us << '\n';
}
```

この例は実時間で打ち切るため、呼出し回数や探索量、経過時間は実行ごとに変わります。`feasible=1`となり、目的値は0以上49以下です。49は利益の大きい8件を選んだ場合の上限で、例の正しさに49の取得を必須とはしていません。

問題は、利益1から7が繰り返す24件から最大8件を選ぶだけです。各項の係数1は件数を数え、上限8は選べる件数を指定します。小さな3変数例と違い、時間を使う探索の呼出し方を体験するために変数を増やしています。

| 指定 | このコードでの意味 |
|---|---|
| `Clock=chrono::steady_clock` | 経過時間や締切を測る時計。時刻を戻す時計調整の影響を受けにくい |
| `milliseconds(20)` | `deadline`を決めた瞬間から20ミリ秒後を共通締切とする |
| `time_limit_us=3'000` | 各`solve`の入口から3ミリ秒の相対予算 |
| `param.deadline=deadline` | すべての呼出しへ同じ絶対締切を渡す |
| seedの`1` | 乱数の初期値。既定値と同じ |

相対予算と絶対締切が両方あると、早い方が適用されます。締切はモデル構築より前に決めているため、構築に使った時間も全体20ミリ秒に含まれます。`do-while`にしたのは、初期化と評価を少なくとも1回行って`PboResult`を得るためです。

`saved`へは各回の返却結果をコピーします。`iterations`は最後の呼出しで進めた探索量、`elapsed_us`は最後の呼出し内で計測した経過マイクロ秒です。全呼出しの合計値ではありません。コピーや出力など、`solve`の外側の処理は`elapsed_us`に含みません。

1ミリ秒は1,000マイクロ秒です。`time_limit_us=1'950'000`なら1,950ミリ秒です。単位を取り違えると1,000倍変わるので、値を設定するときに確認してください。型は`double`です。

時間判定は常時ではなく、初期評価・巻戻し・終了処理も必要なので、締切を厳密に超えない保証はありません。AHCの全体制限に合わせる際は、構築・評価・出力の時間を考慮して余裕を取ります。相対予算は通常の競技用途を想定し、1日を超える指定は対象外です。

`time_limit_us=-1`で相対予算を外すこともできます。その場合は、絶対締切または有限の探索量上限を必ず指定します。`-1`以外の負値、NaN、無限大は指定しません。

## 発展コード23：探索量を固定し、同じ条件からやり直す

前の24変数モデルを使い、時間ではなく探索量で止めます。新しく`max_iterations`、`reset_search`、`reseed`を使います。

保存名：`24_reproducible.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    vector<PboTerm> terms;
    for (int i = 0; i < 24; ++i) {
        problem.profit.push_back(i % 7 + 1);
        terms.push_back({i, 1});
    }
    problem.add_le(terms, 8);
    PboSolver solver(problem, 123);
    PboParam param;
    param.time_limit_us = -1;
    param.max_iterations = 2'000;
    auto first = solver.solve(param);

    solver.reset_search();
    solver.reseed(123);
    auto second = solver.solve(param);
    bool same = first.x == second.x
             && first.evaluation.objective == second.evaluation.objective
             && first.evaluation.violated_hard_rows == second.evaluation.violated_hard_rows
             && first.iterations == second.iterations;
    cout << "same=" << same << '\n';
    if (!same || !first.evaluation.feasible() || !second.evaluation.feasible()) return 1;
}
```

出力：

```text
same=1
```

`time_limit_us=-1`で相対時間による打切りを外し、`max_iterations=2'000`で探索量を最大2,000にします。`deadline`は既定の未指定のままです。2,000はこの例の比較用の上限で、推奨チューニング値という意味ではありません。

`max_iterations`の単位は、局所探索の外側の1試行、または厳密探索の1訪問ノードです。反転回数でも目的関数の評価回数でもありません。0なら探索なし、-1なら探索量上限なしです。時間条件と併用すれば早い方で止まります。

`reset_search()`は、問題モデルを残して探索状態を破棄する指定です。乱数系列は戻しません。`reseed(123)`は乱数系列をseed 123へ戻す指定で、こちらだけでは現在候補や保存解を破棄しません。この例では両方を行い、同じ初期条件からやり直しています。

`same`では解配列、整数目的値、Hard違反行数、探索量を比較します。所要時間はOSなどで変わるため比較しません。同じライブラリ版、入力、呼出し順、seed、コンパイラ環境、探索量をそろえれば、再現比較に使えます。版や環境を変えた場合の一致を保証するものではありません。

新しいsolverを同じモデルとseedで作り直す方法でも状態をそろえられます。探索状態を残したままseedだけ戻すと、この比較にはなりません。

## 発展コード24：疎なモデルを組み立て、更新と部分探索をつなぐ

最後は学んだAPIを組み合わせます。60件の候補がそれぞれ2種類の資源を使うモデルを作り、利益変更後に再評価し、一部の候補だけ見直します。新しい探索APIは追加しません。

保存名：`25_combined.cpp`

```cpp
#include "pbo_solver_v06.hpp"
using namespace std;

int main() {
    PboProblem problem;
    vector<vector<PboTerm>> resource_terms(6);
    for (int i = 0; i < 60; ++i) {
        problem.profit.push_back(i % 11 + 1);
        resource_terms[i % 6].push_back({i, 2});
        resource_terms[(i + 1) % 6].push_back({i, 1});
    }
    for (int resource = 0; resource < 6; ++resource) {
        problem.add_le(resource_terms[resource], 10);
    }
    problem.add_all_profit({0, 6}, 5);
    PboSolver solver(problem, 1);
    PboParam param;
    param.time_limit_us = -1;
    param.max_iterations = 2'000;
    vector<uint8_t> initial(60, 0);
    auto first = solver.solve(param, initial);
    if (!first.evaluation.feasible()) return 1;

    solver.set_profit(0, 30);
    PboParam refresh;
    refresh.time_limit_us = 0;
    auto refreshed = solver.solve(refresh, first.x);
    if (!refreshed.evaluation.feasible()) return 1;

    vector<int> free_variables;
    for (int i = 0; i < 12; ++i) free_variables.push_back(i);
    auto local = solver.improve(refreshed.x, free_variables, param);
    auto checked = solver.evaluate(local.x);
    bool valid = checked.feasible()
              && checked.objective == local.evaluation.objective
              && local.evaluation.objective >= refreshed.evaluation.objective;
    for (int i = 12; i < 60; ++i) valid = valid && local.x[i] == refreshed.x[i];
    cout << "valid=" << valid << '\n';
    if (!valid) return 1;
}
```

出力：

```text
valid=1
```

最初のループで60個の変数を作ります。利益は1から11の繰り返しです。各候補$i$は資源$i\bmod6$を2単位、次の資源を1単位使います。`%6`は6種類の資源IDへ循環させるためです。各行の上限は10です。全変数と全資源の組合せを書かず、実際に使う2項だけを追加している点が疎な表現です。

`add_all_profit({0,6},5)`は候補0と6の同時選択に5点を加えます。seedは1、探索量は2,000、相対時間上限はありません。初期候補は長さ60の全0です。このモデルでは資源条件を守る実行可能解です。

途中で`set_profit(0,30)`により候補0の単独利益を30へ変更します。過去の`first.evaluation`は古い利益の値なので、そのまま比較しません。時間0の`solve(refresh,first.x)`で再評価した候補を得ます。ここでは初期候補も明示していますが、更新後の現在候補・保存候補も新しいモデルで比較されます。

変更できる範囲をID 0から11の12変数に限定して`improve`します。ID 12から59は`refreshed.x`の値を保ちます。12はこの例で見直す範囲のサイズで、APIの上限ではありません。

最後に次の3点を確認しています。

1. `evaluate`で独立に計算し直しても、実行可能で目的値が一致する。
2. 同じ新モデルでの部分探索なので、実行可能な初期候補より目的値が悪化しない。
3. 変更を許していない変数は同じ値のままである。

実際のAHCでは、発展コード22のように全体締切を設定します。固定探索量はこの例の再実行をしやすくするためです。全体の最良解を別に管理する場合は、モデル変更時にその解も再評価し、`improve`の前に必要な結果をコピーします。

## 4. 学んだAPIを探すための索引

ここまでで公開されている型・フィールド・メソッドを一通り使いました。以下は復習用です。新しいAPIをまとめて覚える必要はありません。

### 4.1 問題の定義

| 型・メンバ | 型または引数の順序 | 使う節 |
|---|---|---|
| `PboTerm` | `int var`、`long long coef` | 最小コード |
| `PboProblem.profit` | `vector<long long>`。長さが変数数 | 最小コード、発展06 |
| `PboProblem.rows` | `vector<PboRow>` | 発展11、20 |
| `PboProblem.fixed` | `vector<int8_t>`。空または変数数と同じ長さ | 発展07 |
| `PboProblem.constant` | `long long`。既定0 | 発展06 |
| `PboRow.terms` | `vector<PboTerm>` | 発展11 |
| `PboRow.lower`、`upper` | 各`optional<long long>`。既定は境界なし | 発展04、11 |
| `PboRow.kind` | `PboRowKind`。既定Hard | 発展08、09、11 |
| `PboRow.weight` | `long long`。既定1 | 発展08、09、11 |
| `add_row` | 行 | 発展11 |
| `add_le` | 項、上限、種類=Hard、重み=1 | 最小コード、発展08 |
| `add_ge` | 項、下限、種類=Hard、重み=1 | 発展02 |
| `add_eq` | 項、目標値、種類=Hard、重み=1 | 発展03 |
| `add_range` | 項、下限、上限、種類=Hard、重み=1 | 発展04 |
| `add_all_profit` | 変数IDの配列、正負の利益 | 発展10 |

すべての`add_...`メソッドは`int`の行IDを返します。変数IDは`profit`内の位置、行IDは`rows`内の位置です。どちらも0始まりです。

### 4.2 実行・結果・更新

| API | 引数・動作 | 使う節 |
|---|---|---|
| `PboSolver` | モデル、seed=1 | 最小コード、発展13 |
| `solve` | 実行条件={}、初期候補={}。`const PboResult&`を返す | 最小コード、発展13、14 |
| `improve` | 初期候補、変更可能ID配列、実行条件={}。同じ返却型 | 発展15 |
| `evaluate` | 候補の0/1配列。`PboEvaluation`を値で返す | 発展01 |
| `set_profit` | 変数ID、新しい利益 | 発展16 |
| `set_constant` | 新しい定数 | 発展16 |
| `set_row_bounds` | 行ID、新しい下限、新しい上限 | 発展17 |
| `set_row_penalty` | 行ID、新しい種類、重み=1 | 発展17 |
| `set_coefficient` | 行ID、変数ID、新しい既存係数 | 発展18 |
| `set_fixed` | 変数ID、-1/0/1 | 発展19 |
| `rebuild` | 新しいモデル、keep_solution=true | 発展20 |
| `reset_search` | 引数なし。モデルを保ち探索を初期化待ちにする | 発展23 |
| `reseed` | `uint64_t`のseed。乱数だけ戻す | 発展23 |

| 結果の型・メンバ | 内容 |
|---|---|
| `PboResult.x` | `vector<uint8_t>`。返された選択値 |
| `PboResult.evaluation` | `PboEvaluation`。返された候補の評価 |
| `PboResult.iterations` | `int64_t`。今回の探索量 |
| `PboResult.elapsed_us` | `double`。今回の経過マイクロ秒 |
| `PboEvaluation.objective` | `long long`。真の目的値 |
| `PboEvaluation.violated_hard_rows` | `int`。Hard違反行数 |
| `PboEvaluation.violated_fixed_variables` | `int`。恒久固定への違反変数数 |
| `PboEvaluation.feasible()` | 両違反数が0かを返す |

`solve`と`improve`が返す候補は恒久固定を満たすため、通常その`violated_fixed_variables`は0です。任意の外部候補をそのまま調べる`evaluate`では、0でない値も出ます。

### 4.3 実行条件の全項目

| `PboParam`のフィールド | 型 | 既定値 | 省略・特別値の意味 |
|---|---|---|---|
| `time_limit_us` | `double` | `1'950'000.0` | 0で探索なし、-1で相対時間上限なし |
| `deadline` | `optional<steady_clock::time_point>` | 未指定 | 指定すると相対予算と早い方で止める |
| `max_iterations` | `int64_t` | `-1` | -1で探索量上限なし、0で探索なし |

この3項目がすべてです。公開パラメータに温度、候補数、連鎖深さ、外部RNG、評価関数コールバックはありません。相対時間を無制限にするときは、締切または非負の探索量上限が必要です。

### 4.4 最初につまずきやすい対応

| 状況 | 確認すること |
|---|---|
| 「利益が高いのに使えない」 | `feasible()`も確認する。Hard違反は目的値から自動減点されない |
| 「減点条件を破ったのにfeasibleがtrue」 | ソフト行の違反は実行可能性を損なわない |
| 「元のproblemを書き換えたのに結果が変わらない」 | solverは別のモデルを所有する。setterかrebuildを使う |
| 「前の解を保存したつもりが変わった」 | 参照ではなく値でコピーする |
| 「set_coefficientで追加できない」 | 既存の正規化済み項だけを変更できる。新しい項はrebuild |
| 「初期解を渡したのに以前の解が返る」 | solveは過去の保存解も使う。独立に始めるならreset_search |
| 「improve後に大域最良解がなくなった」 | improveは新しい部分探索。旧解を事前にコピーする |
| 「seedが同じなのに結果が違う」 | 時間、探索状態、モデル、呼出し順も確認する |

### 4.5 コード例とMarkdownの検証

このガイドから抽出した25本のC++コードを、それぞれ独立にコンパイル・実行しました。24本の固定出力は記載どおりに一致し、実時間例も実行可能性と目的値の範囲を確認しました。指定の警告オプションで警告はありませんでした。結果は同梱の`validation.txt`に記録しています。

使用した環境はGCC 13.3.0、C++20、`-O2`です。対象のGCC 12.2はこの検証環境にないため、12.2での実行確認はしていません。例はC++20の機能の範囲で記述しています。

163個の数式について、通常のMarkdown処理後にも式の文字列が保持されることを確認しました。表示式15個はすべて1行です。HTMLタグや独自アンカーは使っていません。すべての外部リーダーにおける表示を保証するものではないので、利用するリーダーの数式機能を有効にしてください。

同梱のヘッダは対象のv06をそのまま収録し、ガイド作成のための本体変更は行っていません。

## 付録A. 実装アルゴリズムの概要と流れ

ここから内部の仕組みを説明します。まず全体像を見て、その後で各処理を掘り下げます。公開APIの使い方だけが目的なら、必要なときに戻って読む形でも構いません。

### A.1 全体で何を組み合わせているか

この実装は、疎な差分更新、小規模な分枝限定、単独反転からの修復連鎖、焼きなまし、違反条件の重み調整、停滞時の部分修復と再出発を組み合わせています。

1. 構築時に項を整理し、「行から項」「変数から関係する行」の両方向をすぐ読める形にする。
2. 呼出しの入口で時間条件を決め、必要ならモデル変更を反映し、現在候補・保存候補を評価する。
3. 恒久固定と部分探索の指定から、今回変更できる変数を決める。
4. 各Hard行について、変更可能変数を使えば境界へ届くかを簡単に調べる。
5. 変更可能な変数が18個以下なら、まず分枝限定で小規模な厳密探索を行う。
6. それで探索を終えられなければ、候補変数を選び、0/1反転と修復の連鎖を提案する。
7. 良い途中候補を保存し、連鎖全体の利得に応じて受理または巻き戻しを行う。
8. 停滞やHard違反が続くと、重み調整、部分的な厳密修復、保存解からの再出発を使う。
9. 時間・探索量などで終了し、現在候補そのものではなく、保存している最良候補を返す。

探索中の候補は一時的にHard条件を破ることがあります。条件を破らない1変数反転だけに限定すると、等式や割当では必要な交換ができなくなるためです。使える解かどうかは、保存と返却時に区別します。

### A.2 真の目的値と、探索を動かす点数を分ける

真の目的値は本文の$F(x)$で、整数として計算します。一方、次に何を変えるかを決める内部点数には、利益の大きさを調整した値とHard違反への罰を使います。

たとえば単独で1件追加すると利益が増えても、等式を破ることがあります。逆に、利益を少し失えば複数の違反を直せることもあります。内部点数は、その比較を進めるための道具です。

内部点数が高いことと、返却する実行可能解の目的値が高いことは同一ではありません。保存解の選択には、実行可能性と真の整数目的値を使います。

## 付録B. データ構造と差分更新

### B.1 項の正規化と、行・列の連続配置

構築時に各行の項を変数ID順に並べ、同じ変数の係数を足し合わせます。合計0になった項は除きます。行自体は除かず、行IDを維持します。

整理後の項は`terms_`へ連続して置きます。`row_begin_`が、各行の項の開始位置と終了位置を示します。行を調べるときは、その区間だけを読みます。

変数を反転するときは、その変数を含む行だけが変わります。そこで変数ごとの区間`col_begin_`を作り、関係する行IDを`column_row_`、係数を`column_coef_`へ連続して置きます。係数と行IDを別配列にした配置です。`col_pos_`は、行側の項から列側の位置をたどり、係数setterで両方を更新するために使います。

これにより、たとえば変数7が3行だけに登場するなら、反転時に全行を走査する必要はなく、その3行だけを更新できます。変数数が多くても、接続が少ないモデルで効果があります。

公開APIの省略境界は`nullopt`ですが、内部では下限省略を`LLONG_MIN`、上限省略を`LLONG_MAX`として保持します。違反量は、値をmax/minで挟んでから差を取ります。利用者が自分で極端な値を入れて境界なしを表す必要はありません。

### B.2 現在候補に対して保持している値

| 内部の値 | 役割 |
|---|---|
| `x_` | 今調べている候補 |
| `sums_` | 各行の現在の合計値 |
| 各行の`amount` | 各行の現在の違反量 |
| `linear_` | 定数と単独利益だけの合計 |
| `objective_` | ソフト行の減点を引いた真の目的値 |
| `bad_rows_`、`bad_pos_` | 違反中のHard行と、その配列内の位置 |
| `movable_`、`free_` | 今回変更できる変数 |
| `locked_` | 1本の連鎖中にすでに反転した変数 |
| `best_` | 返却用に保存している最良候補 |

Hard違反が解消した行を集合から除くときは、末尾の行と入れ替えて削除します。位置も更新することで、集合の追加・削除を定数時間で行います。集合のための木やハッシュ表は使っていません。

候補全体を読み込む`load_state`では、恒久固定を反映し、行和・違反量・目的値などを作り直します。反転時はその差分だけを更新します。違反量のキャッシュも、この2つの経路で整合させます。

### B.3 1変数を反転したときの計算

変数$i$の0/1を入れ替える変化量を$b$とします。

$$b=1-2x_ {i}$$

現在0なら$b=1$、現在1なら$b=-1$です。その変数を含む行$j$の新しい合計値は次です。

$$s'_ {j}=s_ {j}+a_ {ji}b$$

$s'_ {j}$は変更後、$s_ {j}$は変更前の行和です。係数が負でも同じ式で更新できます。

単独利益の変化は$p_ {i}b$です。各ソフト行については「古い減点−新しい減点」を足すと、目的値の差が得られます。Hard行は目的値に入れず、違反集合と探索用違反量を更新します。

`delta`はこの差を見積もり、`delta<true>`は同じ走査で実際の反転も行います。`flip`は適用する側の呼出しです。変更前の違反量は各行の`amount`から読み、毎回計算し直しません。候補評価と適用で同じ計算式を共有しています。

反転にかかる時間は、その変数が現れる項数に比例します。ただし探索中に多数の候補を比較するため、1反復の処理量が1反転分だけとは限りません。

### B.4 同時選択利益を、定数と減点で表す

`add_all_profit`は新しい0–1変数を増やしません。重複を除いた指定変数の数を$k$、それらの選択数の合計を$z$、加算したい正負の利益を$q$とします。すべて選んだときだけ$z=k$です。

正の利益$q$なら、定数項へ$q$を足し、「$z\ge k$を破ったら$q$点減点する」FixedPenalty行を作ります。全件選択なら減点なしで$q$が残り、1件でも欠けると$q$を引くので正味0です。

負の利益$q$なら、「$z\le k-1$を破ったら$-q$点減点する」FixedPenalty行を作ります。全件選択のときだけ境界を超え、正味$q$になります。たとえば$q=-4$なら4点を引くことになります。

指定集合が空なら$k=z=0$です。正の利益では下限0を満たして定数が残り、負の利益では上限-1を必ず破って定数分の減点になります。どちらも空集合の「全件選択」は常に成立する、という仕様に対応します。

これが、同時選択利益を変えるときに減点行の重みだけでは不十分な場合がある理由です。利用者が定義した利益と、内部の定数・行を一体として扱う必要があります。

## 付録C. 評価尺度と保存解

### C.1 Hard違反の単位をそろえる

1単位の不足が重要な行と、係数が何万単位もある行を、そのまま比較すると数値の大きい行だけが支配しがちです。そのため、行ごとの係数の大きさを使って、探索用の違反量を正規化します。

行$j$の非ゼロ係数の絶対値合計を$A_ {j}$、非ゼロ項数を$k_ {j}$として、尺度$r_ {j}$を次で定めます。

$$r_ {j}=\frac{1}{\max(1,A_ {j}/\max(1,k_ {j}))}$$

分母は概ね係数の平均の大きさです。0で割ったり、係数が小さい行を過剰に増幅したりしないよう、1との最大値を取ります。係数をsetterで0にした項は構造として残りますが、非ゼロ項数には数えません。

保存解の比較に使う正規化違反量は、Hard行について次を足した値です。

$$V(x)=\sum_ {j\in H}r_ {j}d_ {j}(x)$$

$H$はHard行の集合、$d_ {j}$は本文で説明した違反量です。真の行和や実行可能性判定は整数のままで、この正規化で条件の意味は変わりません。

### C.2 候補選択用の利得

目的値にも尺度$\alpha$を掛けます。利益の絶対値合計へ、LinearPenalty行なら「重み×係数絶対値合計」、FixedPenalty行なら「重み×非ゼロ項数」を足したものを$S$とし、次を使います。

$$\alpha=\frac{1}{\max(1,S/\max(1,n))}$$

$n$は変数数です。Hard行の重みは$S$に入りません。定数項も、反転で変わらないためこの尺度の対象ではありません。

Hard行ごとの探索用重みを$h_ {j}$とすると、1回の提案に対する探索用利得$\Delta G$は次です。

$$\Delta G=\alpha\Delta F-\sum_ {j\in H}h_ {j}r_ {j}\Delta d_ {j}$$

$\Delta F$は目的値の「変更後−変更前」、$\Delta d_ {j}$は違反量の「変更後−変更前」です。違反が減ると$\Delta d_ {j}$が負になるので、利得にはプラスに働きます。$h_ {j}$は初期値1で、違反が続くと後述の規則で調整します。

この計算はdoubleです。返却する目的値はlong longの整数であり、探索用重みを掛けた値を返すわけではありません。

### C.3 保存解の順序

`remember`は、候補を次の順で比較します。

1. Hard違反行数が少ない方。
2. 行数が同じなら、正規化違反量$V(x)$が小さい方。
3. それも同程度なら、真の目的値$F(x)$が大きい方。

doubleの微小な誤差を考慮し、違反量には現在の保存値に対して$10^{-10}\max(1,\lvert V\rvert)$の許容幅を設けています。$V$は保存候補の正規化違反量です。実行可能性そのものはこの許容幅で決めず、整数のHard違反集合で決めます。

実行可能解同士なら、違反行数も違反量も0なので、整数目的値だけの比較になります。保存解の違反量には、候補選択用の動的重み$h_ {j}$を掛けません。探索の重点を変えても、保存解の優劣の意味を変えないためです。

## 付録D. 小規模な分枝限定と到達可能性

### D.1 行の境界に届くかを先に調べる

ある行で、変更できる変数をすべて都合よく選んだときの最小値と最大値を考えます。正の係数なら0を選ぶと小さく、1を選ぶと大きくなります。負の係数は逆です。変更できない変数の寄与は固定です。

この到達区間がHard行の許容範囲と交わらなければ、今回の変更範囲では実行可能解に届きません。`reachable`はこの必要条件を調べ、届かない場合は探索を省きます。

区間が交わるだけでは、実際にその整数値を作れるとは限りません。たとえば$2x_ {0}=1$では最小0・最大2の間に1がありますが、実際の値は0か2だけです。また、各行を別々に満たせても同時には満たせない場合があります。したがって到達区間は、十分条件ではありません。

### D.2 自由変数が18以下なら全体を分枝限定する

変更可能変数が18個以下なら、`exact_block`がそれらに0または1を割り当てる探索を行います。すでに選んだ値を先に、その逆を後に調べます。不要と分かる枝を切り落とすのが分枝限定です。

「この先を一番都合よく埋めても、どれほど高い目的値になり得るか」を上界と呼びます。未決定変数の単独利益には、正なら得る、負なら払わないと仮定します。これは実際より都合よい見積りなので、上界になります。

ソフト行は、未決定変数が作れる行和の区間から「少なくとも必要な減点」を計算し、上界から引きます。対象変数が触らない行の減点も含めます。Hard行の区間が条件に届かなければ、その枝は切ります。

すでに実行可能な保存解があり、上界がその目的値以下なら、そこからよりよい実行可能解は出ないため枝を切れます。葉まで到達した候補は`remember`で評価します。

全自由変数の探索を途中打切りなしで完了すれば、その呼出しで許される範囲の最適性を確認できます。ただし時間・探索量で中断した場合は保証できず、公開結果から探索完了による証明の有無は取得できません。全体探索の内部ノード上限は1,000,000です。

### D.3 一部だけ厳密に調べる用途

同じ関数は、後述する最大16変数の修復にも使います。その場合、外側の変数は現在の値で固定し、内部の変数だけを調べます。上界と行和の到達区間も、その固定値を考慮します。

分枝を戻るときは反転と区間更新を巻き戻し、途中で時間切れになっても、再帰を抜けるために必要な復元を行います。よい候補は別に保存しているので、作業中の候補を復元しても発見した解は失いません。

## 付録E. 候補選択、修復連鎖、焼きなまし

### E.1 初回の開始候補

初期候補を指定しない初回は、恒久固定を反映した全0候補をまず保存します。小規模な厳密探索などで終了せず、時間が残っていれば、正の単独利益の自由変数を1にした候補も調べます。

後者はHard条件を満たすとは限りません。全0候補を先に保存するのは、すでに実行可能な候補を失わないためでもあります。初期候補を明示した場合や継続呼出しは、同じ初期化を毎回行いません。

### E.2 最初に反転する変数を選ぶ

1回の外側反復では、主に次の候補を比較します。

- Hard違反があれば、違反行を1本選び、その違反量を減らせる変数。
- 変更可能変数から無作為に選ぶ12個の候補。
- 重みが正のソフト行があれば、4分の1の確率で選んだソフト行の違反を減らす候補。

行からの候補選択では、行長が12以下なら全項、長ければ12回の標本抽出を行います。無作為抽出には重複があり得ます。動かせない変数、連鎖中にすでに使った変数、その対象行の違反量を減らさない変数は除きます。

比較の基本は$\Delta G$です。ほぼ同点の候補がいつも同じ順にならないよう、0以上$10^{-7}$未満の小さい乱数を足します。ソフト行から有効な候補が得られた場合は、その行の改善を狙って開始変数を置き換えます。

### E.3 最大12変数の修復連鎖

まず1変数を反転します。その変更でHard行が壊れたなら、それを直す変数を探して次に反転します。新しく反転した変数に関係する違反行を優先し、なければ別のHard違反行、さらに必要なら選んだソフト行へ進みます。

たとえば「ちょうど1件」の条件でAからBへ変更したい場合、Aだけを0にしてもBだけを1にしても、一時的に条件を破ります。2つを連続して反転すれば、条件を守る交換になります。このような変更をまとまりとして提案するための機構です。

連鎖では同じ変数を2回使わないようロックし、深さは最大12です。各途中状態で`remember`を呼び、返却用のよい候補を保存します。

さらに、連鎖のどこまでを実際の提案として残すかを決めます。その比較では、実行可能な途中状態を優先し、同じ実行可能性なら累積探索利得の大きい方を選びます。選んだところより後ろだけを巻き戻し、ロックを解除します。

ここでの途中状態の選択と、返却用の`remember`は別の比較です。連鎖の提案を後で棄却しても、その途中で見つかったよい解は保存したままです。

### E.4 悪化する提案も確率で受け入れる

提案全体の利得$\Delta G$が0以上なら受理します。負なら、温度$T$を使う次の確率で受理します。

$$P=\exp(\Delta G/T)$$

$P$は受理確率、$\exp(z)$は$e$の$z$乗です。負の利得なので0から1の間になります。利得が同じ負値なら、温度が高いほど受け入れやすくなります。受け入れなければ、残していた連鎖を逆順に反転して元に戻します。

温度は探索量$t$に対し、次の周期的な形です。

$$T=0.01^{\phi},\qquad \phi=(t\bmod4096)/4096$$

$t$は内部の今回の探索量カウンタ、$\phi$は周期内の進み具合です。温度は1付近から0.01付近へ下がり、周期ごとに戻ります。実行時間の残り割合で冷却する方式ではありません。新しい`solve`呼出しでは探索量カウンタを数え直すので、位相も再開します。

確率的に悪化を受け入れるのは、少し離れた良い候補へ移動するためです。返却するのは現在候補ではなく保存解なので、探索途中の悪化がそのまま返却解の悪化になるわけではありません。

## 付録F. 違反の重み付け、部分修復、再出発

### F.1 未解消のHard行へ重点を移す

32反復の区切りで、現在違反しているHard行の探索用重み$h_ {j}$を増やします。まだ実行可能な保存解がなければ1、すでにあれば0.25を加えます。

長く違反し続けた行ほど、同じ違反量の増減が内部利得に大きく効くようになります。一方、1024反復ごとに次の形で重みを緩めます。

$$h_ {j}\leftarrow1+(h_ {j}-1)/2$$

1から上に増えた分だけを半分へ戻す操作です。過去に難しかった行の重みがいつまでも大きすぎる状態を避けます。この重みは`PboRow.weight`とは別物で、利用者が設定した減点額や目的値を変更しません。

### F.2 実行可能解が未発見なら、小さな範囲をまとめて修復する

実行可能な保存解がなく、保存解の改善が128探索単位を超えて止まり、部分修復の予定時刻にも達した場合、`lns`を試します。次の実行までの間隔は512探索単位です。

保存候補へ戻り、違反行を1本選びます。その行の変更可能変数と、周囲の関係行にある変数を最大16個集めます。重複や固定変数は除きます。

その変数集合だけを対象に分枝限定を呼びます。ただし内部ノード上限は256なので、16変数の全組合せを必ず調べるわけではありません。修復後は、その時点での保存候補を読み込み直します。

この部分修復は、**実行可能解がすでにある状態で目的値を磨くためには呼ばれません**。また、利用者が呼ぶ`improve`とは異なり、内部が違反行の周辺を選ぶ処理です。

### F.3 停滞したら保存候補から少し崩す

保存解の改善が512探索単位を超えて止まり、128反復の区切りに来ると、保存候補へ戻ってから2〜6回程度の無作為反転を行います。自由変数数が少なければ回数も抑えます。

反転対象の無作為抽出には重複があるため、実際に異なる値になった変数の個数がその回数と一致するとは限りません。途中の候補は保存対象として確認します。

この再出発は、モデルと保存解を捨てて最初から作り直す`reset_search`とは異なります。蓄積したよい候補を出発点に、別の周辺を調べる処理です。

## 付録G. 呼出し間の状態と終了管理

### G.1 モデル更新後の再評価

setterでモデルが変わると、次の`solve`の入口で探索尺度を作り直します。保存候補と現在候補に新しい固定値を反映し、真の目的値と違反を新モデルで評価し直して比較します。同一候補の重複評価は省きます。

`evaluate`はこの探索用状態に頼らず、指定配列と現在のモデルから行和を計算します。そのためsetter直後でも、新しいモデルの評価を返せます。

`rebuild`は接続構造まで作り直します。trueで引き継ぐ場合は新しい構造で候補を再評価します。`reset_search`は構造を保ったまま次回初期化を要求し、`reseed`は乱数状態だけを書き換えます。

### G.2 初期配列と返却領域が同じでも扱える理由

呼出し元が、solver自身の返却した`x`を次の初期候補に渡す場合があります。実装は、内部の結果や状態を書き換える前に、渡された初期候補を退避します。そのため同じ領域への参照であっても、読み取り途中に結果が変わる問題を避けています。

ただし、過去の解として保持したい場合に参照でよい、という意味ではありません。呼出し後も旧解を残したいならコピーが必要です。

### G.3 時間と探索量を確認する場所

`tick`は外側の試行や分枝限定のノードで呼ばれ、探索量上限を確認してカウンタを増やします。通常は32探索単位ごとに時計も確認します。連鎖中や部分修復・再出発の前には追加の時間確認があります。

締切を検出した後も、仮適用した変更を正しく戻す処理などは残ります。モデルの初期評価も省けません。したがってマイクロ秒単位で指定できることと、マイクロ秒単位で厳密に止まることは別です。

`elapsed_us`は呼出し入口から、返却直前に記録する時点までです。初期候補の退避、必要な全評価、探索、巻戻しを含みます。モデルの事前構築、呼出し後の結果コピー、呼出し側での出力は含みません。

`iterations`は呼出しごとに数え直します。連鎖の反転全部を数えるカウンタではないので、同じiterationsでもモデルや処理経路によって時間は変わります。

### G.4 乱数の扱い

各solverは独立した64bitの乱数状態を持ち、SplitMix64方式で更新します。整数候補の抽出には軽い乗算による範囲変換を使い、実数乱数には上位53bitを使います。

外部の乱数器を引数で注入するAPIはありません。構築時のseedと`reseed`で系列を指定します。1個のsolverを複数スレッドから同時操作する前提ではありません。

## 付録H. 計算量、数値の前提、適用上の限界

### H.1 計算量

$n$を変数数、$m$を行数、$K$を正規化後に残った項数、$k_ {j}$を行$j$の項数、$\deg(i)$を変数$i$が現れる項数とします。構築のソートでは、正規化前の各行の項数を使います。

| 処理 | 主な計算量 |
|---|---|
| 構築・rebuild | $O(n+m+\sum_ {j}k_ {j}\log k_ {j})$。候補引継ぎ時は全評価も行う |
| 必要メモリ | $O(n+m+K)$ |
| evaluate、候補全体の読込み | $O(n+m+K)$ |
| 1変数の差分評価・反転 | $O(\deg(i))$ |
| set_profit、set_constant、set_fixed、境界・種類変更 | 各$O(1)$。次回探索では必要な全評価を行う |
| set_coefficient | 対象行の項数に対し$O(\log k_ {j})$ |
| reset_search、reseed | 各$O(1)$。reset後の次回初期化は別途必要 |
| 結果全体のコピー | $O(n)$ |
| solve、improve | 初期化・再評価などの線形処理と、指定予算内の探索 |

`PboProblem`の行追加は、すでに作った項配列を移動する場合、行配列への追加自体は償却$O(1)$です。ただし項配列の生成、左辺値からのコピー、行配列の再確保などのコストは別です。`add_all_profit`は、指定IDの重複除去のためソートも行います。

部分探索で自由変数が少なくても、候補の読込みや各行の補助配列準備にはモデル全体に比例する処理が残ります。「部分探索なら全処理が部分のサイズだけに比例する」とは限りません。

### H.2 整数の前提

利益、係数、境界、定数、減点の重み、行和、目的値は`long long`です。入力値だけでなく、**途中演算もすべてlong longに収まること**が前提です。

具体的には、重複係数の合計、係数×選択値、行和、境界との差、重み×違反量、目的値の増減、部分探索の上界なども含みます。最終結果が小さくても途中であふれれば正しく扱えません。入力に`LLONG_MIN`は使いません。変数ID・行ID・項数などはintの範囲に収めます。

上下限を両方指定するなら、**下限が上限以下**である必要があります。ソフト行の重みは0以上です。0/1配列に2などを入れることも、想定した入力ではありません。基本的な前提はassertで検査するため、`NDEBUG`を付けると検査は無効になります。

実数係数をそのまま渡すAPIはありません。必要なら利用側で共通の尺度を選んで整数化します。近似的に丸めると境界や最適解が変わり得ます。目的値側と制約側の尺度、減点の単位が意図に合っているかも確認します。

### H.3 解ける形式でも、解きやすいとは限らない

係数が大きい等式、多数の相互依存、ほとんどの変数が多数の行に出る密なモデルでは、修復や差分計算が重くなる場合があります。実行可能解の構築方法を問題の構造から用意できるなら、それを渡すと探索を改善へ向けやすくなります。

多値変数も、たとえば「値0を選ぶ」「値1を選ぶ」「値2を選ぶ」という3個の0–1変数と、ちょうど1個選ぶ等式で表せます。そのとき元の値は$0x_ {0}+1x_ {1}+2x_ {2}$です。ただし変数数と条件が増えます。表現できるからといって、専用の整数・連続最適化より有利とは限りません。

本ガイドの小さな例は、式を手で追ってAPIの意味を学ぶためのものです。多くは小規模な厳密探索で解けます。大規模なAHCにおける解品質や実行時間を示すベンチマークではありません。最後の3例では、18変数を超えるモデル、継続、再現性、部分探索も実際に通るようにしています。

