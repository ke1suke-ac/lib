# qap_solver サンプルコード集

対象は `qap_solver_v09.hpp` です。この1つのヘッダーが提供するAPIと、それを使う24本の独立したプログラムを扱います。数学は高校までの四則演算、表、順列を前提にします。C++の変数、配列、関数、ループの基本は必要です。

本書は、1章で問題の意味、2章で公開APIの契約、3章で用途別の実行例を説明します。3章の各コードは `main`、入力値、出力、注意点を含み、その例だけをコピーして実行できます。コード内の説明は日本語です。

## 1. 何を解くライブラリか？

### 1.1 物体と位置を一対一で対応させる

例えば、工場の機器をどの場所に置くかを考えます。機器同士で運ぶ品物が多ければ、近くに置いた方が運搬費は小さくなります。一方で、特定の機器を特定の場所へ置くと、電源工事や引越しの費用がかかることもあります。

このライブラリは、同じ個数の「物体」と「位置」を一対一で対応させ、これらの費用の合計が小さい配置を求めます。この問題を**二次割当問題（QAP）**と呼びます。「二次」という名前の理由は、1つの物体の行き先だけでなく、2つの物体の行き先の組合せによっても費用が変わるためです。

物体も位置も0から番号を付けます。物体が3つのとき、配置 `p = {2, 0, 1}` は「物体0を位置2、物体1を位置0、物体2を位置1へ置く」という意味です。位置0、1、2をちょうど1回ずつ使っています。このように、各番号を1回ずつ並べた配列を**置換**と呼びます。

返却される配列の向きも、常に**物体番号から位置番号**です。位置番号をキーに物体を取り出したい場合は、利用側で逆対応を作ります。

### 1.2 目的関数

費用を表す関数を $C(p)$ とします。求めたいのは、次の値を最も小さくする配置 $p$ です。

$$C(p)=\sum_ {i=0}^{n-1}U_ {i,p_ {i}}+\sum_ {i=0}^{n-1}\sum_ {j=0}^{n-1}F_ {i,j}D_ {p_ {i},p_ {j}}$$

| 記号 | 意味 | コードとの対応 |
| --- | --- | --- |
| $n$ | 物体数であり、位置数でもある。0以上の整数 | `problem.n` |
| $i,j$ | 物体番号。0以上 $n-1$ 以下。同じ番号になる場合もある | flowの行・列の番号 |
| $p$ | 各物体の行き先を並べた配置。0〜 $n-1$ を1回ずつ含む | `result.location_of` などの配列 |
| $p_ {i}$ | 物体 $i$ の位置番号 | `p[i]` |
| $F_ {i,j}$ | 物体 $i$ から物体 $j$ への関係の強さ。例では運搬量 | `flow[i * n + j]` |
| $D_ {p_ {i},p_ {j}}$ | 物体 $i,j$ を置いた2位置間の費用係数。例では距離 | `distance[p[i] * n + p[j]]` |
| $U_ {i,p_ {i}}$ | 物体 $i$ をその位置に置くだけで発生する費用 | `unary[i * n + p[i]]` |
| $C(p)$ | 配置 $p$ の全費用。小さい方がよい | `problem.evaluate(p)`、対応する `result.cost` |
| $\sum$ | 指定した番号の範囲について足し合わせる記号 | for文による加算に相当 |

最初の和は、各物体について「その物体をその場所に置く費用」を1回ずつ足します。これを**単項費用**と呼びます。2つ目の二重の和は、物体の組をすべて調べ、「関係の強さ×行き先の位置間の費用」を足します。

一般に位置番号を $x,y$ と書くと、$D_ {x,y}$ は位置 $x$ から位置 $y$ の係数、$U_ {i,x}$ は物体 $i$ を位置 $x$ に置く費用です。$x,y$ の範囲も0以上 $n-1$ 以下です。座標から距離を作る処理は利用側で行います。

手計算の例では、機器0、1、2を位置0、1、2へ順に置き、位置の座標を0、1、3とします。配置は `p = {0, 1, 2}` です。機器0–1の運搬量8、0–2の運搬量1、1–2の運搬量2に対して、それぞれの距離は1、3、2になります。各方向が同じ値で、単項費用が0なら、費用は次のとおりです。

$$C(p)=2(8\times1+1\times3+2\times2)=30$$

先頭の2は、0→1と1→0を別々に数えるためです。この式の数値は例01の入力に対応します。

### 1.3 数え方と表現できる問題

`flow` と `distance` は、左右対称でなくても構いません。例えば、片道だけ混雑する移動費をそのまま表現できます。対角の値も無視しません。物体 $i$ 自身の係数と、その物体を置く位置の自己係数の積も費用に含まれます。負の数も扱え、最小化費用における負の値は報酬や補助に相当します。

`distance` という名前でも、必ずしも地理的な距離である必要はありません。別の班へ分かれる費用、巡回順の隣接関係、位置ペアの報酬係数などを入れられます。0以上であることや三角不等式を満たすことも必須ではありません。

一対一の配置そのものは問題の条件ですが、配置禁止、物体ごとに違う重量の容量制約、3台以上の組で決まる条件などを直接指定するAPIはありません。位置を班の座席とみなして人数を固定する、ゼロ費用のダミーを足す、といった表現で扱えるものもあります。独自の禁止条件については、返却候補を利用側で検査する必要があります。

物体が0個なら費用の和は0です。単項費用を使わないときは `unary` を空にできます。意味は全要素0と同じです。

## 2. APIリファレンス

### 2.1 実行環境と型引数

必要なのは `qap_solver_v09.hpp` と、GCC・C++20以降の環境です。ヘッダーは `bits/stdc++.h`、128bit整数、GCC向けの機能を利用します。任意のC++コンパイラで動くことを意味しません。サンプルは単一スレッドで動き、追加の外部ライブラリは使いません。

| 型引数 | 意味 | 既定値 | 値の条件 |
| --- | --- | --- | --- |
| `Weight` | flowとdistanceの各要素の型 | `int` | 本書では `int` または `long long`。負値も使用可能。入力が型の範囲に収まること |
| `Score` | unary、積、総和、交換差分、結果costの型 | `long long` | 符号付き整数を使う。`int`、`long long`、GCCの `__int128_t` を目的の範囲に合わせて選ぶ |

型引数を `Weight, Score` の順で取る公開型は、`qap_problem`、`qap_prepared`、`qap_swap_state`、`qap_subproblem` です。`qap_result` は `Score` だけを取ります。`qap_problem<>` は `qap_problem<int, long long>` と同じ意味です。

型引数への厳密なコンパイル時制約は置かれていませんが、それは任意の型で安全に動く保証ではありません。負の交換差分を扱うため、探索のScoreに符号なし型を使わないでください。本書の対象は上表の整数型です。

最終的なcostだけでなく、掛け算、総和、差、符号反転、絶対値、内部の補正式の途中値もScoreに収める必要があります。ライブラリは一般のオーバーフロー検査を行いません。入力作成時の演算が先にあふれないよう、例22のように掛ける前に型を広げます。

この後の表では、`P = qap_problem<W, S>`、`R = qap_result<S>`、`Prepared = qap_prepared<W, S>`、`State = qap_swap_state<W, S>`、`Sub = qap_subproblem<W, S>` と略記します。`W` はWeight、`S` はScoreであり、これらの略記自体はヘッダーが提供する型名ではありません。表の `vector` は `std::vector` の略記です。

`vector<int>` は長さを持つ整数の配列です。引数の `const T&` は、T型の値をコピーせず読み取る参照です。戻り値が参照のときは、参照先が生きている間だけ使います。メソッド末尾の `const` は呼出しによってそのオブジェクトを書き換えないこと、`noexcept` は例外を投げない指定です。`explicit` の付いたコンストラクタも、例のように型名と引数を明示して構築できます。

### 2.2 引数に共通する条件

| 引数・値 | 意味と取り得る値 | 既定値・省略 |
| --- | --- | --- |
| `n` | 物体数=位置数の `int`。0以上。行列の要素数や添字の `n*n` がintに収まる必要がある | 引数として渡す場合は必須。問題の既定構築では0 |
| `flow`, `distance` | `std::vector<W>`。それぞれちょうど `n*n` 要素。行ごとに並べる | 行列受取り構築では必須。個数だけの構築では0埋め |
| `unary` | `std::vector<S>`。空、またはちょうど `n*n` 要素 | 行列受取り構築では `{}`、つまり空 |
| `p`, `initial`, `location_of`, `full_initial` | `std::vector<int>` の物体→位置。長さn、0〜n−1を1回ずつ含む完全置換 | 配置を受け取るAPIでは必須 |
| `i,j` または `r,s` | 交換する物体番号。各0以上n未満。同じ番号同士なら差分0 | 必須 |
| `movable_objects` | `std::vector<int>`。0〜n−1の物体番号を重複なく列挙。空も全物体も可。順序は任意 | 部分問題APIでは必須 |
| `known_cost`, `full_initial_cost` | `S`。同じ入力行列で、その引数の完全配置を評価した正しい費用 | 既知費用を受け取る形式では必須。別の形式では内部評価 |
| `deadline` | `std::chrono::steady_clock::time_point` の絶対時刻。現在・過去の時刻も渡せる | 探索APIでは必須。時間幅のミリ秒数を直接渡すものではない |
| `seed` | `std::uint64_t`。0から $2^{64}-1$ まで。乱数の開始値 | すべての内蔵探索APIで `1` |

通常の32bit intなら、`n*n` の条件だけでも `n<=46340` が必要です。これは実用上その規模を扱えるという保証ではありません。行列を密に保持するため、メモリと計算量を別に考えます。

`seed=0` は時刻などからランダムseedを自動生成する指定ではありません。また、同じseedでも実時間で探索を打ち切る位置が変われば結果は変わり得ます。

`known_cost` は他の目的関数のスコアや、入力更新前のcostではありません。型も `S` に合わせます。例えばScoreがlong longなら、整数リテラルより `problem.evaluate(p)` の返り値をそのまま渡すと型と値を合わせやすくなります。

### 2.3 `qap_problem`：入力と評価

#### 公開データ

| メンバー | 型 | 意味 | 既定構築時 |
| --- | --- | --- | --- |
| `n` | `int` | 物体数=位置数 | 0 |
| `flow` | `std::vector<W>` | `flow[i*n+j]` が物体i→jの係数 | 空 |
| `distance` | `std::vector<W>` | `distance[x*n+y]` が位置x→yの係数 | 空 |
| `unary` | `std::vector<S>` | `unary[i*n+x]` が物体iを位置xに置く費用 | 空 |

4つとも公開データなので、構築後に入力を設定できます。ただし `n` だけ変えても行列は自動でサイズ変更されません。前処理を借用中の問題も変更しません。

#### 構築とメソッド

| API | 引数と既定値 | 戻り値・動作 | 掲載例 |
| --- | --- | --- | --- |
| `qap_problem()` | 引数なし | n=0で全配列が空の問題を構築 | 23 |
| `explicit qap_problem(int n)` | 個数n。既定値なし | flowとdistanceをn×n個の0で構築。unaryは空 | 01ほか |
| `qap_problem(int n, vector<W> flow, vector<W> distance, vector<S> unary = {})` | n、2行列、任意の単項費用。長さの条件は2.2 | 行列を所有する問題を構築 | 13、23 |
| `bool is_valid_permutation(const vector<int>& p) const` | 調べる配列p。既定値なし。この検査には不正な配列も渡せる | 長さ・範囲・重複が正しければtrue。空配列が正しいのはn=0の場合 | 01、02ほか |
| `S evaluate(const vector<int>& p) const` | 有効な完全置換p。既定値なし | unaryを含む全目的関数値を返す。pを変更しない | 02ほか |
| `S swap_delta(const vector<int>& p, int r, int s) const` | 有効なp、物体r、物体s。すべて必須 | rとsの位置を交換した場合の「交換後の費用−交換前の費用」。pを変更しない | 02 |

行列受取り構築の配列引数は値渡しです。普通に配列変数を渡すとコピーされ、`std::move` を使うと配列の内容を移せます。コピー・移動後は、問題が自身の入力配列を所有します。

`is_valid_permutation` が検査するのは一対一の配置だけです。入力行列の長さや、独自の配置禁止、容量条件は検査しません。`evaluate` や差分APIへ不正な配置を渡してエラー値を得る仕様でもありません。事前条件違反は、assertで止まる場合や未定義動作になる場合があります。特にassertを無効にしても正しい入力を渡す責任は変わりません。

### 2.4 `qap_result`：返却値と出力

| データ・操作 | 意味・取り得る値 | 既定値・戻り値 |
| --- | --- | --- |
| `std::vector<int> location_of` | `location_of[i]` が物体iの位置。探索APIから返るときは完全置換 | 既定構築では空 |
| `S cost` | その配置を同じ問題で評価したQAP費用。負になることもある | 既定構築では0 |
| `qap_result<S>{p, cost}` | 配置と費用を順番に指定する集成体初期化。正しい組を利用側で与える | costの自動評価はしない |
| `std::ostream& operator<<(std::ostream& out, const qap_result<S>& result)` | 出力先outと読むだけの結果resultを渡す。両引数に既定値なし | 同じoutへの参照を返し、続けて文字列や改行を挿入できる |

