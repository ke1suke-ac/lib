# LinearOrderingSolver サンプルコード集

この文書は `linear_ordering_solver_v10.hpp` の公開APIと、独立して実行できる24例をまとめたものです。数学は、足し算・掛け算・大小比較と、有限個の値の合計が分かれば読み進められます。コードはC++20です。

構成は「1. 何を解くライブラリか？」「2. APIリファレンス」「3. ユースケース」です。使いたい例から読む場合も、その例の説明・入力・コード・出力をまとめて利用できます。

## 1. 何を解くライブラリか？

### 1.1 全員を1回ずつ並べて、合計の損失を小さくする

発表者を発表順に並べる、仕事を処理順に並べる、商品を展示位置へ配置する、といった問題を考えます。対象がn個なら、全対象をちょうど1回ずつ使って、n個の位置を埋めます。省略する対象を選んだり、同じ対象を2回使ったりする問題ではありません。

対象には0からn−1までのIDを付けます。位置も0から数えます。順列 `[2, 0, 1]` は「先頭がID 2、次がID 0、最後がID 1」です。これはID別の位置一覧ではありません。ID 0の位置は1、ID 1の位置は2、ID 2の位置は0になります。

よさの基準を「費用」という点数で表し、その合計を小さくします。費用は金額でなくてもよく、待ち時間、希望に逆らう点数、段取りの負担などを使えます。低い方がよく、負の値を割引や報酬として使うこともできます。

### 1.2 三種類の費用と、定数

| 費用 | 発生する条件 | 例 |
|---|---|---|
| 順序費用 | 要素iを要素jより前に置いた | 0を2より先にすると損失4。間に別の要素が入っても払う |
| 位置費用 | 要素iを位置pに置いた | 重要な発表が最後になると損失5 |
| 有向隣接費用 | 要素iの直後に要素jを置いた | 赤色の製品の直後に青色を作ると洗浄費3 |
| 定数費用 | どんな順番でも発生する | 固定の運営費10 |

「有向」は向きがあるという意味です。iからjへの費用と、jからiへの費用は違っていて構いません。順序費用も同様です。設定しなかった費用は0です。

### 1.3 目的関数

求めるのは、次の合計ができるだけ小さい順列です。

$$F(a)=K+\sum_ {0\le p<q<n}C_ {a_ {p},a_ {q}}+\sum_ {p=0}^{n-1}U_ {a_ {p},p}+\sum_ {p=0}^{n-2}T_ {a_ {p},a_ {p+1}}$$

| 記号 | 意味・範囲 |
|---|---|
| $n$ | 要素数。0以上の整数 |
| $a$ | 全要素を1回ずつ含む順列 |
| $p,q$ | 順列上の位置を表す整数。0からn−1まで |
| $a_ {p}$ | 位置pに置いた要素のID。0からn−1まで。異なる位置で同じIDは使わない |
| $F(a)$ | 順列aに対して計算した合計費用。小さいほどよい |
| $K$ | どの順列にも共通して足す定数費用 |
| $C_ {i,j}$ | 要素iがjより前にあるときの順序費用。式ではiとjに、その位置のIDを代入している |
| $U_ {i,p}$ | 要素iを絶対位置pに置いたときの位置費用 |
| $T_ {i,j}$ | 要素iの直後がjであるときの有向隣接費用 |
| $i,j$ | 費用の表で使う要素ID。0からn−1まで |
| $\sum$ | 指定された組について、値を全部足す記号 |

最初の合計は、前の位置pと後ろの位置qをすべて選んで、そこにいる2要素の順序費用を足します。真ん中の合計は、各要素の位置費用を足します。最後の合計は、順列の中の隣り合う2要素だけについて費用を足します。

例えば `[2, 0, 1]` なら、式は次の足し算です。

$$F([2,0,1])=K+C_ {2,0}+C_ {2,1}+C_ {0,1}+U_ {2,0}+U_ {0,1}+U_ {1,2}+T_ {2,0}+T_ {0,1}$$

順序費用には、離れている2と1の組も入ります。隣接費用にはその組が入りません。最後の1から先頭の2へ戻る隣接費用も入りません。巡回路を表したい場合のモデル化は例10で扱います。

順序費用・隣接費用の表でiとjが同じ成分は使われません。要素0個なら三つの合計はいずれも0で、費用は定数だけになります。要素1個なら隣接費用も順序費用もなく、定数とその要素の位置費用だけになります。

### 1.4 希望と、破れない条件を区別する

「できればiをjより前にしたい」という希望は、逆になったときの非負の費用で表せます。重みwの希望なら、jがiより前のときにwを加えます。すべての希望を守れなくても、その合計費用を小さくできれば意味があります。

それに対して、必ず守る条件には次を設定できます。

- **先行関係**：iは必ずjより前。
- **位置の許可範囲**：ある要素を置ける位置を、区間または飛び飛びの一覧で指定。
- **位置の固定**：ある要素は必ず指定した位置。
- **呼び出し単位の部分固定**：既存の順列のうち、指定した要素をその位置から動かさずに改善。

すべての必須条件を満たす順列を「実行可能解」と呼びます。必須条件の違反は、上の目的関数に大きな罰金を足して許容するものではありません。返却状態が `Feasible` の解は必須条件を守ります。ただし、条件を満たす解がある場合でも、与えた時間内に見つからないことはあります。また、返された解が最小費用であるとは限りません。

### 1.5 表せる範囲

三種類の費用と制約は自由に組み合わせられます。位置だけの費用なら担当者と枠の割り当て、隣接費用なら全地点を1回ずつ通る経路、順序費用ならランキングや一部の待ち時間の評価を表せます。

位置は「何番目か」であり、経過した実時間ではありません。所要時間が異なる仕事の実時間の締切、複数資源の同時使用、要素を選んで省略する操作、任意の三つ組に依存する費用は、そのまま登録するAPIを持ちません。自分の評価式を上の形へ変換できるかを確認してください。任意の評価関数をコールバックとして渡すAPIもありません。

## 2. APIリファレンス

### 2.1 利用環境と共通の約束

ヘッダー `linear_ordering_solver_v10.hpp` をインクルードして使います。C++20以上が必要です。ヘッダーはGNUの `<bits/stdc++.h>` を使うため、本書ではGCCとlibstdc++を前提にします。各例はGCC 13.3.0、C++20、`-O2 -Wall -Wextra -Wpedantic` で独立してコンパイル・実行しています。

型の宣言は `template<class Cost = int64_t> class LinearOrderingSolver` です。本文とAPI表では、この型に `Solver` という別名を付けたものとして書きます。`Problem`、`OrderTerm`、`Options`、`Result`、`Status`、`Clock` はすべて `Solver` の公開メンバー型です。

共通の引数条件は以下のとおりです。各表の「既定値なし」は、その引数を省略できないという意味です。

| 名前・型 | 意味、取り得る値、既定値 |
|---|---|
| `Cost` | 費用の型。既定値は `int64_t`。符号付き整数型、または `float`・`double`・`long double` などの浮動小数点型。符号なし整数や独自の数値クラスは不可 |
| `n: int` | 要素数。`n >= 0`。既定値なし。IDの集合も位置の集合も0以上n未満になる |
| `i, j, before, after: int` | 要素ID。`0 <= ID < n`。既定値なし |
| `pos: int` | 絶対位置。`0 <= pos < n`。既定値なし |
| `lo, hi: int` | 半開区間 `[lo, hi)`。`0 <= lo <= hi <= n`。hiは含まない。既定値なし |
| `value, delta, cost: Cost` | 上書きする値、加算する値、費用。負・0・正を指定できる。浮動小数点ではNaN・無限大は不可。引数の既定値なし |
| `weight: Cost` | 希望違反の費用。0以上。浮動小数点なら有限値。既定値なし |
| `std::span<const T>` | 連続した配列を読み取る引数。`std::vector<T>`、`std::array<T,N>`、C配列などを渡せる。span自体は配列を所有しない |

費用の入力値だけでなく、合計・差・積など、呼び出し側とライブラリ内で生じる中間値も型の範囲に収めます。特に整数の桁あふれに注意してください。小数には丸め誤差があるので、アプリケーションで異なる計算結果を比較するときは、必要に応じて小さな許容誤差を設けます。1円のような最小単位が決まる値は、整数で持つと扱いやすくなります。

配列を受け取る設定メソッドは内容を内部へコピーします。呼び出し後の入力配列の変更は、設定済みの問題には反映されません。`solve` / `improve` も同期的に完了し、返る `Result` は順列を自分で所有します。入力配列は呼び出し中は有効である必要がありますが、呼び出し後まで維持する必要はありません。

IDや配列長などの不正なAPI入力は、主に `assert` で検査されます。通常のエラー結果として `NotFound` が返るという契約ではありません。`NDEBUG` を指定すると検査が消えるため、事前条件は利用者が守ります。一方で、正しいIDによる先行関係の循環や、空の許可集合は「入力形式は正しいが、条件を満たせない問題」であり、探索結果の状態で扱います。

同じソルバーを複数スレッドで同時に使うことは想定しません。問題を変更しながら探索することも避けます。

### 2.2 `Solver` の構築・設定窓口・採点・探索

| API | 引数・既定値 | 戻り値・働き |
|---|---|---|
| `explicit Solver(int n)` | `n >= 0`、省略不可 | n要素の問題を所有するソルバーを作る。全費用0、先行条件なし、全位置許可 |
| `explicit Solver(Problem problem)` | 同じCost型の `Problem`、省略不可 | 問題を所有する。変数をそのまま渡すとコピー、`std::move` で渡すとデータを移す |
| `Problem& problem()` | 引数なし | 所有する問題への変更可能な参照。`auto& p = solver.problem();` で設定を編集できる |
| `const Problem& problem() const` | 引数なし | constソルバーからの読み取り専用参照。`size()` などを呼べる |
| `void prepare()` | 引数なし | 現在の問題に必要な事前準備を行う。省略可。探索メソッドからも必要に応じて準備される。結果の有無は返さず、時間制限引数もない |
| `Result solve(const Options& options = {})` | `options` は探索設定。省略すると全フィールド既定値 | 内部でID順 `[0,1,...,n-1]` を出発点として解を探す。前回の解を暗黙に引き継ぐ呼び出しではない |
| `Result improve(std::span<const int> initial, const Options& options = {}, std::span<const uint8_t> movable = {})` | `initial` は必須。`options` と `movable` は省略可。詳細は下表 | 与えた完全な順列を修復・改善して結果を返す |
| `Cost evaluate(std::span<const int> order) const` | 全IDを各1回含む長さnの完全な順列。省略不可 | 最新の問題の元の目的関数を返す。必須条件を満たしていなくても採点できる。必須条件違反の罰金は含まない |
| `bool is_feasible(std::span<const int> order) const` | 調べる配列。省略不可 | 完全な順列で、最新の問題の必須条件を満たすときだけtrue。長さ違い・ID範囲外・重複はfalse |

