# PboSolver サンプルコード集

対象ソース：`pbo_solver_v08.hpp`。C++20・GCCで利用する、0–1整数最適化ライブラリです。本書は、このソースが提供する問題表現と公開APIを説明します。

1. 何を解くライブラリか
2. APIリファレンス
3. ユースケースと、そのまま実行できるサンプル

高校数学の範囲で読み進められるよう、式の記号と意味を説明します。C++の変数、配列、ループ、関数の基本は知っていることを前提とします。

## 1. 何を解くライブラリか？

### 1.1 候補を選ぶか、選ばないかを決める

たとえば、仕事A・B・Cの利益が10・7・6、必要な資源が3・2・2で、使える資源が4だとします。Aだけなら利益10、BとCなら利益13です。AとBなら利益17ですが、資源を5使うため選べません。ライブラリには利益と条件を渡し、条件を守りながら利益を大きくする組合せを求めます。

各候補を選ぶなら1、選ばないなら0とします。仕事だけでなく、設備を開くか、仕事を特定の担当者に割り当てるか、ある開始時刻を採用するか、といった判断も0/1で表せます。

PBOはPseudo-Boolean Optimizationの略です。変数は0/1ですが、評価値は利益や費用などの整数です。「疎な」は、各条件に関係する変数が全体の一部であることを指します。

### 1.2 条件の左辺を「行」として書く

変数の個数を$n$、条件の本数を$m$とします。変数IDは$i=0,1,\ldots,n-1$、条件のIDは$j=0,1,\ldots,m-1$です。条件1本を「行」と呼び、その合計値を次で表します。

$$s_ {j}(x)=\sum_ {i=0}^{n-1}a_ {ji}x_ {i}$$

| 記号 | 意味 |
|---|---|
| $x$ | 全変数の値を並べた配列 |
| $x_ {i}$ | 変数$i$の0/1値 |
| $a_ {ji}$ | 行$j$で変数$i$に掛ける整数。正・0・負を使える |
| $s_ {j}(x)$ | 行$j$の合計。資源使用量、選択件数、入出の差など |
| $l_ {j}$ | 行$j$の下限。省略できる |
| $u_ {j}$ | 行$j$の上限。省略できる |

$\sum$は、指定した範囲の値を足す記号です。コードには、係数が0の項をすべて書く必要はありません。資源の例は$3x_ {0}+2x_ {1}+2x_ {2}\le4$という1行になります。

両境界がある場合、範囲からはみ出た量を次で表します。

$$d_ {j}(x)=\max(0,l_ {j}-s_ {j}(x))+\max(0,s_ {j}(x)-u_ {j})$$

$\max$は引数の大きい方です。第1項は下限への不足量、第2項は上限の超過量です。下限を省略した場合は第1項を0、上限を省略した場合は第2項を0とします。両境界を指定するなら$l_ {j}\le u_ {j}$が必要です。

### 1.3 目的関数と、必ず守る条件

行には3種類あります。

| 種類 | 条件を破ったときの扱い |
|---|---|
| `Hard` | 必須条件。破った候補は実行可能解ではない |
| `LinearPenalty` | 不足・超過量に比例する点数を引く |
| `FixedPenalty` | 違反量によらず、その行につき一定点数を1回引く |

ライブラリが大きくする目的値は次です。

$$F(x)=c+\sum_ {i=0}^{n-1}p_ {i}x_ {i}-\sum_ {j\in L}w_ {j}d_ {j}(x)-\sum_ {j\in Q}w_ {j}I(d_ {j}(x)>0)$$

| 記号 | 意味と対応する入力 |
|---|---|
| $F(x)$ | 目的値。`evaluation.objective`。大きいほどよい |
| $c$ | 常に加える定数。`PboProblem.constant` |
| $p_ {i}$ | 変数$i$を1にしたときの正負の利益。`profit[i]` |
| $L$ | `LinearPenalty`の行IDを集めた集合 |
| $Q$ | `FixedPenalty`の行IDを集めた集合 |
| $w_ {j}$ | 行$j$の減点の重み。ソフト行では0以上の整数。`weight` |
| $I(t)$ | 条件$t$が成立すれば1、成立しなければ0 |
| $d_ {j}(x)$ | 前節の不足・超過量 |

入力の係数・利益・境界・行の種類・重み・定数は利用者が決めます。解として求めるのは$x$です。行和$s_ {j}$、違反量$d_ {j}$、目的値$F$は、その$x$から決まります。同時に選んだ場合だけの利益も、専用の公開APIでこの目的に組み込めます。

必須条件の行IDの集合を$H$、固定する変数IDの集合を$J$、固定値を$f_ {i}$とすると、実行可能解は次を満たします。

$$d_ {j}(x)=0\quad(j\in H),\qquad x_ {i}=f_ {i}\quad(i\in J),\qquad x_ {i}\in\lbrace0,1\rbrace$$

$f_ {i}$は0または1です。「実行可能」とは、必須条件と固定値を守っていることです。「最適」とは、それより目的値が高い実行可能解が存在しないことです。

Hard違反を目的値から自動で巨大減点するわけではありません。したがって、**目的値が大きくても、その候補が使えるとは限りません**。`feasible()`を確認します。ソフト行を破っても、Hard行と固定値を守っていれば実行可能です。

費用を最小化したい場合は、費用の符号を反転して利益にします。費用5の候補には利益-5を設定します。定数や別の減点を含めた場合、総費用が単純に`-objective`になるとは限らないので、元の評価式に戻して読みます。

### 1.4 表せる範囲と結果の読み方

容量付き選択、被覆、割当、排他関係、依存関係、目標からのずれ、設備の開閉などを、同じモデルへまとめられます。順序や小さな整数を扱うときは、候補ごとの0/1変数へ置き換える必要があります。実数の変数や、任意のC++評価関数を直接渡すAPIではありません。

大きな問題で、実行可能解の発見や最適性は保証されません。`feasible()==false`は、返却された候補に違反が残っているという意味です。問題そのものが実行不可能だと証明した状態ではありません。最適性証明のフラグもありません。

## 2. APIリファレンス

### 2.1 導入と共通の入力条件

