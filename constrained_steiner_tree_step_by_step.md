# constrained_steiner_tree v09 ステップバイステップガイド

対象ヘッダー: `constrained_steiner_tree_v09.hpp`  
対象読者: C++の変数・関数・`vector`・繰り返しを知っている人。数学は高校数学までを想定する。

このガイドは、最小コード1本と発展コード21本を順に読み、最後に実装の仕組みへ進む構成になっている。各C++コードは独立したプログラムであり、それ以前のコードを継ぎ足さなくても動く。入力ファイルも必要ない。

最小例と結果の読み方から始め、探索設定、排他・禁止条件、既存解の正規化・改善・修復・追加接続、時間とメモリ、外側を保った部分解き直しへ進む。数式は一般的な `$...$` と `$$...$$` のLaTeX表記を使う。数式対応のMarkdownリーダーで読むとよいが、記号の意味は本文でも説明する。

コードは同名ヘッダーと同じフォルダーに置く。たとえば最初のコードを `example01.cpp` に保存し、GCCのC++20以上で次のように実行する。

```sh
g++ -std=c++20 -O2 -Wall -Wextra example01.cpp -o example01
./example01
```

ヘッダーはGCC系の `bits/stdc++.h` を使用する。ヘッダーそのものを実行プログラムとしてコンパイルするのではなく、各例の `.cpp` から `#include` する。

コード横の「出力」は小さな例での確認結果である。一般の入力で最適解や同一の辺集合を保証する意味ではない。時間制限を使う例では、探索量や出力が環境によって変わる。

## 1. 何を解くライブラリか

### 1.1 必要な場所だけを、安くつなぎたい

複数の拠点を道路やケーブルでつなぐ場面を考える。

- 場所を「頂点」と呼ぶ。
- 2つの場所を直接つなげる候補を「辺」と呼ぶ。
- 辺を採用するために必要な金額・距離などを「費用」と呼ぶ。
- 必ずつなぎたい頂点を「terminal」と呼ぶ。以下では「端子」とも書く。

端子どうしをつなぐためなら、端子ではない場所を中継点として使ってよい。中継点を共有すれば、端子どうしを別々につなぐより安くなることがある。このような接続を扱うのがSteiner tree問題である。

ここで必要なのは、**すべての端子が同じネットワーク内で行き来できること**。すべての頂点を使う必要はない。出発点や巡回順を決める問題でもなく、辺を通る回数に応じて費用が増える問題でもない。採用した辺の費用を1回ずつ足す。

本ライブラリが扱う辺は無向である。`0` と `1` を結ぶ辺は両方向に使える。辺の費用は0以上の整数であり、頂点自体には費用を設定しない。

### 1.2 目的関数

使う辺の集合を選び、その合計費用をできるだけ小さくする。

$$\min_ {F \subseteq E} \sum_ {e \in F} c_ {e}$$

記号の意味を一つずつ確認する。

|記号|意味|
|---|---|
|$V$|グラフに登録したすべての頂点の集合|
|$E$|登録したすべての辺の集合|
|$T \subseteq V$|必ずつなぎたい端子の集合|
|$F \subseteq E$|今回の答えとして採用する辺の集合。これを選ぶ|
|$e$|採用候補となる1本の辺|
|$c_ {e}$|辺 $e$ の費用。0以上の整数|
|$\sum_ {e \in F}$|集合 $F$ に入っている辺について足し合わせる、という記号|
|$\min$|条件を満たす選択肢の中で、値を最小にしたいという意味|

式だけでは、何も選ばない費用0の答えになってしまう。そこで「$F$ の辺を使って、$T$ のすべての端子が互いに到達できること」という接続条件を課す。端子が0個または1個なら、接続のための辺は必要ない。

### 1.3 さらに、同時に使えない場所がある

本ライブラリでは、接続条件に加えて次の指定ができる。

1. 頂点1と頂点2を同時に使ってはいけない。
2. 指定したグループの中から、使える頂点は高々1個。
3. この呼び出しでは、ある頂点や辺を使ってはいけない。
4. 別の処理で選択済みの頂点とも、排他条件を守りたい。

排他条件が見るのは、端子だけではない。**返却された辺の途中に現れる中継頂点も使用頂点に含む**。端子として指定されなくても、接続に使ったなら制約の対象になる。

「高々1個」は0個でも1個でもよいという意味であり、「必ず1個選ぶ」指定ではない。また、外側で選択済みの頂点は排他条件に含めるが、その頂点まで今回のネットワークを延ばす義務はない。

既設のネットワークを撤去せず、新しい辺だけ追加する使い方もある。その場合は「全費用」ではなく「追加費用」を最小にする。違いは発展⑫で改めて説明する。

### 1.4 得られる答えの保証

これはヒューリスティック、つまり有望な答えを探索するライブラリである。最適性は保証しない。制約を満たす答えが存在しても、探索範囲や時間の都合で発見できないことがある。

一方、成功として返された答えは、指定された接続と制約を満たす解として扱える。まず成功したかを確認し、それから費用や辺を読む。ここまでが問題の定義である。解を探す内部アルゴリズムは末尾の付録で扱う。

## 2. 最小コード：3つの端子をつなぐ

### やりたいこと

頂点は `0, 1, 2, 3` の4個。端子は `0, 2, 3` で、頂点1は使っても使わなくてもよい中継候補である。

|辺の両端|費用|
|---|---:|
|0と1|2|
|1と2|2|
|1と3|2|
|0と2|7|
|0と3|7|
|2と3|7|

端子間の直接の辺は高い。頂点1を共有すれば、費用2の辺を3本使い、合計6でつなげられる。

### 動くコード — example01.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>

int main() {
    constrained_steiner_tree solver(4);
    solver.add_edge(0, 1, 2);
    solver.add_edge(1, 2, 2);
    solver.add_edge(1, 3, 2);
    solver.add_edge(0, 2, 7);
    solver.add_edge(0, 3, 7);
    solver.add_edge(2, 3, 7);

    auto answer = solver.solve({0, 2, 3});
    if (!answer.ok()) {
        std::cout << "no feasible result\n";
        return 1;
    }
    std::cout << answer.cost << '\n';
}
```

出力は `6`。

### 一つずつ読む

`constrained_steiner_tree solver(4)` の `4` は頂点数。頂点番号は0から始まるため、有効なのは0以上4未満である。辺はまだ1本も登録されていない。

`add_edge(u, v, cost)` は、頂点 `u` と `v` を結ぶ無向辺を追加する。

- 第1引数は一方の頂点番号。
- 第2引数はもう一方の頂点番号。
- 第3引数はその辺を使う費用。型は `long long` で、0以上にする。

たとえば `add_edge(1, 3, 2)` は「頂点1と頂点3を、費用2でつなげられる」という登録である。逆向きの `add_edge(3, 1, 2)` を追加する必要はない。それを追加すると、別番号の並行辺をもう1本登録したことになる。

`solve({0, 2, 3})` に渡すのは**端子の頂点番号**。接続に使う辺番号でも、通過する順番でもない。`{0, 2, 3}` は、その場で作った端子リストである。

戻り値の型は `constrained_steiner_tree::result`。`auto` はその型をコンパイラに推定させている。

`answer.ok()` は「実行可能解を持っているか」を返す。`false` のときに `answer.cost == 0` であっても、費用0の答えが見つかったとは限らない。必ず成功判定を先に行う。

`answer.cost` は、成功した答えで使う辺の合計費用である。今回は制約も探索設定も指定せず、既定の設定を使った。

## 3. 発展①：どの辺が選ばれたかを読む

**前の例からの変更:** グラフと端子はそのままに、辺番号とその内容を出力する。

### 動くコード — example02.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>

int main() {
    constrained_steiner_tree solver(4);
    const int first = solver.add_edge(0, 1, 2);
    solver.add_edge(1, 2, 2);
    solver.add_edge(1, 3, 2);
    solver.add_edge(0, 2, 7);
    solver.add_edge(0, 3, 7);
    solver.add_edge(2, 3, 7);

    const auto answer = solver.solve({0, 2, 3});
    if (!answer.ok()) return 1;
    std::cout << "first=" << first << '\n';
    std::cout << "vertices=" << solver.vertex_count()
              << " edges=" << solver.edge_count() << '\n';
    for (int id : answer.edges) {
        const auto &e = solver.edge(id);
        std::cout << id << ": " << e.from << ' ' << e.to
                  << " cost=" << e.cost << '\n';
    }
}
```

出力は次のとおり。

```text
first=0
vertices=4 edges=6
0: 0 1 cost=2
1: 1 2 cost=2
2: 1 3 cost=2
```

`add_edge` の戻り値は辺番号であり、登録順に `0, 1, 2, ...` と増える。**頂点番号と辺番号は別の番号体系**なので、両者を取り違えない。

`answer.edges` は選ばれた辺番号の `std::vector<int>`。経路の通過順ではないので、隣り合う要素どうしが端点を共有するとは限らない。

`edge(id)` の引数 `id` は登録済みの辺番号である。返る `edge_type` の `from` と `to` は両端の頂点、`cost` は登録した費用を表す。無向辺なので、`from` から `to` へしか通れないという意味ではない。

`const auto &e` は辺情報をコピーせず参照している。`add_edge` によって内部配列が移動する可能性があるため、この参照を保持したまま辺を追加する使い方は避ける。辺番号を保持し、必要な時点で `edge(id)` を呼び直すとよい。

`vertex_count()` は頂点数、`edge_count()` は登録辺数。どちらも引数を取らず、選ばれた頂点・辺の数を返す関数ではない。

## 4. 発展②：探索の設定を指定する

**変更:** 同じグラフに `options` を追加する。以後の例では長い型名を `Solver` という別名で書く。

