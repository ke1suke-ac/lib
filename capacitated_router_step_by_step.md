# CapacitatedRouter v05 ステップバイステップガイド

容量を分け合う複数の経路を、C++で少しずつ組み立てて学ぶ

対象は `capacitated_router_v05.hpp` です。数学は高校までの知識を前提とします。C++の変数、配列、関数、`for` 文を知っていれば読み進められます。ライブラリ固有の用語や、`std::optional` などの使い方は登場時に説明します。

このガイドの27本のC++例は、それぞれ独立したプログラムです。前の例からコードを補う必要はありません。例題一式には同じヘッダ、全例の `.cpp`、実行結果、検証スクリプトを含めています。本文の数式は `$...$` と `$$...$$` に対応するMarkdownリーダーを想定します。

| 章 | 学ぶこと | コード例 |
|---|---|---|
| 1〜3 | 対象問題、目的関数、使い分け、実行準備 | 準備 |
| 4〜12 | 経路、容量、費用、共有資源、要求別上限 | 01〜09 |
| 13〜15 | 接続の選択と結果の診断 | 10〜12 |
| 16〜23 | 初期解、追加計算、時間、固定、ターン更新 | 13〜20 |
| 24〜28 | 独自の費用・通行条件、小数 | 21〜25 |
| 29〜30 | 状態展開と複数試行 | 26〜27 |
| 31〜33 | API索引、実装付録、検証方法 | 参照用 |

## 1. 何を解くライブラリか

### 1.1 道と、道を使いたい要求

交差点を「頂点」、通行できる道を「辺」として表します。このような点と線の集まりを**グラフ**と呼びます。基本の辺には向きがあり、`0 → 1` と `1 → 0` は別の辺です。

「頂点0から頂点2まで荷物を運びたい」という注文が**要求**です。各要求に対して、出発点から目的地までの**1本の経路**を選びます。経路は辺を順番につないだもので、同じ頂点を2回通りません。この条件を満たす経路を**単純路**といいます。

たとえば、次の2通りの道があるとします。

| 経路 | 辺ごとの費用 | 合計費用 | 通せる荷物の量 |
|---|---|---:|---:|
| `0 → 1 → 2` | 1、1 | 2 | 各辺で1まで |
| `0 → 2` | 5 | 5 | 1まで |

量1の要求が1件なら、費用2の経路が有利です。同じ要求が2件あると、両方を安い経路に通すことはできません。1件ずつに分けると、容量を守りながら合計費用7で運べます。

この「複数の要求の経路を、一緒に考えて決める」ことが本ライブラリの役目です。要求の量を2にしても、1ずつ別々の道へ分割することはできません。量2のすべてを1本の経路に通します。

### 1.2 容量を「資源」としてまとめる

容量は道だけにあるとは限りません。駅の処理能力、通信回線の帯域、複数の道が共同で使う設備などもあります。これらをまとめて**資源**と呼びます。

辺、頂点、要求に「どの資源を、いくら使うか」を付けます。同じ資源を使うすべての要求の消費量を合計し、容量以下に収めます。別の用途がすでに使っている量も、**背景使用量**として加えられます。

さらに「この要求だけは燃料4以下」という**要求別上限**も設定できます。全員の合計にかかる容量と、1件だけにかかる上限は別の制約です。

### 1.3 必須、任意、無効、固定

**必須要求**は接続できなければ制約違反です。費用を払って見送ることはできません。**任意要求**は接続を見送れますが、見送ると指定した未接続損失がかかります。

要求を**無効**にすると、その要求は接続せず、費用・消費・必須条件の対象から外れます。また、特定の経路を**固定**して、残りの要求だけを調整できます。固定は探索の希望ではなく、守る必要がある条件です。

### 1.4 目的関数を読む準備

目的関数は「どの答えを良いとするか」を数式で書いたものです。ここでは、すべての制約を満たす答えの中で、費用を小さくします。

まず、経路を選ぶための記号を定めます。

| 記号 | 意味 | 入力・出力との対応 |
|---|---|---|
| $K$ | 有効な要求の集合 | `active=true` の要求 |
| $k$ | 要求の番号 | `requests[k]` |
| $\mathcal R$、$r$ | 資源全体の集合と、その番号 | `resources[r]` |
| $z_ {k}$ | 接続するなら1、見送るなら0 | `routes[k].connected` |
| $P_ {k}$ | 接続する要求に選んだ単純路 | `routes[k].arcs` とその通過頂点 |
| $A(P_ {k})$ | 経路に含まれる有向辺の集合 | 経路の辺ID列 |
| $V(P_ {k})$ | 経路に含まれる頂点の集合 | 出発点と目的地も含む |
| $a$、$v$ | 辺番号、頂点番号 | `arcs[a]`、`vertices[v]` |
| $d_ {k}$ | 要求の量 | `demand` |
| $s_ {k}$ | 経路費用に掛ける倍率 | `cost_scale` |
| $p_ {k}$ | 任意要求を見送ったときの損失 | `reject_cost` |
| $c_ {ka}$ | 要求 $k$ が辺 $a$ を使う費用 | 標準では `Arc::cost` |
| $t_ {kv}$ | 要求 $k$ が頂点 $v$ を通る費用 | 標準では `Vertex::cost` |

`Model` を差し替えると、辺・頂点の費用を要求ごとに変えられます。そのため、費用の記号には要求番号 $k$ も付けています。出発点と目的地が同じ経路では、その頂点の費用を1回だけ数えます。

### 1.5 資源消費と制約

資源消費には「荷物の量に比例する分」と「その指定を使うたびに必要な固定分」があります。

| 記号 | 意味 | APIとの対応 |
|---|---|---|
| $O_ {kr}(P_ {k})$ | 経路と要求自身に現れる、資源 $r$ の消費指定の集まり | 辺・頂点・要求の `uses`。同じ資源が複数箇所に現れれば別々に数える |
| $o$ | その消費指定1個 | `ResourceUse` 1個 |
| $\alpha_ {o}$ | 量1あたりの消費 | `per_unit` |
| $\beta_ {o}$ | その指定1回あたりの固定消費 | `per_use` |
| $u_ {kr}$ | 要求 $k$ が資源 $r$ を消費する総量。未接続なら0 | その要求についての消費指定の合計 |
| $B_ {r}$ | 背景使用量 | `background_load` |
| $L_ {r}$ | 背景と全要求を合わせた資源使用量 | `Evaluation::resource_load[r]` |
| $U_ {r}$ | 資源全体の容量 | `capacity`。`-1` ならこの上限制約なし |
| $H_ {kr}$ | 要求 $k$ だけに設定する資源 $r$ の上限 | `LocalLimit::upper`。設定した組だけ制約を課す |

接続した要求について、指定1個の消費は「量に比例する分＋固定分」です。

$$u_ {kr}=\sum_ {o\in O_ {kr}(P_ {k})}(\alpha_ {o}d_ {k}+\beta_ {o})$$

記号 $\sum$ は「該当するものを全部足す」という意味です。未接続なら上の経路を使わず、$u_ {kr}=0$ とします。`consume_endpoints=false` の場合は、出発点と目的地に付いた頂点の消費指定を集計から外します。頂点費用は外しません。

$$L_ {r}=B_ {r}+\sum_ {k\in K}u_ {kr},\qquad L_ {r}\le U_ {r},\qquad u_ {kr}\le H_ {kr}$$

中央の不等式は有限容量の資源だけに、右の不等式は要求別上限を指定した組だけに適用します。要求別上限には、他の要求の使用量と背景使用量は入りません。

たとえば量2の要求が `{per_unit=2, per_use=1}` という指定を1回使うと、消費は5です。同じ指定が経路の2本の辺に付いていれば、消費は10です。「同じ資源なので1回だけ」とは数えません。

### 1.6 標準の目的関数

費用を3つに分けて考えます。

| 記号 | 意味 | 出力との対応 |
|---|---|---|
| $C_ {\mathrm{reject}}$ | 任意要求を見送った損失の合計 | `Value::reject_cost` |
| $C_ {\mathrm{route}}$ | 接続した全要求の経路費用 | `Value::route_cost` |
| $f_ {r}(L_ {r})$ | 資源 $r$ の合計使用量に対してかかる費用 | `Model::resource_cost`。標準では常に0 |
| $C_ {\mathrm{resource}}$ | 全資源の費用の合計 | `Value::resource_cost` |
| $J$ | 3つを足した総費用 | `Value::total()` |

$$C_ {\mathrm{reject}}=\sum_ {k\in K,\ k\text{ is optional}}(1-z_ {k})p_ {k}$$

$$C_ {\mathrm{route}}=\sum_ {k\in K,\ z_ {k}=1}s_ {k}\left(\sum_ {a\in A(P_ {k})}c_ {ka}+\sum_ {v\in V(P_ {k})}t_ {kv}\right)$$

$$C_ {\mathrm{resource}}=\sum_ {r\in\mathcal R}f_ {r}(L_ {r}),\qquad \min J=\min\left(C_ {\mathrm{reject}}+C_ {\mathrm{route}}+C_ {\mathrm{resource}}\right)$$

1つ目の和は有効な任意要求だけが対象です。接続したときは $1-z_ {k}=0$ なので損失はありません。2つ目は辺費用と頂点費用を足し、要求の倍率を掛けます。量 $d_ {k}$ は、経路費用には自動では掛かりません。3つ目は、要求ごとではなく**資源の合計使用量に対して1回**費用を計算します。

たとえば未接続損失3、経路費用7、資源費用1なら総費用は11です。すべての費用は非負です。資源費用は使用量が増えても減らず、使用量0では0である必要があります。背景使用量も資源費用に含まれます。

この目的を `PenaltyPlusCost` と呼び、標準で使用します。

### 1.7 未接続損失を最優先する目的

もう1つの `PenaltyThenCost` では、次の組を小さくします。

$$\min_ {\mathrm{lex}}\left(C_ {\mathrm{reject}},\ C_ {\mathrm{route}}+C_ {\mathrm{resource}}\right)$$

`lex` は**辞書式の順番**という意味です。まず左側だけを比べ、小さい方を選びます。左側が等しいときだけ右側を比べます。

たとえば `(3, 0)` と `(0, 5)` なら、後者を選びます。総和は3から5へ増えますが、未接続損失が減るためです。巨大な重みを自分で作る必要はありません。必須要求の接続や容量は、この場合も必ず守る条件です。

### 1.8 使える範囲

本ライブラリは、限られた時間で良い実行可能解を探します。**最適解の保証はありません。また、実行可能解があるのに見つからない場合もあります。** 見つからなかったことと、不可能だと証明されたことは別です。

向いているのは、配線、通信、通路の割当てなど、「複数の始終点を、共有資源の制限の下でつなぐ」形に切り出せる部分問題です。AHCの公式スコアを直接受け取るAPIはありません。自分のスコアに対応する費用や制約へ変換して利用します。

1要求を複数の経路へ分ける流量、1本の木で多数の端点をつなぐ問題、負の費用、解全体を自由に見て判定する任意の制約は直接の対象ではありません。「設備を使う要求が何件あっても固定料金1回」は資源費用で表せますが、「1要求が同じ資源を何回通っても消費1」は通常の `uses` では表せません。

## 2. 類似ソルバーと使い分け

制約が単純なら、厳密解を求める方法を先に検討します。厳密解とは、その条件の下で最も良いことまで保証された答えです。下表の変換条件は、本ライブラリの問題定義に基づく判断です。

