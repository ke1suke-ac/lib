# SetPackingSolver サンプルコード集

対象ファイルは `set_packing_solver_v06.hpp` です。本書は、このヘッダを `#include` して利用するときの公開APIと、用途別の独立したプログラムを説明します。ほかのガイドや会話を読んでいることは前提にしません。

各サンプルは、コード全体を一つの `main.cpp` にコピーし、同じディレクトリへヘッダを置けば実行できます。標準入力を使う最後の例を除き、入力データはコード内に記載しています。複数の例を一つのファイルに連結する必要はありません。

GCCのC++20以降とGNUの標準ライブラリを使います。ヘッダが `bits/stdc++.h` を使うため、すべてのC++20処理系でそのまま使えるという意味ではありません。

```sh
g++ -std=c++20 -O2 -Wall -Wextra main.cpp -o main
./main
```

入力契約の違反を見つけやすくするため、例の確認時は `-DNDEBUG` を付けません。サンプルに含まれる `assert` も有効にして実行してください。ヘッダを単体でコンパイルするのではなく、上の `main.cpp` をコンパイルします。

## 1. 何を解くライブラリか？

### 1.1 候補の中から、両立できる組み合わせを選ぶ

例えば、利益8の作業A、利益7の作業B、利益6の作業Cがあるとします。AはBともCとも同時に実施できませんが、BとCは同時に実施できます。このとき、Aだけの利益8より、BとCを選ぶ利益13の方が有利です。

本ライブラリは、このような**利益付きの候補を、条件を守って選ぶ問題**を扱います。候補を選ぶか選ばないかの二択で、同じ候補を何個も選ぶことはありません。候補には、作業、商品の組、割当案、時間帯、配置案、事前に作った経路などを対応させられます。

候補そのものは呼び出し側で作ります。例えば「どの商品をどこに置くか」を扱うなら、候補を「商品Aを位置Pに置く案」「商品Aを位置Qに置く案」のように用意します。ライブラリが返すのは、採用した候補のIDです。

### 1.2 目的関数

候補数を $N$、候補のIDを $i$ とし、利益の合計を最大にします。

$$\max \sum_ {i=0}^{N-1} w_ {i} x_ {i}$$

| 記号 | 意味 | コードとの対応 |
|---|---|---|
| $N$ | 候補の数。0以上の整数 | `problem.weight.size()` |
| $i$ | 候補のID。0から $N-1$ まで | `weight[i]` の添字 |
| $w_ {i}$ | 候補 $i$ を選んだときの利益。負の値や0も指定可能 | `problem.weight[i]` |
| $x_ {i}$ | 候補 $i$ を選ぶなら1、選ばないなら0 | 結果の `selected` にIDが含まれるか |
| $\sum$ | 対象の項をすべて足す記号 | 選ばれた候補の利益の合計 |

選ばない候補では $x_ {i}=0$ なので利益は加わりません。選んだ候補だけで利益を足した値が `result.value` です。$N=0$ なら候補はなく、この合計は0です。

目的は選択個数の最大化ではありません。個数を最大にしたい場合は、全候補の利益を1にします。一般の入力について最適解であることを保証する機能はありません。

### 1.3 指定できる三種類の条件

**二つの候補の競合。** 同時に選べない候補の組を `conflicts` に登録します。競合する組全体を $E$ とすると、条件は次の形です。

$$x_ {i}+x_ {j}\le 1\qquad ((i,j)\in E)$$

$j$ も候補IDです。例えば `{0, 1}` は、候補0と候補1を同時に選ばないという指定です。

**資源の容量。** 資源数を $M$、資源IDを $r$、資源 $r$ の容量を $c_ {r}$ とします。候補 $i$ が使う資源の集合を $R_ {i}$ とすると、次の条件を守ります。

$$\sum_ {i:r\in R_ {i}} x_ {i}\le c_ {r}\qquad (r=0,\ldots,M-1)$$

| 記号 | 意味 | コードとの対応 |
|---|---|---|
| $M$ | 資源の数 | `problem.capacity.size()` |
| $r$ | 0から $M-1$ までの資源ID | `capacity[r]` の添字 |
| $c_ {r}$ | その資源を使える候補数の上限。0以上の整数 | `problem.capacity[r]` |
| $R_ {i}$ | 候補 $i$ が使う資源IDの集合 | `problem.resources[i]` |
| $r\in R_ {i}$ | 候補 $i$ が資源 $r$ を使う、という条件 | その行に資源IDが含まれること |

候補は、所属する各資源を**1単位ずつ**使います。容量1なら、その資源を共有する候補は高々一つしか選べません。容量2なら、高々二つまで選べます。候補IDと資源IDは別の番号体系です。

**必須・禁止。** `fixed[i]` によって、候補を必ず選ぶか、必ず除外するかを指定できます。

| `fixed[i]` | 条件 | 意味 |
|---|---|---|
| `-1` | 固定しない | ほかの条件と利益に従って選ぶかどうかを決める |
| `0` | $x_ {i}=0$ | 選んではいけない |
| `1` | $x_ {i}=1$ | 必ず選ぶ。利益が負でも必要 |

`fixed` を空にすると全候補が `-1` 扱いになります。**固定指定をしない場合に、全要素を0で埋めてはいけません。** それは全候補を禁止する指定です。

### 1.4 表現できる範囲を先に確認する

三種類の条件は組み合わせられます。重複要素を持たない集合の選択、利益付きのマッチング、同時利用数に上限がある予約、互いに重ならない配置案の選択などを表現できます。

一方、次はそのままでは表現できません。

| 条件 | 理由・利用時の考え方 |
|---|---|
| あるグループから必ず一つ選ぶ | 容量1は「高々一つ」。選ばないことも許される |
| 全要素を必ず覆う、全依頼を必ず処理する | 覆う数や選択数の下限を指定するAPIがない |
| 候補ごとに資源を2個、3個と消費する | 各候補の消費量は資源ごとに1。資源IDの重複記載は消費量の増加にならない |
| 二つを同時に選んだ場合だけ利益が増える | 利益は候補ごとの足し算であり、組み合わせ固有の利益を直接指定できない |
| 作業の順序、開始時刻、経路そのものを決める | 先に候補案を作り、案同士の競合や容量を表す必要がある |
| 必須条件に矛盾がある問題を無理に解く | 実行可能な組み合わせがない場合は `feasible=false` になる |

選択しないことが許されるため、必須候補がなければ空の選択も実行可能です。「利益が正だから、すべてのグループから一つずつ選ばれるはず」とは限りません。

## 2. APIリファレンス

### 2.1 公開型の一覧

通常のinclude利用で公開される、ライブラリ固有の型は以下です。`Solver` は説明用の別名で、`using Solver = SetPackingSolver<>;` と宣言したものとします。

| 型・別名 | 内容・既定値 |
|---|---|
| `SetPackingProblem<Weight = long long>` | 入力データの構造体 |
| `SetPackingResult<Weight = long long>` | 結果の構造体 |
| `SetPackingSolver<Weight = long long>` | 入力と探索状態を保持するソルバー。利益型は `long long` または `double` |
| `Solver::Problem` | そのソルバーと同じ利益型の `SetPackingProblem<Weight>` |
| `Solver::Result` | そのソルバーと同じ利益型の `SetPackingResult<Weight>` |
| `Solver::Clock` | `std::chrono::steady_clock`。絶対締切の時刻を作る時計 |
| `Solver::Options` | 1回の呼び出しの時間・反復上限 |
| `Solver::Update` | 利益、容量、必須・禁止指定の変更リスト |

ライブラリ固有の列挙型はありません。`fixed` の3値は列挙型ではなく `int8_t` の数値です。`SetPackingSolver<double>` を使うときも、ID・容量・反復上限は整数のままです。

### 2.2 `Problem`：入力の全フィールド

すべての配列の既定値は空です。候補数を別途渡す引数はありません。

| フィールド | 型 | 意味・入力条件 | 空の場合 |
|---|---|---|---|
| `weight` | `vector<Weight>` | 各候補の利益。長さが候補数。IDは0始まり | 候補なし |
| `conflicts` | `vector<pair<int, int>>` | 同時に選べない候補IDの組。両端は範囲内で異なるID | 個別の競合なし |
| `resources` | `vector<vector<int>>` | `resources[i]` は候補iが使う資源ID。全体を空にするか、行数を候補数と一致させる | 全候補がどの資源も使わない |
| `capacity` | `vector<int>` | 各資源の容量。長さが資源数。各値は0以上 | 資源なし |
| `fixed` | `vector<int8_t>` | 全体を空にするか、長さを候補数と一致させる。各要素は `-1`・`0`・`1` | 全候補が自由 |

競合の向きに意味はありません。`{a,b}` と `{b,a}` の両方を登録する必要はなく、重複した競合は正規化されます。同じ候補の `resources[i]` に同じ資源IDを重ねても一つにまとめられます。例えば `{0,0}` は資源0を2単位使う指定ではありません。

一部の候補だけ資源を使うときは、`resources.resize(N)` で全候補の行を用意し、使わない候補の行を空にします。容量0の資源を使う候補は選べません。

### 2.3 サイズ・数値・IDの共通契約

| 対象 | 守る条件 |
|---|---|
| 候補数 | 0以上、10,000,000以下。実際に扱える規模はメモリと時間に依存する |
| 資源数 | `int` に収まる。配列を確保できる必要もある |
| 内部の接続要素数 | 重複除去前でも `int` に収まる。競合は両向きに格納されるので、登録ペア数の2倍もこの制限内 |
| 候補ID | 0以上、`weight.size()` 未満 |
| 資源ID | 0以上、`capacity.size()` 未満 |
| `long long` の利益 | 全候補の利益の絶対値の合計を `LLONG_MAX / 4` 以下にする |
| `double` の利益 | 各値と、全候補の絶対値の合計が有限。NaN・正負の無限大は不可 |
| 利益の符号 | 負や0も有効。ただし自由な候補のうち利益が0以下のものは追加対象にならない。必須なら負でも選択する |
| `span` に渡す候補ID列 | 範囲内で、IDに重複がないこと。順序は実行可能性には影響しないが、初期案を修復する際の採用順に影響し得る |

これらは呼び出し側が守る前提です。入力を自動修正する、例外を返す、あるいは不正なIDを `false` として扱うAPIではありません。多くの違反はassertで検出されますが、すべての上限を実行時に検査するわけではありません。

### 2.4 `Options`：停止条件の全フィールド

| フィールド | 型 | 既定値 | 指定できる値・意味 |
|---|---|---|---|
| `time_limit_us` | `double` | `1'950'000.0` | 呼び出し開始からの相対時間。単位はマイクロ秒。`-1` で無効、または0以上 `1e12` 以下の有限値 |
| `deadline` | `optional<Solver::Clock::time_point>` | `nullopt` | 全体で共有したい絶対締切。`nullopt` なら絶対締切なし。過去の時刻も指定可能 |
| `iteration_limit` | `long long` | `-1` | 今回の探索反復の上限。`-1` で無効、または0以上の整数 |