`improve` の各引数を詳しく説明します。

| 引数 | 取り得る値・既定値・意味 |
|---|---|
| `initial` | 0からn−1までを各1回含む長さnの順列。省略不可。必須の先行・位置条件には違反していてもよい。`-1` や重複、欠損を埋めるAPIではない |
| `options` | `Options`。既定値 `{}`。全フィールドの意味は2.4参照 |
| `movable` | 既定値は空のspanで、全要素を可動にする。空でなければ長さは必ずn。`movable[id] == 0` はそのIDをinitial中の位置へ固定、非0は可動。添字は位置番号ではなく要素ID。`uint8_t` の配列を使い、`std::vector<bool>` は渡さない |

可動要素は固定要素の位置を埋められません。固定要素を除いた空き位置の間で順番を変えますが、費用・先行関係の計算から固定要素が除かれるわけではありません。返却順列は固定要素も含む長さnです。この可動フラグは今回の `improve` にだけ適用され、次回の呼び出しには残りません。

`is_feasible` が確認するのはProblemの条件です。直前の呼び出しで使った可動フラグは記憶しないので、その固定を別途確認したいときは例17のように初期位置と比較します。

初期順列が現在のProblemの条件を満たすとき、`improve` はそれより悪い費用の順列を返しません。時間目標が0でも、その初期解の採点と検証は行います。初期順列が制約違反なら、修復で費用が上がる場合があり、実行可能解が必ず見つかる保証もありません。

`problem()` を `auto p = solver.problem();` と受けるとProblemのコピーになります。元のソルバーを変更するには参照の `&` が必要です。問題を変更した後も古い `Result` の内容はそのままなので、必要に応じて `is_feasible` と `evaluate` で確認し直します。要素数を変更するAPIはなく、別のソルバーを構築します。

### 2.3 結果型 `Result` と列挙型 `Status`

`solve` と `improve` は同じ `Result` を値で返します。`Result` 自身をコピー・代入して、最良解を保持できます。

| `Result` のメンバー | 型 | 既定値 | 意味・取り得る値 |
|---|---|---|---|
| `status` | `Status` | `Status::NotFound` | 下表の3状態のいずれか |
| `order` | `std::vector<int>` | 空 | Feasibleなら全n要素の順列。その他なら空。要素0個のFeasibleでも空になる |
| `cost` | `Cost` | `0` | Feasibleのときにだけ意味がある元の目的関数値。その他の状態で0が入っていても、費用0の解が見つかった意味ではない |

| `Status` の値 | 誤解しないための意味 |
|---|---|
| `Status::Feasible` | 今回の必須条件を満たす順列が得られた。大域的な最適性を証明した状態ではない |
| `Status::NotFound` | 今回は実行可能解が得られなかった。不可能だと証明されたわけではない。時間不足などが該当する |
| `Status::ProvenInfeasible` | 今回の条件を同時に満たす順列がないことが確認された。部分固定を使った場合は、その固定条件を含む部分問題についての結論。元のProblem全体が不可能とは限らない |

不可能な問題が必ず `ProvenInfeasible` になるとは限りません。時間や検出できる条件によって `NotFound` になる場合もあります。成功判定には `status` を使い、`order.empty()` や `cost == 0` で代用しません。

公開のデバッグ出力は `std::ostream& operator<<(std::ostream& os, const Result& r)` です。2引数とも省略不可で、`os` は `std::cout`・`std::cerr`・`std::ostringstream` などの出力先、`r` は表示する結果です。戻り値は渡した `os` 自身なので、`std::cout << result << '\n';` のように続けて書けます。`Solver` や `Problem` 全体、`Status` 単体を表示する演算子は用意されていません。

| 表示の場面 | 形式 |
|---|---|
| 実行可能解 | `Result{status=Feasible, cost=1, order=[2, 0, 1]}` |
| 未発見 | `Result{status=NotFound, order=[]}` |
| 不可能 | `Result{status=ProvenInfeasible, order=[]}` |
| 20要素を超える場合の末尾 | `..., 19, ... (25 total)]}` のように、先頭20要素の後で省略と総要素数を表示 |

演算子は改行を付けず、費用はFeasibleの場合だけ表示します。浮動小数点の桁数などは出力ストリーム側の設定に従います。順列のデータ自体は省略しないので、全要素を出力したいときは `result.order` をループで表示します。これは人が読む要約であり、保存形式・機械処理用の形式として扱うことは勧めません。

### 2.4 探索設定 `Options` と時計 `Clock`

`Options options;` と書くと、以下の既定値が入ります。`solve()`、`improve(initial)`、`improve(initial, {}, movable)` でも同じ設定が使われます。

| フィールド | 型 | 既定値 | 意味・指定できる値 |
|---|---|---|---|
| `time_limit_us` | `int64_t` | `100'000` | 0以上のマイクロ秒。1000マイクロ秒が1ミリ秒。呼び出し開始からの終了目標で、探索ループだけの予算ではない |
| `deadline` | `Clock::time_point` | `Clock::time_point::max()` | steady_clock上の絶対締切。通常 `Clock::now() + std::chrono::milliseconds(...)` で作る。過去の時点も指定できるが、その時点で探索予算は尽きている |
| `seed` | `uint64_t` | `0` | 0から `UINT64_MAX` までの乱数の種。種を変えると探索の進み方を変えられる。時間で止めるため同じ種でも結果の完全一致は保証しない |
| `allow_infeasible_moves` | `bool` | `false` | false：実行可能解を得た後は、制約を破る中間移動を許さない。true：探索途中の制約違反も許す。どちらでもFeasibleとして返す結果は制約を守る。falseでも違反した初期順列の修復は行う |

`Clock` は `std::chrono::steady_clock` の公開別名です。時計を合わせる操作で戻らない、経過時間を測るための時計です。カレンダー上の日時を直接指定するものではありません。

呼び出しは「開始時刻＋time_limit_us」と `deadline` の早い方を終了目標にします。例19のように複数回の探索へ同じdeadlineを渡すと、残り時間を共有できます。これは厳密なリアルタイム保証ではなく、初期解の採点・検査、最終確認、返却などに時間が必要です。時間0でも呼び出しコストが0になるわけではありません。`prepare()` にはこのOptionsは渡せず、その処理自体をdeadlineで中断しません。

### 2.5 `Problem` の構築と所有権

| API | 引数・既定値 | 意味・戻り値 |
|---|---|---|
| `explicit Problem(int n)` | `n >= 0`、省略不可 | 費用0・先行条件なし・全位置許可の問題を作る |
| `Problem(const Problem& other)` | 同じ型の問題、省略不可 | データをコピーして独立した問題を作る。戻り値はなく、新しいオブジェクトの構築 |
| `Problem(Problem&& other)` | 同じ型の問題を `std::move` して渡す。省略不可 | データを移して新しい問題を作る。移動元は破棄するまで問題操作に再利用しない |
| `Problem& operator=(const Problem&) = delete` | 呼び出し不可 | コピー代入は使えない。コピー構築と区別する |
| `Problem& operator=(Problem&&) = delete` | 呼び出し不可 | ムーブ代入は使えない。ムーブ構築と区別する |
| `int size() const` | 引数なし | 構築時に指定したn。0以上の整数 |

デフォルト引数なしのコンストラクターなので、`Solver solver;` や `Problem problem;` は使えません。ソルバー自体についても、C++が生成するコピー構築・ムーブ構築は利用できますが、Problemが代入不可なのでソルバーのコピー代入・ムーブ代入も使えません。通常は例22のようにProblemをコピーし、用途ごとにソルバーを作ると意図が明確です。

### 2.6 順序費用 `C` のAPI

下表の設定メソッドはすべて `void` を返します。記載する引数はすべて省略不可です。n、ID、費用の型・範囲は2.1と共通です。

| API | 各パラメータと効果 |
|---|---|
| `void add_order_cost(int i, int j, Cost delta)` | iがjより前の費用 `C[i,j]` にdeltaを加算する。反対向き `C[j,i]` は変えない |
| `void set_order_cost(int i, int j, Cost value)` | `C[i,j]` をvalueへ上書きする。他の組は変えない |
| `void add_preference(int i, int j, Cost weight)` | iをjより前にしたい希望。非負のweightを **`C[j,i]`** に加算する。`add_order_cost(i,j,weight)` とは向きが逆 |
| `void set_order_costs(std::span<const Cost> values)` | valuesは必ずn×n個。`values[i*n+j]` を `C[i,j]` として、順序費用全体を置き換える。空を渡してよいのはn=0のときだけ |
| `void set_order_terms(std::span<const OrderTerm> terms)` | 0件以上のレコード一覧で順序費用全体を置き換える。空なら全順序費用0。同方向の重複は合算、反対方向は別。合計0の項は省略される |
| `void clear_order_costs()` | 引数なし。全順序費用を0へ戻す。位置・隣接・定数・制約は変えない |

初期状態と `set_order_terms` は、費用のある組のレコードを保持する形式です。`set_order_costs` は全n×n個の表を保持する形式です。疎な関係にはレコードや個別追加、ほぼ全組の費用を持つ場合には表というように選べます。どちらも同じ目的関数を表します。単一成分のadd/setで現在の入力形式を指定する必要はありません。

`OrderTerm` は集成体で、`Solver::OrderTerm{before, after, cost}` のように作ります。

| フィールド | 型・指定範囲 | 初期化・意味 |
|---|---|---|
| `before` | `int`、0以上n未満 | 先にある要素のID。個別のメンバー既定値なし |
| `after` | `int`、0以上n未満 | 後ろにある要素のID。個別のメンバー既定値なし |
| `cost` | `Cost`、有限・負も可 | beforeがafterより前なら加える費用。個別のメンバー既定値なし |

`OrderTerm term;` のように初期化を省くとメンバーは未初期化になり得ます。値を3個とも明示してください。`OrderTerm{}` はC++の値初期化により0になりますが、n=0ではID 0の項自体が無効です。`before == after` は費用の計算に寄与しません。`OrderTerm` は必須の先行制約を登録する型ではありません。

### 2.7 位置費用 `U` のAPI

すべて戻り値は `void`、記載する引数はすべて省略不可です。

| API | 各パラメータと効果 |
|---|---|
| `void add_position_cost(int i, int pos, Cost delta)` | 要素iを位置posに置く費用 `U[i,pos]` にdeltaを足す |
| `void set_position_cost(int i, int pos, Cost value)` | `U[i,pos]` をvalueで上書きする |
| `void set_position_costs(std::span<const Cost> values)` | valuesはn×n個、`values[i*n+pos]` が `U[i,pos]`。全位置費用を置き換える。n=0なら空 |
| `void clear_position_costs()` | 引数なし。全位置費用を0へ戻す。他の費用や制約は維持 |

位置費用は行が要素ID、列が位置です。位置制限とは異なり、どれだけ高い費用でもその位置を禁止したことにはなりません。禁止には2.10のAPIを使います。