| 問題をこの形に限定できる場合 | 候補 | 本ライブラリとの使い分け |
|---|---|---|
| 要求間の容量競合・共有費用がなく、要求別上限もなく、各経路の費用が非負の辺・頂点費用の和 | Dijkstraによる最短路。例：Boost Graph Library [1] | 各要求を独立に最適化できる。頂点費用を辺側へ足すなどの変換が可能。任意要求は最短路費用と未接続損失を比較できる |
| 全要求が同じ始終点、量1、必須。費用は要求に依存しない線形の辺費用。制約は有向辺ごとの整数容量だけ | 最小費用流。例：AC Library `mcf_graph` [2] | 要求数だけ流せれば、その整数流を1単位ずつの経路へ分けられる。無関係な辺を束ねた共有資源や要求別上限は、この単純な変換の対象外 |
| 同じ始終点で「何単位流せるか」だけを最大化し、費用を考えない。制約は辺ごとの容量 | 最大流。例：AC Library `mf_graph` [3] | 線形な流量問題なら専用ソルバーが直接的。元の要求が量2などで分割禁止なら、通常の最大流とは条件が違う |
| 無向の木上で、すべての要求が必須 | 一意な経路の列挙と集計 | 始終点間の単純路は1本なので経路選択がない。容量・上限・禁止・固定条件を満たすか調べればよい。任意要求が混ざると、どれを採用するかは別の最適化になる |
| 頂点数や要求数が十分小さい、または最適性を証明したい | 全単純路の列挙、整数計画、OR-Tools CP-SAT [4] | 小規模な正解値作成にも有用。一般の制約を正しく定式化する作業と外部依存が必要。制限時間内に最適性まで証明できるとは限らない |

最小費用流は「要求ごとに決まった始終点の組」を自動で保持するものではありません。異なる出発点と目的地をまとめると、別の要求の目的地へ流れてしまうことがあります。また、流量が整数でも「1要求の量2を1本の道で通す」という条件は自動では守られません。この違いが、本ライブラリを使う理由の1つです。

CP-SATは整数で問題を表します。一般の非線形資源費用などは、有限の範囲を表にする等の追加の定式化が必要です。`OPTIMAL` と、最適性未証明の `FEASIBLE` は区別します [4]。AHCの提出環境で外部ソルバーを利用できるかは、個々の環境で確認してください。

「複数の分割できない要求」「辺をまたいだ共有容量」「要求ごとの制限」「短い時間で繰り返し解く」が組み合わさると、本ライブラリの用途に近づきます。

参照した一次資料（2026-10-07確認）：

1. [Boost Graph Library: Dijkstra's Shortest Paths](https://www.boost.org/doc/libs/1_86_0/libs/graph/doc/dijkstra_shortest_paths.html)
2. [AC Library: MinCostFlow](https://atcoder.github.io/ac-library/master/document_ja/mincostflow.html)
3. [AC Library: MaxFlow](https://atcoder.github.io/ac-library/master/document_ja/maxflow.html)
4. [Google OR-Tools: CP-SAT Solver](https://developers.google.com/optimization/cp/cp_solver)

## 3. 実行の準備

C++20を使えるGCCと `capacitated_router_v05.hpp` を用意します。ヘッダはGCCの `<bits/stdc++.h>` を使用します。他のコンパイラでの動作はこのガイドでは確認していません。コード例の検証はGCC 13.3.0で行いました。

任意のC++例を `main.cpp` として保存し、同じディレクトリにヘッダを置きます。すべての例は入力を読み取らないので、起動するだけで結果が出ます。

```sh
g++ -std=c++20 -O2 main.cpp -o example
./example
```

例題ZIPを展開した場合は、そのルートディレクトリで次のように実行できます。`-I.` は、ヘッダを現在のディレクトリから探す指定です。

```sh
g++ -std=c++20 -O2 -I. examples/01_minimum.cpp -o example
./example
```

以降の `assert(条件)` は学習用の確認です。条件が偽なら実行を停止します。通常の失敗を扱うプログラムでは、例01のように `if (!result.feasible())` で分岐してください。例の検証では `-DNDEBUG` を付けず、確認を有効にします。

`using R = CapacitatedRouter<>;` は長い型名に `R` という短い名前を付けます。`R::Problem` の `::` は、「`R` の中で定義された `Problem`」という意味です。公開型はすべてこの中にまとまっています。

## 4. 最小コード：1件の要求をつなぐ

**例01 — `examples/01_minimum.cpp`**

まずは1.1節のグラフをそのまま作ります。標準設定を使い、費用2の経路を探します。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(3);
    p.add_arc(0, 1, 1, 1);
    p.add_arc(1, 2, 1, 1);
    p.add_arc(0, 2, 5, 1);
    p.add_request(0, 2);

    auto solver = R::make_solver(p);
    const auto& result = solver.solve({});
    if (!result.feasible()) {
        std::cout << "no feasible solution found\n";
        return 1;
    }
    std::cout << "total=" << result.summary.value->total() << '\n';
}
```

出力：

```text
total=2
```

1. `R::Problem p(3)` で頂点を3個作ります。頂点番号は0、1、2です。まだ辺も要求もありません。
2. `add_arc(0, 1, 1, 1)` の引数は順に、出発頂点0、到着頂点1、辺費用1、辺容量1です。逆向きの辺は作りません。2本目も同じ読み方です。3本目は0から2への費用5、容量1の辺です。
3. `add_request(0, 2)` は出発点0、目的地2の要求を追加します。省略した量は1です。標準では有効な必須要求で、費用倍率は1です。
4. `make_solver(p)` は、この入力を解く `Solver` を作ります。`p` はSolverより長く生存させます。Solverが入力全体のコピーを持つわけではありません。
5. `solve({})` の `{}` は標準の `Limits` です。相対時間100,000マイクロ秒、すなわち100ミリ秒を使います。反復回数の上限と絶対締切は標準では無効です。
6. `result.feasible()` は、必要な制約をすべて満たしたかを返します。偽なら、今回の計算では実行可能解を返せなかったという意味です。
7. 成功時は `summary.value` に費用が入っています。これは「値がない」状態も持てる `std::optional` です。`->total()` で中の `Value` の3費用を足します。

`const auto&` は返された結果をコピーせず参照します。その結果はSolverの持ち物です。Solverの次の操作をまたいで残したいときは、例14のようにコピーします。

頂点費用と資源費用は標準で0です。必須要求を接続できているため、この例の総費用は辺費用の1＋1＝2です。

## 5. 発展コード01：2件の要求と経路の読み取り

**例02 — `examples/02_routes.cpp`**

例01に同じ要求を1件追加します。容量1の安い道を2件で同時に使えないことと、戻り値から経路を読む方法を確認します。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(3);
    p.add_arc(0, 1, 1, 1); // arc 0
    p.add_arc(1, 2, 1, 1); // arc 1
    p.add_arc(0, 2, 5, 1); // arc 2
    p.add_request(0, 2);   // request 0
    p.add_request(0, 2);   // request 1
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    const auto& result = solver.solve(limits);
    assert(result.feasible());
    assert(result.summary.value->total() == 7);
    for (int k = 0; k < 2; ++k) {
        const auto& route = result.solution.routes[k];
        std::cout << "request " << k << ": " << p.requests[k].source;
        for (int a : route.arcs) std::cout << " -> " << p.arcs[a].to;
        std::cout << '\n';
    }
    std::cout << "total=" << result.summary.value->total() << '\n';
}
```

出力例：

```text
request 0: 0 -> 2
request 1: 0 -> 1 -> 2
total=7
```

どちらの要求が安い道を使うかは本質ではありません。費用2と費用5の経路を1件ずつ使うので合計7です。

`add_arc` は追加された有向辺のIDを返します。IDは0から順に付くため、辺0は `0 → 1`、辺1は `1 → 2`、辺2は `0 → 2` です。`add_request` も同様に要求IDを返します。

`result.solution.routes[k]` が要求 `k` の答えです。`connected` が接続の有無、`arcs` が通る**辺IDの列**です。頂点の列ではありません。出発点を表示し、各辺の `to` を順に表示すると頂点列に戻せます。同じ両端を持つ平行な辺があっても、辺IDなら区別できます。

この例からは学習用に反復回数を固定します。`time_limit_us=-1` は相対時間制限を無効にし、`max_iterations=100` はこの呼出しで最大100反復を行う指定です。「100回で最適解を保証する」という意味ではありません。1反復が何をするかは最後の実装付録で説明します。第2引数の `Options` を省略したので乱数の `seed` は0です。

`std::vector<int>{...}` は整数の列です。以降では `assert` で辺ID列や手計算した費用を照合します。

## 6. 発展コード02：要求の量と費用倍率

**例03 — `examples/03_demand.cpp`**

要求を1件に戻し、運ぶ量を2にします。安い経路の容量は1のままなので通れません。直通辺の容量を2へ増やし、費用倍率も設定します。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(3);
    p.add_arc(0, 1, 1, 1);
    p.add_arc(1, 2, 1, 1);
    int direct = p.add_arc(0, 2, 5, 2);
    int k = p.add_request(0, 2, 2);
    p.requests[k].cost_scale = 3;
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    const auto& result = solver.solve(limits);
    assert(result.feasible());
    assert(result.solution.routes[k].arcs == std::vector<int>{direct});
    assert(result.summary.value->total() == 15);
    std::cout << "total=" << result.summary.value->total() << '\n';
}
```

出力：

```text
total=15
```

`add_arc(0, 2, 5, 2)` の最後の2が直通辺の容量です。`add_request(0, 2, 2)` の最後の2は要求の量です。この2は、経路の各辺で消費する量になります。

`k` は追加した要求のIDです。`p.requests[k].cost_scale=3` により、この要求の辺・頂点費用の合計を3倍にします。選ばれた直通経路の費用は5×3＝15です。

`demand=2` だけなら経路費用は5です。量に比例する運賃を表したい場合は、例えば `cost_scale=demand` と利用側で指定します。資源費用と未接続損失には、この倍率は掛かりません。

## 7. 発展コード03：無向辺の往復で容量を共有する

**例04 — `examples/04_undirected.cpp`**

ここではグラフを頂点2個に縮め、往復する2件を考えます。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(2);
    auto [forward, backward] = p.add_undirected_edge(0, 1, 1, 2);
    p.add_request(0, 1);
    p.add_request(1, 0);
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    const auto& result = solver.solve(limits);
    assert(result.feasible());
    assert(result.solution.routes[0].arcs == std::vector<int>{forward});
    assert(result.solution.routes[1].arcs == std::vector<int>{backward});
    assert(result.summary.value->total() == 2);
    std::cout << "forward_arc=" << forward << " backward_arc=" << backward
              << " total=" << result.summary.value->total() << '\n';
}
```

出力：

```text
forward_arc=0 backward_arc=1 total=2
```

`add_undirected_edge(0, 1, 1, 2)` は、頂点0と1の間に費用1の有向辺を2本作り、**往復合計で容量2**とします。`auto [forward, backward]` は戻り値の2つの辺IDをそれぞれ受け取るC++の書き方です。最初が `0 → 1`、次が `1 → 0` です。

量1の要求が片方向に1件ずつあるので、合計消費2となります。容量を1にすると、両方を接続することはできません。

方向ごとに独立した容量を持たせたい場合は、`add_arc(0, 1, 1, 1)` と `add_arc(1, 0, 1, 1)` を別々に呼びます。こちらは各方向で1まで使えます。

## 8. 発展コード04：頂点の費用と容量

**例05 — `examples/05_vertices.cpp`**

今度は4頂点のグラフで、中継点1の処理能力を1に制限します。辺自身には容量を設定しません。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(4);
    p.add_arc(0, 1, 1);
    p.add_arc(1, 3, 1);
    p.add_arc(0, 2, 4);
    p.add_arc(2, 3, 4);
    p.vertices[1].cost = 3;
    p.add_vertex_capacity(1, 1);
    p.add_request(0, 3);
    p.add_request(0, 3);
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    const auto& result = solver.solve(limits);
    assert(result.feasible());
    assert(result.summary.value->total() == 13);
    std::cout << "total=13\n";
}
```

出力：

```text
total=13
```

`add_arc(0, 1, 1)` は容量を省略しています。標準の容量 `-1` は上限なしです。`p.vertices[1].cost=3` は、頂点1を通る要求1件につき費用3を追加します。

`add_vertex_capacity(1, 1)` の最初の1が頂点番号、次の1が容量です。量1の2要求のうち、頂点1を通れるのは1件です。

頂点1経由は辺費用1＋1と頂点費用3で5、頂点2経由は4＋4で8、合計13です。頂点容量も量 `demand` を消費します。費用は量に自動比例しません。

出発点と目的地も、標準では頂点費用・頂点資源の対象です。`add_vertex_capacity` の戻り値は、内部で作った資源のIDです。任意の共有資源と同じ仕組みになっており、例07から直接扱います。

## 9. 発展コード05：端点資源の免除と長さ0の経路

**例06 — `examples/06_endpoints.cpp`**

出発点や目的地の容量を数えたくない場面を扱います。違いが見えやすいよう、出発点と目的地を同じ頂点にします。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(1);
    p.vertices[0].cost = 7;
    p.add_vertex_capacity(0, 0);
    int k = p.add_request(0, 0);
    p.requests[k].consume_endpoints = false;
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 10;
    const auto& result = solver.solve(limits);
    assert(result.feasible());
    const auto& route = result.solution.routes[k];
    assert(route.connected && route.arcs.empty());
    assert(result.summary.value->total() == 7);
    std::cout << "connected=" << route.connected
              << " arcs=" << route.arcs.size() << " total=7\n";
}
```

出力：

```text
connected=1 arcs=0 total=7
```

`Problem(1)` は頂点0だけを作ります。`add_request(0, 0)` は移動せずに接続が完了する要求です。`consume_endpoints=false` によって、端点に付いた頂点資源を消費しなくなるため、容量0でも接続できます。

これは頂点費用の免除ではありません。頂点0の費用7は1回掛かります。辺・要求自身に付いた消費、端点の `enabled`、Modelによる通行禁止も免除しません。

辺列が空でも、`connected=true` なら接続済みの長さ0経路です。`connected=false` かつ空の辺列は未接続です。出発点と目的地が違うのに空の辺列で接続済みにすることはできません。

## 10. 発展コード06：複数の辺が同じ資源を使う

**例07 — `examples/07_shared_resource.cpp`**

例02のグラフに戻ります。各辺の専用容量を外し、安い2本の辺が同じ設備を使うようにします。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(3);
    int channel = p.add_resource(3);
    int a = p.add_arc(0, 1, 1);
    int b = p.add_arc(1, 2, 1);
    p.add_arc(0, 2, 5);
    p.arcs[a].uses.push_back({channel, 1, 0});
    p.arcs[b].uses.push_back({channel, 1, 0});
    p.add_request(0, 2);
    p.add_request(0, 2);
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    const auto& result = solver.solve(limits);
    assert(result.feasible());
    auto evaluation = solver.evaluate(result.solution);
    assert(evaluation.resource_load[channel] == 2);
    assert(result.summary.value->total() == 7);
    std::cout << "channel_load=2 total=7\n";
}
```

出力：

```text
channel_load=2 total=7
```

`add_resource(3)` は容量3、背景使用量0の資源を作り、そのIDを返します。変数 `channel` がそのIDです。

`uses.push_back({channel, 1, 0})` は、消費指定 `ResourceUse` を1個追加します。3つの値は順に、資源ID、`per_unit`、`per_use` です。ここでは要求の量×1＋0を消費します。辺 `a` と辺 `b` に同じ資源を付けたので、量1の要求が安い経路を通ると2消費します。

2件とも安い道を選ぶと合計4となり容量3を超えます。1件を直通辺へ回し、合計消費2、費用7になります。同じ資源が2回現れたら2回数える点を確認してください。

`solver.evaluate(result.solution)` は、経路を探さず、与えた解を現在の入力で集計するAPIです。`resource_load[channel]` から背景を含む実際の使用量を読めます。`evaluate` は自身の集計用領域で全評価するため、詳細確認に向いています。毎反復の軽い操作とは考えないでください。

辺・頂点の追加関数による容量も、この資源と `uses` の組合せで実現されています。容量を指定して追加した辺に、さらに `uses` を付けると、元の専用容量と新しい資源の**両方**を消費します。

## 11. 発展コード07：固定消費と接続時だけの消費

**例08 — `examples/08_consumption.cpp`**

設備を使うたびに一定量の電力が必要な状況を追加します。1本の辺に縮めて計算を追います。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(2);
    int energy = p.add_resource(7);
    int a = p.add_arc(0, 1, 1);
    p.arcs[a].uses.push_back({energy, 2, 1});
    int k = p.add_request(0, 1, 2);
    p.requests[k].uses.push_back({energy, 0, 2});
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    const auto& result = solver.solve(limits);
    assert(result.feasible());
    auto evaluation = solver.evaluate(result.solution);
    assert(evaluation.resource_load[energy] == 7); // 2*2+1 + 0*2+2
    assert(result.summary.value->total() == 1);
    std::cout << "energy_load=7 total=1\n";
}
```