有効な停止条件のうち、先に到達したものが使われます。相対時間と絶対締切を両方指定すれば、早い方が時間の上限になります。三つをすべて無効にしてはいけません。

よく使う指定は次のとおりです。

| 目的 | 指定 |
|---|---|
| 既定の約1.95秒で呼び出す | `solver.solve({})` |
| 時間に依存しない1000反復 | `time_limit_us=-1`、`deadline=nullopt`、`iteration_limit=1000` |
| 呼び出しごとに10ミリ秒 | `time_limit_us=10'000.0`、`deadline=nullopt`、`iteration_limit=-1` |
| 外側の共通締切のみ使う | `time_limit_us=-1`、`deadline=stop`、`iteration_limit=-1` |
| 新しい探索をせず、準備や修復だけ行う | `time_limit_us=-1`、`iteration_limit=0` |

**反復上限0では、初回の候補追加も行いません。** 必須候補がない新しいソルバーで `solve` を呼べば、空の実行可能解になり得ます。`improve` に渡した案の修復や、更新後の準備は行います。

相対時間0や過去の締切でも、初期化・必須条件の検査・必要な結果の組み立ては省略されません。時間上限は厳密な中断ではなく、実行中の処理や返却用コピーなどの分だけ超えることがあります。呼び出しのたびに相対時間は計り直され、反復上限も今回分として解釈されます。コンストラクタ、呼び出し側の入力変換、出力時間は相対時間の外です。

### 2.5 `Result`：戻り値の全フィールド

| フィールド | 型 | 既定値 | 意味 |
|---|---|---|---|
| `feasible` | `bool` | `false` | 条件を満たす選択を返せているか。最適性の証明を意味しない |
| `value` | `Weight` | 0 | 選択した候補の利益の合計。必須候補の利益も含む |
| `selected` | `vector<int>` | 空 | 元の入力における選択候補ID。重複なし。昇順である保証はない |
| `elapsed_us` | `double` | 0 | 今回の呼び出しの経過時間。単位はマイクロ秒 |
| `iterations` | `long long` | 0 | 今回実施した探索反復数。累計ではない |

`feasible` を確認してから `value` と `selected` を解として利用します。`feasible=true` で `selected` が空という結果は有効です。全利益が負で必須候補がない場合などに自然に生じます。

正しい形式の入力では、必須候補同士の競合または必須候補による容量超過があれば実行可能解はありません。自由な候補をすべて除外しても矛盾が残るためです。この場合に `feasible=false` となります。

`iterations` は、選択が変わらなかった試行も含みます。初期化などは反復数に含まず、選択対象がなくなると上限より早く終わることがあります。`elapsed_us` は `solve` と `improve` では返却用コピーの処理まで、`solve_view` では内部結果を返す準備までを含みます。出力にかかった時間は含みません。

### 2.6 構築と状態管理

| API | 引数・既定値 | 戻り値・動作 |
|---|---|---|
| `explicit SetPackingSolver(const Problem& problem, uint64_t initial_seed = 1)` | `problem` は必須。`initial_seed` は乱数の初期値で、`uint64_t` の全範囲を指定可能。既定値1 | ソルバーを構築し、入力を内部に保持する。構築だけでは解を返さない |
| `void rebuild(const Problem& problem)` | `problem` は新しい入力。省略不可 | 問題全体を置き換え、保存解と探索状態を破棄する。乱数の状態は維持する |
| `void seed(uint64_t value)` | `value` は新しい乱数状態。省略不可。0も使用可能 | 乱数状態だけを変更する。保存解や探索状態を最初からやり直す操作ではない |

構築後に元の `problem` を変更しても、ソルバーへは反映されません。数値だけの変更には `update`、候補数・競合・資源所属の変更には `rebuild` を使います。IDの意味が変わる場合、手元に保存した古い選択IDを新しい問題のIDとして使わないでください。

ソルバーには通常のコピー・ムーブも使えます。例えば `Solver branch = original;` は、入力だけでなく保存解・探索状態・乱数状態も複製します。別々の条件を試す際に便利ですが、大きな問題ではコピーの時間とメモリが必要です。ムーブ元をそのまま探索に再利用する使い方は避け、必要なら新しく構築します。`Problem`・`Result`・`Options`・`Update` も通常の値としてコピーできます。

### 2.7 解を求める三つのメソッド

| API | 引数 | 戻り値 |
|---|---|---|
| `Result solve(const Options& options)` | `options` は必須。既定設定を使うなら `{}` を渡す | 呼び出し側が所有する結果。ソルバーの次の操作後も保持できる |
| `const Result& solve_view(const Options& options)` | `solve` と同じ。省略不可 | ソルバーが内部で所有する結果へのconst参照 |
| `Result improve(span<const int> initial_selected, const Options& options)` | `initial_selected` は候補ID列。空も可能。範囲内・重複なし。`options` も必須 | 初期案を必要なら修復し、その後改善した、呼び出し側が所有する結果 |

`solve` と `solve_view` は、同じ問題への繰り返し呼び出しで探索を継続します。毎回ゼロからの独立試行をするには、新しいソルバーを作ります。`solve()` のように引数を完全に省略するオーバーロードはありません。

`solve_view` の参照は、**次の非const操作までに使い終える**ものとして扱います。次の `solve`、`solve_view`、`improve`、`update`、`rebuild`、`seed` などの後に、以前の結果を保持できると考えてはいけません。ソルバーの破棄後も使用できません。

| 受け取り方 | 結果の所有者・使い方 |
|---|---|
| `auto result = solver.solve(options);` | 呼び出し側。独立した結果を保存する |
| `const auto& result = solver.solve_view(options);` | ソルバー。コピーせず、その場で読む |
| `Solver::Result saved = solver.solve_view(options);` | 呼び出し側。参照先をコピーして保存する |
| `auto result = solver.solve_view(options);` | `&` がないためコピーになる |

`improve` は、渡した候補列から選択状態を準備し直します。同じ問題で既存の最良解があれば、それも比較対象として保持します。入力契約を守り、初期案が現在の条件を満たすなら、その利益より悪くならない結果を返します。初期案に競合・容量超過・禁止候補がある場合は修復するため、初期案の単純な利益合計を維持する保証はありません。初期案のIDは改変されません。

`span<const int>` には `vector<int>` や `array<int, K>` を渡せます。要素をコピーせずに引数として参照する型で、呼び出し中はそのID列が有効である必要があります。中括弧だけの `improve({1,2}, options)` に頼らず、サンプルのようにID列を用意してください。

### 2.8 選択の検査と利益計算

| API | 引数・前提 | 戻り値・注意点 |
|---|---|---|
| `Weight evaluate(span<const int> selected) const` | 範囲内・重複なしの候補ID列。省略不可 | 現在の利益の合計。競合・容量・必須・禁止の検査はしない |
| `bool is_feasible(span<const int> selected) const` | 範囲内・重複なしの候補ID列。省略不可 | 全条件を満たせば `true`、条件違反なら `false` |

空のID列も渡せます。ただし必須候補があれば、その候補を含まない空集合は実行不可能です。`evaluate` が大きな値を返すことと、実行可能であることは別です。不正なIDやIDの重複を検査するために `is_feasible` を使うのではなく、それらは先に呼び出し側で検査してください。

### 2.9 `Update` と `update`：数値や固定指定を変える

`void update(const Update& changes)` の引数 `changes` は必須で、戻り値はありません。`Update` の全フィールドは次のとおりで、既定値はすべて空です。

| フィールド | 型 | 一つの要素の意味 |
|---|---|---|
| `weights` | `vector<pair<int, Weight>>` | `{候補ID, 新しい利益}` |
| `capacities` | `vector<pair<int, int>>` | `{資源ID, 新しい容量}` |
| `fixed` | `vector<pair<int, int8_t>>` | `{候補ID, 新しい固定値}`。`-1`・`0`・`1` |

新しい値は差分ではなく**置き換える値**です。各フィールド内では同じIDを二度指定してはいけません。`weights` と `fixed` の両方で同じ候補IDを指定することは可能です。指定しないIDの値は変わりません。新しい値にも、`Problem` と同じ数値・IDの契約が適用されます。

`update` 自体は探索しません。次の `solve`・`solve_view`・`improve` で必要な準備や再評価を行います。`evaluate` と `is_feasible` は更新された値・条件に従います。手元にコピーした過去の `Result` の数値が自動で変わるわけではありません。

容量更新には、必須候補の使用分を引く前の**資源全体の容量**を渡します。必須候補を自由へ戻す値は `-1`、必須から禁止へ変える値は `0` です。入力に保持していた `Problem` も自動更新されないので、後でそれを `rebuild` に使うなら呼び出し側でも変更を反映します。

### 2.10 結果のデバッグ出力

`ostream& operator<<(ostream& os, const SetPackingResult& result)` により、`cout`・`cerr`・`ostringstream` へ結果を直接渡せます。`os` と `result` は省略不可で、戻り値は渡したストリームへの参照です。

`std::cerr << result << '\n';` は、次の形式で全フィールドを1行に表示します。

```text
SetPackingResult{feasible=true, value=13, selected=[1, 2], elapsed_us=123.5, iterations=1000}
```

`selected` は格納順の先頭20要素までです。超過時は `... (25 total)` のように総件数を表示します。空なら `selected=[]` です。出力によって元の配列が短くなることはありません。改行は追加しないため、呼び出し側で付けます。

数値はストリームの書式設定に従います。演算子自身は `fixed`・精度・`boolalpha` を変更しません。`feasible` は常に `true` または `false` です。結果の表示形式はデバッグ向けで、全選択IDを保存・提出するための形式ではありません。

### 2.11 操作の選び方と費用の目安

| やりたいこと | 使うAPI | 費用上の注意 |
|---|---|---|
| 初めて解く、探索を継続する | `solve` | 結果の選択ID列を返却用にコピーする |
| 結果をその場で読む | `solve_view` | 返却用コピーを省く。探索や必要な内部結果の準備は行う |
| 外部の案から改善する | `improve` | 案の修復・準備と結果のコピーがある |
| 同じ候補と接続を保って値を変える | `update` | 通常は指定件数に応じた変更。次の探索で別途準備が必要になり得る |
| 候補や接続を変える | `rebuild` | 問題全体の再構築が必要 |
| 利益だけ確認する | `evaluate` | 基本は選択数に比例。assert有効時は候補数に応じた重複検査用領域も使う |
| 全条件を検査する | `is_feasible` | 候補数・資源数と、選択候補の競合や資源所属を調べる費用がある |
| ログを短く出す | `operator<<` | 選択IDの表示は高々20件。出力先のI/O時間は別途かかる |

