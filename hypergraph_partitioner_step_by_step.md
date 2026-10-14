# HypergraphPartitioner v07 ステップバイステップガイド

**仕事の割当、共有物の容量、候補の採否を、小さな動くC++コードから学ぶ。**

対象は `hypergraph_partitioner_v07.hpp`。数学は高校で学ぶ和・不等式・二次式を使います。C++の変数、配列、繰返しを知っていれば読み進められるよう、ライブラリ特有の用語は登場時に説明します。

最小例1本と発展例26本は、すべて独立したプログラムです。各例の「変更点」は直前または指定した例との関係を示しますが、コードをつぎ足して実行する必要はありません。

| 読む順番 | 学ぶこと |
|---|---|
| 第1部 | 解く問題と目的関数 |
| 第2部 | 厳密解法が使える条件、類似ソルバーとの使い分け |
| 第3部・例01 | 準備、最小の割当問題、結果の読み方 |
| 例02〜09 | 配置費用、資源、配置制限、固定費、負荷の目標 |
| 例10〜17 | 共有物、3種類の個数、採否、個数に応じた費用 |
| 例18〜21 | 違反の診断、既存解・部分範囲の改善、再開 |
| 例22〜27 | 条件更新、範囲の衝突、問題の交換、探索設定、複数開始 |
| 第4部 | 入力とAPIの索引、間違えやすい点 |
| 付録 | 実装の流れ、各アルゴリズム、差分計算の詳細 |

## 第1部　何を解くライブラリか

### 1.1　「何をどこへ置くか」を決める

4個の仕事を2台のサーバーへ割り当てる場面を考えます。各サーバーには2個まで仕事を置けます。仕事0と仕事1は同じデータを使うため、別のサーバーへ置くと追加費用が5かかる、とします。

| 割当 | サーバー0 | サーバー1 | 容量を守るか | 追加費用 |
|---|---|---|---|---|
| A | 仕事0、1 | 仕事2、3 | 守る | 0 |
| B | 仕事0、2 | 仕事1、3 | 守る | 5 |
| C | 仕事0、1、2 | 仕事3 | 守らない | 0 |

AとBは条件を満たす**実行可能解**です。その中では費用が小さいAが望ましく、Cは費用が小さくても採用できません。このように、条件を守る割当の中で、費用を小さくするのが本ライブラリの仕事です。

対象はサーバーに限りません。人を班へ分ける、部品を工場へ配る、実行方式の候補を選んで機械へ割り当てる、といった問題も表せます。

| ライブラリの言葉 | この例での意味 |
|---|---|
| 要素 `Vertex` | 仕事。各要素を1か所へ置く |
| グループ `Group` | サーバー。番号は0から始まる |
| 資源・負荷 `load` | 消費する容量。仕事数、CPU量、メモリ量など |
| ハイパー辺 `Hyperedge` | 関係する要素の集合。この例では `{0, 1}` |
| 未採用 `-1` | その要素をどのグループにも置かない。明示的に許可した要素だけが選べる |

普通のグラフの辺は2個の要素を結びます。ハイパー辺は3個以上でもよく、「この集合が同じデータを使う」「この集合から2個採用する」のような関係を表します。辺は方向も順番も持ちません。

1個の要素を複数グループへ複製する機能ではありません。複数方式の候補を用意する場合は、候補ごとに別の要素を作り、後で学ぶ採用数の制約を組み合わせます。

### 1.2　3種類の「何個」を区別する

ハイパー辺が仕事 `{0, 1, 2}` を含み、0と1をグループ0へ、2をグループ1へ置いたとします。

| 個数 | 値 | 数えているもの |
|---|---:|---|
| `span` | 2 | この辺の要素が使っている通常グループの数 |
| `selected` | 3 | この辺の採用された要素の数 |
| グループ0の `local` | 2 | この辺からグループ0へ置いた要素の数 |
| グループ1の `local` | 1 | この辺からグループ1へ置いた要素の数 |

仕事2を未採用にすると、それぞれ1、2、2、0になります。未採用を表す `-1` は通常グループではなく、`span` に数えません。

### 1.3　変数と集計値

以下の式は全機能をまとめた仕様です。最小例では多くの費用を0にして使います。まず、番号と割当を定義します。

| 記号 | 意味 |
|---|---|
| $N$、$v$ | 要素数と要素番号。$v=0,\ldots,N-1$ |
| $K$、$k$ | グループ数とグループ番号。$k=0,\ldots,K-1$ |
| $R$、$r$ | 資源の種類数と資源番号。$r=0,\ldots,R-1$ |
| $M$、$e$ | ハイパー辺数と辺番号。$e=0,\ldots,M-1$ |
| $a$、$a_ {v}$ | 割当全体と、要素$v$の配置先。通常は$0$以上$K-1$以下、未採用は$-1$ |
| $T_ {e}$ | 辺$e$に含まれる要素番号の集合 |
| $\mathbf{1}[条件]$ | 条件が成り立つなら1、そうでなければ0 |

記号 $\sum$ は「足し合わせる」という意味です。例えば $\sum_ {v=0}^{N-1}$ は全要素について足し合わせます。$\sum_ {v\in T_ {e}}$ は、その辺の要素だけが対象です。

$$n_ {k}=\sum_ {v=0}^{N-1}\mathbf{1}[a_ {v}=k],\qquad c_ {e,k}=\sum_ {v\in T_ {e}}\mathbf{1}[a_ {v}=k]$$

$$s_ {e}=\sum_ {k=0}^{K-1}\mathbf{1}[c_ {e,k}>0],\qquad q_ {e}=\sum_ {k=0}^{K-1}c_ {e,k}$$

| 記号 | 意味 |
|---|---|
| $n_ {k}$ | グループ$k$に置いた全要素の個数 |
| $c_ {e,k}$ | 辺$e$のグループ$k$における `local` 個数 |
| $s_ {e}$ | 辺$e$の `span` |
| $q_ {e}$ | 辺$e$の `selected` |

資源の合計には、要素が単独で使う分と、共有物が使う分があります。

$$x_ {k,r}=\sum_ {v=0}^{N-1}\mathbf{1}[a_ {v}=k]\ell_ {v,k,r}+\sum_ {e=0}^{M-1}\mathbf{1}[c_ {e,k}>0]b_ {e,k,r}$$

| 記号 | 意味・対応する入力 |
|---|---|
| $\ell_ {v,k,r}$ | 要素$v$をグループ$k$へ置いたときの資源$r$の負荷。`Vertex.load` と `load_by_group` |
| $b_ {e,k,r}$ | 辺$e$の共有物がグループ$k$で使う資源$r$の量。`shared_load` と `shared_load_by_group` |
| $x_ {k,r}$ | グループ$k$で使う資源$r$の合計 |

共有物は、同じ辺の要素を1個置いても3個置いても、そのグループで**一度だけ**負荷に加わります。別のグループにも置けば、そちらでも一度加わります。別々の辺に設定した共有負荷は足し算です。

### 1.4　目的関数

割当全体の費用を $J(a)$ と書くと、目的関数は次の式です。全制約を満たす割当$a$の中で、$J(a)$を最小にします。

$$J(a)=C_ {0}+\sum_ {v=0}^{N-1}d_ {v}(a_ {v})+\sum_ {k=0}^{K-1}\left(o_ {k}\mathbf{1}[n_ {k}>0]+\sum_ {r=0}^{R-1}P_ {k,r}(x_ {k,r})\right)+\sum_ {e=0}^{M-1}\left(F_ {e}(s_ {e})+H_ {e}(q_ {e})+\sum_ {k=0}^{K-1}G_ {e,k}(c_ {e,k})\right)$$

長い式ですが、以下の費用をすべて足しているだけです。

| 記号 | 何の費用か | API |
|---|---|---|
| $J(a)$ | 割当$a$全体の費用。小さいほどよい | `Evaluation.cost`、`Result.cost` |
| $C_ {0}$ | 割当によらない定数 | `Problem.constant` |
| $d_ {v}(a_ {v})$ | 要素ごとの配置費用。未採用なら未採用費用 | `default_cost`、`cost_by_group`、`reject_cost` |
| $o_ {k}$ | グループを一度でも使うと発生する固定費 | `Group.open_cost` |
| $P_ {k,r}(x_ {k,r})$ | 資源の負荷が目標区間から外れた費用 | `Group.load_penalty[r]` |
| $F_ {e}(s_ {e})$ | 辺が何グループに広がったかによる費用 | `Hyperedge.span_cost` |
| $H_ {e}(q_ {e})$ | 辺の何要素を採用したかによる費用 | `Hyperedge.selected_cost` |
| $G_ {e,k}(c_ {e,k})$ | 辺から特定グループへ何要素置いたかによる費用 | `local_cost`、`local_cost_by_group` |

`cost_by_group` が指定されたグループでは、その値が `default_cost` を**置き換えます**。同様に、名前が `_by_group` で終わる設定は、対応する既定値への加算ではなく置換です。

費用を負にすると報酬を表せます。例えば費用$-6$は費用$-2$より望ましくなります。円、遅延、満足度などを同時に扱うなら、互いの重みを決めて同じ評価尺度に換算するのは利用者の役割です。

**個数から費用を作る `CountCost`。** 上の $F$、$G$、$H$ はすべて、非負整数$c$を費用へ変える同じ型を使います。

$$f(c)=\alpha\mathbf{1}[c>0]+\beta\max(c-1,0)+\gamma\mathbf{1}[c\geq2]+t_ {c}$$

| 記号 | 意味・API |
|---|---|
| $f$、$c$ | 費用関数と入力個数。設定場所に応じて `span`、`selected`、`local` を渡す |
| $\alpha$ | 1個以上なら発生する費用。`active` |
| $\beta$ | 2個目から1個増えるごとに加える費用。`extra` |
| $\gamma$ | 2個以上になった場合に一度加える費用。`cut_value` |
| $t_ {c}$ | `table[c]` の追加費用。表を省略すると全て0。$t_ {0}=0$が必要 |
| $\max(u,v)$ | $u$と$v$の大きい方。ここでは個数が0でも追加費用を負にしないために使う |

例えば `connectivity(5)` は $\alpha=0,\beta=5,\gamma=0$、表なしです。1グループに収まれば0、2グループに広がると5、3グループなら10です。

**負荷から費用を作る `LoadPenalty`。** 目標より足りない量を$z^-$、多すぎる量を$z^+$とします。

$$z^-=\max(\ell^{\mathrm{soft}}_ {k,r}-x_ {k,r},0),\qquad z^+=\max(x_ {k,r}-u^{\mathrm{soft}}_ {k,r},0)$$

$$P_ {k,r}(x_ {k,r})=A_ {k,r}z^-+B_ {k,r}(z^-)^2+C_ {k,r}z^++D_ {k,r}(z^+)^2$$

| 記号 | API・意味 |
|---|---|
| $\ell^{\mathrm{soft}}_ {k,r}$、$u^{\mathrm{soft}}_ {k,r}$ | `LoadPenalty.lower`、`upper`。望ましい負荷の下端・上端 |
| $z^-$、$z^+$ | 目標に対する不足量・超過量。いずれも0以上 |
| $A_ {k,r}$、$B_ {k,r}$ | `under_linear`、`under_quadratic`。不足の一次・二次係数 |
| $C_ {k,r}$、$D_ {k,r}$ | `over_linear`、`over_quadratic`。超過の一次・二次係数 |

二次式は、外れる量が大きいほど費用が急に増える形を作れます。通常は係数を0以上にします。コード上は有限の負の係数も指定できますが、その場合は「目標から外れることへの報酬」という別の意味になります。

### 1.5　必ず守る条件と、費用で表す希望

| 必ず守る条件 | 設定 |
|---|---|
| 置いてよい先、固定先、未採用を許すか | `allowed_groups`、`fixed`、`allow_reject` |
| 資源ごとの負荷の下限・上限 | `Group.lower`、`Group.upper` |
| 辺が使うグループ数の範囲 | `min_span`、`max_span` |
| 辺の採用要素数の範囲 | `min_selected`、`max_selected` |
| 辺から各グループへ置く要素数の範囲 | `local_lower`、`local_upper` と各 `_by_group` |

資源の下限・上限を $L_ {k,r}$、$U_ {k,r}$ と書けば、条件は $L_ {k,r}\leq x_ {k,r}\leq U_ {k,r}$ です。これは `LoadPenalty` の目標区間とは別です。`lower_only_if_used=true` のグループは、空ならこの**必須の下限だけ**を免除します。

同様に、辺の `span`、`selected`、`local` は、それぞれ指定した下限以上・上限以下でなければいけません。特に `local_lower` は、その辺の要素をまだ置いていないグループにも適用されます。