出力：

```text
energy_load=7 total=1
```

資源 `energy` の容量は7です。辺の指定 `{energy, 2, 1}` は、量2に対して2×2＋1＝5消費します。`per_use=1` は「資源全体で初回だけ」ではなく、**この指定が経路に現れるたび**の消費です。

要求自身の `{energy, 0, 2}` は、接続したときに1回だけ0×2＋2＝2消費します。経路の長さには関係しません。合計5＋2＝7です。要求が未接続なら、要求自身の消費も発生しません。

頂点の `uses` に同じ形式を追加することもできます。各数値は非負である必要があります。`demand=0` でも `per_use` が正なら固定分は残ります。消費量と目的関数の費用は別の量なので、この例の総費用は辺費用1のままです。

## 12. 発展コード08：1要求の燃料と通過辺数を制限する

**例09 — `examples/09_local_limits.cpp`**

共有設備の容量ではなく、各要求の燃料や移動回数を制限します。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(3);
    int fuel = p.add_resource();
    int hops = p.add_resource();
    int a = p.add_arc(0, 1, 1);
    int b = p.add_arc(1, 2, 1);
    int direct = p.add_arc(0, 2, 5);
    p.arcs[a].uses.push_back({fuel, 0, 3});
    p.arcs[b].uses.push_back({fuel, 0, 3});
    p.arcs[direct].uses.push_back({fuel, 0, 4});
    for (auto& arc : p.arcs) arc.uses.push_back({hops, 0, 1});
    int k = p.add_request(0, 2);
    p.requests[k].limits.push_back({fuel, 4});
    p.requests[k].limits.push_back({hops, 1});
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    const auto& result = solver.solve(limits);
    assert(result.feasible());
    assert(result.solution.routes[k].arcs == std::vector<int>{direct});
    auto evaluation = solver.evaluate(result.solution);
    assert(evaluation.resource_load[fuel] == 4);
    assert(evaluation.resource_load[hops] == 1);
    std::cout << "fuel=4 hops=1 total=5\n";
}
```

出力：

```text
fuel=4 hops=1 total=5
```

引数を省略した `add_resource()` は容量 `-1`、背景使用量0です。ここでは全要求の合計に上限を付けず、使用量を測る目盛りとして資源を使います。

`fuel` は燃料です。安い2本の辺はそれぞれ `{fuel, 0, 3}`、直通辺は `{fuel, 0, 4}` を消費します。量に比例させないため `per_unit=0` としています。

`hops` は通過辺数です。すべての辺に `{hops, 0, 1}` を追加し、1本通るたびに1増えるようにします。

`requests[k].limits` に入れる `LocalLimit` の2項目は、資源IDと、その要求だけの上限です。`{fuel, 4}` は燃料4以下、`{hops, 1}` は辺数1以下です。2つの条件を同時に守る必要があります。同じ要求の `limits` に同じ資源IDを重ねて指定してはいけません。

安い経路は燃料6、辺数2なので使えません。直通経路は燃料4、辺数1で使えます。要求を増やした場合でも、`LocalLimit` は要求ごとの合計を調べます。全体の燃料4という意味ではありません。必要なら有限の共有容量も同時に設定できます。

## 13. 発展コード09：接続を見送れる要求

**例10 — `examples/10_optional.cpp`**

費用5を掛けて接続するか、損失3で見送るかを選びます。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(2);
    p.add_arc(0, 1, 5, 1);
    int k = p.add_request(0, 1);
    p.requests[k].required = false;
    p.requests[k].reject_cost = 3;
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    const auto& result = solver.solve(limits);
    assert(result.feasible());
    assert(!result.solution.routes[k].connected);
    const auto& value = *result.summary.value;
    assert(value.reject_cost == 3 && value.route_cost == 0);
    std::cout << "reject=" << value.reject_cost
              << " route=" << value.route_cost
              << " resource=" << value.resource_cost
              << " total=" << value.total() << '\n';
}
```

出力：

```text
reject=3 route=0 resource=0 total=3
```

`required=false` は任意要求にする指定です。`reject_cost=3` は、未接続を選んだときに目的関数へ足す損失です。標準の `PenaltyPlusCost` では、接続の総費用5より未接続の総費用3が小さいので、見送ります。

この未接続は制約違反ではありません。したがって `feasible()` は真です。`*result.summary.value` は `std::optional` の中の値を取り出します。コードでは成功確認後に取り出しています。

`reject_cost`、`route_cost`、`resource_cost` は費用の内訳です。任意要求の `reject_cost` を標準の0のままにすると、費用のかかる接続を見送る方が有利になります。「つないでほしい程度」を入力として決めてください。

## 14. 発展コード10：接続の損失を最優先する

**例11 — `examples/11_lexicographic.cpp`**