`qap_result<> r;` は空配置・cost=0の入れ物です。非空問題の解を自動生成するわけではありません。独自探索の状態を結果へまとめる場合も、配置とcostを対応させます。

出力の形は `qap_result{cost=30, location_of=[0, 1, 2]}` です。`std::cout`、`std::cerr`、`std::ostringstream` へ直接渡せます。改行は利用側で付けます。配列は先頭20要素までで、21要素以上なら `...` と `(size=全要素数)` を表示します。空配列は `[]` です。表示の省略は結果本体には影響しません。

128bit整数のcostも結果全体の出力に対応し、その部分は10進数で表示されます。通常の整数はストリームの書式に従います。結果やストリームの書式フラグは変更せず、費用の再計算も行いません。デバッグ表示は全配置を保存する形式ではありません。

### 2.5 `solve_qap` と `improve_qap`：完全配置を求める

| API | 引数の順序・既定値 | 戻り値と保証 | 掲載例 |
| --- | --- | --- | --- |
| `solve_qap(problem, deadline, seed = 1)` | `const P& problem`、絶対締切、seed | `R`。初期配置を内部で作り、完全配置とその費用を返す | 01ほか |
| `improve_qap(problem, initial, deadline, seed = 1)` | `const P& problem`、`const vector<int>& initial`、絶対締切、seed | `R`。同じ問題で `cost <= problem.evaluate(initial)` を満たす | 12 |

seed以外の引数は省略できません。問題は2.3の形、initialは2.2の完全置換を満たす必要があります。入力や渡した配置は書き換えません。結果は自身の配列を所有します。

`solve_qap` へ初期配置を追加して渡す形式はありません。開始解を指定する場合は `improve_qap` を使います。どちらも一般の最適性を証明するAPIではありません。

### 2.6 `qap_prepared`：同じ問題の前処理を再利用する

| API | 引数と既定値 | 戻り値・動作 | 掲載例 |
| --- | --- | --- | --- |
| `explicit qap_prepared(const P& problem)` | 有効な問題。既定値なし | problemへの参照と前処理を保持する。問題のコピーは所有しない | 13 |
| `const P& problem() const noexcept` | なし | 借りている元の問題へのconst参照 | 13 |
| `S evaluate(const vector<int>& p) const` | 有効な完全置換p。必須 | 元の問題で全費用を計算。pは変更しない | 13 |
| `S swap_delta(const vector<int>& p, int i, int j) const` | 有効なp、交換物体i、j。必須 | 交換後−交換前。pは変更しない | 13 |
| `R solve(time_point deadline, uint64_t seed = 1) const` | 必須の絶対締切と任意seed | 前処理を再利用して新しい配置を求める | 13 |
| `R improve(const vector<int>& initial, time_point deadline, uint64_t seed = 1) const` | 必須の完全配置initial、必須の絶対締切、任意seed | initialを全評価し、その費用以下の結果を返す | 13、20 |

ここでの `time_point` は `std::chrono::steady_clock::time_point`、`uint64_t` は `std::uint64_t` の略記です。

`prepared.solve` は、前回の結果を内部に保存してそこから自動再開するAPIではありません。結果を引き継ぐときは、その配置を `prepared.improve` へ渡します。`prepared.improve` には既知costを受け取る形式はありません。

`prepared.swap_delta` は毎回完全な置換検査をしません。重複のない有効なpを利用側で保証します。繰り返しの前に `problem.is_valid_permutation(p)` で検査できます。

**寿命の条件：** preparedを使う間、元のproblemを変更・移動・破棄しないでください。入力を変えたらpreparedを作り直します。関数からpreparedだけを返して、その参照先のローカルなproblemを失う使い方もできません。

### 2.7 `qap_swap_state`：現在配置と費用を一緒に保持する

| API | 引数と既定値 | 戻り値・動作 | 掲載例 |
| --- | --- | --- | --- |
| `qap_swap_state(const Prepared& prepared, vector<int> location_of)` | preparedと有効な配置。両方必須 | 配置を所有し、その費用を全評価して保持 | 17、18 |
| `qap_swap_state(const Prepared& prepared, vector<int> location_of, S known_cost)` | prepared、有効な配置、その正しい費用。すべて必須 | 全費用の再評価を省いてknown_costを保持 | 17 |
| `const vector<int>& location_of() const noexcept` | なし | 現在の物体→位置へのconst参照 | 17、18 |
| `S cost() const noexcept` | なし | 保持している現在のQAP費用 | 17、18 |
| `S swap_delta(int i, int j) const` | 交換する物体i、j。必須 | 現在配置について交換後−交換前。状態は変更しない | 17、18 |
| `S apply_swap(int i, int j)` | 交換する物体i、j。必須 | 実際に位置と保持costを更新し、交換差分を返す | 17、18 |

各i、jの範囲は0以上n未満で、同じ物体同士なら差分0です。既知costは構築時の配置をpreparedの元の問題で評価した値でなければなりません。ライブラリがその値の正しさを再検算する仕様ではありません。

`apply_swap` は費用が増える交換も行います。採用条件は利用側が決めます。返却値は更新後のcostそのものではありません。また、先に計算したdeltaを渡す `apply_swap(i,j,delta)` という形式はありません。

位置配列の引数は値渡しです。配列をコピーするか、`std::move` で内容を移して状態に所有させます。`location_of()` のconst参照は、stateが生きている間に利用します。現在配置を直接書き換えるための参照ではありません。

**寿命の条件：** stateはpreparedを借り、preparedはproblemを借ります。stateを使っている間、この2つを変更・移動・破棄しません。別の状態を作るときも、配置とknown_costを同じ時点の組にします。

### 2.8 部分問題の生成と復元

#### `make_qap_subproblem`

| API | 引数の順序・既定値 | 戻り値・動作 | 掲載例 |
| --- | --- | --- | --- |
| `make_qap_subproblem(problem, full_initial, movable_objects)` | `const P&`、完全配置へのconst参照、可動物体一覧へのconst参照。すべて必須 | `Sub`。開始費用を全評価して、小さい問題を作る | 15 |
| `make_qap_subproblem(problem, full_initial, movable_objects, full_initial_cost)` | 上の3引数と、型Sの正しい完全費用。すべて必須 | `Sub`。渡した費用から定数項を作る | 15 |

ここにはdeadlineやseedの引数はありません。問題を変換するAPIであり、それだけで配置を探索するわけではありません。元の問題、完全配置、可動物体一覧は変更しません。返却されたSubは必要なデータを所有し、元のproblemを参照して使い続ける形ではありません。

可動物体数を $k$ とします。動かせるのは指定物体だけで、行き先は**その物体たちが開始時に使っていた位置集合**だけです。固定物体の位置へ入ることはできません。可動一覧は重複なしならどの順序でもよく、縮約番号はその一覧の順になります。

#### `qap_subproblem` の全公開データ

| メンバー | 型 | 生成された値の意味 | 既定構築時 |
| --- | --- | --- | --- |
| `problem` | `P` | k物体・k位置の縮約問題。固定部分との相互作用も含む | n=0の空問題 |
| `base_location_of` | `vector<int>` | 復元の土台となる元の完全配置。長さn | 空 |
| `object` | `vector<int>` | `object[a]` は縮約物体aに対応する元の物体。長さk | 空 |
| `location` | `vector<int>` | `location[x]` は縮約位置xに対応する元の位置。長さk | 空 |
| `initial_location_of` | `vector<int>` | 元の開始配置に対応する縮約配置。生成時は0、1、…、k−1 | 空 |
| `constant_offset` | `S` | 縮約費用に加えて完全費用へ戻す定数。負の値もあり得る | 0 |

生成関数が作った対応関係では `object[a] = movable_objects[a]`、`location[a] = full_initial[object[a]]` となります。配列が公開されていても、対応を保たず個別に書き換えると復元できなくなります。通常は `make_qap_subproblem` で作り、読み取って利用します。

#### `restore`

| API | 引数と既定値 | 戻り値・動作 |
| --- | --- | --- |
| `R restore(const R& reduced) const` | 縮約問題の結果reduced。必須。配置は長さkの完全置換、costは縮約問題での正しい費用 | 元のn物体の完全結果。可動物体の行き先を対応表で戻し、costへconstant_offsetを加算 |

`restore` は受け取ったcostを再計算しません。縮約問題の配置を $q$、それを復元した完全配置を $p$ とすると、費用の関係は次のとおりです。

$$C(p)=c+C_ {\mathrm{sub}}(q)$$

ここで $c$ は `constant_offset`、$C_ {\mathrm{sub}}(q)$ は `sub.problem.evaluate(q)` です。$p$ の可動物体の行き先は `p[sub.object[a]] = sub.location[q[a]]`、それ以外は `base_location_of` のままです。単にrestoreしただけで費用が改善する保証はありません。縮約の開始解以下の結果を渡せば、完全問題でも開始解以下になります。

### 2.9 `improve_qap_subset`：部分改善をまとめて実行する

| API | 引数の順序・既定値 | 戻り値と保証 | 掲載例 |
| --- | --- | --- | --- |
| `improve_qap_subset(problem, full_initial, movable_objects, deadline, seed = 1)` | 有効な問題、完全配置、重複なしの可動一覧、必須の絶対締切、任意seed | `R`。元の問題の完全配置と完全費用。開始費用を内部で評価 | 14 |
| `improve_qap_subset(problem, full_initial, movable_objects, full_initial_cost, deadline, seed = 1)` | 上の引数に、正しい型Sの完全費用をdeadlineの前へ追加 | `R`。既知costを使う部分改善 | 16、23 |

problem・full_initial・movable_objectsの型と範囲は2.8と同じです。結果は縮約配置ではなく、n物体すべての配置です。指定外の物体は固定され、指定内の物体も開始時の位置集合内でのみ移動します。正しい入力では開始費用より悪化しません。

可動物体が0個または1個なら並べ替えの余地がありません。可動物体が全物体なら、通常の完全配置改善へ進む経路があります。この経路では既知costを渡していても開始費用が全評価されます。

### 2.10 締切、終了、保証の範囲

| 状況 | 返却・処理の意味 |
| --- | --- |
| n=0 | 空配置、cost=0 |
| n=1 | 配置は `{0}` のみ。対角項とunaryを含む費用を返す |
| `solve_qap` / `prepared.solve` で締切到達 | 初期配置の構築と評価は行う。定数時間で即時終了する指定ではない |
| `improve_qap` / `prepared.improve` で締切到達 | 開始配置を全評価し、開始配置と正しい費用を返す |
| 費用省略版subsetで早期終了 | まず完全費用を評価してから、開始配置を返す |
| 既知cost版subsetで締切到達、または可動数が2未満 | 開始配置をコピーし、渡されたcostとともに返す。値の正しさは利用側の責任 |
| 部分問題の構築や前処理の途中で締切到達 | 各処理を直ちに中断する保証はない。返却や出力の時間も必要 |

deadlineは終了の目安です。呼出し全体、前処理、結果のコピー、表示までを厳密な時刻以内に収める契約ではありません。プログラム全体に時間制限がある場合は、入力作成や出力の余裕も含めて締切を決めます。

返却結果には最適性を表すフラグ、追加制約を満たしたかのフラグ、終了理由のenumはありません。**このヘッダーに利用者向けの列挙型はありません。** `is_valid_permutation` はbool、各差分APIはScoreを返します。戻り値0を「時間切れ」の印と解釈しないでください。

### 2.11 公開APIの範囲と所有関係

| 型 | 所有するもの・参照するもの |
| --- | --- |
| `qap_problem` | 入力行列を所有する |
| `qap_result` | 配置配列と費用を所有する |
| `qap_prepared` | 前処理を所有し、元のproblemを参照する |
| `qap_swap_state` | 現在配置と費用を所有し、preparedを参照する |
| `qap_subproblem` | 縮約問題、対応表、復元の土台、定数を所有する |

これらの型の通常のコピー・移動・代入はC++の機能として利用できますが、preparedやstateをコピーしても参照先の寿命は延びません。入力や前処理を借用中に移動してよい、という意味でもありません。

上記がこのヘッダーの利用者向けAPIです。公開の探索オプション型、探索方式を選ぶenum、温度や反復回数の設定API、独自評価のコールバックを内蔵探索へ渡すAPIはありません。独自の受理規則は例17・18、別目的による採否は例19のように外側へ書きます。`qap_debug_detail` 内の表示補助、クラスのprivate部分、自己テスト用の型は利用者向けAPIに含めません。

## 3. ユースケース

### 3.0 すべての例に共通する実行方法

各例は**ハードコードされた入力値をmain内に持つ独立したプログラム**です。標準入力を待たず、そのまま実行できます。問題の大きさ・行列・初期配置などを、用途に合わせて差し替えます。外部入力を読む場合も、型の範囲、行列サイズ、置換や可動集合の条件を満たしてからAPIへ渡します。