### 動くコード — example03.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(4);
    solver.add_edge(0, 1, 2);
    solver.add_edge(1, 2, 2);
    solver.add_edge(1, 3, 2);
    solver.add_edge(0, 2, 7);
    solver.add_edge(0, 3, 7);
    solver.add_edge(2, 3, 7);

    Solver::options opts;
    opts.preset_mode = Solver::preset::quality;
    opts.seed = 7;
    opts.restart_limit = 4;
    const auto answer = solver.solve({0, 2, 3}, opts);
    if (!answer.ok()) return 1;
    std::cout << answer.cost << '\n';
}
```

出力は `6`。

`using Solver = constrained_steiner_tree` はC++の型の別名を付ける文であり、ライブラリの動作は変えない。

`Solver::options opts` は、設定値をまとめたオブジェクト。書き換えなかった項目は既定値のままである。

|今回の項目|指定した値|意味|
|---|---|---|
|`preset_mode`|`quality`|解の質を重視する探索構成を選ぶ|
|`seed`|`7`|内部乱数の初期値。別の値で異なる候補を探せる|
|`restart_limit`|`4`|追加の出発点を変えた探索の上限。基本の候補作成まで4回に制限する意味ではない|

プリセットには `fast`、`balanced`、`quality` がある。既定は `balanced`。`quality` は多くの工夫を使うが、すべての入力で `fast` よりよい答えになる保証はない。

`restart_limit = 0` でも基本の候補生成や、そのプリセットに応じた改善は残る。`-1` は自動設定。時間予算がなく自動設定なら、追加再スタートの既定上限はfastで0、balancedで4、qualityで20である。時間予算も指定した場合の扱いは後で説明する。

`solve` の第1引数は端子、第2引数は探索設定。乱数seedをそろえると比較しやすいが、時間制限、cacheの状態、コンパイラ・実行環境まで異なる場合の完全な再現性を保証するものではない。

## 5. 発展③：2つの頂点を同時に使わない

**変更:** 制約の効果が分かるよう、ここからしばらく別の小さなグラフを使う。安い経路は `0→1→2→3` で費用3、高い代替経路は `0→4→3` で費用6。頂点1と2を同時使用不可にする。

### 動くコード — example04.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(5);
    solver.add_edge(0, 1, 1);
    solver.add_edge(1, 2, 1);
    solver.add_edge(2, 3, 1);
    solver.add_edge(0, 4, 3);
    solver.add_edge(4, 3, 3);

    Solver::selection_rules rules(5);
    rules.add_conflict(1, 2);
    Solver::query_context context;
    context.rules = &rules;

    const auto answer = solver.solve({0, 3}, context);
    if (!answer.ok()) return 1;
    std::cout << answer.cost << '\n';
}
```

出力は `6`。頂点1と2は端子ではないが、安い経路の中継点なので排他条件に抵触する。

`selection_rules rules(5)` の `5` は、制約を定義するグラフの頂点数。solverの頂点数と一致させる。

`add_conflict(1, 2)` の引数は両方とも頂点番号。「1を使うなら2を使わない、2を使うなら1を使わない」という対称な条件である。両方とも使わないのは許される。異なる有効な頂点番号を渡す。

`query_context` は、その呼び出しで使う制約や外部条件をまとめる型。`context.rules = &rules` は `rules` を参照させる指定であり、制約全体をコピーするわけではない。`rules` は呼び出しが終わるまで生存させる。

この `solve({0, 3}, context)` は、設定を省略し、制約だけを渡す便利な呼び出し方である。設定も渡すときは `solve(terminals, opts, context)` の順になる。最後の2引数を逆にしない。

## 6. 発展④：グループから高々1頂点を使う

**変更:** 1組ずつの排他を、グループ指定へ置き換える。グラフは同じ。

### 動くコード — example05.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
#include <vector>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(5);
    solver.add_edge(0, 1, 1);
    solver.add_edge(1, 2, 1);
    solver.add_edge(2, 3, 1);
    solver.add_edge(0, 4, 3);
    solver.add_edge(4, 3, 3);

    Solver::selection_rules rules(5);
    const std::vector<int> group{1, 2, 4};
    rules.add_exclusive_group(group);
    Solver::query_context context;
    context.rules = &rules;
    const auto answer = solver.solve({0, 3}, context);
    if (!answer.ok()) return 1;
    std::cout << answer.cost << '\n';
}
```

出力は `6`。安い経路はグループ内の1と2を両方使うので不可。代替経路は4だけを使うので許される。

`add_exclusive_group(group)` の引数は頂点番号の列。長さではなく、そこに書いたすべての頂点が同じグループに所属する。`std::vector<int>` の代わりに `std::array<int, 3>` も渡せる。

使用頂点の集合を $S$、ある排他グループを $H$ と書くと、条件は次のように表せる。

$$\lvert S \cap H \rvert \le 1$$

$S \cap H$ は「使用していて、かつ、そのグループに入っている頂点」。縦棒はその個数を表す。0個は許されるので、グループの代表を必ず選ぶ機能ではない。

グループは重なってよい。同じ頂点が複数のグループに属す場合は、すべての条件を同時に満たす必要がある。入力内の同じ頂点は重複除去され、異なる頂点が2個未満のグループは登録されない。引数の列は登録時にコピーされるため、登録後まで `group` を保持する必要はない。

## 7. 発展⑤：この呼び出しだけ頂点を禁止する

**変更:** グラフは同じまま、排他条件ではなく頂点1の利用を禁止する。

### 動くコード — example06.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
#include <vector>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(5);
    solver.add_edge(0, 1, 1);
    solver.add_edge(1, 2, 1);
    solver.add_edge(2, 3, 1);
    solver.add_edge(0, 4, 3);
    solver.add_edge(4, 3, 3);

    const std::vector<int> blocked_vertices{1};
    Solver::query_context context;
    context.forbidden_vertices = blocked_vertices;
    const auto answer = solver.solve({0, 3}, context);
    if (!answer.ok()) return 1;
    std::cout << answer.cost << '\n';
}
```

出力は `6`。`forbidden_vertices` は「列にある頂点を使わない」という絶対的な禁止であり、他の頂点を使った場合だけ禁止する排他条件とは異なる。禁止頂点を端子にも指定すれば、固定条件が矛盾する。

`context` の列は `std::span<const int>` であり、配列を見るための窓のようなもの。ここへの代入は、`blocked_vertices` の内容をコピーしない。呼び出し中は元のvectorを生存させ、要素の追加などで記憶領域を動かさない。

これは後で出てくる `forbidden_edges` と `selected_outside` にも共通する。短命な一時vectorを代入して次の文でsolverを呼ぶ、といった書き方はしない。名前付きのvectorを用意するのが分かりやすい。

## 8. 発展⑥：頂点ではなく、特定の辺を禁止する

**変更:** 前の例の頂点禁止を、辺禁止へ置き換える。

### 動くコード — example07.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
#include <vector>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(5);
    solver.add_edge(0, 1, 1);
    const int damaged = solver.add_edge(1, 2, 1);
    solver.add_edge(2, 3, 1);
    solver.add_edge(0, 4, 3);
    solver.add_edge(4, 3, 3);

    const std::vector<int> blocked_edges{damaged};
    Solver::query_context context;
    context.forbidden_edges = blocked_edges;
    const auto answer = solver.solve({0, 3}, context);
    if (!answer.ok()) return 1;
    std::cout << answer.cost << '\n';
}
```

出力は `6`。`damaged` は `add_edge` が返した辺番号。この例では1であるが、頂点1を意味しているわけではない。

`forbidden_edges` の要素は辺番号である。辺1を禁止しても、頂点1や頂点2そのものが禁止されるわけではない。他の辺を通じて使うことはできる。

排他条件・頂点禁止・辺禁止は、同じ `context` に同時に設定できる。その場合はすべての条件を満たす必要がある。一方、グラフ自体から辺を削除しているわけではないので、別の呼び出しでこのcontextを渡さなければ禁止は引き継がれない。

## 9. 発展⑦：外側ですでに選んだ頂点を考慮する

**変更:** 2つの経路を持つグラフへ小さく組み替え、グラフ内で孤立した頂点4を「外側で選択済み」とする。

### 動くコード — example08.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
#include <vector>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(5);
    solver.add_edge(0, 1, 1);
    solver.add_edge(1, 3, 1);
    solver.add_edge(0, 2, 2);
    solver.add_edge(2, 3, 2);
    // 頂点4には辺を登録しない。

    Solver::selection_rules rules(5);
    rules.add_conflict(1, 4);
    const std::vector<int> outside{4};
    Solver::query_context context;
    context.rules = &rules;
    context.selected_outside = outside;

    const auto answer = solver.solve({0, 3}, context);
    if (!answer.ok()) return 1;
    std::cout << answer.cost << '\n';
}
```

出力は `4`。頂点4が選択済みなので、排他相手の1を経由する費用2の経路は使えない。頂点2を経由する費用4の経路を使う。

ここで重要なのは、**頂点4が孤立していても成功する**こと。`selected_outside` は、接続する端子を追加する指定ではない。排他条件を評価するときに、すでに使用中と見なすだけである。

今回の端子は第1引数の `{0, 3}` のまま。もし4にも接続する必要があるなら、端子へ4を追加しなければならない。その場合、このグラフでは4に辺がないため接続できない。

外側で選んだ頂点も禁止頂点と重なってはいけない。また、外側で選んだ頂点どうしの排他衝突も許されない。外側の辺そのものを撤去不能として使いたい場合は、この指定だけでは足りず、発展⑫の `augment` を使う。

## 10. 発展⑧：成功・不可能・入力ミスを区別する

**変更:** 答えの `state` を読む。異なる原因を小さな独立ケースで確認する。最後のケースだけ、既存の接続を改善する `improve` を先に使う。これは端子・初期辺番号・contextの順に渡す関数で、初期辺が制約違反なら拒否する。その性質を確認した後、発展⑩で詳しく扱う。

### 動くコード — example09.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
using Solver = constrained_steiner_tree;

const char *name(Solver::status state) {
    switch (state) {
    case Solver::status::feasible: return "feasible";
    case Solver::status::proven_infeasible: return "proven_infeasible";
    case Solver::status::not_found: return "not_found";
    case Solver::status::invalid_candidate: return "invalid_candidate";
    case Solver::status::invalid_input: return "invalid_input";
    }
    return "unknown";
}