例10から目的を1行だけ変更します。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(2);
    p.add_arc(0, 1, 5, 1);
    int k = p.add_request(0, 1);
    p.requests[k].required = false;
    p.requests[k].reject_cost = 3;
    p.objective = R::Objective::PenaltyThenCost;
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    const auto& result = solver.solve(limits);
    assert(result.feasible() && result.solution.routes[k].connected);
    const auto& value = *result.summary.value;
    assert(value.reject_cost == 0 && value.route_cost == 5);
    std::cout << "reject=" << value.reject_cost
              << " route=" << value.route_cost
              << " total=" << value.total() << '\n';
}
```

出力：

```text
reject=0 route=5 total=5
```

`p.objective=R::Objective::PenaltyThenCost` が新しい指定です。未接続の評価は `(損失3, その他0)`、接続の評価は `(損失0, その他5)` なので、接続を選びます。

この目的でも `Value::total()` は単なる3費用の和です。辞書式の良し悪しを `total()` だけで比較してはいけません。例27で両目的に対応した比較を書きます。

「未接続件数を最小化したい」なら、各任意要求の損失をすべて1にすると件数と一致します。損失が異なる場合は件数ではなく損失の合計が最優先です。損失0の要求は、接続を優先する効果を持ちません。

## 15. 発展コード11：解を作って診断する

**例12 — `examples/12_evaluation.cpp`**

自分で作った解や、更新前の解の問題点を調べます。ここでは探索を呼びません。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(3);
    int fuel = p.add_resource(5);
    int a = p.add_arc(0, 1, 1);
    int b = p.add_arc(1, 2, 1);
    p.arcs[a].uses.push_back({fuel, 0, 3});
    p.arcs[b].uses.push_back({fuel, 0, 3});
    int k = p.add_request(0, 2);
    p.requests[k].limits.push_back({fuel, 4});
    auto solver = R::make_solver(p);

    R::Solution proposed;
    proposed.routes.resize(p.requests.size());
    auto missing = solver.evaluate(proposed);
    assert(!missing.feasible() && missing.summary.value.has_value());
    std::cout << "missing=" << missing.missing_required.size()
              << " has_value=" << missing.summary.value.has_value() << '\n';

    proposed.routes[k] = R::Route{true, {a, b}};
    auto invalid = solver.evaluate(proposed);
    assert(!invalid.feasible() && !invalid.summary.value.has_value());
    std::cout << "overloaded=" << invalid.overloaded_resources.size()
              << " locally_invalid=" << invalid.summary.locally_invalid_requests
              << " has_value=" << invalid.summary.value.has_value() << '\n';
    for (const auto& v : invalid.local_violations) {
        std::cout << "request=" << v.request << " resource=" << v.resource
                  << " used=" << v.used << " upper=" << v.upper << '\n';
    }
}
```

出力：

```text
missing=1 has_value=1
overloaded=1 locally_invalid=1 has_value=0
request=0 resource=0 used=6 upper=4
```

`Solution` は要求順の `Route` 配列を持ちます。`resize(p.requests.size())` で要求数に合わせると、各要素は標準で未接続になります。`evaluate` に渡す配列長は、要求数と完全に一致させます。

最初の評価では、必須要求1件が未接続です。費用は計算できるので `value` はありますが、`feasible()` は偽です。

次に `Route{true, {a, b}}` として接続します。最初の `true` は接続済み、次の配列は通る辺IDです。燃料を3＋3＝6使うので、共有容量5と要求別上限4の両方に違反します。共有容量違反があるため、この評価の `value` はありません。

| 診断 | `Summary` の件数 | `Evaluation` の詳細 |
|---|---|---|
| 必須の未接続 | `missing_required` | `missing_required` に要求ID |
| 共有容量超過 | `overloaded_resources` | `overloaded_resources` に資源ID |
| 要求別上限違反 | `locally_invalid_requests` | `local_violations` に要求ID、資源ID、使用量、上限 |
| 無効・禁止された経路 | `forbidden_requests` | `forbidden_requests` に要求ID |
| 固定経路との不一致 | `fixed_mismatches` | `fixed_mismatches` に要求ID |

`local_violations` の `used` はその要求の使用量、`upper` はその上限です。1要求が2つの上限に違反すれば、詳細は2件、`locally_invalid_requests` は1件になります。

`Summary::feasible()`、`Result::feasible()`、`Evaluation::feasible()` は同じ制約充足の意味です。共有容量超過または禁止経路があれば `value` は空になります。必須未接続、要求別上限、固定不一致だけなら値がある場合もあるので、成功判定は必ず `feasible()` で行います。

`evaluate` は壊れた入力を何でも受け付ける検証器ではありません。辺IDが有効で、始終点がつながり、単純路であること等は呼出し側の前提です。構造不正は通常 `assert` の対象です。未接続経路の辺列も空にしてください。

## 16. 発展コード12：自作の初期解から改善する

**例13 — `examples/13_warm_start.cpp`**

直通辺を使う費用5の解を用意し、そこから改善します。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(3);
    p.add_arc(0, 1, 1, 1);
    p.add_arc(1, 2, 1, 1);
    int direct = p.add_arc(0, 2, 5, 1);
    p.add_request(0, 2);
    R::Solution initial;
    initial.routes.push_back(R::Route{true, {direct}});
    auto solver = R::make_solver(p);
    auto before = solver.evaluate(initial);
    assert(before.feasible() && before.summary.value->total() == 5);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    const auto& result = solver.improve(initial, limits);
    assert(result.feasible() && result.summary.value->total() == 2);
    std::cout << "before=5 after=" << result.summary.value->total() << '\n';
}
```

出力：

```text
before=5 after=2
```

`improve(initial, limits)` の第1引数は出発点となる解、第2引数は今回の計算予算です。第3引数には `solve` と同じ `Options` を指定でき、省略時の `seed` は0です。

`solve` は固定条件から新しく始めます。`improve` は利用側の構築法や前のSolverが得た解を受け取れます。固定指定がある要求は、初期解より固定指定が優先されます。現在は禁止された可動経路などは初期化時に取り除かれます。

`improve` だけは、要求数より短い `routes` を受け付け、不足分を未接続で補います。要求数より長い配列は渡せません。`evaluate` は短い配列を補わない点に注意してください。

初期解が現在の入力に対して実行可能なら、保存される最良解の候補になります。追加探索で必ず改善するとは限りませんが、入力が同じなら、見つけた最良実行可能解を保持します。受け取った解は内部にコピーするので、Solver自身の結果の `solution` を `improve` に渡すこともできます。

## 17. 発展コード13：計算を分けて続ける

**例14 — `examples/14_resume.cpp`**

20反復を実行した後、内部の探索状態を保って80反復を追加します。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(3);
    p.add_arc(0, 1, 1, 1);
    p.add_arc(1, 2, 1, 1);
    p.add_arc(0, 2, 5, 1);
    p.add_request(0, 2);
    p.add_request(0, 2);
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 20;
    R::Options options;
    options.seed = 42;
    auto first = solver.solve(limits, options); // Copy, not a reference.
    limits.max_iterations = 80;
    const auto& second = solver.resume(limits);
    assert(first.feasible() && second.feasible());
    assert(second.summary.value->total() <= first.summary.value->total());
    assert(first.statistics.iterations == 20 && second.statistics.iterations == 80);
    std::cout << "first_iterations=" << first.statistics.iterations
              << " second_iterations=" << second.statistics.iterations
              << " total=" << second.summary.value->total() << '\n';
}
```

出力：

```text
first_iterations=20 second_iterations=80 total=7
```

`Options::seed=42` は乱数列の出発点です。42に特別な意味はなく、比較実験で条件を揃えるための任意の番号です。固定反復・同じ入力・同じ実行環境なら、同じseedで挙動を再現しやすくなります。時間制限では実行できる反復数が変わるため、seedだけでは同一出力を保証しません。

`resume(limits)` は以前の `solve` または `improve` に続ける操作です。未初期化のSolverには呼べません。乱数列、作業中の経路、最良解、前処理と作業領域を保ちます。seedの引数はありません。

`max_iterations=80` は前回を含めた累計80ではなく、今回追加する上限です。`Statistics` も各呼出し単位なので、最初は20、次は80になります。初期構築の反復もこの件数に入ります。

`auto first=...` は `Result` のコピーを作ります。`const auto& first=...` にすると、次の `resume` で中身が書き換わるため、この比較には使えません。経路だけ残せばよいなら `Solution` だけをコピーすると、意図が明確になります。

## 18. 発展コード14：マイクロ秒と絶対締切

**例15 — `examples/15_time_budget.cpp`**

時間制限の例です。短い予算でも既知の経路を返せるよう、最初から実行可能な経路を `improve` に渡します。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    auto deadline = R::Clock::now() + std::chrono::microseconds(20'000);
    R::Problem p(2);
    int a = p.add_arc(0, 1, 1, 1);
    p.add_request(0, 1);
    R::Solution initial;
    initial.routes.push_back(R::Route{true, {a}});
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = 2'000;
    limits.deadline = deadline;
    const auto& result = solver.improve(initial, limits);
    assert(result.feasible());
    assert(result.stop_reason == R::StopReason::TimeLimit);
    const auto& s = result.statistics;
    std::cout << "time_limit total=" << result.summary.value->total() << '\n';
    std::cout << "iterations=" << s.iterations
              << " paths=" << s.path_searches
              << " labels=" << s.label_searches
              << " accepted=" << s.accepted
              << " elapsed_us=" << s.elapsed_us << '\n';
}
```

出力例（2行目の数値は実行ごとに変わります）：

```text
time_limit total=1
iterations=4014 paths=4013 labels=0 accepted=4013 elapsed_us=2000
```

`Clock` は `std::chrono::steady_clock` です。時刻合わせで時計が飛ばない、経過時間の測定用時計を使います。20,000マイクロ秒は20ミリ秒、2,000マイクロ秒は2ミリ秒です。桁区切りの `'` は値を変えません。

`deadline` は「いつまでに終えたいか」の絶対時刻です。この例では入力構築前から20ミリ秒後としました。一方、`time_limit_us=2'000` は `improve` が始まってからの相対予算です。**両方あれば、先に来る方**を使います。絶対締切だけ使いたいときは、`time_limit_us=-1` にします。

`max_iterations` は省略したので標準の `-1`、すなわち回数制限なしです。時間・絶対締切・回数の3つをすべて無効にすることはできません。

| 統計 | 内容 |
|---|---|
| `iterations` | この呼出しの外側の探索反復数 |
| `path_searches` | 通常の経路探索を開始した回数。1反復で複数回あり得る |
| `label_searches` | 上限を追跡する追加の経路探索を開始した回数 |
| `accepted` | 採用した経路再構築の回数。目的値の改善回数とは限らない |
| `elapsed_us` | `solve` / `improve` / `resume` 内で測った経過マイクロ秒 |

`elapsed_us` は `solve` / `improve` の初期化を含みますが、呼出し前のコンストラクタや `refresh`、呼出し後の結果コピーは含みません。全体の時間を管理したいなら、それらより前に外側の締切を作ります。

| `StopReason` | 意味 |
|---|---|
| `TimeLimit` | 相対時間または絶対締切で終了 |
| `IterationLimit` | 今回の反復上限で終了 |
| `NoMovableRequests` | 有効かつ固定されていない要求がない |
| `FixedConflict` | 背景使用量や固定指定が両立しないと初期化で判定 |

停止理由と成功判定は別です。時間終了でも良い実行可能解が返る場合があり、回数終了でも見つかっていない場合があります。

時間制限は途中で時計を確認する方式です。前処理、経路集計、取消し、結果保存などに追加の時間が必要で、厳密な実時間上限ではありません。`0` マイクロ秒も処理全体が即座に終わる指定ではありません。AHCでは、入出力や最後の処理の余裕を外側に確保して使います。

## 19. 発展コード15：一部の経路を固定する

**例16 — `examples/16_fixed_routes.cpp`**

