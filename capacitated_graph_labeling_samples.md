# 容量制約付きグラフラベリング サンプルコード集

対象：`capacitated_graph_labeling_v25.hpp`。このファイルが提供する公開APIと、その使い方を説明します。

数学は高校の関数・和・不等式を前提にしています。C++では、変数、配列、`if`、`for`の基本を使います。第3章の22例は、それぞれ`main`を含む独立したプログラムです。各例の入力形式・注意点は、本文と日本語のコードコメントの両方に記載しています。

## 1. 何を解くライブラリか？

### 1.1 一つずつ割り当て、個別費用と関係費用を小さくする

このライブラリは、**複数の対象を、決められた種類の割当先へ一つずつ割り当てる問題**を扱います。

4つの仕事を2人に割り当てる場面を考えます。仕事を分割することはできず、各仕事は必ずどちらか1人へ渡します。仕事ごとに担当者との相性があり、「仕事0は担当者0なら費用0、担当者1なら費用6」といった個別費用を設定できます。

さらに、「仕事0と仕事2を別の人に任せると引継ぎ費用5がかかる」のような、2つの仕事の組合せに関する費用も設定できます。各担当者に割り当てる総工数や件数の上下限を守りながら、合計費用を小さくする割当を探します。

| ライブラリの言葉 | 意味 | 仕事の割当での例 |
|---|---|---|
| 頂点 | 割り当てる対象 | 仕事0、仕事1、仕事2、仕事3 |
| ラベル | 割当先の番号 | 担当者0、担当者1 |
| 辺 | 2頂点の間に費用を定義する関係 | 仕事0と仕事2の引継ぎ |
| 単項コスト | 1頂点の割当先だけで決まる費用 | 仕事0を担当者1へ置く費用6 |
| 二項コスト | 1辺の両端の割当先で決まる費用 | 仕事0と2を別担当にしたときの費用5 |
| 解、ラベル列 | 全頂点の割当先を順に並べたもの | `{0, 0, 1, 1}` |
| 実行可能解 | 指定された必須条件をすべて満たす解 | 担当件数・工数・資格などを守る割当 |
| 最適解 | 実行可能解の中で費用が最も小さい解 | 最安の割当 |

頂点や辺は入力データの呼び方であり、図を描く必要はありません。仕事以外にも、処理のサーバー配置、データのクラスタ分け、設備の場所への配置、段階値の選択などを同じ形で表せます。

返された成功解は必須条件を満たしますが、最適であることの証明は付きません。失敗した場合も、実行可能解が存在しないという証明が得られたわけではありません。

### 1.2 目的関数

各頂点のラベルを決めて、次の値を最小にします。

$$\min_ {\ell} C(\ell),\qquad C(\ell)=\sum_ {v=0}^{N-1}U(v,\ell_ {v})+\sum_ {e=0}^{M-1}P_ {e}(\ell_ {u_ {e}},\ell_ {v_ {e}})$$

式に出てくる記号は次のとおりです。

| 記号 | 意味 |
|---|---|
| $N$ | 頂点数。仕事の個数に相当する。0以上 |
| $K$ | ラベル数。担当先の種類数に相当する。1以上 |
| $M$ | 登録された辺の本数。0以上 |
| $v$ | 頂点番号。$0,1,\ldots,N-1$ |
| $e$ | 辺番号。$0,1,\ldots,M-1$。入力した辺配列の添字 |
| $u_ {e},v_ {e}$ | 辺$e$の第1端点と第2端点の頂点番号 |
| $\ell_ {v}$ | 頂点$v$へ選んだラベル。$0,1,\ldots,K-1$のどれか |
| $\ell$ | 全頂点のラベルをまとめた列。これを決めたい |
| $U(v,a)$ | 頂点$v$をラベル$a$へ置く単項費用。入力で与える |
| $P_ {e}(a,b)$ | 辺$e$の第1端点を$a$、第2端点を$b$へ置く二項費用。規則を入力で与える |
| $a,b$ | 費用を説明するためのラベルの仮の名前。0以上K未満 |
| $C(\ell)$ | ラベル列$\ell$に対する合計費用。目的値とも呼ぶ |
| $\sum$ | 指定範囲の項を全部足す記号 |
| $\min_ {\ell}$ | 必須条件を満たすラベル列の中で、値を最小にしたいという意味 |

第1の和は「各仕事を誰へ渡すか」の費用、第2の和は「関係のある仕事同士をどう組み合わせるか」の費用です。単項費用や辺の重みには負の値も使えます。小さいほどよいことは変わりません。

たとえば`{0,0,1,1}`という割当の単項費用が順に0、1、0、1なら、単項費用の合計は2です。仕事0と2、仕事1と3を別担当にすると、それぞれ5かかるなら関係費用は10で、目的値は12になります。

ラベル数Kは事前に指定します。使われないラベルを許すか、すべてのラベルを使うかは下限で決まります。

### 1.3 必ず守る条件

頂点$v$の需要量を$d_ {v}$とします。仕事の例では工数です。同じ頂点の需要量は、割当先によらず一定です。

ラベル$a$に集まる需要量$W_ {a}$と頂点数$n_ {a}$は次のように表せます。

$$W_ {a}=\sum_ {v=0}^{N-1}d_ {v}[\ell_ {v}=a],\qquad n_ {a}=\sum_ {v=0}^{N-1}[\ell_ {v}=a]$$

角括弧$[\ell_ {v}=a]$は、条件が正しければ1、違えば0です。したがって、担当先$a$に入る仕事だけを足し合わせています。

$$L_ {a}\le W_ {a}\le R_ {a},\qquad A_ {a}\le n_ {a}\le B_ {a}$$

| 記号 | 意味 |
|---|---|
| $d_ {v}$ | 頂点$v$の需要量。0以上の整数 |
| $W_ {a}$ | ラベル$a$へ置いた需要量の合計 |
| $n_ {a}$ | ラベル$a$へ置いた頂点数 |
| $L_ {a},R_ {a}$ | 需要量合計の下限・上限 |
| $A_ {a},B_ {a}$ | 頂点数の下限・上限 |

需要量0の頂点も1件です。需要量をすべて1にすると、需要量合計と件数は同じ数になります。

このほか、各頂点について「このラベルしか使わない」という固定先と、「このラベルは使ってよい」という許可表を指定できます。**固定先を指定した頂点では、許可表より固定先が優先されます。** 両方の条件の共通部分を取る仕様ではありません。

### 1.4 費用で表す希望と、必須条件の違い

辺に大きな罰金を置くのは希望の表現です。罰金を払って希望を破った解でも、需要量・件数・固定・許可を満たしていれば実行可能解です。

たとえば、同じ色の辺へ罰金を設定しても、`feasible=true`が「隣同士は必ず異なる色」を意味するわけではありません。衝突ゼロが必要なら、元の意味での罰金が0かどうかも確認します。

| 表現したいこと | このライブラリでの扱い |
|---|---|
| 頂点ごとに1つのラベルを選ぶ | そのまま表せる |
| 総需要量と件数の上下限、固定、許可先 | 必須条件として指定できる |
| 2頂点のラベルの任意の組合せ費用 | 二項費用として指定できる |
| 未割当・延期 | それを表す特別なラベルと費用を用意する |
| グループ内の連結性 | 必須条件としては直接指定できない |
| 3頂点以上を一度に評価する費用、全体の最大値 | 単項・二項の和へ変形できる場合を除き、そのままは指定できない |
| 割当先により変わる需要量、複数の重み付き資源 | 直接指定できない。件数と1種類の需要量は併用できる |
| ラベル数そのものの決定 | ラベル数を入力する。外側で候補数を変えて比較する形になる |

## 2. APIリファレンス

### 2.1 準備と、API全体の見取り図

GCC系・libstdc++のC++20環境を使います。ヘッダが`<bits/stdc++.h>`、128bit整数、`__INCLUDE_LEVEL__`を利用するため、任意の標準C++処理系への可搬性を保証するものではありません。ACL、追加のハッシュマップ、外部乱数ライブラリは不要です。

各例のプログラムを`main.cpp`、その節の入力例を`input.txt`として保存し、同じフォルダに`capacitated_graph_labeling_v25.hpp`を置きます。

```bash
g++ -std=c++20 -O2 -Wall -Wextra main.cpp -o main
./main < input.txt
```

`-std=c++20`はC++の版、`-O2`は最適化、`-Wall -Wextra`は警告の指定、`-o main`は実行ファイル名です。各例を別々に実行し、複数の`main`を一つのファイルへ連結しないでください。付属ZIPには、このMarkdown、対象ヘッダ、22例の`.cpp`・入力・実行時の出力、検証用コードを収録しています。

掲載した22例はGCC 13.3.0・C++20で警告をエラーとしてコンパイルし、各入力例で実行しました。また、制約・目的値・負荷・件数・部分修復の差分を、別の検証コードで照合しています。この確認は掲載入力に対するもので、任意の入力での成功や最適性を保証するものではありません。

公開型は次の8種類です。`Cost`は費用を表す型で、以下の型名の山括弧内に指定します。

| 公開型 | 役割 | `Cost`の省略 |
|---|---|---|
| `capacitated_graph_labeling_problem<Cost>` | 問題データ | `long long` |
| `capacitated_graph_labeling_edge<Cost>` | 辺1本のデータ | 省略不可 |
| `capacitated_potts_cost<Cost>` | 異なるラベルに辺重みを課す規則 | `long long` |
| `capacitated_label_matrix_cost<Cost>` | 共通のラベル間行列を使う規則 | `long long` |
| `capacitated_graph_labeling_options` | 時間・反復数などの設定 | 型引数なし |
| `capacitated_graph_labeling_status` | 成功・失敗理由の列挙型 | 型引数なし |
| `capacitated_graph_labeling_result<Cost>` | 通常の完全解の結果 | `long long` |
| `capacitated_graph_labeling_repair_result<Cost>` | 指定部分の変更点と差分 | `long long` |

`Cost`は符号付きの数値型を使います。本書では整数の`long long`と小数の`double`を扱います。需要量・容量はCostとは別に`long long`、頂点・ラベル・件数は`int`です。問題型は`std::numeric_limits<Cost>::is_signed`がtrueであることを要求します。必要な数値演算・比較・doubleへの変換ができる型を使い、結果をストリーム出力するときはCost自身も`<<`に対応している必要があります。数値は各型の表現範囲に収め、浮動小数点の費用には有限値だけを使います。

利用する自由関数は5つです。`problem.solve()`というメソッド形式ではありません。

| 関数 | 用途 | 戻り値 |
|---|---|---|
| `solve_capacitated_graph_labeling` | 初期解を構築し、改善する | `capacitated_graph_labeling_result<Cost>` |
| `improve_capacitated_graph_labeling` | 実行可能な完全解を改善する | 同上 |
| `improve_capacitated_graph_labeling_subset` | 完全解のうち指定頂点だけを改善する | `capacitated_graph_labeling_repair_result<Cost>` |
| `is_feasible_capacitated_graph_labeling` | 候補が必須条件を満たすか調べる | `bool` |
| `evaluate_capacitated_graph_labeling` | 候補の費用を計算する | `Cost` |

このほか、2種類の組込み費用規則の`operator()`、結果2種類とstatusを表示する`operator<<`があります。問題型の`edge_type`と費用規則の`is_potts`も公開されています。構造体の通常の初期化・コピーはC++の機能として利用できます。入力データを変えられる専用setterや、内部探索状態を保持する公開エンジンはありません。

### 2.2 問題データ：`capacitated_graph_labeling_problem<Cost>`

Nは`vertex_count`、Kは`label_count`です。配列は`std::vector`で、初期状態はすべて空です。「空の意味」と「要素に0を入れる意味」は同じとは限りません。