戻り値の `feasible` が `true` なら全条件を満たしています。`false` は「返された割当が条件を満たしていない」という意味です。問題そのものに実行可能解がないと証明したわけではありません。本ライブラリは、実行可能解の発見も最適性も保証しません。

## 第2部　類似ソルバーと使い分け

### 2.1　条件を絞ると厳密に解ける場合

厳密解法は、実行可能解が存在し、計算を必要なところまで完了できれば、最適解を求められます。対象条件に合うなら、まず専用の厳密解法を検討する価値があります。「一般の問題を同じ時間で必ずよりよく解ける」という意味ではありません。

以下は、本ライブラリのモデルから費用や制約を取り除いた場合の数学的な整理です。ここではアルゴリズムの手順は扱いません。

| 限定した問題 | 検討できる厳密解法 | 境界となる条件 |
|---|---|---|
| 要素ごとの配置費用だけ。要素同士を結ぶ容量・辺・固定費などがない | 各要素で最安の許可先を選ぶ | 他の要素の選択が自分の条件や費用に影響しないこと |
| 各要素の負荷が一律1。グループの無条件の人数上下限と配置費用だけ | 最小費用流。1対1なら線形割当ソルバーも候補 | 使用時だけの人数下限、異なる重量、共有負荷、グループ固定費、辺の費用・制約を加えると、そのままではこの形でなくなる |
| 必須要素を2グループに分ける。空グループも可。配置費用と、2要素の組が分かれたときの非負費用だけ | s-t最小カット | 人数・容量の均等化条件や他の集団制約を足さない。負の分離費用もこの条件から外れる |
| グループが1個、採否選択、1種類の整数容量、要素ごとの費用だけ | 0–1ナップサックの動的計画法など | 共有物や辺による相互作用がないこと。容量の数値が大きいと計算量が問題になる |
| 要素がごく少ない | 全割当の列挙 | 全要素が未採用も選べる場合、候補数は最大$(K+1)^N$。急増する |

最小費用流による割当の例は[OR-Tools公式解説](https://developers.google.com/optimization/flow/assignment_min_cost_flow)にあります。2値の費用をグラフカットで扱える条件の一次資料は[Kolmogorov・Zabihの論文](https://www.microsoft.com/en-us/research/publication/what-energy-functions-can-be-minimized-via-graph-cuts/)です。上表の条件は、それらの一般論に本ライブラリの設定を対応させたものです。

### 2.2　同じ問題領域のソルバー

| 選択肢 | 検討しやすい場面 | 本ライブラリとの比較で見る点 |
|---|---|---|
| [KaHyPar](https://kahypar.org/) | 容量・バランスを伴う通常のハイパーグラフ分割。cut-netやconnectivityの最小化 | 分割そのものに特化した候補。本書の `cut`、`connectivity` と目的を対応させられる。採否や任意の個数表、共有容量まで自分のモデルが対応できるかを確認する |
| [Mt-KaHyPar](https://github.com/kahypar/mt-kahypar) | 並列処理も利用して大きなグラフ・ハイパーグラフを分割したい | 同じ入力・同じ制約・同じ時間で比較する。名前が似ていても全機能が同じとは限らない |
| [OR-Tools CP-SAT](https://developers.google.com/optimization/cp/cp_solver)、MIP系ソルバー | 条件が複雑、制約を追加したい、小中規模で最適性や不可能性の証明が欲しい | 割当や共有物の使用を変数にして別途モデル化する。個数表は場合分けで表せるがモデルが大きくなり得る。整数化・費用の精度にも注意する |
| HypergraphPartitioner | 共有負荷、採否、個数制約を1個のC++ヘッダで組み合わせ、既存解や一部分を時間予算内で改善したい | 問題更新・部分改善を直接扱える。解の品質と実行可能性は、対象データで確かめる必要がある |

KaHyParの2つの代表的な目的関数と、Mt-KaHyParの並列分割という位置づけは、それぞれの公式サイト・公式リポジトリに基づきます。どちらが本ライブラリより速い・高品質かを、本書では測定していません。

CP-SATなどにも時間制限があります。終了状態が「実行可能解を得た」だけなら、最適性が証明されたとは限りません。「解をまだ得ていない」と「解が存在しないと証明した」も区別します。

経路の訪問順、移動距離、締切付きスケジュール、各グループのグラフ上の連結性は、本ライブラリの組込み条件ではありません。それらが本質なら、それを直接扱うルーティング・スケジューリング・連結分割のソルバーを検討します。`connectivity` という費用名も、各グループが連結であることを保証しません。

## 第3部　コードを動かしながら学ぶ

### 準備

ZIPには本書、対象ヘッダ、`examples/example01.cpp`〜`example27.cpp`、確認用の `run_examples.py` を同梱しています。ヘッダを別の版に差し替えると、APIや挙動が本書と異なる場合があります。

GCCとlibstdc++、C++20以上を使います。ヘッダは `<bits/stdc++.h>` とGCC系の `__INCLUDE_LEVEL__` を使うため、すべてのC++処理系でそのまま動くという意味ではありません。外部の数理最適化ライブラリは不要です。

各例をコピーして `example.cpp` として保存し、同じディレクトリへ `hypergraph_partitioner_v07.hpp` を置きます。そのディレクトリで次を実行します。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -pedantic example.cpp -o example
./example
```

ZIPを展開して同梱例を直接使うなら、展開先で次を実行します。

```bash
g++ -std=c++20 -O2 -I . examples/example01.cpp -o example01
./example01
```

`-std=c++20` は言語の版、`-O2` は最適化、`-I .` はヘッダを現在のディレクトリから探す指定です。`-o` の後ろが実行ファイル名です。`-Wall -Wextra -pedantic` はコンパイラの注意を表示します。入力の誤りを見つけやすくするため、このガイドでは `assert` を無効化する `-DNDEBUG` を付けていません。

以下のコードにある `using S = HypergraphPartitioner;` は長いクラス名に短い別名を付けています。`auto&` は対象そのものへの参照で、例えば `for (auto& g : p.groups)` の代入は問題のグループ設定を書き換えます。`const auto&` はコピーせず読み取る参照です。

各例の出力の行順は固定ですが、時間制限を使う探索の割当や試行回数は実行環境で変わり得ます。説明中の最適値は小さな例を理解するための値であり、探索APIの最適性保証ではありません。

### 例01　最小コード：4個の仕事を2グループへ分ける

第1部の問題をそのまま作ります。各要素の負荷は既定で1なので、容量を2にすれば「2個まで」を表せます。新しい概念は、問題の作成、辺の追加、探索、結果の4つだけです。

ファイル：`examples/example01.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2);
    for (auto& g : p.groups) g.upper[0] = 2;
    p.add_hyperedge({0, 1}, S::CountCost::connectivity(5));

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

**1. 問題の入れ物を作る。** `S::Problem p(4, 2)` の4は要素数、2はグループ数です。要素番号は0〜3、グループ番号は0〜1になります。省略した第3引数は資源数で、既定値は1です。各要素は第0資源を1使い、各グループの下限は0、上限は `S::unlimited` から始まります。

**2. 容量を決める。** `g.upper[0] = 2` は「このグループの第0資源の上限を2にする」という意味です。`for` で全グループへ同じ値を設定しています。この段階では資源が1種類なので添字は0だけです。

**3. 関係する集合を作る。** `add_hyperedge` の第1引数 `{0, 1}` は、関係する要素の番号です。第2引数 `connectivity(5)` はこの辺の `span_cost`、つまり使用グループ数に対する費用です。5は2番目以降の使用グループ1個につき加える費用です。要素0と1が一緒なら0、分かれると5になります。

`add_hyperedge` は追加した辺の番号を返します。ここでは後から辺の設定を変えないので、戻り値を保存しません。辺の追加順が番号になります。

**4. 探索する。** `S solver(p, 7)` は問題のコピーを所有するソルバーを作ります。7は乱数の初期値であるシードです。良さの重みや試行回数ではありません。省略したシードは0です。

`solve({20'000})` の引数は `Options` です。最初の欄 `time_limit_us` に20,000マイクロ秒、すなわち20ミリ秒を渡しています。C++の数値中の `'` は桁区切りです。第2の欄 `annealing` は省略して既定の `true` を使います。時間は厳密な打切り時刻ではなく、初期化や最後の評価などで超えることがあります。

**5. 結果を読む。** `r.feasible` は全制約を満たしたか、`r.cost` は目的関数の値です。`r.assignment[v]` が要素番号$v$の配置先です。`std::boolalpha` により真偽値を `1/0` ではなく `true/false` と表示します。

例えば `assignment: 0 0 1 1` は最適解の一つで、`feasible=true cost=0` になります。グループ名を交換した `1 1 0 0` も同じ費用です。`feasible=false` の割当は、低い費用でも実際の配置に採用しません。原因の読み方は例18で学びます。

この例の目的関数は、辺1本の $5\max(s_ {0}-1,0)$ だけです。それ以外の費用は既定で0です。

### 例02　発展①：配置先によって費用を変える

**例01からの変更点：** 仕事0と1をグループ0へ置くと無料、他のグループなら各3とします。さらに、どの割当にも共通の費用2を加えます。

ファイル：`examples/example02.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2);
    for (auto& g : p.groups) g.upper[0] = 2;
    p.add_hyperedge({0, 1}, S::CountCost::connectivity(5));

    p.vertices[0].default_cost = 3;
    p.vertices[0].cost_by_group = {{0, 0}};
    p.vertices[1].default_cost = 3;
    p.vertices[1].cost_by_group = {{0, 0}};
    p.constant = 2;

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

| 新しい設定 | 各値の意味 |
|---|---|
| `p.vertices[0]`、`p.vertices[1]` | 設定対象の要素番号0、1 |
| `default_cost = 3` | 通常グループへ置く場合の既定費用3 |
| `cost_by_group = {{0, 0}}` | 内側の組の第1値はグループ番号0、第2値はその配置費用0 |
| `constant = 2` | 全割当に加える定数2 |

`{{0, 0}}` は「グループ0だけ例外を設定した一覧」です。ここにある0は、既定費用3へ足す値ではありません。グループ0へ置いた費用は3ではなく0になります。複数の例外は複数の組で指定できますが、同じグループ番号を重複させてはいけません。

要素0、1をグループ0へ、2、3をグループ1へ置けば、配置費用0、辺の費用0、定数2で合計2です。2つのグループを交換すると、配置費用が合計6増えます。

`constant` は、割当同士の優劣を変えません。出力する総額の調整に使います。費用の各設定は `double` で、有限の負の値も使えます。例えば「ここへ置くと2の報酬」は配置費用$-2$です。

### 例03　発展②：仕事ごとの大きさを変える

**例01からの変更点：** 同じ「1個の仕事」でも負荷が違うものとします。仕事0と1は2、仕事2と3は1を使い、グループの容量を4にします。

ファイル：`examples/example03.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2);
    p.vertices[0].load = {2};
    p.vertices[1].load = {2};
    p.vertices[2].load = {1};
    p.vertices[3].load = {1};
    for (auto& g : p.groups) g.upper[0] = 4;
    p.add_hyperedge({0, 1}, S::CountCost::connectivity(5));

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

`load = {2}` は、第0資源の負荷が2という長さ1の配列です。`{1}` も同様です。負荷は0以上の整数で、整数型は `int64_t` です。小数の量を表したい場合は、例えば0.1単位を1として全入力を同じ倍率で整数化します。

上限4は、要素の個数ではなく負荷の合計への上限です。グループへ仕事0と1を置けば、2個ですが負荷は $2+2=4$。仕事2と3をもう一方へ置けば負荷は2です。この配置は実行可能で、辺の費用は0です。

`load` を設定しなかった要素だけは既定の1を使います。ここでは、どの値がどの要素に対応するか見やすいよう4要素とも明示しました。

### 例04　発展③：CPUとメモリを同時に制限する

**例03からの変更点：** 資源を2種類にします。第0資源をCPU、第1資源をメモリとし、それぞれ別の上限を持たせます。

ファイル：`examples/example04.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2, 2);
    p.vertices[0].load = {2, 3};
    p.vertices[1].load = {2, 3};
    p.vertices[2].load = {1, 1};
    p.vertices[3].load = {1, 1};
    for (auto& g : p.groups) g.upper = {4, 4};
    p.add_hyperedge({0, 1}, S::CountCost::connectivity(5));

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

| 新しい値 | 意味 |
|---|---|
| `Problem(4, 2, 2)` の第3引数2 | 資源を2種類にする |
| `{2, 3}` | CPUを2、メモリを3使う |
| `{1, 1}` | CPUを1、メモリを1使う |
| `g.upper = {4, 4}` | CPU合計4以下、メモリ合計4以下 |

CPUとメモリを足して8以下、という意味ではありません。各資源について別々に条件を判定します。仕事0と1を同じグループに置くとCPUは4でもメモリが6になり、実行不可能です。大きい仕事と小さい仕事を1個ずつ組み合わせれば、各グループの負荷は `{3, 4}` になります。その場合、仕事0と1は分かれるので費用は5です。

`Problem` が作った既定の要素負荷は `{1, 0}` です。資源数を増やしただけで全資源の負荷が1になるわけではありません。`load`、グループの上下限、共有負荷を明示する場合は、資源数と同じ長さの配列を使います。

### 例05　発展④：配置先によって資源消費を変える

**例04からの変更点：** グループ1には仕事0に適した設備があり、その仕事だけCPU・メモリを少なく消費するとします。

ファイル：`examples/example05.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2, 2);
    p.vertices[0].load = {2, 3};
    p.vertices[1].load = {2, 3};
    p.vertices[2].load = {1, 1};
    p.vertices[3].load = {1, 1};
    for (auto& g : p.groups) g.upper = {4, 4};
    p.add_hyperedge({0, 1}, S::CountCost::connectivity(5));

    p.vertices[0].load_by_group = {{1, {1, 1}}};

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

`load_by_group = {{1, {1, 1}}}` の最初の1はグループ番号です。後ろの `{1, 1}` は、そのグループへ置いた場合のCPU・メモリ負荷です。したがって仕事0の負荷は、グループ0では元の `{2, 3}`、グループ1では `{1, 1}` です。

これは負荷の差分ではなく、負荷配列全体の置換です。指定のないグループには `load` が使われます。

今度は仕事0と1をグループ1へまとめると `{1, 1}+{2, 3}={3, 4}` で収まります。仕事2と3をグループ0へ置けば、費用0の実行可能解を作れます。例04との違いを、資源の合計で確かめてください。

### 例06　発展⑤：置ける先を制限し、一部を固定する

**例01からの変更点：** 仕事0はグループ0だけを許し、仕事1はグループ0に固定します。

ファイル：`examples/example06.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2);
    for (auto& g : p.groups) g.upper[0] = 2;
    p.add_hyperedge({0, 1}, S::CountCost::connectivity(5));

    p.vertices[0].allowed_groups = std::vector<int>{0};
    p.vertices[1].fixed = 0;

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

| 新しい設定 | 意味 |
|---|---|
| `allowed_groups = std::vector<int>{0}` | 許可する通常グループの一覧。この例では番号0だけ |
| `fixed = 0` | この要素の配置先を番号0に固定 |

許可先が1個だけの仕事0も結果的に動けません。`allowed_groups` は置いてよい先の条件、`fixed` はそのうち1つを指定する条件なので、役割が異なります。`fixed` が許可先の制限を上書きすることはありません。例えば、グループ0しか許していないのにグループ1へ固定する入力は無効です。

`allowed_groups` は `optional<vector<int>>` です。「一覧を指定しない」と「空の一覧を指定する」は異なります。未指定は全通常グループを許可します。空の一覧は通常グループを1つも許さず、後で学ぶ未採用を許可した要素でだけ使えます。`fixed` も未指定か整数1個を持つ `optional` で、値0は有効な固定先です。

許可先の一覧は順不同でもよく、重複番号は内部で除かれます。固定された要素も、負荷・辺・費用の計算に含まれます。

### 例07　発展⑥：使うグループ数に固定費をかける

**例01からの変更点：** グループを3個に増やし、それぞれ4個まで仕事を置けるようにします。グループを1個使うたびに費用8がかかります。

ファイル：`examples/example07.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 3);
    for (auto& g : p.groups) {
        g.upper[0] = 4;
        g.open_cost = 8;
    }
    p.add_hyperedge({0, 1}, S::CountCost::connectivity(5));

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