## 3. ユースケース

それぞれの節で、具体的な入力、完全なコード、出力例を示します。構造が同じでも単に名前を変えただけの例は増やさず、入力の作り方やAPIの使い方が異なる用途を選んでいます。

ほとんどの例では、学習中の再現性のために相対時間を無効にし、反復数を指定します。時間制限を使う例の結果や `elapsed_us` は環境に依存します。表示用にIDを昇順にする例がありますが、ライブラリの返却順に意味はありません。

| 節 | 用途 | 主なAPI・入力の工夫 |
|---|---|---|
| 3.1 | 両立できない作業から、利益の大きい組み合わせを選ぶ | `Problem::weight / conflicts、構築、Options、solve、is_feasible、evaluate` |
| 3.2 | 同じ品物を二度使わないセット商品を選ぶ | `Problem::resources / capacity、solve` |
| 3.3 | 同じ時間帯に二つまで受け入れる予約 | `容量が2以上のProblem::capacity、solve` |
| 3.4 | 設備の取り合いと、個別の禁止組を同時に守る | `Problem::resources / capacity / conflictsの併用` |
| 3.5 | 担当者と仕事を重複なく組み合わせる | `入力の候補化、資源IDの区分、solve` |
| 3.6 | 複数の実施案から、同じ企画は一案まで選ぶ | `グループの排他制約と複数資源の組み合わせ` |
| 3.7 | 時刻の重なる講演を避けて受講計画を作る | `実データからProblem::conflictsを生成` |
| 3.8 | 重ならない長方形の配置案を選ぶ | `幾何条件からの競合生成、同一品物の排他資源` |
| 3.9 | 材料を一つずつ使う製造注文を、在庫内で選ぶ | `異なる資源容量、単位消費のモデル化` |
| 3.10 | 総件数と、重なり合うカテゴリの上限を守る | `全候補共通資源、複数グループの上限制約` |
| 3.11 | 事前に作った経路案を、辺の容量内で採用する | `辺容量と依頼ごとの排他制約、候補の複合資源` |
| 3.12 | 確定済みの作業と、採用できない作業を指定する | `Problem::fixedの-1・0・1、負の必須利益` |
| 3.13 | 実行不可能と、何も選ばない実行可能解を区別する | `Result::feasible / selected / value、空のProblem` |
| 3.14 | 期待利益など、小数の評価値で選ぶ | `SetPackingSolver<double>、Problem<double> / Result<double>に対応する別名` |
| 3.15 | 別の方法で作った選択案の、利益と条件違反を調べる | `evaluate、is_feasible、vector / arrayからspanへの受け渡し` |
| 3.16 | 手元の実行可能解を出発点にして改善する | `improve、is_feasible、evaluate` |
| 3.17 | 条件に違反した提案を、探索をせずに修復する | `improve、Options::iteration_limit=0` |
| 3.18 | 探索を分割し、その場で読む結果と保存する結果を使い分ける | `solve、solve_view、Resultのコピー、Result::iterations` |
| 3.19 | 利益が変わるたびに、同じ候補から選び直す | `Update::weights、update、更新後のsolve` |
| 3.20 | 容量変更と、必須・禁止・自由への切替を行う | `Update::capacities / fixed、update、固定解除の-1` |
| 3.21 | 候補の追加や、競合関係の変更に合わせて作り直す | `rebuild、seed、元のProblemとの所有関係` |
| 3.22 | 外側の選択を固定し、一部の候補だけ解き直す | `Problem::fixedで部分問題化、improve、全体のvalue / selected` |
| 3.23 | 異なるseedで独立に試し、最も良い結果を残す | `seed、独立した構築、Resultの既定値とコピー` |
| 3.24 | 呼び出しごとの時間と、全体で共通の締切を併用する | `Clock、Options::deadline / time_limit_us、solveの継続` |
| 3.25 | 大きな結果を、短いデバッグログに出す | `Resultのoperator<<、solve_view、selected.size()` |
| 3.26 | 同じ状態から、別の将来条件を比較する | `Solverのコピー、Update::weights、結果の独立性` |
| 3.27 | 標準入力から一般形の問題を読み、選択IDを出力する | `Problemの全入力フィールド、構築のinitial_seed、Options、solve` |

### 3.1. 両立できない作業から、利益の大きい組み合わせを選ぶ

干渉する作業や、同時掲載できない広告など、二つの候補の関係をそのまま書ける用途です。最初の例では資源を使わず、競合だけを登録します。

**具体的な問題・入力**

候補0・1・2の利益は8・7・6。候補0は1とも2とも競合します。1と2は両立できるので、両方を選ぶと利益13です。固定指定はありません。

**完全なコード：`01_conflicts.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 候補の利益と制約を入れる。未指定の配列は空。
    Solver::Problem problem;
    // ID0・1・2の利益を、この順で入力する。
    problem.weight = {8, 7, 6};
    // 各組の両端は同時に選べない。逆向きの登録は不要。
    problem.conflicts = {{0, 1}, {0, 2}};

    // fixedを空のままにすると全候補が自由。0で埋めると全候補を禁止してしまう。

    // 入力を保持するソルバーを、乱数seed=1で構築する。
    Solver solver(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 条件を満たす候補集合を求め、結果をコピーで受け取る。
    auto result = solver.solve(options);

    // 必須条件などに矛盾があれば、利益やIDを解として使わない。
    if (!result.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 返却されたID列が現在の全条件を満たすか確認する。
    assert(solver.is_feasible(result.selected));
    // 選択IDから計算した利益と結果を照合する。
    assert(solver.evaluate(result.selected) == result.value);

    // 所有する結果コピーの表示順だけを整える。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益を表示する。
    std::cout << "value=" << result.value << "\nselected:";
    // 選択された元の候補IDをすべて出力する。
    for (int id : result.selected) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

```

**標準出力の例**

```text
value=13
selected: 1 2
```

### 3.2. 同じ品物を二度使わないセット商品を選ぶ

一つの候補が複数の品物をまとめて使う、集合パッキングの用途です。共有する品物ごとに資源を一つ作ると、候補間の競合をすべて列挙せずに表せます。

**具体的な問題・入力**

品物0〜3を各1個持っています。候補セットは順に {0,1}、{0}、{1}、{2,3}、{3}、利益は9・6・5・8・4です。同じ品物を共有するセットは両立しません。

**完全なコード：`02_disjoint_sets.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 候補の利益と制約を入れる。未指定の配列は空。
    Solver::Problem problem;
    // 五つのセット商品の利益。
    problem.weight = {9, 6, 5, 8, 4};
    // 行iにセットiが使う品物IDを列挙する。
    problem.resources = {
        {0, 1},
        {0},
        {1},
        {2, 3},
        {3}
    };
    // 各品物は1個だけなので、使えるセット数も1。
    problem.capacity = {1, 1, 1, 1};

    // 同じ行に品物IDを二度書いても2個消費にはならない。ここでは各品物を1個ずつ使う。

    // 入力を保持するソルバーを、乱数seed=1で構築する。
    Solver solver(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 条件を満たす候補集合を求め、結果をコピーで受け取る。
    auto result = solver.solve(options);

    // 必須条件などに矛盾があれば、利益やIDを解として使わない。
    if (!result.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 返却されたID列が現在の全条件を満たすか確認する。
    assert(solver.is_feasible(result.selected));
    // 選択IDから計算した利益と結果を照合する。
    assert(solver.evaluate(result.selected) == result.value);

    // 所有する結果コピーの表示順だけを整える。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益を表示する。
    std::cout << "value=" << result.value << "\nselected:";
    // 選択された元の候補IDをすべて出力する。
    for (int id : result.selected) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

```

**標準出力の例**

```text
value=19
selected: 1 2 3
```

### 3.3. 同じ時間帯に二つまで受け入れる予約

複数台の同型設備があり、同時に二つまで予約を受け付けられる場合です。時間帯を資源にし、その時間帯に使える台数を容量にします。

**具体的な問題・入力**

時間帯0・1にはそれぞれ設備が2台あります。予約0と3は両時間帯、予約1は時間帯0、予約2は時間帯1を使います。利益は12・8・7・10です。各予約の設備需要は各時間帯で1台です。

**完全なコード：`03_parallel_slots.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 候補の利益と制約を入れる。未指定の配列は空。
    Solver::Problem problem;
    // 四つの予約を受けたときの利益。
    problem.weight = {12, 8, 7, 10};
    // 各予約が占有する時間帯。
    problem.resources = {
        {0, 1},
        {0},
        {1},
        {0, 1}
    };
    // 各時間帯で受け付ける予約を高々2件にする。
    problem.capacity = {2, 2};

    // 設備に個別の種類や相性がある場合は、その条件も候補・資源として別途表す。

    // 入力を保持するソルバーを、乱数seed=1で構築する。
    Solver solver(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 条件を満たす候補集合を求め、結果をコピーで受け取る。
    auto result = solver.solve(options);

    // 必須条件などに矛盾があれば、利益やIDを解として使わない。
    if (!result.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 返却されたID列が現在の全条件を満たすか確認する。
    assert(solver.is_feasible(result.selected));
    // 選択IDから計算した利益と結果を照合する。
    assert(solver.evaluate(result.selected) == result.value);

    // 所有する結果コピーの表示順だけを整える。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益を表示する。
    std::cout << "value=" << result.value << "\nselected:";
    // 選択された元の候補IDをすべて出力する。
    for (int id : result.selected) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

```

**標準出力の例**

```text
value=27
selected: 0 1 2
```

### 3.4. 設備の取り合いと、個別の禁止組を同時に守る

資源だけでは表せない個別の相性を、競合ペアで補う用途です。設備を共有していなくても、運用上同時に採用できない作業を登録できます。

**具体的な問題・入力**

候補0は設備0・1、候補1は設備0、候補2は設備1、候補3は設備2を1台ずつ使います。各設備は1台で、候補1と3には別途競合があります。利益は10・9・8・6です。

**完全なコード：`04_mixed_constraints.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 候補の利益と制約を入れる。未指定の配列は空。
    Solver::Problem problem;
    // 四つの作業の利益。
    problem.weight = {10, 9, 8, 6};
    // 作業ごとの設備使用。
    problem.resources = {
        {0, 1},
        {0},
        {1},
        {2}
    };
    // 各設備を使える作業は一つまで。
    problem.capacity = {1, 1, 1};
    // 資源を共有しなくても、この二作業は同時に実施しない。
    problem.conflicts = {{1, 3}};

    // 入力を保持するソルバーを、乱数seed=1で構築する。
    Solver solver(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 条件を満たす候補集合を求め、結果をコピーで受け取る。
    auto result = solver.solve(options);

    // 必須条件などに矛盾があれば、利益やIDを解として使わない。
    if (!result.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 返却されたID列が現在の全条件を満たすか確認する。
    assert(solver.is_feasible(result.selected));
    // 選択IDから計算した利益と結果を照合する。
    assert(solver.evaluate(result.selected) == result.value);

    // 所有する結果コピーの表示順だけを整える。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益を表示する。
    std::cout << "value=" << result.value << "\nselected:";
    // 選択された元の候補IDをすべて出力する。
    for (int id : result.selected) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

```