`qap_solver_v09.hpp` と、選んだ例をコピーした `main.cpp` を同じディレクトリへ置きます。

```bash
g++ -std=c++20 -O2 -Wall -Wextra main.cpp -o qap_sample
./qap_sample
```

`-std=c++20` は言語規格、`-O2` は最適化、`-Wall -Wextra` は警告、`-o` の後は実行ファイル名です。C++23なら `-std=c++23` にします。各例の `assert` は入力・変換・返却値の確認用なので、学習時は `-DNDEBUG` を付けずに使います。assertがライブラリのすべての事前条件を検査するわけではありません。

ZIPを使う場合は、展開したルートで `g++ -std=c++20 -O2 -Wall -Wextra -I. examples/01_minimal.cpp -o qap_sample` のようにビルドできます。

サンプルの50msなどは短い実演用の探索目安です。独自探索の例にも時間チェックを入れていますが、厳密な停止保証ではありません。出力例は検証時の一例で、同じ入力・seedでも実行時間の違いで配置が変わる場合があります。固定的な値を契約として使うのではなく、コード内の妥当性・費用一致・非悪化などの検査を参照してください。

例01〜11は入力モデル、例12〜20は探索の組込み、例21〜24は制約・数値・境界・ログを主に扱います。例02は既存配置の採点という、探索を呼ばない使い方です。各例は他の例の関数定義や変数に依存しません。

| 例 | 解く問題・利用場面 | 主なAPI・表現 |
| --- | --- | --- |
| 01 | よく通信する機器を近くに配置する | 通常のsolve、返却値、配置の全件出力 |
| 02 | 持ち込んだ配置と交換候補を採点する | 置換検査、evaluate、swap_delta |
| 03 | 移設費や場所の相性を含めて配置する | unaryの追加と複数費用の合算 |
| 04 | 一方向の運搬と機器自身の場所依存費用を表す | 非対称行列、対角項、負の費用 |
| 05 | 相性のよい組合せによる報酬を最大化する | 最大化から最小化への変換 |
| 06 | 機器より多い候補位置から設置場所を選ぶ | ダミー物体による空き位置 |
| 07 | 少数の接続だけを持つ機器をグリッドへ置く | 辺リスト、座標変換、空きマス |
| 08 | 人数が決まったグループへ分ける | 固定人数の容量を位置数で表現 |
| 09 | 都市を一度ずつ巡る順序を決める | 巡回順序と逆置換 |
| 10 | 候補を選び、その配置も同時に決める | ダミー位置による選択と配置 |
| 11 | 似たグラフの頂点を対応付ける | 定数項の除去と元の目的値への復元 |
| 12 | 既に持っている配置を改善する | improve_qap、明示的seed、非悪化 |
| 13 | 同じ問題を複数回試して最良候補を磨く | 行列受取り構築、preparedの全メソッド、複数試行 |
| 14 | 固定した設備を動かさずに一部だけ調整する | 費用省略版improve_qap_subset |
| 15 | 小さい可動部分を全通り調べて最善にする | 部分問題生成の両形式、全公開フィールド、restore |
| 16 | 動かす範囲を変えながら部分改善を繰り返す | 既知cost版improve_qap_subset、共通締切 |
| 17 | 交換を選ぶ独自の探索を作る | swap_stateの両構築形式と全メソッド |
| 18 | 3台以上の関係で決まる追加費用も扱う | QAP差分と利用側評価の合成 |
| 19 | QAPで候補を作り、本当の目的で採否を決める | 代理目的と本来目的の分離 |
| 20 | 運搬量が変わった後に再計画する | 入力更新、参照寿命、再評価と前処理の再構築 |
| 21 | 配置禁止を罰点に変え、採用前に検査する | 罰点、制約検査、実行可能解の保持 |
| 22 | 積や総和が64bitを超える費用を計算する | 広い整数型と結果の出力演算子 |
| 23 | 対象なし・1台だけ・締切到達を扱う | 既定構築、行列とunaryの受取り、退化ケース |
| 24 | 担当者ごとの割当費用と長い結果のログ出力 | 単項のみの問題、cout/cerr/ostringstream、表示上限 |


### 3.1 例01：よく通信する機器を近くに配置する

工場の3台の機器を、直線上の3つの設置場所へ1台ずつ置きます。運搬回数の多い機器同士ほど近くに置きたい場面です。

**具体的な問題。** 機器0–1の運搬量は8、0–2は1、1–2は2。位置の座標は0、1、3です。運搬量×距離の合計を小さくします。

**入力の表し方。** flowに機器間の運搬量、distanceに位置間の距離を入れます。両方向を同じ値で入力するので、無向の各ペアは2回数えます。

ファイル名：`01_minimal.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：よく通信する機器を近くに配置する。

// 問題：機器0–1の運搬量は8、0–2は1、1–2は2。
// 位置の座標は0、1、3です。
// 運搬量×距離の合計を小さくします。

// 注意：両方向を数える目的式です。
// 片方向だけの運搬費と比べるときは2倍の数え方に注意。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 3物体・3位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(3);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1,  // 物体0から全物体への係数。
        8, 0, 2,  // 物体1から全物体への係数。
        1, 2, 0   // 物体2から全物体への係数。
    };

    // 入力：位置ペアの係数を行ごとに並べる。
    // 自動的な対称化は行われない。
    problem.distance = {
        0, 1, 3,  // 位置0から全位置への係数。
        1, 0, 2,  // 位置1から全位置への係数。
        3, 2, 0   // 位置2から全位置への係数。
    };

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 初期配置を内部生成して探索。
    // 締切は必須、seedを省略すると1。
    auto result = solve_qap(problem, deadline);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    std::cout << "cost = " << result.cost << '\n';

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

**出力の読み方。** costと各機器の位置番号を表示します。位置番号と座標は別で、位置2の座標は3です。

検証時の標準出力の一例：

```text
cost = 30
object 0 -> location 0
object 1 -> location 1
object 2 -> location 2
```


### 3.2 例02：持ち込んだ配置と交換候補を採点する

別のプログラムや人が作った配置について、費用を調べ、2台を交換すると得かを比較します。探索を起動せずにライブラリを評価器として使う例です。

**具体的な問題。** 3台を位置0、1、2へ順に置いた配置を採点し、機器0と1を交換したときの費用の増減を求めます。

**入力の表し方。** swap_deltaの引数0、1は位置番号ではなく物体番号です。負なら改善、正なら悪化です。swap_delta自体は配置を変えません。

ファイル名：`02_evaluate_swap.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：持ち込んだ配置と交換候補を採点する。

// 問題：3台を位置0、1、2へ順に置いた配置を採点し、
// 機器0と1を交換したときの費用の増減を求めます。

// 注意：評価前に置換を検査します。
// swap_deltaは物体番号を受け取り、配置を変更しません。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 3物体・3位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(3);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1,  // 物体0から全物体への係数。
        8, 0, 2,  // 物体1から全物体への係数。
        1, 2, 0   // 物体2から全物体への係数。
    };

    // 入力：位置ペアの係数を行ごとに並べる。
    // 自動的な対称化は行われない。
    problem.distance = {
        0, 1, 3,  // 位置0から全位置への係数。
        1, 0, 2,  // 位置1から全位置への係数。
        3, 2, 0   // 位置2から全位置への係数。
    };

    // 入力配置。
    // 物体0、1、2を同番号の位置へ置く。
    std::vector<int> p{0, 1, 2};

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(p));

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(!problem.is_valid_permutation({0, 0, 2}));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    const long long before = problem.evaluate(p);

    // 指定した2物体の交換差分。
    // 配置は変更せず、同じ物体同士なら0。
    const long long delta = problem.swap_delta(p, 0, 1);
    std::swap(p[0], p[1]);

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    const long long after = problem.evaluate(p);
    assert(before == 30);
    assert(delta == 2);
    assert(after == before + delta);
    std::cout << before << " + " << delta << " = " << after << '\n';
}
```

**出力の読み方。** 30 + 2 = 32を表示します。重複位置を含む不正な配置も、評価する前に検査しています。

検証時の標準出力の一例：

```text
30 + 2 = 32
```


### 3.3 例03：移設費や場所の相性を含めて配置する

通信距離を短くしたい一方で、既設機器を動かすと費用がかかります。特定の機器だけ、ある場所に設置しづらい場合もあります。

**具体的な問題。** 現在の配置は0、1、2。機器を現在と異なる位置へ移すと1台につき5、機器2を位置0に置くと追加で4かかります。運搬費とこれらの費用の合計を最小化します。

**入力の表し方。** unaryの物体i・位置xの欄へ、その1台だけで決まる費用を足します。ここでのpreviousは費用作成用のデータであり、solve_qapに渡す初期解ではありません。

ファイル名：`03_unary.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：移設費や場所の相性を含めて配置する。

// 問題：現在の配置は0、1、2。
// 機器を現在と異なる位置へ移すと1台につき5、機器2を位置0に置くと追加で4かかります。
// 運搬費とこれらの費用の合計を最小化します。

// 注意：unaryは空のまま添字アクセスしないこと。
// 現在配置previousは費用を作るための入力です。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 3物体・3位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(3);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1,  // 物体0から全物体への係数。
        8, 0, 2,  // 物体1から全物体への係数。
        1, 2, 0   // 物体2から全物体への係数。
    };

    // 入力：位置ペアの係数を行ごとに並べる。
    // 自動的な対称化は行われない。
    problem.distance = {
        0, 1, 3,  // 位置0から全位置への係数。
        1, 0, 2,  // 位置1から全位置への係数。
        3, 2, 0   // 位置2から全位置への係数。
    };

    // 入力：移設前の物体→位置。
    // 費用を作るために参照する。
    const std::vector<int> previous{0, 1, 2};

    // 入力：1台を別位置へ移すときの追加費用。
    const long long move_cost = 5;

    // 単項費用を使うためn×n個の領域を確保し、指定値で埋める。
    problem.unary.assign(problem.n * problem.n, 0);

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // 問題の物体数=位置数nを使う。
        // 添字は0以上n未満。
        for (int x = 0; x < problem.n; ++x) {
            // unary[物体i*n+位置x]へ、その1物体と位置だけで決まる費用を設定する。
            if (x != previous[i]) {
                problem.unary[i * problem.n + x] += move_cost;
            }
        }
    }

    // unary[物体i*n+位置x]へ、その1物体と位置だけで決まる費用を設定する。
    problem.unary[2 * problem.n + 0] += 4;

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(problem.evaluate(previous) == 30);

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 初期配置を内部生成して探索。
    // 締切は必須、seedを省略すると1。
    auto result = solve_qap(problem, deadline);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    std::cout << "cost = " << result.cost << '\n';

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

**出力の読み方。** 移設費と相性費用も含むcost、および新しい配置を表示します。

検証時の標準出力の一例：

```text
cost = 30
object 0 -> location 0
object 1 -> location 1
object 2 -> location 2
```


### 3.4 例04：一方向の運搬と機器自身の場所依存費用を表す

往路と復路で運搬量や移動費が異なる施設を考えます。機器自身の発熱量と場所の冷却難度の積のような費用も加えます。

**具体的な問題。** 機器0→1の流量は9、1→0は2、位置0→1の移動費は4、1→0は1です。機器2の自己係数を3、場所の自己係数を2、1、4とし、機器2を位置0に置くと5の補助が得られます。

**入力の表し方。** flowとdistanceは非対称でも使えます。対角要素も目的関数に含まれます。補助5は最小化費用では−5です。