変更できない配線を固定し、他の要求を配置します。その後、固定条件の矛盾と解除も確認します。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(3);
    p.add_arc(0, 1, 1, 1);
    p.add_arc(1, 2, 1, 1);
    int direct = p.add_arc(0, 2, 5, 1);
    int fixed = p.add_request(0, 2);
    p.add_request(0, 2);
    p.requests[fixed].fixed_route = R::Route{true, {direct}};
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    auto first = solver.solve(limits);
    assert(first.feasible());
    assert(first.solution.routes[fixed] == *p.requests[fixed].fixed_route);
    std::cout << "fixed_total=" << first.summary.value->total() << '\n';

    p.requests[fixed].fixed_route = R::Route{};
    solver.refresh();
    const auto& conflict = solver.resume(limits);
    assert(!conflict.feasible());
    assert(conflict.stop_reason == R::StopReason::FixedConflict);
    std::cout << "fixed_conflict\n";

    p.requests[fixed].fixed_route.reset();
    solver.refresh();
    const auto& recovered = solver.resume(limits);
    assert(recovered.feasible() && recovered.summary.value->total() == 7);
    std::cout << "unlocked_total=7\n";
}
```

出力：

```text
fixed_total=7
fixed_conflict
unlocked_total=7
```

`fixed_route` は `std::optional<Route>` です。値なしが可動、値ありならその経路に固定します。最初の `Route{true, {direct}}` は直通辺を使った接続に固定するため、もう1件が安い経路を使い、合計7になります。

ここで更新用APIを導入します。**入力の属性を変更したら `refresh()`、その後に `resume(limits)`** を呼ぶのが基本です。`refresh` は現在の入力で旧解を再評価して探索状態を作り直します。計算を続けるのが `resume` です。

`fixed_route=Route{}` は「固定を外す」意味ではなく、**未接続に固定する**指定です。例では必須要求なので矛盾し、`FixedConflict` となります。任意要求なら未接続固定は可能で、未接続損失が掛かります。

固定を外すのは `fixed_route.reset()` です。解除後に `refresh` と `resume` を呼ぶと、再び経路を選べます。

固定した経路も容量、要求別上限、通行許可に従います。`evaluate` に固定経路と異なる解を渡すと `fixed_mismatches` に要求IDが入ります。全要求を有効な固定経路にした場合、再配置対象がないので `NoMovableRequests` で返ります。

## 20. 発展コード16：すでに使われている容量

**例17 — `examples/17_background.cpp`**

経路を詳しく持たない外部利用分を、資源の背景使用量として表します。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(2);
    int channel = p.add_resource(3, 2);
    int a = p.add_arc(0, 1, 1);
    p.arcs[a].uses.push_back({channel, 1, 0});
    int k = p.add_request(0, 1);
    p.requests[k].limits.push_back({channel, 1});
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    const auto& result = solver.solve(limits);
    assert(result.feasible());
    auto evaluation = solver.evaluate(result.solution);
    assert(evaluation.resource_load[channel] == 3);
    assert(evaluation.local_violations.empty());
    std::cout << "background=2 request_use=1 total_load=3\n";
}
```

出力：

```text
background=2 request_use=1 total_load=3
```

`add_resource(3, 2)` の引数は容量3、背景使用量2です。新しい要求が使える余裕は1です。量1の要求を通すと、`resource_load` は2＋1＝3になります。

要求別上限 `{channel, 1}` が見るのは、その要求が使った1だけです。背景の2は含めないため違反しません。

背景使用量は資源費用には含まれます。ただし外部経路の辺費用・頂点費用を自動で復元することはできません。必要な定数費用は利用側で別に管理します。非線形資源費用は合計負荷に掛かるので、外部利用分と今回の利用分を独立に計算して足すと意味が変わる場合があります。

容量より背景が大きいと、どの経路を選んでも解消できず `FixedConflict` の対象になります。背景は「Solverが移動させてよい仮の要求」ではありません。

## 21. 発展コード17：通行条件を更新する

**例18 — `examples/18_refresh.cpp`**

前のターンで使った辺や頂点が、次のターンで使えなくなる状況です。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(3);
    int a = p.add_arc(0, 1, 1, 1);
    p.add_arc(1, 2, 1, 1);
    p.add_arc(0, 2, 5, 1);
    p.add_request(0, 2);
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    auto old = solver.solve(limits);
    assert(old.feasible() && old.summary.value->total() == 2);

    p.arcs[a].enabled = false;
    solver.refresh();
    auto diagnostic = solver.evaluate(old.solution);
    assert(diagnostic.forbidden_requests == std::vector<int>{0});
    assert(!diagnostic.summary.value);
    auto next = solver.resume(limits);
    assert(next.feasible() && next.summary.value->total() == 5);

    p.arcs[a].enabled = true;
    p.vertices[1].enabled = false;
    solver.refresh();
    const auto& last = solver.resume(limits);
    assert(last.feasible() && last.summary.value->total() == 5);
    std::cout << "before=2 edge_disabled=5 vertex_disabled=5\n";
}
```

出力：

```text
before=2 edge_disabled=5 vertex_disabled=5
```

`Arc::enabled=false` は全要求についてその辺を禁止します。`Vertex::enabled=false` は全要求についてその頂点を禁止します。出発点・目的地も対象です。例では安い道が使えなくなるので、直通の費用5へ移ります。

更新前の結果は `auto old` でコピーしてあります。`refresh` 後に旧解を `evaluate` すると、その経路を使えなくなった要求0が `forbidden_requests` に入ります。これは構造が壊れた経路ではなく、現在の通行条件を満たさない経路です。

頂点数、辺数、各辺の両端が同じなら、費用、容量、消費指定、要求、目的、Modelの参照先の変更は `refresh` で取り込みます。途中で資源や要求を追加することもできます。既存IDの意味が変わらないように管理してください。

`refresh` は、以前の最良実行可能解があればそれを、なければ作業中の解を出発点に再評価します。以前は良い解でも、新しい入力では制約違反になり得ます。可動の禁止経路は取り除き、固定経路の矛盾は報告します。

再利用するのは入力の接続構造や作業領域です。`refresh` 自体は入力と旧経路を広く走査するので、「変更した1辺分だけの時間」で終わるAPIではありません。

## 22. 発展コード18：要求が入れ替わるターン制

**例19 — `examples/19_turns.cpp`**

前の要求が終了し、同じ設備を次の要求に使います。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(2);
    p.add_arc(0, 1, 1, 1);
    int previous = p.add_request(0, 1);
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    assert(solver.solve(limits).feasible());

    p.requests[previous].active = false;
    p.requests[previous].fixed_route.reset();
    int current = p.add_request(0, 1);
    solver.refresh();
    const auto& result = solver.resume(limits);
    assert(result.feasible());
    assert(!result.solution.routes[previous].connected);
    assert(result.solution.routes[current].connected);
    assert(result.summary.value->total() == 1);
    std::cout << "old_connected=0 new_connected=1 total=1\n";
}
```

出力：

```text
old_connected=0 new_connected=1 total=1
```

`active=false` にすると、旧要求は接続せず、容量も使わず、必須条件や未接続損失の対象にもなりません。`required=false` は有効な任意要求として残す指定なので、意味が違います。

要求を配列から消す代わりに無効化すると、他の要求IDがずれません。新しい要求は末尾へ追加し、`refresh` 後に `resume` します。返る `routes` の長さは新しい要求数です。

この例の `fixed_route.reset()` は、以前に固定していた場合にも安全に無効化できるよう明示しています。無効要求に接続済みの固定経路を残すと、無効化と固定が矛盾します。

大量のターンで無効要求が増え続けるなら、利用側で整理する判断が必要です。IDを詰め直す際は保存した `Solution` の要求順も対応させます。ライブラリは古いIDの意味を推測しません。

## 23. 発展コード19：辺を追加して前処理を作り直す

**例20 — `examples/20_reset.cpp`**

安い直通辺が新設された状況です。辺の本数が変わるため `refresh` ではなく `reset` を使います。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(3);
    p.add_arc(0, 1, 2, 1);
    p.add_arc(1, 2, 2, 1);
    p.add_request(0, 2);
    R::Solver<> solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    auto old = solver.solve(limits);
    assert(old.feasible() && old.summary.value->total() == 4);

    p.add_arc(0, 2, 1, 1);
    solver.reset(p);
    const auto& result = solver.improve(old.solution, limits);
    assert(result.feasible() && result.summary.value->total() == 1);
    std::cout << "before=4 after=1\n";
}
```

出力：

```text
before=4 after=1
```

`R::Solver<> solver(p)` は `make_solver(p)` と同じ目的の、型を明示する書き方です。標準Modelを使います。

`reset(p)` は参照するProblemを設定し直し、頂点・辺から前処理を再構築して探索状態を捨てます。直後に `resume` は呼べません。新規なら `solve`、保存した解があれば `improve` を呼びます。

ここでは辺を末尾へ追加しただけなので、保存した経路の辺IDはそのまま有効です。`improve(old.solution, limits)` に渡すと、旧解を出発点に直通辺を検討できます。

頂点数・辺数の変更だけでなく、既存辺の `from` または `to` を変えた場合も `reset` が必要です。辺や頂点のIDを詰め替えた場合、旧経路のID対応は利用側で直します。別の `Problem` に `reset` する場合も、そのProblemをSolverより長く生存させてください。

`reset` はModelを保持します。Modelが持つ問題別の配列や外部参照までは更新しないので、新しい入力と一致させる必要があります。

## 24. 発展コード20：要求別の費用と通行禁止

**例21 — `examples/21_custom_model.cpp`**

入力欄だけでは足りない規則を `Model` で表します。要求1だけ特定の辺を禁止し、要求0だけ中継点の追加費用を課します。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(3);
    int a = p.add_arc(0, 1, 1, 1);
    p.add_arc(1, 2, 1, 1);
    p.add_arc(0, 2, 5, 1);
    p.add_request(0, 2);
    p.add_request(0, 2);
    struct Model : R::DefaultModel {
        int closed_arc;
        int special_vertex;
        Model(int a, int v) : closed_arc(a), special_vertex(v) {}
        std::optional<int64_t> arc_cost(const R::Problem& p, int k, int a) const {
            if (k == 1 && a == closed_arc) return std::nullopt;
            return p.arcs[a].cost;
        }
        std::optional<int64_t> vertex_cost(const R::Problem& p, int k, int v) const {
            return p.vertices[v].cost + (k == 0 && v == special_vertex ? 2 : 0);
        }
    };
    auto solver = R::make_solver(p, Model(a, 1));
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    const auto& result = solver.solve(limits);
    assert(result.feasible() && result.summary.value->total() == 9);
    assert(result.solution.routes[0].arcs.size() == 2);
    assert(result.solution.routes[1].arcs.size() == 1);
    std::cout << "request0_cost=4 request1_cost=5 total=9\n";
}
```

出力：

```text
request0_cost=4 request1_cost=5 total=9
```

`struct Model : R::DefaultModel` は、標準Modelを土台に必要な関数だけを置き換える書き方です。例では `resource_cost` を書いていないため、標準の資源費用0を引き継ぎます。Modelの型はこのプログラムの `main` 内だけで使います。

`Model(a, 1)` の第1引数は閉鎖する辺ID、第2引数は追加費用を課す頂点IDです。コンストラクタがそれぞれ `closed_arc` と `special_vertex` に記録します。

| コールバックの引数・戻り値 | 意味 |
|---|---|
| `const Problem& p` | 現在の入力。参照なのでコピーしない |
| `int k` | 費用を知りたい要求のID |
| `int a` / `int v` | 問い合わせる辺ID / 頂点ID |
| `std::optional<int64_t>` | 非負の費用、または使用禁止を表す値なし |

`arc_cost` では要求1かつ辺 `a` だけ `std::nullopt` を返します。他は通常の辺費用です。`vertex_cost` の `条件 ? 2 : 0` は、要求0が指定頂点を通る場合だけ2を足します。

要求0の中継経路は1＋1＋2＝4、要求1の直通は5で、合計9です。`vertex_cost` でも `nullopt` を返せます。`enabled=false` の辺・頂点はModelの返り値に関係なく禁止です。