`pbo_solver_v08.hpp`とサンプルの`.cpp`を同じフォルダに置きます。GCCで、たとえば次のようにコンパイルします。サンプルは1本ずつコンパイルしてください。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion 01_capacity.cpp -o example
./example < input.txt
```

同梱ZIPの配置では、`-I source`を追加し、ソースに`examples/01_capacity.cpp`を指定します。ヘッダをincludeすると、ヘッダ内の検証用`main`は含まれません。以下はライブラリの明示的な公開型・関数をすべて列挙したものです。型名の`vector`、`optional`、`span`、`ostream`、`chrono`などは`std::`を省略して表記しています。

| 共通事項 | 条件 |
|---|---|
| 変数数 | `profit.size()`を$n$とする。0変数も表現可能 |
| 変数ID | `int`で0以上$n$未満 |
| 行ID | `int`で0以上`rows.size()`未満。追加順に0から付く |
| 解の配列 | 長さ$n$、各値0または1。`vector<uint8_t>`、`array<uint8_t, N>`など。`vector<bool>`は不可 |
| 行の項 | `{変数ID, 係数}`。負係数・重複項・0係数を入力可能 |
| 整数値 | 利益・定数・係数・境界・重みは`long long`。`LLONG_MIN`を使わない |
| 整数演算 | 行和、重複係数の合計、減点、差分、上界を含む途中の演算もすべて`long long`の範囲内にする |
| 大きさ | ID・変数数・行数・項数と、それらの内部オフセットが`int`に収まる範囲 |
| 時間値 | 有限で、時計の時間型に収まる現実的な予算を使う。NaN・無限大は不可 |
| 並列利用 | 1つのsolverを同時に変更しない。異なるsolverは状態を共有しない |

前提違反は一部を`assert`で検出します。`assert`を無効にしても、不正な入力が有効になるわけではありません。例外やエラー結果で全入力を検証するAPIではないため、外部からの入力は必要に応じて利用側で検査します。

### 2.2 `PboRowKind`：行の種類

| 列挙値 | 意味 | `weight`の扱い | 実行可能性への影響 |
|---|---|---|---|
| `PboRowKind::Hard` | 必ず守る下限・上限・等式 | 無視される。通常は既定値1のままでよい | 違反行があれば不可 |
| `PboRowKind::LinearPenalty` | 境界から離れた量に比例して減点 | 0以上。減点は`weight × 違反量` | 影響しない |
| `PboRowKind::FixedPenalty` | 境界違反が1つでもあれば定額減点 | 0以上。違反中は`weight`を1回引く | 影響しない |

たとえば上限4に対して行和7、重み2なら、LinearPenaltyの減点は6、FixedPenaltyの減点は2です。Hardなら違反行数が1増えます。等式のLinearPenaltyでは、目標から上下どちらに離れても、その距離に重みを掛けます。ソフト行の重み0は減点なしです。

### 2.3 入力用の構造体

#### `PboTerm`

| メンバ | 型 | 値の意味・範囲 | 初期値 |
|---|---|---|---|
| `var` | `int` | 対象変数ID | メンバ初期値なし。必ず明示する |
| `coef` | `long long` | その変数に掛ける正負の係数 | メンバ初期値なし。必ず明示する |

`PboTerm{2, -3}`は「変数2に-3を掛ける」です。単に`PboTerm t;`と書いた場合、各数値は初期化されません。`PboTerm{}`なら値初期化により両方0になりますが、意図が明確になるよう項の値を明示する使い方を勧めます。

#### `PboRow`

| メンバ | 型 | 意味・範囲 | 既定値 |
|---|---|---|---|
| `terms` | `vector<PboTerm>` | この行に足す項 | 空配列 |
| `lower` | `optional<long long>` | 下限。`nullopt`なら下限なし | `nullopt` |
| `upper` | `optional<long long>` | 上限。`nullopt`なら上限なし | `nullopt` |
| `kind` | `PboRowKind` | 前節の3種類のいずれか | `Hard` |
| `weight` | `long long` | ソフト行の減点単価・定額 | 1 |

両境界を指定するときは`lower <= upper`とします。両方を省略すると違反量は常に0です。空の`terms`の行和は0です。したがって、空のHard行に下限1を付けると、達成できない条件になります。

#### `PboProblem`

| メンバ | 型 | 意味・範囲 | 既定値 |
|---|---|---|---|
| `profit` | `vector<long long>` | 変数ごとの正負の利益。長さが変数数 | 空配列 |
| `rows` | `vector<PboRow>` | 条件と減点の行 | 空配列 |
| `fixed` | `vector<int8_t>` | 恒久固定。-1は自由、0は0固定、1は1固定 | 空配列＝すべて自由 |
| `constant` | `long long` | 目的値に常に加える定数 | 0 |

`fixed`を指定するときは、長さを`profit.size()`とそろえます。一部だけ固定するときは、全要素を-1で用意してから対象の位置を0または1にします。構造体の各メンバは直接設定できます。solver構築後に元の`PboProblem`を編集してもsolverへ自動反映されません。

### 2.4 `PboProblem`に行を追加する全メソッド

ここに挙げる6メソッドは、すべて追加した行IDを`int`で返します。IDは0始まりです。必要なら保存し、後でsolverの行更新メソッドへ渡します。`terms`の共通ルールは2.1、行の種類は2.2のとおりです。

| メソッドと引数 | 意味 | 省略時 |
|---|---|---|
| `add_row(PboRow row)` | 完成した行を追加する。`row`の全メンバをそのまま使う | `row`の省略不可 |
| `add_le(vector<PboTerm> terms, long long upper, PboRowKind kind, long long weight)` | 行和を`upper`以下にする条件、または超過への減点 | `kind=Hard`、`weight=1`。他は必須 |
| `add_ge(vector<PboTerm> terms, long long lower, PboRowKind kind, long long weight)` | 行和を`lower`以上にする条件、または不足への減点 | `kind=Hard`、`weight=1`。他は必須 |
| `add_eq(vector<PboTerm> terms, long long value, PboRowKind kind, long long weight)` | 下限・上限をともに`value`とする | `kind=Hard`、`weight=1`。他は必須 |
| `add_range(vector<PboTerm> terms, optional<long long> lower, optional<long long> upper, PboRowKind kind, long long weight)` | 両側の境界を指定する。各境界は`nullopt`にできる | `kind=Hard`、`weight=1`。`lower`・`upper`自体の引数は必須 |
| `add_all_profit(vector<int> vars, long long value)` | `vars`の変数がすべて1の場合だけ、正負の`value`を加える | 両引数とも必須 |

`upper`、`lower`、`value`は整数境界で、負の値も使えます。下限・上限は境界値そのものを含みます。`terms`や`vars`は値渡しなので、利用側の配列を破壊したくないなら普通に渡し、不要なら`std::move`で移動できます。

`add_all_profit`の`vars`は有効な変数IDの配列です。重複IDは1つにまとめられます。`value`が正ならまとめて選ぶ報酬、負ならまとめて選ぶ追加費用です。空配列では「全部が選ばれた」を常に満たすため、定数の加算になります。

このメソッドは、定数とFixedPenalty行の組合せで同時選択利益を表します。補助変数は作りません。戻り値は、そのために追加した行のIDです。利益の金額を変えるときは、対応する行だけでなく定数も関係する場合があるため、元のモデルから作り直して`rebuild`するのが分かりやすい使い方です。

### 2.5 実行条件 `PboParam`

| メンバ | 型 | 取り得る値・意味 | 既定値 |
|---|---|---|---|
| `time_limit_us` | `double` | 0以上は相対時間予算。0は探索なし。-1は相対時間制限なし。他の負数・NaN・無限大は不可 | 1,950,000マイクロ秒＝1.95秒 |
| `deadline` | `optional<chrono::steady_clock::time_point>` | 絶対締切。未指定ならこの制限は使わない | `nullopt` |
| `max_iterations` | `int64_t` | 0以上は探索量の上限。0は探索なし。-1はこの上限なし。他の負数は不可 | -1 |

時間と探索量を両方指定すると、いずれかの終了条件に達したところで止めます。相対時間と絶対締切を両方指定すると、早い方を使います。`time_limit_us=-1`にするなら、絶対締切または0以上の`max_iterations`を必ず併用します。

`iterations`は局所探索の外側の試行、または小規模探索の訪問ノードの数です。単純な変数反転回数ではありません。同じ値でも、問題によって実時間は変わります。

時間は`solve`または`improve`の入口から測ります。初期化・再評価などは必要なので、時間0でも呼出し時間が0になるわけではありません。構築や、その前の入力処理・setter呼出しは、この相対時間予算に含まれません。締切の確認には間隔があり、厳密な時刻での停止は保証しません。出力や終了処理の余裕を確保します。

### 2.6 結果を表す構造体

#### `PboEvaluation`

| メンバ・メソッド | 型・戻り値 | 意味 | 既定値 |
|---|---|---|---|
| `objective` | `long long` | 定数と利益からソフト減点を引いた真の目的値 | 0 |
| `violated_hard_rows` | `int` | 破っているHard行の本数。違反量ではない | 0 |
| `violated_fixed_variables` | `int` | 恒久固定の指定と食い違う変数の個数 | 0 |
| `feasible() const` | `bool` | 2種類の違反数がともに0ならtrue | 引数なし |

計算結果の違反数は0以上です。Soft行の違反数・行別違反量を取り出すAPIはありません。利用者が`PboEvaluation{}`を作っただけでも各数値が0なので`feasible()`はtrueになりますが、それ自体はsolverが問題を解いたことを意味しません。

#### `PboResult`

| メンバ | 型 | 意味 | 既定値 |
|---|---|---|---|
| `x` | `vector<uint8_t>` | 返却候補の全変数の0/1値。探索結果では長さ$n$ | 空配列 |
| `evaluation` | `PboEvaluation` | その候補を、その呼出しのモデルで評価した値 | 上表の既定値 |
| `iterations` | `int64_t` | 今回の呼出しの探索量。累計ではない | 0 |
| `elapsed_us` | `double` | 今回の呼出しの経過時間。マイクロ秒 | 0 |

`solve`と`improve`が返すのは、solver内部の`PboResult`へのconst参照です。次の非const操作をまたいで保存したいときは、`auto saved = solver.solve(param);`のようにコピーします。`const auto&`を付けるとコピーではなく参照になるので、過去結果の保存にはなりません。

返却候補のHard違反は残る場合があります。`solve`・`improve`の候補は恒久固定を守りますが、`evaluate`は渡された候補をそのまま調べるため、固定値違反も報告できます。

### 2.7 `PboSolver`：構築・探索・評価

#### コンストラクタ

`explicit PboSolver(PboProblem problem, uint64_t seed = 1)`

- `problem`：入力モデル。省略不可。値渡しで取り込み、solverが自分のモデルを持ちます。
- `seed`：乱数の開始値。0を含む`uint64_t`の範囲で指定でき、既定値は1です。
- 戻り値：コンストラクタなのでありません。引数なしの構築はできません。

構築時に同じ行内の同一変数の係数を合計し、合計0の項を除きます。行自体と行IDは残ります。構築後、元の`problem`への編集は反映されないため、以下のsetterか`rebuild`を使います。

#### `solve`

`const PboResult& solve(const PboParam& param = {}, span<const uint8_t> initial = {})`

| 引数 | 意味・条件 | 省略時 |
|---|---|---|
| `param` | 2.5の実行条件 | `PboParam{}` |
| `initial` | 追加する初期候補。指定するなら長さ$n$、各値0/1 | 空span＝初期候補を追加しない |

初回は自動で候補を用意します。同じsolverの次の`solve`は、探索状態・保存候補・乱数状態を引き継ぎます。初期候補を省略してもリセットにはなりません。初期候補を指定した場合も、過去の保存候補を捨てる操作ではありません。

`initial`が恒久固定に一致しなければ、solver側の候補を固定値へ補正します。呼出し元の配列は変更しません。初期候補は実行可能でなくてもかまいません。実行可能な候補を与えると、その呼出しでは同じモデルでその目的値以上の実行可能候補が保持されます。

返却値は見つかった候補のうち保存されたものです。実行可能候補を優先し、それがない場合はHard違反行数、正規化した違反量、目的値の順で選びます。参照の保持条件は2.6のとおりです。

#### `improve`

`const PboResult& improve(span<const uint8_t> initial, span<const int> free_variables, const PboParam& param = {})`

| 引数 | 意味・条件 | 省略時 |
|---|---|---|
| `initial` | 長さ$n$、各値0/1の候補。**恒久固定に一致すること** | 省略不可 |
| `free_variables` | 今回変更してよい変数ID。重複可。空配列も可 | 省略不可 |
| `param` | 2.5の実行条件 | `PboParam{}` |

指定範囲の外は`initial`の値を保つ、新しい部分探索です。リストに含めた変数でも恒久固定が優先されます。空リストなら初期候補をそのまま評価して返します。初期候補はHard違反を持っていてもかまいませんが、固定値の違反は許されません。

以前の別の保存解を、この部分探索へ混ぜません。呼出しはsolverの探索状態と保存結果を置き換えます。大域的なベストを残したいなら、呼出し前にコピーを保持します。次の`solve`では、今回の可変リストの制限は外れ、恒久固定以外の変数を探索できます。

#### `evaluate`

`PboEvaluation evaluate(span<const uint8_t> x) const`

`x`は長さ$n$、各値0/1の候補です。省略不可です。現在のモデルで全体を評価した`PboEvaluation`を値として返します。候補の補正・探索・初期候補登録は行いません。setterの直後でも新しいモデルで評価できます。

`span`は配列を読み取るための窓口です。通常は適切な`vector`や`array`をそのまま渡せます。返却値をまたいで入力配列を借り続けるAPIではありません。要素型を取り違えず、0/1値に`vector<int>`や`vector<bool>`をそのまま渡さないようにします。

### 2.8 探索状態・乱数を操作する全メソッド

| メソッド | 引数・既定値 | 動作 | 戻り値 |
|---|---|---|---|
| `reseed(uint64_t seed)` | 開始値。省略不可 | 乱数状態だけを再設定。候補やモデルは保持 | `void` |
| `reset_search()` | 引数なし | モデルを保ち、次の探索を新規開始にする。乱数は戻さない | `void` |

同じ条件を再現したいときは、`reset_search()`と`reseed(seed)`を併用するか、同じモデル・seedでsolverを作り直します。実時間制限ではOSの都合などで探索量が変わります。比較には、相対時間制限を外して有限の`max_iterations`を指定し、deadlineを使わない方法が適します。

### 2.9 モデル更新の全メソッド

以下はすべて`void`を返します。表に既定値を書いていない引数は省略できません。`var`・`row`は現在のモデルの有効なIDです。

| メソッド | 各引数の意味・値域 | 動作・注意 |
|---|---|---|
| `set_profit(int var, long long profit)` | `var`：変数ID。`profit`：新しい正負の利益 | その変数の利益を変更 |
| `set_constant(long long constant)` | `constant`：新しい正負の定数 | 定数を置き換える。加算ではない |
| `set_fixed(int var, int value)` | `var`：変数ID。`value`：-1で自由、0/1で固定 | 固定・固定値変更・解除。候補への反映は次の探索時 |
| `set_row_bounds(int row, optional<long long> lower, optional<long long> upper)` | `row`：行ID。各境界は整数または`nullopt` | 両境界を置き換える。片側だけ変える場合も、残す側の値を渡す |
| `set_row_penalty(int row, PboRowKind kind, long long weight = 1)` | `row`：行ID。`kind`：列挙値のいずれか。ソフト時の`weight`は0以上 | 行の種類と重みを変更。省略した重みは1になり、以前の重みを維持する指定ではない |
| `set_coefficient(int row, int var, long long coef)` | `row`：行ID。`var`：その行に正規化後も存在する変数。`coef`：新しい正負の係数 | 既存項の係数を変更。0にした項は残り、再変更できる |
| `rebuild(PboProblem problem, bool keep_solution = true)` | `problem`：新しいモデル。`keep_solution`：現在候補・保存候補の引継ぎ可否 | 項・行の追加削除や変数数の変更に使う |

重複項が構築時に合計0になって除かれた場合、その項を`set_coefficient`で復活させることはできません。新しい項を追加するときは`rebuild`します。重複項がまとめられた場合、変更するのはその合計係数です。

`rebuild`で`keep_solution=true`なら、変数数と各変数IDの意味を保ちます。初期化前でも変数数を変えるならfalseが必要です。falseなら新しい問題として候補を作り直します。行のIDは新モデルの並び順に従うため、古い行IDを無条件に使い回さないようにします。

setterはモデルを変えますが、過去の`PboResult.evaluation`を即座に再計算しません。次の探索で、保存候補などを新しいモデルで再評価します。今すぐ採点したい場合は`evaluate(saved.x)`を使います。別のモデルで採点した目的値同士を、そのまま良し悪しの比較に使わないようにします。

### 2.10 デバッグ出力の全演算子

| 演算子 | 引数 | 戻り値・表示 |
|---|---|---|
| `ostream& operator<<(ostream& os, const PboEvaluation& e)` | `os`：出力先。`e`：評価結果 | 同じ`os`への参照。可否・目的値・2種類の違反数 |
| `ostream& operator<<(ostream& os, const PboResult& r)` | `os`：出力先。`r`：探索結果 | 同じ`os`への参照。評価・配列・探索量・経過時間 |

既定引数はありません。`cout`、`cerr`、`ostringstream`へ直接渡せます。`feasible`は`true`または`false`です。配列は先頭20要素までを数値として表示し、残りがあれば`...`を付け、総要素数を`size`に表示します。空配列は`[]`です。

出力対象は変えません。改行・明示的なflush・書式設定の変更は行いません。必要なら`'\n'`を続けます。数値の表示精度などは呼出し側のストリーム設定に従います。出力の省略は表示だけであり、`r.x`には全要素が残っています。

### 2.11 型の通常操作と、使い分けの要点

入力・結果・パラメータの構造体は、通常のC++構造体としてコピー・代入できます。solverもコンパイラ生成のコピー・ムーブに対応し、コピーはモデル・候補・乱数状態を含む独立した状態になります。大きいモデルのコピーには相応の時間とメモリが必要です。移動後の元solverは、再代入せずに探索へ使わないようにします。

| やりたいこと | 使うAPI |
|---|---|
| 新しい候補を求める／続きを探す | `solve` |
| 外部の候補を採点するだけ | `evaluate` |
| 候補の一部だけ変える | `improve` |
| 同じ形のモデルで数値だけ変える | 対応するsetter |
| 項・行・候補数などの構造を変える | `rebuild` |
| モデルを残して独立にやり直す | `reset_search`と必要なら`reseed` |
| 結果を後の呼出しまで保存する | 参照ではなく`PboResult`をコピー |

## 3. ユースケース

各サンプルは独立したプログラムです。共通ヘッダ以外に前の例を必要とせず、記載の入力を標準入力へ渡せば動きます。ライブラリを使う各行に日本語コメントを付けています。入力例は説明用の小さい問題で、汎用的な入力形式の厳密な検査までは行いません。

時間によらず出力を確認する例では、`time_limit_us=-1`と有限の`max_iterations`を併用します。例中の100,000は小さい入力に対して用意した探索量であり、大きな問題の最適性を保証する値ではありません。実際に使うときも、必ず実行可能性を確認してください。

時刻に関係する例を除き、示した出力と実行結果を照合しています。時間や探索量が変わる出力は、その節で区別して説明します。

| 節 | ユースケース | 主なAPI・表現 |
|---|---|---|
| 3.1 | 予算の範囲で採用する案件を選ぶ | 利益・上限・solve |
| 3.2 | 工数とメモリなど、複数の資源を同時に守る | 複数のHard行 |
| 3.3 | すべての地点を覆う設備を、最小費用で選ぶ | 負の利益・add_ge |
| 3.4 | 各カテゴリから、ちょうど1案ずつ選ぶ | add_eq・グループ |
| 3.5 | 担当者の容量を守り、各仕事を1人へ割り当てる | 仕事×担当者の変数・容量 |
| 3.6 | 衝突する案件を同時に採用しない | 2変数の排他条件 |
| 3.7 | 必要な準備も含めて、機能を採用する | 負係数・含意・代替前提 |
| 3.8 | 時間帯ごとの必要人数と上限を満たす勤務パターンを選ぶ | add_range・費用最小化 |
| 3.9 | 入る量と出る量が釣り合う取引を選ぶ | 正負係数の等式 |
| 3.10 | OR・NOTを含む論理条件を満たす | OR・NOTの行表現 |
| 3.11 | 供給量を目標へ近づけ、ずれ1単位ごとに減点する | LinearPenalty・目標 |
| 3.12 | 通常枠を超えたら、追加料金を1回だけ払う | FixedPenalty・Hard併用 |
| 3.13 | 組合せで発生する報酬や追加費用を扱う | add_all_profit・正負の追加利益 |
| 3.14 | 予算内で覆った地点の価値を、一度だけ数える | 補助変数・被覆の一致 |
| 3.15 | 設備を開く固定費と、割り当て費用を一緒に最小化する | 設備変数と割当変数 |
| 3.16 | 小さい整数の生産数量を、2進数で表す | 2進数・整数の上限 |
| 3.17 | 目標から大きく離れるほど、二乗で強く減点する | constant・二乗の展開・2変数の積 |
| 3.18 | 各仕事の開始時刻を選び、機械の重複使用を防ぐ | 開始候補・時間帯の占有 |
| 3.19 | 行データを読み込み、汎用のモデルを作る | PboRow・PboTerm・add_row・fixed |
| 3.20 | 外部で作った候補の利益と違反を調べる | evaluate・array・評価の出力 |
| 3.21 | 初期解を取り込み、保存してから続きを探す | 初期解・継続・max_iterations=0 |
| 3.22 | 確定済みの外側を保ち、一部の変数だけ解き直す | improve・範囲外の保持 |
| 3.23 | 報酬や基準点の変更を反映する | set_profit・set_constant |
| 3.24 | 容量の変更や、片側の境界の解除を行う | set_row_bounds・nullopt |
| 3.25 | 必須条件と、2種類の減点方針を切り替える | set_row_penalty・3種類の切替 |
| 3.26 | 既存候補の資源使用量を変更する | set_coefficient・0化と復帰 |
| 3.27 | 採用・不採用を確定し、後から解除する | set_fixed・固定の解除 |
| 3.28 | 条件の追加と、候補数の変更に対応する | rebuild・引継ぎtrue/false |
| 3.29 | 探索量とseedをそろえ、同じ条件を再現する | reset_search・reseed・固定探索量 |
| 3.30 | 全体締切を共有して、複数の独立した探索を試す | deadline・相対予算・複数回探索 |
| 3.31 | 大きい結果を、そのままログへ出す | PboResult出力・PboEvaluation出力 |
| 3.32 | 探索なしで初期候補を取り込み、固定値の補正を確認する | 時間0・固定の補正・未実行可能候補 |

### 3.1 予算の範囲で採用する案件を選ぶ

採用する案件の利益は大きくしたい一方、使える工数には上限があります。1件を途中まで採用することはできず、採用するか見送るかを決めます。

**解く問題**

利益10・7・6、工数3・2・2の3案件から、工数合計4以下で選びます。変数iが1なら案件iを採用します。

**入力形式**

1行目：案件数nと工数上限B。2行目：n件の利益。3行目：n件の工数。

**入力例**

```text
3 4
    10 7 6
    3 2 2