| メンバー | 型・サイズ | 値の範囲と意味 | 既定値／空の場合 |
|---|---|---|---|
| `vertex_count` | `int` | N≥0 | 0 |
| `label_count` | `int` | K>0 | 0。使用前に必ず正へ設定 |
| `demand` | `vector<long long>`、空またはN個 | 各頂点の需要量。各値0以上 | 各頂点1 |
| `lower_capacity` | `vector<long long>`、空またはK個 | 総需要量の下限。各値0以上 | 全ラベル0 |
| `upper_capacity` | `vector<long long>`、空またはK個 | 総需要量の上限。各値0以上 | 全ラベルが総需要量の合計 |
| `lower_count` | `vector<int>`、空またはK個 | 件数の下限。各値0〜N | 全ラベル0 |
| `upper_count` | `vector<int>`、空またはK個 | 件数の上限。各値0〜N | 全ラベルN |
| `unary_cost` | `vector<Cost>`、空またはN×K個 | `unary_cost[v*K+a]`が頂点vをラベルaへ置く費用 | 全単項費用0 |
| `edges` | `vector<edge_type>`、M個 | 二項費用を加算する辺の列 | 辺なし |
| `fixed_label` | `vector<int>`、空またはN個 | -1は自由、0〜K-1は固定先 | 全頂点自由 |
| `allowed` | `vector<unsigned char>`、空またはN×K個 | `allowed[v*K+a]`が0なら禁止、非0なら許可 | 全組合せ許可 |

`edge_type`は`capacitated_graph_labeling_edge<Cost>`の別名です。たとえば`capacitated_graph_labeling_problem<long long>::edge_type`で辺型を参照できます。

行優先のN×K表では、頂点0のK個、頂点1のK個、…という順に並べます。N=2、K=3の費用表`{10,11,12,20,21,22}`なら、頂点1・ラベル2の費用は添字`1*3+2=5`の22です。

値を入れるときの重要な条件は次です。

- 明示した容量上限は同じラベルの下限以上にします。件数も下限を上限以下にし、どちらも0〜Nの範囲にします。下限と上限を同じにすれば、その量・件数に固定できます。総需要量に対して下限が大きすぎるなど、全体として両立しない条件には成功解がありません。
- `demand.assign(N,0)`は全頂点の需要量0です。省略時の需要量1とは違います。
- `fixed_label.resize(N)`は全要素0なので全頂点をラベル0に固定します。自由にする配列は`assign(N,-1)`で作ります。
- `allowed.resize(N*K)`は全要素0なので全組合せを禁止します。最初に全許可へ置くなら`assign(N*K,1)`です。
- 固定先が0以上の頂点では、許可表のその行より固定先が優先されます。
- N=0でもKは正です。辺は空にし、需要量・件数の正の下限がなければ、空の割当が実行可能です。

### 2.3 辺：`capacitated_graph_labeling_edge<Cost>`

| メンバー | 型 | 意味・範囲 | 既定値 |
|---|---|---|---|
| `u` | `int` | 第1端点。0〜N-1 | 0 |
| `v` | `int` | 第2端点。0〜N-1 | 0 |
| `weight` | `Cost` | 組込み規則で費用に掛ける重み。負も可 | `Cost{1}` |
| `type` | `int` | intの範囲内の任意の整数。利用者が意味を定める種類番号で、組込み規則は使わない | 0 |

`{u,v,weight,type}`の順で初期化できます。typeを省略した`{u,v,weight}`も使えます。

辺は登録した1本につき1回、`pair_cost(edge_id, edge, label[u], label[v])`で評価されます。第1端点と第2端点の順序は保たれます。

- 同じ端点の辺を複数登録すると、全部の費用を足します。異なる種類の関係を並行して置くこともできます。
- `u==v`という自己辺も登録できます。Potts型では費用0です。一般の費用では同じラベルを両引数に渡した値を1回足します。
- 対称な関係を1回だけ数える問題では、(u,v)と(v,u)を両方登録しません。方向ごとの費用を両方数える問題なら2本を登録します。

### 2.4 二項費用の規則

#### Potts規則：`capacitated_potts_cost<Cost>`

ラベルが同じなら0、異なれば辺のweightを返します。

$$P_ {e}(a,b)=w_ {e}[a\ne b]$$

ここで$w_ {e}$は辺eのweightです。正の重みは同じラベルを好み、負の重みは異なるラベルを好みます。

公開呼出しは`Cost operator()(int edge_id, const capacitated_graph_labeling_edge<Cost>& edge, int label_u, int label_v) const`です。

| 引数／戻り値 | 意味・範囲 | 省略 |
|---|---|---|
| `edge_id` | 探索中は0〜M-1。組込み規則では未使用 | 不可 |
| `edge` | 評価する辺のconst参照。weightを使用 | 不可 |
| `label_u` | 第1端点のラベル。問題では0〜K-1 | 不可 |
| `label_v` | 第2端点のラベル。問題では0〜K-1 | 不可 |
| 戻り値 | 同ラベルなら`Cost{}`、異なれば`edge.weight` | — |

データの設定は不要です。`inline static constexpr bool is_potts=true`という公開定数を持ちます。

#### 行列規則：`capacitated_label_matrix_cost<Cost>`

ラベルの組合せごとの共通行列を使います。

$$P_ {e}(a,b)=w_ {e}Q_ {a,b}$$

$Q_ {a,b}$は行列のa行b列の値です。辺ごとの重み$w_ {e}$は別に指定できます。行列が対称である必要はなく、対角要素が0である必要もありません。

| メンバー | 型 | 意味・範囲 | 既定値 |
|---|---|---|---|
| `label_count` | `int` | 行列の行数・列数。問題のKと同じ正の値 | 0 |
| `matrix` | `const vector<Cost>*` | 少なくともK×K個を持つ費用配列への非nullポインタ。`a*K+b`で参照 | `nullptr` |
| `is_potts` | `inline static constexpr bool` | この規則は一般の行列型であることを示す定数 | `false` |

`capacitated_label_matrix_cost<Cost> cost{K, &matrix};`で使います。初期状態のlabel_count=0、matrix=nullptrのまま評価してはいけません。参照先のvectorと内容は、API呼出しが終わるまで保持します。行列のコピーを内部へ保存する型ではありません。

`operator()`の引数・const性・戻り値型はPotts規則と同じです。`edge_id`と`edge.type`は未使用で、戻り値は`edge.weight * (*matrix)[label_u*label_count+label_v]`です。

#### 自作の費用規則

同じ4引数でCostを返せるconst呼出し可能な関数オブジェクト、ラムダ、関数を渡せます。辺IDで別配列を参照したり、edge.typeで規則を変えたりできます。

| 引数 | 自作規則での用途 |
|---|---|
| `edge_id` | `problem.edges`の添字。辺ごとの補足情報を参照できる |
| `edge` | u、v、weight、typeを読める。ライブラリ側では変更しない |
| `label_u`、`label_v` | 今回評価したい両端ラベル。入力順はu、vの順 |
| 戻り値 | この1本の辺の費用。全辺の合計や容量の罰金を返す場所ではない |

同じ入力には同じ費用を返してください。参照する入力・補足配列を計算中に書き換えたり、全体の現在解に依存する値を返したりしません。負の費用も使えますが、NaN・無限大は使いません。

自作型の`is_potts`は省略するか、`inline static constexpr bool is_potts=false;`というクラスの定数として宣言します。trueを宣言できるのは、**全入力で正確に`edge.weight * [label_u != label_v]`と一致する場合だけ**です。対称な費用、対角が0の行列、同色に罰金を置く費用であるだけではtrueにできません。誤った宣言は、誤った費用評価につながります。

### 2.5 実行設定：`capacitated_graph_labeling_options`

| メンバー | 型 | 値の範囲と意味 | 既定値 |
|---|---|---|---|
| `time_limit_ms` | `double` | 有限値。0以上で相対時間制限を有効、負で無効。単位ms | `1000.0` |
| `iteration_limit` | `long long` | 0以上で目的値改善の提案回数の上限、負で無効 | `-1` |
| `seed` | `uint64_t` | 0から型の最大値までの乱数seed | `1` |
| `initial_trials` | `int` | solveの初期構築の試行数。0以下でも最低1回。時間により途中終了する | `8` |
| `initial_temperature` | `double` | 有限値。負なら自動、非負なら開始温度を指定 | `-1.0` |
| `final_temperature` | `double` | 有限値。負なら自動、非負なら終了温度を指定 | `-1.0` |
| `potts_cache_max_bytes` | `size_t` | Potts差分表などに使うバイト数の予算。0で禁止 | `256ULL << 20`、256MiB |
| `deadline` | `steady_clock::time_point` | 絶対的な締切。同じ値を複数呼出しで共有できる | `time_point::max()`、制限なし |

温度は、探索の途中で一時的な費用増加をどの程度受け入れるかを調整する値です。同じ費用増加なら温度が高いほど受け入れやすく、低いほど受け入れにくくなります。自動指定では問題上の費用差から温度を決めます。途中で費用が悪化しても、返すのは記録した最良解です。キャッシュは、繰り返し使う費用計算の一部をメモリ上の表へ保存して、計算を省く仕組みです。

時間・反復の停止条件は併用できます。有効な条件のどれかに達すると停止を試みます。相対時間は公開探索関数の内部準備から数え、入力データを作る時間は含みません。deadlineは必ず`std::chrono::steady_clock`の時刻を使います。正のtime_limit_msは、この時計のdurationへ変換して現在時刻へ足しても表現範囲を超えない大きさにします。

次の違いに注意してください。

- `iteration_limit=0`でもsolveは初期解を構築し、必要に応じて制約修復も試します。initial_trialsは全体improve・subsetでは使いません。
- `time_limit_ms=0`や期限切れでも、solveは最初の構築を試します。improve・subsetは入力検査と初期化を行い、実行可能な入力なら変更なしで返せます。
- 時間は厳密な上限ではありません。準備処理や間欠的な確認のため超過し得るので、外側の締切には出力などの余裕を残します。
- 相対時間を負、deadlineを最大時点、iteration_limitを負にすると、一般には停止条件がなくなります。少なくとも一つの停止条件を有効にします。
- 温度は費用の尺度に関係します。開始・終了の片方だけを自動にすることもできます。内部では少なくとも`1e-12`となるため、0指定は厳密な山登りの指定ではありません。
- キャッシュ予算は全メモリの上限ではありません。一般の費用規則にはPotts差分表を作りません。予算内でも探索を行わない場合などは`used_potts_cache=false`です。
- 同じseedだけで、時間制限実行の出力まで再現する保証はありません。反復数・入力順・policy・キャッシュ設定・実行環境も結果に関係します。

### 2.6 探索関数の引数と戻り値

以下の`problem`、`pair_cost`、`options`はすべてconst参照です。入力問題や初期ラベル列は書き換えられません。結果のvectorは返された結果が所有します。テンプレートの型Costは問題データから、PairCostは費用規則から自動で推論されるため、通常は関数名に山括弧で型を指定する必要はありません。

#### `solve_capacitated_graph_labeling(problem, pair_cost, options={})`

| 引数 | 型 | 条件・意味 | 既定値 |
|---|---|---|---|
| `problem` | `const capacitated_graph_labeling_problem<Cost>&` | 正しい形・値域の問題データ | なし |
| `pair_cost` | `const PairCost&` | 上記の費用規則。参照先も呼出し中は有効 | なし |
| `options` | `const capacitated_graph_labeling_options&` | 時間・反復・seed等の設定 | `{}` |
| 戻り値 | `capacitated_graph_labeling_result<Cost>` | 成功した最良の完全解、または構築失敗 | — |

初期解は不要です。成功判定は`result.feasible`で行います。失敗は`status=construction_failed`です。この関数のinitial_objectiveは、採用した実行可能初期解の費用です。

#### `improve_capacitated_graph_labeling(problem, initial_label, pair_cost, options={})`

| 引数 | 型 | 条件・意味 | 既定値 |
|---|---|---|---|
| `problem` | `const capacitated_graph_labeling_problem<Cost>&` | 今回の費用・制約を持つ正しい問題 | なし |
| `initial_label` | `const vector<int>&` | 長さNで、今回の問題に対して全体が実行可能な完全解 | なし |
| `pair_cost` | `const PairCost&` | 今回の二項費用規則 | なし |
| `options` | `const capacitated_graph_labeling_options&` | 改善処理の設定 | `{}` |
| 戻り値 | `capacitated_graph_labeling_result<Cost>` | 初期解以下の目的値の完全解、または初期解の不正 | — |

不正な初期解を自動修復する関数ではありません。初期解が不適格なら`feasible=false`、`status=infeasible_initial_solution`です。成功時のinitial_objectiveは、今回のproblemとpair_costで計算した入力解の費用です。以前の問題での費用は引き継ぎません。