### 2.8 有向隣接費用 `T` と定数 `K` のAPI

すべて戻り値は `void`、記載する引数はすべて省略不可です。

| API | 各パラメータと効果 |
|---|---|
| `void add_adjacent_cost(int i, int j, Cost delta)` | iの直後がjのときの費用 `T[i,j]` にdeltaを足す |
| `void set_adjacent_cost(int i, int j, Cost value)` | `T[i,j]` をvalueで上書きする。逆向きは変えない |
| `void set_adjacent_costs(std::span<const Cost> values)` | valuesはn×n個、`values[i*n+j]` が `T[i,j]`。全隣接費用を置き換える。n=0なら空 |
| `void clear_adjacent_costs()` | 引数なし。全隣接費用を0へ戻す。他の費用や制約は維持 |
| `void add_constant_cost(Cost delta)` | 全順列に共通のKへdeltaを足す |
| `void set_constant_cost(Cost value)` | Kをvalueへ上書きする。0にすれば定数を除去できる |

`T[i,i]` は使われず、末尾から先頭への費用も自動では加わりません。ある隣接を絶対に禁止する専用APIはありません。非常に大きな費用を置いても必須制約と同じ保証にはなりません。

位置費用と隣接費用は、個別成分を1個設定したときでも、それぞれn×nの表を必要に応じて確保します。これらには `OrderTerm` に対応する疎な一括入力型はありません。メモリーに入る問題サイズかも確認してください。

### 2.9 必須先行関係のAPI

すべて戻り値は `void`、記載する引数はすべて省略不可です。

| API | 各パラメータと効果 |
|---|---|
| `void require_before(int i, int j)` | iがjより前である条件を1本追加する。同じ関係の重複追加は条件を増やさない |
| `void set_precedences(std::span<const std::pair<int,int>> edges)` | 0本以上の `(先行ID, 後続ID)` の一覧。すべて有効なID。先行関係全体を置き換え、重複を除く。空なら先行関係なし |
| `void clear_precedences()` | 引数なし。すべての先行条件を解除する。位置制約や費用は変更しない |

「iはjより前、jはkより前」なら、iはkより前にもなります。そのためにiからkの関係を重ねて入力する必要はありません。自分自身より前という条件、または輪になる関係を登録すると、実行可能解がありません。入力直後に例外が出るのではなく、結果の状態を見て扱います。

### 2.10 許可位置・固定位置のAPI

すべて戻り値は `void`、記載する引数はすべて省略不可です。「許可集合」とは、その要素を置いてよい位置を集めたものです。

| API | パラメータの範囲と効果 |
|---|---|
| `void set_position_range(int i, int lo, int hi)` | iの許可集合を `[lo,hi)` へ置き換える。`0 <= lo <= hi <= n`。以前の飛び飛びの制限も消える |
| `void restrict_position_range(int i, int lo, int hi)` | iの現在の許可集合と `[lo,hi)` の共通部分だけを残す。引数範囲は上と同じ |
| `void set_positions(int i, std::span<const int> positions)` | iの許可集合を一覧へ置き換える。全要素が0以上n未満。順不同・重複可。空の一覧はどこにも置けないという意味 |
| `void restrict_positions(int i, std::span<const int> positions)` | 現在の許可集合と一覧の共通部分だけを残す。一覧の値の範囲・重複の扱いは上と同じ |
| `void fix_position(int i, int pos)` | 現在の許可集合を、指定したposだけへ**絞る**。posは0以上n未満。既存の許可集合を無視して上書きする機能ではない |
| `void clear_position_restriction(int i)` | iに対する位置制約を解除し、0からn−1までの全位置を許可する。他の要素の位置制約は変えない |

`lo == hi` の空区間は有効な指定ですが、その要素が置けなくなります。また、集合の共通部分がなくなった場合や、複数要素を同じ1位置へ固定した場合も、実行可能解はありません。

`fix_position(i, 1)` の後に `fix_position(i, 2)` と書くと、固定を移動したのではなく、許可集合が空になります。固定位置を変更するには、残したい条件を考慮した上で、例えば `clear_position_restriction(i)` で位置条件を解除してから固定し直します。全解除すると、その要素の区間や飛び飛びの制限も失われる点に注意してください。

### 2.11 APIを選ぶ早見表

| やりたいこと | 主なAPI | 例 |
|---|---|---|
| 希望に逆らう費用を小さくする | `add_preference`, `solve` | 1・2 |
| 任意の前後費用を登録する | `add_order_cost`, `set_order_costs`, `set_order_terms`, `OrderTerm` | 3・12・13 |
| 置く位置で点数を変える | `set_position_cost`, `add_position_cost`, `set_position_costs` | 5・7・11・14 |
| 隣接する組の費用を使う | `set_adjacent_cost`, `set_adjacent_costs` | 9・10・11 |
| 必ず守る前後関係を入れる | `set_precedences`, `require_before` | 4・11 |
| 許可位置・固定を使い分ける | `set_position_range`, `set_positions`, `restrict_*`, `fix_position`, `clear_position_restriction` | 6・7・8 |
| 採点・制約検査だけ行う | `evaluate`, `is_feasible` | 15 |
| 不適合な初期値を修復する | `improve`, `allow_infeasible_moves` | 16 |
| 一部だけ動かす | `improve` の `movable` | 17・20 |
| 最良解を引き継ぐ・複数開始を試す | `Result`, `improve`, `seed` | 18・19 |
| 時間を配分する | `Options`, `time_limit_us`, `deadline`, `Clock`, `prepare` | 19 |
| 費用・制約を更新・解除する | 各 `add_*`, `set_*`, `clear_*` | 21 |
| 共通問題を独立に複製する | `Problem` のコピー・ムーブ、`problem`, `size` | 22 |
| 結果の状態を正しく判定する | `Status` の全値 | 23 |
| 短いデバッグ表示をする | `operator<<` | 24および各探索例 |

## 3. ユースケース

### 実行方法と入力の約束

以下の各コードは、他の例の関数や変数に依存しない完全なプログラムです。共通ヘッダー `linear_ordering_solver_v10.hpp` は必要です。使いたいコードを `main.cpp`、その入力例を `input.txt` に保存し、ヘッダーと同じフォルダーで実行できます。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic main.cpp -o sample
./sample < input.txt
```

ZIP同梱の例をそのまま動かす場合は、ZIPを展開したフォルダーで次のようにします。例の番号を変えると他のプログラムも実行できます。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -I. examples/case01.cpp -o sample
./sample < inputs/case01.txt
```

全例の確認用には、GCCとPython 3を使う `python3 check_examples.py` も同梱しています。これは入力例でコンパイル・実行し、目的関数と制約を別実装で照合するスクリプトです。

入力例はすべてそのまま使えます。値を変更する場合は2.1の範囲、例ごとの入力形式、完全な順列という条件を守ってください。説明を短く保つため、各プログラムは入力の欠損などは確認しますが、外部からの不正なIDや極端なサイズを網羅的に検証する入出力サービスではありません。費用の乗算・加算で桁あふれしない範囲の入力を使います。数値のIDに人名や商品名を対応させる表示処理は、利用側で追加できます。

出力はこの環境での**一実行例**です。時間で探索を止めるため、機械や実行時の負荷によって順列・費用は変わる可能性があります。同じ費用の解が複数ある場合もあります。ここに載せた出力と文字列が一致することをライブラリの保証として使わず、状態・制約・目的関数の意味で判断してください。

### 用途の一覧

| 例 | ユースケース | ソース |
|---|---|---|
| 1 | 希望の前後関係から、発表順を決める | `case01.cpp` |
| 2 | 複数人のランキングを、一つの総合順位にする | `case02.cpp` |
| 3 | 重要な仕事の待ち時間を減らす | `case03.cpp` |
| 4 | 前提作業を必ず守った工程順を作る | `case04.cpp` |
| 5 | 同じ長さの仕事を、納期遅れの罰金が少ない順にする | `case05.cpp` |
| 6 | 冒頭・最後・許可区間のある番組表を作る | `case06.cpp` |
| 7 | 飛び飛びの空き枠へ、担当者を割り当てる | `case07.cpp` |
| 8 | 複数の部署の許可条件を重ね、固定を取り消す | `case08.cpp` |
| 9 | 戻り道を含まない、有向の訪問経路を作る | `case09.cpp` |
| 10 | 決めた拠点から出発し、最後に拠点へ戻る | `case10.cpp` |
| 11 | 工程の希望・遅い位置の罰金・段取り替えを同時に扱う | `case11.cpp` |
| 12 | すべての組の前後費用を、表から読み込む | `case12.cpp` |
| 13 | 少数の前後費用と割引を、レコード一覧から読み込む | `case13.cpp` |
| 14 | 小数の満足度を最大化する | `case14.cpp` |
| 15 | 手作業で作った順番を、採点・検査する | `case15.cpp` |
| 16 | 新しい必須条件に合わない予定を、修復する | `case16.cpp` |
| 17 | 確定済みの仕事を動かさず、残りだけを改善する | `case17.cpp` |
| 18 | 現在の最良案を引き継ぎ、短い時間で繰り返し改善する | `case18.cpp` |
| 19 | 全体の締切を共有し、複数の初期順序を試す | `case19.cpp` |
| 20 | 長い順番を、窓の中だけ少しずつ組み替える | `case20.cpp` |
| 21 | 費用設定を更新し、不要な評価項目を外す | `case21.cpp` |
| 22 | 共通モデルをコピーして、別々の条件を比較する | `case22.cpp` |
| 23 | 未発見・不可能・空問題を、状態で区別する | `case23.cpp` |
| 24 | 結果を短くログへ出し、必要なときだけ全順列を出す | `case24.cpp` |

### 3.1 希望の前後関係から、発表順を決める

3人の発表順を決めます。「2を0より先に」は重要度4、「0を1より先に」は重要度3、
「1を2より先に」は重要度1です。三つを全部守ることはできないので、破った希望の重要度の合計を小さくします。
例えば順列 `[2, 0, 1]` は最後の希望だけを破るため、費用は1です。希望は必須制約ではありません。

**入力形式**：`n m` の後に、希望 `先にしたいID 後にしたいID 重み` を `m` 行。重みは非負整数です。

**入力例**（`inputs/case01.txt`）

```text
3 3
2 0 4
0 1 3
1 2 1
```

**コード**（`examples/case01.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <cstdint>
#include <iostream>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n, m;
    if (!(std::cin >> n >> m)) {
        return 1;
    }

    // n個を各1回並べる問題を作る。IDは0以上n未満。
    Solver solver(n);

    // &を付け、ソルバーが所有する問題を直接設定する。
    auto& p = solver.problem();

    for (int k = 0; k < m; ++k) {
        int before, after;
        int64_t weight;
        if (!(std::cin >> before >> after >> weight)) {
            return 1;
        }

        // 希望に逆らったときだけweightを払う。同じ希望は加算される。
        p.add_preference(before, after, weight);
    }

    // 省略時の時間目標は100,000マイクロ秒、乱数seedは0。
    const auto result = solver.solve();

    // 状態・費用・順列を表示。費用はFeasibleのときだけ意味がある。
    std::cout << result << '\n';
}
```