ファイル名：`04_directed_diagonal.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：一方向の運搬と機器自身の場所依存費用を表す。

// 問題：機器0→1の流量は9、1→0は2、位置0→1の移動費は4、1→0は1です。
// 機器2の自己係数を3、場所の自己係数を2、1、4とし、
// 機器2を位置0に置くと5の補助が得られます。

// 注意：非対称・対角・負値はそのまま数えます。
// flowとdistanceの添字の向きを取り違えないこと。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 3物体・3位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(3);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1,  // 物体0から全物体への係数。
        8, 0, 2,  // 物体1から全物体への係数。
        1, 2, 0   // 物体2から全物体への係数。
    };

    // 入力：位置ペアの係数を行ごとに並べる。
    // 自動的な対称化は行われない。
    problem.distance = {
        0, 1, 3,  // 位置0から全位置への係数。
        1, 0, 2,  // 位置1から全位置への係数。
        3, 2, 0   // 位置2から全位置への係数。
    };

    // flow[物体i*n+物体j]の向きで係数を設定・参照する。
    problem.flow[0 * 3 + 1] = 9;

    // flow[物体i*n+物体j]の向きで係数を設定・参照する。
    problem.flow[1 * 3 + 0] = 2;

    // distance[位置x*n+位置y]の向きで係数を設定・参照する。
    problem.distance[0 * 3 + 1] = 4;

    // distance[位置x*n+位置y]の向きで係数を設定・参照する。
    problem.distance[1 * 3 + 0] = 1;

    // flow[物体i*n+物体j]の向きで係数を設定・参照する。
    problem.flow[2 * 3 + 2] = 3;

    // distance[位置x*n+位置y]の向きで係数を設定・参照する。
    problem.distance[0 * 3 + 0] = 2;

    // distance[位置x*n+位置y]の向きで係数を設定・参照する。
    problem.distance[1 * 3 + 1] = 1;

    // distance[位置x*n+位置y]の向きで係数を設定・参照する。
    problem.distance[2 * 3 + 2] = 4;

    // 単項費用を使うためn×n個の領域を確保し、指定値で埋める。
    problem.unary.assign(3 * 3, 0);

    // unary[物体i*n+位置x]へ、その1物体と位置だけで決まる費用を設定する。
    problem.unary[2 * 3 + 0] = -5;

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(problem.evaluate({0, 1, 2}) == 64);

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 初期配置を内部生成して探索。
    // 締切は必須、seedを省略すると1。
    auto result = solve_qap(problem, deadline);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    std::cout << "cost = " << result.cost << '\n';

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

**出力の読み方。** 方向・対角・負の単項費用を含むcostと配置を表示します。

検証時の標準出力の一例：

```text
cost = 39
object 0 -> location 2
object 1 -> location 1
object 2 -> location 0
```


### 3.5 例05：相性のよい組合せによる報酬を最大化する

配置による費用を減らす代わりに、相性のよい機器を好条件の位置ペアへ置いて報酬を増やします。

**具体的な問題。** 物体ペアの親和度と位置ペアの報酬係数の積に、物体ごとの位置報酬も加えた合計を最大化します。

**入力の表し方。** flowとunaryの符号だけを反転します。distanceまで同時に反転すると積の符号が戻るため、最大化への変換になりません。入力値の最小負数を反転するとあふれる点にも注意します。

ファイル名：`05_maximize.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：相性のよい組合せによる報酬を最大化する。

// 問題：物体ペアの親和度と位置ペアの報酬係数の積に、
// 物体ごとの位置報酬も加えた合計を最大化します。

// 注意：最大化はflowとunaryの符号だけを反転。
// distanceも反転すると二次項の符号が戻ります。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 3物体・3位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(3);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1,  // 物体0から全物体への係数。
        8, 0, 2,  // 物体1から全物体への係数。
        1, 2, 0   // 物体2から全物体への係数。
    };

    // 入力：位置ペアの係数を行ごとに並べる。
    // 自動的な対称化は行われない。
    problem.distance = {
        0, 1, 3,  // 位置0から全位置への係数。
        1, 0, 2,  // 位置1から全位置への係数。
        3, 2, 0   // 位置2から全位置への係数。
    };

// この例では distance の数値を、距離ではなく位置ペアの報酬係数と読む。

    // 入力：物体ごとの位置報酬。
    // 後で符号を反転して最小化費用にする。
    problem.unary = {
        2, 0, 1,
        0, 3, 1,
        1, 0, 4
    };

    // 元の報酬を検算するため問題をコピーする。
    const auto reward_problem = problem;

    // ペア報酬の符号を反転。
    // 最小負数を反転しないこと。
    for (int& value : problem.flow) {
        value = -value;
    }

    // 1物体だけの報酬も符号を反転する。
    for (long long& value : problem.unary) {
        value = -value;
    }

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 初期配置を内部生成して探索。
    // 締切は必須、seedを省略すると1。
    auto result = solve_qap(problem, deadline);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    // 符号反転前の問題で同じ配置を評価し、元の最大化報酬を取り出す。
    const long long reward = reward_problem.evaluate(result.location_of);

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    assert(reward == -result.cost);
    std::cout << "reward = " << reward << '\n';
}
```

**出力の読み方。** 元の符号に戻したrewardを表示します。ライブラリのcostはこのrewardの負値です。

検証時の標準出力の一例：

```text
reward = 61
```


### 3.6 例06：機器より多い候補位置から設置場所を選ぶ

機器は3台ですが、設置できる場所は4つあります。どの場所を空けるかも配置と一緒に選びます。

**具体的な問題。** 位置の座標0、1、3、4から3か所を使い、実機器間の運搬費を最小化します。

**入力の表し方。** ゼロ費用のダミー物体を1つ加え、4物体と4位置の一対一対応にします。ダミーに関するflowの行と列、unaryを0に保ちます。distanceは実際の位置の距離のままです。

ファイル名：`06_dummy.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：機器より多い候補位置から設置場所を選ぶ。

// 問題：位置の座標0、1、3、4から3か所を使い、実機器間の運搬費を最小化します。

// 注意：ダミーに関するflowの行・列とunaryを0に保ちます。
// 実物体の出力は先頭real_objects台です。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    const int real_objects = 3;
    const int slots = 4;

    // 物体数と位置数を等しくして問題を構築。
    // 行列受取り版は入力を所有する。
    qap_problem<> problem(slots);
    const std::vector<int> real_flow{
        0, 8, 1,
        8, 0, 2,
        1, 2, 0
    };

    for (int i = 0; i < real_objects; ++i) {
        for (int j = 0; j < real_objects; ++j) {
            // flow[物体i*n+物体j]の向きで係数を設定・参照する。
            problem.flow[i * slots + j] = real_flow[i * real_objects + j];
        }
    }

    const std::vector<int> coordinate{0, 1, 3, 4};
    for (int x = 0; x < slots; ++x) {
        for (int y = 0; y < slots; ++y) {
            // distance[位置x*n+位置y]の向きで係数を設定・参照する。
            problem.distance[x * slots + y] =
                std::abs(coordinate[x] - coordinate[y]);
        }
    }

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 初期配置を内部生成して探索。
    // 締切は必須、seedを省略すると1。
    auto result = solve_qap(problem, deadline);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    long long direct = 0;
    for (int i = 0; i < real_objects; ++i) {
        for (int j = 0; j < real_objects; ++j) {
            // 返却配列の物体番号から位置番号を取り出す。
            // 配列の向きは物体→位置。
            direct += 1LL * real_flow[i * real_objects + j]
                    * std::abs(coordinate[result.location_of[i]]
                             - coordinate[result.location_of[j]]);
        }
    }

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    assert(direct == result.cost);

    // 返却配列の物体番号から位置番号を取り出す。
    // 配列の向きは物体→位置。
    std::cout << "empty location = " << result.location_of[3] << '\n';

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    std::cout << "cost = " << result.cost << '\n';

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

**出力の読み方。** ダミー物体3の行き先が空き位置です。実機器だけを直接採点した費用とcostの一致も検査します。

検証時の標準出力の一例：

```text
empty location = 3
cost = 30
object 0 -> location 0
object 1 -> location 1
object 2 -> location 2
object 3 -> location 3
```


### 3.7 例07：少数の接続だけを持つ機器をグリッドへ置く

接続のある機器の組が少ない回路や通信網を、2行3列のマス目に置きます。残りのマスは空けます。

**具体的な問題。** 4機器の接続0–1、1–2、2–3の重みを8、2、6とし、辺の重み×縦横移動距離を小さくします。

**入力の表し方。** 辺のリストからflowの対応する2方向を加算します。マス番号xの行はx/width、列はx%widthです。入力配列は疎な接続でもn×nで用意します。

ファイル名：`07_grid_sparse.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：少数の接続だけを持つ機器をグリッドへ置く。

// 問題：4機器の接続0–1、1–2、2–3の重みを8、2、6とし、
// 辺の重み×縦横移動距離を小さくします。

// 注意：非ゼロ辺だけ設定しても入力行列は密なn×nです。
// 無向辺は両方向へ1度ずつ加算します。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    const int height = 2;
    const int width = 3;
    const int real_objects = 4;
    const int slots = height * width;

    // 物体数と位置数を等しくして問題を構築。
    // 行列受取り版は入力を所有する。
    qap_problem<> problem(slots);

    const std::vector<std::tuple<int, int, int>> edges{
        {0, 1, 8},
        {1, 2, 2},
        {2, 3, 6}
    };

    for (auto [i, j, weight] : edges) {
        // flow[物体i*n+物体j]の向きで係数を設定・参照する。
        problem.flow[i * slots + j] += weight;

        // flow[物体i*n+物体j]の向きで係数を設定・参照する。
        problem.flow[j * slots + i] += weight;
    }

    for (int x = 0; x < slots; ++x) {
        for (int y = 0; y < slots; ++y) {
            // distance[位置x*n+位置y]の向きで係数を設定・参照する。
            // 行の差と列の差を足したマンハッタン距離をdistanceへ設定する。
            problem.distance[x * slots + y] =
                std::abs(x / width - y / width)
                + std::abs(x % width - y % width);
        }
    }

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 初期配置を内部生成して探索。
    // 締切は必須、seedを省略すると1。
    auto result = solve_qap(problem, deadline);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    long long undirected_cost = 0;
    for (auto [i, j, weight] : edges) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        undirected_cost +=
            1LL * weight
            * problem.distance[
                result.location_of[i] * slots + result.location_of[j]
            ];
    }

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    assert(result.cost == 2 * undirected_cost);

    for (int i = 0; i < real_objects; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        const int x = result.location_of[i];
        std::cout << i << ": (" << x / width << ", " << x % width << ")\n";
    }
}
```

**出力の読み方。** 実機器の行・列を表示します。costは無向辺を1度ずつ数えた費用の2倍です。

検証時の標準出力の一例：

```text
0: (0, 2)
1: (0, 1)
2: (1, 1)
3: (1, 2)
```


### 3.8 例08：人数が決まったグループへ分ける

一緒にしたい人同士をなるべく同じ班へ入れながら、各班の人数を指定どおりにします。班ごとに人数が違っても構いません。

**具体的な問題。** 4人を定員1人の班0と定員3人の班1に分けます。親和度の高い人同士が別の班になる費用を小さくします。

**入力の表し方。** 位置を班の座席と考え、位置0を班0、位置1〜3を班1とします。同じ班の座席間のdistanceを0、異なる班では1にします。扱っている容量は人数であり、人ごとに違う荷物重量の容量ではありません。

ファイル名：`08_groups.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：人数が決まったグループへ分ける。

// 問題：4人を定員1人の班0と定員3人の班1に分けます。
// 親和度の高い人同士が別の班になる費用を小さくします。

// 注意：位置の数で班の人数を固定します。
// 人ごとに異なる重量を収容する容量制約ではありません。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 4物体・4位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(4);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1, 0,  // 物体0から全物体への係数。
        8, 0, 2, 1,  // 物体1から全物体への係数。
        1, 2, 0, 6,  // 物体2から全物体への係数。
        0, 1, 6, 0   // 物体3から全物体への係数。
    };

    // 入力：位置0は班0、位置1〜3は班1の座席。
    // 人数は1人と3人に固定される。
    const std::vector<int> group_of_slot{0, 1, 1, 1};

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int x = 0; x < problem.n; ++x) {
        // 問題の物体数=位置数nを使う。
        // 添字は0以上n未満。
        for (int y = 0; y < problem.n; ++y) {
            // distance[位置x*n+位置y]の向きで係数を設定・参照する。
            // 同じ班の座席ペアなら0、異なる班なら1をdistanceへ設定する。
            problem.distance[x * problem.n + y] =
                group_of_slot[x] == group_of_slot[y] ? 0 : 1;
        }
    }

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 初期配置を内部生成して探索。
    // 締切は必須、seedを省略すると1。
    auto result = solve_qap(problem, deadline);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    std::vector<int> count(2, 0);

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        const int group = group_of_slot[result.location_of[i]];
        ++count[group];
        std::cout << "person " << i << " -> group " << group << '\n';
    }

    assert(count[0] == 1 && count[1] == 3);
}
```

**出力の読み方。** 各人の班番号を表示し、班人数が1と3であることを確認します。

検証時の標準出力の一例：

```text
person 0 -> group 1
person 1 -> group 1
person 2 -> group 1
person 3 -> group 0
```


### 3.9 例09：都市を一度ずつ巡る順序を決める

複数の都市を1回ずつ訪れて出発都市に戻る巡回順を作ります。各都市から次の都市への費用の合計を減らします。

**具体的な問題。** 4都市を一周します。flowを都市間の移動費、位置を巡回順の0番目〜3番目とします。

**入力の表し方。** distanceは順序xの直後がyであるときだけ1にします。最後から先頭への辺も1です。返却されるのは都市→順序なので、出力時に順序→都市へ逆引きします。