#### `improve_capacitated_graph_labeling_subset(problem, initial_label, mutable_vertices, pair_cost, options={})`

| 引数 | 型 | 条件・意味 | 既定値 |
|---|---|---|---|
| `problem` | `const capacitated_graph_labeling_problem<Cost>&` | 範囲の外も含む完全な問題 | なし |
| `initial_label` | `const vector<int>&` | 長さNで全体が実行可能な完全解 | なし |
| `mutable_vertices` | `const vector<int>&` | 変更してよい頂点番号。0〜N-1、重複なし、fixed_labelで固定した頂点なし | なし |
| `pair_cost` | `const PairCost&` | 全辺に適用する二項費用規則 | なし |
| `options` | `const capacitated_graph_labeling_options&` | 改善処理の設定 | `{}` |
| 戻り値 | `capacitated_graph_labeling_repair_result<Cost>` | 変更点と費用差分、または失敗理由 | — |

範囲外の頂点は変更しません。範囲内と範囲外をつなぐ辺の費用、全体の需要量・件数制約も考慮します。入力解が全体で不適格なら、変更範囲の内側だけが問題であっても成功しません。

変更範囲は空でも構いません。その場合も初期解は検査し、実行可能なら変更なしのsuccessを返します。範囲が不正なら`invalid_repair_scope`、範囲は正しいが初期解が不正なら`infeasible_initial_solution`です。両方が不正な場合は範囲のエラーが先になります。

### 2.7 検査と採点

#### `is_feasible_capacitated_graph_labeling(problem, label)`

| 引数／戻り値 | 型 | 条件・意味 | 既定値 |
|---|---|---|---|
| `problem` | `const capacitated_graph_labeling_problem<Cost>&` | 配列サイズや値域が正しい問題 | なし |
| `label` | `const vector<int>&` | 調べたいラベル列。長さ違いや範囲外の番号もfalseになる | なし |
| 戻り値 | `bool` | 長さ・番号・固定・許可・需要量・件数の全条件を満たせばtrue | — |

費用は計算しません。二項費用規則も渡しません。問題データ自体の形式不備をすべてfalseへ変換する関数ではない点に注意してください。

#### `evaluate_capacitated_graph_labeling(problem, label, pair_cost)`

| 引数／戻り値 | 型 | 条件・意味 | 既定値 |
|---|---|---|---|
| `problem` | `const capacitated_graph_labeling_problem<Cost>&` | 有効な問題データ | なし |
| `label` | `const vector<int>&` | 長さN、すべての番号が0〜K-1である列 | なし |
| `pair_cost` | `const PairCost&` | 有効な二項費用規則 | なし |
| 戻り値 | `Cost` | 全単項費用と、登録した全辺の二項費用の合計 | — |

この関数は実行可能性を検査しません。容量違反などがあっても、ラベル番号が有効なら費用は計算されます。候補を採用する場面では、通常is_feasibleを先に呼びます。負のPotts費用へ変換した元の罰金など、外側の定数補正は利用者が加えます。

### 2.8 通常結果：`capacitated_graph_labeling_result<Cost>`

solveと全体improveが返す型です。

| メンバー | 型 | 成功時の意味・範囲 | 初期値 |
|---|---|---|---|
| `label` | `vector<int>` | N個のラベル。各0〜K-1 | 空 |
| `load` | `vector<long long>` | K個のラベル別総需要量 | 空 |
| `count` | `vector<int>` | 件数制約を片方でも指定した場合はK個。両方省略時は空 | 空 |
| `objective` | `Cost` | 返した完全解の目的値 | `Cost{}` |
| `initial_objective` | `Cost` | solveで採用した構築解、またはimproveに渡した初期解の目的値 | `Cost{}` |
| `iterations` | `long long` | 目的値改善の提案反復数。0以上 | 0 |
| `elapsed_ms` | `double` | 内部準備を含む0以上の経過時間。単位ms | 0.0 |
| `feasible` | `bool` | 実行可能な完全解を返せたか | false |
| `used_potts_cache` | `bool` | Potts差分表を実際に作ったか | false |
| `status` | `capacitated_graph_labeling_status` | 成功または失敗理由 | `construction_failed` |

iterationsには、無効な提案・採用しなかった提案も含みます。初期構築・制約修復・温度準備は含みません。成功解の内部目的値は`objective <= initial_objective`です。doubleを外部で再計算するときは丸め差を考慮します。

失敗時にはstatusとelapsed_msを確認できます。他の数値0や空配列は有効な解を意味しません。件数だけ取得したい場合は、`upper_count.assign(K,N)`のような実質的に制限しない件数上限を明示するとcountが入ります。

### 2.9 部分結果：`capacitated_graph_labeling_repair_result<Cost>`

| メンバー | 型 | 成功時の意味・範囲 | 初期値 |
|---|---|---|---|
| `changed_vertices` | `vector<int>` | 変更範囲のうちラベルが変わった頂点番号。0〜N-1、重複なし | 空 |
| `new_labels` | `vector<int>` | 同じ添字の頂点へ設定する0〜K-1の新ラベル。changed_verticesと同じ要素数 | 空 |
| `objective_delta` | `Cost` | 新しい全体目的値−入力解の全体目的値。内部計算では0以下 | `Cost{}` |
| `iterations` | `long long` | 0以上の目的値改善の提案反復数 | 0 |
| `elapsed_ms` | `double` | 内部準備を含む0以上の経過ms | 0.0 |
| `used_potts_cache` | `bool` | Potts差分表を作ったか | false |
| `status` | `capacitated_graph_labeling_status` | 成功または失敗理由 | `invalid_repair_scope` |

この型にはfeasible、完全ラベル列、全体目的値、load、countはありません。statusを確認してから、`label[changed_vertices[i]]=new_labels[i]`を各iへ適用します。変更点の順序に依存せず、返された頂点番号を使ってください。

成功でも改善がなければ、変更点は空で差分0です。同じ費用の別解へ移すためのAPIではありません。差分は今回渡した同じ問題の入力解を基準にし、境界辺も含みます。外側で費用を更新したときは、更新前の目的値にそのまま差分を足してはいけません。

### 2.10 statusのすべての値

`enum class capacitated_graph_labeling_status : unsigned char`です。数値の順番に頼らず、列挙名で比較します。

| 列挙値 | 主に返すAPI | 正確な意味 | 利用側の対応 |
|---|---|---|---|
| `success` | solve・improve・subset | 実行可能な結果。探索0回や変更なしも含む。最適性や希望違反ゼロの保証ではない | 完全解または変更点を利用する |
| `infeasible_initial_solution` | improve・subset | 初期完全解が今回の問題に対して実行可能でない。長さ・番号の不備も含む | 初期解を直すか、solveで構築する |
| `construction_failed` | solve | 実行可能初期解を見つけられなかった。不可能性の証明ではない | 入力条件・時間を確認し、別の初期解や設定を検討する |
| `invalid_repair_scope` | subset | 変更範囲に重複・範囲外・固定頂点のいずれかがある | 範囲を直す。空範囲そのものは不正ではない |

入力問題の壊れた配列サイズや負の需要量は、これらのstatusで受け止める設計ではありません。入力契約に反するとassertで停止したり、不正な参照につながったりします。assertを無効にしても契約は必要です。

### 2.11 デバッグ出力：`operator<<`

| 対象 | シグネチャの形 | 表示 |
|---|---|---|
| 通常結果 | `std::ostream& operator<<(std::ostream&, const capacitated_graph_labeling_result<Cost>&)` | `result{status=..., feasible=..., ...}` |
| subset結果 | `std::ostream& operator<<(std::ostream&, const capacitated_graph_labeling_repair_result<Cost>&)` | `repair_result{status=..., objective_delta=..., ...}` |
| status | `std::ostream& operator<<(std::ostream&, capacitated_graph_labeling_status)` | `success`などの名前 |

第1引数は`std::cout`、`std::cerr`、`std::ostringstream`などの出力先、第2引数は表示対象です。どちらの引数も省略できません。戻り値は同じ出力先への参照なので、`std::cerr << result << '\n';`のようにつなげて書けます。

結果は各フィールド名付きの1行で表示します。真偽値はtrue／falseです。各配列はそれぞれ先頭20要素までで、残りがあれば`... (+6 more)`のように省略個数を示します。空配列は`[]`です。表示が省略されても元のvectorは変わりません。

数値は呼出し側の書式に従い、演算子はflags・precisionを変更しません。改行・flushを明示的に追加しないため、必要なら呼出し側で指定します。失敗結果も表示できますが、全フィールドが有効な解を表すわけではありません。

### 2.12 数値と入力の扱い

Costの積・合計・差分、long longの需要量・容量計算は、中間値も含めて型の範囲へ収めます。頂点数・辺数・隣接情報やN×Kの添字がintに収まり、行列を使う場合はK×Kの添字もintに収まるサイズにします。自動的な飽和演算や、あらゆるオーバーフローの検出を行うAPIではありません。

入力データは、APIを呼んでいない間に変更できます。新しい条件へ変えたら、以前の完全解をis_feasibleで再検査してください。行列や自作費用規則が参照する配列も呼出し中は有効でなければなりません。

## 3. ユースケース

前半は問題の表し方、後半は実際の組込み方を扱います。同じPotts規則でも、正負の意味や元の費用への補正が違う例は区別しています。各例は単独で読み、入力例をそのまま与えて実行できます。

| 節 | ユースケース | 主に扱うAPI・概念 |
|---|---|---|
| 3.01 | 担当者別の費用を最小化する割当 | 単項費用、容量上限、Potts規則、既定設定でsolve |
| 3.02 | 工数と件数を同時に制限する負荷分散 | demand、容量下限、件数の上下限、loadとcount |
| 3.03 | 拠点の所属を固定した均等グラフ分割 | 固定先、単項費用の省略、均等分割 |
| 3.04 | 資格と確定済み担当を持つ割当 | allowed、fixed_labelの優先順位 |
| 3.05 | 衝突を減らす色分けと均等Max-Cut | 負のPotts重み、定数補正、衝突の確認 |
| 3.06 | 同居希望と分離希望が混在するクラスタリング | 正負のPotts重みの混在、edge_type |
| 3.07 | 隣同士の差を抑える段階値ラベリング | ラベル間行列、段階差、参照先の寿命 |
| 3.08 | 方向によって料金が違う拠点配置 | 非対称行列、端点の向き、費用規則の直接呼出し |
| 3.09 | 順序・同時実行・近さの希望を同時に扱う時間帯割当 | 自作ラムダ、edge_id、type、平行辺・自己辺 |
| 3.10 | 設備を場所へ一つずつ置く疎なQAP | 一対一配置、件数の上下限、流量×距離 |
| 3.11 | 受け入れられない仕事を延期する | 特別なラベルによる延期・未割当 |
| 3.12 | 小数で表した消費電力・料金を最小化する | Cost=double、再計算、丸め誤差 |
| 3.13 | 外部で作った候補の検査と採点 | is_feasibleとevaluate |
| 3.14 | 既に使っている割当を全体的に改善する | 全体improve、初期解、initial_objective |
| 3.15 | 確定済み部分を保ち、指定した仕事だけ入れ替える | subset、変更点の適用、objective_delta |
| 3.16 | 費用や制約が変わる次のターンへ解を引き継ぐ | 入力の更新、前回解の再検査と再利用 |
| 3.17 | 複数のseedから最良の成功解を選ぶ | seed、initial_trials、複数回の結果の選択 |
| 3.18 | 共通の締切の中で少しずつ再最適化する | time_limit_ms、deadline、共通時間枠 |
| 3.19 | 目的値の改善を行わず、実行可能初期解を作る | iteration_limit=0、初期構築だけの実行 |
| 3.20 | 温度とキャッシュの設定を比較する | 開始・終了温度、キャッシュ予算、used_potts_cache |
| 3.21 | 失敗理由と空の問題を正しく扱う | 全status、空範囲、0頂点の問題 |
| 3.22 | 長い結果をcout・cerrへそのまま出力する | 結果とstatusのoperator<<、配列の表示上限 |

出力例は掲載コードを実行したものです。時間に関係する値、時間で止める処理の完了回数、同点のラベル列などは環境によって変わり得ます。小さい例で最適値が得られても、任意の問題で最適解を保証するものではありません。


### 3.01 担当者別の費用を最小化する割当