**標準出力の一例**（`outputs/case01.txt`）

```text
Result{status=Feasible, cost=1, order=[2, 0, 1]}
```

### 3.2 複数人のランキングを、一つの総合順位にする

4作品について、3人がそれぞれ順位を付けました。人ごとに、上位の作品が下位より先になる希望をすべて登録します。
できた総合順位と各人の順位の間で、前後が逆転した作品の組数の合計を最小にします。
単に順位の数字を足す方法とは別の評価基準です。同じ希望が複数人から出ると、その人数分だけ重くなります。
入力の各ランキングは、全作品を重複なく含むものとします。

**入力形式**：`n voters` の後に、各人のランキングを `n` 個のIDで1行ずつ。

**入力例**（`inputs/case02.txt`）

```text
4 3
0 2 1 3
2 0 3 1
0 1 2 3
```

**コード**（`examples/case02.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n, voters;
    if (!(std::cin >> n >> voters)) {
        return 1;
    }

    // 作品IDを並べた総合順位を求める。
    Solver solver(n);

    // 希望を同じ問題へ積み上げる。
    auto& p = solver.problem();
    std::vector<int> ranking(n);

    for (int v = 0; v < voters; ++v) {
        for (int& x : ranking) {
            if (!(std::cin >> x)) {
                return 1;
            }
        }

        for (int a = 0; a < n; ++a) {
            for (int b = a + 1; b < n; ++b) {
                // この人の前後関係が逆転したら1点の損失。
                p.add_preference(ranking[a], ranking[b], 1);
            }
        }
    }

    // 費用が小さいほど、合計の逆転数が少ない。
    const auto result = solver.solve();

    // 最適性の証明ではなく、見つかった実行可能な順位を表示する。
    std::cout << result << '\n';
}
```

**標準出力の一例**（`outputs/case02.txt`）

```text
Result{status=Feasible, cost=3, order=[0, 2, 1, 3]}
```

### 3.3 重要な仕事の待ち時間を減らす

1台の機械で仕事を一つずつ、休みなく処理します。仕事ごとに処理時間と重要度があります。
「終了した時刻×重要度」を全仕事について足した値を小さくします。入力例は、仕事0が時間3・重要度1、
仕事1が時間1・重要度5、仕事2が時間2・重要度2です。

仕事iが仕事jより前なら、jの終了時刻にはiの処理時間が加わります。
そこで `C[i,j] = 処理時間[i] × 重要度[j]`、定数を「各仕事自身の処理時間×重要度」の合計にします。
例えば `[1, 2, 0]` の終了時刻は順に1、3、6なので、費用は `1×5 + 3×2 + 6×1 = 17` です。
このモデルは途中の空き時間、開始可能時刻、並列機械を含みません。

**入力形式**：`n` の後に、ID順で `処理時間 重要度` を `n` 行。どちらも非負整数です。

**入力例**（`inputs/case03.txt`）

```text
3
3 1
1 5
2 2
```

**コード**（`examples/case03.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<int64_t> duration(n), weight(n);

    for (int i = 0; i < n; ++i) {
        if (!(std::cin >> duration[i] >> weight[i])) {
            return 1;
        }
    }

    // 仕事の順番を決める。位置は実時刻ではなく何番目かを表す。
    Solver solver(n);

    // 時刻の評価を順序費用と定数へ変換して登録する。
    auto& p = solver.problem();

    for (int i = 0; i < n; ++i) {
        // 自分自身の処理時間はどの順序でも終了時刻に含まれる。
        p.add_constant_cost(duration[i] * weight[i]);

        for (int j = 0; j < n; ++j) {
            if (i != j) {
                // iが先なら、jはこの時間だけ待つ。
                p.add_order_cost(i, j, duration[i] * weight[j]);
            }
        }
    }

    // 積や費用の合計がint64_tに収まる入力にする。
    const auto result = solver.solve();

    // costは重要度付き終了時刻の合計そのもの。
    std::cout << result << '\n';
}
```

**標準出力の一例**（`outputs/case03.txt`）

```text
Result{status=Feasible, cost=17, order=[1, 2, 0]}
```

### 3.4 前提作業を必ず守った工程順を作る

費用の優劣はなく、必須条件を全部守る順番が一つあればよい場合です。
5工程について「0は2より前」「1は2より前」「2は3より前」「3は4より前」を守ります。
費用を登録しないので、実行可能な順列の費用はすべて0です。
最初の3本は一覧から一括設定し、最後の1本を個別に追加します。

**入力形式**：`n m`、続いて一括登録する `先行ID 後続ID` が `m` 行、最後に追加する関係が1行。

**入力例**（`inputs/case04.txt`）

```text
5 3
0 2
1 2
2 3
3 4
```

**コード**（`examples/case04.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <iostream>
#include <utility>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n, m;
    if (!(std::cin >> n >> m)) {
        return 1;
    }

    std::vector<std::pair<int, int>> edges(m);

    for (auto& [a, b] : edges) {
        if (!(std::cin >> a >> b)) {
            return 1;
        }
    }

    int before, after;
    if (!(std::cin >> before >> after)) {
        return 1;
    }

    // 費用の初期値はすべて0。実行可能な工程順を探す。
    Solver solver(n);

    // 必須条件を設定するための参照を取得する。
    auto& p = solver.problem();

    // 先行関係の一覧を置き換える。重複していても同じ制約として扱う。
    p.set_precedences(edges);

    // この関係を追加する。add_preferenceと違って破ることはできない。
    p.require_before(before, after);

    // 矛盾があれば実行可能解は返らない。状態を見て判断する。
    const auto result = solver.solve();

    // Feasibleなら、表示された工程順がすべての条件を満たす。
    std::cout << result << '\n';
}
```

**標準出力の一例**（`outputs/case04.txt`）

```text
Result{status=Feasible, cost=0, order=[0, 1, 2, 3, 4]}
```

### 3.5 同じ長さの仕事を、納期遅れの罰金が少ない順にする

すべての仕事が1時間で終わり、時刻0から続けて実行されるとします。
位置pに置いた仕事の終了時刻は `p+1` です。納期より遅れた時間に、その仕事の1時間あたりの罰金を掛けます。
4仕事の「納期・罰金」は順に `(4,1), (1,5), (2,3), (2,2)` です。
仕事の長さが全部同じだから、終了時刻を絶対位置だけで計算できます。長さが異なる場合にはこの表現をそのまま使えません。

**入力形式**：`n` の後に、ID順で `納期 1時間あたりの罰金` を `n` 行。どちらも非負整数です。

**入力例**（`inputs/case05.txt`）

```text
4
4 1
1 5
2 3
2 2
```

**コード**（`examples/case05.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    // 仕事1個につき位置を1枠使う。枠は0から数える。
    Solver solver(n);

    // 仕事と位置の組に費用を付ける。
    auto& p = solver.problem();

    for (int i = 0; i < n; ++i) {
        int64_t due, rate;
        if (!(std::cin >> due >> rate)) {
            return 1;
        }

        for (int pos = 0; pos < n; ++pos) {
            const int64_t late = std::max<int64_t>(0, int64_t(pos) + 1 - due);

            // 要素iを位置posへ置いたときの遅延罰金を上書きする。
            p.set_position_cost(i, pos, late * rate);
        }
    }

    // 納期を超える配置も許し、その分を費用として最小化する。
    const auto result = solver.solve();

    // 費用0でなければ失敗という意味ではない。必須制約とは別の評価。
    std::cout << result << '\n';
}
```

**標準出力の一例**（`outputs/case05.txt`）

```text
Result{status=Feasible, cost=2, order=[1, 2, 3, 0]}
```

### 3.6 冒頭・最後・許可区間のある番組表を作る

5本の番組を並べます。番組0は冒頭、番組4は最後に固定します。
番組3は位置1または2だけに出せます。その範囲で、番組3を1より前にしたい希望を重要度7で反映します。
入力の `[1,3)` は1以上3未満、つまり位置1・2です。人がいう2番目・3番目に当たります。

**入力形式**：`n`、固定 `ID 位置` が2行、区間制限 `ID lo hi` が1行、希望 `先行ID 後続ID 重み` が1行。

**入力例**（`inputs/case06.txt`）

```text
5
0 0
4 4
3 1 3
3 1 7
```

**コード**（`examples/case06.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <cstdint>
#include <iostream>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    // n本を各1回並べる。各位置には必ず1本だけ入る。
    Solver solver(n);

    // 位置制約はこの問題の以後の呼び出しにも残る。
    auto& p = solver.problem();

    for (int k = 0; k < 2; ++k) {
        int id, pos;
        if (!(std::cin >> id >> pos)) {
            return 1;
        }

        // 現在の許可位置をこの1位置へ絞る。今回は全位置許可からの固定。
        p.fix_position(id, pos);
    }

    int id, lo, hi, before, after;
    int64_t weight;
    if (!(std::cin >> id >> lo >> hi >> before >> after >> weight)) {
        return 1;
    }

    // この要素の許可位置を[lo, hi)へ置き換える。hiは含まない。
    p.set_position_range(id, lo, hi);

    // 必須の位置制約を守った上で、希望の違反費用を減らす。
    p.add_preference(before, after, weight);

    // 固定や区間を同時に満たす順列を探す。
    const auto result = solver.solve();

    // 順列は位置順のID。IDごとの位置一覧ではない。
    std::cout << result << '\n';
}
```

**標準出力の一例**（`outputs/case06.txt`）

```text
Result{status=Feasible, cost=0, order=[0, 3, 2, 1, 4]}
```

### 3.7 飛び飛びの空き枠へ、担当者を割り当てる

4人を4枠へ1人ずつ割り当てます。担当者0は枠0・2、1は1・3、2は0・1、3は2・3に参加できます。
さらに、人と枠の組ごとに負担の点数を付け、その合計を小さくします。
空き枠を単一の区間で表せないので `set_positions` を使います。
入力の負担表は、行が担当者ID、列が枠です。禁止枠の表の値は使われません。

**入力形式**：`n`、各IDについて `許可枠数 許可枠...` が `n` 行、その後に負担の `n×n` 表。

**入力例**（`inputs/case07.txt`）

```text
4
2 0 2
2 1 3
2 0 1
2 2 3
4 0 1 0
0 2 0 1
1 3 0 0
0 0 2 3
```