int main() {
    Solver solver(3);
    solver.add_edge(0, 1, 1);
    solver.add_edge(1, 2, 1);
    solver.add_edge(0, 2, 5);
    std::cout << name(solver.solve({0, 2}).state) << '\n';

    Solver::selection_rules impossible(3);
    impossible.add_conflict(0, 2);
    Solver::query_context context;
    context.rules = &impossible;
    std::cout << name(solver.solve({0, 2}, context).state) << '\n';

    Solver disconnected(2);
    std::cout << name(disconnected.solve({0, 1}).state) << '\n';
    std::cout << name(solver.solve({3}).state) << '\n';

    Solver::selection_rules rules(3);
    rules.add_conflict(0, 1);
    context.rules = &rules;
    std::cout << name(solver.improve({0, 2}, {0, 1}, context).state) << '\n';
}
```

出力は順に `feasible`、`proven_infeasible`、`not_found`、`invalid_input`、`invalid_candidate`。

|状態|意味|この例での理由|
|---|---|---|
|`feasible`|合法な答えを保持している|0と2を接続できた|
|`proven_infeasible`|固定条件から不可能と証明できた|必須端子0と2が同時使用不可|
|`not_found`|この呼び出しでは答えを得られなかった|辺がないグラフで接続を要求した|
|`invalid_input`|IDや制約定義などの入力が不正|3頂点のグラフに頂点番号3を渡した|
|`invalid_candidate`|`improve` に渡した初期解が制約違反|初期辺0・1が、同時使用不可の頂点0・1を使う|

`proven_infeasible` は、ライブラリが証明できる特定の矛盾で返る。現実に解が存在しないすべてのケースをこの状態へ分類するわけではない。上の非連結グラフでも `not_found` である。逆に `not_found` だからといって、解が存在しないと結論してはいけない。

最後の `improve` は「渡した既存解を改善する」関数で、詳しくは発展⑩で扱う。ここでは `improve({端子}, {初期辺番号}, context)` という引数の区別だけ押さえる。

クエリの不正IDは実行時に調べられるが、グラフ構築の `add_edge` や制約登録の `add_conflict` などでは、有効範囲の入力が呼び出し側の前提である。`assert` が無効になるビルドでも、構築時の不正入力を安全に扱ってくれると考えてはいけない。

## 11. 発展⑨：候補辺の中だけで接続を整える — normalize

**変更:** `solve` の代わりに、「使ってよい候補辺」を渡す `normalize` を使う。重複した辺や、端子接続に不要な枝があってもよい。

### 動くコード — example10.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
#include <vector>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(6);
    solver.add_edge(0, 1, 1);   // 辺0
    solver.add_edge(1, 2, 1);   // 辺1
    solver.add_edge(2, 3, 1);   // 辺2
    solver.add_edge(0, 4, 3);   // 辺3
    solver.add_edge(4, 3, 3);   // 辺4
    solver.add_edge(3, 5, 100); // 辺5: 不要な枝

    Solver::selection_rules rules(6);
    rules.add_conflict(1, 2);
    Solver::query_context context;
    context.rules = &rules;
    const std::vector<int> candidates{0, 1, 2, 3, 4, 5, 3};

    const auto answer = solver.normalize({0, 3}, candidates, context);
    if (!answer.ok()) return 1;
    std::cout << answer.cost << " edges=" << answer.edges.size() << '\n';
}
```

出力は `6 edges=2`。この例の返却辺は3と4である。

引数を順に読む。

1. `{0, 3}` は、つなぐべき端子。
2. `candidates` は、今回採用してよい**辺番号**の列。
3. `context` は、前と同じ制約条件。省略すれば追加制約なし。

`normalize` は候補にない辺を追加しない。候補の範囲で、余分な辺を落として合法な接続を作る。候補に頂点1と2を通る辺があっても、結果からそれらを外して合法化できれば成功する。

「グラフには別のよい経路があるから補ってほしい」というときには向かない。候補辺だけではつながらなければ失敗する。その用途は発展⑪の `repair` で扱う。

`normalize` は候補内での最適Steiner treeを厳密に求める関数ではない。制約付きの場合も制限付き探索を行うため、見つけられない場合がある。v09の公開APIは `options` を受け取らず、制約分岐の上限は内部でノード32・深さ24に設定される。`solve` 用の時間設定をそのまま渡せる関数ではない点に注意する。

## 12. 発展⑩：合法な既存解を悪くせず改善する — improve

**変更:** 今度は候補の寄せ集めではなく、すでにつながっている合法な解を渡す。説明を簡単にするため、追加制約は外す。

### 動くコード — example11.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
#include <vector>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(5);
    solver.add_edge(0, 1, 1);
    solver.add_edge(1, 2, 1);
    solver.add_edge(2, 3, 1);
    solver.add_edge(0, 4, 3);
    solver.add_edge(4, 3, 3);

    const std::vector<int> initial{3, 4};
    const auto answer = solver.improve({0, 3}, initial);
    if (!answer.ok()) return 1;
    std::cout << answer.cost << '\n';
}
```

出力は `3`。初期解は辺3・4による費用6の経路だったが、登録済みの他の辺も使って改善できた。

`improve` の第1引数は端子、第2引数は初期解の辺番号。設定と制約も渡すなら `improve(terminals, initial_edges, opts, context)`。制約だけなら `improve(terminals, initial_edges, context)` と書く。

初期解は、今の条件で合法かつ端子を接続している必要がある。初期辺は撤去不能ではない。辺を削ったり別の辺へ置き換えたりしてよい。

合法で費用を表現できる連結初期解は、不要部分を除いた形で保持される。探索でそれよりよい合法解が得られなければ、保持した初期解を返す。したがって、正しい条件で渡した既知の解を改善する用途に適している。ただし、改善幅が必ず正になるわけではない。

初期辺に制約違反があると、後でその辺を削れば合法になりそうでも `invalid_candidate` となる。制約変更後の旧解を無条件に渡す用途には、次の `repair` を使う。非連結の初期集合を `improve` へ渡した場合の救済動作に頼らず、用途を分ける。

## 13. 発展⑪：途中までの接続や、制約違反の旧解を修復する — repair

**変更:** 途中までしかつながっていない辺集合を渡し、新しく追加した排他条件も守って修復する。

### 動くコード — example12.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
#include <vector>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(5);
    solver.add_edge(0, 1, 1);
    solver.add_edge(1, 2, 1);
    solver.add_edge(2, 3, 1);
    solver.add_edge(0, 4, 3);
    solver.add_edge(4, 3, 3);

    Solver::selection_rules rules(5);
    rules.add_conflict(1, 2);
    Solver::query_context context;
    context.rules = &rules;
    const std::vector<int> partial{0, 1};

    const auto answer = solver.repair({0, 3}, partial, context);
    if (!answer.ok()) return 1;
    std::cout << "repaired=" << answer.cost << '\n';
    const auto old = solver.improve({0, 3}, {0, 1, 2}, context);
    std::cout << "old_legal=" << old.ok() << '\n';
}
```

出力は次のとおり。

```text
repaired=6
old_legal=0
```

`partial` は辺0・1だけなので、頂点0から2まではつながるが端子3には届かない。しかも頂点1・2の排他条件を破っている。

`repair(terminals, partial_edges, context)` は、この部分集合を出発材料にして、辺の追加と削除を両方許す。`normalize` と違い、入力の `partial` にない辺3・4も使える。

`partial_edges` の各IDは有効である必要があるが、その集合自体は非連結でも制約違反でもよい。空の集合も渡せる。設定も使う場合の順番は `repair(terminals, partial_edges, opts, context)` である。

`partial` はヒントであって固定ではない。渡した辺を全部捨てる結果もあり得る。また、修復を必ず成功させる保証はない。合法な連結初期解を期限切れでも保持したい用途は、前の `improve` を選ぶ。

### ここまでの3関数を整理する

|関数|渡すもの|入力にない辺の追加|入力の辺の削除|主な用途|
|---|---|---|---|---|
|`normalize`|採用候補の辺集合|しない|する|候補の範囲だけで整理する|
|`improve`|合法な連結初期解|する|する|既知の合法解を保持しつつ改善する|
|`repair`|途中まで、または違反を含む辺集合|する|する|壊れた旧解を出発材料に作り直す|

## 14. 発展⑫：撤去不能な既設網へ追加する — augment

**変更:** 初期辺をヒントとして渡すのではなく、取り除けない既設辺として渡す。

### 何を最小化するか

既設辺の集合を $B$、今回追加する辺の集合を $A$ とする。

$$\min_ {A \subseteq E \setminus B} \sum_ {e \in A} c_ {e}$$

$E$ は登録した全辺、$c_ {e}$ は辺 $e$ の登録費用。$E \setminus B$ は既設辺を除いた辺の集合である。選ぶのは追加分の $A$ で、既設辺 $B$ は動かさない。接続条件は、両方を合わせた $B \cup A$ が端子をつなぐこと。

既設辺の費用はすでに支払ったものと考え、今回の追加費用には含めない。登録済みの辺重みを書き換える必要はない。

### 動くコード — example13.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
#include <vector>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(4);
    const int existing = solver.add_edge(0, 1, 100);
    solver.add_edge(1, 3, 5);
    solver.add_edge(0, 2, 1);
    solver.add_edge(2, 3, 1);

    Solver::selection_rules rules(4);
    rules.add_conflict(1, 2);
    Solver::query_context context;
    context.rules = &rules;
    const std::vector<int> base{existing};

    const auto answer = solver.augment({0, 3}, base, context);
    if (!answer.ok()) return 1;
    auto network = base;
    network.insert(network.end(), answer.added_edges.begin(), answer.added_edges.end());
    std::cout << "added=" << answer.added_cost
              << " new_edges=" << answer.added_edges.size()
              << " total_edges=" << network.size() << '\n';
}
```

出力は `added=5 new_edges=1 total_edges=2`。

第1引数 `{0, 3}` は接続したい端子。第2引数 `base` は撤去不能な**辺番号**の集合。第3引数は制約。設定も渡す形は `augment(terminals, base_edges, opts, context)` である。

既設辺0によって頂点1はすでに使用中なので、排他相手の2を通る追加費用2の経路は使えない。既設の0と1の接続を無料で利用し、1と3の間へ費用5の辺を追加する。

戻り値は `augmentation_result`。通常の `result` と次の点が異なる。

- `added_cost` は追加分の合計費用。既設辺の100は含まない。
- `added_edges` は新規追加した辺番号だけ。既設辺0は含まない。
- `state`、`ok()`、`stats` の意味は通常の結果と同じ。

最終ネットワークが必要なら、コードのように既設辺と追加辺を合わせる。この例の `base` には重複がない。重複を含むbaseを渡した場合は、利用者側で最終辺集合を作る際にも重複を除いて扱う。

既設辺は非連結でも、閉路を含んでいてもよい。端子接続に不要な既設成分も残るため、最終ネットワーク全体が1本の木になるとは限らない。また、すべての既設成分を互いにつなぐ義務もない。それらもつなぎたいなら、必要な成分の代表頂点などを端子に含める。

## 15. 発展⑬：接続に使わない既設辺の端点も制約に含まれる

**変更:** 既設辺を端子から離れた場所へ置き、`selected_outside` だけでは説明しきれない既設辺の扱いを確認する。

### 動くコード — example14.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(5);
    solver.add_edge(0, 1, 2);
    const int existing = solver.add_edge(3, 4, 100);
    Solver::selection_rules rules(5);
    rules.add_conflict(1, 4);
    Solver::query_context context;
    context.rules = &rules;

    const auto fixed = solver.augment({0, 1}, {existing}, context);
    std::cout << "infeasible="
              << (fixed.state == Solver::status::proven_infeasible) << '\n';
    const auto free = solver.augment({0, 1}, {}, context);
    if (!free.ok()) return 1;
    std::cout << "without_base=" << free.added_cost << '\n';
}
```