```

**コード：`01_capacity.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long capacity;
    cin >> n >> capacity;

    PboProblem problem; // 利益配列の長さが変数数になる入力モデル
    problem.profit.resize(n); // 案件ごとに1個の0/1変数を用意する
    for (auto& profit : problem.profit) { // 1にしたときの利益を読む
        cin >> profit;
    }

    vector<PboTerm> usage; // 工数合計を表す項。各項は{変数ID, 係数}
    for (int i = 0; i < n; ++i) {
        long long amount;
        cin >> amount;

        usage.push_back({i, amount}); // 案件iを選んだ場合だけ工数amountを加える
    }
    problem.add_le(usage, capacity); // 合計工数<=capacity。種類は既定のHard、weightは無視される

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=13
x=011
```

**読み方と注意点**

この入力では案件1と2を選ぶと工数4、利益13です。利益が負の案件も入力できますが、採用が必要な理由がなければ通常は見送る方が有利です。

### 3.2 工数とメモリなど、複数の資源を同時に守る

1つの上限を守っても、別の資源が足りなくなることがあります。たとえば処理を選ぶ際に、実行時間とメモリの両方を制限します。

**解く問題**

4処理の利益は9・8・7・6。時間とメモリの上限をそれぞれ5とし、どちらの上限も守る選択を求めます。

**入力形式**

n m、n個の利益に続き、資源ごとに「上限、n個の使用量」をm行入力します。

**入力例**

```text
4 2
    9 8 7 6
    5 4 3 2 2
    5 1 3 3 2
```

**コード：`02_multiple_resources.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n, resource_count;
    cin >> n >> resource_count;

    PboProblem problem; // 処理を選ぶ0/1変数を持つモデル
    problem.profit.resize(n); // 利益配列をn変数分にする
    for (auto& profit : problem.profit) { // 各処理の利益
        cin >> profit;
    }

    for (int r = 0; r < resource_count; ++r) {
        long long limit;
        cin >> limit;

        vector<PboTerm> usage; // 今回の資源に対する使用量の行
        for (int i = 0; i < n; ++i) {
            long long amount;
            cin >> amount;

            usage.push_back({i, amount}); // 変数iに、この資源の使用量を掛ける
        }
        problem.add_le(std::move(usage), limit); // 資源ごとに独立したHard上限を追加する
    }

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=14
x=0101
```

### 3.3 すべての地点を覆う設備を、最小費用で選ぶ

どの地点にも少なくとも1つの設備が必要だとします。設備が覆う範囲は重なっていてよく、必要な地点をすべて覆ったうえで設置費用を小さくします。

**解く問題**

3地点を覆う4設備から選びます。費用は4・3・5・2。地点0は設備0か2、地点1は0か1、地点2は1か2か3で覆えます。利益に負の費用を設定します。

**入力形式**

n m、n設備の費用、その後m行に「候補数k、地点を覆う設備IDをk個」。IDは0始まりです。

**入力例**

```text
4 3
    4 3 5 2
    2 0 2
    2 0 1
    3 1 2 3
```

**コード：`03_minimum_cover.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n, points;
    cin >> n >> points;

    PboProblem problem; // 設備を設置するかどうかのモデル
    problem.profit.resize(n); // 設備iに対応する変数iを用意

    for (int i = 0; i < n; ++i) {
        long long cost;
        cin >> cost;

        problem.profit[i] = -cost; // 費用最小化を、負の費用の最大化へ変換する
    }

    for (int j = 0; j < points; ++j) {
        int k;
        cin >> k;

        vector<PboTerm> covering; // この地点を覆える設備を数える
        for (int t = 0; t < k; ++t) {
            int id;
            cin >> id;

            covering.push_back({id, 1}); // 設置した設備を1件と数える
        }
        problem.add_ge(std::move(covering), 1); // この地点は最低1設備で覆う。Hard条件
    }

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=-6
x=1001
```

**読み方と注意点**

この例では定数やソフト減点がないため、費用は目的値の符号を反転した6です。入力で同じ設備IDを重複させると係数が合計されるので、地点ごとの候補リストは重複なく与えます。

### 3.4 各カテゴリから、ちょうど1案ずつ選ぶ

製品構成のように、CPUから1案、メモリから1案という選び方を考えます。カテゴリ内で複数選ぶことも、1つも選ばないことも許しません。

**解く問題**

4候補を2カテゴリに分け、ID0・1から1つ、ID2・3から1つを選びます。費用上限5の中で利益を大きくします。

**入力形式**

n g B、利益n個、費用n個、各カテゴリの「候補数k、候補IDをk個」をg行。

**入力例**

```text
4 2 5
    8 5 7 4
    4 2 3 1
    2 0 1
    2 2 3
```

**コード：`04_one_per_group.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n, groups;
    long long budget;
    cin >> n >> groups >> budget;

    PboProblem problem; // 候補ごとに採用の0/1変数を置く
    problem.profit.resize(n); // 変数数と利益を確保する
    for (auto& profit : problem.profit) { // 各候補の利益
        cin >> profit;
    }

    vector<PboTerm> costs; // 全候補を通した費用の行
    for (int i = 0; i < n; ++i) {
        long long cost;
        cin >> cost;

        costs.push_back({i, cost}); // 候補iを選ぶとcostだけ使う
    }
    problem.add_le(costs, budget); // 全体の費用上限をHardで追加

    for (int g = 0; g < groups; ++g) {
        int k;
        cin >> k;

        vector<PboTerm> choices; // このカテゴリ内の選択数を数える
        for (int t = 0; t < k; ++t) {
            int id;
            cin >> id;

            choices.push_back({id, 1}); // 1候補を1として数える。重複IDは入れない
        }
        problem.add_eq(choices, 1); // カテゴリごとに「ちょうど1」を要求する
    }

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=12
x=0110
```

**読み方と注意点**

同じ最良目的値になる構成が複数あります。出力例はその1つです。どの案を好むかまで指定したい場合は、その優先度を目的に含めます。

### 3.5 担当者の容量を守り、各仕事を1人へ割り当てる

仕事ごとの適性点を最大にしたい一方、担当者ごとに引き受けられる量が決まっています。「仕事iを担当者aに渡す」を1個の0/1変数にします。

**解く問題**

3仕事のサイズは2・2・1、2人の容量は3・3。仕事×担当者の適性点を使い、全仕事をちょうど1人へ割り当てます。

**入力形式**

仕事数n、担当者数a、仕事サイズn個、容量a個、続くn行に担当者別の適性点a個。

**入力例**

```text
3 2
    2 2 1
    3 3
    9 4
    8 7
    3 6
```

**コード：`05_assignment.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int jobs, agents;
    cin >> jobs >> agents;

    vector<long long> size(jobs), capacity(agents);
    for (auto& v : size) {
        cin >> v;
    }

    for (auto& v : capacity) {
        cin >> v;
    }

    PboProblem problem; // 変数ID=仕事ID*agents+担当者IDとする
    problem.profit.resize(jobs * agents); // 仕事と担当者の組合せごとに変数を置く
    for (auto& profit : problem.profit) { // 行優先の順で適性点を読む
        cin >> profit;
    }

    for (int i = 0; i < jobs; ++i) {
        vector<PboTerm> assign; // 仕事iを引き受ける担当者数
        for (int a = 0; a < agents; ++a) { // 各対応を1件と数える
            assign.push_back({i * agents + a, 1});
        }
        problem.add_eq(assign, 1); // すべての仕事を、ちょうど1人に割り当てる
    }

    for (int a = 0; a < agents; ++a) {
        vector<PboTerm> load; // 担当者aの総負荷
        for (int i = 0; i < jobs; ++i) { // サイズを係数にする
            load.push_back({i * agents + a, size[i]});
        }
        problem.add_le(load, capacity[a]); // 担当者aの容量を超えない
    }

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=22
x=100101
```

**読み方と注意点**

出力の6ビットを、仕事ごとに2ビットずつ読みます。容量不足で全仕事を割り当てられない場合もあるので、可否の確認を省かないでください。

### 3.6 衝突する案件を同時に採用しない

同じ場所を使うイベントや、同じ機材を独占する作業など、特定の2件を同時に採用できない問題です。衝突する組ごとに「2つの選択数の合計が1以下」と書きます。

**解く問題**

利益8・7・6・5の4案件について、(0,1)、(1,2)、(2,3)が衝突します。衝突しない案件の利益合計を最大化します。

**入力形式**

n m、n個の利益、続くm行に同時に選べない2つのID。

**入力例**

```text
4 3
    8 7 6 5
    0 1
    1 2
    2 3
```

**コード：`06_conflicts.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n, conflicts;
    cin >> n >> conflicts;

    PboProblem problem; // 案件の採否を決める
    problem.profit.resize(n); // n件の利益を用意
    for (auto& profit : problem.profit) { // 各案件の利益
        cin >> profit;
    }

    for (int e = 0; e < conflicts; ++e) {
        int a, b;
        cin >> a >> b;

        problem.add_le({{a, 1}, {b, 1}}, 1); // a+b<=1なので両方1になることを禁止
    }

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=14
x=1010
```

### 3.7 必要な準備も含めて、機能を採用する

ある機能を採用するなら、その前提となる準備も必要になる問題です。準備に費用がかかる場合は、準備の利益を負にします。準備だけを採用することは禁止しません。

**解く問題**

変数0は費用4の基盤、1と2は利益8・7の機能で、どちらも基盤が必要です。利益5の機能3は、機能1か2の少なくとも1つを必要とします。

**入力形式**

n d、n個の利益、d行に「機能ID、必須の前提ID」、最後に「機能ID、代替前提の数k、前提IDをk個」。

**入力例**

```text
4 2
    -4 8 7 5
    1 0
    2 0
    3 2 1 2