**コード**（`examples/case07.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    // 担当者と枠の一対一対応を、枠順の担当者IDという順列で表す。
    Solver solver(n);

    // 参加可能な枠と負担を同じ問題へ設定する。
    auto& p = solver.problem();

    for (int i = 0; i < n; ++i) {
        int count;
        if (!(std::cin >> count)) {
            return 1;
        }

        std::vector<int> positions(count);

        for (int& pos : positions) {
            if (!(std::cin >> pos)) {
                return 1;
            }
        }

        // 許可位置を一覧へ置き換える。空の一覧は、その人を配置不能にする。
        p.set_positions(i, positions);
    }

    std::vector<int64_t> costs(static_cast<size_t>(n) * n);

    for (auto& value : costs) {
        if (!(std::cin >> value)) {
            return 1;
        }
    }

    // 行優先のn*n個。costs[i*n+pos]が担当者iを枠posへ入れる負担。
    p.set_position_costs(costs);

    // 負担表はコピーされるので、以後costsを書き換えても問題は変わらない。
    const auto result = solver.solve();

    // order[pos]が、その枠に割り当てられた担当者ID。
    std::cout << result << '\n';
}
```

**標準出力の一例**（`outputs/case07.txt`）

```text
Result{status=Feasible, cost=7, order=[2, 1, 0, 3]}
```

### 3.8 複数の部署の許可条件を重ね、固定を取り消す

番組3について、第一部署が位置0・2・4を許可し、第二部署が位置1以上4未満だけを許可します。
さらに設備の都合で位置2・4だけに絞ると、共通して使えるのは位置2です。その位置へ固定して一度解きます。
次に、この番組に関する許可条件をすべて解除し、位置4へ固定して解き直します。
「置き換え」「共通部分を残す」「全解除」の違いを見る例です。他の番組には制約を付けません。

**入力形式**：`n ID`、`個数 許可位置...`、`lo hi`、`個数 許可位置...`、`最初の固定位置 次の固定位置`。

**入力例**（`inputs/case08.txt`）

```text
5 3
3 0 2 4
1 4
2 2 4
2 4
```

**コード**（`examples/case08.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n, id, count, lo, hi, first_pos, next_pos;
    if (!(std::cin >> n >> id >> count)) {
        return 1;
    }

    std::vector<int> first(count);

    for (int& x : first) {
        if (!(std::cin >> x)) {
            return 1;
        }
    }

    if (!(std::cin >> lo >> hi >> count)) {
        return 1;
    }

    std::vector<int> second(count);

    for (int& x : second) {
        if (!(std::cin >> x)) {
            return 1;
        }
    }

    if (!(std::cin >> first_pos >> next_pos)) {
        return 1;
    }

    // 各部署の条件を一つの問題へ重ねる。
    Solver solver(n);

    // 問題の設定はsolve後も維持される。
    auto& p = solver.problem();

    // 第一部署の一覧で、この要素の許可集合を置き換える。
    p.set_positions(id, first);

    // 現在の集合と[lo, hi)の共通部分だけを残す。
    p.restrict_position_range(id, lo, hi);

    // さらに設備側の一覧との共通部分だけを残す。
    p.restrict_positions(id, second);

    // first_posが既存集合にないと、許可位置が空になる点に注意。
    p.fix_position(id, first_pos);

    // 共通部分を満たす最初の配置を表示する。
    std::cout << "first: " << solver.solve() << '\n';

    // この要素の位置条件をすべて解除して、全位置を許可する。
    p.clear_position_restriction(id);

    // 解除後なら、以前の固定位置と異なる位置にも固定できる。
    p.fix_position(id, next_pos);

    // 費用・ほかの要素の条件はそのままで解き直す。
    std::cout << "second: " << solver.solve() << '\n';
}
```

**標準出力の一例**（`outputs/case08.txt`）

```text
first: Result{status=Feasible, cost=0, order=[0, 1, 3, 4, 2]}
second: Result{status=Feasible, cost=0, order=[0, 1, 4, 2, 3]}
```

### 3.9 戻り道を含まない、有向の訪問経路を作る

4地点を1回ずつ訪問します。地点間の移動費は方向によって違います。
出発地点・終着地点は自由で、最後から最初へは戻りません。費用表の行が出発地、列が到着地です。
入力例では `[0, 2, 1, 3]` の移動費は `1+1+1=3` です。
直接続けて訪れない地点の組には移動費を払いません。これが順序費用との違いです。

**入力形式**：`n` の後に移動費の `n×n` 表。全ての地点間を移動できる設定です。

**入力例**（`inputs/case09.txt`）

```text
4
0 4 1 7
6 0 5 1
3 1 0 4
2 6 3 0
```

**コード**（`examples/case09.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<int64_t> travel(static_cast<size_t>(n) * n);

    for (auto& value : travel) {
        if (!(std::cin >> value)) {
            return 1;
        }
    }

    // 各地点を1回訪れる順列。出発点と終着点は今回は固定しない。
    Solver solver(n);

    // travel[i*n+j]はiの直後にjを訪れるときだけ加算する。
    solver.problem().set_adjacent_costs(travel);

    // 最後から最初への辺は目的関数に含まれない。
    const auto result = solver.solve();

    // 未設定の移動費は0になるため、必要な全組の費用を表に入れる。
    std::cout << result << '\n';
}
```

**標準出力の一例**（`outputs/case09.txt`）

```text
Result{status=Feasible, cost=3, order=[0, 2, 1, 3]}
```

### 3.10 決めた拠点から出発し、最後に拠点へ戻る

4地点のうち地点0を拠点にします。0から出発し、ほかの地点をすべて1回ずつ訪問した後、0へ戻ります。
ライブラリの順列自体には終端から始端への移動がありません。そこで0を先頭に固定し、
「最後に地点iを置いたときの位置費用」に、iから0へ戻る移動費を入れます。
入力例で `[0, 2, 1, 3]` なら、帰路を含む費用は `1+1+1+2=5` です。
出力の末尾へ0を付けるのは表示だけで、ライブラリへ重複したIDを渡すわけではありません。

**入力形式**：`n 拠点ID` の後に移動費の `n×n` 表。`n>=2` とします。

**入力例**（`inputs/case10.txt`）

```text
4 0
0 4 1 7
6 0 5 1
3 1 0 4
2 6 3 0
```

**コード**（`examples/case10.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n, depot;
    if (!(std::cin >> n >> depot) || n < 2) {
        return 1;
    }

    std::vector<int64_t> travel(static_cast<size_t>(n) * n);

    for (auto& value : travel) {
        if (!(std::cin >> value)) {
            return 1;
        }
    }

    // 拠点も含めてn地点を各1回並べる。
    Solver solver(n);

    // 通常の移動・帰路・始点固定をまとめて設定する。
    auto& p = solver.problem();

    // 順列内で隣り合う地点間の移動費。
    p.set_adjacent_costs(travel);

    // 拠点を位置0へ固定する。全位置許可からの固定なので矛盾しない。
    p.fix_position(depot, 0);

    for (int i = 0; i < n; ++i) {
        // 最後の地点から拠点への帰路。
        p.set_position_cost(i, n - 1, travel[static_cast<size_t>(i) * n + depot]);
    }

    // costには帰路も含まれる。順列のIDは重複しない。
    const auto result = solver.solve();

    // 状態を確認するための要約を出す。
    std::cout << result << '\n';

    // 未発見・不可能の場合、経路は使わない。
    if (result.status != Solver::Status::Feasible) {
        return 0;
    }

    std::cout << "route:";

    // 要約と違い、このループは経路全体を出力する。
    for (int id : result.order) {
        std::cout << ' ' << id;
    }

    std::cout << ' ' << depot << '\n';
}
```

**標準出力の一例**（`outputs/case10.txt`）

```text
Result{status=Feasible, cost=5, order=[0, 2, 1, 3]}
route: 0 2 1 3 0
```

### 3.11 工程の希望・遅い位置の罰金・段取り替えを同時に扱う

4仕事を1列で処理します。仕事の種類は順に0・1・0・1で、隣の仕事と種類が変わるたびに段取り費用2が発生します。
仕事2は前半の位置0・1だけに置け、位置が1つ遅くなるごとに費用2を払います。
必須条件は「1を3より前」。希望は「2を0より前：6」「0を1より前：3」「1を2より前：1」です。
三つの費用を足し、必須条件を全部守った中で合計を小さくします。
単位の違う費用を足す場合は、利用者が重要度を決めて共通の点数に換算します。

**入力形式**：`n 段取り費用`、種類が `n` 個、希望数と希望一覧、必須先行関係1本、最後に `制限するID lo hi 遅い位置1つあたりの費用`。

**入力例**（`inputs/case11.txt`）

```text
4 2
0 1 0 1
3
2 0 6
0 1 3
1 2 1
1 3
2 0 2 2
```

**コード**（`examples/case11.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n, m;
    int64_t setup;
    if (!(std::cin >> n >> setup)) {
        return 1;
    }

    std::vector<int> family(n);

    for (int& x : family) {
        if (!(std::cin >> x)) {
            return 1;
        }
    }

    // すべての費用を同じ整数の点数へ換算して扱う。
    Solver solver(n);

    // 三種類の費用と必須制約は併用できる。
    auto& p = solver.problem();
    if (!(std::cin >> m)) {
        return 1;
    }

    for (int k = 0; k < m; ++k) {
        int a, b;
        int64_t w;
        if (!(std::cin >> a >> b >> w)) {
            return 1;
        }

        // 離れた仕事の間でも、希望の前後が逆ならwを払う。
        p.add_preference(a, b, w);
    }

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (family[i] != family[j]) {
                // 隣の仕事と種類が変わったときだけ払う。
                p.set_adjacent_cost(i, j, setup);
            }
        }
    }

    int before, after, id, lo, hi;
    int64_t late;
    if (!(std::cin >> before >> after >> id >> lo >> hi >> late)) {
        return 1;
    }

    // 費用を払っても破れない先行関係。
    p.require_before(before, after);

    // この仕事に許す位置を[lo, hi)へ限定する。
    p.set_position_range(id, lo, hi);

    for (int pos = 0; pos < n; ++pos) {
        // 絶対位置が遅いほど高い費用。未設定の費用は0。
        p.add_position_cost(id, pos, late * pos);
    }

    // 三種類の費用を合計した値を小さくする。
    const auto result = solver.solve();

    // costが各費用の合計であり、必須制約違反の罰金は混ざらない。
    std::cout << result << '\n';
}
```

**標準出力の一例**（`outputs/case11.txt`）

```text
Result{status=Feasible, cost=3, order=[2, 0, 1, 3]}
```

### 3.12 すべての組の前後費用を、表から読み込む

3件の案件について、前後関係ごとの損失が既に表で与えられているとします。
行i・列jの数値は「iをjより前に置いたときに払う費用」です。希望に逆らった罰金を自動で表すわけではありません。
入力例では0が2より前なら4、1が0より前なら3、2が1より前なら1を払います。
順列 `[2, 0, 1]` の費用は1です。両方向に数値があっても、その順列に現れる向きだけが使われます。

**入力形式**：`n` の後に順序費用の `n×n` 表。行を上から順に読みます。

**入力例**（`inputs/case12.txt`）

```text
3
0 0 4
3 0 0
0 1 0
```