出力は `infeasible=1` と `without_base=2`。

既設辺3–4は端子0・1をつなぐために使わない。それでも撤去不能なので頂点4は使用中のままである。端子1も必須であり、1と4の排他条件を満たせない。

この規則は、端子が0個や1個でも変わらない。既設辺の端点・外側選択・端子・禁止条件は、接続に使うかどうかだけではなく、残るネットワーク全体の整合性として評価する。

既設辺を明示的に禁止したり、その端点を禁止したりした場合も矛盾する。撤去を許してよいなら `augment` のbaseにせず、`repair` の材料として渡す。

## 16. 発展⑭：1回の予算と、全体の締切を指定する

**変更:** 発展②の設定に、相対的な時間予算と絶対的な締切を追加する。

### 動くコード — example15.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <chrono>
#include <iostream>
using Solver = constrained_steiner_tree;

int main() {
    using Clock = std::chrono::steady_clock;
    const auto deadline = Clock::now() + std::chrono::milliseconds(100);
    Solver solver(5);
    solver.add_edge(0, 1, 1);
    solver.add_edge(1, 2, 1);
    solver.add_edge(2, 3, 1);
    solver.add_edge(0, 4, 3);
    solver.add_edge(4, 3, 3);

    Solver::options opts;
    opts.preset_mode = Solver::preset::balanced;
    opts.time_limit_ms = 10;
    opts.deadline = deadline;
    opts.restart_limit = -1;
    if (Clock::now() >= deadline) {
        std::cout << "deadline reached\n";
        return 0;
    }
    const auto answer = solver.solve({0, 3}, opts);
    if (answer.ok()) std::cout << answer.cost << '\n';
    else std::cout << "not found within search\n";
}
```

通常は `3` を出力する。締切を扱うコードなので、出力は実行環境に依存する。

|項目・値|意味|
|---|---|
|`steady_clock`|経過時間を測るための時計。時刻合わせに影響される壁時計と区別する|
|`now() + milliseconds(100)`|この時点から100ミリ秒後という絶対的な時点|
|`time_limit_ms = 10`|その呼び出しで使う探索の相対的な予算。単位はミリ秒|
|`deadline = deadline`|探索を止めたい絶対的な時点|
|`restart_limit = -1`|再スタート数を自動設定する。有限予算があるbalanced/qualityでは、予算内で追加探索を続けられる設定になる|

両方を設定した場合、早い方の停止目標が使われる。`time_limit_ms = 0` は即時停止ではなく「相対時間制限なし」。`deadline` の既定値は `steady_clock::time_point::max()` で、絶対締切を設けない。

複数回呼ぶとき、`time_limit_ms` は各回の予算である。合計を抑えたいなら、外側で一度決めた同じ `deadline` を各回へ渡す。毎回 `now() + 100ms` と設定し直すと、全体の締切が後ろへずれる。

### 競技の制限時間と同じものではない

このライブラリの時間制限は、途中で時間を確認して止めるための目標であり、ハードな実行時間保証ではない。個々の最短路計算や候補の整理が、処理途中で即時中断されるとは限らない。通常の初期候補生成は短い予算でも実行する設計である。

加えて、通常engineの初回CSR構築時間は、相対探索予算から除外する経路がある。公開入力の確認なども含め、関数全体の経過時間が `time_limit_ms` 以下になるとは限らない。絶対deadlineの方が外側の予算を共有しやすいが、こちらも確認間隔による超過をゼロにはできない。

提出プログラムでは、入出力・後処理の余裕を確保し、外側でも時間を確認する。たとえば2秒制限に対する1950msという外側目標も、どんな入力でも間に合う保証ではなく、実測して余裕を決める必要がある。`normalize` は時間設定を受け取らないので、締切付き処理の外側から安易に大きな入力で呼び足さない。

## 17. 発展⑮：制約探索の上限と、既知の解の保持

**変更:** 時間とは別に、制約についてどれだけ探すかを設定する。0回のときの `solve` と `improve` の違いも見る。

### 動くコード — example16.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(5);
    solver.add_edge(0, 1, 1);
    solver.add_edge(1, 2, 1);
    solver.add_edge(2, 3, 1);
    solver.add_edge(0, 4, 3);
    solver.add_edge(4, 3, 3);
    Solver::selection_rules rules(5);
    rules.add_conflict(1, 2);
    Solver::query_context context;
    context.rules = &rules;

    Solver::options opts;
    opts.constraint_node_limit = 16;
    opts.constraint_depth_limit = 8;
    const auto normal = solver.solve({0, 3}, opts, context);
    if (!normal.ok()) return 1;
    std::cout << "normal=" << normal.cost << '\n';

    opts.constraint_node_limit = 0;
    const auto stopped = solver.solve({0, 3}, opts, context);
    const auto kept = solver.improve({0, 3}, {3, 4}, opts, context);
    if (!kept.ok()) return 1;
    std::cout << "solve_ok=" << stopped.ok() << '\n';
    std::cout << "kept=" << kept.cost << '\n';
}
```

出力は `normal=6`、`solve_ok=0`、`kept=6`。

`constraint_node_limit` は、制約付き探索で内部solverを評価する状態数の上限。`16` は、グラフの頂点数を16に制限する意味ではない。

`constraint_depth_limit` は、制約を満たすために追加の禁止を課して探す深さの上限。深さ0は、追加の衝突分岐をしない初期状態だけを許す。ノード上限0とは異なり、初期状態の評価は可能である。

両項目の `-1` は自動設定。0以上が明示指定で、`-2` 以下は `invalid_input` になる。ノード上限を増やしても、深さや締切の方で止まることがある。

上の後半ではノード上限0なので、一から探す `solve` は解を返せない。しかし `improve` は、合法な初期辺3・4を保持しているため、その費用6の解を返せる。期限切れのときにも、合法な連結初期解を保持する考え方は同じである。

これらは制約付き経路の設定であり、制約のない通常経路の探索回数を制限するものではない。また、実際の仕事量は「制約の状態数」だけでなく、各状態内の探索量にも依存する。`restart_limit` とは別の上限である。

v09の自動設定は次のとおり。有限予算とは、正の `time_limit_ms` または有限の `deadline` がある場合を指す。

|preset|予算なしのノード/深さ|有限予算ありのノード/深さ|
|---|---:|---:|
|`fast`|4 / 4|16 / 8|
|`balanced`|16 / 12|32 / 12|
|`quality`|48 / 24|48 / 24|

これは探索できる最大範囲であり、その数だけ必ず実行する設定ではない。時間予算なしのfastでは、合法な候補が見つかった時点で制約探索を終了する。

## 18. 発展⑯：最短路cacheとメモリ上限を使う

**変更:** 同じグラフで端子を少しずつ変えて解く。繰り返し計算を助ける距離表を明示的に使い、必要に応じて解放する。

### 動くコード — example17.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(100);
    for (int v = 1; v < 100; ++v) solver.add_edge(v - 1, v, 1);
    Solver::options opts;
    opts.path_mode_value = Solver::path_mode::terminal_cache;
    opts.memory_limit_bytes = 64ULL * 1024;
    opts.restart_limit = 0;

    const auto first = solver.solve({0, 99}, opts);
    const auto second = solver.solve({0, 49, 99}, opts);
    if (!first.ok() || !second.ok()) return 1;
    std::cout << first.cost << ' ' << second.cost << '\n';
    std::cout << "cache_bytes=" << second.stats.path_memory_bytes << '\n';

    solver.clear_terminal_cache();
    opts.memory_limit_bytes = 0;
    const auto third = solver.solve({0, 99}, opts);
    if (!third.ok()) return 1;
    std::cout << third.cost << " on_demand="
              << (third.stats.path_mode_used == Solver::path_mode::on_demand) << '\n';
}
```

通常の64bit GCC環境での出力は `99 99`、`cache_bytes=3600`、`99 on_demand=1`。

頂点 `0` から `99` まで一直線に並べ、隣接頂点間の費用を1にした。`v - 1` と `v` は辺の両端、最後の `1` は費用。どちらの端子集合でも、全長99の接続が必要である。

最短路cacheとは、各端子から各頂点へ最も安く行くための距離と、経路を復元する情報を保存した表のこと。この章ではその用途だけを押さえ、表の作り方は付録で扱う。

|`path_mode_value`|意味|
|---|---|
|`auto_select`|端子数・メモリ・時間などから自動選択。既定値|
|`terminal_cache`|端子ごとの距離表を使うことを要求する|
|`on_demand`|端子ごとの永続的な距離表を作らず、必要な計算を行う|

`memory_limit_bytes` は、その距離表を使うか判断するためのデータ量上限。単位はバイトで、`64ULL * 1024` は64KiB。`ULL` は符号なしの大きな整数型のリテラルを示す。既定値は256MiBである。

頂点数を $N$、重複除去後の端子数を $K$ とすると、一般的なGCC環境では距離表のデータ部分はおよそ次の大きさになる。

$$12 N K \text{ bytes}$$

1組につき距離の `long long` が8バイト、親辺番号の `int` が4バイトであるため。上の2回目は100頂点×3端子×12バイトで3600バイトとなる。

同じ端子の行は、通常グラフに変更がなければ再利用できる。端子集合が変わった場合も、共通する端子の行を再利用する。辺を追加すると古い距離は無効になる。

`terminal_cache` を明示しても、メモリ上限を超える場合や時間が尽きている場合は `on_demand` に切り替わる。また、**制約付きの探索分岐では指定によらず `on_demand` を使う**。禁止条件ごとに距離が変わるためである。`augment` の無料既設辺用の表も、通常グラフの永続表とは分けて扱う。

### メモリ上限を誤解しない

`memory_limit_bytes` は、プログラム全体の使用メモリ上限ではない。グラフ・作業領域・探索候補・コンテナ管理情報は別に必要である。また、以前確保したvectorの容量や、前の呼び出しで保持したcacheが、上限値を下げるだけで必ず即時解放されるわけではない。

そのため例では、上限を0にする前に `clear_terminal_cache()` を呼んだ。この関数は保持中のterminal cacheを解放するもので、グラフを消したり、すべての作業領域を解放したりする関数ではない。

### 統計で動作を確認する

`answer.stats` はその呼び出しの診断情報。`augmentation_result` にも同じ `stats` がある。

|項目|読み方|
|---|---|
|`path_mode_used`|内部engineで報告された距離計算モード。実際に何を使ったかの確認用|
|`path_memory_bytes`|その呼び出しで使ったterminal表のデータ量。全メモリ使用量ではない|
|`initial_solution_count`|初期候補生成処理で計上された回数。最終的な合法解の数ではない|
|`restart_count`|追加再スタートの実行回数|
|`local_move_count`|局所変更の試行として計上された回数|
|`improvement_count`|費用を改善する局所変更として計上された回数|
|`dijkstra_count`|最短路計算の呼び出し回数。Dijkstraの仕組みは付録で説明する|
|`edge_relaxation_count`|最短路計算で走査したarcについて計上される回数。採用辺数ではない|
|`constraint_nodes`|制約探索で評価した状態数|
|`conflict_candidates`|排他衝突を検出した候補数|
|`feasible_candidates`|制約探索等で合法として計上された候補数。最適解数ではない|

制約分岐をまたぐ回数系の統計は加算し、表のデータ量は最大値を取る。`path_mode_used` は集計された全処理の内訳ではない。早期終了や、既知の初期解をそのまま返す経路では、成功していても一部のカウンタが0になることがある。統計だけで成功判定をせず、`ok()` を使う。

## 19. 発展⑰：同じグラフ・制約オブジェクトを更新して使い回す

**変更:** solverを作り直さず、制約の追加・全削除・辺の追加を行う。

### 動くコード — example18.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
#include <vector>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(5);
    solver.add_edge(0, 1, 1);
    solver.add_edge(1, 2, 1);
    solver.add_edge(2, 3, 1);
    solver.add_edge(0, 4, 3);
    solver.add_edge(4, 3, 3);
    Solver::selection_rules rules(5);
    rules.add_conflict(1, 2);
    Solver::query_context context;
    context.rules = &rules;

    auto answer = solver.solve({0, 3}, context);
    if (!answer.ok()) return 1;
    std::cout << "restricted=" << answer.cost << '\n';
    rules.clear();
    answer = solver.solve({0, 3}, context);
    if (!answer.ok()) return 1;
    std::cout << "clear=" << answer.cost << '\n';

    const std::vector<int> group{1, 2, 4};
    rules.add_exclusive_group(group);
    std::cout << "groups=" << rules.group_count()
              << " rule_vertices=" << rules.vertex_count() << '\n';
    solver.add_edge(0, 3, 1);
    answer = solver.solve({0, 3}, context);
    if (!answer.ok()) return 1;
    std::cout << "added=" << answer.cost << '\n';
    context = Solver::query_context{};
    answer = solver.solve({0, 3}, context);
    if (!answer.ok()) return 1;
    std::cout << "plain=" << answer.cost << '\n';
}
```