ファイル名：`09_cycle.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：都市を一度ずつ巡る順序を決める。

// 問題：4都市を一周します。
// flowを都市間の移動費、位置を巡回順の0番目〜3番目とします。

// 注意：location_of[都市]は巡回順です。
// 都市列が欲しい場合は逆対応を作ります。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 4物体・4位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(4);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1, 0,  // 物体0から全物体への係数。
        8, 0, 2, 1,  // 物体1から全物体への係数。
        1, 2, 0, 6,  // 物体2から全物体への係数。
        0, 1, 6, 0   // 物体3から全物体への係数。
    };

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int x = 0; x < problem.n; ++x) {
        // distance[位置x*n+位置y]の向きで係数を設定・参照する。
        problem.distance[x * problem.n + (x + 1) % problem.n] = 1;
    }

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 初期配置を内部生成して探索。
    // 締切は必須、seedを省略すると1。
    auto result = solve_qap(problem, deadline);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    std::vector<int> city_at(problem.n);

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int city = 0; city < problem.n; ++city) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        city_at[result.location_of[city]] = city;
    }

    long long route_cost = 0;

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int x = 0; x < problem.n; ++x) {
        const int from = city_at[x];

        // 問題の物体数=位置数nを使う。
        // 添字は0以上n未満。
        const int to = city_at[(x + 1) % problem.n];

        // flow[物体i*n+物体j]の向きで係数を設定・参照する。
        route_cost += problem.flow[from * problem.n + to];
        std::cout << from << ' ';
    }

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    assert(route_cost == result.cost);
    std::cout << city_at[0] << "  cost = " << route_cost << '\n';
}
```

**出力の読み方。** 一周する都市列と巡回費用を表示します。巡回に特化したAPIや訪問回数制約の指定ではなく、行列による表現です。

検証時の標準出力の一例：

```text
0 2 1 3 0  cost = 4
```


### 3.10 例10：候補を選び、その配置も同時に決める

設置候補は4台ありますが、実際に置ける場所は2つです。採用による報酬と、採用した機器同士の干渉を合わせて判断します。

**具体的な問題。** 必ず2台を採用します。採用報酬は12、9、10、8で、干渉費用を差し引いた正味報酬を最大化します。

**入力の表し方。** 候補物体は4つのまま、位置0・1を実位置、2・3を不採用位置とします。不採用位置に関するdistanceは0、実位置へのunaryは採用報酬の負値です。

ファイル名：`10_selection.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：候補を選び、その配置も同時に決める。

// 問題：必ず2台を採用します。
// 採用報酬は12、9、10、8で、干渉費用を差し引いた正味報酬を最大化します。

// 注意：位置real_slots以降は不採用を表します。
// その位置に関するdistanceは0のままにします。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    const int candidates = 4;
    const int real_slots = 2;

    // 物体数と位置数を等しくして問題を構築。
    // 行列受取り版は入力を所有する。
    qap_problem<> problem(candidates);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 3, 2, 4,
        3, 0, 1, 2,
        2, 1, 0, 5,
        4, 2, 5, 0
    };

    // distance[位置x*n+位置y]の向きで係数を設定・参照する。
    problem.distance[0 * candidates + 1] = 1;

    // distance[位置x*n+位置y]の向きで係数を設定・参照する。
    problem.distance[1 * candidates + 0] = 1;
    const std::vector<long long> reward{12, 9, 10, 8};

    // 単項費用を使うためn×n個の領域を確保し、指定値で埋める。
    problem.unary.assign(candidates * candidates, 0);
    for (int i = 0; i < candidates; ++i) {
        for (int x = 0; x < real_slots; ++x) {
            // unary[物体i*n+位置x]へ、その1物体と位置だけで決まる費用を設定する。
            problem.unary[i * candidates + x] = -reward[i];
        }
    }

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 初期配置を内部生成して探索。
    // 締切は必須、seedを省略すると1。
    auto result = solve_qap(problem, deadline);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    std::vector<int> selected;
    long long selected_reward = 0;
    for (int i = 0; i < candidates; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        if (result.location_of[i] < real_slots) {
            selected.push_back(i);
            selected_reward += reward[i];
            std::cout << "selected = " << i << '\n';
        }
    }

    assert(selected.size() == 2);
    const int a = selected[0], b = selected[1];

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    // flow[物体i*n+物体j]の向きで係数を設定・参照する。
    assert(result.cost == problem.flow[a * candidates + b]
                        + problem.flow[b * candidates + a] - selected_reward);

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    std::cout << "net reward = " << -result.cost << '\n';
}
```

**出力の読み方。** 採用候補と正味報酬を表示します。不採用位置は機器が物理的に移動する場所ではありません。

検証時の標準出力の一例：

```text
selected = 0
selected = 2
net reward = 18
```


### 3.11 例11：似たグラフの頂点を対応付ける

同じ数の頂点を持つ2つのネットワークについて、どの頂点同士が対応するかを調べます。番号を付け替えた後の辺重みが近い対応を選びます。

**具体的な問題。** 3頂点ずつのグラフA、Bについて、対応する全向き付き辺の重みの差の二乗和を小さくします。

**入力の表し方。** (a−b)²=a²+b²−2abを使い、flow=−2A、distance=Bとします。一対一対応ではA²とB²の全要素の和は配置によらないので、最後にその定数を戻します。

ファイル名：`11_graph_matching.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：似たグラフの頂点を対応付ける。

// 問題：3頂点ずつのグラフA、Bについて、対応する全向き付き辺の重みの差の二乗和を小さくし
// ます。

// 注意：QAP costへ定数constantを加えて二乗誤差に戻します。
// 大きい辺重みは型を広げてから乗算します。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    const int n = 3;
    const std::vector<int> a{
        0, 8, 1,
        8, 0, 2,
        1, 2, 0
    };

    const std::vector<int> b{
        0, 1, 2,
        1, 0, 8,
        2, 8, 0
    };

    // 物体数と位置数を等しくして問題を構築。
    // 行列受取り版は入力を所有する。
    qap_problem<> problem(n);
    long long constant = 0;
    for (int t = 0; t < n * n; ++t) {
        // flow[物体i*n+物体j]の向きで係数を設定・参照する。
        problem.flow[t] = -2 * a[t];

        // distance[位置x*n+位置y]の向きで係数を設定・参照する。
        problem.distance[t] = b[t];
        constant += 1LL * a[t] * a[t] + 1LL * b[t] * b[t];
    }

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 初期配置を内部生成して探索。
    // 締切は必須、seedを省略すると1。
    auto result = solve_qap(problem, deadline);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    long long squared_error = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            // 返却配列の物体番号から位置番号を取り出す。
            // 配列の向きは物体→位置。
            const long long difference = a[i * n + j]
                - b[result.location_of[i] * n + result.location_of[j]];
            squared_error += difference * difference;
        }
    }

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    assert(squared_error == constant + result.cost);
    std::cout << "squared error = " << squared_error << '\n';
}
```

**出力の読み方。** 二乗誤差を表示します。負になることのあるQAP costを、二乗誤差そのものと読み違えないようにします。

検証時の標準出力の一例：

```text
squared error = 0
```


### 3.12 例12：既に持っている配置を改善する

人が作った配置や別のソルバーの解を出発点とし、その費用以下の配置を探します。

**具体的な問題。** 3台の開始配置を0、2、1とし、同じ問題の目的関数でこれより悪くない配置を返します。

**入力の表し方。** improve_qapのinitialへ完全な置換を渡します。seed=42は乱数の開始値です。改善が見つからなければ開始解が保持されます。

ファイル名：`12_improve.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：既に持っている配置を改善する。

// 問題：3台の開始配置を0、2、1とし、同じ問題の目的関数でこれより悪くない配置を返しま
// す。

// 注意：initialは重複のない完全配置を渡します。
// 非悪化の保証は同じ入力の目的関数についてです。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 3物体・3位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(3);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1,  // 物体0から全物体への係数。
        8, 0, 2,  // 物体1から全物体への係数。
        1, 2, 0   // 物体2から全物体への係数。
    };

    // 入力：位置ペアの係数を行ごとに並べる。
    // 自動的な対称化は行われない。
    problem.distance = {
        0, 1, 3,  // 位置0から全位置への係数。
        1, 0, 2,  // 位置1から全位置への係数。
        3, 2, 0   // 位置2から全位置への係数。
    };

    // 入力：物体0→位置0、物体1→位置2、物体2→位置1。
    const std::vector<int> initial{0, 2, 1};

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(initial));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    const long long initial_cost = problem.evaluate(initial);

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 乱数の開始値42。
    // 時刻打切りがあるため完全な再現を保証する指定ではない。
    const std::uint64_t seed = 42;

    // 初期配置から改善。
    // 引数は問題、初期配置、絶対締切、必要ならseed。
    auto result = improve_qap(
        problem,
        initial,
        deadline,
        seed
    );

    // 同じ問題で開始解より費用が悪化していないことを検査する。
    assert(result.cost <= initial_cost);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    std::cout << "cost = " << result.cost << '\n';

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

**出力の読み方。** 開始費用以下のcostと配置を表示します。一般の最適解である保証はありません。

検証時の標準出力の一例：

```text
cost = 30
object 0 -> location 0
object 1 -> location 1
object 2 -> location 2
```


### 3.13 例13：同じ問題を複数回試して最良候補を磨く

入力が変わらない問題を異なるseedで繰り返し解き、得られた最良配置をさらに改善します。毎回使える前処理を再利用します。

**具体的な問題。** 最大3回の新規探索を行い、最良候補を最後にimproveします。全体の探索目安150ms、各新規試行40msとします。

**入力の表し方。** preparedはproblemへの参照を持ちます。利用中はproblemを変更・破棄しません。各試行の締切は全体締切と40ms後の早い方にします。

ファイル名：`13_prepared.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：同じ問題を複数回試して最良候補を磨く。

// 問題：最大3回の新規探索を行い、最良候補を最後にimproveします。
// 全体の探索目安150ms、各新規試行40msとします。

// 注意：preparedはproblemを借ります。
// 利用中にproblemを変更・移動・破棄しないこと。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    std::vector<int> flow{
        0, 8, 1,
        8, 0, 2,
        1, 2, 0
    };

    std::vector<int> distance{
        0, 1, 3,
        1, 0, 2,
        3, 2, 0
    };

    // n=3と2行列を渡す。
    // moveで配列を移し、単項費用は省略して空にする。
    qap_problem<> problem(3, std::move(flow), std::move(distance));

    // problemを借りて前処理。
    // preparedの利用中はproblemを変更・破棄しない。
    qap_prepared<> prepared(problem);

    // 入力：物体0→位置0、物体1→位置2、物体2→位置1。
    const std::vector<int> initial{0, 2, 1};

    // 参照を返すAPI。
    // preparedはこのproblemを借りており、コピーは持たない。
    assert(&prepared.problem() == &problem);

    // 前処理付きの差分と通常の差分が一致することを検査する。
    assert(
        prepared.swap_delta(initial, 0, 1)
        == problem.swap_delta(initial, 0, 1)
    );

    // 初期配置と、それを同じ問題で評価した費用を結果型にまとめる。
    qap_result<> best{initial, prepared.evaluate(initial)};

    using Clock = std::chrono::steady_clock;

    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto final_deadline =
        Clock::now()
        + std::chrono::milliseconds(150);

    // seed=1、2、3の最大3試行。
    // 全試行で同じ問題・前処理を使う。
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        if (Clock::now() >= final_deadline) {
            break;
        }

        // 全体の終了目安と1試行の終了目安のうち早い方を選ぶ。
        // ミリ秒の時間枠を加える。
        // 締切は厳密な停止時刻ではなく目安。
        const auto deadline = std::min(
            final_deadline,
            Clock::now() + std::chrono::milliseconds(40)
        );

        // 前処理を再利用して新しい候補を生成。
        // 引数は絶対締切とseed。
        auto candidate = prepared.solve(deadline, seed);

        // 同じ問題の費用を比較し、小さい候補だけを保存する。
        if (candidate.cost < best.cost) {
            best = std::move(candidate);
        }
    }

    // 最良候補から全体締切まで改善。
    // 第3引数99はこの試行のseed。
    auto result = prepared.improve(best.location_of, final_deadline, 99);

    // 同じ問題で開始解より費用が悪化していないことを検査する。
    assert(result.cost <= best.cost);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    std::cout << "cost = " << result.cost << '\n';

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

**出力の読み方。** 複数候補を比較した後の結果を表示します。計画した3回すべてが時間内に終わることを仮定しません。

検証時の標準出力の一例：

```text
cost = 30
object 0 -> location 0
object 1 -> location 1
object 2 -> location 2
```


### 3.14 例14：固定した設備を動かさずに一部だけ調整する

稼働中のため動かせない機器を固定し、止められる機器だけを移動して配置を改善します。

**具体的な問題。** 4台のうち物体0と3は現在の位置に固定し、物体1と2だけを互いが現在占めている位置の中で並べ替えます。

**入力の表し方。** movable_objectsは物体番号の重複なしの一覧です。移動先の一覧ではありません。固定した物体の位置が自由に使えるわけでもありません。