コールバックは同じ入力・引数なら同じ答えを返す関数にします。呼出し回数、乱数、時計、探索の途中状態によって費用を変えてはいけません。最適化する問題自体が途中で変わってしまうためです。返す費用は `cost_scale` 適用前で、有限かつ非負である必要があります。

## 25. 発展コード21：混雑に応じた資源費用

**例22 — `examples/22_resource_cost.cpp`**

容量は守れても、一つの設備に集中するほど不利になる費用を加えます。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(2);
    int busy = p.add_resource(2);
    int a = p.add_arc(0, 1, 1);
    p.arcs[a].uses.push_back({busy, 1, 0});
    p.add_arc(0, 1, 3);
    p.add_request(0, 1);
    p.add_request(0, 1);
    struct Model : R::DefaultModel {
        int64_t resource_cost(const R::Problem&, int, int64_t load) const {
            return load * load;
        }
    };
    auto solver = R::make_solver(p, Model{});
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    const auto& result = solver.solve(limits);
    assert(result.feasible());
    auto evaluation = solver.evaluate(result.solution);
    const auto& value = *result.summary.value;
    assert(evaluation.resource_load[busy] == 1);
    assert(value.route_cost == 4 && value.resource_cost == 1);
    std::cout << "route=4 resource=1 total=" << value.total() << '\n';
}
```

出力：

```text
route=4 resource=1 total=5
```

頂点0から1へ平行な辺を2本作ります。安い辺は費用1で資源 `busy` を1消費し、高い辺は費用3でその資源を使いません。

`resource_cost(const Problem&, int, int64_t load)` の第1引数は問題、第2引数は資源ID、第3引数はその資源の**背景を含む合計使用量**です。この例では第1・第2引数を使わないため名前を省略しています。返り値は `Cost` と同じ `int64_t` です。

式 `load * load` は使用量の2乗です。

| 安い辺を使う要求数 | 経路費用 | 資源費用 | 合計 |
|---:|---:|---:|---:|
| 0 | 3＋3＝6 | 0 | 6 |
| 1 | 1＋3＝4 | 1 | 5 |
| 2 | 1＋1＝2 | 4 | 6 |

1件ずつ分散した合計5が有利です。資源費用を要求ごとに1＋1と数えるのではなく、同じ資源の合計2に対して4と数えることがポイントです。

資源費用は使用量に対して非減少で、使用量0のとき0、有限かつ非負である必要があります。2乗のような形に限らず、「使えば固定料金」の段差も条件を満たします。有限容量が `U` の場合、コールバックに渡される `load` は0から `U` の範囲です。容量超過の解の目的値をこの関数で自由に定義するAPIではありません。

## 26. 発展コード22：Modelのパラメータを変更する

**例23 — `examples/23_set_model.cpp`**

例22の2乗費用に倍率を付け、途中で倍率を変えます。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(2);
    int busy = p.add_resource(2);
    int a = p.add_arc(0, 1, 1);
    p.arcs[a].uses.push_back({busy, 1, 0});
    p.add_arc(0, 1, 3);
    p.add_request(0, 1);
    p.add_request(0, 1);
    struct Model : R::DefaultModel {
        int64_t factor;
        explicit Model(int64_t x) : factor(x) {}
        int64_t resource_cost(const R::Problem&, int, int64_t load) const {
            return factor * load * load;
        }
    };
    auto solver = R::make_solver(p, Model(1));
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    auto first = solver.solve(limits);
    assert(first.feasible() && first.summary.value->total() == 5);

    solver.set_model(Model(4));
    const auto& second = solver.resume(limits);
    assert(second.feasible() && second.summary.value->total() == 6);
    assert(solver.evaluate(second.solution).resource_load[busy] == 0);
    std::cout << "factor1_total=5 factor4_total=6 busy_load=0\n";
}
```

出力：

```text
factor1_total=5 factor4_total=6 busy_load=0
```

`Model(1)` は `factor=1`、`Model(4)` は `factor=4` のModelを作ります。最初は例22と同じ合計5です。倍率4になると、安い辺を1件使うだけで資源費用4が掛かるため、2件とも費用3の辺を選ぶ合計6が有利になります。

`solver.set_model(Model(4))` は、Solverが持つModelを同じ型の新しい値に置き換え、`refresh` 相当の再評価まで行います。ここで追加の `refresh` は不要です。その後 `resume` します。

新旧で目的関数自体が変わったため、5と6を比べて「悪化した」と判断しないでください。新しい倍率で旧経路を評価すると、新しい解より高くなっています。

ModelはSolverが値として所有します。この例のように標準コンストラクタがない型でも、生成時に渡せば使えます。参照メンバやムーブ専用型も扱えます。異なるModel型へ変える場合は、新しいSolverを作り、保存した `Solution` を `improve` に渡します。

## 27. 発展コード23：外部の料金表を参照するModel

**例24 — `examples/24_external_model.cpp`**

大きな表をModelごとコピーせず、利用側の表を参照する例です。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(2);
    int a = p.add_arc(0, 1, 1);
    int b = p.add_arc(0, 1, 3);
    p.add_request(0, 1);
    std::vector<int64_t> toll(p.arcs.size(), 0);
    struct Model : R::DefaultModel {
        const std::vector<int64_t>& toll;
        explicit Model(const std::vector<int64_t>& x) : toll(x) {}
        std::optional<int64_t> arc_cost(const R::Problem& p, int, int a) const {
            return p.arcs[a].cost + toll[a];
        }
    };
    auto solver = R::make_solver(p, Model(toll));
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    auto first = solver.solve(limits);
    assert(first.feasible() && first.summary.value->total() == 1);

    toll[a] = 10;
    solver.refresh();
    const auto& second = solver.resume(limits);
    assert(second.feasible() && second.summary.value->total() == 3);
    assert(second.solution.routes[0].arcs == std::vector<int>{b});
    std::cout << "before=1 after=3\n";
}
```

出力：

```text
before=1 after=3
```

`toll` は辺ごとの追加料金です。`p.arcs.size()` 個の要素を、すべて0で初期化します。Modelの `const std::vector<int64_t>& toll` は、その表を読み取る参照です。表そのものをModelへコピーしません。

`toll[a]=10` で費用1の辺の料金を10増やすと、その辺の費用は11になります。費用3の別の辺を使う方が有利です。

Problem本体を変更していなくても、Modelが参照する表を変えたので `refresh` が必要です。表とProblemはSolverの使用中ずっと生存させ、探索中には変更しません。例では `p`、`toll`、`solver` の順で作るため、最後に作ったSolverが先に破棄されます。

料金表の要素数も、辺IDにアクセスできる長さを保つ必要があります。辺を追加する場合は表を更新し、前の例で説明した `reset` を使います。

## 28. 発展コード24：小数の費用

**例25 — `examples/25_double.cpp`**

整数に収まらない費用を使います。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<double>;
    R::Problem p(3);
    p.add_arc(0, 1, 0.5, 1);
    p.add_arc(1, 2, 0.75, 1);
    p.add_arc(0, 2, 2.0, 1);
    int k = p.add_request(0, 2);
    p.requests[k].cost_scale = 1.5;
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    const auto& result = solver.solve(limits);
    assert(result.feasible());
    double total = result.summary.value->total();
    assert(std::abs(total - 1.875) < 1e-9);
    std::cout << std::fixed << std::setprecision(3) << "total=" << total << '\n';
}
```

出力：

```text
total=1.875
```

型を `CapacitatedRouter<double>` に変えると、辺・頂点費用、未接続損失、費用倍率、Modelの返す費用、目的値が `double` になります。容量、消費量、要求量、要求別上限は引き続き `int64_t` の整数です。

安い経路の費用は `(0.5＋0.75)×1.5＝1.875` です。`std::abs(total-1.875)<1e-9` は、差の絶対値が10億分の1より小さいことを確認します。小数計算は一般に丸め誤差を含むため、テストでは必要な精度に応じて誤差幅を決めます。

`std::fixed` と `std::setprecision(3)` は小数点以下3桁で表示するための指定で、探索精度のパラメータではありません。

費用が元々整数なら標準の `int64_t` が扱いやすくなります。すべての中間積・総和が型の範囲に収まることが前提です。単位を細かくしすぎて桁あふれしないよう、問題に合った単位を選びます。

## 29. 発展コード25：状態を頂点に持たせる

**例26 — `examples/26_state_graph.cpp`**

「高速移動は各要求で1回まで」のように、過去の行動に依存する条件を表します。位置だけではなく、高速移動をすでに使ったかも頂点に含めます。これを**状態展開**と呼びます。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    auto state = [](int position, int used_fast) { return 2 * position + used_fast; };
    const int goal = 6;
    R::Problem p(7);
    int fast = p.add_resource(1);
    for (int position = 0; position < 2; ++position) {
        for (int used = 0; used < 2; ++used)
            p.add_arc(state(position, used), state(position + 1, used), 3);
        int a = p.add_arc(state(position, 0), state(position + 1, 1), 1);
        p.arcs[a].uses.push_back({fast, 0, 1});
    }
    p.add_arc(state(2, 0), goal, 0);
    p.add_arc(state(2, 1), goal, 0);
    p.add_request(state(0, 0), goal);
    p.add_request(state(0, 0), goal);
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    const auto& result = solver.solve(limits);
    assert(result.feasible() && result.summary.value->total() == 10);
    assert(solver.evaluate(result.solution).resource_load[fast] == 1);
    std::cout << "fast_uses=1 total=10\n";
}
```

出力：

```text
fast_uses=1 total=10
```

位置は0、1、2です。`used_fast` は未使用なら0、使用済みなら1です。`state(position, used_fast)` の `2*position+used_fast` によって、組を次の頂点IDへ変換します。

| 位置 | 未使用の頂点 | 使用済みの頂点 |
|---:|---:|---:|
| 0 | 0 | 1 |
| 1 | 2 | 3 |
| 2 | 4 | 5 |

`state` は小さな関数を変数に入れたラムダ式です。通常移動の費用は3で、使用済み状態を変えずに位置を1進めます。高速移動は費用1で、未使用から使用済みへだけ移れます。使用済みからもう一度高速移動する辺を作らないので、1要求で2回使う経路は存在しません。

位置2のどちらの状態でも到着としたいので、共通の仮想終点 `goal=6` を作り、頂点4・5から費用0でつなぎます。そのため `Problem(7)` です。

さらに資源 `fast` の容量を1にし、全要求を合わせた高速移動も1回までにします。2件のうち1件は高速1＋通常3で4、もう1件は通常3＋3で6、合計10になります。

この例の「1要求で1回まで」は要求別上限でも表せます。ここでは状態展開の仕組みを学ぶために使っています。向きによる移動費用、順番に通るチェックポイント、時刻により変わる移動可能性などにも、必要な履歴を有限の状態にまとめられれば同じ考え方を使えます。

展開後の別の頂点でも、同じ物理設備を使うなら同じ資源IDを付けます。単純路の制約がかかるのは**展開後の頂点**です。同じ位置へ別の状態で戻ることは自動では禁止されません。物理位置の再訪も禁止したい場合は、位置ごとの消費と要求別上限等で表す必要があります。

## 30. 発展コード26：複数の出発点から試す

**例27 — `examples/27_multistart.cpp`**

異なるseedで新規探索を繰り返し、最も良い実行可能解を利用側で保存します。

```cpp
#include "capacitated_router_v05.hpp"
#include <iostream>