出力は `restricted=6`、`clear=3`、`groups=1 rule_vertices=5`、`added=1`、`plain=1`。

`rules.clear()` は登録済みの排他グループをすべて削除する。制約対象の頂点数は変わらず、solverの辺やcontextの明示禁止も消さない。

`group_count()` は登録グループ数。排他ペアも内部では1グループなので、この数に含まれる。`rules.vertex_count()` は制約対象の頂点数であり、グループに現れる頂点の数ではない。

`context.rules` は同じ `rules` を参照しているので、その内容を更新すれば次の呼び出しへ反映される。solverの探索中に同時更新してはいけない。

`solver.add_edge(0, 3, 1)` は新しい辺を追加する。既存辺の番号は変わらない。最短路cacheなど、辺追加によって古くなる内部情報は無効化される。公開APIには辺の削除・費用変更・頂点数変更の関数はない。一時的な不使用はcontextで表し、恒久的にグラフを組み替えるならsolverを再構築する。

`context = Solver::query_context{}` は、そのcontextの全指定を初期状態へ戻す。参照していた `rules` 自体は消さないし、その内容も変更しない。前回の禁止条件がsolverへ自動的に持ち越されることもない。

## 20. 発展⑱：辺集合が不要なら、費用だけを得る

**変更:** `solve` の戻り値から取り出す代わりに、費用だけを返す `solve_cost` を使う。

### 動くコード — example19.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(3);
    solver.add_edge(0, 1, 3);
    const long long cost = solver.solve_cost({0, 1});
    const long long missing = solver.solve_cost({0, 2});
    std::cout << cost << '\n';
    std::cout << "failed=" << (missing == Solver::inf()) << '\n';
}
```

出力は `3` と `failed=1`。

`solve_cost(terminals)` は、内部で通常の `solve` を呼び、成功時は費用だけを返す。計算そのものが大幅に軽くなる専用評価関数ではない。`solve_cost(terminals, opts, context)` と、設定を省略した `solve_cost(terminals, context)` も使える。

失敗時は `Solver::inf()` を返す。この値は `long long` の最大値を4で割った整数であり、数学上の無限大ではない。費用が十分小さい上の例では、等値比較で失敗を判別できる。

ただし非常に大きい費用を扱う場合、この値だけで解の有無を区別する方法には限界がある。費用、失敗理由、辺集合、統計まで必要なら `solve` と `ok()` を使う。すでに `solve` の結果を持っているなら `answer.cost` を読むだけでよく、同じ問い合わせを `solve_cost` で計算し直す必要はない。

## 21. 発展⑲：seedを変えて複数回試し、良い答えを保持する

**変更:** 設定は同じまま、seedだけ変える。外側のプログラムで最良の合法解を残す。

### 動くコード — example20.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <cstdint>
#include <iostream>
#include <utility>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(4);
    solver.add_edge(0, 1, 2);
    solver.add_edge(1, 2, 2);
    solver.add_edge(1, 3, 2);
    solver.add_edge(0, 2, 7);
    solver.add_edge(0, 3, 7);
    solver.add_edge(2, 3, 7);
    Solver::options opts;
    opts.preset_mode = Solver::preset::quality;
    opts.restart_limit = 4;
    Solver::result best;

    for (std::uint64_t seed = 0; seed < 3; ++seed) {
        opts.seed = seed;
        auto answer = solver.solve({0, 2, 3}, opts);
        if (answer.ok() && (!best.ok() || answer.cost < best.cost))
            best = std::move(answer);
    }
    if (!best.ok()) return 1;
    std::cout << best.cost << '\n';
}
```

出力は `6`。この小さな例では何度試しても同じ費用だが、大きな入力で候補を比較する形は同じである。

`best` は初期状態では解を持たない。そこで「新しい結果が成功し、かつ、まだbestがないか、新しい方が安い」という条件で更新する。失敗した結果の費用と比較してはいけない。

`std::move(answer)` は、辺のvectorなどをbestへ移すC++の操作。この後のコードでは `answer` を再利用していない。

外側の3回の呼び出しと、各回の `restart_limit = 4` は別の繰り返しである。単一呼び出しの `improve` で既知の解を深く改善する方が適する場合もあり、seedを増やせば必ず効率がよくなるとは限らない。

実時間予算で使う場合は、発展⑭の共通deadlineと外側の時間確認をこのループへ適用する。同じ比較の中では、端子・グラフ・制約をそろえる。途中で条件が変われば、以前のbestが合法であるとは限らない。

## 22. 発展⑳：外側を保持し、一部分だけを解き直す

**変更:** 既存ネットワークを「変えない外側」と「作り直す内側」に分ける。外側の選択頂点は制約へ含め、内側に必要な接続だけを問い合わせる。

旧ネットワークは `0→1→2→3→4`、全体の端子は0と4である。後半の `2→3→4` を残し、前半の0から2だけを解き直す。

### 動くコード — example21.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
#include <vector>
using Solver = constrained_steiner_tree;

int main() {
    Solver solver(7);
    solver.add_edge(0, 1, 4); // 辺0: 旧内側
    solver.add_edge(1, 2, 4); // 辺1: 旧内側
    solver.add_edge(2, 3, 2); // 辺2: 固定する外側
    solver.add_edge(3, 4, 2); // 辺3: 固定する外側
    solver.add_edge(0, 5, 1); // 辺4: 新しい内側の候補
    solver.add_edge(5, 2, 1); // 辺5: 新しい内側の候補
    solver.add_edge(0, 6, 0); // 辺6: 安いが排他条件に抵触
    solver.add_edge(6, 2, 0); // 辺7: 安いが排他条件に抵触
    Solver::selection_rules rules(7);
    rules.add_conflict(4, 6);

    const std::vector<int> outside_edges{2, 3};
    const std::vector<int> outside_vertices{2, 3, 4};
    Solver::query_context local;
    local.rules = &rules;
    local.selected_outside = outside_vertices;
    local.forbidden_edges = outside_edges;

    const auto part = solver.repair({0, 2}, {0, 1}, local);
    if (!part.ok()) return 1;
    auto network = outside_edges;
    network.insert(network.end(), part.edges.begin(), part.edges.end());
    long long total_cost = 0;
    for (int id : network) total_cost += solver.edge(id).cost;
    std::cout << "local=" << part.cost << " total=" << total_cost << '\n';
}
```

出力は `local=2 total=6`。旧内側の費用は8、外側の費用は4だったため、全体を12から6へ減らせた。

この例で指定したものを分解する。

1. `outside_edges = {2, 3}` は、本当の全体解に残す辺番号。外側の0-indexed辺番号である。
2. `outside_vertices = {2, 3, 4}` は、その端点として選択済みの頂点。ここから4が制約に入り、排他相手の6を経由する費用0の候補が使えなくなる。
3. 内側の端子 `{0, 2}` は、今回つなぎ直す両端。頂点2は外側へ戻る境界である。元の全体端子 `{0, 4}` をそのまま渡してはいない。
4. 内側の材料 `{0, 1}` は旧内側の辺番号。取り外してもよいので `repair` に渡す。
5. `local.forbidden_edges = outside_edges` は、内側の解に外側の辺を重複して取り込ませないための、一時的な局所問い合わせの条件である。外側の辺を全体解から削除する指示ではない。

solverには元のグラフを保持しており、頂点番号を振り直した小さなグラフは作っていない。ただし、グラフを切り出さなければ常に同じ速度になる、という保証はない。

### 自動で行われないこと

`selected_outside` だけで「内側以外を編集禁止」にできるわけではない。外側の辺を保持して結合するのは利用者側であり、どの頂点を境界端子にするかも利用者が決める。

この例では外側が2から4へつながっているため、内側で0と2をつなげば全体の0と4がつながる。外側が複数成分に分かれる場合は、必要な成分が最終的に接続されるよう、境界の設定を見直す。既存グラフの他領域へ入ってほしくないなら、必要な頂点・辺の禁止も追加する。

内側と外側で辺が重複しないため、費用をそのまま足せる。外側の辺を無料で通りながら接続を探したい用途なら、この禁止方式ではなく、外側をbaseにした `augment` が自然である。

全体制約の下で外側がすでに合法であることを確認し、外側の全使用頂点を登録する。この例の `local` は局所用の禁止を含むので、完成した全体ネットワークへそのまま適用する検証条件ではない。最終的な採否も、元問題の全体の費用・制約で判断する。

## 23. 発展㉑：空の端子・重複・数値範囲を確認する

**変更:** 境界的な入力で、接続義務と制約の違いを確認する。

### 動くコード — example22.cpp

```cpp
#include "constrained_steiner_tree_v09.hpp"
#include <iostream>
using Solver = constrained_steiner_tree;