`Problem(4, 3)` の3は候補となるグループ数です。3個すべてを使うという指定ではありません。`upper[0] = 4` により、全仕事を1グループへまとめることも可能になります。

`open_cost = 8` は、要素を1個以上置いたグループに一度だけ加える費用です。2個置いても4個置いても固定費は8です。空グループの固定費は0です。

この例では1グループにまとめれば、辺の費用0、固定費8、合計8になります。負荷0の要素しか置いていなくても、要素があればグループは「使用中」で固定費が発生します。使用中かどうかは負荷の大小ではなく要素数で判定します。

### 例08　発展⑦：使うなら最低人数を満たす

**例07からの変更点：** グループを使うなら最低3個の仕事をまとめたい、とします。使わないグループは空のままでよいものとします。

ファイル：`examples/example08.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 3);
    for (auto& g : p.groups) {
        g.upper[0] = 4;
        g.open_cost = 8;
    }
    p.add_hyperedge({0, 1}, S::CountCost::connectivity(5));

    for (auto& g : p.groups) {
        g.lower[0] = 3;
        g.lower_only_if_used = true;
    }

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

`lower[0] = 3` は必須の負荷下限です。この例では負荷が一律1なので、最低3個という意味になります。`lower_only_if_used = true` によって、空グループの下限だけを免除します。上限4は引き続き適用されます。

4個を1グループにまとめれば、そのグループは負荷4、他のグループは空なので実行可能です。もしフラグが既定の `false` のままなら、3グループ全部に最低3が必要で、必要合計9に対して仕事は4しかありません。この条件では実行可能解が存在しません。

このフラグが免除するのは `Group.lower` だけです。次の例で扱う負荷の目標費用や、辺の `local_lower` を免除するものではありません。

### 例09　発展⑧：容量とは別に「望ましい負荷」を決める

**例03からの変更点：** 必須の容量上限を6まで緩めつつ、負荷3を望ましい値にします。目標から外れた量に費用をかけます。

ファイル：`examples/example09.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2);
    p.vertices[0].load = {2};
    p.vertices[1].load = {2};
    for (auto& g : p.groups) {
        g.upper[0] = 6;
        auto& f = g.load_penalty[0];
        f.lower = 3;
        f.upper = 3;
        f.under_linear = 1;
        f.under_quadratic = 2;
        f.over_linear = 3;
        f.over_quadratic = 4;
    }
    p.add_hyperedge({0, 1}, S::CountCost::connectivity(5));
    std::cout << "penalty(1)=" << p.groups[0].load_penalty[0](1)
              << " penalty(5)=" << p.groups[0].load_penalty[0](5) << '\n';

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

| 設定 | 意味 |
|---|---|
| `g.upper[0] = 6` | 絶対に超えてはいけない容量 |
| `g.load_penalty[0]` | 第0資源の目標費用 `LoadPenalty` |
| `f.lower = 3`、`f.upper = 3` | 費用が0となる目標区間。ここではちょうど3 |
| `under_linear = 1` | 不足1単位につき1 |
| `under_quadratic = 2` | 不足量の二乗に2をかける |
| `over_linear = 3` | 超過1単位につき3 |
| `over_quadratic = 4` | 超過量の二乗に4をかける |

負荷1なら不足は2なので、費用は $1\times2+2\times2^2=10$。負荷5なら超過は2で、$3\times2+4\times2^2=22$ です。コードの `f(負荷)` に相当する呼出しで、探索せず費用を確かめられます。

例03で0と1を同じグループへ置いた割当は、負荷4と2になります。目標費用が7と3で合計10です。0と1を分けて負荷3ずつにすれば、目標費用0、辺の費用5、合計5です。ここでは分ける方が望ましくなります。

目標を外れること自体は制約違反ではありません。例えば負荷4は必須上限6以内なので実行可能です。また、空グループも目標費用の計算対象です。`lower_only_if_used=true` でも、この費用は自動的に消えません。

### 例10　発展⑨：共有データのメモリを一度だけ数える

**例04を基にした変更点：** 第0資源を仕事数、第1資源を共有データのメモリにします。仕事0・1がデータA、仕事2・3がデータBを使い、各データのメモリ量は4です。

ファイル：`examples/example10.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2, 2);
    for (auto& g : p.groups) g.upper = {2, 5};
    for (auto ids : {std::vector<int>{0, 1}, std::vector<int>{2, 3}}) {
        int e = p.add_hyperedge(ids);
        p.hyperedges[e].shared_load = {0, 4};
    }

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

各要素の `load` を変更していないので、既定の `{1, 0}` を使います。`g.upper = {2, 5}` は仕事数2以下、メモリ5以下です。外側のループは、要素集合 `{0, 1}` と `{2, 3}` について同じ種類の辺を1本ずつ作っています。

この `add_hyperedge(ids)` は第2引数を省略しています。省略時の辺の費用は0です。戻り値の `e` は辺番号で、`p.hyperedges[e]` に詳しい設定を追加します。

`shared_load = {0, 4}` は、その辺を利用する各グループで仕事数資源を0、メモリ資源を4使うという指定です。Aを使う仕事を2個とも同じグループへ置いても、Aのメモリは4です。仕事1個ごとの `load` に4を入れると8になってしまい、別のモデルになります。

| グループに置く仕事 | 仕事数 | 共有メモリ | 上限を守るか |
|---|---:|---:|---|
| 0、1 | 2 | Aの4 | 守る |
| 2、3 | 2 | Bの4 | 守る |
| 0、2 | 2 | Aの4＋Bの4＝8 | 守らない |

したがって実行可能な割当では、各ペアをまとめる必要があります。共有負荷そのものは費用ではなく容量消費です。この例の目的関数は全て0なので、実行可能解の中に費用上の優劣はありません。

### 例11　発展⑩：共有物の費用と、利用先ごとの違い

**例10からの変更点：** 共有物を使うたびに費用を設定し、グループ1ではメモリ量と料金を変更します。

ファイル：`examples/example11.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2, 2);
    for (auto& g : p.groups) g.upper = {2, 5};
    for (auto ids : {std::vector<int>{0, 1}, std::vector<int>{2, 3}}) {
        int e = p.add_hyperedge(ids);
        auto& h = p.hyperedges[e];
        h.shared_load = {0, 4};
        h.shared_load_by_group = {{1, {0, 3}}};
        h.local_cost = S::CountCost::activation(2);
        h.local_cost_by_group = {{1, S::CountCost::activation(5)}};
    }

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

| 新しい設定 | 意味 |
|---|---|
| `shared_load_by_group = {{1, {0, 3}}}` | グループ1で使う共有負荷だけ `{0, 3}` に置き換える |
| `local_cost = activation(2)` | この辺の要素を、そのグループへ1個以上置くと費用2 |
| `local_cost_by_group = {{1, activation(5)}}` | グループ1では費用2を費用5へ置き換える |

`activation(w)` は個数0で0、1以上で一律$w$を返す `CountCost` です。`local_cost` に設定したので、入力する個数は「この辺から、このグループへ置いた要素の数」です。共有メモリとは別に共有料金を表せます。

1本の辺が2グループを使えば、グループ0で2、グループ1で5を払い、その辺だけで7です。ここでは容量によって各ペアが別々のグループにまとまるため、辺2本で2と5を一度ずつ払い、合計7になります。

`span_cost = activation(2)` に設定した場合は、辺全体で一度でも採用されたら2となります。設定先が `local_cost` か `span_cost` かで、「一度」の単位が異なる点を確認してください。

### 例12　発展⑪：同居の希望とは別に、必須の分散数を指定する

**例01を基にした変更点：** 6要素・3グループにし、辺 `{0, 1, 2}` がちょうど2グループを使うようにします。例えば、同じ種類の仕事を一か所へ集中させない条件です。

ファイル：`examples/example12.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(6, 3);
    for (auto& g : p.groups) g.upper[0] = 3;
    int e = p.add_hyperedge({0, 1, 2}, S::CountCost::connectivity(5));
    p.hyperedges[e].min_span = 2;
    p.hyperedges[e].max_span = 2;

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

`Problem(6, 3)` の6は仕事数、3はグループ数です。各容量3なので、3個の仕事を一緒に置く余地はあります。そのうえで、辺の `min_span = 2` と `max_span = 2` により、辺の要素をちょうど2グループへ分散させます。

同じ値を上下限に指定すると、個数をちょうどその値にできます。例では2個＋1個に分ける必要があり、`connectivity(5)` の費用は5です。1グループにまとめると費用は0でも下限違反になります。

これは既存の3要素の配置条件であり、データや仕事を自動で複製する指示ではありません。辺に含めなかった仕事3〜5は、この `span` の計算には入りません。

### 例13　発展⑫：グループ別に、その集合から置く個数を制限する

**例12からの変更点：** 要素0〜3を対象の集合にします。グループ0と1には、この集合から1〜2個ずつ置き、グループ2には置かないようにします。前の `span` 条件は外し、グループ別の個数条件で表します。

ファイル：`examples/example13.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(6, 3);
    for (auto& g : p.groups) g.upper[0] = 3;
    int e = p.add_hyperedge({0, 1, 2, 3});
    auto& h = p.hyperedges[e];
    h.local_lower = 1;
    h.local_upper = 2;
    h.local_lower_by_group = {{2, 0}};
    h.local_upper_by_group = {{2, 0}};

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