**コード**（`examples/case12.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<int64_t> costs(static_cast<size_t>(n) * n);

    for (auto& value : costs) {
        if (!(std::cin >> value)) {
            return 1;
        }
    }

    // IDの全順列を対象にする。
    Solver solver(n);

    // 行優先のn*n個で順序費用全体を置き換える。対角成分は使わない。
    solver.problem().set_order_costs(costs);

    // 前後の費用なので、隣接していない組も評価に含まれる。
    const auto result = solver.solve();

    // 費用表は内部へコピーされ、costsの寿命に依存しない。
    std::cout << result << '\n';
}
```

**標準出力の一例**（`outputs/case12.txt`）

```text
Result{status=Feasible, cost=1, order=[2, 0, 1]}
```

### 3.13 少数の前後費用と割引を、レコード一覧から読み込む

費用が付く案件の組だけを一覧にします。正の値は追加費用、負の値は割引と考えられます。
同じ向きのレコードは合算されるので、入力の `(0,1,7)` と `(0,1,-2)` は合わせて費用5です。
`(1,0,1)` は反対向きの別の費用です。一覧にない向きの費用は0です。
すべての組の表を用意しなくてもよい入力方法です。

**入力形式**：`n 件数` の後に `先に置くID 後に置くID 費用` を件数分。費用は負でも構いません。

**入力例**（`inputs/case13.txt`）

```text
3 6
0 1 7
0 1 -2
1 0 1
2 0 4
0 2 -1
1 2 2
```

**コード**（`examples/case13.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n, count;
    if (!(std::cin >> n >> count)) {
        return 1;
    }

    // 1件を{before, after, cost}で表す公開構造体の一覧。
    std::vector<Solver::OrderTerm> terms;

    for (int k = 0; k < count; ++k) {
        int before, after;
        int64_t cost;
        if (!(std::cin >> before >> after >> cost)) {
            return 1;
        }

        // 実際にbeforeが先ならcostを加える。負の値は割引。
        terms.push_back(Solver::OrderTerm{before, after, cost});
    }

    // 初期状態では登録のない前後費用は0。
    Solver solver(n);

    // 順序費用全体を置き換え、同じ向きの重複を合算する。
    solver.problem().set_order_terms(terms);

    // これは費用の一覧。満たすべき先行関係の一覧ではない。
    const auto result = solver.solve();

    // 費用が負でも、通常どおり小さい方がよい。
    std::cout << result << '\n';
}
```

**標準出力の一例**（`outputs/case13.txt`）

```text
Result{status=Feasible, cost=2, order=[1, 0, 2]}
```

### 3.14 小数の満足度を最大化する

3個の商品を3つの展示位置へ配置します。商品と位置の組ごとの満足度を足し、合計を最大にします。
このライブラリは最小化するので、満足度の符号を反転して位置費用へ入れます。
さらに定数10を加えて、目的関数を「10−満足度の合計」とします。10は順列の選び方には影響しません。
入力例で `[2, 0, 1]` なら満足度は `4+1.5+2=7.5`、費用は2.5です。
元の満足度は「10−返却費用」で取り出せます。

**入力形式**：`n 基準値` の後に満足度の `n×n` 表。小数は有限値を指定します。

**入力例**（`inputs/case14.txt`）

```text
3 10
0 1.5 0
0 0 2
4 1 0
```

**コード**（`examples/case14.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <iomanip>
#include <iostream>

// 費用を小数の doubleで扱う。
using Solver = LinearOrderingSolver<double>;

int main() {
    int n;
    double baseline;
    if (!(std::cin >> n >> baseline)) {
        return 1;
    }

    // この例のSolverはLinearOrderingSolver<double>。整数以外の費用も扱える。
    Solver solver(n);

    // 最大化したい値を、符号を反転した費用として登録する。
    auto& p = solver.problem();

    // 定数は基準値。最適な順番は変えず、返却費用だけをずらす。
    p.set_constant_cost(baseline);

    for (int i = 0; i < n; ++i) {
        for (int pos = 0; pos < n; ++pos) {
            double reward;
            if (!(std::cin >> reward)) {
                return 1;
            }

            // 満足度が高い組ほど、最小化する費用は低くなる。
            p.set_position_cost(i, pos, -reward);
        }
    }

    // 入力だけでなく、合計や差分も有限値の範囲に収める。
    const auto result = solver.solve();

    // 小数点以下2桁で要約を表示する。
    std::cout << std::fixed << std::setprecision(2) << result << '\n';

    // 費用を使う前に、実行可能解があることを確認する。
    if (result.status != Solver::Status::Feasible) {
        return 0;
    }

    // 最小化用の費用を、元の満足度へ戻す。
    std::cout << "reward=" << baseline - result.cost << '\n';
}
```

**標準出力の一例**（`outputs/case14.txt`）

```text
Result{status=Feasible, cost=2.50, order=[2, 0, 1]}
reward=7.50
```

### 3.15 手作業で作った順番を、採点・検査する

探索を行わず、外部から渡された順番を確認します。3案件について「2を0より前にしたい：4」があり、
「0を1より前」は必須です。入力順列 `[1, 2, 0]` は希望を守るので費用0ですが、必須条件には違反します。
このように、費用が低いことと実行可能であることは別です。
また `evaluate` へは完全な順列だけを渡せるため、このプログラムでは先にIDの範囲と重複を確認します。

**入力形式**：`n`、希望1件、必須先行関係1件、候補IDが `n` 個。重複などがあれば採点せず終了します。

**入力例**（`inputs/case15.txt`）

```text
3
2 0 4
0 1
1 2 0
```

**コード**（`examples/case15.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n, a, b, before, after;
    int64_t weight;
    if (!(std::cin >> n >> a >> b >> weight >> before >> after) || n < 0) {
        return 1;
    }

    std::vector<int> order(n);

    for (int& id : order) {
        if (!(std::cin >> id)) {
            return 1;
        }
    }

    // 採点だけでも、同じ問題をソルバーへ設定しておく。
    Solver solver(n);

    // 費用と必須条件は別々に登録する。
    auto& p = solver.problem();

    // 希望違反だけを費用にする。
    p.add_preference(a, b, weight);

    // 必須条件の違反はevaluateの費用には加算されない。
    p.require_before(before, after);

    // 不正な順列でもfalseを返す検査API。
    std::cout << std::boolalpha << "feasible=" << solver.is_feasible(order) << '\n';

    std::vector<int> sorted = order;
    std::sort(sorted.begin(), sorted.end());

    for (int i = 0; i < n; ++i) {
        if (sorted[i] != i) {
            std::cout << "not a permutation\n";
            return 0;
        }
    }

    // 完全な順列なら、必須条件違反があっても元の費用を採点できる。
    std::cout << "cost=" << solver.evaluate(order) << '\n';
}
```

**標準出力の一例**（`outputs/case15.txt`）

```text
feasible=false
cost=0
```

### 3.16 新しい必須条件に合わない予定を、修復する

既存の予定 `[2, 0, 1]` に、後から「1は2より前」という必須条件が加わりました。
希望の費用は例1と同じです。既存の予定は完全な順列ですが、新しい必須条件には違反しています。
この順列を初期値にして修復・改善します。ここでは探索の途中での制約違反も許す設定を示します。
最終的に `Feasible` として返る順列は、必ず必須条件を守ります。

**入力形式**：`n m`、希望 `m` 件、必須先行関係1件、既存の順列が `n` 個。

**入力例**（`inputs/case16.txt`）

```text
3 3
2 0 4
0 1 3
1 2 1
1 2
2 0 1
```

**コード**（`examples/case16.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n, m;
    if (!(std::cin >> n >> m)) {
        return 1;
    }

    // 新しい条件が適用される問題を作る。
    Solver solver(n);

    // 既存の順番とは別に、最新のモデルを設定する。
    auto& p = solver.problem();

    for (int k = 0; k < m; ++k) {
        int a, b;
        int64_t w;
        if (!(std::cin >> a >> b >> w)) {
            return 1;
        }

        // 守りたい希望と、破ったときの費用。
        p.add_preference(a, b, w);
    }

    int before, after;
    if (!(std::cin >> before >> after)) {
        return 1;
    }

    // 新しい必須条件。初期値はこれに違反していてもよい。
    p.require_before(before, after);
    std::vector<int> initial(n);

    for (int& id : initial) {
        if (!(std::cin >> id)) {
            return 1;
        }
    }

    // 省略した設定には既定値が入る。
    Solver::Options options;

    // この呼び出し全体の終了目標を50ミリ秒にする。
    options.time_limit_us = 50'000;

    // 乱数の種。時間制限があるため、同じ種でも完全な再現性は保証しない。
    options.seed = 7;

    // 探索途中の違反を許す。falseでも初期違反の修復は行われる。
    options.allow_infeasible_moves = true;

    // 全IDが1回ずつ必要。欠損や重複を補完するAPIではない。
    const auto result = solver.improve(initial, options);

    // 実行可能解が見つからない場合もあるため、statusを必ず確認する。
    std::cout << result << '\n';
}
```

**標準出力の一例**（`outputs/case16.txt`）

```text
Result{status=Feasible, cost=3, order=[1, 2, 0]}
```

### 3.17 確定済みの仕事を動かさず、残りだけを改善する

6仕事の予定 `[4, 2, 0, 5, 1, 3]` があり、仕事4・2・5の位置は確定しています。
仕事0・1・3だけを残った位置へ並べ直します。可動フラグは位置番号ではなく、仕事IDで引きます。
希望は「3を0より先：8」「0を1より先：5」。固定仕事2の直後が0なら段取り費用7、3なら1です。
仕事3を最後に置くと費用4も掛かります。固定仕事も目的関数から消えるわけではありません。

**入力形式**：`n`、既存順列、ID順の可動フラグ（0は固定、1は可動）。この例は `n=6` 専用です。

**入力例**（`inputs/case17.txt`）

```text
6
4 2 0 5 1 3
1 1 0 1 0 0
```

**コード**（`examples/case17.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n;
    if (!(std::cin >> n) || n != 6) {
        return 1;
    }

    std::vector<int> initial(n);

    for (int& id : initial) {
        if (!(std::cin >> id)) {
            return 1;
        }
    }

    std::vector<uint8_t> movable(n);

    for (auto& flag : movable) {
        int value;
        if (!(std::cin >> value) || (value != 0 && value != 1)) {
            return 1;
        }

        flag = static_cast<uint8_t>(value);
    }

    // 初期順列全体と同じ6要素の問題を使う。
    Solver solver(n);

    // 固定要素に関係する費用も通常どおり登録する。
    auto& p = solver.problem();

    // 仕事3を0より先にしたい。
    p.add_preference(3, 0, 8);

    // 仕事0を1より先にしたい。
    p.add_preference(0, 1, 5);

    // 固定仕事2の直後が0だと7の費用。
    p.set_adjacent_cost(2, 0, 7);

    // 固定仕事2の直後が3なら1の費用。
    p.set_adjacent_cost(2, 3, 1);

    // 仕事3を最後の位置5へ置くと4の費用。
    p.set_position_cost(3, 5, 4);

    // {}は既定Options。movable[id]==0のIDを初期位置に固定する。
    const auto result = solver.improve(initial, {}, movable);

    // 返るのは可動部分だけでなく、固定要素も含む全順列。
    std::cout << result << '\n';

    // 未発見・不可能なら順列を利用しない。
    if (result.status != Solver::Status::Feasible) {
        return 0;
    }

    bool fixed_ok = true;

    for (int pos = 0; pos < n; ++pos) {
        if (!movable[initial[pos]]) {
            // 今回の呼び出しの固定条件を確認する。
            fixed_ok &= result.order[pos] == initial[pos];
        }
    }

    std::cout << std::boolalpha << "fixed=" << fixed_ok << '\n';
}
```