int main() {
    using R = CapacitatedRouter<>;
    R::Problem p(3);
    p.add_arc(0, 1, 1, 1);
    p.add_arc(1, 2, 1, 1);
    p.add_arc(0, 2, 5, 1);
    p.add_request(0, 2);
    p.add_request(0, 2);
    auto solver = R::make_solver(p);
    R::Limits limits;
    limits.time_limit_us = -1;
    limits.max_iterations = 100;
    auto better = [&](const R::Value& a, const R::Value& b) {
        if (p.objective == R::Objective::PenaltyPlusCost) return a.total() < b.total();
        if (a.reject_cost != b.reject_cost) return a.reject_cost < b.reject_cost;
        return a.route_cost + a.resource_cost < b.route_cost + b.resource_cost;
    };
    std::optional<R::Result> best;
    for (uint64_t seed : {0ULL, 1ULL, 2ULL}) {
        R::Options options;
        options.seed = seed;
        const auto& result = solver.solve(limits, options);
        if (result.feasible() && (!best || better(*result.summary.value, *best->summary.value)))
            best = result;
    }
    assert(best && best->feasible() && best->summary.value->total() == 7);
    std::cout << "best_total=" << best->summary.value->total() << '\n';
}
```

出力：

```text
best_total=7
```

seedは0、1、2の3種類です。`solve` を呼ぶたびに探索状態と乱数列の出発点がリセットされます。`max_iterations=100` は各回に適用するため、最大で合計300反復です。今回の小例はどのseedでも同じ費用になります。複数seedが常に有利という意味ではなく、再出発を比較するための利用パターンです。

`best` はまだ答えがない場合もあるため `std::optional<Result>` にしています。`best=result` でコピーして保存するので、次の `solve` でも消えません。

`better` は、和目的なら `total()`、辞書式目的なら未接続損失を先に比較する小さな関数です。`[&]` によって現在の `p.objective` を参照します。実行可能性を確認した結果同士だけを比較します。

時間制限でこのパターンを使うなら、全試行共通の `deadline` を用意し、次の試行を始める前にも残り時間を確認します。1回ずつ同じ相対予算を与えるだけでは、全体の時間予算が試行数倍になります。再出発と `resume` による継続のどちらが有利かは、対象問題で比べます。

## 31. 利用時に引くAPI索引

### 31.1 入力を作るAPI

| 型・API | 内容・標準値 | 学習例 |
|---|---|---|
| `CapacitatedRouter<Cost>` | 標準 `int64_t`。費用用に `double` も使用可 | 01、25 |
| `Problem(n=0)` | 頂点を `n` 個作る | 01 |
| `Problem::vertices/arcs/resources/requests` | 公開の入力配列。IDは各配列の添字 | 02以降 |
| `add_arc(u,v,cost=1,capacity=-1)` | 有向辺を追加し辺IDを返す | 01、03 |
| `add_undirected_edge(u,v,cost=1,capacity=-1)` | 逆向き2辺と共有容量。2つの辺IDを返す | 04 |
| `add_vertex_capacity(v,capacity)` | 頂点の専用容量を追加し資源IDを返す | 05 |
| `add_resource(capacity=-1,background_load=0)` | 共有資源を追加し資源IDを返す | 07、17 |
| `add_request(s,t,demand=1)` | 有効な必須要求を追加し要求IDを返す | 01、03 |
| `Vertex` | `cost=0`、`enabled=true`、`uses` | 05、06、18 |
| `Arc` | `from`、`to`、`cost=1`、`enabled=true`、`uses` | 02、18、20 |
| `Resource` | `capacity=-1`、`background_load=0` | 07、17 |
| `ResourceUse` | `resource`、`per_unit=1`、`per_use=0` | 07、08 |
| `LocalLimit` | `resource`、`upper`。同じ要求で資源IDを重複させない | 09 |
| `Request` | 下表の設定。`source` と `target` は頂点ID | 01以降 |
| `Objective` | `PenaltyPlusCost` または `PenaltyThenCost` | 10、11 |

| 要求の設定 | 標準値 | 意味 |
|---|---|---|
| `demand` | 1 | 資源消費に用いる量 |
| `active` | true | falseなら無効 |
| `required` | true | trueなら接続が必要 |
| `reject_cost` | 0 | 有効な任意要求が未接続のときの損失 |
| `cost_scale` | 1 | 辺・頂点費用の合計に掛ける倍率 |
| `consume_endpoints` | true | 端点の頂点資源を消費するか |
| `uses` | 空 | 接続時に1回適用する資源消費 |
| `limits` | 空 | この要求だけの資源消費上限 |
| `fixed_route` | 値なし | 値ありなら指定経路、未接続も含めて固定 |

要求・辺・資源を新しく追加する際は、補助関数を使うとID付与と既定値が明確です。公開配列を直接変更する場合も同じ入力前提を守ります。費用・量・消費係数・背景・要求別上限は非負、容量だけは上限なしの `-1` を許します。IDは有効な配列添字である必要があります。

### 31.2 探索と再利用のAPI

| 操作 | 用途 | 乱数・状態の扱い |
|---|---|---|
| `make_solver(p, model={})` | Model型を推論して生成 | Problemを参照、Modelを値として所有 |
| `Solver<Model>(p, model={})` | 型を明示して生成 | 同上 |
| `solve(limits, options={})` | 新規に解く | seed・探索反復をリセット |
| `improve(solution, limits, options={})` | 渡した解から始める | seed・探索反復をリセット |
| `resume(limits)` | 入力を変えず継続 | 探索・乱数・最良解・作業領域を継続 |
| `refresh()` | 属性、資源、要求、参照データの更新 | 旧解を現在の条件で再評価。最良解の比較をやり直す |
| `set_model(model)` | 同じ型のModelを置換 | 置換後に再評価も行う |
| `reset(p)` | 接続構造の変更、別Problemへの切替え | 前処理を再構築。探索状態を破棄。Modelは保持 |
| `evaluate(solution)` | 現在の入力・Modelで全評価 | 探索を進めない。詳細診断を返す |

`Limits` は相対マイクロ秒 `time_limit_us=100'000`、絶対締切 `deadline=Clock::time_point::max()`、反復上限 `max_iterations=-1` です。`Options` は `seed=0` のみです。`solve` / `improve` / `resume` の時間指定は省略できませんが、標準値を使う `{}` を渡せます。

ProblemとModelの参照先はSolver使用中に有効である必要があります。特に、一時的なProblemを渡してSolverだけを保存しないでください。Problemオブジェクト自身を移動したり破棄したりする場合も参照が問題になります。探索中に入力を変更する使い方はしません。

### 31.3 出力の型

| 型 | 内容 |
|---|---|
| `Route` | `connected=false` と空の `arcs` が標準。辺列は有向辺ID順 |
| `Solution` | 要求順の `routes` |
| `Value` | 3費用と `total()` |
| `Summary` | 5種類の制約違反件数、任意の `value`、`feasible()` |
| `Evaluation::LimitViolation` | `request`、`resource`、`used`、`upper` |
| `Evaluation` | `summary`、`resource_load`、違反の詳細配列、`feasible()` |
| `Statistics` | 反復・探索・採用の回数と `elapsed_us` |
| `Result` | `solution`、`summary`、`stop_reason`、`statistics`、`feasible()` |

`Route`、`Solution`、`Value` は `==` で比較できます。経路の同じ費用を調べる比較ではなく、保存されたフィールドが等しいかを比較します。`double` の `Value` の等値比較も、自動的に誤差を許すものではありません。

### 31.4 AHCへ導入する順番

1. 公式の条件が、単純路、加算する消費、共有容量、要求別上限、Modelの費用に合うかを確認します。
2. 極小入力で、公式の評価器と `evaluate` の使用量・費用を手計算と照合します。スコアの最大化を費用の最小化へどう対応させたかも確認します。
3. まず固定反復・固定seedで比較し、入力の規模や混雑に応じて実行可能解を得られるかを見ます。
4. その後、前処理と入出力を含む実時間を測って、`time_limit_us` や外側の `deadline` を設定します。
5. ターン制なら、入力不変は `resume`、属性更新は `refresh`、接続構造更新は `reset` を使い分けます。

単一ファイル提出が必要なら、ヘッダのライブラリ本体部分と利用コードを1つにまとめます。配布ヘッダの末尾には自己テストがあり、`#if __INCLUDE_LEVEL__ == 0` 以降はヘッダを直接コンパイルしたとき用です。単に全内容を貼り付けるとテストの `main` が有効になるため、提出時はその自己テスト部分を除き、利用例のヘッダへの `#include` も除きます。通常のヘッダ利用では自己テストは含まれません。

## 32. 付録：実装アルゴリズムの概要

ここからは「APIをどう使うか」ではなく、「内部でどう答えを探すか」です。利用コードを書くために、すべてを先に覚える必要はありません。

### 32.1 全体の流れ

1. **前処理**：各頂点から出る辺をすぐ列挙できる形に整理し、作業用の配列を用意します。
2. **初期化**：背景使用量と固定経路を集計し、初期解があれば取り込みます。固定条件の矛盾を調べます。
3. **初期構築**：必須要求を先に、任意要求は未接続損失が大きいものを先に扱い、経路を作ります。
4. **再構築**：一部の要求の経路をいったん外し、空いた資源を使ってつなぎ直します。
5. **判定**：制約、目的値、探索を広げるための受理判定を行います。採用しない場合や途中で時間切れになった場合は元へ戻します。
6. **保存**：すべての制約を満たす解のうち、見つけた最良解を別に保持します。予算が尽きたらその解を返します。

一度も実行可能解を得ていない場合は、作業中の解を全評価し、違反情報を添えて返します。返却時には必ず `feasible()` を確認します。

### 32.2 前処理とメモリの再利用

入力の辺配列はID順に保ち、別の配列で「頂点ごとの出辺」をまとめます。この形式をCSRと呼びます。頂点ごとの開始位置と、出辺IDを並べた配列があれば、必要な辺だけを連続して読み出せます。

経路探索の距離、親の辺、訪問状態、優先度付き待ち行列の領域などもSolver内で再利用します。毎回同じ大きさの配列を作る負担を抑えるためです。

各経路について、費用と「資源ID・合計消費量」の組を保持します。要求を取り外すときはその分だけ引き、追加するときはその分だけ足します。資源費用は変更前の合計負荷の費用を引き、変更後の合計負荷の費用を足します。非線形でも合計負荷に対する定義を保てます。

### 32.3 初期化と探索順

新規探索では背景と固定経路から始めます。初期解がある場合も、固定指定を優先し、無効要求を未接続にし、現在の条件で使えない可動経路を除きます。固定分と背景だけで容量を超える等の矛盾があれば、通常の探索へ進みません。

可動要求を一度シャッフルし、その後で必須要求を前に、任意要求は未接続損失の大きい順に安定ソートします。これにより同じ優先度の要求にはseedによる順序の違いが残ります。

最初はこの順に1要求ずつ再構築します。以後は一部の要求を選んでつなぎ直す段階へ移ります。`improve` が実行可能解を受け取った場合は、初期構築に入る前からその解を最良候補として保存できます。

### 32.4 経路候補を作る通常の探索

通常の候補生成には、非負の重みを扱うDijkstra法を使います。「現在わかっている候補のうち、重みが最も小さい頂点から順に調べる」方法です。優先度付き待ち行列で小さい候補を取り出し、辺を1本延ばした候補を更新します。

ここで使う**探索用の重み**は、公式の目的値そのものではありません。概ね次を足します。