```

**コード：`07_dependencies.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n, dependencies;
    cin >> n >> dependencies;

    PboProblem problem; // 機能と準備を同じ0/1変数で扱う
    problem.profit.resize(n); // 負の利益も指定できる
    for (auto& profit : problem.profit) { // 準備費用は負の値にする
        cin >> profit;
    }

    for (int j = 0; j < dependencies; ++j) {
        int feature, prerequisite;
        cin >> feature >> prerequisite;

        problem.add_le({{feature, 1}, {prerequisite, -1}}, 0); // feature<=prerequisiteを表す
    }

    int feature, k;
    cin >> feature >> k;

    vector<PboTerm> alternatives{{feature, 1}}; // feature-前提の選択数<=0にする
    for (int j = 0; j < k; ++j) {
        int prerequisite;
        cin >> prerequisite;

        alternatives.push_back({prerequisite, -1}); // 前提を1つ選ぶと右辺相当を1増やす
    }
    problem.add_le(alternatives, 0); // 機能を選ぶなら代替前提を少なくとも1つ選ぶ

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=16
x=1111
```

### 3.8 時間帯ごとの必要人数と上限を満たす勤務パターンを選ぶ

1つの勤務パターンが複数の時間帯を担当するとき、時間帯ごとに最低人数と配置上限を守ります。各パターンを使う費用の合計を小さくします。

**解く問題**

3時間帯を覆う4パターンを選びます。最初と最後はちょうど1人、中央は1～2人必要です。費用は6・4・5・3です。

**入力形式**

n t、費用n個、続くt行に「下限、上限、各パターンが担当するなら1・しないなら0をn個」。

**入力例**

```text
4 3
    6 4 5 3
    1 1 1 0 1 0
    1 2 1 1 0 1
    1 1 0 1 1 0
```

**コード：`08_staffing.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n, slots;
    cin >> n >> slots;

    PboProblem problem; // パターンiを使うかどうかを変数iで表す
    problem.profit.resize(n); // nパターン分の利益を用意
    for (auto& profit : problem.profit) { // 費用を負利益へ
        long long cost;
        cin >> cost;
        profit = -cost;
    }

    for (int t = 0; t < slots; ++t) {
        long long lower, upper;
        cin >> lower >> upper;

        vector<PboTerm> staffing; // この時間帯に配置される人数
        for (int i = 0; i < n; ++i) {
            int covers;
            cin >> covers;

            if (covers) { // 担当するパターンだけを行へ追加
                staffing.push_back({i, 1});
            }
        }
        problem.add_range(staffing, lower, upper); // 両端を含むHard範囲。lower<=upperが必要
    }

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=-8
x=0011
```

### 3.9 入る量と出る量が釣り合う取引を選ぶ

採用すると在庫が増える取引と、在庫が減る取引を一緒に選びます。最終的な在庫変化を0にしながら利益を得る問題です。

**解く問題**

4取引の利益は3・4・5・1、在庫変化は+3・-2・-1・+1です。選んだ取引の在庫変化の合計を0にします。

**入力形式**

nと必要な合計変化、利益n個、正負の在庫変化n個。

**入力例**

```text
4 0
    3 4 5 1
    3 -2 -1 1
```

**コード：`09_balance.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long target;
    cin >> n >> target;

    PboProblem problem; // 取引の採否を表すモデル
    problem.profit.resize(n); // 各取引の利益
    for (auto& profit : problem.profit) { // 正負どちらも入力可能
        cin >> profit;
    }

    vector<PboTerm> balance; // 増加は正、減少は負の係数で足す
    for (int i = 0; i < n; ++i) {
        long long change;
        cin >> change;

        balance.push_back({i, change}); // この取引を選ぶ場合だけ在庫を変える
    }
    problem.add_eq(balance, target); // 符号付き合計をtargetに一致させるHard等式

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=12
x=1110
```

**読み方と注意点**

ここで守るのは最終的な合計だけです。途中の時刻の在庫が負にならない条件まで必要なら、時刻ごとの累積在庫を別の行として追加します。

### 3.10 OR・NOTを含む論理条件を満たす

「AかBの少なくとも一方」「Bを使わないか、Cを使う」「AとCを同時には使わない」といった条件をまとめて扱います。trueを1、falseを0と考えます。

**解く問題**

3変数について、(x0またはx1)、(x1でないまたはx2)、(x0でないまたはx2でない)をすべて満たし、利益2・4・3の合計を大きくします。

**入力形式**

n c、利益n個、続くc行は「論理項数k、符号付き番号をk個」。+vはx[v-1]、-vはその否定です。番号は1始まりで0は使いません。

**入力例**

```text
3 3
    2 4 3
    2 1 2
    2 -2 3
    2 -1 -3
```

**コード：`10_logical_clauses.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n, clauses;
    cin >> n >> clauses;

    PboProblem problem; // trueを1、falseを0として各命題を変数にする
    problem.profit.resize(n); // trueにする場合の利益を設定
    for (auto& profit : problem.profit) { // 利益を読む
        cin >> profit;
    }

    for (int c = 0; c < clauses; ++c) {
        int k;
        cin >> k;

        int negated = 0;

        vector<PboTerm> terms; // 正の項はx、否定は1-xとして扱う
        for (int j = 0; j < k; ++j) {
            int literal;
            cin >> literal;

            int id = abs(literal) - 1;
            terms.push_back({id, literal > 0 ? 1 : -1}); // 入力番号を0始まりIDに直し、符号を係数へ
            if (literal < 0) {
                ++negated;
            }
        }
        problem.add_ge(terms, 1 - negated); // 否定の定数1を右辺へ移し、少なくとも1項がtrueとする
    }

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=7
x=011
```

**読み方と注意点**

否定を含むORは、真になっている項の数を1以上にするだけです。入力番号の+1は変数ID0を意味するので、IDのずれに注意します。

### 3.11 供給量を目標へ近づけ、ずれ1単位ごとに減点する

必要量に近づけたいものの、供給の組合せが粗く、必ずぴったりにできるとは限りません。購入費用と、目標から離れた量の両方を評価します。

**解く問題**

供給量2・3・4、費用2・4・7の3パッケージから選びます。目標5から1単位離れるごとに3点引き、費用も点数から引きます。

**入力形式**

n 目標 減点単価、費用n個、供給量n個。

**入力例**

```text
3 5 3
    2 4 7
    2 3 4
```

**コード：`11_linear_penalty.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long target, penalty;
    cin >> n >> target >> penalty;

    PboProblem problem; // 購入するパッケージを選ぶ
    problem.profit.resize(n); // 変数ごとの費用を負の利益へ変換
    for (auto& profit : problem.profit) { // 購入費用を引く
        long long cost;
        cin >> cost;
        profit = -cost;
    }

    vector<PboTerm> supply; // 購入した場合の供給量
    for (int i = 0; i < n; ++i) {
        long long amount;
        cin >> amount;

        supply.push_back({i, amount}); // パッケージiの供給量を係数とする
    }
    // 上下どちらのずれも単価penaltyで減点
    problem.add_eq(supply, target, PboRowKind::LinearPenalty, penalty);

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=-6
x=110
```

**読み方と注意点**

目標を外れても実行可能です。この例にHard行はないからです。不足だけに罰を付けたい場合は、同じ種類・重みでadd_geを使います。

### 3.12 通常枠を超えたら、追加料金を1回だけ払う

資源を少しでも通常枠より多く使うと、固定の追加料金が発生する契約を考えます。超過量そのものには比例しません。ただし絶対に超えられない最大枠もあります。

**解く問題**

利益8・7・6、使用量3・2・2の3案件。使用量5以下を必須とし、通常枠4を超えると1点だけ引きます。

**入力形式**

n 最大枠 通常枠 追加料金、利益n個、使用量n個。

**入力例**

```text
3 5 4 1
    8 7 6
    3 2 2
```

**コード：`12_fixed_penalty.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long hard_limit, normal_limit, fee;
    cin >> n >> hard_limit >> normal_limit >> fee;

    PboProblem problem; // 案件の採否を表す
    problem.profit.resize(n); // 各案件の利益
    for (auto& profit : problem.profit) { // 採用時の利益を読む
        cin >> profit;
    }

    vector<PboTerm> usage; // 通常枠と最大枠で同じ使用量を使う
    for (int i = 0; i < n; ++i) {
        long long amount;
        cin >> amount;

        usage.push_back({i, amount}); // 案件iの使用量
    }
    problem.add_le(usage, hard_limit); // 絶対に超えないHard上限
    problem.add_le(usage, normal_limit, PboRowKind::FixedPenalty, fee); // 超過時にfeeを1回だけ減点

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=14
x=110
```

**読み方と注意点**

案件0と1なら利益15から追加料金1を引いて目的値14です。通常枠に違反しても、最大枠と固定値を守っているためfeasible()はtrueです。

### 3.13 組合せで発生する報酬や追加費用を扱う

機能を一緒に提供すると追加報酬が得られたり、特定の組合せだけ相性が悪く追加費用が発生したりする問題です。単独の利益だけでは表せない評価を加えます。

**解く問題**

4候補から3つまで選びます。単独利益4・3・2・1に加え、{0,1}で+5、{1,2}で-6、{2,3}で+4を加算します。

**入力形式**

n 選択数上限 組合せ数b、単独利益n個、続くb行は「要素数k、追加利益、IDをk個」。

**入力例**

```text
4 3 3
    4 3 2 1
    2 5 0 1
    2 -6 1 2
    2 4 2 3
```

**コード：`13_bundle_profit.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n, limit, bundles;
    cin >> n >> limit >> bundles;

    PboProblem problem; // 候補を採用するかのモデル
    problem.profit.resize(n); // 単独の利益を用意
    for (auto& profit : problem.profit) { // 組合せ利益は後で追加する
        cin >> profit;
    }

    vector<PboTerm> selected; // 選択数を数える
    for (int i = 0; i < n; ++i) { // 全変数を1ずつ数える
        selected.push_back({i, 1});
    }
    problem.add_le(selected, limit); // 最大limit個まで選べる

    for (int b = 0; b < bundles; ++b) {
        int k;
        long long bonus;
        cin >> k >> bonus;

        vector<int> ids(k);
        for (auto& id : ids) {
            cin >> id;
        }
        problem.add_all_profit(ids, bonus); // 指定した全IDが1のときだけ正負のbonusを加える
    }

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=13
x=1101
```

**読み方と注意点**

すべての組合せは独立に加算されます。1つ成立したら別の報酬が無効になる仕組みではありません。負の追加利益は、他の利益と合わせて目的値から差し引かれます。

### 3.14 予算内で覆った地点の価値を、一度だけ数える

すべてを覆う費用がない場合、予算内で価値の高い地点を多く覆いたいことがあります。2設備で同じ地点を覆っても、その地点の価値を二重には数えません。

**解く問題**

費用1の3設備から予算2で選び、4地点の価値5・4・3・2を得ます。設備0は地点0・1、設備1は1・2、設備2は2・3を覆います。設備変数に加え、地点が覆われたかを表す変数を置きます。

**入力形式**

設備数n 地点数m 予算B、設備費用n個、続くm行は「地点の価値、設備数k、覆う設備IDをk個」。

**入力例**

```text
3 4 2
    1 1 1
    5 1 0
    4 2 0 1
    3 2 1 2
    2 1 2
```

**コード：`14_maximum_coverage.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n, points;
    long long budget;
    cin >> n >> points >> budget;

    PboProblem problem; // 0..n-1は設備、n..n+points-1は地点の被覆状態
    problem.profit.assign(n + points, 0); // 設備自体の利益は0。地点側に価値を入れる

    vector<PboTerm> costs; // 設備の設置費用
    for (int i = 0; i < n; ++i) {
        long long cost;
        cin >> cost;

        costs.push_back({i, cost}); // 設備iを開く費用
    }
    problem.add_le(costs, budget); // 設置費用のHard上限

    for (int j = 0; j < points; ++j) {
        long long value;
        int k;
        cin >> value >> k;

        problem.profit[n + j] = value; // 地点jが覆われた場合だけ価値を得る

        vector<PboTerm> cover; // 地点jを覆う設備の選択数
        for (int t = 0; t < k; ++t) {
            int id;
            cin >> id;

            cover.push_back({id, 1}); // 入力の設備IDは重複させない
        }

        auto can_claim = cover; // 設備選択数-y>=0にするためのコピー
        can_claim.push_back({n + j, -1}); // y=1なら少なくとも1設備が必要
        problem.add_ge(can_claim, 0); // 覆っていない地点の価値を受け取ることを禁止

        cover.push_back({n + j, -k}); // 設備選択数-k*y<=0にする
        problem.add_le(cover, 0); // 1設備でも選んだらy=1。実際の被覆状態と一致させる
    }

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=14
x=1011111
```

**読み方と注意点**

出力の先頭3ビットが設備、残り4ビットが地点の被覆状態です。2本の条件で地点変数を実際の被覆と一致させているため、価値が0や負でも状態の意味が崩れません。

### 3.15 設備を開く固定費と、割り当て費用を一緒に最小化する

設備を使うには開設費が必要で、利用者を割り当てると別の費用もかかる問題です。開設していない設備へ利用者を割り当てることは禁止します。

**解く問題**

2設備の開設費は4・3。2利用者を1設備ずつへ割り当て、割当費用は利用者0が1・4、利用者1が2・1です。総費用を小さくします。

**入力形式**

設備数f 利用者数c、開設費f個、続くc行に設備ごとの割当費用f個。

**入力例**

```text
2 2
    4 3
    1 4
    2 1