**標準出力の例**

```text
value=17
selected: 1 2
```

### 3.5. 担当者と仕事を重複なく組み合わせる

候補を「担当者と仕事の組」にする、利益付きの割当です。担当者の重複と仕事の重複を、別々の資源で禁止します。全員への割当は強制せず、採用する割当だけを選びます。

**具体的な問題・入力**

担当者0・1と仕事0・1があり、四つの割当案の利益は (0,0):8、(0,1):7、(1,0):6、(1,1):2 です。各担当者と各仕事は高々一つの割当で使います。

**完全なコード：`05_assignment.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 候補の利益と制約を入れる。未指定の配列は空。
    Solver::Problem problem;

    struct Assignment {
        int worker, job;
        long long profit;
    };

    const std::vector<Assignment> plans = {
        {0, 0, 8},
        {0, 1, 7},
        {1, 0, 6},
        {1, 1, 2}
    };

    // 資源0・1は担当者、2・3は仕事。どれも一度まで。
    problem.capacity.assign(4, 1);
    for (const auto& plan : plans) {
        // 一つの割当案を一つの候補にする。
        problem.weight.push_back(plan.profit);
        // 担当者と仕事を別の番号範囲へ変換する。
        problem.resources.push_back({plan.worker, 2 + plan.job});
    }

    // 容量1は「高々一つ」。全員・全仕事への割当を保証する指定ではない。

    // 入力を保持するソルバーを、乱数seed=1で構築する。
    Solver solver(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 条件を満たす候補集合を求め、結果をコピーで受け取る。
    auto result = solver.solve(options);

    // 必須条件などに矛盾があれば、利益やIDを解として使わない。
    if (!result.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 返却されたID列が現在の全条件を満たすか確認する。
    assert(solver.is_feasible(result.selected));
    // 選択IDから計算した利益と結果を照合する。
    assert(solver.evaluate(result.selected) == result.value);

    // 所有する結果コピーの表示順だけを整える。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益を表示する。
    std::cout << "value=" << result.value << "\nselected:";
    // 選択された元の候補IDをすべて出力する。
    for (int id : result.selected) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

```

**標準出力の例**

```text
value=13
selected: 1 2
```

### 3.6. 複数の実施案から、同じ企画は一案まで選ぶ

同じ企画に「設備を多く使う高利益案」と「別設備を使う案」がある場合です。企画ごとの資源と設備の資源を併用すると、二案を同時採用せず、他企画との取り合いも表せます。

**具体的な問題・入力**

企画0の案0は設備0・1を使って利益12、案1は設備2を使って利益7。企画1の案2は設備0で利益8、企画2の案3は設備1で利益6です。設備は各1台で、同じ企画は一案までです。

**完全なコード：`06_alternative_plans.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 候補の利益と制約を入れる。未指定の配列は空。
    Solver::Problem problem;

    struct Plan {
        int project;
        std::vector<int> machines;
        long long profit;
    };

    const std::vector<Plan> plans = {
        {0, {0, 1}, 12},
        {0, {2}, 7},
        {1, {0}, 8},
        {2, {1}, 6}
    };

    // 資源0〜2は企画の選択枠、3〜5は設備。
    problem.capacity.assign(6, 1);
    for (const auto& plan : plans) {
        // 案を候補として登録する。
        problem.weight.push_back(plan.profit);

        std::vector<int> used{plan.project};
        for (int machine : plan.machines) {
            used.push_back(3 + machine);
        }

        // 同企画の案は共通の選択枠を使い、設備も占有する。
        problem.resources.push_back(used);
    }

    // 企画を見送ることも許される。「各企画から必ず一案」にはならない。

    // 入力を保持するソルバーを、乱数seed=1で構築する。
    Solver solver(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 条件を満たす候補集合を求め、結果をコピーで受け取る。
    auto result = solver.solve(options);

    // 必須条件などに矛盾があれば、利益やIDを解として使わない。
    if (!result.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 返却されたID列が現在の全条件を満たすか確認する。
    assert(solver.is_feasible(result.selected));
    // 選択IDから計算した利益と結果を照合する。
    assert(solver.evaluate(result.selected) == result.value);

    // 所有する結果コピーの表示順だけを整える。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益を表示する。
    std::cout << "value=" << result.value << "\nselected:";
    // 選択された元の候補IDをすべて出力する。
    for (int id : result.selected) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

```

**標準出力の例**

```text
value=21
selected: 1 2 3
```

### 3.7. 時刻の重なる講演を避けて受講計画を作る

候補に開始・終了時刻がある場合は、時刻の重なりから競合ペアを作れます。ここでは一人が同時に二つの講演を受けられない条件を表します。

**具体的な問題・入力**

講演の (開始,終了,利益) は (0,4,9)、(0,2,6)、(2,4,6)、(4,6,5)、(1,5,13)。区間は開始を含み終了を含まないので、終了時刻と次の開始時刻が等しければ連続して参加できます。

**完全なコード：`07_intervals.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 候補の利益と制約を入れる。未指定の配列は空。
    Solver::Problem problem;

    struct Talk {
        int start, end;
        long long profit;
    };

    const std::vector<Talk> talks = {
        {0, 4, 9},
        {0, 2, 6},
        {2, 4, 6},
        {4, 6, 5},
        {1, 5, 13}
    };

    // 講演を候補へ変換する。
    for (const auto& talk : talks) {
        problem.weight.push_back(talk.profit);
    }

    for (int i = 0; i < static_cast<int>(talks.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(talks.size()); ++j) {
            if (std::max(talks[i].start, talks[j].start) <
                std::min(talks[i].end, talks[j].end)) {
                // 正の長さで重なる講演だけを競合にする。
                problem.conflicts.emplace_back(i, j);
            }
        }
    }

    // この競合生成は候補数の二乗の比較をする。大量の講演には入力生成側の工夫が必要。

    // 入力を保持するソルバーを、乱数seed=1で構築する。
    Solver solver(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 条件を満たす候補集合を求め、結果をコピーで受け取る。
    auto result = solver.solve(options);

    // 必須条件などに矛盾があれば、利益やIDを解として使わない。
    if (!result.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 返却されたID列が現在の全条件を満たすか確認する。
    assert(solver.is_feasible(result.selected));
    // 選択IDから計算した利益と結果を照合する。
    assert(solver.evaluate(result.selected) == result.value);

    // 所有する結果コピーの表示順だけを整える。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益を表示する。
    std::cout << "value=" << result.value << "\nselected:";
    // 選択された元の候補IDをすべて出力する。
    for (int id : result.selected) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

```

**標準出力の例**

```text
value=17
selected: 1 2 3
```

### 3.8. 重ならない長方形の配置案を選ぶ

座標や向きが決まった配置案を先に用意し、重なる案を競合させます。同じ品物に複数の配置案がある場合は、品物ごとの資源で二重採用も防ぎます。

**具体的な問題・入力**

品物0には領域 [0,2)×[0,2) の案0（利益8）と [2,4)×[0,2) の案1（利益7）があります。品物1の案2は [0,2)×[0,2)（利益6）、品物2の案3は [0,2)×[2,4)（利益5）です。辺の接触は許します。

**完全なコード：`08_rectangles.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 候補の利益と制約を入れる。未指定の配列は空。
    Solver::Problem problem;

    struct Placement {
        int item, x1, y1, x2, y2;
        long long profit;
    };

    const std::vector<Placement> plans = {
        {0, 0, 0, 2, 2, 8},
        {0, 2, 0, 4, 2, 7},
        {1, 0, 0, 2, 2, 6},
        {2, 0, 2, 2, 4, 5}
    };

    // 品物ごとに配置案を一つまで選べる。
    problem.capacity.assign(3, 1);
    for (const auto& plan : plans) {
        // この配置案を採用する利益。
        problem.weight.push_back(plan.profit);
        // 同じ品物の配置案は同じ資源を使う。
        problem.resources.push_back({plan.item});
    }

    for (int i = 0; i < static_cast<int>(plans.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(plans.size()); ++j) {
            const auto& a = plans[i];
            const auto& b = plans[j];

            if (std::max(a.x1, b.x1) < std::min(a.x2, b.x2) &&
                std::max(a.y1, b.y1) < std::min(a.y2, b.y2)) {
                // 面積を持って重なる案を同時採用しない。
                problem.conflicts.emplace_back(i, j);
            }
        }
    }

    // ライブラリは座標を動かさない。置ける候補位置は呼び出し側が用意する。

    // 入力を保持するソルバーを、乱数seed=1で構築する。
    Solver solver(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 条件を満たす候補集合を求め、結果をコピーで受け取る。
    auto result = solver.solve(options);

    // 必須条件などに矛盾があれば、利益やIDを解として使わない。
    if (!result.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 返却されたID列が現在の全条件を満たすか確認する。
    assert(solver.is_feasible(result.selected));
    // 選択IDから計算した利益と結果を照合する。
    assert(solver.evaluate(result.selected) == result.value);

    // 所有する結果コピーの表示順だけを整える。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益を表示する。
    std::cout << "value=" << result.value << "\nselected:";
    // 選択された元の候補IDをすべて出力する。
    for (int id : result.selected) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

```

**標準出力の例**

```text
value=18
selected: 1 2 3
```

### 3.9. 材料を一つずつ使う製造注文を、在庫内で選ぶ

種類ごとの在庫を容量にする用途です。一つの注文が使う各材料を1個ずつ消費する場合に、そのままモデル化できます。

**具体的な問題・入力**

小麦粉0が2個、卵1が1個、牛乳2が1個あります。注文0は {0,1} で利益7、注文1は {0,2} で利益6、注文2は {1,2} で利益8、注文3は {0} で利益4です。各注文は一回までです。