| 重みに入るもの | 役割 |
|---|---|
| 辺・到着頂点の費用と要求倍率 | 安い経路を選ぶ |
| 資源使用量を増やしたときの資源費用の差 | 設備の集中や稼働費用を考える |
| 混雑に対する価格 | 容量を圧迫する経路を避ける |
| 要求別上限の資源に対する価格 | 燃料などを使いすぎる経路を避ける |

1回の経路探索中は、他の経路による現在負荷を固定します。同じ候補経路が途中で資源を何度も使うことまでは、通常探索の距離1個では完全に表せません。そのため、得られた経路を後から正確に集計し、重複消費と上限を確認します。

開始頂点や要求自身の費用・消費も最終的な集計に入ります。通常の辺探索では経路に依存しない開始部分を省けますが、これを「端点費用を数えていない」と読まないでください。

通常は現在負荷に対して容量を守る候補を探します。必須要求でうまくいかない場合に限り、他の可動要求との一時的な容量超過を認める候補も試します。ただし個々の消費指定だけで、固定経路と背景を除いた余地を超える遷移は除外します。経路全体の重複消費による容量超過は別途集計し、実行可能解としては保存しません。

### 32.5 要求別上限に違反したときの再試行

通常探索で得た経路が要求別上限を超えていたら、**違反した資源だけ**の探索価格の係数を4倍にして、別の経路を探します。最大3回繰り返します。違反していない資源まで一緒に高くしないことで、余裕のある資源へ回る選択を残します。

係数は各要求の候補生成を始める際に1へ戻ります。ずっと学習して増え続ける価格ではありません。同じ資源に続けて違反すれば、係数は4、16、64と増えます。

この価格は目的関数の燃料費を変更していません。候補を見つける方向付けであり、採否は元の制約と目的で判定します。

### 32.6 複数の途中候補を持つ探索

再試行でも上限に合う経路が得られない場合、費用だけでなく一部の資源消費も記録した**ラベル**を使います。ラベルは「ある頂点までの途中経路と、費用・消費量の記録」です。

たとえば同じ頂点に「費用2・燃料5」と「費用3・燃料1」の2通りで来た場合、前者だけ残すと、その後の燃料不足で失敗するかもしれません。両方を残すことで続きを比較できます。

記録する資源は、要求別上限にあるものと、直前の候補が容量超過したものです。共有容量がある場合は、残容量に合わせて途中消費の上限も設定します。

既存候補の費用が新候補以下で、追跡資源の消費量もすべて新候補以下なら、新候補を省きます。これを支配関係による枝刈りと呼びます。ただし訪問済み頂点の違いも将来に影響するので、この枝刈りを含む実装は完全な列挙ではありません。

保持するラベルは1頂点あたり最大4個です。生成ラベル数が頂点数の32倍以上になったら、次の頂点展開へ進まず打ち切ります。1頂点の展開中に多くの辺を追加できるため、総数が厳密に32倍を超えないという制限ではありません。

親ラベルをたどって頂点の再訪を防ぎ、終点に着いた候補も全資源を正確に集計します。追跡する資源とラベル数を限定しているため、可解性を完全に判定するアルゴリズムではありません。

### 32.7 経路の取り外しとつなぎ直し

通常は1要求を選びます。4分の1の確率で2〜6要求のまとまりを作ろうとしますが、要求数が少ない場合はそれより小さくなります。

最初の要求は、未接続の必須要求や混雑している経路を優先して探します。全要求を毎回調べず、少数の候補を抽出します。複数要求を動かすときは、同じ資源を使う要求を集め、足りなければ別の要求も抽出します。

選んだ経路をいったん全部外すことで、相互に譲り合わなければ変更できない配置も試せます。再構築順はシャッフルしたうえで必須を先にします。各要求の候補は、未接続状態に比べて内部評価が良くなる場合に採用します。任意要求では、接続費用が損失に見合わず未接続のままになることもあります。

再構築中は辺重みに小さな乱れを与え、毎回同じ経路だけを試す状況を減らします。倍率はおおむね0.8以上1.2未満です。同じ候補生成内では一貫した辺ごとの乱れを使います。辞書式目的では、1反復おきに弱い占有率価格も加えます。

### 32.8 一時的に悪い解を受け入れる理由

1本だけを変えると費用が増えるのに、その後に別の1本を変えると大きく改善する場合があります。毎回の改善だけを許すと、その途中へ進めません。

本実装は焼きなまし法に基づく受理判定を使います。良い候補は採用し、悪化する候補も一定の確率で採用します。概念上の確率は次の形です。

$$q=\exp(-\Delta/T)$$

ここで $q$ は悪化候補を受け入れる確率、$\Delta$ は内部評価で測った悪化量、$T$ は温度と呼ぶ正の調整値です。`exp` は指数関数です。$\Delta$ が大きいほど採用しにくく、$T$ が高いほど悪化を許しやすくなります。

内部評価は、必須未接続の件数、制約違反の有無と大きさ、目的値を段階的に見ます。受理確率の悪化量もこの段階に応じて決めます。したがって、常に「総費用の差だけ」を式に代入しているわけではありません。

内部の「良くなったか」の比較は、まず必須未接続が少ない方を優先します。同数なら容量・要求別上限に違反しない方、両方に違反があれば正規化した超過量が小さい方、その後に目的値を見ます。正規化は、超えた量を上限の大きさで割る考え方です。上限0のときは1で割ります。

受理確率で使う差の尺度は、必須未接続と超過量には4倍の差、辞書式の未接続損失には最大の未接続損失を基準にした差、経路等の費用には問題の規模に応じた差を使います。費用の基準は、入力に保存された平均辺費用に頂点数の平方根を掛け、最低1とした値です。独自Modelの費用分布を完全に推定するものではありません。

温度は反復数で周期的に下がります。可動要求数を $M$、探索開始からの累計反復数を $I$、冷却周期を $N$、周期内の割合を $\phi$ とすると、実装は次の形です。

$$N=32\max(1,M),\qquad \phi=(I\bmod N)/N,\qquad T=0.25\times0.04^{\phi}$$

`mod` は割り算の余りです。周期が進むと温度が下がり、周期の先頭へ戻ると再び高くなります。`resume` では累計反復数と乱数状態を継続します。`solve` と `improve` では出発点をリセットします。

探索の作業状態が一時的に悪くなっても、最良実行可能解は別に保存します。これが「探索途中の状態」と「返す答え」を分ける理由です。

### 32.9 取消し、数値、時間確認

要求の再構築前に、元の経路と集計結果を保存します。不採用や時間切れなら元へ戻します。途中まで一部の資源だけ更新された状態を、そのまま返さないためです。

整数版は実際の目的値を `Cost` で保持して比較します。探索用の重み、違反量の正規化、受理確率には `double` を使います。小数版は差分計算の誤差が蓄積するため、最良解を保存する際に全評価で目的値を確定します。

容量超過中の探索では、有限容量の資源費用を容量までの負荷で評価し、超過自体には別の混雑評価を付けます。これは探索内部の扱いです。`evaluate` は容量超過の解の `value` を返しません。

通常経路探索では概ね128作業単位ごと、ラベル探索では概ね32作業単位ごとに締切を調べます。待ち行列の取り出しや辺の走査が作業単位で、各Model呼出しや経路集計の途中で必ず中断できるわけではありません。Modelが重ければ、それ自体も時間超過の要因になります。

IDは `int`、容量・消費・要求量は `int64_t` を前提とします。配列長、積、総和は採用型の範囲に収めます。`long double`、`__int128` を通常処理で使う実装ではありません。入力前提は必要箇所で `assert` により確認し、独自に例外を `throw` しません。

### 32.10 再利用の効き方と計算量

ここで $n$ は頂点数、$m$ は有向辺数、$r$ は資源数、$k$ は要求数、$u$ は入力全体の消費・上限指定数、$h$ は経路集計で調べる頂点・辺・消費指定等の作業量です。$O(\cdot)$ は、入力が大きくなったときに処理量がどう増えるかを表す記号です。

| 処理 | おおよその増え方・注意 |
|---|---|
| Solver生成・`reset` | 通常 $O(n+m+r+k+u)$。接続構造と入力を取り込む |
| `refresh`・`set_model` | 通常 $O(n+m+r+k+u+h)$。保存経路も再評価する |
| `evaluate` | 通常 $O(n+r+k+h)$。全経路の集計と診断 |
| 入力不変の `resume` | 主に追加探索分。実行可能解未発見なら最後に全評価も必要 |
| 実行可能な最良解を保持した0反復 `resume` | 内部では $O(1)$。利用側が結果をコピーする時間は別 |

これらはコールバックが定数時間程度の場合の目安です。`assert` 有効時には、要求内の上限指定の重複確認などの追加処理があります。複数ラベル探索はラベル数、追跡資源数、経路長にも依存し、単純な最短路と同じ計算量とは考えられません。

`refresh` は接続構造を再利用しますが、変更箇所だけの差分更新ではありません。少しずつ時間を足して同じ問題を解くなら `resume`、入力が変わったなら必要な再評価をしてから続ける、という区別が性能と正しさの両方に関係します。

### 32.11 ソースコードを読む際の対応表

以下はSolver内部の関数です。利用側から呼ぶAPIではありません。

| 内部処理 | 主な関数名 | 読む観点 |
|---|---|---|
| 経路の正確な集計 | `analyse` | 頂点・辺・要求自身の消費、単純路、要求別上限 |
| 差分更新 | `change`、`replace_route` | 経路を外す・追加する際の負荷と費用 |
| 最良実行可能解の保存 | `remember` | 整数と小数の目的値比較、作業状態との分離 |
| 初期化 | `initialize` | 背景、固定、旧解、可動要求、初期順序 |
| 探索用価格 | `use_weight`、`edge_weight` | 資源費用の増分、混雑、要求別価格、辺の摂動 |
| 通常の候補経路 | `shortest` | 再利用する配列と待ち行列、親辺からの経路復元 |
| 資源を追跡する候補経路 | `labelled` | 複数ラベル、支配判定、単純路、生成数制限 |
| 候補の再試行 | `candidate`、`construct` | 違反した資源だけの価格増加、必須要求の追加試行 |
| 動かす要求と取消し | `select_group`、`trial` | 近傍、再構築、採用、不採用時の復元 |
| 比較・受理・終了 | `better_value`、`better`、`accept`、`run` | 本来の目的、探索用評価、予算と停止理由 |

複数ラベルの消費量は、ラベルごとに独立した小配列を作らず、連続した配列へ並べます。資源の集計用配列は、触った資源の値を集計後に0へ戻して再利用します。このような領域の再利用と、経路単位の差分更新が、表現力を保ちながら反復を多く回すための実装上の要点です。

## 33. このガイドの検証

本文の各C++ブロックは、同梱の `.cpp` と同じ内容です。27本を独立に `-std=c++20 -O2 -Wall -Wextra` でコンパイルし、実行時の確認と標準出力を照合しました。数学的な最適性の一般的保証をテストしたという意味ではなく、各教材が説明どおり動くことの確認です。

同梱ディレクトリでPython 3とGCCを使い、次のコマンドで再実行できます。

```sh
python3 check_examples.py
```

`verification/examples.json` にコンパイラ、ヘッダのSHA-256、各例の出力と結果があります。時間制限例の統計値は固定値として照合せず、費用・成功・停止理由と出力形式を確認します。例02は同費用の要求割当てが入れ替わっても許容します。

外部Markdownリーダー向けに、見出し・表・コードブロック・数式の前処理を確認し、本文で使った数式をLaTeXでも検証しています。数式の表示には、利用するリーダー側の数式拡張を有効にしてください。