**標準出力の一例**（`outputs/case17.txt`）

```text
Result{status=Feasible, cost=1, order=[4, 2, 3, 5, 0, 1]}
fixed=true
```

### 3.18 現在の最良案を引き継ぎ、短い時間で繰り返し改善する

毎回20ミリ秒ずつ使って、同じ発表順を3回改善します。問題と費用は変えません。
最初の予定 `[1, 0, 2]` の費用は7です。実行可能な順列から `improve` を呼ぶと、
その呼び出しでは初期値より費用が大きい結果を返しません。そのため、結果を次回へ引き継げます。
`solve` を繰り返すだけでは前回の順列を初期値として引き継がないので、ここでは `improve` を使います。

**入力形式**：`n 希望数 回数`、希望一覧、初期順列。回数は1以上とします。

**入力例**（`inputs/case18.txt`）

```text
3 3 3
2 0 4
0 1 3
1 2 1
1 0 2
```

**コード**（`examples/case18.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n, m, rounds;
    if (!(std::cin >> n >> m >> rounds) || rounds < 1) {
        return 1;
    }

    // 何回改善しても、同じ問題を使い続ける。
    Solver solver(n);

    for (int k = 0; k < m; ++k) {
        int a, b;
        int64_t w;
        if (!(std::cin >> a >> b >> w)) {
            return 1;
        }

        // 希望を登録する。この例では後で費用を変更しない。
        solver.problem().add_preference(a, b, w);
    }

    std::vector<int> current(n);

    for (int& id : current) {
        if (!(std::cin >> id)) {
            return 1;
        }
    }

    // 改善保証を使うので、初期値が実行可能であることを確認する。
    if (!solver.is_feasible(current)) {
        return 1;
    }

    // 初期値の費用を最新の問題で採点する。
    std::cout << "initial_cost=" << solver.evaluate(current) << '\n';

    // 各回の設定を一つ用意する。
    Solver::Options options;

    // 1回20ミリ秒。全体では回数分の時間と呼び出しの付随時間が必要。
    options.time_limit_us = 20'000;

    for (int turn = 0; turn < rounds; ++turn) {
        // 同じ初期値でも違う探索を試すため種を変える。
        options.seed = static_cast<uint64_t>(turn);

        // 実行可能な初期値より悪い費用にはしない。
        const auto result = solver.improve(current, options);

        // 各回で得た状態・費用・順列を表示する。
        std::cout << "round " << turn << ": " << result << '\n';

        // 結果を取り出す前に状態を確認する。
        if (result.status != Solver::Status::Feasible) {
            return 1;
        }

        // 順列を自分で保存し、次のimproveへ明示的に渡す。
        current = result.order;
    }
}
```

**標準出力の一例**（`outputs/case18.txt`）

```text
initial_cost=7
round 0: Result{status=Feasible, cost=1, order=[2, 0, 1]}
round 1: Result{status=Feasible, cost=1, order=[2, 0, 1]}
round 2: Result{status=Feasible, cost=1, order=[2, 0, 1]}
```

### 3.19 全体の締切を共有し、複数の初期順序を試す

5件の案件の順序を、ランダムな初期順列を変えながら最大4回探します。
全体の終了目標は120ミリ秒で、1回あたりも最大40ミリ秒を目標にします。
各回の最良値を比較して、全体の最良案を残します。
事前準備の時間も全体の締切に含めるため、締切を作ってから `prepare` を呼びます。
`prepare` 自体は時間制限付きではなく、処理全体も厳密に時刻どおり止まる保証ではありません。

**入力形式**：`n 希望数 最大回数`、希望一覧。最大回数は1以上とします。

**入力例**（`inputs/case19.txt`）

```text
5 5 4
4 0 6
0 1 5
1 2 4
2 3 3
3 4 2
```

**コード**（`examples/case19.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n, m, starts;
    if (!(std::cin >> n >> m >> starts) || starts < 1) {
        return 1;
    }

    // 費用は全試行で共有し、初期順列だけを変える。
    Solver solver(n);

    for (int k = 0; k < m; ++k) {
        int a, b;
        int64_t w;
        if (!(std::cin >> a >> b >> w)) {
            return 1;
        }

        // 全試行に共通する希望。
        solver.problem().add_preference(a, b, w);
    }

    // 相対的な時間目標と、共通の絶対締切を両方設定する。
    Solver::Options options;

    // 各回は40ミリ秒を上限目標にする。
    options.time_limit_us = 40'000;

    // steady_clock上の全体締切。実時刻とは別。
    options.deadline = Solver::Clock::now() + std::chrono::milliseconds(120);

    // 任意の事前準備。通常solve/improve内でも準備される。これは締切で中断しない。
    solver.prepare();

    // 初期状態はNotFound、空順列、cost=0。0を最良費用とみなしてはいけない。
    Solver::Result best;

    std::mt19937_64 random(123);
    std::vector<int> initial(n);
    std::iota(initial.begin(), initial.end(), 0);

    for (int trial = 0; trial < starts; ++trial) {
        // 共有締切を過ぎたら新しい試行を始めない。
        if (trial > 0 && Solver::Clock::now() >= options.deadline) {
            break;
        }

        std::shuffle(initial.begin(), initial.end(), random);

        // 初期順列を作る乱数と、ソルバー内部の乱数は別々。
        options.seed = static_cast<uint64_t>(trial);

        // 同じdeadlineを再使用し、残り時間内で試す。
        const auto result = solver.improve(initial, options);

        // 費用比較は、実行可能解に対してだけ行う。
        // 最初の実行可能解は必ず保存する。
        if (result.status == Solver::Status::Feasible &&
            (best.status != Solver::Status::Feasible || result.cost < best.cost)) {
            // 結果は値としてコピーできる。順列も一緒に保存する。
            best = result;
        }
    }

    // 全試行の中の最良案。締切到達後も検証・返却の時間は必要。
    std::cout << best << '\n';
}
```

**標準出力の一例**（`outputs/case19.txt`）

```text
Result{status=Feasible, cost=2, order=[4, 0, 1, 2, 3]}
```

### 3.20 長い順番を、窓の中だけ少しずつ組み替える

6件の案件の順序を、一度に連続3枠ずつ改善します。窓の外にいる案件はその呼び出しでは固定し、
窓を1枠ずつずらします。要素間の評価は全体の問題のままなので、窓の外との前後関係も正しく数えられます。
入力例の希望は「5を0より先：8」「4を1より先：5」「3を2より先：2」です。
窓が小さいと、大きく順番を変えないと得られない改善を逃すことがあります。窓幅を大きくすると自由度も増えます。

**入力形式**：`n 希望数 窓幅`、希望一覧、初期順列。窓幅は1以上 `n` 以下です。

**入力例**（`inputs/case20.txt`）

```text
6 3 3
5 0 8
4 1 5
3 2 2
0 1 2 3 4 5
```

**コード**（`examples/case20.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n, m, width;
    if (!(std::cin >> n >> m >> width) || width < 1 || width > n) {
        return 1;
    }

    // 窓だけの小問題に切り出さず、全n要素の費用を保つ。
    Solver solver(n);

    for (int k = 0; k < m; ++k) {
        int a, b;
        int64_t w;
        if (!(std::cin >> a >> b >> w)) {
            return 1;
        }

        // 窓の外との前後関係も含む全体の費用。
        solver.problem().add_preference(a, b, w);
    }

    std::vector<int> initial(n);

    for (int& id : initial) {
        if (!(std::cin >> id)) {
            return 1;
        }
    }

    // 以後の各部分改善に実行可能な初期値を与える。
    if (!solver.is_feasible(initial)) {
        return 1;
    }

    // 呼び出しごとの設定。
    Solver::Options options;

    // まず探索せず、初期値をResultとして受け取る。
    options.time_limit_us = 0;

    // 時間0でも、実行可能な初期値は採点して返す。
    auto best = solver.improve(initial, options);

    // ここから各窓に20ミリ秒の終了目標を与える。
    options.time_limit_us = 20'000;

    for (int left = 0; left + width <= n; ++left) {
        std::vector<uint8_t> movable(n, 0);

        for (int pos = left; pos < left + width; ++pos) {
            // フラグは位置でなくIDで引く。現在の窓内のIDを可動にする。
            movable[best.order[pos]] = 1;
        }

        // 窓ごとに乱数の種を変える。
        options.seed = static_cast<uint64_t>(left);

        // 窓の外を今の位置に固定して全体費用を改善する。
        const auto result = solver.improve(best.order, options, movable);

        // 実行可能な初期値からなので実行可能解を維持する。
        if (result.status != Solver::Status::Feasible) {
            return 1;
        }

        // 改善された全順列を次の窓へ引き継ぐ。
        best = result;
    }

    // 各窓での改善結果を反映した全体の順番を表示する。
    std::cout << best << '\n';
}
```

**標準出力の一例**（`outputs/case20.txt`）

```text
Result{status=Feasible, cost=8, order=[0, 3, 4, 2, 1, 5]}
```

### 3.21 費用設定を更新し、不要な評価項目を外す

運用中に評価基準が変わる例です。3件の予定 `[0,1,2]` に、前後費用・位置費用・隣接費用・定数を付けて採点します。
その後、それぞれの一部を上書き・加算して再採点し、三種類の費用を順に全解除します。
最後に定数を0へ戻し、先行条件を変更して再び解きます。費用を消しても、ほかの費用や制約は消えません。
保存済みの `Result.cost` は問題を変えても自動更新されません。比較するときは現在の問題で再採点します。

**入力形式**：`n` と採点する順列。この例の費用設定は `n=3` 専用です。

**入力例**（`inputs/case21.txt`）

```text
3
0 1 2
```