**完全なコード：`09_inventory.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 候補の利益と制約を入れる。未指定の配列は空。
    Solver::Problem problem;
    // 注文を一件受けるときの利益。
    problem.weight = {7, 6, 8, 4};
    // 注文ごとに1個ずつ使う材料。
    problem.resources = {
        {0, 1},
        {0, 2},
        {1, 2},
        {0}
    };
    // 材料0・1・2の在庫数。
    problem.capacity = {2, 1, 1};

    // 小麦粉2個を必要とする一注文は、この行に0を二度書いても表現できない。

    // 入力を保持するソルバーを、乱数seed=1で構築する。
    Solver solver(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 条件を満たす候補集合を求め、結果をコピーで受け取る。
    auto result = solver.solve(options);

    // 必須条件などに矛盾があれば、利益やIDを解として使わない。
    if (!result.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 返却されたID列が現在の全条件を満たすか確認する。
    assert(solver.is_feasible(result.selected));
    // 選択IDから計算した利益と結果を照合する。
    assert(solver.evaluate(result.selected) == result.value);

    // 所有する結果コピーの表示順だけを整える。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益を表示する。
    std::cout << "value=" << result.value << "\nselected:";
    // 選択された元の候補IDをすべて出力する。
    for (int id : result.selected) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

```

**標準出力の例**

```text
value=13
selected: 0 1
```

### 3.10. 総件数と、重なり合うカテゴリの上限を守る

全体で三件まで、カテゴリAは二件まで、カテゴリBは一件まで、という複数の枠を同時に守る用途です。一つの候補が複数カテゴリに属していても扱えます。

**具体的な問題・入力**

五つの施策の利益は10・9・8・7・6。候補0・1・4はカテゴリA、候補1・2はカテゴリBに属します。候補1は両方の枠を使います。全体は三件、Aは二件、Bは一件まで選びます。

**完全なコード：`10_overlapping_quotas.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 候補の利益と制約を入れる。未指定の配列は空。
    Solver::Problem problem;
    // 五つの施策の利益。
    problem.weight = {10, 9, 8, 7, 6};
    // 資源0は全体、1はA、2はB。
    problem.resources = {
        {0, 1},
        {0, 1, 2},
        {0, 2},
        {0},
        {0, 1}
    };
    // 全体3件、A2件、B1件まで。
    problem.capacity = {3, 2, 1};

    // どれも上限。各カテゴリから最低一件選ぶ、という条件は指定していない。

    // 入力を保持するソルバーを、乱数seed=1で構築する。
    Solver solver(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 条件を満たす候補集合を求め、結果をコピーで受け取る。
    auto result = solver.solve(options);

    // 必須条件などに矛盾があれば、利益やIDを解として使わない。
    if (!result.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 返却されたID列が現在の全条件を満たすか確認する。
    assert(solver.is_feasible(result.selected));
    // 選択IDから計算した利益と結果を照合する。
    assert(solver.evaluate(result.selected) == result.value);

    // 所有する結果コピーの表示順だけを整える。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益を表示する。
    std::cout << "value=" << result.value << "\nselected:";
    // 選択された元の候補IDをすべて出力する。
    for (int id : result.selected) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

```

**標準出力の例**

```text
value=26
selected: 0 1 3
```

### 3.11. 事前に作った経路案を、辺の容量内で採用する

経路を探す処理は別に用意し、得られた経路候補から同時利用できる案を選ぶ用途です。辺ごとの容量に加え、同じ依頼を二つの経路で二重採用しない条件も付けます。

**具体的な問題・入力**

辺0はA→B、辺1はB→C、辺2はA→C、辺3はC→Dで、各容量は1です。依頼0（A→C）には辺列 {0,1} と {2} の二案、依頼1（B→D）には {1,3}、依頼2（A→D）には {0,1,3} があり、利益は10・9・8・7です。

**完全なコード：`11_route_candidates.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 候補の利益と制約を入れる。未指定の配列は空。
    Solver::Problem problem;

    struct Route {
        int request;
        std::vector<int> edges;
        long long profit;
    };

    const std::vector<Route> routes = {
        {0, {0, 1}, 10},
        {0, {2}, 9},
        {1, {1, 3}, 8},
        {2, {0, 1, 3}, 7}
    };

    // 資源0〜3は辺、4〜6は依頼の選択枠。
    problem.capacity.assign(7, 1);
    for (const auto& route : routes) {
        // 事前に作った一経路を一候補とする。
        problem.weight.push_back(route.profit);

        auto used = route.edges;
        used.push_back(4 + route.request);

        // 通る辺と、その依頼の枠を1単位ずつ使う。
        problem.resources.push_back(used);
    }

    // 経路の接続・始点終点は呼び出し側で確認する。全依頼の採用は強制されない。
    // 同じ辺を複数回通る消費量は表せないため、この例は辺を繰り返さない経路を使う。

    // 入力を保持するソルバーを、乱数seed=1で構築する。
    Solver solver(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 条件を満たす候補集合を求め、結果をコピーで受け取る。
    auto result = solver.solve(options);

    // 必須条件などに矛盾があれば、利益やIDを解として使わない。
    if (!result.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 返却されたID列が現在の全条件を満たすか確認する。
    assert(solver.is_feasible(result.selected));
    // 選択IDから計算した利益と結果を照合する。
    assert(solver.evaluate(result.selected) == result.value);

    // 所有する結果コピーの表示順だけを整える。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益を表示する。
    std::cout << "value=" << result.value << "\nselected:";
    // 選択された元の候補IDをすべて出力する。
    for (int id : result.selected) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

```

**標準出力の例**

```text
value=17
selected: 1 2
```

### 3.12. 確定済みの作業と、採用できない作業を指定する

契約済みの作業は残し、故障などで実施できない作業は除外する用途です。利益が負の必須候補も、解には含めなければなりません。

**具体的な問題・入力**

利益は8・7・6・-2。候補0と1は競合し、候補0・1・2が使う資源の容量は2です。候補1は禁止、候補3は必須、ほかは自由です。

**完全なコード：`12_required_forbidden.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 候補の利益と制約を入れる。未指定の配列は空。
    Solver::Problem problem;
    // 候補3は費用がかかるため負の利益。
    problem.weight = {8, 7, 6, -2};
    // 候補0と1は同時に実施できない。
    problem.conflicts = {{0, 1}};
    // 候補3はこの資源を使わない。
    problem.resources = {
        {0},
        {0},
        {0},
        {}
    };
    // 候補0・1・2から高々二つ。
    problem.capacity = {2};
    // 自由・禁止・自由・必須。値0は「固定なし」ではない。
    problem.fixed = {-1, 0, -1, 1};

    // 入力を保持するソルバーを、乱数seed=1で構築する。
    Solver solver(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 条件を満たす候補集合を求め、結果をコピーで受け取る。
    auto result = solver.solve(options);

    // 必須条件などに矛盾があれば、利益やIDを解として使わない。
    if (!result.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 返却されたID列が現在の全条件を満たすか確認する。
    assert(solver.is_feasible(result.selected));
    // 選択IDから計算した利益と結果を照合する。
    assert(solver.evaluate(result.selected) == result.value);

    // 所有する結果コピーの表示順だけを整える。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益を表示する。
    std::cout << "value=" << result.value << "\nselected:";
    // 選択された元の候補IDをすべて出力する。
    for (int id : result.selected) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

```

**標準出力の例**

```text
value=12
selected: 0 2 3
```

### 3.13. 実行不可能と、何も選ばない実行可能解を区別する

選択IDが空だから失敗とは限りません。必須条件の矛盾、候補なし、選ぶと損になる候補だけの場合を区別して、アプリケーション側の分岐を作ります。

**具体的な問題・入力**

①競合する二候補を両方必須にする。②容量0の資源を使う候補を必須にする。③候補を一つも登録しない。④利益-3と0の自由候補だけを登録する、の四入力を調べます。

**完全なコード：`13_empty_and_infeasible.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 各問題を短い固定反復で調べる。
    Solver::Options options;
    // 時間制限を無効にする。
    options.time_limit_us = -1;
    // 反復上限を必ず有効にしておく。
    options.iteration_limit = 100;

    // 競合する必須候補の入力。
    Solver::Problem conflict;
    // 二つの候補の利益。
    conflict.weight = {5, 4};
    // 同時選択を禁止する。
    conflict.conflicts = {{0, 1}};
    // 両方必須なので矛盾する。
    conflict.fixed = {1, 1};
    // 値で返すsolveなので、一時ソルバーの破棄後も結果を所有できる。
    auto a = Solver(conflict).solve(options);

    // 必須候補が容量を超える入力。
    Solver::Problem capacity;
    // 一候補だけを作る。
    capacity.weight = {5};
    // 資源0を1単位使う。
    capacity.resources = {{0}};
    // 使用可能な容量は0。
    capacity.capacity = {0};
    // 使用できない候補を必須にしている。
    capacity.fixed = {1};
    // この入力も実行不可能。
    auto b = Solver(capacity).solve(options);

    // 全配列が空なら候補は0件。
    Solver::Problem empty;
    // 空の選択は実行可能で、利益0になる。
    auto c = Solver(empty).solve(options);

    // 選ぶ必要がなく、利益も増えない候補の入力。
    Solver::Problem nonpositive;
    // fixedは空なので、どちらも自由。
    nonpositive.weight = {-3, 0};
    // 何も選ばずに利益0を得る。
    auto d = Solver(nonpositive).solve(options);

    // フラグで成功と矛盾を区別する。
    assert(!a.feasible && !b.feasible && c.feasible && d.feasible);
    // 実行可能でも選択IDが空になり得る。
    assert(c.selected.empty() && d.selected.empty());

    std::cout << std::boolalpha;
    // 実行不可能な結果のvalueを解として表示しない。
    std::cout << "conflict=" << a.feasible << '\n';
    // 必須候補による容量超過もfalse。
    std::cout << "capacity=" << b.feasible << '\n';
    // 候補なしの有効な解。
    std::cout << "empty=" << c.feasible << " value=" << c.value << '\n';
    // 利益が増えないときの有効な空集合。
    std::cout << "nonpositive=" << d.feasible
              << " value=" << d.value << '\n';
}

```

**標準出力の例**

```text
conflict=false
capacity=false
empty=true value=0
nonpositive=true value=0
```

### 3.14. 期待利益など、小数の評価値で選ぶ

候補の価値が確率を含む期待値や、小数の評価点の場合です。ソルバー・入力・結果の利益型をdoubleに揃えます。容量や候補IDは整数のままです。

**具体的な問題・入力**

三つの候補の期待利益は0.8・0.7・0.6で、候補0は1および2と競合します。1と2を選ぶ利益は1.3です。

**完全なコード：`14_decimal_profit.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を小数で扱うソルバー型。
    using Solver = SetPackingSolver<double>;

    // 候補の利益と制約を入れる。未指定の配列は空。
    Solver::Problem problem;
    // Solver::Problemもdoubleの利益を持つ型になる。
    problem.weight = {0.8, 0.7, 0.6};
    // 利益型を変えても候補IDはintのまま。
    problem.conflicts = {{0, 1}, {0, 2}};

    // NaNや無限大は渡さない。小数の計算結果は丸め誤差を考慮して比較する。

    // 入力を保持するソルバーを、乱数seed=1で構築する。
    Solver solver(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 条件を満たす候補集合を求め、結果をコピーで受け取る。
    auto result = solver.solve(options);

    // 必須条件などに矛盾があれば、利益やIDを解として使わない。
    if (!result.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 返却されたID列が現在の全条件を満たすか確認する。
    assert(solver.is_feasible(result.selected));
    // 小数の丸め誤差を許して利益を照合する。
    assert(std::abs(solver.evaluate(result.selected) - result.value) < 1e-9);

    // 所有する結果コピーの表示順だけを整える。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益を表示する。
    std::cout << "value=" << result.value << "\nselected:";
    // 選択された元の候補IDをすべて出力する。
    for (int id : result.selected) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

```