ファイル名：`14_subset.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：固定した設備を動かさずに一部だけ調整する。

// 問題：4台のうち物体0と3は現在の位置に固定し、
// 物体1と2だけを互いが現在占めている位置の中で並べ替えます。

// 注意：movable_objectsは物体番号。
// 移動先は、その物体たちが現在使っている位置集合の中だけです。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 4物体・4位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(4);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1, 0,  // 物体0から全物体への係数。
        8, 0, 2, 1,  // 物体1から全物体への係数。
        1, 2, 0, 6,  // 物体2から全物体への係数。
        0, 1, 6, 0   // 物体3から全物体への係数。
    };

    // 入力：位置ペアの係数を行ごとに並べる。
    // 自動的な対称化は行われない。
    problem.distance = {
        0, 1, 3, 4,  // 位置0から全位置への係数。
        1, 0, 2, 3,  // 位置1から全位置への係数。
        3, 2, 0, 1,  // 位置2から全位置への係数。
        4, 3, 1, 0   // 位置3から全位置への係数。
    };

    // 入力：4物体の完全配置。
    // 各位置0〜3を1回ずつ使う。
    const std::vector<int> initial{0, 2, 1, 3};

    // 入力：動かす物体は1と2。
    // 位置番号の指定ではない。
    const std::vector<int> movable_objects{1, 2};

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 指定した物体だけを現在の位置集合内で改善し、完全配置を返す。
    auto result = improve_qap_subset(
        problem,
        initial,
        movable_objects,
        deadline,
        42
    );

    // 返却配列の物体番号から位置番号を取り出す。
    // 配列の向きは物体→位置。
    assert(result.location_of[0] == initial[0]);

    // 返却配列の物体番号から位置番号を取り出す。
    // 配列の向きは物体→位置。
    assert(result.location_of[3] == initial[3]);

    // 同じ問題で開始解より費用が悪化していないことを検査する。
    assert(result.cost <= problem.evaluate(initial));

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    std::cout << "cost = " << result.cost << '\n';

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

**出力の読み方。** 完全な4台分の配置を表示し、固定した2台の位置が変わっていないことを検査します。

検証時の標準出力の一例：

```text
cost = 48
object 0 -> location 0
object 1 -> location 1
object 2 -> location 2
object 3 -> location 3
```


### 3.15 例15：小さい可動部分を全通り調べて最善にする

全体の問題は大きくても、今回は数台だけを動かせばよい場面があります。その小部分の並べ方を全て比べます。

**具体的な問題。** 物体0を固定し、可動物体2、1、3の3台だけを並べ替えます。3台の並べ方6通りを全列挙し、その固定条件下で最も費用の小さいものを選びます。

**入力の表し方。** make_qap_subproblemで固定物体との相互作用も含む小問題を作ります。全列挙はこの例の利用側コードです。restoreは縮約のcostへconstant_offsetを加えて完全配置に戻します。

ファイル名：`15_subproblem_exact.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：小さい可動部分を全通り調べて最善にする。

// 問題：物体0を固定し、可動物体2、1、3の3台だけを並べ替えます。
// 3台の並べ方6通りを全列挙し、その固定条件下で最も費用の小さいものを選びます。

// 注意：全列挙は3台なので6通りです。
// 可動台数を増やすと並べ方が急増するため、小部分用です。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 4物体・4位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(4);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1, 0,  // 物体0から全物体への係数。
        8, 0, 2, 1,  // 物体1から全物体への係数。
        1, 2, 0, 6,  // 物体2から全物体への係数。
        0, 1, 6, 0   // 物体3から全物体への係数。
    };

    // 入力：位置ペアの係数を行ごとに並べる。
    // 自動的な対称化は行われない。
    problem.distance = {
        0, 1, 3, 4,  // 位置0から全位置への係数。
        1, 0, 2, 3,  // 位置1から全位置への係数。
        3, 2, 0, 1,  // 位置2から全位置への係数。
        4, 3, 1, 0   // 位置3から全位置への係数。
    };

    // 入力：4物体の完全配置。
    // 各位置0〜3を1回ずつ使う。
    const std::vector<int> initial{0, 2, 1, 3};

    // 入力：物体2、1、3だけを動かす。
    // 縮約番号はこの順序になる。
    const std::vector<int> movable_objects{2, 1, 3};

    // 可動物体だけの小問題を作る。
    // 固定物体との相互作用も単項費用へ反映する。
    auto sub = make_qap_subproblem(problem, initial, movable_objects);

    // 可動物体だけの小問題を作る。
    // 固定物体との相互作用も単項費用へ反映する。
    // 第4引数は元の完全配置の正しい費用。
    // 渡さなければ内部で評価される。
    auto known_sub = make_qap_subproblem(
        problem,
        initial,
        movable_objects,
        problem.evaluate(initial)
    );

    // 固定部分などの定数。
    // 完全費用=constant_offset+縮約費用を確認する。
    assert(sub.constant_offset == known_sub.constant_offset);

    // 復元の土台となる完全配置。
    // 可動集合外はこの位置に固定される。
    assert(sub.base_location_of == initial);

    // 縮約物体の初期配置。
    // 生成直後は0、1、…、k−1で、元の開始配置に対応する。
    std::vector<int> q = sub.initial_location_of;

    // 初期配置と、それを同じ問題で評価した費用を結果型にまとめる。
    qap_result<> best{q, sub.problem.evaluate(q)};
    int examined = 0;

    do {
        // 今の縮約配置qを、小問題の目的関数で正確に採点する。
        const long long cost = sub.problem.evaluate(q);

        // 同じ配置に対応するQAP費用を読む。
        // 値が小さいほど良い。
        if (cost < best.cost) {
            best = {q, cost};
        }

        // 縮約配置を元の物体・位置へ戻し、縮約costへconstant_offsetを加える。
        const auto restored = sub.restore(qap_result<>{q, cost});

        // 固定部分などの定数。
        // 完全費用=constant_offset+縮約費用を確認する。
        assert(
            problem.evaluate(restored.location_of)
            == sub.constant_offset + cost
        );
        ++examined;
    } while (std::next_permutation(q.begin(), q.end()));

    // 縮約配置を元の物体・位置へ戻し、縮約costへconstant_offsetを加える。
    auto result = sub.restore(best);
    assert(examined == 6);

    // 返却配列の物体番号から位置番号を取り出す。
    // 配列の向きは物体→位置。
    assert(result.location_of[0] == initial[0]);

    // 縮約問題の物体数kだけ、対応関係を調べる。
    for (int a = 0; a < sub.problem.n; ++a) {
        // 縮約物体a→元の物体、縮約位置→元の位置という2つの対応で検査する。
        assert(
            result.location_of[sub.object[a]]
            == sub.location[best.location_of[a]]
        );
    }

    // 同じ問題で開始解より費用が悪化していないことを検査する。
    assert(result.cost <= problem.evaluate(initial));

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    std::cout << "cost = " << result.cost << '\n';

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

**出力の読み方。** 復元した完全配置を表示します。最適なのは指定した可動集合の中だけで、全体配置の最適性ではありません。

検証時の標準出力の一例：

```text
cost = 48
object 0 -> location 0
object 1 -> location 1
object 2 -> location 2
object 3 -> location 3
```


### 3.16 例16：動かす範囲を変えながら部分改善を繰り返す

一度に全体を動かす代わりに、隣り合う作業領域を順に見直します。ある回の結果を次の回へ引き継ぎます。

**具体的な問題。** 可動集合を{0,1}、{1,2}、{2,3}の順で切り替えます。各回は25ms、全体は90msを目安とします。

**入力の表し方。** 毎回のresult.costは同じ問題・その時点のresult.location_ofの正しい費用なので、既知cost版へそのまま渡せます。入力行列を変更したらこの引継ぎは使えません。

ファイル名：`16_repeat_subset.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：動かす範囲を変えながら部分改善を繰り返す。

// 問題：可動集合を{0,1}、{1,2}、{2,3}の順で切り替えます。
// 各回は25ms、全体は90msを目安とします。

// 注意：既知costは同じ問題・同じ配置の値を渡します。
// 各回の時間枠は全体締切を超えないようにします。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 4物体・4位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(4);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1, 0,  // 物体0から全物体への係数。
        8, 0, 2, 1,  // 物体1から全物体への係数。
        1, 2, 0, 6,  // 物体2から全物体への係数。
        0, 1, 6, 0   // 物体3から全物体への係数。
    };

    // 入力：位置ペアの係数を行ごとに並べる。
    // 自動的な対称化は行われない。
    problem.distance = {
        0, 1, 3, 4,  // 位置0から全位置への係数。
        1, 0, 2, 3,  // 位置1から全位置への係数。
        3, 2, 0, 1,  // 位置2から全位置への係数。
        4, 3, 1, 0   // 位置3から全位置への係数。
    };

    // 入力：4物体の完全配置。
    // 各位置0〜3を1回ずつ使う。
    const std::vector<int> initial{0, 2, 1, 3};

    // 配置と正しいQAP費用を対応させて結果型へ保存する。
    qap_result<> result{initial, problem.evaluate(initial)};

    // 入力：各回に動かす物体番号の集合。
    // 回をまたいだ重複は構わない。
    const std::vector<std::vector<int>> groups{
        {0, 1},
        {1, 2},
        {2, 3}
    };

    using Clock = std::chrono::steady_clock;

    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto final_deadline =
        Clock::now()
        + std::chrono::milliseconds(90);

    for (std::size_t round = 0; round < groups.size(); ++round) {
        if (Clock::now() >= final_deadline) {
            break;
        }

        // 全体の終了目安と1試行の終了目安のうち早い方を選ぶ。
        // ミリ秒の時間枠を加える。
        // 締切は厳密な停止時刻ではなく目安。
        const auto deadline = std::min(
            final_deadline,
            Clock::now() + std::chrono::milliseconds(25)
        );

        // 非悪化と固定条件を確認するため、今回の開始結果を保存する。
        const auto before = result;

        // 指定した物体だけを現在の位置集合内で改善し、完全配置を返す。
        // 同じ配置の既知cost、今回の締切、回ごとのseedを渡す。
        result = improve_qap_subset(
            problem,
            result.location_of,
            groups[round],
            result.cost,
            deadline,
            static_cast<std::uint64_t>(round + 1)
        );

        // 同じ問題で開始解より費用が悪化していないことを検査する。
        assert(result.cost <= before.cost);

        // 問題の物体数=位置数nを使う。
        // 添字は0以上n未満。
        for (int i = 0; i < problem.n; ++i) {
            if (
                std::find(groups[round].begin(), groups[round].end(), i)
                == groups[round].end()
            ) {
                // 返却配列の物体番号から位置番号を取り出す。
                // 配列の向きは物体→位置。
                assert(result.location_of[i] == before.location_of[i]);
            }
        }
    }

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    std::cout << "cost = " << result.cost << '\n';

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

**出力の読み方。** 最終配置を表示し、各回の費用が悪化しないことと、その回の可動集合外が固定されることを確認します。

検証時の標準出力の一例：

```text
cost = 48
object 0 -> location 0
object 1 -> location 1
object 2 -> location 2
object 3 -> location 3
```


### 3.17 例17：交換を選ぶ独自の探索を作る

探索の受理ルールを自分で書きたいとき、交換差分と現在費用の管理だけをライブラリに任せます。

**具体的な問題。** 3台について費用が厳密に下がる交換を受理し、改善交換がなくなるか締切に達するまで繰り返します。

**入力の表し方。** swap_stateは現在配置とcostを一緒に持ちます。swap_deltaで見積もり、apply_swapで実際に更新します。apply_swapは内部でも差分を計算します。

ファイル名：`17_swap_state.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：交換を選ぶ独自の探索を作る。

// 問題：3台について費用が厳密に下がる交換を受理し、
// 改善交換がなくなるか締切に達するまで繰り返します。

// 注意：problem→prepared→stateの順に寿命を保ちます。
// apply_swapの返り値は新costではなく差分です。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 3物体・3位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(3);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1,  // 物体0から全物体への係数。
        8, 0, 2,  // 物体1から全物体への係数。
        1, 2, 0   // 物体2から全物体への係数。
    };

    // 入力：位置ペアの係数を行ごとに並べる。
    // 自動的な対称化は行われない。
    problem.distance = {
        0, 1, 3,  // 位置0から全位置への係数。
        1, 0, 2,  // 位置1から全位置への係数。
        3, 2, 0   // 位置2から全位置への係数。
    };

    // 入力：物体0→位置0、物体1→位置2、物体2→位置1。
    const std::vector<int> initial{0, 2, 1};

    // problemを借りて前処理。
    // preparedの利用中はproblemを変更・破棄しない。
    qap_prepared<> prepared(problem);

    // 初期配置を所有し、preparedで全評価したcostも保持する。
    qap_swap_state<> state(prepared, initial);

    // 同じ初期配置の正しい既知costを渡し、全評価を省いて状態を作る。
    qap_swap_state<> known_state(prepared, initial, state.cost());

    // 保持中のQAP費用を読む。
    // 利用側で追加した費用は含まれない。
    assert(known_state.cost() == state.cost());

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 改善があり、締切より前の間だけ探索を続ける。
    bool changed;
    do {
        changed = false;

        // 問題の物体数=位置数nを使う。
        // 添字は0以上n未満。
        for (
            int i = 0;
            i < problem.n && std::chrono::steady_clock::now() < deadline;
            ++i
        ) {
            // 問題の物体数=位置数nを使う。
            // 添字は0以上n未満。
            for (int j = i + 1; j < problem.n; ++j) {
                // QAP費用の交換後−交換前を計算。
                // ここではまだ配置を変更しない。
                const long long delta = state.swap_delta(i, j);

                if (delta < 0) {
                    // 物体iとjの位置を交換し、保持costも更新する。
                    // 返り値は交換後−交換前。
                    const long long applied = state.apply_swap(i, j);
                    assert(applied == delta);

                    // この配置の費用を全再計算する。
                    // 入力更新後や検算にも使う。
                    assert(
                        state.cost() == problem.evaluate(state.location_of())
                    );
                    changed = true;
                }
            }
        }
    } while (changed && std::chrono::steady_clock::now() < deadline);

    // 配置と正しいQAP費用を対応させて結果型へ保存する。
    qap_result<> result{state.location_of(), state.cost()};

    // 同じ問題で開始解より費用が悪化していないことを検査する。
    assert(result.cost <= problem.evaluate(initial));

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    std::cout << "cost = " << result.cost << '\n';

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