4つの仕事を2人へ割り当てます。同じ仕事でも、得意な担当者へ頼む方が安く済むと考えてください。仕事同士の関係はなく、担当件数と個別費用だけを扱います。

**具体的に解く問題**：各仕事を1人へ割り当て、各担当者を2件以下にして、担当費用の合計を小さくします。

**この例で使うAPI・概念**：単項費用、容量上限、Potts規則、既定設定でsolve。

**入力形式**：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。番号は0始まりです。この例のMは0です。

**入力例**

```text
4 2 0
2 2
0 6
1 4
5 0
4 1
```

**完全なプログラム：`example_01.cpp`**

```cpp
// 解く問題：各仕事を1人へ割り当て、各担当者を2件以下にして、担当費用の合計を小さくします。
// 入力形式：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。
// 番号は0始まりです。
// この例のMは0です。
// 注意：最小限の呼出し形を示しています。
// 実行時間を自分で管理する場合はoptionsを明示します。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 設定を省略すると、相対時間1000ms、seed=1などの既定値を使う。
    const auto r = solve_capacitated_graph_labeling(p, cost);

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }
}
```

**標準出力例**

```text
status=success
objective=2
label 0 0 1 1
load 2 2
```

**利用上の注意**：最小限の呼出し形を示しています。実行時間を自分で管理する場合はoptionsを明示します。


### 3.02 工数と件数を同時に制限する負荷分散

重い仕事と軽い仕事が混ざっています。工数の合計だけでなく、担当件数にも上限がある現場を表します。工数0の確認作業も1件として数えます。

**具体的に解く問題**：工数が2、1、2、0の4仕事を2人へ配り、工数を各2〜3、件数を各2件にします。

**この例で使うAPI・概念**：demand、容量下限、件数の上下限、loadとcount。

**入力形式**：`N K M`、需要量N個、容量下限K個、容量上限K個、件数下限K個、件数上限K個、単項費用N×K個。この例はM=0です。

**入力例**

```text
4 2 0
2 1 2 0
2 2
3 3
2 2
2 2
0 6
1 4
5 0
4 1
```

**完全なプログラム：`example_02.cpp`**

```cpp
// 解く問題：工数が2、1、2、0の4仕事を2人へ配り、工数を各2〜3、件数を各2件にします。
// 入力形式：`N K M`、需要量N個、容量下限K個、容量上限K個、件数下限K個、件数上限K個、単項費用N×K
// 個。
// この例はM=0です。
// 注意：demandは担当先によらず一定です。
// 担当先ごとに処理工数が変わる問題や、CPU・メモリの2資源制約を直接表すAPIではありません。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.demand.resize(n); // 非負の整数工数。0でも件数は1増える。
    for (auto& x : p.demand) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.lower_capacity.resize(k); // 担当先ごとの総工数の下限。
    for (auto& x : p.lower_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.lower_count.resize(k); // 件数の下限。需要量とは独立に数える。
    for (auto& x : p.lower_count) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.upper_count.resize(k); // 件数の上限。0〜nで、下限以上にする。
    for (auto& x : p.upper_count) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // 問題・費用規則・設定から完全解を探す。
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }
}
```

**標準出力例**

```text
status=success
objective=2
label 0 0 1 1
load 3 2
count 2 2
```

**利用上の注意**：demandは担当先によらず一定です。担当先ごとに処理工数が変わる問題や、CPU・メモリの2資源制約を直接表すAPIではありません。


### 3.03 拠点の所属を固定した均等グラフ分割

通信する処理を2台のサーバーへ分けます。別サーバーになった関係にだけ通信費がかかります。一部の処理は設置済みなので移せません。同じ形で、種となる点を固定した領域ラベリングにも使えます。

**具体的に解く問題**：6頂点を3個ずつの2グループへ分け、頂点0を0、頂点5を1に固定して、グループをまたぐ辺重みを小さくします。

**この例で使うAPI・概念**：固定先、単項費用の省略、均等分割。

**入力形式**：`N K M`、固定ラベルN個（-1は自由）、辺M行の`u v weight`。NはKで割り切れる値です。

**入力例**

```text
6 2 5
0 -1 -1 -1 -1 1
0 1 8
1 2 7
2 3 1
3 4 7
4 5 8
```

**完全なプログラム：`example_03.cpp`**

```cpp
// 解く問題：6頂点を3個ずつの2グループへ分け、頂点0を0、頂点5を1に固定して、グループをまたぐ辺重
// みを小さくします。
// 入力形式：`N K M`、固定ラベルN個（-1は自由）、辺M行の`u v weight`。
// NはKで割り切れる値です。
// 注意：グループ内の連結性は制約に含まれません。
// 出力が連結していることを必要とする用途では、別の制約・手法が必要です。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    // 各グループの頂点数を等しくする。
    if (n % k != 0) {
        return 1;
    }

    p.lower_count.assign(k, n / k); // 各ラベルの件数下限。
    p.upper_count = p.lower_count; // 下限と上限を一致させ、件数を固定する。

    p.fixed_label.resize(n); // -1は自由、0〜k-1は固定先。
    for (auto& x : p.fixed_label) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // 問題・費用規則・設定から完全解を探す。
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }
}
```

**標準出力例**

```text
status=success
objective=1
label 0 0 0 1 1 1
load 3 3
count 3 3
```

**利用上の注意**：グループ内の連結性は制約に含まれません。出力が連結していることを必要とする用途では、別の制約・手法が必要です。


### 3.04 資格と確定済み担当を持つ割当

各仕事を扱える担当者が限られ、さらに一部の担当は既に決まっている場合です。単なる費用の好みと、割り当ててはいけない条件を区別します。

**具体的に解く問題**：仕事1は担当者0に割当不可、仕事0は担当者0に固定します。仕事0の許可表が0でも、固定先を優先する仕様を確認します。

**この例で使うAPI・概念**：allowed、fixed_labelの優先順位。

**入力形式**：`N K M`、容量上限、単項費用N×K、固定ラベルN個、許可表N×K個（0は禁止、非0は許可）。M=0です。

**入力例**

```text
4 2 0
2 2
0 6
1 4
5 0
4 1
0 -1 -1 -1
0 1
0 1
1 1
1 1
```

**完全なプログラム：`example_04.cpp`**

```cpp
// 解く問題：仕事1は担当者0に割当不可、仕事0は担当者0に固定します。
// 仕事0の許可表が0でも、固定先を優先する仕様を確認します。
// 入力形式：`N K M`、容量上限、単項費用N×K、固定ラベルN個、許可表N×K個（0は禁止、非0は許可）。
// M=0です。
// 注意：固定と許可の共通部分を取る仕様ではありません。
// 業務上それが望ましくない場合は、入力作成時に矛盾を検出してください。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.fixed_label.resize(n); // 固定しない頂点には-1を入れる。
    for (auto& x : p.fixed_label) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.allowed.resize(n * k); // 頂点v・ラベルaの許可はv*k+aに置く。
    for (auto& x : p.allowed) { // 頂点順・ラベル順に設定する。
        // unsigned charへ直接読むと文字入力になるため、整数で読む。
        int allowed = 0;
        std::cin >> allowed;
        x = static_cast<unsigned char>(allowed != 0); // 0は禁止、1は許可。
    }

    // fixed_label[v]>=0の頂点では、許可表より固定先が優先される。

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // 問題・費用規則・設定から完全解を探す。
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }
}
```

**標準出力例**

```text
status=success
objective=8
label 0 1 1 0
load 2 2
```

**利用上の注意**：固定と許可の共通部分を取る仕様ではありません。業務上それが望ましくない場合は、入力作成時に矛盾を検出してください。


### 3.05 衝突を減らす色分けと均等Max-Cut

同時に動かすと干渉する機器を、別の色や時間帯へ分けます。完全に衝突を消せない場合も、衝突の重さの合計を小さくしたい問題です。

**具体的に解く問題**：4頂点を2色に2個ずつ分け、同じ色になった辺の罰金を最小化します。2色では、別の色になる辺重みを最大化する均等Max-Cutとも同じです。

**この例で使うAPI・概念**：負のPotts重み、定数補正、衝突の確認。

**入力形式**：`N K M`、辺M行の`u v 非負の衝突罰金`。NはKで割り切れる値です。

**入力例**

```text
4 2 4
0 1 5
1 2 1
2 3 5
3 0 1
```

**完全なプログラム：`example_05.cpp`**

```cpp
// 解く問題：4頂点を2色に2個ずつ分け、同じ色になった辺の罰金を最小化します。
// 2色では、別の色になる辺重みを最大化する均等Max-Cutとも同じです。
// 入力形式：`N K M`、辺M行の`u v 非負の衝突罰金`。
// NはKで割り切れる値です。
// 注意：feasible=trueでも衝突ゼロとは限りません。
// 厳密な彩色が必要なら、最後にconflict_penaltyが0であることも確認します。
// Kを3以上にすると多色の例になります。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    // この例では各色の頂点数を等しくする。
    if (n % k != 0) {
        return 1;
    }

    p.lower_count.assign(k, n / k); // 各色の件数下限。
    p.upper_count = p.lower_count; // 各色をちょうどn/k個にする。

    Cost all_penalties = 0; // 元の罰金へ戻すための定数。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // 関係を1本表す。
        Cost w = 0;
        std::cin >> e.u >> e.v >> w; // 同色ならw、異色なら0の罰金にしたい。

        if (w < 0) { // 元の罰金は非負とする。
            return 1;
        }

        all_penalties += w;
        e.weight = -w; // 異色を-wだけ有利にするPotts費用へ変換する。
        p.edges.push_back(e); // 重複入力すると罰金も重複して数える。
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // 問題・費用規則・設定から完全解を探す。
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }

    // 定数を戻し、元の同色罰金を表示する。
    std::cout << "conflict_penalty=" << all_penalties + r.objective << '\n';
}
```

**標準出力例**

```text
status=success
objective=-12
label 0 1 0 1
load 2 2
count 2 2
conflict_penalty=0
```

**利用上の注意**：feasible=trueでも衝突ゼロとは限りません。厳密な彩色が必要なら、最後にconflict_penaltyが0であることも確認します。Kを3以上にすると多色の例になります。


### 3.06 同居希望と分離希望が混在するクラスタリング

「この2件は同じ担当にしたい」と「この2件は別担当にしたい」という希望が混在します。希望を破る罰金の重さを使い、すべてを満たせない場合にも総罰金を抑えます。

**具体的に解く問題**：4対象を2クラスタへ2個ずつ分け、同居希望・分離希望に反した罰金の合計を最小化します。

**この例で使うAPI・概念**：正負のPotts重みの混在、edge_type。

**入力形式**：`N K M`、関係M行の`u v 非負の罰金 same`。same=1は同居希望、0は分離希望です。

**入力例**

```text
4 2 4
0 1 7 1
2 3 7 1
0 2 5 0
1 3 5 0
```

**完全なプログラム：`example_06.cpp`**

```cpp
// 解く問題：4対象を2クラスタへ2個ずつ分け、同居希望・分離希望に反した罰金の合計を最小化します。
// 入力形式：`N K M`、関係M行の`u v 非負の罰金 same`。
// same=1は同居希望、0は分離希望です。
// 注意：クラスタ数Kは入力で決めます。
// クラスタ数そのものを自動選択するAPIではありません。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    // この例ではクラスタの頂点数を等しくする。
    if (n % k != 0) {
        return 1;
    }

    p.lower_count.assign(k, n / k); // 件数の下限。
    p.upper_count = p.lower_count; // 件数の上限も同じにする。

    Cost correction = 0; // 分離希望を負の重みに変える際の定数補正。
    for (int i = 0; i < m; ++i) {
        // 問題型にあるedge_typeという別名でも辺型を参照できる。
        capacitated_graph_labeling_problem<Cost>::edge_type e;
        Cost w = 0;
        int same = 0;
        std::cin >> e.u >> e.v >> w >> same; // 罰金と、同居希望かを読む。

        if (w < 0 || (same != 0 && same != 1)) {
            return 1;
        }

        e.weight = same ? w : -w; // 同居希望は異色に+w、分離希望は異色に-w。
        if (!same) {
            correction += w; // w[同色]=w-w[異色]なので定数wを戻す。
        }
        p.edges.push_back(e); // 正負の関係を同じ配列へ登録する。
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // 問題・費用規則・設定から完全解を探す。
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }

    // 元の希望違反の総罰金へ戻す。
    std::cout << "original_penalty=" << correction + r.objective << '\n';
}
```