**標準出力の例**

```text
value=1.3
selected: 1 2
```

### 3.15. 別の方法で作った選択案の、利益と条件違反を調べる

手作業や別の探索で作った案を採点する用途です。利益だけならevaluate、条件を満たすかどうかはis_feasibleで調べます。

**具体的な問題・入力**

利益8・7・6、競合 {0,1}・{0,2} の問題で、案 {1,2} と案 {0,1} を比べます。後者は単純な利益合計が15でも、競合しているため採用できません。

**完全なコード：`15_inspect_proposal.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 検査したい問題を用意する。
    Solver::Problem problem;
    // 候補ごとの利益。
    problem.weight = {8, 7, 6};
    // 候補0と両立しない組。
    problem.conflicts = {{0, 1}, {0, 2}};

    // 乱数seedは既定値1。検査だけならsolveを呼ぶ必要はない。
    Solver solver(problem);

    const std::array<int, 2> good{1, 2};
    const std::vector<int> bad{0, 1};

    // どちらの案も、IDの範囲内・重複なしは呼び出し側で保証している。
    std::cout << std::boolalpha;
    // arrayをspanとして渡して全条件を調べる。
    std::cout << "good: feasible=" << solver.is_feasible(good);
    // 利益は現在のweightで計算する。
    std::cout << " value=" << solver.evaluate(good) << '\n';

    // vectorも同様に渡せる。
    std::cout << "bad: feasible=" << solver.is_feasible(bad);
    // evaluateだけでは競合を検出しない。
    std::cout << " value=" << solver.evaluate(bad) << '\n';
}

```

**標準出力の例**

```text
good: feasible=true value=13
bad: feasible=false value=15
```

### 3.16. 手元の実行可能解を出発点にして改善する

最初から一つの案がある場合に、その利益を保ちながら改善を試す用途です。単に探索を続けるsolveとは違い、初期案を明示して選択状態を準備します。

**具体的な問題・入力**

利益8・7・6、競合 {0,1}・{0,2} の問題で、初期案は候補0だけ、利益8です。ここから候補1と2の利益13を目指します。

**完全なコード：`16_warm_start.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 改善したい問題を作る。
    Solver::Problem problem;
    // 三候補の利益。
    problem.weight = {8, 7, 6};
    // 初期案と入れ替える候補の競合。
    problem.conflicts = {{0, 1}, {0, 2}};

    // 新しいソルバーを作る。
    Solver solver(problem, 1);

    const std::vector<int> initial{0};
    // 利益維持の前提として、初期案の実行可能性を確認する。
    assert(solver.is_feasible(initial));
    // 初期案の利益を保存する。
    const auto before = solver.evaluate(initial);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 初期案を渡して改善する。initial自体は変更されない。
    auto result = solver.improve(initial, options);
    // 実行可能な初期案より利益が悪くならないことを確認する。
    assert(result.feasible && result.value >= before);

    std::cout << "initial=" << before << '\n';

    // 必須条件などに矛盾があれば、利益やIDを解として使わない。
    if (!result.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 返却されたID列が現在の全条件を満たすか確認する。
    assert(solver.is_feasible(result.selected));
    // 選択IDから計算した利益と結果を照合する。
    assert(solver.evaluate(result.selected) == result.value);

    // 所有する結果コピーの表示順だけを整える。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益を表示する。
    std::cout << "value=" << result.value << "\nselected:";
    // 選択された元の候補IDをすべて出力する。
    for (int id : result.selected) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

```

**標準出力の例**

```text
initial=8
value=13
selected: 1 2
```

### 3.17. 条件に違反した提案を、探索をせずに修復する

外部から届く案や、制約が変わる前の案が、そのままでは実行できないときの入口です。ここでは修復だけの効果を見るために、反復上限を0にします。

**具体的な問題・入力**

利益8・7・6、競合 {0,1}・{0,2} の問題に、同時選択できない案 {0,1} を渡します。ID自体は有効で重複もありません。この順序では候補0が残り、候補1が外れます。

**完全なコード：`17_repair_only.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 修復に使う現在の条件。
    Solver::Problem problem;
    // 候補の利益。
    problem.weight = {8, 7, 6};
    // 元の案の0と1は競合する。
    problem.conflicts = {{0, 1}, {0, 2}};

    // 入力を保持するソルバー。
    Solver solver(problem);

    const std::vector<int> proposed{0, 1};
    // 条件違反を確認する。
    std::cout << std::boolalpha
              << "proposal_valid=" << solver.is_feasible(proposed) << '\n';

    // 修復だけを行う設定。
    Solver::Options options;
    // 相対時間は使わない。
    options.time_limit_us = -1;
    // 候補追加・探索をせず、初期案の準備と修復まで行う。
    options.iteration_limit = 0;

    // 競合する候補を必要に応じて除いて結果を返す。
    auto result = solver.improve(proposed, options);

    // 違反した案の利益は維持されない。IDの範囲外や重複まで修復するAPIではない。

    // 必須条件などに矛盾があれば、利益やIDを解として使わない。
    if (!result.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 返却されたID列が現在の全条件を満たすか確認する。
    assert(solver.is_feasible(result.selected));
    // 選択IDから計算した利益と結果を照合する。
    assert(solver.evaluate(result.selected) == result.value);

    // 所有する結果コピーの表示順だけを整える。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益を表示する。
    std::cout << "value=" << result.value << "\nselected:";
    // 選択された元の候補IDをすべて出力する。
    for (int id : result.selected) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

```

**標準出力の例**

```text
proposal_valid=false
value=8
selected: 0
```

### 3.18. 探索を分割し、その場で読む結果と保存する結果を使い分ける

最初は短く探索し、残りの時間や処理状況を見て追加で探索する用途です。その場で読むならsolve_view、後で比較するならResultのコピーを保持します。

**具体的な問題・入力**

同じ三候補の問題を、200反復、追加300反復、追加200反復の三回に分けます。二回目は参照で読み、その内容を保存してから次の操作へ進みます。

**完全なコード：`18_continue_and_view.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 三回とも同じ入力を使う。
    Solver::Problem problem;
    // 各候補の利益。
    problem.weight = {8, 7, 6};
    // 両立できない組。
    problem.conflicts = {{0, 1}, {0, 2}};

    // 一つのソルバーで探索状態を継続する。
    Solver solver(problem, 1);

    // 最初と最後の呼び出し用設定。
    Solver::Options options;
    // 時間に依存しない例にする。
    options.time_limit_us = -1;
    // 今回分の上限。累計の指定ではない。
    options.iteration_limit = 200;

    // 所有する結果として一回目を保存する。
    auto first = solver.solve(options);

    // Optionsは通常の値としてコピーできる。
    auto more = options;
    // 次の呼び出しで追加する反復の上限。
    more.iteration_limit = 300;

    // コピーせず、ソルバー内部の今回の結果を参照する。
    const auto& view = solver.solve_view(more);
    // 利益を読む前に実行可能性を確認する。
    assert(first.feasible && view.feasible);

    // 保存した一回目の値は変わらない。
    std::cout << "first=" << first.value << '\n';
    // 次の非const操作より前に読む。
    std::cout << "view=" << view.value
              << " iterations=" << view.iterations << '\n';

    // 次の操作後も必要な結果は、ここでコピーしておく。
    Solver::Result saved = view;

    // さらに探索を続ける。これ以後、以前のviewを使わない。
    auto third = solver.solve(options);
    // 同じ問題を続けると最良利益は悪化しない。
    assert(third.feasible && third.value >= saved.value);

    // 独立して所有する二つの結果を比較する。
    std::cout << "saved=" << saved.value
              << " third=" << third.value << '\n';
}

```

**標準出力の例**

```text
first=13
view=13 iterations=300
saved=13 third=13
```

### 3.19. 利益が変わるたびに、同じ候補から選び直す

価格や評価点が変わっても、候補同士の関係が変わらない場合です。全入力を作り直さず、変更した候補の新しい利益を通知します。

**具体的な問題・入力**

三候補の利益8・7・6から始め、候補0の利益を20へ、さらに0へ変更します。競合 {0,1}・{0,2} は同じです。新しい利益が正でなくなった場合も、このAPIで変更できます。

**完全なコード：`19_changing_profit.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 候補数と競合は全ターン共通。
    Solver::Problem problem;
    // 最初の利益。
    problem.weight = {8, 7, 6};
    // 候補0とほかの候補の競合。
    problem.conflicts = {{0, 1}, {0, 2}};

    // 問題の接続を保持して使い続ける。
    Solver solver(problem);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 最初の結果をコピーとして保存する。
    const auto first = solver.solve(options);
    // 最初の入力は実行可能。
    assert(first.feasible);

    // 初期利益は13。
    std::cout << "initial=" << first.value << '\n';

    for (long long new_profit : {20LL, 0LL}) {
        // この回の変更だけを入れる。各リストは空から始まる。
        Solver::Update changes;
        // 加算量ではなく、候補0の新しい利益そのものを渡す。
        changes.weights = {{0, new_profit}};
        // 探索はまだしない。元のproblemも自動では書き換わらない。
        solver.update(changes);

        // 新しい利益で準備・探索し、独立した結果を得る。
        const auto result = solver.solve(options);
        // 現在の条件で検査する。
        assert(result.feasible && solver.is_feasible(result.selected));
        // 更新後の利益と結果を照合する。
        assert(solver.evaluate(result.selected) == result.value);

        // 今回の最良利益を表示する。
        std::cout << "weight0=" << new_profit
                  << " value=" << result.value << '\n';
    }

    // 過去のResultコピーは更新で書き換わらない。
    std::cout << "saved_initial=" << first.value << '\n';
}

```

**標準出力の例**

```text
initial=13
weight0=20 value=20
weight0=0 value=13
saved_initial=13
```

### 3.20. 容量変更と、必須・禁止・自由への切替を行う

設備の台数や予約の確定状況が変わる場合です。容量を減らした結果、既存の必須指定と矛盾することもあるため、変更後もfeasibleを確認します。

**具体的な問題・入力**

利益8・7・6の三候補が同じ資源を使い、最初の容量は2。次に容量1で候補2を必須にし、容量0へ閉鎖します。その後、容量1へ戻して候補2を自由に、候補0を禁止にします。

