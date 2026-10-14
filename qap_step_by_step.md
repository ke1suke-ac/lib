# qap_solver ステップバイステップガイド

対象は `qap_solver_v09.hpp`（v0.9.1）です。設備の配置を題材に、問題の表し方から公開APIの使い方までを学びます。数学は四則演算、表、順列が分かれば読めるように説明します。C++の変数、配列、条件分岐、ループ、関数の基礎は前提とし、テンプレートやラムダ式は登場時に補足します。

本書は単独で読めます。各C++コードは `main` を含む完全なプログラムです。前のコードを貼り足す必要はありません。各節の「変更点」は直前までの例との違いを示し、コード中では共通の入力も再掲します。必要なファイルは、そのコードと本ヘッダーだけです。末尾の付録で初めてライブラリ内部の実装を説明します。

| 読む順番 | 学ぶこと |
| --- | --- |
| 1〜2章 | 対象問題、目的関数、他のソルバーとの使い分け |
| 3章・例01 | 3設備・3位置の最小プログラム |
| 4章・例02〜05 | 費用の確認、単項費用、非対称入力、最大化 |
| 5章・例06〜11 | 空き位置、グリッド、グループ、巡回、選択、グラフ対応 |
| 6章・例12〜16 | 既存解の改善、前処理の再利用、固定配置、部分問題 |
| 7章・例17〜20 | 独自探索、追加評価、代理評価、入力変更 |
| 8章・例21〜24 | 禁止条件、整数の範囲、空問題と締切、結果のデバッグ出力 |
| 9章 | 公開APIの索引と利用時の確認事項 |
| 付録 | 実装の流れと各アルゴリズムの詳細 |

## 1. 何を解くライブラリか？

### 1.1 設備をどこに置くか

3台の設備A・B・Cを、3か所の設置場所へ置く場面を考えます。各設備は1か所を使い、同じ場所に2台は置けません。すべての設備を置けば、すべての場所がちょうど1回使われます。

AとBの間で荷物を頻繁に運ぶなら、両者を近くに置くと輸送費が小さくなります。しかし、BとCの関係や、各設備の設置費も考えると、1台ずつ独立に配置先を決めることはできません。すべての配置を一緒に選び、合計費用を小さくするのが対象問題です。これを**二次割当問題（Quadratic Assignment Problem、QAP）**と呼びます。

本書では設備を一般に「物体」、設置場所を「位置」と呼びます。物体は人やネットワークの頂点でも構いません。位置も、席、グループ内の枠、巡回順の何番目か、と読み替えられます。

### 1.2 答えは一対一の対応

物体と位置に、どちらも0から始まる番号を付けます。配置を $p=(2,0,1)$ と書いたら、次の意味です。

| 物体番号 | 配置先の位置番号 |
| --- | --- |
| 0（A） | 2 |
| 1（B） | 0 |
| 2（C） | 1 |

配列の添字が物体、配列の値が位置です。「位置2に物体0がある」という逆向きの配列ではありません。0、1、2をそれぞれ1回ずつ使った並びを**置換**と呼びます。`{0,0,2}` は位置0が重複するため、正しい置換ではありません。

### 1.3 目的関数とすべての記号

最小化したい費用を次の式で定義します。

$$C(p)=\sum_ {i=0}^{n-1}U_ {i,p_ {i}}+\sum_ {i=0}^{n-1}\sum_ {j=0}^{n-1}F_ {i,j}D_ {p_ {i},p_ {j}}$$

選べる配置には、次の条件があります。

$$0\le p_ {i}<n,\qquad p_ {i}\ne p_ {j}\quad(i\ne j)$$

| 記号 | 意味 |
| --- | --- |
| $n$ | 物体の個数であり、位置の個数でもある |
| $i,j$ | 物体の番号。どちらも0から $n-1$ まで |
| $x,y$ | 位置の番号。どちらも0から $n-1$ まで。入力表を説明するときに使う |
| $p$ | 全物体の配置をまとめた配列 |
| $p_ {i}$ | 物体 $i$ を置く位置番号。プログラムでは `p[i]` |
| $F_ {i,j}$ | 物体 $i$ から物体 $j$ へ運ぶ量など、物体ペアの係数。`flow` |
| $D_ {x,y}$ | 位置 $x$ から位置 $y$ への距離など、位置ペアの係数。`distance` |
| $U_ {i,x}$ | 物体 $i$ を位置 $x$ に置くことだけで生じる費用。`unary` |
| $C(p)$ | 配置 $p$ の合計費用。値が小さい配置ほどよい |
| $\sum$ | 指定した番号を順番に変え、すべて足す記号 |

$F,D,U$ は入力として決まっている表です。選びたい変数は $p$ です。第1項は、各設備の設置費などを1台ずつ足します。このような費用を**単項費用**と呼びます。使わなければすべて0です。

第2項は、まず物体 $i,j$ を選び、実際に置かれた位置 $p_ {i},p_ {j}$ を調べます。運ぶ量と距離を掛け、その値をすべての組について足します。運ぶ量が8、距離が3なら、その向きの費用は24です。二重の $\sum$ は「物体を1つ選び、相手も1つ選ぶ」という2重ループに対応します。

例えば $p=(0,1,2)$、単項費用なし、A–Bの両方向の量が8、A–Cが1、B–Cが2、位置の座標が0、1、3なら、費用は次の値です。

$$C(p)=8\cdot1+8\cdot1+1\cdot3+1\cdot3+2\cdot2+2\cdot2=30$$

### 1.4 数え方と表現できる範囲

式には $i\to j$ と $j\to i$ が別々に入っています。対称な表でも自動で2では割りません。無向の関係を両方向に入れると、関係の費用を2回数えます。後で単項費用を加えるときは、その相対的な重さも考えます。

$i=j$ の項も含みます。これは同じ物体に関する費用 $F_ {i,i}D_ {p_ {i},p_ {i}}$ です。不要なら対角成分を0にします。負の値、方向で異なる値、0以外の対角成分も表せます。`distance` は一般の位置ペア係数なので、実際の距離である必要はありません。

物体ペアの係数と位置ペアの係数の**1組の積**で二次費用を表せることが必要です。物体2個と位置2個の組合せごとに全く自由な費用を入力したり、3物体の同時条件を直接入力したりするAPIはありません。

ライブラリが直接扱う条件は一対一対応です。一般の配置禁止、重さによる容量制約などは別途扱います。「置換として正しい」と「追加した禁止条件をすべて満たす」は別です。出力は配置と費用であり、一般QAPに対する最適性の証明や、追加制約の実行不可能性の証明は返しません。

## 2. 類似ソルバーと使い分け

### 2.1 条件を絞ると厳密解を狙える場合

**厳密解**は、許されるどの配置よりも悪くないと確認された解です。十分な計算を終えられるなら、近似解より費用が大きくなることはありません。ただし計算時間が長くなり、時間切れでその確認まで終わらないことがあります。

| 条件 | 検討する方法 | 得られる保証の範囲 |
| --- | --- | --- |
| 全体の物体数がごく少ない | すべての置換を評価 | 全列挙を完走すれば全体の最適解 |
| 動かす物体だけがごく少ない | 部分問題を作り、その置換を全列挙 | 他の物体と使える位置を固定した条件での最適解。例15で実行する |
| 二次項がない、または配置によらない定数 | 線形割当ソルバー | 単項費用についての最適な一対一割当 |
| 費用が巡回の隣り合う都市間だけで決まる小規模TSP | 部分集合を使う厳密DPなど | 定式化した巡回問題の最適解 |

3物体なら $3!=3\cdot2\cdot1=6$ 通り、8物体なら40,320通り、10物体なら3,628,800通りです。「少ない」の実用上の境界は、評価の重さと利用環境で変わります。