**標準出力例**

```text
status=success
objective=-10
label 1 1 0 0
load 2 2
count 2 2
original_penalty=0
```

**利用上の注意**：クラスタ数Kは入力で決めます。クラスタ数そのものを自動選択するAPIではありません。


### 3.07 隣同士の差を抑える段階値ラベリング

測定地点へ低・中・高の3段階を付けます。各地点の観測に合うことと、隣り合う地点で急に段階が変わらないことを同時に評価します。

**具体的に解く問題**：3地点に0、1、2の段階を付け、地点別費用と、隣接地点間の段階差の費用を小さくします。

**この例で使うAPI・概念**：ラベル間行列、段階差、参照先の寿命。

**入力形式**：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。番号は0始まりです。辺のweightは段階差1あたりの費用です。

**入力例**

```text
3 3 2
3 3 3
0 4 9
4 0 4
9 4 0
0 1 2
1 2 2
```

**完全なプログラム：`example_07.cpp`**

```cpp
// 解く問題：3地点に0、1、2の段階を付け、地点別費用と、隣接地点間の段階差の費用を小さくします。
// 入力形式：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。
// 番号は0始まりです。
// 辺のweightは段階差1あたりの費用です。
// 注意：段階差は共通行列で指定します。
// 各辺の実費用はweight×matrix[a*K+b]です。
// 行列は対称でなくても使えます。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // a行b列を、段階aとbの差にする。
    std::vector<Cost> matrix(k * k);
    for (int a = 0; a < k; ++a) {
        for (int b = 0; b < k; ++b) {
            matrix[a * k + b] = std::abs(a - b);
        }
    }

    // ラベル数と、行列へのポインタを渡す。
    // matrixはAPI呼出し終了まで保持し、途中で書き換えない。
    const capacitated_label_matrix_cost<Cost> cost{k, &matrix};

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // 問題・費用規則・設定から完全解を探す。
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }
}
```

**標準出力例**

```text
status=success
objective=4
label 0 1 2
load 1 1 1
```

**利用上の注意**：段階差は共通行列で指定します。各辺の実費用はweight×matrix[a*K+b]です。行列は対称でなくても使えます。


### 3.08 方向によって料金が違う拠点配置

拠点0から1への転送と、1から0への転送で料金が違う場合です。辺の第1端点と第2端点を区別した費用を使います。

**具体的に解く問題**：3処理を2拠点へ置き、個別配置費用と、方向付き通信量×拠点間料金を小さくします。

**この例で使うAPI・概念**：非対称行列、端点の向き、費用規則の直接呼出し。

**入力形式**：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。番号は0始まりです。末尾にK行×K個の料金行列を追加します。行は送信元、列は送信先です。

**入力例**

```text
3 2 2
2 2
0 6
2 0
4 0
0 1 3
1 2 2
0 1
4 0
```

**完全なプログラム：`example_08.cpp`**

```cpp
// 解く問題：3処理を2拠点へ置き、個別配置費用と、方向付き通信量×拠点間料金を小さくします。
// 入力形式：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。
// 番号は0始まりです。
// 末尾にK行×K個の料金行列を追加します。
// 行は送信元、列は送信先です。
// 注意：辺(u,v)を(v,u)へ入れ替えると別の問題になります。
// 往復を両方評価したい場合だけ2本を登録します。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    // 送信元a・送信先bの単価をa*k+bへ読む。
    std::vector<Cost> matrix(k * k);
    for (auto& x : matrix) {
        std::cin >> x;
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // 非対称行列をそのまま使う。参照先は呼出し終了まで保持する。
    const capacitated_label_matrix_cost<Cost> cost{k, &matrix};

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // 問題・費用規則・設定から完全解を探す。
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }

    if (!p.edges.empty()) { // 辺がある場合だけ、1本分の費用も表示する。
        const auto& e = p.edges[0]; // 第1辺のuとvの順序を保つ。

        // 費用規則は、辺ID・辺・両端ラベルから直接呼び出せる。
        std::cout << "first_edge_cost="
                  << cost(0, e, r.label[e.u], r.label[e.v]) // u,vの順で採点する。
                  << '\n';
    }
}
```

**標準出力例**

```text
status=success
objective=3
label 0 1 1
load 1 2
first_edge_cost=3
```

**利用上の注意**：辺(u,v)を(v,u)へ入れ替えると別の問題になります。往復を両方評価したい場合だけ2本を登録します。


### 3.09 順序・同時実行・近さの希望を同時に扱う時間帯割当

作業を時間帯へ置きます。作業対ごとに、同時刻を避けたい、先に終わらせたい、近い時刻に置きたい、という異なる希望があります。

**具体的に解く問題**：4作業を3時間帯へ割り当て、各時間帯2件以下で、種類別の希望違反を小さくします。希望は罰金であり、必須の先行制約ではありません。

**この例で使うAPI・概念**：自作ラムダ、edge_id、type、平行辺・自己辺。

**入力形式**：`N K M`、容量上限、単項費用N×K、関係M行の`u v weight type gap`。type=0は同時刻の罰金、1はuからvへgap以上空ける希望、2は時刻差の罰金です。

**入力例**

```text
4 3 5
2 2 2
0 3 7
3 0 4
6 2 0
0 1 5
0 1 4 1 1
1 2 4 1 1
0 3 2 0 0
0 1 1 2 0
2 2 1 2 0
```

**完全なプログラム：`example_09.cpp`**

```cpp
// 解く問題：4作業を3時間帯へ割り当て、各時間帯2件以下で、種類別の希望違反を小さくします。
// 希望は罰金であり、必須の先行制約ではありません。
// 入力形式：`N K M`、容量上限、単項費用N×K、関係M行の`u v weight type gap`。
// type=0は同時刻の罰金、1はuからvへgap以上空ける希望、2は時刻差の罰金です。
// 注意：単項費用・二項費用の和で表せる希望をまとめています。
// gapsと入力問題は計算中に変更しません。
// 連続時間や作業長の重なりを直接管理するスケジューラではありません。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 辺ごとの時間帯差を、辺IDと同じ順序で保持する。
    std::vector<int> gaps(m);
    for (int id = 0; id < m; ++id) {
        capacitated_graph_labeling_edge<Cost> e; // typeの意味は利用者が決める。
        std::cin >> e.u >> e.v >> e.weight >> e.type >> gaps[id]; // 1辺を読む。

        // この例では、非負の重み・時間差、種類0〜2を使う。
        if (e.weight < 0 || e.type < 0 || e.type > 2 || gaps[id] < 0) {
            return 1;
        }
        p.edges.push_back(e); // 平行辺・自己辺も1本ずつ評価される。
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // const呼出し可能な費用規則。同じ引数には同じ値を返す。
    const auto cost = [&](
        int id, // p.edgesの添字。gapsもこの添字で参照する。
        const capacitated_graph_labeling_edge<Cost>& e, // 今回評価する辺。
        int a, // e.uのラベル。
        int b  // e.vのラベル。
    ) -> Cost {
        if (e.type == 0) { // 同時刻に置くと罰金。
            return a == b ? e.weight : Cost{0}; // 辺のweightを罰金にする。
        }

        if (e.type == 1) { // uからvへ指定以上の時間差を空けたい。
            return e.weight * std::max(0LL, 1LL * a + gaps[id] - b); // 不足量へ罰金。
        }

        return e.weight * std::abs(a - b); // 時刻差の罰金。自己辺の差は0。
    };
    // ラムダにはis_pottsを付けず、一般の二項費用として渡す。

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // 問題・費用規則・設定から完全解を探す。
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }
}
```

**標準出力例**

```text
status=success
objective=2
label 0 1 2 1
load 1 2 1
```

**利用上の注意**：単項費用・二項費用の和で表せる希望をまとめています。gapsと入力問題は計算中に変更しません。連続時間や作業長の重なりを直接管理するスケジューラではありません。


### 3.10 設備を場所へ一つずつ置く疎なQAP

複数の設備を同じ数の場所へ置きます。頻繁にやり取りする設備同士を近い場所に置くと、運搬費を減らせます。

**具体的に解く問題**：3設備を3場所へ1対1に配置し、設備間の流量×場所間距離を最小化します。

**この例で使うAPI・概念**：一対一配置、件数の上下限、流量×距離。

**入力形式**：`N K M`（K=N）、M行の`設備u 設備v 流量`、K行×K個の場所間距離。各設備対は評価したい回数だけ登録します。

**入力例**

```text
3 3 3
0 1 4
1 2 3
0 2 1
0 1 3
1 0 2
3 2 0
```

**完全なプログラム：`example_10.cpp`**

```cpp
// 解く問題：3設備を3場所へ1対1に配置し、設備間の流量×場所間距離を最小化します。
// 入力形式：`N K M`（K=N）、M行の`設備u 設備v 流量`、K行×K個の場所間距離。
// 各設備対は評価したい回数だけ登録します。
// 注意：距離と流量が対称でも、(u,v)と(v,u)を両方登録すれば2回加算されます。
// 問題の定義に合わせて登録本数を決めます。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    if (k != n) { // この例は、設備数と場所数が同じ一対一配置。
        return 1;
    }

    p.lower_count.assign(k, 1); // 各場所に少なくとも1設備。
    p.upper_count.assign(k, 1); // 各場所に高々1設備。合わせてちょうど1設備。

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    // 場所aと場所bの距離をa*k+bへ読む。
    std::vector<Cost> distance(k * k);
    for (auto& x : distance) {
        std::cin >> x;
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // 辺のweightを流量とし、流量×距離を費用にする。
    const capacitated_label_matrix_cost<Cost> cost{k, &distance};

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // 問題・費用規則・設定から完全解を探す。
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }
}
```

**標準出力例**

```text
status=success
objective=13
label 0 1 2
load 1 1 1
count 1 1 1
```

**利用上の注意**：距離と流量が対称でも、(u,v)と(v,u)を両方登録すれば2回加算されます。問題の定義に合わせて登録本数を決めます。


### 3.11 受け入れられない仕事を延期する

全仕事を今すぐ処理できない場合です。「未割当」を特別なラベルとして用意し、そのラベルへ置くと延期費用がかかるようにします。

**具体的に解く問題**：4仕事に対して実担当者は2人、各1件までです。残りを延期先ラベル2へ置き、担当費用と延期費用の合計を小さくします。

**この例で使うAPI・概念**：特別なラベルによる延期・未割当。

**入力形式**：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。番号は0始まりです。Kは実担当者数+1。最後のラベルを延期先とし、その容量をN以上にします。M=0です。

**入力例**

```text
4 3 0
1 1 4
0 8 20
1 6 20
7 0 20
6 1 20
```

**完全なプログラム：`example_11.cpp`**

```cpp
// 解く問題：4仕事に対して実担当者は2人、各1件までです。
// 残りを延期先ラベル2へ置き、担当費用と延期費用の合計を小さくします。
// 入力形式：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。
// 番号は0始まりです。
// Kは実担当者数+1。
// 最後のラベルを延期先とし、その容量をN以上にします。
// M=0です。
// 注意：ライブラリの上では延期先も普通のラベルです。
// 辺があるモデルでは、延期先との二項費用も意図どおりになるよう行列や自作policyで定義します。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // 問題・費用規則・設定から完全解を探す。
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }

    // 最後のラベルを、業務上の「延期」と読み替える。
    std::cout << "deferred";
    for (int v = 0; v < n; ++v) {
        if (r.label[v] == k - 1) { // 延期先へ割り当てられた仕事。
            std::cout << ' ' << v;
        }
    }
    std::cout << '\n';
}
```

**標準出力例**

```text
status=success
objective=40
label 0 2 1 2
load 1 1 2
deferred 1 3
```

**利用上の注意**：ライブラリの上では延期先も普通のラベルです。辺があるモデルでは、延期先との二項費用も意図どおりになるよう行列や自作policyで定義します。


### 3.12 小数で表した消費電力・料金を最小化する

料金や消費電力に小数がある場合です。工数・容量は整数のまま、費用だけをdoubleにします。返された値と再計算値は、丸め誤差を許して比べます。