**完全なコード：`20_changing_constraints.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 接続は変えず、数値と固定指定だけを変更する。
    Solver::Problem problem;
    // 三候補の利益。
    problem.weight = {8, 7, 6};
    // 全候補が資源0を1単位使う。
    problem.resources = {
        {0},
        {0},
        {0}
    };
    // 最初は二候補まで選べる。
    problem.capacity = {2};

    // 初期のfixedは空なので、全候補自由。
    Solver solver(problem);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 初期条件を解く。
    auto initial = solver.solve(options);
    // 解として利益を読む前に確認する。
    assert(initial.feasible);

    // 利益8と7の合計。
    std::cout << "initial=" << initial.value << '\n';

    // 容量と固定指定を同じ更新に入れる。
    Solver::Update changes;
    // 必須使用分を引く前の、資源全体の容量を1にする。
    changes.capacities = {{0, 1}};
    // 候補2を必須にする。
    changes.fixed = {{2, 1}};
    // 指定した値へ置き換える。
    solver.update(changes);

    // 新しい条件で再度解く。
    auto required = solver.solve(options);
    // 候補2だけなら容量1を満たす。
    assert(required.feasible);

    // 必須候補の利益6。
    std::cout << "required=" << required.value << '\n';

    // 前回の変更リストを消し、今回の変更だけにする。
    changes = Solver::Update{};
    // 候補2の必須指定は維持したまま資源を閉鎖する。
    changes.capacities = {{0, 0}};
    // この更新で必須条件が矛盾する。
    solver.update(changes);

    // 実行不可能という結果を受け取る。
    auto closed = solver.solve(options);

    // falseのときvalueは解として使わない。
    std::cout << std::boolalpha << "closed=" << closed.feasible << '\n';

    // 更新項目を作り直す。
    changes = Solver::Update{};
    // 資源を容量1で再開する。
    changes.capacities = {{0, 1}};
    // 候補2を自由へ戻し、候補0は禁止する。IDは重複させない。
    changes.fixed = {{2, -1}, {0, 0}};
    // 変更しない候補1は自由のまま。
    solver.update(changes);

    // 復旧した条件で選び直す。
    auto reopened = solver.solve(options);
    // 現在の全条件を確認する。
    assert(reopened.feasible && solver.is_feasible(reopened.selected));

    // 候補1の利益7になる。
    std::cout << "reopened=" << reopened.value << '\n';
}

```

**標準出力の例**

```text
initial=15
required=6
closed=false
reopened=7
```

### 3.21. 候補の追加や、競合関係の変更に合わせて作り直す

新しい作業が増えた場合など、候補数や接続そのものが変わる用途です。updateで変更できるのは数値と固定指定だけなので、問題全体をrebuildで渡します。

**具体的な問題・入力**

利益8・7・6、競合 {0,1}・{0,2} の問題に、どの候補とも競合しない利益4の候補3を追加します。入力側のProblemを変更しただけでは、ソルバーにはまだ反映されません。

**完全なコード：`21_rebuild.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 再構築にも使う入力を呼び出し側で保持する。
    Solver::Problem problem;
    // 初期の三候補。
    problem.weight = {8, 7, 6};
    // 初期の競合。
    problem.conflicts = {{0, 1}, {0, 2}};

    // ここで入力の内容をソルバー側へ渡す。
    Solver solver(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 再構築前の結果を所有する。
    const auto before = solver.solve(options);
    // 値を使う前に実行可能性を確認する。
    assert(before.feasible);

    // 候補3を入力側に追加する。まだソルバーには反映されない。
    problem.weight.push_back(4);

    // resourcesやfixedが空でなければ、新候補の行・値も追加して長さを揃える。
    // 候補と競合を再構築し、保存解と探索状態を破棄する。
    solver.rebuild(problem);
    // rebuildは乱数状態を維持するため、初期seedも揃えたい場合に明示する。
    solver.seed(1);

    // 新しい四候補の問題を解く。
    const auto after = solver.solve(options);
    // 新しいID体系で条件を確認する。
    assert(after.feasible && solver.is_feasible(after.selected));

    // コピーで保存した以前の結果を読める。
    std::cout << "before=" << before.value << '\n';
    // 候補3を追加した問題の利益。
    std::cout << "after=" << after.value << '\n';

    // IDの意味を入れ替える再構築では、古いselectedをそのまま新問題へ渡さない。
}

```

**標準出力の例**

```text
before=13
after=17
```

### 3.22. 外側の選択を固定し、一部の候補だけ解き直す

大きな解の一部分だけ変更して改善する用途です。変更してよい候補を自由にし、それ以外は「選択済みなら必須、未選択なら禁止」にします。候補を削除しないので、返却IDも元のIDのままです。

**具体的な問題・入力**

利益は10・9・8・7・6・5、競合は {0,1}・{2,3}・{2,4}。初期解は {0,2,5} で利益23です。候補2〜5だけ変更を許し、外側の候補0は選択を維持、候補1は未選択を維持します。

**完全なコード：`22_local_reoptimization.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 元の候補IDを維持したまま全体の問題を作る。
    Solver::Problem problem;
    // 全候補の利益を渡す。
    problem.weight = {10, 9, 8, 7, 6, 5};
    // 外側との競合も含めて保持する。
    problem.conflicts = {{0, 1}, {2, 3}, {2, 4}};

    const std::vector<int> initial{0, 2, 5};

    // 固定する前の問題で初期解を確認する。
    Solver checker(problem);
    // 外側を固定してよい実行可能解であることを確かめる。
    assert(checker.is_feasible(initial));
    // 固定した外側も含む、元の全体利益を記録する。
    const auto before = checker.evaluate(initial);

    // 元の候補数を得る。
    const int n = static_cast<int>(problem.weight.size());
    std::vector<bool> chosen(static_cast<size_t>(n), false);
    for (int id : initial) {
        chosen[id] = true;
    }

    const std::vector<bool> movable = {false, false, true, true, true, true};
    // まず全候補を自由にする。
    problem.fixed.assign(static_cast<size_t>(n), -1);
    for (int id = 0; id < n; ++id) {
        // 外側は現在の採用・不採用の両方を固定する。
        if (!movable[id]) {
            problem.fixed[id] = chosen[id] ? 1 : 0;
        }
    }

    // 外側も含む入力を渡し、変更可能な範囲だけ動かす。
    Solver solver(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 元の全体解を渡す。必須候補分も返却結果に含まれる。
    auto result = solver.improve(initial, options);
    // 実行可能な元の解を悪化させないことを確認する。
    assert(result.feasible && result.value >= before);
    // 外側の固定・競合・容量も含めて確認する。
    assert(solver.is_feasible(result.selected));

    std::vector<bool> after(static_cast<size_t>(n), false);
    // 返却IDは局所番号ではなく、元の候補ID。
    for (int id : result.selected) {
        after[id] = true;
    }

    bool outside_kept = true;
    for (int id = 0; id < n; ++id) {
        if (!movable[id]) {
            outside_kept = outside_kept && (chosen[id] == after[id]);
        }
    }
    assert(outside_kept);

    std::cout << "before=" << before << '\n';
    // 自由部分だけでなく全体の利益を出力する。
    std::cout << "after=" << result.value << '\n';
    std::cout << std::boolalpha << "outside_kept=" << outside_kept << '\n';
}

```

**標準出力の例**

```text
before=23
after=28
outside_kept=true
```

### 3.23. 異なるseedで独立に試し、最も良い結果を残す

一回の試行に頼らず、複数の開始条件で結果を比較する用途です。seedだけ変えて同じソルバーを使い続けると、保存解や探索状態は残るため、独立試行にはなりません。

**具体的な問題・入力**

利益8・7・6の競合問題を、seed 0・1・2でそれぞれ1000反復ずつ解きます。毎回新しくソルバーを作り、最良の実行可能な結果をコピーして保存します。

**完全なコード：`23_multistart.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 全試行で共通の入力。
    Solver::Problem problem;
    // 候補の利益。
    problem.weight = {8, 7, 6};
    // 共通の競合。
    problem.conflicts = {{0, 1}, {0, 2}};

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 既定値ではfeasible=falseなので、まだ解がないことを表せる。
    Solver::Result best;
    for (std::uint64_t initial_seed : {0ULL, 1ULL, 2ULL}) {
        // 毎回構築し直して、保存解や探索状態を持ち越さない。
        Solver solver(problem);
        // 乱数状態を指定する。構築時の第2引数で指定してもよい。
        solver.seed(initial_seed);

        // 今回の独立試行の結果を所有する。
        const auto result = solver.solve(options);

        // 最良の実行可能解をコピーする。
        if (result.feasible && (!best.feasible || result.value > best.value)) {
            best = result;
        }

        // 試行の状態を表示する。
        std::cout << "seed=" << initial_seed
                  << " feasible=" << result.feasible << '\n';
    }

    // すべて実行不可能なら、解の値を使わない。
    if (!best.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    // 各試行のソルバーを破棄しても保存解を読める。
    std::cout << "best=" << best.value << '\n';
}

```

**標準出力の例**

```text
seed=0 feasible=1
seed=1 feasible=1
seed=2 feasible=1
best=13
```

### 3.24. 呼び出しごとの時間と、全体で共通の締切を併用する

外側の処理にも時間を残しながら、残った時間で探索を続ける用途です。相対時間は呼び出しごとに計り直されますが、同じ絶対締切を渡せば全体の終了時刻を共有できます。

**具体的な問題・入力**

最初の探索には最大10ミリ秒を割り当て、二回目は共通の30ミリ秒の締切まで続けます。締切はソルバー構築より前に作ります。例の30ミリ秒は学習用の短い予算です。

時間で止める例なので、利益や反復数は実行ごとに変わり得ます。締切が早く来れば、空の実行可能解を返す場合もあります。下の出力は検証時の一例です。

**完全なコード：`24_time_budget.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // steady_clockで共通締切を作る。
    const auto stop = Solver::Clock::now() + std::chrono::milliseconds(30);

    // 入力準備も共通締切より前からの経過時間に含まれる。
    Solver::Problem problem;
    // 三候補の利益。
    problem.weight = {8, 7, 6};
    // 両立できない組。
    problem.conflicts = {{0, 1}, {0, 2}};

    // 構築時間も共通締切までの残り時間を使う。
    Solver solver(problem, 1);

    // 最初の呼び出しの条件。
    Solver::Options options;
    // 呼び出しから10ミリ秒。ただし絶対締切の方が早ければそちらを使う。
    options.time_limit_us = 10'000.0;
    // 全体で共有する絶対締切。
    options.deadline = stop;
    // 反復数では止めない。時間条件は有効なので無制限にはならない。
    options.iteration_limit = -1;

    // 最初の時間枠で探索し、結果を所有する。
    const auto first = solver.solve(options);

    // 二回目は相対時間を外す。共通締切はそのまま残す。
    options.time_limit_us = -1;

    // 締切まで継続する。既に締切後なら新しい探索は行わない。
    const auto second = solver.solve(options);
    // この入力には矛盾する必須条件がない。
    assert(first.feasible && second.feasible);
    // 最終結果の条件を確認する。
    assert(solver.is_feasible(second.selected));

    // 同じ問題の最良利益は保持される。
    std::cout << std::boolalpha
              << "nondecreasing=" << (second.value >= first.value) << '\n';

    // 値は実行環境や時間の余裕に依存する。
    std::cout << "first=" << first.value
              << " second=" << second.value << '\n';
    // 今回の反復数だけを示す。
    std::cout << "second_iterations=" << second.iterations << '\n';

    // 時間は厳密な強制中断ではない。
    // 初期化・処理完了・返却コピー・外側の出力には余裕を残す。
}

```