| 新しい設定 | 意味 |
|---|---|
| `local_lower = 1` | この辺から、原則として全グループへ最低1個ずつ置く |
| `local_upper = 2` | この辺から、各グループへ最大2個まで置く |
| `local_lower_by_group = {{2, 0}}` | グループ2だけ下限を0へ置き換える |
| `local_upper_by_group = {{2, 0}}` | グループ2だけ上限を0へ置き換える |

要素0〜3は必須なので、グループ0と1へ2個ずつ置くことになります。要素4と5はこの辺に入っていないため、これらの個数制約では数えません。グループ全体の容量3は別に適用されます。

`local_lower = 1` は「その辺を置いたグループだけ最低1個」ではありません。空の配置先にも下限を課します。例ではグループ2の下限を0へ変更することで、「グループ2へは置かなくてよい」を表しています。使用時だけ個数を3以上にする、といった条件を `local_lower` と `Group.lower_only_if_used` の組合せで自動的に表すことはできません。

### 例14　発展⑬：すべてを処理できないときに採否を選ぶ

**例01からの変更点：** 容量を各1へ減らし、仕事を未採用にしてもよいことにします。未採用1個につき費用8を払い、仕事3は事前に未採用へ固定します。

ファイル：`examples/example14.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2);
    for (auto& g : p.groups) g.upper[0] = 1;
    for (auto& v : p.vertices) {
        v.allow_reject = true;
        v.reject_cost = 8;
    }
    p.vertices[3].fixed = -1;
    p.add_hyperedge({0, 1}, S::CountCost::connectivity(5));

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

| 新しい設定 | 意味 |
|---|---|
| `allow_reject = true` | この要素に配置先 `-1` を許す |
| `reject_cost = 8` | 未採用の場合の費用8。通常の配置費用に加算するのではなく、代わりに使う |
| `fixed = -1` | 仕事3を未採用へ固定。`allow_reject=true` が必要 |

通常グループに置いた要素は負荷・辺の個数に数えます。未採用の要素は、どの通常グループの負荷にも、`span` にも、`local` にも、`selected` にも入りません。その要素の未採用費用は残ります。

容量合計が2なので、少なくとも2個を未採用にします。仕事2と、仕事0か1のどちらかを採用すれば、辺は1グループだけを使い、辺の費用0、未採用費用16です。仕事0と1を両方採用すると別グループに分かれるため費用は21になります。

`allow_reject=true` だけを設定して `reject_cost` を省略すると、未採用費用は0です。他に採用数の下限や報酬がなければ、全部未採用が望ましいモデルになる場合があります。

### 例15　発展⑭：複数の候補方式から、仕事ごとに1個選ぶ

**例14を基にした変更点：** 「要素＝仕事」から「要素＝仕事の候補方式」へ読み替えます。3仕事それぞれに2方式を用意し、各仕事からちょうど1方式を採用します。

ファイル：`examples/example15.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(6, 2);
    for (auto& g : p.groups) g.upper[0] = 2;
    const std::vector<double> cost = {3, 1, 2, 4, 9, 1};
    for (int v = 0; v < 6; ++v) {
        p.vertices[v].allow_reject = true;
        p.vertices[v].default_cost = cost[v];
    }
    p.vertices[4].allowed_groups = std::vector<int>{};
    for (int job = 0; job < 3; ++job) {
        int e = p.add_hyperedge({2 * job, 2 * job + 1});
        p.hyperedges[e].min_selected = 1;
        p.hyperedges[e].max_selected = 1;
    }

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

| 仕事番号 `job` | 候補要素 | 採用した場合の費用 |
|---|---|---|
| 0 | 0、1 | 3、1 |
| 1 | 2、3 | 2、4 |
| 2 | 4、5 | 9、1 |

`Problem(6, 2)` の6は仕事数3ではなく候補数6です。`cost` は各候補の採用費用を番号順に並べた配列です。`default_cost = cost[v]` により採用時だけその費用を払い、`reject_cost` は既定の0のままなので、選ばなかった方式の費用は0です。

`{2 * job, 2 * job + 1}` は各仕事の2方式です。その辺へ `min_selected = 1`、`max_selected = 1` を設定すると、全通常グループを合わせてちょうど1方式だけが採用されます。各グループで1方式ずつ、という意味ではありません。

候補4は利用不能という設定にしました。`allowed_groups = std::vector<int>{}` は通常グループを一切許しませんが、未採用は許してあるため入力として有効です。したがって3番目の仕事では候補5を採用します。

安い方式を選ぶと候補1、2、5が採用され、費用は $1+2+1=4$。各グループの容量2に3候補を分ければ実行可能です。方式ごとに `load` を変えれば、安いが重い方式と高いが軽い方式の比較にも拡張できます。

### 例16　発展⑮：採用数に応じた報酬を費用表で与える

**例14を基にした変更点：** 採用数を1〜3個にし、「2個採用したとき特にうれしい」という報酬を表にします。未採用費用は0に戻し、採用1個につき処理費用1も払います。