**具体的に解く問題**：4仕事を2担当先へ各2件まで割り当て、単項料金と引継ぎ料金の合計を小さくします。

**この例で使うAPI・概念**：Cost=double、再計算、丸め誤差。

**入力形式**：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。番号は0始まりです。単項費用と辺重みには有限の小数を入力できます。

**入力例**

```text
4 2 2
2 2
0 0.6
0.1 0.4
0.5 0
0.4 0.1
0 2 0.5
1 3 0.5
```

**完全なプログラム：`example_12.cpp`**

```cpp
// 解く問題：4仕事を2担当先へ各2件まで割り当て、単項料金と引継ぎ料金の合計を小さくします。
// 入力形式：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。
// 番号は0始まりです。
// 単項費用と辺重みには有限の小数を入力できます。
// 注意：NaN・無限大は使いません。
// 誤差の許容幅は費用の大きさと用途に応じて決めます。
// 需要量を小数にしたい場合は、意味を保つ整数単位へ換算します。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = double;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // 問題・費用規則・設定から完全解を探す。
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);

    // 表示の桁数だけを指定し、内部の費用は丸めない。
    std::cout << std::fixed << std::setprecision(6);

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }

    // 全頂点・全辺から費用を再計算する。
    const Cost recomputed = evaluate_capacitated_graph_labeling(p, r.label, cost);

    // この小さな例の許容誤差。費用の大きさに合わせる。
    const Cost tolerance = 1e-9 * std::max({
        1.0,
        std::abs(recomputed),
        std::abs(r.objective) // 返された目的値の大きさも使う。
    });

    std::cout << "recomputed=" << recomputed << '\n';

    // 小数の再計算値は、完全一致だけでは判定しない。
    std::cout << "consistent="
              << (std::abs(recomputed - r.objective) <= tolerance) // 丸め差を許容する。
              << '\n';
}
```

**標準出力例**

```text
status=success
objective=1.000000
label 0 1 0 1
load 2 2
recomputed=1.000000
consistent=1
```

**利用上の注意**：NaN・無限大は使いません。誤差の許容幅は費用の大きさと用途に応じて決めます。需要量を小数にしたい場合は、意味を保つ整数単位へ換算します。


### 3.13 外部で作った候補の検査と採点

人手や別の処理が作った割当を受け取り、採用できるかと費用を別々に調べます。容量違反の候補に対しても目的値だけは計算できてしまうため、検査を先に行います。

**具体的に解く問題**：4仕事の候補を2本読み、制約を満たす候補だけを採点します。一方は有効、もう一方は担当0へ集中して容量違反です。

**この例で使うAPI・概念**：is_feasibleとevaluate。

**入力形式**：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。番号は0始まりです。末尾に候補数Q、続けて各候補のラベルN個を入力します。

**入力例**

```text
4 2 2
2 2
0 6
1 4
5 0
4 1
0 2 5
1 3 5
2
0 0 1 1
0 0 0 0
```

**完全なプログラム：`example_13.cpp`**

```cpp
// 解く問題：4仕事の候補を2本読み、制約を満たす候補だけを採点します。
// 一方は有効、もう一方は担当0へ集中して容量違反です。
// 入力形式：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。
// 番号は0始まりです。
// 末尾に候補数Q、続けて各候補のラベルN個を入力します。
// 注意：is_feasibleは問題データ自体の不正をすべてfalseで受け止める関数ではありません。
// 問題の配列サイズなどは入力契約を守ります。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 検査する候補数を読み込む。
    int q = 0;
    std::cin >> q;
    if (!std::cin || q < 0) {
        return 1;
    }

    for (int i = 0; i < q; ++i) {
        // 外部で作った完全ラベル列を読み込む。
        std::vector<int> label(n);
        for (int& a : label) {
            std::cin >> a;
        }
        if (!std::cin) {
            return 1;
        }

        // ラベル番号・許可・固定・容量・件数を検査する。
        const bool ok = is_feasible_capacitated_graph_labeling(p, label);
        std::cout << "candidate=" << i << " feasible=" << ok;

        if (ok) { // 費用が安くても、制約を満たさない候補は採用しない。
            // 費用を全件再計算する。
            std::cout << " objective="
                      << evaluate_capacitated_graph_labeling(p, label, cost); // 候補の費用。
        }
        std::cout << '\n';
    }
}
```

**標準出力例**

```text
candidate=0 feasible=1 objective=12
candidate=1 feasible=0
```

**利用上の注意**：is_feasibleは問題データ自体の不正をすべてfalseで受け止める関数ではありません。問題の配列サイズなどは入力契約を守ります。


### 3.14 既に使っている割当を全体的に改善する

実行可能な割当が既にあるなら、その割当を出発点にします。元の配列を保持したまま改善候補を受け取れるため、既存システムへ段階的に組み込めます。

**具体的に解く問題**：初期割当0,0,1,1の費用12を出発点に、容量を守りながら全仕事の担当を改善します。

**この例で使うAPI・概念**：全体improve、初期解、initial_objective。

**入力形式**：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。番号は0始まりです。末尾に初期ラベルN個を追加します。

**入力例**

```text
4 2 2
2 2
0 6
1 4
5 0
4 1
0 2 5
1 3 5
0 0 1 1
```

**完全なプログラム：`example_14.cpp`**

```cpp
// 解く問題：初期割当0,0,1,1の費用12を出発点に、容量を守りながら全仕事の担当を改善します。
// 入力形式：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。
// 番号は0始まりです。
// 末尾に初期ラベルN個を追加します。
// 注意：返却解は内部で管理する目的値が入力解以下のものです。
// 初期解を作る回数initial_trialsは、このAPIでは使いません。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    // 初期解は長さnの完全ラベル列として読み込む。
    std::vector<int> initial(n);
    for (int& a : initial) {
        std::cin >> a;
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // 不正な初期解は修復せず、失敗を返す。
    const auto r = improve_capacitated_graph_labeling(p, initial, cost, opt);

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }

    // 今回の問題で測った入力解の費用を表示する。
    std::cout << "initial_objective=" << r.initial_objective << '\n';

    // APIは入力したinitialを書き換えない。
    std::cout << "input_unchanged";
    for (int a : initial) {
        std::cout << ' ' << a;
    }
    std::cout << '\n';
}
```

**標準出力例**

```text
status=success
objective=10
label 0 1 0 1
load 2 2
initial_objective=12
input_unchanged 0 0 1 1
```

**利用上の注意**：返却解は内部で管理する目的値が入力解以下のものです。初期解を作る回数initial_trialsは、このAPIでは使いません。


### 3.15 確定済み部分を保ち、指定した仕事だけ入れ替える

全体を毎回変えると、現場の手戻りが大きくなります。変更してよい仕事だけを指定し、外側の仕事との関係と全体の容量も含めて改善します。

**具体的に解く問題**：完全解0,0,1,1のうち仕事1と2だけを変更します。仕事0と3は動かさず、境界の辺費用を含めて費用を減らします。

**この例で使うAPI・概念**：subset、変更点の適用、objective_delta。

**入力形式**：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。番号は0始まりです。末尾に初期ラベルN個、変更可能頂点数S、続けてS個の頂点番号を入力します。

**入力例**

```text
4 2 2
2 2
0 6
1 4
5 0
4 1
0 2 5
1 3 5
0 0 1 1
2
1 2
```

**完全なプログラム：`example_15.cpp`**

```cpp
// 解く問題：完全解0,0,1,1のうち仕事1と2だけを変更します。
// 仕事0と3は動かさず、境界の辺費用を含めて費用を減らします。
// 入力形式：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。
// 番号は0始まりです。
// 末尾に初期ラベルN個、変更可能頂点数S、続けてS個の頂点番号を入力します。
// 注意：範囲の外だけを固定した別問題に作り替える必要はありません。
// scopeが空なら、実行可能な入力に対して変更なしのsuccessです。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    // 初期解は長さnの完全ラベル列として読み込む。
    std::vector<int> initial(n);
    for (int& a : initial) {
        std::cin >> a;
    }

    // 変更してよい頂点の個数と番号を読み込む。
    int s = 0;
    std::cin >> s;
    if (!std::cin || s < 0) {
        return 1;
    }

    // 重複・範囲外・fixed_labelで固定された頂点は含めない。
    std::vector<int> scope(s);
    for (int& v : scope) {
        std::cin >> v;
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // 初期解は、範囲外も含めた全体で実行可能であること。
    const auto repair = improve_capacitated_graph_labeling_subset(
        p,       // 範囲外も含む問題全体。
        initial, // 実行可能な完全ラベル列。
        scope,   // 変更してよい頂点の列。
        cost,    // 二項費用の規則。
        opt      // 探索の設定。
    );

    std::cout << "status=" << repair.status << '\n'; // subset結果にはfeasibleがない。
    if (repair.status != capacitated_graph_labeling_status::success) { // 成功を確認する。
        return 0; // 失敗時には変更点を適用しない。
    }

    // 入力は変わらないため、採用したい変更を自分で反映する。
    auto updated = initial;
    for (std::size_t i = 0; i < repair.changed_vertices.size(); ++i) { // 変更点を順に読む。
        // 同じ添字の頂点番号と新ラベルが対応する。
        updated[repair.changed_vertices[i]] = repair.new_labels[i];
    }

    // 差分の基準は、同じ問題で測った元の解。
    const Cost before = evaluate_capacitated_graph_labeling(p, initial, cost);
    const Cost after = evaluate_capacitated_graph_labeling(p, updated, cost); // 再計算。

    std::cout << "before=" << before
              << " delta=" << repair.objective_delta // 負の差分が改善。
              << " after=" << after << '\n';

    std::cout << "label";
    for (int a : updated) {
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    // 整数費用なので、再計算値と差分が一致することを確認する。
    if (!is_feasible_capacitated_graph_labeling(p, updated) ||
        after != before + repair.objective_delta) { // 返された差分との整合を確認する。
        return 1;
    }
}
```

**標準出力例**

```text
status=success
before=12 delta=-2 after=10
label 0 1 0 1
```

**利用上の注意**：範囲の外だけを固定した別問題に作り替える必要はありません。scopeが空なら、実行可能な入力に対して変更なしのsuccessです。


### 3.16 費用や制約が変わる次のターンへ解を引き継ぐ

新しい料金や担当不可の情報が届くたびに割当を更新します。前の割当を新しい条件で検査し、まだ使えるなら改善、使えなくなったら作り直します。

**具体的に解く問題**：最初に解を作り、仕事0の費用を変更して改善します。その後、仕事1の現在の担当先を禁止し、再構築へ切り替えます。

**この例で使うAPI・概念**：入力の更新、前回解の再検査と再利用。

**入力形式**：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。番号は0始まりです。末尾に費用を変える頂点v、続けてその頂点の新しい費用K個、さらに現在の割当を禁止する頂点を入力します。

**入力例**

```text
4 2 2
2 2
0 6
1 4
5 0
4 1
0 2 5
1 3 5
0 10 0
1
```

**完全なプログラム：`example_16.cpp`**