**標準出力の例**

```text
nondecreasing=true
first=13 second=13
second_iterations=563552
```

### 3.25. 大きな結果を、短いデバッグログに出す

選択候補が多い場合でも、結果構造体をそのままストリームへ渡せます。選択IDは先頭20件に制限されるため、ログを必要以上に長くせず状態を確認できます。

**具体的な問題・入力**

競合や資源制約のない25候補を作り、利益をすべて1にします。全候補を選んだ結果を標準出力に出し、参照で受け取った結果を標準エラー出力にも出します。

elapsed_usは実測値なので変化します。標準出力と標準エラー出力の表示順も、実行環境によって前後することがあります。

**完全なコード：`25_debug_output.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 固定・競合・資源制約を指定しない入力。
    Solver::Problem problem;
    // 25候補を用意し、全候補の利益を1にする。
    problem.weight.assign(25, 1);

    // 既定seed=1で構築する。
    Solver solver(problem);

    // 小さい例なので、短い固定反復でよい。
    Solver::Options options;
    // 相対時間を無効にする。
    options.time_limit_us = -1;
    // 0にはしない。0だと初回の候補追加も行わない。
    options.iteration_limit = 1;

    // 所有する結果を受け取る。
    const auto result = solver.solve(options);
    // 実際の結果には25件すべてが残る。
    assert(result.feasible && result.selected.size() == 25);

    std::cout << std::fixed << std::setprecision(3);
    // 全フィールドを1行表示。IDは20件までで、改行は自分で付ける。
    std::cout << result << '\n';
    // デバッグ表示の省略は元の配列に影響しない。
    std::cout << "selected_count=" << result.selected.size() << '\n';

    // 追加探索をせず、現在の結果を参照で読む。
    options.iteration_limit = 0;

    // 次の非const操作までに使い終える参照。
    const auto& view = solver.solve_view(options);
    // cerrにも直接渡せる。数値はcerr側の書式に従う。
    std::cerr << "view: " << view << '\n';

    // 全IDが必要な提出用出力では、operator<<ではなくselectedを全件ループする。
}

```

**標準出力の例**

```text
SetPackingResult{feasible=true, value=25, selected=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ... (25 total)], elapsed_us=8.653, iterations=0}
selected_count=25
```

**標準エラー出力の例**

```text
view: SetPackingResult{feasible=true, value=25, selected=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ... (25 total)], elapsed_us=0.07, iterations=0}
```

### 3.26. 同じ状態から、別の将来条件を比較する

現在の探索結果を残したまま、別の利益条件を試す用途です。ソルバーのコピーは入力・保存解・探索状態・乱数状態をまとめて複製するので、コピー側の変更は元に影響しません。

**具体的な問題・入力**

三候補の問題を一度解き、元の利益8・7・6を保つ案と、候補0の利益を20に変更する案を比較します。

**完全なコード：`26_scenario_copy.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    // 比較前に共通している入力。
    Solver::Problem problem;
    // 現在の利益。
    problem.weight = {8, 7, 6};
    // 両シナリオで共通の競合。
    problem.conflicts = {{0, 1}, {0, 2}};

    // 元の状態を持つソルバー。
    Solver original(problem, 1);

    // 今回の停止条件を用意する。
    Solver::Options options;
    // 相対時間では止めず、下の反復上限を使う。
    options.time_limit_us = -1;
    // 今回の探索を高々1000反復にする。
    options.iteration_limit = 1'000;

    // 比較の出発点となる探索を行う。
    const auto initial = original.solve(options);
    // 出発点の結果を確認する。
    assert(initial.feasible);

    // 入力だけでなく探索状態も複製する。大規模入力ではコピー費用に注意。
    Solver branch = original;

    // コピー側にだけ適用する変更。
    Solver::Update changes;
    // 候補0の利益を20に置き換える。
    changes.weights = {{0, 20}};
    // 元のoriginalには影響しない。
    branch.update(changes);

    // 変更した将来条件で探索する。
    const auto alternative = branch.solve(options);
    // 元の利益のまま探索を続ける。
    const auto unchanged = original.solve(options);
    // 両方の結果を検査する。
    assert(alternative.feasible && unchanged.feasible);
    // コピー側の条件で確認する。
    assert(branch.is_feasible(alternative.selected));
    // 元側の条件で確認する。
    assert(original.is_feasible(unchanged.selected));

    // 元の利益13。
    std::cout << "original=" << unchanged.value << '\n';
    // 変更後の利益20。
    std::cout << "changed=" << alternative.value << '\n';
}

```

**標準出力の例**

```text
original=13
changed=20
```

### 3.27. 標準入力から一般形の問題を読み、選択IDを出力する

生成器や外部プログラムから入力を受け取るためのひな型です。この節のプログラムだけは標準入力が必要です。前提を満たす入力を与え、整数利益の問題を解きます。

**具体的な問題・入力**

入力は、1行目に候補数N・資源数M・競合数E・seed・反復上限を指定します。続いてN個の利益、M個の容量、候補ごとの資源列をN行、N個のfixed値、競合をE行の順に読みます。資源列の各行は「使用資源数k、その後にk個の資源ID」です。M=0なら容量の行は省略できます。出力は利益、選択数、昇順の選択ID列です。

下の入力では資源数が0なので、利益の次にある三つの0は、それぞれの候補が使う資源数です。外部から任意の巨大入力を受けるサービス用の完全な入力検証器ではありません。利益の総量や確保できるメモリなどの共通契約も守ってください。

**完全なコード：`27_standard_input.cpp`**

```cpp
// 入力・結果・ソルバーの定義を読み込む。
#include "set_packing_solver_v06.hpp"

int main() {
    // 利益を整数で扱うソルバー型。
    using Solver = SetPackingSolver<long long>;

    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int n, m, e;
    std::uint64_t initial_seed;
    long long iterations;

    if (!(std::cin >> n >> m >> e >> initial_seed >> iterations)) {
        return 0;
    }

    if (n < 0 || n > 10'000'000 ||
        m < 0 ||
        e < 0 || e > INT_MAX / 2 ||
        iterations < 0) {
        return 1;
    }

    // 読み取った全入力をこの構造体へ格納する。
    Solver::Problem problem;
    // 候補数はweightの長さとして渡す。
    problem.weight.resize(static_cast<size_t>(n));
    // 利益の絶対値合計はLLONG_MAX/4以下という入力契約を守る。
    for (auto& weight : problem.weight) {
        if (!(std::cin >> weight)) {
            return 1;
        }
    }

    // 資源数はcapacityの長さで決まる。
    problem.capacity.resize(static_cast<size_t>(m));
    // 容量は非負整数。
    for (auto& capacity : problem.capacity) {
        if (!(std::cin >> capacity) || capacity < 0) {
            return 1;
        }
    }

    // 資源を使わない候補も空の行を持つ。
    problem.resources.resize(static_cast<size_t>(n));
    long long total_resources = 0;
    // 行の順序が候補IDに対応する。
    for (auto& used : problem.resources) {
        int k;
        if (!(std::cin >> k) || k < 0) {
            return 1;
        }

        total_resources += k;
        if (total_resources > INT_MAX) {
            return 1;
        }

        used.resize(static_cast<size_t>(k));
        for (int& resource : used) {
            if (!(std::cin >> resource) || resource < 0 || resource >= m) {
                return 1;
            }
        }
    }

    // 入力の三値を候補ごとに読む。
    problem.fixed.resize(static_cast<size_t>(n));
    // int8_tへ直接入力すると文字扱いになるため、いったんintで読む。
    for (auto& flag : problem.fixed) {
        int value;
        if (!(std::cin >> value) || value < -1 || value > 1) {
            return 1;
        }

        // -1=自由、0=禁止、1=必須。
        flag = static_cast<int8_t>(value);
    }

    // 競合ペアをE組用意する。
    problem.conflicts.resize(static_cast<size_t>(e));
    // 同一ID同士の競合は入力契約違反。
    for (auto& [a, b] : problem.conflicts) {
        if (!(std::cin >> a >> b) ||
            a < 0 || a >= n ||
            b < 0 || b >= n ||
            a == b) {
            return 1;
        }
    }

    // 入力で指定したseedから構築する。
    Solver solver(problem, initial_seed);

    // 反復数で止める設定。
    Solver::Options options;
    // 相対時間を使わない。
    options.time_limit_us = -1;
    // 0なら準備だけ。全停止条件を無効にはしていない。
    options.iteration_limit = iterations;

    // 所有する結果を受け取る。
    auto result = solver.solve(options);

    // 必須条件が矛盾している場合の出力を定める。
    if (!result.feasible) {
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    // 出力を候補IDの昇順にする。
    std::sort(result.selected.begin(), result.selected.end());
    // 利益と選択数を出力する。
    std::cout << result.value << '\n' << result.selected.size() << '\n';
    // デバッグ出力の20件上限を使わず全IDを出す。
    for (size_t i = 0; i < result.selected.size(); ++i) {
        if (i) {
            std::cout << ' ';
        }
        // 元の候補IDを出力する。
        std::cout << result.selected[i];
    }
    std::cout << '\n';
}

```

**標準入力の例**

```text
3 0 2 42 1000
8 7 6
0
0
0
-1 -1 -1
0 1
0 2
```

例えば上の入力を `input.txt` に保存し、`./main < input.txt` で実行します。

**標準出力の例**

```text
13
2
1 2
```

### 検証について

本書の27例は、掲載したコードをそのまま抽出し、`g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0` のC++20モードで個別にコンパイル・実行して確認しています。標準入力の例には掲載した入力を与えました。各出力例はこの検証で得られたものです。時間や選択順が変わり得ること、一般の入力での最適性を保証しないことは、各例でも同じです。

競技環境へ持ち込む場合は、候補の生成費用、ソルバーの構築、結果のコピー、出力を含む全体の時間を測ってください。固定した反復数だけから実行時間を判断することはできません。