**出力の読み方。** 最終状態をqap_resultへコピーして表示します。ローカルな改善であり、全体の最適性を保証しません。

検証時の標準出力の一例：

```text
cost = 30
object 0 -> location 0
object 1 -> location 1
object 2 -> location 2
```


### 3.18 例18：3台以上の関係で決まる追加費用も扱う

機器のペアだけでは表しにくい追加条件があるとき、QAPの費用と自分で評価する費用を足して探索します。

**具体的な問題。** 3台がp[0]<p[1]<p[2]の順に並ぶと混雑費20を加えます。QAP費用と混雑費の合計が下がる交換を採用します。

**入力の表し方。** 候補配置をコピーして混雑費を計算し、QAPのswap差分と合算します。state.cost()に混雑費は含まれません。採用時に追加費用も同期します。

ファイル名：`18_extra_objective.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：3台以上の関係で決まる追加費用も扱う。

// 問題：3台がp[0]<p[1]<p[2]の順に並ぶと混雑費20を加えます。
// QAP費用と混雑費の合計が下がる交換を採用します。

// 注意：state.cost()はQAP部分だけです。
// 採用時には利用側の追加費用も同じ配置へ更新します。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 4物体・4位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(4);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1, 0,  // 物体0から全物体への係数。
        8, 0, 2, 1,  // 物体1から全物体への係数。
        1, 2, 0, 6,  // 物体2から全物体への係数。
        0, 1, 6, 0   // 物体3から全物体への係数。
    };

    // 入力：位置ペアの係数を行ごとに並べる。
    // 自動的な対称化は行われない。
    problem.distance = {
        0, 1, 3, 4,  // 位置0から全位置への係数。
        1, 0, 2, 3,  // 位置1から全位置への係数。
        3, 2, 0, 1,  // 位置2から全位置への係数。
        4, 3, 1, 0   // 位置3から全位置への係数。
    };

    // 入力：4物体の完全配置。
    // 各位置0〜3を1回ずつ使う。
    const std::vector<int> initial{0, 2, 1, 3};

    // problemを借りて前処理。
    // preparedの利用中はproblemを変更・破棄しない。
    qap_prepared<> prepared(problem);

    // 初期配置を所有し、preparedで全評価したcostも保持する。
    qap_swap_state<> state(prepared, initial);

    const long long congestion = 20;
    const auto extra = [congestion](const std::vector<int>& p) -> long long {
        return p[0] < p[1] && p[1] < p[2] ? congestion : 0LL;
    };

    long long extra_cost = extra(initial);

    // 開始時のQAP費用と追加費用を足し、本来の合計費用を保存する。
    const long long initial_total = state.cost() + extra_cost;

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 改善があり、締切より前の間だけ探索を続ける。
    bool changed;
    do {
        changed = false;

        // 問題の物体数=位置数nを使う。
        // 添字は0以上n未満。
        for (
            int i = 0;
            i < problem.n && std::chrono::steady_clock::now() < deadline;
            ++i
        ) {
            // 問題の物体数=位置数nを使う。
            // 添字は0以上n未満。
            for (int j = i + 1; j < problem.n; ++j) {
                // 参照の返り値をautoでコピーし、現在状態を壊さず候補を作る。
                auto candidate = state.location_of();
                std::swap(candidate[i], candidate[j]);
                const long long next_extra = extra(candidate);

                // QAP差分と追加費用の変化を足し、合計費用の交換後−交換前を求める。
                const long long delta =
                    state.swap_delta(i, j) + next_extra - extra_cost;

                if (delta < 0) {
                    // 物体iとjの位置を交換し、保持costも更新する。
                    // 返り値は交換後−交換前。
                    state.apply_swap(i, j);
                    extra_cost = next_extra;
                    changed = true;
                }
            }
        }
    } while (changed && std::chrono::steady_clock::now() < deadline);

    // 現在の物体→位置を読むconst参照。
    // 保持状態そのものは直接変更しない。
    assert(extra_cost == extra(state.location_of()));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(state.cost() == problem.evaluate(state.location_of()));

    // QAP費用と追加費用の合計が開始時より悪化していないことを検査する。
    assert(state.cost() + extra_cost <= initial_total);

    // QAP費用と追加費用を足した、本来の合計費用を表示する。
    std::cout << "total = " << state.cost() + extra_cost << '\n';
}
```

**出力の読み方。** 追加費用込みのtotalを表示します。QAP cost単体の非悪化を要求しているわけではありません。

検証時の標準出力の一例：

```text
total = 48
```


### 3.19 例19：QAPで候補を作り、本当の目的で採否を決める

本来の評価が複雑な場合に、簡単なQAPで改善候補を作り、本来の評価値を計算してから採用します。

**具体的な問題。** 本来の費用はQAP費用に、p[0]<p[1]<p[2]の場合の20を加えた値です。QAPで提案した候補が本来の費用も下げる場合だけ採用します。

**入力の表し方。** 候補生成の評価と最終採否の評価を分けて保持します。QAP費用が良くても、本来の目的が悪くなれば候補を捨てます。

ファイル名：`19_surrogate.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：QAPで候補を作り、本当の目的で採否を決める。

// 問題：本来の費用はQAP費用に、p[0]<p[1]<p[2]の場合の20を加えた値です。
// QAPで提案した候補が本来の費用も下げる場合だけ採用します。

// 注意：result.costはQAPの費用です。
// 本来の費用は別変数で持ち、採用条件も自分で確認します。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 4物体・4位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(4);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1, 0,  // 物体0から全物体への係数。
        8, 0, 2, 1,  // 物体1から全物体への係数。
        1, 2, 0, 6,  // 物体2から全物体への係数。
        0, 1, 6, 0   // 物体3から全物体への係数。
    };

    // 入力：位置ペアの係数を行ごとに並べる。
    // 自動的な対称化は行われない。
    problem.distance = {
        0, 1, 3, 4,  // 位置0から全位置への係数。
        1, 0, 2, 3,  // 位置1から全位置への係数。
        3, 2, 0, 1,  // 位置2から全位置への係数。
        4, 3, 1, 0   // 位置3から全位置への係数。
    };

    // 入力：4物体の完全配置。
    // 各位置0〜3を1回ずつ使う。
    const std::vector<int> initial{0, 2, 1, 3};
    const long long congestion = 20;
    const auto true_cost =
        [&problem, congestion](const std::vector<int>& p) -> long long {
            const long long extra = p[0] < p[1] && p[1] < p[2] ? congestion : 0LL;

            // この配置の費用を全再計算する。
            // 入力更新後や検算にも使う。
            return problem.evaluate(p) + extra;
        };

    // 配置と正しいQAP費用を対応させて結果型へ保存する。
    qap_result<> result{initial, problem.evaluate(initial)};
    long long accepted_true_cost = true_cost(initial);

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 初期配置から改善。
    // 引数は問題、初期配置、絶対締切、必要ならseed。
    auto candidate = improve_qap(
        problem,
        result.location_of,
        deadline,
        42
    );

    // 候補の配置を、本来の目的関数でも評価する。
    const long long candidate_true_cost = true_cost(candidate.location_of);
    if (candidate_true_cost < accepted_true_cost) {
        // 採用した候補の配置とQAP費用を一緒に保存する。
        result = std::move(candidate);
        accepted_true_cost = candidate_true_cost;
    }

    assert(accepted_true_cost <= true_cost(initial));

    // 結果の完全配置を読み、対応する条件を検査する。
    assert(accepted_true_cost == true_cost(result.location_of));

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    std::cout << "QAP cost = " << result.cost
              << ", true cost = " << accepted_true_cost << '\n';
}
```

**出力の読み方。** QAP costとtrue costを別々に表示します。本来の目的で開始解より悪化しないことを利用側で確認します。

検証時の標準出力の一例：

```text
QAP cost = 48, true cost = 68
```


### 3.20 例20：運搬量が変わった後に再計画する

稼働状況が変わって通信量や運搬量が更新されたとき、前回の配置を引き継いで再び改善します。

**具体的な問題。** 最初の配置を改善した後、機器0と2の間の運搬量を両方向20へ変更し、その変更後の目的関数で再改善します。

**入力の表し方。** 古いpreparedを破棄してから入力を更新し、preparedを作り直します。旧result.costは更新後の費用ではないため、更新後の行列で再評価します。

ファイル名：`20_input_update.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：運搬量が変わった後に再計画する。

// 問題：最初の配置を改善した後、機器0と2の間の運搬量を両方向20へ変更し、
// その変更後の目的関数で再改善します。

// 注意：入力を変えたらpreparedは作り直します。
// 変更前のcostを既知costとして再利用しないこと。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 3物体・3位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(3);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1,  // 物体0から全物体への係数。
        8, 0, 2,  // 物体1から全物体への係数。
        1, 2, 0   // 物体2から全物体への係数。
    };

    // 入力：位置ペアの係数を行ごとに並べる。
    // 自動的な対称化は行われない。
    problem.distance = {
        0, 1, 3,  // 位置0から全位置への係数。
        1, 0, 2,  // 位置1から全位置への係数。
        3, 2, 0   // 位置2から全位置への係数。
    };

    // 入力：物体0→位置0、物体1→位置2、物体2→位置1。
    const std::vector<int> initial{0, 2, 1};

    // 空配置・cost=0の結果を用意。
    // 非空問題の解ではないので後で代入する。
    qap_result<> result;
    {
        // problemを借りて前処理。
        // preparedの利用中はproblemを変更・破棄しない。
        qap_prepared<> prepared(problem);

        // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
        // ミリ秒の時間枠を加える。
        // 締切は厳密な停止時刻ではなく目安。
        const auto deadline =
            std::chrono::steady_clock::now()
            + std::chrono::milliseconds(30);

        // 渡した開始配置を再評価し、同じ問題でその費用以下の結果を返す。
        result = prepared.improve(initial, deadline, 1);
    }

    // flow[物体i*n+物体j]の向きで係数を設定・参照する。
    problem.flow[0 * 3 + 2] = 20;

    // flow[物体i*n+物体j]の向きで係数を設定・参照する。
    problem.flow[2 * 3 + 0] = 20;

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    const long long baseline = problem.evaluate(result.location_of);
    {
        // problemを借りて前処理。
        // preparedの利用中はproblemを変更・破棄しない。
        qap_prepared<> prepared(problem);

        // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
        // ミリ秒の時間枠を加える。
        // 締切は厳密な停止時刻ではなく目安。
        const auto deadline =
            std::chrono::steady_clock::now()
            + std::chrono::milliseconds(30);

        // 渡した開始配置を再評価し、同じ問題でその費用以下の結果を返す。
        result = prepared.improve(result.location_of, deadline, 2);
    }

    // 同じ問題で開始解より費用が悪化していないことを検査する。
    assert(result.cost <= baseline);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    std::cout << "cost = " << result.cost << '\n';

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

**出力の読み方。** 更新後のcostと配置を表示します。比較対象は旧costではなく、旧配置を新しい入力で評価した費用です。

検証時の標準出力の一例：

```text
cost = 84
object 0 -> location 1
object 1 -> location 2
object 2 -> location 0
```


### 3.21 例21：配置禁止を罰点に変え、採用前に検査する

置いてはいけない場所がある場合、違反へ大きな費用を付けて候補を探し、最後に禁止条件を別途確認します。

**具体的な問題。** 3台それぞれに許可する位置を表で指定し、禁止位置へ置くと罰点1000を加えます。既知の実行可能配置0、1、2を保険として保持します。

**入力の表し方。** 内蔵APIは配置禁止を直接保証しません。違反0かつ費用が改善した候補だけを採用し、そうでなければ元の実行可能解を残します。

ファイル名：`21_penalty.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：配置禁止を罰点に変え、採用前に検査する。

// 問題：3台それぞれに許可する位置を表で指定し、禁止位置へ置くと罰点1000を加えます。
// 既知の実行可能配置0、1、2を保険として保持します。