```

**コード：`15_facility_opening.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int facilities, customers;
    cin >> facilities >> customers;

    PboProblem problem; // 先頭は開設変数、後ろは利用者×設備の割当変数
    problem.profit.resize(facilities + customers * facilities); // 必要な変数をすべて確保

    for (int f = 0; f < facilities; ++f) {
        long long cost;
        cin >> cost;

        problem.profit[f] = -cost; // 開設費を負の利益にする
    }

    for (int c = 0; c < customers; ++c) {
        vector<PboTerm> assignment; // 利用者cが選ぶ設備数
        for (int f = 0; f < facilities; ++f) {
            int id = facilities + c * facilities + f;
            long long cost;
            cin >> cost;

            problem.profit[id] = -cost; // この割当の費用を引く
            assignment.push_back({id, 1}); // 割当を1件として数える
            problem.add_le({{id, 1}, {f, -1}}, 0); // 割り当てるなら設備fが開設済みであること
        }
        problem.add_eq(assignment, 1); // 利用者はちょうど1設備を使う
    }

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=-7
x=101010
```

**読み方と注意点**

最初の2ビットが開設状態、その後を利用者ごとに2ビットずつ読みます。各設備の受入容量も必要なら、割当変数の合計を制限する行を追加できます。

### 3.16 小さい整数の生産数量を、2進数で表す

数量を0か1だけでなく、0～5個などの整数から選びたい場合です。1個分、2個分、4個分のビットを用意し、その合計を数量として扱います。

**解く問題**

製品0は上限5、単位利益3、資源2。製品1は上限3、単位利益5、資源3です。資源上限10で数量を決めます。各製品のビットは1、2、4…の順に並びます。

**入力形式**

製品数nと資源上限B。続くn行は「数量上限、単位利益、単位資源」。この例の数量上限は0～1,000,000とします。

**入力例**

```text
2 10
    5 3 2
    3 5 3
```

**コード：`16_integer_quantity.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int products;
    long long budget;
    cin >> products >> budget;

    PboProblem problem; // 数量を表すビットを順次追加していく

    vector<PboTerm> resource; // 各ビットが使う資源量
    for (int p = 0; p < products; ++p) {
        long long upper, unit_profit, unit_resource;
        cin >> upper >> unit_profit >> unit_resource;

        vector<PboTerm> quantity; // 製品pの数量を表す1,2,4,...の重み
        for (long long bit = 1; bit <= upper; bit *= 2) {
            int id = int(problem.profit.size()); // 追加するビットの変数ID
            problem.profit.push_back(bit * unit_profit); // このビットが立つとbit個分の利益
            quantity.push_back({id, bit}); // 数量にbitを加える
            resource.push_back({id, bit * unit_resource}); // 資源にもbit個分を加える
        }
        problem.add_le(quantity, upper); // ビットで表せても数量上限を超える値は禁止する
    }
    problem.add_le(resource, budget); // すべての製品で共有する資源上限

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=16
x=01001
```

**読み方と注意点**

製品0の3ビットが010なら数量2、製品1の2ビットが01なら数量2です。上限5を3ビットで表すと6や7も表せるので、数量の上限行を省いてはいけません。数量上限を大きく拡張するときは、ビットの倍増・係数の積・探索規模の範囲を確認してください。

### 3.17 目標から大きく離れるほど、二乗で強く減点する

目標から1離れる場合より、2離れる場合を4倍悪く評価したいことがあります。0/1変数では二乗を単独利益と2変数の同時選択利益へ展開できます。

**解く問題**

量2・3・4、費用1・2・5の3候補を選びます。総量の目標は4、ずれの二乗の重みは2。費用と二乗減点の合計を小さくします。

**入力形式**

n 目標T 重みw、量n個、費用n個。wは0以上とし、展開後の積もlong longに収まる入力を使います。

**入力例**

```text
3 4 2
    2 3 4
    1 2 5
```

**コード：`17_squared_deviation.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long target, weight;
    cin >> n >> target >> weight;

    vector<long long> amount(n), cost(n);
    for (auto& v : amount) {
        cin >> v;
    }

    for (auto& v : cost) {
        cin >> v;
    }

    PboProblem problem; // 数量を持つ候補を選ぶ
    problem.constant = -weight * target * target; // 二乗を展開した定数項を設定する
    problem.profit.resize(n); // 一次の項を変数ごとの利益へ入れる

    for (int i = 0; i < n; ++i) {
        // x_i^2=x_iを使う
        problem.profit[i] = -cost[i]
            + 2 * weight * target * amount[i]
            - weight * amount[i] * amount[i];

        for (int j = 0; j < i; ++j) {
            // 異なる2変数の積を1組につき1回追加
            problem.add_all_profit({j, i}, -2 * weight * amount[j] * amount[i]);
        }
    }

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=-4
x=010
```

**読み方と注意点**

数量の係数を$a_ {i}$、費用を$c_ {i}$、目標を$T$、重みを$w$、候補数を$n$とすると、この例の目的は次です。

$$F(x)=-\sum_ {i=0}^{n-1}c_ {i}x_ {i}-w\left(\sum_ {i=0}^{n-1}a_ {i}x_ {i}-T\right)^2$$

0と1のどちらでも$x_ {i}^2=x_ {i}$です。高校数学の二乗の展開を使うと、定数は$-wT^2$、変数$i$の係数は$-c_ {i}+2wTa_ {i}-wa_ {i}^2$、異なる$i,j$の積の係数は$-2wa_ {i}a_ {j}$です。これをコードの3か所へ入れています。量3だけを選ぶと、費用2とずれの二乗減点2で目的値-4です。候補間の組数が増えるため、大きい入力ではモデルが密になる点に注意します。

### 3.18 各仕事の開始時刻を選び、機械の重複使用を防ぐ

開始時刻を連続値として決めるのではなく、候補時刻を列挙します。「仕事iを時刻tに開始する」を変数にして、各仕事は1つの時刻を選び、各時間帯の機械使用数を制限します。

**解く問題**

4時間帯、1台の機械に、長さ2の仕事を2つ入れます。開始候補は0・1・2で、早さなどに応じた利益を入力します。処理区間は開始時刻を含み、終了時刻を含みません。

**入力形式**

仕事数nと時間帯数H。仕事ごとに「長さd、開始時刻0～H-dの利益をH-d+1個」。1<=d<=Hが必要です。

**入力例**

```text
2 4
    2 8 6 3
    2 7 5 4
```

**コード：`18_start_times.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int jobs, horizon;
    cin >> jobs >> horizon;

    struct Choice {
        int start, length, id;
    };
    vector<Choice> choices;

    PboProblem problem; // 各仕事の各開始時刻を別の0/1変数にする
    for (int i = 0; i < jobs; ++i) {
        int length;
        cin >> length;

        vector<PboTerm> one_start; // 仕事iの開始候補を数える
        for (int t = 0; t + length <= horizon; ++t) {
            long long gain;
            cin >> gain;

            int id = int(problem.profit.size()); // 入力順に開始候補のIDを割り当てる
            problem.profit.push_back(gain); // この開始時刻を採用した場合の利益
            one_start.push_back({id, 1}); // 仕事iが選ぶ候補を1件として数える
            choices.push_back({t, length, id});
        }
        problem.add_eq(one_start, 1); // 各仕事はちょうど1つの開始時刻を選ぶ
    }

    for (int t = 0; t < horizon; ++t) {
        vector<PboTerm> occupied; // 時間帯tに機械を使う候補を集める
        for (const auto& c : choices) {
            if (c.start <= t && t < c.start + c.length) { // 区間の終端は含めない
                occupied.push_back({c.id, 1});
            }
        }
        problem.add_le(occupied, 1); // 1台の機械なので同時実行は最大1仕事
    }

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=12
x=100001
```

**読み方と注意点**

出力の先頭3ビットは仕事0、次の3ビットは仕事1の開始時刻0・1・2です。時間を細かく刻むほど変数数・行数が増えるため、実際には有用な開始候補へ絞る方法も検討します。

### 3.19 行データを読み込み、汎用のモデルを作る

別のプログラムや表から作った条件を読み込みたい場合です。便利メソッドだけでなく、PboRowの各メンバを直接指定して、上下限・行の種類・重みをまとめて追加します。

**解く問題**

3変数の利益4・3・2に定数5を加え、変数2を0に固定します。Hard条件は2*x0+x1<=2、ソフト条件はx1>=1で不足1あたり2点減点です。Hard行の変数0の項は重複して入力します。

**入力形式**

n m 定数、利益n個、固定値n個。各行は「下限あり0/1 下限値 上限あり0/1 上限値 種類0/1/2 重み 項数k」に続き、k組の「変数ID 係数」。種類0=Hard、1=LinearPenalty、2=FixedPenaltyです。

**入力例**

```text
3 2 5
    4 3 2
    -1 -1 0
    0 0 1 2 0 1 4
    0 1
    0 1
    1 1
    2 0
    1 1 0 0 1 2 1
    1 1