```cpp
// 解く問題：最初に解を作り、仕事0の費用を変更して改善します。
// その後、仕事1の現在の担当先を禁止し、再構築へ切り替えます。
// 入力形式：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。
// 番号は0始まりです。
// 末尾に費用を変える頂点v、続けてその頂点の新しい費用K個、さらに現在の割当を禁止する頂点を入力
// します。
// 注意：以前の費用との差と、今の問題での改善差は別です。
// initial_objectiveやobjective_deltaは、今回渡した問題に基づきます。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    // 費用を変更する頂点と、その頂点の新しい費用を読む。
    int edited = 0;
    int forbidden = 0;
    std::cin >> edited;

    std::vector<Cost> next_cost(k);
    for (auto& x : next_cost) {
        std::cin >> x;
    }
    std::cin >> forbidden; // 次に、その時点の担当先を禁止する頂点。

    if (!std::cin || edited < 0 || edited >= n ||
        forbidden < 0 || forbidden >= n) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // まず現在の問題で完全解を作る。
    auto r = solve_capacitated_graph_labeling(p, cost, opt);
    if (!r.feasible) { // 失敗解は再利用しない。
        std::cout << r.status << '\n'; // 失敗理由を表示する。
        return 0;
    }
    std::cout << "first=" << r.objective << '\n'; // 最初の問題での目的値。

    // API呼出しの間で入力費用を更新する。
    for (int a = 0; a < k; ++a) {
        p.unary_cost[edited * k + a] = next_cost[a]; // 頂点editedの費用だけを更新。
    }
    const auto previous = r.label; // 前ターンの完全解を保存する。

    // 費用変更だけなら制約上は引き継げる。新条件で必ず検査する。
    if (is_feasible_capacitated_graph_labeling(p, previous)) {
        // 新しい費用で既存解を改善する。
        r = improve_capacitated_graph_labeling(p, previous, cost, opt);
    } else {
        r = solve_capacitated_graph_labeling(p, cost, opt); // 不適格なら再構築する。
    }

    if (!r.feasible) { // 再最適化の成功も確認する。
        std::cout << r.status << '\n'; // 失敗理由を表示する。
        return 0;
    }
    std::cout << "after_cost_update=" << r.objective << '\n'; // 更新後の目的値。

    // 全許可から始める。既存の許可表がある用途ではその条件を維持する。
    p.allowed.assign(n * k, 1);
    p.allowed[forbidden * k + r.label[forbidden]] = 0; // 現在の担当先を禁止する。

    // 新しい制約でも、前回解を必ず再検査する。
    const bool reusable = is_feasible_capacitated_graph_labeling(p, r.label);
    std::cout << "reusable=" << reusable << '\n';

    if (reusable) {
        // 有効な完全解だけをimproveへ渡す。
        r = improve_capacitated_graph_labeling(p, r.label, cost, opt);
    } else {
        // 無効な初期解はimproveで修復されないため、作り直す。
        r = solve_capacitated_graph_labeling(p, cost, opt);
    }

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }
}
```

**標準出力例**

```text
first=10
after_cost_update=5
reusable=0
status=success
objective=20
label 0 1 0 1
load 2 2
```

**利用上の注意**：以前の費用との差と、今の問題での改善差は別です。initial_objectiveやobjective_deltaは、今回渡した問題に基づきます。


### 3.17 複数のseedから最良の成功解を選ぶ

同じ時間や反復数でも、出発点によって結果が変わることがあります。複数回試して、成功した解の中で目的値が最小のものを残します。

**具体的に解く問題**：同じ4仕事の問題をseed=1,2,3で解き、最良の実行可能解を採用します。

**この例で使うAPI・概念**：seed、initial_trials、複数回の結果の選択。

**入力形式**：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。番号は0始まりです。末尾に試行回数R（1以上）を入力します。

**入力例**

```text
4 2 2
2 2
0 6
1 4
5 0
4 1
0 2 5
1 3 5
3
```

**完全なプログラム：`example_17.cpp`**

```cpp
// 解く問題：同じ4仕事の問題をseed=1,2,3で解き、最良の実行可能解を採用します。
// 入力形式：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。
// 番号は0始まりです。
// 末尾に試行回数R（1以上）を入力します。
// 注意：自動で全runをまとめて止める公開APIではありません。
// 総時間を制限したい場合は、同じdeadlineを各呼出しへ渡します。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    int runs = 0;
    std::cin >> runs;
    if (!std::cin || runs <= 0) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    opt.initial_trials = 2; // 1回のsolve内の初期構築数。外側のrunsとは別。

    // 初期状態はfeasible=false。目的値0を成功解として扱わない。
    capacitated_graph_labeling_result<Cost> best;
    int succeeded = 0;

    for (int i = 0; i < runs; ++i) {
        opt.seed = static_cast<std::uint64_t>(i) + 1; // 試行ごとに乱数列を変える。
        // 反復上限は1回ごとに適用される。
        const auto candidate = solve_capacitated_graph_labeling(p, cost, opt);

        if (!candidate.feasible) { // 失敗した回の目的値は比較しない。
            continue;
        }
        ++succeeded;

        if (!best.feasible || candidate.objective < best.objective) { // 最良か比較する。
            best = candidate; // 最良の成功結果を保存する。
        }
    }

    std::cout << "attempted=" << runs
              << " succeeded=" << succeeded << '\n';

    const auto& r = best; // 採用候補の完全解。

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }
}
```

**標準出力例**

```text
attempted=3 succeeded=3
status=success
objective=10
label 0 1 0 1
load 2 2
```

**利用上の注意**：自動で全runをまとめて止める公開APIではありません。総時間を制限したい場合は、同じdeadlineを各呼出しへ渡します。


### 3.18 共通の締切の中で少しずつ再最適化する

外側の処理が全体の時間枠を持ち、その中で何回か改善したい場合です。各呼出しの時間と、全体で共有する終了時刻の両方を指定します。

**具体的に解く問題**：最大3回の処理を、全体30ms・1回5msを目安に実行します。最初は構築し、次から直前の完全解を改善します。

**この例で使うAPI・概念**：time_limit_ms、deadline、共通時間枠。

**入力形式**：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。番号は0始まりです。末尾に最大回数R、全体時間budget_ms、1回の時間slice_msを入力します。両時間は有限の正値で、この例では1000ms以下です。

**入力例**

```text
4 2 2
2 2
0 6
1 4
5 0
4 1
0 2 5
1 3 5
3 30 5
```

**完全なプログラム：`example_18.cpp`**

```cpp
// 解く問題：最大3回の処理を、全体30ms・1回5msを目安に実行します。
// 最初は構築し、次から直前の完全解を改善します。
// 入力形式：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。
// 番号は0始まりです。
// 末尾に最大回数R、全体時間budget_ms、1回の時間slice_msを入力します。
// 両時間は有限の正値で、この例では1000ms以下です。
// 注意：経過時間、反復数、完了回数、同点解の選ばれ方は環境に依存します。
// 時間制限をなくすために無限大を渡すのではなく、負のtime_limit_msと既定のdeadlineを使います。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    int rounds = 0;
    double budget_ms = 0;
    double slice_ms = 0;
    std::cin >> rounds >> budget_ms >> slice_ms;

    if (!std::cin || rounds <= 0 ||
        !std::isfinite(budget_ms) || !std::isfinite(slice_ms) ||
        budget_ms <= 0 || budget_ms > 1000 ||
        slice_ms <= 0 || slice_ms > 1000) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    capacitated_graph_labeling_options opt; // 時間で止める設定を用意する。
    opt.time_limit_ms = slice_ms; // 1回の準備も含む相対時間の目安。
    opt.iteration_limit = -1; // 反復数では止めない。時間側は必ず有効にする。

    // この絶対締切を、すべての呼出しで共用する。
    opt.deadline = std::chrono::steady_clock::now()
        + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double, std::milli>(budget_ms)
        );

    capacitated_graph_labeling_result<Cost> r; // 未成功の初期状態。
    int completed = 0;

    for (int i = 0; i < rounds; ++i) {
        if (std::chrono::steady_clock::now() >= opt.deadline) { // 共通の締切を確認する。
            break; // 呼出し側でも全体締切を確認する。
        }
        opt.seed = static_cast<std::uint64_t>(i) + 1; // 今回の乱数列。

        capacitated_graph_labeling_result<Cost> next; // 今回の結果を受け取る。
        if (r.feasible) { // 既存解がある場合。
            next = improve_capacitated_graph_labeling(p, r.label, cost, opt); // 前回解を改善。
        } else {
            next = solve_capacitated_graph_labeling(p, cost, opt); // 初期解を作る。
        }
        ++completed;

        if (next.feasible) { // 成功した完全解を次回へ渡す。
            r = std::move(next);
        }
    }

    std::cout << "completed=" << completed << '\n';

    // 準備処理や間欠的な時刻確認のため、厳密な30ms上限は保証されない。
    // 外側の時間制限には、出力などの余裕を残す。

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }
}
```

**標準出力例**

```text
completed=3
status=success
objective=10
label 0 1 0 1
load 2 2
```

**利用上の注意**：経過時間、反復数、完了回数、同点解の選ばれ方は環境に依存します。時間制限をなくすために無限大を渡すのではなく、負のtime_limit_msと既定のdeadlineを使います。


### 3.19 目的値の改善を行わず、実行可能初期解を作る

まず制約を満たす割当だけを用意し、その後の改善は外側の処理で行いたい場合です。反復上限0は初期構築を省略する指定ではありません。

**具体的に解く問題**：4仕事の容量を守る初期解を作り、目的値改善の反復数が0であることを確認します。

**この例で使うAPI・概念**：iteration_limit=0、初期構築だけの実行。

**入力形式**：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。番号は0始まりです。

**入力例**

```text
4 2 2
2 2
0 6
1 4
5 0
4 1
0 2 5
1 3 5
```

**完全なプログラム：`example_19.cpp`**

```cpp
// 解く問題：4仕事の容量を守る初期解を作り、目的値改善の反復数が0であることを確認します。
// 入力形式：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。
// 番号は0始まりです。
// 注意：time_limit_ms=0でもsolveは最初の構築を試します。
// 失敗は実行可能解が存在しないという証明ではありません。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    opt.iteration_limit = 0; // 目的値改善だけを止める。構築と必要時の制約修復は行う。
    opt.initial_trials = 1; // 構築の試行数。実行可能性を保証する値ではない。

    // 問題・費用規則・設定から完全解を探す。
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);

    std::cout << "status=" << r.status << '\n'; // 成功・失敗理由を名前で表示する。
    if (!r.feasible) { // 失敗時の空ラベル列や目的値0を解として使わない。
        return 0;
    }

    std::cout << "objective=" << r.objective << '\n'; // 小さいほどよい目的値。

    std::cout << "label";
    for (int a : r.label) { // 頂点順の割当先。
        std::cout << ' ' << a;
    }
    std::cout << '\n';

    std::cout << "load";
    for (long long x : r.load) { // ラベル順の総需要量。
        std::cout << ' ' << x;
    }
    std::cout << '\n';

    if (!r.count.empty()) { // 件数制約を両方省略した場合、countは空になる。
        std::cout << "count";
        for (int x : r.count) { // ラベル順の頂点数。
            std::cout << ' ' << x;
        }
        std::cout << '\n';
    }

    // 初期構築や制約修復の手数はiterationsに含まれない。
    std::cout << "iterations=" << r.iterations << '\n';
}
```

**標準出力例**

```text
status=success
objective=12
label 0 0 1 1
load 2 2
iterations=0
```

**利用上の注意**：time_limit_ms=0でもsolveは最初の構築を試します。失敗は実行可能解が存在しないという証明ではありません。


### 3.20 温度とキャッシュの設定を比較する

探索中にどの程度の一時的な悪化を許すかと、計算を速くするためにどの程度メモリを使うかを調整する例です。入力モデルを変えずに設定だけを比較します。

**具体的に解く問題**：同じ問題を、入力した開始温度・終了温度で、Pottsキャッシュを禁止した場合と256MiBまで許した場合の2通りで解きます。

**この例で使うAPI・概念**：開始・終了温度、キャッシュ予算、used_potts_cache。

**入力形式**：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。番号は0始まりです。末尾に開始温度と終了温度を入力します。有限の値を使い、負ならその側を自動設定します。

**入力例**

```text
4 2 2
2 2
0 6
1 4
5 0
4 1
0 2 5
1 3 5
2.0 0.05
```

**完全なプログラム：`example_20.cpp`**

```cpp
// 解く問題：同じ問題を、入力した開始温度・終了温度で、Pottsキャッシュを禁止した場合と256MiBまで
// 許した場合の2通りで解きます。
// 入力形式：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。
// 番号は0始まりです。
// 末尾に開始温度と終了温度を入力します。
// 有限の値を使い、負ならその側を自動設定します。
// 注意：設定によって乱数の使われ方や到達解が変わるため、同じseedでも結果の完全一致は保証されま
// せん。
// 一般の行列・自作費用にはPotts用キャッシュを適用しません。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    double start = 0;
    double finish = 0;
    std::cin >> start >> finish;
    if (!std::cin || !std::isfinite(start) || !std::isfinite(finish)) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    opt.initial_temperature = start; // 非負なら開始温度を指定。負なら自動。
    opt.final_temperature = finish; // 非負なら終了温度を指定。負なら自動。

    // 温度は費用の尺度で考える。0でも内部では少なくとも1e-12になる。
    // 0を指定することは、厳密な山登りモードの指定ではない。
    for (int mode = 0; mode < 2; ++mode) {
        // 差分表などの予算。全メモリの上限ではない。
        opt.potts_cache_max_bytes = mode == 0 ? 0 : (256ULL << 20);

        // キャッシュがなくても同じ問題を解ける。
        const auto r = solve_capacitated_graph_labeling(p, cost, opt);
        std::cout << "mode=" << mode << " status=" << r.status << '\n'; // 成否。

        if (r.feasible) { // 成功した場合だけ目的値を読む。
            std::cout << "objective=" << r.objective // 最良解の目的値。
                      << " cache=" << r.used_potts_cache // 表を実際に作ったか。
                      << '\n';
        }
    }
}
```