// 注意：罰点だけで禁止配置の解消は保証されません。
// 既知実行可能解を保持し、候補を検査します。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 3物体・3位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(3);

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, 8, 1,  // 物体0から全物体への係数。
        8, 0, 2,  // 物体1から全物体への係数。
        1, 2, 0   // 物体2から全物体への係数。
    };

    // 入力：位置ペアの係数を行ごとに並べる。
    // 自動的な対称化は行われない。
    problem.distance = {
        0, 1, 3,  // 位置0から全位置への係数。
        1, 0, 2,  // 位置1から全位置への係数。
        3, 2, 0   // 位置2から全位置への係数。
    };

    const std::vector<unsigned char> allowed{
        1, 1, 0,
        0, 1, 1,
        1, 0, 1
    };

    const long long penalty = 1000;

    // 単項費用を使うためn×n個の領域を確保し、指定値で埋める。
    problem.unary.assign(3 * 3, 0);
    for (int i = 0; i < 3; ++i) {
        for (int x = 0; x < 3; ++x) {
            // unary[物体i*n+位置x]へ、その1物体と位置だけで決まる費用を設定する。
            if (!allowed[i * 3 + x]) {
                problem.unary[i * 3 + x] = penalty;
            }
        }
    }

    const auto violations = [&allowed](const std::vector<int>& p) {
        int count = 0;
        for (int i = 0; i < 3; ++i) {
            count += !allowed[i * 3 + p[i]];
        }

        return count;
    };

    const std::vector<int> feasible_initial{0, 1, 2};

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(feasible_initial));
    assert(violations(feasible_initial) == 0);

    // 配置と正しいQAP費用を対応させて結果型へ保存する。
    qap_result<> result{feasible_initial, problem.evaluate(feasible_initial)};

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 初期配置を内部生成して探索。
    // 締切は必須、seedを省略すると1。
    auto candidate = solve_qap(problem, deadline, 42);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(candidate.location_of));

    // 候補の物体→位置を読み、利用側の採用条件を確認する。
    const int found_violations = violations(candidate.location_of);
    std::cout << "candidate violations = " << found_violations << '\n';

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    if (found_violations == 0 && candidate.cost < result.cost) {
        // 採用した候補の配置とQAP費用を一緒に保存する。
        result = std::move(candidate);
    }

    // 結果の完全配置を読み、対応する条件を検査する。
    assert(violations(result.location_of) == 0);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    std::cout << "cost = " << result.cost << '\n';

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

**出力の読み方。** 候補の違反数と採用した配置を表示します。既知の実行可能解がない状況では、この例の保険は使えません。

検証時の標準出力の一例：

```text
candidate violations = 0
cost = 30
object 0 -> location 0
object 1 -> location 1
object 2 -> location 2
```


### 3.22 例22：積や総和が64bitを超える費用を計算する

運搬量と位置間費用がどちらも大きい場合、その積がlong longの上限を超えることがあります。

**具体的な問題。** 2台のflowとdistanceに40億を使います。40億の二乗を単項費用にも使い、配置ごとの費用を128bit整数で計算します。

**入力の表し方。** Weightをlong long、Scoreを__int128_tにします。利用側の掛け算も演算前にScoreへ変換します。演算が終わってから変換しても、先に起きたあふれは直りません。

ファイル名：`22_wide_score.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：積や総和が64bitを超える費用を計算する。

// 問題：2台のflowとdistanceに40億を使います。
// 40億の二乗を単項費用にも使い、配置ごとの費用を128bit整数で計算します。

// 注意：積・総和・差分の途中もScore内に収めます。
// 値を広い型へ変換してから掛け算します。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 費用・積・総和を符号付き128bit整数にする。
    using Score = __int128_t;

    // flowとdistanceは64bit、unaryと評価値は128bitで保持する。
    qap_problem<long long, Score> problem(2);
    const long long large = 4000000000LL;

    // 入力：物体ペアの係数を行ごとに並べる。
    // 逆方向も明示的に指定する。
    problem.flow = {
        0, large,
        large, 0
    };

    // 入力：位置ペアの係数を行ごとに並べる。
    // 自動的な対称化は行われない。
    problem.distance = {
        0, large,
        large, 0
    };

    // 単項費用を使うためn×n個の領域を確保し、指定値で埋める。
    problem.unary.assign(4, Score{0});

    // unary[物体i*n+位置x]へ、その1物体と位置だけで決まる費用を設定する。
    problem.unary[0] = Score{large} * large;

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 初期配置を内部生成して探索。
    // 締切は必須、seedを省略すると1。
    auto result = solve_qap(problem, deadline);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    const Score unit = Score{large} * large;

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(problem.evaluate({0, 1}) == 3 * unit);

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(problem.evaluate({1, 0}) == 2 * unit);

    // operator<<で結果を表示。
    // 改行はここで付け、配列は先頭20要素まで。
    std::cout << result << '\n';
}
```

**出力の読み方。** resultをそのまま出力し、128bitのcostと配置を表示します。result.cost単体を標準のostreamへ渡す形式にはしません。

検証時の標準出力の一例：

```text
qap_result{cost=32000000000000000000, location_of=[1, 0]}
```


### 3.23 例23：対象なし・1台だけ・締切到達を扱う

日によって対象が0件になるバッチ処理や、呼出し前に時間を使い切った処理でも、返却値を正しく解釈します。

**具体的な問題。** 空問題を解いた後、1台の問題へ切り替え、過去の締切と空の可動集合を渡します。1台の費用は3×4+5=17です。

**入力の表し方。** 空問題の結果は空配置・cost=0です。締切到達時のimproveは開始配置を評価して返します。空の可動集合では何も動きません。存在しない物体番号は指定しません。

ファイル名：`23_boundaries.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：対象なし・1台だけ・締切到達を扱う。

// 問題：空問題を解いた後、1台の問題へ切り替え、過去の締切と空の可動集合を渡します。
// 1台の費用は3×4+5=17です。

// 注意：締切は即時停止の命令ではありません。
// 期限後も開始配置の構築・評価などは発生します。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 既定構築はn=0の空問題。
    // 行列もすべて空。
    qap_problem<> problem;

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    assert(problem.n == 0);

    // 初期配置を内部生成して探索。
    // 締切は必須、seedを省略すると1。
    auto empty = solve_qap(problem, std::chrono::steady_clock::now());

    // 空問題では空配置とcost=0が返る。
    assert(empty.location_of.empty() && empty.cost == 0);

    // 1物体の問題へ代入。
    // 続く引数はflow、distance、unaryの順。
    // distance=4、unary=5。
    // 単項費用を渡す型はScoreに合わせる。
    problem = qap_problem<>(
        1,
        std::vector<int>{3},
        std::vector<int>{4},
        std::vector<long long>{5}
    );

    const std::vector<int> initial{0};

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    const auto expired =
        std::chrono::steady_clock::now()
        - std::chrono::seconds(1);

    // 初期配置から改善。
    // 引数は問題、初期配置、絶対締切、必要ならseed。
    auto result = improve_qap(problem, initial, expired);

    // 結果の完全配置を読み、対応する条件を検査する。
    assert(result.location_of == initial && result.cost == 17);

    // 指定した物体だけを現在の位置集合内で改善し、完全配置を返す。
    auto unchanged = improve_qap_subset(
        problem,
        initial,
        {},
        result.cost,
        expired
    );

    // 結果の完全配置を読み、対応する条件を検査する。
    assert(unchanged.location_of == initial && unchanged.cost == result.cost);

    // 指定した2物体の交換差分。
    // 配置は変更せず、同じ物体同士なら0。
    assert(problem.swap_delta(initial, 0, 0) == 0);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));

    // 同じ配置に対応するQAP費用を読む。
    // 値が小さいほど良い。
    std::cout << "cost = " << result.cost << '\n';

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // 返却配列の物体番号から位置番号を取り出す。
        // 配列の向きは物体→位置。
        std::cout << "object " << i << " -> location "
                  << result.location_of[i] << '\n';
    }
}
```

**出力の読み方。** 1台の配置とcost=17を表示します。時間切れを表す特別なenumや空結果が返る仕様ではありません。

検証時の標準出力の一例：

```text
cost = 17
object 0 -> location 0
```


### 3.24 例24：担当者ごとの割当費用と長い結果のログ出力

担当者同士の相互作用はなく、誰をどの仕事に割り当てるかの費用だけがある場合です。人数が多い結果を短くログへ出します。

**具体的な問題。** 25人と25仕事を一対一で対応させ、同じ番号同士なら費用0、それ以外は10とします。

**入力の表し方。** flowとdistanceを0のままにし、unaryだけを使います。このように二次項がない問題も表現できますが、solveの返却値に最適性の証明が付くわけではありません。

ファイル名：`24_debug_output.cpp`。次を全文コピーして実行できます。

```cpp
// この1ヘッダーでQAPと必要な標準ライブラリを利用できる。
#include "qap_solver_v09.hpp"

// 用途：担当者ごとの割当費用と長い結果のログ出力。

// 問題：25人と25仕事を一対一で対応させ、同じ番号同士なら費用0、それ以外は10とします。

// 注意：デバッグ表示は先頭20要素までです。
// 全配置を保存したい場合は配列を全要素読み出します。

// 入力はmain内の値で完結します。
// 費用は小さいほど良く、一般の最適性は保証されません。
// 入力サイズ・添字・演算の途中値まで、使う整数型の範囲に収めてください。

int main() {
    // 25物体・25位置。
    // Weight=int、Score=long long。
    // flowとdistanceは0、unaryは空。
    qap_problem<> problem(25);

    // 単項費用を使うためn×n個の領域を確保し、指定値で埋める。
    problem.unary.assign(25 * 25, 10);

    // 問題の物体数=位置数nを使う。
    // 添字は0以上n未満。
    for (int i = 0; i < problem.n; ++i) {
        // unary[物体i*n+位置x]へ、その1物体と位置だけで決まる費用を設定する。
        problem.unary[i * problem.n + i] = 0;
    }

    // 時間の逆戻りがない時計で現在時刻を取得し、絶対締切を作る。
    // ミリ秒の時間枠を加える。
    // 締切は厳密な停止時刻ではなく目安。
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::milliseconds(50);

    // 初期配置を内部生成して探索。
    // 締切は必須、seedを省略すると1。
    const auto result = solve_qap(problem, deadline, 1);

    // operator<<で結果を表示。
    // 改行はここで付け、配列は先頭20要素まで。
    std::cout << result << '\n';

    // operator<<で結果を表示。
    // 改行はここで付け、配列は先頭20要素まで。
    std::cerr << "debug: " << result << '\n';

    // 画面の代わりに、出力文字列をためる標準ライブラリのストリーム。
    std::ostringstream log;

    // 結果のoperator<<で文字列へ出力。
    // 配列は先頭20要素まで。
    log << result;
    assert(log.str().find("...]") != std::string::npos);
    assert(log.str().find("(size=25)") != std::string::npos);

    // 表示が省略されても、結果本体には全物体分の要素がある。
    assert(result.location_of.size() == 25);

    // 長さ・位置の範囲・重複を検査。
    // 禁止配置などの独自制約は別途確認する。
    assert(problem.is_valid_permutation(result.location_of));

    // この配置の費用を全再計算する。
    // 入力更新後や検算にも使う。
    assert(result.cost == problem.evaluate(result.location_of));
}
```

**出力の読み方。** cout・cerr・文字列へ結果を出力します。配列は先頭20要素まで、残りは...、全要素数はsize=25です。実データは25要素のままです。

検証時の標準出力の一例：

```text
qap_result{cost=0, location_of=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ...] (size=25)}
```

この例の標準エラー出力：

```text
debug: qap_result{cost=0, location_of=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ...] (size=25)}
```



### 3.25 検証範囲と再実行

本文の24個のC++コードブロックと、ZIPの `examples/` の24ファイルは同一内容です。各ブロックを1つずつコピーすることで独立してコンパイルできます。

検証環境は g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0 です。全24例をC++20でビルド・実行し、C++23でも全24例の構文・型検査を行いました。警告指定は `-Wall -Wextra -Wshadow -Wconversion -Werror` です。実行確認のビルドには `-O0` を指定し、assertを有効にしています。GCC 15.2そのものでの確認は行っていません。実行結果はZIP内の `verification/samples_results.json`、C++23の検査結果は `verification/syntax_results.json` に収録しています。

検査では、実行が終了することに加え、配置の妥当性、全費用との一致、交換差分の符号、ダミーの費用、班人数、巡回の逆対応、グラフ対応の定数復元、固定条件、縮約と復元の費用対応、追加目的の採否、入力更新後の再評価、禁止条件の検査、128bitの演算、配列表示の省略を確認しています。

ZIPのルートで次を実行すると、全例のビルド・実行と、本文のコードとの一致確認を再実行できます。Python 3とg++が必要です。

```bash
python3 verify_samples.py
```

この検証はサンプルの正しさとAPIの利用条件を確認するものです。一般の問題で最適解に到達する保証や、特定の時間以内に終了する保証の測定ではありません。