int main() {
    Solver empty(0);
    const auto zero = empty.solve({});
    if (!zero.ok()) return 1;
    std::cout << "empty=" << zero.cost << '\n';

    Solver solver(2);
    const int existing = solver.add_edge(0, 1, 5);
    const auto single = solver.solve({1, 1});
    const auto added = solver.augment({}, {existing});
    if (!single.ok() || !added.ok()) return 1;
    std::cout << "single=" << single.cost << '\n';
    std::cout << "added=" << added.added_cost << '\n';

    Solver::selection_rules rules(2);
    rules.add_conflict(0, 1);
    Solver::query_context context;
    context.rules = &rules;
    const auto conflict = solver.augment({}, {existing}, context);
    std::cout << "base_conflict="
              << (conflict.state == Solver::status::proven_infeasible) << '\n';
}
```

出力は `empty=0`、`single=0`、`added=0`、`base_conflict=1`。

重複した端子 `{1, 1}` は同じ端子1個として扱われる。したがって接続用の辺は不要。ただし、最後のケースのように撤去不能な既設辺の端点が排他条件を破っていれば、端子が空でも不可能になる。

端子・候補辺・base辺の重複は許容されるが、頂点番号と辺番号の範囲は正しくなければならない。自己loopや並行辺、費用0の辺も登録できる。ただし、`add_conflict(u, u)` は同じ頂点を禁止するためのAPIではない。禁止したいなら `forbidden_vertices` を使う。

### 大きな費用を使うとき

辺費用は非負の `long long`。浮動小数点の重みを直接渡して、小数部分を保持するAPIではない。必要なら、利用者側で丸め方と倍率を決めて整数へ変換する。

最短路計算で新たに探索する経路の費用は `Solver::inf()` 未満に収める。`inf()` は `LLONG_MAX / 4`。一方、`normalize` で渡した木の合計や、`improve` が保持する合法初期木の合計は、`long long` の範囲内ならこの値より大きくても表現できる。

合計が `long long` を超える候補は、負数へ巻き戻した「安い解」として採用せず、成功扱いにしない。数値範囲の問題による失敗は、グラフの接続が数学的に不可能だという証明ではない。実用上は、探索経路と最終費用が十分小さく収まる単位を選び、利用者側で合計する部分にもoverflowへ注意する。

## 24. 読み終えた後のAPI早見表

### 24.1 最初にどの操作を選ぶか

|手元にあるもの・目的|選ぶ操作|参照先|
|---|---|---|
|一から接続を作る|`solve`|最小コード・発展③|
|候補辺の範囲を変えず整理する|`normalize`|発展⑨|
|合法な連結解を保持し、改善する|`improve`|発展⑩・⑮|
|非連結・違反を含む旧辺集合を直す|`repair`|発展⑪|
|撤去不能な辺を無料で使い、新しい辺だけ追加する|`augment`|発展⑫・⑬|
|費用だけ得たい|`solve_cost`|発展⑱|
|外側の選択と矛盾しない部分問題を解く|`selected_outside` と適切な端子・禁止条件|発展⑦・⑳|

### 24.2 公開関数と引数

ここで `terminals` は頂点番号のvector、各 `*_edges` は辺番号のvector、`opts` は `options`、`context` は `query_context` を表す。

|関数|返すもの・注意|
|---|---|
|`constrained_steiner_tree(n)`|頂点数nの空グラフ。nは0以上|
|`add_edge(u, v, cost)`|登録順の辺番号。u、vは頂点番号、costは非負|
|`edge(id)`|両端と登録費用の参照。idの有効範囲は利用者が守る|
|`vertex_count()` / `edge_count()`|登録頂点数 / 登録辺数|
|`solve(terminals, opts, context)`|`result`|
|`normalize(terminals, candidate_edges, context)`|`result`。optsは受け取らない|
|`improve(terminals, initial_edges, opts, context)`|`result`|
|`repair(terminals, partial_edges, opts, context)`|`result`|
|`augment(terminals, base_edges, opts, context)`|`augmentation_result`|
|`solve_cost(terminals, opts, context)`|費用。失敗時は `inf()`|
|`clear_terminal_cache()`|terminal表を解放する。返り値なし|
|`inf()`|最短路の到達不能値。`static` なので `Solver::inf()` と呼べる|
|`selection_rules(n)`|n頂点用の制約集合。solverと同じnを指定する|
|`rules.add_conflict(u, v)`|異なる2頂点の排他を登録。返り値なし|
|`rules.add_exclusive_group(vertices)`|高々1頂点のグループを登録。返り値なし|
|`rules.clear()`|全排他グループを削除する|
|`rules.group_count()` / `rules.vertex_count()`|登録グループ数 / 対象頂点数|
|`answer.ok()`|`state == feasible` かどうか|

`opts` と `context` は省略できる。制約だけを指定する呼び出しも、`normalize` 以外の上記探索関数に用意されている。たとえば `repair(terminals, partial_edges, context)`。`normalize` はもともと第3引数がcontextである。

### 24.3 設定を一覧で振り返る

|`options` の項目|既定値|本書で説明した場所|
|---|---|---|
|`preset_mode`|`balanced`|発展②|
|`path_mode_value`|`auto_select`|発展⑯|
|`seed`|`0`|発展②・⑲|
|`time_limit_ms`|`0`。相対制限なし|発展⑭|
|`restart_limit`|`-1`。自動|発展②・⑭|
|`memory_limit_bytes`|256MiB|発展⑯|
|`deadline`|時計の最大時点。絶対制限なし|発展⑭|
|`constraint_node_limit`|`-1`。自動|発展⑮|
|`constraint_depth_limit`|`-1`。自動|発展⑮|

`query_context` の `rules` は既定で `nullptr`、`forbidden_vertices`・`forbidden_edges`・`selected_outside` は既定で空。resultの費用と辺集合の意味、全status、全statsはそれぞれ最小コード・発展①・⑧・⑫・⑯で説明した。

### 24.4 よくある取り違え

- `selected_outside` は、接続すべき端子や撤去不能な辺の指定ではない。
- `repair` に渡した辺は固定ではない。固定したいなら `augment` のbaseを使う。
- `augment` の返却辺は全体ではなく追加分だけ。
- `normalize` は、入力にない辺を補う処理ではない。
- `not_found` は、不可能という証明ではない。
- `quality` や大きな上限は、全入力で費用がよくなる保証ではない。
- 同じsolverを複数スレッドから同時に呼ばない。探索APIが `const` でも内部作業領域を更新する。
- solverはコピー用のAPIを提供しない。v09では通常のコピー構築・コピー代入もできない。別個に構築するか、所有を移すならC++のmoveを使う。
- contextは配列やrulesを所有しない。参照元を破棄・再配置してから呼ばない。
- 辺番号はそのsolverの登録順に属する。別の登録順で作ったsolverへ答えの辺番号をそのまま移さない。

## 付録A. ライブラリ実装：まず全体の流れ

ここからは使い方ではなく、内部で何をしているかを説明する。公開APIを利用するだけなら、この付録を先に理解する必要はない。

### A.1 二つの層に分けて考える

内部は、おおむね「通常の接続候補を作る層」と「候補が頂点排他を守るように探索を分ける層」からなる。

単に使えない頂点・辺を除くだけなら、残ったグラフで経路を探せばよい。しかし「1を使うなら2を使わない」という条件は、1や2を常に消せばよいわけではない。そこで、まず候補を作り、その候補に衝突があれば、どちらかを禁止した別の状態を試す。

`solve` の概略は次の順序になる。

1. 公開入力のIDなどを確認し、端子を整列・重複除去する。
2. 制約があれば、固定選択や明示禁止の矛盾を確認する。
3. 現在の禁止条件で、通常のSteiner接続候補を作る。
4. 候補を正規化し、使う頂点の排他衝突を調べる。
5. 合法なら最良解を更新する。衝突があれば禁止条件を分けて探索する。
6. 時間・ノード・深さなどの条件で止め、その時点の最良の合法解を返す。

制約のない呼び出しは、外側の制約探索を通らず通常engineへ直接進む。`normalize`、`repair`、`improve` は材料が異なるので、すべてがこの候補生成を丸ごと同じ順で実行するわけではない。違いはB.18で整理する。

### A.2 通常engineの大まかな順序

`solve` の内部では、まず軽い初期候補を作り、その後に必要ならcacheを構築する。次に逐次接続や再スタートで候補を増やし、よい候補を組み合わせ、局所的な置き換えを行う。balanced/qualityでは、最後に少し大きい部分のつなぎ直しも試す。

この順番には「表を作るだけで予算を使い切る前に、まず答えの候補を持つ」「初期候補の多様性を確保してから、良いものを磨く」という意図がある。

有限の探索予算では、おおむね最初の4分の3までを候補生成・組合せに使い、残りを局所改善へ回す。これは厳密にその時間だけ使う予約ではなく、途中の時間確認に使う閾値である。

## 付録B. 各アルゴリズムとデータ構造

### B.1 グラフを連続した配列へまとめる — CSR

探索時には「今いる頂点から出る辺」を何度も列挙する。そのたびに全辺を探すと無駄が多い。

そこで、頂点ごとの隣接辺を連続した配列に詰める。各頂点には自分の範囲の開始位置を持たせる。頂点vに対応する範囲は `start[v]` 以上 `start[v + 1]` 未満。これがCSRと呼ばれる表現である。

無向辺1本は、探索用には両方向のarc2本になる。それぞれに隣の頂点、元の辺番号、費用を持たせる。自己loopは接続を広げないので、この隣接arcには入れない。

CSRは最初に必要になった時点で構築し、同じグラフでは再利用する。辺追加時は再構築が必要になる。制約付きの内部engineは必要になってから別途作られ、辺追加も通常engineと同期する。このため制約付き呼び出しを使うと、グラフと作業領域の保持量が増えることがある。

禁止条件ごとにCSRを作り直すのではなく、辺の使用可否を示すmaskを切り替える。最短路を繰り返す分岐では、端点禁止と辺禁止を辺ごとの配列へまとめる。最短路が不要な軽い操作では、その全辺前処理を遅らせる。

### B.2 最短路の基本 — Dijkstra

ある頂点から他の頂点までの最小費用を求めたいとき、各頂点について「今分かっている最小の費用」を持つ。出発点だけは0、それ以外はまだ不明とする。

次に、未処理の中で最も安い頂点を取り出し、そこから隣へ進んだ場合の費用を調べる。今まで分かっていた費用より安ければ更新する。これを繰り返すのがDijkstra法である。

費用が非負なので、安い順に処理する考え方が成り立つ。負の費用を扱わないという公開APIの前提は、この内部計算にも関係している。

内部には次の3種類がある。

|処理|使い道|省略・再利用するもの|
|---|---|---|
|全体の距離計算 `dijkstra_full`|端子ごとの距離表、近い端子の担当領域を作る|必要に応じて、どの出発端子から来たかも記録する|
|目標までの距離計算 `dijkstra_to_target`|残った木のどこかへ安くつなぎ直す|目標へ着いたら終了。改善できない距離でも打ち切る|
|増分計算 `incremental_dijkstra`|木へ頂点を追加しながら距離を更新する|前の距離を保ち、新しく費用0の出発点になった部分から改善を伝える|

複数の出発点を同時に費用0で登録することもできる。これは「その集合のどこから出てもよい」場合の距離になる。架空の無料な出発頂点を全出発点につないだと考えると理解しやすい。

経路を復元するため、各頂点へ最後に入った親辺番号も記録する。答えの頂点から親辺をたどれば、出発点までの辺が集まる。復元回数には上限があり、不整合があっても際限なくたどり続けない。

打ち切り型では、各回に全頂点の記憶を初期化せず、今回訪問したことを示す世代番号を付ける。今回の世代でない距離は未訪問として扱い、必要な頂点だけ初期化する。

### B.3 安い頂点を取り出す — radix heap

Dijkstraでは、一番安い未処理候補を繰り返し取り出す。一般的には優先度付きキューを使うが、本実装では非負整数距離の性質を利用したradix heapを使う。

取り出す距離は単調に増える。直前に取り出した値と新しい候補値の二進表現が、どの桁から異なるかによってbucketを分ける。最小のbucketが空なら、次のbucketの最小値を基準に、要素を細かいbucketへ分配し直す。

64bitのキーに対して65個のbucketを持つ。0距離や同じ距離の候補も扱う。これは取り出しを効率化するデータ構造であり、元問題の費用を近似したり丸めたりする処理ではない。

### B.4 辺の順序をそろえる — 安定radix sort

費用の小さい辺から調べたい場面では、整数費用を1バイトずつ見て並べ替える。全要素で同じ値のバイトは順序へ影響しないため、その桁の処理を省く。

同じ費用の要素は入力順を保つ「安定」な並べ替えである。元の辺番号順から始めれば、費用が同じときも辺番号順を保てる。費用順に並べた元グラフの辺列は、同じグラフで再利用する。

これは費用の値そのものを変える処理ではない。一般の候補辺集合など、別の箇所では比較による通常のsortも使う。

### B.5 候補を木へ整える — 併合と葉刈り

複数の経路を足し合わせると、同じ辺の重複や閉路、不要な枝が生じる。そのまま費用を足さず、次のように整理する。

1. 候補辺の重複を除き、費用順に見る。無料の既設辺は費用0として扱う。
2. 別々の連結成分をつなぐ辺だけを採用する。
3. 端子が同じ成分につながったら、必要な接続が得られたと判断する。
4. 端子ではない葉を繰り返し削除する。

葉とは、木の中で接続辺が1本だけの頂点。その頂点が端子でなければ、その枝を取り除いても端子どうしの接続は壊れない。

連結成分の管理にはDSU、別名Union-Findを使う。各頂点がどの成分に属すかを高速に調べ、辺を選んだら2成分をまとめる。成分の大きさと、端子を含む成分数も管理する。

葉刈りには各頂点の次数と、接続辺番号をXORした値を持つ。次数1ならXORの値が唯一の接続辺番号になる。辺を外したら隣の次数とXORを更新するので、葉刈り専用の隣接リストを作らずに済む。

最後に残った辺の実際の費用を、overflowを確認しながら加算する。無料の既設辺はこの追加費用へ入れない。`normalize` が「候補内で整理する」であって「最適Steiner treeを厳密に解く」ではない理由は、辺の採用をこのような選択手順で行うためである。

### B.6 初期候補を作る — Voronoiと端子間の木

まず全端子を同時に出発点として最短路計算を行い、各頂点を「どの端子から一番安く到達できるか」で担当分けする。この担当領域を、ここではVoronoi領域と呼ぶ。幾何学的な平面座標は必要ない。

異なる担当領域の境界にある辺を見る。その辺の両端をu、v、辺をeとすると、両側の端子をつなぐ候補経路の費用は次のようになる。

$$d(u) + w_ {e} + d(v)$$

$d(u)$ はuの担当端子からuまでの距離、$d(v)$ はvの担当端子からvまでの距離。$w_ {e}$ はこの候補生成で使う辺eの費用であり、通常は登録費用、無料扱いの辺なら0である。

この候補を費用順に並べ、端子の成分どうしをつなぐものを選ぶ。端子間の候補グラフに対して最小全域木の考え方を使い、選んだものを元グラフの親経路へ展開する。その後、前節の正規化で重複や不要部分を除く。

元グラフの全頂点を結ぶ最小全域木を求めるわけではない。端子を結ぶための候補を作る工程であり、Steiner問題全体の厳密最適性を保証するものでもない。

### B.7 別の形の初期候補 — 逐次最短路接続

一つの端子を出発点として木を育てる方法も使う。

1. まだ木に入っていない端子の、現在の木への接続費用を調べる。
2. 安い候補をいくつか選び、その中から一つを決める。
3. その接続経路を木へ追加する。
4. 新しく加わった頂点によって、他の端子が安く接続できないか更新する。

常に最安の1候補だけを選ぶ場合もあれば、安い候補の上位数個から乱数で選ぶ場合もある。この短い候補リストがRCLである。v09の再スタートではその幅を1〜4で変え、開始端子も変えて探索の偏りを減らす。内部乱数には `std::mt19937_64` を使う。

cacheを使う経路では、各未接続端子の現在の最良接続先を記録し、木へ新しく入った頂点だけを調べて更新する。cacheなしでは、新しく木へ入った頂点を費用0の出発点として、増分Dijkstraで距離を更新する。

大きすぎる端子集合でこの生成を繰り返すと重いので、逐次候補生成には端子数の内部上限がある。v09ではcacheあり192、on-demand512。これはライブラリ全体の端子数制限ではなく、この工夫を使うかどうかの条件であり、他の候補生成は残る。

### B.8 距離表の保存と再利用

距離表は「端子ごとの行」を持つ一次元の配列で、距離と親辺番号を別々に並べる。1行の長さは頂点数N。端子iの行と頂点vに対応する位置は `i * N + v` である。

同じ端子集合・同じグラフなら行全体を再利用する。端子集合が一部だけ変わった場合は、次の手順になる。

1. 前後で共通する端子の行を見つける。
2. 共通行を前から詰めて保存する。
3. 配列を新しい行数へ調整する。
4. 上書き事故が起こらないよう後ろから新しい位置へ広げる。
5. 新しい端子の行だけ最短路計算で追加する。

端子を昇順にしていることが、共通行の安全な移動にも役立つ。辺追加後は古い経路が最短とは限らないため、再利用できる状態を無効にする。

自動選択は、fastなら基本的にon-demandを選ぶ。それ以外は、端子数、距離表のサイズ、辺数と端子数から見積もる構築負担、残り予算を見てcacheを選ぶ。v09では自動cacheの端子数上限は32で、表は設定メモリの半分以下を目安にする。これらは内部の判断定数であり、公開機能の正しさを決める条件ではない。

### B.9 よい候補を残し、組み合わせる — elite

費用が最小の候補1個だけでなく、少数の異なるよい候補を残す。同じ費用かつ同じ辺集合は重複として除く。

2候補の辺を合わせてから正規化すると、一方のよい部分と他方のよい部分を組み合わせた候補ができることがある。単に2候補の費用を足すのではない。

v09ではeliteの上限はbalancedで4、qualityで8、fastでは使わない。qualityでは途中のよい候補も種として別途記憶し、それぞれを局所改善する。現在最良の候補からしか探索しないと、同じ形の周辺だけを調べ続けやすいためである。

### B.10 局所改善①：今使っている頂点で辺を選び直す

現在の木で使用している頂点はそのままに、元グラフでそれらの頂点間に存在する別の辺も候補へ入れる。そして費用の安い辺で接続を作り直し、葉刈りする。これが誘導部分グラフでのMST改善である。

中継頂点を新しく増やさなくても、辺の選び方だけで改善する場合がある。既設辺を無料で使う場合は、その端点も対象へ含め、既設辺を先に成分併合へ利用する。

更新するのは、正規化後の候補費用が現在より**厳密に小さい**ときだけ。fastでも使う、比較的軽い改善である。

### B.11 局所改善②：端子の枝を付け替える

現在の木で葉になっている端子を探し、そこから最初の分岐点または別の端子までを一本の枝として取り出す。高い枝から、試行数上限の範囲で調べる。

枝を外したあと、その端子から残りの木のどこかへ、より安く到達できないか探す。cacheがあれば距離表を見て、なければ目標集合に向けた打ち切りDijkstraを行う。元の枝の費用以上なら改善できないので、それが探索の上限になる。

新しい経路が残りの木へ途中で入ったなら、最初に入ったところで止める。そこから先を余分に足すと、不要な辺や閉路を作るためである。無料既設辺を含む場合は、置き換えた候補を再び正規化して費用を確認する。

### B.12 局所改善③：key-pathを交換する

次数とは、その頂点に接続する木の辺の本数である。端子、または次数が2でない頂点をkey vertexと考える。中間がすべて次数2の非端子である最大の道筋がkey-pathになる。

key-pathを一本外すと、その両端側に残った木の成分を考えられる。片側の成分のどの頂点からでも出発できるようにし、もう片側のどこかへ、外した道筋より安くつなぐ経路を探す。

固定された2つの端点だけを結び直すのではない。残った成分どうしを結べばよいので、接続位置をずらせる。sourceとしては頂点数の小さい側を選び、目標へ届いたところで最短路計算を止める。

木の探索用表現では無向辺を2本のarcで持ち、arc番号を2で割ると木内の辺番号が得られる。木内番号と、公開APIで使うグラフ全体の辺番号は別であり、置き換えるときに対応を使う。

### B.13 最終改善①：3方向の分岐点を移す

非端子の次数3の分岐から延びる3本のkey-pathを外す。通常の木なら、その外側に3つの成分が残る。それぞれの成分から各頂点vへの最小追加費用を求める。

成分1、2、3からの距離を、それぞれ $d_ {1}(v)$、$d_ {2}(v)$、$d_ {3}(v)$ とする。接合候補vを次の距離和で評価する。

$$\min_ {v \in V} \bigl(d_ {1}(v) + d_ {2}(v) + d_ {3}(v)\bigr)$$

$V$ は元グラフの全頂点。各距離は残した森を無料で使う設定で計算する。距離和が小さい場所を新しい分岐候補にして、3本の経路を復元する。経路の重複があり得るので、最後は正規化した辺集合の実費で改善を判断する。

外した部分の費用で元の接続へ戻せるため、それより良くなり得ない距離まで探索する必要はない。また、ある2成分をつなぐために最低限必要な距離を下界として使い、他の計算の範囲を絞る。

### B.14 最終改善②：分岐周辺を壊して再構成する

一つの非端子分岐に接するkey-pathをまとめて外し、残った辺を材料として初期候補生成をやり直す。

枝一本の交換より変更範囲が広いので、一本ずつの改善では抜けられない形から移れる可能性がある。一方で重くなるため、対象の分岐数を制限する。

残った辺は候補生成中に優先して使うため無料扱いするが、通常の問題でその辺の本当の費用が消えるわけではない。戻ってきた候補を元の費用で正規化し、安くなった場合だけ採用する。`augment` の本当に追加費用0である既設辺とは区別する。

### B.15 最終改善③：4つの成分をつなぎ直す

隣接する次数3の非端子分岐を2つ選び、その周辺の道筋を外す。通常の木なら4つの外側成分が残る。3成分の接合より少し大きい組み替えである。

各成分から各頂点への距離を計算した後、4成分を2成分ずつに分けて考える。分け方は「1・2と3・4」「1・3と2・4」「1・4と2・3」の3通り。

成分a、bをどこかで一度まとめ、そこから頂点vへ延ばす費用の尺度を次のように作る。

$$p_ {ab}(v) = \min_ {u \in V} \bigl(d_ {a}(u) + d_ {b}(u) + d(u,v)\bigr)$$

aとbは成分の番号、uは2成分を合流させる候補頂点、vはそこから延ばす先。$d_ {a}(u)$ と $d_ {b}(u)$ はそれぞれの成分からuまでの費用で、$d(u,v)$ はuからvまでの経路費用。残した森は無料で使う。

各uに初期費用 $d_ {a}(u) + d_ {b}(u)$ を置き、そこから距離を伝えることで、すべてのuを一つずつ別計算することを避ける。次に相補的な2組の $p$ を同じvで足し、よい結合位置を選ぶ。3種類の分け方を比べ、対応する経路を復元する。

ここでも元の周辺費用を上限にし、残り2成分を結ぶ最低費用を下界として探索を削る。経路の重複や無料辺を含め、最後は正規化後の実費で判断する。この計算を一部の周辺だけで行うのであって、グラフ全体のSteiner問題を厳密に解くものではない。

### B.16 排他制約の準備と衝突分岐

固定扱いする頂点を最初に集める。

- すべての端子。
- `selected_outside` の頂点。
- `augment` の既設辺の全端点。

固定頂点が禁止されていたり、固定頂点どうしが同じ排他グループへ入っていたりすれば、取り除くことができないため `proven_infeasible` となる。固定頂点に衝突する他の頂点は、あらかじめ禁止できる。

制約定義には、グループから所属頂点への列と、頂点から所属グループへの疎な索引がある。前者は固定頂点に対する禁止の伝播、後者は候補の使用頂点の検査に役立つ。役割が逆なので、同じ情報の無意味な二重保持ではない。

通常engineが返した候補に頂点aとbの衝突があれば、合法解では少なくとも片方を使えない。そこで次の状態へ分ける。

1. aを禁止して候補を探す。
2. bを禁止して候補を探す。

禁止集合は整列・重複除去し、同じ禁止集合を繰り返し評価しないよう記憶する。探索にはstackを使い、深さ優先の順序で進める。頂点の次数も分岐の順序へ利用する。

禁止条件を変えてもCSRと元辺の費用順は変わらないので再利用し、使用可否maskを更新する。制約付き分岐では通常のterminal cacheを持ち越さず、on-demandで計算する。分岐ごとの乱数seedにも禁止集合を反映する。

候補が合法なら最良解へ反映し、衝突していれば制限内でさらに分ける。ただし各状態で返るのはヒューリスティックな候補であり、状態数・深さ・時間にも制限がある。全体最適性や、失敗したときの不存在を証明する分枝限定法ではない。

### B.17 時間予算の配分

各通常engineは、相対予算と絶対deadlineの早い方を停止目標として持つ。一定の処理の区切りでその時点を超えたか確認し、追加探索を止める。

制約探索では、残り時間を残りノード数に応じて割り、1状態だけで時間を使い切りにくくする。v09では除数を1〜32の範囲へ収め、各内部engineにも共通の絶対deadlineを渡す。初期候補生成や一まとまりの最短路計算などがあるため、割り当てたミリ秒数を厳密に守るプリエンプティブな実行制御ではない。

遠いdeadlineで「予算の4分の3」を計算するときは、巨大な時間差に先に3を掛けてoverflowしないよう、商と余りに分けて計算する。

### B.18 各公開操作が内部で使うもの

|入口|内部の流れの特徴|
|---|---|
|`solve`|Voronoi候補、逐次接続・再スタート、elite組合せ、局所改善、最後の広めの改善|
|`normalize`|渡された辺だけを費用順に整理し、木化・葉刈り。制約付きでは費用順を一度作り全分岐で再利用|
|`improve`|初期辺を正規化して保持し、誘導MST、枝再接続、key-path交換、最終改善を行う|
|`repair`|部分辺を正規化できるか試し、部分辺を材料にした候補と通常候補を比較し、局所改善を行う|
|`augment`|通常探索と制御を共有し、既設辺を費用0として扱う。最後は新規辺だけ返す|

`improve` の制約付き経路では、内部engineが正規化した初期木を外側へ渡し、よい合法候補が発見できなかった場合にも保持する。engineを一度も呼べなかった場合は、返却用の初期木を別途正規化する。

`augment` では、内部の作業用の木から不要な既設辺が外れる場合があっても、利用者の既設網を撤去するわけではない。既設辺の全端点は、外側の制約準備ですでに固定選択として保持している。

通常の探索と無料辺ありの探索、禁止条件ありとなしは、内部のテンプレート引数で切り替える。必要ない経路へ毎回の条件分岐を追加しにくくしつつ、主要な処理を共有している。

### B.19 計算量・メモリをどう考えるか

Nを頂点数、Mを登録辺数、Kを端子数、Lを入力候補辺数、Bを実際に評価する制約状態数とする。

|部分|大きくなる要因|
|---|---|
|グラフ・CSR・通常の作業配列|基本的にNとMに比例する領域。制約engineの保持分も別に必要|
|terminal cache|N×Kに比例する領域。新規行には端子ごとの最短路計算が必要|
|候補の正規化|候補L本の並べ替え、連結管理、木の葉刈り|
|逐次接続|端子を追加する回数と、その都度の距離更新|
|局所改善|調べる枝・key-path・分岐の数と、各最短路計算の範囲|
|制約付き探索|通常engine相当の仕事をB状態ぶん繰り返す可能性|

したがって、辺数だけでは処理時間は決まらない。端子数、制約の衝突の起き方、cacheの再利用、初期解の形も効く。プリセットや上限は、それらの仕事量を調整するための設定である。

メモリ制限が厳しい場合、terminal cacheだけでなく、グラフの二重保持、作業配列、複数の候補木、制約の索引も考慮する。constな探索APIでもこれらの再利用状態を更新するため、同じsolverを同時実行せず、スレッドごとに独立したsolverを使う。

### B.20 このライブラリだけでは直接扱わない条件

対象は非負整数費用の無向グラフと、頂点選択の排他・明示禁止である。辺容量、次数上限、一般の論理式、辺どうしの排他、グループから必ず1個選ぶ条件、頂点の追加費用などを、そのまま指定する公開APIはない。

これらを持つ元問題へ利用するときは、変換によって意味を保てるか、外側で候補を検証すべきかを別途検討する。「何でもcontextへ入れれば解ける」汎用制約solverではない。

## 検証と同梱物

本書の22個のC++コードは、対象のv09ヘッダーに対し、GCC 13.3.0・C++20・`-O2 -Wall -Wextra -Wshadow -Wconversion -Werror` でコンパイル・実行を確認済み。時間依存の例を除き、本文の出力と照合済みである。時間依存の例は、正常終了し、説明したいずれかの出力になることを確認した。

数式45箇所は、数式拡張なしのGFM Markdown前処理を通しても、LaTeX部分が変化しないことを確認した。表示数式7箇所はそれぞれ1行に置き、HTMLタグやHTMLアンカーは使用していない。すべてのMarkdownビューアーでの表示を保証するものではない。

同梱の例集では、各例を `example01.cpp` から `example22.cpp` として個別に収録する。ヘッダー、検証スクリプト、実行結果も付属する。コードはライブラリの内部や過去の会話を参照せず、このMarkdownとヘッダーだけでも理解・実行できる構成である。

参照したソースのSHA-256: `583d27cc503981a46908069f7e501f57ce531bebdf41126ba4b2301689f08c9c`。