二次項がなくなる最も簡単な例は `flow` がすべて0の場合です。このとき、目的式は $C(p)=\sum_ {i=0}^{n-1}U_ {i,p_ {i}}$ だけになります。線形割当はHungarian法などの厳密な方法で解けます。方法そのものの説明はここでは省略します。[SciPyの線形割当API](https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.linear_sum_assignment.html)や[OR-Toolsの線形割当ソルバー](https://developers.google.com/optimization/assignment/linear_assignment)が候補です。

目的式を整理すると、次のケースも線形割当に直せます。これは本書の式からの帰結です。

| 入力の形 | 整理した結果 |
| --- | --- |
| `distance` がすべて0 | 単項費用だけになる |
| `flow` または `distance` の非対角成分がすべて0 | $U_ {i,x}+F_ {i,i}D_ {x,x}$ を新しい単項費用にする |
| `flow` の全要素が同じ定数 | 二次項が全位置ペアの総和の定数倍になり、配置によらない |
| `distance` の全要素が同じ定数 | 二次項が全物体ペアの総和の定数倍になり、配置によらない |

本ヘッダーは、これらすべてを判定して厳密ソルバーへ切り替えるAPIではありません。v09の単項向け処理も、厳密解を必ず返す契約にはなっていません。対称、疎、非負という条件だけでは一般QAPの厳密解が容易になるとは限りません。

### 2.2 類似のソフトウェア

| 選択肢 | 検討したい場面 | 本ライブラリとの使い分け |
| --- | --- | --- |
| 本ライブラリ | C++へ1ヘッダーで組み込み、整数費用、単項費用、固定配置の部分改善を扱いたい | deadlineを指定して配置を探す。一般の最適性証明はしない |
| [SciPy `quadratic_assignment`](https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.quadratic_assignment.html) | PythonでQAPやグラフ対応を試す | FAQと2-optの近似法がある。部分的な対応固定にも対応。ここで使う任意の `unary` 行列をそのまま渡す同名引数はない |
| SciPy / OR-Toolsの線形割当 | 費用が各物体の配置先だけで決まる | より限定された問題を厳密に解く。一般の二次相互作用をそのまま渡す用途ではない |
| [OR-Tools CP-SAT](https://developers.google.com/optimization/cp/cp_solver)などの整数・制約ソルバー | 配置禁止や重さ別の容量を明示し、制約充足や最適性を確認したい | 別のモデル化が必要。QAPの行列を渡すだけの置換品ではない。時間制限内に最適性まで確認できるとは限らない |

OR-Toolsには[仕事の大きさと容量を含む割当例](https://developers.google.com/optimization/assignment/assignment_cp)があります。CP-SATの `OPTIMAL` は最適解の確認、`FEASIBLE` は実行可能解を見つけた段階、`INFEASIBLE` は不可能性の証明、`UNKNOWN` は時間切れなどで判断が付かない状態です。この区別が必要なら、証明・状態を返すソルバーを比較します。

上の使い分けはAPIと保証の違いによるものです。本ライブラリが各ソルバーより常に速い、または常に良い解を返す、という性能比較はしていません。外部APIの記述は2026-10-11に公式資料で確認しました。

## 3. 最小コード：3台の設備を配置する

### 3.1 実行準備

`qap_solver_v09.hpp` を、作成する `main.cpp` と同じディレクトリに置きます。例01のコード全体を `main.cpp` として保存し、そのディレクトリで次を実行します。

```bash
g++ -std=c++20 -O2 -Wall -Wextra main.cpp -o qap_example
./qap_example
```

`g++` はC++コンパイラです。`-std=c++20` は言語規格、`-O2` は最適化、`-Wall -Wextra` は警告の指定、`-o qap_example` は実行ファイル名です。C++23環境では `-std=c++23` に置き換えられます。

本ヘッダーはGCCの `bits/stdc++.h`、128bit整数、インラインasmなどを利用します。外部ライブラリの導入は不要ですが、任意のC++コンパイラでそのまま使えるという意味ではありません。本書の検証はGCC 13.3.0で行いました。全例をC++20・C++23の両方で検証し、GCC 15.2自体での実行確認はしていません。

説明を短く保つため、各例ではヘッダーが取り込む標準ライブラリの宣言も利用します。別の補助ヘッダーや、入力データのダウンロードは必要ありません。

### 3.2 例01：最小の一通り

まず単項費用を使わず、運搬費だけを小さくします。設備間の運ぶ量を `flow`、場所間の距離を `distance` に入れます。

保存する例: `01_minimal.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(3);
    problem.flow = {
        0, 8, 1,
        8, 0, 2,
        1, 2, 0
    };
    problem.distance = {
        0, 1, 3,
        1, 0, 2,
        3, 2, 0
    };

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    auto result = solve_qap(problem, deadline);

    std::cout << "cost = " << result.cost << '\n';
    for (int i = 0; i < problem.n; ++i) {
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

### 3.3 入力の作り方を1行ずつ読む

`qap_problem<> problem(3)` は3物体・3位置の問題を作ります。`<>` は型の既定値を使うという意味です。省略された型は `qap_problem<int, long long>` で、行列 `flow` と `distance` は `int`、費用とその合計は `long long` になります。`long long` を使うのは、行列の値を掛けて何度も足すと大きくなるためです。

コンストラクタの `3` が `problem.n` になります。`flow` と `distance` は初めに9要素の0、`unary` は空になります。その後の代入で前者の2表を設定します。

| `flow` の行＼列 | 設備0へ | 設備1へ | 設備2へ |
| --- | --- | --- | --- |
| 設備0から | 0 | 8 | 1 |
| 設備1から | 8 | 0 | 2 |
| 設備2から | 1 | 2 | 0 |

0–1間の量8が最も大きいので、特に近づけたい関係です。0–2間の量は1、1–2間は2です。対角の0は自分自身への運搬を数えない指定です。逆方向も同じ値を明示しています。

`distance` の位置0、1、2は、それぞれ直線上の座標0、1、3に対応します。位置0–1は距離1、0–2は3、1–2は2です。ライブラリが座標から計算したのではなく、コード側で距離を表にして渡しています。

実際の格納先は1次元の `std::vector` です。横1行を並べ、次に次の行を並べます。行 $i$、列 $j$ の要素は `i * n + j` 番目です。例えば `flow[1 * 3 + 2]` が設備1から設備2への値2です。

### 3.4 呼出しの全引数と返却値

| コードの要素 | 意味 |
| --- | --- |
| `std::chrono::steady_clock::now()` | 経過時間の測定に使う時計の現在時刻 |
| `milliseconds(50)` | 50ミリ秒。この教材で短く動かすための時間設定 |
| `deadline` | 現在時刻に50msを足した「終了を目指す絶対時刻」 |
| `solve_qap` の第1引数 `problem` | 設定し終えた問題 |
| 第2引数 `deadline` | 探索の終了目安。ミリ秒数そのものを渡すのではない |
| 省略した第3引数 `seed` | 乱数の開始値。既定値は1。例12で明示する |
| `result.location_of[i]` | 物体 `i` の位置番号 |
| `result.cost` | 同じ配置に対する目的関数 $C$ の値 |

`auto` は戻り値の型をコンパイラに決めてもらう書き方です。この例の戻り値は `qap_result<long long>` です。最後のループは各物体の位置を表示します。位置番号は座標そのものではありません。結果を短く確認したいときは `std::cerr << result << '\n';` と書くこともできます。例24で、配列の表示上限も含めて説明します。

例えば `cost = 30`、配置が `0,1,2` と表示された場合、1.3の手計算と一致します。探索の出力は一般には時間配分や実行環境で変わります。**表示例と同じ配置が出ることをAPIの保証として使わないでください。**

この50msは問題の構築後からの目安です。準備、返却、出力までを厳密に50ms以内へ収める指定ではありません。システム全体の制限がある場合は、準備の前に全体締切を作り、出力などの余裕も残します。

## 4. 費用の意味を広げる

### 4.1 例02：配置の検査・全評価・交換差分

変更点は、solverに任せる前に、自分で用意した配置の費用を調べることです。例01と同じ入力で、設備0と1を交換したときの差を確認します。

保存する例: `02_evaluate_swap.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(3);
    problem.flow = {
        0, 8, 1,
        8, 0, 2,
        1, 2, 0
    };
    problem.distance = {
        0, 1, 3,
        1, 0, 2,
        3, 2, 0
    };

    std::vector<int> p{0, 1, 2};
    assert(problem.is_valid_permutation(p));
    assert(!problem.is_valid_permutation({0, 0, 2}));
    const long long before = problem.evaluate(p);
    const long long delta = problem.swap_delta(p, 0, 1);
    std::swap(p[0], p[1]);
    const long long after = problem.evaluate(p);
    assert(before == 30);
    assert(delta == 2);
    assert(after == before + delta);
    std::cout << before << " + " << delta << " = " << after << '\n';
}
```

`p` は `std::vector<int>`、つまり整数の並びです。`{0,1,2}` は設備0を位置0、設備1を位置1、設備2を位置2に置きます。`is_valid_permutation(p)` は長さ、範囲、重複を調べ、正しければ `true` を返します。追加した禁止条件を調べる関数ではありません。

| 新しい呼出し・値 | 説明 |
| --- | --- |
| `assert(条件)` | 条件が偽ならプログラムを停止する開発時の検査。本書では期待する性質を示すために使う |
| `!条件` | 真偽を反転する。重複配置が無効であることを検査している |
| `problem.evaluate(p)` | 配置 `p` の目的値を初めから足し直す。配置は変更しない |
| `problem.swap_delta(p, 0, 1)` | 物体0と物体1の位置を交換した場合の「交換後−交換前」を返す。まだ交換しない |
| `std::swap(p[0], p[1])` | 実際に2物体の配置先を交換する |

出力は `30 + 2 = 32` です。差分が正なので、この交換は費用を2悪化させます。負なら改善、0なら同点です。位置0と位置1にいる物体を指定するAPIではなく、**物体の番号**を渡します。

以後は返却配置が置換であることと、返却costが全評価と等しいことを `assert` で確かめます。`-DNDEBUG` を付けるとassertは無効になります。本書の検証では有効のまま実行します。

### 4.2 例03：移設費と場所の相性を追加する

変更点は `unary` の追加です。元の場所から動かすと1台あたり5かかり、さらに設備2を位置0に置くと追加費用4がかかる、とします。二次費用は例01のままです。

保存する例: `03_unary.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(3);
    problem.flow = {
        0, 8, 1,
        8, 0, 2,
        1, 2, 0
    };
    problem.distance = {
        0, 1, 3,
        1, 0, 2,
        3, 2, 0
    };

    const std::vector<int> previous{0, 1, 2};
    const long long move_cost = 5;
    problem.unary.assign(problem.n * problem.n, 0);
    for (int i = 0; i < problem.n; ++i) {
        for (int x = 0; x < problem.n; ++x) {
            if (x != previous[i]) problem.unary[i * problem.n + x] += move_cost;
        }
    }
    problem.unary[2 * problem.n + 0] += 4;
    assert(problem.evaluate(previous) == 30);

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    auto result = solve_qap(problem, deadline);

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    std::cout << "cost = " << result.cost << '\n';
    for (int i = 0; i < problem.n; ++i) {
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

まず `unary.assign(n*n, 0)` で全9要素を0にします。`assign` の第1引数は要素数、第2引数は初期値です。単項費用を1か所だけ設定したい場合も、表全体を用意します。

| 追加した値・式 | 説明 |
| --- | --- |
| `previous{0,1,2}` | 移設前の位置。設備ごとに1個ずつ指定 |
| `move_cost = 5` | 場所を変えた設備1台に課す費用。距離比例ではなく定額 |
| `x != previous[i]` | 設備 `i` の新しい候補位置 `x` が以前と違うか |
| `unary[i*n+x] += move_cost` | 候補位置ごとに移設費を加える |
| `unary[2*n+0] += 4` | 設備2・位置0という組の相性による追加費用 |

`previous` は費用を作るための入力であり、この例ではsolverの初期配置には渡していません。初期配置も指定する方法は例12で学びます。

この入力では設備2を位置0へ移す単項費用は5+4=9です。`+=` にしているのは費用を合計するためです。移設費を大きくすると、輸送費を減らすために動かす価値が小さくなります。両方を同じ費用単位へ換算して決めます。対称入力では二次費用が両方向分あることも忘れないでください。

### 4.3 例04：方向、自己費用、負の値

例01の運搬表の一部を変更します。往路と復路で異なる費用、設備自身に由来する費用、設置報酬を一度に表せることを確認する例です。変更するのは入力表だけです。

保存する例: `04_directed_diagonal.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(3);
    problem.flow = {
        0, 8, 1,
        8, 0, 2,
        1, 2, 0
    };
    problem.distance = {
        0, 1, 3,
        1, 0, 2,
        3, 2, 0
    };

    problem.flow[0 * 3 + 1] = 9;
    problem.flow[1 * 3 + 0] = 2;
    problem.distance[0 * 3 + 1] = 4;
    problem.distance[1 * 3 + 0] = 1;
    problem.flow[2 * 3 + 2] = 3;
    problem.distance[0 * 3 + 0] = 2;
    problem.distance[1 * 3 + 1] = 1;
    problem.distance[2 * 3 + 2] = 4;
    problem.unary.assign(3 * 3, 0);
    problem.unary[2 * 3 + 0] = -5;
    assert(problem.evaluate({0, 1, 2}) == 64);

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    auto result = solve_qap(problem, deadline);

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    std::cout << "cost = " << result.cost << '\n';
    for (int i = 0; i < problem.n; ++i) {
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

| 変更要素 | 新しい意味 |
| --- | --- |
| `flow[0*3+1]=9`、`flow[1*3+0]=2` | 設備0→1は量9、1→0は量2 |
| `distance[0*3+1]=4`、`distance[1*3+0]=1` | 位置0→1は単価4、1→0は単価1 |
| `flow[2*3+2]=3` | 設備2自身について、配置先の対角係数を3倍して加える |
| `distance` の対角 `2,1,4` | 各位置に置く設備の自己係数へ掛ける値 |
| `unary[2*3+0]=-5` | 設備2を位置0へ置くと費用が5減る、つまり報酬5 |

`3` はこの表の列数です。他の未変更要素は例01と同じです。初期配置 `{0,1,2}` の対角以外の二次費用は52、対角からの費用は3×4=12、単項費用は0なので全費用は64です。負の値を使っても「小さいほどよい」という比較は変わりません。

### 4.4 例05：合計報酬を最大化する

最小化APIしかなくても、報酬の符号を反転すれば最大化できます。例01の `distance` の数値を、この節では位置ペアの報酬係数と読み替えます。

保存する例: `05_maximize.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(3);
    problem.flow = {
        0, 8, 1,
        8, 0, 2,
        1, 2, 0
    };
    problem.distance = {
        0, 1, 3,
        1, 0, 2,
        3, 2, 0
    };

    // この例では distance の数値を、距離ではなく位置ペアの報酬係数と読む。
    const auto reward_problem = problem;
    for (int& value : problem.flow) value = -value;
    for (long long& value : problem.unary) value = -value;

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    auto result = solve_qap(problem, deadline);

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    const long long reward = reward_problem.evaluate(result.location_of);
    assert(reward == -result.cost);
    std::cout << "reward = " << reward << '\n';
}
```

`reward_problem` は符号反転前の問題のコピーです。`flow` を負にし、単項報酬を使う場合は `unary` も負にします。この例のunaryは空なので、そのループでは何も起こりません。`distance` は変更しません。両方の行列を負にすると積が元に戻ってしまいます。

元の報酬を $R(p)$ とすると、solverが最小化する値は $-R(p)$ です。出力の報酬は `-result.cost`、またはコピーした元問題での全評価です。負号を付ける前の型が、その値を保持できる必要があります。整数の最小値の符号反転は同じ型に収まらないため、例22の型選択も参照してください。

## 5. 入力表を工夫して別の用途へ広げる

### 5.1 例06：空き位置を選ぶ

例01の設備は3台のままで、位置を4か所へ増やします。ライブラリには物体数と位置数を別々に渡せないため、「費用に関係しない架空の設備」を1台足します。これを**ダミー物体**と呼びます。

保存する例: `06_dummy.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    const int real_objects = 3;
    const int slots = 4;
    qap_problem<> problem(slots);
    const std::vector<int> real_flow{0, 8, 1, 8, 0, 2, 1, 2, 0};
    for (int i = 0; i < real_objects; ++i) {
        for (int j = 0; j < real_objects; ++j) {
            problem.flow[i * slots + j] = real_flow[i * real_objects + j];
        }
    }
    const std::vector<int> coordinate{0, 1, 3, 4};
    for (int x = 0; x < slots; ++x) {
        for (int y = 0; y < slots; ++y) {
            problem.distance[x * slots + y] = std::abs(coordinate[x] - coordinate[y]);
        }
    }

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    auto result = solve_qap(problem, deadline);

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    long long direct = 0;
    for (int i = 0; i < real_objects; ++i) {
        for (int j = 0; j < real_objects; ++j) {
            direct += 1LL * real_flow[i * real_objects + j]
                    * std::abs(coordinate[result.location_of[i]]
                             - coordinate[result.location_of[j]]);
        }
    }
    assert(direct == result.cost);
    std::cout << "empty location = " << result.location_of[3] << '\n';

    std::cout << "cost = " << result.cost << '\n';
    for (int i = 0; i < problem.n; ++i) {
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

| 新しい値 | 意味 |
| --- | --- |
| `real_objects=3` | 実在する設備は番号0〜2 |
| `slots=4` | 全位置数であり、ダミーを含む全物体数 |
| `real_flow` | 例01と同じ3×3の実設備間の表 |
| `coordinate{0,1,3,4}` | 位置番号順の直線上の座標 |
| 物体3 | ダミー。そこが置かれた位置を空き位置として読む |

入力側の列幅は3、コピー先の列幅は4です。そのため `real_flow[i*real_objects+j]` を `flow[i*slots+j]` へ入れます。ダミーに出入りするflowは初期値0のままです。unaryもないので、ダミー自身の費用は0です。

`std::abs` は絶対値です。2座標の差の絶対値で距離を作ります。最後の `direct` は実設備だけで費用を再計算し、ダミーを含むQAP費用と等しいことを確認します。実設備だけの出力は0〜3の重複なし配置であり、0〜2の置換になるとは限りません。

ダミーは「許される位置が多いだけの普通の設備」ではありません。関係費用を0にして初めて、空きを表す費用のない物体になります。

### 5.2 例07：疎な関係をグリッドへ置く

例06の位置を直線から2行3列のマスへ変えます。実物体を4個にし、関係を3本の辺の一覧から入力します。残る2個はダミーです。

保存する例: `07_grid_sparse.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    const int height = 2;
    const int width = 3;
    const int real_objects = 4;
    const int slots = height * width;
    qap_problem<> problem(slots);
    const std::vector<std::tuple<int, int, int>> edges{
        {0, 1, 8}, {1, 2, 2}, {2, 3, 6}
    };
    for (auto [i, j, weight] : edges) {
        problem.flow[i * slots + j] += weight;
        problem.flow[j * slots + i] += weight;
    }
    for (int x = 0; x < slots; ++x) {
        for (int y = 0; y < slots; ++y) {
            problem.distance[x * slots + y] =
                std::abs(x / width - y / width) + std::abs(x % width - y % width);
        }
    }

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    auto result = solve_qap(problem, deadline);

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    long long undirected_cost = 0;
    for (auto [i, j, weight] : edges) {
        undirected_cost += 1LL * weight
            * problem.distance[result.location_of[i] * slots + result.location_of[j]];
    }
    assert(result.cost == 2 * undirected_cost);
    for (int i = 0; i < real_objects; ++i) {
        const int x = result.location_of[i];
        std::cout << i << ": (" << x / width << ", " << x % width << ")\n";
    }
}
```

| 追加・変更した値 | 説明 |
| --- | --- |
| `height=2`, `width=3` | 行数と列数。マスは6個 |
| `real_objects=4` | 実物体は0〜3、ダミーは4〜5 |
| `edges` の `{i,j,weight}` | 無向の関係1本。物体番号2個と重み |
| `{0,1,8}`, `{1,2,2}`, `{2,3,6}` | それぞれの物体間の関係量。これ以外は0 |
| `x/width`, `x%width` | マス番号 `x` の行と列。`/` は整数商、`%` は余り |

マス番号は上の行が0,1,2、下の行が3,4,5です。縦の差と横の差の絶対値を足す距離を**マンハッタン距離**と呼びます。斜め移動を使わず縦横だけで移動する距離です。

`auto [i,j,weight]` は3つ組の内容に名前を付けるC++の書き方です。各辺を両方向へ加えるので、結果costは辺を1回ずつ数えた `undirected_cost` の2倍になります。`+=` なら重複して入力した辺の重みは足し合わされます。

大部分のflowが0の表を**疎な表**と呼びます。別の疎行列APIは不要です。ただし公開入力は引き続き6×6の表であり、一般にメモリは全位置数の二乗に比例します。障害物や通行不可のマスを、この距離式が自動で考慮することはありません。

### 5.3 例08：各2人のグループを作る

ここからは4物体の例です。物体を人、位置をグループ内の席と読み替えます。4人を2人ずつの2組へ分け、強い関係が別グループへ分かれる費用を小さくします。

保存する例: `08_groups.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(4);
    problem.flow = {
        0, 8, 1, 0,
        8, 0, 2, 1,
        1, 2, 0, 6,
        0, 1, 6, 0
    };
    problem.distance = {
        0, 1, 3, 4,
        1, 0, 2, 3,
        3, 2, 0, 1,
        4, 3, 1, 0
    };

    const std::vector<int> group_of_slot{0, 0, 1, 1};
    for (int x = 0; x < problem.n; ++x) {
        for (int y = 0; y < problem.n; ++y) {
            problem.distance[x * problem.n + y] =
                group_of_slot[x] == group_of_slot[y] ? 0 : 1;
        }
    }

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    auto result = solve_qap(problem, deadline);

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    std::vector<int> count(2, 0);
    for (int i = 0; i < problem.n; ++i) {
        const int group = group_of_slot[result.location_of[i]];
        ++count[group];
        std::cout << "person " << i << " -> group " << group << '\n';
    }
    assert(count[0] == 2 && count[1] == 2);
}
```

4人のflowは、0–1の重み8、0–2は1、0–3は0、1–2は2、1–3は1、2–3は6です。両方向へ同じ値を入れています。コード冒頭の元の距離表は、後のループですべて置き換わります。

`group_of_slot{0,0,1,1}` は席0,1がグループ0、席2,3がグループ1という指定です。同じグループならdistanceを0、異なれば1にします。`条件 ? 0 : 1` は条件が真なら0、偽なら1を選びます。

各席にちょうど1人が入るので、両グループの人数2は必ず守られます。`count(2,0)` は2グループの人数を0から数える配列です。グループ内の席の違いに意味はなく、結果は `group_of_slot[result.location_of[i]]` で読みます。

人数を不均一にするなら、位置→グループの表で各グループの出現数を変えられます。ただし人ごとに重さが異なる容量制約とは別です。1人で2席を消費するような一般の制約を、この方法だけで保証することはできません。

### 5.4 例09：巡回順を決める

例08で使った4×4のflowを、都市から都市への移動費として使います。位置は座標ではなく、巡回する順番0〜3です。distanceの作り方と、出力の読み方を変えます。

保存する例: `09_cycle.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(4);
    problem.flow = {
        0, 8, 1, 0,
        8, 0, 2, 1,
        1, 2, 0, 6,
        0, 1, 6, 0
    };
    problem.distance = {
        0, 1, 3, 4,
        1, 0, 2, 3,
        3, 2, 0, 1,
        4, 3, 1, 0
    };

    std::fill(problem.distance.begin(), problem.distance.end(), 0);
    for (int x = 0; x < problem.n; ++x) {
        problem.distance[x * problem.n + (x + 1) % problem.n] = 1;
    }

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    auto result = solve_qap(problem, deadline);

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    std::vector<int> city_at(problem.n);
    for (int city = 0; city < problem.n; ++city) {
        city_at[result.location_of[city]] = city;
    }
    long long route_cost = 0;
    for (int x = 0; x < problem.n; ++x) {
        const int from = city_at[x];
        const int to = city_at[(x + 1) % problem.n];
        route_cost += problem.flow[from * problem.n + to];
        std::cout << from << ' ';
    }
    assert(route_cost == result.cost);
    std::cout << city_at[0] << "  cost = " << route_cost << '\n';
}
```

`std::fill(begin,end,0)` で表をすべて0にし、順番 `x` から次の順番 `(x+1)%n` にだけ1を入れます。最後の順番3の次は0です。このdistanceによって、巡回で隣り合う物体のflowだけが1回ずつ足されます。対称なflowでも、この例の巡回費用を2で割る必要はありません。

`result.location_of[city]` は都市の訪問順番です。表示したい「順番→都市」は逆対応の `city_at` に入れ直します。最後に先頭都市をもう一度表示し、戻る辺を含む費用を再計算します。

この入力には都市0–3間の費用0も含まれます。0は通行禁止ではなく、費用0で通れるという意味です。純粋な巡回問題なら、2章の専用手法も比較してください。

### 5.5 例10：候補の選択と配置を同時に行う

例06は位置が余る場合でした。今度は4候補から2個を選び、2つの実位置へ置きます。使わない候補の行き先として、ダミー**位置**を2つ足します。

保存する例: `10_selection.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    const int candidates = 4;
    const int real_slots = 2;
    qap_problem<> problem(candidates);
    problem.flow = {0, 3, 2, 4, 3, 0, 1, 2, 2, 1, 0, 5, 4, 2, 5, 0};
    problem.distance[0 * candidates + 1] = 1;
    problem.distance[1 * candidates + 0] = 1;
    const std::vector<long long> reward{12, 9, 10, 8};
    problem.unary.assign(candidates * candidates, 0);
    for (int i = 0; i < candidates; ++i) {
        for (int x = 0; x < real_slots; ++x) {
            problem.unary[i * candidates + x] = -reward[i];
        }
    }

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    auto result = solve_qap(problem, deadline);

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    std::vector<int> selected;
    long long selected_reward = 0;
    for (int i = 0; i < candidates; ++i) {
        if (result.location_of[i] < real_slots) {
            selected.push_back(i);
            selected_reward += reward[i];
            std::cout << "selected = " << i << '\n';
        }
    }
    assert(selected.size() == 2);
    const int a = selected[0], b = selected[1];
    assert(result.cost == problem.flow[a * candidates + b]
                        + problem.flow[b * candidates + a] - selected_reward);
    std::cout << "net reward = " << -result.cost << '\n';
}
```

| 値 | 意味 |
| --- | --- |
| `candidates=4` | 物体は候補0〜3。今回はダミー物体を作らない |
| `real_slots=2` | 位置0,1が実位置、2,3は不採用を表す位置 |
| `reward{12,9,10,8}` | 各候補を採用したときの報酬 |
| `distance[0,1]=distance[1,0]=1` | 採用された候補同士の相互費用を数える |
| その他のdistance=0 | 不採用候補が関わる相互費用を数えない |
| 実位置のunary=`-reward[i]` | 採用報酬を最小化費用へ変換する |

flowの各行は順に `{0,3,2,4}`、`{3,0,1,2}`、`{2,1,0,5}`、`{4,2,5,0}` です。数値は採用した2候補間に生じる費用の重みで、負号を付けていません。最小化するのは「相互費用−採用報酬」です。

実位置は2つとも使われるので、採用数は**ちょうど2個**です。「最大2個」ではありません。`selected` は実位置を割り当てられた候補番号を保存します。ダミー位置にunaryを与えれば不採用費用も表現できますが、ここでは0です。

### 5.6 例11：2つのグラフの頂点を対応させる

例05の符号反転をもう一歩広げます。グラフAの物体番号をグラフBの頂点番号へ対応させ、対応後の辺重みの差の二乗和を小さくします。グラフとは頂点と頂点間の関係をまとめたものです。

保存する例: `11_graph_matching.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    const int n = 3;
    const std::vector<int> a{0, 8, 1, 8, 0, 2, 1, 2, 0};
    const std::vector<int> b{0, 1, 2, 1, 0, 8, 2, 8, 0};
    qap_problem<> problem(n);
    long long constant = 0;
    for (int t = 0; t < n * n; ++t) {
        problem.flow[t] = -2 * a[t];
        problem.distance[t] = b[t];
        constant += 1LL * a[t] * a[t] + 1LL * b[t] * b[t];
    }

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    auto result = solve_qap(problem, deadline);

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    long long squared_error = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            const long long difference = a[i * n + j]
                - b[result.location_of[i] * n + result.location_of[j]];
            squared_error += difference * difference;
        }
    }
    assert(squared_error == constant + result.cost);
    std::cout << "squared error = " << squared_error << '\n';
}
```

`a` と `b` は3×3の辺重み表で、行と列は頂点番号です。Aは例01のflow、Bは同じ重み8,1,2の頂点番号を並べ替えた表です。求める値は次の式です。

$$E(p)=\sum_ {i=0}^{n-1}\sum_ {j=0}^{n-1}(A_ {i,j}-B_ {p_ {i},p_ {j}})^2$$

$A_ {i,j}$ はAの辺、$B_ {p_ {i},p_ {j}}$ は対応先のBの辺、$E$ は差の二乗の合計です。二乗は負の差も正にし、大きい違いを強く数えます。

高校数学の $(a-b)^2=a^2+b^2-2ab$ で展開すると、$A^2$ の総和と $B^2$ の総和は対応を変えても一定です。Bの全頂点を1回ずつ使うため、全辺の総和も変わりません。残る積に合わせて `flow=-2*A`、`distance=B` とします。

`constant` がこの一定部分で、目的の誤差は `constant + result.cost` です。`t` は1次元表の通し番号、`difference` は対応した辺重みの差です。`1LL` は掛け算をlong longで行うための指定です。最後のループで元の誤差を直接計算し、変換が合っていることを検査します。

対応誤差が0なら表の重みが一致しています。一方、近似探索で0が見つからなかったことだけで、完全一致する対応が存在しないとは結論できません。

## 6. 解を引き継ぐ・一部だけ直す

### 6.1 例12：既存の配置から改善する

ここでは例01の3物体問題へ戻ります。違いは `solve_qap` を `improve_qap` に替え、完全な初期配置を渡すことです。

保存する例: `12_improve.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(3);
    problem.flow = {
        0, 8, 1,
        8, 0, 2,
        1, 2, 0
    };
    problem.distance = {
        0, 1, 3,
        1, 0, 2,
        3, 2, 0
    };

    const std::vector<int> initial{0, 2, 1};
    assert(problem.is_valid_permutation(initial));
    const long long initial_cost = problem.evaluate(initial);
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    const std::uint64_t seed = 42;
    auto result = improve_qap(problem, initial, deadline, seed);
    assert(result.cost <= initial_cost);

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    std::cout << "cost = " << result.cost << '\n';
    for (int i = 0; i < problem.n; ++i) {
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

| 引数・変数 | 意味 |
| --- | --- |
| `initial{0,2,1}` | 物体0→位置0、1→2、2→1の完全配置。途中までの配置は渡せない |
| 第1引数 `problem` | 改善対象の問題 |
| 第2引数 `initial` | 出発点であり、返却候補として残る配置 |
| 第3引数 `deadline` | この例では呼出し準備後の現在時刻+50ms |
| 第4引数 `seed=42` | 乱数系列の開始値。品質を表す点数ではない |

同じ問題に対して、返却costは初期配置のcost以下です。良い変更が見つからなければ同じ費用の解が返ります。これは正しい入力と整数演算の範囲内での契約で、任意の追加評価についての保証ではありません。

seedの型 `std::uint64_t` は64bitの符号なし整数です。0も指定できますが、毎回ランダムなseedにする指定ではありません。同じseedでも、実時間の打切り位置により最終結果は変わり得ます。

2回目に改善を続けるなら、返された配置を次の `initial` にします。ただし内部の探索履歴までそのまま続くわけではありません。

### 6.2 例13：同じ問題の前処理を再利用する

例12を複数試行へ拡張します。問題が変わらない間、`qap_prepared` に準備結果を保持させます。新しい候補を複数作って比較し、最後に最良候補を改善します。

保存する例: `13_prepared.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    std::vector<int> flow{0, 8, 1, 8, 0, 2, 1, 2, 0};
    std::vector<int> distance{0, 1, 3, 1, 0, 2, 3, 2, 0};
    qap_problem<> problem(3, std::move(flow), std::move(distance));
    qap_prepared<> prepared(problem);
    const std::vector<int> initial{0, 2, 1};
    assert(&prepared.problem() == &problem);
    assert(prepared.swap_delta(initial, 0, 1) == problem.swap_delta(initial, 0, 1));
    qap_result<> best{initial, prepared.evaluate(initial)};
    using Clock = std::chrono::steady_clock;
    const auto final_deadline = Clock::now() + std::chrono::milliseconds(150);
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        if (Clock::now() >= final_deadline) break;
        const auto deadline = std::min(final_deadline,
            Clock::now() + std::chrono::milliseconds(40));
        auto candidate = prepared.solve(deadline, seed);
        if (candidate.cost < best.cost) best = std::move(candidate);
    }
    auto result = prepared.improve(best.location_of, final_deadline, 99);
    assert(result.cost <= best.cost);

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    std::cout << "cost = " << result.cost << '\n';
    for (int i = 0; i < problem.n; ++i) {
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

今回は既にある行列から問題を構築しています。コンストラクタの引数は順に、個数 `3`、flow、distanceです。4番目のunaryを省略すると空になります。`std::move` はvectorの内容を問題へ渡すために使います。渡した後の元vectorの中身は使いません。moveしなければ行列がコピーされます。

| 新しいAPI・値 | 説明 |
| --- | --- |
| `qap_prepared<> prepared(problem)` | 完成済みの問題を参照し、再利用する準備を行う |
| `prepared.problem()` | 参照先の問題を返す。`&` 同士の比較で同じ実体と確認している |
| `prepared.evaluate(initial)` | 同じ問題で全評価する。費用を記憶して使い回す機能ではない |
| `prepared.swap_delta(initial,0,1)` | 前処理を使う交換差分。引数は完全配置、交換する物体2個 |
| `qap_result<> best{initial,cost}` | 配置と正しいcostを組にして保存する |
| `prepared.solve(deadline,seed)` | 新しい初期配置を内部で作り探索する |
| `prepared.improve(best.location_of,final_deadline,99)` | 保持した最良配置を初期解として探索する |

`using Clock = ...` は長い時計の型名に短い別名を付けます。`final_deadline` は150ms後の全体目安、各試行の目安は40ms後と全体目安の早い方です。`std::min` が早い方を選びます。seedは1,2,3の3回、最後の改善は99です。150,40,3,99は教材側の設定であり、ライブラリの内部パラメータではありません。

各試行の候補が `best` より厳密に小さい場合だけ保存します。候補同士に非悪化関係はないため、この比較が必要です。複数回へ時間を分けることが、1回の長い探索より常に良いとは限りません。

preparedはproblemをコピーして所有しません。**problemはpreparedの利用が終わるまで生存させ、内容を変更しない**でください。前処理再利用は同じ行列に対してだけ有効です。`prepared.swap_delta` へ渡す配置の妥当性も呼出し側の前提で、毎回全体を検査するAPIではありません。

### 6.3 例14：動かしてよい物体を限定する

例08などで使った4物体の問題で、物体0と3を固定し、1と2だけ動かします。初期配置を受け取る点は例12と同じですが、可動物体の一覧を追加します。

保存する例: `14_subset.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(4);
    problem.flow = {
        0, 8, 1, 0,
        8, 0, 2, 1,
        1, 2, 0, 6,
        0, 1, 6, 0
    };
    problem.distance = {
        0, 1, 3, 4,
        1, 0, 2, 3,
        3, 2, 0, 1,
        4, 3, 1, 0
    };

    const std::vector<int> initial{0, 2, 1, 3};
    const std::vector<int> movable_objects{1, 2};
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    auto result = improve_qap_subset(problem, initial, movable_objects, deadline, 42);
    assert(result.location_of[0] == initial[0]);
    assert(result.location_of[3] == initial[3]);
    assert(result.cost <= problem.evaluate(initial));

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    std::cout << "cost = " << result.cost << '\n';
    for (int i = 0; i < problem.n; ++i) {
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

ここでのdistanceは座標0,1,3,4の直線距離です。例08のグループ距離へは置き換えていません。初期配置 `{0,2,1,3}` は固定物体0→位置0、3→位置3を満たしています。

`improve_qap_subset` の引数は、問題、完全初期配置、可動物体一覧、締切、seedの順です。`movable_objects{1,2}` は**物体番号**です。重複や範囲外番号を含めません。物体1と2が現在使う位置2と1の中だけで入れ替えます。

固定したい場所を引数に指定するのではなく、初期配置で固定を満たしておき、その物体を可動一覧から外します。別の位置へ移動したいなら、その位置を占有する物体も可動一覧に含める必要があります。ダミー物体も同じです。

固定物体との相互費用も考慮されます。返却costは可動部分だけの費用ではなく、全体の正しい費用です。この例ではcostを事前に渡していないため、開始時に全体を評価します。

### 6.4 例15：部分問題を取り出して厳密に解く

例14の「一部だけ動かす」を2段階に分けます。最初に小さい問題を作り、次にその小さい問題の全配置を試して、全体へ戻します。可動物体は3個なので6通りです。

保存する例: `15_subproblem_exact.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(4);
    problem.flow = {
        0, 8, 1, 0,
        8, 0, 2, 1,
        1, 2, 0, 6,
        0, 1, 6, 0
    };
    problem.distance = {
        0, 1, 3, 4,
        1, 0, 2, 3,
        3, 2, 0, 1,
        4, 3, 1, 0
    };

    const std::vector<int> initial{0, 2, 1, 3};
    const std::vector<int> movable_objects{2, 1, 3};
    auto sub = make_qap_subproblem(problem, initial, movable_objects);
    auto known_sub = make_qap_subproblem(
        problem, initial, movable_objects, problem.evaluate(initial));
    assert(sub.constant_offset == known_sub.constant_offset);
    assert(sub.base_location_of == initial);
    std::vector<int> q = sub.initial_location_of;
    qap_result<> best{q, sub.problem.evaluate(q)};
    int examined = 0;
    do {
        const long long cost = sub.problem.evaluate(q);
        if (cost < best.cost) best = {q, cost};
        const auto restored = sub.restore(qap_result<>{q, cost});
        assert(problem.evaluate(restored.location_of) == sub.constant_offset + cost);
        ++examined;
    } while (std::next_permutation(q.begin(), q.end()));
    auto result = sub.restore(best);
    assert(examined == 6);
    assert(result.location_of[0] == initial[0]);
    for (int a = 0; a < sub.problem.n; ++a) {
        assert(result.location_of[sub.object[a]] == sub.location[best.location_of[a]]);
    }
    assert(result.cost <= problem.evaluate(initial));

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    std::cout << "cost = " << result.cost << '\n';
    for (int i = 0; i < problem.n; ++i) {
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

`make_qap_subproblem(problem,initial,movable_objects)` は小さいQAPを作るだけで、探索はしません。第4引数に正しい元問題の初期costを渡す形もあります。例では両形式の対応を確認しています。その場で `evaluate` を呼ぶと計算の節約にはならず、既に費用を持っているときに第4引数が有効です。

可動一覧 `{2,1,3}` は渡した順序のまま使われます。

| 縮約番号 | `sub.object`：元の物体 | `sub.location`：使える元の位置 |
| --- | --- | --- |
| 0 | 2 | `initial[2]` = 1 |
| 1 | 1 | `initial[1]` = 2 |
| 2 | 3 | `initial[3]` = 3 |

小さい問題の物体0は元の物体2です。その物体を小さい位置1へ置けば、元の位置2へ置いたことになります。この対応を管理する値が次のメンバーです。

| メンバー | 意味 |
| --- | --- |
| `sub.problem` | 可動3物体・3位置の新しい問題。固定物体との費用はunaryに含まれる |
| `sub.base_location_of` | 復元に使う完全初期配置のコピー |
| `sub.object` | 縮約物体番号→元の物体番号 |
| `sub.location` | 縮約位置番号→元の位置番号 |
| `sub.initial_location_of` | 縮約初期配置。常に0,1,…の順から始まる |
| `sub.constant_offset` | 縮約costへ足すと全体costになる一定の費用 |

`q` は小さい問題の配置です。`std::next_permutation(q.begin(),q.end())` は番号の並びを次の辞書順へ進め、最後を過ぎるとfalseを返します。最初が昇順なので、`do ... while` で6通りすべてを調べられます。`examined` は調べた個数です。

`sub.restore(best)` の引数は、小さい問題に対する配置と**正しい縮約cost**の組です。返却値は元の4物体の配置と全体costです。restoreは費用を再評価せず、渡された縮約costに定数を足します。

全列挙を完走したため、物体0を位置0に固定し、可動物体が位置1,2,3を使う条件では最適です。全体を自由にした最適解という意味ではありません。物体数が増えると階乗で候補が増えるため、そのまま大きい可動集合へ使わないでください。部分問題構築とこの全列挙にはdeadline引数がありません。

### 6.5 例16：可動集合を変えて改善を繰り返す

例14を複数回呼びます。毎回、返ってきた完全配置とcostを次へ引き継ぎます。今回から、既に分かっているcostを渡す形のAPIを使います。

保存する例: `16_repeat_subset.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(4);
    problem.flow = {
        0, 8, 1, 0,
        8, 0, 2, 1,
        1, 2, 0, 6,
        0, 1, 6, 0
    };
    problem.distance = {
        0, 1, 3, 4,
        1, 0, 2, 3,
        3, 2, 0, 1,
        4, 3, 1, 0
    };

    const std::vector<int> initial{0, 2, 1, 3};
    qap_result<> result{initial, problem.evaluate(initial)};
    const std::vector<std::vector<int>> groups{{0, 1}, {1, 2}, {2, 3}};
    using Clock = std::chrono::steady_clock;
    const auto final_deadline = Clock::now() + std::chrono::milliseconds(90);
    for (std::size_t round = 0; round < groups.size(); ++round) {
        if (Clock::now() >= final_deadline) break;
        const auto deadline = std::min(final_deadline,
            Clock::now() + std::chrono::milliseconds(25));
        const auto before = result;
        result = improve_qap_subset(problem, result.location_of, groups[round],
            result.cost, deadline, static_cast<std::uint64_t>(round + 1));
        assert(result.cost <= before.cost);
        for (int i = 0; i < problem.n; ++i) {
            if (std::find(groups[round].begin(), groups[round].end(), i) == groups[round].end()) {
                assert(result.location_of[i] == before.location_of[i]);
            }
        }
    }

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    std::cout << "cost = " << result.cost << '\n';
    for (int i = 0; i < problem.n; ++i) {
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

| 値・引数 | 意味 |
| --- | --- |
| `groups{{0,1},{1,2},{2,3}}` | 各回で動かす物体一覧。位置やグループ所属の意味ではない |
| `round` | 何回目かを表す0始まりの番号 |
| `result.cost`（第4引数） | 同じ問題・同じ `result.location_of` の正しい現在費用 |
| `final_deadline` | 90ms後の全体終了目安 |
| 各 `deadline` | 現在+25msと全体終了目安の早い方 |
| `round+1` | 各呼出しのseed。`static_cast` でuint64_t型にそろえる |

`std::size_t` はvectorの要素数や添字に使う符号なしの型です。`std::find(begin,end,i)` は一覧に `i` があるかを探し、見つからなければ `end` を返します。検査では、その回の可動一覧にない物体が動いていないことを確認しています。

各回で固定される物体は違います。例えば物体1は最初の2回で動くため、全期間を通して固定されるわけではありません。永続的に固定したい物体は、すべての一覧から外します。

既知costは自動で照合されません。古い配置のcostや、別の問題のcostを渡すと結果を壊します。小さい部分でも縮約を作る時間がかかるため、極端に短い呼出しを大量に行えば得になるとは限りません。

## 7. 自分の探索や評価へ組み込む

### 7.1 例17：配置と費用を同期して交換する

例02では自分で配列を交換しました。`qap_swap_state` を使うと、現在配置と費用を一緒に管理できます。例12の3物体に対し、費用が下がる交換だけを受理する短い探索を書きます。

保存する例: `17_swap_state.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(3);
    problem.flow = {
        0, 8, 1,
        8, 0, 2,
        1, 2, 0
    };
    problem.distance = {
        0, 1, 3,
        1, 0, 2,
        3, 2, 0
    };

    const std::vector<int> initial{0, 2, 1};
    qap_prepared<> prepared(problem);
    qap_swap_state<> state(prepared, initial);
    qap_swap_state<> known_state(prepared, initial, state.cost());
    assert(known_state.cost() == state.cost());
    bool changed;
    do {
        changed = false;
        for (int i = 0; i < problem.n; ++i) {
            for (int j = i + 1; j < problem.n; ++j) {
                const long long delta = state.swap_delta(i, j);
                if (delta < 0) {
                    const long long applied = state.apply_swap(i, j);
                    assert(applied == delta);
                    assert(state.cost() == problem.evaluate(state.location_of()));
                    changed = true;
                }
            }
        }
    } while (changed);
    qap_result<> result{state.location_of(), state.cost()};
    assert(result.cost <= problem.evaluate(initial));

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    std::cout << "cost = " << result.cost << '\n';
    for (int i = 0; i < problem.n; ++i) {
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

| API | 引数・動作 |
| --- | --- |
| `state(prepared,initial)` | 問題の前処理と完全配置を渡す。初期費用を全評価する |
| `known_state(prepared,initial,state.cost())` | 正しい既知costを第3引数へ渡す形。全評価を省ける |
| `state.swap_delta(i,j)` | 現在配置で物体i,jを交換する差分。まだ変更しない |
| `state.apply_swap(i,j)` | 差分を計算し、配置と費用を変更し、適用した差分を返す |
| `state.location_of()` | 現在の配置への読み取り用参照 |
| `state.cost()` | 現在保持している費用 |

`changed` は1周の間に改善があったかを記録します。`i<j` の全ペアを試し、差分が負のときだけ交換します。1周して何も変わらなければ終了します。配置の個数は有限で、受理するたびに整数費用が厳密に下がるので、この例のループは終了します。ただし2物体交換で改善できない配置が全体最適とは限りません。

`apply_swap` 自体は改善判定をしません。正の差分でも呼べば交換します。また、直前の `swap_delta` の値を引数として受け取る形式はないので、この例では採用する手の差分を2度計算します。

stateはpreparedを参照します。problem→prepared→stateの順に作り、逆の順で利用を終えます。stateは最良解を別に保存しません。悪化手も使う探索なら、必要な最良解の保存は自分で行います。

### 7.2 例18：QAPで表せない追加費用も比較する

例17の受理条件へ追加費用を足します。4物体のうち物体0,1,2が位置番号の昇順に並ぶと混雑費用20が発生する、という教材用の3物体条件を使います。

保存する例: `18_extra_objective.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(4);
    problem.flow = {
        0, 8, 1, 0,
        8, 0, 2, 1,
        1, 2, 0, 6,
        0, 1, 6, 0
    };
    problem.distance = {
        0, 1, 3, 4,
        1, 0, 2, 3,
        3, 2, 0, 1,
        4, 3, 1, 0
    };

    const std::vector<int> initial{0, 2, 1, 3};
    qap_prepared<> prepared(problem);
    qap_swap_state<> state(prepared, initial);
    const long long congestion = 20;
    const auto extra = [congestion](const std::vector<int>& p) -> long long {
        return p[0] < p[1] && p[1] < p[2] ? congestion : 0LL;
    };
    long long extra_cost = extra(initial);
    const long long initial_total = state.cost() + extra_cost;
    bool changed;
    do {
        changed = false;
        for (int i = 0; i < problem.n; ++i) {
            for (int j = i + 1; j < problem.n; ++j) {
                auto candidate = state.location_of();
                std::swap(candidate[i], candidate[j]);
                const long long next_extra = extra(candidate);
                const long long delta = state.swap_delta(i, j) + next_extra - extra_cost;
                if (delta < 0) {
                    state.apply_swap(i, j);
                    extra_cost = next_extra;
                    changed = true;
                }
            }
        }
    } while (changed);
    assert(extra_cost == extra(state.location_of()));
    assert(state.cost() == problem.evaluate(state.location_of()));
    assert(state.cost() + extra_cost <= initial_total);
    std::cout << "total = " << state.cost() + extra_cost << '\n';
}
```

`extra` は配置を受け取って追加費用を返す、小さな関数です。このようにその場で作る関数をラムダ式と呼びます。`[congestion]` は外の値20をコピーして使う指定、`const std::vector<int>& p` は配置を変更せず参照する引数、`-> long long` は返却型です。

`p[0]<p[1] && p[1]<p[2]` が3物体条件で、`&&` は両方が成り立つ意味です。真なら20、偽なら0を返します。`congestion` はsolverのパラメータではなく、今回定義した目的費用です。

各ペアについて配置をコピーした `candidate` を作り、そのコピーだけを交換して `next_extra` を計算します。判断に使う差分は「QAP差分＋追加費用の交換後−交換前」です。合計が改善した場合だけstateを交換し、`extra_cost` も更新します。

ここでは教育用に追加費用を毎回全計算しています。大きい追加状態を持つ場合も、採否を決める前の差分計算で状態を変更せず、採用後に両方の状態をそろえて更新する、という順序が必要です。追加費用関数を内蔵solverへ渡しているわけではありません。

### 7.3 例19：QAPで候補を作り、本当の費用で採否する

例18は各交換を自分で選びました。今回は内蔵の `improve_qap` で候補を1つ作り、その後に追加費用も含めて比較します。本当の費用を直接扱う代わりに、候補生成に使う評価を**代理評価**と呼びます。

保存する例: `19_surrogate.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(4);
    problem.flow = {
        0, 8, 1, 0,
        8, 0, 2, 1,
        1, 2, 0, 6,
        0, 1, 6, 0
    };
    problem.distance = {
        0, 1, 3, 4,
        1, 0, 2, 3,
        3, 2, 0, 1,
        4, 3, 1, 0
    };

    const std::vector<int> initial{0, 2, 1, 3};
    const long long congestion = 20;
    const auto true_cost = [&problem, congestion](const std::vector<int>& p) -> long long {
        const long long extra = p[0] < p[1] && p[1] < p[2] ? congestion : 0LL;
        return problem.evaluate(p) + extra;
    };
    qap_result<> result{initial, problem.evaluate(initial)};
    long long accepted_true_cost = true_cost(initial);
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    auto candidate = improve_qap(problem, result.location_of, deadline, 42);
    const long long candidate_true_cost = true_cost(candidate.location_of);
    if (candidate_true_cost < accepted_true_cost) {
        result = std::move(candidate);
        accepted_true_cost = candidate_true_cost;
    }
    assert(accepted_true_cost <= true_cost(initial));
    assert(accepted_true_cost == true_cost(result.location_of));

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    std::cout << "QAP cost = " << result.cost
              << ", true cost = " << accepted_true_cost << '\n';
}
```

`true_cost` の引数 `p` は完全配置で、返却値はQAP費用と例18の混雑費用20の合計です。`[&problem,congestion]` の `&problem` は元問題を参照し、`congestion` はコピーします。参照先のproblemをこの関数の利用中に破棄しません。

`result.cost` にはQAP費用だけを保ち、本当の費用を `accepted_true_cost` に別に保存します。候補を本評価した `candidate_true_cost` が小さいときだけ両方を更新します。QAP費用が改善しても、追加費用が増えれば不採用になり得ます。

`improve_qap` が保証するのはQAP側の非悪化です。本来費用の非悪化は、この外側の比較によって守ります。本評価に時間がかかる場合は、その時間をdeadlineの外側に確保してください。

### 7.4 例20：入力が変わったら前処理を作り直す

例13のprepared再利用は同じ問題に対してだけ有効でした。通信量が次のターンで変わる場面を例に、前回配置だけを引き継ぐ手順を確認します。

保存する例: `20_input_update.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(3);
    problem.flow = {
        0, 8, 1,
        8, 0, 2,
        1, 2, 0
    };
    problem.distance = {
        0, 1, 3,
        1, 0, 2,
        3, 2, 0
    };

    const std::vector<int> initial{0, 2, 1};
    qap_result<> result;
    {
        qap_prepared<> prepared(problem);
        const auto deadline = std::chrono::steady_clock::now()
                            + std::chrono::milliseconds(30);
        result = prepared.improve(initial, deadline, 1);
    }
    problem.flow[0 * 3 + 2] = 20;
    problem.flow[2 * 3 + 0] = 20;
    const long long baseline = problem.evaluate(result.location_of);
    {
        qap_prepared<> prepared(problem);
        const auto deadline = std::chrono::steady_clock::now()
                            + std::chrono::milliseconds(30);
        result = prepared.improve(result.location_of, deadline, 2);
    }
    assert(result.cost <= baseline);

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    std::cout << "cost = " << result.cost << '\n';
    for (int i = 0; i < problem.n; ++i) {
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

最初の `{ ... }` が終了するとpreparedが破棄されます。その後にflowの0→2、2→0を20へ変更し、新しいpreparedを作ります。物体数と番号の意味は変わらないため、前回配置をそのまま使えます。

| 値 | 説明 |
| --- | --- |
| 最初の30ms・seed1 | 旧入力での改善 |
| 2か所へ代入する20 | 新しい通信量。対称性を保つため両向きを変更 |
| `baseline` | 新入力で前回配置を評価し直した費用 |
| 次の30ms・seed2 | 新入力での改善 |

比較するのは新しい `baseline` と新しい結果です。以前のcostは違う目的関数の数値なので、そのまま既知costとして使いません。unaryだけを変える場合も、契約上preparedを作り直します。行列の一部変更をpreparedへ通知する専用APIはありません。

## 8. 制約と数値の扱いを仕上げる

### 8.1 例21：配置禁止を罰点で表し、返却候補を検査する

例03のunaryを使い、望ましくない配置へ罰点を付けます。さらに、**実際に禁止を守った配置だけを採用する**外側の処理を付けます。あらかじめ分かっている実行可能配置を保険として保持します。

保存する例: `21_penalty.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(3);
    problem.flow = {
        0, 8, 1,
        8, 0, 2,
        1, 2, 0
    };
    problem.distance = {
        0, 1, 3,
        1, 0, 2,
        3, 2, 0
    };

    const std::vector<unsigned char> allowed{
        1, 1, 0,
        0, 1, 1,
        1, 0, 1
    };
    const long long penalty = 1000;
    problem.unary.assign(3 * 3, 0);
    for (int i = 0; i < 3; ++i) {
        for (int x = 0; x < 3; ++x) {
            if (!allowed[i * 3 + x]) problem.unary[i * 3 + x] = penalty;
        }
    }
    const auto violations = [&allowed](const std::vector<int>& p) {
        int count = 0;
        for (int i = 0; i < 3; ++i) count += !allowed[i * 3 + p[i]];
        return count;
    };
    const std::vector<int> feasible_initial{0, 1, 2};
    assert(problem.is_valid_permutation(feasible_initial));
    assert(violations(feasible_initial) == 0);
    qap_result<> result{feasible_initial, problem.evaluate(feasible_initial)};
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    auto candidate = solve_qap(problem, deadline, 42);
    assert(problem.is_valid_permutation(candidate.location_of));
    const int found_violations = violations(candidate.location_of);
    std::cout << "candidate violations = " << found_violations << '\n';
    if (found_violations == 0 && candidate.cost < result.cost) {
        result = std::move(candidate);
    }
    assert(violations(result.location_of) == 0);

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    std::cout << "cost = " << result.cost << '\n';
    for (int i = 0; i < problem.n; ++i) {
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

| 新しい入力 | 意味 |
| --- | --- |
| `allowed` | 物体×位置の3×3表。1なら許可、0なら禁止 |
| 第0行 `1,1,0` | 物体0は位置0,1へ置ける |
| 第1行 `0,1,1` | 物体1は位置1,2へ置ける |
| 第2行 `1,0,1` | 物体2は位置0,2へ置ける |
| `penalty=1000` | 禁止配置1件に加える費用 |
| `violations(p)` | 置換pが禁止している組をいくつ使ったかを数える利用側の関数 |
| `feasible_initial{0,1,2}` | すべて許可されることを確認済みの完全配置 |

`allowed` はこの例のコードが持つ表であり、solverへ渡す制約APIではありません。solverに伝わるのは、それを数値化したunaryだけです。v09は大きい単項費用を扱う候補生成を追加していますが、どの入力でも禁止を解消する保証はありません。

この小例では元の二次費用は非負で、全flowの和22に最大距離3を掛けた66以下です。したがって罰点1000を含む違反解より、違反0の解が目的関数上は良くなります。これは**解どうしの費用の順序**についての説明で、近似探索が必ず良い方を見つける証明ではありません。一般には負の値や他の単項費用も含めて幅を見積もる必要があります。

そこで候補の違反数を自分で調べ、違反0かつ費用が改善した候補だけを採用しています。候補が条件を満たさなくても、既知の実行可能配置が残ります。この例の返却配置が実行可能なのは、その保持と検査によるものです。

既知の実行可能配置がない場合、この保険は使えません。禁止する物体×位置の組と一対一対応だけなら、許可辺上の二部マッチングで実行可能性を調べられます。重さによる容量などもあれば、2章の制約ソルバーを検討します。罰点を無制限に大きくすると整数範囲を超えるので、次の節も必要です。

### 8.2 例22：大きい整数には型を広げる

値1個が32bit整数に収まらず、積が64bit整数にも収まらない場合を考えます。例01の型を `qap_problem<long long,__int128_t>` に変更します。

保存する例: `22_wide_score.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    using Score = __int128_t;
    qap_problem<long long, Score> problem(2);
    const long long large = 4000000000LL;
    problem.flow = {0, large, large, 0};
    problem.distance = {0, large, large, 0};
    problem.unary.assign(4, Score{0});
    problem.unary[0] = Score{large} * large;

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    auto result = solve_qap(problem, deadline);

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    const Score unit = Score{large} * large;
    assert(problem.evaluate({0, 1}) == 3 * unit);
    assert(problem.evaluate({1, 0}) == 2 * unit);
    std::cout << result << '\n';
}
```

| 型・値 | 意味 |
| --- | --- |
| `long long`（第1型引数） | flowとdistanceの要素型 |
| `Score=__int128_t`（第2型引数） | unary、積、総和、交換差分の計算型。GCCの符号付き128bit整数 |
| `large=4000000000LL` | 40億。末尾LLによりlong longの定数にする |
| `Score{large} * large` | 掛ける前に128bitへ変換し、その型で積を求める |
| `problem.unary[0]` | 物体0を位置0へ置く費用。今回は40億の二乗 |

40億の二乗は $16\cdot10^{18}$ で、64bit符号付き整数の最大値である約 $9.22\cdot10^{18}$ を超えます。掛け算を終えてから128bitへ変換しても、先にあふれた結果は修復できません。

`std::cout << result` は結果の構造体全体を表示します。v0.9.1から、`qap_result` の出力処理が128bit整数のcostも10進数へ変換するため、自作の変換関数は不要です。例えば `qap_result{cost=32000000000000000000, location_of=[1, 0]}` と表示されます。標準の `std::cout << result.cost` は128bit整数単体には対応しないので、ここでは `result` 全体を渡します。演算子の読み方と出力規則は例24で説明します。

型を広げても無限ではありません。最終costに加え、入力作成時の演算、符号反転、差分や内部補正式の途中値も範囲内に収めます。本ライブラリは一般のオーバーフロー検査を行いません。負の差分を扱うためScoreは符号付き整数を使います。本書では浮動小数点Scoreを検証対象にしていません。

### 8.3 例23：既定構築、空問題、締切到達時

ここでは境界の使い方を確認します。入力を後で用意する場合や、既に締切へ到達している場合でも、何が返るかを区別します。

保存する例: `23_boundaries.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem;
    assert(problem.n == 0);
    auto empty = solve_qap(problem, std::chrono::steady_clock::now());
    assert(empty.location_of.empty() && empty.cost == 0);
    problem = qap_problem<>(1, std::vector<int>{3},
        std::vector<int>{4}, std::vector<long long>{5});
    const std::vector<int> initial{0};
    const auto expired = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    auto result = improve_qap(problem, initial, expired);
    assert(result.location_of == initial && result.cost == 17);
    auto unchanged = improve_qap_subset(problem, initial, {}, result.cost, expired);
    assert(unchanged.location_of == initial && unchanged.cost == result.cost);
    assert(problem.swap_delta(initial, 0, 0) == 0);

    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));

    std::cout << "cost = " << result.cost << '\n';
    for (int i = 0; i < problem.n; ++i) {
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

`qap_problem<> problem;` は `n=0`、すべての行列が空の問題です。空問題の解は空配置、cost=0です。非空問題のための行列が自動で用意されるわけではありません。

次の代入は、個数1、flowの唯一の要素3、distanceの唯一の要素4、unaryの唯一の要素5を、行列受取りコンストラクタへ渡します。costは $3\cdot4+5=17$ で、配置は `{0}` しかありません。

`expired` は現在より1秒前の時刻です。`improve_qap` はその場合も初期配置を全評価し、初期配置と正しいcostを返します。既知cost版subsetへ空の可動一覧 `{}` を渡すと、配置をコピーし、渡したcostと共に返します。同じ物体0を2度指定するswap差分は0です。

空問題に `swap_delta(...,0,0)` を呼んではいけません。物体0が存在しないためです。また、期限を過ぎた `solve_qap` でも初期配置を作る処理は行われ、締切到達が即時返却や定数時間を意味するわけではありません。

### 8.4 例24：結果をそのままデバッグ出力する

例01ではcostと配置を自分で取り出して表示しました。今回は、返却された構造体をそのまま出力します。物体数を25に増やし、長い配列がどう省略されるかも確認します。

問題は「各物体を同じ番号の位置に置けば費用0、それ以外は費用10」です。flowとdistanceはコンストラクタが用意する0のままなので、費用はunaryだけで決まります。

保存する例: `24_debug_output.cpp`（または `main.cpp`）。

```cpp
#include "qap_solver_v09.hpp"

int main() {
    qap_problem<> problem(25);
    problem.unary.assign(25 * 25, 10);
    for (int i = 0; i < problem.n; ++i) {
        problem.unary[i * problem.n + i] = 0;
    }
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(50);
    const auto result = solve_qap(problem, deadline, 1);

    std::cout << result << '\n';
    std::cerr << "debug: " << result << '\n';

    std::ostringstream log;
    log << result;
    assert(log.str().find("...]") != std::string::npos);
    assert(log.str().find("(size=25)") != std::string::npos);
    assert(result.location_of.size() == 25);
    assert(problem.is_valid_permutation(result.location_of));
    assert(result.cost == problem.evaluate(result.location_of));
}
```

まず `problem(25)` で25物体・25位置の問題を作ります。`unary.assign(25 * 25, 10)` は全625通りの割当費用を10にします。続くループで物体 `i` と位置 `i` の組だけ0にします。これまでと同じ行列の作り方で、出力方法だけを新しくしています。

| 新しく使う値・書き方 | 意味 |
| --- | --- |
| `25` | 物体数と位置数。表示上限20を超える例にするための数 |
| `10` | 物体と位置の番号が異なるときの単項費用 |
| `0` | 同じ番号へ置いたときの単項費用 |
| `milliseconds(50)` | 探索に渡す締切を現在時刻から50ms後にする |
| `solve_qap(problem, deadline, 1)` | 問題、絶対締切、seed=1を順に渡す |
| `const auto result` | 出力時に結果を書き換えない、読み取り専用の変数 |
| `std::cout << result` | 標準出力へ費用と配置をまとめて表示 |
| `std::cerr << "debug: " << result` | 標準エラー出力へ説明文字列と結果を続けて表示 |
| `'\n'` | 呼出し側が付ける改行。結果の出力自体は改行しない |
| `std::ostringstream log` | 画面の代わりに文字列へ出力をためるストリーム |
| `log.str()` | ためた文字列を取り出す。表示内容の検査やログ保存に使える |
| `find("...]")` と `std::string::npos` | 文字列内を検索し、見つからなかったときだけnposを返す |

C++の `operator<<` は、`出力先 << 値` という書き方の動作を決める関数です。本ライブラリは費用の型を `Score` とする各 `qap_result<Score>` に対して、`std::ostream& operator<<(std::ostream&, const qap_result<Score>&)` を定義しています。第1引数は出力先、第2引数は読むだけの結果です。返却値は同じ出力先への参照なので、さらに `<< '\n'` を続けられます。追加の設定や別ヘッダーは不要です。

配置が物体番号順のとき、標準出力は次の1行になります。

```text
qap_result{cost=0, location_of=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ...] (size=25)}
```

`cost` は目的関数値、`location_of` は物体番号順の位置です。配列は**先頭20要素まで**表示します。21要素以上なら末尾の `...` で省略を示し、`size=25` で元の要素数を知らせます。20要素以下なら全要素を表示し、空配列は `[]` です。表示を省略しても結果の配列には全25要素が残ります。

最後のassert群は、省略表示があること、全要素数、配置の妥当性、costの一致を確認しています。`find("...]")` と `find("(size=25)")` の引数は、それぞれ出力に含まれるはずの文字列です。

`solve_qap` だけでなく、`improve_qap`、`prepared.solve`、`prepared.improve`、`improve_qap_subset`、`subproblem.restore` も同じ結果型を返すので、そのまま表示できます。配布の `reference/qap_solver_usecase_examples.hpp` にある9種類の用途別結果型も同じ規則で表示でき、複数の配列がある場合は**配列ごとに**20要素までです。型名・フィールド名から、元の費用や最大化スコアを区別できます。

これは人が読むためのデバッグ表示です。全配置を保存する用途では `location_of` を全要素読み出します。通常の整数は出力先の書式指定に従い、128bit整数は10進数で表示します。出力処理は結果やストリームの書式フラグを変更しません。

## 9. 公開APIの索引と確認事項

### 9.1 どの例を見ればよいか

型引数の既定値は `Weight=int`、`Score=long long` です。`qap_result` はScoreだけを持ちます。すべての探索APIでseedの既定値は1、deadlineの省略はできません。

| API・公開データ | 掲載例 |
| --- | --- |
| `qap_problem(n)`、`n/flow/distance/unary` | 01、03 |
| 行列を受け取るコンストラクタ（unaryなし／あり） | 13、23 |
| `qap_problem()` | 23 |
| `is_valid_permutation`、`evaluate`、`swap_delta` | 02 |
| `qap_result::location_of`、`cost` | 01、13 |
| 結果の `operator<<`（cout・cerr・文字列） | 22、24 |
| `solve_qap`、`improve_qap` | 01、12 |
| `qap_prepared` と `problem/evaluate/swap_delta/solve/improve` | 13 |
| `improve_qap_subset`（cost省略／既知） | 14、16 |
| `make_qap_subproblem`（cost省略／既知） | 15 |
| `qap_subproblem` の全メンバーと `restore` | 15 |
| `qap_swap_state` の両コンストラクタと全メソッド | 17 |
| 型引数を広げる | 22 |

`prepared.improve` に既知costを渡す形式はありません。`state.apply_swap` に既知deltaを渡す形式もありません。温度、tabu期間、探索方式を選ぶ公開設定クラスはありません。通常の調整は入力モデル、deadline、seed、外側の試行回数や可動集合で行います。

### 9.2 実用コードへ移すとき

1. `n` は0以上とし、flow・distanceの長さを `n*n`、unaryを空または `n*n` にする。サイズと添字の計算も型の範囲に収める。
2. 初期配置は完全な置換にする。可動一覧は重複のない物体番号にする。
3. `problem` と、それを借りるprepared、それを借りるstateの寿命を守る。参照先をmoveしたり変更したりしない。
4. 既知costは同じ入力・同じ配置の数値だけを使う。入力を変えたら全評価と必要な前処理をやり直す。
5. 追加の禁止条件、本当の費用、復元時に戻す定数を、QAPのcostと区別する。
6. deadlineを超える準備・補完・全評価があり得ることを見込み、アプリケーション全体の時間に余裕を持つ。

`n*n` や `i*n+j` にはint演算があるため、通常の32bit intでは `n<=46340` が数式上の必要条件です。これは実用上その大きさで動く保証ではありません。密行列を持つため、メモリと計算時間が先に問題になり得ます。

### 9.3 教材の検証範囲

本書の24コードブロックは、配布ZIPの `examples/01_minimal.cpp` 〜 `examples/24_debug_output.cpp` と同じ内容です。GCC 13.3.0、`-O2 -Wall -Wextra -Wshadow -Wconversion -Werror` で、C++20・C++23それぞれで、計48通りのビルドと実行を確認しました。

単に終了することに加え、置換の妥当性、全評価との一致、交換差分、固定物体、縮約と復元、ダミー・巡回・グラフ対応の元の目的式、入力更新後の費用、追加制約の採用条件、結果の省略表示を検査しています。近似解が特定の最適値になることや、50ms以内で必ず終了することを一般のテスト条件にはしていません。

ZIPのルートで次を実行すると、本文とのコード一致確認を含めて検証を再実行できます。Python 3とg++が必要です。

```bash
python3 verify_examples.py
```

## 付録A. ライブラリ実装の全体像

ここからは、APIの使い方ではなくv09の内部を読みたい方向けです。まず流れを示し、その後に個々の処理を説明します。内部の名前や数値は公開の設定引数ではありません。

### A.1 探索全体の流れ

1. `solve` は候補配置を内部で作る。`improve` は渡された初期配置を使う。
2. 目的値を全評価し、返却用の最良解として保存する。物体が2個未満、または締切到達なら終了する。
3. 必要なら対称性とflowの疎性を調べ、評価器を準備する。prepared版はこの準備を再利用する。
4. 大きいunaryが有望と判定された場合、割当処理で追加候補を作り、全目的値が改善した場合だけ保存する。
5. その後の残り時間を基準に、最初の1/4までThreshold Acceptingを行う。
6. それまでの最良解から再びThreshold Acceptingを行い、同じ基準で残り時間の1/2まで進む。
7. 残り時間にswap差分表の構築が間に合いそうなら、表を作ってtabu探索をする。構築前の見積りで間に合わなさそうならThreshold Acceptingを続ける。
8. 保存済み最良配置とcostを返す。

ここで**swap**は2物体の位置を交換する操作、**探索**は配置を変えながら良い候補を探す処理です。現在動かしている配置と、返却用の最良配置は分けて保持されます。そのため途中で悪化手を使っても、`improve` の返却値は初期配置より悪化しません。

既に差分表の構築を始め、途中で締切に達した場合は最良解を返します。この場合にThreshold Acceptingへ戻るわけではありません。

### A.2 計算量の読み方

以後の $O(n)$ は、おおむねnに比例して処理が増えるという上界の書き方です。$O(n^2)$ は表を全走査するような処理、$O(n^3)$ はそれをさらにn回行う規模です。実際の秒数や一定倍率を表すものではありません。

| 処理 | 時間の上界・目安 |
| --- | --- |
| 置換の検査 | $O(n)$ |
| 全目的値の評価 | $O(n^2)$ |
| 前処理 | $O(n^2)$ |
| 1回の密なswap差分 | $O(n)$ |
| 疎なswap差分 | 交換物体につながる非ゼロの入出辺数に比例。定数分も必要 |
| v09の単項候補生成 | 最悪 $O(n^3)$、一時領域 $O(n^2)$ |
| 密な全swap差分表の初期構築 | $O(n^3)$ |
| 差分表を使う1手の選択と更新 | それぞれ $O(n^2)$ |
| 可動k物体の縮約、既知costあり | $O(n+k^2n)$ |
| 同、既知costなし | $O(n^2+k^2n)$ |
| 縮約配置の復元 | $O(n+k)$ |

表は主計算についての上界です。debug時のassertは追加の置換検査・作業領域を使います。探索全体の回数は実時間で決まるため、nだけで実行時間は決まりません。入力やtabu用の表は密なので、flowが疎でも全体メモリは $O(n^2)$ です。

## 付録B. 個々のアルゴリズムと実装

### B.1 初期配置

`solve` はランダムに並べた置換と、関係の大きい物体を中央寄りの位置へ置く配置を作ります。両方をunaryも含めて全評価し、後者が厳密に良いときだけ後者を選びます。

関係の大きさは、物体ごとのflowの入出絶対値の和です。中央寄りかどうかは、位置ごとのdistanceの入出和で測ります。式で書くと次の量です。

$$s_ {i}=\sum_ {j=0}^{n-1}(\lvert F_ {i,j}\rvert+\lvert F_ {j,i}\rvert),\qquad c_ {x}=\sum_ {y=0}^{n-1}(D_ {x,y}+D_ {y,x})$$

$s_ {i}$ の大きい物体順と、$c_ {x}$ の小さい位置順を対応させます。同点は番号順です。ここでの $\lvert a\rvert$ は絶対値です。負の係数や一般の位置係数では「中央が有利」という直感が常に当たるわけではないので、ランダム候補との比較を行います。

### B.2 v09：強い単項費用に対する候補生成

禁止に相当する大きな罰点があると、数物体をまとめて入れ替えれば解消できても、2物体交換の途中で罰点が増えることがあります。v09はこのような場合へ、単項費用を手掛かりにした割当候補を追加します。この処理はsolveだけでなく、通常の探索ドライバへ進むimproveや部分問題の改善からも使われます。

まずunaryがなければ何もしません。存在するときは、入口で残る時間の約1/4を追加処理の時間枠にします。以下の順番で処理します。

1. **安価な予備判定。** unaryの第0行と、約1/4、1/2、3/4の位置にある行との差の幅を調べます。flowとdistanceの16標本から得た尺度と比較し、弱そうなら追加処理を省きます。
2. **整数範囲の確認。** unaryの絶対値が `Score` 最大値の `8*n` 分の1を超える場合、追加処理を省きます。これはこの処理の差や累積のための保守的な検査で、ライブラリ全体のオーバーフロー検査ではありません。
3. **二次項との比較。** 全flowの絶対値和とdistanceの最大値−最小値を掛け、二次項が変わり得る幅の上界を求めます。
4. **単項の改善余地を確認。** 列ごとの平均を引いたunaryについて、現在の位置と各行の最小値との差を合計し、二次項の幅と比較します。
5. **安い候補辺で完全割当を試す。** 各行の最安値から、二次項の幅以内の位置だけを候補にします。候補が少ない物体から順に、交互道を使って割り当てます。
6. **完成しない場合の最小費用割当。** 残った時間でHungarian法を試します。時間切れなら、既に完成した割当を保ち、残りを空き位置へ貪欲に埋めます。
7. **完全目的値で採否。** 完成した置換をunaryと二次項の両方で全評価し、保存済み最良値を厳密に改善した場合だけ採用します。

予備判定は全行を見ないため、観測外の強いunaryを見落とすことがあります。尺度の比較にはlong doubleも使うため、丸めの影響もあります。数値が大きいだけで常に発動するわけではありません。

二次項の変動幅の上界を $R$ とすると、実装で使う値は次の形です。

$$R=\left(\sum_ {i=0}^{n-1}\sum_ {j=0}^{n-1}\lvert F_ {i,j}\rvert\right)(D_ {\max}-D_ {\min})$$

$D_ {\max},D_ {\min}$ はdistance表の最大値と最小値です。どの配置でも各flowに掛かるdistanceはこの範囲内なので、その変化を全項で足せば上界になります。

列平均を整数のまま扱うため、実装は次のn倍した値を使います。

$$V_ {i,x}=nU_ {i,x}-\sum_ {r=0}^{n-1}U_ {r,x}$$

ここで $r$ は同じ列を足すための物体番号、$V$ は判定用の一時的な費用です。物体ごとの定数、位置ごとの定数をunaryへ加えても、すべての置換で同じ合計が加わるだけです。そのような配置と無関係な成分を判定から減らします。ただし、丸めや範囲検査もあり、追加処理全体が定数の加算に対して完全に同じ挙動になる保証はありません。特に候補辺を選ぶ段階は元のunaryの行最小値を使います。

**交互道**は、ある物体が欲しい位置を別の物体が使っているとき、占有者をさらに別の位置へ動かす連鎖です。空き位置までつながれば、連鎖を逆にたどって一対一対応を保ちながら割当を増やせます。幅優先にこの連鎖を探します。候補辺で完全割当ができても、単項費用の最小割当を求めたことにはなりません。

Hungarian法の段階では、行・列の調整値（双対変数）を持ち、調整後の費用が小さい辺をたどって割当を増やします。`row_potential` と `column_potential` がその調整値、`minimum` が各未訪問列へ到達する追加費用、`previous` が経路の復元先です。1物体ずつ空き位置までの経路を完成させ、対応を更新します。完走した場合は単項費用の最小割当ですが、QAP全体の最適解という意味ではありません。

追加処理で使う候補辺は、利用者が指定したハード制約ではありません。その後の探索にも禁止表として引き継がれません。時間枠に達した後の未割当補完と全評価に、さらに $O(n^2)$ の処理が必要なことがあります。最悪時間は $O(n^3)$、一時メモリは $O(n^2)$ です。

### B.3 対称性と疎なflowの前処理

flowとdistanceの両方が対称かつ対角0の場合だけ、対称用の短い差分式を選びます。unaryに対称性は必要ありません。

flowの非ゼロ要素数を $m$ とすると、$3m<n^2$ のときに疎な差分経路を用意します。各物体から出る非ゼロ辺、各物体へ入る非ゼロ辺を連続配列に保存します。この形式をCSRと呼びます。

まずflowを行方向に読み、出る辺を保存し、各頂点への入辺数を数えます。入辺数の累積和で各頂点の格納区間を決め、保存済み出辺を転記して入辺を作ります。入辺を作るために密なflowをもう一度全走査する必要はありません。

判定が見るのはflowの疎さです。distanceが疎なだけではこの経路になりません。全目的値の `evaluate` は疎CSRへ切り替わらず、常に全物体ペアを走査します。

### B.4 交換差分の式

物体 $r,s$ を交換し、交換前の位置を $a=p_ {r}$、$b=p_ {s}$ とします。交換に関係しない物体同士の費用は変わりません。差分を、単項、他の物体との関係、交換した2物体自身の関係に分けます。

$$\Delta_ {U}=U_ {r,b}-U_ {r,a}+U_ {s,a}-U_ {s,b}$$

$$\Delta_ {\mathrm{ext}}=\sum_ {k\ne r,s}\left[(F_ {r,k}-F_ {s,k})(D_ {b,p_ {k}}-D_ {a,p_ {k}})+(F_ {k,r}-F_ {k,s})(D_ {p_ {k},b}-D_ {p_ {k},a})\right]$$

$$\Delta_ {\mathrm{pair}}=(F_ {r,r}-F_ {s,s})(D_ {b,b}-D_ {a,a})+(F_ {r,s}-F_ {s,r})(D_ {b,a}-D_ {a,b})$$

$$\Delta=\Delta_ {U}+\Delta_ {\mathrm{ext}}+\Delta_ {\mathrm{pair}}$$

$k$ は交換しない物体の番号、$\Delta$ は「交換後−交換前」です。入る向きと出る向きを両方含むので、非対称にも使えます。各項は変更前に加えていた費用を引き、変更後に加える費用を足したものです。

両行列が対称・対角0なら、入出方向が等しくなり、次に簡約できます。

$$\Delta=\Delta_ {U}+2\sum_ {k\ne r,s}(F_ {r,k}-F_ {s,k})(D_ {b,p_ {k}}-D_ {a,p_ {k}})$$

密な経路では他の全物体を調べ、疎な経路では非ゼロの関係だけを調べます。公開の `problem.swap_delta` は一般式、preparedとstateは準備済みの経路を使います。

### B.5 Threshold Accepting

費用を下げる手だけを使うと、その場で動けなくなることがあります。Threshold Acceptingは、差分が許容量以下なら、小さな悪化も一時的に受け入れる方法です。

まず最大256回のランダムswapを調べ、正の差分だけを集めます。小さい順に並べた中央の値（偶数個なら中央2個の大きい方）を尺度にします。正の標本がなければ1です。尺度を $s$、初期許容量を $T_0$ として次を使います。

$$T_0=0.1171875\max(1,s),\qquad T(q)=T_0(1-q)$$

$q$ は標本化の終了からフェーズ締切までの経過割合です。最初は少し悪化を許し、最後に0へ近づけます。標本化では32回ごと、探索では256反復ごとに時刻を確認するため、厳密な時刻で強制停止する処理ではありません。

差分が `T` 以下なら現在配置を交換し、最良costを更新した場合だけ返却用配置も保存します。標本間で同じ物体ペアを選ぶことはあります。内部の係数0.1171875や標本数を指定する公開引数はありません。

### B.6 全swap差分表の構築と更新

全ペアの差分を毎回計算し直す代わりに、表へ保持できます。ペア数は $P=n(n-1)/2$ です。実装はn×n要素の領域を確保し、`i<j` の側を使います。

表を作る前に $\min(P,\max(16,n))$ 回の差分を計測し、全ペア分の構築時間を見積もります。その2倍が残り時間未満なら構築へ進みます。標本の計算がコンパイラに削除されないよう、符号なしのchecksumとインラインasmを使います。checksumの値自体で探索方式を変えません。

表の構築は4行ごとに時刻を確認します。swapを1回行った後は、交換した2物体を含まない各ペアの差分を、変化した関係だけで定数時間ずつ補正します。交換物体を含むペアは通常の差分計算と、3物体の並べ替えに関する恒等式を組み合わせます。表全体の更新は $O(n^2)$ です。

恒等式の詳細も記します。交換前のr,s,iの位置をa,b,c、交換前差分を $\delta_ {u,v}$、r,s交換後の差分を $\delta'_ {u,v}$ とします。

$$G_ {F}=F_ {r,s}-F_ {s,r}+F_ {s,i}-F_ {i,s}+F_ {i,r}-F_ {r,i}$$

$$G_ {D}=D_ {a,b}-D_ {b,a}+D_ {b,c}-D_ {c,b}+D_ {c,a}-D_ {a,c}$$

$$\delta'_ {r,i}+\delta'_ {s,i}=\delta_ {r,i}+\delta_ {s,i}-\delta_ {r,s}+G_ {F}G_ {D}$$

$G_ {F}$ と $G_ {D}$ は3点を一周する向きと逆向きの係数差です。3物体の6通りの配置を足し引きすると、単項費用や外部物体の費用が消え、この積だけが残ります。片方の新差分を通常計算すれば、もう片方を上式から復元できます。対称行列では対応するGが0になります。r,sを元へ戻す差分は、直前に使った差分の符号反転です。

この全差分表は非公開の内蔵探索用です。公開の `qap_swap_state` がこの表を持っているわけではありません。

### B.7 Tabu探索

全swapの中から、許可されている最小差分の手を選びます。改善手がなければ悪化手を選ぶこともあります。すぐ元の配置へ戻って同じ所を往復しないよう、直前に使っていた「物体→位置」を一定反復数だけ避けます。これがtabuです。

禁止期間は、$\max(1,\lfloor n/10\rfloor)$ から $\max(\max(1,\lfloor n/10\rfloor),\lfloor3n/10\rfloor)$ までの整数から選びます。$\lfloor a\rfloor$ は小数部分を切り下げる記号です。期間は秒数ではなく探索の反復数です。

ただし、保存済み最良解を厳密に更新する手はtabu中でも許可します。この例外をaspirationと呼びます。同じ差分の手は走査順で先のものを残します。

前回の表更新にかかった時間も測り、次の更新にその1.25倍の余裕がなければ終了を選びます。これは時間超過を減らす目安であり、次の処理時間を保証するものではありません。ここでいうtabuの禁止は探索履歴上の一時的な禁止で、例21の業務上の配置禁止とは別です。

### B.8 部分問題の縮約と復元

可動物体一覧を $m_0,\ldots,m_ {k-1}$、その現在位置を $l_ {a}=p_ {m_ {a}}$ とします。$a,b$ は縮約物体番号、$x,y$ は縮約位置番号、$I$ は固定物体の集合です。

$$F'_ {a,b}=F_ {m_ {a},m_ {b}},\qquad D'_ {x,y}=D_ {l_ {x},l_ {y}}$$

$$U'_ {a,x}=U_ {m_ {a},l_ {x}}+\sum_ {f\in I}(F_ {m_ {a},f}D_ {l_ {x},p_ {f}}+F_ {f,m_ {a}}D_ {p_ {f},l_ {x}})$$

固定物体との相互費用は、可動物体の配置先が決まれば決まるので、単項費用へ移せます。固定物体同士の費用は変わりません。縮約初期配置idを0,1,…の順として、一定部分を次で求めます。

$$K=C(p)-C_ {\mathrm{sub}}(\mathrm{id})$$

$C_ {\mathrm{sub}}$ は縮約問題の費用、$K$ は `constant_offset` です。縮約配置 $q$ を戻すと、元物体 $m_ {a}$ の位置は $l_ {q_ {a}}$ になり、固定物体はそのままです。全体費用は $K+C_ {\mathrm{sub}}(q)$ です。

実装は固定物体を外側にして、必要な位置間距離をk要素の作業配列へ展開し、複数の可動物体で共有します。縮約unaryがすべて0なら空のvectorにします。

`improve_qap_subset` は可動数kが2未満、または締切到達ならそのまま返します。既知costを省いた形では、その前に全評価します。k=nで通常探索へ進める場合は、全体のimproveへ委譲するため、既知costを渡しても開始費用を再計算します。`make_qap_subproblem` 自体はk=nでも縮約を構築し、途中停止のdeadlineを持ちません。

### B.9 乱数、公開状態、自己テスト

内部の乱数器は64bit状態を左へ7bit、右へ9bitずらしてxorする方式です。seed=0では非ゼロの固定状態から始めます。範囲内の整数は128bit積の上位64bitから作ります。フェーズごとにseedへ定数を加える、またはxorして異なる系列を使います。

乱数の現在位置、tabu履歴、全差分表は公開APIで取り出せません。別の `improve` 呼出しへ引き継げるのは配置であり、preparedが共有するのは同じ問題に対する前処理です。

ヘッダーを直接コンパイルすると、末尾の13群の自己テストとmainが有効になります。通常のincludeでは無効です。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -x c++ qap_solver_v09.hpp -o qap_selftest
./qap_selftest
```

直接コンパイル時の `#pragma once in main file` 警告は想定内です。自己テストは一般の入力契約や差分・縮約などを確認しますが、あらゆる実データの解品質や締切厳守を証明するものではありません。

### B.10 結果のデバッグ出力

出力演算子はcostなどのスカラーを先に書き、各配列を添字0から最大19まで読みます。全配列をコピーしたり、費用を再計算したりしません。表示する要素数は配列ごとに最大20なので、配列の長さを増やしても、その走査回数は増えません。

128bit整数の表示では、10で割った余りから末尾の数字を取り出し、固定長の文字配列へ後ろから書きます。負の最小値はそのまま符号を反転すると範囲を超えるため、`-(value + 1)` を先に求め、符号なし128bit整数へ変換してから1を足します。この補助処理は `qap_debug_detail` 内部にあり、利用者は結果型の `operator<<` だけを使います。

演算子はヘッダー内のinlineなfriend関数として定義されるため、複数のソースファイルから同じヘッダーをincludeできます。データメンバーや集成体初期化の形は変わりません。探索経路・費用計算・時間配分への変更もありません。

### B.11 参照資料

ライブラリ固有の記述は `qap_solver_v09.hpp` の公開宣言と実装を一次資料としています。入力変換や式の整理は、本書に示した目的式から導いています。外部ソルバーの仕様確認には次の公式資料を使いました。

- [SciPy: quadratic_assignment](https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.quadratic_assignment.html)
- [SciPy: linear_sum_assignment](https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.linear_sum_assignment.html)
- [OR-Tools: Linear Sum Assignment Solver](https://developers.google.com/optimization/assignment/linear_assignment)
- [OR-Tools: CP-SAT Solver](https://developers.google.com/optimization/cp/cp_solver)
- [OR-Tools: Assignment with Task Sizes](https://developers.google.com/optimization/assignment/assignment_cp)