**コード**（`examples/case21.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n;
    if (!(std::cin >> n) || n != 3) {
        return 1;
    }

    std::vector<int> order(n);

    for (int& id : order) {
        if (!(std::cin >> id)) {
            return 1;
        }
    }

    // 同じ要素数のまま、費用や制約を変更できる。
    Solver solver(n);

    // 所有する問題を参照で編集する。
    auto& p = solver.problem();

    // 0が1より前なら7を加える。
    p.add_order_cost(0, 1, 7);

    // 2が位置2なら5を加える。
    p.add_position_cost(2, 2, 5);

    // 0の直後が1なら4を加える。
    p.add_adjacent_cost(0, 1, 4);

    // 全順列に共通の定数を2増やす。
    p.add_constant_cost(2);

    // [0,1,2]なら7+5+4+2=18。
    std::cout << "initial=" << solver.evaluate(order) << '\n';

    // この向きだけ7から3へ上書きする。
    p.set_order_cost(0, 1, 3);

    // この向きへさらに1を足し、4にする。
    p.add_order_cost(0, 1, 1);

    // この要素と位置の費用を2へ上書きする。
    p.set_position_cost(2, 2, 2);

    // 位置費用を1増やして3にする。
    p.add_position_cost(2, 2, 1);

    // この隣接費用を1へ上書きする。
    p.set_adjacent_cost(0, 1, 1);

    // 隣接費用を2増やして3にする。
    p.add_adjacent_cost(0, 1, 2);

    // 定数全体を5へ置き換える。
    p.set_constant_cost(5);

    // 最新の設定では4+3+3+5=15。
    std::cout << "updated=" << solver.evaluate(order) << '\n';

    // 全方向の順序費用を0へ戻す。位置費用・隣接費用・定数は残る。
    p.clear_order_costs();

    // 残る費用は3+3+5=11。
    std::cout << "without_order=" << solver.evaluate(order) << '\n';

    // 全要素・全位置の位置費用を0へ戻す。
    p.clear_position_costs();

    // 残る費用は3+5=8。
    std::cout << "without_position=" << solver.evaluate(order) << '\n';

    // 全方向の隣接費用を0へ戻す。
    p.clear_adjacent_costs();

    // 定数5だけが残る。
    std::cout << "constant_only=" << solver.evaluate(order) << '\n';

    // 定数専用のclearはないので、0を代入して解除する。
    p.set_constant_cost(0);

    // 例として先行条件を追加する。
    p.require_before(2, 0);

    // 先行条件をすべて解除する。位置制約は解除しない。
    p.clear_precedences();

    // 新しい先行条件を追加する。
    p.require_before(0, 2);

    // 手動のprepareは不要。変更後の設定で解かれる。
    const auto result = solver.solve();

    // この時点では全費用0で、0が2より先という条件だけが残る。
    std::cout << result << '\n';
}
```

**標準出力の一例**（`outputs/case21.txt`）

```text
initial=18
updated=15
without_order=11
without_position=8
constant_only=5
Result{status=Feasible, cost=0, order=[0, 1, 2]}
```

### 3.22 共通モデルをコピーして、別々の条件を比較する

同じ3件の希望を持つ基本モデルから、シナリオA・Bを作ります。
Aには「0を位置0へ固定」、Bには「0を位置1へ固定」を付け、独立に解きます。
基本モデルをコピーして使う方法と、作ったモデルの所有権を移して使う方法の両方を示します。
`problem()` の戻り値を編集するときに `auto&` と書く理由も、この例で確認できます。

**入力形式**：`n 希望数` と希望一覧。この例の固定条件には `n>=2` が必要です。

**入力例**（`inputs/case22.txt`）

```text
3 3
2 0 4
0 1 3
1 2 1
```

**コード**（`examples/case22.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <cstdint>
#include <iostream>
#include <utility>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n, m;
    if (!(std::cin >> n >> m) || n < 2) {
        return 1;
    }

    // ソルバーとは別に、再利用する基本問題を作る。
    Solver::Problem base(n);

    for (int k = 0; k < m; ++k) {
        int a, b;
        int64_t w;
        if (!(std::cin >> a >> b >> w)) {
            return 1;
        }

        // すべてのシナリオで使う希望。
        base.add_preference(a, b, w);
    }

    // 左辺値baseを渡すと問題はコピーされる。baseは引き続き使える。
    Solver a(base);

    // Aだけの条件を追加する。baseやBには伝わらない。
    a.problem().fix_position(0, 0);

    // Problem自身もコピー構築できる。コピー代入はできない。
    Solver::Problem scenario(base);

    // B用の条件を、ソルバーを作る前に付ける。
    scenario.fix_position(0, 1);

    // Problemをムーブ構築する。scenarioは以後、再利用しない。
    Solver::Problem moved(std::move(scenario));

    // データの所有権をBへ移す。movedは以後、再利用しない。
    Solver b(std::move(moved));

    // autoだけだと問題のコピーができる。Aそのものの設定窓口ではない。
    auto detached = a.problem();

    // コピーだけを変更するため、Aの費用は消えない。
    detached.clear_order_costs();

    // 読み取り専用としてソルバーを参照する。
    const Solver& view = b;

    // const版problem()とsize()で要素数を読む。
    std::cout << "size=" << view.problem().size() << '\n';

    // コピー後にAへ付けた固定条件で解く。
    std::cout << "A: " << a.solve() << '\n';

    // Bの別の固定条件で解く。
    std::cout << "B: " << b.solve() << '\n';
}
```

**標準出力の一例**（`outputs/case22.txt`）

```text
size=3
A: Result{status=Feasible, cost=4, order=[0, 1, 2]}
B: Result{status=Feasible, cost=1, order=[2, 0, 1]}
```

### 3.23 未発見・不可能・空問題を、状態で区別する

同じ「順列が空」に見える場合でも、意味は異なります。
2要素の例で、時間0では初期順列が固定条件に合わず `NotFound`、正しい初期順列を渡すと `Feasible` になります。
次に、互いに相手より前という矛盾した条件を置くと `ProvenInfeasible` になります。
さらに、問題そのものは解けても、今回の可動フラグが全要素を誤った位置へ固定していると、
その部分問題について `ProvenInfeasible` になることを示します。
最後は要素0個、定数費用7の正当な空問題です。この結果は空順列でも `Feasible` です。

**入力形式**：空問題の定数費用を整数1個で指定します。ほかの条件は、状態の違いを見せるためコード中に固定しています。

**入力例**（`inputs/case23.txt`）

```text
7
```

**コード**（`examples/case23.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int64_t constant;
    if (!(std::cin >> constant)) {
        return 1;
    }

    // 時間以外は既定設定。
    Solver::Options zero;

    // 探索時間を与えない。初期順列の採点・検証は行われる。
    zero.time_limit_us = 0;

    // 初期順列はsolveだと[0,1]。
    Solver limited(2);

    // [0,1]は条件違反だが、[1,0]なら実行可能。
    limited.problem().fix_position(0, 1);

    // NotFound。不可能と証明されたという意味ではない。
    std::cout << "limited: " << limited.solve(zero) << '\n';

    std::vector<int> valid{1, 0};

    // 時間0でも実行可能な初期順列は返る。
    const auto feasible = limited.improve(valid, zero);

    // Feasibleは最適性の証明ではない。
    std::cout << "supplied: " << feasible << '\n';

    // 結果の判定は順列の長さではなく、この列挙型で行う。
    switch (feasible.status) {
        // 必須条件を満たす解を使用できる。
        case Solver::Status::Feasible:
            std::cout << "use the order\n";
            break;

        // 解の有無はまだ結論できない。
        case Solver::Status::NotFound:
            std::cout << "no solution found yet\n";
            break;

        // 今回の条件に矛盾が確認された。
        case Solver::Status::ProvenInfeasible:
            std::cout << "constraints conflict\n";
            break;
    }

    // 矛盾を持つ別の問題。
    Solver cyclic(2);

    // 0は1より前。
    cyclic.problem().require_before(0, 1);

    // 1も0より前なので、同時には成立しない。
    cyclic.problem().require_before(1, 0);

    // 十分な時間を与え、この矛盾が検出されることを見る。
    std::cout << "cycle: " << cyclic.solve() << '\n';

    std::vector<int> wrong{0, 1};
    std::vector<uint8_t> fixed{0, 0};

    // 条件に反する位置へ全要素を固定した部分問題は不可能。
    std::cout << "fixed: " << limited.improve(wrong, {}, fixed) << '\n';

    // 要素数0も有効。空順列が唯一の順列。
    Solver empty(0);

    // 空問題でも定数は評価される。
    empty.problem().set_constant_cost(constant);

    // Feasible、order=[]、costは入力の定数。
    std::cout << "empty: " << empty.solve(zero) << '\n';
}
```

**標準出力の一例**（`outputs/case23.txt`）

```text
limited: Result{status=NotFound, order=[]}
supplied: Result{status=Feasible, cost=0, order=[1, 0]}
use the order
cycle: Result{status=ProvenInfeasible, order=[]}
fixed: Result{status=ProvenInfeasible, order=[]}
empty: Result{status=Feasible, cost=7, order=[]}
```

### 3.24 結果を短くログへ出し、必要なときだけ全順列を出す

25要素の順列を、そのまま初期値として渡します。費用・制約を付けず、探索時間0なので、入力順列がそのまま返ります。
結果構造体をストリームへ渡すと、先頭20要素までの見やすい要約になります。
その後、`order` を自分で走査して全要素を出します。要約表示が省略されても、結果のデータが切られるわけではありません。
`std::cout` と `std::cerr` の両方を示します。二つの出力先を混ぜた表示の順序は環境によって前後するので、出力例は分けて示します。

**入力形式**：`n` と完全な初期順列。入力例の `n=25` は表示上限を超える様子を確認するためです。

**入力例**（`inputs/case24.txt`）

```text
25
0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
```

**コード**（`examples/case24.cpp`）

```cpp
// 順列を作るライブラリ本体。同じフォルダーか検索パスに置く。
#include "linear_ordering_solver_v10.hpp"

#include <iostream>
#include <vector>

// 費用を符号付き整数の int64_tで扱う。
using Solver = LinearOrderingSolver<>;

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<int> initial(n);

    for (int& id : initial) {
        if (!(std::cin >> id)) {
            return 1;
        }
    }

    // 全位置を許可し、費用は0の問題。
    Solver solver(n);

    // 結果表示に集中するため、時間だけを設定する。
    Solver::Options options;

    // 実行可能な初期順列をそのまま採点して受け取る。
    options.time_limit_us = 0;

    // 戻り値のResultが全n要素の順列を所有する。
    const auto result = solver.improve(initial, options);

    // operator<<は先頭20要素まで。改行は自分で付ける。
    std::cout << result << '\n';

    // 同じ演算子を標準エラー出力でも利用できる。
    std::cerr << "debug: " << result << '\n';

    // 全要素を取り出す前に成功状態を確認する。
    if (result.status != Solver::Status::Feasible) {
        return 0;
    }

    std::cout << "full:";

    // 必要な用途では自分のループで全要素を出す。
    for (int id : result.order) {
        std::cout << ' ' << id;
    }

    std::cout << '\n';
}
```

**標準出力の一例**（`outputs/case24.txt`）

```text
Result{status=Feasible, cost=0, order=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ... (25 total)]}
full: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
```

**標準エラー出力の一例**

```text
debug: Result{status=Feasible, cost=0, order=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ... (25 total)]}
```