**標準出力例**

```text
mode=0 status=success
objective=10 cache=0
mode=1 status=success
objective=10 cache=1
```

**利用上の注意**：設定によって乱数の使われ方や到達解が変わるため、同じseedでも結果の完全一致は保証されません。一般の行列・自作費用にはPotts用キャッシュを適用しません。


### 3.21 失敗理由と空の問題を正しく扱う

「解が見つからない」「初期解が不正」「変更範囲が不正」は対処が異なります。また、仕事が0件なら空のラベル列が正しい成功結果になることもあります。

**具体的に解く問題**：4つのstatusを表示し、空の変更範囲と0頂点の問題が正常に処理されることも確認します。

**この例で使うAPI・概念**：全status、空範囲、0頂点の問題。

**入力形式**：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。番号は0始まりです。この例はN=4、K=2、容量上限が2,2の入力を使います。

**入力例**

```text
4 2 2
2 2
0 6
1 4
5 0
4 1
0 2 5
1 3 5
```

**完全なプログラム：`example_21.cpp`**

```cpp
// 解く問題：4つのstatusを表示し、空の変更範囲と0頂点の問題が正常に処理されることも確認します。
// 入力形式：`N K M`、容量上限K個、単項費用N行×K個、関係M行の`u v weight`。
// 番号は0始まりです。
// この例はN=4、K=2、容量上限が2,2の入力を使います。
// 注意：配列のサイズ違い、負の需要量などの問題データ不備をstatusへ変換する仕組みではありません。
// assertで停止する場合があり、assertを無効にしても入力契約は必要です。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    // 仕事数、担当先数、関係数を読み込む。
    int n = 0;
    int k = 0;
    int m = 0;
    std::cin >> n >> k >> m;

    if (!std::cin || n <= 0 || k <= 0 || m < 0 ||
        1LL * n * k > INT_MAX) {
        return 1;
    }

    capacitated_graph_labeling_problem<Cost> p; // 問題データを用意する。
    p.vertex_count = n; // 頂点番号は0〜n-1。
    p.label_count = k; // ラベル番号は0〜k-1。ラベル数は正にする。

    p.upper_capacity.resize(k); // 総需要量の上限。demandの省略時は各頂点1。
    for (auto& x : p.upper_capacity) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    p.unary_cost.resize(n * k); // 頂点v・ラベルaの費用はv*k+aに置く。負も可。
    for (auto& x : p.unary_cost) { // 各要素を入力順に設定する。
        std::cin >> x;
    }

    // 関係を1本ずつ読み込む。
    for (int i = 0; i < m; ++i) {
        capacitated_graph_labeling_edge<Cost> e; // u=0、v=0、weight=1、type=0が初期値。
        std::cin >> e.u >> e.v >> e.weight; // 端点は0〜n-1。Pottsでは異色の費用がweight。
        p.edges.push_back(e); // 1本登録。同じ辺を2回入れると2回課金される。
    }

    // 全入力を読み終えてからAPIを呼ぶ。
    if (!std::cin) {
        return 1;
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // この失敗例は、4仕事・2担当・容量上限2,2を前提にする。
    if (n != 4 || k != 2 || p.upper_capacity != std::vector<long long>{2, 2}) {
        return 1;
    }

    using Status = capacitated_graph_labeling_status; // 列挙型の短い別名。
    opt.iteration_limit = 0; // 成否の確認が目的なので、目的値を改善しない。

    // 1. 通常の成功。
    const auto valid = solve_capacitated_graph_labeling(p, cost, opt);
    std::cout << "normal=" << valid.status << '\n'; // successを期待する。
    if (!valid.feasible || valid.status != Status::success) { // 完全解の成功を確認する。
        return 1; // 成功でも、最適性の証明ではない。
    }

    // 2. 初期解が不正：全仕事を担当0へ置くと容量違反になる。
    const std::vector<int> bad(n, 0);
    // 不正な初期解は、修復されずに失敗する。
    const auto invalid_initial = improve_capacitated_graph_labeling(p, bad, cost, opt);
    std::cout << "bad_initial=" << invalid_initial.status << '\n'; // 初期解の不正。

    // 3. 変更範囲が不正：頂点1が重複している。
    // 範囲外の番号や、固定された頂点も含めてはいけない。
    const auto invalid_scope = improve_capacitated_graph_labeling_subset(
        p,           // 問題全体。
        valid.label, // 実行可能な完全解。
        {1, 1},      // 重複を入れた不正な範囲。
        cost,        // 二項費用の規則。
        opt          // 探索の設定。
    );
    std::cout << "bad_scope=" << invalid_scope.status << '\n'; // 範囲の不正。

    // 4. 空の変更範囲：変更なしの成功になる。
    const auto unchanged = improve_capacitated_graph_labeling_subset(
        p,           // 問題全体。
        valid.label, // 実行可能な完全解。
        {},          // 変更してよい頂点なし。
        cost,        // 二項費用の規則。
        opt          // 探索の設定。
    );
    std::cout << "empty_scope=" << unchanged.status // successを期待する。
              << " delta=" << unchanged.objective_delta // 差分は0。
              << '\n';

    // 5. 構築失敗：4仕事を合計2件分へ置こうとする、解のない問題。
    p.upper_capacity = {1, 1}; // 容量上限を減らす。
    // 条件を満たす初期解を構築できるか試す。
    const auto missing = solve_capacitated_graph_labeling(p, cost, opt);
    std::cout << "not_found=" << missing.status << '\n'; // construction_failed。
    // このstatus自体が、不可能性の証明を与えるわけではない。

    // 6. 仕事が0件：空の割当が正しい完全解になる。
    capacitated_graph_labeling_problem<Cost> empty; // vertex_countの既定値は0。
    empty.label_count = 2; // 0頂点でもラベル数は正。正の下限は指定しない。
    // 空の割当を、実行可能な完全解として返す。
    const auto zero = solve_capacitated_graph_labeling(empty, cost, opt);
    std::cout << "empty_problem=" << zero.status // 成功理由の名前。
              << " feasible=" << zero.feasible // 空でも実行可能。
              << " labels=" << zero.label.size() // 要素数は0。
              << '\n';

    // 期待するstatusを列挙名で確認する。
    if (invalid_initial.status != Status::infeasible_initial_solution ||
        invalid_scope.status != Status::invalid_repair_scope || // 範囲の不正を区別する。
        missing.status != Status::construction_failed || !zero.feasible) { // 他の成否。
        return 1;
    }
}
```

**標準出力例**

```text
normal=success
bad_initial=infeasible_initial_solution
bad_scope=invalid_repair_scope
empty_scope=success delta=0
not_found=construction_failed
empty_problem=success feasible=1 labels=0
```

**利用上の注意**：配列のサイズ違い、負の需要量などの問題データ不備をstatusへ変換する仕組みではありません。assertで停止する場合があり、assertを無効にしても入力契約は必要です。


### 3.22 長い結果をcout・cerrへそのまま出力する

調査中に全フィールドをまとめて見たい場合です。頂点数が多くても、配列の表示は先頭20要素に制限されます。完全解と部分修復のどちらも同じ書き方で出力できます。

**具体的に解く問題**：26仕事の担当先を表示し、担当0だけの初期解から全仕事を変更可能にした部分修復も表示します。配列末尾の省略個数を確認します。

**この例で使うAPI・概念**：結果とstatusのoperator<<、配列の表示上限。

**入力形式**：仕事数Nだけを入力します。この例では1〜100、担当先数は2です。

**入力例**

```text
26
```

**完全なプログラム：`example_22.cpp`**

```cpp
// 解く問題：26仕事の担当先を表示し、担当0だけの初期解から全仕事を変更可能にした部分修復も表示し
// ます。
// 配列末尾の省略個数を確認します。
// 入力形式：仕事数Nだけを入力します。
// この例では1〜100、担当先数は2です。
// 注意：elapsed_msは実行ごとに変わります。
// 出力は人が読むための要約で、配列の全要素を保存する形式ではありません。
// 演算子はflags・precisionを変更せず、改行やflushを明示的に追加しません。

#include "capacitated_graph_labeling_v25.hpp" // ライブラリ本体。
#include <iostream> // 標準入力・標準出力。

int main() {
    // 費用を表す型。需要量・容量は別にlong longで扱う。
    using Cost = long long;

    int n = 0;
    std::cin >> n;
    if (!std::cin || n < 1 || n > 100) {
        return 1;
    }

    // 全仕事を担当1へ置くと費用0になる問題を作る。
    capacitated_graph_labeling_problem<Cost> p;
    p.vertex_count = n; // n=26なら、表示でラベル列の最後の6要素が省略される。
    p.label_count = 2; // 担当者は0と1。
    p.upper_count.assign(2, n); // 実質的に制限しない上限を置き、countも取得する。
    p.unary_cost.assign(n * 2, 0); // 仕事順・担当順の単項費用を、まず全部0にする。

    for (int v = 0; v < n; ++v) {
        p.unary_cost[v * 2] = 1; // 担当0だけ費用1、担当1は費用0。
    }

    // 同じラベルなら0、異なるラベルなら辺のweightを課す。
    const capacitated_potts_cost<Cost> cost;

    // 指定しない項目には既定値を使う。
    capacitated_graph_labeling_options opt;
    opt.time_limit_ms = -1; // 相対時間を無効にし、反復数で止める。
    opt.iteration_limit = 20000; // 改善提案の上限。採用された変更数ではない。
    opt.seed = 7; // 乱数列の出発点。時間制限時の結果まで固定する指定ではない。

    // 完全解を得て、全フィールドを1行で表示する。改行は自分で付ける。
    const auto r = solve_capacitated_graph_labeling(p, cost, opt);
    std::cout << std::fixed << std::setprecision(3) << r << '\n';
    if (!r.feasible) { // 表示できても、失敗結果を有効な解として使わない。
        return 0;
    }

    // 全仕事を担当0にした、実行可能な初期解を用意する。
    const std::vector<int> initial(n, 0);

    // 全頂点を、重複なしで変更可能にする。
    std::vector<int> scope(n);
    std::iota(scope.begin(), scope.end(), 0);

    // 完全ラベル列ではなく、変更点を返すAPIを使う。
    const auto repair = improve_capacitated_graph_labeling_subset(
        p,       // 問題全体。
        initial, // 実行可能な完全解。
        scope,   // 今回は全頂点を変更可能にする。
        cost,    // 二項費用の規則。
        opt      // 探索の設定。
    );

    // 標準エラーにも直接渡せる。各配列はそれぞれ先頭20要素まで表示する。
    std::cerr << std::fixed << std::setprecision(3) << repair << '\n';
    std::cout << "status=" << repair.status << '\n'; // status単体も名前で表示する。
    std::cout << "stored_labels=" << r.label.size() << '\n'; // 元の要素は消えない。

    if (repair.status != capacitated_graph_labeling_status::success) { // 成功を確認する。
        return 1; // 変更点を使う前に成功を確認する。
    }
}
```

**標準出力例**

```text
result{status=success, feasible=true, objective=0, initial_objective=0, iterations=20000, elapsed_ms=1.188, used_potts_cache=true, label=[1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, ... (+6 more)], load=[0, 26], count=[0, 26]}
status=success
stored_labels=26
```

**標準エラー出力例（`std::cerr`）**

```text
repair_result{status=success, objective_delta=-26, iterations=20000, elapsed_ms=1.051, used_potts_cache=true, changed_vertices=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ... (+6 more)], new_labels=[1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, ... (+6 more)]}
```

**利用上の注意**：elapsed_msは実行ごとに変わります。出力は人が読むための要約で、配列の全要素を保存する形式ではありません。演算子はflags・precisionを変更せず、改行やflushを明示的に追加しません。