ファイル：`examples/example16.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2);
    for (auto& g : p.groups) g.upper[0] = 2;
    for (auto& v : p.vertices) v.allow_reject = true;
    int e = p.add_hyperedge({0, 1, 2, 3}, S::CountCost::cut(2));
    auto& h = p.hyperedges[e];
    h.min_selected = 1;
    h.max_selected = 3;
    h.selected_cost = S::CountCost::lookup({0, -6, -9, -7, -8});
    h.local_cost = S::CountCost::linear(1);

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

| 新しい設定 | 意味 |
|---|---|
| `min_selected = 1`、`max_selected = 3` | 辺の採用数を1以上3以下にする |
| `lookup({0, -6, -9, -7, -8})` | 個数0、1、2、3、4の順に費用を与える |
| `selected_cost` | 表の添字を辺全体の採用数にする |
| `local_cost = linear(1)` | 各グループで、この辺の要素1個につき費用1 |
| `span_cost = cut(2)` | 2グループ以上へ広がった場合に一度だけ費用2 |

`lookup` は個数に対応する表をそのまま費用に使います。負の費用は報酬です。2個を同じグループに置けば、報酬$-9$、処理費用2、分離費用0で、合計$-7$になります。

`linear(w)` は個数$c$に対して $wc$。`cut(w)` は0個・1個では0、2個以上で一律$w$です。`span_cost` の `cut` は「2グループ以上に分かれたか」、`local_cost` の `linear` は「そのグループへ何個置いたか」を見ています。

**表の長さは、必須制約で許した個数より短くしてはいけません。** 探索では一時的に制約外の個数も評価するため、この4要素の辺では `selected_cost` と `local_cost` の表に0〜4の5項目が必要です。`max_selected=3` でも4の項目を用意します。`span_cost` の表なら最大は「グループ数と辺の要素数の小さい方」なので、この例では0〜2の3項目です。

表の0番目は必ず0です。個数0でも常に加えたい費用は、適切な定数項などで別に表します。

### 例17　発展⑯：基本料金・追加料金・表を組み合わせる

**例12を基にした変更点：** 4要素・3グループ、各容量2とし、辺 `{0, 1, 2}` の使用グループ数に複合料金を設定します。個数の上下限制約は付けません。

ファイル：`examples/example17.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 3);
    for (auto& g : p.groups) g.upper[0] = 2;
    auto c = S::CountCost::lookup({0, 0, 1, 4});
    c.active = 2;
    c.extra = 3;
    c.cut_value = 5;
    for (int count = 0; count <= 3; ++count)
        std::cout << "count=" << count << " cost=" << c(count) << '\n';
    p.add_hyperedge({0, 1, 2}, c);

    S solver(p, 7);
    const auto& r = solver.solve({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

| 設定 | 意味 |
|---|---|
| `lookup({0, 0, 1, 4})` | 使用グループ数0〜3に対する追加の表料金 |
| `active = 2` | 1グループ以上使ったら基本料金2 |
| `extra = 3` | 2番目以降の使用グループごとに3 |
| `cut_value = 5` | 2グループ以上になったら一度だけ5 |
| `c(count)` | この設定から、指定した個数の費用を直接計算 |

この3係数と `table` は足し合わせます。例えば2グループなら $2+3+5+1=11$、3グループなら $2+6+5+4=17$ です。出力する費用は個数0〜3に対して0、2、11、17になります。

辺の3要素は各容量2のため最低2グループを使い、最適費用は11です。`CountCost` の呼出し自体は探索ではありません。表を直接参照するので、負の個数や表の範囲外を渡してはいけません。

ここまで出てきた作り方をまとめると、次のとおりです。$w$は渡した費用、$c$は非負の個数です。

| 作り方 | $c=0$ | $c=1$ | $c=2$ | $c=3$ |
|---|---:|---:|---:|---:|
| `activation(w)` | 0 | $w$ | $w$ | $w$ |
| `connectivity(w)` | 0 | 0 | $w$ | $2w$ |
| `cut(w)` | 0 | 0 | $w$ | $w$ |
| `linear(w)` | 0 | $w$ | $2w$ | $3w$ |
| `lookup(table)` | 0 | `table[1]` | `table[2]` | `table[3]` |

関数名にかかわらず、同じ `CountCost` を `span_cost`、`selected_cost`、`local_cost` のいずれにも使えます。`connectivity` を `local_cost` に置けば、「同じ辺の2個目以降をそのグループへ置く費用」になります。

### 例18　発展⑰：探索前に割当を採点し、違反を読む

**例01からの変更点：** 仕事0をグループ0に固定し、辺を `{0, 2}` に変えます。ここでは探索せず、こちらが用意した2つの割当を `evaluate` で確認します。

ファイル：`examples/example18.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;


const char* name(S::ViolationKind kind) {
    using K = S::ViolationKind;
    switch (kind) {
        case K::Domain: return "Domain";
        case K::LoadLower: return "LoadLower";
        case K::LoadUpper: return "LoadUpper";
        case K::SpanLower: return "SpanLower";
        case K::SpanUpper: return "SpanUpper";
        case K::SelectedLower: return "SelectedLower";
        case K::SelectedUpper: return "SelectedUpper";
        case K::LocalLower: return "LocalLower";
        case K::LocalUpper: return "LocalUpper";
    }
    return "unknown";
}

int main() {
    S::Problem p(4, 2);
    for (auto& g : p.groups) g.upper[0] = 2;
    p.vertices[0].fixed = 0;
    p.add_hyperedge({0, 2}, S::CountCost::connectivity(5));
    S solver(p, 7);

    std::vector<int> bad = {1, 1, 1, 1};
    auto a = solver.evaluate(bad);
    std::cout << std::boolalpha << "bad feasible=" << a.feasible
              << " cost=" << a.cost << '\n';
    for (const auto& v : a.violation)
        std::cout << name(v.kind) << " id=" << v.id << " group=" << v.group
                  << " resource=" << v.resource << " amount=" << v.amount << '\n';
    std::vector<int> good = {0, 0, 1, 1};
    auto b = solver.evaluate(good);
    std::cout << "good feasible=" << b.feasible << " cost=" << b.cost << '\n';
}
```

`evaluate(assignment)` は、現在の問題設定で費用と制約違反を再計算します。引数は要素番号順の配置先一覧です。`vector<int>` を、その内容をコピーしない配列ビューである `span<const int>` として受け取ります。初回の `solve` より前でも使え、探索状態や乱数を変更しません。

`bad = {1, 1, 1, 1}` は全要素をグループ1へ置くので辺の費用は0ですが、次の2つの違反があります。

| 違反 | `id` | `group` | `resource` | `amount` |
|---|---:|---:|---:|---:|
| `Domain`：要素0の固定先と異なる | 0 | 1 | -1 | 1 |
| `LoadUpper`：グループ1の負荷4が容量2を超える | 1 | 1 | 0 | 2 |

`good = {0, 0, 1, 1}` は全条件を満たしますが、辺の要素0と2が分かれるため費用5です。費用0の違反解より、費用5の実行可能解を採用すべきです。

`Evaluation` は `cost`、`feasible`、`violation` を持ちます。`violation` は単数形のフィールド名ですが、内容は違反の一覧です。空なら実行可能です。表示用の `name` 関数は列挙値を文字列へ変えているだけで、採点を変更しません。

| `ViolationKind` | `id` の意味 | `group` | `resource` | `amount` |
|---|---|---|---|---|
| `Domain` | 要素番号 | 与えた配置先。未採用なら-1 | -1 | 1 |
| `LoadLower` / `LoadUpper` | グループ番号 | 同じグループ番号 | 資源番号 | 不足・超過の元の負荷単位 |
| `SpanLower` / `SpanUpper` | 辺番号 | -1 | -1 | 使用グループ数の不足・超過 |
| `SelectedLower` / `SelectedUpper` | 辺番号 | -1 | -1 | 採用数の不足・超過 |
| `LocalLower` / `LocalUpper` | 辺番号 | 対象グループ番号 | -1 | そのグループの個数不足・超過 |

長さが要素数と異なる一覧や、範囲外の配置先番号を渡すことはできません。配置先番号は `-1` 以上 `K-1` 以下に限ります。その範囲内で「未採用を許していないのに-1」「許可先・固定先と異なる」といった割当は `Domain` として診断できます。

費用は違反の有無にかかわらず元の目的関数です。`cost` に違反ペナルティを足した値が返るわけではありません。

### 例19　発展⑱：手持ちの初期割当を改善する

**例18からの変更点：** 容量を一部超える割当 `{0, 0, 0, 1}` を出発点にして、`improve` で改善します。

ファイル：`examples/example19.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2);
    for (auto& g : p.groups) g.upper[0] = 2;
    p.vertices[0].fixed = 0;
    p.add_hyperedge({0, 2}, S::CountCost::connectivity(5));
    S solver(p, 7);

    std::vector<int> initial = {0, 0, 0, 1};
    std::cout << std::boolalpha
              << "initial feasible=" << solver.evaluate(initial).feasible << '\n';
    const auto& r = solver.improve(initial, {20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

`improve(initial, options)` の第1引数は初期割当、第2引数は `solve` と共通の探索設定です。初期割当を指定して始めるため、ソルバーがゼロから割当を作る `solve` と使い分けます。

この初期割当はグループ0の容量2に対して3個を置いています。ただし、各要素の配置先は許可された先で、仕事0の固定先も守っています。このように、負荷や辺の個数の制約に違反する初期解は渡せます。

**許可先・固定先・未採用許可に違反する初期解は渡せません。** 例18の `bad` のような割当を直接 `improve` で修復しようとすると入力検査に失敗します。まず各要素が置ける先へ直してから渡します。`evaluate` が診断できる範囲と `improve` が受け付ける範囲は、この点が異なります。

`improve` の第3引数を省略すると、固定されていない全要素が改善対象です。返る `Result` の読み方は `solve` と同じです。費用0で実行可能な割当として、例えば `{0, 1, 0, 1}` があります。

### 例20　発展⑲：一部の要素だけを動かす

**例19からの変更点：** 初期割当を `{0, 0, 1, 1}` にし、仕事1と2だけを動かします。

ファイル：`examples/example20.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2);
    for (auto& g : p.groups) g.upper[0] = 2;
    p.vertices[0].fixed = 0;
    p.add_hyperedge({0, 2}, S::CountCost::connectivity(5));
    S solver(p, 7);

    std::vector<int> initial = {0, 0, 1, 1};
    const auto& r = solver.improve(initial, {20'000}, std::vector<int>{1, 2});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';

    assert(r.assignment[0] == initial[0] && r.assignment[3] == initial[3]);
    const auto& held = solver.improve(r.assignment, {0}, std::vector<int>{});
    std::cout << "held iterations=" << held.iterations << '\n';
}
```

`improve` の第3引数 `std::vector<int>{1, 2}` は、動かしてよい要素番号の一覧 `movable` です。グループ番号の一覧ではありません。要素0、3は初期割当の位置を保ちます。`fixed` が設定された要素は、一覧へ入れても動きません。

初期割当の費用は5です。1と2を交換すると `{0, 1, 0, 1}` になり、容量を守って費用0になります。動かさない要素も負荷・辺の個数・共有負荷・費用の計算に全て含まれます。範囲外を削除した小さな別問題として採点するわけではありません。

最後の呼出しは2つの境界条件を示します。

| 指定 | 意味 |
|---|---|
| `r.assignment` をそのまま渡す | 自分の直前の結果も初期割当に使える。内部で安全にコピーしてから状態を変更する |
| `{0}` | 時間予算0。初期割当の取込みなど必要な処理は行うが、近傍探索は行わない |
| `std::vector<int>{}` | 動かせる要素が0個。第3引数の省略とは異なる |

`held.iterations` は0になります。時間予算0は「処理時間が0」という意味ではありません。`improve` は渡した割当の集計を行います。

### 例21　発展⑳：再開し、結果を保存する

**例20からの変更点：** 最初は2ミリ秒だけ改善し、その後20ミリ秒の探索を再開します。途中結果はコピーして残します。

ファイル：`examples/example21.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2);
    for (auto& g : p.groups) g.upper[0] = 2;
    p.vertices[0].fixed = 0;
    p.add_hyperedge({0, 2}, S::CountCost::connectivity(5));
    S solver(p, 7);

    std::vector<int> initial = {0, 0, 1, 1};
    auto saved = solver.improve(initial, {2'000}, std::vector<int>{1, 2});
    solver.set_seed(11);
    solver.resume({20'000});
    const auto& r = solver.result();
    std::cout << "saved cost=" << saved.cost << '\n';

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';

    std::cout << "iterations=" << r.iterations << " elapsed_us=" << r.elapsed_us << '\n';
    const auto& synced = solver.resume({0});
    std::cout << "zero-budget iterations=" << synced.iterations << '\n';
}
```

| 呼出し・値 | 意味 |
|---|---|
| 最初の `{2'000}` | 2,000マイクロ秒の探索予算 |
| `auto saved = ...` | 戻り値をコピーして保存。`const auto&` と異なり後の変更に影響されない |
| `set_seed(11)` | 乱数状態だけを11から初期化する。問題・割当・探索範囲は保持 |
| `resume({20'000})` | 現在の探索状態と、要素1・2だけという範囲を引き継ぐ |
| `result()` | 最後に確定した公開結果への参照を得る。新しい探索はしない |
| `iterations` | 今回の呼出しでの探索試行回数。累計でも採用回数でもない |
| `elapsed_us` | 今回の呼出しの経過時間。単位はマイクロ秒 |

戻り値は探索中に見つけて保持した最良の割当です。探索内部で最後にいた位置と同じとは限りません。`resume` は内部の現在位置から続け、保存してある最良解も引き継ぎます。`solve` は新しい割当を作り、部分範囲の制限も解除するので、同じ意味ではありません。

問題を変更しなければ、実行可能な保存済み最良解は再開によって失われません。ただし、予算を追加すれば必ず費用が下がるという保証もありません。

`solve`、`improve`、`resume`、`result` の戻り値はソルバー内部への参照です。次の変更操作をまたいで値を残したいときは、この例の `saved` のようにコピーします。

`result()` は、最初の探索前や条件更新直後には呼べません。探索後に使います。未実行のソルバーで `resume` を呼んだ場合は、必要な初期割当を作ります。既に同期済みの状態で `resume({0})` を呼ぶと、探索を追加せず結果を受け取れます。

同じシードでも、時間制限の下ではCPUの速さや他の処理によって試行回数が変わり、完全に同じ結果になるとは限りません。固定試行回数を指定する公開APIはありません。

### 例22　発展㉑：要素・グループの条件を更新する

**例19の問題を基にした変更点：** 一度解いてから、仕事1をグループ1へ置く報酬、グループ0の固定費、グループ1の容量を更新します。

ファイル：`examples/example22.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2);
    for (auto& g : p.groups) g.upper[0] = 2;
    p.vertices[0].fixed = 0;
    p.add_hyperedge({0, 2}, S::CountCost::connectivity(5));
    S solver(p, 7);

    auto saved = solver.solve({20'000});
    p.vertices[1].cost_by_group = {{1, -2}};
    p.groups[0].open_cost = 4;
    p.groups[1].upper[0] = 3;
    solver.set_vertex(1, p.vertices[1]);
    solver.set_group(0, p.groups[0]);
    solver.set_group(1, p.groups[1]);
    std::cout << "old assignment under new costs="
              << solver.evaluate(saved.assignment).cost << '\n';
    const auto& r = solver.resume({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

| 変更 | 各引数・値の意味 |
|---|---|
| `cost_by_group = {{1, -2}}` | グループ1へ置く費用を-2、すなわち報酬2にする |
| `p.groups[0].open_cost = 4` | グループ0の使用固定費を4にする |
| `p.groups[1].upper[0] = 3` | グループ1の第0資源の上限を3へ変更 |
| `set_vertex(1, p.vertices[1])` | 要素番号1の設定を、新しい `Vertex` 全体で置き換える |
| `set_group(0, ...)`、`set_group(1, ...)` | 各グループ番号の設定を、新しい `Group` 全体で置き換える |

ソルバーはコンストラクタで問題のコピーを所有します。手元の `p` だけを変更しても、ソルバーの問題は変わりません。対応するsetterで変更を伝えます。

setterは差分のパッチではなくオブジェクト全体の置換です。元の `p` を保持して編集してから渡すと、変更しなかった負荷や制限を残せます。特にデフォルト構築した `Vertex` の `load` は空で、そのまま渡すと資源数と合いません。

setter直後でも `evaluate(saved.assignment)` は新しい問題を使って採点できます。一方、公開探索結果はまだ更新されていないので `result()` を呼びません。`resume` によって内部の集計・探索候補を新条件へ同期し、続けて改善します。新条件で費用の意味や実行可能性が変わるため、変更前の費用と単純に優劣を比べないようにします。

同期だけしたい場合は `resume({0})` を使えます。ただし、容量超過などを改善する探索は行いません。

### 例23　発展㉒：ハイパー辺の費用・所属を更新する

**例01からの変更点：** いったん解いた後、共有関係を `{0, 1}` から `{0, 1, 2}` へ広げ、分離費用を9に変更します。

ファイル：`examples/example23.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2);
    for (auto& g : p.groups) g.upper[0] = 2;
    int e = p.add_hyperedge({0, 1}, S::CountCost::connectivity(5));
    S solver(p, 7);
    solver.solve({20'000});
    p.hyperedges[e].vertices = {0, 1, 2};
    p.hyperedges[e].span_cost = S::CountCost::cut(9);
    solver.set_hyperedge(e, p.hyperedges[e]);
    const auto& r = solver.resume({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

`add_hyperedge` から受け取った辺番号 `e` を保持します。`p.hyperedges[e].vertices` が所属要素の一覧です。ここでは3要素へ増やしました。`span_cost = cut(9)` は2グループ以上を使うと費用9という設定です。

`set_hyperedge(e, p.hyperedges[e])` は、番号 `e` の辺全体を置き換えます。費用だけ、負荷だけ、上下限だけを変更する用途にも使えます。所属要素を変更しても同じAPIです。

容量2のグループに3要素は収まらないので、この辺は必ず2グループへ分かれ、実行可能解の費用は9です。手元の `p.hyperedges` を増減させただけではソルバーの辺数は変わりません。要素数・グループ数・資源数・辺数を変更する方法は例25で扱います。

### 例24　発展㉓：部分範囲と入力変更が衝突したとき

**例20を基にした変更点：** 動かしてはいけない要素0の許可先を、現在地0からグループ1へ変更します。この例では要素0を `fixed` にしていません。部分範囲の外にあることだけが移動を止めています。

ファイル：`examples/example24.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2);
    for (auto& g : p.groups) g.upper[0] = 2;
    p.add_hyperedge({0, 2}, S::CountCost::connectivity(5));
    S solver(p, 7);
    std::vector<int> initial = {0, 0, 1, 1};
    solver.improve(initial, {20'000}, std::vector<int>{1, 2});
    p.vertices[0].allowed_groups = std::vector<int>{1};
    solver.set_vertex(0, p.vertices[0]);
    auto conflict = solver.resume({0});
    std::cout << std::boolalpha << "scope_conflict=" << conflict.scope_conflict
              << " feasible=" << conflict.feasible
              << " kept=" << conflict.assignment[0] << '\n';
    auto repaired = conflict.assignment;
    repaired[0] = 1;
    const auto& r = solver.improve(repaired, {20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

`improve(..., {1, 2})` により動かせるのは要素1・2です。要素0は0に置かれたまま、`allowed_groups = {1}` に変わります。ソルバーが勝手に0を動かすと部分範囲の約束を破り、動かさないと新しい許可先を破る、という衝突です。

`resume({0})` の結果では `scope_conflict=true` になり、要素0の配置先は0のままです。`feasible=false` で `Domain` 違反が残ります。この衝突がある間は、時間を与えても探索は進みません。

そこで結果の割当を `repaired` へコピーし、要素0を新しい許可先1へ直します。次の `improve(repaired, {20'000})` では第3引数を省略し、固定以外の全要素を動かせるようにして再開します。これにより、新条件の下で容量も含めて改善できます。初期割当の許可先が正しいことは、例19と同じ要件です。

新しい割当から作り直してよい場合は、全範囲で始める `solve` でも対応できます。なお、単なる容量超過が `scope_conflict` になるわけではありません。このフラグは、更新後の配置先条件と範囲外を動かさない約束との衝突を示します。

### 例25　発展㉔：サイズも含めて問題全体を入れ替える

**例01からの変更点：** 一度解いた後、3要素・3グループ・2資源の別問題へ交換します。

ファイル：`examples/example25.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2);
    for (auto& g : p.groups) g.upper[0] = 2;
    p.add_hyperedge({0, 1}, S::CountCost::connectivity(5));

    S solver(p, 7);
    solver.solve({20'000});
    S::Problem next(3, 3, 2);
    for (auto& g : next.groups) g.upper = {1, S::unlimited};
    next.add_hyperedge({0, 1, 2}, S::CountCost::connectivity(2));
    solver.reset_problem(next);
    solver.set_seed(7);
    const auto& r = solver.resume({20'000});

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

`next(3, 3, 2)` は新しい `Problem` です。`upper = {1, S::unlimited}` は第0資源を1以下、第1資源には実用上の上限を設けない指定です。`unlimited` は `int64_t` の最大値を表す定数で、数学的な無限大ではありません。実際に集計する負荷と途中の計算は整数の範囲内に収めます。

`reset_problem(next)` は問題全体を置き換え、前の割当・部分範囲・公開結果を破棄します。要素・グループ・資源・辺の数を変えられます。これだけでは探索結果は作られません。

乱数状態は `reset_problem` では初期化されないため、ここでは `set_seed(7)` で明示的に初期化しました。続く `resume({20'000})` は新しい問題の初期割当から始めます。`solve` を使っても構いません。

新しい問題は各グループに1個しか置けないので、3要素は3グループを使います。辺の費用は $2\times(3-1)=4$ です。第1資源の既定負荷は全要素0のままです。

### 例26　発展㉕：探索の設定を名前付きで書く

**例19を基にした変更点：** 初期割当を実行可能な `{0, 0, 1, 1}` とし、`Options` の各フィールドを明示します。焼きなましによる確率的な受入れを無効にします。

ファイル：`examples/example26.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(4, 2);
    for (auto& g : p.groups) g.upper[0] = 2;
    p.vertices[0].fixed = 0;
    p.add_hyperedge({0, 2}, S::CountCost::connectivity(5));
    S solver(p, 7);

    std::vector<int> initial = {0, 0, 1, 1};
    S::Options options;
    options.time_limit_us = 20'000;
    options.annealing = false;
    const auto& r = solver.improve(initial, options);

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

| フィールド | この例の値 | 既定値 |
|---|---|---|
| `time_limit_us` | 20,000マイクロ秒 | 100,000マイクロ秒 |
| `annealing` | `false` | `true` |

`S::Options options;` で既定設定を作り、必要なフィールドを代入します。これまでの `{20'000}` は、第1フィールドだけを指定した同じ型の値です。`solve({})` のように空の設定を渡すと既定値を使います。引数なしの `solve()`、`improve(initial)`、`resume()` という呼び方はありません。

`annealing=false` では、費用と制約違反を合わせた内部評価が改善または同点になる候補だけを受け入れます。「元の費用が絶対に増えない」「探索中は常に実行可能」という指定ではありません。内部評価には違反量も入るためです。仕組みは付録で説明します。

この例では1と2の交換で費用5から0へ改善できます。`true` と `false` のどちらがよいかは問題に依存します。候補の作り方には引き続き乱数を使うため、`false` にしても乱数を使わない厳密解法にはなりません。

時間予算は各呼出しごとです。コンストラクタの入力検査・コピーは `solve` の時間計測の外側にあります。全体の時間を制御したい場合は、次の例のように呼出し元でも時計を持ちます。

### 例27　発展㉖：複数の開始点を全体予算内で比べる

**例12の問題を拡張：** 重なり合う2本の辺を作り、開始点を変えて複数回解きます。各回は最大10ミリ秒、全体の目安は60ミリ秒とし、見つかった実行可能解の中で最も安いものを保存します。

ファイル：`examples/example27.cpp`

```cpp
#include "hypergraph_partitioner_v07.hpp"

using S = HypergraphPartitioner;

int main() {
    S::Problem p(6, 3);
    for (auto& g : p.groups) g.upper[0] = 2;
    p.add_hyperedge({0, 1, 2}, S::CountCost::connectivity(5));
    p.add_hyperedge({2, 3, 4}, S::CountCost::connectivity(3));

    using Clock = std::chrono::steady_clock;
    const auto deadline = Clock::now() + std::chrono::milliseconds(60);
    std::optional<S::Result> best;
    int starts = 0;
    for (uint64_t seed = 0; Clock::now() < deadline; ++seed) {
        S solver(p, seed);
        int64_t left = std::chrono::duration_cast<std::chrono::microseconds>(
            deadline - Clock::now()).count();
        if (left <= 0) break;
        auto candidate = solver.solve({std::min<int64_t>(10'000, left)});
        ++starts;
        if (candidate.feasible && (!best || candidate.cost < best->cost))
            best = candidate;
    }
    std::cout << "starts=" << starts << '\n';
    if (!best) {
        std::cout << "no feasible solution found within budget\n";
        return 0;
    }
    const auto& r = *best;

    std::cout << std::boolalpha << "feasible=" << r.feasible
              << " cost=" << r.cost << '\n';
    std::cout << "assignment:";
    for (int k : r.assignment) std::cout << ' ' << k;
    std::cout << '\n';
}
```

| 新しい値・処理 | 意味 |
|---|---|
| `Problem(6, 3)`、各容量2 | 6要素を3グループに2個ずつ分ける |
| 辺 `{0, 1, 2}` の重み5 | 1つ目の共有関係の分散費用 |
| 辺 `{2, 3, 4}` の重み3 | 2つ目の共有関係の分散費用。要素2は両辺に所属する |
| `milliseconds(60)` | 呼出し元で管理する全体の時間目安 |
| `seed = 0, 1, ...` | 開始ごとに変える乱数の初期値 |
| `left` | 期限までの残り時間をマイクロ秒へ変換した値 |
| `min(10'000, left)` | 各回の予算を10ミリ秒と残り時間の小さい方にする |
| `optional<S::Result> best` | 実行可能解をまだ見つけていない間は空。見つけたらコピーを保持 |

`steady_clock` は経過時間を測る時計です。コンストラクタの処理後にも残り時間を確認することで、そのコストも全体予算へ含めています。各探索の終了には多少の超過があるため、60ミリ秒を厳密に保証する処理ではありません。

比較条件の最初に `candidate.feasible` を置いています。費用の小さい違反解で実行可能解を上書きしないためです。どの開始でも実行可能解を得られなかった場合は、専用のメッセージを出して終了します。それは問題の不可能性の証明ではありません。

この例では両辺とも最低2グループを使うため費用は少なくとも8で、例えば `{0, 0, 1, 1, 2, 2}` がその値を達成します。複数開始は常に単一の長い探索よりよいとは限らず、初期化の費用も毎回かかります。実際の問題では、同じ全体予算で比較して使い分けます。

ここでの繰返しは利用者が書いた処理です。公開APIに自動マルチスタートや試行回数指定の設定はありません。状態を保って続きを探索する用途には、例21の `resume` を使います。

## 第4部　入力とAPIをひと通り確認する

### 4.1　入力の基本ルール

| 項目 | ルール |
|---|---|
| `Problem(n, k, r)` | 既定引数は0要素・1グループ・1資源。要素数は0以上、グループ数と資源数は1以上 |
| 配列の長さ | 要素負荷、グループ上下限、負荷費用は資源数分必要。共有負荷だけは省略すると全資源0になる |
| 負荷 | 0以上の `int64_t`。合計・差分計算が型の範囲に収まる必要がある |
| 費用 | 有限の `double`。NaNや無限大を渡さず、中間計算も有限に収める |
| 上下限 | 下限・上限とも0以上で、下限が上限以下。例：下限2、上限5 |
| 個数上限の既定値 | `INT_MAX`。負荷上限の `S::unlimited` とは型が異なる |
| 許可先・辺の所属 | 順不同でよい。同じ番号の重複は除かれる |
| `_by_group` | 順不同でよい。同じグループ番号の重複は禁止。既定値を置き換える |
| 同じ所属を持つ別々の辺 | 自動で統合されない。費用・共有負荷は辺ごとに加算される |
| 空の辺 | 作成可能。個数はすべて0。正の個数下限を課せば満たせない |
| 入力検査 | 主に `assert`。例外やエラーコードで無効入力を回復するAPIではない |

資源を使わないモデルでも、資源数は1以上にし、要素負荷を0、上下限を適切に設定します。負荷0の要素が置かれたグループも、要素数に基づいて使用中と判定されます。

`S::unlimited` を実際の負荷として大量に足すことはできません。また、有限の費用でも極端な値を多数足すとオーバーフローや桁落ちが起こり得ます。業務上必要な単位・重みに揃えて入力します。

### 4.2　データ型の索引

| 型・フィールド | 役割 | 対応例 |
|---|---|---|
| `Problem.num_groups`、`num_resources`、`vertices`、`groups`、`hyperedges` | 問題の寸法と各入力一覧 | 01、04、25 |
| `Problem.constant`、`add_hyperedge` | 定数費用、辺の追加とIDの取得 | 01、02、10 |
| `Vertex.load`、`load_by_group` | 要素負荷と配置先別の負荷 | 03〜05 |
| `Vertex.default_cost`、`cost_by_group` | 配置費用と配置先別の費用 | 02、22 |
| `Vertex.allowed_groups`、`fixed` | 許可先と固定先 | 06、14、15 |
| `Vertex.allow_reject`、`reject_cost` | 採否と未採用費用 | 14〜16 |
| `Group.lower`、`upper`、`lower_only_if_used` | 必須の負荷範囲と空グループの扱い | 03、08 |
| `Group.open_cost`、`load_penalty` | 固定費と負荷の目標費用 | 07、09 |
| `LoadPenalty.lower`、`upper`、4係数、`operator()` | 目標区間、一次・二次費用、その直接計算 | 09 |
| `Hyperedge.vertices` | 関係する要素の集合 | 01、23 |
| `Hyperedge.span_cost`、`selected_cost`、`local_cost`、`local_cost_by_group` | 3種類の個数に対する費用 | 01、11、16 |
| `Hyperedge.shared_load`、`shared_load_by_group` | 共有物の負荷 | 10、11 |
| `Hyperedge.min_span`、`max_span` | 使用グループ数の上下限 | 12 |
| `Hyperedge.min_selected`、`max_selected` | 採用数の上下限 | 15、16 |
| `Hyperedge.local_lower`、`local_upper` と各 `_by_group` | グループ別個数の上下限 | 13 |
| `CountCost.activation`、`connectivity`、`cut`、`linear`、`lookup` | 個数費用を作る5つの関数 | 01、11、16、17 |
| `CountCost.active`、`extra`、`cut_value`、`table`、`operator()` | 費用関数の部品と直接計算 | 17 |
| `Evaluation.cost`、`feasible`、`violation` | 採点と診断 | 18 |
| `Violation.kind`、`id`、`group`、`resource`、`amount` | 違反の種類・場所・大きさ | 18 |
| `Result.assignment`、`scope_conflict`、`iterations`、`elapsed_us` | 割当、範囲の衝突、今回の計測値 | 01、21、24 |
| `Options.time_limit_us`、`annealing` | 探索予算と受入れ方針 | 01、26 |
| `S::unlimited` | 負荷上限の既定値に使う定数 | 25 |

`Result` は `Evaluation` を継承しているため、`cost`、`feasible`、`violation` も直接参照できます。寸法を変更したい場合は、個々の数値だけを後から書き換えて配列との整合を壊さず、新しい `Problem` を作って `reset_problem` に渡すと分かりやすくなります。

### 4.3　公開操作の索引

| 操作 | 役割・返り値 | 対応例 |
|---|---|---|
| `HypergraphPartitioner(problem, seed)` | 入力を所有し検査する。seedは省略可能で0 | 01 |
| `solve(options)` | 新しい割当を作り、全範囲で探索。`const Result&` | 01 |
| `improve(initial, options, movable)` | 指定割当から、指定範囲を改善。範囲省略は全体。`const Result&` | 19、20 |
| `resume(options)` | 状態・範囲を引き継いで探索。必要なら初期化・更新同期。`const Result&` | 21 |
| `evaluate(assignment) const` | 新しい問題設定で完全採点。値として `Evaluation` を返す | 18、22 |
| `result() const` | 最後の公開結果への参照。同期前には使わない | 21 |
| `set_vertex(id, value)` | 要素設定全体を置換 | 22 |
| `set_group(id, value)` | グループ設定全体を置換 | 22 |
| `set_hyperedge(id, value)` | 辺設定全体を置換 | 23 |
| `reset_problem(problem)` | 寸法を含め問題全体を交換し、探索状態を破棄 | 25 |
| `set_seed(seed)` | 乱数だけを初期化 | 21、25 |

`evaluate` と初期割当の引数は `std::span<const int>` です。長さを持つ連続配列の参照であり、例の `std::vector<int>` のほか `std::array<int, N>` なども渡せます。部分範囲の型は `optional<vector<int>>` です。省略、明示的な空配列、一部の番号の指定を区別します。

### 4.4　実際の問題へ移すときの確認

1. 要素を「仕事そのもの」とするか「候補方式」とするかを先に決める。
2. 資源の単位を決め、仕事単独の負荷と共有物の負荷を分ける。
3. 必ず守る条件は上下限・許可先に、守れるとうれしい条件は費用にする。
4. 辺ごとに、`span`、`selected`、`local` のどれを数えたいかを確認する。
5. 小さな手計算できる割当を `evaluate` に渡し、費用と違反を確認する。
6. 実運用へ渡す前に `feasible` を確認する。見つからなければ違反一覧を調べる。

合計容量が不足している、固定先が偏っている、同じ集合に両立しない個数制約がある、といった原因は時間を増やすだけでは解消しません。一方、容量合計に余裕があっても、個々の要素が大きすぎる、共有物が入り切らない、許可先が狭い、といった理由で実行可能解が存在しない場合があります。

## 付録A　ライブラリ実装：全体の流れ

ここからは利用方法ではなく、v07の内部実装を扱います。公開APIだけで利用する場合は後回しにして構いません。

大枠は「初期割当を用意し、小さな変更を繰り返してよい割当を保持する」方式です。組込みの厳密解法、分割・縮約を繰り返す多段階方式、自動マルチスタートはありません。

1. **入力を正規化する。** 許可先・所属・例外設定を整列し、入力の形を検査する。
2. **索引と条件を同期する。** 要素から関係する辺を調べる索引、可動範囲、費用・違反の尺度を準備する。条件変更後は現在解と保存済み最良解を新しい条件で採点し直す。
3. **開始位置を用意する。** `solve` は割当を作成、`improve` は渡された割当を集計、`resume` は現在状態を使う。未開始の `resume` は割当を作成する。
4. **候補を作る。** 1要素の移動、2要素の交換、採用・未採用の交換から、小さな変更を提案する。
5. **差分だけ採点する。** 影響する要素・辺・グループの変化をまとめ、変更を採用するか決める。採用したときだけ状態を更新する。
6. **最良解を保管する。** 実行可能解を優先し、その中では費用が小さいものを残す。時間まで4〜6を繰り返す。
7. **公開結果を作る。** 保存した最良割当の元の費用・生の違反量を確定し、今回の試行回数と経過時間を返す。

| 処理 | 主な内部関数 |
|---|---|
| 入力検査・整列 | `normalize`、`normalize_vertex`、`normalize_edge`、`check_group` |
| 許可先・既定値の参照 | `allowed`、`permitted`、`choice_count`、`choice`、`pick` |
| 入力変更の反映 | `synchronize` |
| 割当全体の集計 | `build` |
| 元の単位で違反一覧を作成 | `diagnose` |
| 初期割当 | `construct` |
| 候補作成 | `destination`、`generate` |
| 差分採点・確定 | `propose`、`commit` |
| 最良解の比較・保存 | `better`、`remember` |
| 時間制限と候補の受入れ | `search` |

## 付録B　入力の正規化とデータ構造

### B.1　入力の所有と整列

コンストラクタは `Problem` を値で受け取り、内部に所有します。呼出し元が通常の変数を渡せばコピー、`std::move` した値を渡せば移動として扱えます。例では説明を揃えるためコピーを使っています。

要素の許可先と辺の所属要素は、整列して重複を除きます。配置先別の例外設定も整列しますが、重複番号は無効入力です。例外値を参照する `pick` は、整列済み一覧を二分探索し、なければ既定値を使います。

費用表の長さ、負荷の符号・配列長、グループ番号、固定先なども検査します。形式検査に通ることと、全体の実行可能解が存在することは別です。例えば3個の必須要素に容量合計2を指定する入力は、形式上は正しくても実行不可能です。

### B.2　要素から辺を引く索引

ある要素を動かすと、その要素を含む辺だけが変化します。そこで「各要素がどの辺に入るか」を、1本の配列 `incidence_` と開始位置 `offsets_` で保存します。これはCSRと呼ばれる格納方法です。

例えば辺0が `{0, 1}`、辺1が `{1, 2}` なら、次の対応になります。

| 要素 | 関係する辺 | 平らな配列の参照範囲 |
|---|---|---|
| 0 | 0 | 添字0だけ |
| 1 | 0、1 | 添字1〜2 |
| 2 | 1 | 添字3だけ |

このとき `incidence_` は `{0, 0, 1, 1}`、`offsets_` は `{0, 1, 3, 4}` です。要素$v$の範囲は `offsets_[v]` 以上 `offsets_[v+1]` 未満です。辺番号順に作るため、各要素の辺一覧も整列済みになります。

費用や容量だけの変更では、この所属関係は変わりません。索引を作り直すのは、最初と、`set_hyperedge` で所属が変わった場合、問題全体を交換した場合です。

### B.3　割当状態に何を持つか

| 内部の値 | 役割 |
|---|---|
| `a` | 要素ごとの現在の配置先 |
| `size`、`load` | グループごとの要素数・資源別合計負荷 |
| `span`、`selected` | 辺ごとの使用グループ数・採用数 |
| `count` | 辺ごとの、個数が正であるグループと個数の組 |
| `members`、`position` | グループにいる要素一覧と、各要素がその一覧の何番目か |
| `score` | 費用、正規化した違反量、違反条件数 |

`count` は全ての辺とグループの組を大きな表にせず、使っているグループだけを番号順に保存します。例えば1000グループ中2グループだけを使う辺なら、その2組だけです。参照時は8組以下なら順に探し、多ければ二分探索します。挿入・削除時には整列を保ちます。

`members` には未採用要素の一覧も追加で持ちます。要素を削除するときは末尾の要素で穴を埋め、対応する `position` を直します。一覧全体をずらす必要がありません。

公開結果用の最良割当 `best_a_` は、探索中の現在割当とは別に保存します。費用が一時的に悪くなる移動を試しても、既に得たよい解を失わないためです。

## 付録C　完全評価と、比較に使う値

### C.1　割当全体を評価する

`build` は、割当から状態を作り直します。

1. 各要素の配置費用または未採用費用を加える。通常グループへ置かれた要素の負荷と個数を加える。
2. 各辺について、採用された要素の配置先を集めて整列し、グループ別個数を作る。
3. その個数から `span`、`selected`、個数費用・制約、共有負荷を集計する。
4. 全グループの開設費用、負荷費用、負荷の上下限を評価する。

共通の正の `local_lower` は空グループにも効きます。内部ではまず「全グループが空の場合の違反」をまとめて計算し、例外設定と実際に使ったグループについて補正します。公開する違反一覧では、必要な個々のグループを明示します。

`diagnose` は元の負荷単位で不足・超過を列挙します。この段階で `Problem.constant` を費用へ加えます。探索内部の費用からは定数を除き、大きな定数を足した状態で小さな費用差を引いて精度を失うことを避けています。

### C.2　実行可能性と最良解の比較

最良解を更新する `better` は、次の順に比較します。

1. 一方だけ実行可能なら、実行可能な方を選ぶ。
2. 両方が違反解なら、正規化した違反量の合計が小さい方を選ぶ。
3. 違反量が同程度、または両方とも実行可能なら、費用が小さい方を選ぶ。

違反条件の「件数」が少ないだけで優先するわけではありません。例えば、大きな容量超過1件と小さな超過2件なら、違反量の比較が重要になります。件数は、違反が0件かどうかの判定にも使います。

内部比較では浮動小数点の微小な差を無視するため、相対的な幅 $2\times10^{-15}$ を使います。整数の制約に余裕を足しているわけではありません。公開される制約判定は元の整数の上下限に基づきます。

### C.3　資源の単位を揃えて違反量を比較する

CPUとメモリでは数値の桁が異なる場合があります。容量違反を比較する内部尺度として、資源$r$の代表単位を次のように作ります。

$$u_ {r}=\max\left(1,\frac{\sum_ {v=0}^{N-1}\ell^{\mathrm{default}}_ {v,r}}{\max(1,N)}\right)$$

ここで $u_ {r}$ は比較用の単位、$\ell^{\mathrm{default}}_ {v,r}$ は `Vertex.load[r]` です。配置先別の上書き値や共有負荷を平均に含めるわけではありません。容量違反は元の違反量を $u_ {r}$ で割り、辺の個数違反と配置先違反は個数のまま加えます。この合計を以下では $V(a)$ と書きます。

この正規化は内部でどちらの違反解を試すかの基準です。入力の容量、元の目的関数、`Violation.amount` の単位は変更しません。

## 付録D　初期割当を作る

`construct` は一時的に全要素を未採用の位置に置き、要素を1個ずつ配置します。必須要素もこの途中状態では未配置ですが、これは内部の組立て用状態であり、利用者へ実行可能として返すものではありません。

1. 要素順をシャッフルする。
2. 固定要素を先に処理するよう順を並べ直す。
3. 各要素の候補先を仮に試し、その時点の費用と違反量で比較する。
4. 選んだ先へ置き、最後に割当全体を集計し直す。

初期配置の試し方は入力と時間予算で変わります。

| 条件 | 候補先の調べ方 | 比較 |
|---|---|---|
| 予算が10,000マイクロ秒以上で、未採用を許す要素が一つもない | 候補数32以下なら全候補。列挙開始点をランダムにずらす | 保存用の最良解と同じ、実行可能性・違反量・費用の順 |
| それ以外 | 候補数8以下なら全候補 | 費用に違反量のペナルティを加えた値 |
| 上記の候補数のしきい値を超える場合 | 8候補をランダムに試す。同じ候補が重なる可能性もある | 各行の比較方法 |

固定要素は固定先だけを試します。全候補を試す場合でも、これから置く要素まで全組合せを調べるわけではありません。その時点でよさそうな選択を積み重ねるため、実行可能な完成形を見逃す場合があります。

時間予算0でも、`solve` や未開始の `resume` はこの初期化を行います。初期化の途中で厳密に時間切れへ割り込む処理はありません。

## 付録E　小さな変更の候補を作る

### E.1　動かせる要素と配置先

探索対象は、部分範囲内、固定ではない、選択肢が2個以上、という条件を満たす要素です。この一覧から1要素を選びます。未採用を許す場合、その選択肢も1個として数えます。

行き先は通常の許可先一覧から選びますが、要素に関係する辺がある場合は75%の確率で、関係する辺を1本選び、その辺の別要素を含む任意の所属要素の現在地を試します。自分自身を引くこともあります。その行き先が許可されていなければ、通常の選択肢から選び直します。

関係する要素を近くへ置く候補を増やす意図ですが、常に同じグループへ寄せるわけではありません。候補先が現在地と同じなら、その試行は変更なしになります。

### E.2　移動と交換

通常の候補は1要素の移動です。その経路では25%の確率で、行き先にいる1要素を選び、元の場所との交換を試みます。未採用の一覧も交換先に含みます。

交換相手が範囲外だったり、元の場所へ置けなかったり、行き先に相手がいなかったりすると、その候補生成は失敗します。失敗時に必ず単点移動へ切り替えるわけではありません。

交換により、人数上限が埋まった2グループ間でも配置を入れ替えられます。ただし、要素ごとの負荷や共有物が違えば交換でも容量違反が起こり得ます。交換は実行可能性を一般に保証する操作ではありません。

### E.3　採用数固定の集合で、採否を交換する

選んだ要素が未採用を許す場合、最初に4分の1の確率で特別な交換を試します。関係する辺のうち、「採用数の上下限が等しく、その数が0でも全要素数でもない」辺を探します。そこで採用中と未採用の要素を交換できれば、その候補を使います。

この操作は、その辺の現在の採用数を変えずに採用する候補の顔ぶれを変えます。例15の方式選択のように、1個ずつ動かすと一時的に採用数条件を破る問題で役立ちます。他の辺や資源の条件まで常に保つとは限りません。

公開探索が生成する変更は1要素または2要素です。3要素の巡回、大きな集合の再配置、外部の厳密ソルバーによる部分問題の解き直しは実装されていません。

## 付録F　差分評価の詳細

毎回全割当を採点し直すと、変わらない部分まで同じ計算を繰り返します。`propose` は現在状態をその場で書き換えず、変更後の差分を一時領域へまとめます。

### F.1　要素の差分

要素が配置先から行き先へ動くと、配置費用、配置先条件の違反、要素数、要素単独の負荷が変わります。配置先別負荷があれば、移動元で引く量と移動先で足す量は違うことがあります。

影響するグループを `touch` で登録し、同じグループの負荷差・個数差を1つの領域へ足します。未採用は通常の資源を持つグループとして処理しません。

### F.2　辺の個数差をまとめる

CSR索引で、その要素が関係する辺だけをたどります。各辺について移動元の個数を1減らし、移動先の個数を1増やす、というイベントを作ります。未採用側のグループイベントは不要です。

単点移動では、索引の順によりイベントが既に整列しています。2要素交換では2本の整列済み列を併合し、同じ「辺・グループ」の増減を足し合わせます。同じ辺に属する2要素を交換すると、減少と増加が打ち消し合う場合があります。差分が0の組は採点しません。

要素を1個ずつ仮に移動した中間状態の費用を足すのではなく、交換前と交換後を直接比較するのが重要です。共有負荷や非線形の個数費用では、中間状態の単純な足し算が誤った費用差になることがあるためです。

### F.3　共有物の開始・終了だけを反映する

ある辺・グループの個数が0から正になると共有物が必要になり、正から0になると不要になります。正から別の正に変わるだけなら、共有負荷は変わりません。

この境目を使って、共有負荷、`span`、`selected`、`local` の差を作ります。`span` と `selected` が共に変わらない辺は、その全体費用・全体制約の計算を省けます。個数費用・個数制約が全くない辺は、局所採点自体を省く印を入力同期時に作ります。この判定は入力変更後にやり直します。

### F.4　負荷費用は合計してから評価する

要素負荷と共有負荷の差をグループごとに全て足してから、開設費、必須上下限、`LoadPenalty` を計算します。

例えば $x^2$ 型の費用がある場合、同じ$x$を基準に計算した「1増やす差」を2倍しても、$x$を2増やす差にはなりません。変更全体を合算した負荷で一度採点することで、この非線形性を正しく扱います。

### F.5　採用した変更だけを確定する

`commit` は、候補が受け入れられた後に疎な個数一覧、辺の集計、グループ負荷、要素の所属一覧、割当、現在の評価値をまとめて更新します。拒否された候補では現在状態を変更していないので、巻き戻し処理は不要です。

内部の差分処理は複数要素分を集計できる形ですが、前述のとおり公開探索から生成する候補は最大2要素です。内部テスト用の一般性と公開機能を混同しないようにします。

## 付録G　候補を受け入れる基準

### G.1　費用と違反の組合せ

候補の受入れには、次の内部評価を使います。

$$\Phi(a)=C(a)+\lambda V(a),\qquad C(a)=J(a)-C_ {0}$$

| 記号 | 意味 |
|---|---|
| $\Phi(a)$ | 候補受入れ用の内部評価値 |
| $C(a)$ | 定数を除いた元の費用 |
| $V(a)$ | 付録Cの正規化した違反量の合計 |
| $\lambda$ | 違反をどれくらい重く見るかを決める正の重み |

公開する `Result.cost` は $J(a)$ であり、$\Phi(a)$ ではありません。また、候補の受入れと最良解の保存は別の比較です。現在解では費用と違反を交換条件にして移動できても、保存する最良解では実行可能性を優先します。

費用の代表的な大きさを $\sigma$ とします。内部では、0でない費用係数の絶対値などの平均を使います。個数表では隣り合う項目の差、配置先費用では既定費用との差も対象です。全て0なら $\sigma=1$ とします。配置先別の個数費用や負荷費用の係数、開設費も含みます。定数項は含めません。

違反重みは最初 $\lambda=10\sigma$ です。512試行ごとに、直近512回の現在状態が実行可能だった回数を調べます。

| 実行可能な回数 | 更新 |
|---|---|
| 64回未満 | 重みを1.5倍。上限は $10^8\sigma$ |
| 384回より多い | 重みを1.1で割る。下限は $\sigma$ |
| それ以外 | 変更しない |

実行可能状態にほとんど到達していなければ、違反を減らすことを次第に重視する仕組みです。

### G.2　焼きなましと温度

候補の内部評価から現在の内部評価を引いた値を $\Delta$ とします。$\Delta\leq0$ なら受け入れます。悪化する候補は、`annealing=true` の場合に次の確率で受け入れます。

$$p=\exp\left(-\frac{\Delta}{T_ {\mathrm{eff}}}\right)$$

$p$は受入れ確率、$\exp(x)$は自然対数の底を使う指数関数、$T_ {\mathrm{eff}}$は有効温度です。温度が高いと悪化する変更も試しやすく、低いと試しにくくなります。例えば $\Delta/T_ {\mathrm{eff}}=1$ なら、確率は約0.37です。

時間予算のうち経過した割合を $\rho$ とし、冷却係数を $h=0.01^{\rho}$ とします。通常の温度は $T=\sigma h$。まだ実行可能な最良解を得ていない間は、有効温度に $\lambda h$ を追加します。つまりその間は $T_ {\mathrm{eff}}=T+\lambda h$、実行可能解を保存した後は $T_ {\mathrm{eff}}=T$ です。

`annealing=false` では悪化候補を確率的に受け入れる部分が無効になります。違反重みの調整や、移動候補を乱数で選ぶ処理は残ります。

`resume` は割当・最良解・部分範囲・乱数状態を引き継ぎますが、温度と違反重みのスケジュールは各探索呼出しで開始し直します。1回100ミリ秒と10回10ミリ秒は、同じ処理を完全に区切っただけにはなりません。

### G.3　乱数

内部には64ビットの乱数状態を持ち、定数の加算、ビットの混合から次の値を作ります。整数の候補選択や、0以上1未満の値を使う受入れ判定に利用します。`set_seed` はこの状態だけを更新します。

候補生成に失敗した試行や現在地を選んだ試行も `iterations` に数えます。したがって、この値は差分を計算した回数、変更を採用した回数のどちらとも一致しません。

## 付録H　更新・再開・時間計測

### H.1　入力変更の同期

setterは変更を記録し、次の `solve`、`improve`、`resume` で同期します。所属を変えていれば索引を再構築し、可動範囲や費用尺度を作り直します。

継続中の割当が新しい許可先・固定先に違反した場合、変更可能な範囲内なら、新しい固定先または先頭の許可選択肢へ修復します。範囲外なら勝手に変えず、`scope_conflict` を立てます。固定要素を可動候補から除く前の範囲で修復を判定するので、範囲内の要素を新しく固定する更新にも対応します。

現在解と保存済み最良解は、新しい問題で両方採点し直します。2つの割当が同じなら二重の全体集計を避けます。更新前に最良だった解が更新後も最良とは限らないため、比較し直して残す必要があります。

### H.2　時間と公開結果

探索は通常32試行ごとに時計を確認します。差分イベント数と資源数から見て重い処理をした後は、その周期を待たずに確認します。確認時点で予算を超えれば、次の候補探索へ進まず終了します。

入力同期、初期割当、重い候補の途中、最後の結果作成は瞬時ではないため、時間予算は厳密な上限ではありません。`elapsed_us` はそれらを含む公開探索呼出し全体の時間です。コンストラクタや、呼出し元で行ったsetterの時間は含みません。

最後は最良割当について元の費用と違反を確定します。探索前から確定していた結果で、集計をそのまま使える場合は再利用します。問題も最良割当も変わっていない `resume({0})` では、全体評価を毎回やり直さないようにしています。

## 付録I　計算量を読むための目安

以下では $P=\sum_ {e=0}^{M-1}\lvert T_ {e}\rvert$ を、全ての辺に含まれる要素数の合計とします。同じ要素が2本の辺に入れば2回数えます。$L$ は負荷配列・例外設定・費用表を含む入力全体の大きさです。

| 処理 | 主に効く量 |
|---|---|
| 入力検査・正規化 | 入力サイズと、許可先・所属・例外一覧の整列 |
| 所属索引の構築 | 要素数、辺数、延べ所属数 $P$ |
| 完全評価 | 全要素、全辺の所属と配置先の整列、資源数、例外設定の参照 |
| 1候補の差分評価 | 動く要素の関係辺数、資源数、変更先の疎な個数一覧 |
| 最良解の保存 | 割当一覧のコピーなので要素数に比例 |
| 正の共通 `local_lower` の診断 | 空のグループも含めるため、対象の辺ごとにグループ数分が必要 |

差分評価は全体評価より小さくなることが多いものの、1要素が非常に多くの辺に属する問題や、資源の種類が多い問題では1候補も重くなります。疎な個数一覧はメモリを節約しますが、多数のグループを使う辺では挿入・削除時の移動が増えます。

内部状態は、入力、延べ所属、各グループの資源合計、実際に使われている辺・グループの組などに応じたメモリを使います。全ての要素・グループ・資源の組を無条件に密な表で持つ設計ではありません。ただし、利用者が全ての配置先別例外を入力すれば、その入力分のメモリは必要です。

小さな変更の繰返しであるため、3要素以上を同時に動かさないと脱出しにくい配置、非常に狭い実行可能領域、重なる採用数条件などは難しくなり得ます。初期解を与える、問題を限定した厳密解法と使い分ける、同じ全体予算で複数開始を比べる、といった判断につながります。

## 付録J　検証と再実行

同梱ヘッダは本書が対象にしたv07そのものです。SHA-256は次の値です。

```text
1895984246454ff10eb42b0d3eafe6fd6148d4f7537ab2c9c313f531adb9827b
```

コード例はGCC 13.3.0のC++20・C++23で確認しています。本文の27個のC++コードブロックと同梱の27ソースを照合し、それぞれコンパイル・実行しています。診断値、費用関数の手計算値、空の可動範囲、条件変更時の範囲衝突なども確認対象です。実際の標準出力と結果は `validation/c++20.json`、`validation/c++23.json` に収録しています。一般の入力に対する解品質を測るベンチマークではありません。

Python 3.9以上とGCCがあれば、ZIP展開先で再確認できます。

```bash
python3 run_examples.py --standard c++20
python3 run_examples.py --standard c++23
```

`--standard` はC++の版です。既定は `c++20`。コンパイラ名は `--compiler`、同時コンパイル数は `--jobs` で変更でき、既定はそれぞれ `g++`、2です。生成される実行ファイルは `build/`、実行記録は `validation/` に置きます。

本書の数式は、インラインを `$...$`、独立した式を1行の `$$...$$` で記しています。数式対応のMarkdownビューアで閲覧してください。通常のMarkdownとして前処理したときにも、数式内部の下付き記号が強調記法などへ変換されない形にしています。

## 参考資料

- 対象実装：同梱の `hypergraph_partitioner_v07.hpp`。公開仕様と実装の説明はこのソースに基づく。
- [OR-Tools — Assignment as a Minimum Cost Flow Problem](https://developers.google.com/optimization/flow/assignment_min_cost_flow)：割当を最小費用流で扱う公式例。
- [Kolmogorov and Zabih — What Energy Functions Can Be Minimized via Graph Cuts?](https://www.microsoft.com/en-us/research/publication/what-energy-functions-can-be-minimized-via-graph-cuts/)：グラフカットで最小化できる費用の条件。
- [KaHyPar公式サイト](https://kahypar.org/)：ハイパーグラフ分割とcut-net・connectivityの目的関数。
- [Mt-KaHyPar公式リポジトリ](https://github.com/kahypar/mt-kahypar)：並列のグラフ・ハイパーグラフ分割。
- [OR-Tools — CP-SAT Solver](https://developers.google.com/optimization/cp/cp_solver)：整数制約の取扱いと、最適・実行可能・実行不可能などの終了状態。

外部資料の確認日：2026年10月11日。外部ソルバーのAPI・対応機能は更新されるため、採用時には公式資料の対象版を確認してください。