```

**コード：`19_table_model.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n, m;
    long long constant;
    cin >> n >> m >> constant;

    PboProblem problem; // 表データから組み立てる入力モデル
    problem.constant = constant; // 常に加算する定数
    problem.profit.resize(n); // 変数ごとの利益
    for (auto& profit : problem.profit) { // 利益を読み込む
        cin >> profit;
    }

    problem.fixed.resize(n); // 指定する場合は必ず変数数と同じ長さにする
    for (auto& fixed : problem.fixed) { // -1/0/1を数値として読む
        int v;
        cin >> v;
        fixed = int8_t(v);
    }

    // この入力形式の対応表
    const PboRowKind kinds[] = {
        PboRowKind::Hard,          // 0: 必ず守る条件
        PboRowKind::LinearPenalty, // 1: 違反量に比例して減点
        PboRowKind::FixedPenalty   // 2: 違反したら定額を減点
    };

    for (int j = 0; j < m; ++j) {
        int has_lower, has_upper, kind, k;
        long long lower, upper, weight;
        cin >> has_lower >> lower >> has_upper >> upper >> kind >> weight >> k;
        if (kind < 0 || kind > 2) {
            return 1;
        }

        PboRow condition; // 初期値は上下限なし・Hard・重み1
        condition.lower = has_lower ? optional<long long>(lower) : nullopt; // 下限なしはnullopt
        condition.upper = has_upper ? optional<long long>(upper) : nullopt; // 上限なしもnullopt
        condition.kind = kinds[kind]; // 入力の番号を列挙型へ対応させる
        condition.weight = weight; // ソフト行の重みは0以上。Hardでは評価に使われない

        for (int t = 0; t < k; ++t) {
            int var;
            long long coef;
            cin >> var >> coef;

            condition.terms.push_back(PboTerm{var, coef}); // 項は{変数ID,係数}。重複は合計される
        }

        // 追加した行IDは、この入力ではjと一致する
        int row_id = problem.add_row(std::move(condition));
        (void)row_id;
    }

    PboSolver solver(std::move(problem), 42); // モデルを移して所有させる。42は乱数seed

    PboParam param; // 時間と探索量の条件。各メンバには既定値がある
    param.time_limit_us = -1; // 相対時間制限を外す。次の行の探索量上限と必ず併用する
    param.max_iterations = 100'000; // この小さい入力を調べる探索量の上限

    auto result = solver.solve(param); // 初期候補を省略して解く。結果は値コピーで保持する

    if (!result.evaluation.feasible()) { // Hard条件と固定値の両方を満たすか確認する
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "objective=" << result.evaluation.objective << '\n'; // 真の目的値。大きいほどよい

    cout << "x=";
    for (auto bit : result.x) { // 0/1配列は文字扱いを避けてintに変換する
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
objective=8
x=010
```

**読み方と注意点**

ソースを改造せず、表現可能な行をデータとして増やせます。変数0の係数1を2回入力すると、係数2になります。合計0になった項は構築時に消えるため、その項を後からset_coefficientで変更することはできません。

### 3.20 外部で作った候補の利益と違反を調べる

別のアルゴリズムや手作業で作った候補を採点したい場合です。探索を始めず、必須条件と固定値のどこに違反数が出るかを確認します。

**解く問題**

利益10・7・6、資源3・2・2、上限4の問題で、変数1を0に固定します。候補110をそのまま評価し、資源超過と固定値違反を別々に報告します。

**入力形式**

3変数の利益、3変数の資源量、資源上限、固定値3個、候補値3個。各候補値は0/1です。

**入力例**

```text
10 7 6
    3 2 2
    4
    -1 0 -1
    1 1 0
```

**コード：`20_evaluate_candidate.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    PboProblem problem; // この例は3変数固定の入力形式
    problem.profit.resize(3); // 3個の利益を読む
    for (auto& profit : problem.profit) { // 1にした場合の利益
        cin >> profit;
    }

    vector<PboTerm> usage; // 資源量の行
    for (int i = 0; i < 3; ++i) {
        long long v;
        cin >> v;

        usage.push_back({i, v}); // 変数iの資源係数
    }

    long long capacity;
    cin >> capacity;

    problem.add_le(usage, capacity); // Hard上限を追加

    problem.fixed.resize(3); // 恒久固定値も3個指定
    for (auto& f : problem.fixed) { // -1/0/1を数値で読む
        int v;
        cin >> v;
        f = int8_t(v);
    }

    array<uint8_t, 3> candidate{}; // spanに渡せる連続した0/1配列。vector<bool>は使わない
    for (auto& bit : candidate) { // uint8_tへ直接文字入力しない
        int v;
        cin >> v;
        bit = uint8_t(v);
    }

    const PboSolver solver(problem); // 評価だけならconstなsolverからも呼べる。seed既定値は1
    PboEvaluation evaluation = solver.evaluate(candidate); // 補正や探索をせず、指定候補を全評価する

    cout << evaluation << '\n'; // 出力演算子が可否・目的値・Hard違反数・固定違反数を表示
}
```

**出力例（標準出力）**

```text
PboEvaluation{feasible=false, objective=17, violated_hard_rows=1, violated_fixed_variables=1}
```

**読み方と注意点**

目的値17だけを見ると良さそうですが、Hard違反も固定値違反も1つあります。evaluateは候補を0固定に合わせて補正しません。

### 3.21 初期解を取り込み、保存してから続きを探す

既存の実行可能解があり、その内容を保存しつつ追加の探索を行いたい場合です。同じsolverへ再び初期候補を渡した場合の意味も確認します。

**解く問題**

3案件の容量付き選択に、初期解100を与えます。まず探索量0で取り込み、次に探索を継続します。その後で古い100を再提示しても、保存された良い候補は残ります。

**入力形式**

n 上限、利益n個、資源量n個、初期候補n個。

**入力例**

```text
3 4
10 7 6
3 2 2
1 0 0
```

**コード：`21_initial_and_continue.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long capacity;
    cin >> n >> capacity;

    PboProblem problem; // n候補の容量付き選択問題
    problem.profit.resize(n); // 各候補の利益を用意
    for (auto& profit : problem.profit) { // 利益を入力
        cin >> profit;
    }

    vector<PboTerm> usage; // 資源の使用量を表す
    for (int i = 0; i < n; ++i) {
        long long amount;
        cin >> amount;

        usage.push_back({i, amount}); // 変数iの使用量を係数へ入れる
    }

    // Hard上限を追加し、後で変更できる行IDを得る
    int capacity_row = problem.add_le(usage, capacity);
    (void)capacity_row;

    vector<uint8_t> initial(n); // 初期候補の0/1配列。変数数と同じ長さが必要
    for (auto& bit : initial) { // 0/1を数値として読む
        int v;
        cin >> v;
        bit = uint8_t(v);
    }

    PboSolver solver(problem); // モデルを所有させる

    PboParam param; // 取り込みと継続で同じ条件オブジェクトを使う
    param.time_limit_us = -1; // 相対時間制限なし。探索量上限と併用する
    param.max_iterations = 0; // 最初は探索せず、初期候補の取り込みと評価だけ行う

    auto before = solver.solve(param, initial); // 値コピーなので後の呼出しでもbeforeは保存される

    if (!before.evaluation.feasible()) { // この用途では初期解の可否を確認
        cout << "INITIAL_NOT_FEASIBLE\n";
        return 0;
    }

    param.max_iterations = 100'000; // 続く呼出しでは探索を許す

    auto after = solver.solve(param); // 初期候補の省略はリセットではなく、探索の継続

    auto offered = solver.solve(param, initial); // 古い候補を渡しても保存済み候補を捨てない

    if (!after.evaluation.feasible() || !offered.evaluation.feasible()) { // 採用前に必須条件を確認
        return 1;
    }

    // 各時点の評価
    cout << before.evaluation.objective
         << ' ' << after.evaluation.objective
         << ' ' << offered.evaluation.objective
         << '\n';
}
```

**出力例（標準出力）**

```text
10 13 13
```

**読み方と注意点**

auto beforeはコピーです。const auto& beforeとするとsolver内部への参照になり、後の呼出しを経た過去結果の保存にはなりません。独立にやり直す用途は「同じ条件を再現する」の例を参照してください。

### 3.22 確定済みの外側を保ち、一部の変数だけ解き直す

全体を変更すると困る場面で、指定した範囲だけを見直します。外側の固定は今回渡す初期候補を基準にし、以前見つけた別の解を混ぜないことが大切です。

**解く問題**

全体では011が良い解ですが、初期候補100の変数0を外側として保持し、1・2だけ変更できる状態で解き直します。最後にsolveを呼ぶと可変範囲の制限が外れることも確認します。

**入力形式**

n 上限、利益n個、資源量n個、初期候補n個、変更可能ID数kとIDをk個。

**入力例**

```text
3 4
10 7 6
3 2 2
1 0 0
2 1 2
```

**コード：`22_partial_improvement.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long capacity;
    cin >> n >> capacity;

    PboProblem problem; // n候補の容量付き選択問題
    problem.profit.resize(n); // 各候補の利益を用意
    for (auto& profit : problem.profit) { // 利益を入力
        cin >> profit;
    }

    vector<PboTerm> usage; // 資源の使用量を表す
    for (int i = 0; i < n; ++i) {
        long long amount;
        cin >> amount;

        usage.push_back({i, amount}); // 変数iの使用量を係数へ入れる
    }

    // Hard上限を追加し、後で変更できる行IDを得る
    int capacity_row = problem.add_le(usage, capacity);
    (void)capacity_row;

    vector<uint8_t> initial(n); // 初期候補の0/1配列。変数数と同じ長さが必要
    for (auto& bit : initial) { // 0/1を数値として読む
        int v;
        cin >> v;
        bit = uint8_t(v);
    }

    int k;
    cin >> k;

    vector<int> free_variables(k);
    for (auto& id : free_variables) {
        cin >> id;
    }

    PboSolver solver(problem); // 恒久固定のないモデルを構築

    PboParam param; // 1回の探索に使う条件
    param.time_limit_us = -1; // 相対時間制限を外し、探索量で止める
    param.max_iterations = 100'000; // 有限の上限を必ず指定する

    auto global = solver.solve(param); // 全体の結果を値コピーして、部分探索後も残しておく

    // 外側はinitialの値を保持。initialは恒久固定に一致させる
    auto local = solver.improve(initial, free_variables, param);

    auto full = solver.solve(param); // 次のsolveは全自由変数を使い、前回の可変範囲に縛られない

    // 返却解を確認
    if (!global.evaluation.feasible() ||
        !local.evaluation.feasible() ||
        !full.evaluation.feasible()) {
        return 1;
    }

    // 各時点の目的値
    cout << "global=" << global.evaluation.objective
         << " local=" << local.evaluation.objective
         << " full=" << full.evaluation.objective
         << '\n';

    cout << "local_x=";
    for (auto bit : local.x) { // 外側の変数0が初期候補の1を保っているか確認
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力）**

```text
global=13 local=10 full=13
local_x=100
```

**読み方と注意点**

localの目的値10は、外側を固定した今回の部分問題の結果です。以前のglobal=13を黙って返すことはありません。free_variablesに恒久固定した変数を入れても、その固定は解除されません。

### 3.23 報酬や基準点の変更を反映する

問題の形はそのままで、候補の価値や全体の基準点だけ変わる場面です。過去の選択に付いていた古い点数を、そのまま新しい点数と比較しないようにします。

**解く問題**

3案件の問題を解いた後、案件0の利益を20、常に加える定数を100に変えます。保存した候補を新しいモデルで評価し、さらに解き直します。

**入力形式**

n 上限、利益n個、資源量n個、最後に「変更する変数ID、新利益、新定数」。

**入力例**

```text
3 4
10 7 6
3 2 2
0 20 100
```

**コード：`23_profit_update.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long capacity;
    cin >> n >> capacity;

    PboProblem problem; // n候補の容量付き選択問題
    problem.profit.resize(n); // 各候補の利益を用意
    for (auto& profit : problem.profit) { // 利益を入力
        cin >> profit;
    }

    vector<PboTerm> usage; // 資源の使用量を表す
    for (int i = 0; i < n; ++i) {
        long long amount;
        cin >> amount;

        usage.push_back({i, amount}); // 変数iの使用量を係数へ入れる
    }

    // Hard上限を追加し、後で変更できる行IDを得る
    int capacity_row = problem.add_le(usage, capacity);
    (void)capacity_row;

    int var;
    long long profit, constant;
    cin >> var >> profit >> constant;

    PboSolver solver(problem); // 元のモデルを構築

    PboParam param; // 1回の探索に使う条件
    param.time_limit_us = -1; // 相対時間制限を外し、探索量で止める
    param.max_iterations = 100'000; // 有限の上限を必ず指定する

    auto before = solver.solve(param); // 更新前の結果をコピー

    solver.set_profit(var, profit); // 変数varの利益だけを置き換える
    solver.set_constant(constant); // 定数は加算ではなく、新しい値へ置き換える
    auto revalued = solver.evaluate(before.x); // setter直後から新しいモデルで採点できる
    auto after = solver.solve(param); // 保存候補も再評価したうえで続きを探す

    if (!before.evaluation.feasible() || !after.evaluation.feasible()) { // 使用する結果の可否を確認
        return 1;
    }

    // 古い評価・同じ選択の新評価・新解
    cout << "old=" << before.evaluation.objective
         << " revalued=" << revalued.objective
         << " new=" << after.evaluation.objective
         << '\n';
}
```

**出力例（標準出力）**

```text
old=13 revalued=113 new=120
```

**読み方と注意点**

定数100を足すだけなら最良の選び方は変わりません。この例で選び方が変わるのは、案件0の利益を上げたためです。コピーしたbeforeの評価は自動で書き換わりません。

### 3.24 容量の変更や、片側の境界の解除を行う

資源が追加で使えるようになったり、上限の代わりに最低使用量だけを課すようになったりする場面です。条件の項を作り直さず、境界だけを変更できます。

**解く問題**

容量4で解いた後、上限を5へ変えます。さらに上限を外し、使用量2以上という条件だけを残します。

**入力形式**

n 元の上限、利益n個、資源量n個、最後に「新しい上限、次に使う下限」。

**入力例**

```text
3 4
10 7 6
3 2 2
5 2
```

**コード：`24_bounds_update.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long capacity;
    cin >> n >> capacity;

    PboProblem problem; // n候補の容量付き選択問題
    problem.profit.resize(n); // 各候補の利益を用意
    for (auto& profit : problem.profit) { // 利益を入力
        cin >> profit;
    }

    vector<PboTerm> usage; // 資源の使用量を表す
    for (int i = 0; i < n; ++i) {
        long long amount;
        cin >> amount;

        usage.push_back({i, amount}); // 変数iの使用量を係数へ入れる
    }

    // Hard上限を追加し、後で変更できる行IDを得る
    int capacity_row = problem.add_le(usage, capacity);
    (void)capacity_row;

    long long new_upper, new_lower;
    cin >> new_upper >> new_lower;

    PboSolver solver(problem); // 行IDは入力モデルから引き継がれる

    PboParam param; // 1回の探索に使う条件
    param.time_limit_us = -1; // 相対時間制限を外し、探索量で止める
    param.max_iterations = 100'000; // 有限の上限を必ず指定する

    auto before = solver.solve(param); // 元の容量で解く

    // 下限なし、上限new_upperへ両境界を置き換える
    solver.set_row_bounds(capacity_row, nullopt, new_upper);
    auto enlarged = solver.solve(param); // 新しい容量で候補を再評価して探索

    solver.set_row_bounds(capacity_row, new_lower, nullopt); // 下限だけを残し、上限を解除する
    auto lower_only = solver.solve(param); // 片側境界でも同じsolveを使う

    // 各条件での可否
    if (!before.evaluation.feasible() ||
        !enlarged.evaluation.feasible() ||
        !lower_only.evaluation.feasible()) {
        return 1;
    }

    // 3つのモデルの結果
    cout << before.evaluation.objective
         << ' ' << enlarged.evaluation.objective
         << ' ' << lower_only.evaluation.objective
         << '\n';
}
```

**出力例（標準出力）**

```text
13 17 23
```

**読み方と注意点**

nulloptは0という境界ではなく、「その側の境界なし」です。以前の境界を維持する特別値でもないので、残す側の値も呼出し時に渡します。

### 3.25 必須条件と、2種類の減点方針を切り替える

同じ条件でも、必ず守る契約なのか、超過量に応じた料金なのか、固定料金で緩和できるのかで扱いが変わります。方針を切り替えて結果を比較します。

**解く問題**

容量4の行を、Hard、超過1あたり2点のLinearPenalty、超過時3点のFixedPenalty、減点なし、再びHardの順に変更します。

**入力形式**

n 境界、利益n個、使用量n個、最後に「比例減点単価、定額減点」。

**入力例**

```text
3 4
10 7 6
3 2 2
2 3
```

**コード：`25_penalty_update.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long capacity;
    cin >> n >> capacity;

    PboProblem problem; // n候補の容量付き選択問題
    problem.profit.resize(n); // 各候補の利益を用意
    for (auto& profit : problem.profit) { // 利益を入力
        cin >> profit;
    }

    vector<PboTerm> usage; // 資源の使用量を表す
    for (int i = 0; i < n; ++i) {
        long long amount;
        cin >> amount;

        usage.push_back({i, amount}); // 変数iの使用量を係数へ入れる
    }

    // Hard上限を追加し、後で変更できる行IDを得る
    int capacity_row = problem.add_le(usage, capacity);
    (void)capacity_row;

    long long per_unit, fixed_fee;
    cin >> per_unit >> fixed_fee;

    PboSolver solver(problem); // 境界と項を共有して種類だけ変えていく

    PboParam param; // 1回の探索に使う条件
    param.time_limit_us = -1; // 相対時間制限を外し、探索量で止める
    param.max_iterations = 100'000; // 有限の上限を必ず指定する

    auto hard = solver.solve(param); // 最初はHard条件

    // 超過量に単価を掛けて減点
    solver.set_row_penalty(capacity_row, PboRowKind::LinearPenalty, per_unit);
    auto linear = solver.solve(param); // ソフト違反は実行可能性を損なわない

    // 超過したら定額を1回引く
    solver.set_row_penalty(capacity_row, PboRowKind::FixedPenalty, fixed_fee);
    auto fixed = solver.solve(param); // 新しい減点方式で再評価して解く

    // 重み0でこの行の減点を無効にする
    solver.set_row_penalty(capacity_row, PboRowKind::FixedPenalty, 0);
    auto unpenalized = solver.solve(param); // Hardではないので、この行は可否にも影響しない

    // 省略したweightは1になるが、Hardでは使わない
    solver.set_row_penalty(capacity_row, PboRowKind::Hard);
    auto restored = solver.solve(param); // 再び必須条件として守らせる

    // Hardへ戻したときも可否を確認
    if (!hard.evaluation.feasible() || !restored.evaluation.feasible()) {
        return 1;
    }

    // 方針ごとの目的値
    cout << hard.evaluation.objective
         << ' ' << linear.evaluation.objective
         << ' ' << fixed.evaluation.objective
         << ' ' << unpenalized.evaluation.objective
         << ' ' << restored.evaluation.objective
         << '\n';
}
```

**出力例（標準出力）**

```text
13 17 20 23 13
```

**読み方と注意点**

Softに変えた直後、以前の境界を超える候補が選ばれても異常ではありません。また、これらの目的値は異なる評価方針の点数であり、そのまま同じ条件での性能比較とは扱えません。

### 3.26 既存候補の資源使用量を変更する

処理が軽くなった、消費電力が変わったなど、既存の変数が条件へ与える量だけを変更したい場面です。項を0にして無効化し、後で戻すこともできます。

**解く問題**

案件0の工数3を1へ変え、次に0へし、最後に元の3へ戻します。容量4と利益は同じです。

**入力形式**

n 上限、利益n個、工数n個、最後に「対象ID、新工数」。対象の元の工数は非ゼロとします。

**入力例**

```text
3 4
10 7 6
3 2 2
0 1
```

**コード：`26_coefficient_update.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long capacity;
    cin >> n >> capacity;

    PboProblem problem; // n候補の容量付き選択問題
    problem.profit.resize(n); // 各候補の利益を用意
    for (auto& profit : problem.profit) { // 利益を入力
        cin >> profit;
    }

    vector<PboTerm> usage; // 資源の使用量を表す
    for (int i = 0; i < n; ++i) {
        long long amount;
        cin >> amount;

        usage.push_back({i, amount}); // 変数iの使用量を係数へ入れる
    }

    // Hard上限を追加し、後で変更できる行IDを得る
    int capacity_row = problem.add_le(usage, capacity);
    (void)capacity_row;

    int var;
    long long replacement;
    cin >> var >> replacement;
    long long original = usage[var].coef; // この入力ではusage[i]が変数iの項。元の係数を保持する

    PboSolver solver(problem); // 元の非ゼロ項が構築時に存在することを前提にする

    PboParam param; // 1回の探索に使う条件
    param.time_limit_us = -1; // 相対時間制限を外し、探索量で止める
    param.max_iterations = 100'000; // 有限の上限を必ず指定する

    auto before = solver.solve(param); // 元の工数で解く

    // 既存項の係数だけを変更。新規項は追加できない
    solver.set_coefficient(capacity_row, var, replacement);
    auto changed = solver.solve(param); // 工数の変化を反映して解く

    solver.set_coefficient(capacity_row, var, 0); // setterで0にした項は構造に残る
    auto zero = solver.solve(param); // 今回はその変数が工数を使わない

    solver.set_coefficient(capacity_row, var, original); // 残っている項なので非ゼロへ戻せる
    auto restored = solver.solve(param); // 元の工数へ戻ったモデルで解く

    // 各結果を確認
    if (!before.evaluation.feasible() ||
        !changed.evaluation.feasible() ||
        !zero.evaluation.feasible() ||
        !restored.evaluation.feasible()) {
        return 1;
    }

    // 工数ごとの目的値
    cout << before.evaluation.objective
         << ' ' << changed.evaluation.objective
         << ' ' << zero.evaluation.objective
         << ' ' << restored.evaluation.objective
         << '\n';
}
```

**出力例（標準出力）**

```text
13 17 23 13
```

**読み方と注意点**

構築時に係数0の項を置いても、正規化で消えるので同じ使い方はできません。構造にない項を追加する用途は、rebuildの例で扱います。

### 3.27 採用・不採用を確定し、後から解除する

すでに発注した案件を必ず選ぶ、利用できなくなった候補を必ず外すなどの確定条件です。部分探索の可変リストと異なり、恒久固定はsolveにもimproveにも効きます。

**解く問題**

自由な3案件を解いた後、案件0を1固定、0固定、自由へと変更します。

**入力形式**

n 上限、利益n個、工数n個、最後に固定する変数ID。

**入力例**

```text
3 4
10 7 6
3 2 2
0
```

**コード：`27_commitments.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long capacity;
    cin >> n >> capacity;

    PboProblem problem; // n候補の容量付き選択問題
    problem.profit.resize(n); // 各候補の利益を用意
    for (auto& profit : problem.profit) { // 利益を入力
        cin >> profit;
    }

    vector<PboTerm> usage; // 資源の使用量を表す
    for (int i = 0; i < n; ++i) {
        long long amount;
        cin >> amount;

        usage.push_back({i, amount}); // 変数iの使用量を係数へ入れる
    }

    // Hard上限を追加し、後で変更できる行IDを得る
    int capacity_row = problem.add_le(usage, capacity);
    (void)capacity_row;

    int var;
    cin >> var;

    PboSolver solver(problem); // fixedを省略したので初めは全変数が自由

    PboParam param; // 1回の探索に使う条件
    param.time_limit_us = -1; // 相対時間制限を外し、探索量で止める
    param.max_iterations = 100'000; // 有限の上限を必ず指定する

    auto before = solver.solve(param); // 固定前の結果をコピー

    solver.set_fixed(var, 1); // 変数varを採用で固定。次の探索で候補へ反映される
    auto selected = solver.solve(param); // 固定値を守りながら他を解く

    solver.set_fixed(var, 0); // 同じ変数を不採用で固定
    auto excluded = solver.solve(param); // 固定の変更を反映

    solver.set_fixed(var, -1); // -1で固定を解除する。0では解除にならない
    auto released = solver.solve(param); // 再び自由な変数として探索する

    // 固定とHardの両方を確認
    if (!before.evaluation.feasible() ||
        !selected.evaluation.feasible() ||
        !excluded.evaluation.feasible() ||
        !released.evaluation.feasible()) {
        return 1;
    }

    // 固定状態ごとの目的値
    cout << before.evaluation.objective
         << ' ' << selected.evaluation.objective
         << ' ' << excluded.evaluation.objective
         << ' ' << released.evaluation.objective
         << '\n';
}
```

**出力例（標準出力）**

```text
13 10 13 13
```

**読み方と注意点**

固定同士やHard条件と矛盾する指定をすると、実行可能解を返せない場合があります。fixedは「最初だけこの値で始める」という指定ではありません。初期候補を渡す操作とは区別します。

### 3.28 条件の追加と、候補数の変更に対応する

数値の更新だけでなく、新しい条件や変数が必要になった場合です。同じIDの意味を保って候補を引き継ぐ場合と、違う問題として作り直す場合を分けます。

**解く問題**

まず3案件の問題に「案件0を必ず採用」を追加して解き直します。次に候補数を2へ変え、利益8・9から1つだけ選ぶ別モデルへ切り替えます。

**入力形式**

n 上限、利益n個、工数n個、必須にするID、新モデルの変数数n2・選択数上限、利益n2個。

**入力例**

```text
3 4
10 7 6
3 2 2
0
2 1
8 9
```

**コード：`28_rebuild.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long capacity;
    cin >> n >> capacity;

    PboProblem problem; // n候補の容量付き選択問題
    problem.profit.resize(n); // 各候補の利益を用意
    for (auto& profit : problem.profit) { // 利益を入力
        cin >> profit;
    }

    vector<PboTerm> usage; // 資源の使用量を表す
    for (int i = 0; i < n; ++i) {
        long long amount;
        cin >> amount;

        usage.push_back({i, amount}); // 変数iの使用量を係数へ入れる
    }

    // Hard上限を追加し、後で変更できる行IDを得る
    int capacity_row = problem.add_le(usage, capacity);
    (void)capacity_row;

    int required;
    cin >> required;
    int new_n, count_limit;
    cin >> new_n >> count_limit;

    PboProblem replacement; // 変数数と意味が異なる新モデルを別に作る
    replacement.profit.resize(new_n); // 新しい変数数を設定
    for (auto& profit : replacement.profit) { // 新しい候補の利益
        cin >> profit;
    }

    vector<PboTerm> count; // 新モデルの選択数
    for (int i = 0; i < new_n; ++i) { // 候補を1件ずつ数える
        count.push_back({i, 1});
    }
    replacement.add_le(count, count_limit); // 新モデルだけの条件を追加する

    PboSolver solver(problem); // problemをコピーするので、利用側にも元モデルを残せる

    PboParam param; // 1回の探索に使う条件
    param.time_limit_us = -1; // 相対時間制限を外し、探索量で止める
    param.max_iterations = 100'000; // 有限の上限を必ず指定する

    auto before = solver.solve(param); // 元モデルの結果

    // 利用側モデルに新しい行を追加。これだけではsolverは変わらない
    problem.add_eq({{required, 1}}, 1);
    solver.rebuild(problem); // keep_solution既定値true。変数数と各IDの意味を保って再構築
    auto constrained = solver.solve(param); // 新条件で引継ぎ候補も再評価して解く

    solver.rebuild(std::move(replacement), false); // 変数数・意味が変わるので必ず引継ぎを無効にする
    auto changed = solver.solve(param); // 新しい変数集合で候補を作る

    // すべてのモデルで可否を確認
    if (!before.evaluation.feasible() ||
        !constrained.evaluation.feasible() ||
        !changed.evaluation.feasible()) {
        return 1;
    }

    // 新しい配列長も確認
    cout << before.evaluation.objective
         << ' ' << constrained.evaluation.objective
         << ' ' << changed.evaluation.objective
         << " n=" << changed.x.size()
         << '\n';
}
```

**出力例（標準出力）**

```text
13 10 9 n=2
```

**読み方と注意点**

keep_solution=trueでも新しいHard条件を自動で満たせる保証はありません。再構築後の結果を確認します。行IDは新モデルの並び順になるため、古いIDが同じ条件を指すかは利用側で管理します。

### 3.29 探索量とseedをそろえ、同じ条件を再現する

設定の比較やデバッグで同じ結果を再現したい場面です。乱数だけでなく、保存候補や探索の開始状態もそろえ、実時間による打切りを使わないようにします。

**解く問題**

24候補から12個まで選ぶ問題を、同じseed・探索量で2回解きます。経過時間は一致対象にせず、選択値・目的値・探索量を比較します。

**入力形式**

n 上限、利益n個、工数n個、最後に「seed、0以上の探索量上限」。

**入力例**

```text
24 12
1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1
123 1000
```

**コード：`29_reproducible.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long capacity;
    cin >> n >> capacity;

    PboProblem problem; // n候補の容量付き選択問題
    problem.profit.resize(n); // 各候補の利益を用意
    for (auto& profit : problem.profit) { // 利益を入力
        cin >> profit;
    }

    vector<PboTerm> usage; // 資源の使用量を表す
    for (int i = 0; i < n; ++i) {
        long long amount;
        cin >> amount;

        usage.push_back({i, amount}); // 変数iの使用量を係数へ入れる
    }

    // Hard上限を追加し、後で変更できる行IDを得る
    int capacity_row = problem.add_le(usage, capacity);
    (void)capacity_row;

    uint64_t seed;
    int64_t iterations;
    cin >> seed >> iterations;

    PboSolver solver(problem, seed); // 入力したseedで開始する

    PboParam param; // 再現実験用の実行条件
    param.time_limit_us = -1; // 実時間による打切りを使わない
    param.max_iterations = iterations; // 有限・0以上の探索量で必ず停止させる

    auto first = solver.solve(param); // 最初の結果をコピーする

    solver.reset_search(); // モデルを残して、保存候補と探索履歴を引き継がずに始める
    solver.reseed(seed); // reset_searchだけでは乱数は戻らないので、同じseedを再設定
    auto second = solver.solve(param); // 同じ入力・seed・探索量でもう一度実行

    // elapsed_usは比較しない
    bool same = first.x == second.x
        && first.evaluation.objective == second.evaluation.objective
        && first.iterations == second.iterations;

    // 同じ候補になったかと可否を表示
    cout << "same=" << same << " feasible=" << second.evaluation.feasible() << '\n';
}
```

**出力例（標準出力）**

```text
same=1 feasible=1
```

**読み方と注意点**

同一ソース、入力、環境、呼出し順での再現例です。reset_searchだけ、またはreseedだけでは、それぞれ乱数状態または探索状態がそろいません。deadlineも指定しないことが必要です。

### 3.30 全体締切を共有して、複数の独立した探索を試す

1回に全時間を渡す代わりに、短い探索を複数回試して最良結果を利用側に残します。全体の締切は共通にし、各呼出しにも小さい時間予算を与えます。

**解く問題**

24候補から12個まで選びます。実行可能な初期候補を毎回渡し、seedを変えて最大20回試します。全体予算2ミリ秒、各呼出しの予算200マイクロ秒です。

**入力形式**

n 上限、利益n個、工数n個、初期候補n個、最後に「全体ミリ秒、1回マイクロ秒、最大呼出し数、最初のseed」。時間は正、最大呼出し数は1以上とします。

**入力例**

```text
24 12
1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 0 0
2 200 20 100
```

**コード：`30_shared_deadline.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long capacity;
    cin >> n >> capacity;

    PboProblem problem; // n候補の容量付き選択問題
    problem.profit.resize(n); // 各候補の利益を用意
    for (auto& profit : problem.profit) { // 利益を入力
        cin >> profit;
    }

    vector<PboTerm> usage; // 資源の使用量を表す
    for (int i = 0; i < n; ++i) {
        long long amount;
        cin >> amount;

        usage.push_back({i, amount}); // 変数iの使用量を係数へ入れる
    }

    // Hard上限を追加し、後で変更できる行IDを得る
    int capacity_row = problem.add_le(usage, capacity);
    (void)capacity_row;

    vector<uint8_t> initial(n); // 初期候補の0/1配列。変数数と同じ長さが必要
    for (auto& bit : initial) { // 0/1を数値として読む
        int v;
        cin >> v;
        bit = uint8_t(v);
    }

    double budget_ms, slice_us;
    int max_calls;
    uint64_t seed;
    cin >> budget_ms >> slice_us >> max_calls >> seed;

    PboSolver solver(problem, seed); // 構築は全体締切を作る前に行う例
    auto initial_score = solver.evaluate(initial); // 初期解を先に採点する

    if (!initial_score.feasible()) { // 初期解の実行可能性を確認
        cout << "INITIAL_NOT_FEASIBLE\n";
        return 0;
    }

    using Clock = chrono::steady_clock;
    auto deadline = Clock::now() + chrono::microseconds(int64_t(budget_ms * 1000));

    PboParam param; // 全呼出しで共有する条件
    param.time_limit_us = slice_us; // 各呼出し単独の相対予算。単位はマイクロ秒
    param.deadline = deadline; // 絶対締切は全呼出しで同じ。毎回延長しない

    optional<PboResult> best; // solverの参照ではなく、全体最良の結果を所有して保持
    int calls = 0;

    do {
        solver.reset_search(); // 各試行を独立した探索状態から始める
        solver.reseed(seed + uint64_t(calls)); // 試行ごとにseedを変える
        // 実行可能な初期候補を毎回渡し、その品質を保持する
        auto candidate = solver.solve(param, initial);

        // 同じモデルの目的値で全体最良を保存
        if (candidate.evaluation.feasible() &&
            (!best ||
             candidate.evaluation.objective > best->evaluation.objective)) {
            best = std::move(candidate);
        }
        ++calls;
    } while (calls < max_calls && Clock::now() < deadline);

    if (!best) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // 可否・最良目的値・実際の呼出し数
    cout << "feasible=" << best->evaluation.feasible()
         << " objective=" << best->evaluation.objective
         << " calls=" << calls
         << '\n';

    cout << "x=";
    for (auto bit : best->x) { // 最良結果の全変数を数値で表示
        cout << int(bit);
    }
    cout << '\n';
}
```

**出力例（標準出力。実行ごとに変わる値を含む）**

```text
feasible=1 objective=222 calls=8
x=000000000000111111111111
```

**読み方と注意点**

目的値・呼出し数・選択値は実行環境や時間配分で変わります。初期候補の目的値78以上の実行可能解を保持します。初期化や終了処理による超過があり得るため、本番では締切に余裕を取り、入力・構築も含めた全体予算ならdeadlineをさらに前で作ります。

### 3.31 大きい結果を、そのままログへ出す

目的値や選択配列を毎回手で整形する代わりに、返却構造体全体を出力します。変数が多くてもログが配列全件で埋まらないよう、表示は先頭20要素に制限されます。

**解く問題**

25変数をすべて1にした初期候補を取り込み、PboResultを標準出力、評価だけを標準エラー出力に表示します。配列が25個のまま保持されていることも表示します。

**入力形式**

変数数n、続いて候補値0/1をn個。各変数の利益はコード内で1とします。

**入力例**

```text
25
1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1
```

**コード：`31_debug_output.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    cin >> n;

    PboProblem problem; // 条件なしで、候補の表示を確認するモデル
    problem.profit.assign(n, 1); // 各変数を選ぶと1点

    vector<uint8_t> initial(n); // 入力候補はn個の0/1値
    for (auto& bit : initial) { // 数値で読み、uint8_tへ変換
        int v;
        cin >> v;
        bit = uint8_t(v);
    }

    PboSolver solver(problem); // 利益と変数数をsolverへ渡す

    PboParam param; // 候補を取り込む実行条件
    param.time_limit_us = 0; // 探索せず、初期候補の評価と返却だけ行う

    auto result = solver.solve(param, initial); // 結果をコピーして保持

    cout << result << '\n'; // PboResultの出力演算子。xは先頭20個と総要素数を表示
    cerr << result.evaluation << '\n'; // PboEvaluationも直接出せる。標準エラー出力へ分離
    cout << "stored=" << result.x.size() << '\n'; // 表示が省略されても、実際の配列長は変わらない
}
```

**出力例（標準出力。実行ごとに変わる値を含む）**

```text
PboResult{evaluation=PboEvaluation{feasible=true, objective=25, violated_hard_rows=0, violated_fixed_variables=0}, x=[1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, ...] (size=25), iterations=0, elapsed_us=1.552}
stored=25
```

**標準エラー出力**

```text
PboEvaluation{feasible=true, objective=25, violated_hard_rows=0, violated_fixed_variables=0}
```

**読み方と注意点**

elapsed_usは毎回変わります。iterationsは探索していないので0です。標準出力と標準エラー出力は別の出力先なので、表示画面での両者の順番は保証されません。演算子は改行しないため、末尾に改行を付けています。

### 3.32 探索なしで初期候補を取り込み、固定値の補正を確認する

時間予算を使わずに、候補をsolverへ取り込んだ時点の状態を確認したい場合です。単純なevaluateと違い、solveは恒久固定を候補へ反映します。

**解く問題**

容量4の3案件について変数0を1固定にし、初期候補011を与えます。評価だけなら固定値違反、探索時間0のsolve後は111に補正されて資源のHard違反が残ることを確認します。

**入力形式**

n 上限、利益n個、工数n個、固定値n個、初期候補n個。

**入力例**

```text
3 4
10 7 6
3 2 2
1 -1 -1
0 1 1
```

**コード：`32_zero_budget.cpp`**

```cpp
#include "pbo_solver_v08.hpp" // PboSolverと入力・結果の型を読み込む

using namespace std;

int main() {
    int n;
    long long capacity;
    cin >> n >> capacity;

    PboProblem problem; // n候補の容量付き選択問題
    problem.profit.resize(n); // 各候補の利益を用意
    for (auto& profit : problem.profit) { // 利益を入力
        cin >> profit;
    }

    vector<PboTerm> usage; // 資源の使用量を表す
    for (int i = 0; i < n; ++i) {
        long long amount;
        cin >> amount;

        usage.push_back({i, amount}); // 変数iの使用量を係数へ入れる
    }

    // Hard上限を追加し、後で変更できる行IDを得る
    int capacity_row = problem.add_le(usage, capacity);
    (void)capacity_row;

    problem.fixed.resize(n); // 指定する固定値は必ずn個
    for (auto& f : problem.fixed) { // -1/0/1を数値で読む
        int v;
        cin >> v;
        f = int8_t(v);
    }

    vector<uint8_t> initial(n); // 初期候補の0/1配列。変数数と同じ長さが必要
    for (auto& bit : initial) { // 0/1を数値として読む
        int v;
        cin >> v;
        bit = uint8_t(v);
    }

    PboSolver solver(problem); // 固定値込みのモデルを構築
    auto raw = solver.evaluate(initial); // 配列を補正しない評価

    PboParam param; // 取り込みだけ行う条件
    param.time_limit_us = 0; // 時間0でも固定値反映・初期評価は行う。Hard修復の探索はしない

    // 呼出し元initialを変えず、内部候補を固定値へ補正する
    auto loaded = solver.solve(param, initial);

    cout << "raw=" << raw << '\n'; // 元の候補の評価
    cout << "loaded=" << loaded.evaluation << '\n'; // 補正後にもHard違反が残り得る
    cout << "iterations=" << loaded.iterations << " x="; // 0は探索量であり、経過時間0の意味ではない
    for (auto bit : loaded.x) { // 補正後の配列を確認
        cout << int(bit);
    }
    cout << '\n';

    if (!loaded.evaluation.feasible()) { // 実行可能と確認できるまで採用しない
        cout << "NOT_READY\n";
    }
}
```

**出力例（標準出力）**

```text
raw=PboEvaluation{feasible=false, objective=13, violated_hard_rows=0, violated_fixed_variables=1}
loaded=PboEvaluation{feasible=false, objective=23, violated_hard_rows=1, violated_fixed_variables=0}
iterations=0 x=111
NOT_READY
```

**読み方と注意点**

固定値を反映することと、Hard条件を修復することは別です。時間0では固定値の補正だけで実行可能解が得られるとは限りません。improveへ同じ固定値違反の初期候補を渡すことはできません。

### 3.33 検証とファイルの配置

GCC 13.3.0、C++20、最適化`-O2`で32本を独立にコンパイル・実行し、指定した警告オプションで警告がないことを確認しました。小さいモデルを解く19例は、別実装の全列挙で実行可能性・目的値・最良目的値を照合しています。状態更新などの11例は期待出力と完全照合し、時間を含む2例は制約・値の範囲・出力形式を確認しています。大きい問題で最適性を保証する検証ではありません。

Markdown処理後も数式の文字列が保持されることを確認しています。表示数式は各1行です。数式を表示するには、利用する外部リーダーの数式機能を有効にしてください。HTMLタグ・独自アンカーは使っていません。

同梱ZIPには、このMarkdown、対応ヘッダ、32本の`.cpp`、入力`.in`、確認時の出力`.out`、必要な標準エラー出力`.err`、検証スクリプトと結果を収録しています。時間を含む出力ファイルは、その実行時の一例です。

ZIPを展開したフォルダで1例を動かすには、次を実行します。

```sh
g++ -std=c++20 -O2 -I source examples/01_capacity.cpp -o example
./example < examples/01_capacity.in
```

全例を再検証するには、Python 3とGCCを用意して次を実行します。

```sh
python3 work/verify.py
```

入力規模を変える場合、例に置いた探索量上限や整数の範囲も見直します。目的値だけで判断せず、必須条件と固定値を満たしているかを確認してから、その候補を利用してください。
